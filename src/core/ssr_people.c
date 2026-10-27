#include "ssr_people.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_tsv.h"

/* ------------------------------------------------------------ the names */

typedef struct {
    char *name;
    char **first;
    int firstCount;
    char **last;
    int lastCount;
} NameGroup;

typedef struct {
    int id;
    int group;
} GroupLink;

// A real player of a club, from world/Players.txt of a pack.
typedef struct {
    int club;
    char first[24];
    char last[32];
    int nation, age, position, skill, potential;
} RosterEntry;

// The position of the manager in world/Players.txt.
#define ROSTER_MANAGER SSR_POS_COUNT

struct SSRNames {
    char *text;
    RosterEntry *roster;
    int rosterCount;
    NameGroup *groups;
    int groupCount;
    GroupLink *nations;
    int nationCount;
    GroupLink *continents;
    int continentCount;
};

static int group_index(SSRNames *names, const char *name, int make) {
    for (int i = 0; i < names->groupCount; i++) {
        if (strcmp(names->groups[i].name, name) == 0) return i;
    }
    if (!make) return -1;
    NameGroup *grown = realloc(names->groups, sizeof(NameGroup) * (size_t)(names->groupCount + 1));
    if (!grown) return -1;
    names->groups = grown;
    memset(&grown[names->groupCount], 0, sizeof(NameGroup));
    grown[names->groupCount].name = (char *)name;
    return names->groupCount++;
}

static void add_link(GroupLink **links, int *count, int id, int group) {
    GroupLink *grown = realloc(*links, sizeof(GroupLink) * (size_t)(*count + 1));
    if (!grown) return;
    *links = grown;
    grown[*count].id = id;
    grown[*count].group = group;
    (*count)++;
}

