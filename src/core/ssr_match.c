#include "ssr_match.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// The pixels of one yard (c_TMatch.m_YardsToPixels).
#define YARD 15.0f

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/* ------------------------------------------------------------ texts */

static const char *text_of(const SSRMatch *m, const char *key) {
    const char *text = m->localise ? m->localise(m->context, key) : NULL;
    return text ? text : key;
}

// Replaces each "$name" in text with value.
static void replace(char *text, size_t capacity, const char *name, const char *value) {
    char out[512] = "";
    size_t used = 0, length = strlen(name);
    for (const char *p = text; *p && used + 1 < sizeof(out);) {
        if (strncmp(p, name, length) == 0) {
            used += (size_t)snprintf(out + used, sizeof(out) - used, "%s", value);
            p += length;
        } else {
            out[used++] = *p++;
        }
    }
    out[used < sizeof(out) ? used : sizeof(out) - 1] = 0;
    snprintf(text, capacity, "%s", out);
}

static void add_line(SSRMatch *m, int stamped, const char *text, int team, int goal) {
    if (m->lineCount == SSR_MATCH_LINES) {
        m->lineFirst = (m->lineFirst + 1) % SSR_MATCH_LINES;
        m->lineCount--;
    }
    SSRMatchLine *line = &m->lines[(m->lineFirst + m->lineCount++) % SSR_MATCH_LINES];
    if (stamped) {
        snprintf(line->minute, sizeof(line->minute), "%d", m->minute);
    } else {
        line->minute[0] = 0;
    }
    snprintf(line->text, sizeof(line->text), "%s", text);
    line->team = team;
    line->goal = goal;
}

// A text of the language with $playername and $teamname.
static void say(SSRMatch *m, int stamped, const char *key, int team, int goal) {
    char text[256];
    snprintf(text, sizeof(text), "%s", text_of(m, key));
    replace(text, sizeof(text), "$playername", m->setup.playerName ? m->setup.playerName : "");
    const char *names[3] = {"", m->setup.homeName, m->setup.awayName};
    int other = m->attacking == 1 ? 2 : 1;
    replace(text, sizeof(text), "$teamname", names[m->attacking ? m->attacking : 1]);
    replace(text, sizeof(text), "$otherteam", names[other]);
    add_line(m, stamped, text, team, goal);
}

static void say_numbered(SSRMatch *m, int stamped, const char *key, int number, int team, int goal) {
    char full[64];
    snprintf(full, sizeof(full), "%s%d", key, number);
    say(m, stamped, full, team, goal);
}

// A random number from 1 to n that is not the last one.
static int fresh(SSRRandom *random, int n, int *last) {
    int r;
    do {
        r = ssr_rand(random, n);
    } while (r == *last && n > 1);
    *last = r;
    return r;
}

static void rate(SSRMatch *m, int index, int success) {
    if (m->ratingCount == SSR_MATCH_RATINGS) return;
    m->ratingIndex[m->ratingCount] = index;
    m->ratingSuccess[m->ratingCount++] = success;
}

/* ------------------------------------------------------------ start */

int ssr_match_on_pitch(const SSRMatch *m) {
    int sub = m->setup.subTime;
    return sub == 0 || (sub > 0 && m->minute >= sub);
}

// c_TMatch.m_SetUpChances: the minutes with a chance for the player.
static void set_up_chances(SSRMatch *m, SSRRandom *random) {
    const SSRMatchSetup *s = &m->setup;
    memset(m->playerChance, 0, sizeof(m->playerChance));
    m->playerChance[ssr_rand2(random, 0, 44)] = 2;
    m->playerChance[ssr_rand2(random, 45, 89)] = 2;
    if (s->relationTeam < 50) m->playerChance[ssr_rand2(random, 0, 89)] = 2;
    if ((s->level == 0 && (s->captain == 1 || s->captain == 3)) ||
        (s->level == 1 && (s->captain == 2 || s->captain == 3))) {
        m->playerChance[ssr_rand2(random, 0, 89)] = 2;
    }
    m->playerChance[ssr_rand2(random, 90, 119)] = 2;
    if (s->subTime > 0) m->playerChance[ssr_rand2(random, 79, 89)] = 2;
    for (int i = 1; i <= (int)((float)s->relationTeam / 14.0f); i++) m->playerChance[ssr_rand2(random, 0, 89)] = 1;
    if (s->relationTeam > ssr_rand(random, 100)) m->playerChance[ssr_rand2(random, 90, 119)] = 1;
    if (s->relationTeam > ssr_rand(random, 100)) m->playerChance[ssr_rand2(random, 90, 119)] = 1;
    // Match.LastChanceChance is -1: the draw never gives a last chance.
    ssr_rand(random, 100);
}

