// ssr_training: a training drill of the player (c_TMatch.m_SetUpTraining
// and m_UpdateTraining): the ball, the training objects (poles, cones,
// dummies, and lines), the lives, and the end of the drill. The five
// trials of a new career are drills 2, 3, 4, 8, and 6 at level 1 with 3
// lives. One update is one tick of 60 a second.

#ifndef SSR_TRAINING_H
#define SSR_TRAINING_H

#include "ssr_chance.h"

typedef enum {
    SSR_OBJECT_CONE = 1,
    SSR_OBJECT_DUMMY = 2,
    SSR_OBJECT_POLE = 3,
    SSR_OBJECT_LINE = 4,
} SSRTrainingObjectType;

// c_TTrainingObject and its kinds.
typedef struct {
    SSRTrainingObjectType type;
    float x, y;
    // The other end of a line.
    int x2, y2;
    int alive;
    // A cone: 1 stands, 0 fell. A dummy: 1 after a hit.
    int frame;
    // "FFFF00" a pole, "FF0000" a hit pole, "0000FF" a gate line, "00FF00"
    // the finish line.
    char colour[8];
    int finishLine, tall;
} SSRTrainingObject;

#define SSR_TRAINING_OBJECTS 16

// The facts of the player that a drill reads.
typedef struct {
    int power, technique, pace;
    int starLevel;
    int controlBoots;
} SSRTrainingPlayer;

typedef struct {
    SSRChance chance;
    SSRTrainingPlayer player;
    // m_training, m_level, m_lives, and m_trial (1 to 5, or 0).
    int training, level, lives, trial;
    // -1 while the drill runs, 0 after a lost life, 1 after a success.
    int success;
    SSRTrainingObject objects[SSR_TRAINING_OBJECTS];
    int objectCount;
    // The language tag of the instruction. The drill waits for a touch
    // while it shows.
    const char *instruction;
    // The ball fades after it dies (c_TBall.m_alph, from 1.5).
    float ballAlpha;
    // The quick message: its language tag, the banner code, and its end.
    const char *message;
    int outcome, messageEnd;
    int startTime;
    // 1 after the drill ends with a success, -1 with a failure.
    int over;
    float windSpeed, windDirection;
} SSRTraining;

// SetUpTraining(type, level, lives, trial). Type 5 is the dribble drill 8
// in 4.32.
void ssr_training_start(SSRTraining *training, int type, int level, int lives, int trial,
                        const SSRTrainingPlayer *player, SSRRandom *random);

// One tick at the time now in milliseconds.
void ssr_training_update(SSRTraining *training, int now, const SSRTapTouch *touch, SSRRandom *random);

// The number of lines that are still there (c_TTrainingLine.m_Count).
int ssr_training_lines(const SSRTraining *training);

// The skill that a drill trains: 0 technique, 1 set pieces, 2 power,
// 3 pace, 4 vision, or -1.
int ssr_training_skill(int type);

#endif