// Splits the text in place into its words. Returns the number of words.
static int split_words(char *line, char **words, int max) {
    int count = 0;
    char *p = line;
    while (*p && count < max) {
        while (*p == ' ' || *p == '\t') *p++ = 0;
        if (!*p) break;
        words[count++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return count;
}

SSRNames *ssr_names_parse(const char *text) {
    if (!text) return NULL;
    SSRNames *names = calloc(1, sizeof(SSRNames));
    if (!names) return NULL;
    names->text = malloc(strlen(text) + 1);
    if (!names->text) {
        free(names);
        return NULL;
    }
    strcpy(names->text, text);
    char *line = names->text;
    while (line && *line) {
        char *end = line + strcspn(line, "\r\n");
        char *next = *end ? end + 1 : NULL;
        *end = 0;
        if (next && *next == '\n') next++;
        if (line[0] != '#') {
            char *words[512];
            int count = split_words(line, words, 512);
            if (count >= 3 && (strcmp(words[0], "first") == 0 || strcmp(words[0], "last") == 0)) {
                int g = group_index(names, words[1], 1);
                if (g >= 0) {
                    NameGroup *group = &names->groups[g];
                    int isFirst = words[0][0] == 'f';
                    char ***list = isFirst ? &group->first : &group->last;
                    int *n = isFirst ? &group->firstCount : &group->lastCount;
                    char **grown = realloc(*list, sizeof(char *) * (size_t)(*n + count - 2));
                    if (grown) {
                        *list = grown;
                        for (int i = 2; i < count; i++) grown[(*n)++] = words[i];
                    }
                }
            } else if (count == 3 && strcmp(words[0], "nation") == 0) {
                add_link(&names->nations, &names->nationCount, atoi(words[1]), group_index(names, words[2], 1));
            } else if (count == 3 && strcmp(words[0], "continent") == 0) {
                add_link(&names->continents, &names->continentCount, atoi(words[1]), group_index(names, words[2], 1));
            }
        }
        line = next;
    }
    int good = 0;
    for (int i = 0; i < names->groupCount; i++) good += names->groups[i].firstCount && names->groups[i].lastCount;
    if (!good) {
        ssr_names_free(names);
        return NULL;
    }
    return names;
}

void ssr_names_free(SSRNames *names) {
    if (!names) return;
    for (int i = 0; i < names->groupCount; i++) {
        free(names->groups[i].first);
        free(names->groups[i].last);
    }
    free(names->groups);
    free(names->roster);
    free(names->nations);
    free(names->continents);
    free(names->text);
    free(names);
}

static int roster_position(const char *text) {
    static const char *const NAMES[] = {"GK", "DEF", "MID", "ATT", "MGR"};
    for (int i = 0; i <= ROSTER_MANAGER; i++) {
        if (strcmp(text, NAMES[i]) == 0) return i;
    }
    return -1;
}

int ssr_names_add_players(SSRNames *names, const char *text) {
    if (!names || !text) return 0;
    SSRTsv *table = ssr_tsv_parse(text, strlen(text));
    if (!table) return 0;
    int club = ssr_tsv_column(table, "club"), first = ssr_tsv_column(table, "first");
    int last = ssr_tsv_column(table, "last"), position = ssr_tsv_column(table, "position");
    int rows = ssr_tsv_rows(table), added = 0;
    RosterEntry *grown = club < 0 || last < 0 || position < 0 ? NULL
                         : realloc(names->roster, sizeof(RosterEntry) * (size_t)(names->rosterCount + rows + 1));
    if (grown) names->roster = grown;
    for (int row = 0; grown && row < rows; row++) {
        RosterEntry e = {0};
        e.club = ssr_tsv_int(table, row, club, 0);
        e.position = roster_position(ssr_tsv_cell(table, row, position));
        const char *surname = ssr_tsv_cell(table, row, last);
        // A row with no club, no surname, or no good position is not a player.
        if (e.club <= 0 || !*surname || e.position < 0) continue;
        snprintf(e.first, sizeof(e.first), "%s", ssr_tsv_cell(table, row, first));
        snprintf(e.last, sizeof(e.last), "%s", surname);
        e.nation = ssr_tsv_int(table, row, ssr_tsv_column(table, "nation"), 0);
        e.age = ssr_tsv_int(table, row, ssr_tsv_column(table, "age"), 0);
        e.skill = ssr_tsv_int(table, row, ssr_tsv_column(table, "skill"), 0);
        e.potential = ssr_tsv_int(table, row, ssr_tsv_column(table, "potential"), 0);
        names->roster[names->rosterCount++] = e;
        added++;
    }
    ssr_tsv_free(table);
    return added;
}

static int linked_group(const GroupLink *links, int count, int id) {
    for (int i = 0; i < count; i++) {
        if (links[i].id == id) return links[i].group;
    }
    return -1;
}

int ssr_names_pick(const SSRNames *names, int nation, int continent, SSRRandom *random, char *first,
                   size_t firstCapacity, char *last, size_t lastCapacity) {
    if (!names) return 0;
    int g = linked_group(names->nations, names->nationCount, nation);
    if (g < 0) g = linked_group(names->continents, names->continentCount, continent);
    if (g < 0 || !names->groups[g].firstCount || !names->groups[g].lastCount) {
        for (g = 0; g < names->groupCount; g++) {
            if (names->groups[g].firstCount && names->groups[g].lastCount) break;
        }
    }
    const NameGroup *group = &names->groups[g];
    snprintf(first, firstCapacity, "%s", group->first[ssr_rand(random, group->firstCount) - 1]);
    snprintf(last, lastCapacity, "%s", group->last[ssr_rand(random, group->lastCount) - 1]);
    return 1;
}

/* ----------------------------------------------------------- the people */

SSRPeople *ssr_people_create(int seed) {
    SSRPeople *people = calloc(1, sizeof(SSRPeople));
    if (!people) return NULL;
    people->seed = seed;
    people->random.seed = seed;
    people->nextId = 1;
    return people;
}

void ssr_people_free(SSRPeople *people) {
    if (!people) return;
    free(people->managers);
    free(people->list);
    free(people);
}

SSRPerson *ssr_people_find(SSRPeople *people, int id) {
    for (int i = 0; people && i < people->count; i++) {
        if (people->list[i].id == id) return &people->list[i];
    }
    return NULL;
}

static SSRPerson *add_person(SSRPeople *people) {
    if (people->count == people->capacity) {
        int capacity = people->capacity ? people->capacity * 2 : 256;
        SSRPerson *grown = realloc(people->list, sizeof(SSRPerson) * (size_t)capacity);
        if (!grown) return NULL;
        people->list = grown;
        people->capacity = capacity;
    }
    SSRPerson *p = &people->list[people->count++];
    memset(p, 0, sizeof(*p));
    p->id = people->nextId++;
    return p;
}

static int clamp(int value, int low, int high) {
    return value < low ? low : value > high ? high : value;
}

// The skill and the skill to reach of a new player of the age at a club of
// the strength. A young player is below the level of the club, and can
// grow past it.
static void set_skill(SSRPerson *p, SSRRandom *r, int strength, int age) {
    int skill = strength + ssr_rand2(r, -10, 8);
    if (age < 24) skill -= (24 - age) * 3;
    if (age > 31) skill -= (age - 31) * 2;
    p->skill = clamp(skill, 1, 99);
    int growth = age < 24 ? (24 - age) * 3 + ssr_rand2(r, -4, 12) : 0;
    p->potential = clamp(p->skill + growth, p->skill, 100);
}

// The nation of a new player: mostly the nation of the club, else another
// nation of the same continent.
static int pick_nation(SSRRandom *r, const SSRWorld *world, int home) {
    const SSRNation *own = ssr_world_nation(world, home);
    if (!own || ssr_rand(r, 100) <= 85) return home;
    for (int tries = 0; tries < 20; tries++) {
        const SSRNation *n = &world->nations[ssr_rand(r, world->nationCount) - 1];
        if (n->continent == own->continent) return n->id;
    }
    return home;
}

static SSRPerson *new_player(SSRPeople *people, const SSRWorld *world, const SSRNames *names, const SSRClub *club,
                             int position, int age, int year) {
    SSRPerson *p = add_person(people);
    if (!p) return NULL;
    SSRRandom *r = &people->random;
    p->club = club->id;
    p->position = position;
    p->born = year - age;
    p->nation = pick_nation(r, world, club->nation);
    const SSRNation *nation = ssr_world_nation(world, p->nation);
    ssr_names_pick(names, p->nation, nation ? nation->continent : 0, r, p->first, sizeof(p->first), p->last,
                   sizeof(p->last));
    set_skill(p, r, club->strength, age);
    return p;
}

// The positions of a squad of 22: 3 keepers, 7 defenders, 7 midfielders,
// and 5 attackers.
static const int SHAPE[SSR_POS_COUNT] = {3, 7, 7, 5};

int ssr_people_squad(SSRPeople *people, int club, SSRPerson **out, int max) {
    int n = 0;
    for (int i = 0; people && i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        if (p->club != club || p->retired) continue;
        if (out && n < max) out[n] = p;
        n++;
    }
    return n;
}

SSRManager *ssr_people_manager(SSRPeople *people, int club) {
    for (int i = 0; people && i < people->managerCount; i++) {
        if (people->managers[i].club == club) return &people->managers[i];
    }
    return NULL;
}

// A new manager for the club takes the place of the old one.
static SSRManager *new_manager(SSRPeople *people, const SSRWorld *world, const SSRNames *names, const SSRClub *club,
                               int year) {
    SSRManager *m = ssr_people_manager(people, club->id);
    if (!m) {
        if (people->managerCount == people->managerCapacity) {
            int capacity = people->managerCapacity ? people->managerCapacity * 2 : 64;
            SSRManager *grown = realloc(people->managers, sizeof(SSRManager) * (size_t)capacity);
            if (!grown) return NULL;
            people->managers = grown;
            people->managerCapacity = capacity;
        }
        m = &people->managers[people->managerCount++];
    }
    SSRRandom *r = &people->random;
    memset(m, 0, sizeof(*m));
    m->club = club->id;
    m->nation = pick_nation(r, world, club->nation);
    const SSRNation *nation = ssr_world_nation(world, m->nation);
    ssr_names_pick(names, m->nation, nation ? nation->continent : 0, r, m->first, sizeof(m->first), m->last,
                   sizeof(m->last));
    m->born = year - ssr_rand2(r, 38, 62);
    m->style = ssr_rand(r, SSR_STYLE_COUNT) - 1;
    m->since = year;
    return m;
}

int ssr_people_judge_managers(SSRPeople *people, const SSRWorld *world, const SSRNames *names, const int *bottom,
                              int bottomCount, int year) {
    if (!people || !world || !names) return 0;
    people->sackedCount = 0;
    int made = 0;
    for (int i = 0; i < people->managerCount; i++) {
        int club = people->managers[i].club, low = 0;
        for (int k = 0; k < bottomCount && !low; k++) low = bottom[k] == club;
        // Six in ten managers in the bottom places go, and one in twenty
        // of the others.
        int goes = low ? ssr_rand(&people->random, 10) <= 6 : ssr_rand(&people->random, 20) == 1;
        const SSRClub *c = ssr_world_club(world, club);
        if (!goes || !c || !new_manager(people, world, names, c, year)) continue;
        if (people->sackedCount < 16) people->sacked[people->sackedCount++] = club;
        made++;
    }
    return made;
}

int ssr_people_make_squad(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int club, int year) {
    const SSRClub *c = ssr_world_club(world, club);
    if (!people || !c || !names) return 0;
    if (year > people->year) people->year = year;
    // A squad of a save from before the managers gets its manager now.
    if (ssr_people_squad(people, club, NULL, 0) > 0) {
        if (!ssr_people_manager(people, club)) new_manager(people, world, names, c, year);
        return 0;
    }
    int made = 0, have[SSR_POS_COUNT] = {0};
    // The real players of the club come first, then made-up players fill
    // each position up to the shape of a squad.
    for (int i = 0; i < names->rosterCount && made < SSR_SQUAD_MAX; i++) {
        const RosterEntry *e = &names->roster[i];
        if (e->club != club) continue;
        if (e->position == ROSTER_MANAGER) {
            SSRManager *m = new_manager(people, world, names, c, year);
            if (!m) continue;
            snprintf(m->first, sizeof(m->first), "%s", e->first);
            snprintf(m->last, sizeof(m->last), "%s", e->last);
            if (ssr_world_nation(world, e->nation)) m->nation = e->nation;
            if (e->age > 0) m->born = year - e->age;
            continue;
        }
        int age = e->age > 0 ? e->age : ssr_rand2(&people->random, 17, 34);
        SSRPerson *p = new_player(people, world, names, c, e->position, age, year);
        if (!p) continue;
        snprintf(p->first, sizeof(p->first), "%s", e->first);
        snprintf(p->last, sizeof(p->last), "%s", e->last);
        if (ssr_world_nation(world, e->nation)) p->nation = e->nation;
        if (e->skill > 0) {
            int growth = p->potential - p->skill;
            p->skill = clamp(e->skill, 1, 99);
            p->potential = clamp(e->potential > 0 ? e->potential : p->skill + growth, p->skill, 100);
        }
        have[e->position]++;
        made++;
    }
    for (int pos = 0; pos < SSR_POS_COUNT; pos++) {
        for (int k = have[pos]; k < SHAPE[pos]; k++) made += new_player(people, world, names, c, pos, ssr_rand2(&people->random, 17, 34), year) != NULL;
    }
    if (!ssr_people_manager(people, club)) new_manager(people, world, names, c, year);
    return made;
}

/* ---------------------------------------------------------- the matches */

// The best fit players of the position who are not yet in the team.
static int pick_best(SSRPerson **squad, int n, int *used, int position, SSRPerson **team, int count, int wanted) {
    for (int k = 0; k < wanted; k++) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (used[i] || squad[i]->injury > 0 || squad[i]->position != position) continue;
            if (best < 0 || squad[i]->skill > squad[best]->skill) best = i;
        }
        // No fit player of the position: the best fit player of any.
        for (int i = 0; best < 0 && i < n; i++) {
            if (!used[i] && squad[i]->injury == 0) best = i;
        }
        if (best < 0) break;
        used[best] = 1;
        team[count++] = squad[best];
    }
    return count;
}

