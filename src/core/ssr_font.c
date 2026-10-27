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
    // The face of the glyph: 0 the font, 1 or more a fallback font.
    int face;
    int glyph;
    int present;
    // The place in the atlas, and the offset from the pen to the top left.
    int ax, ay, w, h;
    int xoff, yoff;
    float advance;
    // The width of the ink (Paint.getTextBounds), for the kerning.
    int inkWidth;
} Glyph;

typedef struct {
    // The first character in the high 16 bits, as MakeKerningKey.
    uint32_t key;
    // The place in the file. A later pair takes the place of an earlier
    // pair with the same key, as in a map.
    int order;
    float factor;
} KernPair;

// The metrics of a character from a glyph table of the game.
typedef struct {
    uint32_t code;
    short w, h, bearingX, bearingY, advance;
} TableEntry;

#define MAX_FACES 4

// A TTF or OTF file.
typedef struct {
    unsigned char *file;
    stbtt_fontinfo info;
    float scale;
} Face;

// An image in the place of a character.
typedef struct {
    uint32_t code;
    int w, h, bearingX, bearingY, advance;
} Icon;

struct SSRFont {
    // The font, then the fallback fonts.
    Face faces[MAX_FACES];
    int faceCount;
    float emSize;
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
    int spacing;
    KernPair *kerns;
    int kernCount;
    Icon icons[8];
    int iconCount;
    // The glyph table, in the order of the characters.
    TableEntry *table;
    int tableCount;
};

static int icon_for(const SSRFont *font, uint32_t c) {
    for (int i = 0; i < font->iconCount; i++) {
        if (font->icons[i].code == c) return i + 1;
    }
    return 0;
}

int ssr_font_add_icon(SSRFont *font, uint32_t code, int width, int height, int bearingX, int bearingY, int advance) {
    if (!font || font->iconCount >= 8) return 0;
    int i = font->iconCount++;
    font->icons[i].code = code;
    font->icons[i].w = width;
    font->icons[i].h = height;
    font->icons[i].bearingX = bearingX;
    font->icons[i].bearingY = bearingY;
    font->icons[i].advance = advance;
    return i + 1;
}

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
    int inSet = 0, count = 0, last = -1;
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
        if (!inSet) continue;
        char part[128];
        if (strncmp(line, "Inject", 6) == 0) {
            // The icon belongs to the slot of the line before it.
            if (last < 0) continue;
            SSRFontSlot *slot = &slots[last];
            if (slot->iconCount >= 4) continue;
            int values[5] = {0};
            const char *q = field(line, ':', part, sizeof(part));
            if (q) q = field(q, ':', part, sizeof(part));
            uint32_t code = (uint32_t)strtoul(part, NULL, 10);
            char file[64] = "";
            if (q) q = field(q, ':', file, sizeof(file));
            for (int i = 0; i < 4 && q; i++) {
                q = field(q, ':', part, sizeof(part));
                values[i] = atoi(part);
            }
            int at = slot->iconCount++;
            slot->icons[at].code = code;
            snprintf(slot->icons[at].file, sizeof(slot->icons[at].file), "%.*s", (int)strcspn(file, " \t\r"), file);
            slot->icons[at].bearingX = values[0];
            slot->icons[at].bearingY = values[1];
            slot->icons[at].advance = values[2];
            slot->icons[at].flags = values[3];
            continue;
        }
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
        last = index;
    }
    return count;
}

// Starts a face on the bytes of a font file, which the face then owns.
// Returns 0 and frees the bytes when they are not a font.
static int start_face(Face *face, unsigned char *file, float emSize) {
    int offset = file ? stbtt_GetFontOffsetForIndex(file, 0) : -1;
    if (offset < 0 || !stbtt_InitFont(&face->info, file, offset)) {
        free(file);
        return 0;
    }
    face->file = file;
    face->scale = stbtt_ScaleForMappingEmToPixels(&face->info, emSize);
    return 1;
}