void ssr_match_start(SSRMatch *m, const SSRMatchSetup *setup, const char *(*localise)(void *, const char *),
                     void *context, SSRRandom *random) {
    memset(m, 0, sizeof(*m));
    m->setup = *setup;
    m->localise = localise;
    m->context = context;
    m->state = SSR_MATCH_PREMATCH;
    m->half = 1;
    m->textSpeed = 800;
    m->energy = setup->energy;
    m->lastFocus = m->lastWorkRate = 2;
    m->penaltyTaker = ssr_rand(random, 9);
    // c_TMatch: the stadium by the strength of the home team.
    float home = setup->homeStrength;
    m->stadiumSize = home >= 90 ? 4 : home >= 75 ? 3 : home >= 55 ? 2 : home >= 35 ? 1 : 0;
    set_up_chances(m, random);
    // The wind at kick-off.
    int top = setup->starLevel / 2;
    if (top > 5) top = 5;
    if (top < 0) top = 0;
    m->windSpeed = ssr_rand_float(random, -1, (float)top);
    if (m->windSpeed < 1) m->windSpeed = 0;
    m->windDirection = (float)ssr_rand(random, 360);
    m->focusTextTime = ssr_rand2(random, 5, 30);
    m->workRateTextTime = ssr_rand2(random, 5, 30);
}

static void next_kick(SSRMatch *m);

int ssr_match_play(SSRMatch *m, int now) {
    switch (m->state) {
    case SSR_MATCH_PREMATCH:
    case SSR_MATCH_HALF_TIME:
        m->state = SSR_MATCH_CLOCK;
        m->matchTick = now;
        if (m->minute == 0) {
            say(m, 0, "Kick Off", 0, 0);
            if (m->setup.weather == 1) say(m, 0, text_of(m, "SSR_MatchRain"), 0, 0);
        }
        return 0;
    case SSR_MATCH_PENALTIES:
        // DoShootOut: the first kick steps up.
        m->state = SSR_MATCH_SHOOTOUT;
        m->shootoutNumber = 0;
        m->pens[1] = m->pens[2] = 0;
        m->matchTick = now;
        next_kick(m);
        return 0;
    case SSR_MATCH_FULL_TIME:
        return 1;
    default:
        return 0;
    }
}

// c_TMatch.CheckShootOutComplete: five kicks each, then sudden death.
static int shootout_complete(const SSRMatch *m) {
    int n = m->shootoutNumber, a = m->pens[1], b = m->pens[2];
    switch (n) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        return 0;
    case 6:
        return a - b > 2 || b - a > 2;
    case 7:
        return a - b > 2 || b - a > 1;
    case 8:
        return a - b > 1 || b - a > 1;
    case 9:
        return a - b > 1 || b - a > 0;
    case 10:
        return a != b;
    default:
        return n % 2 == 0 && a != b;
    }
}

/* -------------------------------------------------------- chances */

static void update_wind(SSRMatch *m, SSRRandom *random) {
    if (m->windSpeed > 1.25f) m->windSpeed = clampf(m->windSpeed + ssr_rand_float(random, -0.25f, 0.25f), 0, 10);
    m->windDirection = clampf(m->windDirection + ssr_rand_float(random, 5, 5), 1, 360);
}

static void text_chance_for_player(SSRMatch *m, SSRRandom *random) {
    int header = m->chanceType == 9 || m->chanceType == 10;
    int r = fresh(random, header ? 5 : 10, &m->lastChanceForPlayer);
    say_numbered(m, 1, header ? "CMATCHTEXT_CHANCEFORPLAYERHEADER" : "CMATCHTEXT_CHANCEFORPLAYER", r, m->setup.myTeam,
                 0);
}

static int header_or(int type, int fallback, const SSRMatch *m) {
    return m->setup.careerApps < 2 ? fallback : type;
}

