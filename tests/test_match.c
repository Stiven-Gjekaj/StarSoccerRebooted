// Tests ssr_match with matches that the test makes and plays.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssr_match.h"
#include "ssr_test.h"

static const char *localise(void *context, const char *tag) {
    (void)context;
    if (strncmp(tag, "CMATCHTEXT_CHANCEFORPLAYER", 26) == 0 && strlen(tag) <= 28) return "$playername collects the ball";
    if (strcmp(tag, "CMATCHTEXT_CHANCEMISSED") == 0) return "Chance missed";
    if (strncmp(tag, "CCHANCESTAGE_SHOOT", 18) == 0) return "$teamname have a chance to score...";
    return tag;
}

static SSRMatchSetup setup_for(void) {
    SSRMatchSetup s = {.homeName = "AAA",
                       .awayName = "BBB",
                       .homeStrength = 60,
                       .awayStrength = 50,
                       .myTeam = 1,
                       .playerName = "Smith",
                       .focus = 2,
                       .workRate = 2,
                       .energy = 100,
                       .age = 17,
                       .relationTeam = 75,
                       .relationBoss = 75,
                       .setPieces = 10,
                       .vision = 20,
                       .careerApps = 3,
                       .starLevel = 1};
    return s;
}

typedef struct {
    int lines, kickOff, halfTime, fullTime, goals, chances, misses;
    char last[256];
} Seen;

// Plays the match to its end. Each chance of the player ends with result.
static void play(SSRMatch *m, SSRRandom *random, SSRChanceResult result, Seen *seen) {
    memset(seen, 0, sizeof(*seen));
    int now = 0;
    ssr_match_play(m, now);
    for (int frame = 0; frame < 200000 && m->state != SSR_MATCH_FULL_TIME; frame++) {
        now += 16;
        SSRMatchEvent event = ssr_match_update(m, now, random);
        if (event == SSR_MATCH_EVENT_CHANCE) {
            seen->chances++;
            SSRChance chance;
            memset(&chance, 0, sizeof(chance));
            chance.result = result;
            chance.info.chanceType = m->chanceType;
            ssr_match_chance_done(m, &chance, now, random);
        }
        if (event == SSR_MATCH_EVENT_HALF_TIME) ssr_match_play(m, now);
        SSRMatchLine line;
        while (ssr_match_take_line(m, &line)) {
            seen->lines++;
            seen->kickOff += strcmp(line.text, "Kick Off") == 0;
            seen->halfTime += strncmp(line.text, "Half Time", 9) == 0 && strcmp(line.minute, "45") == 0;
            seen->fullTime += strncmp(line.text, "Full Time", 9) == 0 && strcmp(line.minute, "90") == 0;
            seen->goals += line.goal;
            seen->misses += strcmp(line.text, "Chance missed") == 0;
            CHECK(strstr(line.text, "$playername") == NULL && strstr(line.text, "$teamname") == NULL);
            snprintf(seen->last, sizeof(seen->last), "%s", line.text);
        }
    }
}

// The difficulty changes only the other team, and stops at 100.
static void test_sets_the_difficulty(void) {
    SSRMatchSetup s = {.homeStrength = 50, .awayStrength = 60, .myTeam = 1};
    ssr_match_set_difficulty(&s, 0.8f);
    CHECK(s.homeStrength == 50 && fabsf(s.awayStrength - 48) < 1e-4f && s.windFactor == 0.8f);
    s = (SSRMatchSetup){.homeStrength = 90, .awayStrength = 60, .myTeam = 2};
    ssr_match_set_difficulty(&s, 1.2f);
    CHECK(s.homeStrength == 100 && s.awayStrength == 60);
    s.homeStrength = 110;
    ssr_match_set_difficulty(&s, 1);
    CHECK(s.homeStrength == 110);
    // The wind of a chance grows with the difficulty.
    static SSRMatch m;
    m.setup = setup_for();
    ssr_match_set_difficulty(&m.setup, 1.2f);
    m.windSpeed = 4;
    SSRChanceSetup c = ssr_match_chance_setup(&m, 50, 50, 1);
    CHECK(fabsf(c.windSpeed - 4.8f) < 1e-4f);
}

