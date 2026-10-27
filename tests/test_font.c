// Tests ssr_font. The UTF-8 tests need nothing. The font tests need a TTF
// font of the system, so that they do not hold on to the fonts of the
// repository. When there is no such font, the test reports that it skipped
// them.

#include <math.h>
#include <stdio.h>
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

static void test_reads_a_font_set(void) {
    const char *text = "// WARNING\n"
                       "FONTSET new 2\n"
                       "\t0:Small.ttf:14:-1,0:6,5,32,29,-26:1.75:0\n"
                       "ENDSET\n"
                       "#ANDROID\r\n"
                       "FONTSET other 1\r\n"
                       "\t0:Other.ttf:99:0,0:1,1,1,1,1\r\n"
                       "ENDSET\r\n"
                       "FONTSET new 2\r\n"
                       "\t0:Veriox-Rg.ttf:31:-1,0:6,5,32,28,-26\r\n"
                       "\t\tInject:9733:newstarbuck32.png:0:28:30:255\r\n"
                       "\t1:Veriox-Rg.ttf:44:-2,0:6,5,45,40,-40\r\n"
                       "ENDSET\r\n"
                       "FONTSET default 1\r\n"
                       "\t0:TCB.ttf:33:-1,0:16,15,32,28,-26\r\n"
                       "ENDSET\r\n";
    SSRFontSlot slots[4];
    CHECK(ssr_font_read_set(text, "new", slots, 4) == 2);
    CHECK(strcmp(slots[0].file, "Veriox-Rg.ttf") == 0 && slots[0].size == 31 && slots[0].spacing == -1);
    CHECK(slots[1].size == 44 && slots[1].spacing == -2 && slots[1].padLeft == 6 && slots[1].padRight == 5);
    CHECK(slots[1].padTop == 45 && slots[1].padMiddle == 40 && slots[1].padBottom == -40);
    // The Inject: line gives the star of the bux to the slot above it.
    CHECK(slots[0].iconCount == 1 && slots[1].iconCount == 0 && slots[0].icons[0].code == 9733);
    CHECK(strcmp(slots[0].icons[0].file, "newstarbuck32.png") == 0 && slots[0].icons[0].bearingY == 28);
    CHECK(slots[0].icons[0].advance == 30 && slots[0].icons[0].flags == 255);
    // A set that is not there gives the default set.
    CHECK(ssr_font_read_set(text, "classic", slots, 4) == 1 && strcmp(slots[0].file, "TCB.ttf") == 0);
    // A file with no "#ANDROID" uses all of its text.
    CHECK(ssr_font_read_set("FONTSET new 1\n0:A.ttf:14:-1,0:6,5,32,29,-26\nENDSET\n", "new", slots, 4) == 1);
    CHECK(slots[0].size == 14 && slots[0].padTop == 32);
    // A slot out of the range is not read.
    CHECK(ssr_font_read_set("FONTSET new 1\n9:A.ttf:14:0,0\nENDSET\n", "new", slots, 4) == 0);
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

// The star of the bux takes the place of its character: a quad of the size
// of the image, above the baseline, and the advance of the Inject: line.
static void test_lays_out_an_icon(const char *path) {
    SSRFont *font = ssr_font_load(path, 32);
    CHECK(font != NULL);
    if (!font) return;
    CHECK(ssr_font_add_icon(font, 9733, 32, 32, 0, 28, 30) == 1);
    SSRGlyphQuad quads[8];
    float star = ssr_font_measure(font, "\xE2\x98\x85");
    CHECK(star == 30 + 0);
    float plain = ssr_font_measure(font, "9");
    CHECK(ssr_font_measure(font, "\xE2\x98\x85" "9") == 30 + plain);
    CHECK(ssr_font_layout(font, "\xE2\x98\x85" "9", 10, 0, quads, 8) == 2);
    float baseline = floorf(ssr_font_ascent(font) + 0.5f);
    CHECK(quads[0].icon == 1 && quads[0].x0 == 10 && quads[0].y0 == baseline - 28);
    CHECK(quads[0].x1 - quads[0].x0 == 32 && quads[0].u1 == 1 && quads[1].icon == 0 && quads[1].x0 >= 40);
    ssr_font_free(font);
}

static void test_lays_out_text(const char *path) {
    SSRFont *font = ssr_font_load(path, 32);
    CHECK(font != NULL);
    if (!font) return;
    // The em is 32 pixels, so the box of the font is taller, and a whole
    // number of pixels.
    float line = ssr_font_line_height(font);
    CHECK(line > 32 && line < 64 && line == floorf(line));
    CHECK(ssr_font_ascent(font) > 20 && ssr_font_ascent(font) < line);

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

    // The advances are whole pixels.
    CHECK(one == floorf(one));
    // The spacing comes after each character.
    ssr_font_set_spacing(font, -1);
    CHECK(ssr_font_measure(font, "HH") == 2 * one - 2);
    ssr_font_set_spacing(font, 0);

    // The kerning of the game: the pair "HH" moves by -0.5 of the width of
    // the ink of the first H. A later pair takes the place of an earlier.
    const char *metrics = "test_font.metrics";
    FILE *file = fopen(metrics, "wb");
    CHECK(file != NULL);
    if (file) {
        int32_t ints[] = {2, 0, 0, 0};
        fwrite(ints, 4, 4, file);
        int32_t pair[] = {'H', 'H'};
        float factors[] = {0.25f, -0.5f};
        for (int i = 0; i < 2; i++) {
            fwrite(pair, 4, 2, file);
            fwrite(&factors[i], 4, 1, file);
        }
        fclose(file);
    }
    CHECK(ssr_font_load_kerning(font, metrics));
    int inkH = (int)(quads[0].x1 - quads[0].x0);
    CHECK(ssr_font_measure(font, "HH") == 2 * one + (float)(int)(inkH * -0.5f));
    CHECK(ssr_font_measure(font, "Hi") == ssr_font_measure(font, "H") + ssr_font_measure(font, "i"));
    remove(metrics);
    CHECK(!ssr_font_load_kerning(font, "/nonexistent/font.metrics"));

    // Only the quads that fit go into the array.
    CHECK(ssr_font_layout(font, "HHHH", 0, 0, quads, 2) == 4);
    ssr_font_free(font);
}

static void put_int(unsigned char *p, int value) {
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void put_entry(unsigned char *p, int code, int w, int h, int bearingX, int bearingY, int advance) {
    put_int(p, code);
    int values[] = {w, h, bearingX, bearingY, advance};
    for (int i = 0; i < 5; i++) {
        p[4 + 2 * i] = (unsigned char)(values[i] >> 8);
        p[5 + 2 * i] = (unsigned char)values[i];
    }
}

// A glyph table of two blocks, as GlyphMetricsLibrary writes it. Size 0
// has "H" and "i". Size 1 has "H" only.
static size_t make_table(unsigned char *data) {
    put_int(data, 2);
    int start0 = 4 + 2 * 16, start1 = start0 + 2 * 14;
    int blocks[2][4] = {{0, 0, start0, start1}, {0, 1, start1, start1 + 14}};
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < 4; i++) put_int(data + 4 + 16 * b + 4 * i, blocks[b][i]);
    }
    put_entry(data + start0, 'H', 12, 20, 3, 20, 40);
    put_entry(data + start0 + 14, 'i', 4, 20, -2, 21, 9);
    put_entry(data + start1, 'H', 12, 20, 1, 20, 99);
    return (size_t)(start1 + 14);
}

