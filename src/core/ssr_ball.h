// ssr_ball: the ball of a match moment (c_TBall). The units are the pixels
// of the 640 x 960 screen. The goal is at the top. One update is one tick of
// 60 a second, as in the game. The arithmetic is in 32-bit floats, as in
// the game, so that a test can compare it with the original.

#ifndef SSR_BALL_H
#define SSR_BALL_H

#include "ssr_random.h"

typedef enum {
    SSR_BALL_FLYING,
    SSR_BALL_GOAL,
    // Over the bar, wide, or stopped short.
    SSR_BALL_MISSED,
    SSR_BALL_STOPPED,
} SSRBallOutcome;

// The goal: the x of its posts, the line, and the back of the net.
typedef struct {
    float left, right;
    float line;
    float back;
    float crossbar;
    float postSize;
    // The height of the roof of the net at the front and at the back.
    float roofFront, roofBack;
} SSRGoal;

typedef struct {
    float x, y, z;
    float oldX, oldY, oldZ;
    // The speed on the ground in pixels a tick, and the heading in degrees:
    // 270 is up the screen, to the goal.
    float power, direction;
    float zVelocity, dip, curl;
    float windForce;
    // The factors of the ball type, 1 for the NS-Pro ball.
    float speedFactor, bounceFactor, weightFactor, curlFactor;
    // 0 normal, 1 rain, 2 snow.
    int weather;
    float windSpeed, windDirection;
    int alive;
    int inGoal;
    int postHit;
    int overBar;
    int hitOutsideSideNet;
    int bounces;
    int kicks;
    // 1 keeps a slow ball alive while the players still run (c_TBall.m_Update).
    int holdStop;
    SSRBallOutcome outcome;
} SSRBall;

// The goal of pitch side 0, 1, or 2 in the modern look.
SSRGoal ssr_goal(int side);

// A ball at rest at (x, y), with the factors of the NS-Pro ball.
void ssr_ball_place(SSRBall *ball, float x, float y);

// c_TBall.Kick: the heading in degrees, the power of the shot (10 to 100),
// the dip and the curl, the height, and 1 for a header.
void ssr_ball_kick(SSRBall *ball, float direction, float power, float dip, float curl, float height, int header);

// One tick of the ball in the air or on the grass, then the posts, the
// bar, the nets, and the goal line. A post or a net takes random numbers.
void ssr_ball_update(SSRBall *ball, const SSRGoal *goal, SSRRandom *random);

// The aim of a shot (c_TBall.m_Update when kicking == 1): the player drags
// from the ball to (dragX, dragY). The shot goes the other way. The power
// is from 10 to 0.4 * Power + 60.
void ssr_ball_aim(const SSRBall *ball, float dragX, float dragY, int powerSkill, float *direction, float *power);

// The tap on the big ball (c_TBall2): tapX and tapY are from the centre of
// the tap ball. Gives the arguments of the kick.
void ssr_ball_tap(float aimDirection, float aimPower, float tapX, float tapY, int technique, float depth,
                  float tapBallZ, float *direction, float *power, float *dip, float *curl, float *height,
                  int *header);

#endif
