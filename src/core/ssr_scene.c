#include "ssr_scene.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_ex.h"
#include "ssr_expr.h"
#include "ssr_random.h"

#define MAX_ORIGINS 64
#define MAX_LAYERS 64
#define PI_F 3.14159265f
// A depth that the gel takes from its parent.
#define DEPTH_INHERIT INT_MIN

typedef struct {
    char *name;
    char *file;
} ImageFile;

// A 2D matrix as c_GMatrix: x' = a*x + c*y + tx, y' = b*x + d*y + ty.
typedef struct {
    float a, b, c, d, tx, ty;
} Mat;

// A colour of the game (c_GColour): r, g, b from 0 to 255, a from 0 to 1,
// and a mix factor from 0 to 1 for each channel.
typedef struct {
    float r, g, b, a;
    float mr, mg, mb, ma;
} Colour;

typedef enum {
    GEL_GROUP,
    GEL_DRAW,
    GEL_TEXT,
    GEL_RECT,
    GEL_CLIP,
    // Sheet:: a 3D plane with perspective (c_GelSheet).
    GEL_SHEET,
    GEL_CROWD,
    GEL_SPINE,
    GEL_ANIM,
    GEL_ANIMATION,
    GEL_DURING,
    GEL_TWEEN,
    GEL_SEQUENCE,
    GEL_EMITTER,
    GEL_EMBED,
    GEL_DRAW_EMBEDDED,
} GelKind;

typedef enum {
    D_SHOW,
    D_SET_TWEAK,
    D_COL_READOUT,
    D_CONVERGE_POS,
    D_CONVERGE_SCALE,
    D_CONVERGE_SIZE,
    D_CONVERGE_COL,
    D_DELAY,
    D_PRESS,
    D_NOPRESS,
    D_MOVER,
    D_SPINNER,
    D_WRAP_X,
    D_WRAP_Y,
    D_FADER,
    D_LIFETIME,
    D_TWEEN_ANGLE,
    D_TWEEN_SIZE,
    D_TWEEN_COL,
    D_TWEEN_ALPHA,
    D_TWEEN_SCALE,
    D_TWEEN_POS,
    D_SHINE,
    D_PLAY_ANIM,
    D_READOUT,
    D_STRING_READOUT,
    D_PROVIDER,
    D_SELECTOR,
    D_INCREMENT_TWEAK,
    D_IF,
    D_PLAY_SOUND,
    D_SCROLLING_LIST,
    D_SHOW_SHELL,
    D_SWIPE_PANEL,
    D_EMIT_PLACER,
    D_EMIT_MOVER,
    D_EMIT_SPINNER,
    D_EMIT_LIFETIME,
    D_EMIT_ANIM_START,
    D_EMIT_SIZE,
    // set, add, lerp, and the other doodads of the expression language.
    D_EX,
    // DoIf(Cat,Name,min,max) < >: the block runs while the tweak is in range.
    D_DOIF,
    // CloneTweak(Cat,Name,Cat,Name): the second tweak takes the value of the
    // first.
    D_CLONE_TWEAK,
} DoodadKind;

// The flags of a doodad, as c_Doodad has them.
#define F_ACTIVE 1u
#define F_ONCE 2u
#define F_REARM 4u
#define F_BANG 8u
#define F_FIRED 16u

typedef struct Doodad Doodad;
struct Doodad {
    DoodadKind kind;
    unsigned flags;
    Doodad *next;
    // Press(), NoPress(), and if(): the doodads of the "< >" block. other
    // is the block of the else of an if().
    Doodad *chain;
    Doodad *other;
    // D_SHOW: the node with the ShowIf modifiers.
    const SSRUiNode *node;
    float v[8];
    Colour from, to;
    const char *text[3];
    // The ref of the line, for the code that changes a doodad (ACombo).
    const char *ref;
    // D_EX and D_IF: the items of the phrase. text[0] of D_EX is the op.
    SSRExNode *ex[3];
    int exCount;
};

typedef struct Gel Gel;
struct Gel {
    GelKind kind;
    const SSRUiNode *node;
    const char *ref;
    Gel *parent;
    Gel **children;
    int childCount, childCapacity;
    // The local transform, in the space of the parent.
    float x, y, angle, sx, sy;
    // The size and the handle: the point of the box at (x, y).
    float w, h, hx, hy;
    int hasColour;
    Colour colour;
    int hidden;
    // The gel goes away after this frame (Lifetime).
    int dead;
    // -1 takes the layer of the parent.
    int layer;
    int depth;
    SSRBlend blend;
    Doodad *doodads;
    // The transform on the screen in the last frame, or at the parse.
    Mat baked;

    // GEL_DRAW and GEL_SEQUENCE frames.
    const char *image;
    // Tile:, as Copies(), Spacing(), and TileShear().
    int tiled, columns, rows;
    float stepX, stepY, shearX, shearY;
    // GEL_TEXT: the text expression, and 1 when it is a language tag.
    const char *text;
    int localise;
    int font;
    char alignX, alignY;
    float fontScaleX, fontScaleY;
    int wrap;
    float italic;
    // Click(message) < PlaySnd(name) >, and the actions of its block.
    const char *click, *clickSound;
    Doodad *clickChain;
    // The text that a readout gives, in place of the text of the line.
    char *shown;
    // The place of the next entry that the code adds to this list.
    float listY;

    // GEL_ANIM: the name of the animation, and its state.
    const char *animName;
    Gel *animation;
    float frame, speed;
    int state;
    // GEL_ANIMATION: the length in frames, the mode, and the loop start.
    float length, loopStart;
    char mode;
    // GEL_DURING and GEL_SEQUENCE: the frames, and the time range.
    float first, last, tfirst, tlength;
    // GEL_TWEEN: the remap of t.
    float tweenFrom, tweenTo;
    // GEL_SEQUENCE.
    const char **frames;
    int frameCount, repeats;
    // GEL_EMITTER.
    const char *category, *templateRef;
    int started, emitted, rateMs, emitCount;
    float next;
    // GEL_CROWD and GEL_SPINE: the values of the line at the parse.
    float p[10];
    const char *name, *animationName;
    // The %variables of float(), strvar(), and colvar().
    SSRExVar **vars;
    int varCount;
    // YDepth(n): the depth is the y on the screen plus n.
    int yDepth;
    float yDepthOffset;
    // The cel colours that CloneTweak gave in this frame: "Cel=RRGGBBAA;".
    char cels[192];
    // GEL_SHEET: the model matrix of the plane (3 rows of 4), the focal
    // length of the perspective (0 for a flat sheet), and 1 when it draws
    // into a surface.
    float trs[12];
    float focal;
    int rtt;
};

typedef struct {
    char name[32];
    int depth;
    int sorted, highToLow;
} Layer;

// The values that an emitter rolls for one particle.
typedef struct {
    float xStart, yStart, angle, lifetime, spin, xSize, ySize, drag, angularDrag;
    float xVel, yVel, xAcc, yAcc, animStart;
    int hasSize;
} Roll;

// The layer depth and the walk order of an item, for the sort.
typedef struct {
    int depth;
    int order;
} SortKey;

struct SSRScene {
    SSRSceneItem *items;
    SortKey *keys;
    int count;
    int capacity;
    ImageFile *files;
    int fileCount;
    char **strings;
    int stringCount;
    int stringCapacity;
    // The strings of the items of the last frame. The next frame frees them.
    char **frameStrings;
    int frameStringCount;
    int frameStringCapacity;
    // The sounds that PlaySnd() of a click asked for.
    const char *sounds[8];
    int soundCount;

    Gel *root;
    // The gels with no parent: animations, templates, and emitters.
    Gel **detached;
    int detachedCount, detachedCapacity;
    // The gels with a ref, in the order of the file.
    Gel **refs;
    int refCount, refCapacity;
    Layer layers[MAX_LAYERS];
    int layerCount;
    // "@x" is vars['x'] (c_MathUtils.m_SimpleVars).
    double vars[128];
    // Embed:(Name) puts its gel here when it draws, and
    // DrawEmbedded:(Name) draws the children of that gel (c_GelEmbed).
    struct {
        const char *name;
        Gel *gel;
    } embeds[32];
    int embedCount;
    // The copies of templates that the code made, by id - 1.
    Gel *spawned[64];
    // The random numbers of the emitters. The crowds have their own.
    SSRRandom random;

    // The time base of the game (bb_timers.g_PumpGameTime).
    int gameMS;
    int msDelta;
    float frameDelta;
    int lerps;
    int started;
    float shineTarget;

    // The touch. pressed is the gel of Press() from the last frame.
    int touchDown;
    float touchX, touchY;
    // The gel that the parse reads now, for the %variables.
    Gel *parseGel;
    // The boxes of the Clip: gels around the gel that draws now.
    struct {
        float x, y, w, h;
    } clips[32];
    int clipCount;
    // The cel colours of the gel that draws now (CloneTweak), or NULL.
    const char *cels;
    // The sheet with perspective that the gels draw on now, or NULL.
    const Gel *sheet;
    Gel *pressed;
    Gel *candidate;
    // The swipe (SwipePanel): the gel that the touch started on, and if the
    // touch moved more than 10 units, so that it is not a click.
    int touchWasDown;
    int touchStarted;
    Gel *swipe;
    Gel *swipeCandidate;
    int dragged;

    // The state of the walk of a frame.
    const SSRSceneEnv *env;
    float animFrame;
    float tweenT;
    Gel *instance;
    int order;
    SSRRandom crowdRandom;

    // The state of the parse.
    Mat origins[MAX_ORIGINS];
    float panel[MAX_ORIGINS][4];
    int originCount;
    float lastAnimationLength;
};

/* ---------------------------------------------------------------- helpers */

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

static const char *keep_frame(SSRScene *scene, const char *text) {
    if (!text) return NULL;
    if (scene->frameStringCount == scene->frameStringCapacity) {
        int capacity = scene->frameStringCapacity ? scene->frameStringCapacity * 2 : 256;
        char **grown = realloc(scene->frameStrings, (size_t)capacity * sizeof(char *));
        if (!grown) return "";
        scene->frameStrings = grown;
        scene->frameStringCapacity = capacity;
    }
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return "";
    strcpy(copy, text);
    scene->frameStrings[scene->frameStringCount++] = copy;
    return copy;
}

static void free_frame_strings(SSRScene *scene) {
    for (int i = 0; i < scene->frameStringCount; i++) free(scene->frameStrings[i]);
    scene->frameStringCount = 0;
}

static void push_gel(Gel ***list, int *count, int *capacity, Gel *gel) {
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 16;
        Gel **bigger = realloc(*list, (size_t)grown * sizeof(Gel *));
        if (!bigger) return;
        *list = bigger;
        *capacity = grown;
    }
    (*list)[(*count)++] = gel;
}

static Mat mat_identity(void) {
    Mat m = {1, 0, 0, 1, 0, 0};
    return m;
}

// The matrix of c_GTransform: i = (cos, -sin) * sx, j = (sin, cos) * sy. A
// positive angle turns anticlockwise on the screen.
static Mat mat_make(float x, float y, float angle, float sx, float sy) {
    float r = angle * PI_F / 180, c = cosf(r), s = sinf(r);
    Mat m = {c * sx, -s * sx, s * sy, c * sy, x, y};
    return m;
}

static Mat mat_concat(Mat p, Mat l) {
    Mat m = {p.a * l.a + p.c * l.b,      p.b * l.a + p.d * l.b,      p.a * l.c + p.c * l.d,
             p.b * l.c + p.d * l.d,      p.a * l.tx + p.c * l.ty + p.tx, p.b * l.tx + p.d * l.ty + p.ty};
    return m;
}

static Mat mat_inverse(Mat m) {
    float det = m.a * m.d - m.b * m.c;
    if (det == 0) return mat_identity();
    Mat i = {m.d / det, -m.b / det, -m.c / det, m.a / det, 0, 0};
    i.tx = -(i.a * m.tx + i.c * m.ty);
    i.ty = -(i.b * m.tx + i.d * m.ty);
    return i;
}

static void mat_apply(Mat m, float x, float y, float *ox, float *oy) {
    *ox = m.a * x + m.c * y + m.tx;
    *oy = m.b * x + m.d * y + m.ty;
}

static float mat_angle(Mat m) {
    return atan2f(-m.b, m.a) * 180 / PI_F;
}

static float mat_scale_x(Mat m) {
    return sqrtf(m.a * m.a + m.b * m.b);
}

static float mat_scale_y(Mat m) {
    float sx = mat_scale_x(m);
    return sx > 0 ? (m.a * m.d - m.b * m.c) / sx : 0;
}

// Sets the local transform of the gel from a matrix (c_GTransform.GetLocal).
static void set_local(Gel *g, Mat m) {
    g->x = m.tx;
    g->y = m.ty;
    g->angle = mat_angle(m);
    g->sx = mat_scale_x(m);
    g->sy = mat_scale_y(m);
}

static Mat local_matrix(const Gel *g) {
    return mat_make(g->x, g->y, g->angle, g->sx, g->sy);
}

