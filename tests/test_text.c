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

// The texts of the remake add new tags in the language, or in English.
static void test_adds_the_texts_of_the_remake(void) {
    SSRText *text = ssr_text_load(write_language());
    static const char OWN[] = "Tag\ten\tde\n"
                              "SSR_SQUAD\tSquad\tKader\n"
                              "SSR_RIVAL\tRival\t\n"
                              "Tag_Language\tNot this\tNicht\n";
    CHECK(ssr_text_add(text, OWN, sizeof(OWN) - 1, "de") == 2);
    CHECK(strcmp(ssr_text_get(text, "SSR_SQUAD", 0), "Kader") == 0);
    CHECK(strcmp(ssr_text_get(text, "SSR_RIVAL", 0), "Rival") == 0);
    CHECK(strcmp(ssr_text_get(text, "Tag_Language", 0), "English") == 0);
    CHECK(strcmp(ssr_text_get(text, "CMESSAGE_HELLO", 1), "She scores for $teamname.") == 0);
    // A language with no column takes English.
    CHECK(ssr_text_add(text, "Tag\ten\nSSR_NEW\tNew\n", 19, "fr") == 1);
    CHECK(strcmp(ssr_text_get(text, "SSR_NEW", 0), "New") == 0 && ssr_text_count(text) == 6);
    CHECK(ssr_text_add(text, "No\ttag\n", 7, "en") == 0);
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
          "TICKER_NEWPLAYER_1\tWelcome to the NSS Leagues! NSSX and XNSS stay.\t\n"
          "CNEWS_PLAIN\tNothing to change.\t\n",
          file);
    fclose(file);
    SSRText *text = ssr_text_load(path);
    CHECK(ssr_text_rebrand(text) == 5);
    CHECK(strcmp(ssr_text_get(text, "help_newplayer", 0), "Welcome to Star Soccer Rebooted.") == 0);
    CHECK(strcmp(ssr_text_get(text, "help_newplayer", 1), "Welcome to Star Soccer Rebooted!") == 0);
    CHECK(strcmp(ssr_text_get(text, "Title_NS-Pure", 1), "SSR-Pure") == 0);
    CHECK(strcmp(ssr_text_get(text, "NS-Pure", 0), "[S2] SSR-Pure") == 0);
    CHECK(strcmp(ssr_text_get(text, "PRO_COLOUR", 0), "The SSR-Pro is balanced. TRANS-FER and BONUS-X stay.") == 0);
    CHECK(strcmp(ssr_text_get(text, "TICKER_NEWPLAYER_1", 0), "Welcome to the SSR Leagues! NSSX and XNSS stay.") == 0);
    // The credits of the original and the other games keep their names.
    CHECK(strcmp(ssr_text_get(text, "Credits_NS_Title", 0), "New Star Soccer") == 0);
    CHECK(strcmp(ssr_text_get(text, "CMESSAGE_CREDITS", 0), "New Star Soccer was created by Simon Read.") == 0);
    CHECK(strcmp(ssr_text_get(text, "GWL_TITLE_NSC", 0), "New Star Cricket") == 0);
    CHECK(strcmp(ssr_text_get(text, "CNEWS_PLAIN", 0), "Nothing to change.") == 0);
    // A second pass finds nothing more.
    CHECK(ssr_text_rebrand(text) == 0);
    ssr_text_free(text);
}

// The table has the columns of DeviceLanguageCodes.txt.
static void test_finds_the_language_of_the_device(void) {
    const char *table = "Locale Code\tAndroid/WP Language Code\tiOS Language Code\tDistance\t\r\n"
                        "0\ten\ten\t0\tEnglish\r\n"
                        "2\tpt_BR\tpt_BR\t1\tPortugese Brasil\r\n"
                        "8\tpt_PT\tpt_PT\t1\tPortugese\r\n"
                        "12\tzh\tzh\t1\tChinese\r\n";
    int metres = -1;
    CHECK(ssr_text_device_language(table, "en", "US", &metres) == 0 && metres == 0);
    CHECK(ssr_text_device_language(table, "pt", "BR", &metres) == 2 && metres == 1);
    CHECK(ssr_text_device_language(table, "pt", "PT", NULL) == 8);
    CHECK(ssr_text_device_language(table, "pt", NULL, NULL) == -1);
    CHECK(ssr_text_device_language(table, "zh", "CN", NULL) == 12);
    CHECK(ssr_text_device_language(table, "nl", "NL", NULL) == -1);
    CHECK(strcmp(ssr_text_language_code(12), "ch") == 0);
    CHECK(strcmp(ssr_text_language_code(2), "br") == 0);
    CHECK(strcmp(ssr_text_language_code(13), "en") == 0);
}

int main(void) {
    test_finds_the_language_of_the_device();
    test_gets_the_text_of_a_tag();
    test_adds_the_texts_of_the_remake();
    test_puts_the_names_of_the_remake();
    test_formats_the_values();
    return TEST_RESULT();
}
