#include "ssr_world.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_font.h"
#include "ssr_tsv.h"

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

SSRColor ssr_color_parse(const char *text, SSRColor fallback) {
    if (!text || text[0] != '#') return fallback;
    size_t length = strlen(text + 1);
    if (length != 3 && length != 6) return fallback;
    SSRColor color = 0;
    for (size_t i = 0; i < length; i++) {
        int value = hex_value(text[1 + i]);
        if (value < 0) return fallback;
        // "#F80" is "#FF8800".
        color = length == 3 ? (color << 8) | (SSRColor)(value * 17) : (color << 4) | (SSRColor)value;
    }
    return color;
}

// Keeps a copy of the text in the world, so that the tables can go.
static const char *keep(SSRWorld *world, const char *text) {
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return "";
    strcpy(copy, text);
    if (world->stringCount == world->stringCapacity) {
        int capacity = world->stringCapacity ? world->stringCapacity * 2 : 1024;
        char **strings = realloc(world->strings, (size_t)capacity * sizeof(char *));
        if (!strings) {
            free(copy);
            return "";
        }
        world->strings = strings;
        world->stringCapacity = capacity;
    }
    world->strings[world->stringCount++] = copy;
    return copy;
}

// A file of the pack takes the place of the file of the data set.
static SSRTsv *open_table(const char *directory, const char *pack, const char *name) {
    char path[4096];
    if (pack) {
        snprintf(path, sizeof(path), "%s/%s", pack, name);
        SSRTsv *table = ssr_tsv_load(path);
        if (table) return table;
    }
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    return ssr_tsv_load(path);
}

typedef struct {
    SSRWorld *world;
    const SSRTsv *table;
    int row;
} Reader;

static int int_of(const Reader *r, const char *column) {
    return ssr_tsv_int(r->table, r->row, ssr_tsv_column(r->table, column), 0);
}

static double double_of(const Reader *r, const char *column) {
    return ssr_tsv_double(r->table, r->row, ssr_tsv_column(r->table, column), 0);
}

static const char *text_of(const Reader *r, const char *column) {
    return keep(r->world, ssr_tsv_cell(r->table, r->row, ssr_tsv_column(r->table, column)));
}

static SSRColor color_of(const Reader *r, const char *column) {
    return ssr_color_parse(ssr_tsv_cell(r->table, r->row, ssr_tsv_column(r->table, column)), 0xFFFFFF);
}

static SSRKit kit_of(const Reader *r, const char *shirt1, const char *shirt2, const char *shorts) {
    SSRKit kit;
    kit.shirt1 = color_of(r, shirt1);
    kit.shirt2 = shirt2 ? color_of(r, shirt2) : kit.shirt1;
    kit.shorts = color_of(r, shorts);
    return kit;
}

static void read_continent(const Reader *r, void *record) {
    SSRContinent *c = record;
    c->id = int_of(r, "id");
    c->name = text_of(r, "name");
    c->tla = text_of(r, "tla");
    c->federation = text_of(r, "federationname");
    c->federationShort = text_of(r, "federationshortname");
    c->strength = int_of(r, "strength");
}

static void read_nation(const Reader *r, void *record) {
    SSRNation *n = record;
    n->id = int_of(r, "id");
    n->name = text_of(r, "shortname");
    n->tla = text_of(r, "tla");
    n->nationality = text_of(r, "nationality");
    n->strength = int_of(r, "strength");
    n->continent = int_of(r, "continent");
    n->longitude = double_of(r, "stadiumlongitude");
    n->latitude = double_of(r, "stadiumlatitude");
    n->homeKitType = int_of(r, "homekittype");
    n->home = kit_of(r, "homekitshirtcol1", "homekitshirtcol2", "homekitshortscol");
    n->away = kit_of(r, "awaykitshirtcol1", NULL, "awaykitshortscol");
}

static void read_club(const Reader *r, void *record) {
    SSRClub *c = record;
    c->id = int_of(r, "id");
    c->name = text_of(r, "shortname");
    c->tla = text_of(r, "tla");
    c->strength = int_of(r, "strength");
    c->longitude = double_of(r, "stadiumlongitude");
    c->latitude = double_of(r, "stadiumlatitude");
    c->homeStyle = int_of(r, "homestyle");
    c->home = kit_of(r, "homekitshirtcol1", "homekitshirtcol2", "homekitshortscol");
    c->away = kit_of(r, "awaykitshirtcol1", NULL, "awaykitshortscol");
    c->nation = int_of(r, "nationid");
    c->league = int_of(r, "leagueid");
    c->continentalCompetition = int_of(r, "continentalcompid");
    c->bTeamOf = int_of(r, "bteamof");
}

