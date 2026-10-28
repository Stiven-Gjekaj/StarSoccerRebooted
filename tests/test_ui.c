// Tests ssr_ui with layout files that the test holds in memory.

#include <stdlib.h>
#include <string.h>

#include "ssr_test.h"
#include "ssr_ui.h"

typedef struct {
    const char *name;
    const char *text;
} MemoryFile;

static char *read_memory(void *context, const char *name, size_t *size) {
    for (const MemoryFile *file = context; file->name; file++) {
        if (strcmp(file->name, name) == 0) {
            *size = strlen(file->text);
            char *copy = malloc(*size + 1);
            memcpy(copy, file->text, *size + 1);
            return copy;
        }
    }
    return NULL;
}

static SSRUiTree *parse(const MemoryFile *files, const char *name) {
    SSRUiFiles reader = {read_memory, (void *)files};
    return ssr_ui_parse(name, &reader);
}

static const SSRUiNode *child(const SSRUiNode *node, int index) {
    return node && index < node->childCount ? node->children[index] : NULL;
}

static int is(const SSRUiNode *node, const char *name) {
    return node && strcmp(node->name, name) == 0;
}

static void test_reads_elements_modifiers_and_blocks(void) {
    const MemoryFile files[] = {
        {"Screen.uis",
         "// A comment\r\n"
         "Draw:(Title_Sky,@L,@T,@W,@H) ref:Sky // the sky\r\n"
         "\tHandle(42,16)\r\n"
         "\tAdditive()\r\n"
         "Rect:(0,0,100,[@B-960]*0.7)\r\n"
         "\tPress()\r\n"
         "\t<\r\n"
         "\t\tConvergeTo(0,8)\r\n"
         "\t>\r\n"
         "{\r\n"
         "\tText:(Hello,0,0,100,40)\r\n"
         "\tPopOrigin\r\n"
         "}\r\n"
         "\t.set(%t,2)\r\n"
         "DownloadImage(http://example.com/a.png)\r\n"
         "Rect:(0,0,0,0) $HIDE\r\n"
         "Rect:()\r\n"
         "\t$HIDE\r\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "Screen.uis");
    CHECK(tree != NULL);
    const SSRUiNode *root = ssr_ui_root(tree);
    CHECK(root && root->childCount == 4);
    CHECK(child(root, 2) && (child(root, 2)->flags & SSR_UI_HIDE));
    CHECK(child(root, 3) && (child(root, 3)->flags & SSR_UI_HIDE) && child(root, 3)->argCount == 0);
    CHECK(!(child(root, 1)->flags & SSR_UI_HIDE));

    const SSRUiNode *draw = child(root, 0);
    CHECK(is(draw, "Draw") && (draw->flags & SSR_UI_ELEMENT));
    CHECK(draw->argCount == 5 && strcmp(draw->args[0], "Title_Sky") == 0 && strcmp(draw->args[4], "@H") == 0);
    CHECK(draw->ref && strcmp(draw->ref, "Sky") == 0);
    CHECK(draw->modifierCount == 2 && is(draw->modifiers[0], "Handle") && is(draw->modifiers[1], "Additive"));
    CHECK(draw->modifiers[1]->argCount == 0);
    CHECK(draw->raw && strcmp(draw->raw, "Title_Sky,@L,@T,@W,@H") == 0);
    CHECK(draw->line == 2);

    const SSRUiNode *rect = child(root, 1);
    CHECK(is(rect, "Rect") && rect->argCount == 4 && strcmp(rect->args[3], "[@B-960]*0.7") == 0);
    CHECK(rect->modifierCount == 3);
    const SSRUiNode *press = rect->modifiers[0];
    CHECK(is(press, "Press") && press->childCount == 1 && is(press->children[0], "ConvergeTo"));
    CHECK(rect->childCount == 2 && is(rect->children[0], "Text") && is(rect->children[1], "PopOrigin"));
    CHECK(is(rect->modifiers[1], "set") && (rect->modifiers[1]->flags & SSR_UI_DOT));
    CHECK(is(rect->modifiers[2], "DownloadImage") && strcmp(rect->modifiers[2]->args[0], "http://example.com/a.png") == 0);
    CHECK(ssr_ui_problem_count(tree) == 0);
    ssr_ui_free(tree);
}

static void test_expands_imports_macros_and_constants(void) {
    const MemoryFile files[] = {
        {"Macros.uis",
         "Const:(?ButtonWidth?=290)\n"
         "Macro:(M_Button,?X?,?Y?,?TEXT?,?REF?)\n"
         "\tRect:(?X?,?Y?,?ButtonWidth?,80) ref:?REF?\n"
         "\tIf:(?TEXT?)\n"
         "\t\tText:(?TEXT?,?X?,?Y?)\n"
         "\tElse\n"
         "\t\tText:(Empty)\n"
         "\tEndIf\n"
         "EndM\n"},
        {"Screen.uis",
         "Import:(Macros.uis)\n"
         "M_Button:(10,20,Play,PlayButton)\n"
         "M_Button:(10,120,,Other)\n"
         "If:(?ButtonWidth?=290)\n"
         "\tImg:(Yes)\n"
         "Else\n"
         "\tImg:(No)\n"
         "EndIf\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "Screen.uis");
    const SSRUiNode *root = ssr_ui_root(tree);
    CHECK(ssr_ui_problem_count(tree) == 0);
    CHECK(root && root->childCount == 5);
    CHECK(strcmp(ssr_ui_constant(tree, "?ButtonWidth?"), "290") == 0);

    const SSRUiNode *first = child(root, 0);
    CHECK(is(first, "Rect") && strcmp(first->args[2], "290") == 0 && strcmp(first->ref, "PlayButton") == 0);
    CHECK(is(child(root, 1), "Text") && strcmp(child(root, 1)->args[0], "Play") == 0);
    CHECK(is(child(root, 2), "Rect") && strcmp(child(root, 2)->args[1], "120") == 0);
    CHECK(is(child(root, 3), "Text") && strcmp(child(root, 3)->args[0], "Empty") == 0);
    CHECK(is(child(root, 4), "Img") && strcmp(child(root, 4)->args[0], "Yes") == 0);
    ssr_ui_free(tree);
}

static void test_reports_problems(void) {
    const MemoryFile files[] = {
        {"Screen.uis",
         "Import:(Missing.uis)\n"
         "}\n"
         "Rect:(0,0)\n"
         "{\n"
         "If:(1)\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "Screen.uis");
    CHECK(tree != NULL);
    CHECK(ssr_ui_problem_count(tree) == 4);
    CHECK(strstr(ssr_ui_problem(tree, 0), "cannot import Missing.uis") != NULL);
    CHECK(strstr(ssr_ui_problem(tree, 0), "Screen.uis:1") != NULL);
    ssr_ui_free(tree);
    CHECK(parse(files, "Nothing.uis") == NULL);
}

// The button macros of the game end with PushParent() and PopParent(). Then
// the block and the modifiers after the macro go to the inner element.
static void test_pushes_and_pops_the_parent(void) {
    const MemoryFile files[] = {
        {"Screen.uis",
         "Rect:(5,5,90,40)\n"
         "\tPress()\n"
         "{\n"
         "\tRect:(0,0,100,50) ref:Inner\n"
         "\t{\n"
         "\t\tDraw:(GreenButton,0,0,100,50)\n"
         "\t}\n"
         "\tPushParent()\n"
         "}\n"
         "PopParent()\n"
         "{\n"
         "\tText:(Play,0,0,100,50)\n"
         "}\n"
         "\tDelay(0.1)\n"
         ":Click(play)\n"
         "Draw:(Next,0,0)\n"
         "\tAdditive()\n"
         "{{\n"
         "\tDraw:(Boxed,0,0)\n"
         "}}\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "Screen.uis");
    const SSRUiNode *root = ssr_ui_root(tree);
    const SSRUiNode *outer = child(root, 0);
    CHECK(outer && is(outer, "Rect") && outer->modifierCount == 1);
    const SSRUiNode *inner = outer ? child(outer, 0) : NULL;
    CHECK(inner && inner->ref && strcmp(inner->ref, "Inner") == 0);
    CHECK(inner && inner->childCount == 2 && is(inner->children[1], "Text"));
    CHECK(inner && inner->modifierCount == 2 && is(inner->modifiers[0], "Delay"));
    CHECK(inner && (inner->flags & SSR_UI_BLOCK) && inner->blockAt == 0);
    CHECK(outer && (outer->flags & SSR_UI_BLOCK) && outer->blockAt == 1);
    CHECK(inner && (inner->modifiers[1]->flags & SSR_UI_REARM));
    const SSRUiNode *next = NULL;
    for (int i = 0; i < root->childCount; i++) {
        if (is(root->children[i], "Draw")) next = root->children[i];
    }
    CHECK(next && next->modifierCount == 1 && (next->flags & SSR_UI_BOXED));
    CHECK(next && next->childCount == 1 && is(next->children[0], "Draw"));
    CHECK(ssr_ui_problem_count(tree) == 0);
    ssr_ui_free(tree);
}

// A block after PopOrigin gets a group of its own. It is not in the element
// before the PopOrigin.
static void test_opens_a_group_after_pop_origin(void) {
    const MemoryFile files[] = {
        {"Screen.uis",
         "PushOrigin:(98,194)\n"
         "\tDraw:(Back,0,0,444,599)\n"
         "PopOrigin()\n"
         "{\n"
         "\tRect:(-10,0,0,0) ref:List\n"
         "}\n"
         "Macro:(M_Bar)\n"
         "\tDraw:(Bar,0,0)\n"
         "\tPopOrigin\n"
         "EndM\n"
         "M_Bar:()\n"
         "{\n"
         "\tText:(Buy,0,0)\n"
         "}\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "Screen.uis");
    const SSRUiNode *root = ssr_ui_root(tree);
    const SSRUiNode *draw = NULL, *group = NULL, *bar = NULL;
    for (int i = 0; root && i < root->childCount; i++) {
        const SSRUiNode *node = root->children[i];
        if (is(node, "Draw") && strcmp(node->args[0], "Back") == 0) draw = node;
        if (is(node, "Draw") && strcmp(node->args[0], "Bar") == 0) bar = node;
        if (is(node, "Group")) group = node;
    }
    CHECK(draw && draw->childCount == 0);
    CHECK(group && group->childCount == 1 && is(child(group, 0), "Rect"));
    // A block after a macro that ends with PopOrigin is in the last element
    // of the macro.
    CHECK(bar && bar->childCount == 1 && is(child(bar, 0), "Text"));
    CHECK(ssr_ui_problem_count(tree) == 0);
    ssr_ui_free(tree);
}

static void test_keeps_quoted_text_whole(void) {
    const MemoryFile files[] = {
        {"q.uis", "Group:(0,0)\n\tset(gel.text,\"(\"::fmt(%c,X)::\", \"::\")\") ref:Q\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "q.uis");
    const SSRUiNode *group = child(ssr_ui_root(tree), 0);
    const SSRUiNode *set = group && group->modifierCount ? group->modifiers[0] : NULL;
    CHECK(is(set, "set") && set->argCount == 2);
    CHECK(set && strcmp(set->args[1], "\"(\"::fmt(%c,X)::\", \"::\")\"") == 0);
    CHECK(set && strcmp(set->raw, "gel.text,\"(\"::fmt(%c,X)::\", \"::\")\"") == 0);
    CHECK(set && set->ref && strcmp(set->ref, "Q") == 0);
    ssr_ui_free(tree);
}

static void test_nests_branch_blocks(void) {
    const MemoryFile files[] = {
        {"n.uis", "Draw:(G,0,0,5,5)\n"
                  "\tDoIf(A,B,1)\n"
                  "\t<\n"
                  "\t\tDoIf(C,D,1)\n"
                  "\t\t<\n"
                  "\t\t\tConvergeTo(0,-410)\n"
                  "\t\t>\n"
                  "\t\tFader(1)\n"
                  "\t>\n"},
        {NULL, NULL},
    };
    SSRUiTree *tree = parse(files, "n.uis");
    const SSRUiNode *draw = child(ssr_ui_root(tree), 0);
    CHECK(is(draw, "Draw") && draw->modifierCount == 1 && draw->childCount == 0);
    const SSRUiNode *outer = draw->modifiers[0];
    CHECK(is(outer, "DoIf") && outer->childCount == 2);
    const SSRUiNode *inner = child(outer, 0);
    CHECK(is(inner, "DoIf") && inner->childCount == 1 && is(child(inner, 0), "ConvergeTo"));
    CHECK(is(child(outer, 1), "Fader"));
    CHECK(ssr_ui_problem_count(tree) == 0);
    ssr_ui_free(tree);
}

static void test_puts_a_patch_on_a_layout(void) {
    const char *text = "Rect:(0,0,10,10)\r\n"
                       "\t//TM\r\n"
                       "\tText:(TM,428,6)\r\n"
                       "\tText:(TM,421,6)  \n"
                       "\tText:(TMX,1,1)\n"
                       "Draw:(Logo,0,0)";
    const char *patch = "# The remake has no TM.\n"
                        "-\tText:(TM, \n"
                        "-\n"
                        "Text:(TMX,1,1)\n";
    int removed = 0;
    char *out = ssr_ui_patch(text, patch, &removed);
    CHECK(removed == 2);
    CHECK(out && strcmp(out, "Rect:(0,0,10,10)\r\n\t//TM\r\n\tText:(TMX,1,1)\n" "Draw:(Logo,0,0)") == 0);
    free(out);
    // A patch with no "-" lines changes nothing.
    out = ssr_ui_patch(text, "Text:(TMX,1,1)\n", &removed);
    CHECK(removed == 0 && out && strcmp(out, text) == 0);
    free(out);
    // The "+" lines after a "-" line take the place of each line that it
    // removes, with the spaces of that line.
    out = ssr_ui_patch(text, "- Text:(TM,\n+ Draw:(Word,-32,168)\n+  Layer(Top) \n- Draw:(Logo\n", &removed);
    CHECK(removed == 3);
    CHECK(out && strcmp(out, "Rect:(0,0,10,10)\r\n\t//TM\r\n"
                             "\tDraw:(Word,-32,168)\n\tLayer(Top)\n"
                             "\tDraw:(Word,-32,168)\n\tLayer(Top)\n"
                             "\tText:(TMX,1,1)\n") == 0);
    free(out);
}

int main(void) {
    test_puts_a_patch_on_a_layout();
    test_nests_branch_blocks();
    test_opens_a_group_after_pop_origin();
    test_keeps_quoted_text_whole();
    test_pushes_and_pops_the_parent();
    test_reads_elements_modifiers_and_blocks();
    test_expands_imports_macros_and_constants();
    test_reports_problems();
    return TEST_RESULT();
}
