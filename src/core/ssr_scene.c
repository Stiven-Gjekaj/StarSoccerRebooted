#include "ssr_scene.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_expr.h"

#define MAX_ORIGINS 64
#define MAX_LAYERS 32

typedef struct {
    char *name;
    char *file;
} ImageFile;

struct SSRScene {
    SSRSceneItem *items;
    int count;
    int capacity;
    ImageFile *files;
    int fileCount;
    char **strings;
    int stringCount;
    int stringCapacity;
};

typedef struct {
    SSRScene *scene;
    const SSRSceneEnv *env;
    // The origin and the panel (left, top, right, bottom) of each level of
    // PushOrigin. This is the origin stack of c_SceneFactory.
    float originX[MAX_ORIGINS], originY[MAX_ORIGINS];
    float panel[MAX_ORIGINS][4];
    int originCount;
    // "@x" is vars['x']. The game keeps the screen edges and the Set:
    // values in one table like this (c_MathUtils.m_SimpleVars).
    double vars[128];
    char layerNames[MAX_LAYERS][32];
    int layerCount;
} Walker;

static const char *keep(SSRScene *scene, const char *text) {
    if (!text) return NULL;
    if (scene->stringCount == scene->stringCapacity) {
        int capacity = scene->stringCapacity ? scene->stringCapacity * 2 : 256;
        char **grown = realloc(scene->strings, (size_t)capacity * sizeof(char *));
        if (!grown) return "";
        scene->strings = grown;
        scene->stringCapacity = capacity;
    }
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return "";
    strcpy(copy, text);
    scene->strings[scene->stringCount++] = copy;
    return copy;
}

static double variable(void *context, char kind, const char *name) {
    const Walker *w = context;
    if (kind != '@' || !name[0]) return 0;
    return w->vars[(unsigned char)name[0] & 127];
}

// Sets the letters of the panel of the top level: @L, @T, @R, @B, the size
// @W and @H, and the centre @C and @M.
static void set_panel_vars(Walker *w) {
    const float *p = w->panel[w->originCount - 1];
    w->vars['L'] = p[0];
    w->vars['T'] = p[1];
    w->vars['R'] = p[2];
    w->vars['B'] = p[3];
    w->vars['W'] = p[2] - p[0];
    w->vars['H'] = p[3] - p[1];
    w->vars['C'] = (p[0] + p[2]) * 0.5f;
    w->vars['M'] = (p[1] + p[3]) * 0.5f;
}

static double tweak(void *context, const char *group, const char *name) {
    const Walker *w = context;
    return ssr_tweaks_number(w->env->tweaks, group, name);
}

static double number(Walker *w, const SSRUiNode *node, int index, double fallback) {
    if (!node || index >= node->argCount || !node->args[index][0]) return fallback;
    SSRExprScope scope = {variable, tweak, w};
    int ok = 0;
    double value = ssr_expr_eval(node->args[index], &scope, &ok);
    return ok ? value : fallback;
}

static const char *arg(const SSRUiNode *node, int index) {
    return node && index < node->argCount ? node->args[index] : "";
}

// Finds the ')' that closes the '(' at text[0]. Returns NULL when there is
// none.
static const char *closing(const char *text) {
    int depth = 0, quoted = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '"') quoted = !quoted;
        if (quoted) continue;
        if (*p == '(') depth++;
        if (*p == ')' && --depth == 0) return p;
    }
    return NULL;
}

static void append(char *out, size_t capacity, const char *text, size_t length) {
    size_t used = strlen(out);
    if (used + 1 >= capacity) return;
    if (length > capacity - used - 1) length = capacity - used - 1;
    memcpy(out + used, text, length);
    out[used + length] = 0;
}

