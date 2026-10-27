#include "ssr_season.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fixdata.h"
#include "ssr_sort.h"

/* ---------------------------------------------------------------- dates */

int ssr_date_make(int day, int week, int year) {
    return (year - 1) * 364 + (week - 1) * 7 + day;
}

int ssr_date_day(int sdate) {
    int day = sdate % 7;
    return day == 0 ? 7 : day;
}

int ssr_date_week(int sdate) {
    return ((sdate - 1) % 364) / 7 + 1;
}

int ssr_date_year(int sdate) {
    return (sdate - 1) / 364 + 1;
}

/* --------------------------------------------------------- random numbers */

// Rnd3(x): a float from 0 to under x.
static float rnd3(SSRSeason *s, float x) {
    return ssr_rnd(s->random) * x;
}

// Rnd2(a, b): a float from a to under b.
static float rnd2(SSRSeason *s, float a, float b) {
    return rnd3(s, b - a) + a;
}

/* ------------------------------------------------------------ the sort */

typedef int (*Compare)(SSRSeason *s, const void *context, int a, int b);

// The compare of the season, with the season and a context.
typedef struct {
    SSRSeason *season;
    Compare compare;
    const void *context;
} SortContext;

static int sort_compare(void *context, int a, int b) {
    const SortContext *c = context;
    return c->compare(c->season, c->context, a, b);
}

static void sort(SSRSeason *s, int *items, int count, Compare compare, const void *context, int descending) {
    SortContext c = {s, compare, context};
    ssr_sort(items, count, sort_compare, &c, descending);
}

/* ------------------------------------------------------------ lookups */

SSRSeasonComp *ssr_season_comp(SSRSeason *s, int id) {
    if (!s) return NULL;
    for (int i = 0; i < s->compCount; i++) {
        if (s->comps[i].data->id == id) return &s->comps[i];
    }
    return NULL;
}

static SSRSeasonNation *nation(SSRSeason *s, int id) {
    for (int i = 0; i < s->nationCount; i++) {
        if (s->nations[i].data->id == id) return &s->nations[i];
    }
    return NULL;
}

static int club_index(const SSRSeason *s, int id) {
    for (int i = 0; i < s->clubCount; i++) {
        if (s->clubs[i].data->id == id) return i;
    }
    return -1;
}

static int is_loaded(const SSRSeason *s, int index) {
    for (int i = 0; i < s->clubListCount; i++) {
        if (s->clubList[i] == index) return 1;
    }
    return 0;
}

// A club as the data set gives it (c_TClub.CreateClub).
static void reset_club(SSRSeason *s, SSRSeasonClub *club, const SSRClub *data) {
    club->data = data;
    club->strength = data->strength;
    club->league = data->league;
    club->continental = data->continentalCompetition;
    // Each club object takes a random number (c_TBase_Team).
    club->randNo = (int)rnd3(s, 9999);
}

SSRSeasonClub *ssr_season_club(SSRSeason *s, int id, int load) {
    if (!s) return NULL;
    int index = club_index(s, id);
    if (index < 0) return NULL;
    if (is_loaded(s, index)) return &s->clubs[index];
    if (!load) return NULL;
    // c_TClub.LoadClub makes a new club that the list does not keep.
    reset_club(s, &s->scratch, s->clubs[index].data);
    return &s->scratch;
}

// The tla of a club or a nation as the game keeps it: "/" is "-".
static void copy_tla(char *out, size_t size, const char *tla) {
    size_t n = 0;
    for (; tla && tla[n] && n + 1 < size; n++) out[n] = tla[n] == '/' ? '-' : tla[n];
    out[n] = 0;
}

/* -------------------------------------------------------------- tables */

SSRTableRow *ssr_season_row(SSRSeason *s, const SSRPool *pool, int id) {
    if (!s || !pool) return NULL;
    for (int i = 0; i < pool->count; i++) {
        if (s->rows[pool->rows[i]].id == id) return &s->rows[pool->rows[i]];
    }
    return NULL;
}

// A new row of a table (c_TTableData). Returns its index, or -1.
static int new_row(SSRSeason *s, int teamId, const char *name, int strength) {
    if (s->rowCount == s->rowCapacity) {
        int capacity = s->rowCapacity ? s->rowCapacity * 2 : 256;
        SSRTableRow *rows = realloc(s->rows, (size_t)capacity * sizeof(SSRTableRow));
        if (!rows) return -1;
        s->rows = rows;
        s->rowCapacity = capacity;
    }
    SSRTableRow *row = &s->rows[s->rowCount];
    memset(row, 0, sizeof(*row));
    row->teamId = teamId;
    copy_tla(row->name, sizeof(row->name), name);
    row->strength = strength;
    row->randNo = (int)rnd3(s, 9999);
    row->prevPosition = -1;
    return s->rowCount++;
}

static void pool_append(SSRPool *pool, int row) {
    if (row < 0) return;
    if (pool->count == pool->capacity) {
        int capacity = pool->capacity ? pool->capacity * 2 : 16;
        int *rows = realloc(pool->rows, (size_t)capacity * sizeof(int));
        if (!rows) return;
        pool->rows = rows;
        pool->capacity = capacity;
    }
    pool->rows[pool->count++] = row;
}

// c_TTeamPool.AddItem: a new row with the next id.
static void pool_add(SSRSeason *s, SSRPool *pool, int teamId, const char *name, int strength) {
    int row = new_row(s, teamId, name, strength);
    if (row < 0) return;
    s->rows[row].id = pool->count + 1;
    pool_append(pool, row);
}

static int compare_int(int a, int b) {
    return a < b ? -1 : a > b;
}

// c_TTableData.Compare for the modes of a table.
static int compare_rows(SSRSeason *s, const void *context, int a, int b) {
    int mode = *(const int *)context;
    const SSRTableRow *x = &s->rows[a], *y = &s->rows[b];
    switch (mode) {
    case 4: {
        if (x->points != y->points) return x->points > y->points ? -1 : 1;
        int dx = x->goalsFor - x->goalsAgainst, dy = y->goalsFor - y->goalsAgainst;
        if (dx != dy) return dx > dy ? -1 : 1;
        if (x->goalsFor != y->goalsFor) return x->goalsFor > y->goalsFor ? -1 : 1;
        if (x->won != y->won) return x->won > y->won ? -1 : 1;
        if (x->id != y->id) return x->id < y->id ? -1 : 1;
        return strcmp(x->name, y->name);
    }
    case 16: {
        // With no match played the division gives NaN or Inf. A compare
        // with NaN is false, so the rule falls through, as in Java.
        float px = (float)x->points / (float)x->played, py = (float)y->points / (float)y->played;
        if (px > py) return -1;
        if (px < py) return 1;
        float gx = (float)(x->goalsFor - x->goalsAgainst) / (float)x->played;
        float gy = (float)(y->goalsFor - y->goalsAgainst) / (float)y->played;
        if (gx > gy) return -1;
        if (gx < gy) return 1;
        if (x->goalsFor != y->goalsFor) return x->goalsFor > y->goalsFor ? -1 : 1;
        return strcmp(x->name, y->name);
    }
    case 1:
        return compare_int(x->id, y->id);
    case 2:
        return strcmp(x->name, y->name);
    case 5:
        return compare_int(x->randNo, y->randNo);
    case 24:
        return x->longLat > y->longLat ? -1 : x->longLat < y->longLat;
    case 25:
        return x->longLat < y->longLat ? -1 : x->longLat > y->longLat;
    default:
        return 0;
    }
}

void ssr_season_sort_pool(SSRSeason *s, SSRPool *pool, int mode) {
    if (!s || !pool) return;
    for (int i = 0; i < pool->count; i++) {
        SSRTableRow *row = &s->rows[pool->rows[i]];
        if (mode == 5) row->randNo = (int)rnd3(s, 9999);
        if (mode == 24 || mode == 25) {
            SSRSeasonClub *club = ssr_season_club(s, row->teamId, 1);
            if (club) row->longLat = (float)(mode == 24 ? club->data->latitude : club->data->longitude);
        }
    }
    sort(s, pool->rows, pool->count, compare_rows, &mode, 0);
}

int ssr_season_position(SSRSeason *s, SSRPool *pool, int teamId) {
    if (!s || !pool) return 0;
    ssr_season_sort_pool(s, pool, 4);
    for (int i = 0; i < pool->count; i++) {
        if (s->rows[pool->rows[i]].teamId == teamId) return i + 1;
    }
    return 0;
}

// c_TTeamPool.ShuffleIds: the rows get new ids in a random order.
static void shuffle_ids(SSRSeason *s, SSRPool *pool) {
    ssr_season_sort_pool(s, pool, 5);
    for (int i = 0; i < pool->count; i++) s->rows[pool->rows[i]].id = i + 1;
    ssr_season_sort_pool(s, pool, 1);
}

/* ------------------------------------------------------- fixture tables */

typedef struct {
    const SSRFixTable *table;
    int index;
    // A league of 35 or 36 teams plays 8 rounds of 18 matches.
    int custom;
} FixReader;

// RestoreFixtureData: a size with no table keeps the table that was there.
static void restore_fixtures(FixReader *reader, int size, int cup) {
    reader->custom = 0;
    if (!cup && (size == 35 || size == 36)) {
        size = 36;
        reader->custom = 1;
    }
    for (int i = 0; i < SSR_FIX_TABLE_COUNT; i++) {
        if (SSR_FIX_TABLES[i].size == size && SSR_FIX_TABLES[i].cup == cup) reader->table = &SSR_FIX_TABLES[i];
    }
    reader->index = -1;
}

