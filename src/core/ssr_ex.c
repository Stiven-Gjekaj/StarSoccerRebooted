#include "ssr_ex.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    // Values.
    K_NUMBER,
    K_TEMPFLOAT,
    K_WORD,
    K_STRING,
    K_COLOUR,
    K_TRUE,
    K_FALSE,
    K_NULL,
    K_TWEAK,
    K_TWEAK_RANGE,
    K_VAR,
    K_GEL,
    K_FRAMETIME,
    K_TOUCHX,
    K_TOUCHY,
    K_TOUCHHIT,
    K_TOUCHDOWN,
    K_TOUCHRELEASED,
    // Brackets.
    K_OPEN,
    K_CLOSE,
    K_SEPARATOR,
    // Infix operators.
    K_PLUS,
    K_MINUS,
    K_MULTIPLY,
    K_DIVIDE,
    K_FMOD,
    K_CONCAT,
    K_EQUAL,
    K_NOT_EQUAL,
    K_LESS,
    K_GREATER,
    K_LESS_EQUAL,
    K_GREATER_EQUAL,
    K_AND,
    K_OR,
    // Functions.
    K_NOT,
    K_ABS,
    K_SIN,
    K_COS,
    K_SQRT,
    K_LOC,
    K_SHELL,
    K_MAX,
    K_MIN,
    K_FMT,
    K_TRIMSTR,
    K_ATAN2,
    K_LERP,
    K_FRLERP,
    K_COLLERP,
    K_CURVE,
    K_IMGDOWNLOADED,
    K_ONEOF,
    K_NOTONEOF,
    K_CONTAINS,
    K_ENDSWITH,
    K_REP,
} Kind;

struct SSRExNode {
    Kind kind;
    int complete;
    SSRExNode **kids;
    int kidCount;
    // K_NUMBER: the value, and the text of the token until the parse ends.
    float f;
    char *s;
    SSRExColour c;
    // K_OPEN: a bracket that becomes a tuple.
    int tuple;
    SSRExNode **contents;
    int contentCount;
    // K_TWEAK: the tweak, its kind, and a colour part 'r', 'g', 'b', 'a'.
    char *group, *name;
    SSRTweakKind tweakKind;
    char part;
    SSRExVar *var;
    SSRGelProperty property;
    int mask;
    // K_STRING made by a fold, not a token (c_EX_String).
    int folded;
    // The list of the parse.
    SSRExNode *prev, *next;
};

static const SSRExColour WHITE = {255, 255, 255, 1, 0, 0, 0, 0};

/* ------------------------------------------------------------ nodes */

static SSRExNode *node_new(Kind kind) {
    SSRExNode *n = calloc(1, sizeof(SSRExNode));
    if (!n) return NULL;
    n->kind = kind;
    n->c = WHITE;
    return n;
}

static char *copy_text(const char *text, size_t length) {
    char *s = malloc(length + 1);
    if (!s) return NULL;
    memcpy(s, text, length);
    s[length] = 0;
    return s;
}

void ssr_ex_free(SSRExNode *n) {
    if (!n) return;
    for (int i = 0; i < n->kidCount; i++) ssr_ex_free(n->kids[i]);
    free(n->kids);
    for (int i = 0; i < n->contentCount; i++) ssr_ex_free(n->contents[i]);
    free(n->contents);
    free(n->s);
    free(n->group);
    free(n->name);
    free(n);
}

static int is_value(Kind k) {
    return k <= K_TOUCHRELEASED;
}

static int is_infix(Kind k) {
    return k >= K_PLUS && k <= K_OR;
}

static int is_bool_infix(Kind k) {
    return k >= K_EQUAL && k <= K_OR;
}

static int is_float_comparison(Kind k) {
    return k >= K_LESS && k <= K_GREATER_EQUAL;
}

static int is_float_op(Kind k) {
    return k >= K_PLUS && k <= K_FMOD;
}

static int is_list_test(Kind k) {
    return k >= K_ONEOF && k <= K_ENDSWITH;
}

static int params_before(const SSRExNode *n) {
    if (is_infix(n->kind) || is_list_test(n->kind) || n->kind == K_REP) return 1;
    return 0;
}

static int params_after(const SSRExNode *n) {
    switch (n->kind) {
    case K_TWEAK_RANGE:
        return n->complete ? 0 : 1;
    case K_NOT:
    case K_ABS:
    case K_SIN:
    case K_COS:
    case K_SQRT:
    case K_LOC:
    case K_SHELL:
        return 1;
    case K_MAX:
    case K_MIN:
    case K_FMT:
    case K_TRIMSTR:
    case K_ATAN2:
        return 2;
    case K_LERP:
    case K_FRLERP:
    case K_COLLERP:
    case K_CURVE:
        return 3;
    default:
        return is_infix(n->kind) ? 1 : 0;
    }
}

// The functions that take their arguments from a tuple after them.
static int takes_tuple(const SSRExNode *n) {
    return n->kind == K_IMGDOWNLOADED || is_list_test(n->kind) || n->kind == K_REP ||
           (n->kind == K_GEL && n->property == SSR_GEL_SLOTCOL);
}

static int block_left(const SSRExNode *n) {
    return n->kind == K_CLOSE || n->kind == K_SEPARATOR || (is_bool_infix(n->kind) && !n->complete);
}

static int block_right(const SSRExNode *n) {
    return n->kind == K_OPEN || n->kind == K_SEPARATOR || (is_bool_infix(n->kind) && !n->complete);
}

int ssr_ex_is_const(const SSRExNode *n) {
    switch (n->kind) {
    case K_NUMBER:
    case K_TEMPFLOAT:
    case K_WORD:
    case K_STRING:
    case K_COLOUR:
    case K_TRUE:
    case K_FALSE:
    case K_NULL:
        return 1;
    default:
        return 0;
    }
}

// These functions never fold: their Bake does nothing (lerp, sin, ...).
static int never_folds(Kind k) {
    return k == K_LERP || k == K_FRLERP || k == K_COLLERP || k == K_CURVE || k == K_ABS || k == K_SIN ||
           k == K_COS || k == K_SQRT || k == K_ATAN2 || k == K_SHELL || k == K_IMGDOWNLOADED;
}

/* --------------------------------------------------------- the parse */

typedef struct {
    SSRExNode *first;
    int changed;
    const SSRExHost *host;
} List;

static void list_remove(List *l, SSRExNode *n) {
    if (n->prev) {
        n->prev->next = n->next;
    } else {
        l->first = n->next;
    }
    if (n->next) n->next->prev = n->prev;
    n->prev = n->next = NULL;
}

static void list_insert_after(List *l, SSRExNode *at, SSRExNode *n) {
    n->prev = at;
    n->next = at ? at->next : l->first;
    if (n->next) n->next->prev = n;
    if (at) {
        at->next = n;
    } else {
        l->first = n;
    }
}

static void list_insert_before(List *l, SSRExNode *at, SSRExNode *n) {
    list_insert_after(l, at->prev, n);
}

static void list_replace(List *l, SSRExNode *old, SSRExNode *n) {
    n->prev = old->prev;
    n->next = old->next;
    if (n->prev) {
        n->prev->next = n;
    } else {
        l->first = n;
    }
    if (n->next) n->next->prev = n;
    old->prev = old->next = NULL;
}

static void add_kid(SSRExNode *n, SSRExNode *kid) {
    SSRExNode **grown = realloc(n->kids, (size_t)(n->kidCount + 1) * sizeof(SSRExNode *));
    if (!grown) return;
    n->kids = grown;
    n->kids[n->kidCount++] = kid;
}