// Evaluates a text expression: "quoted" parts, twk(Group,Name), and
// loc(expression), joined with "::". Other text stays as it is.
static void text_value(Walker *w, const char *text, char *out, size_t capacity) {
    out[0] = 0;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        const char *next = NULL;
        if (*p == '"') {
            const char *end = strchr(p + 1, '"');
            if (!end) {
                append(out, capacity, p + 1, strlen(p + 1));
                return;
            }
            append(out, capacity, p + 1, (size_t)(end - p - 1));
            next = end + 1;
        } else if (strncmp(p, "twk(", 4) == 0 && closing(p + 3)) {
            const char *end = closing(p + 3);
            char inside[128];
            snprintf(inside, sizeof(inside), "%.*s", (int)(end - p - 4), p + 4);
            char *comma = strchr(inside, ',');
            if (comma) {
                *comma = 0;
                const char *value = ssr_tweaks_get(w->env->tweaks, inside, comma + 1);
                append(out, capacity, value, strlen(value));
            }
            next = end + 1;
        } else if (strncmp(p, "loc(", 4) == 0 && closing(p + 3)) {
            const char *end = closing(p + 3);
            char inside[512], tag[512];
            snprintf(inside, sizeof(inside), "%.*s", (int)(end - p - 4), p + 4);
            text_value(w, inside, tag, sizeof(tag));
            const char *value = w->env->text ? w->env->text(w->env->context, tag) : tag;
            append(out, capacity, value, strlen(value));
            next = end + 1;
        } else {
            const char *join = strstr(p, "::");
            size_t length = join ? (size_t)(join - p) : strlen(p);
            append(out, capacity, p, length);
            next = p + length;
        }
        p = next;
        while (*p == ' ') p++;
        if (strncmp(p, "::", 2) == 0) {
            p += 2;
        } else if (*p) {
            // Text after a closed part that is not a join stays as it is.
            append(out, capacity, p, strlen(p));
            return;
        }
    }
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// Reads "#RRGGBBAA", "#RRGGBB", or "#RRGGBBAARRGGBBAA". The second colour
// of the long form is not used yet.
static uint32_t colour(const char *text, uint32_t fallback) {
    if (!text || text[0] != '#') return fallback;
    size_t length = strlen(text + 1);
    if (length != 6 && length != 8 && length != 16) return fallback;
    uint32_t value = 0;
    size_t digits = length == 6 ? 6 : 8;
    for (size_t i = 0; i < digits; i++) {
        int v = hex_value(text[1 + i]);
        if (v < 0) return fallback;
        value = (value << 4) | (uint32_t)v;
    }
    return length == 6 ? (value << 8) | 0xFF : value;
}

static SSRSceneItem *add_item(Walker *w, SSRItemKind kind, int layer, const char *ref) {
    SSRScene *s = w->scene;
    if (s->count == s->capacity) {
        int capacity = s->capacity ? s->capacity * 2 : 256;
        SSRSceneItem *grown = realloc(s->items, (size_t)capacity * sizeof(SSRSceneItem));
        if (!grown) return NULL;
        s->items = grown;
        s->capacity = capacity;
    }
    SSRSceneItem *item = &s->items[s->count++];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->colour = 0xFFFFFFFF;
    item->layer = layer;
    item->ref = ref ? keep(s, ref) : NULL;
    item->alignX = 'C';
    item->alignY = 'M';
    item->font = 1;
    item->fontScaleX = item->fontScaleY = 1;
    return item;
}

static const SSRUiNode *modifier(const SSRUiNode *node, const char *name) {
    for (int i = node->modifierCount - 1; i >= 0; i--) {
        if (strcmp(node->modifiers[i]->name, name) == 0) return node->modifiers[i];
    }
    return NULL;
}

// ShowIf(Group,Name,Value), ShowIfNot(...), AndShowIf(...), and
// OrShowIf(...). They go in order.
static int visible(Walker *w, const SSRUiNode *node) {
    if (node->flags & SSR_UI_HIDE) return 0;
    int shown = 1, any = 0;
    for (int i = 0; i < node->modifierCount; i++) {
        const SSRUiNode *m = node->modifiers[i];
        if (strcmp(m->name, "set") == 0 && strcmp(arg(m, 0), "gel.vis") == 0) {
            if (number(w, m, 1, 1) == 0) return 0;
            continue;
        }
        int show = strcmp(m->name, "ShowIf") == 0, not = strcmp(m->name, "ShowIfNot") == 0;
        int and = strcmp(m->name, "AndShowIf") == 0, or = strcmp(m->name, "OrShowIf") == 0;
        if (!show && !not && !and && !or) continue;
        int value = m->argCount >= 3 ? ssr_tweaks_equals(w->env->tweaks, arg(m, 0), arg(m, 1), arg(m, 2))
                                     : ssr_tweaks_number(w->env->tweaks, arg(m, 0), arg(m, 1)) != 0;
        if (not) value = !value;
        if (or && any) {
            shown = shown || value;
        } else if (and && any) {
            shown = shown && value;
        } else {
            shown = value;
        }
        any = 1;
    }
    return shown;
}