static Colour colour_white(void) {
    Colour c = {255, 255, 255, 1, 0, 0, 0, 0};
    return c;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// Reads "#RRGGBB", "#RRGGBBAA", or "#RRGGBBAArrggbbaa". The second 8 digits
// are the mix factors (c_GColour.SetFromString). The '#' can be missing.
static int parse_colour(const char *text, Colour *out) {
    if (!text) return 0;
    if (*text == '#') text++;
    size_t length = strlen(text);
    if (length != 6 && length != 8 && length != 16) return 0;
    int v[16];
    for (size_t i = 0; i < length; i++) {
        v[i] = hex_value(text[i]);
        if (v[i] < 0) return 0;
    }
    Colour c;
    c.r = (float)(v[0] * 16 + v[1]);
    c.g = (float)(v[2] * 16 + v[3]);
    c.b = (float)(v[4] * 16 + v[5]);
    c.a = length >= 8 ? (v[6] * 16 + v[7]) / 255.0f : 1;
    c.mr = c.mg = c.mb = c.ma = 0;
    if (length == 16) {
        c.mr = (v[8] * 16 + v[9]) / 255.0f;
        c.mg = (v[10] * 16 + v[11]) / 255.0f;
        c.mb = (v[12] * 16 + v[13]) / 255.0f;
        c.ma = (v[14] * 16 + v[15]) / 255.0f;
    }
    *out = c;
    return 1;
}

// The colour of a gel on the parent colour (c_GColourStack.PushMix). With
// no mix the own colour replaces the parent. A mix of 1 multiplies.
static Colour colour_mix(Colour own, Colour parent) {
    if (own.mr + own.mg + own.mb + own.ma == 0) return own;
    Colour c = own;
    c.r = own.r + (own.r * parent.r / 255 - own.r) * own.mr;
    c.g = own.g + (own.g * parent.g / 255 - own.g) * own.mg;
    c.b = own.b + (own.b * parent.b / 255 - own.b) * own.mb;
    c.a = own.a + (own.a * parent.a - own.a) * own.ma;
    return c;
}

static uint32_t colour_rgba(Colour c) {
    float v[4] = {c.r, c.g, c.b, c.a * 255};
    uint32_t out = 0;
    for (int i = 0; i < 4; i++) {
        float x = v[i] < 0 ? 0 : v[i] > 255 ? 255 : v[i];
        out = (out << 8) | (uint32_t)(x + 0.5f);
    }
    return out;
}

static Colour colour_lerp(Colour a, Colour b, float t) {
    Colour c = a;
    c.r = a.r + (b.r - a.r) * t;
    c.g = a.g + (b.g - a.g) * t;
    c.b = a.b + (b.b - a.b) * t;
    c.a = a.a + (b.a - a.a) * t;
    return c;
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static double variable(void *context, char kind, const char *name) {
    const SSRScene *s = context;
    if (kind != '@' || !name[0]) return 0;
    return s->vars[(unsigned char)name[0] & 127];
}

static double tweak(void *context, const char *group, const char *name) {
    const SSRScene *s = context;
    return ssr_tweaks_number(s->env->tweaks, group, name);
}

static double number(SSRScene *s, const SSRUiNode *node, int index, double fallback) {
    if (!node || index >= node->argCount || !node->args[index][0]) return fallback;
    SSRExprScope scope = {variable, tweak, s};
    int ok = 0;
    double value = ssr_expr_eval(node->args[index], &scope, &ok);
    return ok ? value : fallback;
}

static const char *arg(const SSRUiNode *node, int index) {
    return node && index < node->argCount ? node->args[index] : "";
}

static int is(const char *name, const char *word) {
    return strcmp(name, word) == 0;
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
static void text_value(SSRScene *s, const char *text, char *out, size_t capacity) {
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
                const char *value = ssr_tweaks_get(s->env->tweaks, inside, comma + 1);
                append(out, capacity, value, strlen(value));
            }
            next = end + 1;
        } else if (strncmp(p, "loc(", 4) == 0 && closing(p + 3)) {
            const char *end = closing(p + 3);
            char inside[512], tag[512];
            snprintf(inside, sizeof(inside), "%.*s", (int)(end - p - 4), p + 4);
            text_value(s, inside, tag, sizeof(tag));
            const char *value = s->env->text ? s->env->text(s->env->context, tag) : tag;
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

/* ------------------------------------------------------------------- gels */

static Gel *new_gel(GelKind kind, const SSRUiNode *node) {
    Gel *g = calloc(1, sizeof(Gel));
    if (!g) return NULL;
    g->kind = kind;
    g->node = node;
    g->sx = g->sy = 1;
    g->layer = -1;
    g->depth = DEPTH_INHERIT;
    g->baked = mat_identity();
    g->alignX = 'C';
    g->alignY = 'M';
    g->font = 1;
    g->fontScaleX = g->fontScaleY = 1;
    g->italic = -0.25f;
    g->speed = 1;
    g->repeats = 1;
    return g;
}

static void free_doodads(Doodad *d) {
    while (d) {
        Doodad *next = d->next;
        free_doodads(d->chain);
        free_doodads(d->other);
        for (int i = 0; i < d->exCount; i++) ssr_ex_free(d->ex[i]);
        free(d);
        d = next;
    }
}

static void free_gel(Gel *g) {
    if (!g) return;
    for (int i = 0; i < g->childCount; i++) free_gel(g->children[i]);
    free(g->children);
    free_doodads(g->doodads);
    free_doodads(g->clickChain);
    free(g->shown);
    free(g->frames);
    for (int i = 0; i < g->varCount; i++) free(g->vars[i]);
    free(g->vars);
    free(g);
}

static void add_child(Gel *parent, Gel *child) {
    child->parent = parent;
    push_gel(&parent->children, &parent->childCount, &parent->childCapacity, child);
}

// The variables of a copy: each old variable of the copied gels and its new
// variable. A variable of a gel outside the copy stays the same.
static struct {
    SSRExVar **from, **to;
    int count, capacity;
} cloneVars;

static SSRExVar *remap_var(void *context, SSRExVar *old) {
    (void)context;
    for (int i = 0; i < cloneVars.count; i++) {
        if (cloneVars.from[i] == old) return cloneVars.to[i];
    }
    return old;
}

static void clone_vars(Gel *copy, const Gel *g) {
    copy->vars = NULL;
    copy->varCount = 0;
    if (!g->varCount) return;
    copy->vars = calloc((size_t)g->varCount, sizeof(SSRExVar *));
    if (!copy->vars) return;
    for (int i = 0; i < g->varCount; i++) {
        SSRExVar *v = malloc(sizeof(SSRExVar));
        if (!v) break;
        *v = *g->vars[i];
        copy->vars[copy->varCount++] = v;
        if (cloneVars.count == cloneVars.capacity) {
            int capacity = cloneVars.capacity ? cloneVars.capacity * 2 : 32;
            SSRExVar **from = realloc(cloneVars.from, (size_t)capacity * sizeof(SSRExVar *));
            if (!from) continue;
            cloneVars.from = from;
            SSRExVar **to = realloc(cloneVars.to, (size_t)capacity * sizeof(SSRExVar *));
            if (!to) continue;
            cloneVars.to = to;
            cloneVars.capacity = capacity;
        }
        cloneVars.from[cloneVars.count] = g->vars[i];
        cloneVars.to[cloneVars.count++] = v;
    }
}

static Doodad *clone_doodads(const Doodad *d) {
    Doodad *first = NULL, **tail = &first;
    for (; d; d = d->next) {
        Doodad *copy = malloc(sizeof(Doodad));
        if (!copy) break;
        *copy = *d;
        copy->next = NULL;
        copy->chain = clone_doodads(d->chain);
        copy->other = clone_doodads(d->other);
        for (int i = 0; i < d->exCount; i++) copy->ex[i] = ssr_ex_clone(d->ex[i], remap_var, NULL);
        *tail = copy;
        tail = &copy->next;
    }
    return first;
}

static Gel *clone_tree(const Gel *g) {
    Gel *copy = malloc(sizeof(Gel));
    if (!copy) return NULL;
    *copy = *g;
    copy->parent = NULL;
    copy->children = NULL;
    copy->childCount = copy->childCapacity = 0;
    // The variables come first, so that the doodads can find them.
    clone_vars(copy, g);
    copy->doodads = clone_doodads(g->doodads);
    copy->clickChain = clone_doodads(g->clickChain);
    if (g->shown) {
        copy->shown = malloc(strlen(g->shown) + 1);
        if (copy->shown) strcpy(copy->shown, g->shown);
    }
    if (g->frameCount) {
        copy->frames = malloc((size_t)g->frameCount * sizeof(char *));
        if (copy->frames) memcpy(copy->frames, g->frames, (size_t)g->frameCount * sizeof(char *));
    }
    for (int i = 0; i < g->childCount; i++) {
        Gel *child = clone_tree(g->children[i]);
        if (child) add_child(copy, child);
    }
    return copy;
}

// A deep copy of the gel and its children (InstantiateHierarchy). Each copy
// has its own variables, with the values of the model.
static Gel *clone_gel(const Gel *g) {
    cloneVars.count = 0;
    return clone_tree(g);
}

static void append_doodad(Doodad **list, Doodad *d) {
    while (*list) list = &(*list)->next;
    *list = d;
}

static Doodad *new_doodad(DoodadKind kind, const SSRUiNode *m) {
    Doodad *d = calloc(1, sizeof(Doodad));
    if (!d) return NULL;
    d->kind = kind;
    d->flags = F_ACTIVE;
    if (m) {
        // ".Kw" runs once. ":Kw" runs once and Press() can run it again.
        // "!Kw" stops after its branch.
        if (m->flags & SSR_UI_DOT) d->flags |= F_ONCE;
        if (m->flags & SSR_UI_REARM) d->flags |= F_REARM;
        if (m->flags & SSR_UI_BANG) d->flags |= F_BANG;
    }
    return d;
}

static Gel *find_ref(const SSRScene *s, const char *ref) {
    for (int i = s->refCount - 1; i >= 0; i--) {
        if (strcmp(s->refs[i]->ref, ref) == 0) return s->refs[i];
    }
    return NULL;
}

static Gel *find_animation(const SSRScene *s, const char *name) {
    for (int i = s->refCount - 1; i >= 0; i--) {
        if (s->refs[i]->kind == GEL_ANIMATION && strcmp(s->refs[i]->ref, name) == 0) return s->refs[i];
    }
    return NULL;
}

static int find_layer(const SSRScene *s, const char *name) {
    for (int i = 0; i < s->layerCount; i++) {
        if (strcmp(s->layers[i].name, name) == 0) return i;
    }
    return 0;
}

/* ------------------------------------------------------------------ parse */

static Mat origin(const SSRScene *s) {
    return s->origins[s->originCount - 1];
}

// Sets the letters of the panel of the top level: @L, @T, @R, @B, the size
// @W and @H, and the centre @C and @M.
static void set_panel_vars(SSRScene *s) {
    const float *p = s->panel[s->originCount - 1];
    s->vars['L'] = p[0];
    s->vars['T'] = p[1];
    s->vars['R'] = p[2];
    s->vars['B'] = p[3];
    s->vars['W'] = p[2] - p[0];
    s->vars['H'] = p[3] - p[1];
    s->vars['C'] = (p[0] + p[2]) * 0.5f;
    s->vars['M'] = (p[1] + p[3]) * 0.5f;
}

static void push_origin_matrix(SSRScene *s, Mat m, float l, float t, float r, float b) {
    if (s->originCount >= MAX_ORIGINS) return;
    int level = s->originCount++;
    s->origins[level] = m;
    s->panel[level][0] = l;
    s->panel[level][1] = t;
    s->panel[level][2] = r;
    s->panel[level][3] = b;
    set_panel_vars(s);
}

// PushOrigin:(x,y,w,h,angle). x can be Left, Right, or Centre, and y can be
// Top, Bottom, or Centre: then the place is on the screen, not from the
// origin. w and h are the size of the new panel, @W and @H by default. The
// children see a panel from (0, 0).
static void push_origin(SSRScene *s, const SSRUiNode *node, int ltrb) {
    const SSRSceneEnv *env = s->env;
    int absolute = 0;
    float x, y;
    const char *ax = arg(node, 0), *ay = arg(node, 1);
    if (is(ax, "Left")) {
        x = env->left;
        absolute = 1;
    } else if (is(ax, "Right")) {
        x = env->left + env->width;
        absolute = 1;
    } else if (is(ax, "Centre")) {
        x = env->left + env->width * 0.5f;
        absolute = 1;
    } else {
        x = (float)number(s, node, 0, 0);
    }
    if (is(ay, "Top")) {
        y = env->top;
        absolute = 1;
    } else if (is(ay, "Bottom")) {
        y = env->top + env->height;
        absolute = 1;
    } else if (is(ay, "Centre")) {
        y = env->top + env->height * 0.5f;
        absolute = 1;
    } else {
        y = (float)number(s, node, 1, 0);
    }
    float width = (float)s->vars['W'], height = (float)s->vars['H'];
    float pw = ltrb ? (float)number(s, node, 2, width + x) - x : (float)number(s, node, 2, width);
    float ph = ltrb ? (float)number(s, node, 3, height + y) - y : (float)number(s, node, 3, height);
    Mat local = mat_make(x, y, (float)number(s, node, 4, 0), 1, 1);
    push_origin_matrix(s, absolute ? local : mat_concat(origin(s), local), 0, 0, pw, ph);
}

// Places a new gel: (x, y, angle) is in the frame of the origin. The local
// transform is from the parent, as SetParent of the game makes it.
static void place_gel(SSRScene *s, Gel *g, Gel *parent, float x, float y, float angle) {
    Mat world = mat_concat(origin(s), mat_make(x, y, angle, 1, 1));
    g->baked = world;
    if (parent) {
        set_local(g, mat_concat(mat_inverse(parent->baked), world));
        add_child(parent, g);
    } else {
        set_local(g, world);
    }
}

static void bake_at_parse(Gel *g) {
    g->baked = g->parent ? mat_concat(g->parent->baked, local_matrix(g)) : local_matrix(g);
}

static void detach(SSRScene *s, Gel *g) {
    if (g->parent) {
        Gel *p = g->parent;
        for (int i = 0; i < p->childCount; i++) {
            if (p->children[i] == g) {
                memmove(&p->children[i], &p->children[i + 1], (size_t)(p->childCount - i - 1) * sizeof(Gel *));
                p->childCount--;
                break;
            }
        }
        g->parent = NULL;
    }
    set_local(g, g->baked);
    push_gel(&s->detached, &s->detachedCount, &s->detachedCapacity, g);
}

static void add_file(SSRScene *s, const char *name, const char *file) {
    ImageFile *grown = realloc(s->files, (size_t)(s->fileCount + 1) * sizeof(ImageFile));
    if (!grown) return;
    s->files = grown;
    s->files[s->fileCount].name = (char *)keep(s, name);
    s->files[s->fileCount].file = (char *)keep(s, file);
    s->fileCount++;
}

// "@x=expression" sets vars['x']. Only the first letter of the name
// counts, as in the game.
static void set_variable(SSRScene *s, const char *assignment) {
    if (assignment[0] != '@' || !assignment[1]) return;
    const char *equals = strchr(assignment, '=');
    if (!equals) return;
    SSRExprScope scope = {variable, tweak, s};
    s->vars[(unsigned char)assignment[1] & 127] = ssr_expr_eval(equals + 1, &scope, NULL);
}

static int image_default_size(SSRScene *s, const char *image, float *w, float *h) {
    const char *file = ssr_scene_image_file(s, image);
    return s->env->image_size && s->env->image_size(s->env->context, image, file, w, h) && *w > 0 && *h > 0;
}

// A size of 0 takes the size of the image, as c_Draw_Factory does.
static void default_size(SSRScene *s, const char *image, float *width, float *height) {
    if (*width != 0 && *height != 0) return;
    float iw, ih;
    if (!image_default_size(s, image, &iw, &ih)) return;
    if (*width == 0 && *height == 0) {
        *width = iw;
        *height = ih;
    } else if (*width == 0) {
        *width = iw * *height / ih;
    } else {
        *height = ih * *width / iw;
    }
}

static void ensure_colour(Gel *g) {
    if (!g->hasColour) {
        g->hasColour = 1;
        g->colour = colour_white();
    }
}

/* ------------------------------------------------------- expressions */

static const char *ex_localise(void *context, const char *tag) {
    SSRScene *s = context;
    return s->env && s->env->text ? s->env->text(s->env->context, tag) : NULL;
}

// The variable with the name, from the gel of the parse up through its
// parents. The newest declaration in a gel wins.
static SSRExVar *ex_find_var(void *context, const char *name) {
    SSRScene *s = context;
    for (Gel *g = s->parseGel; g; g = g->parent) {
        for (int i = g->varCount - 1; i >= 0; i--) {
            if (strcmp(g->vars[i]->name, name) == 0) return g->vars[i];
        }
    }
    return NULL;
}

static float ex_simple_var(void *context, char letter) {
    SSRScene *s = context;
    return (float)s->vars[(unsigned char)letter & 127];
}

static float ex_get(void *context, void *gel, SSRGelProperty property) {
    (void)context;
    Gel *g = gel;
    switch (property) {
    case SSR_GEL_VIS:
        return g->hidden ? 0.0f : 1.0f;
    case SSR_GEL_X:
        return g->x;
    case SSR_GEL_Y:
        return g->y;
    case SSR_GEL_W:
        return g->w;
    case SSR_GEL_H:
        return g->h;
    case SSR_GEL_HX:
        return g->hx;
    case SSR_GEL_HY:
        return g->hy;
    case SSR_GEL_GX:
        return g->baked.tx;
    case SSR_GEL_GY:
        return g->baked.ty;
    case SSR_GEL_ANGLE:
        return g->angle;
    case SSR_GEL_ALPHA:
        return g->hasColour ? g->colour.a : 1.0f;
    case SSR_GEL_SCALE:
    case SSR_GEL_SCALEX:
        return g->sx;
    case SSR_GEL_SCALEY:
        return g->sy;
    case SSR_GEL_DEPTH:
        return (float)g->depth;
    case SSR_GEL_ANIM_SPEED:
        return g->speed;
    case SSR_GEL_ANIM_TIME:
        return g->frame;
    case SSR_GEL_ANIM_NORMTIME:
        return g->animation && g->animation->length > 0 ? g->frame / g->animation->length : 0;
    case SSR_GEL_ANIM_PLAYING:
        return g->state != 0 ? 1.0f : 0.0f;
    default:
        return 0;
    }
}

static void ex_set(void *context, void *gel, SSRGelProperty property, float value) {
    (void)context;
    Gel *g = gel;
    switch (property) {
    case SSR_GEL_VIS:
        g->hidden = value == 0;
        break;
    case SSR_GEL_X:
        g->x = value;
        break;
    case SSR_GEL_Y:
        g->y = value;
        break;
    case SSR_GEL_W:
        g->w = value;
        break;
    case SSR_GEL_H:
        g->h = value;
        break;
    case SSR_GEL_HX:
        g->hx = value;
        break;
    case SSR_GEL_HY:
        g->hy = value;
        break;
    case SSR_GEL_ANGLE:
        g->angle = value;
        break;
    case SSR_GEL_ALPHA:
        ensure_colour(g);
        g->colour.a = value;
        break;
    case SSR_GEL_SCALE:
        g->sx = g->sy = value;
        break;
    case SSR_GEL_SCALEX:
        g->sx = value;
        break;
    case SSR_GEL_SCALEY:
        g->sy = value;
        break;
    case SSR_GEL_DEPTH:
        g->depth = (int)value;
        break;
    case SSR_GEL_ANIM_SPEED:
        g->speed = value;
        break;
    case SSR_GEL_ANIM_TIME:
        // The time and the speed go to the value and 0 (c_EX_Gel_AnimTime).
        g->frame = value;
        g->speed = 0;
        break;
    case SSR_GEL_ANIM_PLAYING:
        g->state = value != 0;
        break;
    default:
        break;
    }
}

static const char *ex_get_text(void *context, void *gel, SSRGelProperty property) {
    (void)context;
    Gel *g = gel;
    if (property == SSR_GEL_IMG) return g->image ? g->image : "";
    if (property == SSR_GEL_ANIM) return g->animName ? g->animName : "";
    return g->shown ? g->shown : g->text ? g->text : "";
}

static void set_shown(Gel *g, const char *text);
static void set_animation(Gel *anim, const char *name);

static void ex_set_text(void *context, void *gel, SSRGelProperty property, const char *text) {
    SSRScene *s = context;
    Gel *g = gel;
    if (property == SSR_GEL_IMG) {
        if (!g->image || strcmp(g->image, text) != 0) g->image = keep(s, text);
    } else if (property == SSR_GEL_ANIM) {
        if (!g->animName || strcmp(g->animName, text) != 0) set_animation(g, keep(s, text));
    } else {
        set_shown(g, text);
    }
}

static SSRExColour ex_get_colour(void *context, void *gel) {
    (void)context;
    Gel *g = gel;
    Colour c = g->hasColour ? g->colour : colour_white();
    return (SSRExColour){c.r, c.g, c.b, c.a, c.mr, c.mg, c.mb, c.ma};
}

static void ex_set_colour(void *context, void *gel, SSRGelProperty property, const char *slot, SSRExColour c,
                          int mask) {
    (void)context;
    (void)slot;
    (void)mask;
    Gel *g = gel;
    // The colours of the slots of a Spine gel and of the text shadow are
    // not in the remake yet.
    if (property != SSR_GEL_COLOUR) return;
    g->hasColour = 1;
    g->colour = (Colour){c.r, c.g, c.b, c.a, c.mr, c.mg, c.mb, c.ma};
}

// shell(Name) is the screen of the shell. The app reads the tweak Shell,Name.
static const char *ex_get_shell(void *context, const char *shell) {
    SSRScene *s = context;
    return s->env && s->env->tweaks ? ssr_tweaks_get(s->env->tweaks, "Shell", shell) : "";
}

static void ex_set_shell(void *context, const char *shell, const char *screen) {
    SSRScene *s = context;
    if (s->env && s->env->tweaks) ssr_tweaks_set(s->env->tweaks, "Shell", shell, screen);
}

static SSRExHost ex_host(SSRScene *s, Gel *g) {
    SSRExHost host = {0};
    host.tweaks = s->env ? s->env->tweaks : NULL;
    host.context = s;
    host.localise = ex_localise;
    host.find_var = ex_find_var;
    host.simple_var = ex_simple_var;
    host.gel = g;
    host.get = ex_get;
    host.set = ex_set;
    host.get_text = ex_get_text;
    host.set_text = ex_set_text;
    host.get_colour = ex_get_colour;
    host.set_colour = ex_set_colour;
    host.get_shell = ex_get_shell;
    host.set_shell = ex_set_shell;
    host.frameTime = s->frameDelta > 0 ? s->frameDelta : 1;
    host.touchX = s->touchX;
    host.touchY = s->touchY;
    host.touchDown = s->touchDown;
    host.touchHit = s->touchDown && !s->touchWasDown;
    host.touchReleased = !s->touchDown && s->touchWasDown;
    return host;
}

// Reads the phrase of the line into the items of the doodad.
static void parse_phrase(SSRScene *s, Gel *g, const SSRUiNode *m, Doodad *d) {
    char phrase[1024];
    snprintf(phrase, sizeof(phrase), "(%s)", m->raw ? m->raw : "");
    s->parseGel = g;
    SSRExHost host = ex_host(s, g);
    d->exCount = ssr_ex_parse(phrase, &host, d->ex, 3);
    s->parseGel = NULL;
}

// float(%a,%b), strvar(%c), and colvar(%d) make variables in the gel.
static void declare_vars(Gel *g, const SSRUiNode *m, SSRExVarKind kind) {
    for (int i = 0; i < m->argCount; i++) {
        if (m->args[i][0] != '%') continue;
        SSRExVar *v = calloc(1, sizeof(SSRExVar));
        if (!v) return;
        SSRExVar **grown = realloc(g->vars, (size_t)(g->varCount + 1) * sizeof(SSRExVar *));
        if (!grown) {
            free(v);
            return;
        }
        g->vars = grown;
        snprintf(v->name, sizeof(v->name), "%s", m->args[i]);
        v->kind = kind;
        v->c = (SSRExColour){255, 255, 255, 1, 0, 0, 0, 0};
        g->vars[g->varCount++] = v;
    }
}

static int is_ex_op(const char *n) {
    static const char *OPS[] = {"set", "add", "sub", "mul", "div", "fmod", "lerp", "frlerp", "collerp", "init"};
    for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
        if (strcmp(n, OPS[i]) == 0) return 1;
    }
    return 0;
}

// The rate arguments of the Converge doodads from index i: a number, Soft,
// or Instant, then the most rate and the acceleration.
static void converge_rates(SSRScene *s, const SSRUiNode *m, int i, Doodad *d) {
    float rate = 0.2f, maxRate = 0.2f, accel = 0;
    const char *a = arg(m, i);
    if (is(a, "Soft")) {
        rate = 0;
        accel = 0.05f;
    } else if (is(a, "Instant")) {
        rate = maxRate = 1;
    } else if (a[0]) {
        rate = (float)number(s, m, i, 0.2);
        maxRate = (float)number(s, m, i + 1, 0.2);
        accel = (float)number(s, m, i + 2, 0);
    }
    d->v[2] = rate;
    d->v[3] = maxRate;
    d->v[4] = accel;
}

// The easing arguments of the Tween doodads at index i: in and out.
static void tween_ease(SSRScene *s, const SSRUiNode *m, int i, Doodad *d) {
    d->v[6] = (float)number(s, m, i, 0);
    d->v[7] = (float)number(s, m, i + 1, d->v[6]);
}

static void build_children(SSRScene *s, const SSRUiNode *node, Gel *parent);
static Doodad *make_doodads(SSRScene *s, Gel *g, const SSRUiNode *const *mods, int count, int *show);

// Makes one doodad from a modifier, or changes the gel when the modifier
// works at the parse. Returns the doodad, or NULL.
static Doodad *make_doodad(SSRScene *s, Gel *g, const SSRUiNode *m, int *show) {
    const char *n = m->name;
    Doodad *d = NULL;
    if (is(n, "ShowIf") || is(n, "ShowIfNot") || is(n, "AndShowIf") || is(n, "OrShowIf")) {
        // One doodad tests all the conditions of the gel, in order.
        if (*show) return NULL;
        *show = 1;
        d = new_doodad(D_SHOW, NULL);
        if (d) d->node = g->node;
        return d;
    }
    if (is(n, "float") || is(n, "flt")) {
        declare_vars(g, m, SSR_VAR_FLOAT);
    } else if (is(n, "strvar") || is(n, "stv")) {
        declare_vars(g, m, SSR_VAR_STRING);
    } else if (is(n, "colvar") || is(n, "clv")) {
        declare_vars(g, m, SSR_VAR_COLOUR);
    } else if (is_ex_op(n)) {
        d = new_doodad(D_EX, m);
        if (!d) return NULL;
        d->text[0] = keep(s, n);
        parse_phrase(s, g, m, d);
        if (is(n, "init")) {
            // init(T,V) runs once, at the parse (c_InitHelper).
            if (d->exCount >= 2) {
                SSRExHost host = ex_host(s, g);
                ssr_ex_assign(d->ex[0], d->ex[1], &host);
            }
            free_doodads(d);
            return NULL;
        }
    } else if (is(n, "Handle")) {
        float hx = (float)number(s, m, 0, 0), hy = (float)number(s, m, 1, 0);
        // A handle of 1 or less is a part of the size.
        if (fabsf(hx) <= 1) hx *= g->w;
        if (fabsf(hy) <= 1) hy *= g->h;
        g->hx = hx;
        g->hy = hy;
    } else if (is(n, "Scale")) {
        g->sx = (float)number(s, m, 0, g->sx);
        g->sy = (float)number(s, m, 1, g->sy);
    } else if (is(n, "Angle")) {
        g->angle = (float)number(s, m, 0, g->angle);
    } else if (is(n, "Colour")) {
        g->hasColour = parse_colour(arg(m, 0), &g->colour);
    } else if (is(n, "Additive")) {
        g->blend = SSR_BLEND_ADD;
    } else if (is(n, "Gloss")) {
        g->blend = SSR_BLEND_GLOSS;
    } else if (is(n, "Layer")) {
        g->layer = find_layer(s, arg(m, 0));
    } else if (is(n, "Depth")) {
        g->depth = (int)number(s, m, 0, 0);
    } else if (is(n, "YDepth")) {
        g->yDepth = 1;
        g->yDepthOffset = (float)number(s, m, 0, 0);
    } else if (is(n, "FontSize")) {
        g->font = (int)number(s, m, 0, 1);
        if (g->font < 0 || g->font > 3) g->font = 1;
    } else if (is(n, "FontScale")) {
        g->fontScaleX = (float)number(s, m, 0, 1);
        g->fontScaleY = (float)number(s, m, 1, g->fontScaleX);
    } else if (is(n, "Italicise")) {
        g->italic = (float)number(s, m, 0, -0.25);
    } else if (is(n, "WrapX") && g->kind == GEL_TEXT) {
        g->wrap = 1;
    } else if (is(n, "Copies")) {
        g->columns = (int)number(s, m, 0, 0);
        g->rows = (int)number(s, m, 1, 0);
    } else if (is(n, "Spacing")) {
        g->stepX = (float)number(s, m, 0, g->w);
        g->stepY = (float)number(s, m, 1, g->h);
    } else if (is(n, "TileShear")) {
        g->shearX = (float)number(s, m, 0, 0);
        g->shearY = (float)number(s, m, 1, 0);
    } else if (is(n, "SpreadOver")) {
        g->first = (float)number(s, m, 0, g->first);
        g->last = (float)number(s, m, 1, g->last);
    } else if (is(n, "Repeat")) {
        g->repeats = (int)number(s, m, 0, 1);
    } else if (is(n, "Click")) {
        g->click = keep(s, m->argCount ? arg(m, 0) : "");
        for (int i = 0; i < m->childCount; i++) {
            if (is(m->children[i]->name, "PlaySnd")) {
                g->clickSound = keep(s, arg(m->children[i], 0));
                break;
            }
        }
        // The block runs once each time that the player clicks.
        int innerShow = 0;
        free_doodads(g->clickChain);
        g->clickChain = make_doodads(s, g, (const SSRUiNode *const *)m->children, m->childCount, &innerShow);
    } else if (is(n, "PlaySnd")) {
        d = new_doodad(D_PLAY_SOUND, m);
        if (d) d->text[0] = keep(s, arg(m, 0));
    } else if (is(n, "Readout") || is(n, "ReadoutLoc")) {
        // The value of a tweak as text: prefix, number, suffix (c_Readout).
        d = new_doodad(D_READOUT, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
        // The prefix keeps its spaces, as the game reads it.
        char prefix[64] = "";
        const char *at = m->raw;
        for (int comma = 0; at && *at && comma < 2; at++) {
            if (*at == ',') comma++;
        }
        if (at && m->argCount > 2) snprintf(prefix, sizeof(prefix), "%.*s", (int)strcspn(at, ","), at);
        // The suffix comes after the number. ReadoutLoc reads both from the
        // language. The two go in one text: prefix, 0x1f, suffix.
        char suffix[64];
        snprintf(suffix, sizeof(suffix), "%s", arg(m, 3));
        if (is(n, "ReadoutLoc") && s->env && s->env->text) {
            const char *p = prefix[0] ? s->env->text(s->env->context, prefix) : "";
            const char *q = suffix[0] ? s->env->text(s->env->context, suffix) : "";
            snprintf(prefix, sizeof(prefix), "%s", p ? p : "");
            snprintf(suffix, sizeof(suffix), "%s", q ? q : "");
        }
        char both[160];
        snprintf(both, sizeof(both), "%s\x1f%s", prefix, suffix);
        d->text[2] = keep(s, both);
        d->v[0] = (float)number(s, m, 5, 0);
        d->v[1] = 1.8347578e11f;
    } else if (is(n, "StringReadout")) {
        d = new_doodad(D_STRING_READOUT, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
    } else if (strncmp(n, "GS_", 3) == 0) {
        // A text that the logic of the game gives, by its name.
        d = new_doodad(D_PROVIDER, m);
        if (d) d->text[0] = keep(s, n);
    } else if (is(n, "Selector")) {
        d = new_doodad(D_SELECTOR, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
    } else if (is(n, "IncrementTweak")) {
        d = new_doodad(D_INCREMENT_TWEAK, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
        d->v[0] = (float)number(s, m, 2, 0);
        d->v[1] = (float)number(s, m, 3, 0);
    } else if (is(n, "ShowShell")) {
        // ShowShell(Name,1): the shell, for example the ticker, shows. The
        // app reads the tweak Shell,Name.
        d = new_doodad(D_SHOW_SHELL, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->v[0] = (float)number(s, m, 1, 0);
    } else if (is(n, "ScrollingList")) {
        // ScrollingList(Cat,Scroll,height,Cat,Velocity) (c_ScrollingList).
        d = new_doodad(D_SCROLLING_LIST, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
        d->v[0] = (float)number(s, m, 2, 0);
        char velocity[160];
        snprintf(velocity, sizeof(velocity), "%s\x1f%s", arg(m, 3), arg(m, 4));
        d->text[2] = keep(s, velocity);
    } else if (is(n, "SwipePanel")) {
        // SwipePanel(Cat,Scroll,Cat,Velocity): a drag moves the scroll
        // (c_SwipePanel).
        d = new_doodad(D_SWIPE_PANEL, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
        char velocity[160];
        snprintf(velocity, sizeof(velocity), "%s\x1f%s", arg(m, 2), arg(m, 3));
        d->text[2] = keep(s, velocity);
    } else if (is(n, "CloneTweak")) {
        d = new_doodad(D_CLONE_TWEAK, m);
        if (!d) return NULL;
        char source[160];
        snprintf(source, sizeof(source), "%s\x1f%s", arg(m, 0), arg(m, 1));
        d->text[0] = keep(s, source);
        d->text[1] = keep(s, arg(m, 2));
        d->text[2] = keep(s, arg(m, 3));
    } else if (is(n, "DoIf") || is(n, "DoIfNot")) {
        // c_DoIf: a number tweak from min to max, or a text tweak that is the
        // same as the text.
        d = new_doodad(D_DOIF, m);
        if (!d) return NULL;
        d->text[0] = keep(s, arg(m, 0));
        d->text[1] = keep(s, arg(m, 1));
        d->text[2] = keep(s, arg(m, 2));
        d->v[0] = (float)number(s, m, 2, 0);
        d->v[1] = (float)number(s, m, 3, d->v[0]);
        d->v[2] = is(n, "DoIf");
        int innerShow = 0;
        d->chain = make_doodads(s, g, (const SSRUiNode *const *)m->children, m->childCount, &innerShow);
    } else if (is(n, "if")) {
        d = new_doodad(D_IF, m);
        if (!d) return NULL;
        parse_phrase(s, g, m, d);
        int innerShow = 0;
        d->chain = make_doodads(s, g, (const SSRUiNode *const *)m->children, m->childCount, &innerShow);
    } else if (is(n, "SetTweak")) {
        d = new_doodad(D_SET_TWEAK, m);
        if (d) {
            d->text[0] = keep(s, arg(m, 0));
            d->text[1] = keep(s, arg(m, 1));
            d->text[2] = keep(s, arg(m, 2));
        }
    } else if (is(n, "ColReadout")) {
        // The colour of the gel is the colour of the tweak, each frame.
        d = new_doodad(D_COL_READOUT, m);
        if (d) {
            d->text[0] = keep(s, arg(m, 0));
            d->text[1] = keep(s, arg(m, 1));
        }
        ensure_colour(g);
    } else if (is(n, "ConvergeTo") || is(n, "ConvergeFrom")) {
        d = new_doodad(D_CONVERGE_POS, m);
        if (!d) return NULL;
        d->v[0] = g->x;
        d->v[1] = g->y;
        if (is(n, "ConvergeTo")) {
            d->v[0] += (float)number(s, m, 0, 0);
            d->v[1] += (float)number(s, m, 1, 0);
        } else {
            // The gel starts at the point in the frame of the origin.
            float wx, wy;
            mat_apply(origin(s), (float)number(s, m, 0, 0), (float)number(s, m, 1, 0), &wx, &wy);
            Mat parent = g->parent ? g->parent->baked : mat_identity();
            mat_apply(mat_inverse(parent), wx, wy, &g->x, &g->y);
            bake_at_parse(g);
        }
        converge_rates(s, m, 2, d);
    } else if (is(n, "ConvergeScaleTo") || is(n, "ConvergeScaleFrom")) {
        d = new_doodad(D_CONVERGE_SCALE, m);
        if (!d) return NULL;
        float x = (float)number(s, m, 0, 1), y = (float)number(s, m, 1, x);
        if (is(n, "ConvergeScaleTo")) {
            d->v[0] = x;
            d->v[1] = y;
        } else {
            d->v[0] = g->sx;
            d->v[1] = g->sy;
            g->sx = x;
            g->sy = y;
        }
        converge_rates(s, m, 2, d);
    } else if (is(n, "ConvergeSizeTo") || is(n, "ConvergeSizeFrom")) {
        d = new_doodad(D_CONVERGE_SIZE, m);
        if (!d) return NULL;
        float x = (float)number(s, m, 0, g->w), y = (float)number(s, m, 1, g->h);
        if (is(n, "ConvergeSizeTo")) {
            d->v[0] = x;
            d->v[1] = y;
        } else {
            d->v[0] = g->w;
            d->v[1] = g->h;
            g->w = x;
            g->h = y;
        }
        converge_rates(s, m, 2, d);
    } else if (is(n, "ConvergeCol")) {
        d = new_doodad(D_CONVERGE_COL, m);
        if (!d) return NULL;
        ensure_colour(g);
        if (!parse_colour(arg(m, 0), &d->to)) d->to = colour_white();
        d->v[2] = (float)number(s, m, 1, 0.2);
    } else if (is(n, "Delay")) {
        d = new_doodad(D_DELAY, m);
        if (d) d->v[0] = (float)number(s, m, 0, 0) * 1000;
    } else if (is(n, "Press") || is(n, "NoPress")) {
        d = new_doodad(is(n, "Press") ? D_PRESS : D_NOPRESS, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0);
        int innerShow = 0;
        d->chain = make_doodads(s, g, (const SSRUiNode *const *)m->children, m->childCount, &innerShow);
    } else if (is(n, "Mover")) {
        d = new_doodad(D_MOVER, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0);
        d->v[1] = (float)number(s, m, 1, 0);
        d->v[2] = (float)number(s, m, 2, 1);
        d->v[3] = (float)number(s, m, 3, 0);
        d->v[4] = (float)number(s, m, 4, 0);
    } else if (is(n, "Spinner")) {
        d = new_doodad(D_SPINNER, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0);
        d->v[1] = (float)number(s, m, 1, 1);
        if (m->argCount >= 4) {
            d->text[0] = keep(s, arg(m, 2));
            d->text[1] = keep(s, arg(m, 3));
        }
    } else if ((is(n, "WrapX") || is(n, "WrapY")) && g->kind != GEL_TEXT) {
        d = new_doodad(is(n, "WrapX") ? D_WRAP_X : D_WRAP_Y, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0);
        d->v[1] = (float)number(s, m, 1, 0);
    } else if (is(n, "Fader")) {
        d = new_doodad(D_FADER, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0) * 1000;
        d->from = g->hasColour ? g->colour : colour_white();
        parse_colour("#FFFFFF00", &d->to);
        if (m->argCount >= 3) {
            parse_colour(arg(m, 1), &d->from);
            parse_colour(arg(m, 2), &d->to);
        } else if (m->argCount == 2) {
            parse_colour(arg(m, 1), &d->to);
        }
    } else if (is(n, "Lifetime")) {
        d = new_doodad(D_LIFETIME, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0) * 1000;
        d->v[1] = (float)s->gameMS + d->v[0];
    } else if (is(n, "TweenAngle")) {
        d = new_doodad(D_TWEEN_ANGLE, m);
        if (!d) return NULL;
        d->v[0] = g->angle;
        d->v[1] = (float)number(s, m, 0, 0);
        tween_ease(s, m, 1, d);
    } else if (is(n, "TweenSize")) {
        d = new_doodad(D_TWEEN_SIZE, m);
        if (!d) return NULL;
        d->v[0] = g->w;
        d->v[1] = g->h;
        d->v[2] = (float)number(s, m, 0, g->w);
        d->v[3] = (float)number(s, m, 1, g->h);
        float hxp = (float)number(s, m, 2, 0), hyp = (float)number(s, m, 3, 0);
        // A handle of more than 1 is in pixels of the first size.
        if (fabsf(hxp) > 1 && g->w != 0) hxp /= g->w;
        if (fabsf(hyp) > 1 && g->h != 0) hyp /= g->h;
        d->v[4] = hxp;
        d->v[5] = hyp;
        tween_ease(s, m, 4, d);
    } else if (is(n, "TweenCol")) {
        d = new_doodad(D_TWEEN_COL, m);
        if (!d) return NULL;
        ensure_colour(g);
        d->from = g->colour;
        if (!parse_colour(arg(m, 0), &d->to)) d->to = d->from;
        tween_ease(s, m, 1, d);
    } else if (is(n, "TweenAlpha")) {
        d = new_doodad(D_TWEEN_ALPHA, m);
        if (!d) return NULL;
        ensure_colour(g);
        d->v[0] = g->colour.a;
        d->v[1] = (float)number(s, m, 0, 1);
        tween_ease(s, m, 1, d);
    } else if (is(n, "TweenScale")) {
        d = new_doodad(D_TWEEN_SCALE, m);
        if (!d) return NULL;
        d->v[0] = g->sx;
        d->v[1] = g->sy;
        d->v[2] = (float)number(s, m, 0, 1);
        d->v[3] = (float)number(s, m, 1, d->v[2]);
        tween_ease(s, m, 2, d);
    } else if (is(n, "TweenPos")) {
        d = new_doodad(D_TWEEN_POS, m);
        if (!d) return NULL;
        d->v[0] = g->x;
        d->v[1] = g->y;
        float wx, wy;
        mat_apply(origin(s), (float)number(s, m, 0, 0), (float)number(s, m, 1, 0), &wx, &wy);
        Mat parent = g->parent ? g->parent->baked : mat_identity();
        mat_apply(mat_inverse(parent), wx, wy, &d->v[2], &d->v[3]);
        tween_ease(s, m, 2, d);
    } else if (is(n, "Shine")) {
        d = new_doodad(D_SHINE, m);
        if (!d) return NULL;
        d->v[0] = (float)number(s, m, 0, 0);
        d->v[1] = (float)number(s, m, 1, 0);
        d->v[2] = -1;
        ensure_colour(g);
    } else if (is(n, "PlayAnim") || is(n, "PlayAnimOn")) {
        d = new_doodad(D_PLAY_ANIM, m);
        if (!d) return NULL;
        int on = is(n, "PlayAnimOn");
        d->text[0] = on ? keep(s, arg(m, 0)) : NULL;
        d->text[1] = keep(s, arg(m, on ? 1 : 0));
    } else if (is(n, "EmitPlacer")) {
        d = new_doodad(D_EMIT_PLACER, NULL);
    } else if (is(n, "EmitMover")) {
        d = new_doodad(D_EMIT_MOVER, NULL);
    } else if (is(n, "EmitSpinner")) {
        d = new_doodad(D_EMIT_SPINNER, NULL);
    } else if (is(n, "EmitLifetime")) {
        d = new_doodad(D_EMIT_LIFETIME, NULL);
    } else if (is(n, "EmitAnimStart")) {
        d = new_doodad(D_EMIT_ANIM_START, NULL);
    } else if (is(n, "EmitSize")) {
        d = new_doodad(D_EMIT_SIZE, NULL);
        if (d) d->v[0] = m->argCount > 0;
    }
    return d;
}

// "else < >" gives its block to the if() before it, as the chain for a
// false condition. Returns 1 when the modifier is an else.
static int attach_else(SSRScene *s, Gel *g, Doodad *list, const SSRUiNode *m) {
    if (!is(m->name, "else")) return 0;
    Doodad *last = list;
    while (last && last->next) last = last->next;
    if (last && last->kind == D_IF && !last->other) {
        int innerShow = 0;
        last->other = make_doodads(s, g, (const SSRUiNode *const *)m->children, m->childCount, &innerShow);
    }
    return 1;
}

static Doodad *make_doodads(SSRScene *s, Gel *g, const SSRUiNode *const *mods, int count, int *show) {
    Doodad *list = NULL;
    for (int i = 0; i < count; i++) {
        if (attach_else(s, g, list, mods[i])) continue;
        Doodad *d = make_doodad(s, g, mods[i], show);
        if (d && mods[i]->ref) d->ref = keep(s, mods[i]->ref);
        if (d) append_doodad(&list, d);
    }
    return list;
}

static void register_ref(SSRScene *s, Gel *g, const char *ref) {
    if (!ref) return;
    g->ref = keep(s, ref);
    push_gel(&s->refs, &s->refCount, &s->refCapacity, g);
}

// Instance:(Ref,x,y,angle[,Hide]): copies each gel with the ref into the
// parent, at the point in the frame of the origin (c_InstanceHelper).
static void instance(SSRScene *s, const SSRUiNode *node, Gel *parent) {
    const char *ref = arg(node, 0);
    float x = (float)number(s, node, 1, 0), y = (float)number(s, node, 2, 0);
    float angle = (float)number(s, node, 3, 0);
    int hide = is(arg(node, 4), "Hide");
    int count = s->refCount;
    for (int i = 0; i < count; i++) {
        if (strcmp(s->refs[i]->ref, ref) != 0 || s->refs[i]->kind == GEL_ANIMATION) continue;
        Gel *copy = clone_gel(s->refs[i]);
        if (!copy) continue;
        copy->hidden = hide;
        copy->sx = copy->sy = 1;
        place_gel(s, copy, parent, x, y, angle);
        for (int k = 0; k < copy->childCount; k++) {
            // The children keep their place in the copy.
            Gel *child = copy->children[k];
            child->baked = mat_concat(copy->baked, local_matrix(child));
        }
    }
}

// Makes the gel of an element line and its children.
static void build_node(SSRScene *s, const SSRUiNode *node, Gel *parent) {
    const char *name = node->name;
    if (is(name, "PopOrigin")) {
        if (s->originCount > 1) s->originCount--;
        set_panel_vars(s);
        return;
    }
    if (is(name, "Set") && node->argCount > 0) {
        set_variable(s, arg(node, 0));
        return;
    }
    if (is(name, "Img") && node->argCount >= 2) {
        add_file(s, arg(node, 0), arg(node, 1));
        return;
    }
    if (!(node->flags & SSR_UI_ELEMENT) && !is(name, "Group")) return;

    if (is(name, "UnsortedLayer") || is(name, "SortedLayer")) {
        int at = s->layerCount;
        for (int i = 0; i < s->layerCount; i++) {
            if (strcmp(s->layers[i].name, arg(node, 0)) == 0) at = i;
        }
        if (at >= MAX_LAYERS) return;
        Layer *layer = &s->layers[at];
        snprintf(layer->name, sizeof(layer->name), "%s", arg(node, 0));
        layer->depth = (int)number(s, node, 1, 0);
        layer->sorted = is(name, "SortedLayer");
        layer->highToLow = 0;
        for (int i = 2; i < node->argCount; i++) {
            if (is(arg(node, i), "HTL") || is(arg(node, i), "\"HTL\"")) layer->highToLow = 1;
        }
        if (at == s->layerCount) s->layerCount++;
        return;
    }
    if (is(name, "PushOrigin") || is(name, "PushOriginLTRB")) {
        push_origin(s, node, strstr(name, "LTRB") != NULL);
        build_children(s, node, parent);
        return;
    }
    if (is(name, "Instance")) {
        instance(s, node, parent);
        return;
    }

    int ltrb = strstr(name, "LTRB") != NULL;
    Gel *g = NULL;
    // The gel that takes the modifiers and the children. A Sequence gives
    // them to its draw.
    Gel *target = NULL;
    if (is(name, "Draw") || is(name, "DrawLTRB") || is(name, "Tile") || is(name, "DrawFixedScale") ||
        is(name, "DrawFixedScaleLTRB")) {
        g = new_gel(GEL_DRAW, node);
        if (!g) return;
        g->image = keep(s, arg(node, 0));
        float x = (float)number(s, node, 1, 0), y = (float)number(s, node, 2, 0);
        float width = (float)number(s, node, 3, 0), height = (float)number(s, node, 4, 0);
        if (ltrb) {
            width -= x;
            height -= y;
        }
        default_size(s, g->image, &width, &height);
        g->w = width;
        g->h = height;
        g->hasColour = parse_colour(arg(node, 6), &g->colour);
        g->tiled = is(name, "Tile");
        g->stepX = width;
        g->stepY = height;
        place_gel(s, g, parent, x, y, (float)number(s, node, 5, 0));
    } else if (is(name, "Text") || is(name, "TextLoc") || is(name, "TextBox")) {
        g = new_gel(GEL_TEXT, node);
        if (!g) return;
        g->text = keep(s, arg(node, 0));
        // TextLoc and TextBox take a language tag.
        g->localise = !is(name, "Text");
        g->w = (float)number(s, node, 3, 0);
        g->h = (float)number(s, node, 4, 0);
        if (arg(node, 5)[0]) g->alignX = (char)toupper((unsigned char)arg(node, 5)[0]);
        if (arg(node, 6)[0]) g->alignY = (char)toupper((unsigned char)arg(node, 6)[0]);
        g->hasColour = parse_colour(arg(node, 7), &g->colour);
        g->wrap = is(name, "TextBox");
        place_gel(s, g, parent, (float)number(s, node, 1, 0), (float)number(s, node, 2, 0), 0);
    } else if (is(name, "Rect") || is(name, "RectLTRB") || is(name, "Clip") || is(name, "ClipLTRB")) {
        g = new_gel(name[0] == 'R' ? GEL_RECT : GEL_CLIP, node);
        if (!g) return;
        float x = (float)number(s, node, 0, 0), y = (float)number(s, node, 1, 0);
        g->w = (float)number(s, node, 2, 0);
        g->h = (float)number(s, node, 3, 0);
        if (ltrb) {
            g->w -= x;
            g->h -= y;
        }
        if (g->kind == GEL_RECT) g->hasColour = parse_colour(arg(node, 5), &g->colour);
        place_gel(s, g, parent, x, y, (float)number(s, node, 4, 0));
    } else if (is(name, "Crowd") || is(name, "BottomCrowd")) {
        g = new_gel(GEL_CROWD, node);
        if (!g) return;
        int bottom = name[0] == 'B';
        g->p[0] = (float)number(s, node, 2, 0);
        g->p[1] = (float)number(s, node, 3, 0);
        g->p[2] = bottom;
        g->p[3] = !bottom && node->argCount >= 5 && number(s, node, 4, 1) == 0;
        place_gel(s, g, parent, (float)number(s, node, 0, 0), (float)number(s, node, 1, 0), 0);
        g->w = g->p[0];
        g->h = g->p[1];
        g->stepX = g->p[0];
        g->stepY = g->p[1];
    } else if (is(name, "Spine")) {
        g = new_gel(GEL_SPINE, node);
        if (!g) return;
        g->name = keep(s, arg(node, 0));
        g->animationName = keep(s, arg(node, 4));
        g->p[0] = is(arg(node, 5), "Loop");
        g->p[1] = (float)number(s, node, 6, 0);
        place_gel(s, g, parent, (float)number(s, node, 1, 0), (float)number(s, node, 2, 0),
                  (float)number(s, node, 3, 0));
    } else if (is(name, "AnimInstance")) {
        g = new_gel(GEL_ANIM, node);
        if (!g) return;
        g->animName = keep(s, arg(node, 2));
        g->frame = (float)number(s, node, 3, 0);
        g->speed = (float)number(s, node, 4, 1);
        g->state = number(s, node, 5, 1) != 0 ? 1 : 2;
        place_gel(s, g, parent, (float)number(s, node, 0, 0), (float)number(s, node, 1, 0), 0);
    } else if (is(name, "Animation")) {
        g = new_gel(GEL_ANIMATION, node);
        if (!g) return;
        g->length = (float)number(s, node, 0, 1);
        char mode = (char)toupper((unsigned char)arg(node, 1)[0]);
        g->mode = mode == 'L' || mode == 'C' ? mode : 'O';
        g->loopStart = (float)number(s, node, 2, 0);
        s->lastAnimationLength = g->length;
        // An animation has no parent. Only an AnimInstance draws it.
        place_gel(s, g, NULL, 0, 0, 0);
        push_gel(&s->detached, &s->detachedCount, &s->detachedCapacity, g);
    } else if (is(name, "Sheet") || is(name, "SHT")) {
        // Sheet:(PosX,PosY,PosZ,RotX,RotY,RotZ,FOV,ScaleX,ScaleY,ScaleZ,RTT)
        // (c_Sheet_Factory). The model matrix is T * Yaw(RotY) * Pitch(RotX) *
        // Roll(RotZ) * S. The sheet has no place of its own on the screen,
        // so its children keep their places in the layout.
        g = new_gel(GEL_SHEET, node);
        if (!g) return;
        float pos[3], rot[3], scale[3];
        for (int i = 0; i < 3; i++) {
            pos[i] = (float)number(s, node, i, 0);
            rot[i] = (float)number(s, node, 3 + i, 0) * PI_F / 180;
            scale[i] = (float)number(s, node, 7 + i, 1);
        }
        float fov = (float)number(s, node, 6, 0);
        float cx = cosf(rot[0]), sx = sinf(rot[0]), cy = cosf(rot[1]), sy = sinf(rot[1]);
        float cz = cosf(rot[2]), sz = sinf(rot[2]);
        // Pitch * Roll, then Yaw on the left. Yaw(a) is the Y rotation by -a.
        float pr[9] = {cz, -sz, 0, cx * sz, cx * cz, -sx, sx * sz, sx * cz, cx};
        float yaw[9] = {cy, 0, -sy, 0, 1, 0, sy, 0, cy};
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                float v = 0;
                for (int k = 0; k < 3; k++) v += yaw[r * 3 + k] * pr[k * 3 + c];
                g->trs[r * 4 + c] = v * scale[c];
            }
            g->trs[r * 4 + 3] = pos[r];
        }
        g->focal = fov > 0 ? 320.0f / tanf(fov / 2 * PI_F / 180) : 0;
        g->rtt = arg(node, 10)[0] != 0;
        place_gel(s, g, parent, 0, 0, 0);
        // c_Gel3D.p_Bake gives identity: the children keep their places.
        set_local(g, mat_concat(mat_inverse(parent ? parent->baked : mat_identity()), mat_identity()));
        g->baked = mat_identity();
    } else if (is(name, "During")) {
        g = new_gel(GEL_DURING, node);
        if (!g) return;
        g->first = (float)number(s, node, 0, 0);
        g->last = (float)number(s, node, 1, g->first);
        g->tfirst = g->first;
        g->tlength = g->last - g->first;
        const char *f = arg(node, 2);
        if (strchr(f, 'o')) g->tlength += 1;
        if (strchr(f, 'i')) {
            g->tfirst -= 1;
            g->tlength += 1;
        }
        place_gel(s, g, parent, 0, 0, 0);
    } else if (is(name, "Tween")) {
        g = new_gel(GEL_TWEEN, node);
        if (!g) return;
        g->tweenFrom = (float)number(s, node, 0, 0);
        g->tweenTo = (float)number(s, node, 1, 1);
        place_gel(s, g, parent, 0, 0, 0);
    } else if (is(name, "Sequence")) {
        g = new_gel(GEL_SEQUENCE, node);
        if (!g) return;
        const char *pattern = arg(node, 0);
        const char *hash = strchr(pattern, '#');
        int tokens = node->argCount - 1;
        g->frames = tokens > 0 ? calloc((size_t)tokens, sizeof(char *)) : NULL;
        for (int i = 0; i < tokens && g->frames && hash; i++) {
            char image[256];
            snprintf(image, sizeof(image), "%.*s%s%s", (int)(hash - pattern), pattern, arg(node, i + 1), hash + 1);
            g->frames[g->frameCount++] = keep(s, image);
        }
        g->first = 0;
        g->last = s->lastAnimationLength - 1;
        place_gel(s, g, parent, 0, 0, 0);
        Gel *draw = new_gel(GEL_DRAW, node);
        if (draw) {
            draw->image = g->frameCount ? g->frames[0] : "";
            default_size(s, draw->image, &draw->w, &draw->h);
            place_gel(s, draw, g, 0, 0, 0);
            target = draw;
        }
    } else if (is(name, "Embed") || is(name, "DrawEmbedded")) {
        g = new_gel(is(name, "Embed") ? GEL_EMBED : GEL_DRAW_EMBEDDED, node);
        if (!g) return;
        g->name = keep(s, arg(node, 0));
        place_gel(s, g, parent, 0, 0, 0);
    } else if (is(name, "Emitter")) {
        g = new_gel(GEL_EMITTER, node);
        if (!g) return;
        g->category = keep(s, arg(node, 0));
        g->templateRef = keep(s, arg(node, 1));
        // An emitter has no parent. Instance: places it.
        place_gel(s, g, NULL, 0, 0, 0);
        push_gel(&s->detached, &s->detachedCount, &s->detachedCapacity, g);
    } else {
        // Group:, and the elements that the scene does not draw, hold
        // their children in place.
        g = new_gel(GEL_GROUP, node);
        if (!g) return;
        place_gel(s, g, parent, 0, 0, 0);
    }
    if (!target) target = g;
    if (node->flags & SSR_UI_HIDE) target->hidden = 1;
    register_ref(s, g, node->ref);
    if (target != g && node->ref) target->ref = g->ref;

    int show = 0, isTemplate = 0;
    // The modifiers before the block come first, then the children, then
    // the modifiers after the block, in the order of the file.
    int before = (node->flags & SSR_UI_BLOCK) ? node->blockAt : node->modifierCount;
    for (int i = 0; i < before; i++) {
        const SSRUiNode *m = node->modifiers[i];
        if (is(m->name, "Template")) {
            isTemplate = 1;
            continue;
        }
        if (attach_else(s, target, target->doodads, m)) continue;
        Doodad *d = make_doodad(s, target, m, &show);
        if (d && m->ref) d->ref = keep(s, m->ref);
        if (d) append_doodad(&target->doodads, d);
        if (g->kind == GEL_SEQUENCE && (is(m->name, "SpreadOver") || is(m->name, "Repeat"))) {
            g->first = target->first;
            g->last = target->last;
            g->repeats = target->repeats;
        }
    }
    bake_at_parse(target);

    if (node->flags & SSR_UI_BOXED) {
        // "{{": the children are in the box of the gel (c_ParseNest).
        Mat local = mat_concat(mat_inverse(origin(s)), target->baked);
        float si = mat_scale_x(local), sj = mat_scale_y(local);
        Mat at = mat_make(local.tx, local.ty, mat_angle(local), 1, 1);
        push_origin_matrix(s, mat_concat(origin(s), at), -target->hx * si, -target->hy * sj,
                           (target->w - target->hx) * si, (target->h - target->hy) * sj);
        build_children(s, node, target);
        if (s->originCount > 1) s->originCount--;
        set_panel_vars(s);
    } else {
        build_children(s, node, target);
    }
    for (int i = before; i < node->modifierCount; i++) {
        const SSRUiNode *m = node->modifiers[i];
        if (is(m->name, "Template")) {
            isTemplate = 1;
            continue;
        }
        if (attach_else(s, target, target->doodads, m)) continue;
        Doodad *d = make_doodad(s, target, m, &show);
        if (d && m->ref) d->ref = keep(s, m->ref);
        if (d) append_doodad(&target->doodads, d);
    }
    bake_at_parse(target);
    // "} Template": the gel leaves the tree. Instance: and Emitter: copy it.
    if (isTemplate) detach(s, g);
}

static void build_children(SSRScene *s, const SSRUiNode *node, Gel *parent) {
    for (int i = 0; i < node->childCount; i++) build_node(s, node->children[i], parent);
}

SSRScene *ssr_scene_open(const SSRUiTree *tree, const SSRSceneEnv *env) {
    if (!env) return NULL;
    SSRScene *s = calloc(1, sizeof(SSRScene));
    if (!s) return NULL;
    s->env = env;
    s->random.seed = SSR_RANDOM_START;
    s->root = new_gel(GEL_GROUP, NULL);
    snprintf(s->layers[0].name, sizeof(s->layers[0].name), "Default");
    s->layerCount = 1;
    s->originCount = 1;
    s->origins[0] = mat_identity();
    s->panel[0][0] = env->left;
    s->panel[0][1] = env->top;
    s->panel[0][2] = env->left + env->width;
    s->panel[0][3] = env->top + env->height;
    set_panel_vars(s);
    const SSRUiNode *root = tree ? ssr_ui_root(tree) : NULL;
    if (root && s->root) build_children(s, root, s->root);
    s->env = NULL;
    return s;
}

/* ------------------------------------------------------------------ frame */

static SSRSceneItem *add_item(SSRScene *s, SSRItemKind kind, const Gel *g, int layer, int depth) {
    if (s->count == s->capacity) {
        int capacity = s->capacity ? s->capacity * 2 : 256;
        SSRSceneItem *grown = realloc(s->items, (size_t)capacity * sizeof(SSRSceneItem));
        if (!grown) return NULL;
        s->items = grown;
        SortKey *keys = realloc(s->keys, (size_t)capacity * sizeof(SortKey));
        if (!keys) return NULL;
        s->keys = keys;
        s->capacity = capacity;
    }
    s->keys[s->count].depth = depth;
    s->keys[s->count].order = s->order++;
    SSRSceneItem *item = &s->items[s->count++];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->colour = 0xFFFFFFFF;
    item->layer = layer;
    item->ref = g ? g->ref : NULL;
    item->alignX = 'C';
    item->alignY = 'M';
    item->font = 1;
    item->fontScaleX = item->fontScaleY = 1;
    item->italic = -0.25f;
    if (g) {
        item->click = g->click;
        item->clickSound = g->clickSound;
        item->owner = g;
    }
    item->cels = s->cels;
    // The item keeps its clip, so that the sort of the layers cannot lose it.
    if (s->clipCount > 0) {
        item->clipped = 1;
        item->clipX = s->clips[s->clipCount - 1].x;
        item->clipY = s->clips[s->clipCount - 1].y;
        item->clipW = s->clips[s->clipCount - 1].w;
        item->clipH = s->clips[s->clipCount - 1].h;
    }
    return item;
}

// Sets the box of the item from the gel: the corners on the screen, and
// the box before the rotation, with its pivot at the place of the gel.
static void set_box(SSRSceneItem *item, const Gel *g, Mat m, float ox, float oy) {
    float sx = mat_scale_x(m), sy = mat_scale_y(m);
    float x0 = ox - g->hx, y0 = oy - g->hy, x1 = x0 + g->w, y1 = y0 + g->h;
    mat_apply(m, x0, y0, &item->corners[0], &item->corners[1]);
    mat_apply(m, x1, y0, &item->corners[2], &item->corners[3]);
    mat_apply(m, x1, y1, &item->corners[4], &item->corners[5]);
    mat_apply(m, x0, y1, &item->corners[6], &item->corners[7]);
    item->quad = 1;
    float px, py;
    mat_apply(m, ox, oy, &px, &py);
    item->pivotX = px;
    item->pivotY = py;
    item->x = px - g->hx * sx;
    item->y = py - g->hy * sy;
    item->w = g->w * sx;
    item->h = g->h * sy;
    item->angle = mat_angle(m);
}

// The easing of a Tween: in and out (c_Tweener).
static float ease(float t, float a, float b) {
    return t + a * t * (1 - t) * (1 - t) - b * t * t * (1 - t);
}

// b + (a - b) * (1 - r)^n: n lerps of one frame (frameLerp.Do).
static float frame_lerp(const SSRScene *s, float a, float b, float rate) {
    return b + (a - b) * powf(1 - rate, (float)s->lerps);
}

static int shown(SSRScene *s, const SSRUiNode *node) {
    int result = 1, any = 0;
    for (int i = 0; i < node->modifierCount; i++) {
        const SSRUiNode *m = node->modifiers[i];
        int show = is(m->name, "ShowIf"), not = is(m->name, "ShowIfNot");
        int and = is(m->name, "AndShowIf"), or = is(m->name, "OrShowIf");
        if (!show && !not && !and && !or) continue;
        int value;
        if (m->argCount >= 4 && ssr_tweaks_kind(s->env->tweaks, arg(m, 0), arg(m, 1)) != SSR_TWEAK_STRING) {
            // ShowIf(Cat,Name,min,max): a number tweak in the range.
            double v = ssr_tweaks_number(s->env->tweaks, arg(m, 0), arg(m, 1));
            value = v >= number(s, m, 2, 0) && v <= number(s, m, 3, 0);
        } else if (m->argCount >= 3) {
            value = ssr_tweaks_equals(s->env->tweaks, arg(m, 0), arg(m, 1), arg(m, 2));
        } else {
            value = ssr_tweaks_number(s->env->tweaks, arg(m, 0), arg(m, 1)) != 0;
        }
        if (not) value = !value;
        if (or && any) {
            result = result || value;
        } else if (and && any) {
            result = result && value;
        } else {
            result = value;
        }
        any = 1;
    }
    return result;
}

static void rearm(Doodad *d) {
    for (; d; d = d->next) {
        if (d->flags & F_REARM) {
            d->flags |= F_ACTIVE;
            if (d->kind == D_DELAY) d->v[1] = 0;
            if (d->kind == D_FADER) d->v[1] = 0;
        }
    }
}

static void set_animation(Gel *anim, const char *name) {
    anim->animName = name;
    anim->animation = NULL;
    anim->frame = 0;
    anim->state = 1;
    anim->speed = 1;
}

static void update(SSRScene *s, Gel *g, Doodad *d);

// Runs the chain of doodads (c_Doodad.Pump). A Delay stops the rest of the
// chain until its time is up.
static void pump(SSRScene *s, Gel *g, Doodad *d) {
    for (; d; d = d->next) {
        if (d->kind == D_DELAY) {
            d->v[1] += (float)s->msDelta;
            if (d->v[1] < d->v[0]) return;
            continue;
        }
        if (d->kind == D_PRESS || d->kind == D_NOPRESS) {
            int pressed = s->pressed == g;
            int on = d->kind == D_PRESS ? pressed : !pressed;
            if (on) {
                d->flags |= F_FIRED;
                pump(s, g, d->chain);
            } else if (d->flags & F_FIRED) {
                rearm(d->chain);
                if (d->flags & F_BANG) d->flags &= ~F_ACTIVE;
            }
            continue;
        }
        if (d->flags & F_ACTIVE) {
            update(s, g, d);
            if (d->flags & F_ONCE) d->flags &= ~F_ACTIVE;
        }
    }
}

static void set_shown(Gel *g, const char *text) {
    if (g->shown && strcmp(g->shown, text) == 0) return;
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return;
    strcpy(copy, text);
    free(g->shown);
    g->shown = copy;
}

// A number as c_Readout shows it: the text of the float as Java makes it,
// cut after the decimal places, or padded with zeros.
static void readout_text(float value, int decimals, char *out, size_t capacity) {
    int digits = 1;
    float limit = 10;
    while (fabsf(value) >= limit && digits < 12) {
        limit *= 10;
        digits++;
    }
    if (value < 0) digits++;
    char text[64];
    snprintf(text, sizeof(text), "%.6f", value);
    // Java writes a float with a ".0" at least, so the integer part is
    // there in full.
    int keep = digits;
    if (decimals > 0) keep += 1 + decimals;
    if (keep > (int)strlen(text)) keep = (int)strlen(text);
    snprintf(out, capacity, "%.*s", keep, text);
}

// The second tweak of a doodad: "Group\x1fName" in text[2].
static double tweak_pair(const SSRScene *s, const char *pair) {
    char group[80];
    const char *split = strchr(pair, '\x1f');
    if (!split) return 0;
    snprintf(group, sizeof(group), "%.*s", (int)(split - pair), pair);
    return ssr_tweaks_number(s->env->tweaks, group, split + 1);
}

static void set_tweak_pair(const SSRScene *s, const char *pair, double value) {
    char group[80];
    const char *split = strchr(pair, '\x1f');
    if (!split) return;
    snprintf(group, sizeof(group), "%.*s", (int)(split - pair), pair);
    ssr_tweaks_set_number(s->env->tweaks, group, split + 1, value);
}

// The bottom of the children of g and of their children, in g.
static float content_bottom(const Gel *g) {
    float bottom = 0;
    for (int i = 0; i < g->childCount; i++) {
        const Gel *child = g->children[i];
        if (child->hidden) continue;
        float inner = content_bottom(child);
        float b = child->y + (inner > child->h ? inner : child->h);
        if (b > bottom) bottom = b;
    }
    return bottom;
}

static void update(SSRScene *s, Gel *g, Doodad *d) {
    float t = s->tweenT;
    switch (d->kind) {
    case D_SHOW:
        g->hidden = !shown(s, d->node);
        break;
    case D_SET_TWEAK:
        if (s->env->tweaks) ssr_tweaks_set(s->env->tweaks, d->text[0], d->text[1], d->text[2]);
        break;
    case D_COL_READOUT: {
        const char *value = ssr_tweaks_get(s->env->tweaks, d->text[0], d->text[1]);
        char text[24];
        snprintf(text, sizeof(text), "#%s", value[0] == '#' ? value + 1 : value);
        Colour c;
        if (parse_colour(text, &c)) g->colour = c;
        break;
    }
    case D_CONVERGE_POS:
    case D_CONVERGE_SCALE:
    case D_CONVERGE_SIZE: {
        float *a = d->kind == D_CONVERGE_POS ? &g->x : d->kind == D_CONVERGE_SCALE ? &g->sx : &g->w;
        float *b = d->kind == D_CONVERGE_POS ? &g->y : d->kind == D_CONVERGE_SCALE ? &g->sy : &g->h;
        *a = frame_lerp(s, *a, d->v[0], d->v[2]);
        *b = frame_lerp(s, *b, d->v[1], d->v[2]);
        d->v[2] = frame_lerp(s, d->v[2], d->v[3], d->v[4]);
        break;
    }
    case D_CONVERGE_COL: {
        Colour c = colour_lerp(g->colour, d->to, 1 - powf(1 - d->v[2], (float)s->lerps));
        c.mr = g->colour.mr;
        c.mg = g->colour.mg;
        c.mb = g->colour.mb;
        c.ma = g->colour.ma;
        g->colour = c;
        break;
    }
    case D_MOVER:
        g->x += d->v[0];
        g->y += d->v[1];
        d->v[0] = d->v[0] * d->v[2] + d->v[3];
        d->v[1] = d->v[1] * d->v[2] + d->v[4];
        break;
    case D_SPINNER:
        if (d->text[0]) d->v[0] = (float)ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]);
        g->angle += d->v[0];
        d->v[0] *= d->v[1];
        break;
    case D_WRAP_X:
    case D_WRAP_Y: {
        // The test is on the screen place of the last frame (c_Wrap).
        float at = d->kind == D_WRAP_X ? g->baked.tx : g->baked.ty;
        float *v = d->kind == D_WRAP_X ? &g->x : &g->y;
        float span = d->v[1] - d->v[0];
        if (at > d->v[1]) {
            *v -= span;
        } else if (at < d->v[0]) {
            *v += span;
        }
        break;
    }
    case D_FADER:
        if (d->v[1] == 0) {
            d->v[1] = 1;
            d->v[2] = (float)s->gameMS;
        }
        if (s->gameMS < d->v[2] + d->v[0] && d->v[0] > 0) {
            g->colour = colour_lerp(d->from, d->to, ((float)s->gameMS - d->v[2]) / d->v[0]);
        } else {
            g->colour = d->to;
            d->flags &= ~F_ACTIVE;
        }
        g->hasColour = 1;
        break;
    case D_LIFETIME:
        if ((float)s->gameMS > d->v[1]) g->dead = 1;
        break;
    case D_TWEEN_ANGLE:
        g->angle = lerp(d->v[0], d->v[1], ease(t, d->v[6], d->v[7]));
        break;
    case D_TWEEN_SIZE: {
        float e = ease(t, d->v[6], d->v[7]);
        g->w = lerp(d->v[0], d->v[2], e);
        g->h = lerp(d->v[1], d->v[3], e);
        g->hx = g->w * d->v[4];
        g->hy = g->h * d->v[5];
        break;
    }
    case D_TWEEN_COL: {
        Colour c = colour_lerp(d->from, d->to, ease(t, d->v[6], d->v[7]));
        c.mr = d->from.mr;
        c.mg = d->from.mg;
        c.mb = d->from.mb;
        c.ma = d->from.ma;
        g->colour = c;
        g->hasColour = 1;
        break;
    }
    case D_TWEEN_ALPHA:
        g->colour.a = lerp(d->v[0], d->v[1], ease(t, d->v[6], d->v[7]));
        break;
    case D_TWEEN_SCALE: {
        float e = ease(t, d->v[6], d->v[7]);
        g->sx = lerp(d->v[0], d->v[2], e);
        g->sy = lerp(d->v[1], d->v[3], e);
        break;
    }
    case D_TWEEN_POS: {
        float e = ease(t, d->v[6], d->v[7]);
        g->x = lerp(d->v[0], d->v[2], e);
        g->y = lerp(d->v[1], d->v[3], e);
        break;
    }
    case D_SHINE: {
        // A glint that runs along the button to a target that moves.
        float pos = d->v[2] = frame_lerp(s, d->v[2], s->shineTarget, 0.05f);
        g->colour.a = clampf(8 * pos * (1 - pos), 0, 1);
        float left = d->v[0], right = d->v[1];
        g->x = lerp(left, right, pos);
        float room = g->x - left < right - g->x ? g->x - left : right - g->x;
        g->w = 2 * room < 84 ? 2 * room : 84;
        if (g->w < 0) g->w = 0;
        g->hx = g->w / 2;
        break;
    }
    case D_READOUT: {
        float value = (float)ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]);
        if (value == d->v[1]) break;
        d->v[1] = value;
        char number[64], text[256];
        readout_text(value, (int)d->v[0], number, sizeof(number));
        const char *split = strchr(d->text[2], '\x1f');
        int prefixLength = split ? (int)(split - d->text[2]) : (int)strlen(d->text[2]);
        snprintf(text, sizeof(text), "%.*s%s%s", prefixLength, d->text[2], number, split ? split + 1 : "");
        set_shown(g, text);
        break;
    }
    case D_STRING_READOUT:
        set_shown(g, ssr_tweaks_get(s->env->tweaks, d->text[0], d->text[1]));
        break;
    case D_PROVIDER:
        if (s->env->provide) {
            const char *text = s->env->provide(s->env->context, d->text[0]);
            if (text) set_shown(g, text);
        }
        break;
    case D_SELECTOR: {
        // Only the child at the index of the tweak shows.
        int index = (int)ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]);
        for (int i = 0; i < g->childCount; i++) g->children[i]->hidden = i != index;
        break;
    }
    case D_INCREMENT_TWEAK: {
        double value = ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]) + 1;
        if (value > d->v[1]) value = d->v[0];
        ssr_tweaks_set_number(s->env->tweaks, d->text[0], d->text[1], value);
        break;
    }
    case D_EX: {
        SSRExHost host = ex_host(s, g);
        if (d->exCount >= 1) {
            ssr_ex_operate(d->text[0], d->ex[0], d->exCount >= 2 ? d->ex[1] : NULL, d->exCount >= 3 ? d->ex[2] : NULL,
                           &host);
        }
        break;
    }
    case D_CLONE_TWEAK: {
        char group[80];
        const char *split = strchr(d->text[0], '\x1f');
        if (!split) break;
        snprintf(group, sizeof(group), "%.*s", (int)(split - d->text[0]), d->text[0]);
        const char *value = ssr_tweaks_get(s->env->tweaks, group, split + 1);
        ssr_tweaks_set(s->env->tweaks, d->text[1], d->text[2], value);
        // The game draws at once, with the tweak of this moment. The remake
        // draws later, so the items of the gel keep the colour.
        if (strcmp(d->text[1], "CelColours") == 0) {
            size_t used = strlen(g->cels);
            snprintf(g->cels + used, sizeof(g->cels) - used, "%s=%s;", d->text[2], value[0] == '#' ? value + 1 : value);
        }
        break;
    }
    case D_DOIF: {
        SSRTweaks *t = s->env->tweaks;
        int on;
        if (ssr_tweaks_kind(t, d->text[0], d->text[1]) == SSR_TWEAK_STRING) {
            on = strcmp(ssr_tweaks_get(t, d->text[0], d->text[1]), d->text[2]) == 0;
        } else {
            float v = (float)ssr_tweaks_number(t, d->text[0], d->text[1]);
            on = v >= d->v[0] && v <= d->v[1];
        }
        if (on == (d->v[2] != 0)) {
            d->flags |= F_FIRED;
            pump(s, g, d->chain);
        } else {
            // The block runs again from its start the next time.
            rearm(d->chain);
            if (d->flags & F_BANG) d->flags &= ~F_ACTIVE;
            d->flags &= ~F_FIRED;
        }
        break;
    }
    case D_IF: {
        SSRExHost host = ex_host(s, g);
        int on = d->exCount >= 1 && ssr_ex_bool(d->ex[0], &host);
        // The other block runs again when the condition changes (c_Iff).
        if (on) {
            if (!(d->flags & F_FIRED)) rearm(d->other);
            pump(s, g, d->chain);
            d->flags |= F_FIRED;
        } else {
            if (d->flags & F_FIRED) rearm(d->chain);
            pump(s, g, d->other);
            // "!if" stops for good when the condition is false.
            if (d->flags & F_BANG) d->flags &= ~F_ACTIVE;
            d->flags &= ~F_FIRED;
        }
        break;
    }
    case D_SHOW_SHELL:
        ssr_tweaks_set_number(s->env->tweaks, "Shell", d->text[0], d->v[0]);
        break;
    case D_SCROLLING_LIST: {
        float at = (float)ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]);
        float contents = content_bottom(g);
        float lowest = d->v[0] - contents;
        if (lowest > 0) lowest = 0;
        float velocity = (float)tweak_pair(s, d->text[2]);
        float y = clampf(at + velocity, lowest - 64, 64);
        velocity *= 0.95f;
        g->y = y;
        // Past an end, the list goes back with a spring.
        if (y > 0) {
            y *= 0.9f;
            if (velocity > 0) velocity = 0;
        }
        if (y < lowest) {
            y = (y - lowest) * 0.9f + lowest;
            if (velocity < 0) velocity = 0;
        }
        set_tweak_pair(s, d->text[2], velocity);
        ssr_tweaks_set_number(s->env->tweaks, d->text[0], d->text[1], y);
        break;
    }
    case D_SWIPE_PANEL: {
        if (s->swipe != g) break;
        float lx, ly;
        mat_apply(mat_inverse(g->baked), s->touchX, s->touchY, &lx, &ly);
        if (s->touchStarted) {
            d->v[0] = ly;
            d->v[1] = ly;
            d->v[2] = (float)ssr_tweaks_number(s->env->tweaks, d->text[0], d->text[1]);
            d->v[3] = 0;
        } else if (s->touchDown) {
            d->v[3] = ly - d->v[1];
            d->v[1] = ly;
            ssr_tweaks_set_number(s->env->tweaks, d->text[0], d->text[1], d->v[2] + (ly - d->v[0]));
            if (fabsf(d->v[0] - ly) > 10) s->dragged = 1;
        } else {
            // The list keeps the speed of the last move.
            if (d->v[3] != 0) set_tweak_pair(s, d->text[2], d->v[3]);
            d->v[3] = 0;
            s->swipe = NULL;
        }
        break;
    }
    case D_PLAY_SOUND:
        if (s->soundCount < 8) s->sounds[s->soundCount++] = d->text[0];
        break;
    case D_PLAY_ANIM: {
        Gel *target = d->text[0] ? find_ref(s, d->text[0]) : s->instance;
        if (target && target->kind == GEL_ANIM) set_animation(target, d->text[1]);
        break;
    }
    default:
        break;
    }
}