// ReadData: the next value. The tables of 28 and 36 have no end, so a read
// past the end gives -1, as the restart of the table needs.
static int read_fixture(FixReader *reader) {
    reader->index++;
    if (!reader->table || reader->index >= reader->table->count) return -1;
    return reader->table->values[reader->index];
}

/* ---------------------------------------------------------- competitions */

// c_TCompetition.SetPriority: the order of the set-up.
static int priority(const SSRCompetition *c) {
    if (c->level == 1) return 1;
    if (c->level != 0) return 0;
    if (c->locale == 1 || c->locale == 2) return 2;
    if (c->locale != 0) return 0;
    if (c->type == 1) return 3;
    if (c->type == 0 || c->type == 4) return 4;
    if (c->type == 2 || c->type == 3 || c->type == 5) return 5;
    return 0;
}

static void int_append(int **items, int *count, int item) {
    int *grown = realloc(*items, (size_t)(*count + 1) * sizeof(int));
    if (!grown) return;
    *items = grown;
    (*items)[(*count)++] = item;
}

static int compare_places(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    return compare_int(s->world->promotions[a].place, s->world->promotions[b].place);
}

// The promotion places of the data set, in the lists of the competitions
// (c_TPromotionPlace.LoadData). The game reads them again for each set-up.
static void load_places(SSRSeason *s) {
    for (int i = 0; i < s->compCount; i++) {
        free(s->comps[i].places);
        free(s->comps[i].placesToMe);
        s->comps[i].places = s->comps[i].placesToMe = NULL;
        s->comps[i].placeCount = s->comps[i].placesToMeCount = 0;
    }
    for (int i = 0; i < s->world->promotionCount; i++) {
        const SSRPromotion *p = &s->world->promotions[i];
        SSRSeasonComp *parent = ssr_season_comp(s, p->parent), *target = ssr_season_comp(s, p->promotionTo);
        if (!parent || !target) continue;
        int_append(&parent->places, &parent->placeCount, i);
        int_append(&target->placesToMe, &target->placesToMeCount, i);
    }
    for (int i = 0; i < s->compCount; i++) {
        sort(s, s->comps[i].places, s->comps[i].placeCount, compare_places, NULL, 0);
        sort(s, s->comps[i].placesToMe, s->comps[i].placesToMeCount, compare_places, NULL, 0);
    }
}

static const SSRPromotion *place_of(const SSRSeason *s, int index) {
    return &s->world->promotions[index];
}

// The competition of the place, or of the place that sends the team.
static SSRSeasonComp *place_parent(SSRSeason *s, int index) {
    return ssr_season_comp(s, place_of(s, index)->parent);
}

static SSRSeasonComp *place_target(SSRSeason *s, int index) {
    return ssr_season_comp(s, place_of(s, index)->promotionTo);
}

// Empties a competition of another nation (the lists only).
static void clear_comp(SSRSeasonComp *c) {
    c->fixtureCount = 0;
    c->replayCount = 0;
    c->placeCount = 0;
    c->placesToMeCount = 0;
    for (int i = 0; i < c->poolCount; i++) free(c->pools[i].rows);
    free(c->pools);
    c->pools = NULL;
    c->poolCount = 0;
}

// c_TCompetition.GetMyContinentId.
static int my_continent(SSRSeason *s, const SSRSeasonComp *c) {
    if (c->data->level == 0 && c->data->locale != 1) {
        SSRSeasonNation *n = nation(s, c->data->based);
        return n ? n->data->continent : 0;
    }
    return c->data->based;
}

static int is_league_type(int type) {
    return type == 0 || type == 4;
}

// c_TCompetition.CheckFixtureClash: 1 when a competition that the teams of
// this one can play in has a match on the day.
static int clash(SSRSeason *s, const SSRSeasonComp *c, int sdate) {
    const SSRCompetition *d = c->data;
    int week = ssr_date_week(sdate);
    if (d->level == 1 && (week > 47 || week < 7)) return 1;
    int mc = my_continent(s, c);
    for (int i = 0; i < s->compCount; i++) {
        const SSRSeasonComp *x = &s->comps[s->compOrder[i]];
        const SSRCompetition *xd = x->data;
        if (xd->id == d->id) continue;
        int checked = (xd->level == 1 && xd->locale == 2) || (d->level == 0 && xd->level == 1 && xd->based == mc) ||
                      (d->level == 0 && d->locale == 0 && xd->level == 0 && xd->locale == 1 && xd->based == mc) ||
                      (d->level == 0 && xd->level == 0 && d->locale == xd->locale && d->based == xd->based &&
                       (!is_league_type(xd->type) || !is_league_type(d->type)));
        if (!checked) continue;
        for (int k = 0; k < x->fixtureCount; k++) {
            if (x->fixtures[k].sdate == sdate) return 1;
        }
    }
    return 0;
}

static SSRFixture *fixture_append(SSRFixture **list, int *count, int *capacity) {
    if (*count == *capacity) {
        int grown = *capacity ? *capacity * 2 : 64;
        SSRFixture *fixtures = realloc(*list, (size_t)grown * sizeof(SSRFixture));
        if (!fixtures) return NULL;
        *list = fixtures;
        *capacity = grown;
    }
    SSRFixture *f = &(*list)[(*count)++];
    memset(f, 0, sizeof(*f));
    return f;
}

// c_TFixture.Create: a new fixture with a random number.
static void make_fixture(SSRSeason *s, SSRFixture *f, int sdate, int matchType, int round, int group, int leg,
                         int home, int away, const SSRSeasonComp *c) {
    f->sdate = sdate;
    f->matchType = matchType;
    f->round = round;
    f->group = group;
    f->leg = leg;
    f->home = home;
    f->away = away;
    f->level = c->data->level;
    f->compId = c->data->id;
    f->randNo = ssr_rand(s->random, 1000);
}

static void add_fixture(SSRSeason *s, SSRSeasonComp *c, int sdate, int matchType, int round, int group, int leg,
                        int home, int away) {
    SSRFixture *f = fixture_append(&c->fixtures, &c->fixtureCount, &c->fixtureCapacity);
    if (f) make_fixture(s, f, sdate, matchType, round, group, leg, home, away, c);
}

// The order of a fixture list (c_TFixture.Compare mode 17).
static int compare_fixtures(SSRSeason *s, const void *context, int a, int b) {
    (void)s;
    const SSRFixture *list = context, *x = &list[a], *y = &list[b];
    if (x->sdate != y->sdate) return compare_int(x->sdate, y->sdate);
    if (x->level != y->level) return x->level > y->level ? -1 : 1;
    if (x->round != y->round) return compare_int(x->round, y->round);
    if (x->group != y->group) return compare_int(x->group, y->group);
    if (x->result != y->result) return x->result > y->result ? -1 : 1;
    return 0;
}

static void sort_fixtures(SSRSeason *s, SSRSeasonComp *c) {
    if (c->fixtureCount < 2) return;
    int *order = malloc((size_t)c->fixtureCount * sizeof(int));
    SSRFixture *copy = malloc((size_t)c->fixtureCount * sizeof(SSRFixture));
    if (!order || !copy) {
        free(order);
        free(copy);
        return;
    }
    for (int i = 0; i < c->fixtureCount; i++) order[i] = i;
    sort(s, order, c->fixtureCount, compare_fixtures, c->fixtures, 0);
    for (int i = 0; i < c->fixtureCount; i++) copy[i] = c->fixtures[order[i]];
    memcpy(c->fixtures, copy, (size_t)c->fixtureCount * sizeof(SSRFixture));
    free(order);
    free(copy);
}

/* ------------------------------------------------------------- the count */

static int count_in_division(const SSRSeason *s, int league) {
    int n = 0;
    for (int i = 0; i < s->clubListCount; i++) n += s->clubs[s->clubList[i]].league == league;
    return n;
}

static int count_in_continental(const SSRSeason *s, int league) {
    int n = 0;
    for (int i = 0; i < s->clubListCount; i++) {
        const SSRSeasonClub *c = &s->clubs[s->clubList[i]];
        n += c->league == league && c->continental > 0;
    }
    return n;
}

// c_TCompetition.GetNoofTeamsInRound.
static int teams_in_round(SSRSeason *s, const SSRSeasonComp *c) {
    int count = 0;
    for (int i = 0; i < c->placesToMeCount; i++) {
        const SSRPromotion *p = place_of(s, c->placesToMe[i]);
        SSRSeasonComp *parent = ssr_season_comp(s, p->parent);
        switch (p->place) {
        case 100:
            count += count_in_division(s, p->parent);
            if (parent && parent->data->type == 4) count += parent->placesToMeCount;
            break;
        case 101:
            count += count_in_division(s, p->parent) - count_in_continental(s, p->parent);
            break;
        case 102:
        case 105:
        case 106:
            count += 1;
            break;
        case 103:
        case 104:
            if (parent) count += teams_in_round(s, parent) / 2;
            break;
        case 107:
            count += count_in_continental(s, p->parent);
            break;
        default:
            if (p->place >= 1 && p->place <= 99 && parent) count += parent->data->groups > 1 ? parent->data->groups : 1;
            break;
        }
    }
    if (c->data->type == 1 && c->data->rounds > 1) {
        for (int i = 0; i < c->data->rounds; i++) count /= 2;
    }
    return count;
}

// c_TCompetition.GetNoofQualifiers.
static int qualifiers(SSRSeason *s, const SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    if (d->level == 0 && d->locale == 0 && d->type == 0) return c->poolCount > 0 ? c->pools[0].count : 0;
    if (d->level == 1 && c->placesToMeCount == 0) {
        int n = 0;
        for (int i = 0; i < s->nationCount; i++) n += s->nations[i].data->continent == d->based;
        return n;
    }
    return teams_in_round(s, c);
}