static void test_plays_a_match(void) {
    SSRMatch m;
    SSRRandom random = {SSR_RANDOM_START};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    int marked = 0;
    for (int i = 0; i < 120; i++) marked += m.playerChance[i] != 0;
    CHECK(marked >= 3);
    CHECK(m.state == SSR_MATCH_PREMATCH && m.textSpeed == 800);
    CHECK(m.stadiumSize == 2 && m.penaltyTaker >= 1 && m.penaltyTaker <= 9);
    Seen seen;
    play(&m, &random, SSR_RESULT_MISSED, &seen);
    CHECK(m.state == SSR_MATCH_FULL_TIME && m.minute == 90);
    CHECK(seen.kickOff == 1 && seen.halfTime == 1 && seen.fullTime == 1);
    // The player has two sure chances, one in each half.
    CHECK(seen.chances >= 2 && seen.misses == seen.chances);
    char full[64];
    snprintf(full, sizeof(full), "Full Time %d - %d", m.score[1], m.score[2]);
    CHECK(strcmp(seen.last, full) == 0);
    CHECK(seen.goals == m.score[1] + m.score[2]);
    CHECK(m.energy < 100 && m.energy > 0);
    CHECK(ssr_match_play(&m, 0) == 1);
}

static void test_a_cup_tie_goes_to_extra_time_and_penalties(void) {
    int extra = 0, shootouts = 0;
    for (int seed = 1; seed < 400 && shootouts < 2; seed++) {
        SSRMatch m;
        SSRRandom random = {seed};
        SSRMatchSetup s = setup_for();
        s.matchType = 3;
        s.awayStrength = 60;
        ssr_match_start(&m, &s, localise, NULL, &random);
        Seen seen;
        play(&m, &random, SSR_RESULT_MISSED, &seen);
        CHECK(m.state == SSR_MATCH_FULL_TIME);
        if (m.minute == 90) {
            // Over at 90 minutes only with a winner.
            CHECK(m.score[1] != m.score[2]);
            continue;
        }
        extra++;
        CHECK(m.minute == 120 && m.half == 5);
        if (m.score[1] != m.score[2]) {
            CHECK(m.shootoutNumber == 0);
            continue;
        }
        // Level after 120 minutes: five kicks each at least, and a winner.
        shootouts++;
        CHECK(m.shootoutNumber >= 6 && m.pens[1] != m.pens[2]);
        CHECK(strstr(seen.last, "CMATCHTEXT_PENRESULT") != NULL);
    }
    CHECK(extra > 0 && shootouts > 0);
    // A league match ends at 90 minutes, also with a draw.
    SSRMatch m;
    SSRRandom random = {7};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    Seen seen;
    play(&m, &random, SSR_RESULT_MISSED, &seen);
    CHECK(m.minute == 90);
}

static void test_counts_the_goals_of_the_player(void) {
    SSRMatch m;
    SSRRandom random = {99};
    SSRMatchSetup s = setup_for();
    s.myTeam = 2;
    ssr_match_start(&m, &s, localise, NULL, &random);
    Seen seen;
    play(&m, &random, SSR_RESULT_GOAL, &seen);
    CHECK(m.goals == seen.chances && m.score[2] >= m.goals);
    CHECK(m.ratingCount >= m.goals);
}

static void test_a_tired_player_comes_off(void) {
    SSRMatch m;
    SSRRandom random = {7};
    SSRMatchSetup s = setup_for();
    s.energy = 10;
    s.workRate = 3;
    ssr_match_start(&m, &s, localise, NULL, &random);
    Seen seen;
    play(&m, &random, SSR_RESULT_MISSED, &seen);
    CHECK(m.setup.subTime < 0 && !ssr_match_on_pitch(&m));
    // 0.75 * 1.5 = 1.125 each minute: 1 is left after 8 minutes, so the
    // player comes off in the 9th.
    CHECK(m.setup.subTime == -9);
}

// After a good dribble the attack goes on to a shot or a cross.
static void test_a_dribble_leads_to_an_attack(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    ssr_match_play(&m, 0);
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.chanceType = 11;
    SSRChance chance;
    memset(&chance, 0, sizeof(chance));
    chance.result = SSR_RESULT_DRIBBLED;
    chance.info.chanceType = 11;
    chance.shotsOnTarget = 1;
    ssr_match_chance_done(&m, &chance, 1000, &random);
    CHECK(m.state == SSR_MATCH_STAGES && m.stage == 11 && m.dribbles == 1 && m.shotsOnTarget == 1);
    SSRMatchEvent event = SSR_MATCH_EVENT_NONE;
    for (int now = 1000; now < 60000 && event != SSR_MATCH_EVENT_CHANCE; now += 16) {
        event = ssr_match_update(&m, now, &random);
    }
    CHECK(event == SSR_MATCH_EVENT_CHANCE && (m.chanceType == 2 || m.chanceType == 8));
}