static int layer_of(Walker *w, const SSRUiNode *node, int inherited) {
    const SSRUiNode *m = modifier(node, "Layer");
    if (!m || m->argCount < 1) return inherited;
    for (int i = 0; i < w->layerCount; i++) {
        if (strcmp(w->layerNames[i], arg(m, 0)) == 0) return i + 1;
    }
    if (w->layerCount < MAX_LAYERS) {
        snprintf(w->layerNames[w->layerCount], sizeof(w->layerNames[0]), "%s", arg(m, 0));
        w->layerCount++;
        return w->layerCount;
    }
    return inherited;
}

// The modifiers that change how an image or a text draws.
static void apply_look(Walker *w, const SSRUiNode *node, SSRSceneItem *item) {
    const SSRUiNode *m;
    if ((m = modifier(node, "Colour"))) item->colour = colour(arg(m, 0), item->colour);
    if (modifier(node, "Additive")) item->additive = 1;
    if ((m = modifier(node, "Angle"))) item->angle = (float)number(w, m, 0, 0);
    float handleX = 0, handleY = 0;
    if ((m = modifier(node, "Handle"))) {
        handleX = (float)number(w, m, 0, 0);
        handleY = (float)number(w, m, 1, 0);
    }
    float scaleX = 1, scaleY = 1;
    if ((m = modifier(node, "Scale"))) {
        scaleX = (float)number(w, m, 0, 1);
        scaleY = (float)number(w, m, 1, scaleX);
    }
    // The handle is the point of the box that sits at (x, y). The box
    // scales and turns about it.
    float px = item->x, py = item->y;
    item->x = px - handleX * scaleX;
    item->y = py - handleY * scaleY;
    item->w *= scaleX;
    item->h *= scaleY;
    item->pivotX = px;
    item->pivotY = py;
}

static void walk(Walker *w, const SSRUiNode *node, int layer);

// A Tile: draws Copies(columns,rows) of its image. A copy is Spacing(x,y)
// from the one before it, and TileShear(x,y) moves each row and column. A
// tile with no Copies() draws nothing, as in the game.
static void tile(Walker *w, const SSRUiNode *node, SSRSceneItem *first) {
    const SSRUiNode *copies = modifier(node, "Copies");
    const SSRUiNode *spacing = modifier(node, "Spacing");
    const SSRUiNode *shear = modifier(node, "TileShear");
    int columns = copies ? (int)number(w, copies, 0, 0) : 0;
    int rows = copies ? (int)number(w, copies, 1, 0) : 0;
    float stepX = spacing ? (float)number(w, spacing, 0, first->w) : first->w;
    float stepY = spacing ? (float)number(w, spacing, 1, first->h) : first->h;
    float shearX = shear ? (float)number(w, shear, 0, 0) : 0;
    float shearY = shear ? (float)number(w, shear, 1, 0) : 0;
    if (columns <= 0 || rows <= 0) {
        w->scene->count--;
        return;
    }
    SSRSceneItem model = *first;
    w->scene->count--;
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            SSRSceneItem *copy = add_item(w, SSR_ITEM_IMAGE, model.layer, NULL);
            if (!copy) return;
            *copy = model;
            float dx = column * stepX + row * shearX, dy = row * stepY + column * shearY;
            copy->x += dx;
            copy->y += dy;
            copy->pivotX += dx;
            copy->pivotY += dy;
        }
    }
}