// A player of the team, with weights by position and skill.
static SSRPerson *pick_weighted(SSRRandom *r, SSRPerson **team, int count, const int *weights, const SSRPerson *not) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        if (team[i] != not) total += weights[team[i]->position] * team[i]->skill;
    }
    if (total <= 0) return NULL;
    int pick = ssr_rand(r, total);
    for (int i = 0; i < count; i++) {
        if (team[i] == not) continue;
        pick -= weights[team[i]->position] * team[i]->skill;
        if (pick <= 0) return team[i];
    }
    return NULL;
}

void ssr_people_play(SSRPeople *people, int club, int goals) {
    SSRPerson *squad[64], *team[11];
    int n = ssr_people_squad(people, club, squad, 64);
    if (n > 64) n = 64;
    if (n == 0) return;
    int used[64] = {0}, count = 0;
    const SSRManager *manager = ssr_people_manager(people, club);
    int style = manager ? manager->style : SSR_STYLE_BALANCED;
    // A youth manager counts a player under 23 as 5 skill points better.
    int bonus[64] = {0};
    for (int i = 0; style == SSR_STYLE_YOUTH && i < n; i++) {
        if (people->year - squad[i]->born < 23) bonus[i] = 5;
        squad[i]->skill += bonus[i];
    }
    // A 4-4-2, or a 4-3-3 of an attacking manager.
    int attack = style == SSR_STYLE_ATTACKING;
    count = pick_best(squad, n, used, SSR_POS_KEEPER, team, count, 1);
    count = pick_best(squad, n, used, SSR_POS_DEFENDER, team, count, 4);
    count = pick_best(squad, n, used, SSR_POS_MIDFIELDER, team, count, attack ? 3 : 4);
    count = pick_best(squad, n, used, SSR_POS_ATTACKER, team, count, attack ? 3 : 2);
    for (int i = 0; i < n; i++) squad[i]->skill -= bonus[i];
    SSRRandom *r = &people->random;
    for (int i = 0; i < count; i++) {
        team[i]->apps++;
        team[i]->seasonApps++;
        // About one injury in 250 appearances.
        if (ssr_rand(r, 250) == 1) team[i]->injury = ssr_rand2(r, 7, 60);
    }
    static const int SCORE[SSR_POS_COUNT] = {0, 1, 3, 6};
    static const int ASSIST[SSR_POS_COUNT] = {0, 2, 5, 3};
    for (int g = 0; g < goals; g++) {
        SSRPerson *scorer = pick_weighted(r, team, count, SCORE, NULL);
        if (!scorer) break;
        scorer->goals++;
        scorer->seasonGoals++;
        if (ssr_rand(r, 10) > 7) continue;
        SSRPerson *helper = pick_weighted(r, team, count, ASSIST, scorer);
        if (!helper) continue;
        helper->assists++;
        helper->seasonAssists++;
    }
}