// The match keeps the furthest goal and pass, and the headed ones apart.
static void test_keeps_the_furthest_goal_and_pass(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    ssr_match_play(&m, 0);
    SSRChance chance;
    memset(&chance, 0, sizeof(chance));
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.chanceType = 1;
    chance.info.chanceType = 1;
    chance.result = SSR_RESULT_PASS;
    chance.kickYards = 40;
    ssr_match_chance_done(&m, &chance, 1000, &random);
    m.state = SSR_MATCH_PLAYER_CHANCE;
    chance.kickYards = 10;
    ssr_match_chance_done(&m, &chance, 2000, &random);
    CHECK(m.furthest[1] == 600 && m.furthest[3] == 0);
    // A header 300 pixels in front of the centre of the goal.
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.chanceType = 9;
    chance.info.chanceType = 9;
    chance.info.pitchSide = 1;
    chance.info.startX = ssr_goal_centre(1);
    chance.info.startY = SSR_GOAL_LINE + 300;
    chance.result = SSR_RESULT_GOAL;
    ssr_match_chance_done(&m, &chance, 3000, &random);
    CHECK(fabsf(m.furthest[0] - 300) < 0.01f && fabsf(m.furthest[2] - 300) < 0.01f && m.furthest[1] == 600);
}

// The match counts the post hits and the goals from a corner. A goal at a
// level score in the 90th minute wins the match at the last minute.
static void test_counts_the_posts_and_the_late_goals(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    ssr_match_play(&m, 0);
    SSRChance chance;
    memset(&chance, 0, sizeof(chance));
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.chanceType = 2;
    chance.info.chanceType = 2;
    chance.result = SSR_RESULT_SAVED;
    chance.postHits = 2;
    ssr_match_chance_done(&m, &chance, 1000, &random);
    CHECK(m.postHits == 2 && m.cornerGoals == 0 && m.lastMinuteWinner == 0);
    // A goal from a corner in the 89th minute.
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.minute = 89;
    chance.postHits = 0;
    chance.actionType = 7;
    chance.result = SSR_RESULT_GOAL;
    ssr_match_chance_done(&m, &chance, 2000, &random);
    CHECK(m.postHits == 2 && m.cornerGoals == 1 && m.lastMinuteWinner == 0);
    // A goal at 1-1 in the 90th minute.
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.minute = 90;
    m.score[1] = m.score[2] = 1;
    chance.actionType = 0;
    ssr_match_chance_done(&m, &chance, 3000, &random);
    CHECK(m.cornerGoals == 1 && m.lastMinuteWinner == 1);
    // A second leg counts the first leg: 1-1 now and 2-0 then is no tie.
    m.lastMinuteWinner = 0;
    m.state = SSR_MATCH_PLAYER_CHANCE;
    m.setup.matchType = 5;
    m.setup.firstLegHome = 2;
    m.score[1] = m.score[2] = 1;
    ssr_match_chance_done(&m, &chance, 4000, &random);
    CHECK(m.lastMinuteWinner == 0);
}

// The goals of the home team in the last half hour, over many matches,
// from a score of 0 to the away goals at the hour.
static int late_home_goals(int awayGoals) {
    int goals = 0;
    for (int seed = 1; seed <= 300; seed++) {
        SSRMatch m;
        SSRRandom random = {seed};
        SSRMatchSetup s = setup_for();
        s.homeStrength = s.awayStrength = 50;
        ssr_match_start(&m, &s, localise, NULL, &random);
        int now = 0;
        ssr_match_play(&m, now);
        m.minute = 60;
        m.half = 2;
        m.score[2] = awayGoals;
        for (int frame = 0; frame < 200000 && m.state != SSR_MATCH_FULL_TIME; frame++) {
            now += 16;
            SSRMatchEvent event = ssr_match_update(&m, now, &random);
            if (event == SSR_MATCH_EVENT_CHANCE) {
                SSRChance chance;
                memset(&chance, 0, sizeof(chance));
                chance.result = SSR_RESULT_MISSED;
                chance.info.chanceType = m.chanceType;
                ssr_match_chance_done(&m, &chance, now, &random);
            }
            if (event == SSR_MATCH_EVENT_HALF_TIME) ssr_match_play(&m, now);
            SSRMatchLine line;
            while (ssr_match_take_line(&m, &line)) {
            }
        }
        goals += m.score[1];
    }
    return goals;
}