static SSRExNode *evaluate_const(const SSRExNode *n, const SSRExHost *host);

// c_Expression.Bake: a node with only constant children becomes its value.
static void bake(List *l, SSRExNode *n) {
    if (!n->complete || never_folds(n->kind) || is_value(n->kind) || n->kind == K_OPEN) return;
    for (int i = 0; i < n->kidCount; i++) {
        if (!ssr_ex_is_const(n->kids[i])) return;
    }
    SSRExNode *value = evaluate_const(n, l->host);
    if (!value) return;
    l->changed = 1;
    list_replace(l, n, value);
    ssr_ex_free(n);
}

static int complete_ahead(const SSRExNode *n, int count) {
    for (const SSRExNode *p = n->next; count > 0; count--, p = p->next) {
        if (!p || !p->complete) return 0;
    }
    return 1;
}

static int complete_behind(const SSRExNode *n, int count) {
    for (const SSRExNode *p = n->prev; count > 0; count--, p = p->prev) {
        if (!p || !p->complete) return 0;
    }
    return 1;
}

// c_Expression.Collapse: the node takes its operands from its neighbours.
static SSRExNode *collapse_default(List *l, SSRExNode *n) {
    int before = params_before(n), after = params_after(n);
    if (!n->complete && complete_ahead(n, after) && complete_behind(n, before)) {
        l->changed = 1;
        SSRExNode *taken[8];
        for (int i = before - 1; i >= 0; i--) {
            taken[i] = n->prev;
            list_remove(l, n->prev);
        }
        for (int i = before; i < before + after; i++) {
            taken[i] = n->next;
            list_remove(l, n->next);
        }
        for (int i = 0; i < before + after; i++) add_kid(n, taken[i]);
        n->complete = 1;
        SSRExNode *next = n->next;
        bake(l, n);
        return next;
    }
    return n->next;
}

// A function with a tuple after it (oneof, rep, imgdownloaded, slotcol).
static SSRExNode *collapse_tuple(List *l, SSRExNode *n) {
    SSRExNode *t = n->next;
    if (n->complete || !t || t->kind != K_OPEN || !t->tuple || !t->complete) return n->next;
    int before = params_before(n);
    if (!complete_behind(n, before)) return t;
    if (before) {
        SSRExNode *left = n->prev;
        list_remove(l, left);
        add_kid(n, left);
    }
    for (int i = 0; i < t->contentCount; i++) add_kid(n, t->contents[i]);
    t->contentCount = 0;
    list_remove(l, t);
    ssr_ex_free(t);
    n->complete = 1;
    l->changed = 1;
    SSRExNode *next = n->next;
    bake(l, n);
    return next;
}

static SSRExNode *collapse(List *l, SSRExNode *n) {
    switch (n->kind) {
    case K_OPEN:
        if (n->complete) return n->next;
        // An empty argument gets a Null.
        if (n->next && block_left(n->next)) {
            l->changed = 1;
            list_insert_after(l, n, node_new(K_NULL));
            n->next->complete = 1;
        }
        return n->next;
    case K_CLOSE: {
        int count = 0;
        SSRExNode *p = n->prev;
        while (p && p->complete) {
            p = p->prev;
            count++;
        }
        if (!p || p->kind != K_OPEN) return n->next;
        l->changed = 1;
        SSRExNode *after = n->next, *before = p->prev;
        if (p->tuple) {
            // The tuple holds the values between its brackets.
            p->contents = calloc((size_t)(count ? count : 1), sizeof(SSRExNode *));
            for (int i = 0; i < count && p->contents; i++) {
                SSRExNode *v = p->next;
                list_remove(l, v);
                p->contents[p->contentCount++] = v;
            }
            p->complete = 1;
        } else {
            list_remove(l, p);
            ssr_ex_free(p);
        }
        list_remove(l, n);
        ssr_ex_free(n);
        // A function before a plain bracket can take its arguments now.
        if (before && params_after(before) > 0 && !before->complete && !p->tuple) return before;
        if (p->tuple && before && takes_tuple(before) && !before->complete) return before;
        return after;
    }
    case K_SEPARATOR: {
        if (n->next && block_left(n->next)) {
            l->changed = 1;
            SSRExNode *null = node_new(K_NULL);
            null->complete = 1;
            list_insert_after(l, n, null);
        }
        if (n->prev && !n->prev->complete) return n->next;
        if (n->next && !n->next->complete) return n->next;
        l->changed = 1;
        SSRExNode *next = n->next;
        list_remove(l, n);
        ssr_ex_free(n);
        return next;
    }
    case K_MINUS:
        if (!n->complete) {
            SSRExNode *p = n->prev, *x = n->next;
            int unary = !p || block_right(p) || (is_float_op(p->kind) && !p->complete);
            if (unary && x && x->complete) {
                // Unary minus is 0 - x (c_EX_Minus).
                l->changed = 1;
                SSRExNode *zero = node_new(K_NUMBER);
                zero->complete = 1;
                list_insert_before(l, n, zero);
            }
            return collapse_default(l, n);
        }
        return n->next;
    case K_PLUS:
        if (!n->complete) {
            SSRExNode *x = n->next, *p = n->prev;
            int drop = x ? block_left(x) : 0;
            if (x && !x->complete && (x->kind == K_MINUS || x->kind == K_PLUS)) drop = 1;
            if (p && block_right(p)) drop = 1;
            if (drop) {
                // A unary plus goes away.
                l->changed = 1;
                list_remove(l, n);
                ssr_ex_free(n);
                return x;
            }
            return collapse_default(l, n);
        }
        return n->next;
    default:
        break;
    }
    if (is_float_comparison(n->kind) && !n->complete) {
        // "0 < x < 5" is "(0 < x) and (x < 5)" (c_EX_FloatComparison).
        SSRExNode *p = n->prev, *x = n->next;
        if (p && is_float_comparison(p->kind) && p->complete) {
            if (x && x->complete) {
                l->changed = 1;
                SSRExNode *copy = ssr_ex_clone(p->kids[1], NULL, NULL);
                list_remove(l, x);
                add_kid(n, copy);
                add_kid(n, x);
                n->complete = 1;
                SSRExNode *and = node_new(K_AND);
                list_insert_after(l, n, and);
                // The order is p, and, n.
                list_remove(l, n);
                list_insert_after(l, and, n);
                return and;
            }
            return n->next;
        }
    }
    if (takes_tuple(n)) return collapse_tuple(l, n);
    return collapse_default(l, n);
}

// c_EX_InlineOp.ForceComplete: an operator with no operands is text.
static SSRExNode *force_complete(List *l, SSRExNode *n) {
    if (!is_infix(n->kind) || n->complete) return n->next;
    static const char *TOKENS[] = {"+", "-", "*", "/", "fmod", "::", "=", "!=", "<", ">", "<=", ">=", "and", "or"};
    const char *token = TOKENS[n->kind - K_PLUS];
    l->changed = 1;
    if (n->prev && n->prev->kind == K_STRING && n->prev->folded) {
        size_t length = strlen(n->prev->s);
        char *grown = realloc(n->prev->s, length + strlen(token) + 1);
        if (grown) {
            strcpy(grown + length, token);
            n->prev->s = grown;
        }
        SSRExNode *next = n->next;
        list_remove(l, n);
        ssr_ex_free(n);
        return next;
    }
    SSRExNode *text = node_new(K_STRING);
    text->s = copy_text(token, strlen(token));
    text->complete = 1;
    text->folded = 1;
    list_replace(l, n, text);
    ssr_ex_free(n);
    return text->next;
}

