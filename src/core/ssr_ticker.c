#include "ssr_ticker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 32

typedef struct {
    char text[256];
    int priority;
    int active;
    // The copy of the template on the tape, and its width.
    int spawn;
    float width;
    // 1 when the text after this one is on the tape.
    int followed;
} Item;

struct SSRTicker {
    SSRTickerMeasure measure;
    void *context;
    Item items[MAX_ITEMS];
    int count;
    // The two templates take turns (Ticker1, Ticker2).
    int nextTemplate;
};

SSRTicker *ssr_ticker_create(SSRTickerMeasure measure, void *context) {
    SSRTicker *ticker = calloc(1, sizeof(SSRTicker));
    if (!ticker) return NULL;
    ticker->measure = measure;
    ticker->context = context;
    return ticker;
}

void ssr_ticker_free(SSRTicker *ticker) {
    free(ticker);
}

void ssr_ticker_add(SSRTicker *ticker, const char *text, int priority) {
    if (!ticker || ticker->count >= MAX_ITEMS || !text) return;
    Item *item = &ticker->items[ticker->count++];
    memset(item, 0, sizeof(*item));
    snprintf(item->text, sizeof(item->text), "%s", text);
    item->priority = priority;
}

// c_TickerManager.Spawn: the text with the highest priority that is not on
// the tape. Of equal texts, the first one goes.
static int spawn(SSRTicker *ticker, SSRScene *scene, float x) {
    int best = -1, bestPriority = -1;
    for (int i = 0; i < ticker->count; i++) {
        const Item *item = &ticker->items[i];
        if (!item->active && item->priority > bestPriority) {
            best = i;
            bestPriority = item->priority;
        }
    }
    if (best < 0) return 0;
    Item *item = &ticker->items[best];
    // The text is 100 wider than its words (c_Ticker.Activate).
    item->width = (ticker->measure ? ticker->measure(ticker->context, item->text) : 0) + 100;
    const char *template_ = ticker->nextTemplate == 0 ? "Ticker1" : "Ticker2";
    ticker->nextTemplate = !ticker->nextTemplate;
    item->spawn = ssr_scene_spawn(scene, "TickerTape", template_, x, item->text, item->width);
    if (!item->spawn) return 0;
    item->active = 1;
    item->followed = 0;
    return 1;
}

void ssr_ticker_start(SSRTicker *ticker, SSRScene *scene) {
    if (!ticker || !scene) return;
    for (int i = 0; i < ticker->count; i++) ticker->items[i].active = 0;
    spawn(ticker, scene, 500);
}

void ssr_ticker_update(SSRTicker *ticker, SSRScene *scene) {
    if (!ticker || !scene) return;
    for (int i = 0; i < ticker->count; i++) {
        Item *item = &ticker->items[i];
        if (!item->active) continue;
        float x = ssr_scene_spawn_x(scene, item->spawn);
        if (!item->followed && x + item->width < 700) item->followed = spawn(ticker, scene, x + item->width - 10);
        if (x + item->width < 0) {
            ssr_scene_spawn_remove(scene, item->spawn);
            item->active = 0;
            item->spawn = 0;
        }
    }
}