// Reads a font file. Returns 0 when it cannot.
static int load_face(Face *face, const char *path, float emSize) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    long length = -1;
    unsigned char *bytes = NULL;
    if (fseek(file, 0, SEEK_END) == 0) length = ftell(file);
    if (length > 0) bytes = malloc((size_t)length);
    int ok = bytes && fseek(file, 0, SEEK_SET) == 0 && fread(bytes, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!ok) {
        free(bytes);
        return 0;
    }
    return start_face(face, bytes, emSize);
}

SSRFont *ssr_font_load(const char *path, float emSize) {
    SSRFont *font = calloc(1, sizeof(SSRFont));
    if (!font) return NULL;
    font->emSize = emSize;
    if (load_face(&font->faces[0], path, emSize)) font->faceCount = 1;
    if (font->faceCount) font->atlas = calloc(SSR_FONT_ATLAS_SIZE, SSR_FONT_ATLAS_SIZE);
    if (!font->faceCount || !font->atlas) {
        ssr_font_free(font);
        return NULL;
    }
    // Android takes the top and the bottom of the font from its box.
    const Face *face = &font->faces[0];
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&face->info, &x0, &y0, &x1, &y1);
    font->ascent = y1 * face->scale;
    font->lineHeight = floorf((y1 - y0) * face->scale);
    char metrics[1024];
    const char *dot = strrchr(path, '.');
    size_t stem = dot ? (size_t)(dot - path) : strlen(path);
    if (stem + 9 < sizeof(metrics)) {
        memcpy(metrics, path, stem);
        memcpy(metrics + stem, ".metrics", 9);
        ssr_font_load_kerning(font, metrics);
    }
    return font;
}

// Forgets the glyphs, so that they come again with new metrics or faces.
static void clear_glyphs(SSRFont *font) {
    font->glyphCount = 0;
    font->shelfX = font->shelfY = font->shelfHeight = 0;
    memset(font->atlas, 0, (size_t)SSR_FONT_ATLAS_SIZE * SSR_FONT_ATLAS_SIZE);
    font->version++;
}

int ssr_font_add_fallback(SSRFont *font, const void *data, size_t size) {
    if (!font || !data || size < 12 || font->faceCount >= MAX_FACES) return 0;
    unsigned char *copy = malloc(size);
    if (!copy) return 0;
    memcpy(copy, data, size);
    if (!start_face(&font->faces[font->faceCount], copy, font->emSize)) return 0;
    font->faceCount++;
    clear_glyphs(font);
    return 1;
}

void ssr_font_clear_fallbacks(SSRFont *font) {
    if (!font) return;
    for (int i = 1; i < font->faceCount; i++) {
        free(font->faces[i].file);
        memset(&font->faces[i], 0, sizeof(Face));
    }
    font->faceCount = font->faceCount ? 1 : 0;
    clear_glyphs(font);
}

static uint32_t read_big(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static short read_short(const unsigned char *p) {
    return (short)((uint16_t)p[0] << 8 | p[1]);
}

int ssr_font_set_glyph_table(SSRFont *font, const unsigned char *data, size_t size, int face, int sizeIndex) {
    if (!font) return 0;
    free(font->table);
    font->table = NULL;
    font->tableCount = 0;
    clear_glyphs(font);
    if (!data || size < 4) return 0;
    uint32_t headers = read_big(data);
    if (headers > (size - 4) / 16) return 0;
    for (uint32_t i = 0; i < headers; i++) {
        const unsigned char *h = data + 4 + 16 * i;
        uint32_t start = read_big(h + 8), end = read_big(h + 12);
        if ((int)read_big(h) != face || (int)read_big(h + 4) != sizeIndex) continue;
        if (start > end || end > size) return 0;
        int count = (int)((end - start) / 14);
        font->table = malloc((size_t)(count ? count : 1) * sizeof(TableEntry));
        if (!font->table) return 0;
        for (int k = 0; k < count; k++) {
            const unsigned char *e = data + start + 14 * k;
            TableEntry *t = &font->table[k];
            t->code = read_big(e);
            t->w = read_short(e + 4);
            t->h = read_short(e + 6);
            t->bearingX = read_short(e + 8);
            t->bearingY = read_short(e + 10);
            t->advance = read_short(e + 12);
        }
        font->tableCount = count;
        return count;
    }
    return 0;
}

// The entry of the character in the glyph table, or NULL. The game sorts
// the table by character (GlyphMetricsLibrary.GetGlyphMetrics).
static const TableEntry *table_entry(const SSRFont *font, uint32_t code) {
    int low = 0, high = font->tableCount - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        uint32_t c = font->table[middle].code;
        if (c == code) return &font->table[middle];
        if (c < code) low = middle + 1;
        else high = middle - 1;
    }
    return NULL;
}

