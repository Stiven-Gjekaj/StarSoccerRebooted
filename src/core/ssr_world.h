// ssr_world: the nations, the clubs, and the competitions of one data set,
// for example "MyData/v2025b" of the import. The world owns all of its
// text.

#ifndef SSR_WORLD_H
#define SSR_WORLD_H

#include <stdint.h>

// The newest data set of the game. The game picks it in TPlayer.
#define SSR_WORLD_DEFAULT_SET "v2025b"

// A colour as 0xRRGGBB.
typedef uint32_t SSRColor;

typedef struct {
    SSRColor shirt1;
    SSRColor shirt2;
    SSRColor shorts;
} SSRKit;

typedef struct {
    int id;
    const char *name;
    const char *tla;
    const char *federation;
    const char *federationShort;
    int strength;
} SSRContinent;

typedef struct {
    int id;
    const char *name;
    const char *tla;
    const char *nationality;
    int strength;
    int continent;
    double longitude;
    double latitude;
    int homeKitType;
    SSRKit home;
    SSRKit away;
} SSRNation;

typedef struct {
    int id;
    const char *name;
    const char *tla;
    int strength;
    double longitude;
    double latitude;
    int homeStyle;
    SSRKit home;
    SSRKit away;
    int nation;
    int league;
    int continentalCompetition;
    // The id of the first team when this club is a B team, or 0.
    int bTeamOf;
} SSRClub;

// The columns of Competitions.txt, with the names of the file.
typedef struct {
    int id;
    const char *tla;
    int locale;
    int level;
    int based;
    int type;
    int startYear;
    int startWeek;
    int duration;
    int recurring;
    int primaryMatchday;
    int secondaryMatchday;
    int groups;
    int rounds;
    int legs;
    int townRegion;
    int status;
} SSRCompetition;

typedef struct {
    int parent;
    int place;
    int promotionTo;
} SSRPromotion;

typedef struct {
    SSRContinent *continents;
    int continentCount;
    SSRNation *nations;
    int nationCount;
    SSRClub *clubs;
    int clubCount;
    SSRCompetition *competitions;
    int competitionCount;
    SSRPromotion *promotions;
    int promotionCount;
    // The text of all the names.
    char **strings;
    int stringCount;
    int stringCapacity;
} SSRWorld;

// Reads the data set in the directory. Returns NULL when a file is missing
// or a file has no id column.
SSRWorld *ssr_world_load(const char *directory);
void ssr_world_free(SSRWorld *world);

// The record with the id, or NULL.
const SSRContinent *ssr_world_continent(const SSRWorld *world, int id);
const SSRNation *ssr_world_nation(const SSRWorld *world, int id);
const SSRClub *ssr_world_club(const SSRWorld *world, int id);
const SSRCompetition *ssr_world_competition(const SSRWorld *world, int id);

// Puts the clubs of the league into clubs, up to max. Returns the number of
// clubs in the league.
int ssr_world_league_clubs(const SSRWorld *world, int league, const SSRClub **clubs, int max);

// Reads "#FFF" or "#FF0000". Returns fallback for other text.
SSRColor ssr_color_parse(const char *text, SSRColor fallback);

#endif
