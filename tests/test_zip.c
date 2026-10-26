// Tests nsr_zip with archives that the test writes.

#include <stdlib.h>
#include <string.h>

#include "nsr_test.h"
#include "nsr_zip.h"
#include "nsr_zipwriter.h"

static void test_reads_stored_and_deflated_entries(void) {
    const char *directory = nsr_test_directory("zip");
    char path[1100];
    snprintf(path, sizeof(path), "%s/game.apk", directory);

    char repeated[4000];
    memset(repeated, 'a', sizeof(repeated));
    NSRZipFile files[] = {
        {"assets/monkey/MyData/Content.txt", "beach#\nboss#\n", 13, 0},
        {"assets/monkey/Images/big.png", repeated, sizeof(repeated), 1},
    };
    CHECK(nsr_zipwriter_write(path, files, 2));

    NSRZip *zip = nsr_zip_open(path);
    CHECK(zip != NULL);
    CHECK(nsr_zip_count(zip) == 2);
    CHECK(strcmp(nsr_zip_name(zip, 1), "assets/monkey/Images/big.png") == 0);

    size_t size = 0;
    unsigned char *text = nsr_zip_read(zip, "assets/monkey/MyData/Content.txt", &size);
    CHECK(text != NULL && size == 13 && strcmp((char *)text, "beach#\nboss#\n") == 0);
    free(text);

    unsigned char *big = nsr_zip_read(zip, "assets/monkey/Images/big.png", &size);
    CHECK(big != NULL && size == sizeof(repeated) && memcmp(big, repeated, size) == 0);
    free(big);

    CHECK(nsr_zip_read(zip, "assets/monkey/missing.txt", &size) == NULL);
    nsr_zip_close(zip);
}

static void test_refuses_a_file_that_is_not_a_zip(void) {
    const char *directory = nsr_test_directory("notzip");
    char path[1100];
    snprintf(path, sizeof(path), "%s/notes.txt", directory);
    FILE *file = fopen(path, "wb");
    fputs("This is not an archive.", file);
    fclose(file);
    CHECK(nsr_zip_open(path) == NULL);
}

int main(void) {
    test_reads_stored_and_deflated_entries();
    test_refuses_a_file_that_is_not_a_zip();
    return TEST_RESULT();
}
