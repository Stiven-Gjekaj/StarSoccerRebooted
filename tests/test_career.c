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
    c.starMan = 4;
    c.chances = 21;
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
    // No shopping trip runs in a new career.
    CHECK(c.teamShopping == -1);
    c.teamShopping = 2;
    c.gambling = 36;
    c.casinoCount = 12;
    CHECK(c.expiredBoots == -1 && ssr_career_agent(&c) == 0 && ssr_career_trainer(&c) == 0);
    c.agent[0] = 51;
    c.trainer[1] = 9;
    c.trainer[2] = 0;
    c.bootId = 21;
    c.bootUses = 4;
    c.oldProBoots = 21;
    c.ballCup = 3;
    CHECK(c.showContractTip == 1 && c.captain == 0);
    c.lastNegotiations = 70;
    c.lastTransferDate = 66;
    c.captain = 3;
    snprintf(c.headline, sizeof(c.headline), "Line one\nA \\ two");
    // A new career has no news in its first week.
    CHECK(c.newsDone == 1);
    c.newsDone = 0;
    c.tipCount = 7;
    c.interviewSkill = 4;
    c.lastInterview = 90;
    snprintf(c.headlineResult, sizeof(c.headlineResult), "ARS have won the CUP!");
    snprintf(c.horses, sizeof(c.horses), "3:61.5:4:454545FF00000000#12:100:1:B0B0B0FF00000000#");
    char path[1200];
    snprintf(path, sizeof(path), "%s/slot0.txt", ssr_test_directory("career_save"));
    CHECK(ssr_career_save(&c, path));
    CHECK(ssr_career_load(&back, path));
    CHECK(strcmp(back.fullName, "The Kid") == 0 && back.clubId == 1234 && back.hardMode == 1);
    CHECK(back.technique[0] == 3 && back.power[99] == 7 && fabsf(back.ratings[4] - 6.5f) < 1e-6f);
    CHECK(back.bank == 15 && back.relationFans == 75 && back.nrg[1] == 1);
    CHECK(strcmp(back.headline, c.headline) == 0 && strcmp(back.gameVersion, "v2.32") == 0);
    CHECK(back.apps == 12 && back.goals == 5 && back.assists == 3 && back.passes == 40 && back.starMan == 4 && back.chances == 21);
    CHECK(back.starRating == 250 && ssr_career_star_level(&back) == 3);
    CHECK(back.seasonApps == 9 && back.seasonStarMan == 2 && back.seasonChances == 30 && back.intGoals == 4);
    CHECK(back.seasonTotalRating == 61.5f && back.intTotalRating == 14);
    // The newest form is last.
    CHECK(back.form[3] == 6 && back.form[4] == 8 && back.form[0] == 0 && back.intForm[0] == 3);
    CHECK(back.potyAwards == 2 && back.wonMajorAward == 1);
    CHECK(back.relationBossInt == 60 && back.relationFansInt == 12.5f && back.debut == 0);
    CHECK(back.intNotPickedMessage == 400);
    CHECK(back.interviewSkill == 4 && back.lastInterview == 90);
    CHECK(back.tipCount == 7 && back.newsDone == 0 && strcmp(back.headlineResult, "ARS have won the CUP!") == 0);
    CHECK(strcmp(back.horses, "3:61.5:4:454545FF00000000#12:100:1:B0B0B0FF00000000#") == 0);
    CHECK(back.relationPartner == 40.5f && back.relationSponsors == 22 && back.spentTime == 2);
    CHECK(back.lastSpendTimePartner == 28 && back.lastSpendTimeFriends == 28);
    CHECK(back.sponsors[3] == 9 && back.sponsors[0] == 0);
    CHECK(back.teamShopping == 2);
    CHECK(back.gambling == 36 && back.casinoCount == 12);
    CHECK(ssr_career_agent(&back) == 1 && ssr_career_trainer(&back) == 2 && back.agent[0] == 51);
    CHECK(back.bootId == 21 && back.bootUses == 4 && back.expiredBoots == -1 && back.oldProBoots == 21);
    CHECK(back.ballCup == 3 && back.ballLeague == 1);
    CHECK(back.lastNegotiations == 70 && back.lastTransferDate == 66 && back.captain == 3 && back.showContractTip == 1);
    CHECK(back.items[0] == 120 && back.vehicles[19] == 10500 && back.properties[4] == 1);
    // The new season starts the counts again.
    back.energy = 20;
    back.relationBoss = 50;
    ssr_career_reset_season(&back);
    CHECK(back.relationBoss == 45 && back.relationBossInt == 54);
    CHECK(back.seasonApps == 0 && back.seasonTotalRating == 0 && back.wonMajorAward == 0 && back.energy == 100);
    CHECK(back.headlineResult[0] == 0);
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