// Takes the click message and its sound from the Click() modifier.
static void apply_click(Walker *w, const SSRUiNode *node, SSRSceneItem *item) {
    const SSRUiNode *click = modifier(node, "Click");
    if (!click) return;
    item->click = keep(w->scene, click->argCount ? arg(click, 0) : "");
    for (int i = 0; i < click->childCount; i++) {
        if (strcmp(click->children[i]->name, "PlaySnd") == 0) {
            item->clickSound = keep(w->scene, arg(click->children[i], 0));
            break;
        }
    }
}

static void walk_children(Walker *w, const SSRUiNode *node, int layer) {
    for (int i = 0; i < node->childCount; i++) walk(w, node->children[i], layer);
}

// "@x=expression" sets vars['x']. Only the first letter of the name
// counts, as in the game.
static void set_variable(Walker *w, const char *assignment) {
    if (assignment[0] != '@' || !assignment[1]) return;
    const char *equals = strchr(assignment, '=');
    if (!equals) return;
    SSRExprScope scope = {variable, tweak, w};
    w->vars[(unsigned char)assignment[1] & 127] = ssr_expr_eval(equals + 1, &scope, NULL);
}

static void add_file(SSRScene *s, const char *name, const char *file) {
    ImageFile *grown = realloc(s->files, (size_t)(s->fileCount + 1) * sizeof(ImageFile));
    if (!grown) return;
    s->files = grown;
    s->files[s->fileCount].name = (char *)keep(s, name);
    s->files[s->fileCount].file = (char *)keep(s, file);
    s->fileCount++;
}

// PushOrigin:(x,y,w,h,angle). x can be Left, Right, or Centre, and y can be
// Top, Bottom, or Centre: then the place is on the screen, not from the
// origin. w and h are the size of the new panel, @W and @H by default. The
// children see a panel from (0, 0). The angle is not used yet.
static void push_origin(Walker *w, const SSRUiNode *node, int ltrb, float ox, float oy) {
    const SSRSceneEnv *env = w->env;
    int absolute = 0;
    float x, y;
    const char *ax = arg(node, 0), *ay = arg(node, 1);
    if (strcmp(ax, "Left") == 0) {
        x = env->left;
        absolute = 1;
    } else if (strcmp(ax, "Right") == 0) {
        x = env->left + env->width;
        absolute = 1;
    } else if (strcmp(ax, "Centre") == 0) {
        x = env->left + env->width * 0.5f;
        absolute = 1;
    } else {
        x = (float)number(w, node, 0, 0);
    }
    if (strcmp(ay, "Top") == 0) {
        y = env->top;
        absolute = 1;
    } else if (strcmp(ay, "Bottom") == 0) {
        y = env->top + env->height;
        absolute = 1;
    } else if (strcmp(ay, "Centre") == 0) {
        y = env->top + env->height * 0.5f;
        absolute = 1;
    } else {
        y = (float)number(w, node, 1, 0);
    }
    float width = (float)w->vars['W'], height = (float)w->vars['H'];
    float pw = ltrb ? (float)number(w, node, 2, width + x) - x : (float)number(w, node, 2, width);
    float ph = ltrb ? (float)number(w, node, 3, height + y) - y : (float)number(w, node, 3, height);
    int level = w->originCount++;
    w->originX[level] = absolute ? x : ox + x;
    w->originY[level] = absolute ? y : oy + y;
    w->panel[level][0] = 0;
    w->panel[level][1] = 0;
    w->panel[level][2] = pw;
    w->panel[level][3] = ph;
    set_panel_vars(w);
}