static int read_int(FILE *file, int32_t *value) {
    unsigned char b[4];
    if (fread(b, 1, 4, file) != 4) return 0;
    *value = (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
    return 1;
}

static int read_float(FILE *file, float *value) {
    int32_t bits;
    if (!read_int(file, &bits)) return 0;
    memcpy(value, &bits, sizeof(*value));
    return 1;
}

static int compare_kerns(const void *a, const void *b) {
    const KernPair *x = a, *y = b;
    if (x->key != y->key) return x->key < y->key ? -1 : 1;
    return x->order < y->order ? -1 : x->order > y->order;
}

int ssr_font_load_kerning(SSRFont *font, const char *path) {
    if (!font || !path) return 0;
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    int32_t count;
    float unused;
    int ok = read_int(file, &count) && read_float(file, &unused) && read_float(file, &unused) &&
             read_float(file, &unused);
    int capacity = font->kernCount;
    int32_t first, second;
    float factor;
    while (ok && read_int(file, &first) && read_int(file, &second) && read_float(file, &factor)) {
        if (font->kernCount == capacity) {
            capacity = capacity ? capacity * 2 : 256;
            KernPair *kerns = realloc(font->kerns, (size_t)capacity * sizeof(KernPair));
            if (!kerns) break;
            font->kerns = kerns;
        }
        // The game keeps the characters as shorts.
        font->kerns[font->kernCount].key = (uint32_t)(uint16_t)first << 16 | (uint16_t)second;
        font->kerns[font->kernCount].order = font->kernCount;
        font->kerns[font->kernCount].factor = factor;
        font->kernCount++;
    }
    fclose(file);
    qsort(font->kerns, (size_t)font->kernCount, sizeof(KernPair), compare_kerns);
    return ok;
}

void ssr_font_set_spacing(SSRFont *font, int spacing) {
    if (font) font->spacing = spacing;
}

void ssr_font_free(SSRFont *font) {
    if (!font) return;
    for (int i = 0; i < font->faceCount; i++) free(font->faces[i].file);
    free(font->glyphs);
    free(font->kerns);
    free(font->table);
    free(font->atlas);
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
// Takes the last row of the glyph out of the atlas.
static void drop_last_row(SSRFont *font, Glyph *g) {
    if (g->h < 2) return;
    memset(font->atlas + (g->ay + g->h - 1) * SSR_FONT_ATLAS_SIZE + g->ax, 0, (size_t)g->w);
    g->h--;
    font->version++;
}

static void bake(SSRFont *font, Glyph *g) {
    const Face *face = &font->faces[g->face];
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&face->info, g->glyph, face->scale, face->scale, &x0, &y0, &x1, &y1);
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
    stbtt_MakeGlyphBitmap(&face->info, font->atlas + g->ay * SSR_FONT_ATLAS_SIZE + g->ax, g->w, g->h,
                          SSR_FONT_ATLAS_SIZE, face->scale, face->scale, g->glyph);

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
    // A character that the font does not have comes from the first fallback
    // font that has it (FontNative.GetTextPaint).
    for (int i = 0; i < font->faceCount && !g->glyph; i++) {
        g->face = i;
        g->glyph = stbtt_FindGlyphIndex(&font->faces[i].info, (int)codepoint);
    }
    if (!g->glyph) g->face = 0;
    g->present = g->glyph != 0;
    const Face *face = &font->faces[g->face];
    int advance, bearing;
    stbtt_GetGlyphHMetrics(&face->info, g->glyph, &advance, &bearing);
    // Android gives the hinted advance in whole pixels.
    g->advance = roundf(advance * face->scale);
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&face->info, g->glyph, face->scale, face->scale, &x0, &y0, &x1, &y1);
    g->inkWidth = x1 - x0;
    if (g->present) bake(font, g);
    // The glyph table gives the advance that the game had, and the place of
    // the ink after the pen: bearingX (c_FontRenderer.Write). The height of
    // the ink stays with the outline. [I: Android draws hinted glyphs, so
    // the table has their heights. The remake draws the outline with no
    // hinting, and a top from the table would put the bottom of a round
    // letter one pixel under the baseline.]
    const TableEntry *t = table_entry(font, codepoint);
    if (t) {
        g->advance = t->advance;
        g->inkWidth = t->w;
        g->xoff = t->bearingX;
        // A round letter goes a part of a pixel under the baseline. Android
        // hints the outline onto the baseline when the table says that the
        // letter ends there. With no hinting, the faint row under the
        // baseline puts the letter one pixel lower than the flat letters, so
        // the remake drops that row. [I: an approximation of the hinting.]
        if (g->present && g->h > 0 && t->h - t->bearingY == 0 && g->yoff + g->h == 1) drop_last_row(font, g);
    }
    return g;
}

