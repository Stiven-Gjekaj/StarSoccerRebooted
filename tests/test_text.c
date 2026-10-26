// Tests ssr_text with a language file that the test writes.

#include <string.h>

#include "ssr_test.h"
#include "ssr_text.h"
#include "ssr_zipwriter.h"

static const char *write_language(void) {
    static char path[1200];
    snprintf(path, sizeof(path), "%s/en.txt", ssr_test_directory("text"));
    FILE *file = fopen(path, "wb");
    fputs("\xEF\xBB\xBFTag\ten\ten_female\r\n"
          "Tag_Language\tEnglish\tEnglish\r\n"
          "CMESSAGE_HELLO\tHe scores for $teamname.\tShe scores for $teamname.\r\n"
          "CNEWS_ONE\tOnly one text.\r\n"
          "\tA row with no tag.\t\r\n",
          file);
    fclose(file);
    return path;
}

static void test_gets_the_text_of_a_tag(void) {
    SSRText *text = ssr_text_load(write_language());
    CHECK(text != NULL);
    CHECK(ssr_text_count(text) == 3);
    CHECK(strcmp(ssr_text_get(text, "Tag_Language", 0), "English") == 0);
    CHECK(strcmp(ssr_text_get(text, "CMESSAGE_HELLO", 0), "He scores for $teamname.") == 0);
    CHECK(strcmp(ssr_text_get(text, "CMESSAGE_HELLO", 1), "She scores for $teamname.") == 0);
    CHECK(strcmp(ssr_text_get(text, "CNEWS_ONE", 1), "Only one text.") == 0);
    CHECK(strcmp(ssr_text_get(text, "MISSING_TAG", 0), "MISSING_TAG") == 0);
    ssr_text_free(text);
}

static void test_formats_the_values(void) {
    const char *names[] = {"team", "teamname", "amount"};
    const char *values[] = {"WRONG", "Arsenal", "$500"};
    char out[64];
    size_t length = ssr_text_format(out, sizeof(out), "$teamname pays $amount.", names, values, 3);
    CHECK(strcmp(out, "Arsenal pays $500.") == 0);
    CHECK(length == strlen("Arsenal pays $500."));

    CHECK(ssr_text_format(out, sizeof(out), "Costs $5 at $unknown", names, values, 3) == 20);
    CHECK(strcmp(out, "Costs $5 at $unknown") == 0);

    char small[8];
    length = ssr_text_format(small, sizeof(small), "$teamname wins", names, values, 3);
    CHECK(length == strlen("Arsenal wins"));
    CHECK(strcmp(small, "Arsenal") == 0);
}

int main(void) {
    test_gets_the_text_of_a_tag();
    test_formats_the_values();
    return TEST_RESULT();
}
