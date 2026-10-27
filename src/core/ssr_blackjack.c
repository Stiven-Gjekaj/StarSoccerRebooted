#include "ssr_blackjack.h"

#include <math.h>
#include <string.h>

#include "ssr_sort.h"

static void event(SSRBlackjack *bj, SSRBlackjackEventKind kind, int card, int value, const char *tag) {
    if (bj->eventCount >= (int)(sizeof(bj->events) / sizeof(bj->events[0]))) return;
    SSRBlackjackEvent *e = &bj->events[bj->eventCount++];
    e->kind = kind;
    e->card = card;
    e->value = value;
    e->tag = tag;
}

// TCard.Compare9: sortby 5 by randno, sortby 32 by dealorder.
static int compare(void *context, int a, int b) {
    const SSRBlackjack *bj = context;
    int x = bj->sortby == 32 ? bj->cards[a].dealorder : bj->cards[a].randno;
    int y = bj->sortby == 32 ? bj->cards[b].dealorder : bj->cards[b].randno;
    return x < y ? -1 : x > y ? 1 : 0;
}

static void sort_by(SSRBlackjack *bj, int sortby) {
    bj->sortby = sortby;
    ssr_sort(bj->queue, SSR_CARDS, compare, bj, 0);
}

void ssr_blackjack_setup(SSRBlackjack *bj, SSRRandom *random) {
    if (bj->made) return;
    memset(bj, 0, sizeof(*bj));
    for (int suit = 0; suit < 4; suit++) {
        for (int num = 1; num <= 13; num++) {
            int i = suit * 13 + num - 1;
            SSRCard *c = &bj->cards[i];
            c->randno = ssr_rand(random, 9999);
            c->num = num;
            c->suit = (SSRSuit)suit;
            c->flipsound = 1;
            bj->queue[i] = i;
        }
    }
    bj->made = 1;
}

void ssr_blackjack_clear(SSRBlackjack *bj) {
    bj->made = 0;
    bj->playerCount = bj->dealerCount = 0;
}

// TCard.Pull: the first card of the queue goes to its end.
static int pull(SSRBlackjack *bj) {
    sort_by(bj, 5);
    int c = bj->queue[0];
    bj->cards[c].randno = bj->cards[bj->queue[SSR_CARDS - 1]].randno + 1;
    return c;
}

// TCard.Shuffle: each card takes a new number, in the order of the queue.
static void shuffle(SSRBlackjack *bj, SSRRandom *random) {
    for (int k = 0; k < SSR_CARDS; k++) {
        SSRCard *c = &bj->cards[bj->queue[k]];
        c->randno = ssr_rand(random, 9999);
        c->dealorder = 0;
        c->x = c->y = c->desx = c->desy = 0;
        c->flip = c->desflip = 1.0f;
        c->flipsound = 1;
    }
    bj->dealcount = 1;
    sort_by(bj, 5);
}

static int in_position(const SSRCard *c) {
    return fabsf(c->x - c->desx) <= 5 && fabsf(c->y - c->desy) <= 5 && c->flip >= c->desflip;
}

static int hand_in_position(const SSRBlackjack *bj, const int *hand, int count) {
    for (int i = 0; i < count; i++) {
        if (!in_position(&bj->cards[hand[i]])) return 0;
    }
    return 1;
}

// TBlackJack.Hit: a hand never holds two aces; a second ace goes back.
static void hit(SSRBlackjack *bj, SSROwner owner, float flip) {
    int *hand = owner == SSR_OWNER_PLAYER ? bj->player : bj->dealer;
    int *count = owner == SSR_OWNER_PLAYER ? &bj->playerCount : &bj->dealerCount;
    if (bj->scr1 > 21 || *count == 5) return;
    int hasAce = 0;
    for (int i = 0; i < *count; i++) hasAce |= bj->cards[hand[i]].num == 1;
    float px = (float)*count * 42.0f + 220.0f, py = owner == SSR_OWNER_PLAYER ? 580.0f : 380.0f;
    int c;
    do {
        c = pull(bj);
    } while (hasAce && bj->cards[c].num <= 1);
    // TCard.Init6.
    SSRCard *card = &bj->cards[c];
    card->desx = px;
    card->desy = py;
    card->rot = 90;
    card->x = 808;
    card->y = 419;
    card->desrot = 0;
    card->flip = card->desflip = flip;
    card->dealorder = bj->dealcount++;
    card->flipsound = 0;
    card->owner = owner;
    hand[(*count)++] = c;
    card->posinhand = *count;
}