/* --------------------------------------------------------- tokens */

typedef struct {
    const char *text;
    Kind kind;
} Keyword;

// The keywords of c_Phrase.m_words that the remake reads. The longest one
// that starts the text wins.
static const Keyword KEYWORDS[] = {
    {"(", K_OPEN},         {")", K_CLOSE},          {"[", K_OPEN},           {"]", K_CLOSE},
    {",", K_SEPARATOR},    {"true", K_TRUE},        {"false", K_FALSE},      {"and", K_AND},
    {"or", K_OR},          {"not", K_NOT},          {">=", K_GREATER_EQUAL}, {"<=", K_LESS_EQUAL},
    {">", K_GREATER},      {"<", K_LESS},           {"=", K_EQUAL},          {"!=", K_NOT_EQUAL},
    {"oneof", K_ONEOF},    {"notoneof", K_NOTONEOF}, {"contains", K_CONTAINS}, {"endswith", K_ENDSWITH},
    {"lerp", K_LERP},      {"collerp(", K_COLLERP}, {"frlerp", K_FRLERP},    {"curve", K_CURVE},
    {"+", K_PLUS},         {"-", K_MINUS},          {"*", K_MULTIPLY},       {"/", K_DIVIDE},
    {"fmod", K_FMOD},      {"sqrt(", K_SQRT},       {"sin(", K_SIN},         {"cos(", K_COS},
    {"abs", K_ABS},        {"atan2", K_ATAN2},      {"max(", K_MAX},         {"min(", K_MIN},
    {"Max[", K_MAX},       {"Min[", K_MIN},         {"::", K_CONCAT},        {"fmt(", K_FMT},
    {"trimstr(", K_TRIMSTR}, {"loc(", K_LOC},       {"rep(", K_REP},         {"shell(", K_SHELL},
    {"imgdownloaded", K_IMGDOWNLOADED}, {"FrameTime", K_FRAMETIME}, {"TouchX", K_TOUCHX},
    {"TouchY", K_TOUCHY},  {"TouchHit", K_TOUCHHIT}, {"TouchDown", K_TOUCHDOWN},
    {"TouchReleased", K_TOUCHRELEASED},
};

typedef struct {
    const char *text;
    SSRGelProperty property;
} Property;

static const Property PROPERTIES[] = {
    {"vis", SSR_GEL_VIS},           {"text.shadcol", SSR_GEL_TEXT_SHADCOL}, {"text", SSR_GEL_TEXT},
    {"colour", SSR_GEL_COLOUR},     {"slotcol", SSR_GEL_SLOTCOL},         {"x", SSR_GEL_X},
    {"y", SSR_GEL_Y},               {"w", SSR_GEL_W},                     {"h", SSR_GEL_H},
    {"hx", SSR_GEL_HX},             {"hy", SSR_GEL_HY},                   {"gx", SSR_GEL_GX},
    {"gy", SSR_GEL_GY},             {"angle", SSR_GEL_ANGLE},             {"alpha", SSR_GEL_ALPHA},
    {"scalex", SSR_GEL_SCALEX},     {"scaley", SSR_GEL_SCALEY},           {"scale", SSR_GEL_SCALE},
    {"depth", SSR_GEL_DEPTH},       {"img", SSR_GEL_IMG},                 {"anim.speed", SSR_GEL_ANIM_SPEED},
    {"anim.time", SSR_GEL_ANIM_TIME}, {"anim.normtime", SSR_GEL_ANIM_NORMTIME},
    {"anim.playing", SSR_GEL_ANIM_PLAYING}, {"anim", SSR_GEL_ANIM},
};

// MathUtils.ParseFloat2: digits and '.', with a float step of 0.1f for
// each digit after the point.
static float parse_float2(const char *text, size_t length) {
    float value = 0, step = 1;
    int fraction = 0;
    for (size_t i = 0; i < length; i++) {
        char ch = text[i];
        if (ch == '.') {
            fraction = 1;
            continue;
        }
        if (ch < '0' || ch > '9') break;
        if (fraction) {
            step *= 0.1f;
            value += (float)(ch - '0') * step;
        } else {
            value = value * 10 + (float)(ch - '0');
        }
    }
    return value;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// c_GColour.SetFromString: 6 digits RRGGBB, 8 RRGGBBAA, 16 with the mix.
static SSRExColour parse_colour(const char *hex, size_t length) {
    SSRExColour c = WHITE;
    if (length < 6) return c;
    int v[16] = {0};
    for (size_t i = 0; i < length && i < 16; i++) v[i] = hex_digit(hex[i]);
    c.r = (float)(v[0] * 16 + v[1]);
    c.g = (float)(v[2] * 16 + v[3]);
    c.b = (float)(v[4] * 16 + v[5]);
    if (length >= 8) c.a = (float)(v[6] * 16 + v[7]) / 255.0f;
    if (length >= 16) {
        c.mr = (float)(v[8] * 16 + v[9]) / 255.0f;
        c.mg = (float)(v[10] * 16 + v[11]) / 255.0f;
        c.mb = (float)(v[12] * 16 + v[13]) / 255.0f;
        c.ma = (float)(v[14] * 16 + v[15]) / 255.0f;
    }
    return c;
}

// One token at *at. Returns NULL when the token gives no node (a missing
// tweak or variable vanishes). *request is 1 when the next '(' is a tuple.
static SSRExNode *token(const char **at, const SSRExHost *host, int *request) {
    const char *p = *at;
    *request = 0;
    // twk(Cat,Name), read as one token (c_EX_Tweak).
    if (strncmp(p, "twk(", 4) == 0) {
        const char *comma = strchr(p + 4, ',');
        const char *close = comma ? strchr(comma + 1, ')') : NULL;
        if (!comma || !close) {
            *at = p + 4;
            return NULL;
        }
        char *group = copy_text(p + 4, (size_t)(comma - p - 4));
        char *name = copy_text(comma + 1, (size_t)(close - comma - 1));
        *at = close + 1;
        SSRTweakKind kind = host && host->tweaks ? ssr_tweaks_kind(host->tweaks, group, name) : SSR_TWEAK_NONE;
        if (kind == SSR_TWEAK_NONE) {
            free(group);
            free(name);
            return NULL;
        }
        SSRExNode *n = node_new(kind == SSR_TWEAK_RANGE ? K_TWEAK_RANGE : K_TWEAK);
        n->group = group;
        n->name = name;
        n->tweakKind = kind;
        n->complete = kind != SSR_TWEAK_RANGE;
        if (kind == SSR_TWEAK_COLOUR && (*at)[0] == '.' && strchr("rgba", (*at)[1]) && (*at)[1]) {
            n->part = (*at)[1];
            *at += 2;
        }
        return n;
    }
    if (strncmp(p, "gel.", 4) == 0) {
        const Property *best = NULL;
        for (size_t i = 0; i < sizeof(PROPERTIES) / sizeof(PROPERTIES[0]); i++) {
            size_t length = strlen(PROPERTIES[i].text);
            if (strncmp(p + 4, PROPERTIES[i].text, length) == 0 && (!best || length > strlen(best->text))) {
                best = &PROPERTIES[i];
            }
        }
        if (!best) {
            *at = p + 4;
            return NULL;
        }
        SSRExNode *n = node_new(K_GEL);
        n->property = best->property;
        p += 4 + strlen(best->text);
        n->complete = 1;
        if (best->property == SSR_GEL_SLOTCOL) {
            // ".RGBA" and ".rgba" choose the parts that change.
            n->mask = 255;
            if (*p == '.') {
                n->mask = 0;
                p++;
                static const char *LETTERS = "RGBArgba";
                for (const char *q; *p && (q = strchr(LETTERS, *p)) != NULL; p++) n->mask |= 1 << (q - LETTERS);
            }
            n->complete = 0;
            *request = 1;
        }
        *at = p;
        return n;
    }
    if (*p == '"') {
        const char *end = strchr(p + 1, '"');
        if (!end) {
            *at = p + strlen(p);
            return NULL;
        }
        SSRExNode *n = node_new(K_WORD);
        n->s = copy_text(p + 1, (size_t)(end - p - 1));
        n->complete = 1;
        *at = end + 1;
        return n;
    }
    if (*p == '#') {
        size_t length = 1;
        while (hex_digit(p[length]) >= 0) length++;
        SSRExNode *n = node_new(K_COLOUR);
        n->c = parse_colour(p + 1, length - 1);
        n->complete = 1;
        *at = p + length;
        return n;
    }
    if (*p == '@') {
        SSRExNode *n = node_new(K_TEMPFLOAT);
        n->f = host && host->simple_var && p[1] ? host->simple_var(host->context, p[1]) : 0;
        n->complete = 1;
        *at = p + (p[1] ? 2 : 1);
        return n;
    }
    if (*p == '%') {
        size_t length = 1;
        while (isalnum((unsigned char)p[length]) || p[length] == '_') length++;
        char name[64];
        snprintf(name, sizeof(name), "%.*s", (int)length, p);
        *at = p + length;
        SSRExVar *var = host && host->find_var ? host->find_var(host->context, name) : NULL;
        if (!var) return NULL;
        SSRExNode *n = node_new(K_VAR);
        n->var = var;
        n->complete = 1;
        return n;
    }
    // A number: digits and '.', not followed by a letter or '_'.
    if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
        size_t length = 0;
        while (isdigit((unsigned char)p[length]) || p[length] == '.') length++;
        if (!isalpha((unsigned char)p[length]) && p[length] != '_') {
            SSRExNode *n = node_new(*p == '.' ? K_TEMPFLOAT : K_NUMBER);
            n->f = parse_float2(p, length);
            n->s = copy_text(p, length);
            n->complete = 1;
            *at = p + length;
            return n;
        }
    }
    const Keyword *best = NULL;
    for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        size_t length = strlen(KEYWORDS[i].text);
        if (strncmp(p, KEYWORDS[i].text, length) == 0 && (!best || length > strlen(best->text))) best = &KEYWORDS[i];
    }
    if (best) {
        size_t length = strlen(best->text);
        // A keyword that ends in '(' leaves the '(' for the next token.
        if (length > 1 && (best->text[length - 1] == '(' || best->text[length - 1] == '[')) length--;
        *at = p + length;
        if (best->kind == K_DIVIDE && p[1] == '/') {
            // "//" ends the phrase.
            *at = p + strlen(p);
            return NULL;
        }
        SSRExNode *n = node_new(best->kind);
        n->complete = is_value(best->kind);
        if (best->kind == K_REP || best->kind == K_IMGDOWNLOADED || is_list_test(best->kind)) *request = 1;
        return n;
    }
    // A bare word ends at ',' ')' '=' "!=" "::" or a tab (c_EX_TempExp).
    size_t length = 0;
    while (p[length] && p[length] != ',' && p[length] != ')' && p[length] != '=' && p[length] != '\t' &&
           strncmp(p + length, "!=", 2) != 0 && strncmp(p + length, "::", 2) != 0) {
        length++;
    }
    if (length == 0) length = 1;
    SSRExNode *n = node_new(K_WORD);
    n->s = copy_text(p, length);
    n->complete = 1;
    *at = p + length;
    return n;
}