// bb_GSMatchUtility.g_GSChanceForPlayer: the type by the focus and the
// dribbling of the player. Returns 1 when it wrote its own text.
static int chance_type_for_player(SSRMatch *m, SSRRandom *random) {
    int focus = m->setup.focus;
    if (!m->setup.dribbling) {
        if (focus == 1) {
            int r = ssr_rand(random, 6);
            if (r == 1 || r == 2) {
                m->chanceType = 1;
            } else if (r != 3) {
                m->chanceType = 6;
                say_numbered(m, 1, "CMATCHTEXT_CHANCEFORPLAYERINTERCEPT", ssr_rand(random, 5), m->setup.myTeam, 0);
                return 1;
            } else {
                m->chanceType = header_or(9, 1, m);
            }
        } else if (focus == 3) {
            int r = ssr_rand(random, 8);
            if (r == 1) {
                m->chanceType = 1;
            } else if (r == 2) {
                m->chanceType = header_or(9, 1, m);
            } else {
                int k = ssr_rand(random, 3);
                m->chanceType = k == 1 ? header_or(10, 2, m) : k == 2 ? 2 : 8;
            }
        } else {
            int r = ssr_rand(random, 6);
            if (r == 1) {
                m->chanceType = 6;
                say_numbered(m, 1, "CMATCHTEXT_CHANCEFORPLAYERINTERCEPT", ssr_rand(random, 5), m->setup.myTeam, 0);
                return 1;
            }
            m->chanceType = r == 2 ? 8 : r == 3 ? header_or(9, 1, m) : 1;
        }
    } else if (focus == 1) {
        m->chanceType = ssr_rand(random, 4) == 1 ? header_or(9, 1, m) : 1;
    } else if (focus == 3) {
        int r = ssr_rand(random, 8);
        if (r == 1) {
            m->chanceType = 1;
        } else if (r == 2) {
            m->chanceType = header_or(9, 1, m);
        } else if (r == 3 || r == 4) {
            m->chanceType = 11;
        } else {
            int k = ssr_rand(random, 4);
            m->chanceType = k == 1 ? header_or(10, 2, m) : k == 2 ? 2 : k == 3 ? 8 : 11;
        }
        if (m->chanceType == 11) {
            say_numbered(m, 1, "CMATCHTEXT_CHANCEFORPLAYERDRIBBLE", ssr_rand(random, 5), m->setup.myTeam, 0);
            return 1;
        }
    } else {
        int r = ssr_rand(random, 5);
        if (r == 1) {
            m->chanceType = 11;
            say_numbered(m, 1, "CMATCHTEXT_CHANCEFORPLAYERDRIBBLE", ssr_rand(random, 5), m->setup.myTeam, 0);
            return 1;
        }
        m->chanceType = r == 2 ? 8 : r == 3 ? header_or(9, 1, m) : 1;
    }
    m->playerInBuildup = 1;
    return 0;
}

// c_TMatch.m_ChanceForPlayer: a build-up chance, or a shot.
static void chance_for_player(SSRMatch *m, int lastChance, SSRRandom *random) {
    m->stage = 4;
    int n = m->setup.focus == 1 ? 100 : m->setup.focus == 3 ? 2 : 5;
    if (ssr_rand(random, n) != 1 && m->playerInBuildup != 1 && (m->minute <= 87 || m->minute >= 91) &&
        m->minute <= 117 && !lastChance) {
        if (chance_type_for_player(m, random)) return;
    } else {
        m->shotChances++;
        m->chanceType = 2;
        if (ssr_rand(random, 5) == 1 && m->setup.focus != 1 && m->setup.careerApps >= 2) m->chanceType = 10;
    }
    text_chance_for_player(m, random);
}

static void chance_shoot(SSRMatch *m, SSRRandom *random) {
    m->stage = 3;
    say_numbered(m, 1, "CCHANCESTAGE_SHOOT", fresh(random, 10, &m->lastShoot), m->attacking, 0);
}

// Who takes a set piece: 1 the player by the skill, 2 the captain claims
// it, 0 another player.
static int taker(SSRMatch *m, int skillDraw, SSRRandom *random) {
    if (m->setup.setPieces > skillDraw) return 1;
    int c = m->setup.captain, level = m->setup.level;
    if ((level == 0 && (c == 1 || (c == 3 && ssr_rand(random, 4) == 1))) ||
        (level == 1 && (c == 2 || (c == 3 && ssr_rand(random, 4) == 1)))) {
        return 2;
    }
    return 0;
}

static void set_piece_text(SSRMatch *m, const char *key, int claim, SSRRandom *random) {
    char text[256];
    snprintf(text, sizeof(text), "%s", text_of(m, key));
    if (claim == 2 && m->attacking == m->setup.myTeam) {
        char claimKey[48];
        snprintf(claimKey, sizeof(claimKey), "CMATCHTEXT_CAPTAINCLAIMING%d", ssr_rand(random, 3));
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "|%s", text_of(m, claimKey));
    }
    add_line(m, 1, text, m->attacking, 0);
}

static void chance_penalty(SSRMatch *m, SSRRandom *random) {
    m->stage = 6;
    int c = taker(m, ssr_rand2(random, 1, 75), random);
    if (m->attacking == m->setup.myTeam && ssr_match_on_pitch(m) && (c > 0 || m->goals > 1)) {
        m->stage = 4;
        m->chanceType = 4;
    }
    set_piece_text(m, "CMATCHTEXT_PENALTY", c, random);
}

static void chance_free_kick(SSRMatch *m, SSRRandom *random) {
    m->stage = 5;
    int c = taker(m, ssr_rand2(random, -10, 50), random);
    const char *key = "CMATCHTEXT_FREEKICK";
    if (m->attacking == m->setup.myTeam && ssr_match_on_pitch(m) && (c > 0 || m->goals > 1)) {
        m->stage = 4;
        m->chanceType = 3;
        if (ssr_rand(random, 3) == 1) {
            m->chanceType = 7;
            key = "CMATCHTEXT_CORNER";
        }
    }
    set_piece_text(m, key, c, random);
}

