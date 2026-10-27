#include "ssr_players.h"

#include <math.h>
#include <string.h>

#define D2R 0.017453292f
#define R2D 57.29578f
// The time of one frame of a player animation in milliseconds.
#define FRAME_LENGTH 150

// The frames of each animation. -1 ends an animation that does not loop.
static const int STAND[] = {0};
static const int RUN[] = {1, 2, 3, 4, 5, 6};
static const int RUN_GK[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static const int JUMP_GK[] = {10, 11, 12, 12, -1};
static const int JUMP_WITH_BALL_GK[] = {13};
static const int STAND_WITH_BALL_GK[] = {14};
static const int DIVE_GK[] = {15, 16, 16, 17, 17, 17, 17, 17, 17, -1};
static const int DIVE_LOW_GK[] = {15, 17, 17, 17, 17, 17, 17, 17, -1};

static const struct {
    const int *frames;
    int length;
} ANIMS[] = {
    {STAND, 1},
    {RUN, 6},
    {RUN_GK, 9},
    {JUMP_GK, 5},
    {JUMP_WITH_BALL_GK, 1},
    {STAND_WITH_BALL_GK, 1},
    {DIVE_GK, 10},
    {DIVE_LOW_GK, 9},
};

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float distance(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return (float)sqrt((double)(dx * dx + dy * dy));
}

// GetDirection: the heading from a to b in degrees.
static float direction_to(float ax, float ay, float bx, float by) {
    return (float)(atan2((double)(by - ay), (double)(bx - ax)) * (double)R2D);
}

static float cos_deg(float a) {
    return (float)cos((double)(a * D2R));
}

static float sin_deg(float a) {
    return (float)sin((double)(a * D2R));
}

// The difference of two headings, from 0 to 180.
static float angle_difference(float a, float b) {
    float d = fmodf(fabsf(a - b), 360);
    return d > 180 ? 360 - d : d;
}

float ssr_goal_centre(int pitchSide) {
    return pitchSide == 0 ? 535.0f : pitchSide == 2 ? 105.0f : 320.0f;
}

void ssr_players_clear(SSRPlayers *players) {
    memset(players, 0, sizeof(*players));
}

int ssr_player_frame(const SSRMatchPlayer *p) {
    int f = ANIMS[p->anim].frames[p->frame < ANIMS[p->anim].length ? p->frame : 0];
    return f < 0 ? 0 : f;
}

int ssr_player_diving(const SSRMatchPlayer *p) {
    return p->type == SSR_PLAYER_KEEPER && ssr_player_frame(p) > 9;
}

int ssr_player_holding(const SSRMatchPlayer *p) {
    int f = ssr_player_frame(p);
    return f == 13 || f == 14;
}

static SSRMatchPlayer *new_player(SSRPlayers *players, SSRPlayerType type, float strength) {
    if (players->count >= SSR_PLAYERS_MAX) return NULL;
    SSRMatchPlayer *p = &players->list[players->count++];
    memset(p, 0, sizeof(*p));
    p->id = ++players->lastId;
    p->type = type;
    // SetPlayerStrength.
    p->strength = clampf(0.5f * strength, 1, 50);
    p->anim = SSR_ANIM_STAND;
    p->facing = 1;
    return p;
}

static int count_type(const SSRPlayers *players, SSRPlayerType type) {
    int n = 0;
    for (int i = 0; i < players->count; i++) n += players->list[i].type == type;
    return n;
}

static void update_facing(SSRMatchPlayer *p, const SSRPlayerBall *ball);

/* ----------------------------------------------------------- keeper */

// p_GetDesiredPosition: 45 pixels from the centre of the goal, toward the
// ball.
static void keeper_desired(const SSRChanceInfo *chance, float bx, float by, float *x, float *y) {
    float g = ssr_goal_centre(chance->pitchSide);
    float a = (float)(atan2((double)(by - SSR_GOAL_LINE), (double)(bx - g)) * (double)R2D);
    *x = clampf(g + cos_deg(a) * 45, g - 90, g + 90);
    *y = clampf(SSR_GOAL_LINE + sin_deg(a) * 45, SSR_GOAL_LINE, SSR_GOAL_LINE + 180);
}

SSRMatchPlayer *ssr_players_add_keeper(SSRPlayers *players, const SSRChanceInfo *chance, float strength,
                                       SSRRandom *random) {
    SSRMatchPlayer *p = new_player(players, SSR_PLAYER_KEEPER, strength);
    if (!p) return NULL;
    p->x = ssr_goal_centre(chance->pitchSide);
    if (chance->chanceType == 4 || chance->chanceStage == 8) {
        p->x = 320;
        p->y = 134;
    } else if (chance->chanceType == 7 || chance->chanceType == 8) {
        p->x = chance->minute % 2 == 0 ? 520 : 120;
        p->y = 182;
    } else {
        // p_GetKeeperStartPosition.
        float desY;
        keeper_desired(chance, chance->startX, chance->startY, &p->x, &desY);
        float y = SSR_GOAL_LINE + 15;
        if (chance->training == 0 && chance->chanceType != 3 && chance->chanceType != 4) {
            if (ssr_rand(random, 8) == 1) y += (float)(15 * ssr_rand(random, 7));
        }
        int iy = (int)y;
        if (iy < SSR_GOAL_LINE + 15) iy = SSR_GOAL_LINE + 15;
        if (iy > SSR_GOAL_LINE + 105) iy = SSR_GOAL_LINE + 105;
        p->y = (float)iy;
    }
    p->direction = direction_to(p->x, p->y, chance->startX, chance->startY);
    return p;
}

/* ------------------------------------------------------ the others */

// 1 when the player is too near another player or the ball: the place is
// tried again with a wider spread. nearMate is the distance to a team-mate.
static int crowded(const SSRPlayers *players, const SSRMatchPlayer *self, float nearMate, float nearBall,
                   const SSRChanceInfo *chance, int *tries, float *spread) {
    int retry = 0;
    for (int i = 0; i < players->count; i++) {
        const SSRMatchPlayer *o = &players->list[i];
        if (o == self) continue;
        float limit = self->type == SSR_PLAYER_ATTACKER && o->type == SSR_PLAYER_ATTACKER ? nearMate : 75;
        if (distance(self->x, self->y, o->x, o->y) < limit) {
            (*tries)++;
            *spread += 10;
            retry = 1;
        }
    }
    if (distance(self->x, self->y, chance->startX, chance->startY) < nearBall) {
        (*tries)++;
        *spread += 10;
        retry = 1;
    }
    return retry;
}

SSRMatchPlayer *ssr_players_add_defender(SSRPlayers *players, const SSRChanceInfo *chance, float strength, int inWall,
                                         int high, int box, SSRRandom *random) {
    SSRMatchPlayer *p = new_player(players, SSR_PLAYER_DEFENDER, strength);
    if (!p) return NULL;
    p->image = ssr_rand(random, 2);
    p->inWall = inWall;
    int n = count_type(players, SSR_PLAYER_DEFENDER);
    int baseY = high ? 225 : SSR_GOAL_LINE + 225;
    static const float XS[] = {213.33333f, 426.66666f, -15, 655};
    float spread = 60;
    int tries = 0;
    int even = chance->minute % 2 == 0;
    for (;;) {
        int s = (int)spread;
        if (chance->chanceType == 7) {
            int x = ssr_rand2(random, 340, 500);
            p->x = (float)(even ? x : 640 - x);
            p->y = (float)ssr_rand2(random, 50, 430);
        } else if (chance->chanceType == 8) {
            int x = ssr_rand2(random, 240, 500);
            p->x = (float)(even ? x : 640 - x);
            p->y = (float)ssr_rand2(random, 50, 430);
            if (box) {
                p->x = (float)ssr_rand2(random, 260, 380);
                p->y = (float)ssr_rand2(random, 550, 750);
                if (distance(p->x, p->y, chance->startX, chance->startY) < 150) p->y = chance->startY - 187.5f;
            }
        } else {
            if (n >= 1 && n <= 8) {
                p->x = XS[(n - 1) % 4];
                p->y = (float)(n <= 4 ? baseY : baseY + 300);
            }
            if (chance->training == 7) {
                p->x = (float)(320 + ssr_rand2(random, -2 * s, 2 * s));
                p->y -= 45;
                if (n > 4) p->y -= 45;
                p->y += (float)ssr_rand2(random, -s, s);
            } else if (n < 5) {
                p->x += (float)ssr_rand2(random, -s, s);
                p->y += (float)ssr_rand2(random, -30, 30);
            } else {
                p->x += (float)ssr_rand2(random, -s, s);
                p->y += (float)ssr_rand2(random, -s, s);
            }
            if (chance->pitchSide == 0) p->x += 175;
            if (chance->pitchSide == 2) p->x -= 175;
            if (chance->chanceType == 4 && p->y < SSR_GOAL_LINE + 285) {
                p->y = (float)(SSR_GOAL_LINE + 15 * (ssr_rand(random, 3) + 18));
            }
        }
        float nearBall = chance->training == 7 ? 150 : 195;
        if (!crowded(players, p, 75, nearBall, chance, &tries, &spread) || tries > 500) break;
    }
    p->direction = direction_to(p->x, p->y, chance->startX, chance->startY);
    update_facing(p, NULL);
    return p;
}

SSRMatchPlayer *ssr_players_add_attacker(SSRPlayers *players, const SSRChanceInfo *chance, float strength, int near,
                                         SSRRandom *random) {
    SSRMatchPlayer *p = new_player(players, SSR_PLAYER_ATTACKER, strength);
    if (!p) return NULL;
    p->image = ssr_rand(random, 2);
    if (chance->chanceType != 4) p->calling = ssr_rand_float(random, -1.5f, 1.5f);
    int k = count_type(players, SSR_PLAYER_ATTACKER);
    int minDefY = 960;
    for (int i = 0; i < players->count; i++) {
        if (players->list[i].type == SSR_PLAYER_DEFENDER && (int)players->list[i].y < minDefY) {
            minDefY = (int)players->list[i].y;
        }
    }
    float spread = 30;
    int tries = 0;
    int even = chance->minute % 2 == 0;
    for (;;) {
        p->x = ssr_rand_float(random, 100, 540);
        p->y = (float)(SSR_GOAL_LINE + 15 * ssr_rand(random, 30));
        if (near && k == 2) p->y = chance->startY + (float)(15 * ssr_rand2(random, -5, 10));
        p->x += (float)ssr_rand2(random, (int)-spread, (int)spread);
        p->y += (float)(ssr_rand2(random, (int)(-spread / 2), (int)spread) / 2);
        if (p->y < (float)minDefY) p->y = (float)(minDefY + 15 * ssr_rand(random, 5));
        if (chance->chanceType == 7 || chance->chanceType == 8) {
            int x = ssr_rand2(random, 140, 450);
            p->x = (float)(even ? x : 640 - x);
            p->y = (float)ssr_rand2(random, 50, 480);
        }
        if (!crowded(players, p, 180, 225, chance, &tries, &spread) || tries > 500) break;
    }
    p->direction = direction_to(p->x, p->y, chance->startX, chance->startY);
    update_facing(p, NULL);
    return p;
}

void ssr_players_place_wall(SSRPlayers *players, const SSRChanceInfo *chance, float bx, float by) {
    float a = (float)(atan2((double)(by - SSR_GOAL_LINE), (double)(bx - 320)) * (double)R2D);
    // The new look puts 8 degrees between the players of the wall.
    static const float OFFSETS[] = {0, 8, -8, 16, -16};
    int k = 0;
    for (int i = 0; i < players->count && k < 5; i++) {
        SSRMatchPlayer *p = &players->list[i];
        if (!p->inWall) continue;
        p->x = bx - cos_deg(a + OFFSETS[k]) * 157.5f;
        p->y = by - sin_deg(a + OFFSETS[k]) * 157.5f;
        p->direction = direction_to(p->x, p->y, chance->startX, chance->startY);
        update_facing(p, NULL);
        k++;
    }
}

void ssr_players_remove_off_screen(SSRPlayers *players, float left, float right) {
    int kept = 0;
    for (int i = 0; i < players->count; i++) {
        SSRMatchPlayer *p = &players->list[i];
        if (p->x < left || p->x > right) continue;
        players->list[kept++] = *p;
    }
    players->count = kept;
}

float ssr_player_speed(const SSRMatchPlayer *p, const SSRPlayerBall *ball) {
    float s = clampf(p->strength * 0.06f, 1, 2);
    if (p->type == SSR_PLAYER_KEEPER) {
        s *= 0.7f;
    } else if (ball->kicking == 1) {
        s *= 0.35f;
    }
    return s;
}

/* ---------------------------------------------------------- offside */

static int offside_side(const SSRChanceInfo *chance) {
    if (chance->chanceType == 7 || chance->chanceType == 8) return chance->minute % 2 == 0 ? -1 : 1;
    return 0;
}

int ssr_players_offside_line(const SSRPlayers *players, const SSRChanceInfo *chance, const SSRPlayerBall *ball) {
    int px = (int)ball->x, py = (int)ball->y;
    int side = offside_side(chance);
    int first = chance->chanceType != 7;
    for (int i = 0; i < players->count; i++) {
        const SSRMatchPlayer *d = &players->list[i];
        if (d->type != SSR_PLAYER_DEFENDER) continue;
        int take = !first;
        if (side == 0 && d->y < (float)py) take = 1;
        if (side == -1 && d->x > (float)px) take = 1;
        if (side == 1 && d->x < (float)px) take = 1;
        if (take) {
            px = (int)d->x;
            py = (int)d->y;
            first = 1;
        }
    }
    if (side == -1) return 541 - px;
    if (side == 1) return px - 98;
    return py - 127;
}

int ssr_player_offside(const SSRMatchPlayer *p, const SSRChanceInfo *chance, int line) {
    if (chance->wasCorner) return 0;
    int type = chance->chanceType, side;
    if (type == 1 || type == 2 || type == 3 || type == 9 || type == 10) {
        side = 0;
    } else if (type == 8) {
        side = chance->minute % 2 == 0 ? -1 : 1;
    } else {
        return 0;
    }
    if (side == 0) return p->y < (float)(127 + line);
    if (side == -1) return p->x > (float)(541 - line);
    return p->x < (float)(98 + line);
}

/* ----------------------------------------------------------- update */

static void set_anim(SSRMatchPlayer *p, SSRPlayerAnim anim) {
    p->anim = anim;
}

static void update_facing(SSRMatchPlayer *p, const SSRPlayerBall *ball) {
    if (ssr_player_diving(p)) return;
    float a = direction_to(0, 0, p->xVelocity, p->yVelocity);
    float speed = (float)sqrt((double)(p->xVelocity * p->xVelocity + p->yVelocity * p->yVelocity));
    if (speed < 0.25f || p->inWall) {
        if (!ssr_player_holding(p)) p->anim = SSR_ANIM_STAND;
        p->frame = 0;
        a = ball ? direction_to(p->x, p->y, ball->x, ball->y) : p->direction;
    }
    while (a < 0) a += 360;
    while (a >= 360) a -= 360;
    if (a > 315 || a < 45) {
        p->facing = 0;
    } else if (a < 135) {
        p->facing = 1;
    } else if (a < 225) {
        p->facing = 2;
    } else {
        p->facing = 3;
    }
}

// p_InterceptBall: the heading to the ball, or to where the ball goes.
static int intercept(const SSRMatchPlayer *p, const SSRPlayerBall *ball, int chase, int keeperPredict) {
    float ballDistance = distance(p->x, p->y, ball->x, ball->y);
    float power = ball->power * (chase ? 10 : 1);
    float toBall = direction_to(p->x, p->y, ball->x, ball->y);
    if (!chase && (ball->power < 2 || ball->kicking == 1 || ballDistance > 160)) return (int)toBall;
    if (p->type == SSR_PLAYER_ATTACKER) {
        float bounceDistance = distance(p->x, p->y, ball->bounceX, ball->bounceY);
        if (bounceDistance < ballDistance || ball->z > 48) {
            return (int)direction_to(p->x, p->y, ball->bounceX, ball->bounceY);
        }
    }
    float bvx = cos_deg(ball->kickDirection) * power, bvy = sin_deg(ball->kickDirection) * power;
    float relative = distance(bvx, bvy, p->xVelocity, p->yVelocity);
    float t = relative > 0 ? ballDistance / relative * 0.95f : 0;
    float desX = ball->x + bvx * t, desY = ball->y + bvy * t;
    float toDes = direction_to(p->x, p->y, desX, desY);
    if (angle_difference(toDes, toBall) > 90) return (int)toBall;
    if (p->type == SSR_PLAYER_KEEPER && !keeperPredict) return (int)toBall;
    return (int)toDes;
}

// GetClosestOnMyTeam: the id of the player of the same type that is
// nearest the ball. The player that kicked in the last 500 ms does not
// count.
static int closest_on_team(const SSRPlayers *players, const SSRMatchPlayer *self, const SSRPlayerBall *ball, int now) {
    int best = 0;
    float bestDistance = 1e30f;
    for (int i = 0; i < players->count; i++) {
        const SSRMatchPlayer *o = &players->list[i];
        if (o->type != self->type) continue;
        if (ball->lastTouchedBy == o->id && now < ball->kickTime + 500) continue;
        float d = distance(o->x, o->y, ball->x, ball->y);
        if (d < bestDistance) {
            bestDistance = d;
            best = o->id;
        }
    }
    return best;
}

static void stop(SSRMatchPlayer *p) {
    p->xVelocity = p->yVelocity = 0;
}

// p_UpdateChanceOverBehaviour: the ball is dead. A keeper near a stopped
// ball holds it.
static void chance_over(SSRMatchPlayer *p, const SSRPlayerBall *ball) {
    if (!ssr_player_diving(p)) stop(p);
    if (!ssr_player_holding(p) && ball->power == 0 && distance(p->x, p->y, ball->x, ball->y) < 20) {
        set_anim(p, p->z > 0 ? SSR_ANIM_JUMP_WITH_BALL_GK : SSR_ANIM_STAND_WITH_BALL_GK);
        p->frame = 0;
    }
}

static void update_keeper(SSRMatchPlayer *p, const SSRChanceInfo *chance, const SSRPlayerBall *ball, float g,
                          SSRRandom *random) {
    p->yVelocity = 0;
    if (ssr_player_diving(p)) return;
    set_anim(p, fabsf(p->xVelocity) >= 0.5f ? SSR_ANIM_RUN_GK : SSR_ANIM_STAND);
    if (p->anim == SSR_ANIM_STAND) p->frame = 0;
    int holding = ssr_player_holding(p);
    if (chance->chanceType == 4 && !holding && ball->lastTouchedBy != p->id) {
        // A penalty: the side comes from the minute, the height from a
        // random number.
        if (chance->minute % 2 == 0) {
            p->facing = 2;
            p->xVelocity = -p->strength / 20;
        } else {
            p->facing = 0;
            p->xVelocity = p->strength / 20;
        }
        p->zVelocity = 2;
        set_anim(p, SSR_ANIM_DIVE_GK);
        p->frame = 0;
        if (ssr_rand(random, 2) == 1) {
            set_anim(p, SSR_ANIM_DIVE_LOW_GK);
            p->zVelocity = 1;
        }
    } else if (!holding && distance(p->x, p->y, ball->x, ball->y) < 80) {
        float dir = (float)intercept(p, ball, 0, 1);
        if (fabsf(p->x - ball->x) > 20) {
            if (cos_deg(dir) < 0) {
                p->facing = 2;
                p->xVelocity = -p->strength / 35;
            } else {
                p->facing = 0;
                p->xVelocity = p->strength / 35;
            }
            p->zVelocity = 2;
            set_anim(p, SSR_ANIM_DIVE_GK);
            p->frame = 0;
            if (ball->z < 64) {
                set_anim(p, SSR_ANIM_DIVE_LOW_GK);
                p->zVelocity = 1;
            }
        } else if (ball->z > 64 && ball->y > SSR_GOAL_LINE) {
            set_anim(p, SSR_ANIM_JUMP_GK);
            p->frame = 0;
            p->xVelocity = cos_deg(dir);
            p->facing = 1;
            p->zVelocity = 2;
        }
    } else if ((p->xVelocity > 0 && p->x > g + 90) || (p->xVelocity < 0 && p->x < g - 90)) {
        set_anim(p, SSR_ANIM_STAND);
        p->frame = 0;
        p->xVelocity = 0;
    }
}

static int is_set_piece(int type) {
    return type == 7 || type == 3 || type == 4;
}

// p_Update for a dribble chance (type 11): the nearest defender chases where
// the ball goes, the others step along x to the ball. Each always adds a
// little speed on its heading.
static void update_dribble_defender(SSRMatchPlayer *p, const SSRPlayerBall *ball) {
    float sp = ssr_player_speed(p, ball);
    p->direction = (float)intercept(p, ball, 0, 0);
    if (!ball->alive) {
        set_anim(p, SSR_ANIM_STAND);
        stop(p);
        return;
    }
    if (p->chasing) {
        p->direction = (float)intercept(p, ball, 1, 0);
    } else if (ball->y < p->y - 50 || ball->y >= p->y + 250) {
        stop(p);
    } else {
        p->direction = direction_to(p->x, p->y, ball->x, p->y);
    }
    p->xVelocity += cos_deg(p->direction) * 0.065f * sp;
    p->yVelocity += sin_deg(p->direction) * 0.065f * sp;
    float speed = (float)sqrt((double)(p->xVelocity * p->xVelocity + p->yVelocity * p->yVelocity));
    set_anim(p, speed > 0.5f ? SSR_ANIM_RUN : SSR_ANIM_STAND);
}

static void update_player(SSRPlayers *players, SSRMatchPlayer *p, const SSRChanceInfo *chance,
                          const SSRPlayerBall *ball, int now, SSRRandom *random) {
    if (chance->chanceType == 11 && p->type == SSR_PLAYER_DEFENDER) {
        update_dribble_defender(p, ball);
        goto integrate;
    }
    float ballDistance = distance(p->x, p->y, ball->x, ball->y);
    float bounceDistance = distance(p->x, p->y, ball->bounceX, ball->bounceY);
    float sp = ssr_player_speed(p, ball);
    p->direction = (float)intercept(p, ball, 0, 0);
    if (!ball->alive) {
        chance_over(p, ball);
    } else {
        if (!ssr_player_diving(p)) {
            p->xVelocity = cos_deg(p->direction) * sp;
            p->yVelocity = sin_deg(p->direction) * sp;
        }
        if (p->type == SSR_PLAYER_KEEPER) {
            // The keeper stands still while the player aims and taps.
            if (ball->kicking < 2) return;
            update_keeper(p, chance, ball, ssr_goal_centre(chance->pitchSide), random);
        } else if (p->type == SSR_PLAYER_DEFENDER) {
            if (p->inWall) {
                stop(p);
            } else {
                set_anim(p, SSR_ANIM_RUN);
                int chase = closest_on_team(players, p, ball, now) == p->id && !is_set_piece(chance->chanceType) &&
                            ball->actionType != 9 && ball->actionType != 10 &&
                            (chance->starLevel > 1 || chance->clubId == 0);
                if (!chase) {
                    if (ballDistance > 125) {
                        stop(p);
                    } else if (ball->lastTouchedBy == p->id && now < ball->kickTime + 500) {
                        stop(p);
                    }
                }
            }
        } else {
            set_anim(p, SSR_ANIM_RUN);
            if (p->makeRunDistance > 0) {
                if (!(ball->kicking == 2 && ballDistance < 150)) {
                    p->direction = p->makeRunDirection;
                    p->xVelocity = cos_deg(p->direction) * sp;
                    p->yVelocity = sin_deg(p->direction) * sp;
                    if (ball->lastTouchedBy == p->id ||
                        distance(p->x, p->y, (float)p->makeRunX, (float)p->makeRunY) < 5 || bounceDistance < 5) {
                        stop(p);
                        p->makeRunDistance = 0;
                    }
                }
            } else if (!(ball->lastTouchedBy > 0 && closest_on_team(players, p, ball, now) == p->id)) {
                if (ballDistance > 150 || (ball->lastTouchedBy == p->id && now < ball->kickTime + 500) ||
                    bounceDistance < 5) {
                    stop(p);
                }
            }
        }
    }
integrate:
    p->x += p->xVelocity;
    p->y += p->yVelocity;
    p->z += p->zVelocity;
    float f = chance->chanceType == 11 ? 0.96f : 0.98f;
    p->xVelocity *= f;
    p->yVelocity *= f;
    p->zVelocity -= 0.15f;
    if (p->z < 0) {
        p->z = 0;
        p->zVelocity = 0;
    }
    update_facing(p, ball);
    if (now > p->lastFrameTime + FRAME_LENGTH) {
        p->lastFrameTime = now;
        p->frame++;
        int length = ANIMS[p->anim].length;
        if (p->frame == length - 1) {
            if (ANIMS[p->anim].frames[p->frame] == -1) {
                p->frame = 0;
                p->anim = SSR_ANIM_STAND;
            }
        } else if (p->frame > length - 1) {
            p->frame = 0;
        }
    }
}

void ssr_players_update(SSRPlayers *players, const SSRChanceInfo *chance, const SSRPlayerBall *ball, int now,
                        SSRRandom *random) {
    if (ball->kicking < 2) players->offsideLine = ssr_players_offside_line(players, chance, ball);
    if (chance->chanceType == 11) {
        // UpdateDribbleChasers: the nearest player chases, the first on a tie.
        int best = -1;
        float bestDistance = 0;
        for (int i = 0; i < players->count; i++) {
            SSRMatchPlayer *p = &players->list[i];
            p->chasing = 0;
            float d = distance(p->x, p->y, ball->x, ball->y);
            if (best < 0 || d < bestDistance) {
                best = i;
                bestDistance = d;
            }
        }
        if (best >= 0) players->list[best].chasing = 1;
    }
    for (int i = 0; i < players->count; i++) {
        SSRMatchPlayer *p = &players->list[i];
        update_player(players, p, chance, ball, now, random);
        // p_UpdateCalling.
        if (p->calling != 0) p->calling = p->calling > -1.5f ? p->calling - 0.019f : 1.5f;
    }
}