// c_Expression.Retain: the nodes of the parse become the nodes that run.
static void retain(SSRExNode *n) {
    if (!n) return;
    if (n->kind == K_TEMPFLOAT) n->kind = K_NUMBER;
    if (n->kind == K_NUMBER) {
        free(n->s);
        n->s = NULL;
    }
    for (int i = 0; i < n->kidCount; i++) retain(n->kids[i]);
    for (int i = 0; i < n->contentCount; i++) retain(n->contents[i]);
}

int ssr_ex_parse(const char *phrase, const SSRExHost *host, SSRExNode **items, int max) {
    if (!phrase) return 0;
    List l = {NULL, 0, host};
    SSRExNode *last = NULL;
    int makeTuple = 1, depth = 0;
    const char *at = phrase;
    while (*at) {
        while (*at == ' ' || *at == '\t') at++;
        if (!*at) break;
        int request = 0;
        SSRExNode *n = token(&at, host, &request);
        if (!n) continue;
        if (n->kind == K_OPEN) {
            n->tuple = makeTuple;
            depth++;
        }
        if (n->kind == K_CLOSE) depth--;
        makeTuple = request;
        list_insert_after(&l, last, n);
        last = n;
    }
    // A missing ')' at the end is added.
    for (; depth > 0; depth--) {
        SSRExNode *close = node_new(K_CLOSE);
        list_insert_after(&l, last, close);
        last = close;
    }
    for (int round = 0; round < 1000; round++) {
        l.changed = 0;
        int allComplete = 1;
        for (SSRExNode *n = l.first; n;) {
            n = collapse(&l, n);
            if (n && allComplete && !n->complete) allComplete = 0;
        }
        for (SSRExNode *n = l.first; n; n = n->next) {
            if (!n->complete) allComplete = 0;
        }
        if (!l.changed && !allComplete) {
            for (SSRExNode *n = l.first; n;) n = force_complete(&l, n);
        }
        if (!l.changed) break;
    }
    int count = 0;
    SSRExNode *first = l.first;
    if (first && first->kind == K_OPEN && first->tuple && first->complete) {
        for (int i = 0; i < first->contentCount; i++) {
            if (count < max) {
                retain(first->contents[i]);
                items[count++] = first->contents[i];
            } else {
                ssr_ex_free(first->contents[i]);
            }
        }
        first->contentCount = 0;
    }
    for (SSRExNode *n = l.first; n;) {
        SSRExNode *next = n->next;
        ssr_ex_free(n);
        n = next;
    }
    return count;
}

SSRExNode *ssr_ex_clone(const SSRExNode *n, SSRExVar *(*remap)(void *context, SSRExVar *old), void *context) {
    if (!n) return NULL;
    SSRExNode *copy = malloc(sizeof(SSRExNode));
    if (!copy) return NULL;
    *copy = *n;
    copy->prev = copy->next = NULL;
    copy->s = n->s ? copy_text(n->s, strlen(n->s)) : NULL;
    copy->group = n->group ? copy_text(n->group, strlen(n->group)) : NULL;
    copy->name = n->name ? copy_text(n->name, strlen(n->name)) : NULL;
    if (n->var && remap) copy->var = remap(context, n->var);
    copy->kids = n->kidCount ? calloc((size_t)n->kidCount, sizeof(SSRExNode *)) : NULL;
    for (int i = 0; i < n->kidCount && copy->kids; i++) copy->kids[i] = ssr_ex_clone(n->kids[i], remap, context);
    copy->contents = n->contentCount ? calloc((size_t)n->contentCount, sizeof(SSRExNode *)) : NULL;
    for (int i = 0; i < n->contentCount && copy->contents; i++) {
        copy->contents[i] = ssr_ex_clone(n->contents[i], remap, context);
    }
    return copy;
}

/* ---------------------------------------------------------- values */