// Momentum: a team two goals behind at the hour scores more in the last
// half hour than a team at a level score.
static void test_a_team_behind_pushes(void) {
    int level = late_home_goals(0), behind = late_home_goals(2);
    CHECK(behind > level * 11 / 10);
}

// A match in the rain says so at the kick-off, and its chances have a wet
// ball.
static void test_plays_in_the_rain(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    s.weather = 1;
    ssr_match_start(&m, &s, localise, NULL, &random);
    ssr_match_play(&m, 0);
    SSRMatchLine line;
    int rain = 0;
    while (ssr_match_take_line(&m, &line)) rain += strcmp(line.text, "SSR_MatchRain") == 0;
    CHECK(rain == 1 && ssr_match_chance_setup(&m, 50, 50, 1).weather == 1);
}

// A match in the snow says so at the kick-off, and its chances have the
// ball of the snow.
static void test_plays_in_the_snow(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    s.weather = 2;
    ssr_match_start(&m, &s, localise, NULL, &random);
    ssr_match_play(&m, 0);
    SSRMatchLine line;
    int snow = 0;
    while (ssr_match_take_line(&m, &line)) snow += strcmp(line.text, "SSR_MatchSnow") == 0;
    CHECK(snow == 1 && ssr_match_chance_setup(&m, 50, 50, 1).weather == 2);
}

// A cup final is in the biggest stadium, says so at the kick-off, and the
// other team raises its game. A match that the club must win says so too.
static void test_plays_a_big_match(void) {
    SSRMatch plain, final, must;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    s.homeStrength = 20;
    ssr_match_start(&plain, &s, localise, NULL, &random);
    s.bigMatch = SSR_BIG_FINAL;
    ssr_match_start(&final, &s, localise, NULL, &random);
    s.bigMatch = SSR_BIG_MUST_WIN;
    ssr_match_start(&must, &s, localise, NULL, &random);
    CHECK(plain.stadiumSize == 0 && final.stadiumSize == 4 && must.stadiumSize == 0);
    float a = ssr_match_chance_setup(&plain, 50, 50, 1).opponentStrength;
    CHECK(ssr_match_chance_setup(&final, 50, 50, 1).opponentStrength > a);
    CHECK(ssr_match_chance_setup(&must, 50, 50, 1).opponentStrength > a);
    ssr_match_play(&final, 0);
    ssr_match_play(&must, 0);
    SSRMatchLine line;
    int said = 0;
    while (ssr_match_take_line(&final, &line)) said += strcmp(line.text, "SSR_MatchFinal") == 0;
    while (ssr_match_take_line(&must, &line)) said += strcmp(line.text, "SSR_MatchMustWin") == 0;
    CHECK(said == 2);
}

// The stadium of a pack: its seats choose the size, and the commentary
// names it at the kick-off.
static void test_plays_in_the_stadium_of_a_pack(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    s.homeStrength = 20;
    s.stadium = "Harbour Park";
    s.capacity = 41000;
    ssr_match_start(&m, &s, localise, NULL, &random);
    CHECK(m.stadiumSize == 3);
    ssr_match_play(&m, 0);
    SSRMatchLine line;
    int named = 0;
    while (ssr_match_take_line(&m, &line)) named += strcmp(line.text, "SSR_MatchStadium") == 0;
    CHECK(named == 1);
}

