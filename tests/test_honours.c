// Tests ssr_honours with honours that the test makes.

#include <stdio.h>
#include <string.h>

#include "ssr_honours.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static SSRHonour honour(int year, const char *league, int club, const char *scorer, int goals) {
    SSRHonour h;
    memset(&h, 0, sizeof(h));
    h.year = year;
    snprintf(h.league, sizeof(h.league), "%s", league);
    h.club = club;
    snprintf(h.clubName, sizeof(h.clubName), "Club %d", club);
    snprintf(h.scorer, sizeof(h.scorer), "%s", scorer);
    snprintf(h.scorerClub, sizeof(h.scorerClub), "Club %d", club + 1);
    h.goals = goals;
    return h;
}

static void test_keeps_the_honours(void) {
    SSRHonours h = {0};
    SSRHonour a = honour(1, "LG1", 10, "Ann Lake", 21), b = honour(1, "LG2", 11, "", 0), c = honour(2, "LG1", 10, "Bob Hill", 18);
    CHECK(ssr_honours_add(&h, &a) && ssr_honours_add(&h, &b) && ssr_honours_add(&h, &c));
    // The same season and league again takes the place of the first.
    SSRHonour again = honour(2, "LG1", 12, "Cid Moor", 19);
    CHECK(ssr_honours_add(&h, &again) && h.count == 3);
    CHECK(ssr_honours_titles(&h, 10) == 1 && ssr_honours_titles(&h, 12) == 1);
    char path[1200];
    snprintf(path, sizeof(path), "%s/honours.txt", ssr_test_directory("honours"));
    CHECK(ssr_honours_save(&h, path));
    SSRHonours back = {0};
    CHECK(ssr_honours_load(&back, path) && back.count == 3);
    CHECK(memcmp(&back.list[0], &h.list[0], sizeof(SSRHonour)) == 0);
    CHECK(back.list[1].scorer[0] == 0 && back.list[2].goals == 19 && strcmp(back.list[2].scorer, "Cid Moor") == 0);
    CHECK(!ssr_honours_load(&back, "/no/such/file") && back.count == 0);
    ssr_honours_clear(&h);
    ssr_honours_clear(&back);
}

// A rivalry grows from the points of both orders of the clubs, and the
// file keeps it.
static void test_keeps_the_rivalries(void) {
    SSRHonours h = {0};
    CHECK(ssr_honours_add_rivalry(&h, 10, 11, 2) == 2 && ssr_honours_add_rivalry(&h, 11, 10, 3) == 5);
    CHECK(ssr_honours_add_rivalry(&h, 10, 10, 3) == 0 && h.rivalryCount == 1);
    CHECK(ssr_honours_rivalry(&h, 11, 10) == 5 && ssr_honours_rivalry(&h, 10, 12) == 0);
    char path[1200];
    snprintf(path, sizeof(path), "%s/rivals.txt", ssr_test_directory("honours_rivals"));
    CHECK(ssr_honours_save(&h, path));
    SSRHonours back = {0};
    CHECK(ssr_honours_load(&back, path) && back.rivalryCount == 1 && ssr_honours_rivalry(&back, 10, 11) == 5);
    ssr_honours_clear(&h);
    ssr_honours_clear(&back);
}

int main(void) {
    test_keeps_the_rivalries();
    test_keeps_the_honours();
    return TEST_RESULT();
}