void ssr_ex_java_float(float value, char *out, size_t capacity) {
    if (isnan(value)) {
        snprintf(out, capacity, "NaN");
        return;
    }
    if (isinf(value)) {
        snprintf(out, capacity, value > 0 ? "Infinity" : "-Infinity");
        return;
    }
    if (value == 0) {
        snprintf(out, capacity, signbit(value) ? "-0.0" : "0.0");
        return;
    }
    // The fewest significant digits that read back as the same float.
    char digits[32];
    int exponent = 0;
    for (int precision = 1; precision <= 9; precision++) {
        char text[48];
        snprintf(text, sizeof(text), "%.*e", precision - 1, (double)value);
        if (strtof(text, NULL) == value || precision == 9) {
            // text is "-d.ddde+NN".
            const char *p = text;
            size_t used = 0;
            if (*p == '-') p++;
            for (; *p && *p != 'e'; p++) {
                if (*p != '.') digits[used++] = *p;
            }
            digits[used] = 0;
            exponent = atoi(p + 1);
            break;
        }
    }
    // Java writes 1e-3 <= |v| < 1e7 as a decimal, else as d.dddE[-]n.
    char body[64];
    size_t count = strlen(digits);
    while (count > 1 && digits[count - 1] == '0') digits[--count] = 0;
    float magnitude = fabsf(value);
    if (magnitude >= 1e-3f && magnitude < 1e7f) {
        size_t used = 0;
        if (exponent >= 0) {
            for (int i = 0; i <= exponent; i++) body[used++] = (size_t)i < count ? digits[i] : '0';
            body[used++] = '.';
            if ((size_t)(exponent + 1) < count) {
                for (size_t i = (size_t)exponent + 1; i < count; i++) body[used++] = digits[i];
            } else {
                body[used++] = '0';
            }
        } else {
            body[used++] = '0';
            body[used++] = '.';
            for (int i = -1; i > exponent; i--) body[used++] = '0';
            for (size_t i = 0; i < count; i++) body[used++] = digits[i];
        }
        body[used] = 0;
    } else {
        snprintf(body, sizeof(body), "%c.%sE%d", digits[0], count > 1 ? digits + 1 : "0", exponent);
    }
    snprintf(out, capacity, "%s%s", value < 0 ? "-" : "", body);
}

// bb_stringutils.StringIntOrFloat: an integer value has no point.
static void int_or_float(float value, char *out, size_t capacity) {
    if (!isnan(value) && fabsf(value) < 2.1e9f && (float)(int)value == value) {
        snprintf(out, capacity, "%d", (int)value);
    } else {
        ssr_ex_java_float(value, out, capacity);
    }
}

static float java_parse_float(const char *text) {
    while (*text == ' ') text++;
    char *end;
    float value = strtof(text, &end);
    return end == text ? 0 : value;
}

static SSRExNode *kid(const SSRExNode *n, int i) {
    return i < n->kidCount ? n->kids[i] : NULL;
}

static float tweak_float(const SSRExNode *n, const SSRExHost *host) {
    if (!host || !host->tweaks) return 0;
    return (float)ssr_tweaks_number(host->tweaks, n->group, n->name);
}

static SSRExColour tweak_colour(const SSRExNode *n, const SSRExHost *host) {
    const char *text = host && host->tweaks ? ssr_tweaks_get(host->tweaks, n->group, n->name) : "";
    if (*text == '#') text++;
    size_t length = 0;
    while (hex_digit(text[length]) >= 0) length++;
    return parse_colour(text, length);
}

// The kind of the left side of a comparison: 1 a number, 2 a text, 3 a
// colour, 4 true, 5 false, 6 Null.
static int compare_mode(const SSRExNode *n) {
    switch (n->kind) {
    case K_WORD:
    case K_STRING:
    case K_CONCAT:
    case K_LOC:
    case K_REP:
    case K_FMT:
    case K_TRIMSTR:
    case K_SHELL:
        return 2;
    case K_TWEAK:
        if (n->tweakKind == SSR_TWEAK_STRING) return 2;
        if (n->tweakKind == SSR_TWEAK_COLOUR && !n->part) return 3;
        return 1;
    case K_VAR:
        return n->var->kind == SSR_VAR_STRING ? 2 : n->var->kind == SSR_VAR_COLOUR ? 3 : 1;
    case K_GEL:
        if (n->property == SSR_GEL_TEXT || n->property == SSR_GEL_IMG || n->property == SSR_GEL_ANIM) return 2;
        if (n->property == SSR_GEL_COLOUR) return 3;
        return 1;
    case K_COLOUR:
        return 3;
    case K_TRUE:
        return 4;
    case K_FALSE:
        return 5;
    case K_NULL:
        return 6;
    default:
        return 1;
    }
}

static float compare(const SSRExNode *a, const SSRExNode *b, const SSRExHost *host) {
    if (!a || !b) return -1;
    switch (compare_mode(a)) {
    case 2: {
        char x[512], y[512];
        ssr_ex_string(a, host, x, sizeof(x));
        ssr_ex_string(b, host, y, sizeof(y));
        return (float)strcmp(x, y);
    }
    case 3: {
        SSRExColour x = ssr_ex_colour(a, host), y = ssr_ex_colour(b, host);
        return (fabsf(x.r - y.r) + 2 * fabsf(x.g - y.g) + fabsf(x.b - y.b)) / 4;
    }
    case 4:
        return 1 - ssr_ex_float(b, host);
    case 5:
        return 0 - ssr_ex_float(b, host);
    case 6:
        return b->kind == K_NULL ? 0 : -1;
    default:
        return ssr_ex_float(a, host) - ssr_ex_float(b, host);
    }
}

// c_EX_Format: digits are cut, only the last one rounds, and a digit 10
// prints as 9.
static void format(float v, const char *f, char *out, size_t capacity) {
    char sign = 0;
    int start = 0;
    if (f[0] == 'P') {
        sign = v > 0 ? '+' : v < 0 ? '-' : 0;
        start = 1;
    } else if (f[0] == 'U') {
        start = 1;
    } else if (f[0] == 'Z') {
        sign = v >= 0 ? '+' : '-';
        start = 1;
    } else if (v < 0) {
        sign = '-';
    }
    if (v < 0) v = -v;
    int intDigits = 0, fracDigits = 0;
    const char *point = strchr(f + (f[0] ? start : 0), '.');
    intDigits = (int)((point ? point : f + strlen(f)) - (f + start));
    if (intDigits < 0) intDigits = 0;
    if (point) fracDigits = (int)strlen(point + 1);
    int n = 0;
    float p = 1;
    while (p <= v && n < 38) {
        p *= 10;
        n++;
    }
    float step = p / 10;
    size_t used = 0;
    if (sign && used + 1 < capacity) out[used++] = sign;
    for (int i = n; i < intDigits && used + 1 < capacity; i++) out[used++] = '0';
    for (int i = 0; i < n && used + 1 < capacity; i++) {
        int d = (int)(v / step);
        if (d > 9) d = 9;
        out[used++] = (char)('0' + d);
        v -= (float)d * step;
        step /= 10;
    }
    if (fracDigits > 0 && used + 1 < capacity) {
        out[used++] = '.';
        for (int i = 0; i < fracDigits && used + 1 < capacity; i++) {
            if (i == fracDigits - 1) v += step / 2;
            int d = (int)(v / step);
            if (d > 9) d = 9;
            if (d < 0) d = 0;
            out[used++] = (char)('0' + d);
            v -= (float)d * step;
            step /= 10;
        }
    }
    out[used] = 0;
}