/* ------------------------------------------------------------ the pools */

// c_TClub.Compare mode 11: the weakest first, then the lower id.
static int compare_clubs_strength(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    const SSRSeasonClub *x = &s->clubs[a], *y = &s->clubs[b];
    if (x->strength != y->strength) return compare_int(x->strength, y->strength);
    return compare_int(x->data->id, y->data->id);
}

// c_TClub.Compare mode 34: the clubs of the higher tier first, then the
// strongest.
static int compare_clubs_tier(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    const SSRSeasonClub *x = &s->clubs[a], *y = &s->clubs[b];
    SSRSeasonComp *cx = ssr_season_comp(s, x->league), *cy = ssr_season_comp(s, y->league);
    if (!cx) return -1;
    if (!cy || cx->data->status == 0) return 1;
    if (cy->data->status == 0) return -1;
    if (cx->data->status != cy->data->status) return compare_int(cx->data->status, cy->data->status);
    if (x->strength != y->strength) return x->strength > y->strength ? -1 : 1;
    return compare_int(x->data->id, y->data->id);
}

// c_TNation.Compare mode 15: the stronger nation first, with a random
// margin in each compare.
static int compare_nations_draw(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    const SSRSeasonNation *x = &s->nations[a], *y = &s->nations[b];
    if ((float)y->data->strength > (float)x->data->strength + rnd3(s, 5)) return 1;
    if ((float)y->data->strength < (float)x->data->strength - rnd3(s, 5)) return -1;
    if (y->randNo > x->randNo) return 1;
    if (y->randNo < x->randNo) return -1;
    return compare_int(x->data->id, y->data->id);
}

static void deal_clubs(SSRSeason *s, SSRSeasonComp *c, const int *clubs, int count) {
    for (int i = 0; i < count; i++) {
        const SSRSeasonClub *club = &s->clubs[clubs[i]];
        pool_add(s, &c->pools[i % c->poolCount], club->data->id, club->data->tla, club->strength);
    }
}

static int is_fill_place(int place) {
    return place != 100 && place != 103 && place != 104 && place != 107;
}

static int fill_places(const SSRSeason *s, const SSRSeasonComp *c) {
    int n = 0;
    for (int i = 0; i < c->placesToMeCount; i++) n += is_fill_place(place_of(s, c->placesToMe[i])->place);
    return n;
}

// The first loaded club of the nation with no continental place, or -1.
static int free_club_of(const SSRSeason *s, int nationId) {
    for (int i = 0; i < s->clubListCount; i++) {
        const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
        if (club->data->nation == nationId && club->continental == 0) return s->clubList[i];
    }
    return -1;
}

// c_TCompetition.PopulateTeamPool.
static void populate(SSRSeason *s, SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    int nat = s->player.nationId;
    SSRSeasonClub *mine = ssr_season_club(s, s->player.clubId, 1);
    if (mine) nat = mine->data->nation;
    int req = fill_places(s, c);
    if (d->type == 0 && d->level == 1 && d->locale == 1) {
        int *list = malloc((size_t)s->nationCount * sizeof(int));
        int n = 0;
        if (!list) return;
        for (int i = 0; i < s->nationCount; i++) {
            if (s->nations[i].data->continent != d->based) continue;
            s->nations[i].randNo = (int)rnd3(s, 9999);
            list[n++] = i;
        }
        sort(s, list, n, compare_nations_draw, NULL, 0);
        for (int i = 0; i < n; i++) {
            const SSRSeasonNation *x = &s->nations[list[i]];
            pool_add(s, &c->pools[i % c->poolCount], x->data->id, x->data->tla, x->data->strength);
        }
        free(list);
    } else if (d->type == 0 && d->level == 0 && d->locale == 0) {
        for (int i = 0; i < s->clubListCount; i++) {
            const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
            if (club->league == d->id) pool_add(s, &c->pools[0], club->data->id, club->data->tla, club->strength);
        }
    } else if (d->type == 0 && d->level == 0 && d->locale == 1) {
        int *list = malloc((size_t)(s->clubListCount + 1) * sizeof(int));
        int n = 0;
        if (!list) return;
        for (int i = 0; i < s->clubListCount && n < req; i++) {
            const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
            if (club->data->nation == nat && club->continental == d->id) list[n++] = s->clubList[i];
        }
        if (n < req) {
            sort(s, s->clubList, s->clubListCount, compare_clubs_strength, NULL, 1);
            for (int i = 0; i < c->placesToMeCount && n < req; i++) {
                if (!is_fill_place(place_of(s, c->placesToMe[i])->place)) continue;
                SSRSeasonComp *parent = place_parent(s, c->placesToMe[i]);
                if (!parent || parent->data->based == nat) continue;
                int club = free_club_of(s, parent->data->based);
                if (club < 0) continue;
                s->clubs[club].continental = d->id;
                list[n++] = club;
            }
        }
        sort(s, list, n, compare_clubs_strength, NULL, 1);
        deal_clubs(s, c, list, n);
        free(list);
    } else if (d->type == 1 && d->level == 0 && d->locale == 1) {
        SSRPool *pool = &c->pools[0];
        for (int i = 0; i < s->clubListCount && pool->count < req; i++) {
            const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
            if (club->data->nation == nat && club->continental == d->id) {
                pool_add(s, pool, club->data->id, club->data->tla, club->strength);
            }
        }
        if (pool->count < req) {
            sort(s, s->clubList, s->clubListCount, compare_clubs_strength, NULL, 1);
            for (int i = 0; i < c->placesToMeCount && pool->count < req; i++) {
                if (!is_fill_place(place_of(s, c->placesToMe[i])->place)) continue;
                SSRSeasonComp *parent = place_parent(s, c->placesToMe[i]);
                if (!parent || parent->data->locale != 0 || parent->data->based == nat) continue;
                int club = free_club_of(s, parent->data->based);
                if (club < 0) continue;
                s->clubs[club].continental = d->id;
                pool_add(s, pool, s->clubs[club].data->id, s->clubs[club].data->tla, s->clubs[club].strength);
            }
        }
        shuffle_ids(s, pool);
    } else if (d->type == 1) {
        SSRPool *pool = &c->pools[0];
        for (int i = 0; i < c->placesToMeCount; i++) {
            const SSRPromotion *p = place_of(s, c->placesToMe[i]);
            if (p->place != 100 && p->place != 101 && p->place != 107) continue;
            for (int k = 0; k < s->clubListCount; k++) {
                const SSRSeasonClub *club = &s->clubs[s->clubList[k]];
                if (club->league != p->parent) continue;
                if (p->place == 101 && club->continental != 0) continue;
                if (p->place == 107 && club->continental <= 0) continue;
                pool_add(s, pool, club->data->id, club->data->tla, club->strength);
            }
        }
        shuffle_ids(s, pool);
    }
}

// c_TCompetition.CreateTeamPool: one pool for each group.
static void create_pools(SSRSeasonComp *c) {
    if (c->poolCount > 0) {
        for (int i = 0; i < c->poolCount; i++) c->pools[i].count = 0;
        return;
    }
    int n = c->data->groups > 1 ? c->data->groups : 1;
    c->pools = calloc((size_t)n, sizeof(SSRPool));
    c->poolCount = c->pools ? n : 0;
}

/* ---------------------------------------------------------- fixture lists */

// c_TCompetition.CreateFixtureListLeague: the rounds of the league on the
// match days of the competition, with no clash.
static void league_fixtures(SSRSeason *s, SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    if (d->type == 0 && d->duration == 0) return;
    c->fixtureCount = 0;
    int start = ssr_date_make(d->primaryMatchday, d->startWeek, c->startYear);
    int date = start;
    int weeks = 1;
    if (d->startWeek < 48) {
        for (int i = 0; i < d->duration - 1; i++) {
            date += 7;
            int week = ssr_date_week(date);
            if (d->level == 0 && week < 48) weeks++;
            if (d->level == 1 && week > 6 && week < 48) weeks++;
        }
        if (d->level == 0) weeks -= 2;
    }
    int q = qualifiers(s, c);
    if (q == 0) return;
    FixReader reader = {0};
    int extra = 0;
    if (d->groups > 1) {
        int per = q / d->groups;
        extra = q - d->groups * per;
        q = per;
    } else {
        restore_fixtures(&reader, q, 0);
    }
    int rounds = (q - 1) * d->rounds;
    if (extra != 0) rounds = (q % 2 != 0 ? q + 1 : q) * d->rounds;
    if (reader.custom) rounds = 8;
    int gap = weeks / (rounds + 1);
    int mid = 0, every = 0;
    if (d->level == 0 && rounds > weeks) {
        mid = rounds - weeks;
        every = rounds / mid;
    }
    if (d->level == 1) gap = weeks / (rounds / 2 + 1);
    int groups = d->groups > 1 ? d->groups : 1;
    for (int g = 0; g < groups; g++) {
        date = start;
        int add = 0;
        if (extra != 0) {
            extra--;
            add = 1;
        }
        int n = q + add;
        int slots = n % 2 == 0 ? n : n + 1;
        int roundsG = (slots - 1) * d->rounds, perRound = slots / 2;
        restore_fixtures(&reader, n, 0);
        if (reader.custom) {
            perRound = 18;
            roundsG = 8;
        }
        int total = roundsG * perRound;
        int off = (int)rnd3(s, (float)q);
        int round = 0;
        for (int i = 0; i < total; i++) {
            int h = read_fixture(&reader), a = read_fixture(&reader);
            if (h == -1 || a == -1) {
                restore_fixtures(&reader, n, 0);
                h = read_fixture(&reader);
                a = read_fixture(&reader);
            }
            if (h > 0) h += off;
            if (a > 0) a += off;
            if (h > n) h -= n;
            if (a > n) a -= n;
            if (perRound > 0 && i % perRound == 0) {
                int r = round + 1;
                if (d->level == 1 && d->duration < 9) {
                    if (d->secondaryMatchday == 99) {
                        date = start;
                        if (groups > 4) date += (g / 2 + 1) + round * 4 - 1;
                        else date += groups * round + g;
                    } else {
                        while (ssr_date_day(date) != d->primaryMatchday && ssr_date_day(date) != d->secondaryMatchday) {
                            date++;
                        }
                    }
                } else if (mid > 0 && every > 0 && r % every == 0) {
                    date++;
                    int tries = 1;
                    while (ssr_date_day(date) == d->primaryMatchday || clash(s, c, date)) {
                        date++;
                        tries++;
                    }
                    if (tries <= 6) mid--;
                } else if (d->level == 1 && r % 2 == 0) {
                    while (ssr_date_day(date) != d->secondaryMatchday || clash(s, c, date)) date++;
                } else {
                    if (r > 1) date += gap > 1 ? 7 * gap : 1;
                    while (ssr_date_day(date) != d->primaryMatchday) date++;
                    while (clash(s, c, date)) date++;
                }
                round = r;
            }
            if (h > 0 && a > 0) add_fixture(s, c, date, SSR_FIXTURE_LEAGUE, round, g + 1, 0, h, a);
        }
    }
    sort_fixtures(s, c);
}

