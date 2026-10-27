// ssr_random: the random numbers of Monkey X, as the game uses them. The
// arithmetic is in 32-bit floats, as in the game, so that the same seed
// gives the same numbers.

#ifndef SSR_RANDOM_H
#define SSR_RANDOM_H

#include <stdint.h>

typedef struct {
    int32_t seed;
} SSRRandom;

// The game starts its seed at 1234.
#define SSR_RANDOM_START 1234

// Rnd(): a float from 0 to under 1.
float ssr_rnd(SSRRandom *r);

// Rnd2(a, b): a float from a to under b.
float ssr_rnd2(SSRRandom *r, float a, float b);

// Rnd3(x): a float from 0 to under x.
float ssr_rnd3(SSRRandom *r, float x);

// Rand(n): an integer from 1 to n.
int ssr_rand(SSRRandom *r, int n);

// Rand2(a, b): an integer from a to b. Both ends are possible.
int ssr_rand2(SSRRandom *r, int a, int b);

// RandFloat(a, b): a float from a to b.
float ssr_rand_float(SSRRandom *r, float a, float b);

#endif
