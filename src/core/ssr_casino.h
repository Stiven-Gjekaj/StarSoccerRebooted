// ssr_casino: the rules of the casino games (c_TScreen_Casino,
// c_TScreen_Slots, c_TSlotStrip). The screens are in ssr_game.

#ifndef SSR_CASINO_H
#define SSR_CASINO_H

#include "ssr_random.h"

// The stake levels go from 1 to 8.
#define SSR_CASINO_STAKE_LEVELS 8

// GSGetStakeAmount: the stake of a level in bux. Another level gives 1.
int ssr_casino_stake(int level);

// CheckStakeAmount: the highest level up to the level that the bank pays.
// A bank under 2 gives level 1.
int ssr_casino_check_stake(int level, int bank);

// The places of a slot reel: 8 symbols of 86 units each.
#define SSR_SLOTS_SYMBOLS 8
#define SSR_SLOTS_CELL 86
#define SSR_SLOTS_LENGTH 688

// A reel of the slot machine (c_TSlotStrip).
typedef struct {
    float yVel, yPos1, yPos2;
    // 0, 1, or 2.
    int index;
    // 1 when the reel does not turn.
    int stopped;
    // The time of the spin and its length, in ms.
    int spinTime, spinLength;
    // The symbol on the payline, 0 to 7.
    int fruit;
} SSRSlotReel;

// TSlotStrip.Create: the reel starts on a symbol from 1 to 6 (Rand(6)).
void ssr_slots_create(SSRSlotReel *reel, int index, SSRRandom *random);

// TSlotStrip.Spin: the reel turns at 8 to 10 units each update. Reel n (1
// to 3) stops after 1000, 1500, or 2000 ms and Rand(200) ms more.
void ssr_slots_spin(SSRSlotReel *reel, int n, int now, SSRRandom *random);

// TSlotStrip.Update: one update at the time now. The reel stops on a
// symbol when its time is over and the strip is in the top half of a
// symbol. Returns 1 when the reel stops in this update.
int ssr_slots_update(SSRSlotReel *reel, int now);

// DoPrize: three of the same pay 7 times the bet, two of the same pay 3
// times the bet.
int ssr_slots_prize(const SSRSlotReel reels[3], int bet);

// AScreen_Slots.SetByOldUIPos: the frame of the reel animation. Icon i
// shows frame 12 * i + 8 + the frame, in an animation of 96 frames.
int ssr_slots_frame(const SSRSlotReel *reel);

// Roulette: an American wheel of 38 pockets. 37 is "00".
#define SSR_ROULETTE_POCKETS 38

// The bets of roulette (TScreen_Roulette.betcol).
typedef enum {
    SSR_BET_GREEN = 1,
    SSR_BET_RED,
    SSR_BET_BLACK,
    SSR_BET_1TO12,
    SSR_BET_13TO24,
    SSR_BET_25TO36,
} SSRRouletteBet;

// The wheel and the ball (c_TRouletteWheel, c_TRouletteBall). They keep
// their places between visits.
typedef struct {
    // The wheel: its angle, its speed, its centre, and its sizes.
    float rot, speed, cx, cy;
    int wheelSize, rimSize;
    // The ball: its place, its speed round the wheel, its angle, its
    // pocket (a float), its speed to the centre, and its distance.
    float x, y, ballSpeed, lineRot, pocket, vel, dist;
    int stopped;
    // 1 after the ball first drops into a pocket in a spin.
    int dropped;
    // The sounds of the last update: the ball hit the pockets, and it
    // dropped into one (RouletteBallHit and RouletteBallDrop).
    int hit, drop;
    // 0 while a spin runs. The number of the last spin.
    int result, ballNum;
    int made;
} SSRRoulette;

// TRoulette.SetUp: makes the wheel one time, then gives it the sizes of
// the wheel image of width wheelWidth.
void ssr_roulette_setup(SSRRoulette *roulette, float wheelWidth);

// TRoulette.Spin: the wheel gets a speed from 3.1 to 4.1, and the ball
// starts at an angle from 1 to 360. Does nothing while a spin runs.
void ssr_roulette_spin(SSRRoulette *roulette, SSRRandom *random);

// TRoulette.Update: one update. Each bounce of the ball takes a number.
// Returns 1 when the spin ends in this update (GetResult).
int ssr_roulette_update(SSRRoulette *roulette, SSRRandom *random);

// TRouletteWheel.GetColour: 1 red, 0 black, -1 green.
int ssr_roulette_colour(int number);

// GetResult: what the bet pays on the number, the stake included.
int ssr_roulette_prize(int number, SSRRouletteBet bet, int stake);

#endif
