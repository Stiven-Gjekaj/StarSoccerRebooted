// ssr_blackjack: the rules of blackjack (c_TBlackJack, c_TCard). One deck
// of 52 cards is a queue. The screen is in ssr_game.

#ifndef SSR_BLACKJACK_H
#define SSR_BLACKJACK_H

#include "ssr_random.h"

#define SSR_CARDS 52

typedef enum {
    SSR_SUIT_HEART,
    SSR_SUIT_DIAMOND,
    SSR_SUIT_CLUB,
    SSR_SUIT_SPADE,
} SSRSuit;

typedef enum {
    SSR_OWNER_NONE,
    SSR_OWNER_PLAYER,
    SSR_OWNER_DEALER,
} SSROwner;

// A card (c_TCard). num is 1 for an ace, 11 to 13 for the jack, the queen,
// and the king.
typedef struct {
    int num;
    SSRSuit suit;
    int randno;
    float x, y, desx, desy, rot, desrot, flip, desflip;
    int dealorder;
    int flipsound;
    SSROwner owner;
    // The place in the hand, from 1.
    int posinhand;
} SSRCard;

// What the screen must show after a call (AScreen_BlackJack).
typedef enum {
    // A card starts to slide to its place (AScreen.Hit).
    SSR_BJ_DEALT,
    // The hole card of the dealer turns over (FlipDealerCard).
    SSR_BJ_FLIP,
    // A banner: the text tag is in tag.
    SSR_BJ_BANNER,
    // Hit and Hold show (value 1) or hide (value 0).
    SSR_BJ_BUTTONS,
    // The result: value 1 the player wins, 2 the dealer wins, 0 a tie.
    SSR_BJ_RESULT,
    // The hand is over and the table is clear (NextGame).
    SSR_BJ_NEXT,
} SSRBlackjackEventKind;

typedef struct {
    SSRBlackjackEventKind kind;
    int card;
    int value;
    const char *tag;
} SSRBlackjackEvent;

typedef enum {
    SSR_BJ_IDLE,
    SSR_BJ_PLAYER,
    SSR_BJ_DEALER,
    SSR_BJ_SHOW_RESULT,
} SSRBlackjackTurn;

typedef struct {
    SSRCard cards[SSR_CARDS];
    // The queue: the order of the cards (TCard.glist).
    int queue[SSR_CARDS];
    int made, dealcount, sortby;
    // The hands, in the order of the deal.
    int player[5], playerCount, dealer[5], dealerCount;
    SSRBlackjackTurn turn;
    int result, dealerPlayed;
    int scr1, scr2, dscr1, dscr2;
    float pscoreAlpha, dscoreAlpha;
    // 1 while Hit and Hold show.
    int buttons;
    SSRBlackjackEvent events[16];
    int eventCount;
} SSRBlackjack;

// TBlackJack.SetUp: the deck of 52 cards, one time (52 Rand(9999)).
void ssr_blackjack_setup(SSRBlackjack *bj, SSRRandom *random);

// TBlackJack.Clear: the next setup makes a new deck.
void ssr_blackjack_clear(SSRBlackjack *bj);

// TBlackJack.Play: a new deal when no hand runs, or a card for the player
// in the turn of the player.
void ssr_blackjack_play(SSRBlackjack *bj, SSRRandom *random);

// TBlackJack.Hold: the turn of the dealer, when the cards of the player are
// in their places.
void ssr_blackjack_hold(SSRBlackjack *bj);

// TBlackJack.Update, one frame. message is 1 while a banner shows. It
// moves the cards (TCard.UpdateAll).
void ssr_blackjack_update(SSRBlackjack *bj, SSRRandom *random, int message);

// The best total of a hand: the soft total when it is 21 or less.
int ssr_blackjack_best(int hard, int soft);

#endif
