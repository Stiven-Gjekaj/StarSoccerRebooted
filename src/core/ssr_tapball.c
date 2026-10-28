#include "ssr_tapball.h"

#include <math.h>

#include "ssr_ball.h"

#define GRAVITY 0.16f
#define BOUNCE 0.7f
#define TAP_RADIUS 130.0f
#define CONTROL_RADIUS 157.0f

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float distance(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return (float)sqrt((double)(dx * dx + dy * dy));
}

float ssr_tapball_speed(int level, float opponentStrength) {
    if (opponentStrength > 0) return clampf(opponentStrength, 1, 100) / 32.5f;
    return clampf((float)level, 1, 20) * 0.154f;
}

int ssr_tapball_limit(int training, int level, float opponentStrength) {
    if (training == 1 || training == 7) return 20000;
    int k = (int)clampf((float)level, 1, 20);
    if (opponentStrength > 0) k = (int)(clampf(opponentStrength, 1, 100) / 5);
    return (int)clampf(7500.0f - 200.0f * (float)k, 3500, 7500);
}

static void set_up_dead(SSRTapBall *b) {
    b->x = 320;
    b->z = 0;
    b->xVelocity = b->zVelocity = b->rollVelocity = 0;
    b->type = SSR_TAPBALL_DEAD;
}

static void set_up_rolling(SSRTapBall *b, float v, SSRRandom *random) {
    if (ssr_rand(random, 2) == 1) {
        b->x = 140;
        b->rollVelocity = -(v + 1);
    } else {
        b->x = 500;
        b->rollVelocity = v + 1;
    }
    b->xVelocity = b->rollVelocity;
    b->z = b->zVelocity = 0;
    b->bounces = 1;
    b->type = SSR_TAPBALL_ROLLING;
}

static void set_up_bouncing(SSRTapBall *b, float v, SSRRandom *random) {
    b->zVelocity = -4;
    b->rollVelocity = ssr_rand_float(random, -(3.75f + v), 3.75f + v);
    switch (ssr_rand(random, 5)) {
    case 1:
        b->x = ssr_rand_float(random, -50, 100);
        b->z = ssr_rand_float(random, 350, 750);
        b->bounces = 1;
        b->xVelocity = -(v + 1.5f);
        break;
    case 2:
        b->x = 320;
        b->z = 700;
        b->xVelocity = 0;
        b->rollVelocity *= 2;
        break;
    case 3:
        b->x = ssr_rand_float(random, 540, 710);
        b->z = ssr_rand_float(random, 350, 750);
        b->bounces = 1;
        b->xVelocity = v + 1.5f;
        break;
    case 4:
        b->zVelocity = ssr_rand_float(random, 0, 5) + 2;
        b->z = ssr_rand_float(random, 0, 350);
        b->bounces = 1;
        if (ssr_rand(random, 2) == 1) {
            b->x = 0;
            b->xVelocity = -(v + 3);
        } else {
            b->x = 640;
            b->xVelocity = v + 3;
        }
        break;
    default:
        b->zVelocity = -ssr_rand_float(random, 5, 10);
        b->z = ssr_rand_float(random, 400, 750);
        b->bounces = 1;
        if (ssr_rand(random, 2) == 1) {
            b->x = 0;
            b->xVelocity = -(v + 2);
        } else {
            b->x = 640;
            b->xVelocity = v + 2;
        }
        break;
    }
    b->type = SSR_TAPBALL_BOUNCING;
}

static void set_up_headed(SSRTapBall *b, float v, SSRRandom *random) {
    b->zVelocity = 4.5f;
    b->rollVelocity = ssr_rand_float(random, -(3.5f + v), 3.5f + v);
    if (ssr_rand(random, 2) == 1) {
        b->x = 0;
        b->xVelocity = -(v + 3);
    } else {
        b->x = 640;
        b->xVelocity = v + 3;
    }
    b->z = ssr_rand_float(random, 450, 550);
    b->bounces = 1;
    b->depth = 0.65f;
    b->depthAccel = 1.001f;
    b->type = SSR_TAPBALL_HEADED;
}

void ssr_tapball_reset(SSRTapBall *b, const SSRTapSetup *s, SSRRandom *random) {
    *b = (SSRTapBall){0};
    b->rotation = 30;
    b->powScale = -1;
    b->depth = b->depthAccel = 1;
    b->weight = b->bounce = 1;
    float v = ssr_tapball_speed(s->level, s->opponentStrength);
    if (s->training == 0) {
        // A free kick, a corner, a penalty, or a shoot-out is a dead ball.
        if (s->chanceType == 3 || s->chanceType == 7 || s->chanceType == 4 || s->chanceStage == 8) {
            set_up_dead(b);
        } else if (s->header) {
            set_up_headed(b, v, random);
        } else {
            int r = ssr_rand(random, 2);
            if (s->careerApps < 1 && s->chances == 1) {
                r = 3;
            } else if ((s->careerGoals + s->careerPasses) / 2.0f < 10) {
                r = 1;
            }
            if (r == 1) {
                set_up_rolling(b, v, random);
            } else if (r == 2) {
                set_up_bouncing(b, v, random);
            } else {
                set_up_dead(b);
            }
        }
    } else if (s->training != 7) {
        // c_TBall2.Reset: above level 1 the ball of the drills of technique,
        // power, and vision rolls or bounces.
        set_up_dead(b);
        if (s->level > 1 && (s->training == 2 || s->training == 4 || s->training == 6)) {
            if (ssr_rand(random, 2) == 1) set_up_rolling(b, v, random);
            else set_up_bouncing(b, v, random);
        }
    } else if (s->chanceType == 3) {
        // The practice: a free kick is a dead ball.
        set_up_dead(b);
    } else if (s->level <= 1) {
        set_up_dead(b);
    } else if (s->level == 2) {
        set_up_rolling(b, v, random);
    } else if (s->level == 3) {
        if (ssr_rand(random, 2) == 1) set_up_rolling(b, v, random);
        else set_up_bouncing(b, v, random);
    } else {
        set_up_bouncing(b, v, random);
    }
    b->y = 810 - b->z;
    b->limit = ssr_tapball_limit(s->training, s->level, s->opponentStrength);
}

