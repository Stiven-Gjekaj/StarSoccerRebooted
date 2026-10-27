// Tests ssr_players with chances that the test makes.

#include <math.h>

#include "ssr_players.h"
#include "ssr_test.h"

static int near(float a, float b) {
    return fabsf(a - b) < 0.01f;
}

static float dist(const SSRMatchPlayer *p, float x, float y) {
    return sqrtf((p->x - x) * (p->x - x) + (p->y - y) * (p->y - y));
}

static void test_places_the_keeper(void) {
    SSRPlayers players;
    SSRRandom random = {SSR_RANDOM_START};
    SSRChanceInfo chance = {.chanceType = 2, .pitchSide = 1, .startX = 320, .startY = 600};
    int high = 0;
    for (int i = 0; i < 200; i++) {
        ssr_players_clear(&players);
        SSRMatchPlayer *k = ssr_players_add_keeper(&players, &chance, 80, &random);
        CHECK(k && k->id == 1 && k->type == SSR_PLAYER_KEEPER && near(k->x, 320));
        CHECK(k->y >= 143 && k->y <= 233 && ((int)k->y - 143) % 15 == 0);
        CHECK(k->strength == 40);
        high += k->y > 143;
    }
    // One time in 8 the keeper stands further out.
    CHECK(high > 5 && high < 60);

    // A keeper of the side of the goal moves toward the ball.
    chance.pitchSide = 0;
    ssr_players_clear(&players);
    SSRMatchPlayer *k = ssr_players_add_keeper(&players, &chance, 80, &random);
    CHECK(k->x < 535 && k->x >= 445);

    chance = (SSRChanceInfo){.chanceType = 4, .pitchSide = 1, .startX = 320, .startY = 300};
    SSRRandom before = random;
    ssr_players_clear(&players);
    k = ssr_players_add_keeper(&players, &chance, 80, &random);
    CHECK(k->x == 320 && k->y == 134 && random.seed == before.seed);
    chance.chanceType = 7;
    chance.minute = 3;
    ssr_players_clear(&players);
    k = ssr_players_add_keeper(&players, &chance, 80, &random);
    CHECK(k->x == 120 && k->y == 182);
}

static void test_spreads_the_players(void) {
    SSRPlayers players;
    SSRRandom random = {SSR_RANDOM_START};
    SSRChanceInfo chance = {.chanceType = 2, .pitchSide = 1, .startX = 320, .startY = 650};
    ssr_players_clear(&players);
    ssr_players_add_keeper(&players, &chance, 60, &random);
    for (int i = 0; i < 8; i++) ssr_players_add_defender(&players, &chance, 60, 0, 0, 0, &random);
    ssr_players_add_attacker(&players, &chance, 60, 1, &random);
    ssr_players_add_attacker(&players, &chance, 60, 1, &random);
    CHECK(players.count == 11);
    for (int i = 0; i < players.count; i++) {
        const SSRMatchPlayer *p = &players.list[i];
        CHECK(p->id == i + 1);
        if (p->type == SSR_PLAYER_DEFENDER) CHECK(dist(p, chance.startX, chance.startY) >= 195);
        if (p->type == SSR_PLAYER_ATTACKER) CHECK(dist(p, chance.startX, chance.startY) >= 225);
        if (p->type == SSR_PLAYER_ATTACKER) CHECK(p->calling >= -1.5f && p->calling <= 1.5f);
        for (int j = 0; j < i && p->type != SSR_PLAYER_KEEPER; j++) {
            if (players.list[j].type != SSR_PLAYER_KEEPER) CHECK(dist(p, players.list[j].x, players.list[j].y) >= 75);
        }
    }
    // The same seed gives the same places.
    SSRPlayers again;
    SSRRandom same = {SSR_RANDOM_START};
    ssr_players_clear(&again);
    ssr_players_add_keeper(&again, &chance, 60, &same);
    for (int i = 0; i < 8; i++) ssr_players_add_defender(&again, &chance, 60, 0, 0, 0, &same);
    CHECK(again.list[5].x == players.list[5].x && again.list[5].y == players.list[5].y);

    // Players off the screen go away. The ids stay.
    ssr_players_remove_off_screen(&players, 0, 640);
    for (int i = 0; i < players.count; i++) CHECK(players.list[i].x >= 0 && players.list[i].x <= 640);
    CHECK(players.count < 11 && players.list[0].id == 1);
}

