// ssr_tapball: the big ball that the player taps to shoot (c_TBall2). The
// ball moves across the bottom of the screen at (x, 810 - z). A tap near
// it starts the kick. The kick fires some ticks later, and the tap point
// gives the dip and the curl. One update is one tick of 60 a second.

#ifndef SSR_TAPBALL_H
#define SSR_TAPBALL_H

#include "ssr_random.h"

typedef enum {
    SSR_TAPBALL_DEAD,
    SSR_TAPBALL_ROLLING,
    SSR_TAPBALL_BOUNCING,
    SSR_TAPBALL_HEADED,
} SSRTapBallType;

typedef enum {
    SSR_TAPBALL_NONE,
    // The player tapped the ball. The kick comes later.
    SSR_TAPBALL_TAPPED,
    // The kick fires now: ssr_tapball_kick gives its arguments.
    SSR_TAPBALL_KICK,
    // The ball went off the screen or the time ran out: a dud kick.
    SSR_TAPBALL_DUD,
} SSRTapBallEvent;

// The facts of the chance that choose the ball and its time.
typedef struct {
    // 0 in a match. The training drills have their own numbers.
    int training;
    // c_TMatch.m_chancetype and m_chancestage.
    int chanceType, chanceStage;
    // 1 when the ball comes to the head.
    int header;
    // The career matches of the player, and the chances of this match.
    int careerApps, chances;
    float careerGoals, careerPasses;
    // The level of the player, and the strength of the other team (0 when
    // there is no other team).
    int level;
    float opponentStrength;
} SSRTapSetup;

typedef struct {
    float x, y, z;
    float xVelocity, zVelocity, rollVelocity, rotation;
    float depth, depthAccel;
    // The tap: its point, and the time of the kick that it starts.
    float tapX, tapY;
    float powVelocity, powRotation, powScale;
    int bounces;
    // Control boots: the start of the drag, and the time of the last push.
    float controlX, controlY;
    int controlTime, controlCount;
    int touching;
    int beeps;
    // The milliseconds at the first tick, and the time limit.
    int timer, limit;
    // The weight and the bounce of the ball type.
    float weight, bounce;
    SSRTapBallType type;
    int done;
} SSRTapBall;

typedef struct {
    float x, y;
    int down;
    // 1 on the first tick of a touch.
    int hit;
} SSRTapTouch;

// The speed of the ball for the level or the other team
// (c_TBall2.GetBallXVel).
float ssr_tapball_speed(int level, float opponentStrength);

// The time to tap in milliseconds (c_TBall2.m_Reset).
int ssr_tapball_limit(int training, int level, float opponentStrength);

// c_TBall2.m_Reset: chooses the ball and places it. It takes random numbers.
void ssr_tapball_reset(SSRTapBall *ball, const SSRTapSetup *setup, SSRRandom *random);

// One tick at the time now in milliseconds. controlBoots is 1 when the
// player has the boots that push a moving ball. beep gets 1 when a warning
// sound plays.
SSRTapBallEvent ssr_tapball_update(SSRTapBall *ball, int now, const SSRTapTouch *touch, int controlBoots, int *beep);

// The time that is left, from 1 to 0, while the ball moves.
float ssr_tapball_time_left(const SSRTapBall *ball, int now);

// The kick of a tap: the aim of the shot, the technique of the player,
// and the tap. Gives the arguments of ssr_ball_kick.
void ssr_tapball_kick(const SSRTapBall *ball, float aimDirection, float aimPower, int technique, float *direction,
                      float *power, float *dip, float *curl, float *height, int *header);

#endif