typedef struct {
    Mat m;
    Colour colour;
    int layer, depth;
} Frame;

static void draw_gel(SSRScene *s, Gel *g, const Frame *parent);

static void draw_children(SSRScene *s, Gel *g, const Frame *f) {
    for (int i = 0; i < g->childCount; i++) draw_gel(s, g->children[i], f);
}

// One part of a fan of a crowd: the image, the place of its handle, the
// angle, and the tint.
static void crowd_part(SSRScene *s, const Frame *f, const char *image, float x, float y, float width, float height,
                       float handleX, float handleY, float angle, unsigned int rgb) {
    SSRSceneItem *item = add_item(s, SSR_ITEM_IMAGE, NULL, f->layer, f->depth);
    if (!item) return;
    item->image = image;
    item->x = x - handleX;
    item->y = y - handleY;
    item->w = width;
    item->h = height;
    item->pivotX = x;
    item->pivotY = y;
    item->angle = angle;
    item->colour = (rgb << 8) | 0xFF;
}

static unsigned int tweak_rgb(SSRScene *s, const char *group, const char *name) {
    return ssr_tweaks_colour(s->env->tweaks, group, name, 0xFFFFFFFF) >> 8;
}

// A grid of Copies(columns, rows) fans, Spacing(x, y) apart. Each fan has
// trousers, a shirt, a head, arms, and maybe a hat, with random tints from
// the tweaks of the crowd. This is c_CrowdPacket.Draw of the game.
static void draw_crowd(SSRScene *s, const Gel *g, const Frame *f) {
    const SSRTweaks *t = s->env->tweaks;
    if (ssr_tweaks_number(t, "Crowd", "Active") <= 0 || g->p[3]) return;
    int bottom = g->p[2] != 0;
    static const char *arms[] = {"CrowdArms1", "CrowdArms2", "CrowdArms3", "CrowdArms4"};
    static const char *heads[] = {"CrowdHead1", "CrowdHead2"};
    static const char *hats[] = {"CrowdHat1", "CrowdHat2"};
    static const char *trousers[] = {"CrowdTrousers1", "CrowdTrousers2"};
    static const char *bottomArms[] = {"CrowdBottomArms1", "CrowdBottomArms2"};
    static const char *bottomHeads[] = {"CrowdBottomHead1"};
    static const char *bottomTrousers[] = {"CrowdBottomTrousers1", "CrowdBottomTrousers2"};
    const char *const *armList = bottom ? bottomArms : arms;
    const char *const *headList = bottom ? bottomHeads : heads;
    const char *const *trouserList = bottom ? bottomTrousers : trousers;
    int armCount = bottom ? 2 : 4, headCount = bottom ? 1 : 2;
    const char *shirt = bottom ? "CrowdBottomShirt1" : "CrowdShirt";

    float x0 = f->m.tx, y0 = f->m.ty;
    float width = g->w, height = g->h, handleX = g->hx, handleY = g->hy;
    float stepX = g->stepX, stepY = g->stepY;
    int columns = g->columns, rows = g->rows;

    float homeDensity = (float)ssr_tweaks_number(t, "HomeCrowd", "Density");
    float awayDensity = (float)ssr_tweaks_number(t, "AwayCrowd", "Density");
    float hatLikelihood = (float)ssr_tweaks_number(t, "Crowd", "HatLikelihood");
    float bias = (float)ssr_tweaks_number(t, "Crowd", "HomeAwayBias");
    int trouserColours = (int)ssr_tweaks_number(t, "Crowd", "TrouserColCount");
    if (trouserColours < 1) trouserColours = 1;
    SSRRandom *r = &s->crowdRandom;
    float y = y0;
    for (int row = 0; row < rows; row++) {
        float x = x0;
        float rowBias = row * 0.2f / (float)(rows + 1);
        for (int column = 0; column < columns; column++) {
            int home = ssr_rand_float(r, 0, 1) < bias;
            const char *side = home ? "HomeCrowd" : "AwayCrowd";
            float density = (home ? homeDensity : awayDensity) + rowBias;
            if (ssr_rand_float(r, 0, 1) <= density) {
                int px = (int)(ssr_rand_float(r, -1, 1) + x);
                int yBody = (int)(y + ssr_rand_float(r, 0, 2));
                float angle = (float)(int)ssr_rand_float(r, -4, 4);
                int yHead = (int)(ssr_rand_float(r, 0, 2) + (float)yBody);
                int head = ssr_rand2(r, 0, headCount - 1);
                char name[32];
                snprintf(name, sizeof(name), "TrouserCol%d", ssr_rand2(r, 0, trouserColours - 1));
                unsigned int trouser = tweak_rgb(s, "Crowd", name);
                crowd_part(s, f, trouserList[ssr_rand2(r, 0, 1)], (float)px, y, width, height, handleX, handleY, 0,
                           trouser);
                unsigned int sr = (unsigned int)ssr_rand2(r, 30, 200), sg = (unsigned int)ssr_rand2(r, 30, 200);
                unsigned int sb = (unsigned int)ssr_rand2(r, 30, 200);
                unsigned int shirtRgb = sr << 16 | sg << 8 | sb;
                int skin = abs((ssr_rand2(r, 1, 39) + ssr_rand2(r, 1, 39)) / 2 - 20);
                snprintf(name, sizeof(name), "SkinCol%d", skin);
                unsigned int skinRgb = tweak_rgb(s, side, name);
                if (ssr_rand_float(r, 0, 1) < (float)ssr_tweaks_number(t, side, "StripChance")) {
                    int away = ssr_rand_float(r, 0, 1) < (float)ssr_tweaks_number(t, side, "AwayStripChance");
                    shirtRgb = tweak_rgb(s, side, away ? "ShirtColAway" : "ShirtColHome");
                }
                crowd_part(s, f, shirt, (float)px, (float)yBody, width, height, handleX, handleY, angle, shirtRgb);
                crowd_part(s, f, headList[head], (float)px, (float)yHead, width, height, handleX, handleY, angle,
                           skinRgb);
                crowd_part(s, f, armList[ssr_rand2(r, 0, armCount - 1)], (float)px, (float)yBody, width, height,
                           handleX, handleY, angle, skinRgb);
                if (!bottom && ssr_rand_float(r, 0, 1) < hatLikelihood - skin * 0.01f) {
                    crowd_part(s, f, hats[head], (float)px, (float)yHead, width, height, handleX, handleY, angle,
                               shirtRgb);
                }
            }
            x += stepX;
        }
        y += stepY;
    }
}

