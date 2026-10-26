// ssr_ui: reads the screen layouts of the game (NewGUI/Screens/*.uis and
// *.uih) into a tree.
//
// A line "Name:(a,b) ref:R" is an element. A line "Name(a,b)" or "Name" is
// a modifier of the element before it. "{ }" holds the children of the
// element before it. "< >" holds the children of the modifier before it.
// The parser does the work that needs no game state: Import:, Macro: and
// its calls, Const: and Define:, and If:, Else, and EndIf.

#ifndef SSR_UI_H
#define SSR_UI_H

#include <stddef.h>

// The line had a ':' after its name.
#define SSR_UI_ELEMENT 1u
// The line began with '.'.
#define SSR_UI_DOT 2u
// The line began with '!'.
#define SSR_UI_BANG 4u
// The line had "$HIDE": the element starts hidden.
#define SSR_UI_HIDE 8u

typedef struct SSRUiNode SSRUiNode;

struct SSRUiNode {
    char *name;
    unsigned flags;
    char **args;
    int argCount;
    char *ref;
    SSRUiNode **children;
    int childCount;
    SSRUiNode **modifiers;
    int modifierCount;
    // Where the line is, for messages.
    const char *file;
    int line;
};

typedef struct {
    // Gives the text of a layout file, for example "ButtonDefs.uis", in a
    // buffer that the parser frees with free(). Returns NULL when the file
    // is not there.
    char *(*read)(void *context, const char *name, size_t *size);
    void *context;
} SSRUiFiles;

typedef struct SSRUiTree SSRUiTree;

// Reads the layout file and each file that it imports. Returns NULL when
// the file is not there.
SSRUiTree *ssr_ui_parse(const char *name, const SSRUiFiles *files);
void ssr_ui_free(SSRUiTree *tree);

// The top node. Its children are the lines of the file.
const SSRUiNode *ssr_ui_root(const SSRUiTree *tree);

// The number of problems, for example a missing import or an unclosed
// block, and the text of each one.
int ssr_ui_problem_count(const SSRUiTree *tree);
const char *ssr_ui_problem(const SSRUiTree *tree, int index);

// The value of a Const: or Define:, for example "?TweakWidth?". Returns
// NULL when there is none.
const char *ssr_ui_constant(const SSRUiTree *tree, const char *name);

#endif
