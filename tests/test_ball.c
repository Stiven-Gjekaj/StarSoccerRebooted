// Tests ssr_ball against the trajectories that a copy of c_TBall in Java
// gave (the Java harness of the match research). The start is the same in
// each case: the ball 20 yards out in the centre, a drag of 80 straight
// down, Power 50 and Technique 50, pitch side 1.

#include <math.h>
#include <stdio.h>

#include "ssr_ball.h"
#include "ssr_test.h"

typedef struct {
    int tick;
    float x, y, z, zVelocity, power, direction;
} Sample;

static int close_to(float a, float b) {
    return fabsf(a - b) < 0.0005f;
}

static void shoot(float dragX, float dragY, float tapX, float tapY, const Sample *samples, int count, int ticks,
                  SSRBallOutcome outcome, int bounces) {
    SSRBall ball;
    ssr_ball_place(&ball, 320, 427);
    float direction, power, kickDirection, kickPower, dip, curl, height;
    int header;
    ssr_ball_aim(&ball, 320 + dragX, 427 + dragY, 50, &direction, &power);
    ssr_ball_tap(direction, power, tapX, tapY, 50, 1, 0, &kickDirection, &kickPower, &dip, &curl, &height, &header);
    ssr_ball_kick(&ball, kickDirection, kickPower, dip, curl, height, header);
    SSRGoal goal = ssr_goal(1);
    SSRRandom random = {SSR_RANDOM_START};
    int next = 0, tick = 0;
    while (ball.alive && tick < 600) {
        ssr_ball_update(&ball, &goal, &random);
        tick++;
        if (next < count && samples[next].tick == tick) {
            const Sample *s = &samples[next++];
            int ok = close_to(ball.x, s->x) && close_to(ball.y, s->y) && close_to(ball.z, s->z) &&
                     close_to(ball.zVelocity, s->zVelocity) && close_to(ball.power, s->power) &&
                     close_to(ball.direction, s->direction);
            if (!ok) {
                printf("tick %d: %.4f %.4f %.4f %.4f %.4f %.4f\n", tick, ball.x, ball.y, ball.z, ball.zVelocity,
                       ball.power, ball.direction);
            }
            CHECK(ok);
        }
    }
    CHECK(next == count);
    CHECK(tick == ticks);
    CHECK(ball.outcome == outcome);
    CHECK(ball.bounces == bounces);
}

int main(void) {
    static const Sample centre[] = {
        {10, 320.0000f, 358.0110f, 21.7500f, 1.5000f, 6.8679f, 270.0000f},
        {20, 320.0000f, 289.7088f, 28.5000f, -0.0000f, 6.7995f, 270.0000f},
        {30, 320.0000f, 222.0866f, 20.2500f, -1.5000f, 6.7318f, 270.0000f},
        {40, 320.0000f, 155.1975f, 1.9163f, 1.9163f, 6.6048f, 270.0000f},
        {45, 320.0000f, 122.2727f, 9.2475f, 1.1663f, 6.5718f, 270.0000f},
    };
    shoot(0, 80, 0, 0, centre, 5, 45, SSR_BALL_GOAL, 1);
    static const Sample curled[] = {
        {10, 319.0888f, 367.5870f, 28.9110f, 2.4240f, 5.9155f, 268.4000f},
        {20, 316.5446f, 308.8135f, 47.4420f, 1.3860f, 5.8566f, 266.7999f},
        {30, 312.4019f, 250.7182f, 55.5930f, 0.3480f, 5.7983f, 265.1999f},
        {40, 306.6960f, 193.3381f, 53.3640f, -0.6900f, 5.7406f, 263.5999f},
        {50, 299.4630f, 136.7092f, 40.7550f, -1.7280f, 5.6834f, 261.9998f},
        {52, 297.8363f, 125.4766f, 36.9876f, -1.9356f, 5.6721f, 261.6798f},
    };
    shoot(0, 80, 40, 60, curled, 6, 52, SSR_BALL_GOAL, 0);
    static const Sample low[] = {
        {10, 320.0000f, 368.3825f, 0.0000f, 0.0587f, 5.6002f, 270.0000f},
        {40, 320.0000f, 224.0669f, 0.0000f, 0.0630f, 4.1425f, 270.0000f},
        {67, 320.0000f, 126.6023f, 0.0000f, 0.0630f, 3.1580f, 270.0000f},
    };
    shoot(0, 80, 0, -60, low, 3, 67, SSR_BALL_GOAL, 67);
    static const Sample wide[] = {
        {10, 353.6680f, 359.6640f, 24.1370f, 1.8080f, 7.4945f, 296.5651f},
        {40, 452.6744f, 161.6513f, 15.7880f, -2.2300f, 7.2729f, 296.5651f},
        {46, 472.1216f, 122.7569f, 0.0000f, 2.2023f, 7.2294f, 296.5651f},
    };
    shoot(-40, 80, 0, 20, wide, 3, 46, SSR_BALL_MISSED, 1);
    return TEST_RESULT();
}
