#include "ssr_casino.h"

#include <math.h>

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

static const int POCKETS[SSR_ROULETTE_POCKETS] = {37, 1,  13, 36, 24, 3,  15, 34, 22, 5,  17, 32, 20,
                                                  7,  11, 30, 26, 9,  28, 0,  2,  14, 35, 23, 4,  16,
                                                  33, 21, 6,  18, 31, 19, 8,  12, 29, 25, 10, 27};
static const float POCKET_ANGLE = 9.473f;

void ssr_roulette_setup(SSRRoulette *r, float wheelWidth) {
    if (!r->made) {
        // The first wheel and ball (the classic sizes until the setup).
        r->rot = r->speed = 0;
        r->wheelSize = 150;
        r->rimSize = 270;
        r->x = r->y = r->ballSpeed = r->lineRot = r->pocket = r->vel = r->dist = 0;
        r->stopped = r->dropped = 0;
        r->result = 1;
        r->ballNum = 0;
        r->made = 1;
    }
    r->cx = 322.0f;
    r->cy = 468.0f;
    r->wheelSize = (int)(wheelWidth * 0.33f);
    r->rimSize = (int)(wheelWidth * 0.42f);
}

void ssr_roulette_spin(SSRRoulette *r, SSRRandom *random) {
    if (r->result == 0) return;
    r->result = 0;
    r->speed = ssr_rnd2(random, 0.1f, 1.1f) + 3.0f;
    r->lineRot = (float)ssr_rand(random, 360);
    r->ballSpeed = -4.0f;
    r->dist = (float)r->rimSize * 0.49f;
    r->vel = -0.05f;
    r->dropped = 0;
    r->stopped = 0;
}

// TRouletteWheel.Update: the wheel slows down.
static void wheel_update(SSRRoulette *r) {
    float f = r->rot;
    r->speed -= 0.015f;
    if (r->speed <= 0) r->speed = 0;
    r->rot = f - r->speed;
    if (r->rot < 0) r->rot += 360;
}

// TRouletteBall.Update5: the ball runs round the rim, falls in, bounces,
// and then stays in a pocket.
static void ball_update(SSRRoulette *r, SSRRandom *random) {
    r->lineRot += r->ballSpeed;
    if (r->lineRot > 360) r->lineRot -= 360;
    if (r->lineRot < 1) r->lineRot += 360;
    r->pocket = (r->lineRot + r->rot) / POCKET_ANGLE;
    r->pocket -= 38.0f;
    if (r->pocket < 0) r->pocket += 38.0f;
    if (r->speed < 2.0f) {
        r->vel = r->stopped ? -0.25f : r->vel - 0.125f;
        r->dist += r->vel;
        if (r->dist <= (float)r->wheelSize * 0.5f) {
            r->vel = -(r->vel * 0.7f);
            if (r->vel <= -0.5f || r->vel >= 0.5f) {
                r->ballSpeed = ssr_rnd2(random, -0.5f - r->speed * 1.5f, r->speed + 0.5f);
            } else {
                r->dropped = 1;
                r->ballSpeed = r->speed;
                float p = r->pocket;
                int moved = 0;
                if (p > (float)(int)p + 0.62f) {
                    r->lineRot -= 0.3f;
                    moved = 1;
                }
                if (p < (float)(int)p + 0.58f) {
                    r->lineRot += 0.3f;
                    moved = 1;
                }
                if (!moved) r->stopped = 1;
            }
            if (r->stopped) {
                if (r->dist < (float)r->wheelSize * 0.42f) r->dist = (float)r->wheelSize * 0.42f;
            } else {
                r->dist = (float)r->wheelSize * 0.5f;
            }
        }
    }
    const float d2r = 0.017453292f;
    r->x = r->cx + (float)cos((double)(r->lineRot * d2r)) * (r->dist * 2.0f);
    r->y = r->cy + (float)sin((double)(r->lineRot * d2r)) * (r->dist * 2.0f);
}

int ssr_roulette_update(SSRRoulette *r, SSRRandom *random) {
    wheel_update(r);
    ball_update(r, random);
    // The game tests the float speed against the double 0.2.
    if (r->result != 0 || !r->stopped || (double)r->speed >= 0.2) return 0;
    r->result = 1;
    // [I: a pocket of 38 would stop the original; the remake takes 0.]
    int index = (int)r->pocket;
    r->ballNum = POCKETS[index >= 0 && index < SSR_ROULETTE_POCKETS ? index : 0];
    r->speed = 0;
    return 1;
}

int ssr_roulette_colour(int n) {
    static const int RED[18] = {1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
    if (n == 0 || n == 37) return -1;
    for (int i = 0; i < 18; i++) {
        if (RED[i] == n) return 1;
    }
    return 0;
}

int ssr_roulette_prize(int n, SSRRouletteBet bet, int stake) {
    int number = n >= 1 && n <= 36;
    switch (bet) {
    case SSR_BET_GREEN: return n == 0 || n == 37 ? stake * 18 : 0;
    case SSR_BET_RED: return number && ssr_roulette_colour(n) == 1 ? stake * 2 : 0;
    case SSR_BET_BLACK: return number && ssr_roulette_colour(n) == 0 ? stake * 2 : 0;
    case SSR_BET_1TO12: return n >= 1 && n <= 12 ? stake * 3 : 0;
    case SSR_BET_13TO24: return n >= 13 && n <= 24 ? stake * 3 : 0;
    case SSR_BET_25TO36: return n >= 25 && n <= 36 ? stake * 3 : 0;
    }
    return 0;
}
