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
} SSRGlyphQuad;

// Reads the TTF font at path, for text with lines of pixelHeight pixels.
// Returns NULL when the file is not a font.
SSRFont *ssr_font_load(const char *path, float pixelHeight);
void ssr_font_free(SSRFont *font);

// The distance from one line to the next, and from the top of a line to
// its baseline.
float ssr_font_line_height(const SSRFont *font);
float ssr_font_ascent(const SSRFont *font);

// Puts the quads of the text into quads, up to max. The top of the first
// line is at y. A '\n' starts a new line. Returns the number of quads that
// the text needs. A glyph that the font does not have takes no quad.
int ssr_font_layout(SSRFont *font, const char *text, float x, float y, SSRGlyphQuad *quads, int max);

// The width of the widest line of the text.
float ssr_font_measure(SSRFont *font, const char *text);

// The atlas: one byte of coverage for each pixel. version changes each time
// that the font adds glyphs, so that the caller knows when to upload it.
const unsigned char *ssr_font_atlas(const SSRFont *font, int *version);

// Reads one character of UTF-8 text and moves text past it. A bad byte
// gives U+FFFD. Returns 0 at the end of the text.
uint32_t ssr_utf8_next(const char **text);

#endif
