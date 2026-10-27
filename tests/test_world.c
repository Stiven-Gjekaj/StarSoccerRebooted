// Tests ssr_world with a small data set that the test writes.

#include <math.h>
#include <string.h>

#include "ssr_test.h"
#include "ssr_world.h"
#include "ssr_zipwriter.h"

static void write_text(const char *directory, const char *name, const char *text) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    fputs(text, file);
    fclose(file);
}

static const char *write_set(const char *name) {
    const char *directory = ssr_test_directory(name);
    write_text(directory, "Continents.txt",
               "id\tname\ttla\tcontinentality\tfederationname\tfederationshortname\tstrength\r\n"
               "6\tEurope\tEUR\tEuropean\tUnion of European Football Associations\tUEFA\t100\r\n");
    write_text(directory, "Nations.txt",
               "\xEF\xBB\xBFid\tshortname\ttla\tstrength\tstadiumlongitude\tstadiumlatitude\thomekittype\t"
               "homekitshirtcol1\thomekitshirtcol2\thomekitshortscol\tawaykitshirtcol1\tawaykitshortscol\t"
               "nationality\tcontinent\r\n"
               "62\tEngland\tENGLAND\t90\t-0.28\t51.55\t1\t#FFF\t#FFF\t#000080\t#FF0000\t#FF0000\tEnglish\t6\r\n");
    write_text(directory, "Clubs.txt",
               "\xEF\xBB\xBFid\tshortname\ttla\tstrength\tstadiumlongitude\tstadiumlatitude\thomestyle\t"
               "homekitshirtcol1\thomekitshirtcol2\thomekitshortscol\tawaykitshirtcol1\tawaykitshortscol\t"
               "nationid\tleagueid\tcontinentalcompid\tbteamof\r\n"
               "810\tNorth Club\tNORTH\t97\t-0.1\t51.56\t10\t#FF0000\t#FFFFFF\t#FFFFFF\t#8080FF\t#000080\t62\t2270\t380\t0\r\n"
               "915\tWest Club\tWEST\t98\t-2.96\t53.43\t1\t#FF0000\t#FFFFFF\t#FF0000\t#CCC\t#444\t62\t2270\t380\t0\r\n"
               "916\tWest Club B\tWEST B\t60\t-2.96\t53.43\t1\t#FF0000\t#FFFFFF\t#FF0000\t#CCC\t#444\t62\t2280\t0\t915\r\n"
               "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\r\n");
    write_text(directory, "Competitions.txt",
               "\xEF\xBB\xBFid\ttla\tlocale\tlevel\tbased\tcomptype\tstartyear\tstartweek\tduration\trecurring\t"
               "primarymatchday\tsecondarymatchday\tgroups\trounds\tlegs\ttownregion\tcompstatus\r\n"
               "2270\tENG1\t0\t1\t62\t0\t1\t2\t38\t1\t6\t99\t1\t2\t0\t0\t1\r\n");
    write_text(directory, "PromotionPlaces.txt", "parentid\tplace\tpromotiontoid\r\n2280\t1\t2270\r\n");
    return directory;
}

static void test_loads_a_data_set(void) {
    SSRWorld *world = ssr_world_load(write_set("world"));
    CHECK(world != NULL);
    if (!world) return;
    CHECK(world->continentCount == 1 && world->nationCount == 1);
    CHECK(world->clubCount == 3);
    CHECK(world->competitionCount == 1 && world->promotionCount == 1);

    const SSRNation *england = ssr_world_nation(world, 62);
    CHECK(england && strcmp(england->name, "England") == 0 && strcmp(england->nationality, "English") == 0);
    CHECK(england && england->continent == 6 && england->home.shorts == 0x000080);
    CHECK(strcmp(ssr_world_continent(world, 6)->federationShort, "UEFA") == 0);

    const SSRClub *west = ssr_world_club(world, 915);
    CHECK(west && strcmp(west->name, "West Club") == 0 && west->strength == 98);
    CHECK(west && west->away.shirt1 == 0xCCCCCC && west->away.shirt2 == 0xCCCCCC);
    CHECK(west && west->latitude > 53.42 && west->latitude < 53.44);
    CHECK(ssr_world_club(world, 916)->bTeamOf == 915);
    CHECK(ssr_world_club(world, 1) == NULL);

    const SSRCompetition *league = ssr_world_competition(world, 2270);
    CHECK(league && strcmp(league->tla, "ENG1") == 0 && league->duration == 38 && league->based == 62);
    CHECK(world->promotions[0].parent == 2280 && world->promotions[0].promotionTo == 2270);

    const SSRClub *clubs[4];
    CHECK(ssr_world_league_clubs(world, 2270, clubs, 4) == 2);
    CHECK(clubs[0]->id == 810 && clubs[1]->id == 915);
    CHECK(ssr_world_league_clubs(world, 2270, clubs, 1) == 2);
    // North Club is in London and West Club in Liverpool: about 280 km. A
    // B team plays at the same stadium as its first team: a derby.
    double far = ssr_world_club_distance(world, 810, 915);
    CHECK(far > 250 && far < 320 && !ssr_world_is_derby(world, 810, 915));
    CHECK(ssr_world_club_distance(world, 915, 916) < 1 && ssr_world_is_derby(world, 915, 916));
    CHECK(ssr_world_club_distance(world, 810, 1) < 0 && !ssr_world_is_derby(world, 915, 915));
    ssr_world_free(world);
}