// UpdatePlayerScore and UpdateDealerScore: an ace is 1, or 11 in scr2.
static void hand_score(const SSRBlackjack *bj, const int *hand, int count, int *hard, int *soft) {
    int sum = 0, ace = 0;
    for (int i = 0; i < count; i++) {
        int num = bj->cards[hand[i]].num;
        sum += num <= 10 ? num : 10;
        ace |= num == 1;
    }
    *hard = sum;
    *soft = ace ? sum + 10 : 0;
    if (*soft > 21) *soft = 0;
}

int ssr_blackjack_best(int hard, int soft) {
    return soft > hard && soft < 22 ? soft : hard;
}

static void reset(SSRBlackjack *bj, SSRRandom *random) {
    shuffle(bj, random);
    bj->playerCount = bj->dealerCount = 0;
    bj->turn = SSR_BJ_IDLE;
    bj->result = bj->dealerPlayed = 0;
    bj->scr1 = bj->scr2 = bj->dscr1 = bj->dscr2 = 0;
}

static void deal(SSRBlackjack *bj, SSRRandom *random) {
    reset(bj, random);
    hit(bj, SSR_OWNER_PLAYER, 1.0f);
    hit(bj, SSR_OWNER_PLAYER, 1.0f);
    hit(bj, SSR_OWNER_DEALER, 1.0f);
    // The hole card is face down.
    hit(bj, SSR_OWNER_DEALER, -1.0f);
    bj->turn = SSR_BJ_PLAYER;
}

void ssr_blackjack_play(SSRBlackjack *bj, SSRRandom *random) {
    if (bj->turn == SSR_BJ_IDLE || bj->turn == SSR_BJ_SHOW_RESULT) deal(bj, random);
    else if (bj->turn == SSR_BJ_PLAYER) hit(bj, SSR_OWNER_PLAYER, 1.0f);
}

static void set_buttons(SSRBlackjack *bj, int show) {
    if (bj->buttons == show) return;
    bj->buttons = show;
    event(bj, SSR_BJ_BUTTONS, -1, show, NULL);
}

void ssr_blackjack_hold(SSRBlackjack *bj) {
    if (!hand_in_position(bj, bj->player, bj->playerCount)) return;
    bj->turn = SSR_BJ_DEALER;
    set_buttons(bj, 0);
}

// ShowResult: with no result yet, 5 cards beat fewer cards, else the best
// totals decide.
static void show_result(SSRBlackjack *bj) {
    if (bj->result == 0) {
        int p = ssr_blackjack_best(bj->scr1, bj->scr2), d = ssr_blackjack_best(bj->dscr1, bj->dscr2);
        if (bj->playerCount == 5 && bj->dealerCount != 5) bj->result = 1;
        else if (bj->dealerCount == 5 && bj->playerCount != 5) bj->result = 2;
        else if (p > d) bj->result = 1;
        else if (d > p) bj->result = 2;
    }
    if (bj->result == 0) event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_Tie");
    else if (bj->result == 1) event(bj, SSR_BJ_BANNER, -1, 0, "bet_YouWon");
    else if (bj->scr1 <= 21) event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_DealerWins");
    event(bj, SSR_BJ_RESULT, -1, bj->result, NULL);
}

// CheckPlayerScore: a bust, a blackjack, five cards, or a hard 21 ends the
// turn of the player.
static void check_player_score(SSRBlackjack *bj) {
    hand_score(bj, bj->player, bj->playerCount, &bj->scr1, &bj->scr2);
    if (bj->scr1 > 21) {
        bj->turn = SSR_BJ_SHOW_RESULT;
        bj->result = 2;
        event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_Bust");
    } else if (bj->scr2 == 21 && bj->playerCount == 2) {
        bj->turn = SSR_BJ_SHOW_RESULT;
        bj->result = 1;
        event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_BlackJack");
    } else if (bj->playerCount == 5) {
        bj->turn = SSR_BJ_DEALER;
        event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_5CardTrick");
    } else if (bj->scr1 == 21) {
        bj->turn = SSR_BJ_DEALER;
    }
    set_buttons(bj, bj->turn == SSR_BJ_PLAYER && hand_in_position(bj, bj->dealer, bj->dealerCount));
    if (bj->turn == SSR_BJ_SHOW_RESULT) show_result(bj);
}