// Draws a pose of a Spine animation at the time of the screen. Each image
// is drawn at its own size with its centre on its place.
static void draw_spine(SSRScene *s, const Gel *g, const Frame *f) {
    if (!s->env->spine) return;
    const SSRSpine *file = s->env->spine(s->env->context, g->name);
    if (!file) return;
    int animation = ssr_spine_animation(file, g->animationName);
    if (animation < 0) return;
    // A limit of loops plays the animation that many times, then it stops.
    int limit = (int)g->p[1];
    int loop = g->p[0] != 0 && limit <= 0;
    float time = s->gameMS / 1000.0f;
    if (limit > 0) {
        float duration = ssr_spine_duration(file, animation);
        if (time > duration * limit) time = duration;
        else if (duration > 0) time = fmodf(time, duration);
    }
    SSRSpineDraw draws[128];
    int count = ssr_spine_pose(file, animation, time, loop, draws, 128);
    if (count > 128) count = 128;
    for (int i = 0; i < count; i++) {
        const SSRSpineDraw *d = &draws[i];
        float iw = 0, ih = 0;
        if (!s->env->image_size || !s->env->image_size(s->env->context, d->image, d->file, &iw, &ih)) continue;
        SSRSceneItem *item = add_item(s, SSR_ITEM_IMAGE, NULL, f->layer, f->depth);
        if (!item) return;
        item->image = keep_frame(s, d->image);
        item->quad = 1;
        item->w = iw;
        item->h = ih;
        const float local[8] = {-iw / 2, -ih / 2, iw / 2, -ih / 2, iw / 2, ih / 2, -iw / 2, ih / 2};
        for (int k = 0; k < 4; k++) {
            float px = d->m[0] * local[k * 2] + d->m[2] * local[k * 2 + 1] + d->m[4];
            float py = d->m[1] * local[k * 2] + d->m[3] * local[k * 2 + 1] + d->m[5];
            mat_apply(f->m, px, py, &item->corners[k * 2], &item->corners[k * 2 + 1]);
        }
        item->x = item->corners[0];
        item->y = item->corners[1];
        Colour c = {d->r * 255, d->g * 255, d->b * 255, d->a, 1, 1, 1, 1};
        item->colour = colour_rgba(colour_mix(c, f->colour));
    }
}