float ssr_ex_float(const SSRExNode *n, const SSRExHost *host) {
    if (!n) return 0;
    SSRExNode *a = kid(n, 0), *b = kid(n, 1), *c = kid(n, 2);
    switch (n->kind) {
    case K_NUMBER:
    case K_TEMPFLOAT:
        return n->f;
    case K_WORD:
    case K_STRING:
        if (n->s && (isdigit((unsigned char)n->s[0]) || n->s[0] == '-')) return java_parse_float(n->s);
        return 0;
    case K_TRUE:
        return 1;
    case K_TWEAK:
        if (n->tweakKind == SSR_TWEAK_COLOUR) {
            SSRExColour col = tweak_colour(n, host);
            return n->part == 'r' ? col.r : n->part == 'g' ? col.g : n->part == 'b' ? col.b : n->part == 'a' ? col.a : 0;
        }
        if (n->tweakKind == SSR_TWEAK_STRING) {
            return host && host->tweaks ? java_parse_float(ssr_tweaks_get(host->tweaks, n->group, n->name)) : 0;
        }
        return tweak_float(n, host);
    case K_TWEAK_RANGE: {
        double min = 0, max = 0;
        if (!host || !host->tweaks || !ssr_tweaks_range(host->tweaks, n->group, n->name, &min, &max)) return 0;
        float t = ssr_ex_float(a, host);
        return (float)(min + (max - min) * t);
    }
    case K_VAR:
        if (n->var->kind == SSR_VAR_FLOAT) return n->var->f;
        if (n->var->kind == SSR_VAR_STRING) return java_parse_float(n->var->s);
        return 0;
    case K_GEL:
        return host && host->get ? host->get(host->context, host->gel, n->property) : 0;
    case K_FRAMETIME:
        return host ? host->frameTime : 1;
    case K_TOUCHX:
        return host ? host->touchX : 0;
    case K_TOUCHY:
        return host ? host->touchY : 0;
    case K_TOUCHHIT:
        return host ? (float)host->touchHit : 0;
    case K_TOUCHDOWN:
        return host ? (float)host->touchDown : 0;
    case K_TOUCHRELEASED:
        return host ? (float)host->touchReleased : 0;
    case K_PLUS:
        return ssr_ex_float(a, host) + ssr_ex_float(b, host);
    case K_MINUS:
        return ssr_ex_float(a, host) - ssr_ex_float(b, host);
    case K_MULTIPLY:
        return ssr_ex_float(a, host) * ssr_ex_float(b, host);
    case K_DIVIDE:
        return ssr_ex_float(a, host) / ssr_ex_float(b, host);
    case K_FMOD:
        return fmodf(ssr_ex_float(a, host), ssr_ex_float(b, host));
    case K_MAX: {
        float x = ssr_ex_float(a, host), y = ssr_ex_float(b, host);
        return x > y ? x : y;
    }
    case K_MIN: {
        float x = ssr_ex_float(a, host), y = ssr_ex_float(b, host);
        return x < y ? x : y;
    }
    case K_ABS:
        return fabsf(ssr_ex_float(a, host));
    case K_SIN:
        return (float)sin((double)(ssr_ex_float(a, host) * 0.017453292f));
    case K_COS:
        return (float)cos((double)(ssr_ex_float(a, host) * 0.017453292f));
    case K_SQRT:
        return (float)sqrt((double)ssr_ex_float(a, host));
    case K_ATAN2:
        return (float)(atan2((double)ssr_ex_float(a, host), (double)ssr_ex_float(b, host)) * 57.29578);
    case K_LERP:
    case K_FRLERP: {
        float t = ssr_ex_float(c, host);
        if (n->kind == K_FRLERP) {
            int frames = host ? (int)host->frameTime : 1;
            t = 1 - powf(1 - t, (float)(frames < 1 ? 1 : frames));
        }
        float x = ssr_ex_float(a, host);
        return x + (ssr_ex_float(b, host) - x) * t;
    }
    case K_CURVE: {
        float t = ssr_ex_float(a, host), x = ssr_ex_float(b, host), y = ssr_ex_float(c, host);
        return (1 - t) * (1 - t) * t * (x + 1) + ((1 - y) * (1 - t) + 1) * t * t;
    }
    case K_EQUAL:
    case K_NOT_EQUAL:
    case K_LESS:
    case K_GREATER:
    case K_LESS_EQUAL:
    case K_GREATER_EQUAL:
    case K_AND:
    case K_OR:
    case K_ONEOF:
    case K_NOTONEOF:
    case K_CONTAINS:
    case K_ENDSWITH:
        return ssr_ex_bool(n, host) ? 1 : 0;
    case K_IMGDOWNLOADED:
        return 0;
    case K_CONCAT:
    case K_FMT:
    case K_LOC:
    case K_REP:
    case K_TRIMSTR: {
        char text[512];
        return java_parse_float(ssr_ex_string(n, host, text, sizeof(text)));
    }
    default:
        return 0;
    }
}

int ssr_ex_bool(const SSRExNode *n, const SSRExHost *host) {
    if (!n) return 0;
    SSRExNode *a = kid(n, 0), *b = kid(n, 1);
    switch (n->kind) {
    case K_NUMBER:
    case K_TEMPFLOAT:
        return n->f != 0;
    case K_WORD:
    case K_STRING:
        return n->s && (strcmp(n->s, "true") == 0 || strcmp(n->s, "True") == 0);
    case K_TRUE:
        return 1;
    case K_TWEAK:
        if (n->tweakKind == SSR_TWEAK_FLOAT || n->part) return (int)ssr_ex_float(n, host) != 0;
        return 0;
    case K_TWEAK_RANGE:
    case K_MAX:
    case K_MIN:
    case K_LERP:
    case K_FRLERP:
        return (int)ssr_ex_float(n, host) != 0;
    case K_VAR:
        if (n->var->kind == SSR_VAR_FLOAT) return n->var->f != 0;
        if (n->var->kind == SSR_VAR_STRING) return n->var->s[0] == 'T' || n->var->s[0] == 't';
        return 0;
    case K_GEL:
        if (n->property == SSR_GEL_VIS || n->property == SSR_GEL_ANIM_PLAYING) return ssr_ex_float(n, host) != 0;
        return 0;
    case K_EQUAL:
        return compare(a, b, host) == 0;
    case K_NOT_EQUAL:
        return compare(a, b, host) != 0;
    case K_LESS:
        return compare(a, b, host) < 0;
    case K_GREATER:
        return compare(a, b, host) > 0;
    case K_LESS_EQUAL:
        return compare(a, b, host) <= 0;
    case K_GREATER_EQUAL:
        return compare(a, b, host) >= 0;
    case K_AND:
        return ssr_ex_bool(a, host) && ssr_ex_bool(b, host);
    case K_OR:
        return ssr_ex_bool(a, host) || ssr_ex_bool(b, host);
    case K_NOT:
        return !ssr_ex_bool(a, host);
    case K_ONEOF:
    case K_NOTONEOF: {
        int found = 0;
        for (int i = 1; i < n->kidCount && !found; i++) found = compare(a, n->kids[i], host) == 0;
        return n->kind == K_ONEOF ? found : !found;
    }
    case K_CONTAINS:
    case K_ENDSWITH: {
        char x[512], y[512];
        ssr_ex_string(a, host, x, sizeof(x));
        for (int i = 1; i < n->kidCount; i++) {
            ssr_ex_string(n->kids[i], host, y, sizeof(y));
            size_t lx = strlen(x), ly = strlen(y);
            if (n->kind == K_CONTAINS ? strstr(x, y) != NULL : ly <= lx && strcmp(x + lx - ly, y) == 0) return 1;
        }
        return 0;
    }
    case K_FMT:
        return ssr_ex_float(n, host) != 0;
    case K_IMGDOWNLOADED:
        return 0;
    case K_TOUCHHIT:
    case K_TOUCHDOWN:
    case K_TOUCHRELEASED:
        return ssr_ex_float(n, host) != 0;
    default:
        return 0;
    }
}

