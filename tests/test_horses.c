// Tests ssr_horses: the horses of the fixed seed, the market, the runners
// and their odds, and a race.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssr_horses.h"
#include "ssr_test.h"

// A file of a header and 210 names, as Horse.txt has.
static void names(char *text, size_t capacity) {
    size_t used = (size_t)snprintf(text, capacity, "Header\r\n");
    for (int i = 1; i <= 210; i++) {
        used += (size_t)snprintf(text + used, capacity - used, i < 210 ? "Horse %03d\r\n" : "Horse %03d", i);
    }
}

static SSRHorses horses;

static void test_makes_the_horses(void) {
    char text[4096];
    names(text, sizeof(text));
    CHECK(ssr_horses_load(&horses, text) == 210);
    CHECK(strcmp(horses.horses[0].name, "Horse 001") == 0 && strcmp(horses.horses[209].name, "Horse 210") == 0);
    // The seed 4321 gives the same horses in each career.
    const int strength[8] = {2, 4, 3, 3, 1, 3, 2, 2};
    const char *colour[8] = {"5A3B40FF00000000", "734237FF00000000", "454545FF00000000", "2B2B2BFF00000000",
                             "292929FF00000000", "802E28FF00000000", "79412DFF00000000", "583B3FFF00000000"};
    const float frame[8] = {6.374774f, 2.358884f, 3.585503f, 3.675776f, 4.728551f, 5.906647f, 4.663440f, 3.404149f};
    for (int i = 0; i < 8; i++) {
        const SSRHorse *h = &horses.horses[i];
        CHECK(h->id == i + 1 && h->strength == strength[i] && strcmp(h->colour, colour[i]) == 0);
        CHECK(fabsf(h->frame - frame[i]) < 1e-5f && h->energy == 100 && !h->owned);
    }
    // A line with "//" ends the list.
    CHECK(ssr_horses_load(&horses, "Header\nOne\nTwo\n// end\nThree") == 2);
    CHECK(ssr_horses_load(&horses, text) == 210);
}

static void test_keeps_the_stable(void) {
    CHECK(ssr_horses_price(1) == 100 && ssr_horses_price(5) == 2500 && ssr_horses_price(0) == 0);
    CHECK(ssr_horses_train_cost(1) == 150 && ssr_horses_train_cost(4) == 1500 && ssr_horses_train_cost(5) == 0);
    CHECK(ssr_horses_prize(1) == 100 && ssr_horses_prize(3) == 25 && ssr_horses_prize(4) == 0);
    SSRHorse *mine = ssr_horses_by_id(&horses, 3);
    mine->owned = 1;
    mine->energy = 61.5f;
    mine->strength = 4;
    // The market: about one horse in 15, never an owned one.
    ssr_horses_reset_for_sale(&horses, 2 + 12 + 62);
    int forSale = 0;
    for (int i = 0; i < horses.count; i++) forSale += horses.horses[i].forsale;
    CHECK(forSale > 3 && forSale < 30 && !mine->forsale);
    // The same seed gives the same market.
    int first = -1;
    for (int i = 0; i < horses.count && first < 0; i++) {
        if (horses.horses[i].forsale) first = i;
    }
    ssr_horses_reset_for_sale(&horses, 2 + 12 + 62);
    CHECK(horses.horses[first].forsale);
    char text[256];
    ssr_horses_owned_text(&horses, text, sizeof(text));
    CHECK(strcmp(text, "3:61.5:4:454545FF00000000#") == 0);
    SSRHorses copy = horses;
    copy.horses[2].owned = 0;
    copy.horses[2].strength = 1;
    ssr_horses_load_owned(&copy, text);
    CHECK(copy.horses[2].owned && copy.horses[2].strength == 4 && copy.horses[2].energy == 61.5f);
    CHECK(ssr_horses_count_owned(&copy) == 1);
    // Recovery: 25 and up to 10 more, to 100 at most.
    SSRRandom random = {1};
    ssr_horses_recover(&copy, &random);
    CHECK(copy.horses[2].energy >= 86 && copy.horses[2].energy <= 96 && copy.horses[3].energy == 100);
    ssr_horses_recover(&copy, &random);
    CHECK(copy.horses[2].energy == 100);
}

static void test_draws_the_runners(void) {
    SSRRandom random = {5555};
    SSRHorse *mine = ssr_horses_by_id(&horses, 3);
    ssr_horses_select_runners(&horses, 2, &random);
    CHECK(horses.runnerCount == 6);
    int hasMine = 0, odds[6];
    for (int i = 0; i < 6; i++) {
        const SSRHorse *r = &horses.horses[horses.runners[i]];
        hasMine |= r == mine;
        CHECK(r->racenum == i + 1 && r->x >= -20 && r->x < 50 && r->betamount == 0);
        if (i > 0) CHECK(r->id > horses.horses[horses.runners[i - 1]].id);
        if (r != mine) CHECK(!r->owned);
        odds[i] = r->odds;
    }
    CHECK(hasMine);
    // The odds: all different, 2 for the best, a gap for each step down.
    for (int i = 0; i < 6; i++) {
        const SSRHorse *a = &horses.horses[horses.runners[i]];
        CHECK(odds[i] >= 2 && odds[i] <= 12);
        for (int j = 0; j < 6; j++) {
            const SSRHorse *b = &horses.horses[horses.runners[j]];
            if (i != j) CHECK(odds[i] != odds[j]);
            if (a->strength > b->strength) CHECK(odds[i] + 1 < odds[j]);
        }
    }
    int y[6];
    ssr_horses_card_order(&horses, y);
    for (int i = 0; i < 6; i++) {
        const SSRHorse *r = &horses.horses[horses.runners[i]];
        if (r->odds == 2) CHECK(y[i] == 8);
        int top = 1;
        for (int j = 0; j < 6; j++) top &= odds[j] <= r->odds;
        if (top) CHECK(y[i] == 168);
    }
}

static void test_runs_a_race(void) {
    SSRRandom random = {777};
    float frame = 0, moves[6];
    int ticks = 0, frozen = 0;
    while (frame < 1350) {
        ssr_horses_race_step(&horses, &random, moves);
        int freeze = frame >= 1200 && !frozen;
        ssr_horses_race_places(&horses, freeze);
        frozen |= freeze;
        frame += 3.15f;
        ticks++;
    }
    CHECK(ticks == 429 && frozen);
    // The draw left the list in the order of the draw numbers.
    CHECK(horses.list[0] != 0 || horses.list[1] != 1);
    ssr_horses_sort_by_id(&horses);
    for (int i = 0; i < horses.count; i++) CHECK(horses.list[i] == i);
    int seen[7] = {0};
    for (int i = 0; i < 6; i++) {
        const SSRHorse *r = &horses.horses[horses.runners[i]];
        CHECK(r->raceposition >= 1 && r->raceposition <= 6 && r->energy >= 0 && r->energy <= 100);
        CHECK(r->xvel >= -0.3f && r->xvel <= 1.25f);
        seen[r->raceposition]++;
    }
    for (int p = 1; p <= 6; p++) CHECK(seen[p] == 1);
}

int main(void) {
    test_makes_the_horses();
    test_keeps_the_stable();
    test_draws_the_runners();
    test_runs_a_race();
    return TEST_RESULT();
}
