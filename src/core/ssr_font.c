#include "ssr_font.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "stb_truetype.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// One pixel of space around each glyph in the atlas, so that filtering
// does not take pixels of the next glyph.
#define PAD 1

typedef struct {
    uint32_t codepoint;
    int glyph;
    int present;
    // The place in the atlas, and the offset from the pen to the top left.
    int ax, ay, w, h;
    int xoff, yoff;
    float advance;
} Glyph;

struct SSRFont {
    unsigned char *file;
    stbtt_fontinfo info;
    float scale;
    float ascent;
    float lineHeight;
    unsigned char *atlas;
    int version;
    // The shelf packer: glyphs go left to right on a shelf, then on a new
    // shelf below.
    int shelfX, shelfY, shelfHeight;
    Glyph *glyphs;
    int glyphCount;
    int glyphCapacity;
};

uint32_t ssr_utf8_next(const char **text) {
    const unsigned char *p = (const unsigned char *)*text;
    if (!*p) return 0;
    uint32_t c = *p;
    int extra = 0;
    uint32_t minimum = 0;
    if (c < 0x80) {
        *text += 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        c &= 0x1F;
        extra = 1;
        minimum = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        c &= 0x0F;
        extra = 2;
        minimum = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        c &= 0x07;
        extra = 3;
        minimum = 0x10000;
    } else {
        *text += 1;
        return 0xFFFD;
    }
    for (int i = 1; i <= extra; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *text += i;
            return 0xFFFD;
        }
        c = (c << 6) | (p[i] & 0x3F);
    }
    *text += 1 + extra;
    if (c < minimum || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
    return c;
}

// Copies the next part of the line up to stop, or to the end of the line,
// into out. Returns the place after the stop, or NULL at the end.
static const char *field(const char *p, char stop, char *out, size_t size) {
    size_t n = 0;
    while (*p && *p != stop && *p != '\n' && *p != '\r') {
        if (n + 1 < size) out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return *p == stop ? p + 1 : NULL;
}

static const char *skip_space(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

int ssr_font_read_set(const char *text, const char *set, SSRFontSlot *slots, int max) {
    if (!text || !set || !slots || max <= 0) return 0;
    const char *android = strstr(text, "#ANDROID");
    const char *p = android ? android + 8 : text;
    int inSet = 0, count = 0;
    while (*p) {
        const char *line = skip_space(p);
        const char *end = line;
        while (*end && *end != '\n') end++;
        p = *end ? end + 1 : end;
        if (line == end || (line[0] == '/' && line[1] == '/')) continue;
        if (strncmp(line, "ENDSET", 6) == 0) {
            if (inSet) break;
            continue;
        }
        if (strncmp(line, "FONTSET", 7) == 0) {
            char name[64];
            field(skip_space(line + 7), ' ', name, sizeof(name));
            inSet = strcmp(name, set) == 0 || strcmp(name, "default") == 0;
            continue;
        }
        if (!inSet || strncmp(line, "Inject", 6) == 0) continue;
        char part[128];
        const char *q = field(line, ':', part, sizeof(part));
        int index = atoi(part);
        if (!q || index < 0 || index >= max) continue;
        SSRFontSlot *slot = &slots[index];
        memset(slot, 0, sizeof(*slot));
        q = field(q, ':', slot->file, sizeof(slot->file));
        if (q) q = field(q, ':', part, sizeof(part));
        slot->size = (float)atof(part);
        if (q) {
            q = field(q, ':', part, sizeof(part));
            slot->spacing = atoi(part);
        }
        if (q) {
            field(q, ':', part, sizeof(part));
            float pad[5] = {0};
            const char *r = part;
            for (int i = 0; i < 5 && r; i++) {
                char number[32];
                r = field(r, ',', number, sizeof(number));
                pad[i] = (float)atof(number);
            }
            slot->padLeft = pad[0];
            slot->padRight = pad[1];
            slot->padTop = pad[2];
            slot->padMiddle = pad[3];
            slot->padBottom = pad[4];
        }
        if (index + 1 > count) count = index + 1;
    }
    return count;
}

SSRFont *ssr_font_load(const char *path, float pixelHeight) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return NULL;
    SSRFont *font = calloc(1, sizeof(SSRFont));
    long length = -1;
    if (font && fseek(file, 0, SEEK_END) == 0) length = ftell(file);
    if (length > 0) font->file = malloc((size_t)length);
    int ok = font && font->file && fseek(file, 0, SEEK_SET) == 0 &&
             fread(font->file, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (ok) ok = stbtt_InitFont(&font->info, font->file, stbtt_GetFontOffsetForIndex(font->file, 0));
    if (ok) font->atlas = calloc(SSR_FONT_ATLAS_SIZE, SSR_FONT_ATLAS_SIZE);
    if (!ok || !font->atlas) {
        ssr_font_free(font);
        return NULL;
    }
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &gap);
    font->scale = stbtt_ScaleForPixelHeight(&font->info, pixelHeight);
    font->ascent = ascent * font->scale;
    font->lineHeight = (ascent - descent + gap) * font->scale;
    return font;
}

void ssr_font_free(SSRFont *font) {
    if (!font) return;
    free(font->glyphs);
    free(font->atlas);
    free(font->file);
    free(font);
}

float ssr_font_line_height(const SSRFont *font) {
    return font ? font->lineHeight : 0;
}

float ssr_font_ascent(const SSRFont *font) {
    return font ? font->ascent : 0;
}

const unsigned char *ssr_font_atlas(const SSRFont *font, int *version) {
    if (version) *version = font ? font->version : 0;
    return font ? font->atlas : NULL;
}

// Puts the glyph into the atlas. A glyph that does not fit gets no pixels,
// but keeps its advance.
static void bake(SSRFont *font, Glyph *g) {
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&font->info, g->glyph, font->scale, font->scale, &x0, &y0, &x1, &y1);
    g->w = x1 - x0;
    g->h = y1 - y0;
    g->xoff = x0;
    g->yoff = y0;
    if (g->w <= 0 || g->h <= 0) {
        g->w = g->h = 0;
        return;
    }
    if (font->shelfX + g->w + PAD * 2 > SSR_FONT_ATLAS_SIZE) {
        font->shelfY += font->shelfHeight;
        font->shelfX = 0;
        font->shelfHeight = 0;
    }
    if (font->shelfY + g->h + PAD * 2 > SSR_FONT_ATLAS_SIZE || g->w + PAD * 2 > SSR_FONT_ATLAS_SIZE) {
        g->w = g->h = 0;
        return;
    }
    g->ax = font->shelfX + PAD;
    g->ay = font->shelfY + PAD;
    stbtt_MakeGlyphBitmap(&font->info, font->atlas + g->ay * SSR_FONT_ATLAS_SIZE + g->ax, g->w, g->h,
                          SSR_FONT_ATLAS_SIZE, font->scale, font->scale, g->glyph);
    font->shelfX += g->w + PAD * 2;
    if (g->h + PAD * 2 > font->shelfHeight) font->shelfHeight = g->h + PAD * 2;
    font->version++;
}