// The trophy log and the stat history: the rules of the game, and the
// save file.
static void test_keeps_the_trophies_and_the_history(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    c.sdate = 364 * 2 + 10;
    ssr_career_add_trophy(&c, 2, "NORTH", "ENG1 1st", 2);
    ssr_career_add_trophy(&c, 2, "NORTH", "ENG1 1st", 1);
    ssr_career_add_trophy(&c, 3, "NORTH", "ENGCUP WINNER", 3);
    ssr_career_add_trophy(&c, 3, "WEST", "ENG1 1st", 2);
    CHECK(c.trophyCount == 3);
    CHECK(ssr_career_count_medals(&c, 2, 0) == 2 && ssr_career_count_medals(&c, 2, 3) == 1);
    CHECK(ssr_career_count_medal_teams(&c, 2) == 2 && ssr_career_count_medal_teams(&c, 3) == 1);
    // Each row holds the counts that no row holds yet.
    c.apps = 30;
    c.goals = 12;
    c.assists = 4;
    c.starMan = 5;
    ssr_career_add_history(&c, 2, "NORTH");
    c.apps = 50;
    c.goals = 20;
    ssr_career_add_history(&c, 3, "WEST");
    CHECK(c.historyCount == 2 && c.history[1].apps == 20 && c.history[1].goals == 8 && c.history[1].assists == 0);
    CHECK(ssr_career_history_apps(&c, "NORTH") == 30 && ssr_career_history_apps(&c, "SOUTH") == 0);
    int apps, goals;
    ssr_career_history_totals(&c, &apps, &goals, NULL, NULL);
    CHECK(apps == 50 && goals == 20);

    char path[1200];
    snprintf(path, sizeof(path), "%s/history.txt", ssr_test_directory("career_history"));
    CHECK(ssr_career_save(&c, path));
    CHECK(ssr_career_load(&back, path));
    CHECK(back.trophyCount == 3 && back.historyCount == 2);
    CHECK(strcmp(back.trophies[1].tournament, "ENGCUP WINNER") == 0 && strcmp(back.trophies[1].team, "NORTH") == 0);
    CHECK(back.trophies[1].year == 3 && back.trophies[1].medal == 3);
    CHECK(strcmp(back.history[0].club, "NORTH") == 0 && back.history[0].starMan == 5 && back.history[1].apps == 20);

    // A trophy of an old save has no team and no medal. A row of the
    // history needs six parts.
    FILE *file = fopen(path, "ab");
    fputs("trophy=4:ENG2 3rd\nhistory=4:WEST:1:2:3\n", file);
    fclose(file);
    CHECK(ssr_career_load(&back, path));
    CHECK(back.trophyCount == 4 && back.trophies[3].medal == 0 && back.trophies[3].team[0] == 0);
    CHECK(back.historyCount == 2);
}

