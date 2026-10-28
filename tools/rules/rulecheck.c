// rulecheck: prints the results of rules of the remake, one line for each
// case, in the form of RulesProbe.java. scripts/rules-check.sh compares
// them with the rules of the original game.
//
// A float prints as the hex of its bits, so that the compare is exact.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ssr_career.h"
#include "ssr_casino.h"
#include "ssr_horses.h"
#include "ssr_random.h"

static const char *bits(float value, char out[9]) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    snprintf(out, 9, "%08x", raw);
    return out;
}

static void random_numbers(void) {
    static const int32_t SEEDS[5] = {1234, 1, 99991, -5, 2147483647};
    static const int NS[5] = {1, 2, 6, 100, 1000};
    static const int RANGES[4][2] = {{0, 1}, {-3, 3}, {4, 25}, {10, 10}};
    char hex[9];
    for (int k = 0; k < 5; k++) {
        int32_t s = SEEDS[k];
        SSRRandom r = {s};
        for (int i = 0; i < 10; i++) printf("rnd %d %d = %s\n", s, i, bits(ssr_rnd(&r), hex));
        for (int i = 0; i < 5; i++) printf("rand %d %d = %d\n", s, NS[i], ssr_rand(&r, NS[i]));
        for (int i = 0; i < 4; i++) {
            printf("rand2 %d %d %d = %d\n", s, RANGES[i][0], RANGES[i][1], ssr_rand2(&r, RANGES[i][0], RANGES[i][1]));
        }
        printf("rnd2 %d = %s\n", s, bits(ssr_rnd2(&r, -1.5f, 2.5f), hex));
        printf("rnd3 %d = %s\n", s, bits(ssr_rnd3(&r, 7.0f), hex));
        printf("randfloat %d = %s\n", s, bits(ssr_rand_float(&r, 0.5f, 2.0f), hex));
        printf("seed %d = %d\n", s, r.seed);
    }
}

static void roulette(void) {
    for (int n = 0; n <= 37; n++) printf("roulette %d = %d\n", n, ssr_roulette_colour(n));
}

static void horses(void) {
    for (int s = 0; s <= 6; s++) {
        printf("horseprice %d = %d\n", s, ssr_horses_price(s));
        printf("horsetrain %d = %d\n", s, ssr_horses_train_cost(s));
    }
    for (int p = 0; p <= 5; p++) printf("horseprize %d = %d\n", p, ssr_horses_prize(p));
}

static void star_level(void) {
    static const int RATINGS[8] = {0, 1, 99, 100, 250, 999, 1000, 4500};
    for (int i = 0; i < 8; i++) {
        SSRCareer c;
        memset(&c, 0, sizeof(c));
        c.starRating = RATINGS[i];
        printf("starlevel %d = %d\n", RATINGS[i], ssr_career_star_level(&c));
    }
}

int main(void) {
    random_numbers();
    roulette();
    horses();
    star_level();
    return 0;
}
