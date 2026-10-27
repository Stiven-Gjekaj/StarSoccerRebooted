// ssr_font: draws the text of the game with a TTF font of the import, for
// example "Fonts/Veriox-Rg.ttf". The font puts each glyph that the text
// uses into an atlas the first time, and gives a quad for each glyph.

#ifndef SSR_FONT_H
#define SSR_FONT_H

#include <stddef.h>
#include <stdint.h>

// The size of the atlas, in pixels.
#define SSR_FONT_ATLAS_SIZE 1024

typedef struct SSRFont SSRFont;

// One glyph on the screen: where it goes, and where it is in the atlas.
typedef struct {
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
    // 0 for a glyph of the atlas, or the number from 1 of the icon of the
    // font that the quad shows (u and v are then 0 to 1).
    int icon;
} SSRGlyphQuad;

// Reads the TTF font at path with an em of emSize pixels, as the text size
// of an Android Paint. The kerning of the game comes from the file with the
// same name and the extension ".metrics", when it is there. Returns NULL
// when the file is not a font.
SSRFont *ssr_font_load(const char *path, float emSize);
void ssr_font_free(SSRFont *font);

// Reads the kerning of the game from a ".metrics" file (FontNative): an
// int count, three floats, then pairs of int characters with a float
// factor of the width of the first character. Returns 0 when it cannot.
int ssr_font_load_kerning(SSRFont *font, const char *path);

// Adds a font for the characters that the font does not have, at the same
// em size. data holds the font file, and the font keeps a copy. The first
// fallback font that has a character draws it. Returns 0 when the data is
// not a font. ssr_font_clear_fallbacks takes them away.
int ssr_font_add_fallback(SSRFont *font, const void *data, size_t size);
void ssr_font_clear_fallbacks(SSRFont *font);

// The glyph table of the game for the face and the size of this font
// ("Fonts/Glyphs/modern_<code>_glyphs.bin", GlyphMetricsLibrary): an int
// count of blocks, then for each block the face, the size, the start, and
// the end, then 14 bytes for each character: an int character, then the
// width, the height, bearingX, bearingY, and the advance as shorts. All
// numbers are big endian. A character of the table takes its advance, its
// ink width, and the place of its ink from the table. NULL data takes the
// table away. Returns the number of characters of the block.
int ssr_font_set_glyph_table(SSRFont *font, const unsigned char *data, size_t size, int face, int sizeIndex);

// The pixels that each character adds after its advance (the kernX of
// Fonts.txt).
void ssr_font_set_spacing(SSRFont *font, int spacing);

// The distance from one line to the next: the height of the box of the
// font, in whole pixels (FontMetrics bottom - top). And the distance from
// the top of a line to its baseline.
float ssr_font_line_height(const SSRFont *font);
float ssr_font_ascent(const SSRFont *font);

// Puts the quads of the text into quads, up to max. The top of the first
// line is at y. A '\n' starts a new line. Returns the number of quads that
// the text needs. A glyph that the font does not have takes no quad.
int ssr_font_layout(SSRFont *font, const char *text, float x, float y, SSRGlyphQuad *quads, int max);

// The width of the widest line of the text.
float ssr_font_measure(SSRFont *font, const char *text);

// An image in the place of a character (Inject: of Fonts.txt), for example
// the star of the bux. The image is width x height pixels. Its left is at
// the pen plus bearingX, its top is bearingY above the baseline, and the pen
// moves by advance. Returns the number of the icon from 1, or 0.
int ssr_font_add_icon(SSRFont *font, uint32_t code, int width, int height, int bearingX, int bearingY, int advance);

// The atlas: one byte of coverage for each pixel. version changes each time
// that the font adds glyphs, so that the caller knows when to upload it.
const unsigned char *ssr_font_atlas(const SSRFont *font, int *version);

// One size of font from the font set of Fonts.txt (c_FontManagerFR). The
// line is "slot:file:size:kernX,kernY:left,right,top,middle,bottom".
typedef struct {
    char file[64];
    // The size of the em, in pixels.
    float size;
    // The pixels that the game adds after each character.
    int spacing;
    // The space at the sides of a box, and the distance from the top of a
    // box, of the middle, and of the bottom to the baseline.
    float padLeft, padRight, padTop, padMiddle, padBottom;
    // The images in the place of characters: "Inject:code:file:bearingX:
    // bearingY:advance:flags".
    struct {
        uint32_t code;
        char file[64];
        int bearingX, bearingY, advance, flags;
    } icons[4];
    int iconCount;
} SSRFontSlot;

// Reads the slots of the first font set with the name set, or of the set
// "default", from the text of Fonts.txt. The text after "#ANDROID" is used
// when the file has it, as the game does. Returns the number of slots, up
// to max.
int ssr_font_read_set(const char *text, const char *set, SSRFontSlot *slots, int max);

// Reads one character of UTF-8 text and moves text past it. A bad byte
// gives U+FFFD. Returns 0 at the end of the text.
uint32_t ssr_utf8_next(const char **text);

#endif
