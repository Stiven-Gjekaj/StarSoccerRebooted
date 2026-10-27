#include "ssr_horses.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_sort.h"

// THorse.Compare9: each sort ends with the id, so the order is total.
static int compare(void *context, int a, int b) {
    const SSRHorses *h = context;
    const SSRHorse *x = &h->horses[a], *y = &h->horses[b];
    switch (h->sortby) {
    case 5:
        if (y->randno != x->randno) return y->randno < x->randno ? 1 : -1;
        break;
    case 7:
        if (y->x > x->x) return 1;
        if (y->x < x->x) return -1;
        break;
    case 29: {
        int py = y->raceposition ? y->raceposition : 99, px = x->raceposition ? x->raceposition : 99;
        if (py != px) return py < px ? 1 : -1;
        if (y->x > x->x) return 1;
        if (y->x < x->x) return -1;
        break;
    }
    case 11: {
        if (y->strength != x->strength) return y->strength < x->strength ? 1 : -1;
        if (y->energy < x->energy) return 1;
        if (y->energy > x->energy) return -1;
        int c = strcmp(y->name, x->name);
        if (c != 0) return c < 0 ? 1 : -1;
        break;
    }
    default:
        break;
    }
    return x->id > y->id ? 1 : x->id < y->id ? -1 : 0;
}

static void sort_list(SSRHorses *h, int *items, int count, int sortby, int descending) {
    h->sortby = sortby;
    ssr_sort(items, count, compare, h, descending);
}

// GetRandomHorseColour: light grey, dark grey, or brown.
static void random_colour(SSRRandom *random, char out[17]) {
    int k = ssr_rand(random, 11), r, g, b;
    if (k <= 3) {
        r = g = b = ssr_rand2(random, 177, 221);
    } else if (k <= 5) {
        r = g = b = ssr_rand2(random, 21, 88);
    } else {
        r = ssr_rand2(random, 80, 135);
        g = ssr_rand2(random, 45, 75);
        b = ssr_rand2(random, 35, 68);
    }
    snprintf(out, 17, "%02X%02X%02XFF00000000", r, g, b);
}

int ssr_horses_load(SSRHorses *h, const char *text) {
    memset(h, 0, sizeof(*h));
    SSRRandom random = {4321};
    const char *line = text ? strchr(text, '\n') : NULL;
    // The first line is not a horse.
    while (line && h->count < SSR_HORSES_MAX) {
        line++;
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        char name[48];
        snprintf(name, sizeof(name), "%.*s", (int)length, line);
        if (strstr(name, "//")) break;
        // [I: the game keeps the "\r" of a line in the name; the remake
        // takes it away in each place.]
        size_t n = strlen(name);
        if (n > 0 && name[n - 1] == '\r') name[n - 1] = 0;
        SSRHorse *horse = &h->horses[h->count];
        // The constructor takes the stride counter first.
        horse->frame = ssr_rnd3(&random, 7.0f);
        horse->id = h->count + 1;
        snprintf(horse->name, sizeof(horse->name), "%s", name);
        horse->energy = 100;
        horse->racenum = -1;
        horse->previousPosition = -1;
        horse->strength = ssr_rand(&random, 5);
        // The classic sprite set.
        if (ssr_rand2(&random, 0, 3) == 3) ssr_rand2(&random, 0, 3);
        random_colour(&random, horse->colour);
        h->list[h->count] = h->count;
        h->count++;
        line = end;
    }
    return h->count;
}

void ssr_horses_reset_for_sale(SSRHorses *h, int seed) {
    SSRRandom random = {seed};
    for (int k = 0; k < h->count; k++) {
        SSRHorse *horse = &h->horses[h->list[k]];
        horse->forsale = 0;
        if (!horse->owned && ssr_rand(&random, 15) == 1) horse->forsale = 1;
    }
    sort_list(h, h->list, h->count, 1, 0);
}