// c_TMatch.m_CreateChance: a chance of the player, or of a team.
static void create_chance(SSRMatch *m, int forPlayer, SSRRandom *random) {
    m->state = SSR_MATCH_STAGES;
    if (forPlayer) {
        update_wind(m, random);
        m->attacking = m->setup.myTeam;
        m->chances++;
        chance_for_player(m, 0, random);
        return;
    }
    float s1 = m->setup.homeStrength + 5, s2 = m->setup.awayStrength;
    float boost = m->setup.focus == 1 ? -5.0f : m->setup.focus == 3 ? 5.0f : 0.0f;
    if (m->setup.myTeam == 1) {
        s1 += boost;
    } else {
        s2 += boost;
    }
    // Momentum, a rule of the remake: from the hour, the team that is
    // behind pushes forward, with 4 more strength for each goal behind, 10
    // at most. A comeback from 2-0 is then possible.
    int behind = m->score[1] - m->score[2];
    if (m->minute >= 60 && behind != 0) {
        float push = 4.0f * (float)(behind > 0 ? behind : -behind);
        if (push > 10) push = 10;
        if (behind > 0) s2 += push;
        else s1 += push;
    }
    m->attacking = ssr_rand_float(random, 0, s1 > 0 ? s1 : 0) >= ssr_rand_float(random, 0, s2 > 0 ? s2 : 0) ? 1 : 2;
    if (ssr_rand(random, 70) == 1) {
        chance_penalty(m, random);
    } else if (ssr_rand(random, 11) == 1) {
        chance_free_kick(m, random);
    } else {
        chance_shoot(m, random);
    }
}

/* ------------------------------------------------------------ clock */

// c_TMatch.MatchOver: 1 when the match ends after the half that ends now.
// A league match ends at 90 minutes; a tie can need extra time, then
// penalties.
static int match_over(const SSRMatch *m) {
    const SSRMatchSetup *s = &m->setup;
    if (s->matchType == 3) return m->score[1] != m->score[2];
    if (s->matchType == 5) {
        int home = s->firstLegAway + m->score[1], away = s->firstLegHome + m->score[2];
        if (home != away) return 1;
        return 2 * s->firstLegAway + m->score[1] != 2 * m->score[2] + s->firstLegHome;
    }
    return 1;
}

static void half_ends(SSRMatch *m) {
    char text[96];
    if (m->half == 3) {
        // The half time of extra time: the game shows no break.
        m->minute = 105;
        m->half = 4;
        m->energy += 2;
        snprintf(text, sizeof(text), "%s %d - %d", text_of(m, "Half Time"), m->score[1], m->score[2]);
        add_line(m, 1, text, 0, 0);
        return;
    }
    if (m->half == 4) {
        m->minute = 120;
        m->half = 5;
        if (match_over(m)) {
            snprintf(text, sizeof(text), "%s %d - %d", text_of(m, "Full Time"), m->score[1], m->score[2]);
            add_line(m, 1, text, 0, 0);
            m->state = SSR_MATCH_FULL_TIME;
        } else {
            add_line(m, 1, text_of(m, "Penalties"), 0, 0);
            m->state = SSR_MATCH_PENALTIES;
        }
        return;
    }
    if (m->half == 2 && !match_over(m)) {
        m->minute = 90;
        m->half = 3;
        snprintf(text, sizeof(text), "%s %d - %d", text_of(m, "Extra Time"), m->score[1], m->score[2]);
        add_line(m, 1, text, 0, 0);
        m->state = SSR_MATCH_HALF_TIME;
        m->energy += 3;
        return;
    }
    if (m->half == 1) {
        m->windDirection += 180;
        m->minute = 45;
        m->half = 2;
        snprintf(text, sizeof(text), "%s %d - %d", text_of(m, "Half Time"), m->score[1], m->score[2]);
        add_line(m, 1, text, 0, 0);
        m->state = SSR_MATCH_HALF_TIME;
        m->energy += 5;
    } else {
        m->minute = 90;
        m->half = 3;
        snprintf(text, sizeof(text), "%s %d - %d", text_of(m, "Full Time"), m->score[1], m->score[2]);
        add_line(m, 1, text, 0, 0);
        m->state = SSR_MATCH_FULL_TIME;
    }
}

// The energy that one minute takes (GSDepletePlayerEnergy).
static void use_energy(SSRMatch *m) {
    if (!ssr_match_on_pitch(m)) return;
    float rate = m->setup.workRate == 1 ? 0.5f : m->setup.workRate == 3 ? 1.5f : 1.0f;
    m->energy -= 0.75f * rate + clampf((float)(m->setup.age - 23) * 0.01f, 0, 0.5f);
    if (m->energy < 0) m->energy = 0;
}

static int work_rate_chance(const SSRMatch *m, SSRRandom *random) {
    switch (m->setup.workRate) {
    case 1:
        return ssr_rand(random, 5) > 3;
    case 3:
        return 1;
    default:
        return ssr_rand(random, 3) <= 1;
    }
}

