// Tests ssr_match with matches that the test makes and plays.

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
    test_the_captain_draws_runs();
    test_a_dribble_leads_to_an_attack();
    test_plays_a_match();
    test_a_cup_tie_goes_to_extra_time_and_penalties();
    test_counts_the_goals_of_the_player();
    test_a_tired_player_comes_off();
    return TEST_RESULT();
}
