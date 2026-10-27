#include "ssr_ball.h"

#include <math.h>

// The constants of Monkey X (bb_std_lang), in floats.
#define D2R 0.017453292f
#define R2D 57.29578f

// Java takes the cosine of a float angle in double, then casts it.
static float cos_degrees(float angle) {
    return (float)cos((double)(angle * D2R));
}

static float sin_degrees(float angle) {
    return (float)sin((double)(angle * D2R));
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) v = lo;
    return v > hi ? hi : v;
}

static float distance(float x1, float y1, float x2, float y2) {
    float a = x1 - x2, b = y1 - y2;
    return (float)sqrt((double)(a * a + b * b));
}

// bb_various.GetDirection: the angle from (x1, y1) to (x2, y2), 0 to 360.
static float direction_to(float x1, float y1, float x2, float y2) {
    float a = (float)(atan2((double)(y2 - y1), (double)(x2 - x1)) * (double)R2D);
    if (a > 360) a -= 360;
    return a < 0 ? a + 360 : a;
}

typedef struct {
    int hit;
    float ab, cd;
} Intercept;

// bb_various.Lines_Intersect: segment AB against segment CD. cd is the
// place of the hit on CD, from 0 to 1.
static Intercept intersect(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    Intercept p = {0, 0, 0};
    float f9 = ay - cy, f10 = dx - cx, f11 = ax - cx, f12 = dy - cy;
    float f13 = f9 * f10 - f11 * f12;
    float f14 = bx - ax, f15 = by - ay;
    float f16 = f12 * f14 - f10 * f15;
    if (f16 == 0) return p;
    p.ab = f13 / f16;
    p.cd = (f9 * f14 - f11 * f15) / f16;
    p.hit = !(p.ab > 1 || p.ab < 0 || p.cd > 1 || p.cd < 0);
    return p;
}

SSRGoal ssr_goal(int side) {
    // The goal mouth by pitch side (c_TBall.m_CheckGoal), and the tweaks
    // Pitch.PostSize, ModernTopNetFront and ModernTopNetBack.
    static const float LEFT[3] = {452, 237, 22}, RIGHT[3] = {617, 402, 187};
    if (side < 0 || side > 2) side = 1;
    SSRGoal goal = {LEFT[side], RIGHT[side], 127, 85, 54, 5, 54, 30};
    return goal;
}

void ssr_ball_place(SSRBall *ball, float x, float y) {
    SSRBall fresh = {0};
    *ball = fresh;
    ball->x = ball->oldX = x;
    ball->y = ball->oldY = y;
    ball->speedFactor = ball->bounceFactor = ball->weightFactor = ball->curlFactor = 1;
    ball->alive = 1;
}

void ssr_ball_kick(SSRBall *ball, float direction, float power, float dip, float curl, float height, int header) {
    ball->direction = direction;
    ball->power = 0.1f * power;
    if (power > 1) {
        ball->zVelocity = 10 * dip + 3;
        if (header) {
            ball->zVelocity = 100 * dip + 0.5f;
        } else if (dip < 0) {
            ball->zVelocity = 0;
        }
        ball->dip = dip;
    } else {
        ball->zVelocity = 0;
        ball->dip = 0;
    }
    ball->curl = curl;
    ball->z = 2 * height;
    ball->kicks++;
}

// c_TBall.HitPost: the ball comes back off the post, at an angle by where
// it hit, with a random turn of -10 to 10 degrees.
static void hit_post(SSRBall *ball, float where, SSRRandom *random) {
    ball->curl = 0;
    ball->x = ball->oldX;
    ball->y = ball->oldY + 1;
    ball->power *= 0.725f;
    float a = (float)(atan2((double)-sin_degrees(ball->direction), (double)cos_degrees(ball->direction)) * (double)R2D);
    ball->direction = a;
    if (where < 0.15f) {
        ball->direction = a - 90;
    } else if (where < 0.25f) {
        ball->direction = a - 60;
    } else if (where < 0.45f) {
        ball->direction = a - 30;
    } else if (where > 0.85f) {
        ball->direction = a + 90;
    } else if (where > 0.75f) {
        ball->direction = a + 60;
    } else if (where > 0.55f) {
        ball->direction = a + 30;
    }
    ball->direction += ssr_rnd(random) * (10.0f - -10.0f) + -10.0f;
    ball->windForce = 0;
    ball->postHit = 1;
}

// c_TBall.HitNet: the ball stops in the side net.
static void hit_net(SSRBall *ball, int side, SSRRandom *random) {
    ball->curl = 0;
    ball->power *= ssr_rnd(random) * (0.05f - 0.02f) + 0.02f;
    float c = cos_degrees(ball->direction), s = sin_degrees(ball->direction);
    if (side) {
        c = -c;
        ball->x = ball->oldX;
    } else {
        s = -s;
        ball->y = ball->oldY;
    }
    ball->direction = (float)(atan2((double)s, (double)c) * (double)R2D);
    ball->windForce = 0;
}

static void goal_scored(SSRBall *ball) {
    if (!ball->alive) return;
    ball->inGoal = 1;
    ball->alive = 0;
    ball->outcome = SSR_BALL_GOAL;
}