static void draw_image(SSRScene *s, const Gel *g, const Frame *f, const char *image) {
    if (!image || !image[0]) return;
    int columns = g->tiled ? g->columns : 1, rows = g->tiled ? g->rows : 1;
    // A Tile with no Copies() draws nothing, as in the game.
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            SSRSceneItem *item = add_item(s, SSR_ITEM_IMAGE, g, f->layer, f->depth);
            if (!item) return;
            item->image = image;
            float dx = column * g->stepX + row * g->shearX, dy = row * g->stepY + column * g->shearY;
            set_box(item, g, f->m, dx, dy);
            item->colour = colour_rgba(f->colour);
            item->blend = g->blend;
        }
    }
}

static void draw_text(SSRScene *s, const Gel *g, const Frame *f) {
    char value[1024];
    const char *text = value;
    if (g->shown) {
        text = g->shown;
    } else {
        text_value(s, g->text ? g->text : "", value, sizeof(value));
        // A tag that the language does not have stays as it is.
        if (g->localise && s->env->text) text = s->env->text(s->env->context, value);
    }
    SSRSceneItem *item = add_item(s, SSR_ITEM_TEXT, g, f->layer, f->depth);
    if (!item) return;
    item->text = keep_frame(s, text);
    set_box(item, g, f->m, 0, 0);
    item->quad = 0;
    item->alignX = g->alignX;
    item->alignY = g->alignY;
    item->font = g->font;
    item->fontScaleX = g->fontScaleX * mat_scale_x(f->m);
    item->fontScaleY = g->fontScaleY * mat_scale_y(f->m);
    item->wrap = g->wrap;
    item->italic = g->italic;
    item->colour = colour_rgba(f->colour);
    item->blend = g->blend;
}