static void next_minute(SSRMatch *m, int now, SSRRandom *random) {
    m->minute++;
    m->matchTick = now;
    use_energy(m);
    if ((m->half == 1 && m->minute > 45) || (m->half == 2 && m->minute > 90) || (m->half == 3 && m->minute > 105) ||
        (m->half == 4 && m->minute > 120)) {
        half_ends(m);
        return;
    }
    SSRMatchSetup *s = &m->setup;
    // A substitute comes on, or the player goes off.
    if (s->subTime > 0 && m->minute == s->subTime) say(m, 1, "CMATCHTEXT_SUBON", 0, 0);
    if (s->subTime == 0 && m->minute > 60 && s->relationBoss < ssr_rand(random, 160) && m->goals == 0 &&
        m->passes < 2 && m->assists == 0 && m->chances > 3) {
        say(m, 1, "CMATCHTEXT_SUBOFF", 0, 0);
        say(m, 0, "CMATCHTEXT_SUBOFFRATING", 0, 0);
        s->subTime = -m->minute;
    } else if (s->subTime == 0 && m->energy < 1 && m->minute != 90) {
        say(m, 1, "CMATCHTEXT_SUBOFF", 0, 0);
        say(m, 0, "CMATCHTEXT_SUBOFFENERGY", 0, 0);
        s->subTime = -m->minute;
    }
    int value = m->minute >= 1 && m->minute <= 120 ? m->playerChance[m->minute - 1] : 0;
    if (ssr_match_on_pitch(m) && value > 0) {
        if (value == 2 || (value == 1 && ((s->subTime > 0 && m->chances == 0) || work_rate_chance(m, random)))) {
            create_chance(m, 1, random);
        }
    } else if (s->subTime < 0) {
        if (ssr_rand(random, 8) == 1) create_chance(m, 0, random);
    } else if ((s->focus != 1 && ssr_rand(random, 9) == 1) || (s->focus == 2 && ssr_rand(random, 8) == 1) ||
               (s->focus == 3 && ssr_rand(random, 7) == 1)) {
        create_chance(m, 0, random);
    }
}

// The wait before the next stage of a chance.
static int stage_delay(const SSRMatch *m) {
    float d = (float)m->textSpeed;
    if (m->textSpeed != 350) {
        if (m->stage == 3) d *= 2;
        if (m->stage == 5) d *= 2.5f;
        if (m->stage == 6) d *= 3;
    }
    return (int)d;
}

static void cpu_goal(SSRMatch *m, const char *key, int number) {
    m->score[m->attacking]++;
    m->lastGoalTime = m->minute;
    say_numbered(m, 0, key, number, m->attacking, 1);
}

// c_TMatch.m_UpdateChance: one stage of the texts of a chance.
static SSRMatchEvent run_stage(SSRMatch *m, int now, SSRRandom *random) {
    int other = m->attacking == 1 ? 2 : 1;
    switch (m->stage) {
    case 0:
        m->state = SSR_MATCH_CLOCK;
        break;
    case 1:
        m->stage = 2;
        // The text names the team that defends.
        m->attacking = other;
        say_numbered(m, 0, "CCHANCESTAGE_MOVEFORWARD", ssr_rand(random, 10), 0, 0);
        m->attacking = other == 1 ? 2 : 1;
        break;
    case 2:
        if (m->attacking != m->setup.myTeam ||
            (ssr_rand(random, 2) != 1 && (m->playerInBuildup == 0 || ssr_rand(random, 5) <= 2))) {
            chance_shoot(m, random);
        } else {
            chance_for_player(m, 0, random);
        }
        break;
    case 3: {
        int n = m->minute >= m->lastGoalTime + 5 ? 5 : 6;
        int total = m->score[1] + m->score[2];
        if (total > 3) n++;
        if (total > 5) n++;
        if (ssr_rand(random, n) == 1) {
            cpu_goal(m, "CMATCHTEXT_GOAL", fresh(random, 35, &m->lastGoal));
            m->matchTick = now + 3 * m->textSpeed;
        } else {
            say_numbered(m, 0, "CMATCHTEXT_MISS", fresh(random, 30, &m->lastMiss), m->attacking, 0);
        }
        m->stage = 0;
        break;
    }
    case 4:
        if (m->chanceType == 6) {
            // The intercept game is not in the remake yet: the player wins the
            // ball [I].
            m->stage = 9;
            say(m, 0, "CMATCHTEXT_CHANCEPLAYERWONBALL1", m->setup.myTeam, 0);
            break;
        }
        m->state = SSR_MATCH_PLAYER_CHANCE;
        return SSR_MATCH_EVENT_CHANCE;
    case 5:
        if (ssr_rand(random, 4) == 1) {
            cpu_goal(m, "CMATCHTEXT_FREEKICKGOAL", fresh(random, 5, &m->lastFreeKickGoal));
        } else {
            say_numbered(m, 0, "CMATCHTEXT_FREEKICKMISS", fresh(random, 5, &m->lastFreeKickMiss), m->attacking, 0);
        }
        m->matchTick = now + 3 * m->textSpeed;
        m->stage = 0;
        break;
    case 6:
        if (ssr_rand(random, 4) == 1) {
            say_numbered(m, 0, "CMATCHTEXT_PENALTYMISS", fresh(random, 5, &m->lastPenaltyMiss), m->attacking, 0);
        } else {
            cpu_goal(m, "CMATCHTEXT_PENALTYGOAL", fresh(random, 5, &m->lastPenaltyGoal));
        }
        m->matchTick = now + 3 * m->textSpeed;
        m->stage = 0;
        break;
    case 9:
        say(m, 0, "CMATCHTEXT_CHANCEPLAYERWONBALL2", m->setup.myTeam, 0);
        m->stage = 4;
        m->playerInBuildup = 1;
        m->chanceType = 1;
        break;
    case 10:
        m->stage = 0;
        say_numbered(m, 0, "CCHANCESTAGE_CHANCEBREAKSDOWN", ssr_rand(random, 10), 0, 0);
        break;
    case 11:
        say_numbered(m, 0, "CMATCHTEXT_CHANCEPLAYERDRIBBLEATTACK", ssr_rand(random, 5), m->setup.myTeam, 0);
        m->stage = 4;
        m->playerInBuildup = 1;
        m->chanceType = ssr_rand(random, 2) == 1 ? 2 : 8;
        break;
    default:
        m->stage = 0;
        break;
    }
    return SSR_MATCH_EVENT_NONE;
}

