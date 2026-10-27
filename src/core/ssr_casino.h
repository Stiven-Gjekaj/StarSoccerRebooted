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

#endif