// A character of the table takes its advance and the place of its ink
// from the table. Another character keeps the metrics of the font.
static void test_uses_a_glyph_table(const char *path) {
    SSRFont *font = ssr_font_load(path, 32);
    CHECK(font != NULL);
    if (!font) return;
    float x = ssr_font_measure(font, "x");
    unsigned char data[256];
    size_t size = make_table(data);
    CHECK(ssr_font_set_glyph_table(font, data, size, 0, 0) == 2);
    CHECK(ssr_font_measure(font, "H") == 40);
    CHECK(ssr_font_measure(font, "Hix") == 40 + 9 + x);
    SSRGlyphQuad plain[4], quads[4];
    CHECK(ssr_font_set_glyph_table(font, NULL, 0, 0, 0) == 0);
    CHECK(ssr_font_layout(font, "Hi", 10, 0, plain, 4) == 2);
    CHECK(ssr_font_set_glyph_table(font, data, size, 0, 0) == 2);
    CHECK(ssr_font_layout(font, "Hi", 10, 0, quads, 4) == 2);
    // The left of the ink comes from the table, the top from the outline.
    CHECK(quads[0].x0 == 10 + 3 && quads[0].y0 == plain[0].y0 && quads[0].y1 == plain[0].y1);
    CHECK(quads[1].x0 == 10 + 40 - 2 && quads[1].y0 == plain[1].y0);
    CHECK(ssr_font_set_glyph_table(font, data, size, 0, 1) == 1);
    CHECK(ssr_font_measure(font, "H") == 99);
    // No table, or a table with no block for the size.
    CHECK(ssr_font_set_glyph_table(font, data, size, 1, 0) == 0);
    float own = ssr_font_measure(font, "H");
    CHECK(own != 99 && own != 40);
    CHECK(ssr_font_set_glyph_table(font, data, size, 0, 0) == 2);
    CHECK(ssr_font_set_glyph_table(font, NULL, 0, 0, 0) == 0);
    CHECK(ssr_font_measure(font, "H") == own);
    // A round letter that the table puts on the baseline loses the faint
    // row under the baseline, as the hinting of Android does.
    CHECK(ssr_font_set_glyph_table(font, NULL, 0, 0, 0) == 0);
    SSRGlyphQuad o;
    CHECK(ssr_font_layout(font, "o", 0, 0, &o, 1) == 1);
    float base = floorf(ssr_font_ascent(font) + 0.5f);
    if (o.y1 - base == 1) {
        unsigned char round[64];
        put_int(round, 1);
        int blocks[4] = {0, 0, 20, 34};
        for (int i = 0; i < 4; i++) put_int(round + 4 + 4 * i, blocks[i]);
        put_entry(round + 20, 'o', 14, 17, 1, 17, 18);
        CHECK(ssr_font_set_glyph_table(font, round, 34, 0, 0) == 1);
        SSRGlyphQuad snapped;
        CHECK(ssr_font_layout(font, "o", 0, 0, &snapped, 1) == 1);
        CHECK(snapped.y1 - base == 0 && snapped.y0 == o.y0);
        // A table that puts the bottom one pixel under keeps the row.
        put_entry(round + 20, 'o', 14, 18, 1, 17, 18);
        CHECK(ssr_font_set_glyph_table(font, round, 34, 0, 0) == 1);
        CHECK(ssr_font_layout(font, "o", 0, 0, &snapped, 1) == 1);
        CHECK(snapped.y1 - base == 1);
    } else {
        fprintf(stderr, "skipped the check of the baseline: the font puts \"o\" on it\n");
    }
    // A count of blocks that the data cannot hold.
    put_int(data, 1000);
    CHECK(ssr_font_set_glyph_table(font, data, size, 0, 0) == 0);
    ssr_font_free(font);
}