void ssr_people_pass_days(SSRPeople *people, int days) {
    for (int i = 0; people && i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        p->injury = p->injury > days ? p->injury - days : 0;
    }
}

int ssr_people_make_rivals(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int league,
                           int nation, int born, int count, int year) {
    const SSRClub *clubs[64];
    int n = ssr_world_league_clubs(world, league, clubs, 64);
    if (n > 64) n = 64;
    if (!people || n == 0 || !names) return 0;
    int made = 0;
    for (int k = 0; k < count; k++) {
        const SSRClub *club = clubs[ssr_rand(&people->random, n) - 1];
        int position = ssr_rand2(&people->random, SSR_POS_DEFENDER, SSR_POS_ATTACKER);
        SSRPerson *p = new_player(people, world, names, club, position, year - born, year);
        if (!p) break;
        p->nation = ssr_rand(&people->random, 2) == 1 ? nation : p->nation;
        p->born = born;
        p->rival = 1;
        // A rival starts as a youth player with a big future, at the level
        // of a new player of the user.
        p->skill = ssr_rand2(&people->random, 8, 20);
        p->potential = ssr_rand2(&people->random, 70, 95);
        made++;
    }
    return made;
}

/* ----------------------------------------------------------- the season */

// The player gets better, or worse with the age.
static void grow(SSRPerson *p, SSRRandom *r, int age) {
    int change = 0;
    if (age <= 23) change = ssr_rand2(r, 1, 6);
    else if (age <= 28) change = ssr_rand2(r, 0, 3);
    else if (age <= 31) change = ssr_rand2(r, -1, 1);
    else change = -ssr_rand2(r, 1, 4);
    // A rival grows as fast as a player who trains: a quarter of the way
    // to the skill to reach each year until 23.
    if (p->rival && age <= 23 && (p->potential - p->skill) / 4 > change) change = (p->potential - p->skill) / 4;
    if (change > 0) change = p->skill + change > p->potential ? p->potential - p->skill : change;
    p->skill = clamp(p->skill + change, 1, 100);
}

