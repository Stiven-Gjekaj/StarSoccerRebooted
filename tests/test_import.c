// Tests ssr_import with archives that the test writes.

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ssr_import.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

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
    const char *directory = ssr_test_directory(name);
    snprintf(archive, 1100, "%s/copy.zip", directory);
    snprintf(dest, 1100, "%s/game", directory);
}

static void test_imports_the_assets_of_an_apk(void) {
    char archive[1100], dest[1100];
    make_paths("apk", archive, dest);
    SSRZipFile files[] = {
        {"AndroidManifest.xml", "manifest", 8, 0},
        {"classes.dex", "code", 4, 1},
        {"assets/monkey/", "", 0, 0},
        {"assets/monkey/MyData/Content.txt", "beach#\n", 7, 1},
        {"assets/monkey/Images/Backgrounds/Sponsored_SPONSOR.png", "png", 3, 0},
        {"assets/OpenSans-Regular.ttf", "font", 4, 0},
    };
    CHECK(ssr_zipwriter_write(archive, files, 6));

    int count = -1;
    CHECK(ssr_import(archive, dest, &count) == SSR_SOURCE_APK);
    CHECK(count == 2);
    char *text = read_text(dest, "MyData/Content.txt");
    CHECK(text && strcmp(text, "beach#\n") == 0);
    free(text);
    CHECK(exists(dest, "Images/Backgrounds/Sponsored_SPONSOR.png"));
    CHECK(!exists(dest, "classes.dex"));
    CHECK(!exists(dest, "OpenSans-Regular.ttf"));
    CHECK(ssr_import_is_done(dest));
    char *marker = read_text(dest, "import.txt");
    CHECK(marker && strcmp(marker, "source=apk\nfiles=2\n") == 0);
    free(marker);
}

static void test_imports_the_apk_in_an_xapk(void) {
    char archive[1100], dest[1100];
    make_paths("xapk", archive, dest);
    char apk[1100];
    snprintf(apk, sizeof(apk), "%s.inner", archive);
    SSRZipFile inner[] = {{"assets/monkey/MyData/Content.txt", "beach#\n", 7, 1}};
    CHECK(ssr_zipwriter_write(apk, inner, 1));
    FILE *file = fopen(apk, "rb");
    char apkData[4096];
    size_t apkSize = fread(apkData, 1, sizeof(apkData), file);
    fclose(file);

    SSRZipFile outer[] = {
        {"manifest.json", "{}", 2, 0},
        {"config.en.apk", "language", 8, 0},
        {"com.newstargames.newstarsoccer.apk", apkData, apkSize, 0},
    };
    CHECK(ssr_zipwriter_write(archive, outer, 3));

    int count = -1;
    CHECK(ssr_import(archive, dest, &count) == SSR_SOURCE_XAPK);
    CHECK(count == 1);
    char *text = read_text(dest, "MyData/Content.txt");
    CHECK(text && strcmp(text, "beach#\n") == 0);
    free(text);
    CHECK(!exists(dest, ".import.apk"));
    char *marker = read_text(dest, "import.txt");
    CHECK(marker && strcmp(marker, "source=xapk\nfiles=1\n") == 0);
    free(marker);
}

static void test_imports_the_assets_of_an_ipa(void) {
    char archive[1100], dest[1100];
    make_paths("ipa", archive, dest);
    SSRZipFile files[] = {
        {"Payload/soccer.app/Info.plist", "plist", 5, 0},
        {"Payload/soccer.app/soccer", "binary", 6, 1},
        {"Payload/soccer.app/data/MyData/Content.txt", "beach#\n", 7, 1},
        {"Payload/soccer.app/data/Sounds/kick.mp3", "mp3", 3, 0},
        {"Payload/soccer.app/Frameworks/Lib.framework/data/x.txt", "no", 2, 0},
    };
    CHECK(ssr_zipwriter_write(archive, files, 5));

    int count = -1;
    CHECK(ssr_import(archive, dest, &count) == SSR_SOURCE_IPA);
    CHECK(count == 2);
    char *text = read_text(dest, "MyData/Content.txt");
    CHECK(text && strcmp(text, "beach#\n") == 0);
    free(text);
    CHECK(exists(dest, "Sounds/kick.mp3"));
    CHECK(!exists(dest, "x.txt"));
    char *marker = read_text(dest, "import.txt");
    CHECK(marker && strcmp(marker, "source=ipa\nfiles=2\n") == 0);
    free(marker);
}

static void test_refuses_an_archive_with_no_assets(void) {
    char archive[1100], dest[1100];
    make_paths("noassets", archive, dest);
    SSRZipFile files[] = {{"Payload/Other.app/Info.plist", "plist", 5, 0}};
    CHECK(ssr_zipwriter_write(archive, files, 1));
    int count = -1;
    CHECK(ssr_import(archive, dest, &count) == SSR_SOURCE_UNKNOWN);
    CHECK(count == 0);
    CHECK(!ssr_import_is_done(dest));
}

static void test_refuses_a_name_that_leaves_the_directory(void) {
    char archive[1100], dest[1100];
    make_paths("unsafe", archive, dest);
    SSRZipFile files[] = {
        {"assets/monkey/MyData/Content.txt", "beach#\n", 7, 0},
        {"assets/monkey/../../escape.txt", "bad", 3, 0},
    };
    CHECK(ssr_zipwriter_write(archive, files, 2));
    CHECK(ssr_import(archive, dest, NULL) == SSR_SOURCE_UNKNOWN);
    CHECK(!ssr_import_is_done(dest));
    CHECK(!exists(dest, "../escape.txt"));
    CHECK(!exists(dest, "MyData/Content.txt"));
}

static void test_a_failed_import_removes_the_old_marker(void) {
    char archive[1100], dest[1100];
    make_paths("again", archive, dest);
    SSRZipFile good[] = {{"assets/monkey/MyData/Content.txt", "beach#\n", 7, 0}};
    CHECK(ssr_zipwriter_write(archive, good, 1));
    CHECK(ssr_import(archive, dest, NULL) == SSR_SOURCE_APK);
    CHECK(ssr_import_is_done(dest));

    SSRZipFile bad[] = {{"assets/monkey/../x.txt", "bad", 3, 0}};
    CHECK(ssr_zipwriter_write(archive, bad, 1));
    CHECK(ssr_import(archive, dest, NULL) == SSR_SOURCE_UNKNOWN);
    CHECK(!ssr_import_is_done(dest));
}

int main(void) {
    test_imports_the_assets_of_an_apk();
    test_imports_the_apk_in_an_xapk();
    test_imports_the_assets_of_an_ipa();
    test_refuses_an_archive_with_no_assets();
    test_refuses_a_name_that_leaves_the_directory();
    test_a_failed_import_removes_the_old_marker();
    return TEST_RESULT();
}
