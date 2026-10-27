// ssr_season: the competitions of a career as the game runs them
// (c_TCompetition, c_TFixture, c_TTeamPool, c_TTableData). The season sets
// up the leagues and the cups of the nation of the player, plays the
// matches of the other teams, keeps the tables, and moves the clubs up and
// down at the end of the season.

#ifndef SSR_SEASON_H
#define SSR_SEASON_H

#include "ssr_random.h"
#include "ssr_world.h"

// The days of a date: a year has 52 weeks of 7 days. Day 1 is a Monday.
int ssr_date_make(int day, int week, int year);
int ssr_date_day(int sdate);
int ssr_date_week(int sdate);
int ssr_date_year(int sdate);

// The match types of a fixture.
enum {
    SSR_FIXTURE_LEAGUE = 1,
    // A cup tie that has a replay after a draw.
    SSR_FIXTURE_REPLAY_ON_DRAW = 2,
    // A cup tie with extra time and penalties (also a replay).
    SSR_FIXTURE_EXTRA_TIME = 3,
    SSR_FIXTURE_FIRST_LEG = 4,
    SSR_FIXTURE_SECOND_LEG = 5,
};

// The result types of a fixture.
enum {
    SSR_RESULT_NORMAL = 1,
    SSR_RESULT_EXTRA_TIME = 2,
    SSR_RESULT_PENALTIES = 3,
    SSR_RESULT_AWAY_GOALS = 4,
    SSR_RESULT_AGGREGATE = 5,
};

typedef struct {
    int sdate;
    int matchType;
    // League: the round from 1. Cup: 1, and 2 for a replay.
    int round;
    // The group from 1.
    int group;
    // 0, or 1 and 2 for a tie with two legs.
    int leg;
    // The table ids of the teams in the pool of the group. 0, or an id
    // that is not in the pool, is no team.
    int home, away;
    // 0 not played, 1 played, -1 the match of the player, played but not
    // yet counted.
    int result;
    int resultType;
    int score1, score2, pen1, pen2;
    int level;
    int compId;
    int randNo;
} SSRFixture;

// A row of a table. A row can be in two pools: a league continuation takes
// the same row (c_TTeamPool.AddItemLeagueContinuation).
typedef struct {
    // The table id in the pool, from 1.
    int id;
    // The club id, or the nation id in an international competition.
    int teamId;
    char name[16];
    int strength;
    int randNo;
    float longLat;
    int played, won, drawn, lost, goalsFor, goalsAgainst, points;
    // The position before the last round, or -1.
    int prevPosition;
} SSRTableRow;

typedef struct {
    // The rows of the pool, as indexes into SSRSeason.rows.
    int *rows;
    int count, capacity;
} SSRPool;

typedef struct {
    const SSRCompetition *data;
    int priority;
    int startYear;
    SSRPool *pools;
    int poolCount;
    SSRFixture *fixtures;
    int fixtureCount, fixtureCapacity;
    // The replays that wait for the end of the list.
    SSRFixture *replays;
    int replayCount, replayCapacity;
    // The promotion places of this competition, and the places that send
    // teams to it, as indexes into SSRWorld.promotions, by place.
    int *places;
    int placeCount;
    int *placesToMe;
    int placesToMeCount;
} SSRSeasonComp;

// A club as the season changes it.
typedef struct {
    const SSRClub *data;
    int strength;
    int league;
    int continental;
    int randNo;
} SSRSeasonClub;

typedef struct {
    const SSRNation *data;
    int randNo;
} SSRSeasonNation;

// What the season needs to know of the player.
typedef struct {
    int sdate;
    int clubId;
    int nationId;
    int careerApps;
} SSRSeasonPlayer;

typedef struct {
    const SSRWorld *world;
    SSRRandom *random;
    SSRSeasonComp *comps;
    int compCount;
    // The order in which the season plays the competitions (the m_glist
    // of c_TCompetition), as indexes into comps.
    int *compOrder;
    // All the clubs of the data set, as the last load left them. Only the
    // clubs in clubList are loaded (the m_glist of c_TClub).
    SSRSeasonClub *clubs;
    int clubCount;
    int *clubList;
    int clubListCount;
    SSRSeasonNation *nations;
    int nationCount;
    SSRTableRow *rows;
    int rowCount, rowCapacity;
    SSRSeasonPlayer player;
    // The copy of a club that is not loaded (ssr_season_club with load).
    SSRSeasonClub scratch;
    // The last news of a cup winner: the winner and the competition.
    char winnerTla[16];
    char winnerCompetition[16];
    int hasWinnerNews;
} SSRSeason;

// Makes the season of a new career: all the clubs of the data set are
// loaded, in the order of the file, with no competition set up.
SSRSeason *ssr_season_create(const SSRWorld *world, SSRRandom *random);
void ssr_season_free(SSRSeason *season);

// c_TCompetition.SetUpCompetitionsAll: the competitions of the nation of
// the club of the player, of its continent, and (each fourth year) of the
// nations. After it only the clubs of the nation are loaded.
void ssr_season_set_up(SSRSeason *season);