static int wants_to_retire(SSRPerson *p, SSRRandom *r, int age) {
    if (age >= 39) return 1;
    if (p->club == 0 && age >= 30) return 1;
    return age >= 33 && ssr_rand(r, 100) <= (age - 32) * 15;
}

// The clubs with a squad, each once.
static int squad_clubs(SSRPeople *people, int *clubs, int max) {
    int n = 0;
    for (int i = 0; i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        if (p->retired || p->club == 0) continue;
        int seen = 0;
        for (int k = 0; k < n && !seen; k++) seen = clubs[k] == p->club;
        if (!seen && n < max) clubs[n++] = p->club;
    }
    return n;
}

// The best players of a small club move to a bigger club of the same
// nation that has a squad. The weakest player of the same position at the
// bigger club goes the other way.
static int transfers(SSRPeople *people, const SSRWorld *world, const int *clubs, int clubCount, int year) {
    SSRRandom *r = &people->random;
    int moves = 0;
    for (int i = 0; i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        const SSRClub *from = p->retired ? NULL : ssr_world_club(world, p->club);
        if (!from || year - p->born > 30 || p->skill < from->strength + 8 || ssr_rand(r, 10) > 4) continue;
        // The club of the nation with a strength nearest to the skill.
        const SSRClub *to = NULL;
        for (int k = 0; k < clubCount; k++) {
            const SSRClub *c = ssr_world_club(world, clubs[k]);
            if (!c || c->id == from->id || c->nation != from->nation || c->strength <= from->strength) continue;
            int gap = abs(c->strength - p->skill), best = to ? abs(to->strength - p->skill) : 1000;
            if (gap < best) to = c;
        }
        if (!to) continue;
        SSRPerson *out = NULL;
        for (int k = 0; k < people->count; k++) {
            SSRPerson *q = &people->list[k];
            if (q->retired || q->club != to->id || q->position != p->position || q->rival) continue;
            if (!out || q->skill < out->skill) out = q;
        }
        if (!out || out->skill >= p->skill) continue;
        out->club = from->id;
        p->club = to->id;
        if (people->moveCount < 16) {
            int *move = people->moves[people->moveCount++];
            move[0] = p->id;
            move[1] = from->id;
            move[2] = to->id;
        }
        moves++;
    }
    return moves;
}