static int is_match_day(const SSRCompetition *d, int date) {
    int day = ssr_date_day(date);
    return day == d->primaryMatchday || day == d->secondaryMatchday;
}

// c_TCompetition.CreateFixtureListKO: the ties of one round. The teams come
// later; the ids of the pool are shuffled each time that a team comes.
static void cup_fixtures(SSRSeason *s, SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    c->fixtureCount = 0;
    int date = ssr_date_make(d->primaryMatchday, d->startWeek, c->startYear);
    int n = teams_in_round(s, c);
    int legs = d->legs > 1 ? d->legs : 1;
    FixReader reader = {0};
    for (int legNo = 1; legNo <= legs && legNo <= 2; legNo++) {
        int matchType = d->legs == 0 ? SSR_FIXTURE_EXTRA_TIME : d->legs == 1 ? SSR_FIXTURE_REPLAY_ON_DRAW
                        : legNo == 1  ? SSR_FIXTURE_FIRST_LEG
                                      : SSR_FIXTURE_SECOND_LEG;
        if (d->legs > 2) matchType = 0;
        restore_fixtures(&reader, n, 1);
        for (int i = 1; i <= n; i += 2) {
            int x = i, y = i + 1;
            if (n == 4 || n == 8 || n == 16 || n == 32) {
                x = read_fixture(&reader);
                y = read_fixture(&reader);
            }
            if (legNo == 2 && i == 1) {
                date += 7;
                while (!is_match_day(d, date)) date++;
                while (clash(s, c, date)) date++;
            } else if (d->level == 1) {
                if (i > 1 && i % 4 == 1) date++;
            } else if (i == 1) {
                while (!is_match_day(d, date)) date++;
                while (clash(s, c, date)) date++;
            }
            int leg = legs < 2 ? 0 : legNo;
            if (legNo == 1) add_fixture(s, c, date, matchType, 1, 1, leg, x, y);
            else add_fixture(s, c, date, matchType, 1, 1, leg, y, x);
        }
    }
}

// c_TCompetition.SetUpCompetition.
static void set_up_comp(SSRSeason *s, SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    create_pools(c);
    if (c->poolCount == 0) return;
    if (d->type == 0) {
        if (d->level == 1) {
            if (c->placesToMeCount == 0) populate(s, c);
            league_fixtures(s, c);
        } else if (d->locale == 0 || d->locale == 1) {
            populate(s, c);
            league_fixtures(s, c);
        }
    } else if (d->type == 4) {
        if (d->level == 0 && d->locale == 0) league_fixtures(s, c);
    } else if (d->type == 1) {
        if (d->level == 1) {
            if (c->placesToMeCount == 0) populate(s, c);
            cup_fixtures(s, c);
        } else if (d->locale == 0 || d->locale == 1) {
            populate(s, c);
            cup_fixtures(s, c);
        }
    }
}

// c_TClub.AverageOutStrengthAll: the strengths of the clubs of the nation
// go down in a straight line from the strongest, tier by tier. The game
// never finds the weakest, so the line ends at about 15.
static void average_strength(SSRSeason *s, int nat) {
    int *list = malloc((size_t)(s->clubListCount + 1) * sizeof(int));
    int n = 0, high = 0;
    if (!list) return;
    for (int i = 0; i < s->clubListCount; i++) {
        const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
        if (club->data->nation != nat) continue;
        if (club->strength > high) high = club->strength;
        list[n++] = s->clubList[i];
    }
    if (n > 0) {
        sort(s, list, n, compare_clubs_tier, NULL, 0);
        float step = (float)(high - 15) / (float)n, value = (float)high;
        for (int i = 0; i < n; i++) {
            s->clubs[list[i]].strength = (int)value;
            value -= step;
        }
    }
    free(list);
}

// c_TCompetition.Compare mode 22: the order of the set-up.
static int compare_comps_set_up(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    const SSRSeasonComp *x = &s->comps[a], *y = &s->comps[b];
    if (x->priority != y->priority) return compare_int(x->priority, y->priority);
    if (x->data->based != y->data->based) return compare_int(x->data->based, y->data->based);
    if (x->data->status != y->data->status) return compare_int(x->data->status, y->data->status);
    return compare_int(x->data->id, y->data->id);
}

static int compare_comps_id(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    return compare_int(s->comps[a].data->id, s->comps[b].data->id);
}

static int compare_clubs_id(SSRSeason *s, const void *context, int a, int b) {
    (void)context;
    return compare_int(s->clubs[a].data->id, s->clubs[b].data->id);
}

void ssr_season_set_up(SSRSeason *s) {
    if (!s) return;
    SSRSeasonClub *mine = ssr_season_club(s, s->player.clubId, 1);
    if (!mine) return;
    int nat = mine->data->nation;
    SSRSeasonNation *home = nation(s, nat);
    int continent = home ? home->data->continent : 0;
    int year = ssr_date_year(s->player.sdate);
    load_places(s);
    for (int i = 0; i < s->clubListCount; i++) {
        SSRSeasonClub *club = &s->clubs[s->clubList[i]];
        if (club->data->nation != nat) club->continental = 0;
    }
    average_strength(s, nat);
    for (int i = 0; i < s->compCount; i++) s->comps[i].priority = priority(s->comps[i].data);
    sort(s, s->compOrder, s->compCount, compare_comps_set_up, NULL, 0);
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[s->compOrder[i]];
        const SSRCompetition *d = c->data;
        if (d->level == 0 && (d->locale == 0 || d->locale == 1)) {
            int based = d->locale == 1 ? continent : nat;
            if (d->based == based) {
                c->startYear = year;
                set_up_comp(s, c);
            } else {
                clear_comp(c);
            }
        } else if (d->level == 1 && year % 4 == 1) {
            // One set-up serves the four years of the cycle.
            c->startYear = (year - 1) + d->startYear;
            set_up_comp(s, c);
        }
    }
    // Only the clubs of the nation stay loaded.
    int n = 0;
    for (int i = 0; i < s->clubListCount; i++) {
        int index = s->clubList[i];
        if (s->clubs[index].data->nation != nat) continue;
        s->clubs[index].continental = 0;
        s->clubList[n++] = index;
    }
    s->clubListCount = n;
    sort(s, s->clubList, n, compare_clubs_id, NULL, 0);
}

/* ---------------------------------------------------------- the matches */

static SSRPool *fixture_pool(SSRSeason *s, const SSRFixture *f) {
    SSRSeasonComp *c = ssr_season_comp(s, f->compId);
    if (!c || f->group < 1 || f->group > c->poolCount) return NULL;
    return &c->pools[f->group - 1];
}

int ssr_fixture_home_team(SSRSeason *s, const SSRFixture *f) {
    SSRTableRow *row = s && f ? ssr_season_row(s, fixture_pool(s, f), f->home) : NULL;
    return row ? row->teamId : 0;
}

int ssr_fixture_away_team(SSRSeason *s, const SSRFixture *f) {
    SSRTableRow *row = s && f ? ssr_season_row(s, fixture_pool(s, f), f->away) : NULL;
    return row ? row->teamId : 0;
}

// The first leg of the tie of a second leg (c_TFixture.GetFirstLegScore).
static const SSRFixture *first_leg(SSRSeason *s, const SSRFixture *f) {
    SSRSeasonComp *c = ssr_season_comp(s, f->compId);
    if (!c) return NULL;
    for (int i = 0; i < c->fixtureCount; i++) {
        const SSRFixture *x = &c->fixtures[i];
        if (x->leg == 1 && x->group == f->group && x->home == f->away && x->away == f->home) return x;
    }
    return NULL;
}

