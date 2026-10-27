#include "ssr_training.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define D2R 0.017453292f
#define R2D 57.29578f

static float distance(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return (float)sqrt((double)(dx * dx + dy * dy));
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static SSRTrainingObject *add_object(SSRTraining *t, SSRTrainingObjectType type, float x, float y) {
    if (t->objectCount >= SSR_TRAINING_OBJECTS) return NULL;
    SSRTrainingObject *o = &t->objects[t->objectCount++];
    memset(o, 0, sizeof(*o));
    o->type = type;
    o->x = x;
    o->y = y;
    o->alive = 1;
    return o;
}

static void add_pole(SSRTraining *t, float x, float y) {
    SSRTrainingObject *o = add_object(t, SSR_OBJECT_POLE, x, y);
    if (o) snprintf(o->colour, sizeof(o->colour), "FFFF00");
}

static SSRTrainingObject *add_line(SSRTraining *t, int x1, int y1, int x2, int y2, const char *colour,
                                   int finishLine) {
    SSRTrainingObject *o = add_object(t, SSR_OBJECT_LINE, (float)x1, (float)y1);
    if (!o) return NULL;
    o->x2 = x2;
    o->y2 = y2;
    o->finishLine = finishLine;
    snprintf(o->colour, sizeof(o->colour), "%s", colour);
    return o;
}

int ssr_training_lines(const SSRTraining *t) {
    int n = 0;
    for (int i = 0; i < t->objectCount; i++) n += t->objects[i].type == SSR_OBJECT_LINE && t->objects[i].alive;
    return n;
}

static int dummies_alive(const SSRTraining *t) {
    int n = 0;
    for (int i = 0; i < t->objectCount; i++) n += t->objects[i].type == SSR_OBJECT_DUMMY && t->objects[i].alive;
    return n;
}

int ssr_training_skill(int type) {
    switch (type) {
    case 2:
        return 0;
    case 3:
        return 1;
    case 4:
        return 2;
    case 5:
    case 8:
        return 3;
    case 6:
    case 9:
        return 4;
    default:
        return -1;
    }
}

// c_TDummy.m_UpdateWallLocations: the wall stands 10.5 yards of the
// c_TPitch scale (19.2 a yard) from the ball, toward the goal.
static void place_wall(SSRTraining *t, int bx, int by) {
    float a = (float)(atan2((double)((float)by - 128.0f), (double)((float)bx - 320.0f)) * (double)R2D);
    int k = 5;
    for (int i = 0; i < t->objectCount; i++) {
        SSRTrainingObject *o = &t->objects[i];
        if (o->type != SSR_OBJECT_DUMMY) continue;
        int off = k == 5 ? 0 : k == 4 ? 5 : k == 3 ? -5 : k == 2 ? 10 : -10;
        float f = (float)off + a;
        o->x = (float)bx - (float)cos((double)(D2R * f)) * 201.6f;
        o->y = (float)by - (float)sin((double)(f * D2R)) * 201.6f;
        if (k > 1) k--;
    }
}

// The ball of the drill (c_TBall.m_Reset(5)) at its start.
static void reset_ball(SSRTraining *t, SSRRandom *random) {
    SSRChanceSetup s = {0};
    s.power = t->player.power;
    s.technique = t->player.technique;
    s.pace = t->player.pace;
    s.level = t->level;
    s.starLevel = t->player.starLevel;
    s.controlBoots = t->player.controlBoots;
    s.windSpeed = t->windSpeed;
    s.windDirection = t->windDirection;
    float x = t->chance.info.startX, y = t->chance.info.startY;
    ssr_chance_start_training(&t->chance, &s, t->training, x, y, random);
    t->ballAlpha = 1.5f;
}

// c_ACone.m_Randomize: the look of the cones takes one random number.
static void cone_look(SSRRandom *random) {
    ssr_rand_float(random, 0, 2);
}

void ssr_training_start(SSRTraining *t, int type, int level, int lives, int trial, const SSRTrainingPlayer *player,
                        SSRRandom *random) {
    memset(t, 0, sizeof(*t));
    t->player = *player;
    t->training = type == 5 ? 8 : type;
    t->level = level;
    t->lives = lives;
    t->trial = trial;
    t->success = -1;
    int L = level;
    if (trial == 1) {
        // SetUpWeatherAndStadium(0): the wind, and the colours of the
        // hoardings.
        float top = (float)(player->starLevel / 2);
        top = top < 0 ? 0 : top > 5 ? 5 : top;
        float w = ssr_rand_float(random, -1, top);
        t->windSpeed = w < 1 ? 0 : w;
        t->windDirection = (float)ssr_rand(random, 360);
        for (int i = 0; i < 9; i++) ssr_rand2(random, 0, 1);
    }
    cone_look(random);
    int sx = 320, sy = 800;
    switch (t->training) {
    case 2: {
        if (L < 5) t->instruction = "CMESSAGE_TRAININGTECHNIQUE";
        int gate = (int)(ssr_rand_float(random, (float)(-L * 10), (float)(L * 10)) + 320.0f);
        int d = clampi(L * 25 + 150, 150, 650), hw = 40 - L / 2, gy = 800 - d;
        add_pole(t, (float)(gate - hw), (float)gy);
        add_pole(t, (float)(gate + hw), (float)gy);
        SSRTrainingObject *line = add_line(t, gate - hw, gy, gate + hw, gy, "0000FF", 0);
        if (line) line->tall = 1;
        if (L > 3) {
            t->windSpeed = (float)L / 4.0f;
            t->windDirection = ssr_rand_float(random, 0, 360);
        }
        sx = 320;
        sy = 800;
        break;
    }
    case 3: {
        if (L < 5) t->instruction = "CMESSAGE_TRAININGSETPIECES";
        sx = (int)(ssr_rand_float(random, (float)(-L * 12), (float)(L * 12)) + 320.0f);
        sy = (int)(128.0f + 260.0f + (float)(L * 20));
        int n = L <= 1 ? 1 : L == 2 ? 2 : L <= 4 ? 3 : L <= 7 ? 4 : 5;
        for (int i = 0; i < n; i++) add_object(t, SSR_OBJECT_DUMMY, 0, 0);
        if (L > 3) {
            t->windSpeed = (float)L / 4.0f;
            t->windDirection = ssr_rand_float(random, 0, 360);
        }
        if (L < 4) sx = (int)(ssr_rand_float(random, -150.0f, 150.0f) + 320.0f);
        place_wall(t, sx, sy);
        break;
    }
    case 4: {
        if (L < 5) t->instruction = "CMESSAGE_TRAININGPOWER";
        int r = L > 1 ? L * 10 : 0;
        sx = (int)(ssr_rand_float(random, (float)-r, (float)r) + 320.0f);
        sy = (int)(128.0f + 200.0f + (float)(L * 20));
        if (L > 3) {
            t->windSpeed = (float)L / 4.0f;
            t->windDirection = ssr_rand_float(random, -90.0f, 90.0f) + 90.0f;
        }
        break;
    }
    case 8: {
        t->instruction = "CMESSAGE_TRAININGDRIBBLING";
        sx = 320;
        sy = 900 - (20 - L) * 10;
        // The cones make gates, with a line from each cone to the one
        // before it.
        int hist[2] = {0, 1}, cx = 0, px = 0, py = 0;
        for (int i = 0; i <= L / 2 + 1; i++) {
            if (px == 0) {
                cx = sx;
                if (L == 1) cx -= 50;
                py = sy - 100;
                SSRTrainingObject *cone = add_object(t, SSR_OBJECT_CONE, (float)cx, (float)py);
                if (cone) cone->frame = 1;
            } else {
                int r = ssr_rand2(random, 0, 1);
                if (hist[0] == hist[1] && hist[1] == r) r = 1 - r;
                hist[0] = hist[1];
                hist[1] = r;
                cx = r == 0 ? px - (55 - L) : px + (55 - L);
                int ny;
                if (L == 1) {
                    cx = px + 100;
                    ny = py;
                } else {
                    ny = py - (75 - L);
                }
                SSRTrainingObject *cone = add_object(t, SSR_OBJECT_CONE, (float)cx, (float)ny);
                if (cone) cone->frame = 1;
                add_line(t, cx, ny, px, py, "0000FF", 0);
                py = ny;
            }
            px = cx;
        }
        int fy = py - 100;
        add_pole(t, -200, (float)fy);
        add_pole(t, 840, (float)fy);
        add_line(t, -200, fy, 840, fy, "00FF00", 1);
        break;
    }
    case 6: {
        if (L < 5) t->instruction = "CMESSAGE_TRAININGVISION";
        sx = 320;
        sy = (int)(960.0f - (float)(300 - (L * 140) / 20));
        int dx = ssr_rand2(random, -L * 10, L * 10);
        add_object(t, SSR_OBJECT_DUMMY, (float)(320 + dx), (float)(sy - (L * 24 + 120)));
        if (L > 3) {
            t->windSpeed = (float)L / 4.0f;
            t->windDirection = ssr_rand_float(random, 0, 360);
        }
        break;
    }
    default:
        break;
    }
    t->chance.info.startX = (float)sx;
    t->chance.info.startY = (float)sy;
    reset_ball(t, random);
    if (t->training == 4) {
        // CreateKeeper with the power of the player, then the keeper goes to
        // its place between the ball and the goal.
        ssr_players_add_keeper(&t->chance.players, &t->chance.info, (float)player->power, random);
    }
    // c_ATraining.m_SetUp makes the view again: one more random number.
    cone_look(random);
}

static int off_screen(const SSRBall *b) {
    return b->x < 0 || b->x > 640 || b->y < 0 || b->y > 960;
}

static void show_message(SSRTraining *t, int now, const char *tag, int outcome, int ms) {
    t->message = tag;
    t->outcome = outcome;
    t->messageEnd = now + ms;
}

// The ball dies (c_TBall.m_Die): it stops and fades.
static void kill_ball(SSRTraining *t) {
    t->chance.ball.alive = 0;
    if (t->chance.state != SSR_CHANCE_DRIBBLE) t->chance.state = SSR_CHANCE_OVER;
}

// c_TMatch.m_LoseLife. The dribble drill goes on until the last life.
static void lose_life(SSRTraining *t, int now) {
    t->lives--;
    if (t->training == 8 && t->lives >= 1) return;
    t->success = 0;
    if (t->chance.ball.alive) kill_ball(t);
    if (t->training == 3 || t->training == 4) {
        // c_MatchChance.m_CheckMessage: a save, a miss over the line, or no
        // banner.
        if (t->chance.result == SSR_RESULT_SAVED) {
            show_message(t, now, "Saved!", -1, 2000);
        } else if (t->chance.ball.y < 127) {
            show_message(t, now, "Miss", -2, 2000);
        }
    } else {
        show_message(t, now, "Fail!", -1, 1500);
    }
}

static void succeed(SSRTraining *t, int now) {
    t->success = 1;
    show_message(t, now, "Success!", 1, 1500);
}

// Lines_Intersect: 1 when the two segments cross.
static int crosses(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    float f9 = ay - cy, f10 = dx - cx, f11 = ax - cx, f12 = dy - cy;
    float f13 = f9 * f10 - f11 * f12;
    float f14 = bx - ax, f15 = by - ay;
    float f16 = f12 * f14 - f10 * f15;
    if (f16 == 0.0f) return 0;
    float ab = f13 / f16, cd = (f9 * f14 - f11 * f15) / f16;
    return ab >= 0 && ab <= 1 && cd >= 0 && cd <= 1;
}

// c_TTrainingObject.m_UpdateAll: the poles, the cones, the dummies, and
// the lines against the ball, in the order that they were made.
static void update_objects(SSRTraining *t, int now, SSRRandom *random) {
    SSRChance *c = &t->chance;
    SSRBall *b = &c->ball;
    for (int i = 0; i < t->objectCount; i++) {
        SSRTrainingObject *o = &t->objects[i];
        switch (o->type) {
        case SSR_OBJECT_POLE:
            // 0.6 yards of the c_TPitch scale.
            if (strcmp(o->colour, "FF0000") != 0 && distance(o->x, o->y, b->x, b->y) < 11.52f && b->z < 100.0f) {
                o->alive = 0;
                snprintf(o->colour, sizeof(o->colour), "FF0000");
                if (t->training == 8) {
                    c->dribbleX = -c->dribbleX * 0.2f;
                    c->dribbleY = -c->dribbleY * 0.2f;
                    lose_life(t, now);
                } else {
                    kill_ball(t);
                }
            }
            break;
        case SSR_OBJECT_CONE:
            if (o->alive && distance(o->x, o->y, b->x, b->y) < 15.0f && b->z < 10.0f && o->frame == 1) {
                o->frame = 0;
                o->alive = 0;
                if (t->training == 8) {
                    c->dribbleX = -c->dribbleX * 0.4f;
                    c->dribbleY = -c->dribbleY * 0.4f;
                    lose_life(t, now);
                }
            }
            break;
        case SSR_OBJECT_DUMMY: {
            // Only a ball that goes up the screen hits a dummy.
            float d = fmodf(b->direction, 360);
            if (d < 0) d += 360;
            if (o->alive && d > 180.0f && fabsf(o->x - b->x) < 22.0f && fabsf(o->y - b->y) < 8.0f && b->z < 62.0f) {
                o->alive = 0;
                o->frame = 1;
                b->curl = 0;
                b->direction = ssr_rand_float(random, -45.0f, 45.0f) + 90.0f;
                b->power *= 0.3f;
            }
            break;
        }
        case SSR_OBJECT_LINE:
            // CheckSplit: the finish line counts only when it is the last
            // line. A high ball over a gate counts too.
            if (o->alive && b->alive && (!o->finishLine || ssr_training_lines(t) <= 1) &&
                strcmp(o->colour, "FF0000") != 0 &&
                crosses((float)o->x, o->y, (float)o->x2, (float)o->y2, b->oldX, b->oldY, b->x, b->y)) {
                o->alive = 0;
            }
            break;
        }
    }
}

// c_TMatch.m_ResetTraining: a new attempt, or the end of the drill.
static void reset(SSRTraining *t, SSRRandom *random) {
    if (t->lives == 0) {
        t->over = -1;
        return;
    }
    t->success = -1;
    t->message = NULL;
    t->messageEnd = 0;
    if (t->training == 2 || t->training == 6) {
        // The same layout: the ball goes back, and the poles stand again.
        for (int i = 0; i < t->objectCount; i++) {
            SSRTrainingObject *o = &t->objects[i];
            if (o->type != SSR_OBJECT_POLE) continue;
            o->alive = 1;
            snprintf(o->colour, sizeof(o->colour), "FFFF00");
        }
        reset_ball(t, random);
        cone_look(random);
        return;
    }
    SSRTraining next;
    ssr_training_start(&next, t->training, t->level, t->lives, t->trial, &t->player, random);
    *t = next;
}

void ssr_training_update(SSRTraining *t, int now, const SSRTapTouch *touch, SSRRandom *random) {
    if (t->over) return;
    SSRChance *c = &t->chance;
    // While the instruction shows, the drill waits. The touch that ends it
    // does not aim.
    if (t->instruction) {
        if (touch->down) {
            t->instruction = NULL;
            t->startTime = now;
        }
        return;
    }
    ssr_chance_update(c, now, touch, random);
    if (!c->ball.alive) t->ballAlpha -= 0.02f;
    if (off_screen(&c->ball) && c->state != SSR_CHANCE_DRIBBLE) t->ballAlpha = 0;
    update_objects(t, now, random);
    if (t->success == -1) {
        switch (t->training) {
        case 2:
            if (ssr_training_lines(t) == 0) {
                succeed(t, now);
            } else if (t->ballAlpha <= 0) {
                lose_life(t, now);
            }
            break;
        case 3:
        case 4:
            // c_TMatch.m_PlayerScored in training 3 and 4.
            if (c->result == SSR_RESULT_GOAL) {
                t->success = 1;
                show_message(t, now, "Goal!", 0, 2000);
            } else if (t->ballAlpha <= 0) {
                lose_life(t, now);
            }
            break;
        case 8:
            if (t->lives <= 0) break;
            if (ssr_training_lines(t) == 0) {
                succeed(t, now);
            } else if (off_screen(&c->ball)) {
                t->lives = 1;
                lose_life(t, now);
            } else if (t->level > 3 && t->startTime > 0) {
                int limit = 10000 + (t->level > 7 ? (t->level - 7) * 1000 : 0);
                if (now > t->startTime + limit) {
                    t->lives = 1;
                    lose_life(t, now);
                }
            }
            break;
        case 6:
            if (dummies_alive(t) == 0) {
                succeed(t, now);
            } else if (t->ballAlpha <= 0) {
                lose_life(t, now);
            }
            break;
        default:
            break;
        }
    }
    if (now < t->messageEnd) return;
    t->message = NULL;
    if (t->success == 0) {
        reset(t, random);
    } else if (t->success == 1) {
        t->over = 1;
    }
}