int ssr_people_new_season(SSRPeople *people, const SSRWorld *world, const SSRNames *names, int year) {
    if (!people || !world) return 0;
    people->year = year;
    SSRRandom *r = &people->random;
    for (int i = 0; i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        p->seasonApps = p->seasonGoals = p->seasonAssists = 0;
        if (p->retired) continue;
        int age = year - p->born;
        grow(p, r, age);
        if (wants_to_retire(p, r, age)) {
            p->retired = year;
            p->club = 0;
            p->injury = 0;
        }
    }
    int clubs[4096];
    int clubCount = squad_clubs(people, clubs, 4096);
    people->moveCount = 0;
    int moves = transfers(people, world, clubs, clubCount, year);
    // New young players fill each squad.
    for (int k = 0; k < clubCount; k++) {
        const SSRClub *club = ssr_world_club(world, clubs[k]);
        if (!club || !names) continue;
        int have[SSR_POS_COUNT] = {0};
        SSRPerson *squad[64];
        int n = ssr_people_squad(people, club->id, squad, 64);
        for (int i = 0; i < n && i < 64; i++) have[squad[i]->position]++;
        for (int pos = 0; pos < SSR_POS_COUNT; pos++) {
            for (; have[pos] < SHAPE[pos]; have[pos]++) new_player(people, world, names, club, pos, ssr_rand2(r, 17, 19), year);
        }
    }
    return moves;
}

