// Tests ssr_career: a new career and its save file.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssr_career.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static void test_creates_a_career(void) {
    {
        SSRCareer fresh;
        ssr_career_create(&fresh, "A", "B", 0, 0, 62, 1, 1, 1);
        // The nation does not know the player yet.
        CHECK(fresh.debut == 1 && fresh.relationBossInt == 0);
    }
    SSRCareer c;
    ssr_career_create(&c, " Billy ", "", 1, 0, 62, 1, 1, 0);
    CHECK(strcmp(c.fullName, "Billy") == 0 && strcmp(c.shortName, "Billy") == 0);
    CHECK(c.gender == 1 && c.clubNationId == 62 && c.wage == 1 && c.goalBonus == 1);
    CHECK(c.bank == 15 && c.relationBoss == 75 && c.energy == 100);
    CHECK(c.nrg[0] == 1 && c.nrg[2] == 1 && c.nrg[3] == 0 && c.ratings[9] == 5);
    CHECK(c.sdate == 1 && ssr_career_year(&c) == 1 && ssr_career_week(&c) == 1);
    c.sdate = 364 + 7 * 3 + 2;
    CHECK(ssr_career_year(&c) == 2 && ssr_career_week(&c) == 4);
}

static void test_saves_and_loads_a_career(void) {
    SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 1, 62, 1, 1, 0);
    c.clubId = 1234;
    c.nationId = 62;
    c.technique[0] = 3;
    c.power[99] = 7;
    c.ratings[4] = 6.5f;
    c.apps = 12;
    c.goals = 5;
    c.assists = 3;
    c.passes = 40;
    c.starRating = 250;
    c.seasonApps = 9;
    c.seasonStarMan = 2;
    c.seasonChances = 30;
    c.intGoals = 4;
    c.seasonTotalRating = 61.5f;
    c.intTotalRating = 14;
    ssr_career_add_form(c.form, 6);
    ssr_career_add_form(c.form, 8);
    c.intForm[0] = 3;
    c.potyAwards = 2;
    c.relationBossInt = 60;
    c.relationFansInt = 12.5f;
    c.debut = 0;
    c.intNotPickedMessage = 400;
    c.wonMajorAward = 1;
    snprintf(c.headline, sizeof(c.headline), "Line one\nA \\ two");
    char path[1200];
    snprintf(path, sizeof(path), "%s/slot0.txt", ssr_test_directory("career_save"));
    CHECK(ssr_career_save(&c, path));
    CHECK(ssr_career_load(&back, path));
    CHECK(strcmp(back.fullName, "The Kid") == 0 && back.clubId == 1234 && back.hardMode == 1);
    CHECK(back.technique[0] == 3 && back.power[99] == 7 && fabsf(back.ratings[4] - 6.5f) < 1e-6f);
    CHECK(back.bank == 15 && back.relationFans == 75 && back.nrg[1] == 1);
    CHECK(strcmp(back.headline, c.headline) == 0 && strcmp(back.gameVersion, "v2.32") == 0);
    CHECK(back.apps == 12 && back.goals == 5 && back.assists == 3 && back.passes == 40);
    CHECK(back.starRating == 250 && ssr_career_star_level(&back) == 3);
    CHECK(back.seasonApps == 9 && back.seasonStarMan == 2 && back.seasonChances == 30 && back.intGoals == 4);
    CHECK(back.seasonTotalRating == 61.5f && back.intTotalRating == 14);
    // The newest form is last.
    CHECK(back.form[3] == 6 && back.form[4] == 8 && back.form[0] == 0 && back.intForm[0] == 3);
    CHECK(back.potyAwards == 2 && back.wonMajorAward == 1);
    CHECK(back.relationBossInt == 60 && back.relationFansInt == 12.5f && back.debut == 0);
    CHECK(back.intNotPickedMessage == 400);
    // The new season starts the counts again.
    back.energy = 20;
    back.relationBoss = 50;
    ssr_career_reset_season(&back);
    CHECK(back.relationBoss == 45 && back.relationBossInt == 54);
    CHECK(back.seasonApps == 0 && back.seasonTotalRating == 0 && back.wonMajorAward == 0 && back.energy == 100);
    CHECK(back.apps == 12 && back.potyAwards == 2 && back.form[4] == 8);
    back.sdate = 1;
    CHECK(ssr_career_age(&back) == 16);
    back.sdate = 365;
    CHECK(ssr_career_age(&back) == 17);
    snprintf(path, sizeof(path), "%s/none.txt", ssr_test_directory("career_save"));
    CHECK(!ssr_career_load(&back, path));
}

static void test_sums_a_skill(void) {
    int levels[SSR_CAREER_SKILL_LEVELS] = {0};
    CHECK(ssr_career_skill(levels) == 1);
    levels[0] = 30;
    levels[5] = 12;
    CHECK(ssr_career_skill(levels) == 42);
    levels[9] = 80;
    CHECK(ssr_career_skill(levels) == 100);
}

static void test_updates_a_rating(void) {
    SSRCareer c;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    // A rating of 5 moves by half of the step.
    CHECK(c.ratings[2] == 5 && ssr_career_update_rating(&c, 2, 1) == 0.75f && c.ratings[2] == 5.75f);
    // 5.75 is level 5 too. A fault of short passing is -2 * 0.5.
    CHECK(ssr_career_update_rating(&c, 0, 0) == -1 && c.ratings[0] == 4);
    // Level 4 moves by 0.6 of the step.
    CHECK(fabsf(ssr_career_update_rating(&c, 0, 1) - 0.3f) < 1e-6f);
    // A rating stays from 1 to 10.
    c.ratings[7] = 1.2f;
    ssr_career_update_rating(&c, 7, 0);
    CHECK(c.ratings[7] == 1);
    c.ratings[8] = 9.95f;
    ssr_career_update_rating(&c, 8, 1);
    CHECK(c.ratings[8] == 10);
    CHECK(ssr_career_update_rating(&c, 9, 1) == 0);
}

static void test_names_the_player_type(void) {
    SSRCareer c;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.sdate = 1;
    // All under 7: the passes against the shots.
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_Forward") == 0);
    c.ratings[0] = 6;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_Midfielder") == 0);
    // One top rating names its skill.
    c.ratings[4] = 7.5f;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_Heading") == 0);
    // Finishing and long shots at the same level.
    c.ratings[4] = 5;
    c.ratings[2] = 8;
    c.ratings[3] = 8;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_FinishingLongShots") == 0);
    c.ratings[4] = 8;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_ShootingHeading") == 0);
    // Crossing then dribbling.
    for (int i = 0; i < 8; i++) c.ratings[i] = 5;
    c.ratings[5] = 9;
    c.ratings[7] = 9;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_CrossingDribbling") == 0);
    // Near the max, and young.
    for (int i = 0; i < 8; i++) c.ratings[i] = 9;
    CHECK(strcmp(ssr_career_player_type(&c), "playerdesc_NearMaxRatingYoung") == 0);
    CHECK(ssr_career_player_type_index("playerdesc_NearMaxRatingYoung") == 20);
    CHECK(ssr_career_player_type_index("playerdesc_Midfielder") == 0);
    CHECK(ssr_career_player_type_index("playerdesc_NoRating") == -1);
}

int main(void) {
    test_names_the_player_type();
    test_updates_a_rating();
    test_sums_a_skill();
    test_creates_a_career();
    test_saves_and_loads_a_career();
    return TEST_RESULT();
}
