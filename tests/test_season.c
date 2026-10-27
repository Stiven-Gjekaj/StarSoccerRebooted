// Tests ssr_season with a small world that the test makes: two leagues of
// four clubs, a cup of the eight clubs, and the places that move one club
// up and one club down.

#include <stdio.h>
#include <string.h>

#include "ssr_season.h"
#include "ssr_test.h"

static SSRContinent continents[1] = {{1, "Europe", "EUR", "UEFA", "UEFA", 50}};
static SSRNation nations[2] = {
    {1, "Testland", "TST", "Testish", 60, 1, 0, 50, 0, {0}, {0}},
    {2, "Otherland", "OTH", "Otherish", 40, 1, 0, 40, 0, {0}, {0}},
};
static SSRClub clubs[10];
static SSRCompetition comps[3];
static SSRPromotion promotions[4];
static SSRWorld world;

static SSRCompetition comp(int id, const char *tla, int type, int week, int duration, int day, int status) {
    SSRCompetition c;
    memset(&c, 0, sizeof(c));
    c.id = id;
    c.tla = tla;
    c.locale = 0;
    c.level = 0;
    c.based = 1;
    c.type = type;
    c.startYear = 1;
    c.startWeek = week;
    c.duration = duration;
    c.primaryMatchday = day;
    c.secondaryMatchday = 99;
    c.groups = 1;
    c.rounds = type == 0 ? 2 : 1;
    c.legs = 0;
    c.status = status;
    return c;
}

static void make_world(void) {
    static const char *names[10] = {"AAA", "BBB", "CCC", "DDD", "EEE", "FFF", "GGG", "HHH", "XXX", "YYY"};
    memset(clubs, 0, sizeof(clubs));
    for (int i = 0; i < 10; i++) {
        clubs[i].id = 100 + i;
        clubs[i].name = names[i];
        clubs[i].tla = names[i];
        clubs[i].strength = 90 - i * 5;
        clubs[i].nation = i < 8 ? 1 : 2;
        clubs[i].league = i < 4 ? 10 : i < 8 ? 20 : 0;
        clubs[i].latitude = i;
    }
    // XXX has no league, so it is not in the game.
    clubs[9].league = 30;
    comps[0] = comp(10, "LG1", 0, 6, 10, 6, 1);
    comps[1] = comp(20, "LG2", 0, 6, 10, 6, 2);
    comps[2] = comp(30, "CUP", 1, 20, 1, 3, 0);
    comps[2].legs = 0;
    // The last of LG1 goes down, the first of LG2 goes up. All the clubs
    // of both leagues play the cup.
    promotions[0] = (SSRPromotion){10, 4, 20};
    promotions[1] = (SSRPromotion){20, 1, 10};
    promotions[2] = (SSRPromotion){10, 100, 30};
    promotions[3] = (SSRPromotion){20, 100, 30};
    memset(&world, 0, sizeof(world));
    world.continents = continents;
    world.continentCount = 1;
    world.nations = nations;
    world.nationCount = 2;
    world.clubs = clubs;
    world.clubCount = 10;
    world.competitions = comps;
    world.competitionCount = 3;
    world.promotions = promotions;
    world.promotionCount = 4;
}

static SSRSeason *new_season(SSRRandom *random) {
    SSRSeason *s = ssr_season_create(&world, random);
    s->player.sdate = ssr_date_make(1, 1, 1);
    s->player.clubId = 102;
    s->player.nationId = 1;
    ssr_season_set_up(s);
    return s;
}

static void test_dates(void) {
    int d = ssr_date_make(6, 6, 1);
    CHECK(d == 41 && ssr_date_day(d) == 6 && ssr_date_week(d) == 6 && ssr_date_year(d) == 1);
    CHECK(ssr_date_day(ssr_date_make(7, 1, 2)) == 7 && ssr_date_week(365) == 1 && ssr_date_year(365) == 2);
}