static int by_goals(const void *a, const void *b) {
    const SSRPerson *p = *(SSRPerson *const *)a, *q = *(SSRPerson *const *)b;
    if (p->seasonGoals != q->seasonGoals) return q->seasonGoals - p->seasonGoals;
    if (p->seasonApps != q->seasonApps) return p->seasonApps - q->seasonApps;
    return p->id - q->id;
}

int ssr_people_top_scorers(SSRPeople *people, const int *clubs, int clubCount, SSRPerson **out, int max) {
    if (!people || max <= 0) return 0;
    SSRPerson **all = malloc(sizeof(SSRPerson *) * (size_t)(people->count + 1));
    if (!all) return 0;
    int n = 0;
    for (int i = 0; i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        int in = 0;
        for (int k = 0; k < clubCount && !in; k++) in = clubs[k] == p->club;
        if (in && p->seasonGoals > 0 && !p->retired) all[n++] = p;
    }
    qsort(all, (size_t)n, sizeof(SSRPerson *), by_goals);
    if (n > max) n = max;
    memcpy(out, all, sizeof(SSRPerson *) * (size_t)n);
    free(all);
    return n;
}

void ssr_people_rebase(SSRPeople *people, int shift) {
    if (!people || shift == 0) return;
    for (int i = 0; i < people->count; i++) {
        SSRPerson *p = &people->list[i];
        p->born -= shift;
        if (p->retired) {
            p->retired -= shift;
            if (p->retired == 0) p->retired = -1;
        }
    }
    for (int i = 0; i < people->managerCount; i++) {
        people->managers[i].born -= shift;
        people->managers[i].since -= shift;
    }
    people->year -= shift;
}

SSRPerson *ssr_people_add_former(SSRPeople *people, const char *first, const char *last, int nation, int born,
                                 int club, int skill, int apps, int goals, int assists) {
    SSRPerson *p = ssr_people_add_legend(people, first, last, nation, born, 0, apps, goals, assists);
    if (!p) return NULL;
    p->retired = 0;
    p->club = club;
    p->skill = skill < 1 ? 1 : skill > 99 ? 99 : skill;
    p->potential = p->skill;
    p->rival = SSR_FORMER_PLAYER;
    return p;
}