static const Glyph *glyph_for(SSRFont *font, uint32_t codepoint) {
    for (int i = 0; i < font->glyphCount; i++) {
        if (font->glyphs[i].codepoint == codepoint) return &font->glyphs[i];
    }
    if (font->glyphCount == font->glyphCapacity) {
        int capacity = font->glyphCapacity ? font->glyphCapacity * 2 : 128;
        Glyph *glyphs = realloc(font->glyphs, (size_t)capacity * sizeof(Glyph));
        if (!glyphs) return NULL;
        font->glyphs = glyphs;
        font->glyphCapacity = capacity;
    }
    Glyph *g = &font->glyphs[font->glyphCount++];
    memset(g, 0, sizeof(*g));
    g->codepoint = codepoint;
    g->glyph = stbtt_FindGlyphIndex(&font->info, (int)codepoint);
    g->present = g->glyph != 0;
    int advance, bearing;
    stbtt_GetGlyphHMetrics(&font->info, g->glyph, &advance, &bearing);
    g->advance = advance * font->scale;
    if (g->present) bake(font, g);
    return g;
}

int ssr_font_layout(SSRFont *font, const char *text, float x, float y, SSRGlyphQuad *quads, int max) {
    if (!font || !text) return 0;
    int count = 0;
    float penX = x;
    float baseline = y + font->ascent;
    int previous = 0;
    uint32_t c;
    while ((c = ssr_utf8_next(&text)) != 0) {
        if (c == '\n') {
            penX = x;
            baseline += font->lineHeight;
            previous = 0;
            continue;
        }
        const Glyph *g = glyph_for(font, c);
        if (!g) continue;
        if (previous && g->glyph) penX += stbtt_GetGlyphKernAdvance(&font->info, previous, g->glyph) * font->scale;
        if (g->present && g->w > 0) {
            if (quads && count < max) {
                SSRGlyphQuad *q = &quads[count];
                q->x0 = floorf(penX + 0.5f) + g->xoff;
                q->y0 = floorf(baseline + 0.5f) + g->yoff;
                q->x1 = q->x0 + g->w;
                q->y1 = q->y0 + g->h;
                q->u0 = (float)g->ax / SSR_FONT_ATLAS_SIZE;
                q->v0 = (float)g->ay / SSR_FONT_ATLAS_SIZE;
                q->u1 = (float)(g->ax + g->w) / SSR_FONT_ATLAS_SIZE;
                q->v1 = (float)(g->ay + g->h) / SSR_FONT_ATLAS_SIZE;
            }
            count++;
        }
        penX += g->advance;
        previous = g->glyph;
    }
    return count;
}

float ssr_font_measure(SSRFont *font, const char *text) {
    if (!font || !text) return 0;
    float widest = 0, penX = 0;
    int previous = 0;
    uint32_t c;
    while ((c = ssr_utf8_next(&text)) != 0) {
        if (c == '\n') {
            if (penX > widest) widest = penX;
            penX = 0;
            previous = 0;
            continue;
        }
        const Glyph *g = glyph_for(font, c);
        if (!g) continue;
        if (previous && g->glyph) penX += stbtt_GetGlyphKernAdvance(&font->info, previous, g->glyph) * font->scale;
        penX += g->advance;
        previous = g->glyph;
    }
    return penX > widest ? penX : widest;
}
