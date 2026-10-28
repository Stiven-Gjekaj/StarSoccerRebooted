// Tests ssr_training with drills that the test sets up and shots that the
// test kicks.

#include <math.h>
#include <string.h>

#include "ssr_test.h"
#include "ssr_training.h"

static float distance_check(float ax, float ay, float bx, float by) {
    return sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

static const SSRTrainingPlayer PLAYER = {.power = 20, .technique = 20, .pace = 20, .starLevel = 1};

// Ends the instruction with a touch, then lets go.
static int start(SSRTraining *t, int now, SSRRandom *random) {
    SSRTapTouch touch = {320, 900, 1, 1};
    ssr_training_update(t, now += 16, &touch, random);
    SSRTapTouch none = {0};
    ssr_training_update(t, now += 16, &none, random);
    return now;
}

// Kicks the ball at the heading, as a tap would.
static void kick(SSRTraining *t, float direction, float power) {
    SSRChance *c = &t->chance;
    ssr_ball_kick(&c->ball, direction, power, 0, 0, 0, 0);
    c->kicking = 2;
    c->state = SSR_CHANCE_FLIGHT;
}

// Runs until the drill ends, or a new attempt starts with the lives.
static int run(SSRTraining *t, int now, int lives, SSRRandom *random) {
    SSRTapTouch none = {0};
    for (int i = 0; i < 2000 && !t->over && !(t->lives == lives && t->success == -1); i++) {
        ssr_training_update(t, now += 16, &none, random);
    }
    return now;
}

static void test_the_technique_gate(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    ssr_training_start(&t, 2, 1, 3, 1, &PLAYER, &random);
    // Two poles 80 apart at y 625, with the gate line between them.
    CHECK(t.objectCount == 3 && t.objects[0].type == SSR_OBJECT_POLE && t.objects[2].type == SSR_OBJECT_LINE);
    CHECK(t.objects[1].x - t.objects[0].x == 80 && t.objects[0].y == 625 && t.objects[2].tall);
    CHECK(t.objects[0].x >= 270 && t.objects[0].x <= 290);
    CHECK(t.chance.ball.x == 320 && t.chance.ball.y == 800 && t.instruction && t.lives == 3);
    // The same seed gives the same gate.
    SSRRandom again = {SSR_RANDOM_START};
    SSRTraining u;
    ssr_training_start(&u, 2, 1, 3, 1, &PLAYER, &again);
    CHECK(u.objects[0].x == t.objects[0].x);

    // A shot into a pole loses a life when the ball has faded.
    int now = start(&t, 1000, &random);
    CHECK(!t.instruction);
    float gate = (t.objects[0].x + t.objects[1].x) / 2;
    float toPole = (float)(atan2(625.0 - 800.0, (double)(t.objects[0].x - 320.0f)) * 57.29578);
    kick(&t, toPole + 360, 30);
    SSRTapTouch none = {0};
    for (int i = 0; i < 300 && t.chance.ball.alive; i++) ssr_training_update(&t, now += 16, &none, &random);
    CHECK(strcmp(t.objects[0].colour, "FF0000") == 0 && t.chance.ball.y > 600 && t.lives == 3);
    now = run(&t, now, 2, &random);
    CHECK(t.lives == 2 && !t.over && t.chance.ball.y == 800 && strcmp(t.objects[0].colour, "FFFF00") == 0);
    // A shot through the gate is a success.
    float through = (float)(atan2(625.0 - 800.0, (double)(gate - 320.0f)) * 57.29578);
    kick(&t, through + 360, 30);
    run(&t, now, -1, &random);
    CHECK(t.over == 1 && t.lives == 2 && ssr_training_lines(&t) == 0);
}

static void test_the_vision_dummy(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    ssr_training_start(&t, 6, 1, 3, 5, &PLAYER, &random);
    const SSRTrainingObject *dummy = &t.objects[0];
    CHECK(t.objectCount == 1 && dummy->type == SSR_OBJECT_DUMMY && dummy->y == 523);
    CHECK(t.chance.ball.x == 320 && t.chance.ball.y == 667);
    int now = start(&t, 1000, &random);
    float at = (float)(atan2((double)(dummy->y - 667), (double)(dummy->x - 320)) * 57.29578);
    kick(&t, at + 360, 30);
    run(&t, now, -1, &random);
    CHECK(t.over == 1 && !t.objects[0].alive);
}

static void test_the_set_piece_wall(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    ssr_training_start(&t, 3, 1, 3, 2, &PLAYER, &random);
    // One dummy 201.6 from the ball, on the way to the goal.
    const SSRTrainingObject *d = &t.objects[0];
    float bx = t.chance.ball.x, by = t.chance.ball.y;
    CHECK(t.objectCount == 1 && by == 408 && bx >= 170 && bx <= 470);
    CHECK(fabsf(distance_check(d->x, d->y, bx, by) - 201.6f) < 0.01f);
    float side = (d->x - bx) * (128 - by) - (d->y - by) * (320 - bx);
    CHECK(fabsf(side) < 1);
    CHECK(t.chance.players.count == 0);
}

static void test_the_power_keeper(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    ssr_training_start(&t, 4, 1, 3, 3, &PLAYER, &random);
    CHECK(t.chance.ball.x == 320 && t.chance.ball.y == 348);
    CHECK(t.chance.players.count == 1 && t.chance.players.list[0].type == SSR_PLAYER_KEEPER);
    CHECK(t.chance.players.list[0].y == 143 && t.chance.players.list[0].strength == 10);
}

// Counts the players of a drill of each type.
static int count_players(const SSRTraining *t, SSRPlayerType type, int inWall) {
    int n = 0;
    for (int i = 0; i < t->chance.players.count; i++) {
        const SSRMatchPlayer *p = &t->chance.players.list[i];
        n += p->type == type && (inWall < 0 || p->inWall == inWall);
    }
    return n;
}

// The practice (drill 7): the first shot is in the middle with nobody in
// the way. The shot of level 5 has a keeper and a defender. The shot of
// level 10 is a free kick from beyond 20 yards, with a keeper and a wall.
// A record and a star level of 3 make each level two to three steps
// harder.
static void test_the_practice(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    ssr_training_start(&t, 7, 1, 3, 0, &PLAYER, &random);
    float line = ssr_goal(1).line;
    CHECK(t.training == 7 && t.chance.ball.x == 320 && t.chance.ball.y == (float)(int)(line + 12 * 15));
    CHECK(t.chance.players.count == 0 && t.windSpeed == 0);
    ssr_training_start(&t, 7, 5, 3, 0, &PLAYER, &random);
    CHECK(count_players(&t, SSR_PLAYER_KEEPER, -1) == 1 && count_players(&t, SSR_PLAYER_DEFENDER, 0) == 1);
    CHECK(t.chance.info.chanceType == 5);
    ssr_training_start(&t, 7, 10, 3, 0, &PLAYER, &random);
    CHECK(count_players(&t, SSR_PLAYER_KEEPER, -1) == 1 && count_players(&t, SSR_PLAYER_DEFENDER, 1) == 2);
    CHECK(t.chance.info.chanceType == 3 && t.chance.ball.y == (float)(int)(line + 21 * 15));
    SSRTrainingPlayer star = PLAYER;
    star.hasClub = 1;
    star.practiceRecord = 4;
    star.starLevel = 3;
    ssr_training_start(&t, 7, 10, 3, 0, &star, &random);
    CHECK(count_players(&t, SSR_PLAYER_DEFENDER, 1) == 3 && t.windSpeed == 1.0f);
}

static void test_the_dribble_gate(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRTraining t;
    // Type 5 is the dribble drill 8 in 4.32.
    ssr_training_start(&t, 5, 1, 3, 4, &PLAYER, &random);
    CHECK(t.training == 8 && t.chance.state == SSR_CHANCE_DRIBBLE);
    CHECK(t.objects[0].type == SSR_OBJECT_CONE && t.objects[0].x == 270 && t.objects[0].y == 610);
    CHECK(t.objects[1].type == SSR_OBJECT_CONE && t.objects[1].x == 370 && ssr_training_lines(&t) == 2);
    CHECK(t.chance.ball.x == 320 && t.chance.ball.y == 710);
    // Up through the gate, then over the finish line at y 510.
    int now = start(&t, 1000, &random);
    t.chance.dribbling = 1;
    t.chance.dribbleY = -6;
    SSRTapTouch none = {0};
    for (int i = 0; i < 300 && t.success == -1; i++) {
        t.chance.dribbleY = -6;
        ssr_training_update(&t, now += 16, &none, &random);
    }
    CHECK(t.success == 1 && t.lives == 3);
    run(&t, now, -1, &random);
    CHECK(t.over == 1);

    // A ball off the screen ends the drill at once.
    ssr_training_start(&t, 8, 1, 3, 4, &PLAYER, &random);
    now = start(&t, 1000, &random);
    t.chance.dribbling = 1;
    t.chance.dribbleX = 40;
    for (int i = 0; i < 300 && t.success == -1; i++) ssr_training_update(&t, now += 16, &none, &random);
    CHECK(t.success == 0 && t.lives == 0);
    run(&t, now, -1, &random);
    CHECK(t.over == -1);
}

static void test_the_skills(void) {
    CHECK(ssr_training_skill(2) == 0 && ssr_training_skill(3) == 1 && ssr_training_skill(4) == 2);
    CHECK(ssr_training_skill(8) == 3 && ssr_training_skill(6) == 4 && ssr_training_skill(7) == -1);
}

int main(void) {
    test_the_technique_gate();
    test_the_practice();
    test_the_vision_dummy();
    test_the_set_piece_wall();
    test_the_power_keeper();
    test_the_dribble_gate();
    test_the_skills();
    return TEST_RESULT();
}
