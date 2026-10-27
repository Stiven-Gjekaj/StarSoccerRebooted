// Tests ssr_people with a small world and name lists that the test writes.

#include <stdio.h>
#include <string.h>

#include "ssr_people.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static const char NAMES[] = "# names\n"
                            "first north Ann Bob Cid\n"
                            "last north Lake Hill\n"
                            "first south Dee\n"
                            "last south Sand\n"
                            "nation 2 south\n"
                            "continent 6 north\n";

static void write_text(const char *directory, const char *name, const char *text) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    fputs(text, file);
    fclose(file);
}

// Two nations, a strong club and a weak club in one league.
static SSRWorld *small_world(void) {
    const char *d = ssr_test_directory("people_world");
    write_text(d, "Continents.txt", "id\tname\ttla\n6\tEurope\tEUR\n");
    write_text(d, "Nations.txt", "id\tshortname\ttla\tcontinent\n1\tNorland\tNOR\t6\n2\tSudland\tSUD\t6\n");
    write_text(d, "Clubs.txt",
               "id\tshortname\ttla\tstrength\tnationid\tleagueid\n"
               "10\tBig Club\tBIG\t90\t1\t100\n"
               "11\tSmall Club\tSMALL\t40\t1\t100\n");
    write_text(d, "Competitions.txt", "id\ttla\tlocale\n100\tNOR1\t0\n");
    write_text(d, "PromotionPlaces.txt", "parentid\tplace\tpromotiontoid\n");
    return ssr_world_load(d);
}

static void test_picks_names_by_nation(void) {
    SSRNames *names = ssr_names_parse(NAMES);
    CHECK(names != NULL);
    SSRRandom r = {5};
    char first[24], last[32];
    CHECK(ssr_names_pick(names, 2, 6, &r, first, sizeof(first), last, sizeof(last)));
    CHECK(strcmp(first, "Dee") == 0 && strcmp(last, "Sand") == 0);
    // A nation with no line takes its continent.
    CHECK(ssr_names_pick(names, 1, 6, &r, first, sizeof(first), last, sizeof(last)));
    CHECK(strcmp(last, "Lake") == 0 || strcmp(last, "Hill") == 0);
    ssr_names_free(names);
    CHECK(ssr_names_parse("# nothing\nnation 1 x\n") == NULL);
}

// A squad has 22 players in four positions. The skills follow the
// strength of the club. The same seed makes the same squad.
static void test_makes_a_squad(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    CHECK(world && names);
    SSRPeople *a = ssr_people_create(77), *b = ssr_people_create(77);
    CHECK(ssr_people_make_squad(a, world, names, 10, 1) == SSR_SQUAD_SIZE);
    CHECK(ssr_people_make_squad(a, world, names, 10, 1) == 0);
    CHECK(ssr_people_make_squad(a, world, names, 11, 1) == SSR_SQUAD_SIZE);
    CHECK(ssr_people_make_squad(a, world, names, 999, 1) == 0);
    ssr_people_make_squad(b, world, names, 10, 1);
    CHECK(strcmp(a->list[5].last, b->list[5].last) == 0 && a->list[5].skill == b->list[5].skill);
    SSRPerson *squad[40];
    int n = ssr_people_squad(a, 10, squad, 40), keepers = 0, big = 0, small = 0;
    for (int i = 0; i < n; i++) {
        keepers += squad[i]->position == SSR_POS_KEEPER;
        big += squad[i]->skill;
        int age = 1 - squad[i]->born;
        CHECK(age >= 17 && age <= 34 && squad[i]->potential >= squad[i]->skill);
    }
    CHECK(n == 22 && keepers == 3);
    n = ssr_people_squad(a, 11, squad, 40);
    for (int i = 0; i < n; i++) small += squad[i]->skill;
    CHECK(big / 22 > small / 22 + 30);
    ssr_people_free(a);
    ssr_people_free(b);
    ssr_names_free(names);
    ssr_world_free(world);
}

// Eleven players play, and the goals of the match go to them.
static void test_plays_a_match(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    SSRPeople *people = ssr_people_create(3);
    ssr_people_make_squad(people, world, names, 10, 1);
    ssr_people_play(people, 10, 4);
    SSRPerson *squad[40];
    int n = ssr_people_squad(people, 10, squad, 40), apps = 0, goals = 0, keeperGoals = 0;
    for (int i = 0; i < n; i++) {
        apps += squad[i]->seasonApps;
        goals += squad[i]->seasonGoals;
        if (squad[i]->position == SSR_POS_KEEPER) keeperGoals += squad[i]->goals;
        CHECK(squad[i]->seasonGoals == 0 || squad[i]->seasonApps == 1);
    }
    CHECK(apps == 11 && goals == 4 && keeperGoals == 0);
    SSRPerson *top[3];
    int clubs[1] = {10};
    CHECK(ssr_people_top_scorers(people, clubs, 1, top, 3) >= 1 && top[0]->seasonGoals >= 1);
    ssr_people_free(people);
    ssr_names_free(names);
    ssr_world_free(world);
}