static void read_competition(const Reader *r, void *record) {
    SSRCompetition *c = record;
    c->id = int_of(r, "id");
    c->tla = text_of(r, "tla");
    c->locale = int_of(r, "locale");
    c->level = int_of(r, "level");
    c->based = int_of(r, "based");
    c->type = int_of(r, "comptype");
    c->startYear = int_of(r, "startyear");
    c->startWeek = int_of(r, "startweek");
    c->duration = int_of(r, "duration");
    c->recurring = int_of(r, "recurring");
    c->primaryMatchday = int_of(r, "primarymatchday");
    c->secondaryMatchday = int_of(r, "secondarymatchday");
    c->groups = int_of(r, "groups");
    c->rounds = int_of(r, "rounds");
    c->legs = int_of(r, "legs");
    c->townRegion = int_of(r, "townregion");
    c->status = int_of(r, "compstatus");
}

static void read_promotion(const Reader *r, void *record) {
    SSRPromotion *p = record;
    p->parent = int_of(r, "parentid");
    p->place = int_of(r, "place");
    p->promotionTo = int_of(r, "promotiontoid");
}

// Reads each row of the file into a new array of records. The file must
// have the key column. Returns 0 on failure.
static int read_all(SSRWorld *world, const char *directory, const char *pack, const char *file, const char *key,
                    size_t size, void (*read)(const Reader *, void *), void **out, int *count) {
    SSRTsv *table = open_table(directory, pack, file);
    if (!table || ssr_tsv_column(table, key) < 0) {
        ssr_tsv_free(table);
        return 0;
    }
    int rows = ssr_tsv_rows(table);
    char *records = calloc(rows ? (size_t)rows : 1, size);
    if (!records) {
        ssr_tsv_free(table);
        return 0;
    }
    int keyColumn = ssr_tsv_column(table, key);
    int used = 0;
    for (int row = 0; row < rows; row++) {
        // A row with no key is not a record. Clubs.txt ends with one.
        if (ssr_tsv_int(table, row, keyColumn, -1) < 0) continue;
        Reader reader = {world, table, row};
        read(&reader, records + (size_t)used * size);
        used++;
    }
    ssr_tsv_free(table);
    *out = records;
    *count = used;
    return 1;
}

SSRWorld *ssr_world_load(const char *directory) {
    return ssr_world_load_pack(directory, NULL);
}

SSRWorld *ssr_world_load_pack(const char *directory, const char *pack) {
    SSRWorld *world = calloc(1, sizeof(SSRWorld));
    if (!world) return NULL;
    int ok = read_all(world, directory, pack, "Continents.txt", "id", sizeof(SSRContinent),
                      read_continent, (void **)&world->continents,
                      &world->continentCount) &&
             read_all(world, directory, pack, "Nations.txt", "id", sizeof(SSRNation),
                      read_nation, (void **)&world->nations, &world->nationCount) &&
             read_all(world, directory, pack, "Clubs.txt", "id", sizeof(SSRClub),
                      read_club, (void **)&world->clubs, &world->clubCount) &&
             read_all(world, directory, pack, "Competitions.txt", "id", sizeof(SSRCompetition),
                      read_competition, (void **)&world->competitions,
                      &world->competitionCount) &&
             read_all(world, directory, pack, "PromotionPlaces.txt", "parentid", sizeof(SSRPromotion),
                      read_promotion, (void **)&world->promotions,
                      &world->promotionCount);
    if (!ok) {
        ssr_world_free(world);
        return NULL;
    }
    return world;
}

// The capital of a Latin or a Cyrillic letter, as Java toUpperCase gives
// it. Other characters stay.
static uint32_t capital(uint32_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0x131) return 'I';
    if (c >= 0x100 && c <= 0x17F) {
        int oddPairs = (c <= 0x137) || (c >= 0x14A && c <= 0x177);
        int evenPairs = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
        if ((oddPairs && (c & 1)) || (evenPairs && !(c & 1))) return c - 1;
        return c;
    }
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    return c;
}

