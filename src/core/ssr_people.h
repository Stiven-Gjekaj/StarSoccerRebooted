// ssr_people: the other players of the world. In 4.32 the player of the
// user is the only player with a career, and a club is a strength number.
// Here each club has a squad of made-up players. They play, score, get
// better and older, move to other clubs, and retire. New young players
// come each season, so the world stays full.
//
// The people take their random numbers from a stream of their own. The
// stream starts at the seed of the world, so two careers with the same
// seed get the same world, and the numbers of the career do not change.

#ifndef SSR_PEOPLE_H
#define SSR_PEOPLE_H

#include <stddef.h>

#include "ssr_random.h"
#include "ssr_world.h"

// The positions of a player.
enum {
    SSR_POS_KEEPER,
    SSR_POS_DEFENDER,
    SSR_POS_MIDFIELDER,
    SSR_POS_ATTACKER,
    SSR_POS_COUNT,
};

// The players in a squad that a club keeps.
#define SSR_SQUAD_SIZE 22
// The most real players of a pack that a squad takes.
#define SSR_SQUAD_MAX 40

typedef struct {
    int id;
    char first[24];
    char last[32];
    int nation;
    // The club, or 0 for no club.
    int club;
    // The year of the birth, in the years of the career (ssr_career_year).
    // The age in a year is year - born.
    int born;
    int position;
    // The skill from 1 to 100, and the skill that the player can reach.
    int skill, potential;
    // The days of an injury, or 0.
    int injury;
    // The year of the retirement, or 0.
    int retired;
    // 1 for a player who started in the same year as the player of the
    // user, at the same age. SSR_FORMER_PLAYER for the player of the last
    // career of the user, who still plays in the world.
    int rival;
    int apps, goals, assists;
    int seasonApps, seasonGoals, seasonAssists;
} SSRPerson;

// The styles of a manager.
enum {
    SSR_STYLE_BALANCED,
    // A youth manager picks a young player before an older player of
    // nearly the same skill.
    SSR_STYLE_YOUTH,
    // An attacking manager plays with three attackers.
    SSR_STYLE_ATTACKING,
    SSR_STYLE_COUNT,
};

// The manager of a club.
typedef struct {
    int club;
    char first[24];
    char last[32];
    int nation;
    int born;
    int style;
    // The year of the start at the club.
    int since;
} SSRManager;

typedef struct SSRNames SSRNames;

// Reads the name lists (data/names.txt of the remake, or
// world/Names.txt of a pack). Returns NULL when the text has no names.
SSRNames *ssr_names_parse(const char *text);
void ssr_names_free(SSRNames *names);

// Adds the real players of the clubs (world/Players.txt of a pack). The
// text has a header row and the columns club, first, last, position,
// nation, age, skill, and potential. position is GK, DEF, MID, ATT, or MGR
// for the manager. club is the id of the club. A cell that is empty or not
// there is made up: the nation, the age, and the skill. Returns the number
// of rows that it added. ssr_people_make_squad then makes the squad of a
// club from its real players, and fills each position with made-up
// players.
int ssr_names_add_players(SSRNames *names, const char *text);

// Picks a first and a last name for a player of the nation. A nation with
// no group of its own takes the group of its continent, then the first
// group. Returns 0 when there are no names.
int ssr_names_pick(const SSRNames *names, int nation, int continent, SSRRandom *random, char *first,
                   size_t firstCapacity, char *last, size_t lastCapacity);

typedef struct {
    SSRPerson *list;
    int count, capacity;
    int nextId;
    // The seed of the world, and the stream of the people.
    int seed;
    SSRRandom random;
    // The year of the world: the last new season or new squad.
    int year;
    // The transfers of the last new season: the player, the old club, and
    // the new club. They are not in the file.
    int moves[16][3];
    int moveCount;
    // The managers of the clubs with a squad.
    SSRManager *managers;
    int managerCount, managerCapacity;
    // The clubs of the last new season with a new manager. They are not
    // in the file.
    int sacked[16];
    int sackedCount;
} SSRPeople;

SSRPeople *ssr_people_create(int seed);
void ssr_people_free(SSRPeople *people);

// The player with the id, or NULL.
SSRPerson *ssr_people_find(SSRPeople *people, int id);

// Makes a squad for the club when it has none. The skills follow the
// strength of the club. Returns the number of new players.
int ssr_people_make_squad(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int club, int year);

// The manager of the club, or NULL.
SSRManager *ssr_people_manager(SSRPeople *people, int club);

// The end of a season for the managers: a manager of a club in the
// bottom places of its league can go, and a new manager takes the place.
// bottom lists the clubs in the bottom places. Returns the number of new
// managers.
int ssr_people_judge_managers(SSRPeople *people, const SSRWorld *world, const SSRNames *names, const int *bottom,
                              int bottomCount, int year);

// Puts the players of the club into out, up to max. Returns the number of
// players of the club.
int ssr_people_squad(SSRPeople *people, int club, SSRPerson **out, int max);

// A club plays a match: the best eleven fit players get an appearance,
// and the goals go to scorers and assists. The players of the club may get
// an injury.
void ssr_people_play(SSRPeople *people, int club, int goals);

// Makes the rivals of the player of the user: players of the same age at
// the clubs of the league. Returns the number of new rivals.
int ssr_people_make_rivals(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int league,
                           int nation, int born, int count, int year);

// The end of a season: the counts of the season go to zero, the players
// grow older, the young get better and the old get worse, some players
// retire, the clubs fill their squads with young players, and the best
// players of small clubs move to bigger clubs. year is the new year.
// Returns the number of transfers.
int ssr_people_new_season(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int year);

// The days pass: injuries heal.
void ssr_people_pass_days(SSRPeople *people, int days);

// The players with the most goals in the season, of the clubs in the
// list. Puts up to max players into out, the most goals first. Returns the
// number.
int ssr_people_top_scorers(SSRPeople *people, const int *clubs, int clubCount, SSRPerson **out, int max);

// Moves the years of the world back by shift: the births, the
// retirements, the managers, and the year of the world. A new career that
// continues an old world starts in year 1. A retirement stays a
// retirement: a year of 0 or less becomes a year under 0.
void ssr_people_rebase(SSRPeople *people, int shift);

// Adds a retired player with the counts of a career, as the legend of an
// old career in the world. Returns the player, or NULL.
SSRPerson *ssr_people_add_legend(SSRPeople *people, const char *first, const char *last, int nation, int born,
                                 int retired, int apps, int goals, int assists);

// The mark (SSRPerson.rival) of the player of the last career.
#define SSR_FORMER_PLAYER 2

// Adds the player of the last career, who did not retire, to the club with
// the counts and the skill of that career. The next player of the user can
// play against the player. Returns the player, or NULL.
SSRPerson *ssr_people_add_former(SSRPeople *people, const char *first, const char *last, int nation, int born,
                                 int club, int skill, int apps, int goals, int assists);

// Keeps the people in a text file, one player a line. Returns 1 on
// success.
int ssr_people_save(const SSRPeople *people, const char *path);
// Reads the file. Returns NULL when the file is not there or is bad.
SSRPeople *ssr_people_load(const char *path);

#endif