// c_GColour.ToString2: 16 hex digits, RRGGBBAArrggbbaa.
static void colour_text(SSRExColour c, char *out, size_t capacity) {
    snprintf(out, capacity, "%02X%02X%02X%02X%02X%02X%02X%02X", (int)c.r & 255, (int)c.g & 255, (int)c.b & 255,
             (int)(c.a * 255) & 255, (int)(c.mr * 255) & 255, (int)(c.mg * 255) & 255, (int)(c.mb * 255) & 255,
             (int)(c.ma * 255) & 255);
}

static void replace_text(char *text, size_t capacity, const char *from, const char *to) {
    if (!*from) return;
    char out[1024] = "";
    size_t used = 0, fromLength = strlen(from), toLength = strlen(to);
    for (const char *p = text; *p;) {
        if (strncmp(p, from, fromLength) == 0) {
            if (used + toLength < sizeof(out)) {
                memcpy(out + used, to, toLength);
                used += toLength;
            }
            p += fromLength;
        } else {
            if (used + 1 < sizeof(out)) out[used++] = *p;
            p++;
        }
    }
    out[used] = 0;
    snprintf(text, capacity, "%s", out);
}

const char *ssr_ex_string(const SSRExNode *n, const SSRExHost *host, char *out, size_t capacity) {
    out[0] = 0;
    if (!n) return out;
    SSRExNode *a = kid(n, 0), *b = kid(n, 1), *c = kid(n, 2);
    char x[512], y[512];
    switch (n->kind) {
    case K_NUMBER:
        if (n->s) {
            snprintf(out, capacity, "%s", n->s);
        } else {
            int_or_float(n->f, out, capacity);
        }
        break;
    case K_TEMPFLOAT:
        ssr_ex_java_float(n->f, out, capacity);
        break;
    case K_WORD:
    case K_STRING:
        snprintf(out, capacity, "%s", n->s ? n->s : "");
        break;
    case K_TRUE:
    case K_FALSE:
        snprintf(out, capacity, n->kind == K_TRUE ? "true" : "false");
        break;
    case K_COLOUR:
        colour_text(n->c, out, capacity);
        break;
    case K_TWEAK:
        if (n->tweakKind == SSR_TWEAK_STRING) {
            snprintf(out, capacity, "%s", host && host->tweaks ? ssr_tweaks_get(host->tweaks, n->group, n->name) : "");
        } else if (n->tweakKind == SSR_TWEAK_COLOUR && !n->part) {
            colour_text(tweak_colour(n, host), out, capacity);
        } else if (n->part) {
            ssr_ex_java_float(ssr_ex_float(n, host), out, capacity);
        } else {
            int_or_float(ssr_ex_float(n, host), out, capacity);
        }
        break;
    case K_TWEAK_RANGE:
    case K_PLUS:
    case K_MINUS:
    case K_MULTIPLY:
    case K_DIVIDE:
    case K_FMOD:
    case K_MAX:
    case K_MIN:
        int_or_float(ssr_ex_float(n, host), out, capacity);
        break;
    case K_VAR:
        if (n->var->kind == SSR_VAR_FLOAT) {
            ssr_ex_java_float(n->var->f, out, capacity);
        } else if (n->var->kind == SSR_VAR_STRING) {
            snprintf(out, capacity, "%s", n->var->s);
        } else {
            colour_text(n->var->c, out, capacity);
        }
        break;
    case K_GEL:
        if (n->property == SSR_GEL_TEXT || n->property == SSR_GEL_IMG || n->property == SSR_GEL_ANIM) {
            const char *text = host && host->get_text ? host->get_text(host->context, host->gel, n->property) : "";
            snprintf(out, capacity, "%s", text ? text : "");
        } else if (n->property == SSR_GEL_VIS || n->property == SSR_GEL_ANIM_PLAYING) {
            snprintf(out, capacity, ssr_ex_bool(n, host) ? "true" : "false");
        } else if (n->property == SSR_GEL_COLOUR) {
            colour_text(ssr_ex_colour(n, host), out, capacity);
        } else {
            int_or_float(ssr_ex_float(n, host), out, capacity);
        }
        break;
    case K_FRAMETIME:
    case K_TOUCHX:
    case K_TOUCHY:
    case K_ABS:
        ssr_ex_java_float(ssr_ex_float(n, host), out, capacity);
        break;
    case K_ONEOF:
    case K_NOTONEOF:
    case K_CONTAINS:
    case K_ENDSWITH:
        snprintf(out, capacity, ssr_ex_bool(n, host) ? "true" : "false");
        break;
    case K_CONCAT:
        ssr_ex_string(a, host, x, sizeof(x));
        ssr_ex_string(b, host, y, sizeof(y));
        snprintf(out, capacity, "%s%s", x, y);
        break;
    case K_LOC: {
        ssr_ex_string(a, host, x, sizeof(x));
        const char *text = host && host->localise ? host->localise(host->context, x) : NULL;
        snprintf(out, capacity, "%s", text ? text : x);
        break;
    }
    case K_FMT:
        ssr_ex_string(b, host, x, sizeof(x));
        format(ssr_ex_float(a, host), x, out, capacity);
        break;
    case K_TRIMSTR: {
        ssr_ex_string(a, host, x, sizeof(x));
        int keep = (int)ssr_ex_float(b, host);
        if (keep < 0) keep = 0;
        snprintf(out, capacity, "%.*s", keep, x);
        break;
    }
    case K_REP:
        ssr_ex_string(a, host, out, capacity);
        ssr_ex_string(b, host, x, sizeof(x));
        ssr_ex_string(c, host, y, sizeof(y));
        replace_text(out, capacity, x, y);
        break;
    case K_SHELL: {
        ssr_ex_string(a, host, x, sizeof(x));
        const char *screen = host && host->get_shell ? host->get_shell(host->context, x) : "";
        snprintf(out, capacity, "%s", screen ? screen : "");
        break;
    }
    default:
        break;
    }
    return out;
}

SSRExColour ssr_ex_colour(const SSRExNode *n, const SSRExHost *host) {
    if (!n) return WHITE;
    SSRExNode *a = kid(n, 0), *b = kid(n, 1), *c = kid(n, 2);
    switch (n->kind) {
    case K_COLOUR:
        return n->c;
    case K_TWEAK:
        return n->tweakKind == SSR_TWEAK_COLOUR || n->tweakKind == SSR_TWEAK_STRING ? tweak_colour(n, host) : WHITE;
    case K_VAR:
        return n->var->kind == SSR_VAR_COLOUR ? n->var->c : WHITE;
    case K_GEL:
        return host && host->get_colour ? host->get_colour(host->context, host->gel) : WHITE;
    case K_LERP:
    case K_FRLERP:
    case K_COLLERP: {
        float t = ssr_ex_float(c, host);
        if (n->kind == K_FRLERP) {
            int frames = host ? (int)host->frameTime : 1;
            t = 1 - powf(1 - t, (float)(frames < 1 ? 1 : frames));
        }
        SSRExColour x = ssr_ex_colour(a, host), y = ssr_ex_colour(b, host), out = {0, 0, 0, 0, 0, 0, 0, 0};
        out.r = x.r + (y.r - x.r) * t;
        out.g = x.g + (y.g - x.g) * t;
        out.b = x.b + (y.b - x.b) * t;
        out.a = x.a + (y.a - x.a) * t;
        // Only collerp lerps the mix part. lerp gives a colour with no mix.
        if (n->kind == K_COLLERP) {
            out.mr = x.mr + (y.mr - x.mr) * t;
            out.mg = x.mg + (y.mg - x.mg) * t;
            out.mb = x.mb + (y.mb - x.mb) * t;
            out.ma = x.ma + (y.ma - x.ma) * t;
        }
        return out;
    }
    default:
        return WHITE;
    }
}

