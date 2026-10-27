// Tests ssr_pack with archives that the test writes.

#include <stdio.h>
#include <string.h>

#include "ssr_pack.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static const char INFO[] = "id=harbour\nname=Harbour League\nauthor=A. Person\nversion=1.0\nformat=1\n";
static const char CLUBS[] = "id\tshortname\n810\tHarbour Town\n";

static int exists(const char *directory, const char *name) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "rb");
    if (file) fclose(file);
    return file != NULL;
}

static void test_reads_the_info(void) {
    SSRPackInfo info;
    CHECK(ssr_pack_parse(INFO, &info));
    CHECK(strcmp(info.id, "harbour") == 0 && strcmp(info.name, "Harbour League") == 0);
    CHECK(strcmp(info.author, "A. Person") == 0 && strcmp(info.version, "1.0") == 0 && info.format == 1);
    // An id with capitals or a slash, no name, or a newer format: no pack.
    CHECK(!ssr_pack_parse("id=Harbour\nname=H\nformat=1\n", &info));
    CHECK(!ssr_pack_parse("id=../x\nname=H\nformat=1\n", &info));
    CHECK(!ssr_pack_parse("id=harbour\nformat=1\n", &info));
    CHECK(!ssr_pack_parse("id=harbour\nname=H\nformat=2\n", &info));
    CHECK(ssr_pack_parse("id=harbour\r\nname=H\r\nformat=1\r\n", &info) && strcmp(info.name, "H") == 0);
}

// A pack in a folder of the archive installs, and a new version takes the
// place of the old one, with no old files left.
static void test_installs_and_replaces_a_pack(void) {
    char root[1024], archive[1200], packs[1200];
    snprintf(root, sizeof(root), "%s", ssr_test_directory("pack_install"));
    snprintf(archive, sizeof(archive), "%s/harbour.zip", root);
    snprintf(packs, sizeof(packs), "%s/packs", root);
    SSRZipFile files[] = {
        {"harbour/", "", 0, 0},
        {"harbour/pack.txt", INFO, sizeof(INFO) - 1, 0},
        {"harbour/world/Clubs.txt", CLUBS, sizeof(CLUBS) - 1, 1},
        {"harbour/art/Old.png", "png", 3, 0},
    };
    CHECK(ssr_zipwriter_write(archive, files, 4));
    SSRPackInfo info;
    CHECK(ssr_pack_install(archive, packs, &info) && strcmp(info.id, "harbour") == 0);
    char dir[1300];
    snprintf(dir, sizeof(dir), "%s/harbour", packs);
    CHECK(exists(dir, "pack.txt") && exists(dir, "world/Clubs.txt") && exists(dir, "art/Old.png"));
    // Version 2 has no Old.png.
    SSRZipFile next[] = {
        {"pack.txt", INFO, sizeof(INFO) - 1, 0},
        {"world/Clubs.txt", CLUBS, sizeof(CLUBS) - 1, 0},
    };
    CHECK(ssr_zipwriter_write(archive, next, 2));
    CHECK(ssr_pack_install(archive, packs, &info));
    CHECK(exists(dir, "world/Clubs.txt") && !exists(dir, "art/Old.png"));
    char ids[4][48];
    CHECK(ssr_pack_list(packs, ids, 4) == 1 && strcmp(ids[0], "harbour") == 0);
    CHECK(ssr_pack_remove(packs, "harbour") && ssr_pack_list(packs, ids, 4) == 0);
    CHECK(!ssr_pack_remove(packs, "../pack_install"));
}

// A name outside the pack or a file that is not data stops the install
// before it writes a file.
static void test_refuses_a_bad_pack(void) {
    char root[1024], archive[1200], packs[1200];
    snprintf(root, sizeof(root), "%s", ssr_test_directory("pack_bad"));
    snprintf(archive, sizeof(archive), "%s/bad.zip", root);
    snprintf(packs, sizeof(packs), "%s/packs", root);
    const char *bad[] = {"../escape.txt", "world/run.sh", "lib/code.so", ".hidden.txt"};
    for (int i = 0; i < 4; i++) {
        SSRZipFile files[] = {
            {"pack.txt", INFO, sizeof(INFO) - 1, 0},
            {"world/Clubs.txt", CLUBS, sizeof(CLUBS) - 1, 0},
            {bad[i], "x", 1, 0},
        };
        CHECK(ssr_zipwriter_write(archive, files, 3));
        CHECK(!ssr_pack_install(archive, packs, NULL));
        char dir[1300];
        snprintf(dir, sizeof(dir), "%s/harbour", packs);
        CHECK(!exists(dir, "world/Clubs.txt"));
    }
    // No pack.txt: no pack.
    SSRZipFile none[] = {{"world/Clubs.txt", CLUBS, sizeof(CLUBS) - 1, 0}};
    CHECK(ssr_zipwriter_write(archive, none, 1));
    CHECK(!ssr_pack_install(archive, packs, NULL));
}

// A pack of the editor: made once, with its folder of the world.
static void test_makes_a_pack(void) {
    char packs[1200];
    snprintf(packs, sizeof(packs), "%s/packs", ssr_test_directory("pack_make"));
    CHECK(ssr_pack_make(packs, "custom", "My edits") && ssr_pack_make(packs, "custom", "Other"));
    SSRPackInfo info;
    char dir[1300];
    snprintf(dir, sizeof(dir), "%s/custom", packs);
    CHECK(ssr_pack_info(dir, &info) && strcmp(info.name, "My edits") == 0);
    snprintf(dir, sizeof(dir), "%s/custom/world/x.txt", packs);
    FILE *file = fopen(dir, "wb");
    CHECK(file != NULL);
    if (file) fclose(file);
    CHECK(!ssr_pack_make(packs, "Bad Id", "X"));
}

int main(void) {
    test_makes_a_pack();
    test_reads_the_info();
    test_installs_and_replaces_a_pack();
    test_refuses_a_bad_pack();
    return TEST_RESULT();
}