static void test_refuses_a_set_with_a_missing_file(void) {
    const char *directory = write_set("partial");
    char path[1200];
    snprintf(path, sizeof(path), "%s/Clubs.txt", directory);
    remove(path);
    CHECK(ssr_world_load(directory) == NULL);
}

// A data pack changes the clubs. The other files come from the data set.
// A row of the pack changes the club with its id. The other clubs stay.
static void test_reads_a_pack_over_the_data_set(void) {
    char set[1024];
    snprintf(set, sizeof(set), "%s", write_set("packbase"));
    const char *pack = ssr_test_directory("packover");
    write_text(pack, "Clubs.txt",
               "id\tshortname\ttla\tstrength\tstadiumlongitude\tstadiumlatitude\thomestyle\t"
               "homekitshirtcol1\thomekitshirtcol2\thomekitshortscol\tawaykitshirtcol1\tawaykitshortscol\t"
               "nationid\tleagueid\tcontinentalcompid\tbteamof\r\n"
               "810\tHarbour Town\tHARBOUR\t80\t0\t50\t1\t#00F\t#00F\t#FFF\t#FFF\t#000\t62\t2270\t0\t0\r\n");
    SSRWorld *world = ssr_world_load_pack(set, pack);
    CHECK(world != NULL);
    if (!world) return;
    CHECK(world->clubCount == 3 && strcmp(ssr_world_club(world, 810)->name, "Harbour Town") == 0);
    CHECK(strcmp(ssr_world_club(world, 915)->name, "West Club") == 0);
    CHECK(world->nationCount == 1 && world->competitionCount == 1);
    ssr_world_free(world);
}

// A pack gives only the columns that it changes. A new id adds a record.
// The promotion places of a league in the pack take the place of the
// places of that league.
static void test_merges_the_rows_of_a_pack(void) {
    char set[1024];
    snprintf(set, sizeof(set), "%s", write_set("mergebase"));
    const char *pack = ssr_test_directory("mergepack");
    write_text(pack, "Clubs.txt",
               "id\tshortname\thomekitshirtcol1\tstrength\r\n"
               "915\tHarbour Town\t#00F\t\r\n"
               "999\tNew Club\t\t40\r\n");
    write_text(pack, "Competitions.txt", "id\ttla\r\n2270\tHarbour League\r\n");
    write_text(pack, "PromotionPlaces.txt",
               "parentid\tplace\tpromotiontoid\r\n2280\t1\t2270\r\n2280\t2\t2270\r\n");
    SSRWorld *world = ssr_world_load_pack(set, pack);
    CHECK(world != NULL);
    if (!world) return;
    const SSRClub *west = ssr_world_club(world, 915);
    CHECK(world->clubCount == 4 && west && strcmp(west->name, "Harbour Town") == 0);
    CHECK(west && west->home.shirt1 == 0x0000FF && west->home.shorts == 0xFF0000);
    CHECK(west && west->strength == 98 && strcmp(west->tla, "WEST") == 0 && west->league == 2270);
    const SSRClub *added = ssr_world_club(world, 999);
    CHECK(added && added->strength == 40 && added->home.shirt1 == 0xFFFFFF && strcmp(added->tla, "") == 0);
    const SSRCompetition *league = ssr_world_competition(world, 2270);
    CHECK(league && strcmp(league->tla, "Harbour League") == 0 && league->duration == 38);
    CHECK(world->promotionCount == 2 && world->promotions[1].place == 2);
    ssr_world_free(world);
}

// A pack gives the stadiums of the clubs. A club with no row has none.
static void test_reads_the_stadiums_of_a_pack(void) {
    char set[1024];
    snprintf(set, sizeof(set), "%s", write_set("stadiumbase"));
    const char *pack = ssr_test_directory("stadiumpack");
    write_text(pack, "Stadiums.txt", "club\tname\tcapacity\r\n915\tHarbour Park\t41000\r\n1\tNowhere\t5\r\n");
    SSRWorld *world = ssr_world_load_pack(set, pack);
    CHECK(world != NULL);
    if (!world) return;
    const SSRClub *west = ssr_world_club(world, 915), *north = ssr_world_club(world, 810);
    CHECK(strcmp(west->stadium, "Harbour Park") == 0 && west->capacity == 41000);
    CHECK(strcmp(north->stadium, "") == 0 && north->capacity == 0);
    ssr_world_free(world);
}