// c_TMatch.DoShootOut: the result when the shoot-out is over, else the
// next kick, one team then the other.
static void next_kick(SSRMatch *m) {
    if (shootout_complete(m)) {
        char text[256];
        snprintf(text, sizeof(text), "%s", text_of(m, "CMATCHTEXT_PENRESULT"));
        const char *winner = m->pens[1] > m->pens[2] ? m->setup.homeName : m->setup.awayName;
        replace(text, sizeof(text), "$clubname", winner ? winner : "");
        add_line(m, 0, text, m->pens[1] > m->pens[2] ? 1 : 2, 1);
        m->state = SSR_MATCH_FULL_TIME;
        return;
    }
    m->shootoutNumber++;
    m->attacking = m->shootoutNumber % 2 == 0 ? 2 : 1;
    m->stage = 6;
}

// c_TMatch.UpdatePenalties: a team steps up, then scores or misses. The
// player takes one of the kicks of the team.
static SSRMatchEvent shootout_step(SSRMatch *m, int now, SSRRandom *random) {
    switch (m->stage) {
    case 6: {
        int n = m->shootoutNumber;
        if (ssr_match_on_pitch(m) && m->attacking == m->setup.myTeam &&
            (n == m->penaltyTaker || n == m->penaltyTaker + 1)) {
            m->stage = 8;
            say(m, 0, "CMATCHTEXT_PENALTYSTEPUPPLAYER", m->attacking, 0);
        } else {
            m->stage = 7;
            say(m, 0, "CMATCHTEXT_PENALTYSTEPUP", m->attacking, 0);
        }
        break;
    }
    case 7:
        if (ssr_rand(random, 4) == 1) {
            say_numbered(m, 0, "CMATCHTEXT_PENALTYMISS", fresh(random, 5, &m->lastPenaltyMiss), m->attacking, 0);
        } else {
            m->pens[m->attacking]++;
            say_numbered(m, 0, "CMATCHTEXT_PENALTYGOAL", fresh(random, 5, &m->lastPenaltyGoal), m->attacking, 1);
        }
        m->matchTick = now + 5 * m->textSpeed;
        next_kick(m);
        if (m->state == SSR_MATCH_FULL_TIME) return SSR_MATCH_EVENT_FULL_TIME;
        break;
    case 8:
        m->chanceType = 4;
        m->state = SSR_MATCH_PLAYER_CHANCE;
        return SSR_MATCH_EVENT_CHANCE;
    }
    return SSR_MATCH_EVENT_NONE;
}

SSRMatchEvent ssr_match_update(SSRMatch *m, int now, SSRRandom *random) {
    if (m->state == SSR_MATCH_SHOOTOUT) {
        if (now <= m->matchTick + m->textSpeed * 2) return SSR_MATCH_EVENT_NONE;
        m->matchTick = now;
        return shootout_step(m, now, random);
    }
    if (m->state == SSR_MATCH_CLOCK) {
        if (now <= m->matchTick + m->textSpeed / 2) return SSR_MATCH_EVENT_NONE;
        next_minute(m, now, random);
        if (m->state == SSR_MATCH_HALF_TIME || m->state == SSR_MATCH_PENALTIES) return SSR_MATCH_EVENT_HALF_TIME;
        if (m->state == SSR_MATCH_FULL_TIME) return SSR_MATCH_EVENT_FULL_TIME;
        if (m->state == SSR_MATCH_STAGES) m->matchTick = now;
        return SSR_MATCH_EVENT_NONE;
    }
    if (m->state == SSR_MATCH_STAGES) {
        if (now <= m->matchTick + stage_delay(m)) return SSR_MATCH_EVENT_NONE;
        m->matchTick = now;
        return run_stage(m, now, random);
    }
    return SSR_MATCH_EVENT_NONE;
}