// The days of the training of each skill, and the days of the loss with
// the sign of the warning, go into the save. An old save has none.
static void test_keeps_the_days_of_training(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    int days[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        c.lastTrained[i] = days[i];
        c.deplete[i] = i == 2 ? -days[i] : days[i];
    }
    char path[1200];
    snprintf(path, sizeof(path), "%s/training.txt", ssr_test_directory("career_training"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.lastTrained[4] == 50 && back.deplete[2] == -30 && back.deplete[0] == 10);
    FILE *file = fopen(path, "wb");
    fputs("clubid=10\nsdate=5\n", file);
    fclose(file);
    CHECK(ssr_career_load(&back, path) && back.lastTrained[0] == 0 && back.deplete[4] == 0);
}

// An award keeps the day of its unlock. A second unlock changes nothing
// (c_TPlayer.CheckAchievement), and the save keeps the awards.
static void test_keeps_the_awards(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    c.sdate = 40;
    CHECK(ssr_career_award_count(&c) == 0);
    CHECK(ssr_career_unlock_award(&c, 33) == 1);
    c.sdate = 50;
    CHECK(ssr_career_unlock_award(&c, 33) == 0 && c.awards[33] == 40);
    CHECK(ssr_career_unlock_award(&c, 125) == 1 && c.awards[125] == 50);
    CHECK(ssr_career_unlock_award(&c, SSR_CAREER_AWARDS) == 0);
    CHECK(ssr_career_award_count(&c) == 2);
    char path[1200];
    snprintf(path, sizeof(path), "%s/awards.txt", ssr_test_directory("career_awards"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.awards[33] == 40 && back.awards[125] == 50 && ssr_career_award_count(&back) == 2);
}

// The records: the hat tricks of the career, the form of the seasons, and
// the furthest goals and passes. A new season forgets its furthest values.
static void test_keeps_the_records(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    c.hatTricks = 4;
    c.worldSeed = 4242;
    c.startAge = 24;
    c.position = SSR_CAREER_GOALKEEPER;
    c.loanFrom = 915;
    c.charityYear = 3;
    c.businessStake = 700;
    snprintf(c.exBoss, sizeof(c.exBoss), "Ann Lake");
    c.exBossClub = 810;
    c.injury = 12;
    int form[5] = {0, 0, 6, 7, 8};
    memcpy(c.careerForm, form, sizeof(form));
    c.seasonFurthest[0] = 300.5f;
    c.furthest[1] = 640;
    c.intFurthest[3] = 250;
    char path[1200];
    snprintf(path, sizeof(path), "%s/records.txt", ssr_test_directory("career_records"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.worldSeed == 4242 && back.injury == 12);
    // A save of 0.27 kept a striker (1) or a holding midfielder (2).
    static SSRCareer old;
    old = c;
    old.position = 0;
    char oldPath[1200];
    snprintf(oldPath, sizeof(oldPath), "%s/old.txt", ssr_test_directory("career_old_position"));
    CHECK(ssr_career_save(&old, oldPath));
    FILE *file = fopen(oldPath, "a");
    fputs("position=2\n", file);
    fclose(file);
    CHECK(ssr_career_load(&old, oldPath) && old.position == SSR_CAREER_MIDFIELDER);
    CHECK(ssr_career_save(&old, oldPath));
    file = fopen(oldPath, "a");
    fputs("position=1\n", file);
    fclose(file);
    CHECK(ssr_career_load(&old, oldPath) && old.position == SSR_CAREER_FORWARD);
    // A custom career starts at its own age.
    CHECK(back.startAge == 24 && ssr_career_age(&back) == ssr_career_year(&back) + 23);
    CHECK(back.loanFrom == 915 && back.charityYear == 3 && back.businessStake == 700);
    CHECK(strcmp(back.exBoss, "Ann Lake") == 0 && back.exBossClub == 810);
    back.startAge = 0;
    // The position gives the focus at the start of a match.
    CHECK(back.position == SSR_CAREER_GOALKEEPER && ssr_career_focus(&back) == 1);
    back.position = SSR_CAREER_DEFENDER;
    CHECK(ssr_career_focus(&back) == 1);
    back.position = SSR_CAREER_MIDFIELDER;
    CHECK(ssr_career_focus(&back) == 2);
    back.position = SSR_CAREER_FORWARD;
    CHECK(ssr_career_focus(&back) == 2);
    CHECK(ssr_career_age(&back) == ssr_career_year(&back) + 15);
    CHECK(back.hatTricks == 4 && memcmp(back.careerForm, form, sizeof(form)) == 0);
    CHECK(back.seasonFurthest[0] == 300.5f && back.furthest[1] == 640 && back.intFurthest[3] == 250);
    // A hat trick comes at the 3rd, the 6th, and the 9th goal only.
    CHECK(ssr_career_hat_tricks(2) == 0 && ssr_career_hat_tricks(3) == 1 && ssr_career_hat_tricks(6) == 2);
    CHECK(ssr_career_hat_tricks(9) == 3 && ssr_career_hat_tricks(12) == 3);
    ssr_career_reset_season(&back);
    CHECK(back.seasonFurthest[0] == 0 && back.furthest[1] == 640 && back.intFurthest[3] == 250);
}

// The timeline keeps the newest moments, with the day of each.
static void test_keeps_the_moments(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    for (int i = 1; i <= SSR_CAREER_MOMENTS + 2; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Moment %d", i);
        ssr_career_add_moment(&c, i * 10, text);
    }
    CHECK(c.momentCount == SSR_CAREER_MOMENTS && strcmp(c.moments[0].text, "Moment 3") == 0);
    ssr_career_add_moment(&c, 999, "Two lines\nand a \\ sign");
    char path[1200];
    snprintf(path, sizeof(path), "%s/moments.txt", ssr_test_directory("career_moments"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.momentCount == SSR_CAREER_MOMENTS && back.moments[0].sdate == 40);
    CHECK(strcmp(back.moments[SSR_CAREER_MOMENTS - 1].text, "Two lines\nand a \\ sign") == 0);
}

// StoreOldStats: a career of the second year with no history gets a row
// of year 0 with the counts before the season.
static void test_stores_the_old_stats(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    c.apps = 40;
    c.seasonApps = 10;
    c.goals = 9;
    c.seasonGoals = 4;
    char path[1200];
    snprintf(path, sizeof(path), "%s/old.txt", ssr_test_directory("career_old_stats"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.historyCount == 0);
    c.sdate = 364 + 3;
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(back.historyCount == 1 && back.history[0].year == 0 && strcmp(back.history[0].club, "-") == 0);
    CHECK(back.history[0].apps == 30 && back.history[0].goals == 5);
}

// The balls of the match types: a new career has 1 to 4, a field of 0
// takes the ball of its type, and a choice goes into the save.
static void test_chooses_the_match_balls(void) {
    static SSRCareer c, back;
    ssr_career_create(&c, "The", "Kid", 0, 0, 62, 1, 1, 0);
    c.clubId = 10;
    for (int t = 1; t <= 4; t++) CHECK(ssr_career_selected_ball(&c, t) == t);
    c.ballLeague = c.ballCup = c.ballContinent = c.ballInternational = 0;
    for (int t = 1; t <= 4; t++) CHECK(ssr_career_selected_ball(&c, t) == t);
    CHECK(ssr_career_selected_ball(&c, 5) == 0);
    ssr_career_set_selected_ball(&c, 6, 2);
    ssr_career_set_selected_ball(&c, 0, 4);
    char path[1200];
    snprintf(path, sizeof(path), "%s/balls.txt", ssr_test_directory("career_balls"));
    CHECK(ssr_career_save(&c, path) && ssr_career_load(&back, path));
    CHECK(ssr_career_selected_ball(&back, 2) == 6 && ssr_career_selected_ball(&back, 4) == 1);
}

int main(void) {
    test_chooses_the_match_balls();
    test_keeps_the_awards();
    test_keeps_the_records();
    test_keeps_the_moments();
    test_keeps_the_trophies_and_the_history();
    test_stores_the_old_stats();
    test_keeps_the_days_of_training();
    test_changes_the_relations();
    test_names_the_player_type();
    test_updates_a_rating();
    test_sums_a_skill();
    test_creates_a_career();
    test_saves_and_loads_a_career();
    return TEST_RESULT();
}