// DealersTurn: the hole card turns first. Then the dealer stands on 17 or
// more, soft 17 too.
static void dealers_turn(SSRBlackjack *bj) {
    for (int i = 0; i < bj->dealerCount; i++) {
        SSRCard *c = &bj->cards[bj->dealer[i]];
        if (c->desflip != 1.0f) {
            c->desflip = 1.0f;
            event(bj, SSR_BJ_FLIP, bj->dealer[i], 0, NULL);
            return;
        }
    }
    bj->dealerPlayed = 1;
    hand_score(bj, bj->dealer, bj->dealerCount, &bj->dscr1, &bj->dscr2);
    if (bj->dscr1 > 21) {
        bj->turn = SSR_BJ_SHOW_RESULT;
        bj->result = 1;
    } else if (bj->dscr2 == 21 && bj->dealerCount == 2) {
        bj->turn = SSR_BJ_SHOW_RESULT;
        bj->result = 2;
        event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_BlackJack");
    } else if (bj->dealerCount == 5) {
        bj->turn = SSR_BJ_SHOW_RESULT;
        event(bj, SSR_BJ_BANNER, -1, 0, "blackjack_5CardTrick");
    } else if (bj->dscr1 > 16 || bj->dscr2 > 16) {
        bj->turn = SSR_BJ_SHOW_RESULT;
    } else {
        hit(bj, SSR_OWNER_DEALER, 1.0f);
    }
    if (bj->turn == SSR_BJ_SHOW_RESULT) show_result(bj);
}

// Card.Update: the card moves a tenth of the way to its place. Its first
// update starts its slide on the screen.
static void card_update(SSRBlackjack *bj, int i) {
    SSRCard *c = &bj->cards[i];
    if (c->desx != c->x) c->x = c->x + (c->desx - c->x) * 0.1f;
    if (c->desy != c->y) c->y = c->y + (c->desy - c->y) * 0.1f;
    if (c->desrot != c->rot) c->rot = c->rot + (c->desrot - c->rot) * 0.1f;
    if (c->flip < c->desflip) c->flip = c->flip + 0.05f;
    if (!c->flipsound && !(c->flip <= 0 && c->desx + c->desy == 0)) {
        c->flipsound = 1;
        event(bj, SSR_BJ_DEALT, i, 0, NULL);
    }
}

// TCard.UpdateAll: the cards move in the order of the deal, one at a time.
static void update_all(SSRBlackjack *bj) {
    sort_by(bj, 32);
    for (int k = 0; k < SSR_CARDS; k++) {
        card_update(bj, bj->queue[k]);
        if (!in_position(&bj->cards[bj->queue[k]])) return;
    }
    sort_by(bj, 5);
}

void ssr_blackjack_update(SSRBlackjack *bj, SSRRandom *random, int message) {
    switch (bj->turn) {
    case SSR_BJ_IDLE:
        bj->pscoreAlpha = bj->dscoreAlpha = 0;
        break;
    case SSR_BJ_PLAYER:
        if (hand_in_position(bj, bj->player, bj->playerCount)) bj->pscoreAlpha += 0.05f;
        if (bj->pscoreAlpha > 1.0f) {
            bj->pscoreAlpha = 1.0f;
            check_player_score(bj);
        }
        break;
    case SSR_BJ_DEALER:
        if (hand_in_position(bj, bj->dealer, bj->dealerCount)) bj->dscoreAlpha += 0.05f;
        if (bj->dscoreAlpha > 1.0f) bj->dscoreAlpha = 1.0f;
        if (!message && bj->dscoreAlpha == 1.0f && hand_in_position(bj, bj->dealer, bj->dealerCount)) {
            dealers_turn(bj);
        }
        break;
    case SSR_BJ_SHOW_RESULT:
        // NextGame: a new shuffle, and the table is clear.
        if (!message) {
            reset(bj, random);
            set_buttons(bj, 0);
            event(bj, SSR_BJ_NEXT, -1, 0, NULL);
        }
        break;
    }
    update_all(bj);
}
