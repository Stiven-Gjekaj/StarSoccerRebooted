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
    c.relationPartner = 40.5f;
    c.relationSponsors = 22;
    c.spentTime = 2;
    c.sponsors[3] = 9;
    c.items[0] = 120;
    c.vehicles[19] = 10500;
    c.properties[4] = 1;
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
    CHECK(back.relationPartner == 40.5f && back.relationSponsors == 22 && back.spentTime == 2);
    CHECK(back.lastSpendTimePartner == 28 && back.lastSpendTimeFriends == 28);
    CHECK(back.sponsors[3] == 9 && back.sponsors[0] == 0);
    CHECK(back.items[0] == 120 && back.vehicles[19] == 10500 && back.properties[4] == 1);
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

static void test_changes_the_relations(void) {
    SSRCareer c;
    ssr_career_create(&c, "A", "B", 0, 0, 62, 1, 1, 1);
    // A new career has no partner and no sponsors.
    CHECK(c.relationPartner == 0 && c.relationSponsors == 0 && ssr_career_count_sponsors(&c) == 0);
    ssr_career_update_relation(&c, SSR_RELATION_BOSS, 40);
    CHECK(c.relationBoss == 100);
    ssr_career_update_relation(&c, SSR_RELATION_FANS_INT, -5);
    CHECK(c.relationFansInt == 1);
    // A change to no partner still gives the minimum.
    ssr_career_update_relation(&c, SSR_RELATION_PARTNER, 0);
    CHECK(ssr_career_relation(&c, SSR_RELATION_PARTNER) == 1);
    CHECK(ssr_career_relation(&c, (SSRRelation)9) == 0);
    c.sponsors[0] = 5;
    c.sponsors[7] = 1;
    CHECK(ssr_career_count_sponsors(&c) == 2);
    // The club relations and the sponsors count as whole numbers. The
    // partner counts two times, with its fraction.
    c.relationBoss = 60.9f;
    c.relationTeam = 30.5f;
    c.relationFans = 10.2f;
    c.relationPartner = 20.25f;
    c.relationSponsors = 6.9f;
    CHECK(fabsf(ssr_career_happiness(&c) - (60 + 30 + 10 + 40.5f + 6) / 6.0f) < 1e-5f);
    // Each owned product is a sixtieth of the lifestyle, also when it is
    // broken.
    CHECK(ssr_career_lifestyle(&c) == 0);
    c.items[1] = 1;
    c.vehicles[0] = 10200;
    c.properties[19] = 3;
    CHECK(fabsf(ssr_career_lifestyle(&c) - 5.0f) < 1e-5f);
}

int main(void) {
    test_changes_the_relations();
    test_names_the_player_type();
    test_updates_a_rating();
    test_sums_a_skill();
    test_creates_a_career();
    test_saves_and_loads_a_career();
    return TEST_RESULT();
}
