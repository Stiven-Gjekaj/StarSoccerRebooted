// Tests ssr_aftermatch with matches that the test makes.

#include <math.h>

#include "ssr_aftermatch.h"
#include "ssr_test.h"

static SSRAfterMatchFacts facts(void) {
    SSRAfterMatchFacts f = {.home = 1, .homeStrength = 60, .awayStrength = 60, .starLevel = 2};
    return f;
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static void test_rates_a_match(void) {
    SSRAfterMatch out;
    // A quiet draw: 1, then one more for a rating under 6 with no misses.
    SSRAfterMatchFacts f = facts();
    ssr_aftermatch_rate(&f, &out);
    CHECK(near(out.rating, 2) && out.result == 0 && out.outcome == 2 && !out.starMan);
    // A goal and a pass in a 2-1 win at home, with 3 chances.
    f.goals = 1;
    f.passes = 1;
    f.chances = 3;
    f.shotsOnTarget = 1;
    f.score1 = f.myScore = 2;
    f.score2 = f.opponentScore = 1;
    ssr_aftermatch_rate(&f, &out);
    // 1 + 3.2 + 1.1 - 0.175 + 0.5 = 5.625, then + 1 because misses < 2.
    CHECK(near(out.rating, 6.625f) && out.result == 1 && out.outcome == 1);
    // boss (int)1.625 = 1, team (int)(1 - 2 - 1) = -2, fans (int)1.625 = 1.
    // A goal that is not less than a score keeps them from 0 to 4.
    CHECK(out.boss == 1 && out.team == 0 && out.fans == 1);
    // A hat-trick in a win makes the star man.
    f.goals = 3;
    f.score1 = f.myScore = 3;
    ssr_aftermatch_rate(&f, &out);
    CHECK(out.rating == 10 && out.starMan && out.boss == 5);
    // A heavy loss away at a low star level.
    f = facts();
    f.home = 0;
    f.starLevel = 1;
    f.score1 = f.opponentScore = 4;
    f.chances = 3;
    ssr_aftermatch_rate(&f, &out);
    CHECK(out.result == -1 && out.outcome == 3 && out.rating == 1);
    CHECK(out.boss == -4 && out.fans == -4);
}

// The fair rating: a quiet draw is 6, a goal and a pass in a win is near
// 7.9, a hat-trick in a win makes the star man, and a heavy defeat stays
// above 3.
static void test_rates_a_match_fairly(void) {
    SSRAfterMatch out;
    SSRAfterMatchFacts f = facts();
    f.fair = 1;
    ssr_aftermatch_rate(&f, &out);
    CHECK(near(out.rating, 6) && out.result == 0 && !out.starMan);
    f.goals = 1;
    f.passes = 1;
    f.chances = 3;
    f.shotsOnTarget = 1;
    f.score1 = f.myScore = 2;
    f.score2 = f.opponentScore = 1;
    ssr_aftermatch_rate(&f, &out);
    // 6 + 1.1 + 0.35 + 0.15 - 0.12 + 0.3 = 7.78.
    CHECK(near(out.rating, 7.78f) && out.result == 1 && !out.starMan);
    f.goals = 3;
    f.score1 = f.myScore = 3;
    ssr_aftermatch_rate(&f, &out);
    CHECK(out.rating >= 9 && out.starMan);
    f = facts();
    f.fair = 1;
    f.chances = 8;
    f.score2 = f.opponentScore = 5;
    ssr_aftermatch_rate(&f, &out);
    CHECK(out.rating >= 3 && out.rating < 5 && out.result == -1);
}

static void test_work_rate_and_stars(void) {
    int light[3] = {80, 5, 5}, hard[3] = {5, 5, 80}, even[3] = {30, 30, 30};
    CHECK(ssr_aftermatch_work_rate(light) == -2 && ssr_aftermatch_work_rate(hard) == 2);
    CHECK(ssr_aftermatch_work_rate(even) == 0);
    // The star rating never falls under the floor of its level.
    CHECK(ssr_star_rating_add(150, -80) == 101 && ssr_star_rating_add(150, 60) == 210);
    CHECK(ssr_star_rating_add(1, -5) == 1);
    CHECK(ssr_energy_recovery(14, 17) == 100 && ssr_energy_recovery(3, 30) == 20);
    CHECK(ssr_energy_recovery(0, 20) == 1);
}

int main(void) {
    test_rates_a_match();
    test_rates_a_match_fairly();
    test_work_rate_and_stars();
    return TEST_RESULT();
}