/* ------------------------------------------------- player chances */

SSRChanceSetup ssr_match_chance_setup(const SSRMatch *m, int technique, int power, int level) {
    const SSRMatchSetup *s = &m->setup;
    SSRChanceSetup c = {0};
    c.type = m->chanceType;
    c.minute = m->minute;
    c.teamStrength = s->myTeam == 1 ? s->homeStrength : s->awayStrength;
    c.opponentStrength = s->myTeam == 1 ? s->awayStrength : s->homeStrength;
    c.power = power;
    c.technique = technique;
    c.vision = s->vision;
    c.level = level;
    c.starLevel = s->starLevel;
    c.clubId = 1;
    c.careerApps = s->careerApps;
    c.chances = m->chances;
    c.windSpeed = m->windSpeed;
    c.windDirection = m->windDirection;
    c.weather = m->setup.weather;
    // The captain of a club match is captain 1 or 3, of a nation match 2 or
    // 3. Only a player that started the match draws runs.
    int cap = s->captain;
    int captain = s->level == 0 ? cap == 1 || cap == 3 : s->level == 1 ? cap == 2 || cap == 3 : 0;
    c.captain = captain && s->subTime == 0;
    return c;
}

static void goal_line(SSRMatch *m, int now) {
    m->score[m->setup.myTeam]++;
    m->lastGoalTime = m->minute;
    m->matchTick = now + 4 * m->textSpeed;
    char text[64];
    snprintf(text, sizeof(text), "%s", text_of(m, "Goal!"));
    for (char *p = text; *p; p++) *p = (char)toupper((unsigned char)*p);
    add_line(m, 0, text, m->setup.myTeam, 1);
}

// c_TMatch.m_PlayerPassed: the text and the ratings of a good pass.
static void passed(SSRMatch *m, const SSRChance *c, SSRRandom *random) {
    m->passes++;
    int type = m->chanceType;
    float dist = c->kickYards * YARD;
    char text[256];
    if (type == 9 || type == 10) {
        char key[48];
        snprintf(key, sizeof(key), "CMATCHTEXT_HEADERGOOD%d", ssr_rand(random, 5));
        snprintf(text, sizeof(text), "%s", text_of(m, key));
        rate(m, 4, 1);
    } else {
        if (c->actionType == 7) {
            char key[48];
            snprintf(key, sizeof(key), "CMATCHTEXT_CORNERGOOD%d", ssr_rand(random, 2));
            snprintf(text, sizeof(text), "%s", text_of(m, key));
        } else if (dist < 20 * YARD) {
            snprintf(text, sizeof(text), "%s", text_of(m, "CMATCHTEXT_PASSSIMPLE"));
            rate(m, 0, 1);
        } else if (dist > 30 * YARD) {
            snprintf(text, sizeof(text), "%s", text_of(m, "CMATCHTEXT_PASSFANTASTIC"));
            char yards[32], unit[16];
            snprintf(unit, sizeof(unit), "%s", text_of(m, "tla_Yards"));
            for (char *p = unit; *p; p++) *p = (char)tolower((unsigned char)*p);
            snprintf(yards, sizeof(yards), "%d %s", (int)c->kickYards, unit);
            replace(text, sizeof(text), "$dist", yards);
            rate(m, c->actionType == 8 ? 5 : 1, 1);
        } else {
            snprintf(text, sizeof(text), "%s", text_of(m, "CMATCHTEXT_PASSGREAT"));
            if (c->actionType != 3) rate(m, c->actionType == 8 ? 5 : 1, 1);
        }
        rate(m, 6, 1);
    }
    replace(text, sizeof(text), "$playername", m->setup.playerName ? m->setup.playerName : "");
    add_line(m, 0, text, m->setup.myTeam, 0);
}

// The distance of the start of the chance from the goal, in yards.
static float distance_to_goal_yards(const SSRChance *c) {
    float dx = c->info.startX - ssr_goal_centre(c->info.pitchSide), dy = c->info.startY - (float)SSR_GOAL_LINE;
    return (float)sqrt((double)(dx * dx + dy * dy)) / YARD;
}

// The skill that a shot or a pass of the chance rates.
static int rating_of(const SSRChance *c) {
    int type = c->info.chanceType;
    if (type == 11) return 7;
    if (type == 9 || type == 10) return 4;
    if (type == 3 || type == 4 || type == 7) return 6;
    if (type == 8) return 5;
    if (type == 1) return c->kickYards < 20 ? 0 : 1;
    return distance_to_goal_yards(c) < 25 ? 2 : 3;
}

