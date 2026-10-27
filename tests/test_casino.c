// Tests ssr_casino: the stakes and the slot reels.

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

int main(void) {
    test_gives_the_stakes();
    test_spins_the_reels();
    test_pays_the_prizes();
    test_gives_the_frame_of_a_reel();
    return TEST_RESULT();
}