static void test_sets_up_the_leagues_and_the_cup(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRSeason *s = new_season(&random);
    SSRSeasonComp *one = ssr_season_comp(s, 10), *two = ssr_season_comp(s, 20), *cup = ssr_season_comp(s, 30);
    CHECK(one && two && cup);
    CHECK(one->poolCount == 1 && one->pools[0].count == 4 && two->pools[0].count == 4);
    // Four clubs meet two times: 6 rounds of 2 matches, on Saturdays.
    CHECK(one->fixtureCount == 12);
    for (int i = 0; i < one->fixtureCount; i++) {
        CHECK(ssr_date_day(one->fixtures[i].sdate) == 6 && one->fixtures[i].matchType == SSR_FIXTURE_LEAGUE);
        if (i > 0) CHECK(one->fixtures[i].sdate >= one->fixtures[i - 1].sdate);
    }
    // Each club plays each other club at home one time.
    int pairs[5][5] = {{0}};
    for (int i = 0; i < one->fixtureCount; i++) pairs[one->fixtures[i].home][one->fixtures[i].away]++;
    for (int h = 1; h <= 4; h++) {
        for (int a = 1; a <= 4; a++) CHECK(pairs[h][a] == (h == a ? 0 : 1));
    }
    // The cup has the eight clubs and four ties on a Wednesday.
    CHECK(cup->pools[0].count == 8 && cup->fixtureCount == 4);
    CHECK(ssr_date_day(cup->fixtures[0].sdate) == 3 && cup->fixtures[0].matchType == SSR_FIXTURE_EXTRA_TIME);
    // The strengths of the nation go down from the strongest to 15.
    CHECK(ssr_season_club(s, 100, 0)->strength == 90 && ssr_season_club(s, 107, 0)->strength < 30);
    // Only the clubs of the nation stay loaded; another comes as a copy.
    CHECK(s->clubListCount == 8 && ssr_season_club(s, 109, 0) == NULL && ssr_season_club(s, 109, 1) != NULL);
    CHECK(ssr_season_club(s, 108, 1) == NULL);
    // The next match of the player is the first round of the league.
    SSRFixture *next = ssr_season_next_fixture(s);
    CHECK(next && next->compId == 10 && next->sdate == one->fixtures[0].sdate);
    ssr_season_free(s);
}

static void test_plays_a_season(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRSeason *s = new_season(&random);
    SSRSeasonComp *one = ssr_season_comp(s, 10), *two = ssr_season_comp(s, 20), *cup = ssr_season_comp(s, 30);
    int end = ssr_season_play_to(s, ssr_date_make(1, 1, 2));
    CHECK(end && ssr_date_year(s->player.sdate) == 2);
    for (int i = 0; i < one->fixtureCount; i++) CHECK(one->fixtures[i].result == 1);
    // The table adds up: each match gives its goals to both sides.
    int played = 0, goals = 0, against = 0;
    for (int r = 0; r < one->pools[0].count; r++) {
        const SSRTableRow *row = &s->rows[one->pools[0].rows[r]];
        CHECK(row->played == 6 && row->won + row->drawn + row->lost == 6);
        CHECK(row->points == row->won * 3 + row->drawn);
        played += row->played;
        goals += row->goalsFor;
        against += row->goalsAgainst;
    }
    CHECK(played == 24 && goals == against);
    // The last of LG1 and the first of LG2 change places.
    ssr_season_sort_pool(s, &one->pools[0], 4);
    ssr_season_sort_pool(s, &two->pools[0], 4);
    int down = s->rows[one->pools[0].rows[3]].teamId, up = s->rows[two->pools[0].rows[0]].teamId;
    CHECK(ssr_season_club(s, down, 0)->league == 20 && ssr_season_club(s, up, 0)->league == 10);
    // A cup with no place after it is a final: its winner is in the news.
    CHECK(cup->fixtures[0].result == 1 && s->hasWinnerNews && strcmp(s->winnerCompetition, "CUP") == 0);
    // The new season builds the tables from the new leagues.
    ssr_season_new_season(s);
    CHECK(one->pools[0].count == 4 && one->fixtureCount == 12 && one->fixtures[0].result == 0);
    int found = 0;
    for (int r = 0; r < one->pools[0].count; r++) found += s->rows[one->pools[0].rows[r]].teamId == up;
    CHECK(found == 1 && ssr_date_year(one->fixtures[0].sdate) == 2);
    ssr_season_free(s);
}

