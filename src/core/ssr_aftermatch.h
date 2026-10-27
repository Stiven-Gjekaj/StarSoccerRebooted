// ssr_aftermatch: the numbers that come after a career match
// (c_TScreen_MatchStats and c_TScreen_MatchReview): the match rating, the
// changes of the relations, the star rating, and the energy that the rest
// days give. The arithmetic is in 32-bit floats, as in the game.

#ifndef SSR_AFTERMATCH_H
#define SSR_AFTERMATCH_H

// The facts of the match for the player.
typedef struct {
    int goals, assists, passes, chances, intercepts, dribbles, shotsOnTarget;
    // The scores with the penalties of a shoot-out, and without them.
    int myScore, opponentScore;
    int score1, score2;
    // 1 when the player plays for the home team.
    int home;
    float homeStrength, awayStrength;
    // 0 for a starter, the minute for a substitute, less than 0 after the
    // player went off.
    int subTime;
    int starLevel;
    int hardMode;
    // c_TPlayer.GetOverallWorkRate: -2 to 2 from the minutes at each work
    // rate.
    int workRate;
} SSRAfterMatchFacts;

typedef struct {
    float rating;
    // 1 won, 0 draw, -1 lost, and the Match.Outcome of the layout: 1 won,
    // 2 draw, 3 lost.
    int result, outcome;
    int starMan;
    // The changes of the relations with the boss, the team, and the fans.
    int boss, team, fans;
} SSRAfterMatch;

// GetOverallWorkRate: the minutes at work rate 1, 2, and 3.
int ssr_aftermatch_work_rate(const int minutes[3]);

// The match rating and the relation changes (c_TScreen_MatchStats).
void ssr_aftermatch_rate(const SSRAfterMatchFacts *facts, SSRAfterMatch *out);

// UpdateStarRating: the rating goes up or down by change, but never under
// the floor of its level. The level is rating / 100 + 1.
int ssr_star_rating_add(int rating, int change);

// GetEnergyRecovery: the energy that the rest days give at the age.
int ssr_energy_recovery(int restDays, int age);

#endif
