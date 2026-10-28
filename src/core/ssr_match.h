// ssr_match: the flow of a career match (c_TMatch): the clock, the chances
// of the teams, the chances of the player, the texts of the commentary,
// the score, the energy of the player, and the half time and the full time.
// The chances of the player run in ssr_chance. The times are in
// milliseconds.

#ifndef SSR_MATCH_H
#define SSR_MATCH_H

#include "ssr_chance.h"
#include "ssr_random.h"

typedef enum {
    SSR_MATCH_PREMATCH = 0,
    SSR_MATCH_CLOCK = 1,
    // A chance of the player runs. ssr_match_chance_done ends it.
    SSR_MATCH_PLAYER_CHANCE = 2,
    SSR_MATCH_FULL_TIME = 5,
    // The match ends with a penalty shoot-out: the break before it, and the
    // kicks.
    SSR_MATCH_PENALTIES = 7,
    SSR_MATCH_SHOOTOUT = 8,
    // The texts of a chance, one stage at a time.
    SSR_MATCH_STAGES = 6,
    SSR_MATCH_HALF_TIME = 9,
} SSRMatchState;

typedef enum {
    SSR_MATCH_EVENT_NONE,
    // A chance of the player starts: chanceType tells which.
    SSR_MATCH_EVENT_CHANCE,
    SSR_MATCH_EVENT_HALF_TIME,
    SSR_MATCH_EVENT_FULL_TIME,
} SSRMatchEvent;

typedef struct {
    // The short names of the teams (TLA), and their strengths.
    const char *homeName, *awayName;
    float homeStrength, awayStrength;
    // 1 when the player plays for the home team, 2 for the away team.
    int myTeam;
    // The short name of the player.
    const char *playerName;
    // 1 defensive, 2 normal, 3 attacking.
    int focus;
    // 1 light, 2 normal, 3 hard.
    int workRate;
    float energy;
    int age;
    // The relations with the team and the boss, from 0 to 100.
    int relationTeam, relationBoss;
    // 1 league captain, 2 cup captain, 3 both. level is 0 for a league match.
    int captain, level;
    int setPieces, vision;
    int careerApps;
    int starLevel;
    // 0: the player starts. More than 0: the minute that the player comes on.
    int subTime;
    // 1 when the player likes to dribble.
    int dribbling;
    // The type of the fixture (SSR_FIXTURE_*). A cup tie with extra time
    // (3) or a second leg (5) can go on after 90 minutes. A second leg has
    // the score of the first leg: home is the home team of the first leg,
    // the away team now.
    int matchType;
    int firstLegHome, firstLegAway;
    // The weather of the remake: 0 dry, 1 rain, 2 snow. A wet pitch lets
    // the ball roll further, and snow stops it sooner
    // (SSRChanceSetup.weather).
    int weather;
    // A big match of the remake (SSR_BIG_*): the stadium is full, and the
    // other team raises its game.
    int bigMatch;
    // The stadium of the home team from a data pack: the name, or NULL,
    // and the seats, or 0. The seats choose the size of the stadium.
    const char *stadium;
    int capacity;
    // The position of the player (SSR_CAREER_*): it chooses the mix of the
    // moments. 0, a forward, has the moments of 4.32, and a few blocks
    // when blocks is 1.
    int position;
    int blocks;
} SSRMatchSetup;

// The big matches of the remake.
enum {
    SSR_BIG_NONE,
    // A league match near the end of the season that the club must win:
    // for the title, for promotion, or to stay up.
    SSR_BIG_MUST_WIN,
    // The final of a cup.
    SSR_BIG_FINAL,
};

// One row of the commentary.
typedef struct {
    // The minute, or "" for a line that goes on from the line before.
    char minute[8];
    char text[256];
    // 0 neutral, 1 the home team, 2 the away team.
    int team;
    int goal;
} SSRMatchLine;

#define SSR_MATCH_LINES 64
#define SSR_MATCH_RATINGS 32

typedef struct {
    SSRMatchSetup setup;
    const char *(*localise)(void *context, const char *tag);
    void *context;
    SSRMatchState state;
    int minute, half;
    // 1200 slow, 800 normal, 350 fast.
    int textSpeed;
    int matchTick;
    // The stage of a chance (c_TMatch.m_chancestage), its type, and the
    // team that attacks: 1 home, 2 away.
    int stage, chanceType, attacking;
    int score[3];
    int playerChance[120];
    // The stats of the player in this match.
    int goals, assists, passes, chances, shotChances, dribbles, shotsOnTarget;
    // The moments in defence that the player won: saves and blocks.
    int defends;
    // UpdateYardage: the furthest goal, pass, headed goal, and headed pass
    // of the match in pixels.
    float furthest[4];
    // The shots on a post, the goals from a corner, and 1 after a goal
    // that broke a level score in the last minute.
    int postHits, cornerGoals, lastMinuteWinner;
    int playerInBuildup;
    int lastGoalTime;
    float energy;
    float windSpeed, windDirection;
    // The last text of each kind, so that a text does not come two times.
    int lastShoot, lastGoal, lastMiss, lastChanceForPlayer, lastPenaltyGoal, lastPenaltyMiss, lastFreeKickGoal,
        lastFreeKickMiss;
    int lastFocus, lastWorkRate, focusTextTime, workRateTextTime;
    // The commentary rows that the screen did not take yet.
    SSRMatchLine lines[SSR_MATCH_LINES];
    int lineFirst, lineCount;
    // The ratings of the chances: the index of the skill, and 1 for a
    // success (TPlayer.p_UpdateRating).
    int ratingIndex[SSR_MATCH_RATINGS], ratingSuccess[SSR_MATCH_RATINGS];
    int ratingCount;
    // The size of the stadium of the home team, from 0 to 4, and the place
    // of the player in a penalty shoot-out.
    int stadiumSize, penaltyTaker;
    // The penalties of each team in a shoot-out, and the number of the kick
    // (c_TMatch.m_penshootoutnumber).
    int pens[3];
    int shootoutNumber;
} SSRMatch;

// Makes the match before the kick-off: the minutes of the chances of the
// player and the wind. It takes random numbers.
void ssr_match_start(SSRMatch *match, const SSRMatchSetup *setup, const char *(*localise)(void *, const char *),
                     void *context, SSRRandom *random);

// The Play button: the kick-off, the second half, extra time, the
// shoot-out, or the end. Returns 1 when the match is over and the screen
// can go.
int ssr_match_play(SSRMatch *match, int now);

// One frame. Returns what the screen must do.
SSRMatchEvent ssr_match_update(SSRMatch *match, int now, SSRRandom *random);

// The facts of the chance of the player for ssr_chance.
SSRChanceSetup ssr_match_chance_setup(const SSRMatch *match, int technique, int power, int level);

// The end of a chance of the player. chance is the ended chance.
void ssr_match_chance_done(SSRMatch *match, const SSRChance *chance, int now, SSRRandom *random);

// Takes the next commentary row. Returns 0 when there is none.
int ssr_match_take_line(SSRMatch *match, SSRMatchLine *line);

// 1 while the player is on the pitch.
int ssr_match_on_pitch(const SSRMatch *match);

#endif