void ssr_horses_owned_text(const SSRHorses *h, char *out, size_t capacity) {
    size_t used = 0;
    if (capacity) out[0] = 0;
    for (int k = 0; k < h->count; k++) {
        const SSRHorse *horse = &h->horses[h->list[k]];
        if (!horse->owned) continue;
        int n = snprintf(out + used, capacity - used, "%d:%g:%d:%s#", horse->id, horse->energy, horse->strength,
                         horse->colour);
        if (n < 0 || (size_t)n >= capacity - used) break;
        used += (size_t)n;
    }
}

void ssr_horses_load_owned(SSRHorses *h, const char *text) {
    for (const char *p = text; p && *p;) {
        const char *end = strchr(p, '#');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        char part[96];
        snprintf(part, sizeof(part), "%.*s", (int)length, p);
        int id = 0, strength = 0;
        float energy = 0;
        char colour[17] = "";
        int fields = sscanf(part, "%d:%f:%d:%16s", &id, &energy, &strength, colour);
        if (fields < 2) return;
        SSRHorse *horse = ssr_horses_by_id(h, id);
        if (horse) {
            horse->owned = 1;
            horse->energy = energy;
            if (fields >= 3) horse->strength = strength;
            if (fields >= 4) snprintf(horse->colour, sizeof(horse->colour), "%s", colour);
        }
        p = end ? end + 1 : NULL;
    }
}

void ssr_horses_sort_by_id(SSRHorses *h) {
    sort_list(h, h->list, h->count, 1, 0);
}

SSRHorse *ssr_horses_by_id(SSRHorses *h, int id) {
    return id >= 1 && id <= h->count ? &h->horses[id - 1] : NULL;
}

int ssr_horses_count_owned(const SSRHorses *h) {
    int n = 0;
    for (int i = 0; i < h->count; i++) n += h->horses[i].owned != 0;
    return n;
}

int ssr_horses_price(int strength) {
    static const int PRICES[5] = {100, 250, 500, 1000, 2500};
    return strength >= 1 && strength <= 5 ? PRICES[strength - 1] : 0;
}

int ssr_horses_train_cost(int strength) {
    static const int COSTS[4] = {150, 250, 500, 1500};
    return strength >= 1 && strength <= 4 ? COSTS[strength - 1] : 0;
}

int ssr_horses_prize(int place) {
    return place == 1 ? 100 : place == 2 ? 50 : place == 3 ? 25 : 0;
}

void ssr_horses_select_runners(SSRHorses *h, int own, SSRRandom *random) {
    for (int i = 0; i < h->count; i++) h->horses[i].betamount = h->horses[i].raceposition = 0;
    for (int k = 0; k < h->count; k++) h->horses[h->list[k]].randno = ssr_rand(random, 1000);
    sort_list(h, h->list, h->count, 5, 0);
    h->runnerCount = 0;
    if (own >= 0) h->runners[h->runnerCount++] = own;
    for (int k = 0; k < h->count; k++) {
        const SSRHorse *horse = &h->horses[h->list[k]];
        if (h->runnerCount < SSR_RUNNERS && !horse->owned && horse->energy > 50.0f) {
            h->runners[h->runnerCount++] = h->list[k];
        }
    }
    for (int k = 0; k < h->count; k++) {
        SSRHorse *horse = &h->horses[h->list[k]];
        if (!horse->owned) horse->energy = ssr_rnd2(random, 50.0f, 100.0f);
    }
    sort_list(h, h->runners, h->runnerCount, 1, 0);
    for (int i = 0; i < h->runnerCount; i++) {
        SSRHorse *r = &h->horses[h->runners[i]];
        r->racenum = i + 1;
        r->x = ssr_rnd2(random, -20.0f, 50.0f);
        r->xvel = 0;
        r->currentPosition = 0;
        r->previousPosition = -1;
    }
    // SetOdds: the best horse has odds 2, and a step down in strength adds
    // 1 more.
    sort_list(h, h->runners, h->runnerCount, 11, 1);
    int previous = 10, k = 1;
    for (int i = 0; i < h->runnerCount; i++) {
        SSRHorse *r = &h->horses[h->runners[i]];
        if (r->strength < previous) k++;
        r->odds = k++;
        previous = r->strength;
    }
    sort_list(h, h->runners, h->runnerCount, 1, 0);
}

