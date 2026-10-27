// Tests ssr_blackjack: the deck, the deal, the timing, and the rules.

#include <math.h>
#include <string.h>

#include "ssr_blackjack.h"
#include "ssr_test.h"

// Runs frames until an event of the kind comes. Returns the frame of it,
// or -1. A banner shows for 90 frames (1500 ms).
static int frames_to(SSRBlackjack *bj, SSRRandom *random, SSRBlackjackEventKind kind, int *banner, int max) {
    for (int f = 1; f <= max; f++) {
        bj->eventCount = 0;
        ssr_blackjack_update(bj, random, *banner > 0);
        if (*banner > 0) (*banner)--;
        for (int i = 0; i < bj->eventCount; i++) {
            if (bj->events[i].kind == SSR_BJ_BANNER) *banner = 90;
            if (bj->events[i].kind == kind) return f;
        }
    }
    return -1;
}

static void test_makes_the_deck(void) {
    SSRBlackjack bj = {0};
    SSRRandom random = {SSR_RANDOM_START};
    ssr_blackjack_setup(&bj, &random);
    int seen[SSR_CARDS] = {0};
    for (int i = 0; i < SSR_CARDS; i++) {
        seen[bj.queue[i]]++;
        CHECK(bj.cards[i].randno >= 1 && bj.cards[i].randno <= 9999);
    }
    for (int i = 0; i < SSR_CARDS; i++) CHECK(seen[i] == 1);
    CHECK(bj.cards[0].num == 1 && bj.cards[0].suit == SSR_SUIT_HEART);
    CHECK(bj.cards[51].num == 13 && bj.cards[51].suit == SSR_SUIT_SPADE);
    // A second setup keeps the deck: no number is taken.
    int seed = random.seed;
    ssr_blackjack_setup(&bj, &random);
    CHECK(random.seed == seed);
}

static void test_deals_and_times_a_hand(void) {
    SSRBlackjack bj = {0};
    SSRRandom random = {12345};
    ssr_blackjack_setup(&bj, &random);
    int seed = random.seed;
    ssr_blackjack_play(&bj, &random);
    // The deal shuffles the 52 cards: 52 numbers.
    SSRRandom count = {seed};
    for (int i = 0; i < 52; i++) ssr_rand(&count, 9999);
    CHECK(random.seed == count.seed);
    CHECK(bj.turn == SSR_BJ_PLAYER && bj.playerCount == 2 && bj.dealerCount == 2);
    CHECK(bj.cards[bj.dealer[1]].desflip == -1.0f && bj.cards[bj.player[0]].desy == 580);
    CHECK(bj.cards[bj.player[1]].desx == 262 && bj.cards[bj.dealer[0]].desy == 380);
    // The cards slide one at a time, in the order of the deal.
    int banner = 0, dealt[4] = {0}, n = 0, buttons = -1;
    for (int f = 1; f <= 300 && buttons < 0 && bj.turn == SSR_BJ_PLAYER; f++) {
        bj.eventCount = 0;
        ssr_blackjack_update(&bj, &random, 0);
        for (int i = 0; i < bj.eventCount; i++) {
            if (bj.events[i].kind == SSR_BJ_DEALT && n < 4) dealt[n++] = f;
            if (bj.events[i].kind == SSR_BJ_BUTTONS && bj.events[i].value == 1) buttons = f;
        }
    }
    CHECK(n == 4 && dealt[0] == 1 && dealt[1] == 46 && dealt[2] == 90 && dealt[3] == 135);
    // Hit and Hold show when the hole card is in its place: about 3 s.
    CHECK(buttons == 180);
    (void)banner;
}

static int player_in_position(const SSRBlackjack *bj) {
    for (int i = 0; i < bj->playerCount; i++) {
        const SSRCard *c = &bj->cards[bj->player[i]];
        if (fabsf(c->x - c->desx) > 5 || fabsf(c->y - c->desy) > 5 || c->flip < c->desflip) return 0;
    }
    return 1;
}

// Plays many hands: the player holds on 17 or more. The rules must hold
// in each hand.
static void test_keeps_the_rules(void) {
    SSRBlackjack bj = {0};
    SSRRandom random = {777};
    ssr_blackjack_setup(&bj, &random);
    int results[3] = {0}, blackjacks = 0;
    for (int hand = 0; hand < 300; hand++) {
        int banner = 0;
        ssr_blackjack_play(&bj, &random);
        int done = 0;
        for (int f = 0; f < 3000 && !done; f++) {
            // The player acts when the scores of the cards in place are new.
            int ready = bj.turn == SSR_BJ_PLAYER && player_in_position(&bj);
            bj.eventCount = 0;
            ssr_blackjack_update(&bj, &random, banner > 0);
            if (banner > 0) banner--;
            if (ready && bj.turn == SSR_BJ_PLAYER && bj.buttons) {
                if (ssr_blackjack_best(bj.scr1, bj.scr2) >= 17) ssr_blackjack_hold(&bj);
                else ssr_blackjack_play(&bj, &random);
            }
            for (int i = 0; i < bj.eventCount; i++) {
                const SSRBlackjackEvent *e = &bj.events[i];
                if (e->kind == SSR_BJ_BANNER) banner = 90;
                if (e->kind == SSR_BJ_BANNER && strcmp(e->tag, "blackjack_BlackJack") == 0 && bj.playerCount == 2 &&
                    bj.dealerPlayed == 0) {
                    blackjacks++;
                }
                if (e->kind != SSR_BJ_RESULT) continue;
                done = 1;
                results[e->value]++;
                int p = ssr_blackjack_best(bj.scr1, bj.scr2), d = ssr_blackjack_best(bj.dscr1, bj.dscr2);
                // No hand holds two aces.
                int aces = 0;
                for (int k = 0; k < bj.playerCount; k++) aces += bj.cards[bj.player[k]].num == 1;
                CHECK(aces <= 1);
                if (bj.scr1 > 21) {
                    CHECK(e->value == 2);
                } else if (bj.dealerPlayed) {
                    // The dealer stands on 17 or more, or busts, or has 5 cards.
                    CHECK(d >= 17 || bj.dealerCount == 5);
                    if (bj.dscr1 > 21) CHECK(e->value == 1);
                    else if (bj.playerCount == 5 && bj.dealerCount != 5) CHECK(e->value == 1);
                    else if (bj.dealerCount == 5 && bj.playerCount != 5) CHECK(e->value == 2);
                    else if (!(bj.dscr2 == 21 && bj.dealerCount == 2)) CHECK(e->value == (p > d ? 1 : d > p ? 2 : 0));
                }
            }
        }
        CHECK(done);
        // NextGame clears the table after the banner.
        CHECK(frames_to(&bj, &random, SSR_BJ_NEXT, &banner, 600) > 0 && bj.turn == SSR_BJ_IDLE);
    }
    CHECK(results[0] > 0 && results[1] > 0 && results[2] > 0 && blackjacks > 0);
}

int main(void) {
    test_makes_the_deck();
    test_deals_and_times_a_hand();
    test_keeps_the_rules();
    return TEST_RESULT();
}