// The value of a constant node at the parse (c_Expression.Evaluate).
static SSRExNode *evaluate_const(const SSRExNode *n, const SSRExHost *host) {
    SSRExNode *value = NULL;
    switch (n->kind) {
    case K_PLUS:
    case K_MINUS:
    case K_MULTIPLY:
    case K_DIVIDE:
    case K_FMOD:
    case K_MAX:
    case K_MIN:
        value = node_new(K_TEMPFLOAT);
        value->f = ssr_ex_float(n, host);
        break;
    case K_EQUAL:
    case K_NOT_EQUAL:
    case K_LESS:
    case K_GREATER:
    case K_LESS_EQUAL:
    case K_GREATER_EQUAL:
    case K_AND:
    case K_OR:
    case K_NOT:
    case K_ONEOF:
    case K_NOTONEOF:
    case K_CONTAINS:
    case K_ENDSWITH:
        value = node_new(ssr_ex_bool(n, host) ? K_TRUE : K_FALSE);
        break;
    case K_CONCAT:
    case K_FMT:
    case K_LOC:
    case K_REP:
    case K_TRIMSTR: {
        char text[1024];
        ssr_ex_string(n, host, text, sizeof(text));
        value = node_new(K_STRING);
        value->s = copy_text(text, strlen(text));
        value->folded = 1;
        break;
    }
    default:
        return NULL;
    }
    if (value) value->complete = 1;
    return value;
}

/* ------------------------------------------------------------ set */

static void set_tweak_text(const SSRExNode *t, const SSRExHost *host, const char *text) {
    ssr_tweaks_set(host->tweaks, t->group, t->name, text);
}

void ssr_ex_assign(const SSRExNode *t, const SSRExNode *v, const SSRExHost *host) {
    if (!t || !host) return;
    char text[512];
    switch (t->kind) {
    case K_TWEAK:
        if (!host->tweaks) return;
        if (t->tweakKind == SSR_TWEAK_STRING) {
            set_tweak_text(t, host, ssr_ex_string(v, host, text, sizeof(text)));
        } else if (t->tweakKind == SSR_TWEAK_COLOUR) {
            char hex[32];
            colour_text(ssr_ex_colour(v, host), hex, sizeof(hex));
            set_tweak_text(t, host, hex);
        } else {
            ssr_tweaks_set_number(host->tweaks, t->group, t->name, ssr_ex_float(v, host));
        }
        break;
    case K_TWEAK_RANGE:
        // c_EX_FloatTweakRange.Set: both ends take the value.
        if (host->tweaks) {
            float value = ssr_ex_float(v, host);
            ssr_tweaks_set_range(host->tweaks, t->group, t->name, value, value);
        }
        break;
    case K_VAR:
        if (t->var->kind == SSR_VAR_FLOAT) {
            t->var->f = ssr_ex_float(v, host);
        } else if (t->var->kind == SSR_VAR_STRING) {
            ssr_ex_string(v, host, t->var->s, sizeof(t->var->s));
        } else {
            t->var->c = ssr_ex_colour(v, host);
        }
        break;
    case K_GEL:
        switch (t->property) {
        case SSR_GEL_VIS:
        case SSR_GEL_ANIM_PLAYING:
            if (host->set) host->set(host->context, host->gel, t->property, ssr_ex_bool(v, host) ? 1.0f : 0.0f);
            break;
        case SSR_GEL_TEXT:
        case SSR_GEL_IMG:
        case SSR_GEL_ANIM:
            if (host->set_text) host->set_text(host->context, host->gel, t->property, ssr_ex_string(v, host, text, sizeof(text)));
            break;
        case SSR_GEL_COLOUR:
        case SSR_GEL_TEXT_SHADCOL:
        case SSR_GEL_SLOTCOL: {
            const char *slot = NULL;
            if (t->property == SSR_GEL_SLOTCOL) slot = ssr_ex_string(kid(t, 0), host, text, sizeof(text));
            if (host->set_colour) {
                host->set_colour(host->context, host->gel, t->property, slot, ssr_ex_colour(v, host),
                                 t->property == SSR_GEL_SLOTCOL ? t->mask : 255);
            }
            break;
        }
        default:
            if (host->set) host->set(host->context, host->gel, t->property, ssr_ex_float(v, host));
            break;
        }
        break;
    case K_SHELL:
        if (host->set_shell) {
            char name[64];
            ssr_ex_string(kid(t, 0), host, name, sizeof(name));
            host->set_shell(host->context, name, ssr_ex_string(v, host, text, sizeof(text)));
        }
        break;
    default:
        // A number, a text, or a comparison cannot change.
        break;
    }
}

static int is_colour_target(const SSRExNode *t) {
    return (t->kind == K_GEL && (t->property == SSR_GEL_COLOUR || t->property == SSR_GEL_SLOTCOL)) ||
           (t->kind == K_VAR && t->var->kind == SSR_VAR_COLOUR) ||
           (t->kind == K_TWEAK && t->tweakKind == SSR_TWEAK_COLOUR && !t->part);
}

void ssr_ex_operate(const char *op, const SSRExNode *t, const SSRExNode *v, const SSRExNode *third,
                    const SSRExHost *host) {
    if (!t || !op) return;
    if (strcmp(op, "set") == 0) {
        ssr_ex_assign(t, v, host);
        return;
    }
    SSRExNode value = {0};
    value.c = WHITE;
    value.complete = 1;
    if (strcmp(op, "lerp") == 0 || strcmp(op, "frlerp") == 0 || strcmp(op, "collerp") == 0) {
        // T.Set(lerp(T, V, t)), with the node of the target as the first.
        SSRExNode *kids[3] = {(SSRExNode *)t, (SSRExNode *)v, (SSRExNode *)third};
        value.kind = op[0] == 'l' ? K_LERP : op[0] == 'f' ? K_FRLERP : K_COLLERP;
        value.kids = kids;
        value.kidCount = 3;
        if (is_colour_target(t)) {
            SSRExNode colour = {0};
            colour.kind = K_COLOUR;
            colour.complete = 1;
            colour.c = ssr_ex_colour(&value, host);
            ssr_ex_assign(t, &colour, host);
        } else {
            SSRExNode number = {0};
            number.kind = K_NUMBER;
            number.complete = 1;
            number.f = ssr_ex_float(&value, host);
            ssr_ex_assign(t, &number, host);
        }
        return;
    }
    float a = ssr_ex_float(t, host), b = ssr_ex_float(v, host), r = a;
    if (strcmp(op, "add") == 0) r = a + b;
    else if (strcmp(op, "sub") == 0) r = a - b;
    else if (strcmp(op, "mul") == 0) r = a * b;
    else if (strcmp(op, "div") == 0) r = a / b;
    else if (strcmp(op, "fmod") == 0) r = fmodf(a, b);
    else return;
    SSRExNode number = {0};
    number.kind = K_NUMBER;
    number.complete = 1;
    number.f = r;
    ssr_ex_assign(t, &number, host);
}
