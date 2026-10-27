#include "ssr_casino.h"

static const int STAKES[SSR_CASINO_STAKE_LEVELS] = {1, 2, 5, 7, 10, 15, 25, 50};

int ssr_casino_stake(int level) {
    return level >= 1 && level <= SSR_CASINO_STAKE_LEVELS ? STAKES[level - 1] : 1;
}

int ssr_casino_check_stake(int level, int bank) {
    if (bank < 2) return 1;
    while (level > 1 && ssr_casino_stake(level) > bank) level--;
    return level < 1 ? 1 : level;
}

void ssr_slots_create(SSRSlotReel *reel, int index, SSRRandom *random) {
    int r = ssr_rand(random, 6);
    reel->yPos1 = (float)(r * SSR_SLOTS_CELL);
    reel->yPos2 = reel->yPos1 - SSR_SLOTS_LENGTH;
    reel->yVel = 0;
    reel->stopped = 1;
    reel->index = index;
    reel->spinTime = reel->spinLength = 0;
    reel->fruit = (int)(reel->yPos1 / SSR_SLOTS_CELL);
}

void ssr_slots_spin(SSRSlotReel *reel, int n, int now, SSRRandom *random) {
    reel->spinTime = now;
    reel->yVel = ssr_rnd2(random, 8.0f, 10.0f);
    reel->spinLength = ssr_rand(random, 200) + 1000 + 500 * (n - 1);
    reel->stopped = 0;
}

int ssr_slots_update(SSRSlotReel *reel, int now) {
    int stops = 0;
    if (reel->yVel > 0) {
        reel->yPos1 += reel->yVel;
        reel->yPos2 += reel->yVel;
    }
    if (!reel->stopped && now > reel->spinTime + reel->spinLength) {
        int p = (int)reel->yPos1;
        if (p < 0) p += SSR_SLOTS_LENGTH;
        if (p > SSR_SLOTS_LENGTH) p -= SSR_SLOTS_LENGTH;
        for (int k = 0; k < SSR_SLOTS_SYMBOLS; k++) {
            if (p < k * SSR_SLOTS_CELL || p > k * SSR_SLOTS_CELL + SSR_SLOTS_CELL / 2) continue;
            reel->yPos1 = (float)(k * SSR_SLOTS_CELL);
            reel->yPos2 = reel->yPos1 - SSR_SLOTS_LENGTH;
            reel->fruit = k;
            reel->stopped = 1;
            reel->yVel = 0;
            stops = 1;
            break;
        }
    }
    // The two copies of the strip follow each other.
    if (reel->yPos1 > SSR_SLOTS_LENGTH) reel->yPos1 = reel->yPos2 - SSR_SLOTS_LENGTH;
    if (reel->yPos2 > SSR_SLOTS_LENGTH) reel->yPos2 = reel->yPos1 - SSR_SLOTS_LENGTH;
    return stops;
}

int ssr_slots_prize(const SSRSlotReel reels[3], int bet) {
    int a = reels[0].fruit, b = reels[1].fruit, c = reels[2].fruit;
    float s = (float)bet;
    if (a == b && b == c) return (int)(s * 7.0f);
    if (a == b || a == c || b == c) return (int)(s * 3.0f);
    return 0;
}

int ssr_slots_frame(const SSRSlotReel *reel) {
    const float length = 96.0f;
    return (int)((length * reel->yPos1) / (float)SSR_SLOTS_LENGTH) + (int)length;
}
