#include "ssr_aftermatch.h"

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

int ssr_aftermatch_work_rate(const int t[3]) {
    if (t[0] > t[1] + t[2]) return -2;
    if (t[0] > t[1] && t[0] > t[2]) return -1;
    if (t[2] > t[0] + t[1]) return 2;
    if (t[2] > t[0] && t[2] > t[1]) return 1;
    return 0;
}

// The match rating (c_TScreen_MatchStats.SetUpScreen).
// The fair rating of the remake. A quiet match is 6, a goal is about one
// point, and a defeat costs a little. A star man has 9 or more in a win.
static void fair_rating(const SSRAfterMatchFacts *f, SSRAfterMatch *out) {
    float misses = (float)(f->chances - f->passes - f->goals);
    float r = 6.0f + (float)f->goals * 1.1f + (float)f->assists * 0.7f;
    r += clampf((float)f->passes * 0.35f, 0, 1.4f);
    r += (float)f->intercepts * 0.1f + (float)f->dribbles * 0.1f;
    r += (float)clampi(f->shotsOnTarget, 0, 3) * 0.15f - 0.12f * misses;
    if (f->opponentScore > f->myScore) {
        out->result = -1;
        out->outcome = 3;
        r -= 0.3f + 0.1f * (float)(f->opponentScore - f->myScore);
    } else if (f->opponentScore == f->myScore) {
        out->result = 0;
        out->outcome = 2;
    } else {
        out->result = 1;
        out->outcome = 1;
        r += 0.3f;
    }
    if (f->subTime > 0) r -= 0.3f;
    r = clampf(r, 3, 10);
    out->starMan = r >= 9 && out->result == 1;
    out->rating = r;
}

static void rating(const SSRAfterMatchFacts *f, SSRAfterMatch *out) {
    if (f->fair) {
        fair_rating(f, out);
        return;
    }
    float misses = (float)(f->chances - f->passes - f->goals);
    float r;
    if (f->hardMode) {
        r = 0;
        if (f->subTime > 0) r += 1;
    } else {
        r = 1;
        if (f->subTime > 0) r += 2;
        if (f->starLevel < 2) r += 1;
    }
    r = r + (float)f->goals * 3.2f;
    r = r + (float)f->assists * 1.3f;
    r = r + clampf((float)f->passes * 1.1f, 0, 6.5f);
    r = r + (float)f->intercepts * 0.15f;
    r = (r + (float)f->dribbles * 0.25f) - 0.175f * misses;
    if (f->hardMode) {
        r += (float)clampi(f->shotsOnTarget, 0, 4) * 0.6f;
    } else {
        r += (float)clampi(f->shotsOnTarget, 0, 3) * 0.5f;
    }
    if (f->opponentScore > f->myScore) {
        out->result = -1;
        r -= (float)(f->opponentScore - f->myScore) * 0.5f;
        out->outcome = 3;
    } else if (f->opponentScore == f->myScore) {
        out->result = 0;
        out->outcome = 2;
    } else {
        out->result = 1;
        out->outcome = 1;
    }
    if (r < 6 && misses < 2) r += 1;
    r = clampf(r, 1, 10);
    if (r > 9 && f->subTime > 60 && f->goals + f->assists < 3) r = 9;
    int opponentGoals = f->home ? f->score2 : f->score1;
    int won = f->myScore > f->opponentScore;
    if (r > 9 && won) {
        if (r == 10 || f->goals > opponentGoals) {
            r = 10;
            out->starMan = 1;
        }
    } else if (r >= 10) {
        out->starMan = 1;
    }
    out->rating = r;
}

// The relation changes (c_TScreen_MatchStats, the fallback form). The
// home and the away branches differ, as in the game.
static void relations(const SSRAfterMatchFacts *f, SSRAfterMatch *out) {
    float r = out->rating, misses = (float)(f->chances - f->passes - f->goals);
    int boss = (int)(r > 5 ? r - 5 : -(6 - r));
    int team = f->passes - 2;
    team = (int)((float)(team + f->assists * 2) - misses);
    int fans = 0;
    int wr = f->workRate;
    if (wr == -2) {
        boss--;
        team--;
        fans--;
    }
    int s1 = f->score1, s2 = f->score2;
    float t1 = f->homeStrength, t2 = f->awayStrength;
    int mine = f->home ? s1 : s2, theirs = f->home ? s2 : s1;
    if (mine > theirs) {
        if (r > 5) fans = (int)(r - 5);
    } else if (s1 == s2) {
        if (f->home) {
            if (t1 > t2 + 10) fans = -1;
            else if (t2 > t1 + 10) fans = 1;
            if (wr < 0) {
                fans--;
                boss--;
                team--;
            }
        } else {
            if (t1 > t2 + 10) fans = 1;
            else if (t2 > t1 + 10) fans = -1;
        }
    } else {
        if (r <= 5) fans = (int)(-(6 - r));
        if (f->home) {
            int before = fans;
            fans = before - 1;
            if (wr < 0) {
                fans = before - 2;
                boss--;
                team--;
            }
        } else if (wr < 0) {
            fans--;
            boss--;
            team--;
        }
    }
    if (f->starLevel < 2) {
        boss++;
        team++;
        fans++;
    } else if (f->starLevel > 3) {
        boss--;
        team--;
        fans--;
    }
    if (f->hardMode) {
        boss = clampi(boss - 1, -10, 4);
        team = clampi(team - 1, -10, 4);
        fans = clampi(fans - 1, -10, 4);
    } else {
        boss = clampi(boss, -5, 4);
        team = clampi(team, -5, 4);
        fans = clampi(fans, -5, 4);
    }
    if (f->goals + f->assists >= 3) {
        boss++;
        team++;
        fans++;
    } else if (f->goals > 0 && (f->goals >= s2 || f->goals >= s1)) {
        boss = clampi(boss, 0, 4);
        team = clampi(team, 0, 4);
        fans = clampi(fans, 0, 4);
    }
    out->boss = boss;
    out->team = team;
    out->fans = fans;
}

void ssr_aftermatch_rate(const SSRAfterMatchFacts *facts, SSRAfterMatch *out) {
    *out = (SSRAfterMatch){0};
    rating(facts, out);
    relations(facts, out);
}

int ssr_star_rating_add(int rating, int change) {
    return clampi(rating + change, (rating / 100) * 100 + 1, 100000);
}

int ssr_energy_recovery(int restDays, int age) {
    float e = 8.5f * (float)restDays;
    if (age > 41) {
        e *= 0.5f;
    } else if (age > 36) {
        e *= 0.6f;
    } else if (age > 31) {
        e *= 0.7f;
    } else if (age > 26) {
        e *= 0.8f;
    } else if (age > 21) {
        e *= 0.9f;
    }
    return clampi((int)e, 1, 100);
}
