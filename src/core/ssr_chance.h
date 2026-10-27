// ssr_chance: one match moment of the player, from the set-up to the end
// (c_TMatch.m_InitChance and m_UpdateMatch, c_TBall.m_Update and
// m_CheckDefenders). The player drags back from the ball to aim, taps the
// big ball to kick, and the ball flies past the keeper and the defenders.
// One update is one tick of 60 a second.

#ifndef SSR_CHANCE_H
#define SSR_CHANCE_H

#include "ssr_ball.h"
#include "ssr_players.h"
#include "ssr_tapball.h"

typedef enum {
    // The player drags from the ball to aim.
    SSR_CHANCE_AIM,
    // The player taps the big ball.
    SSR_CHANCE_TAP,
    SSR_CHANCE_FLIGHT,
    // A dribble chance: swipes push the ball past the defenders.
    SSR_CHANCE_DRIBBLE,
    SSR_CHANCE_OVER,
} SSRChanceState;

typedef enum {
    SSR_RESULT_NONE,
    SSR_RESULT_GOAL,
    // A team-mate scored after the pass of the player.
    SSR_RESULT_ASSIST,
    // A pass of a midfield chance came to a team-mate.
    SSR_RESULT_PASS,
    SSR_RESULT_SAVED,
    SSR_RESULT_MISSED,
    SSR_RESULT_OFFSIDE,
    // A defender took the ball.
    SSR_RESULT_INTERCEPTED,
    // A defender took the ball while the player aimed.
    SSR_RESULT_LOST,
    // The ball of a dribble crossed the finish line.
    SSR_RESULT_DRIBBLED,
} SSRChanceResult;

// The facts of the player, the teams, and the match for one chance.
typedef struct {
    // c_TMatch.m_chancetype (see SSRChanceInfo).
    int type;
    int minute;
    // The strengths of the team of the player and of the other team.
    float teamStrength, opponentStrength;
    // The skills of the player, from 1 to 100.
    int power, technique, vision, pace;
    int level, starLevel, clubId;
    int careerApps, chances;
    float careerGoals, careerPasses;
    // 1 when the boots push the tap ball.
    int controlBoots;
    // 1 when the player is the captain in this match (SSRChanceInfo).
    int captain;
    float windSpeed, windDirection;
    // 0 normal, 1 rain, 2 snow.
    int weather;
} SSRChanceSetup;

// GetMakeRunMaxDist: (int)(Player.FakeStat * 150 / 10 + 150), with the
// FakeStat 3 of 4.32.
#define SSR_MAKE_RUN_MAX 195

typedef struct {
    SSRChanceSetup setup;
    SSRChanceInfo info;
    SSRChanceState state;
    SSRChanceResult result;
    SSRBall ball;
    SSRGoal goal;
    SSRTapBall tap;
    SSRPlayers players;
    // The fields of c_TBall that the players read.
    int kicking, lastTouchedBy, kickTime, deflects, actionType;
    float bounceX, bounceY;
    // The aim: the touch-down, and the heading and the power of the shot.
    float touchX, touchY;
    int touchTime, touchWasDown;
    float aimDirection, aimPower;
    // The players that were offside at the last kick, by id.
    unsigned char offside[SSR_PLAYERS_MAX + 1];
    // The length of the kick of the player in yards.
    float kickYards;
    // The shots of the player that the keeper or a post stopped
    // (c_TBall.CheckShotsOnTarget). A goal does not count.
    int shotsOnTarget;
    int now;
    // 1 for each warning beep of the tap ball, and for the kick sound.
    int beep, kicked;
    // The dribble: 1 after the first touch, the swipe (c_SwipeDetector),
    // and the speed of the ball.
    int dribbling, swiping;
    float swipeX, swipeY;
    float dribbleX, dribbleY;
} SSRChance;

// The start of the ball for a chance type (c_TBall.m_Reset).
void ssr_chance_ball_start(int type, int pitchSide, int minute, SSRRandom *random, float *x, float *y, float *z);

// Sets up the chance: the side of the pitch, the ball, the tap ball, and
// the players, with the random numbers in the order of the game.
void ssr_chance_start(SSRChance *chance, const SSRChanceSetup *setup, SSRRandom *random);

// Sets up the ball of a training drill (c_TMatch.m_SetUpTraining and
// c_TBall.m_Reset(5)): training 2, 3, 4, or 6 waits for the aim at (x, y),
// and training 8 dribbles from there. The drill adds the keeper and its
// objects.
void ssr_chance_start_training(SSRChance *chance, const SSRChanceSetup *setup, int training, float x, float y,
                               SSRRandom *random);

// One tick at the time now in milliseconds, with the touch of the player.
void ssr_chance_update(SSRChance *chance, int now, const SSRTapTouch *touch, SSRRandom *random);

// The view of the ball that the players read.
SSRPlayerBall ssr_chance_player_ball(const SSRChance *chance);

#endif