// The first count characters of UTF-8 text in capitals (slice and
// toUpperCase of the game).
static const char *first_capitals(SSRWorld *world, const char *text, int count) {
    char part[128];
    size_t n = 0;
    const char *p = text;
    uint32_t c;
    while (count-- > 0 && (c = ssr_utf8_next(&p)) != 0 && n + 5 < sizeof(part)) {
        c = capital(c);
        if (c < 0x80) {
            part[n++] = (char)c;
        } else if (c < 0x800) {
            part[n++] = (char)(0xC0 | c >> 6);
            part[n++] = (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            part[n++] = (char)(0xE0 | c >> 12);
            part[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            part[n++] = (char)(0x80 | (c & 0x3F));
        } else {
            part[n++] = (char)(0xF0 | c >> 18);
            part[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            part[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            part[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    part[n] = 0;
    return keep(world, part);
}

static int count_characters(const char *text) {
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) n += (*p & 0xC0) != 0x80;
    return n;
}

int ssr_world_set_language(SSRWorld *world, const char *nations, const char *continents, const char *code) {
    if (!world || !code) return 0;
    if (!world->dataNames) {
        int count = world->nationCount * 3 + world->continentCount * 2;
        world->dataNames = malloc((size_t)(count ? count : 1) * sizeof(char *));
        if (!world->dataNames) return 0;
        for (int i = 0; i < world->nationCount; i++) {
            world->dataNames[i * 3] = world->nations[i].name;
            world->dataNames[i * 3 + 1] = world->nations[i].tla;
            world->dataNames[i * 3 + 2] = world->nations[i].nationality;
        }
        const char **c = world->dataNames + world->nationCount * 3;
        for (int i = 0; i < world->continentCount; i++) {
            c[i * 2] = world->continents[i].name;
            c[i * 2 + 1] = world->continents[i].tla;
        }
    }
    for (int i = 0; i < world->nationCount; i++) {
        world->nations[i].name = world->dataNames[i * 3];
        world->nations[i].tla = world->dataNames[i * 3 + 1];
        world->nations[i].nationality = world->dataNames[i * 3 + 2];
    }
    const char **c = world->dataNames + world->nationCount * 3;
    for (int i = 0; i < world->continentCount; i++) {
        world->continents[i].name = c[i * 2];
        world->continents[i].tla = c[i * 2 + 1];
    }
    int changed = 0;
    char column[48];
    SSRTsv *table = nations ? ssr_tsv_parse(nations, strlen(nations)) : NULL;
    int name = ssr_tsv_column(table, code);
    snprintf(column, sizeof(column), "%s_short", code);
    int tla = ssr_tsv_column(table, column);
    snprintf(column, sizeof(column), "%s_nationality", code);
    int nationality = ssr_tsv_column(table, column);
    for (int i = 0; table && name >= 0 && i < world->nationCount && i < ssr_tsv_rows(table); i++) {
        SSRNation *n = &world->nations[i];
        n->name = keep(world, ssr_tsv_cell(table, i, name));
        if (tla >= 0) {
            const char *text = ssr_tsv_cell(table, i, tla);
            n->tla = count_characters(text) > 6 ? first_capitals(world, text, 6) : keep(world, text);
        }
        if (nationality >= 0) n->nationality = keep(world, ssr_tsv_cell(table, i, nationality));
        changed++;
    }
    ssr_tsv_free(table);
    table = continents ? ssr_tsv_parse(continents, strlen(continents)) : NULL;
    name = ssr_tsv_column(table, code);
    for (int i = 0; table && name >= 0 && i < world->continentCount && i < ssr_tsv_rows(table); i++) {
        SSRContinent *k = &world->continents[i];
        k->name = keep(world, ssr_tsv_cell(table, i, name));
        k->tla = first_capitals(world, k->name, 6);
        changed++;
    }
    ssr_tsv_free(table);
    return changed;
}

void ssr_world_free(SSRWorld *world) {
    if (!world) return;
    free(world->dataNames);
    for (int i = 0; i < world->stringCount; i++) free(world->strings[i]);
    free(world->strings);
    free(world->continents);
    free(world->nations);
    free(world->clubs);
    free(world->competitions);
    free(world->promotions);
    free(world);
}

#define FIND(array, count, wanted)                                                                          \
    do {                                                                                                    \
        if (!world) return NULL;                                                                            \
        for (int i = 0; i < world->count; i++) {                                                            \
            if (world->array[i].id == (wanted)) return &world->array[i];                                    \
        }                                                                                                   \
        return NULL;                                                                                        \
    } while (0)

const SSRContinent *ssr_world_continent(const SSRWorld *world, int id) {
    FIND(continents, continentCount, id);
}

const SSRNation *ssr_world_nation(const SSRWorld *world, int id) {
    FIND(nations, nationCount, id);
}

const SSRClub *ssr_world_club(const SSRWorld *world, int id) {
    FIND(clubs, clubCount, id);
}

const SSRCompetition *ssr_world_competition(const SSRWorld *world, int id) {
    FIND(competitions, competitionCount, id);
}

int ssr_world_league_clubs(const SSRWorld *world, int league, const SSRClub **clubs, int max) {
    if (!world) return 0;
    int found = 0;
    for (int i = 0; i < world->clubCount; i++) {
        if (world->clubs[i].league != league) continue;
        if (clubs && found < max) clubs[found] = &world->clubs[i];
        found++;
    }
    return found;
}

double ssr_world_club_distance(const SSRWorld *world, int a, int b) {
    const SSRClub *x = ssr_world_club(world, a), *y = ssr_world_club(world, b);
    if (!x || !y) return -1;
    // The haversine rule on a sphere of 6371 km.
    const double rad = 3.14159265358979323846 / 180.0;
    double dLat = (y->latitude - x->latitude) * rad, dLon = (y->longitude - x->longitude) * rad;
    double h = sin(dLat / 2) * sin(dLat / 2) + cos(x->latitude * rad) * cos(y->latitude * rad) * sin(dLon / 2) * sin(dLon / 2);
    return 2 * 6371.0 * asin(sqrt(h < 1 ? h : 1));
}

int ssr_world_is_derby(const SSRWorld *world, int a, int b) {
    const SSRClub *x = ssr_world_club(world, a), *y = ssr_world_club(world, b);
    if (!x || !y || a == b || x->nation != y->nation) return 0;
    // A club with no place in the data has 0 and 0.
    if ((x->latitude == 0 && x->longitude == 0) || (y->latitude == 0 && y->longitude == 0)) return 0;
    return ssr_world_club_distance(world, a, b) < SSR_DERBY_KM;
}