int ssr_fixture_winner(SSRSeason *s, const SSRFixture *f) {
    if (!s || !f) return 0;
    switch (f->matchType) {
    case SSR_FIXTURE_LEAGUE:
    case SSR_FIXTURE_REPLAY_ON_DRAW:
    case SSR_FIXTURE_FIRST_LEG:
        if (f->score1 > f->score2) return f->home;
        if (f->score2 > f->score1) return f->away;
        return 0;
    case SSR_FIXTURE_EXTRA_TIME:
        if (f->score1 > f->score2) return f->home;
        if (f->score1 >= f->score2 && f->pen1 > f->pen2) return f->home;
        return f->away;
    case SSR_FIXTURE_SECOND_LEG: {
        if (f->pen1 > f->pen2) return f->home;
        if (f->pen1 < f->pen2) return f->away;
        const SSRFixture *one = first_leg(s, f);
        int f1h = one ? one->score1 : 0, f1a = one ? one->score2 : 0;
        // The first-leg home team is the away team now.
        int away = f1h + f->score2, home = f1a + f->score1;
        if (away > home) return f->away;
        if (home > away) return f->home;
        if (2 * f->score2 + f1a > 2 * f1h + f->score1) return f->away;
        return f->home;
    }
    default:
        return 0;
    }
}

int ssr_fixture_loser(SSRSeason *s, const SSRFixture *f) {
    int w = ssr_fixture_winner(s, f);
    if (w == 0) return 0;
    return w == f->home ? f->away : f->home;
}

// c_TFixture.RandomGoal: 0 to 3 goals are as likely as each other, more
// goals are rare.
static int random_goal(SSRSeason *s) {
    static const int WEIGHTS[11] = {1536, 1536, 1536, 1536, 256, 32, 8, 4, 2, 1, 1};
    int r = (int)rnd2(s, 1, 6448);
    for (int i = 0; i < 11; i++) {
        if (WEIGHTS[i] > r) return i;
        r -= WEIGHTS[i];
    }
    return 0;
}

// c_TFixture.UpdatePoints. Penalties decide a win only after a draw.
static void update_points(SSRTableRow *h, SSRTableRow *a, const SSRFixture *f) {
    h->played++;
    h->goalsFor += f->score1;
    h->goalsAgainst += f->score2;
    if (f->score1 > f->score2 || (f->score1 <= f->score2 && f->pen1 > f->pen2)) {
        h->won++;
        h->points += 3;
    } else if (f->score1 == f->score2) {
        h->drawn++;
        h->points += 1;
    } else {
        h->lost++;
    }
    a->played++;
    a->goalsFor += f->score2;
    a->goalsAgainst += f->score1;
    if (f->score1 < f->score2 || (f->score1 >= f->score2 && f->pen1 < f->pen2)) {
        a->won++;
        a->points += 3;
    } else if (f->score1 == f->score2) {
        a->drawn++;
        a->points += 1;
    } else {
        a->lost++;
    }
}

// Extra time adds 0 or 1 goal to each side, then penalties of 2 to 6.
static void extra_time(SSRSeason *s, SSRFixture *f, int *level, int homeAdd, int awayAdd) {
    f->score1 = (int)((float)f->score1 + rnd2(s, 0, 2));
    f->score2 = (int)((float)f->score2 + rnd2(s, 0, 2));
    f->resultType = SSR_RESULT_EXTRA_TIME;
    *level = f->score1 + homeAdd == f->score2 + awayAdd;
    if (!*level) return;
    f->pen1 = (int)rnd2(s, 2, 7);
    f->pen2 = (int)rnd2(s, 2, 7);
    f->resultType = SSR_RESULT_PENALTIES;
    if (f->pen1 == f->pen2) {
        // The game tests Rnd3(2) with 1 and 2. This almost never holds,
        // so equal penalties nearly always stay equal.
        float x = rnd3(s, 2);
        if (x == 1.0f) f->pen1++;
        else if (x == 2.0f) f->pen2++;
    }
}

// c_TFixture.CreateReplayFixture: the replay waits for the end of the list.
static void create_replay(SSRSeason *s, SSRSeasonComp *c, const SSRFixture *f) {
    int date = f->sdate + 1;
    while (clash(s, c, date)) date++;
    SSRFixture *r = fixture_append(&c->replays, &c->replayCount, &c->replayCapacity);
    if (r) make_fixture(s, r, date, SSR_FIXTURE_EXTRA_TIME, 2, f->group, 0, f->away, f->home, c);
}

// c_TFixture.PlayFixture: the result of a match of other teams. Only the
// strength of the teams counts, and one match in five is an upset.
static void play_fixture(SSRSeason *s, SSRSeasonComp *c, SSRFixture *f) {
    f->result = 1;
    SSRPool *pool = fixture_pool(s, f);
    if (!pool) return;
    SSRTableRow *h = ssr_season_row(s, pool, f->home), *a = ssr_season_row(s, pool, f->away);
    SSRTableRow homeDummy, awayDummy;
    int walk = 0;
    if (!h) {
        memset(&homeDummy, 0, sizeof(homeDummy));
        homeDummy.randNo = (int)rnd3(s, 9999);
        h = &homeDummy;
        f->score1 = 0;
        f->score2 = 1;
        walk = 1;
    }
    if (!a) {
        memset(&awayDummy, 0, sizeof(awayDummy));
        awayDummy.randNo = (int)rnd3(s, 9999);
        a = &awayDummy;
        f->score1 = 1;
        f->score2 = 0;
        walk = 1;
    }
    if (!walk) {
        int g1 = random_goal(s), g2 = random_goal(s);
        if (abs(h->strength - a->strength) < 20) {
            if (g1 > g2 + 4) g1 = g2 + 4;
            if (g2 > g1 + 4) g2 = g1 + 4;
        }
        int upset = rnd3(s, 5) <= 1.0f;
        if (abs(g1 - g2) > 2) upset = 0;
        int high = g1 > g2 ? g1 : g2, low = g1 > g2 ? g2 : g1;
        int strongHome = h->strength >= a->strength;
        if (strongHome != upset) {
            f->score1 = high;
            f->score2 = low;
        } else {
            f->score1 = low;
            f->score2 = high;
        }
    }
    int level;
    switch (f->matchType) {
    case SSR_FIXTURE_REPLAY_ON_DRAW:
        f->resultType = SSR_RESULT_NORMAL;
        if (f->score1 == f->score2) create_replay(s, c, f);
        break;
    case SSR_FIXTURE_EXTRA_TIME:
        f->resultType = SSR_RESULT_NORMAL;
        if (f->score1 == f->score2) extra_time(s, f, &level, 0, 0);
        break;
    case SSR_FIXTURE_SECOND_LEG: {
        f->resultType = SSR_RESULT_AGGREGATE;
        const SSRFixture *one = first_leg(s, f);
        int f1h = one ? one->score1 : 0, f1a = one ? one->score2 : 0;
        if (f1a + f->score1 == f1h + f->score2) {
            if (2 * f1a + f->score1 != 2 * f->score2 + f1h) {
                f->resultType = SSR_RESULT_AWAY_GOALS;
            } else {
                extra_time(s, f, &level, f1a, f1h);
            }
        }
        break;
    }
    default:
        f->resultType = SSR_RESULT_NORMAL;
        break;
    }
    update_points(h, a, f);
}

// c_TCompetition.AddReplayFixtures: 1 when no replay waits.
static int add_replays(SSRSeasonComp *c) {
    if (c->replayCount == 0) return 1;
    for (int i = 0; i < c->replayCount; i++) {
        SSRFixture *f = fixture_append(&c->fixtures, &c->fixtureCount, &c->fixtureCapacity);
        if (f) *f = c->replays[i];
    }
    c->replayCount = 0;
    return 0;
}

/* ------------------------------------------------------------ promotion */

static void do_promotion(SSRSeason *s, SSRSeasonComp *c);

// c_TCompetition.GetHighestClubNotInContinentalComp: the first club of the
// table with no continental place, or NULL.
static SSRSeasonClub *highest_free_club(SSRSeason *s, SSRSeasonComp *c) {
    if (!c || c->poolCount == 0) return NULL;
    for (int i = 0; i < c->pools[0].count; i++) {
        SSRSeasonClub *club = ssr_season_club(s, s->rows[c->pools[0].rows[i]].teamId, 0);
        if (club && club->continental == 0) return club;
    }
    return NULL;
}

// c_TCompetition.PromoteToMe: the row comes to this competition from the
// competition from.
static void promote_to(SSRSeason *s, SSRSeasonComp *t, int rowIndex, const SSRSeasonComp *from) {
    if (rowIndex < 0 || t->poolCount == 0) return;
    const SSRCompetition *d = t->data;
    SSRTableRow *row = &s->rows[rowIndex];
    if (d->level == 0 && d->locale == 0 && d->type == 0) {
        // Promotion and relegation: the new league counts at the next
        // set-up.
        SSRSeasonClub *club = ssr_season_club(s, row->teamId, 0);
        if (club) club->league = d->id;
    } else if (d->level == 0 && d->locale == 1 && (d->type == 0 || d->type == 1) && from->data->locale == 0) {
        SSRSeasonClub *club = ssr_season_club(s, row->teamId, 1);
        if (!club) return;
        if (club->continental <= 0) {
            club->continental = d->id;
            return;
        }
        SSRSeasonClub *next = highest_free_club(s, ssr_season_comp(s, club->league));
        if (!next) return;
        SSRSeasonComp *e = ssr_season_comp(s, club->continental);
        if (e && e->data->status >= d->status && (e->data->status != d->status || e->data->startWeek <= d->startWeek)) {
            next->continental = club->continental;
            club->continental = d->id;
        } else {
            next->continental = d->id;
        }
    } else if (d->type == 2 || d->type == 3 || d->type == 5) {
        pool_append(&t->pools[0], rowIndex);
        if (t->pools[0].count == qualifiers(s, t)) do_promotion(s, t);
    } else if (d->type == 1) {
        pool_add(s, &t->pools[0], row->teamId, row->name, row->strength);
        if (from->data->groups < 2) shuffle_ids(s, &t->pools[0]);
    } else if (d->type == 4) {
        // The same row, with its points, has a new id in this table.
        row->id = t->pools[0].count + 1;
        pool_append(&t->pools[0], rowIndex);
    } else if (d->type == 0) {
        if (t->poolCount == 1) {
            pool_add(s, &t->pools[0], row->teamId, row->name, row->strength);
            return;
        }
        int first = t->pools[0].count;
        SSRPool *pool = &t->pools[0];
        for (int i = 0; i < t->poolCount; i++) {
            if (t->pools[i].count == 0 || t->pools[i].count < first) {
                pool = &t->pools[i];
                break;
            }
        }
        pool_add(s, pool, row->teamId, row->name, row->strength);
    }
}

