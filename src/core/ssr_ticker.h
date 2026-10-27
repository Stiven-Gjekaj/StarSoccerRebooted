// ssr_ticker: the news ticker at the foot of the screens (c_TickerManager).
// The texts go from right to left on the ticker tape of ticker.uis. When
// the end of a text comes in, the next text follows it.

#ifndef SSR_TICKER_H
#define SSR_TICKER_H

#include <stddef.h>

#include "ssr_scene.h"

typedef struct SSRTicker SSRTicker;

// Gives the width of a text in the font of the ticker.
typedef float (*SSRTickerMeasure)(void *context, const char *text);

SSRTicker *ssr_ticker_create(SSRTickerMeasure measure, void *context);
void ssr_ticker_free(SSRTicker *ticker);

// The priority of an item of a kind when the ticker picks the next text
// (c_Ticker.Priority). -1 does not show. The function can change *count,
// the requests since the item last showed (m_showRequests).
typedef int (*SSRTickerPriority)(void *context, int kind, int *count);
// The text of the item of a kind that shows (c_Ticker.Activate). A function
// that leaves the text keeps the last text of the item.
typedef void (*SSRTickerText)(void *context, int kind, int *count, char *text, size_t capacity);

// The functions of the items of a kind. Their context is the context of
// the ticker.
void ssr_ticker_set_kinds(SSRTicker *ticker, SSRTickerPriority priority, SSRTickerText text);

// Adds an item of a kind (from 1). Its priority and its text come from the
// functions of the kinds.
void ssr_ticker_add_kind(SSRTicker *ticker, int kind);

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
