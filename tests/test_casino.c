// Tests ssr_casino: the stakes, the slot reels, and roulette.

#include <math.h>

#include "ssr_casino.h"
#include "ssr_test.h"

static void test_gives_the_stakes(void) {
    const int stakes[8] = {1, 2, 5, 7, 10, 15, 25, 50};
    for (int level = 1; level <= 8; level++) CHECK(ssr_casino_stake(level) == stakes[level - 1]);
    CHECK(ssr_casino_stake(0) == 1 && ssr_casino_stake(9) == 1);
    // The level goes down until the bank pays the stake.
    CHECK(ssr_casino_check_stake(8, 1000) == 8);
    CHECK(ssr_casino_check_stake(8, 24) == 6);
    CHECK(ssr_casino_check_stake(5, 7) == 4);
    CHECK(ssr_casino_check_stake(3, 1) == 1);
    CHECK(ssr_casino_check_stake(2, 0) == 1);
}

// Spins three reels with updates 16 ms apart, from the time 0.
static void spin(SSRSlotReel reels[3], SSRRandom *random, int stops[3]) {
    for (int i = 0; i < 3; i++) ssr_slots_spin(&reels[i], i + 1, 0, random);
    for (int now = 16; now < 5000 && !(reels[0].stopped && reels[1].stopped && reels[2].stopped); now += 16) {
        for (int i = 0; i < 3; i++) {
            if (ssr_slots_update(&reels[i], now)) stops[i] = now;
        }
    }
}

static void test_spins_the_reels(void) {
    SSRRandom random = {12345};
    SSRSlotReel reels[3];
    for (int i = 0; i < 3; i++) ssr_slots_create(&reels[i], i, &random);
    CHECK(reels[0].fruit == 1 && reels[1].fruit == 1 && reels[2].fruit == 4);
    CHECK(reels[2].yPos1 == 344 && reels[2].yPos2 == 344 - 688 && reels[2].stopped);
    int stops[3] = {0};
    spin(reels, &random, stops);
    CHECK(reels[0].spinLength == 1183 && reels[1].spinLength == 1600 && reels[2].spinLength == 2120);
    CHECK(stops[0] == 1200 && stops[1] == 1680 && stops[2] == 2128);
    CHECK(reels[0].fruit == 1 && reels[1].fruit == 3 && reels[2].fruit == 2);
    // A stopped reel sits on its symbol.
    CHECK(reels[1].yPos1 == 3 * 86 && reels[1].yVel == 0);
    CHECK(ssr_slots_prize(reels, 5) == 0);

    // Another seed gives two of the same.
    random.seed = -7;
    for (int i = 0; i < 3; i++) ssr_slots_create(&reels[i], i, &random);
    spin(reels, &random, stops);
    CHECK(reels[0].fruit == 1 && reels[1].fruit == 6 && reels[2].fruit == 6);
    CHECK(ssr_slots_prize(reels, 5) == 15);
}

static void test_pays_the_prizes(void) {
    SSRSlotReel reels[3] = {{0}};
    reels[0].fruit = reels[1].fruit = reels[2].fruit = 7;
    CHECK(ssr_slots_prize(reels, 50) == 350);
    reels[1].fruit = 2;
    CHECK(ssr_slots_prize(reels, 50) == 150);
    reels[0].fruit = 2;
    CHECK(ssr_slots_prize(reels, 1) == 3);
    reels[2].fruit = 0;
    reels[1].fruit = 1;
    CHECK(ssr_slots_prize(reels, 25) == 0);
}

static void test_gives_the_frame_of_a_reel(void) {
    SSRSlotReel reel = {0};
    reel.yPos1 = 86;
    CHECK(ssr_slots_frame(&reel) == 12 + 96);
    reel.yPos1 = 0;
    CHECK(ssr_slots_frame(&reel) == 96);
    // The cast goes to zero, also for a place above the strip.
    reel.yPos1 = -10;
    CHECK(ssr_slots_frame(&reel) == -1 + 96);
}

// A spin from the seed on a new wheel. Returns the number of updates.
static int roulette_spin(SSRRoulette *roulette, int seed) {
    SSRRandom random = {seed};
    roulette->made = 0;
    ssr_roulette_setup(roulette, 545);
    ssr_roulette_spin(roulette, &random);
    int updates = 1;
    while (!ssr_roulette_update(roulette, &random) && updates < 1000) updates++;
    return updates;
}

static void test_spins_the_roulette_wheel(void) {
    SSRRoulette roulette = {0};
    ssr_roulette_setup(&roulette, 545);
    CHECK(roulette.wheelSize == 179 && roulette.rimSize == 228 && roulette.result == 1);
    // The spin ends 3 to 4 seconds later, on the pocket of the ball.
    CHECK(roulette_spin(&roulette, 12345) == 195 && roulette.ballNum == 22);
    CHECK(roulette.speed == 0 && roulette.stopped && roulette.result == 1);
    CHECK(roulette_spin(&roulette, 1000000) == 246 && roulette.ballNum == 8);
    CHECK(roulette_spin(&roulette, -7) == 209 && roulette.ballNum == 32);
    // The ball sits in the pockets: between 0.42 and 0.5 of the wheel size
    // from the centre, two pixels for each unit.
    float dx = roulette.x - roulette.cx, dy = roulette.y - roulette.cy, d = sqrtf(dx * dx + dy * dy) / 2;
    CHECK(d >= 179 * 0.42f - 0.01f && d <= 179 * 0.5f + 0.01f);
    // A second spin waits until the first ends.
    SSRRandom random = {1};
    ssr_roulette_spin(&roulette, &random);
    float speed = roulette.speed;
    ssr_roulette_spin(&roulette, &random);
    CHECK(roulette.speed == speed && roulette.result == 0);
}

static void test_pays_the_roulette_bets(void) {
    CHECK(ssr_roulette_colour(0) == -1 && ssr_roulette_colour(37) == -1);
    CHECK(ssr_roulette_colour(1) == 1 && ssr_roulette_colour(2) == 0 && ssr_roulette_colour(36) == 1);
    CHECK(ssr_roulette_prize(37, SSR_BET_GREEN, 2) == 36 && ssr_roulette_prize(0, SSR_BET_GREEN, 1) == 18);
    CHECK(ssr_roulette_prize(19, SSR_BET_RED, 5) == 10 && ssr_roulette_prize(19, SSR_BET_BLACK, 5) == 0);
    CHECK(ssr_roulette_prize(22, SSR_BET_BLACK, 5) == 10 && ssr_roulette_prize(0, SSR_BET_BLACK, 5) == 0);
    CHECK(ssr_roulette_prize(12, SSR_BET_1TO12, 7) == 21 && ssr_roulette_prize(13, SSR_BET_1TO12, 7) == 0);
    CHECK(ssr_roulette_prize(24, SSR_BET_13TO24, 1) == 3 && ssr_roulette_prize(25, SSR_BET_25TO36, 1) == 3);
    CHECK(ssr_roulette_prize(37, SSR_BET_25TO36, 1) == 0);
}

int main(void) {
    test_spins_the_roulette_wheel();
    test_pays_the_roulette_bets();
    test_gives_the_stakes();
    test_spins_the_reels();
    test_pays_the_prizes();
    test_gives_the_frame_of_a_reel();
    return TEST_RESULT();
}