void ssr_horses_card_order(const SSRHorses *h, int y[SSR_RUNNERS]) {
    int n = h->runnerCount, hi = 0, lo = 0;
    for (int i = 0; i < n; i++) {
        y[i] = 160 * i + 8;
        if (h->horses[h->runners[i]].odds > h->horses[h->runners[hi]].odds) hi = i;
    }
    if (n < 2) return;
    int t;
#define SWAP(a, b)                                                                                          \
    if ((a) != (b)) {                                                                                       \
        t = y[a];                                                                                           \
        y[a] = y[b];                                                                                        \
        y[b] = t;                                                                                           \
    }
    SWAP(hi, 1);
    for (int i = 0; i < n; i++) {
        if (h->horses[h->runners[i]].odds < h->horses[h->runners[lo]].odds) lo = i;
    }
    if (hi == 0) {
        SWAP(lo, 1);
    } else {
        SWAP(lo, 0);
    }
#undef SWAP
}

// THorse.Update: each stride the horse can push, or slow down.
static int horse_step(SSRHorse *r, SSRRandom *random, float *move) {
    r->frame = r->frame + 0.2f;
    if (r->frame > 7.0f) {
        r->frame = 0;
        r->energy = r->energy - ssr_rnd2(random, 2.0f, 3.0f);
        if (r->x < 0 || (ssr_rnd3(random, 2.0f) > 1.0f && ssr_rnd3(random, 120.0f) < r->energy)) {
            if (ssr_rnd3(random, 7.0f) <= (float)r->strength) {
                r->xvel = r->xvel + 0.45f;
                r->energy = r->energy - ssr_rnd2(random, 2.0f, 3.0f);
            }
        } else if (ssr_rand(random, r->strength + 5) < 3 || r->x > 340.0f) {
            r->xvel = r->xvel - 0.3f;
        }
        r->xvel = r->xvel < -0.3f ? -0.3f : r->xvel > 1.25f ? 1.25f : r->xvel;
        r->energy = r->energy < 0 ? 0 : r->energy > 100.0f ? 100.0f : r->energy;
    }
    *move = r->xvel;
    r->x = r->x + *move;
    return r->raceposition == 0 && r->currentPosition != r->previousPosition;
}

int ssr_horses_race_step(SSRHorses *h, SSRRandom *random, float moves[SSR_RUNNERS]) {
    int any = 0;
    for (int i = 0; i < h->runnerCount; i++) any |= horse_step(&h->horses[h->runners[i]], random, &moves[i]);
    return any;
}

void ssr_horses_race_places(SSRHorses *h, int freeze) {
    sort_list(h, h->runners, h->runnerCount, 7, 0);
    for (int k = 0; k < h->runnerCount; k++) {
        SSRHorse *r = &h->horses[h->runners[k]];
        r->previousPosition = r->currentPosition;
        r->currentPosition = k + 1;
    }
    sort_list(h, h->runners, h->runnerCount, 1, 0);
    if (!freeze) return;
    for (int i = 0; i < h->runnerCount; i++) {
        SSRHorse *r = &h->horses[h->runners[i]];
        if (r->raceposition == 0) r->raceposition = r->currentPosition;
    }
}

void ssr_horses_recover(SSRHorses *h, SSRRandom *random) {
    for (int k = 0; k < h->count; k++) {
        SSRHorse *horse = &h->horses[h->list[k]];
        if (!horse->owned) continue;
        int e = (int)((horse->energy + 25.0f) + ssr_rnd3(random, 10.0f));
        horse->energy = (float)(e < 0 ? 0 : e > 100 ? 100 : e);
    }
}