static int row_index(const SSRSeason *s, const SSRPool *pool, int id) {
    for (int i = 0; i < pool->count; i++) {
        if (s->rows[pool->rows[i]].id == id) return pool->rows[i];
    }
    return -1;
}

// c_TCompetition.DoPromotionPlaces: the places of the finished competition
// send their teams on.
static void do_promotion(SSRSeason *s, SSRSeasonComp *c) {
    const SSRCompetition *d = c->data;
    for (int g = 0; g < c->poolCount; g++) {
        SSRPool *pool = &c->pools[g];
        if (d->type == 0 || d->type == 4) ssr_season_sort_pool(s, pool, 4);
        else if (d->type == 2 || d->type == 3) ssr_season_sort_pool(s, pool, 16);
        else if (d->type == 5 && (d->townRegion == 1 || d->townRegion == 3)) ssr_season_sort_pool(s, pool, 24);
        else if (d->type == 5 && (d->townRegion == 2 || d->townRegion == 4)) ssr_season_sort_pool(s, pool, 25);
    }
    for (int g = 0; g < c->poolCount; g++) {
        for (int k = 0; k < c->placeCount; k++) {
            const SSRPromotion *p = place_of(s, c->places[k]);
            SSRSeasonComp *t = ssr_season_comp(s, p->promotionTo);
            // A promotion pool can fill the pools of this competition, so
            // the pool comes again from the list each time.
            SSRPool *pool = &c->pools[g];
            if (!t) continue;
            if (p->place == 103 || p->place == 104 || p->place == 106) {
                for (int i = 0; i < c->fixtureCount; i++) {
                    const SSRFixture *f = &c->fixtures[i];
                    if (d->legs >= 2 && f->leg != 2) continue;
                    int id = p->place == 104 ? ssr_fixture_loser(s, f) : ssr_fixture_winner(s, f);
                    if (id != 0) promote_to(s, t, row_index(s, pool, id), c);
                }
            } else if (p->place == 105) {
                for (int i = 0; i < c->fixtureCount; i++) {
                    const SSRFixture *f = &c->fixtures[i];
                    if (d->legs >= 2 && f->leg != 2) continue;
                    int id = ssr_fixture_winner(s, f);
                    if (id == 0) continue;
                    SSRTableRow *row = ssr_season_row(s, pool, id);
                    SSRSeasonClub *club = row ? ssr_season_club(s, row->teamId, 1) : NULL;
                    if (club && club->continental != 0) id = ssr_fixture_loser(s, f);
                    if (id != 0) promote_to(s, t, row_index(s, pool, id), c);
                }
            } else if (p->place >= 1 && p->place <= 99 && p->place <= pool->count) {
                promote_to(s, t, pool->rows[p->place - 1], c);
            }
        }
    }
}

// c_TCompetition.IsCupFinal: a cup round that sends no team on to a
// competition of the nation.
static int is_cup_final(SSRSeason *s, const SSRSeasonComp *c, int playOff) {
    const SSRCompetition *d = c->data;
    if (d->type != 1) return 0;
    for (int k = 0; k < c->placeCount; k++) {
        SSRSeasonComp *t = place_target(s, c->places[k]);
        if (d->level == 0 && d->locale == 0) {
            if (t && t->data->locale == 0) return playOff && t->data->type == 0;
        } else if (d->level == 0 && d->locale == 1) {
            if (!t) continue;
            if (t->data->id == 555 || strcmp(t->data->tla, "SUPER") == 0) return 1;
            if (t->data->locale == 1) return 0;
        } else if (d->level == 1) {
            return 0;
        }
    }
    return 1;
}

int ssr_season_is_final(SSRSeason *s, const SSRSeasonComp *c) {
    return s && c && is_cup_final(s, c, 0);
}

int ssr_season_won_final(SSRSeason *s, const SSRSeasonComp *c, const SSRFixture *f, int team) {
    if (!s || !c || !f || team == 0 || f->result == 0 || f->matchType == SSR_FIXTURE_FIRST_LEG) return 0;
    if (!is_cup_final(s, c, 0)) return 0;
    const SSRTableRow *row = ssr_season_row(s, fixture_pool(s, f), ssr_fixture_winner(s, f));
    return row && row->teamId == team;
}

// c_TCompetition.DoCupWinnerNews: the headline of the winner of a final.
static void cup_winner_news(SSRSeason *s, SSRSeasonComp *c) {
    if (c->fixtureCount == 0) return;
    const SSRFixture *f = &c->fixtures[c->fixtureCount - 1];
    const SSRCompetition *d = c->data;
    int winner = ssr_fixture_winner(s, f);
    SSRTableRow *row = ssr_season_row(s, fixture_pool(s, f), winner);
    if (!row) return;
    if (d->level == 1) {
        SSRSeasonNation *mine = nation(s, s->player.nationId);
        if (!(d->locale == 2 || (mine && d->based == mine->data->continent))) return;
        SSRSeasonNation *n = nation(s, row->teamId);
        if (!n) return;
        copy_tla(s->winnerTla, sizeof(s->winnerTla), n->data->tla);
    } else {
        SSRSeasonClub *club = ssr_season_club(s, row->teamId, 1);
        if (!club) return;
        copy_tla(s->winnerTla, sizeof(s->winnerTla), club->data->tla);
    }
    copy_tla(s->winnerCompetition, sizeof(s->winnerCompetition), d->tla);
    s->hasWinnerNews = 1;
}

int ssr_season_play_fixtures(SSRSeason *s) {
    if (!s) return 0;
    int any = 0;
    SSRSeasonClub *mine = ssr_season_club(s, s->player.clubId, 0);
    int nat = mine ? mine->data->nation : 0;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[s->compOrder[i]];
        if (c->fixtureCount == 0) continue;
        int played = 0, complete = 1;
        for (int k = 0; k < c->fixtureCount; k++) {
            SSRFixture *f = &c->fixtures[k];
            if (f->sdate > s->player.sdate) {
                complete = 0;
                break;
            }
            if (f->result == -1) {
                f->result = 1;
                played = 1;
                if (s->played) s->played(s->playedContext, s, c, f);
            }
            if (f->result == 0) {
                play_fixture(s, c, f);
                played = 1;
                if (s->played && f->result == 1) s->played(s->playedContext, s, c, f);
            }
        }
        if (!played) continue;
        if (complete) complete = add_replays(c);
        sort_fixtures(s, c);
        if (complete) {
            do_promotion(s, c);
            if (is_cup_final(s, c, 0)) cup_winner_news(s, c);
        }
        const SSRCompetition *d = c->data;
        if (d->level == 1 || d->locale == 1 || d->based == nat) any = 1;
    }
    return any;
}

/* --------------------------------------------------------------- dates */

// 1 when the fixture a comes before b in a fixture list (mode 17).
static int fixture_before(const SSRFixture *a, const SSRFixture *b) {
    if (a->sdate != b->sdate) return a->sdate < b->sdate;
    if (a->level != b->level) return a->level > b->level;
    if (a->round != b->round) return a->round < b->round;
    return a->group < b->group;
}

// The first fixture with no result of the team, in the competitions of the
// level (0 clubs, 1 nations), with both sides there (c_TBase_Team).
static SSRFixture *team_next_fixture(SSRSeason *s, int teamId, int level) {
    SSRFixture *best = NULL;
    SSRSeasonClub *club = level == 0 ? ssr_season_club(s, teamId, 0) : NULL;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[i];
        const SSRCompetition *d = c->data;
        if (d->level != level) continue;
        if (level == 0 && !((d->locale == 0 && club && d->based == club->data->nation) || d->locale == 1)) continue;
        for (int g = 0; g < c->poolCount; g++) {
            SSRPool *pool = &c->pools[g];
            int id = 0;
            for (int r = 0; r < pool->count; r++) {
                if (s->rows[pool->rows[r]].teamId == teamId) id = s->rows[pool->rows[r]].id;
            }
            if (id == 0) continue;
            for (int k = 0; k < c->fixtureCount; k++) {
                SSRFixture *f = &c->fixtures[k];
                if (f->group != g + 1 || (f->home != id && f->away != id)) continue;
                if (f->result != 0 || f->home <= 0 || f->away <= 0) continue;
                if (!best || fixture_before(f, best)) best = f;
            }
        }
    }
    return best;
}

SSRFixture *ssr_season_next_fixture(SSRSeason *s) {
    if (!s || !ssr_season_club(s, s->player.clubId, 0)) return NULL;
    SSRFixture *c = team_next_fixture(s, s->player.clubId, 0);
    SSRFixture *n = team_next_fixture(s, s->player.nationId, 1);
    if (s->player.careerApps > 5 && c && n) return n->sdate <= c->sdate ? n : c;
    if (c) return c;
    if (n && ssr_date_year(n->sdate) == ssr_date_year(s->player.sdate)) return n;
    return NULL;
}