// Plays the matches of a position. Each moment of the player ends with the
// result, and the test counts the kinds of the moments: defence, middle,
// and attack. A won moment in defence counts in the match.
static void moments_of(int position, SSRChanceResult result, int counts[3], int *defends, int *conceded) {
    counts[0] = counts[1] = counts[2] = 0;
    *defends = *conceded = 0;
    for (int seed = 1; seed <= 30; seed++) {
        SSRMatch m;
        SSRRandom random = {seed};
        SSRMatchSetup s = setup_for();
        s.position = position;
        s.blocks = 1;
        ssr_match_start(&m, &s, localise, NULL, &random);
        int now = 0, before = 0;
        ssr_match_play(&m, now);
        for (int frame = 0; frame < 400000 && m.state != SSR_MATCH_FULL_TIME; frame++) {
            now += 16;
            SSRMatchEvent event = ssr_match_update(&m, now, &random);
            if (event == SSR_MATCH_EVENT_CHANCE && m.shootoutNumber == 0) {
                int t = m.chanceType;
                int defence = t == SSR_CHANCE_SAVE || t == SSR_CHANCE_BLOCK;
                counts[defence ? 0 : t == 8 || t == 11 || t == 1 || t == 9 ? 1 : 2]++;
                SSRChance chance;
                memset(&chance, 0, sizeof(chance));
                chance.result = defence ? result : SSR_RESULT_MISSED;
                chance.info.chanceType = t;
                before = m.score[2];
                ssr_match_chance_done(&m, &chance, now, &random);
                *conceded += m.score[2] - before;
            } else if (event == SSR_MATCH_EVENT_CHANCE) {
                SSRChance chance;
                memset(&chance, 0, sizeof(chance));
                chance.result = SSR_RESULT_MISSED;
                ssr_match_chance_done(&m, &chance, now, &random);
            }
            if (event == SSR_MATCH_EVENT_HALF_TIME) ssr_match_play(&m, now);
            SSRMatchLine line;
            while (ssr_match_take_line(&m, &line)) {
            }
        }
        *defends += m.defends;
    }
}

// The positions: a goalkeeper has only saves, a defender has the most
// moments in defence, a midfielder the most in the middle, and a forward
// the most in attack. A lost save is a goal of the other team.
static void test_gives_the_moments_of_a_position(void) {
    int c[4][3], defends, conceded;
    moments_of(3, SSR_RESULT_DEFENDED, c[3], &defends, &conceded);
    CHECK(c[3][0] > 0 && c[3][1] == 0 && c[3][2] == 0 && defends == c[3][0] && conceded == 0);
    moments_of(3, SSR_RESULT_CONCEDED, c[3], &defends, &conceded);
    CHECK(defends == 0 && conceded == c[3][0]);
    moments_of(2, SSR_RESULT_DEFENDED, c[2], &defends, &conceded);
    CHECK(c[2][0] > c[2][1] && c[2][1] > c[2][2]);
    moments_of(1, SSR_RESULT_DEFENDED, c[1], &defends, &conceded);
    CHECK(c[1][1] > c[1][0] && c[1][1] > c[1][2]);
    moments_of(0, SSR_RESULT_DEFENDED, c[0], &defends, &conceded);
    CHECK(c[0][2] > c[0][0] && c[0][0] > 0);
}

static void test_the_captain_draws_runs(void) {
    SSRMatch m;
    SSRRandom random = {11};
    SSRMatchSetup s = setup_for();
    ssr_match_start(&m, &s, localise, NULL, &random);
    m.chanceType = 2;
    CHECK(!ssr_match_chance_setup(&m, 50, 50, 1).captain);
    m.setup.captain = 1;
    CHECK(ssr_match_chance_setup(&m, 50, 50, 1).captain);
    // The cup captain is not the captain of a league match.
    m.setup.captain = 2;
    CHECK(!ssr_match_chance_setup(&m, 50, 50, 1).captain);
    m.setup.captain = 3;
    m.setup.subTime = 60;
    CHECK(!ssr_match_chance_setup(&m, 50, 50, 1).captain);
}

int main(void) {
    test_sets_the_difficulty();
    test_the_captain_draws_runs();
    test_a_dribble_leads_to_an_attack();
    test_keeps_the_furthest_goal_and_pass();
    test_counts_the_posts_and_the_late_goals();
    test_a_team_behind_pushes();
    test_plays_in_the_rain();
    test_plays_in_the_snow();
    test_plays_a_big_match();
    test_plays_in_the_stadium_of_a_pack();
    test_gives_the_moments_of_a_position();
    test_plays_a_match();
    test_a_cup_tie_goes_to_extra_time_and_penalties();
    test_counts_the_goals_of_the_player();
    test_a_tired_player_comes_off();
    return TEST_RESULT();
}
