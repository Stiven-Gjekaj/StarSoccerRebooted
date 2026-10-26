// Tests ssr_tsv with text that the test builds.

#include <string.h>

#include "ssr_test.h"
#include "ssr_tsv.h"

static void test_reads_a_file_with_a_bom_and_crlf(void) {
    const char text[] = "\xEF\xBB\xBFid\tshortname\tstrength\r\n"
                        "1\tHakim Sanayi\t14\r\n"
                        "\r\n"
                        "2\tJavan Azadi\t\r\n";
    SSRTsv *table = ssr_tsv_parse(text, sizeof(text) - 1);
    CHECK(table != NULL);
    CHECK(ssr_tsv_rows(table) == 2);
    CHECK(ssr_tsv_column(table, "id") == 0);
    CHECK(ssr_tsv_column(table, "ShortName") == 1);
    CHECK(ssr_tsv_column(table, "missing") == -1);
    CHECK(strcmp(ssr_tsv_cell(table, 0, 1), "Hakim Sanayi") == 0);
    CHECK(ssr_tsv_int(table, 0, 2, -1) == 14);
    CHECK(strcmp(ssr_tsv_cell(table, 1, 1), "Javan Azadi") == 0);
    CHECK(ssr_tsv_int(table, 1, 2, -1) == -1);
    ssr_tsv_free(table);
}

static void test_a_short_row_gives_empty_cells(void) {
    const char text[] = "a\tb\tc\nx\n";
    SSRTsv *table = ssr_tsv_parse(text, sizeof(text) - 1);
    CHECK(ssr_tsv_rows(table) == 1);
    CHECK(strcmp(ssr_tsv_cell(table, 0, 0), "x") == 0);
    CHECK(strcmp(ssr_tsv_cell(table, 0, 2), "") == 0);
    CHECK(strcmp(ssr_tsv_cell(table, 5, 0), "") == 0);
    ssr_tsv_free(table);
}

static void test_reads_numbers(void) {
    const char text[] = "lon\tlat\tbad\n69.1900024\t-0.17\t12abc";
    SSRTsv *table = ssr_tsv_parse(text, sizeof(text) - 1);
    CHECK(ssr_tsv_rows(table) == 1);
    CHECK(ssr_tsv_double(table, 0, 0, 0) > 69.18 && ssr_tsv_double(table, 0, 0, 0) < 69.20);
    CHECK(ssr_tsv_double(table, 0, 1, 0) < -0.16);
    CHECK(ssr_tsv_int(table, 0, 2, 7) == 7);
    ssr_tsv_free(table);
}

static void test_refuses_empty_text(void) {
    CHECK(ssr_tsv_parse("", 0) == NULL);
    CHECK(ssr_tsv_parse("\r\n\r\n", 4) == NULL);
}

int main(void) {
    test_reads_a_file_with_a_bom_and_crlf();
    test_a_short_row_gives_empty_cells();
    test_reads_numbers();
    test_refuses_empty_text();
    return TEST_RESULT();
}