// A font with CJK characters, for the fallback test, or NULL.
static const char *cjk_font(void) {
    static const char *paths[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/AquaKana.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *file = fopen(paths[i], "rb");
        if (file) {
            fclose(file);
            return paths[i];
        }
    }
    return NULL;
}

// A character that the font does not have comes from the fallback font.
static void test_draws_with_a_fallback_font(const char *path) {
    const char *cjk = cjk_font();
    if (!cjk) {
        fprintf(stderr, "skipped the fallback test: no CJK font\n");
        return;
    }
    SSRFont *font = ssr_font_load(path, 32);
    CHECK(font != NULL);
    if (!font) return;
    const char *kana = "\xE3\x81\x82";
    SSRGlyphQuad quads[4];
    CHECK(ssr_font_layout(font, kana, 0, 0, quads, 4) == 0);
    CHECK(!ssr_font_add_fallback(font, "not a font file", 15));
    FILE *file = fopen(cjk, "rb");
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)size);
    CHECK(data && fread(data, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    CHECK(ssr_font_add_fallback(font, data, (size_t)size));
    free(data);
    CHECK(ssr_font_layout(font, kana, 0, 0, quads, 4) == 1);
    CHECK(quads[0].x1 - quads[0].x0 > 10 && ssr_font_measure(font, kana) > 20);
    // The font keeps its own characters.
    float h = ssr_font_measure(font, "H");
    ssr_font_clear_fallbacks(font);
    CHECK(ssr_font_measure(font, "H") == h);
    CHECK(ssr_font_layout(font, kana, 0, 0, quads, 4) == 0);
    ssr_font_free(font);
}

static void test_refuses_a_file_that_is_not_a_font(void) {
    CHECK(ssr_font_load("/nonexistent/font.ttf", 20) == NULL);
}

int main(void) {
    test_reads_utf8();
    test_reads_a_font_set();
    test_refuses_a_file_that_is_not_a_font();
    const char *path = system_font();
    if (!path) {
        fprintf(stderr, "skipped the font layout tests: no system font\n");
        return TEST_RESULT() ? 1 : SKIPPED;
    }
    test_lays_out_text(path);
    test_lays_out_an_icon(path);
    test_uses_a_glyph_table(path);
    test_draws_with_a_fallback_font(path);
    return TEST_RESULT();
}