// Over the seasons the old players retire, young players fill the squad,
// and a star of the small club moves to the big club.
static void test_runs_the_seasons(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    SSRPeople *people = ssr_people_create(9);
    ssr_people_make_squad(people, world, names, 10, 1);
    ssr_people_make_squad(people, world, names, 11, 1);
    // A star at the small club.
    SSRPerson *squad[40];
    ssr_people_squad(people, 11, squad, 40);
    int star = squad[21]->id;
    squad[21]->skill = squad[21]->potential = 95;
    squad[21]->born = -24;
    squad[21]->seasonGoals = 20;
    int moved = 0;
    for (int year = 2; year <= 24; year++) {
        int now = ssr_people_new_season(people, world, names, year);
        moved += now;
        CHECK(people->moveCount == (now < 16 ? now : 16));
        for (int m = 0; m < people->moveCount; m++) CHECK(ssr_people_find(people, people->moves[m][0])->club == people->moves[m][2]);
        CHECK(ssr_people_squad(people, 10, NULL, 0) >= SSR_SQUAD_SIZE);
        CHECK(ssr_people_squad(people, 11, NULL, 0) >= SSR_SQUAD_SIZE);
        if (year == 2) CHECK(ssr_people_find(people, star)->seasonGoals == 0);
    }
    CHECK(moved > 0 && people->moveCount <= moved);
    int retired = 0, firstSquad = 0;
    for (int i = 0; i < people->count; i++) {
        retired += people->list[i].retired > 0;
        firstSquad += people->list[i].id <= 44 && !people->list[i].retired;
    }
    // After 23 years all the first players are 40 or more, and gone.
    CHECK(retired >= 44 && firstSquad == 0);
    ssr_people_free(people);
    ssr_names_free(names);
    ssr_world_free(world);
}

// Each club with a squad has a manager. A manager of a club at the bottom
// goes more often, and the style of a manager picks the team.
static void test_runs_the_managers(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    SSRPeople *people = ssr_people_create(21);
    ssr_people_make_squad(people, world, names, 10, 1);
    ssr_people_make_squad(people, world, names, 11, 1);
    CHECK(people->managerCount == 2 && ssr_people_manager(people, 11)->since == 1);
    CHECK(1 - ssr_people_manager(people, 11)->born >= 38);
    int bottom[1] = {11}, sackedSmall = 0, sackedBig = 0;
    for (int year = 2; year < 42; year++) {
        ssr_people_judge_managers(people, world, names, bottom, 1, year);
        for (int k = 0; k < people->sackedCount; k++) {
            sackedSmall += people->sacked[k] == 11;
            sackedBig += people->sacked[k] == 10;
        }
    }
    CHECK(sackedSmall > 15 && sackedBig < 10 && people->managerCount == 2);
    CHECK(ssr_people_manager(people, 11)->since > 1);
    // An attacking manager plays three attackers.
    ssr_people_manager(people, 10)->style = SSR_STYLE_ATTACKING;
    ssr_people_play(people, 10, 0);
    SSRPerson *squad[40];
    int n = ssr_people_squad(people, 10, squad, 40), attackers = 0;
    for (int i = 0; i < n; i++) attackers += squad[i]->position == SSR_POS_ATTACKER && squad[i]->seasonApps == 1;
    CHECK(attackers == 3);
    ssr_people_free(people);
    ssr_names_free(names);
    ssr_world_free(world);
}

static void test_makes_rivals(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    SSRPeople *people = ssr_people_create(4);
    CHECK(ssr_people_make_rivals(people, world, names, 100, 1, -15, 3, 1) == 3);
    for (int i = 0; i < people->count; i++) {
        CHECK(people->list[i].rival && people->list[i].born == -15);
        CHECK(people->list[i].club == 10 || people->list[i].club == 11);
    }
    CHECK(ssr_people_make_rivals(people, world, names, 555, 1, -15, 3, 1) == 0);
    // A rival grows fast to the skill that the rival can reach.
    int id = people->list[0].id, start = people->list[0].skill;
    CHECK(start >= 8 && start <= 20 && people->list[0].potential >= 70);
    for (int year = 2; year <= 6; year++) ssr_people_new_season(people, world, names, year);
    const SSRPerson *rival = ssr_people_find(people, id);
    CHECK(rival->skill >= start + (rival->potential - start) / 2);
    ssr_people_free(people);
    ssr_names_free(names);
    ssr_world_free(world);
}

// The file keeps all players and the stream of the numbers.
static void test_keeps_the_people(void) {
    SSRWorld *world = small_world();
    SSRNames *names = ssr_names_parse(NAMES);
    SSRPeople *people = ssr_people_create(12);
    ssr_people_make_squad(people, world, names, 10, 1);
    ssr_people_play(people, 10, 2);
    people->list[3].injury = 9;
    char path[1200];
    snprintf(path, sizeof(path), "%s/people.txt", ssr_test_directory("people_file"));
    CHECK(ssr_people_save(people, path));
    SSRPeople *back = ssr_people_load(path);
    CHECK(back && back->count == people->count && back->nextId == people->nextId);
    CHECK(back && back->seed == 12 && back->random.seed == people->random.seed);
    CHECK(back && memcmp(&back->list[3], &people->list[3], sizeof(SSRPerson)) == 0);
    CHECK(back && back->managerCount == 1 && back->year == people->year);
    CHECK(back && memcmp(back->managers, people->managers, sizeof(SSRManager)) == 0);
    // The same numbers come next.
    ssr_people_play(people, 10, 3);
    if (back) ssr_people_play(back, 10, 3);
    CHECK(back && memcmp(back->list, people->list, sizeof(SSRPerson) * (size_t)people->count) == 0);
    ssr_people_free(back);
    CHECK(ssr_people_load("/no/such/file") == NULL);
    ssr_people_free(people);
    ssr_names_free(names);
    ssr_world_free(world);
}

int main(void) {
    test_picks_names_by_nation();
    test_makes_a_squad();
    test_plays_a_match();
    test_runs_the_seasons();
    test_runs_the_managers();
    test_makes_rivals();
    test_keeps_the_people();
    return TEST_RESULT();
}
