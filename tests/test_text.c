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

static void test_puts_the_names_of_the_remake(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/brand.txt", ssr_test_directory("text"));
    FILE *file = fopen(path, "wb");
    fputs("Tag\ten\ten_female\n"
          "help_newplayer\tWelcome to New Star Soccer.\tWelcome to New Star Soccer!\n"
          "Title_NS-Pure\tNS-Pure\tNS-Pure\n"
          "NS-Pure\t[S2] NS-Pure\t\n"
          "PRO_COLOUR\tThe NS-Pro is balanced. TRANS-FER and BONUS-X stay.\t\n"
          "Credits_NS_Title\tNew Star Soccer\tNew Star Soccer\n"
          "CMESSAGE_CREDITS\tNew Star Soccer was created by Simon Read.\t\n"
          "GWL_TITLE_NSC\tNew Star Cricket\t\n"
          "CNEWS_PLAIN\tNothing to change.\t\n",
          file);
    fclose(file);
    SSRText *text = ssr_text_load(path);
    CHECK(ssr_text_rebrand(text) == 4);
    CHECK(strcmp(ssr_text_get(text, "help_newplayer", 0), "Welcome to Star Soccer Rebooted.") == 0);
    CHECK(strcmp(ssr_text_get(text, "help_newplayer", 1), "Welcome to Star Soccer Rebooted!") == 0);
    CHECK(strcmp(ssr_text_get(text, "Title_NS-Pure", 1), "SSR-Pure") == 0);
    CHECK(strcmp(ssr_text_get(text, "NS-Pure", 0), "[S2] SSR-Pure") == 0);
    CHECK(strcmp(ssr_text_get(text, "PRO_COLOUR", 0), "The SSR-Pro is balanced. TRANS-FER and BONUS-X stay.") == 0);
    // The credits of the original and the other games keep their names.
    CHECK(strcmp(ssr_text_get(text, "Credits_NS_Title", 0), "New Star Soccer") == 0);
    CHECK(strcmp(ssr_text_get(text, "CMESSAGE_CREDITS", 0), "New Star Soccer was created by Simon Read.") == 0);
    CHECK(strcmp(ssr_text_get(text, "GWL_TITLE_NSC", 0), "New Star Cricket") == 0);
    CHECK(strcmp(ssr_text_get(text, "CNEWS_PLAIN", 0), "Nothing to change.") == 0);
    // A second pass finds nothing more.
    CHECK(ssr_text_rebrand(text) == 0);
    ssr_text_free(text);
}

int main(void) {
    test_gets_the_text_of_a_tag();
    test_puts_the_names_of_the_remake();
    test_formats_the_values();
    return TEST_RESULT();
}
