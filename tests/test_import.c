// Tests nsr_import with archives that the test writes.

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nsr_import.h"
#include "nsr_test.h"
#include "nsr_zipwriter.h"

static char *read_text(const char *dest, const char *name) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dest, name);
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    char *text = calloc(1, 4096);
    fread(text, 1, 4095, file);
    fclose(file);
    return text;
}

static int exists(const char *dest, const char *name) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dest, name);
    struct stat info;
    return stat(path, &info) == 0;
}

static void make_paths(const char *name, char *archive, char *dest) {
    const char *directory = nsr_test_directory(name);
    snprintf(archive, 1100, "%s/copy.zip", directory);
    snprintf(dest, 1100, "%s/game", directory);
}

static void test_imports_the_assets_of_an_apk(void) {
    char archive[1100], dest[1100];
    make_paths("apk", archive, dest);
    NSRZipFile files[] = {
        {"AndroidManifest.xml", "manifest", 8, 0},
        {"classes.dex", "code", 4, 1},
        {"assets/monkey/", "", 0, 0},
        {"assets/monkey/MyData/Content.txt", "beach#\n", 7, 1},
        {"assets/monkey/Images/Backgrounds/Sponsored_SPONSOR.png", "png", 3, 0},
        {"assets/OpenSans-Regular.ttf", "font", 4, 0},
    };
    CHECK(nsr_zipwriter_write(archive, files, 6));

    int count = -1;
    CHECK(nsr_import(archive, dest, &count) == NSR_SOURCE_APK);
    CHECK(count == 2);
    char *text = read_text(dest, "MyData/Content.txt");
    CHECK(text && strcmp(text, "beach#\n") == 0);
    free(text);
    CHECK(exists(dest, "Images/Backgrounds/Sponsored_SPONSOR.png"));
    CHECK(!exists(dest, "classes.dex"));
    CHECK(!exists(dest, "OpenSans-Regular.ttf"));
    CHECK(nsr_import_is_done(dest));
    char *marker = read_text(dest, "import.txt");
    CHECK(marker && strcmp(marker, "source=apk\nfiles=2\n") == 0);
    free(marker);
}

static void test_refuses_an_archive_with_no_assets(void) {
    char archive[1100], dest[1100];
    make_paths("noassets", archive, dest);
    NSRZipFile files[] = {{"Payload/Other.app/Info.plist", "plist", 5, 0}};
    CHECK(nsr_zipwriter_write(archive, files, 1));
    int count = -1;
    CHECK(nsr_import(archive, dest, &count) == NSR_SOURCE_UNKNOWN);
    CHECK(count == 0);
    CHECK(!nsr_import_is_done(dest));
}

static void test_refuses_a_name_that_leaves_the_directory(void) {
    char archive[1100], dest[1100];
    make_paths("unsafe", archive, dest);
    NSRZipFile files[] = {
        {"assets/monkey/MyData/Content.txt", "beach#\n", 7, 0},
        {"assets/monkey/../../escape.txt", "bad", 3, 0},
    };
    CHECK(nsr_zipwriter_write(archive, files, 2));
    CHECK(nsr_import(archive, dest, NULL) == NSR_SOURCE_UNKNOWN);
    CHECK(!nsr_import_is_done(dest));
    CHECK(!exists(dest, "../escape.txt"));
    CHECK(!exists(dest, "MyData/Content.txt"));
}

static void test_a_failed_import_removes_the_old_marker(void) {
    char archive[1100], dest[1100];
    make_paths("again", archive, dest);
    NSRZipFile good[] = {{"assets/monkey/MyData/Content.txt", "beach#\n", 7, 0}};
    CHECK(nsr_zipwriter_write(archive, good, 1));
    CHECK(nsr_import(archive, dest, NULL) == NSR_SOURCE_APK);
    CHECK(nsr_import_is_done(dest));

    NSRZipFile bad[] = {{"assets/monkey/../x.txt", "bad", 3, 0}};
    CHECK(nsr_zipwriter_write(archive, bad, 1));
    CHECK(nsr_import(archive, dest, NULL) == NSR_SOURCE_UNKNOWN);
    CHECK(!nsr_import_is_done(dest));
}

int main(void) {
    test_imports_the_assets_of_an_apk();
    test_refuses_an_archive_with_no_assets();
    test_refuses_a_name_that_leaves_the_directory();
    test_a_failed_import_removes_the_old_marker();
    return TEST_RESULT();
}