void ssr_season_skip_to(SSRSeason *s, int sdate) {
    if (!s || s->player.sdate >= sdate - 1) return;
    do {
        s->player.sdate++;
        ssr_season_play_fixtures(s);
    } while (s->player.sdate < sdate - 1);
}

int ssr_season_play_to(SSRSeason *s, int sdate) {
    if (!s) return 0;
    int noNext = ssr_season_next_fixture(s) == NULL, end = 0;
    do {
        s->player.sdate++;
        int played = ssr_season_play_fixtures(s);
        if (played && noNext && ssr_season_next_fixture(s)) break;
        if (s->player.sdate < sdate) played = 0;
        if (s->player.sdate == sdate) played = 1;
        if (ssr_date_week(s->player.sdate) == 1 && ssr_date_day(s->player.sdate) == 1) end = 1;
        if (played) break;
    } while (!end);
    return end;
}

void ssr_season_keep_positions(SSRSeason *s) {
    if (!s) return;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[i];
        int mode = c->data->type == 2 || c->data->type == 3 ? 16 : 4;
        for (int g = 0; g < c->poolCount; g++) {
            ssr_season_sort_pool(s, &c->pools[g], mode);
            for (int r = 0; r < c->pools[g].count; r++) s->rows[c->pools[g].rows[r]].prevPosition = r + 1;
        }
    }
}

/* ---------------------------------------------------------- the season */

SSRSeason *ssr_season_create(const SSRWorld *world, SSRRandom *random) {
    if (!world || !random) return NULL;
    SSRSeason *s = calloc(1, sizeof(SSRSeason));
    if (!s) return NULL;
    s->world = world;
    s->random = random;
    s->comps = calloc((size_t)world->competitionCount + 1, sizeof(SSRSeasonComp));
    s->compOrder = calloc((size_t)world->competitionCount + 1, sizeof(int));
    s->clubs = calloc((size_t)world->clubCount + 1, sizeof(SSRSeasonClub));
    s->clubList = calloc((size_t)world->clubCount + 1, sizeof(int));
    s->nations = calloc((size_t)world->nationCount + 1, sizeof(SSRSeasonNation));
    if (!s->comps || !s->compOrder || !s->clubs || !s->clubList || !s->nations) {
        ssr_season_free(s);
        return NULL;
    }
    for (int i = 0; i < world->competitionCount; i++) {
        if (world->competitions[i].id < 1) continue;
        s->comps[s->compCount].data = &world->competitions[i];
        s->comps[s->compCount].startYear = world->competitions[i].startYear;
        s->compOrder[s->compCount] = s->compCount;
        s->compCount++;
    }
    // A club with no league is not in the game (c_TClub.CreateClub).
    for (int i = 0; i < world->clubCount; i++) {
        if (world->clubs[i].id < 1 || world->clubs[i].league == 0) continue;
        reset_club(s, &s->clubs[s->clubCount], &world->clubs[i]);
        s->clubList[s->clubListCount++] = s->clubCount;
        s->clubCount++;
    }
    for (int i = 0; i < world->nationCount; i++) {
        s->nations[s->nationCount].data = &world->nations[i];
        s->nations[s->nationCount].randNo = (int)rnd3(s, 9999);
        s->nationCount++;
    }
    return s;
}

void ssr_season_free(SSRSeason *s) {
    if (!s) return;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[i];
        for (int g = 0; g < c->poolCount; g++) free(c->pools[g].rows);
        free(c->pools);
        free(c->fixtures);
        free(c->replays);
        free(c->places);
        free(c->placesToMe);
    }
    free(s->comps);
    free(s->compOrder);
    free(s->clubs);
    free(s->clubList);
    free(s->nations);
    free(s->rows);
    free(s);
}

void ssr_season_new_season(SSRSeason *s) {
    if (!s) return;
    SSRSeasonClub *mine = ssr_season_club(s, s->player.clubId, 0);
    if (!mine) return;
    int nat = mine->data->nation;
    // The leagues, strengths, and continental places of the clubs of the
    // nation, by id.
    int n = 0;
    int *saved = malloc((size_t)(s->clubListCount + 1) * 4 * sizeof(int));
    if (!saved) return;
    for (int i = 0; i < s->clubListCount; i++) {
        const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
        if (club->data->nation != nat) continue;
        saved[n * 4] = club->data->id;
        saved[n * 4 + 1] = club->league;
        saved[n * 4 + 2] = club->strength;
        saved[n * 4 + 3] = club->continental;
        n++;
    }
    // All the clubs again, in the order of the file (c_TClub.LoadData(0)).
    s->clubListCount = 0;
    for (int i = 0; i < s->clubCount; i++) {
        reset_club(s, &s->clubs[i], s->clubs[i].data);
        s->clubList[s->clubListCount++] = i;
        if (s->clubs[i].data->nation != nat) s->clubs[i].continental = 0;
    }
    for (int i = 0; i < n; i++) {
        int index = club_index(s, saved[i * 4]);
        if (index < 0) continue;
        s->clubs[index].league = saved[i * 4 + 1];
        s->clubs[index].strength = saved[i * 4 + 2];
        s->clubs[index].continental = saved[i * 4 + 3];
    }
    free(saved);
    for (int i = 0; i < s->rowCount; i++) s->rows[i].prevPosition = -1;
    ssr_season_set_up(s);
}

/* ------------------------------------------------------------- the save */

static void write_fixture(FILE *file, const char *kind, const SSRFixture *f) {
    fprintf(file, "%s %d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d\n", kind, f->sdate, f->matchType, f->round,
            f->group, f->leg, f->home, f->away, f->result, f->resultType, f->score1, f->score2, f->pen1, f->pen2,
            f->level, f->compId, f->randNo);
}

int ssr_season_save(SSRSeason *s, const char *path) {
    if (!s || !path) return 0;
    char temp[1100];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "w");
    if (!file) return 0;
    // c_TPlayer.GetCompetitionData sorts the competitions by id.
    sort(s, s->compOrder, s->compCount, compare_comps_id, NULL, 0);
    fprintf(file, "season 1\n");
    for (int i = 0; i < s->clubListCount; i++) {
        const SSRSeasonClub *club = &s->clubs[s->clubList[i]];
        fprintf(file, "club %d/%d/%d/%d\n", club->data->id, club->strength, club->league, club->continental);
    }
    for (int i = 0; i < s->compCount; i++) {
        const SSRSeasonComp *c = &s->comps[s->compOrder[i]];
        if (c->poolCount == 0 && c->fixtureCount == 0) continue;
        fprintf(file, "comp %d/%d/%d\n", c->data->id, c->startYear, c->poolCount);
        for (int g = 0; g < c->poolCount; g++) {
            fprintf(file, "pool %d\n", g + 1);
            for (int r = 0; r < c->pools[g].count; r++) {
                const SSRTableRow *row = &s->rows[c->pools[g].rows[r]];
                fprintf(file, "row %d/%d/%s/%d/%d/%d/%d/%d/%d/%d/%d/%d\n", row->id, row->teamId, row->name,
                        row->strength, row->played, row->won, row->drawn, row->lost, row->goalsFor,
                        row->goalsAgainst, row->points, row->prevPosition);
            }
        }
        for (int k = 0; k < c->fixtureCount; k++) write_fixture(file, "fixture", &c->fixtures[k]);
        for (int k = 0; k < c->replayCount; k++) write_fixture(file, "replay", &c->replays[k]);
    }
    int ok = fprintf(file, "end\n") > 0;
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

// Reads the numbers of a "a/b/c" field list. Returns the count.
static int read_numbers(const char *text, int *out, int max) {
    int n = 0;
    while (*text && n < max) {
        out[n++] = atoi(text);
        const char *slash = strchr(text, '/');
        if (!slash) break;
        text = slash + 1;
    }
    return n;
}

static void read_fixture_line(const char *text, SSRFixture *f) {
    int v[16] = {0};
    read_numbers(text, v, 16);
    f->sdate = v[0];
    f->matchType = v[1];
    f->round = v[2];
    f->group = v[3];
    f->leg = v[4];
    f->home = v[5];
    f->away = v[6];
    f->result = v[7];
    f->resultType = v[8];
    f->score1 = v[9];
    f->score2 = v[10];
    f->pen1 = v[11];
    f->pen2 = v[12];
    f->level = v[13];
    f->compId = v[14];
    f->randNo = v[15];
}

