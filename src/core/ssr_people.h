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
    // user, at the same age.
    int rival;
    int apps, goals, assists;
    int seasonApps, seasonGoals, seasonAssists;
} SSRPerson;

typedef struct SSRNames SSRNames;

// Reads the name lists (data/names.txt of the remake, or
// world/Names.txt of a pack). Returns NULL when the text has no names.
SSRNames *ssr_names_parse(const char *text);
void ssr_names_free(SSRNames *names);

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
    // The transfers of the last new season: the player, the old club, and
    // the new club. They are not in the file.
    int moves[16][3];
    int moveCount;
} SSRPeople;

SSRPeople *ssr_people_create(int seed);
void ssr_people_free(SSRPeople *people);

// The player with the id, or NULL.
SSRPerson *ssr_people_find(SSRPeople *people, int id);

// Makes a squad for the club when it has none. The skills follow the
// strength of the club. Returns the number of new players.
int ssr_people_make_squad(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int club, int year);

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

// Keeps the people in a text file, one player a line. Returns 1 on
// success.
int ssr_people_save(const SSRPeople *people, const char *path);
// Reads the file. Returns NULL when the file is not there or is bad.
SSRPeople *ssr_people_load(const char *path);

#endif
