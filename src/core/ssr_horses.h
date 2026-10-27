// ssr_horses: the race horses (c_THorse, c_TRacing). The names come from
// "MyData/Horse.txt" of the import. The screens are in ssr_game.

#ifndef SSR_HORSES_H
#define SSR_HORSES_H

#include <stddef.h>

#include "ssr_random.h"

#define SSR_HORSES_MAX 256
#define SSR_RUNNERS 6
// The stable holds 50 horses, and the tweaks have 50 slots.
#define SSR_STABLE_SLOTS 50

typedef struct {
    int id;
    char name[48];
    int owned, forsale;
    float energy;
    // 1 to 5 stars.
    int strength;
    // The coat: "RRGGBBFF00000000".
    char colour[17];
    // The race: the bet on the horse, the final place (0 not yet), the draw
    // number, the lane (1 to 6), the distance, the speed, the live place,
    // the place of the last tick, the odds, and the stride counter.
    int betamount, raceposition, randno, racenum;
    float x, xvel;
    int currentPosition, previousPosition, odds;
    float frame;
} SSRHorse;

typedef struct {
    SSRHorse horses[SSR_HORSES_MAX];
    int count;
    // The order of the list (THorse.glist). Some loops follow it.
    int list[SSR_HORSES_MAX];
    // The runners of the race, in id order.
    int runners[SSR_RUNNERS];
    int runnerCount;
    int sortby;
} SSRHorses;

// THorse.LoadData: the horses of the names, one for each line after the
// first, with the fixed seed 4321. Returns the number of horses.
int ssr_horses_load(SSRHorses *horses, const char *text);

// ResetHorsesForSale: each horse that is not owned is for sale with 1
// chance in 15, from the seed year + length of the name + nation.
void ssr_horses_reset_for_sale(SSRHorses *horses, int seed);

// GetHorsesOwned: "id:energy:strength:colour#" for each owned horse.
void ssr_horses_owned_text(const SSRHorses *horses, char *out, size_t capacity);
// LoadHorsesOwned.
void ssr_horses_load_owned(SSRHorses *horses, const char *text);

// The horse with the id, or NULL.
SSRHorse *ssr_horses_by_id(SSRHorses *horses, int id);
int ssr_horses_count_owned(const SSRHorses *horses);

// GetPrice2, GetCostToTrain, and GetOwnedHorsePrize.
int ssr_horses_price(int strength);
int ssr_horses_train_cost(int strength);
int ssr_horses_prize(int place);

// SelectRunners: own is the index of the own horse, or -1. Five or six
// other horses with energy over 50 race, and the odds go from 2 to 12.
void ssr_horses_select_runners(SSRHorses *horses, int own, SSRRandom *random);

// OrderRunners: the y of each runner card. The favourite is at the top,
// the long shot second.
void ssr_horses_card_order(const SSRHorses *horses, int y[SSR_RUNNERS]);

// THorse.Update for each runner, one tick: moves[n] gets the distance of
// runner n. Returns 1 when a live place changed.
int ssr_horses_race_step(SSRHorses *horses, SSRRandom *random, float moves[SSR_RUNNERS]);

// The live places by distance. With freeze, the places become final.
void ssr_horses_race_places(SSRHorses *horses, int freeze);

// MatchRecovery: each owned horse gets 25 energy and up to 10 more.
void ssr_horses_recover(SSRHorses *horses, SSRRandom *random);

#endif