// c_TBall.CheckGoal in the modern look.
static void check_goal(SSRBall *ball, const SSRGoal *g, SSRRandom *random) {
    float gl = g->line, f = g->postSize, back = g->back, left = g->left, right = g->right;
    float ox = ball->oldX, oy = ball->oldY;
    if (ball->z < (float)(int)(g->crossbar - 10)) {
        Intercept p = intersect(ox, oy, ball->x, ball->y, left - f, gl, left + 5, gl);
        if (p.hit) {
            hit_post(ball, 1 - p.cd, random);
            return;
        }
        p = intersect(ox, oy, ball->x, ball->y, right - 5, gl, right + f, gl);
        if (p.hit) {
            hit_post(ball, 1 - p.cd, random);
            return;
        }
    } else if (ball->z < g->crossbar && intersect(ox, oy, ball->x, ball->y, left - f, gl, right + f, gl).hit) {
        hit_post(ball, 0.5f, random);
        return;
    }
    Intercept line = intersect(ox, oy, ball->x, ball->y, left, gl, right, gl);
    if (!ball->inGoal && ball->oldZ < g->crossbar) {
        if (line.hit) {
            goal_scored(ball);
        } else {
            if (!ball->overBar) {
                if (intersect(ox, oy, ball->x, ball->y, left, gl, left, back).hit) {
                    hit_net(ball, 1, random);
                    ball->hitOutsideSideNet = 1;
                }
                if (intersect(ox, oy, ball->x, ball->y, right, gl, right, back).hit) {
                    hit_net(ball, 1, random);
                    ball->hitOutsideSideNet = 1;
                }
            }
            if (!ball->hitOutsideSideNet && ball->x > left && ball->x < right && ball->y < gl && ball->y > back) {
                goal_scored(ball);
            }
        }
    } else if (!ball->inGoal && line.hit) {
        ball->overBar = 1;
    }
    if (ball->inGoal) {
        // The net holds the ball.
        ball->curl = 0;
        if (ball->y < back) {
            ball->y = back;
            ball->power *= 0.95f;
        }
        if (ball->x < left) ball->x = left;
        if (ball->x > right) ball->x = right;
        float roof = g->roofBack + (g->roofFront - g->roofBack) * ((ball->y - back) / (gl - back)) - 10;
        if (ball->z > roof) {
            ball->z = roof;
            ball->zVelocity *= 0.95f;
            ball->power *= 0.95f;
        }
    }
}

void ssr_ball_update(SSRBall *ball, const SSRGoal *goal, SSRRandom *random) {
    ball->oldX = ball->x;
    ball->oldY = ball->y;
    ball->oldZ = ball->z;
    if (ball->windSpeed > 0) {
        ball->windForce += ball->inGoal || ball->z < 1 ? -0.02f : 0.01f;
        ball->windForce = clampf(ball->windForce, 0, ball->windSpeed);
        ball->x += cos_degrees(ball->windDirection) * ball->windForce;
        ball->y += sin_degrees(ball->windDirection) * ball->windForce;
    }
    ball->direction += ball->curl * ball->curlFactor;
    while (ball->direction > 359) ball->direction -= 360;
    while (ball->direction < 0) ball->direction += 360;
    float friction = 0.999f;
    if (ball->z < 1) {
        friction = ball->weather == 0 ? 0.99f : ball->weather == 1 ? 0.994f : ball->weather == 2 ? 0.985f : 0.999f;
    }
    ball->power *= friction;
    if (ball->power < 1) ball->power *= 0.98f;
    ball->zVelocity = ball->zVelocity - ball->weightFactor * 0.15f + ball->dip;
    // The game multiplies in this order: (cos * power) * speed.
    ball->x += cos_degrees(ball->direction) * ball->power * ball->speedFactor;
    ball->y += sin_degrees(ball->direction) * ball->power * ball->speedFactor;
    ball->z = ball->z + ball->zVelocity;
    if (ball->z < 0) {
        ball->bounces++;
        ball->zVelocity = -ball->zVelocity * ball->bounceFactor * 0.725f;
        ball->z = 0;
        if (ball->dip < 0) ball->dip = 0;
    }
    if (goal) check_goal(ball, goal, random);
    if (!ball->alive) return;
    // The ball dies past the goal line, or when it stops (c_TBall.m_Update).
    if (goal && ball->y < 125 && !ball->inGoal) {
        ball->alive = 0;
        ball->outcome = SSR_BALL_MISSED;
    } else if (ball->power < 0.15f && !ball->holdStop) {
        ball->alive = 0;
        ball->outcome = SSR_BALL_STOPPED;
    }
}

void ssr_ball_aim(const SSRBall *ball, float dragX, float dragY, int powerSkill, float *direction, float *power) {
    *direction = direction_to(ball->x, ball->y, dragX, dragY) + 180;
    float d = clampf(distance(ball->x, ball->y, dragX, dragY) - 10, 0, 100);
    *power = clampf(d, 10, powerSkill * 0.4f + 60);
}

void ssr_ball_tap(float aimDirection, float aimPower, float tapX, float tapY, int technique, float depth,
                  float tapBallZ, float *direction, float *power, float *dip, float *curl, float *height,
                  int *header) {
    float tech = technique * 2.0e-5f;
    float d = 1 - distance(tapX, tapY, 0, 0) * 0.0076923077f;
    float f8 = depth < 1 ? d * 0.7f + 0.25f : d * 0.25f + 0.75f;
    *power = aimPower * f8;
    *dip = tapY * 7.7e-4f;
    *curl = tapX * -(tech + 0.003f);
    *height = tapBallZ * 0.05f;
    *direction = depth < 1 ? aimDirection + tapX * -0.15f : aimDirection;
    *header = depth < 1;
}