static void test_saves_and_loads(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRSeason *s = new_season(&random);
    SSRFixture *next = ssr_season_next_fixture(s);
    ssr_season_play_to(s, next->sdate + 20);
    const char *path = "test_season.save";
    CHECK(ssr_season_save(s, path));
    SSRSeason *t = ssr_season_create(&world, &random);
    t->player = s->player;
    CHECK(ssr_season_load(t, path));
    SSRSeasonComp *a = ssr_season_comp(s, 10), *b = ssr_season_comp(t, 10);
    CHECK(a->fixtureCount == b->fixtureCount && b->poolCount == 1 && b->pools[0].count == 4);
    for (int i = 0; i < a->fixtureCount && i < b->fixtureCount; i++) {
        CHECK(memcmp(&a->fixtures[i], &b->fixtures[i], sizeof(SSRFixture)) == 0);
    }
    for (int r = 0; r < 4; r++) {
        const SSRTableRow *x = &s->rows[a->pools[0].rows[r]], *y = &t->rows[b->pools[0].rows[r]];
        CHECK(x->id == y->id && x->teamId == y->teamId && x->points == y->points && strcmp(x->name, y->name) == 0);
    }
    CHECK(t->clubListCount == 8 && ssr_season_club(t, 103, 0)->strength == ssr_season_club(s, 103, 0)->strength);
    // The places come back from the data set.
    CHECK(ssr_season_comp(t, 30)->placesToMeCount == 2);
    remove(path);
    CHECK(!ssr_season_load(t, "/nonexistent/season.save"));
    ssr_season_free(s);
    ssr_season_free(t);
}

static void test_counts_the_match_of_the_player(void) {
    SSRRandom random = {SSR_RANDOM_START};
    SSRSeason *s = new_season(&random);
    SSRSeasonComp *one = ssr_season_comp(s, 10), *cup = ssr_season_comp(s, 30);
    SSRFixture *next = ssr_season_next_fixture(s);
    ssr_season_skip_to(s, next->sdate);
    CHECK(s->player.sdate == next->sdate - 1);
    CHECK(ssr_season_next_round(one) == 1 && ssr_season_round_date(one, 2) > next->sdate);
    CHECK(ssr_season_comp_type(s, one) == 1 && ssr_season_comp_type(s, cup) == 2);
    CHECK(ssr_season_actual_league(s, 102) == 10 && ssr_season_group_of(s, one, 102) == 1);
    CHECK(ssr_season_is_relegation_place(s, one, 4) == 0 && ssr_season_is_promotion_place(s, ssr_season_comp(s, 20), 1));
    int compId = next->compId, date = next->sdate, home = next->home, away = next->away;
    ssr_season_my_result(s, next, 3, 1);
    SSRFixture *found = ssr_season_find(s, compId, date, home, away);
    CHECK(found == next && found->result == -1);
    SSRTableRow *winner = ssr_season_row(s, &one->pools[0], home);
    CHECK(winner->played == 1 && winner->points == 3 && winner->goalsFor == 3);
    // The date moves to the day of the match, and the other match of the
    // round plays.
    int end = ssr_season_play_to(s, date);
    CHECK(!end && s->player.sdate == date && found->result == 1);
    CHECK(ssr_season_prev_fixture(s) == found && ssr_season_prev_round(one) == 1);
    CHECK(winner->played == 1);
    int rest = ssr_season_rest_days(s);
    CHECK(rest >= 3 && rest <= 15);
    // A draw in a cup tie with extra time ends with a winner or with
    // penalties.
    SSRFixture *tie = &cup->fixtures[0];
    ssr_season_my_result(s, tie, 1, 1);
    CHECK(tie->resultType == SSR_RESULT_EXTRA_TIME || tie->resultType == SSR_RESULT_PENALTIES);
    CHECK(ssr_fixture_winner(s, tie) != 0);
    ssr_season_free(s);
}

int main(void) {
    make_world();
    test_dates();
    test_sets_up_the_leagues_and_the_cup();
    test_plays_a_season();
    test_saves_and_loads();
    test_counts_the_match_of_the_player();
    return TEST_RESULT();
}
