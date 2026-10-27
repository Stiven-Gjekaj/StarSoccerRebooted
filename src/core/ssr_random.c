#include "ssr_random.h"

// The generator of Monkey X: seed = seed * 1664525 + 1013904223, with
// 32-bit wrap. The value is the bits 8 to 31 of the seed, as a part of 1.
float ssr_rnd(SSRRandom *r) {
    r->seed = (int32_t)((uint32_t)r->seed * 1664525u + 1013904223u);
    return (float)((r->seed >> 8) & 0xFFFFFF) / 16777216.0f;
}

// Rnd(a, b) of Monkey: a + Rnd() * (b - a).
static float rnd_range(SSRRandom *r, float a, float b) {
    float value = ssr_rnd(r) * (b - a);
    return value + a;
}

float ssr_rnd2(SSRRandom *r, float a, float b) {
    return rnd_range(r, a, b);
}

float ssr_rnd3(SSRRandom *r, float x) {
    return ssr_rnd(r) * x;
}

int ssr_rand(SSRRandom *r, int n) {
    return (int)rnd_range(r, 1.0f, (float)n + 1);
}

int ssr_rand2(SSRRandom *r, int a, int b) {
    return (int)rnd_range(r, (float)a, (float)b + 1);
}

float ssr_rand_float(SSRRandom *r, float a, float b) {
    float part = (float)ssr_rand(r, 10000000) / 1.0e7f;
    return a + part * (b - a);
}