// Spine:(name,x,y,angle,animation,Loop,limit) draws a pose of a Spine
// animation at the time of the screen. Each image is drawn at its own size
// with its centre on its place.
static void spine(Walker *w, const SSRUiNode *node, int layer, float ox, float oy) {
    if (!w->env->spine) return;
    const SSRSpine *file = w->env->spine(w->env->context, arg(node, 0));
    if (!file) return;
    int animation = ssr_spine_animation(file, arg(node, 4));
    if (animation < 0) return;
    // A limit of loops plays the animation that many times, then it stops.
    int limit = (int)number(w, node, 6, 0);
    int loop = strcmp(arg(node, 5), "Loop") == 0 && limit <= 0;
    float time = w->env->time;
    if (limit > 0) {
        float duration = ssr_spine_duration(file, animation);
        if (time > duration * limit) time = duration;
        else if (duration > 0) time = fmodf(time, duration);
    }
    float x = ox + (float)number(w, node, 1, 0), y = oy + (float)number(w, node, 2, 0);
    float radians = (float)number(w, node, 3, 0) * 3.14159265f / 180;
    float c = cosf(radians), s = sinf(radians);
    SSRSpineDraw draws[128];
    int count = ssr_spine_pose(file, animation, time, loop, draws, 128);
    if (count > 128) count = 128;
    for (int i = 0; i < count; i++) {
        const SSRSpineDraw *d = &draws[i];
        float iw = 0, ih = 0;
        if (!w->env->image_size || !w->env->image_size(w->env->context, d->image, d->file, &iw, &ih)) continue;
        SSRSceneItem *item = add_item(w, SSR_ITEM_IMAGE, layer, NULL);
        if (!item) return;
        item->image = keep(w->scene, d->image);
        item->quad = 1;
        item->w = iw;
        item->h = ih;
        const float local[8] = {-iw / 2, -ih / 2, iw / 2, -ih / 2, iw / 2, ih / 2, -iw / 2, ih / 2};
        for (int k = 0; k < 4; k++) {
            float px = d->m[0] * local[k * 2] + d->m[2] * local[k * 2 + 1] + d->m[4];
            float py = d->m[1] * local[k * 2] + d->m[3] * local[k * 2 + 1] + d->m[5];
            item->corners[k * 2] = x + px * c - py * s;
            item->corners[k * 2 + 1] = y + px * s + py * c;
        }
        item->x = item->corners[0];
        item->y = item->corners[1];
        uint32_t r = (uint32_t)(d->r * 255 + 0.5f), g = (uint32_t)(d->g * 255 + 0.5f);
        uint32_t b = (uint32_t)(d->b * 255 + 0.5f), a = (uint32_t)(d->a * 255 + 0.5f);
        item->colour = (r << 24) | (g << 16) | (b << 8) | a;
    }
}