static int moving(const SSRTapBall *b) {
    return b->xVelocity > 0 || b->zVelocity > 0 || b->rollVelocity > 0;
}

SSRTapBallEvent ssr_tapball_update(SSRTapBall *b, int now, const SSRTapTouch *touch, int controlBoots, int *beep) {
    if (beep) *beep = 0;
    if (b->done) return SSR_TAPBALL_NONE;
    float radius = b->depth * TAP_RADIUS;
    if (b->timer == 0) b->timer = now;
    if (b->controlX + b->controlY == 0 && touch->down) {
        b->controlX = touch->x;
        b->controlY = touch->y;
    } else if (!touch->down) {
        b->controlX = b->controlY = 0;
    }
    SSRTapBallEvent event = SSR_TAPBALL_NONE;
    if (b->powVelocity > 0) {
        // A tap waits for its kick. The ball does not move.
        b->powVelocity *= 0.99f;
        b->powRotation += b->powVelocity * 1.25f;
        b->powScale += 0.05f;
        if (b->powScale > 0.75f) {
            b->done = 1;
            return SSR_TAPBALL_KICK;
        }
    } else {
        if (fabsf(b->zVelocity) != 0) {
            float g = GRAVITY * b->weight;
            if (b->depth < 1) g *= 0.65f;
            b->zVelocity -= g;
            b->z += b->zVelocity;
            if (b->z < 0) {
                b->zVelocity = -b->zVelocity * BOUNCE * b->bounce;
                b->z = 0;
                b->rollVelocity *= 0.5f;
                b->bounces++;
                if (b->xVelocity == 0) {
                    b->xVelocity = b->rollVelocity;
                } else {
                    if (fabsf(b->xVelocity) > fabsf(b->rollVelocity)) b->rollVelocity = b->xVelocity;
                    b->xVelocity *= 0.75f;
                }
            }
        }
        b->depth *= b->depthAccel;
        b->xVelocity *= 0.997f;
        b->rollVelocity *= 0.997f;
        b->rotation += b->rollVelocity;
        b->x -= 2 * b->xVelocity;
        b->y = 810 - b->z;

        if (b->x < -100 || b->x > 740 || (now > b->timer + b->limit && moving(b))) {
            b->done = 1;
            return SSR_TAPBALL_DUD;
        }

        b->touching = 0;
        if (touch->hit) {
            b->tapX = touch->x;
            b->tapY = touch->y;
            if (distance(b->tapX, b->tapY, b->x, b->y) < radius) {
                b->powVelocity = 2;
                event = SSR_TAPBALL_TAPPED;
            }
        } else if (now > b->controlTime + 500 && fabsf(b->xVelocity) + fabsf(b->zVelocity) > 0) {
            if (controlBoots && touch->down && b->depth >= 1) {
                b->touching = 1;
                if (distance(touch->x, touch->y, b->x, b->y) < CONTROL_RADIUS) {
                    // The finger pushes the ball away from it.
                    b->controlTime = now;
                    b->controlCount++;
                    float speed = (float)sqrt((double)(b->xVelocity * b->xVelocity + b->zVelocity * b->zVelocity));
                    float direction = (float)atan2((double)(touch->y - b->y), (double)(touch->x - b->x));
                    float drag = distance(b->controlX, b->controlY, touch->x, touch->y) * 0.2f;
                    if (speed < drag) speed = drag;
                    if (speed > 15) speed = 15;
                    b->xVelocity = (float)cos((double)direction) * speed * (b->zVelocity == 0 ? 0.5f : 0.35f);
                    b->zVelocity = (float)sin((double)direction) * speed * 0.5f;
                    b->rollVelocity = b->xVelocity / 2;
                    b->controlX = b->controlY = 0;
                }
            } else {
                b->controlTime = 0;
            }
        }
    }
    if (b->xVelocity > 0 || b->zVelocity != 0 || b->rollVelocity > 0) {
        // Three warnings, at 3, 2, and 1 seconds before the end.
        for (int i = 0; i < 3; i++) {
            if (now > b->timer + b->limit - 1000 * (3 - i) && b->beeps == i) {
                b->beeps++;
                if (beep) *beep = 1;
            }
        }
    }
    return event;
}

float ssr_tapball_time_left(const SSRTapBall *b, int now) {
    if (b->xVelocity == 0 && b->zVelocity == 0 && !(b->rollVelocity > 0)) return 0;
    return (float)(b->timer + b->limit - now) / (float)b->limit;
}

void ssr_tapball_kick(const SSRTapBall *b, float aimDirection, float aimPower, int technique, float *direction,
                      float *power, float *dip, float *curl, float *height, int *header) {
    ssr_ball_tap(aimDirection, aimPower, b->tapX - b->x, b->tapY - b->y, technique, b->depth, b->z, direction, power,
                 dip, curl, height, header);
}