// The competition with the id, or NULL.
SSRSeasonComp *ssr_season_comp(SSRSeason *season, int id);
// The club with the id when it is loaded. With load, a club that is not
// loaded comes from the data set (a copy that the season does not keep).
SSRSeasonClub *ssr_season_club(SSRSeason *season, int id, int load);
// A row of a pool by its table id, or NULL.
SSRTableRow *ssr_season_row(SSRSeason *season, const SSRPool *pool, int id);

// The club id or nation id of a side of a fixture, or 0.
int ssr_fixture_home_team(SSRSeason *season, const SSRFixture *fixture);
int ssr_fixture_away_team(SSRSeason *season, const SSRFixture *fixture);
// The table id of the winner and of the loser of a played fixture, or 0.
int ssr_fixture_winner(SSRSeason *season, const SSRFixture *fixture);
int ssr_fixture_loser(SSRSeason *season, const SSRFixture *fixture);
// 1 when the club or nation team won the final of the cup with the
// played fixture (IsCupFinal(false), and not a first leg).
int ssr_season_won_final(SSRSeason *season, const SSRSeasonComp *comp, const SSRFixture *fixture, int team);

// Sorts the rows of the pool as a league table (mode 4), and gives the
// position of the team from 1, or 0.
int ssr_season_position(SSRSeason *season, SSRPool *pool, int teamId);
// Sorts the rows of the pool by a mode of c_TTableData.Compare.
void ssr_season_sort_pool(SSRSeason *season, SSRPool *pool, int mode);

// c_TCompetition.PlayFixtures: plays each fixture of each competition up
// to the date of the player. Returns 1 when a played fixture counts for the
// player (an international, a continental competition, or the nation of
// the club).
int ssr_season_play_fixtures(SSRSeason *season);

// c_TPlayer.GetNextFixture: the next fixture of the club or the nation of
// the player, or NULL.
SSRFixture *ssr_season_next_fixture(SSRSeason *season);

// c_TPlayer.SkipToDate: plays each day up to the day before sdate.
void ssr_season_skip_to(SSRSeason *season, int sdate);

// c_TPlayer.PlayToDate: plays the days after the date up to sdate, or
// further when sdate is behind. Returns 1 when the season ends (the date
// is the first day of a new year).
int ssr_season_play_to(SSRSeason *season, int sdate);

// c_TPlayer.MyMatchPlayed: the score of the match of the player counts in
// the table. A draw in a cup tie goes to extra time and penalties, or to a
// replay, as the season plays the ties of other teams [I: the match of the
// remake has no extra time yet].
void ssr_season_my_result(SSRSeason *season, SSRFixture *fixture, int score1, int score2);

// The result of a tie of the player that went to extra time in the match:
// the score after 120 minutes, and the penalties of a shoot-out (0 and 0
// when there was none).
void ssr_season_my_result_after_extra_time(SSRSeason *season, SSRFixture *fixture, int score1, int score2,
                                           int pen1, int pen2);

// The fixture with the competition, the date, and the sides, or NULL. A
// fixture keeps no fixed place in its list, so the game finds it again.
SSRFixture *ssr_season_find(SSRSeason *season, int compId, int sdate, int home, int away);

// c_TPlayer.GetPrevFixture: the last played fixture of the player, or NULL.
SSRFixture *ssr_season_prev_fixture(SSRSeason *season);

// c_TPlayer.GetRestDays: the days before the next match, from 3 to 15.
int ssr_season_rest_days(SSRSeason *season);

// c_TCompetition.GetNextRound, GetPrevRound, and GetRoundDate.
int ssr_season_next_round(const SSRSeasonComp *comp);
int ssr_season_prev_round(const SSRSeasonComp *comp);
int ssr_season_round_date(const SSRSeasonComp *comp, int round);

// c_TCompetition.GetCompetitionType: 1 national league, 2 national cup, 3
// continental, 4 international, 0 other.
int ssr_season_comp_type(SSRSeason *season, const SSRSeasonComp *comp);

// c_TCompetition.AllFixturesPopulated: 1 when each fixture has both teams.
int ssr_season_all_populated(SSRSeason *season, const SSRSeasonComp *comp);

// c_TCompetition.IsPromotionPlace and IsRelegationPlace, for the colours of
// a table.
int ssr_season_is_promotion_place(const SSRSeason *season, const SSRSeasonComp *comp, int position);
int ssr_season_is_relegation_place(const SSRSeason *season, const SSRSeasonComp *comp, int position);

// c_TClub.GetActualLeagueId: the last national league in the play order
// whose table has the club, or 0.
int ssr_season_actual_league(SSRSeason *season, int clubId);

// The group of the team in the competition, from 1, or 0.
int ssr_season_group_of(SSRSeason *season, const SSRSeasonComp *comp, int teamId);

// c_TPlayer.SetAllPreviousLeaguePositions: keeps the positions of each
// table before the next round, for the arrows of the table.
void ssr_season_keep_positions(SSRSeason *season);

// c_TPlayer.NewSeason: loads all the clubs again, keeps the leagues, the
// strengths, and the continental places of the clubs of the nation, and
// sets up the competitions of the new year.
void ssr_season_new_season(SSRSeason *season);

// Writes and reads the state of the season (the save of c_TPlayer). The
// save sorts the competitions by id, as the game does.
int ssr_season_save(SSRSeason *season, const char *path);
int ssr_season_load(SSRSeason *season, const char *path);

#endif
