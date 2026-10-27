// Tests ssr_chance with chances that the test makes and plays.

#include <math.h>
#include <string.h>

#include "ssr_chance.h"
#include "ssr_test.h"

static SSRChanceSetup setup_for(int type) {
    SSRChanceSetup s = {.type = type,
                        .minute = 30,
                        .teamStrength = 60,
                        .opponentStrength = 60,
                        .power = 60,
                        .technique = 50,
                        .vision = 50,
                        .level = 3,
                        .starLevel = 2,
                        .careerApps = 10,
                        .chances = 3,
                        .careerGoals = 20,
                        .careerPasses = 20};
    return s;
}

// A dead tap ball in the middle, so that a tap at (320, 810) hits it.
static void still_tap_ball(SSRChance *c) {
    c->tap.x = 320;
    c->tap.z = 0;
    c->tap.y = 810;
    c->tap.xVelocity = c->tap.zVelocity = c->tap.rollVelocity = 0;
    c->tap.depth = c->tap.depthAccel = 1;
}

// Aims from the ball to (dragX, dragY), taps the big ball at its centre,
// and plays the chance to its end. Gives the number of ticks.
static int play(SSRChance *c, float dragX, float dragY, SSRRandom *random) {
    int now = 1000, ticks = 0;
    SSRTapTouch touch = {c->ball.x, c->ball.y, 1, 1};
    ssr_chance_update(c, now += 16, &touch, random);
    touch.hit = 0;
    touch.x = dragX;
    touch.y = dragY;
    for (int i = 0; i < 10; i++) ssr_chance_update(c, now += 16, &touch, random);
    touch.down = 0;
    ssr_chance_update(c, now += 16, &touch, random);
    if (c->state != SSR_CHANCE_TAP) return -1;
    SSRTapTouch tap = {320, 810, 1, 1};
    ssr_chance_update(c, now += 16, &tap, random);
    SSRTapTouch none = {0};
    while (c->state != SSR_CHANCE_OVER && ticks < 2000) {
        ssr_chance_update(c, now += 16, &none, random);
        ticks++;
    }
    return ticks;
}

static void test_places_the_ball(void) {
    SSRRandom random = {SSR_RANDOM_START};
    float x, y, z;
    ssr_chance_ball_start(4, 1, 30, &random, &x, &y, &z);
    CHECK(x == 320 && y == 308 && z == 0);
    ssr_chance_ball_start(7, 1, 30, &random, &x, &y, &z);
    CHECK(x == 538 && y == 830);
    ssr_chance_ball_start(7, 1, 31, &random, &x, &y, &z);
    CHECK(x == 102);
    ssr_chance_ball_start(10, 1, 30, &random, &x, &y, &z);
    CHECK(z == 57.6f && x >= 180 && x <= 460 && y >= 233 && y <= 368);
    for (int i = 0; i < 50; i++) {
        ssr_chance_ball_start(2, 0, 30, &random, &x, &y, &z);
        CHECK(x >= 140 && x <= 320 && y >= 323 && y <= 758 && ((int)y - 128) % 15 == 0);
    }
}

static void test_sets_up_the_players(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(4);
    ssr_chance_start(&c, &s, &random);
    int keepers = 0, defenders = 0, attackers = 0;
    for (int i = 0; i < c.players.count; i++) {
        keepers += c.players.list[i].type == SSR_PLAYER_KEEPER;
        defenders += c.players.list[i].type == SSR_PLAYER_DEFENDER;
        attackers += c.players.list[i].type == SSR_PLAYER_ATTACKER;
    }
    CHECK(keepers == 1 && defenders <= 8 && attackers <= 4 && defenders + attackers > 6);
    CHECK(c.ball.x == 320 && c.ball.y == 308 && c.tap.type == SSR_TAPBALL_DEAD && c.state == SSR_CHANCE_AIM);

    // The same seed gives the same chance.
    SSRRandom again = {SSR_RANDOM_START};
    SSRChance d;
    ssr_chance_start(&d, &s, &again);
    CHECK(d.players.count == c.players.count && d.players.list[3].x == c.players.list[3].x);
    CHECK(d.info.pitchSide == c.info.pitchSide);
}