static void walk(Walker *w, const SSRUiNode *node, int layer) {
    const char *name = node->name;
    float ox = w->originX[w->originCount - 1], oy = w->originY[w->originCount - 1];

    if (strcmp(name, "PopOrigin") == 0) {
        if (w->originCount > 1) w->originCount--;
        set_panel_vars(w);
        return;
    }
    if (strcmp(name, "Set") == 0 && node->argCount > 0) {
        set_variable(w, arg(node, 0));
        return;
    }
    if (strcmp(name, "Img") == 0 && node->argCount >= 2) {
        add_file(w->scene, arg(node, 0), arg(node, 1));
        return;
    }
    if (!(node->flags & SSR_UI_ELEMENT) && strcmp(name, "Group") != 0) return;
    if (!visible(w, node)) return;
    layer = layer_of(w, node, layer);

    int ltrb = strstr(name, "LTRB") != NULL;
    if (strcmp(name, "PushOrigin") == 0 || strcmp(name, "PushOriginLTRB") == 0) {
        if (w->originCount < MAX_ORIGINS) push_origin(w, node, ltrb, ox, oy);
        walk_children(w, node, layer);
        return;
    }

    if (strcmp(name, "Draw") == 0 || strcmp(name, "DrawLTRB") == 0 || strcmp(name, "Tile") == 0 ||
        strcmp(name, "DrawFixedScale") == 0) {
        SSRSceneItem *item = add_item(w, SSR_ITEM_IMAGE, layer, node->ref);
        if (item) {
            item->image = keep(w->scene, arg(node, 0));
            float x = (float)number(w, node, 1, 0), y = (float)number(w, node, 2, 0);
            float width = (float)number(w, node, 3, 0), height = (float)number(w, node, 4, 0);
            if (ltrb) {
                width -= x;
                height -= y;
            }
            if (width == 0 || height == 0) {
                float iw = 0, ih = 0;
                const char *file = ssr_scene_image_file(w->scene, item->image);
                if (w->env->image_size && w->env->image_size(w->env->context, item->image, file, &iw, &ih) && iw > 0 &&
                    ih > 0) {
                    if (width == 0 && height == 0) {
                        width = iw;
                        height = ih;
                    } else if (width == 0) {
                        width = iw * height / ih;
                    } else {
                        height = ih * width / iw;
                    }
                }
            }
            item->x = ox + x;
            item->y = oy + y;
            item->w = width;
            item->h = height;
            item->angle = (float)number(w, node, 5, 0);
            item->colour = colour(arg(node, 6), 0xFFFFFFFF);
            apply_look(w, node, item);
            apply_click(w, node, item);
            if (strcmp(name, "Tile") == 0) tile(w, node, item);
        }
        walk_children(w, node, layer);
        return;
    }

    if (strcmp(name, "Text") == 0 || strcmp(name, "TextLoc") == 0 || strcmp(name, "TextBox") == 0) {
        SSRSceneItem *item = add_item(w, SSR_ITEM_TEXT, layer, node->ref);
        if (item) {
            char value[1024];
            text_value(w, arg(node, 0), value, sizeof(value));
            for (int i = 0; i < node->modifierCount; i++) {
                const SSRUiNode *m = node->modifiers[i];
                if (strcmp(m->name, "set") == 0 && strcmp(arg(m, 0), "gel.text") == 0) {
                    text_value(w, arg(m, 1), value, sizeof(value));
                }
            }
            const char *text = value;
            // TextLoc and TextBox take a language tag. A tag that the
            // language does not have stays as it is.
            if (strcmp(name, "Text") != 0 && w->env->text) text = w->env->text(w->env->context, value);
            item->text = keep(w->scene, text);
            item->x = ox + (float)number(w, node, 1, 0);
            item->y = oy + (float)number(w, node, 2, 0);
            item->w = (float)number(w, node, 3, 0);
            item->h = (float)number(w, node, 4, 0);
            if (arg(node, 5)[0]) item->alignX = (char)toupper((unsigned char)arg(node, 5)[0]);
            if (arg(node, 6)[0]) item->alignY = (char)toupper((unsigned char)arg(node, 6)[0]);
            item->colour = colour(arg(node, 7), 0xFFFFFFFF);
            const SSRUiNode *size = modifier(node, "FontSize");
            if (size) item->font = (int)number(w, size, 0, 1);
            if (item->font < 0 || item->font > 3) item->font = 1;
            const SSRUiNode *scale = modifier(node, "FontScale");
            if (scale) {
                item->fontScaleX = (float)number(w, scale, 0, 1);
                item->fontScaleY = (float)number(w, scale, 1, item->fontScaleX);
            }
            item->wrap = strcmp(name, "TextBox") == 0 || modifier(node, "WrapX") != NULL;
            apply_look(w, node, item);
        }
        walk_children(w, node, layer);
        return;
    }

    if (strcmp(name, "Spine") == 0) {
        spine(w, node, layer, ox, oy);
        walk_children(w, node, layer);
        return;
    }

    if (strcmp(name, "Clip") == 0 || strcmp(name, "ClipLTRB") == 0) {
        SSRSceneItem *item = add_item(w, SSR_ITEM_CLIP, layer, NULL);
        if (item) {
            item->x = ox + (float)number(w, node, 0, 0);
            item->y = oy + (float)number(w, node, 1, 0);
            item->w = (float)number(w, node, 2, 0);
            item->h = (float)number(w, node, 3, 0);
            if (ltrb) {
                item->w -= item->x - ox;
                item->h -= item->y - oy;
            }
        }
        walk_children(w, node, layer);
        add_item(w, SSR_ITEM_UNCLIP, layer, NULL);
        return;
    }

    if (strcmp(name, "Rect") == 0 || strcmp(name, "RectLTRB") == 0) {
        if (node->ref || modifier(node, "Click")) {
            SSRSceneItem *item = add_item(w, SSR_ITEM_AREA, layer, node->ref);
            if (item) {
                item->x = ox + (float)number(w, node, 0, 0);
                item->y = oy + (float)number(w, node, 1, 0);
                item->w = (float)number(w, node, 2, 0);
                item->h = (float)number(w, node, 3, 0);
                if (ltrb) {
                    item->w -= item->x - ox;
                    item->h -= item->y - oy;
                }
                apply_click(w, node, item);
            }
        }
        walk_children(w, node, layer);
        return;
    }

    // Other elements can hold children that draw.
    walk_children(w, node, layer);
}

