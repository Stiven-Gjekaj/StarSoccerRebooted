// Tests ssr_font. The UTF-8 tests need nothing. The font tests need a TTF
// font of the system, because the repository holds no font. When there is
// no such font, the test reports that it skipped them.

#include <stdlib.h>
#include <string.h>

#include "ssr_font.h"
#include "ssr_test.h"

#define SKIPPED 77

static void test_reads_utf8(void) {
    const char *text = "A\xC3\xA9\xE2\x98\x85\xF0\x9F\x98\x80";
    CHECK(ssr_utf8_next(&text) == 'A');
    CHECK(ssr_utf8_next(&text) == 0xE9);
    CHECK(ssr_utf8_next(&text) == 0x2605);
    CHECK(ssr_utf8_next(&text) == 0x1F600);
    CHECK(ssr_utf8_next(&text) == 0);

    const char *bad = "\xC3x\xC0\x80\xFF";
    CHECK(ssr_utf8_next(&bad) == 0xFFFD);
    CHECK(ssr_utf8_next(&bad) == 'x');
    CHECK(ssr_utf8_next(&bad) == 0xFFFD);
    CHECK(ssr_utf8_next(&bad) == 0xFFFD);
    CHECK(ssr_utf8_next(&bad) == 0);
}

static const char *system_font(void) {
    static const char *paths[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    const char *env = getenv("SSR_TEST_FONT");
    if (env && *env) return env;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *file = fopen(paths[i], "rb");
        if (file) {
            fclose(file);
            return paths[i];
        }
    }
    return NULL;
}

static void test_lays_out_text(const char *path) {
    SSRFont *font = ssr_font_load(path, 32);
    CHECK(font != NULL);
    if (!font) return;
    CHECK(ssr_font_line_height(font) >= 32);
    CHECK(ssr_font_ascent(font) > 16 && ssr_font_ascent(font) < 32);

    int before;
    ssr_font_atlas(font, &before);
    SSRGlyphQuad quads[8];
    CHECK(ssr_font_layout(font, "Hi H", 10, 20, quads, 8) == 3);
    int after;
    const unsigned char *atlas = ssr_font_atlas(font, &after);
    CHECK(after == before + 2);

    // The two H glyphs share one place in the atlas.
    CHECK(quads[0].u0 == quads[2].u0 && quads[0].v0 == quads[2].v0);
    CHECK(quads[0].x0 >= 10 && quads[1].x0 > quads[0].x1 - 1 && quads[2].x0 > quads[1].x1);
    CHECK(quads[0].y1 <= 20 + ssr_font_ascent(font) + 1);

    // The H has ink in the atlas.
    int ink = 0;
    int ax = (int)(quads[0].u0 * SSR_FONT_ATLAS_SIZE), ay = (int)(quads[0].v0 * SSR_FONT_ATLAS_SIZE);
    for (int y = ay; y < ay + (int)(quads[0].y1 - quads[0].y0); y++) {
        for (int x = ax; x < ax + (int)(quads[0].x1 - quads[0].x0); x++) ink += atlas[y * SSR_FONT_ATLAS_SIZE + x] > 128;
    }
    CHECK(ink > 20);

    // A second line starts again at x, one line lower.
    CHECK(ssr_font_layout(font, "H\nH", 10, 20, quads, 8) == 2);
    CHECK(quads[1].x0 == quads[0].x0);
    CHECK(quads[1].y0 - quads[0].y0 > ssr_font_line_height(font) - 1);

    float one = ssr_font_measure(font, "H");
    CHECK(one > 10);
    CHECK(ssr_font_measure(font, "HH") > one * 1.9f);
    CHECK(ssr_font_measure(font, "HH\nH") == ssr_font_measure(font, "HH"));

    // Only the quads that fit go into the array.
    CHECK(ssr_font_layout(font, "HHHH", 0, 0, quads, 2) == 4);
    ssr_font_free(font);
}

static void test_refuses_a_file_that_is_not_a_font(void) {
    CHECK(ssr_font_load("/nonexistent/font.ttf", 20) == NULL);
}

int main(void) {
    test_reads_utf8();
    test_refuses_a_file_that_is_not_a_font();
    const char *path = system_font();
    if (!path) {
        fprintf(stderr, "skipped the font layout tests: no system font\n");
        return TEST_RESULT() ? 1 : SKIPPED;
    }
    test_lays_out_text(path);
    return TEST_RESULT();
}
