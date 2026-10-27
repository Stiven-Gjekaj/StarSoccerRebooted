// Tests ssr_career: a new career and its save file.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssr_career.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static void test_creates_a_career(void) {
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

int main(void) {
    test_sums_a_skill();
    test_creates_a_career();
    test_saves_and_loads_a_career();
    return TEST_RESULT();
}