// Gives the value of a Range: tweak of the emitter, or the fallback when the
// emitter has no such tweak. A roll takes one random number.
static float roll(SSRScene *s, const char *category, const char *name, float fallback, int *had) {
    double min, max;
    if (!ssr_tweaks_range(s->env->tweaks, category, name, &min, &max)) {
        if (had) *had = 0;
        return fallback;
    }
    if (had) *had = 1;
    float r = ssr_rand_float(&s->random, 0, 1);
    return (float)(min + (max - min) * r);
}

static int has_tweak(SSRScene *s, const char *category, const char *name) {
    double min, max;
    return ssr_tweaks_range(s->env->tweaks, category, name, &min, &max);
}

// Gives the rolled values to the Emit doodads of a new particle.
static void apply_roll(SSRScene *s, Gel *g, const Roll *r) {
    for (Doodad *d = g->doodads; d; d = d->next) {
        switch (d->kind) {
        case D_EMIT_PLACER:
            g->x += r->xStart;
            g->y += r->yStart;
            g->angle += r->angle;
            d->flags &= ~F_ACTIVE;
            break;
        case D_EMIT_MOVER:
            d->kind = D_MOVER;
            d->v[0] = r->xVel;
            d->v[1] = r->yVel;
            d->v[2] = r->drag;
            d->v[3] = r->xAcc;
            d->v[4] = r->yAcc;
            break;
        case D_EMIT_SPINNER:
            d->kind = D_SPINNER;
            d->v[0] = r->spin;
            d->v[1] = r->angularDrag;
            d->text[0] = NULL;
            break;
        case D_EMIT_LIFETIME:
            d->kind = D_LIFETIME;
            d->v[0] = r->lifetime;
            d->v[1] = (float)s->gameMS + r->lifetime;
            break;
        case D_EMIT_ANIM_START:
            if (g->kind == GEL_ANIM) g->frame = r->animStart;
            d->flags &= ~F_ACTIVE;
            break;
        case D_EMIT_SIZE:
            if (r->hasSize) {
                float fx = g->w != 0 ? g->hx / g->w : 0, fy = g->h != 0 ? g->hy / g->h : 0;
                if (d->v[0] && g->h != 0) {
                    float aspect = g->w / g->h;
                    g->w = r->xSize;
                    g->h = r->xSize * aspect;
                    g->hx = r->xSize * fx;
                    g->hy = r->xSize * fy;
                } else {
                    g->w = r->xSize;
                    g->h = r->ySize;
                    g->hx = g->w * fx;
                    g->hy = g->h * fy;
                }
            }
            d->flags &= ~F_ACTIVE;
            break;
        default:
            break;
        }
    }
    for (int i = 0; i < g->childCount; i++) apply_roll(s, g->children[i], r);
}

