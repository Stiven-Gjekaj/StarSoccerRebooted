// ssr_ex: the expression language of the layouts, as c_Phrase and the
// c_EX_* classes of the game have it. A line such as
// "set(gel.text,fmt(twk(Player,Cans),X)::" "::loc(nrg_Cans))" becomes a
// tree of nodes once, when the screen is read. The tree runs each frame.
//
// The language has no operator precedence. The parser puts the tokens in a
// list and walks it from left to right again and again. An operator takes
// its operands when they are complete. So "2+3*4" is 20, as in the game.

#ifndef SSR_EX_H
#define SSR_EX_H

#include <stddef.h>

#include "ssr_tweaks.h"

typedef struct {
    // r, g, b from 0 to 255, a from 0 to 1, and the mix factors from 0 to 1.
    float r, g, b, a;
    float mr, mg, mb, ma;
} SSRExColour;

typedef enum {
    SSR_VAR_FLOAT,
    SSR_VAR_STRING,
    SSR_VAR_COLOUR,
} SSRExVarKind;

// A %variable of a gel: float(%a), strvar(%b), colvar(%c).
typedef struct {
    char name[32];
    SSRExVarKind kind;
    float f;
    char s[256];
    SSRExColour c;
} SSRExVar;

// The properties of "gel.".
typedef enum {
    SSR_GEL_NONE,
    SSR_GEL_VIS,
    SSR_GEL_TEXT,
    SSR_GEL_TEXT_SHADCOL,
    SSR_GEL_COLOUR,
    SSR_GEL_SLOTCOL,
    SSR_GEL_X,
    SSR_GEL_Y,
    SSR_GEL_W,
    SSR_GEL_H,
    SSR_GEL_HX,
    SSR_GEL_HY,
    SSR_GEL_GX,
    SSR_GEL_GY,
    SSR_GEL_ANGLE,
    SSR_GEL_ALPHA,
    SSR_GEL_SCALEX,
    SSR_GEL_SCALEY,
    SSR_GEL_SCALE,
    SSR_GEL_DEPTH,
    SSR_GEL_IMG,
    SSR_GEL_ANIM,
    SSR_GEL_ANIM_SPEED,
    SSR_GEL_ANIM_TIME,
    SSR_GEL_ANIM_NORMTIME,
    SSR_GEL_ANIM_PLAYING,
} SSRGelProperty;

// What the expressions read and change: the tweaks, the language, the
// gel that runs the line, and the time and the touch of the frame.
typedef struct SSRExHost SSRExHost;
struct SSRExHost {
    SSRTweaks *tweaks;
    void *context;
    const char *(*localise)(void *context, const char *tag);
    // The variable with the name for the gel that the parser reads now,
    // from the gel up through its parents. NULL when there is none.
    SSRExVar *(*find_var)(void *context, const char *name);
    // @L, @T, and the other letters of the layouts.
    float (*simple_var)(void *context, char letter);
    // The gel that runs the expression.
    void *gel;
    float (*get)(void *context, void *gel, SSRGelProperty property);
    void (*set)(void *context, void *gel, SSRGelProperty property, float value);
    const char *(*get_text)(void *context, void *gel, SSRGelProperty property);
    void (*set_text)(void *context, void *gel, SSRGelProperty property, const char *text);
    SSRExColour (*get_colour)(void *context, void *gel);
    void (*set_colour)(void *context, void *gel, SSRGelProperty property, const char *slot, SSRExColour colour,
                       int mask);
    // The screen of a shell, or "", and a change of it.
    const char *(*get_shell)(void *context, const char *shell);
    void (*set_shell)(void *context, const char *shell, const char *screen);
    float frameTime;
    float touchX, touchY;
    int touchDown, touchHit, touchReleased;
};

typedef struct SSRExNode SSRExNode;

// Reads the phrase of a doodad line: the text from its first '(' to the end
// of the line, for example "(gel.vis,@W>640)". The result is the list of
// the items of that bracket. Returns the number of items, up to max. The
// parse needs host for the tweaks, the variables, and the language.
int ssr_ex_parse(const char *phrase, const SSRExHost *host, SSRExNode **items, int max);

void ssr_ex_free(SSRExNode *node);

// A copy of the tree. A variable in old is a variable in the copy.
SSRExNode *ssr_ex_clone(const SSRExNode *node, SSRExVar *(*remap)(void *context, SSRExVar *old), void *context);

float ssr_ex_float(const SSRExNode *node, const SSRExHost *host);
int ssr_ex_bool(const SSRExNode *node, const SSRExHost *host);
// Writes the text into out and returns out.
const char *ssr_ex_string(const SSRExNode *node, const SSRExHost *host, char *out, size_t capacity);
SSRExColour ssr_ex_colour(const SSRExNode *node, const SSRExHost *host);

// T.Set(V): the target takes the value, by the kind of the target.
void ssr_ex_assign(const SSRExNode *target, const SSRExNode *value, const SSRExHost *host);

// The doodads of the language: set, add, sub, mul, div, fmod, lerp,
// frlerp, and collerp. op is the keyword. t is the third item, or NULL.
void ssr_ex_operate(const char *op, const SSRExNode *target, const SSRExNode *value, const SSRExNode *t,
                    const SSRExHost *host);

// 1 when the node is a value that the game knows at the parse.
int ssr_ex_is_const(const SSRExNode *node);

// Java Float.toString: the shortest text that reads back as the float.
void ssr_ex_java_float(float value, char *out, size_t capacity);

#endif