int ssr_season_load(SSRSeason *s, const char *path) {
    if (!s || !path) return 0;
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char line[512];
    if (!fgets(line, sizeof(line), file) || strncmp(line, "season ", 7) != 0) {
        fclose(file);
        return 0;
    }
    for (int i = 0; i < s->compCount; i++) {
        clear_comp(&s->comps[i]);
        s->comps[i].startYear = s->comps[i].data->startYear;
        s->compOrder[i] = i;
    }
    s->rowCount = 0;
    s->clubListCount = 0;
    SSRSeasonComp *comp = NULL;
    SSRPool *pool = NULL;
    int complete = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        int v[16] = {0};
        if (strncmp(line, "club ", 5) == 0) {
            read_numbers(line + 5, v, 4);
            int index = club_index(s, v[0]);
            if (index < 0) continue;
            reset_club(s, &s->clubs[index], s->clubs[index].data);
            s->clubs[index].strength = v[1];
            s->clubs[index].league = v[2];
            s->clubs[index].continental = v[3];
            s->clubList[s->clubListCount++] = index;
        } else if (strncmp(line, "comp ", 5) == 0) {
            read_numbers(line + 5, v, 3);
            comp = ssr_season_comp(s, v[0]);
            pool = NULL;
            if (!comp) continue;
            comp->startYear = v[1];
            if (v[2] > 0) {
                comp->pools = calloc((size_t)v[2], sizeof(SSRPool));
                comp->poolCount = comp->pools ? v[2] : 0;
            }
        } else if (strncmp(line, "pool ", 5) == 0 && comp) {
            int g = atoi(line + 5);
            pool = g >= 1 && g <= comp->poolCount ? &comp->pools[g - 1] : NULL;
        } else if (strncmp(line, "row ", 4) == 0 && pool) {
            // The name is the third field, and it has no "/".
            const char *p = line + 4;
            int id = atoi(p);
            p = strchr(p, '/');
            int teamId = p ? atoi(++p) : 0;
            p = p ? strchr(p, '/') : NULL;
            if (!p) continue;
            p++;
            const char *slash = strchr(p, '/');
            if (!slash) continue;
            char name[16];
            size_t length = (size_t)(slash - p) < sizeof(name) - 1 ? (size_t)(slash - p) : sizeof(name) - 1;
            memcpy(name, p, length);
            name[length] = 0;
            read_numbers(slash + 1, v, 9);
            int row = new_row(s, teamId, name, v[0]);
            if (row < 0) continue;
            SSRTableRow *r = &s->rows[row];
            r->id = id;
            r->played = v[1];
            r->won = v[2];
            r->drawn = v[3];
            r->lost = v[4];
            r->goalsFor = v[5];
            r->goalsAgainst = v[6];
            r->points = v[7];
            r->prevPosition = v[8];
            pool_append(pool, row);
        } else if (strncmp(line, "fixture ", 8) == 0 && comp) {
            SSRFixture *f = fixture_append(&comp->fixtures, &comp->fixtureCount, &comp->fixtureCapacity);
            if (f) read_fixture_line(line + 8, f);
        } else if (strncmp(line, "replay ", 7) == 0 && comp) {
            SSRFixture *f = fixture_append(&comp->replays, &comp->replayCount, &comp->replayCapacity);
            if (f) read_fixture_line(line + 7, f);
        } else if (strcmp(line, "end") == 0) {
            complete = 1;
        }
    }
    fclose(file);
    // The places come from the data set again, as c_TPlayer.LoadCareer
    // reads them.
    load_places(s);
    return complete;
}

/* ------------------------------------------------- the match of the player */

void ssr_season_my_result(SSRSeason *s, SSRFixture *f, int score1, int score2) {
    if (!s || !f) return;
    SSRSeasonComp *c = ssr_season_comp(s, f->compId);
    SSRPool *pool = fixture_pool(s, f);
    if (!c || !pool) return;
    f->score1 = score1;
    f->score2 = score2;
    f->pen1 = f->pen2 = 0;
    f->resultType = SSR_RESULT_NORMAL;
    int level;
    if (f->matchType == SSR_FIXTURE_REPLAY_ON_DRAW && score1 == score2) {
        create_replay(s, c, f);
    } else if (f->matchType == SSR_FIXTURE_EXTRA_TIME && score1 == score2) {
        extra_time(s, f, &level, 0, 0);
    } else if (f->matchType == SSR_FIXTURE_SECOND_LEG) {
        f->resultType = SSR_RESULT_AGGREGATE;
        const SSRFixture *one = first_leg(s, f);
        int f1h = one ? one->score1 : 0, f1a = one ? one->score2 : 0;
        if (f1a + score1 == f1h + score2) {
            if (2 * f1a + score1 != 2 * score2 + f1h) f->resultType = SSR_RESULT_AWAY_GOALS;
            else extra_time(s, f, &level, f1a, f1h);
        }
    }
    f->result = -1;
    SSRTableRow *h = ssr_season_row(s, pool, f->home), *a = ssr_season_row(s, pool, f->away);
    if (h && a) update_points(h, a, f);
}

void ssr_season_my_result_after_extra_time(SSRSeason *s, SSRFixture *f, int score1, int score2, int pen1,
                                           int pen2) {
    if (!s || !f) return;
    SSRPool *pool = fixture_pool(s, f);
    if (!pool) return;
    f->score1 = score1;
    f->score2 = score2;
    f->pen1 = pen1;
    f->pen2 = pen2;
    f->resultType = pen1 != pen2 ? SSR_RESULT_PENALTIES : SSR_RESULT_EXTRA_TIME;
    f->result = -1;
    SSRTableRow *h = ssr_season_row(s, pool, f->home), *a = ssr_season_row(s, pool, f->away);
    if (h && a) update_points(h, a, f);
}

SSRFixture *ssr_season_find(SSRSeason *s, int compId, int sdate, int home, int away) {
    SSRSeasonComp *c = ssr_season_comp(s, compId);
    if (!c) return NULL;
    for (int i = 0; i < c->fixtureCount; i++) {
        SSRFixture *f = &c->fixtures[i];
        if (f->sdate == sdate && f->home == home && f->away == away) return f;
    }
    return NULL;
}

// The last played fixture of the team before its next one.
static SSRFixture *team_prev_fixture(SSRSeason *s, int teamId, int level) {
    SSRFixture *next = team_next_fixture(s, teamId, level), *best = NULL;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[i];
        if (c->data->level != level) continue;
        int group = ssr_season_group_of(s, c, teamId);
        if (group == 0) continue;
        int id = 0;
        SSRPool *pool = &c->pools[group - 1];
        for (int r = 0; r < pool->count; r++) {
            if (s->rows[pool->rows[r]].teamId == teamId) id = s->rows[pool->rows[r]].id;
        }
        for (int k = 0; k < c->fixtureCount; k++) {
            SSRFixture *f = &c->fixtures[k];
            if (f->group != group || (f->home != id && f->away != id) || f->result == 0) continue;
            if (next && !fixture_before(f, next)) continue;
            if (!best || fixture_before(best, f)) best = f;
        }
    }
    return best;
}

SSRFixture *ssr_season_prev_fixture(SSRSeason *s) {
    if (!s) return NULL;
    SSRFixture *c = team_prev_fixture(s, s->player.clubId, 0);
    SSRFixture *n = team_prev_fixture(s, s->player.nationId, 1);
    if (s->player.careerApps > 6 && c && n) return c->sdate <= n->sdate ? n : c;
    return c ? c : n;
}

int ssr_season_rest_days(SSRSeason *s) {
    SSRFixture *next = ssr_season_next_fixture(s);
    int days = next ? next->sdate - s->player.sdate - 1 : 14;
    return days < 3 ? 3 : days > 15 ? 15 : days;
}

int ssr_season_next_round(const SSRSeasonComp *c) {
    for (int i = 0; c && i < c->fixtureCount; i++) {
        if (c->fixtures[i].result == 0) return c->fixtures[i].round;
    }
    return 0;
}

int ssr_season_prev_round(const SSRSeasonComp *c) {
    int round = 0;
    for (int i = 0; c && i < c->fixtureCount; i++) {
        if (c->fixtures[i].result == 1 && c->fixtures[i].round > round) round = c->fixtures[i].round;
    }
    return round;
}

int ssr_season_round_date(const SSRSeasonComp *c, int round) {
    for (int i = 0; c && i < c->fixtureCount; i++) {
        if (c->fixtures[i].round == round) return c->fixtures[i].sdate;
    }
    return 0;
}

int ssr_season_comp_type(SSRSeason *s, const SSRSeasonComp *c) {
    if (!s || !c) return 0;
    const SSRCompetition *d = c->data;
    if (d->level == 1) {
        SSRSeasonNation *mine = nation(s, s->player.nationId);
        return d->locale == 2 || (mine && d->based == mine->data->continent) ? 4 : 0;
    }
    if (d->level != 0) return 0;
    if (d->locale == 1) return 3;
    if (d->locale != 0) return 0;
    if (d->type == 0 || d->type == 4) return 1;
    if (d->type == 1) return 2;
    return 0;
}

int ssr_season_all_populated(SSRSeason *s, const SSRSeasonComp *c) {
    if (!s || !c) return 0;
    for (int i = 0; i < c->fixtureCount; i++) {
        SSRPool *pool = fixture_pool(s, &c->fixtures[i]);
        if (!ssr_season_row(s, pool, c->fixtures[i].home) || !ssr_season_row(s, pool, c->fixtures[i].away)) return 0;
    }
    return 1;
}

static int has_place(const SSRSeason *s, const SSRSeasonComp *c, int position) {
    for (int i = 0; c && i < c->placeCount; i++) {
        if (s->world->promotions[c->places[i]].place == position) return 1;
    }
    return 0;
}

int ssr_season_is_promotion_place(const SSRSeason *s, const SSRSeasonComp *c, int position) {
    return s && has_place(s, c, position) && position < 10;
}

int ssr_season_is_relegation_place(const SSRSeason *s, const SSRSeasonComp *c, int position) {
    return s && has_place(s, c, position) && position > 10;
}

int ssr_season_group_of(SSRSeason *s, const SSRSeasonComp *c, int teamId) {
    for (int g = 0; s && c && g < c->poolCount; g++) {
        for (int r = 0; r < c->pools[g].count; r++) {
            if (s->rows[c->pools[g].rows[r]].teamId == teamId) return g + 1;
        }
    }
    return 0;
}

int ssr_season_actual_league(SSRSeason *s, int clubId) {
    int league = 0;
    for (int i = 0; s && i < s->compCount; i++) {
        const SSRSeasonComp *c = &s->comps[s->compOrder[i]];
        if (!is_league_type(c->data->type) || c->data->locale != 0 || c->data->level != 0) continue;
        if (ssr_season_group_of(s, c, clubId)) league = c->data->id;
    }
    return league;
}