static void test_shoots_a_penalty(void) {
    int saves = 0;
    for (int seed = 0; seed < 20; seed++) {
        SSRRandom random = {seed * 7919};
        SSRChance c;
        SSRChanceSetup s = setup_for(4);
        ssr_chance_start(&c, &s, &random);
        CHECK(c.info.pitchSide == 1);
        // Drag straight down: the shot goes straight up at the goal.
        int ticks = play(&c, 320, 308 + 70, &random);
        CHECK(ticks > 0 && c.state == SSR_CHANCE_OVER && c.ball.kicks >= 1);
        CHECK(c.result == SSR_RESULT_GOAL || c.result == SSR_RESULT_ASSIST || c.result == SSR_RESULT_SAVED ||
              c.result == SSR_RESULT_MISSED);
        // A save is on target. A goal is not.
        if (c.result == SSR_RESULT_SAVED) saves++;
        CHECK(c.result != SSR_RESULT_SAVED || c.shotsOnTarget == 1);
        CHECK(c.result != SSR_RESULT_GOAL || c.ball.postHit || c.shotsOnTarget == 0);
    }
    CHECK(saves > 0);
}

static void test_scores_in_the_far_corner(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(4);
    // At an even minute the keeper dives to the left.
    s.minute = 30;
    ssr_chance_start(&c, &s, &random);
    // Drag down and to the left: the shot goes up and to the right.
    play(&c, 320 - 25, 308 + 70, &random);
    CHECK(c.result == SSR_RESULT_GOAL && c.ball.inGoal && c.ball.x > 320);
}

static void test_misses_a_wide_shot(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(4);
    ssr_chance_start(&c, &s, &random);
    // Drag down and to the right: the shot goes far to the left.
    play(&c, 320 + 60, 308 + 40, &random);
    CHECK(c.result == SSR_RESULT_MISSED && !c.ball.inGoal);
}

static void test_passes_to_a_team_mate(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(1);
    ssr_chance_start(&c, &s, &random);
    still_tap_ball(&c);
    ssr_players_clear(&c.players);
    SSRMatchPlayer *mate = ssr_players_add_attacker(&c.players, &c.info, 60, 0, &random);
    mate->x = c.ball.x;
    mate->y = c.ball.y - 200;
    // A defender far up the pitch keeps the team-mate onside.
    SSRMatchPlayer *d = ssr_players_add_defender(&c.players, &c.info, 60, 0, 0, 0, &random);
    d->x = 620;
    d->y = 200;
    // A soft pass straight up. The team-mate runs to the ball.
    play(&c, c.ball.x, c.ball.y + 25, &random);
    CHECK(c.result == SSR_RESULT_PASS);
    CHECK(c.kickYards > 3 && c.kickYards < 14);

    // With no defender, the line is the ball: the team-mate is offside.
    ssr_chance_start(&c, &s, &random);
    still_tap_ball(&c);
    ssr_players_clear(&c.players);
    mate = ssr_players_add_attacker(&c.players, &c.info, 60, 0, &random);
    mate->x = c.ball.x;
    mate->y = c.ball.y - 200;
    play(&c, c.ball.x, c.ball.y + 25, &random);
    CHECK(c.result == SSR_RESULT_OFFSIDE);
}

static void test_a_defender_takes_the_ball(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(1);
    ssr_chance_start(&c, &s, &random);
    still_tap_ball(&c);
    ssr_players_clear(&c.players);
    SSRMatchPlayer *d = ssr_players_add_defender(&c.players, &c.info, 60, 0, 0, 0, &random);
    d->x = c.ball.x;
    d->y = c.ball.y - 150;
    play(&c, c.ball.x, c.ball.y + 25, &random);
    CHECK(c.result == SSR_RESULT_INTERCEPTED);
}

static void test_a_defender_closes_down_the_aim(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(2);
    ssr_chance_start(&c, &s, &random);
    ssr_players_clear(&c.players);
    SSRMatchPlayer *d = ssr_players_add_defender(&c.players, &c.info, 100, 0, 0, 0, &random);
    d->x = c.ball.x + 30;
    d->y = c.ball.y;
    int now = 1000;
    SSRTapTouch touch = {c.ball.x, c.ball.y, 1, 1};
    ssr_chance_update(&c, now += 16, &touch, &random);
    touch.hit = 0;
    touch.y += 60;
    for (int i = 0; i < 200 && c.state == SSR_CHANCE_AIM; i++) ssr_chance_update(&c, now += 16, &touch, &random);
    CHECK(c.state == SSR_CHANCE_OVER && c.result == SSR_RESULT_LOST);
}

static void test_a_short_tap_cancels_the_aim(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(4);
    ssr_chance_start(&c, &s, &random);
    SSRTapTouch touch = {c.ball.x, c.ball.y, 1, 1};
    ssr_chance_update(&c, 1000, &touch, &random);
    CHECK(c.kicking == 1);
    touch.down = touch.hit = 0;
    ssr_chance_update(&c, 1100, &touch, &random);
    CHECK(c.kicking == 0 && c.state == SSR_CHANCE_AIM);
}