SSRScene *ssr_scene_build(const SSRUiTree *tree, const SSRSceneEnv *env) {
    SSRScene *scene = calloc(1, sizeof(SSRScene));
    Walker *w = calloc(1, sizeof(Walker));
    if (!scene || !w || !env) {
        free(scene);
        free(w);
        return NULL;
    }
    w->scene = scene;
    w->env = env;
    w->originCount = 1;
    w->panel[0][0] = env->left;
    w->panel[0][1] = env->top;
    w->panel[0][2] = env->left + env->width;
    w->panel[0][3] = env->top + env->height;
    set_panel_vars(w);
    const SSRUiNode *root = ssr_ui_root(tree);
    if (root) walk_children(w, root, 0);
    free(w);
    // A stable sort by layer: the items of each layer keep the order of the
    // file.
    if (scene->count > 1) {
        int *order = malloc((size_t)scene->count * sizeof(int));
        SSRSceneItem *sorted = malloc((size_t)scene->count * sizeof(SSRSceneItem));
        if (order && sorted) {
            int n = 0;
            for (int layer = 0; layer <= MAX_LAYERS; layer++) {
                for (int i = 0; i < scene->count; i++) {
                    if (scene->items[i].layer == layer) order[n++] = i;
                }
            }
            for (int i = 0; i < n; i++) sorted[i] = scene->items[order[i]];
            memcpy(scene->items, sorted, (size_t)n * sizeof(SSRSceneItem));
            scene->count = n;
        }
        free(order);
        free(sorted);
    }
    return scene;
}

void ssr_scene_free(SSRScene *scene) {
    if (!scene) return;
    for (int i = 0; i < scene->stringCount; i++) free(scene->strings[i]);
    free(scene->strings);
    free(scene->items);
    free(scene->files);
    free(scene);
}

int ssr_scene_count(const SSRScene *scene) {
    return scene ? scene->count : 0;
}

const SSRSceneItem *ssr_scene_item(const SSRScene *scene, int index) {
    return scene && index >= 0 && index < scene->count ? &scene->items[index] : NULL;
}

const char *ssr_scene_image_file(const SSRScene *scene, const char *name) {
    if (!scene || !name) return NULL;
    for (int i = scene->fileCount - 1; i >= 0; i--) {
        if (strcmp(scene->files[i].name, name) == 0) return scene->files[i].file;
    }
    return NULL;
}

static const SSRSceneItem *hit(const SSRScene *scene, float x, float y, int clicksOnly) {
    if (!scene) return NULL;
    for (int i = scene->count - 1; i >= 0; i--) {
        const SSRSceneItem *item = &scene->items[i];
        if (clicksOnly ? !item->click : !item->ref) continue;
        if (item->kind == SSR_ITEM_CLIP || item->kind == SSR_ITEM_UNCLIP) continue;
        float x0 = item->w < 0 ? item->x + item->w : item->x, x1 = item->w < 0 ? item->x : item->x + item->w;
        float y0 = item->h < 0 ? item->y + item->h : item->y, y1 = item->h < 0 ? item->y : item->y + item->h;
        if (x >= x0 && x < x1 && y >= y0 && y < y1) return item;
    }
    return NULL;
}

const SSRSceneItem *ssr_scene_hit_item(const SSRScene *scene, float x, float y) {
    return hit(scene, x, y, 1);
}

const char *ssr_scene_hit(const SSRScene *scene, float x, float y) {
    const SSRSceneItem *item = hit(scene, x, y, 0);
    return item ? item->ref : NULL;
}