SSRPerson *ssr_people_add_legend(SSRPeople *people, const char *first, const char *last, int nation, int born,
                                 int retired, int apps, int goals, int assists) {
    SSRPerson *p = people ? add_person(people) : NULL;
    if (!p) return NULL;
    snprintf(p->first, sizeof(p->first), "%s", first ? first : "");
    snprintf(p->last, sizeof(p->last), "%s", last ? last : "");
    p->nation = nation;
    p->born = born;
    p->retired = retired ? retired : -1;
    p->position = SSR_POS_ATTACKER;
    p->apps = apps;
    p->goals = goals;
    p->assists = assists;
    return p;
}

/* ------------------------------------------------------------- the file */

#define FILE_TAG "ssrpeople"

int ssr_people_save(const SSRPeople *people, const char *path) {
    if (!people || !path) return 0;
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return 0;
    int ok = fprintf(file, "%s\t1\t%d\t%d\t%d\t%d\n", FILE_TAG, people->seed, (int)people->random.seed, people->nextId,
                     people->year) > 0;
    for (int i = 0; ok && i < people->managerCount; i++) {
        const SSRManager *m = &people->managers[i];
        ok = fprintf(file, "M\t%d\t%s\t%s\t%d\t%d\t%d\t%d\n", m->club, m->first, m->last, m->nation, m->born, m->style,
                     m->since) > 0;
    }
    for (int i = 0; ok && i < people->count; i++) {
        const SSRPerson *p = &people->list[i];
        ok = fprintf(file, "%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", p->id, p->first,
                     p->last, p->nation, p->club, p->born, p->position, p->skill, p->potential, p->injury, p->retired,
                     p->rival, p->apps, p->goals, p->assists, p->seasonApps, p->seasonGoals, p->seasonAssists) > 0;
    }
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

SSRPeople *ssr_people_load(const char *path) {
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file) return NULL;
    char line[512];
    int version = 0, seed = 0, stream = 0, next = 0, year = 0;
    if (!fgets(line, sizeof(line), file) ||
        sscanf(line, FILE_TAG "\t%d\t%d\t%d\t%d\t%d", &version, &seed, &stream, &next, &year) < 4 || version != 1) {
        fclose(file);
        return NULL;
    }
    SSRPeople *people = ssr_people_create(seed);
    if (!people) {
        fclose(file);
        return NULL;
    }
    people->random.seed = stream;
    people->year = year;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'M' && line[1] == '\t') {
            SSRManager m;
            memset(&m, 0, sizeof(m));
            if (sscanf(line + 2, "%d\t%23[^\t]\t%31[^\t]\t%d\t%d\t%d\t%d", &m.club, m.first, m.last, &m.nation, &m.born,
                       &m.style, &m.since) != 7) {
                continue;
            }
            if (people->managerCount == people->managerCapacity) {
                int capacity = people->managerCapacity ? people->managerCapacity * 2 : 64;
                SSRManager *grown = realloc(people->managers, sizeof(SSRManager) * (size_t)capacity);
                if (!grown) break;
                people->managers = grown;
                people->managerCapacity = capacity;
            }
            people->managers[people->managerCount++] = m;
            continue;
        }
        SSRPerson p;
        memset(&p, 0, sizeof(p));
        if (sscanf(line, "%d\t%23[^\t]\t%31[^\t]\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d", &p.id,
                   p.first, p.last, &p.nation, &p.club, &p.born, &p.position, &p.skill, &p.potential, &p.injury,
                   &p.retired, &p.rival, &p.apps, &p.goals, &p.assists, &p.seasonApps, &p.seasonGoals,
                   &p.seasonAssists) != 18) {
            continue;
        }
        SSRPerson *slot = add_person(people);
        if (!slot) break;
        *slot = p;
    }
    fclose(file);
    people->nextId = next;
    return people;
}