// The editor renames a club, and the clubs go to a file that the world
// reads again.
static void test_edits_and_saves_the_clubs(void) {
    char set[1024];
    snprintf(set, sizeof(set), "%s", write_set("editbase"));
    SSRWorld *world = ssr_world_load(set);
    CHECK(world && ssr_world_set_club_name(world, 915, "Harbour Town", "HARBOR") && !ssr_world_set_club_name(world, 1, "X", NULL));
    CHECK(strcmp(ssr_world_club(world, 915)->name, "Harbour Town") == 0);
    const char *pack = ssr_test_directory("editpack");
    char path[1200];
    snprintf(path, sizeof(path), "%s/Clubs.txt", pack);
    CHECK(ssr_world_save_clubs(world, path));
    SSRWorld *back = ssr_world_load_pack(set, pack);
    CHECK(back && back->clubCount == 3);
    const SSRClub *a = ssr_world_club(world, 915), *b = back ? ssr_world_club(back, 915) : NULL;
    CHECK(b && strcmp(b->name, "Harbour Town") == 0 && strcmp(b->tla, "HARBOR") == 0);
    CHECK(b && b->strength == a->strength && b->away.shirt1 == a->away.shirt1 && b->bTeamOf == a->bTeamOf);
    CHECK(b && fabs(b->latitude - a->latitude) < 1e-6 && ssr_world_club(back, 916)->bTeamOf == 915);
    ssr_world_free(back);
    ssr_world_free(world);
}

static void test_parses_colours(void) {
    CHECK(ssr_color_parse("#FFF", 1) == 0xFFFFFF);
    CHECK(ssr_color_parse("#f80", 1) == 0xFF8800);
    CHECK(ssr_color_parse("#00BFFF", 1) == 0x00BFFF);
    CHECK(ssr_color_parse("FFF", 1) == 1);
    CHECK(ssr_color_parse("#GGG", 1) == 1);
    CHECK(ssr_color_parse("#FFFF", 1) == 1);
}

// The rows of the translations go to the nations and the continents in
// their order. A language with no column gives the names of the data set.
static void test_names_the_nations_in_a_language(void) {
    SSRWorld *world = ssr_world_load(write_set("world_language"));
    CHECK(world != NULL);
    if (!world) return;
    const char *nations = "\xEF\xBB\xBFid\tFull name (source)\tname\tshortname\tnationality\tde\tde_short\t"
                          "de_nationality\tru\tru_short\tru_nationality\r\n"
                          "62\tEngland\tEngland\tENGLAND\tEnglish\tEngland\tENGLAND\tEnglisch\t"
                          "\xD0\x90\xD0\xBD\xD0\xB3\xD0\xBB\xD0\xB8\xD1\x8F\t"
                          "\xD0\xB0\xD0\xBD\xD0\xB3\xD0\xBB\xD0\xB8\xD1\x8F\xD0\xB0\t\xD0\xB0\r\n";
    const char *continents = "\xEF\xBB\xBFid\ten\tde\r\n6\tEurope\tEuropa-Kontinent\r\n";
    CHECK(ssr_world_set_language(world, nations, continents, "de") == 2);
    CHECK(strcmp(world->nations[0].nationality, "Englisch") == 0);
    CHECK(strcmp(world->continents[0].name, "Europa-Kontinent") == 0);
    CHECK(strcmp(world->continents[0].tla, "EUROPA") == 0);
    // A tla of seven Cyrillic letters keeps six, in capitals.
    CHECK(ssr_world_set_language(world, nations, continents, "ru") == 1);
    CHECK(strcmp(world->nations[0].tla, "\xD0\x90\xD0\x9D\xD0\x93\xD0\x9B\xD0\x98\xD0\xAF") == 0);
    CHECK(strcmp(world->continents[0].name, "Europe") == 0);
    CHECK(strcmp(world->continents[0].tla, "EUR") == 0);
    // The continents have an "en" column. The nations have none.
    CHECK(ssr_world_set_language(world, nations, continents, "en") == 1);
    CHECK(strcmp(world->nations[0].name, "England") == 0 && strcmp(world->nations[0].nationality, "English") == 0);
    CHECK(strcmp(world->nations[0].tla, "ENGLAND") == 0 && strcmp(world->continents[0].tla, "EUROPE") == 0);
    CHECK(ssr_world_set_language(world, NULL, NULL, "de") == 0);
    CHECK(strcmp(world->nations[0].nationality, "English") == 0);
    ssr_world_free(world);
}

int main(void) {
    test_names_the_nations_in_a_language();
    test_loads_a_data_set();
    test_refuses_a_set_with_a_missing_file();
    test_parses_colours();
    test_reads_a_pack_over_the_data_set();
    test_merges_the_rows_of_a_pack();
    test_reads_the_stadiums_of_a_pack();
    test_edits_and_saves_the_clubs();
    return TEST_RESULT();
}