// Swipes up from (x, 900): each swipe pushes the ball up the screen.
static void swipe_up(SSRChance *c, int *now, int count, SSRRandom *random) {
    for (int k = 0; k < count && c->state == SSR_CHANCE_DRIBBLE; k++) {
        SSRTapTouch down = {320, 900, 1, 1}, up = {320, 720, 0, 0};
        ssr_chance_update(c, *now += 16, &down, random);
        ssr_chance_update(c, *now += 16, &up, random);
        SSRTapTouch none = {0};
        for (int i = 0; i < 20 && c->state == SSR_CHANCE_DRIBBLE; i++) ssr_chance_update(c, *now += 16, &none, random);
    }
}

static void test_dribbles_past_the_defenders(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(11);
    s.pace = 50;
    ssr_chance_start(&c, &s, &random);
    CHECK(c.state == SSR_CHANCE_DRIBBLE && c.ball.x == 320 && c.ball.y == 850 && c.players.count == 4);
    // Nothing moves before the first touch.
    SSRTapTouch none = {0};
    int now = 1000;
    for (int i = 0; i < 30; i++) ssr_chance_update(&c, now += 16, &none, &random);
    CHECK(c.ball.y == 850 && !c.dribbling);
    // With the defenders out of the way, the ball crosses the line.
    for (int i = 0; i < c.players.count; i++) c.players.list[i].x = -1000;
    swipe_up(&c, &now, 40, &random);
    CHECK(c.result == SSR_RESULT_DRIBBLED && c.ball.y <= 200);

    // A defender in the way takes the ball.
    ssr_chance_start(&c, &s, &random);
    for (int i = 0; i < c.players.count; i++) c.players.list[i].x = -1000;
    c.players.list[0].x = 320;
    c.players.list[0].y = 600;
    swipe_up(&c, &now, 40, &random);
    CHECK(c.result == SSR_RESULT_INTERCEPTED);
}

// The captain drags a run for a team-mate before the aim.
static void test_the_captain_draws_a_run(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRChance c;
    SSRChanceSetup s = setup_for(2);
    ssr_chance_start(&c, &s, &random);
    for (int i = 0; i < c.players.count; i++) CHECK(!c.players.list[i].canMakeRun);
    random.seed = SSR_RANDOM_START;
    s.captain = 1;
    ssr_chance_start(&c, &s, &random);
    SSRMatchPlayer *mate = NULL;
    for (int i = 0; i < c.players.count && !mate; i++) {
        SSRMatchPlayer *p = &c.players.list[i];
        // A team-mate away from the ball, so that the touch takes it.
        if (p->type == SSR_PLAYER_ATTACKER && fabsf(p->x - c.ball.x) + fabsf(p->y - c.ball.y) > 120) mate = p;
    }
    CHECK(mate && mate->canMakeRun);
    if (!mate) return;
    int now = 1000;
    float x = mate->x, y = mate->y;
    SSRTapTouch touch = {x + 10, y, 1, 1};
    ssr_chance_update(&c, now += 16, &touch, &random);
    CHECK(c.players.makeRun == mate->id && c.kicking == 0);
    // A drag of 300 to the right gives the longest run.
    touch.hit = 0;
    touch.x = x + 300;
    ssr_chance_update(&c, now += 16, &touch, &random);
    CHECK(mate->makeRunDistance == SSR_MAKE_RUN_MAX && mate->makeRunX == (int)(x + SSR_MAKE_RUN_MAX));
    // The release keeps the run. The team-mate has not moved yet.
    touch.down = 0;
    ssr_chance_update(&c, now += 16, &touch, &random);
    CHECK(c.players.makeRun == 0 && mate->makeRunDistance == SSR_MAKE_RUN_MAX && mate->x == x && mate->y == y);
    // A tap on the team-mate and a release on it end the run.
    SSRTapTouch again = {x, y, 1, 1};
    ssr_chance_update(&c, now += 16, &again, &random);
    again.down = again.hit = 0;
    ssr_chance_update(&c, now += 16, &again, &random);
    CHECK(mate->makeRunDistance == 0);
}

int main(void) {
    test_dribbles_past_the_defenders();
    test_places_the_ball();
    test_sets_up_the_players();
    test_shoots_a_penalty();
    test_scores_in_the_far_corner();
    test_misses_a_wide_shot();
    test_passes_to_a_team_mate();
    test_a_defender_takes_the_ball();
    test_a_defender_closes_down_the_aim();
    test_a_short_tap_cancels_the_aim();
    test_the_captain_draws_a_run();
    return TEST_RESULT();
}
