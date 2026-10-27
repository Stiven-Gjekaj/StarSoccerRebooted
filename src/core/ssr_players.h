// ssr_players: the other players of a match moment (c_TMatchPlayer): the
// keeper, the defenders of the other team, and the team-mates of the
// player. The set-up of a chance places them with the random numbers of the
// game. One update is one tick of 60 a second.

#ifndef SSR_PLAYERS_H
#define SSR_PLAYERS_H

#include "ssr_random.h"

#define SSR_PLAYERS_MAX 32
// The y of the goal line of the match (c_TMatch.m_goalline).
#define SSR_GOAL_LINE 128

typedef enum {
    SSR_PLAYER_KEEPER,
    SSR_PLAYER_DEFENDER,
    SSR_PLAYER_ATTACKER,
} SSRPlayerType;

typedef enum {
    SSR_ANIM_STAND,
    SSR_ANIM_RUN,
    SSR_ANIM_RUN_GK,
    SSR_ANIM_JUMP_GK,
    SSR_ANIM_JUMP_WITH_BALL_GK,
    SSR_ANIM_STAND_WITH_BALL_GK,
    SSR_ANIM_DIVE_GK,
    SSR_ANIM_DIVE_LOW_GK,
} SSRPlayerAnim;

typedef struct {
    int id;
    SSRPlayerType type;
    float x, y, z;
    float xVelocity, yVelocity, zVelocity;
    // The heading in degrees: 0 is to the right, 90 is down the screen.
    float direction;
    float strength;
    int inWall;
    // 0 right, 1 down, 2 left, 3 up.
    int facing;
    SSRPlayerAnim anim;
    int frame;
    int lastFrameTime;
    // 1 or 2: the shirt image of the classic look.
    int image;
    // A saw-tooth from 1.5 to -1.5 for the call bubble of a team-mate.
    float calling;
    // A run that the player draws with a finger.
    int canMakeRun;
    // 1 for the defender nearest the ball in a dribble chance.
    int chasing;
    float makeRunDirection, makeRunDistance;
    int makeRunX, makeRunY;
} SSRMatchPlayer;

// The facts of the chance that the players read.
typedef struct {
    // c_TMatch.m_chancetype: 1 midfield, 2 attack, 3 free kick, 4 penalty,
    // 7 corner, 8 wing, 9 and 10 headers.
    int chanceType, chanceStage;
    // 0, 1, or 2: the goal is on the right, in the middle, or on the left.
    int pitchSide;
    // The minute of the match. Its parity chooses the side of a corner.
    int minute;
    // 0 in a match.
    int training;
    // The defenders chase the ball when the star level is more than 1, or
    // when the player has no club.
    int starLevel, clubId;
    // The start of the ball.
    float startX, startY;
    // 1 when the chance came from a corner: no offside.
    int wasCorner;
} SSRChanceInfo;

// What the players see of the ball (c_TBall).
typedef struct {
    float x, y, z;
    float power, kickDirection;
    int alive;
    // 0 waits, 1 the player aims, 2 in the air, 3 dribble.
    int kicking;
    int lastTouchedBy;
    int kickTime;
    int actionType;
    // Where the ball comes down.
    float bounceX, bounceY;
} SSRPlayerBall;

typedef struct {
    SSRMatchPlayer list[SSR_PLAYERS_MAX];
    int count;
    // The ids go up in the order that the players are made.
    int lastId;
    // The distance of the offside line from the goal line.
    int offsideLine;
    // The id of the player that the finger draws a run for, or 0.
    int makeRun;
} SSRPlayers;

// The x of the centre of the goal for the pitch side.
float ssr_goal_centre(int pitchSide);

void ssr_players_clear(SSRPlayers *players);

// c_TMatchPlayer.CreateKeeper, CreateDefender, and CreateAttacker. They take
// random numbers. strength is the strength of the team.
SSRMatchPlayer *ssr_players_add_keeper(SSRPlayers *players, const SSRChanceInfo *chance, float strength,
                                       SSRRandom *random);
SSRMatchPlayer *ssr_players_add_defender(SSRPlayers *players, const SSRChanceInfo *chance, float strength, int inWall,
                                         int high, int box, SSRRandom *random);
SSRMatchPlayer *ssr_players_add_attacker(SSRPlayers *players, const SSRChanceInfo *chance, float strength, int near,
                                         SSRRandom *random);

// Puts the players of the wall between the ball and the goal.
void ssr_players_place_wall(SSRPlayers *players, const SSRChanceInfo *chance, float ballX, float ballY);

// Removes the players that are off the screen. The ids stay.
void ssr_players_remove_off_screen(SSRPlayers *players, float left, float right);

// The speed of the player for a tick (p_Speed).
float ssr_player_speed(const SSRMatchPlayer *player, const SSRPlayerBall *ball);

// The offside line from the defenders and the ball, and the check of one
// team-mate when the ball is kicked.
int ssr_players_offside_line(const SSRPlayers *players, const SSRChanceInfo *chance, const SSRPlayerBall *ball);
int ssr_player_offside(const SSRMatchPlayer *player, const SSRChanceInfo *chance, int offsideLine);

// One tick of all the players (UpdateAll). now is in milliseconds.
void ssr_players_update(SSRPlayers *players, const SSRChanceInfo *chance, const SSRPlayerBall *ball, int now,
                        SSRRandom *random);

// The frame of the image of the player, from 0 to 17.
int ssr_player_frame(const SSRMatchPlayer *player);

// 1 while the keeper is in a dive, or holds the ball.
int ssr_player_diving(const SSRMatchPlayer *player);
int ssr_player_holding(const SSRMatchPlayer *player);

#endif