// Makes one particle: a copy of a template of the emitter, with the values
// that the tweaks of the emitter roll, in the order of c_GelEmitter.Emit.
static void emit(SSRScene *s, Gel *e) {
    Gel *templates[64];
    int count = 0;
    for (int i = 0; i < s->refCount && count < 64; i++) {
        if (strcmp(s->refs[i]->ref, e->templateRef) == 0 && s->refs[i] != e) templates[count++] = s->refs[i];
    }
    if (!count) return;
    int index = (int)(ssr_rnd(&s->random) * (float)count);
    if (index >= count) index = count - 1;
    const char *c = e->category;
    Roll r = {0};
    r.xStart = roll(s, c, "XStart", 0, NULL);
    r.yStart = roll(s, c, "YStart", 0, NULL);
    r.angle = roll(s, c, "Angle", 0, NULL);
    r.lifetime = roll(s, c, "Lifetime", 0.001f, NULL) * 1000;
    r.spin = roll(s, c, "Spin", 0, NULL);
    roll(s, c, "XGravity", 0, NULL);
    roll(s, c, "YGravity", 0, NULL);
    int hadX = 0, hadY = 0;
    r.xSize = roll(s, c, "XSize", 0, &hadX);
    r.ySize = roll(s, c, "YSize", r.xSize, &hadY);
    r.hasSize = hadX;
    r.drag = roll(s, c, "Drag", 1, NULL);
    r.angularDrag = roll(s, c, "AngularDrag", 1, NULL);
    r.xVel = roll(s, c, "XVel", 0, NULL);
    r.yVel = roll(s, c, "YVel", 0, NULL);
    if (has_tweak(s, c, "XEndScale")) {
        roll(s, c, "XEndScale", 1, NULL);
    } else {
        roll(s, c, "XEnd", 0, NULL);
    }
    r.xAcc = roll(s, c, "XAcc", 0, NULL);
    r.yAcc = roll(s, c, "YAcc", 0, NULL);
    // The game rolls XEndScale for the end of y when YEndScale is there.
    if (has_tweak(s, c, "YEndScale")) {
        roll(s, c, "XEndScale", 1, NULL);
    } else {
        roll(s, c, "XEnd", 0, NULL);
    }
    roll(s, c, "Convergence", 0, NULL);
    roll(s, c, "ConvTween", 0, NULL);
    roll(s, c, "FinalConv", 0, NULL);
    roll(s, c, "ConvDelay", 0, NULL);
    float dir = roll(s, c, "EmitDir", 0, NULL) * PI_F / 180;
    float vx = r.xVel, vy = r.yVel;
    r.xVel = vx * cosf(dir) + vy * sinf(dir);
    r.yVel = vy * cosf(dir) - vx * sinf(dir);
    r.animStart = roll(s, c, "AnimStart", 0, NULL);

    Gel *particle = clone_gel(templates[index]);
    if (!particle) return;
    particle->hidden = 0;
    apply_roll(s, particle, &r);
    add_child(e, particle);
}

static void run_emitter(SSRScene *s, Gel *e) {
    const char *c = e->category;
    if (!e->started) {
        e->started = 1;
        e->rateMs = (int)(roll(s, c, "Rate", 0, NULL) * 1000);
        e->emitCount = (int)ssr_tweaks_number(s->env->tweaks, c, "Count");
    }
    e->next += (float)s->msDelta;
    double min = 0, max = 0;
    int varies = ssr_tweaks_range(s->env->tweaks, c, "Rate", &min, &max) && min != max;
    while (e->next > (float)e->rateMs && e->emitted < (e->emitCount > 0 ? e->emitCount : e->emitted + 100)) {
        emit(s, e);
        e->emitted++;
        e->next -= (float)e->rateMs;
        if (varies) e->rateMs = (int)(roll(s, c, "Rate", 0, NULL) * 1000);
        if (e->rateMs <= 0) break;
    }
}

static void draw_animation(SSRScene *s, Gel *g, const Frame *f) {
    if (!g->animation && g->animName && g->animName[0]) g->animation = find_animation(s, g->animName);
    Gel *a = g->animation;
    if (!a || g->state == 0) return;
    float saved = s->animFrame, savedT = s->tweenT;
    Gel *savedInstance = s->instance;
    float length = a->length;
    if (a->mode == 'C') {
        s->animFrame = clampf(g->frame, 0, length - 1);
    } else if (a->mode == 'L') {
        float ls = a->loopStart;
        s->animFrame = g->frame < ls ? g->frame : ls + fmodf(g->frame - ls, length - ls > 0 ? length - ls : 1);
    } else if (g->state == 1 && g->frame >= length - 1) {
        // Once: the animation ends. The frame stays the outer one.
        g->state = 0;
    }
    if (g->state != 0) {
        Frame inner = *f;
        inner.m = mat_concat(f->m, local_matrix(a));
        s->instance = g;
        draw_children(s, a, &inner);
        if (g->state == 1) {
            g->frame += s->frameDelta * g->speed;
            if (a->mode == 'O' && g->frame >= length) g->state = 0;
        }
    }
    s->animFrame = saved;
    s->tweenT = savedT;
    s->instance = savedInstance;
}

static void draw_sequence(SSRScene *s, Gel *g, const Frame *f) {
    if (!g->frameCount || !g->childCount) return;
    float af = s->animFrame;
    if (!(g->first <= af && af < g->last + 1)) return;
    int index = (int)((af - g->first) * (float)(g->frameCount * g->repeats) / (g->last + 1 - g->first));
    index %= g->frameCount;
    if (index < 0) index += g->frameCount;
    Gel *draw = g->children[0];
    draw->image = g->frames[index];
    float saved = s->tweenT;
    s->tweenT = g->last != g->first ? (af - g->first) / (g->last - g->first) : saved;
    draw_gel(s, draw, f);
    s->tweenT = saved;
}

// Draws the gel and its children as c_Gel.Draw: the doodads run, then the
// gel bakes its transform and colour and queues its packet.
static void set_embed(SSRScene *s, const char *name, Gel *g) {
    for (int i = 0; i < s->embedCount; i++) {
        if (strcmp(s->embeds[i].name, name) == 0) {
            s->embeds[i].gel = g;
            return;
        }
    }
    if (s->embedCount < 32) {
        s->embeds[s->embedCount].name = name;
        s->embeds[s->embedCount++].gel = g;
    }
}

static Gel *get_embed(const SSRScene *s, const char *name) {
    for (int i = 0; i < s->embedCount; i++) {
        if (strcmp(s->embeds[i].name, name) == 0) return s->embeds[i].gel;
    }
    return NULL;
}

static void draw_gel_inner(SSRScene *s, Gel *g, const Frame *parent);

// The item goes onto the plane of the sheet: its four corners in the space
// of the camera, for the perspective of the renderer (c_GelSheet).
static void project_item(SSRSceneItem *item, const Gel *sheet) {
    if (item->kind != SSR_ITEM_IMAGE || item->projected) return;
    float c[8];
    if (item->quad) {
        memcpy(c, item->corners, sizeof(c));
    } else {
        // The corners of the box, turned about the pivot as the renderer does.
        float ca = cosf(item->angle * PI_F / 180), sa = sinf(item->angle * PI_F / 180);
        static const float U[4] = {0, 1, 1, 0}, V[4] = {0, 0, 1, 1};
        for (int k = 0; k < 4; k++) {
            float px = item->x + item->w * U[k] - item->pivotX, py = item->y + item->h * V[k] - item->pivotY;
            c[k * 2] = item->pivotX + px * ca + py * sa;
            c[k * 2 + 1] = item->pivotY - px * sa + py * ca;
        }
    }
    const float *m = sheet->trs;
    for (int k = 0; k < 4; k++) {
        for (int r = 0; r < 3; r++) item->camera[k * 3 + r] = m[r * 4] * c[k * 2] + m[r * 4 + 1] * c[k * 2 + 1] + m[r * 4 + 3];
    }
    item->projected = 1;
    item->focal = sheet->focal;
}

static void draw_gel(SSRScene *s, Gel *g, const Frame *parent) {
    const char *cels = s->cels;
    g->cels[0] = 0;
    draw_gel_inner(s, g, parent);
    s->cels = cels;
}

static void draw_gel_inner(SSRScene *s, Gel *g, const Frame *parent) {
    if (g->dead) return;
    if (g->kind == GEL_EMBED) {
        // It draws nothing. A DrawEmbedded draws its children.
        pump(s, g, g->doodads);
        if (!g->hidden) set_embed(s, g->name, g);
        return;
    }
    if (g->kind == GEL_DRAW_EMBEDDED) {
        // The children of the embed draw in the frame of the parent.
        Gel *embed = get_embed(s, g->name);
        pump(s, g, g->doodads);
        if (!embed || g->hidden) return;
        Frame f = *parent;
        if (g->layer >= 0) f.layer = g->layer;
        draw_children(s, embed, &f);
        return;
    }
    Frame f = *parent;
    if (g->layer >= 0) f.layer = g->layer;
    if (g->depth != DEPTH_INHERIT) f.depth = g->depth;
    pump(s, g, g->doodads);
    if (g->cels[0]) s->cels = keep_frame(s, g->cels);
    // The remake has no render to a surface. A sheet that renders into a
    // surface draws its children each frame, in its place [I].
    if (g->kind == GEL_SHEET && g->rtt) g->hidden = 0;
    if (g->hidden || g->dead) return;
    f.m = mat_concat(parent->m, local_matrix(g));
    g->baked = f.m;
    // A gel lower on the screen draws over a gel higher up (c_Gel.YDepth).
    if (g->yDepth) f.depth = (int)(f.m.ty + g->yDepthOffset);
    if (g->hasColour) f.colour = colour_mix(g->colour, parent->colour);

    int pressable = 0;
    for (Doodad *d = g->doodads; d; d = d->next) {
        if (d->kind == D_PRESS || d->kind == D_NOPRESS) pressable = 1;
    }
    for (Doodad *d = g->doodads; d; d = d->next) {
        if (d->kind != D_SWIPE_PANEL || !s->touchDown) continue;
        float lx, ly;
        mat_apply(mat_inverse(f.m), s->touchX, s->touchY, &lx, &ly);
        if (lx >= -g->hx && lx < g->w - g->hx && ly >= -g->hy && ly < g->h - g->hy) s->swipeCandidate = g;
    }
    if (pressable && s->touchDown) {
        float lx, ly;
        mat_apply(mat_inverse(f.m), s->touchX, s->touchY, &lx, &ly);
        if (lx >= -g->hx && lx < g->w - g->hx && ly >= -g->hy && ly < g->h - g->hy) s->candidate = g;
    }

    switch (g->kind) {
    case GEL_DRAW:
        draw_image(s, g, &f, g->image);
        break;
    case GEL_TEXT:
        draw_text(s, g, &f);
        break;
    case GEL_RECT:
        if (g->ref || g->click) {
            SSRSceneItem *item = add_item(s, SSR_ITEM_AREA, g, f.layer, f.depth);
            if (item) {
                set_box(item, g, f.m, 0, 0);
                item->quad = 0;
            }
        }
        break;
    case GEL_SHEET: {
        // The children draw with no 2D transform of the parents. A sheet with
        // perspective gives its items the corners on its plane.
        Frame plane = f;
        plane.m = mat_identity();
        const Gel *outer = s->sheet;
        s->sheet = g->focal > 0 ? g : NULL;
        int first = s->count;
        draw_children(s, g, &plane);
        if (g->focal > 0) {
            for (int i = first; i < s->count; i++) project_item(&s->items[i], g);
        }
        s->sheet = outer;
        return;
    }
    case GEL_CLIP: {
        SSRSceneItem box;
        memset(&box, 0, sizeof(box));
        set_box(&box, g, f.m, 0, 0);
        float x0 = box.x, y0 = box.y, x1 = box.x + box.w, y1 = box.y + box.h;
        if (s->clipCount > 0) {
            // A clip in a clip is the part that both boxes cover.
            const float *o = &s->clips[s->clipCount - 1].x;
            if (x0 < o[0]) x0 = o[0];
            if (y0 < o[1]) y0 = o[1];
            if (x1 > o[0] + o[2]) x1 = o[0] + o[2];
            if (y1 > o[1] + o[3]) y1 = o[1] + o[3];
        }
        if (s->clipCount < 32) {
            s->clips[s->clipCount].x = x0;
            s->clips[s->clipCount].y = y0;
            s->clips[s->clipCount].w = x1 > x0 ? x1 - x0 : 0;
            s->clips[s->clipCount].h = y1 > y0 ? y1 - y0 : 0;
            s->clipCount++;
            draw_children(s, g, &f);
            s->clipCount--;
        }
        return;
    }
    case GEL_CROWD:
        draw_crowd(s, g, &f);
        break;
    case GEL_SPINE:
        draw_spine(s, g, &f);
        break;
    case GEL_ANIM:
        draw_animation(s, g, &f);
        break;
    case GEL_DURING: {
        float af = s->animFrame;
        if (!(g->first <= af && af < g->last + 1)) return;
        float saved = s->tweenT;
        if (g->last > g->first && g->tlength != 0) s->tweenT = clampf((af - g->tfirst) / g->tlength, 0, 1);
        draw_children(s, g, &f);
        s->tweenT = saved;
        return;
    }
    case GEL_TWEEN: {
        float saved = s->tweenT;
        s->tweenT = lerp(g->tweenFrom, g->tweenTo, saved);
        draw_children(s, g, &f);
        s->tweenT = saved;
        return;
    }
    case GEL_SEQUENCE:
        draw_sequence(s, g, &f);
        return;
    case GEL_EMITTER:
        run_emitter(s, g);
        break;
    default:
        break;
    }
    draw_children(s, g, &f);
}

// Takes away the gels that their Lifetime ended.
static void sweep(Gel *g) {
    int kept = 0;
    for (int i = 0; i < g->childCount; i++) {
        Gel *child = g->children[i];
        if (child->dead) {
            free_gel(child);
            continue;
        }
        sweep(child);
        g->children[kept++] = child;
    }
    g->childCount = kept;
}

static const SSRScene *sorting;

static const Layer *layer_of(const SSRScene *s, int layer) {
    return &s->layers[layer >= 0 && layer < s->layerCount ? layer : 0];
}

// The strata of the game: the layers from the lowest depth up. A sorted
// layer orders its packets by depth, the others keep the order of the walk.
static int compare_items(const void *pa, const void *pb) {
    int a = *(const int *)pa, b = *(const int *)pb;
    const SSRScene *s = sorting;
    int layerA = s->items[a].layer, layerB = s->items[b].layer;
    const Layer *la = layer_of(s, layerA), *lb = layer_of(s, layerB);
    if (la->depth != lb->depth) return la->depth < lb->depth ? -1 : 1;
    if (layerA != layerB) return layerA < layerB ? -1 : 1;
    const SortKey *ka = &s->keys[a], *kb = &s->keys[b];
    if (la->sorted && ka->depth != kb->depth) return (ka->depth < kb->depth) != la->highToLow ? -1 : 1;
    return ka->order < kb->order ? -1 : ka->order > kb->order;
}

