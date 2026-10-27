// ssr_ticker: the news ticker at the foot of the screens (c_TickerManager).
// The texts go from right to left on the ticker tape of ticker.uis. When
// the end of a text comes in, the next text follows it.

#ifndef SSR_TICKER_H
#define SSR_TICKER_H

#include "ssr_scene.h"

typedef struct SSRTicker SSRTicker;

// Gives the width of a text in the font of the ticker.
typedef float (*SSRTickerMeasure)(void *context, const char *text);

SSRTicker *ssr_ticker_create(SSRTickerMeasure measure, void *context);
void ssr_ticker_free(SSRTicker *ticker);

// Adds a text that the ticker can show. A higher priority shows first. The
// welcome texts of the game have the priority 0 (c_Ticker_Dummy).
void ssr_ticker_add(SSRTicker *ticker, const char *text, int priority);

// Starts the ticker on the scene of ticker.uis: the first text starts at
// x = 500, as the game does.
void ssr_ticker_start(SSRTicker *ticker, SSRScene *scene);

// Runs after each update of the scene: the next text follows, and a text
// that went out at the left goes away.
void ssr_ticker_update(SSRTicker *ticker, SSRScene *scene);

#endif