// The kerning of the game after the character first: the width of its ink
// times the factor of the pair, in whole pixels (FontNative.GetKerning).
static float kerning(const SSRFont *font, uint32_t first, int inkWidth, uint32_t second) {
    if (first > 0xFFFF || second > 0xFFFF) return 0;
    uint32_t key = first << 16 | second;
    // The last pair with the key: the first place with a greater key, less
    // one.
    int low = 0, high = font->kernCount;
    while (low < high) {
        int middle = (low + high) / 2;
        if (font->kerns[middle].key <= key) low = middle + 1;
        else high = middle;
    }
    if (low == 0 || font->kerns[low - 1].key != key) return 0;
    return (float)(int)(inkWidth * font->kerns[low - 1].factor);
}

int ssr_font_layout(SSRFont *font, const char *text, float x, float y, SSRGlyphQuad *quads, int max) {
    if (!font || !text) return 0;
    int count = 0;
    float penX = x;
    float baseline = y + font->ascent;
    uint32_t previous = 0;
    int previousInk = 0;
    uint32_t c;
    while ((c = ssr_utf8_next(&text)) != 0) {
        if (c == '\n') {
            penX = x;
            baseline += font->lineHeight;
            previous = 0;
            continue;
        }
        int icon = icon_for(font, c);
        if (icon) {
            const Icon *n = &font->icons[icon - 1];
            if (quads && count < max) {
                SSRGlyphQuad *q = &quads[count];
                q->x0 = floorf(penX + 0.5f) + (float)n->bearingX;
                q->y0 = floorf(baseline + 0.5f) - (float)n->bearingY;
                q->x1 = q->x0 + (float)n->w;
                q->y1 = q->y0 + (float)n->h;
                q->u0 = q->v0 = 0;
                q->u1 = q->v1 = 1;
                q->icon = icon;
            }
            count++;
            penX += (float)n->advance + (float)font->spacing;
            previous = 0;
            continue;
        }
        const Glyph *g = glyph_for(font, c);
        if (!g) continue;
        if (previous) penX += kerning(font, previous, previousInk, c);
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
                q->icon = 0;
            }
            count++;
        }
        penX += g->advance + (float)font->spacing;
        previous = c;
        previousInk = g->inkWidth;
    }
    return count;
}

float ssr_font_measure(SSRFont *font, const char *text) {
    if (!font || !text) return 0;
    float widest = 0, penX = 0;
    uint32_t previous = 0;
    int previousInk = 0;
    uint32_t c;
    while ((c = ssr_utf8_next(&text)) != 0) {
        if (c == '\n') {
            if (penX > widest) widest = penX;
            penX = 0;
            previous = 0;
            continue;
        }
        int icon = icon_for(font, c);
        if (icon) {
            penX += (float)font->icons[icon - 1].advance + (float)font->spacing;
            previous = 0;
            continue;
        }
        const Glyph *g = glyph_for(font, c);
        if (!g) continue;
        if (previous) penX += kerning(font, previous, previousInk, c);
        penX += g->advance + (float)font->spacing;
        previous = c;
        previousInk = g->inkWidth;
    }
    return penX > widest ? penX : widest;
}
