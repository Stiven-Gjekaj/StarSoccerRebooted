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

int main(void) {
    test_reads_elements_modifiers_and_blocks();
    test_expands_imports_macros_and_constants();
    test_reports_problems();
    return TEST_RESULT();
}