void ssr_scene_update(SSRScene *s, const SSRSceneEnv *env, int ms) {
    if (!s || !env || !s->root) return;
    s->env = env;
    // The time base of the game: frames of 60 a second, from 1 to 5 a
    // frame, and 16 to 80 ms a frame.
    if (!s->started) {
        s->started = 1;
        s->frameDelta = 1;
        s->msDelta = 0;
    } else {
        s->frameDelta = clampf((float)ms * 60 / 1000, 1, 5);
        s->msDelta = ms < 16 ? 16 : ms > 80 ? 80 : ms;
        s->gameMS += s->msDelta;
    }
    s->lerps = (int)s->frameDelta;
    s->shineTarget = 0.5f - 0.3f * sinf(s->gameMS * 0.04f * PI_F / 180) + 0.1f * sinf(s->gameMS * 0.013f * PI_F / 180);

    free_frame_strings(s);
    s->count = 0;
    s->clipCount = 0;
    s->order = 0;
    s->animFrame = 0;
    s->tweenT = 0;
    s->instance = NULL;
    s->candidate = NULL;
    s->swipeCandidate = NULL;
    s->crowdRandom.seed = SSR_RANDOM_START;
    Frame top = {mat_identity(), colour_white(), 0, 0};
    draw_children(s, s->root, &top);
    s->pressed = s->touchDown ? s->candidate : NULL;
    // A new touch picks the swipe panel under it. The panel starts the
    // swipe in the next frame, as the touch packets of the game do.
    if (s->touchDown && !s->touchWasDown) {
        s->swipe = s->swipeCandidate;
        s->dragged = 0;
        s->touchStarted = s->swipe != NULL;
        s->touchWasDown = 1;
    } else {
        s->touchStarted = 0;
        s->touchWasDown = s->touchDown;
    }
    sweep(s->root);

    if (s->count > 1) {
        int *index = malloc((size_t)s->count * sizeof(int));
        SSRSceneItem *sorted = malloc((size_t)s->count * sizeof(SSRSceneItem));
        if (index && sorted) {
            for (int i = 0; i < s->count; i++) index[i] = i;
            sorting = s;
            qsort(index, (size_t)s->count, sizeof(int), compare_items);
            sorting = NULL;
            for (int i = 0; i < s->count; i++) sorted[i] = s->items[index[i]];
            memcpy(s->items, sorted, (size_t)s->count * sizeof(SSRSceneItem));
        }
        free(index);
        free(sorted);
    }
    s->env = NULL;
}

SSRScene *ssr_scene_build(const SSRUiTree *tree, const SSRSceneEnv *env) {
    SSRScene *scene = ssr_scene_open(tree, env);
    ssr_scene_update(scene, env, 0);
    return scene;
}

void ssr_scene_touch(SSRScene *scene, int down, float x, float y) {
    if (!scene) return;
    scene->touchDown = down;
    scene->touchX = x;
    scene->touchY = y;
}

int ssr_scene_time(const SSRScene *scene) {
    return scene ? scene->gameMS : 0;
}

void ssr_scene_free(SSRScene *scene) {
    if (!scene) return;
    free_gel(scene->root);
    for (int i = 0; i < scene->detachedCount; i++) free_gel(scene->detached[i]);
    free(scene->detached);
    free(scene->refs);
    for (int i = 0; i < scene->stringCount; i++) free(scene->strings[i]);
    free(scene->strings);
    free_frame_strings(scene);
    free(scene->frameStrings);
    free(scene->items);
    free(scene->keys);
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

uint32_t ssr_scene_mix_colour(const char *colour, uint32_t parent) {
    Colour own, under;
    if (!parse_colour(colour, &own)) return parent;
    under.r = (float)(parent >> 24);
    under.g = (float)((parent >> 16) & 0xFF);
    under.b = (float)((parent >> 8) & 0xFF);
    under.a = (parent & 0xFF) / 255.0f;
    under.mr = under.mg = under.mb = under.ma = 0;
    return colour_rgba(colour_mix(own, under));
}

static const SSRSceneItem *hit(const SSRScene *scene, float x, float y, int clicksOnly) {
    if (!scene) return NULL;
    for (int i = scene->count - 1; i >= 0; i--) {
        const SSRSceneItem *item = &scene->items[i];
        if (clicksOnly ? !item->click : !item->ref) continue;
        if (item->clipped && (x < item->clipX || x >= item->clipX + item->clipW || y < item->clipY ||
                              y >= item->clipY + item->clipH)) {
            continue;
        }
        float x0 = item->w < 0 ? item->x + item->w : item->x, x1 = item->w < 0 ? item->x : item->x + item->w;
        float y0 = item->h < 0 ? item->y + item->h : item->y, y1 = item->h < 0 ? item->y : item->y + item->h;
        if (x >= x0 && x < x1 && y >= y0 && y < y1) return item;
    }
    return NULL;
}

const SSRSceneItem *ssr_scene_hit_item(const SSRScene *scene, float x, float y) {
    return hit(scene, x, y, 1);
}

const char *ssr_scene_click(SSRScene *scene, const SSRSceneEnv *env, const SSRSceneItem *item) {
    if (!scene || !env || !item || !item->click) return NULL;
    Gel *g = (Gel *)item->owner;
    if (g) {
        scene->env = env;
        pump(scene, g, g->clickChain);
        scene->env = NULL;
    }
    return item->click;
}

const char *ssr_scene_take_sound(SSRScene *scene) {
    if (!scene || scene->soundCount == 0) return NULL;
    const char *sound = scene->sounds[0];
    memmove(scene->sounds, scene->sounds + 1, (size_t)(scene->soundCount - 1) * sizeof(const char *));
    scene->soundCount--;
    return sound;
}

const char *ssr_scene_hit(const SSRScene *scene, float x, float y) {
    const SSRSceneItem *item = hit(scene, x, y, 0);
    return item ? item->ref : NULL;
}

// The first gel with the ref, in the order of the file: the outer gel of a
// template, not a line inside it with the same ref.
static Gel *find_ref_first(const SSRScene *s, const char *ref) {
    for (int i = 0; i < s->refCount; i++) {
        if (strcmp(s->refs[i]->ref, ref) == 0) return s->refs[i];
    }
    return NULL;
}

static Gel *first_text(Gel *g) {
    if (g->kind == GEL_TEXT) return g;
    for (int i = 0; i < g->childCount; i++) {
        Gel *found = first_text(g->children[i]);
        if (found) return found;
    }
    return NULL;
}

int ssr_scene_set_text(SSRScene *scene, const char *ref, const char *text) {
    if (!scene || !ref || !text) return 0;
    int count = 0;
    for (int i = 0; i < scene->refCount; i++) {
        if (strcmp(scene->refs[i]->ref, ref) != 0) continue;
        Gel *g = first_text(scene->refs[i]);
        if (g) {
            set_shown(g, text);
            count++;
        }
    }
    return count;
}

int ssr_scene_set_visible(SSRScene *scene, const char *ref, int visible) {
    if (!scene || !ref) return 0;
    int count = 0;
    for (int i = 0; i < scene->refCount; i++) {
        if (strcmp(scene->refs[i]->ref, ref) != 0) continue;
        scene->refs[i]->hidden = !visible;
        count++;
    }
    return count;
}

int ssr_scene_set_colour(SSRScene *scene, const char *ref, const char *colour) {
    if (!scene || !ref || !colour) return 0;
    char text[24];
    snprintf(text, sizeof(text), "#%s", colour[0] == '#' ? colour + 1 : colour);
    int count = 0;
    for (int i = 0; i < scene->refCount; i++) {
        if (strcmp(scene->refs[i]->ref, ref) != 0) continue;
        scene->refs[i]->hasColour = parse_colour(text, &scene->refs[i]->colour);
        count++;
    }
    return count;
}

void ssr_scene_list_clear(SSRScene *scene, const char *list) {
    Gel *g = scene && list ? find_ref_first(scene, list) : NULL;
    if (!g) return;
    for (int i = 0; i < g->childCount; i++) free_gel(g->children[i]);
    g->childCount = 0;
    g->listY = 0;
}

static Doodad *find_doodad_ref(Doodad *d, const char *ref) {
    for (; d; d = d->next) {
        if (d->ref && strcmp(d->ref, ref) == 0) return d;
        Doodad *inner = find_doodad_ref(d->chain, ref);
        if (!inner) inner = find_doodad_ref(d->other, ref);
        if (inner) return inner;
    }
    return NULL;
}

int ssr_scene_list_add(SSRScene *scene, const char *list, const char *template_, const char *text, int id) {
    Gel *g = scene && list ? find_ref_first(scene, list) : NULL;
    Gel *model = scene && template_ ? find_ref_first(scene, template_) : NULL;
    if (!g || !model) return 0;
    Gel *entry = clone_gel(model);
    if (!entry) return 0;
    // An entry goes below the one before it (c_ACombo.Add).
    entry->x = 10;
    entry->y = g->listY;
    entry->hidden = 0;
    g->listY += entry->h;
    add_child(g, entry);
    Gel *label = first_text(entry);
    if (label && text) set_shown(label, text);
    // The SetTweak with the ref ID gives the id to the tweak of the list.
    Doodad *d = find_doodad_ref(entry->clickChain, "ID");
    if (!d) d = find_doodad_ref(entry->doodads, "ID");
    if (d && d->kind == D_SET_TWEAK) {
        char value[32];
        snprintf(value, sizeof(value), "%d", id);
        d->text[2] = keep(scene, value);
    }
    return 1;
}

int ssr_scene_dragged(const SSRScene *scene) {
    return scene ? scene->dragged : 0;
}

int ssr_scene_spawn(SSRScene *scene, const char *parent, const char *template_, float x, const char *text,
                    float width) {
    Gel *holder = scene && parent ? find_ref_first(scene, parent) : NULL;
    Gel *model = scene && template_ ? find_ref_first(scene, template_) : NULL;
    if (!holder || !model) return 0;
    int id = 0;
    for (int i = 0; i < 64 && !id; i++) {
        if (!scene->spawned[i]) id = i + 1;
    }
    if (!id) return 0;
    Gel *copy = clone_gel(model);
    if (!copy) return 0;
    copy->hidden = 0;
    // The copy keeps its place on the screen in its new parent
    // (c_Gel.AddChild rebases it). x is on the screen too.
    copy->x = x;
    set_local(copy, mat_concat(mat_inverse(holder->baked), local_matrix(copy)));
    Gel *label = first_text(copy);
    if (label && text) set_shown(label, text);
    // c_GGadget.SetElementWidth(0) and (1): the gel and its text.
    if (width > 0) {
        copy->w = width;
        if (label) label->w = width;
    }
    add_child(holder, copy);
    scene->spawned[id - 1] = copy;
    return id;
}

float ssr_scene_spawn_x(const SSRScene *scene, int id) {
    return scene && id > 0 && id <= 64 && scene->spawned[id - 1] ? scene->spawned[id - 1]->x : 0;
}

void ssr_scene_spawn_remove(SSRScene *scene, int id) {
    if (!scene || id < 1 || id > 64 || !scene->spawned[id - 1]) return;
    Gel *g = scene->spawned[id - 1];
    scene->spawned[id - 1] = NULL;
    Gel *p = g->parent;
    if (p) {
        for (int i = 0; i < p->childCount; i++) {
            if (p->children[i] == g) {
                memmove(&p->children[i], &p->children[i + 1], (size_t)(p->childCount - i - 1) * sizeof(Gel *));
                p->childCount--;
                break;
            }
        }
    }
    free_gel(g);
}

// The gel with the ref in g and its children, or NULL.
static Gel *find_below(Gel *g, const char *ref) {
    if (g->ref && strcmp(g->ref, ref) == 0) return g;
    for (int i = 0; i < g->childCount; i++) {
        Gel *found = find_below(g->children[i], ref);
        if (found) return found;
    }
    return NULL;
}

static Gel *spawned(const SSRScene *scene, int id) {
    return scene && id > 0 && id <= 64 ? scene->spawned[id - 1] : NULL;
}

void ssr_scene_spawn_place(SSRScene *scene, int id, const char *ref, float x, float y) {
    Gel *g = spawned(scene, id);
    if (g && ref) g = find_below(g, ref);
    if (!g) return;
    g->x = x;
    g->y = y;
}

void ssr_scene_spawn_anim(SSRScene *scene, int id, const char *ref, const char *anim) {
    Gel *g = spawned(scene, id);
    if (g && ref) g = find_below(g, ref);
    if (!g || g->kind != GEL_ANIM || !anim) return;
    if (g->animName && strcmp(g->animName, anim) == 0) return;
    set_animation(g, keep(scene, anim));
}

void ssr_scene_spawn_show(SSRScene *scene, int id, const char *ref, int visible) {
    Gel *g = spawned(scene, id);
    if (g && ref) g = find_below(g, ref);
    if (g) g->hidden = !visible;
}

void ssr_scene_place(SSRScene *scene, const char *ref, float x, float y, float scale) {
    Gel *g = scene && ref ? find_ref_first(scene, ref) : NULL;
    if (!g) return;
    g->x = x;
    g->y = y;
    g->sx = g->sy = scale;
}

void ssr_scene_spawn_clone(SSRScene *scene, int id, const char *ref, const char *group, const char *name) {
    Gel *g = spawned(scene, id);
    if (!g || !ref) return;
    char source[160];
    snprintf(source, sizeof(source), "%s\x1f%s", group, name);
    // The CloneTweak doodads with the ref in the copy and in its children.
    Gel *stack[64];
    int count = 0;
    stack[count++] = g;
    while (count > 0) {
        Gel *at = stack[--count];
        for (Doodad *d = at->doodads; d; d = d->next) {
            if (d->kind == D_CLONE_TWEAK && d->ref && strcmp(d->ref, ref) == 0) d->text[0] = keep(scene, source);
        }
        for (int i = 0; i < at->childCount && count < 64; i++) stack[count++] = at->children[i];
    }
}

// The last entry that the code added to the list, or NULL.
static Gel *last_entry(const SSRScene *scene, const char *list) {
    Gel *g = scene && list ? find_ref_first(scene, list) : NULL;
    return g && g->childCount > 0 ? g->children[g->childCount - 1] : NULL;
}

// The element with the number in the entry: the gels with the ref of the
// entry, in the order of the file (c_GGadget.GetElement). 0 is the entry.
static Gel *nth_element(Gel *g, const char *ref, int *index) {
    if (g->ref && ref && strcmp(g->ref, ref) == 0) {
        if (*index == 0) return g;
        (*index)--;
    }
    for (int i = 0; i < g->childCount; i++) {
        Gel *found = nth_element(g->children[i], ref, index);
        if (found) return found;
    }
    return NULL;
}

void ssr_scene_list_colour(SSRScene *scene, const char *list, int element, const char *colour) {
    Gel *entry = last_entry(scene, list);
    if (!entry || !colour) return;
    int index = element;
    Gel *g = nth_element(entry, entry->ref, &index);
    char text[24];
    snprintf(text, sizeof(text), "#%s", colour[0] == '#' ? colour + 1 : colour);
    if (g) g->hasColour = parse_colour(text, &g->colour);
}

// The gels with the ref in g: the text takes the text, and the gels show
// or hide.
static void change_sub(Gel *g, const char *ref, const char *text, int visible) {
    if (g->ref && strcmp(g->ref, ref) == 0) {
        g->hidden = !visible;
        if (text && g->kind == GEL_TEXT) set_shown(g, text);
    }
    for (int i = 0; i < g->childCount; i++) change_sub(g->children[i], ref, text, visible);
}

void ssr_scene_list_sub(SSRScene *scene, const char *list, const char *ref, const char *text, int visible) {
    Gel *entry = last_entry(scene, list);
    if (entry && ref) change_sub(entry, ref, text, visible);
}

// The gel of the copy: ref, or "ref#n" for the element n of the ref (0 is
// the first gel with the ref).
static Gel *spawn_part(const SSRScene *scene, int id, const char *ref) {
    Gel *g = spawned(scene, id);
    if (!g || !ref) return g;
    const char *hash = strchr(ref, '#');
    if (!hash) return find_below(g, ref);
    char name[64];
    snprintf(name, sizeof(name), "%.*s", (int)(hash - ref), ref);
    int index = atoi(hash + 1);
    return nth_element(g, name, &index);
}

void ssr_scene_spawn_set(SSRScene *scene, int id, const char *ref, SSRGelProperty property, float value) {
    Gel *g = spawn_part(scene, id, ref);
    if (g) ex_set(scene, g, property, value);
}

void ssr_scene_spawn_colour(SSRScene *scene, int id, const char *ref, const char *colour) {
    Gel *g = spawn_part(scene, id, ref);
    if (!g || !colour) return;
    char text[24];
    snprintf(text, sizeof(text), "#%s", colour[0] == '#' ? colour + 1 : colour);
    g->hasColour = parse_colour(text, &g->colour);
}

// The AnimInstance gels with the ref, from the gel down.
static void anim_speed(Gel *g, const char *ref, float speed, int toEnd) {
    if (g->kind == GEL_ANIM && g->ref && strcmp(g->ref, ref) == 0) {
        g->speed = speed;
        if (toEnd && g->animation) g->frame = g->animation->length - 1;
    }
    for (int i = 0; i < g->childCount; i++) anim_speed(g->children[i], ref, speed, toEnd);
}

void ssr_scene_anim(SSRScene *scene, const char *ref, float speed, int toEnd) {
    if (!scene || !ref) return;
    if (scene->root) anim_speed(scene->root, ref, speed, toEnd);
    for (int i = 0; i < scene->detachedCount; i++) anim_speed(scene->detached[i], ref, speed, toEnd);
}
