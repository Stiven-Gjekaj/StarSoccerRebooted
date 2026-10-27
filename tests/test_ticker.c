// Tests ssr_ticker on a ticker layout that the test holds.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_test.h"
#include "ssr_ticker.h"

static char *read_layout(void *context, const char *name, size_t *size) {
    (void)context;
    (void)name;
    const char *text = "Rect:() ref:TickerTape\n"
                       "Draw:(TickerBack,0,0,200,37) ref:Ticker1\n"
                       "\tMover(-100,0,1)\n"
                       "{\n"
                       "\tText:(Testing,0,0,300,37) ref:Ticker1\n"
                       "} Template\n"
                       "Rect:(0,0,300,32) ref:Ticker2\n"
                       "\tMover(-100,0,1)\n"
                       "{\n"
                       "\tText:(Testing,0,0,300,37) ref:Ticker2\n"
                       "} Template\n";
    *size = strlen(text);
    char *copy = malloc(*size + 1);
    memcpy(copy, text, *size + 1);
    return copy;
}

// Each letter is 10 wide.
static float measure(void *context, const char *text) {
    (void)context;
    return 10.0f * (float)strlen(text);
}

static const SSRSceneItem *find(const SSRScene *scene, const char *text) {
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (item->text && strcmp(item->text, text) == 0) return item;
    }
    return NULL;
}

static int near(float a, float b) {
    return a - b < 0.01f && b - a < 0.01f;
}

static void step(SSRScene *scene, const SSRSceneEnv *env, SSRTicker *ticker) {
    ssr_scene_update(scene, env, 16);
    ssr_ticker_update(ticker, scene);
}

static void test_runs_the_texts_along_the_tape(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiFiles files = {read_layout, NULL};
    SSRUiTree *tree = ssr_ui_parse("ticker.uis", &files);
    SSRSceneEnv env = {.left = 0, .top = 0, .width = 640, .height = 960, .tweaks = tweaks};
    SSRScene *scene = ssr_scene_open(tree, &env);
    SSRTicker *ticker = ssr_ticker_create(measure, NULL);
    ssr_ticker_add(ticker, "Welcome to the game", 0);
    ssr_ticker_add(ticker, "Tutorial", 0);
    ssr_ticker_start(ticker, scene);
    // The first text starts at 500 and moves 100 a frame. It is 190 + 100
    // wide, so its end is in at once and the next text follows it, 10 back:
    // at 400 + 290 - 10.
    ssr_scene_update(scene, &env, 0);
    ssr_ticker_update(ticker, scene);
    step(scene, &env, ticker);
    const SSRSceneItem *first = find(scene, "Welcome to the game"), *next = find(scene, "Tutorial");
    CHECK(first && next && first->x < 500);
    CHECK(first && next && near(next->x - first->x, 280));
    CHECK(first && first->w == 290);
    // The first text goes out at the left at -300 and away. It comes back
    // after the second text, which waited for a free text.
    for (int i = 0; i < 6; i++) step(scene, &env, ticker);
    ssr_scene_update(scene, &env, 16);
    const SSRSceneItem *again = find(scene, "Welcome to the game");
    next = find(scene, "Tutorial");
    CHECK(again && next && near(again->x - next->x, 170));
    ssr_ticker_free(ticker);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static int asked[3];

// Kind 1 is never shown. Kind 2 counts its requests. Kind 3 keeps its
// text after the first time.
static int priority(void *context, int kind, int *count) {
    (void)context;
    asked[kind - 1]++;
    if (kind == 1) return -1;
    return ++*count;
}

static void text(void *context, int kind, int *count, char *out, size_t capacity) {
    (void)context;
    *count = 0;
    if (kind == 3 && out[0]) return;
    snprintf(out, capacity, "Kind %d", kind);
}

// The items of a kind ask the game for their priority and their text.
static void test_asks_the_kinds(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiFiles files = {read_layout, NULL};
    SSRUiTree *tree = ssr_ui_parse("ticker.uis", &files);
    SSRSceneEnv env = {.left = 0, .top = 0, .width = 640, .height = 960, .tweaks = tweaks};
    SSRScene *scene = ssr_scene_open(tree, &env);
    SSRTicker *ticker = ssr_ticker_create(measure, NULL);
    ssr_ticker_set_kinds(ticker, priority, text);
    ssr_ticker_add(ticker, "Welcome", 0);
    ssr_ticker_add_kind(ticker, 1);
    ssr_ticker_add_kind(ticker, 2);
    ssr_ticker_add_kind(ticker, 3);
    ssr_ticker_start(ticker, scene);
    ssr_scene_update(scene, &env, 0);
    // Kind 2 and kind 3 both give 1, over the welcome text: kind 2 is
    // first in the list.
    CHECK(find(scene, "Kind 2") && !find(scene, "Welcome") && asked[0] == 1);
    ssr_ticker_update(ticker, scene);
    ssr_scene_update(scene, &env, 0);
    // Then kind 3, which asked two times.
    CHECK(find(scene, "Kind 3") && asked[2] == 2 && asked[1] == 1);
    ssr_ticker_free(ticker);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_asks_the_kinds();
    test_runs_the_texts_along_the_tape();
    return TEST_RESULT();
}