// c_TMatch: a goal of the player at a level score in the 90th minute, or
// in the 120th minute of a match with extra time, wins the match. A second
// leg counts the score of the first leg. [I: the game also gives the award
// for some goals in the 90th minute of a second leg with an away goal. The
// remake does not.]
static int last_minute_winner(const SSRMatch *m) {
    int type = m->setup.matchType, one = m->score[1], two = m->score[2];
    if (type == 5) {
        one += m->setup.firstLegAway;
        two += m->setup.firstLegHome;
    }
    int late = m->minute == 90 || ((type == 3 || type == 5) && m->minute == 120);
    return late && one == two;
}

void ssr_match_chance_done(SSRMatch *m, const SSRChance *c, int now, SSRRandom *random) {
    if (m->shootoutNumber > 0) {
        // A kick of the shoot-out counts only in the shoot-out.
        m->state = SSR_MATCH_SHOOTOUT;
        if (c->result == SSR_RESULT_GOAL) {
            m->pens[m->setup.myTeam]++;
            say_numbered(m, 0, "CMATCHTEXT_PENALTYGOAL", fresh(random, 5, &m->lastPenaltyGoal), m->setup.myTeam, 1);
        } else {
            say_numbered(m, 0, "CMATCHTEXT_PENALTYMISS", fresh(random, 5, &m->lastPenaltyMiss), m->setup.myTeam, 0);
        }
        m->matchTick = now + 5 * m->textSpeed;
        next_kick(m);
        return;
    }
    m->state = SSR_MATCH_CLOCK;
    m->stage = 0;
    m->shotsOnTarget += c->shotsOnTarget;
    int pass = m->chanceType == 1 || m->chanceType == 9;
    // UpdateYardage: a goal measures from the start to the centre of the
    // goal, a pass from the start to the ball. A chance of type 9 or 10 is
    // a header.
    int header = c->info.chanceType == 9 || c->info.chanceType == 10;
    if (c->result == SSR_RESULT_GOAL) {
        float dist = distance_to_goal_yards(c) * YARD;
        if (dist > m->furthest[0]) m->furthest[0] = dist;
        if (header && dist > m->furthest[2]) m->furthest[2] = dist;
    } else if (c->result == SSR_RESULT_PASS || c->result == SSR_RESULT_ASSIST) {
        float dist = c->kickYards * YARD;
        if (dist > m->furthest[1]) m->furthest[1] = dist;
        if (header && dist > m->furthest[3]) m->furthest[3] = dist;
    }
    m->postHits += c->postHits;
    switch (c->result) {
    case SSR_RESULT_GOAL:
        m->goals++;
        if (c->actionType == 7) m->cornerGoals++;
        if (last_minute_winner(m)) m->lastMinuteWinner = 1;
        rate(m, rating_of(c), 1);
        goal_line(m, now);
        break;
    case SSR_RESULT_ASSIST:
        m->assists++;
        goal_line(m, now);
        break;
    case SSR_RESULT_PASS:
        passed(m, c, random);
        m->matchTick = now + 4 * m->textSpeed;
        // The attack goes on, or breaks down.
        if (c->ball.y > c->info.startY && ssr_rand(random, 4) != 1) {
            m->stage = 10;
        } else if ((c->kickYards > 30 && ssr_rand(random, 4) != 1) || ssr_rand(random, 2) == 1) {
            m->stage = 1;
        } else {
            m->stage = 10;
        }
        m->state = SSR_MATCH_STAGES;
        break;
    case SSR_RESULT_DRIBBLED:
        // m_ChancePlayerDribbled: the attack goes on at stage 11.
        m->dribbles++;
        rate(m, 7, 1);
        m->matchTick = now + 4 * m->textSpeed;
        say_numbered(m, 0, "CMATCHTEXT_CHANCEPLAYERDRIBBLED", ssr_rand(random, 5), m->setup.myTeam, 0);
        m->stage = 11;
        m->state = SSR_MATCH_STAGES;
        break;
    case SSR_RESULT_INTERCEPTED:
    case SSR_RESULT_LOST:
        rate(m, rating_of(c), 0);
        say(m, 0, pass ? "CMATCHTEXT_PASSBAD" : "CMATCHTEXT_CHANCEMISSED", m->setup.myTeam, 0);
        break;
    default:
        rate(m, rating_of(c), 0);
        say(m, 0, "CMATCHTEXT_CHANCEMISSED", m->setup.myTeam, 0);
        break;
    }
    m->playerInBuildup = m->state == SSR_MATCH_STAGES;
}

int ssr_match_take_line(SSRMatch *m, SSRMatchLine *line) {
    if (m->lineCount == 0) return 0;
    *line = m->lines[m->lineFirst];
    m->lineFirst = (m->lineFirst + 1) % SSR_MATCH_LINES;
    m->lineCount--;
    return 1;
}
