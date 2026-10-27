// Tests ssr_tapball with chances that the test makes.

#include <math.h>

#include "ssr_tapball.h"
#include "ssr_test.h"

static int near(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static void test_gives_the_time_and_the_speed(void) {
    CHECK(ssr_tapball_limit(0, 1, 0) == 7300);
    CHECK(ssr_tapball_limit(0, 20, 0) == 3500);
    CHECK(ssr_tapball_limit(0, 5, 50) == 5500);
    CHECK(ssr_tapball_limit(1, 5, 0) == 20000);
    CHECK(ssr_tapball_limit(7, 5, 0) == 20000);
    CHECK(near(ssr_tapball_speed(1, 0), 0.154f));
    CHECK(near(ssr_tapball_speed(30, 0), 3.08f));
    CHECK(near(ssr_tapball_speed(5, 65), 2));
}

static void test_chooses_the_ball(void) {
    SSRTapBall ball;
    SSRRandom random = {SSR_RANDOM_START};
    // A penalty is a dead ball, and takes no random number.
    SSRTapSetup setup = {.chanceType = 4, .level = 1, .careerApps = 5, .careerGoals = 30};
    ssr_tapball_reset(&ball, &setup, &random);
    CHECK(ball.type == SSR_TAPBALL_DEAD && ball.x == 320 && ball.y == 810 && random.seed == SSR_RANDOM_START);

    // The first chance of the first match is a dead ball, after one number.
    setup = (SSRTapSetup){.level = 1, .careerApps = 0, .chances = 1};
    ssr_tapball_reset(&ball, &setup, &random);
    CHECK(ball.type == SSR_TAPBALL_DEAD && random.seed != SSR_RANDOM_START);

    // A new player gets a rolling ball.
    setup = (SSRTapSetup){.level = 1, .careerApps = 3, .chances = 2, .careerGoals = 4, .careerPasses = 6};
    ssr_tapball_reset(&ball, &setup, &random);
    CHECK(ball.type == SSR_TAPBALL_ROLLING && (ball.x == 140 || ball.x == 500));
    CHECK(near(fabsf(ball.xVelocity), 1.154f) && ball.xVelocity == ball.rollVelocity);

    // A header comes from a side, high, and far away.
    setup = (SSRTapSetup){.header = 1, .level = 1, .careerApps = 3};
    ssr_tapball_reset(&ball, &setup, &random);
    CHECK(ball.type == SSR_TAPBALL_HEADED && ball.depth == 0.65f && (ball.x == 0 || ball.x == 640));
    CHECK(ball.z >= 450 && ball.z <= 550 && ball.zVelocity == 4.5f);

    // A player with goals gets a rolling or a bouncing ball.
    int types[4] = {0};
    for (int i = 0; i < 40; i++) {
        setup = (SSRTapSetup){.level = 3, .careerApps = 9, .careerGoals = 20, .careerPasses = 20};
        ssr_tapball_reset(&ball, &setup, &random);
        types[ball.type]++;
    }
    CHECK(types[SSR_TAPBALL_ROLLING] > 0 && types[SSR_TAPBALL_BOUNCING] > 0 && types[SSR_TAPBALL_DEAD] == 0);
}

static void test_rolls_and_runs_out(void) {
    SSRTapBall ball;
    SSRRandom random = {SSR_RANDOM_START};
    SSRTapSetup setup = {.level = 1, .careerApps = 3};
    // Find a ball that rolls from the left.
    do {
        ssr_tapball_reset(&ball, &setup, &random);
    } while (ball.x != 140);
    SSRTapTouch none = {0};
    int beep = 0;
    CHECK(ssr_tapball_update(&ball, 1000, &none, 0, &beep) == SSR_TAPBALL_NONE);
    CHECK(near(ball.x, 140 + 2 * 1.154f * 0.997f) && ball.timer == 1000);
    // It goes off the right side of the screen: a dud.
    SSRTapBallEvent event = SSR_TAPBALL_NONE;
    int ticks = 0;
    while (event == SSR_TAPBALL_NONE && ticks < 2000) {
        event = ssr_tapball_update(&ball, 1000 + ticks, &none, 0, &beep);
        ticks++;
    }
    CHECK(event == SSR_TAPBALL_DUD && ball.x > 740);
}

static void test_counts_down_and_beeps(void) {
    SSRTapBall ball;
    SSRRandom random = {SSR_RANDOM_START};
    SSRTapSetup setup = {.level = 20, .careerApps = 3};
    do {
        ssr_tapball_reset(&ball, &setup, &random);
    } while (ball.x != 500);
    // The ball rolls to the left, so its speeds are not more than 0. Make it
    // roll slowly to the right, so that the time can run out.
    ball.xVelocity = ball.rollVelocity = 0.01f;
    SSRTapTouch none = {0};
    int beep = 0, beeps = 0;
    SSRTapBallEvent event = SSR_TAPBALL_NONE;
    for (int now = 1; now < 10000 && event == SSR_TAPBALL_NONE; now += 16) {
        event = ssr_tapball_update(&ball, now, &none, 0, &beep);
        beeps += beep;
    }
    CHECK(event == SSR_TAPBALL_DUD && beeps == 3);

    // A dead ball never runs out of time.
    setup = (SSRTapSetup){.chanceType = 3, .level = 20};
    ssr_tapball_reset(&ball, &setup, &random);
    for (int now = 1; now < 30000; now += 16) CHECK(ssr_tapball_update(&ball, now, &none, 0, &beep) == SSR_TAPBALL_NONE);
    CHECK(ssr_tapball_time_left(&ball, 30000) == 0);
}

static void test_kicks_on_the_35th_tick_after_a_tap(void) {
    SSRTapBall ball;
    SSRRandom random = {SSR_RANDOM_START};
    SSRTapSetup setup = {.chanceType = 3, .level = 1};
    ssr_tapball_reset(&ball, &setup, &random);
    // A tap outside the ball does nothing.
    SSRTapTouch far = {.x = 320, .y = 500, .down = 1, .hit = 1};
    CHECK(ssr_tapball_update(&ball, 0, &far, 0, NULL) == SSR_TAPBALL_NONE);
    // A tap below the right of the centre.
    SSRTapTouch tap = {.x = 360, .y = 840, .down = 1, .hit = 1};
    CHECK(ssr_tapball_update(&ball, 16, &tap, 0, NULL) == SSR_TAPBALL_TAPPED);
    SSRTapTouch none = {0};
    int ticks = 0;
    SSRTapBallEvent event = SSR_TAPBALL_NONE;
    while (event == SSR_TAPBALL_NONE && ticks < 100) {
        event = ssr_tapball_update(&ball, 32 + ticks * 16, &none, 0, NULL);
        ticks++;
    }
    CHECK(event == SSR_TAPBALL_KICK && ticks == 35);
    CHECK(ball.x == 320 && ball.y == 810);
    float direction, power, dip, curl, height;
    int header;
    ssr_tapball_kick(&ball, 270, 80, 50, &direction, &power, &dip, &curl, &height, &header);
    float q = 1 - 50.0f / 130.0f;
    CHECK(direction == 270 && !header && height == 0);
    CHECK(near(power, 80 * (0.75f + 0.25f * q)));
    CHECK(near(dip, 30 * 7.7e-4f) && near(curl, 40 * -(50 * 2e-5f + 0.003f)));
    // The ball does nothing after the kick.
    CHECK(ssr_tapball_update(&ball, 2000, &tap, 0, NULL) == SSR_TAPBALL_NONE);
}

static void test_control_boots_push_the_ball(void) {
    SSRTapBall ball;
    SSRRandom random = {SSR_RANDOM_START};
    SSRTapSetup setup = {.level = 1, .careerApps = 3};
    do {
        ssr_tapball_reset(&ball, &setup, &random);
    } while (ball.x != 140);
    // A finger above the ball pushes it down: zvel goes below 0.
    SSRTapTouch hold = {.x = 150, .y = 700, .down = 1, .hit = 0};
    ssr_tapball_update(&ball, 1000, &hold, 1, NULL);
    CHECK(ball.controlCount == 1 && ball.zVelocity < 0 && ball.touching);
    // Not again within 500 ms.
    ssr_tapball_update(&ball, 1200, &hold, 1, NULL);
    CHECK(ball.controlCount == 1);
}

int main(void) {
    test_gives_the_time_and_the_speed();
    test_chooses_the_ball();
    test_rolls_and_runs_out();
    test_counts_down_and_beeps();
    test_kicks_on_the_35th_tick_after_a_tap();
    test_control_boots_push_the_ball();
    return TEST_RESULT();
}