static void test_builds_a_wall(void) {
    SSRPlayers players;
    SSRRandom random = {SSR_RANDOM_START};
    SSRChanceInfo chance = {.chanceType = 3, .pitchSide = 1, .startX = 400, .startY = 500};
    ssr_players_clear(&players);
    for (int i = 0; i < 5; i++) ssr_players_add_defender(&players, &chance, 60, 1, 0, 0, &random);
    ssr_players_place_wall(&players, &chance, 400, 500);
    for (int i = 0; i < 5; i++) {
        CHECK(near(dist(&players.list[i], 400, 500), 157.5f));
        CHECK(players.list[i].y < 500);
    }
}

static void test_moves_and_offside(void) {
    SSRPlayers players;
    ssr_players_clear(&players);
    SSRChanceInfo chance = {.chanceType = 2, .pitchSide = 1, .minute = 10, .startX = 320, .startY = 600};
    SSRRandom random = {SSR_RANDOM_START};
    SSRMatchPlayer *a = ssr_players_add_defender(&players, &chance, 100, 0, 0, 0, &random);
    SSRMatchPlayer *b = ssr_players_add_defender(&players, &chance, 100, 0, 0, 0, &random);
    SSRMatchPlayer *m = ssr_players_add_attacker(&players, &chance, 100, 0, &random);
    a->x = 100;
    a->y = 300;
    b->x = 500;
    b->y = 400;
    SSRPlayerBall ball = {.x = 320, .y = 600, .alive = 1};
    CHECK(ssr_players_offside_line(&players, &chance, &ball) == 300 - 127);
    m->y = 250;
    CHECK(ssr_player_offside(m, &chance, 300 - 127));
    m->y = 350;
    CHECK(!ssr_player_offside(m, &chance, 300 - 127));
    chance.wasCorner = 1;
    m->y = 250;
    CHECK(!ssr_player_offside(m, &chance, 300 - 127));
    chance.wasCorner = 0;

    CHECK(near(ssr_player_speed(a, &ball), 2));
    ball.kicking = 1;
    CHECK(near(ssr_player_speed(a, &ball), 0.7f));
    ball.kicking = 0;

    // The nearest defender runs at the ball when the star level is high.
    chance.starLevel = 2;
    m->x = 600;
    m->y = 900;
    b->x = 330;
    b->y = 500;
    float before = dist(b, ball.x, ball.y);
    ssr_players_update(&players, &chance, &ball, 1000, &random);
    CHECK(dist(b, ball.x, ball.y) < before && b->anim == SSR_ANIM_RUN);
    // The far one stands.
    CHECK(a->xVelocity == 0 && a->yVelocity == 0 && a->anim == SSR_ANIM_STAND);
}

static void test_the_keeper_dives_at_a_penalty(void) {
    SSRPlayers players;
    SSRRandom random = {SSR_RANDOM_START};
    SSRChanceInfo chance = {.chanceType = 4, .pitchSide = 1, .minute = 20, .startX = 320, .startY = 300};
    ssr_players_clear(&players);
    SSRMatchPlayer *k = ssr_players_add_keeper(&players, &chance, 80, &random);
    SSRPlayerBall ball = {.x = 320, .y = 300, .alive = 1, .kicking = 1};
    // The keeper stands still while the player aims.
    ssr_players_update(&players, &chance, &ball, 1000, &random);
    CHECK(k->x == 320 && k->y == 134 && !ssr_player_diving(k));
    ball.kicking = 2;
    ball.power = 20;
    ball.kickDirection = 270;
    ssr_players_update(&players, &chance, &ball, 1016, &random);
    CHECK(k->facing == 2 && k->xVelocity < 0 && ssr_player_diving(k));
    CHECK(k->anim == SSR_ANIM_DIVE_GK || k->anim == SSR_ANIM_DIVE_LOW_GK);

    chance.minute = 21;
    ssr_players_clear(&players);
    k = ssr_players_add_keeper(&players, &chance, 80, &random);
    ssr_players_update(&players, &chance, &ball, 1016, &random);
    CHECK(k->facing == 0 && k->xVelocity > 0);
    // The dive ends after its frames, at one frame each 150 ms. The ball
    // is dead by then, so the keeper does not dive again.
    ball.alive = 0;
    for (int now = 1200; now < 4000; now += 16) ssr_players_update(&players, &chance, &ball, now, &random);
    CHECK(!ssr_player_diving(k) && k->z == 0);
}

int main(void) {
    test_places_the_keeper();
    test_spreads_the_players();
    test_builds_a_wall();
    test_moves_and_offside();
    test_the_keeper_dives_at_a_penalty();
    return TEST_RESULT();
}
