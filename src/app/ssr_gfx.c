#include "ssr_gfx.h"

#include <math.h>

#include "ssr_atlas.h"
#include "ssr_font.h"
#include "ssr_image.h"

// The pixel heights of the Android "new" font set, for the 2x screen.
static const float FONT_PIXELS[SSR_FONT_COUNT] = {31, 44, 60, 18};

#define MAX_TEXTURES 512
#define MAX_QUADS 1024

typedef struct {
    char name[256];
    SDL_Texture *texture;
} NamedTexture;

typedef struct {
    SSRFont *font;
    SDL_Texture *atlas;
    int atlasVersion;
} FontSlot;

struct SSRGfx {
    SDL_Renderer *renderer;
    char gameDirectory[2048];
    NamedTexture textures[MAX_TEXTURES];
    int textureCount;
    FontSlot fonts[SSR_FONT_COUNT];
    SSRAtlases *atlases;
    // One texture for each PNG page, and one for each image of a RLE page.
    SDL_Texture **pageTextures;
    SDL_Texture **frameTextures;
    // The Img: files of the scene that draws now.
    const SSRScene *scene;
    float offsetX, offsetY;
};

SSRGfx *ssr_gfx_create(SDL_Renderer *renderer, const char *gameDirectory) {
    SSRGfx *gfx = SDL_calloc(1, sizeof(SSRGfx));
    if (!gfx) return NULL;
    gfx->renderer = renderer;
    SDL_strlcpy(gfx->gameDirectory, gameDirectory, sizeof(gfx->gameDirectory));
    char path[2300];
    SDL_snprintf(path, sizeof(path), "%s/Fonts/Veriox-Rg.ttf", gameDirectory);
    for (int i = 0; i < SSR_FONT_COUNT; i++) {
        gfx->fonts[i].font = ssr_font_load(path, FONT_PIXELS[i]);
        gfx->fonts[i].atlasVersion = -1;
    }
    // The 2x screen uses the high resolution keys.
    SDL_snprintf(path, sizeof(path), "%s/NewGUI/Atlases", gameDirectory);
    const char *keys[] = {"Key.key", "CharactersHiRes.key"};
    gfx->atlases = ssr_atlas_load(path, keys, 2);
    if (gfx->atlases) {
        gfx->pageTextures = SDL_calloc((size_t)ssr_atlas_page_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
        gfx->frameTextures = SDL_calloc((size_t)ssr_atlas_image_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
    } else {
        SDL_Log("The atlases of the import are not there.");
    }
    return gfx;
}

void ssr_gfx_free(SSRGfx *gfx) {
    if (!gfx) return;
    for (int i = 0; i < gfx->textureCount; i++) SDL_DestroyTexture(gfx->textures[i].texture);
    for (int i = 0; i < SSR_FONT_COUNT; i++) {
        ssr_font_free(gfx->fonts[i].font);
        SDL_DestroyTexture(gfx->fonts[i].atlas);
    }
    if (gfx->atlases) {
        for (int i = 0; i < ssr_atlas_page_count(gfx->atlases); i++) SDL_DestroyTexture(gfx->pageTextures[i]);
        for (int i = 0; i < ssr_atlas_image_count(gfx->atlases); i++) SDL_DestroyTexture(gfx->frameTextures[i]);
    }
    SDL_free(gfx->pageTextures);
    SDL_free(gfx->frameTextures);
    ssr_atlas_free(gfx->atlases);
    SDL_free(gfx);
}

static SDL_Texture *texture_from_pixels(SDL_Renderer *renderer, int width, int height, void *pixels) {
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    SDL_Texture *texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
    SDL_DestroySurface(surface);
    return texture;
}

SDL_Texture *ssr_gfx_texture(SSRGfx *gfx, const char *name) {
    if (!gfx || !name) return NULL;
    for (int i = 0; i < gfx->textureCount; i++) {
        if (SDL_strcmp(gfx->textures[i].name, name) == 0) return gfx->textures[i].texture;
    }
    char path[2400];
    SDL_snprintf(path, sizeof(path), "%s/%s", gfx->gameDirectory, name);
    SSRImage image;
    SDL_Texture *texture = NULL;
    if (ssr_image_load(path, &image)) {
        texture = texture_from_pixels(gfx->renderer, image.width, image.height, image.pixels);
        ssr_image_free(&image);
    }
    // A missing image is kept too, so that the gfx does not look for it
    // again each frame.
    if (gfx->textureCount < MAX_TEXTURES) {
        NamedTexture *slot = &gfx->textures[gfx->textureCount++];
        SDL_strlcpy(slot->name, name, sizeof(slot->name));
        slot->texture = texture;
    }
    return texture;
}

// Uploads the atlas of the font when it has new glyphs. The atlas becomes
// white pixels with the coverage as alpha, so that a colour mod tints it.
static SDL_Texture *atlas_texture(SSRGfx *gfx, FontSlot *slot) {
    int version;
    const unsigned char *coverage = ssr_font_atlas(slot->font, &version);
    if (slot->atlas && version == slot->atlasVersion) return slot->atlas;
    if (!slot->atlas) {
        slot->atlas = SDL_CreateTexture(gfx->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                        SSR_FONT_ATLAS_SIZE, SSR_FONT_ATLAS_SIZE);
        if (!slot->atlas) return NULL;
        SDL_SetTextureBlendMode(slot->atlas, SDL_BLENDMODE_BLEND);
    }
    void *pixels;
    int pitch;
    if (!SDL_LockTexture(slot->atlas, NULL, &pixels, &pitch)) return slot->atlas;
    for (int y = 0; y < SSR_FONT_ATLAS_SIZE; y++) {
        Uint8 *row = (Uint8 *)pixels + y * pitch;
        const unsigned char *in = coverage + y * SSR_FONT_ATLAS_SIZE;
        for (int x = 0; x < SSR_FONT_ATLAS_SIZE; x++) {
            row[x * 4 + 0] = 255;
            row[x * 4 + 1] = 255;
            row[x * 4 + 2] = 255;
            row[x * 4 + 3] = in[x];
        }
    }
    SDL_UnlockTexture(slot->atlas);
    slot->atlasVersion = version;
    return slot->atlas;
}

float ssr_gfx_text_width(SSRGfx *gfx, SSRFontSize size, const char *text) {
    if (!gfx || size < 0 || size >= SSR_FONT_COUNT) return 0;
    return ssr_font_measure(gfx->fonts[size].font, text);
}

float ssr_gfx_line_height(SSRGfx *gfx, SSRFontSize size) {
    if (!gfx || size < 0 || size >= SSR_FONT_COUNT) return 0;
    return ssr_font_line_height(gfx->fonts[size].font);
}

void ssr_gfx_text(SSRGfx *gfx, SSRFontSize size, const char *text, float x, float y, uint32_t color,
                  SSRAlign align) {
    if (!gfx || !text || size < 0 || size >= SSR_FONT_COUNT) return;
    FontSlot *slot = &gfx->fonts[size];
    if (!slot->font) return;
    if (align != SSR_ALIGN_LEFT) {
        float width = ssr_font_measure(slot->font, text);
        x -= align == SSR_ALIGN_CENTER ? width / 2 : width;
    }
    static SSRGlyphQuad quads[MAX_QUADS];
    int count = ssr_font_layout(slot->font, text, x, y, quads, MAX_QUADS);
    if (count > MAX_QUADS) count = MAX_QUADS;
    // The layout can add glyphs, so the upload comes after it.
    SDL_Texture *atlas = atlas_texture(gfx, slot);
    if (!atlas) return;
    SDL_SetTextureColorMod(atlas, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    for (int i = 0; i < count; i++) {
        const SSRGlyphQuad *q = &quads[i];
        SDL_FRect source = {q->u0 * SSR_FONT_ATLAS_SIZE, q->v0 * SSR_FONT_ATLAS_SIZE, q->x1 - q->x0, q->y1 - q->y0};
        SDL_FRect place = {q->x0, q->y0, q->x1 - q->x0, q->y1 - q->y0};
        SDL_RenderTexture(gfx->renderer, atlas, &source, &place);
    }
}

// The texture of a PNG page, or of the frame of a RLE page that holds the
// image.
static SDL_Texture *atlas_texture_for(SSRGfx *gfx, const SSRAtlasImage *image) {
    const SSRAtlasPage *page = ssr_atlas_page(gfx->atlases, image->page);
    if (!page) return NULL;
    SSRImage pixels;
    if (page->rle) {
        int index = page->firstImage + image->frame;
        if (gfx->frameTextures[index]) return gfx->frameTextures[index];
        if (!ssr_atlas_frame_pixels(gfx->atlases, image->page, image->frame, &pixels)) return NULL;
        gfx->frameTextures[index] = texture_from_pixels(gfx->renderer, pixels.width, pixels.height, pixels.pixels);
        ssr_image_free(&pixels);
        return gfx->frameTextures[index];
    }
    if (gfx->pageTextures[image->page]) return gfx->pageTextures[image->page];
    if (!ssr_atlas_page_pixels(gfx->atlases, image->page, &pixels)) return NULL;
    gfx->pageTextures[image->page] = texture_from_pixels(gfx->renderer, pixels.width, pixels.height, pixels.pixels);
    ssr_image_free(&pixels);
    return gfx->pageTextures[image->page];
}

// The atlases hold the images by the names of the Img: lines. The file of
// an Img: line is only there when the atlases do not have the name.
int ssr_gfx_image_size(SSRGfx *gfx, const char *name, const char *file, float *w, float *h) {
    if (!gfx) return 0;
    const SSRAtlasImage *image = ssr_atlas_find(gfx->atlases, name);
    if (image) {
        *w = (float)image->originalWidth;
        *h = (float)image->originalHeight;
        return 1;
    }
    SDL_Texture *texture = file ? ssr_gfx_texture(gfx, file) : NULL;
    return texture && SDL_GetTextureSize(texture, w, h);
}

typedef struct {
    const SSRSceneItem *item;
    SDL_FColor colour;
    float cosA, sinA;
    float offsetX, offsetY;
} Placement;

// A point of the box (as parts of the box) on the screen, after the
// rotation about the pivot.
static SDL_FPoint place(const Placement *p, float x, float y) {
    const SSRSceneItem *item = p->item;
    if (item->quad) {
        // A point of the four corners, by its parts along the top and the
        // left side.
        const float *c = item->corners;
        SDL_FPoint out = {c[0] + (c[2] - c[0]) * x + (c[6] - c[0]) * y + p->offsetX,
                          c[1] + (c[3] - c[1]) * x + (c[7] - c[1]) * y + p->offsetY};
        return out;
    }
    float px = item->x + item->w * x - item->pivotX;
    float py = item->y + item->h * y - item->pivotY;
    // The matrix of the game (c_GTransform): i = (cos, -sin), j = (sin, cos).
    // A positive angle turns the image anticlockwise on the screen.
    SDL_FPoint out = {item->pivotX + px * p->cosA + py * p->sinA + p->offsetX,
                      item->pivotY - px * p->sinA + py * p->cosA + p->offsetY};
    return out;
}

static SDL_Vertex vertex(const Placement *p, float x, float y, float u, float v) {
    SDL_Vertex out;
    out.position = place(p, x, y);
    out.color = p->colour;
    out.tex_coord.x = u;
    out.tex_coord.y = v;
    return out;
}

static void draw_quad(SSRGfx *gfx, SDL_Texture *texture, const Placement *p, float x0, float y0, float x1, float y1,
                      float u0, float v0, float u1, float v1) {
    SDL_Vertex vertices[4] = {
        vertex(p, x0, y0, u0, v0),
        vertex(p, x1, y0, u1, v0),
        vertex(p, x1, y1, u1, v1),
        vertex(p, x0, y1, u0, v1),
    };
    static const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(gfx->renderer, texture, vertices, 4, indices, 6);
}

static void draw_atlas_image(SSRGfx *gfx, const SSRAtlasImage *image, const Placement *p) {
    SDL_Texture *texture = atlas_texture_for(gfx, image);
    if (!texture) return;
    const SSRAtlasPage *page = ssr_atlas_page(gfx->atlases, image->page);
    const SSRSceneItem *item = p->item;
    if (image->kind == SSR_ATLAS_PLAIN) {
        const float *r = image->rect;
        float u0 = r[2], v0 = r[3], u1 = r[6], v1 = r[7];
        if (page->rle) {
            // The frame has a texture of its own, so the coordinates are
            // the whole texture.
            u0 = v0 = 0;
            u1 = v1 = 1;
        }
        draw_quad(gfx, texture, p, r[0], r[1], r[4], r[5], u0, v0, u1, v1);
    } else if (image->kind == SSR_ATLAS_SLICED) {
        // A corner is at o + s * (drawn - fixed), in pixels of the box.
        float stretchX = fabsf(item->w) - (float)image->fixedWidth;
        float stretchY = fabsf(item->h) - (float)image->fixedHeight;
        for (int i = 0; i < image->shapeCount; i++) {
            const float *q = image->shapes + i * 12;
            float x0 = (q[2] + q[0] * stretchX) / fabsf(item->w), y0 = (q[3] + q[1] * stretchY) / fabsf(item->h);
            float x1 = (q[8] + q[6] * stretchX) / fabsf(item->w), y1 = (q[9] + q[7] * stretchY) / fabsf(item->h);
            draw_quad(gfx, texture, p, x0, y0, x1, y1, q[4], q[5], q[10], q[11]);
        }
    } else if (image->kind == SSR_ATLAS_STRIPPED) {
        for (int i = 0; i < image->shapeCount; i++) {
            const float *t = image->shapes + i * 12;
            SDL_Vertex vertices[3] = {
                vertex(p, t[0], t[1], t[2], t[3]),
                vertex(p, t[4], t[5], t[6], t[7]),
                vertex(p, t[8], t[9], t[10], t[11]),
            };
            SDL_RenderGeometry(gfx->renderer, texture, vertices, 3, NULL, 0);
        }
    }
}

static void draw_image_item(SSRGfx *gfx, const SSRSceneItem *item) {
    if (item->w == 0 || item->h == 0) return;
    Placement p;
    p.item = item;
    p.colour.r = ((item->colour >> 24) & 0xFF) / 255.0f;
    p.colour.g = ((item->colour >> 16) & 0xFF) / 255.0f;
    p.colour.b = ((item->colour >> 8) & 0xFF) / 255.0f;
    p.colour.a = (item->colour & 0xFF) / 255.0f;
    float radians = item->angle * (float)SDL_PI_F / 180.0f;
    p.cosA = cosf(radians);
    p.sinA = sinf(radians);
    p.offsetX = gfx->offsetX;
    p.offsetY = gfx->offsetY;

    const SSRAtlasImage *image = ssr_atlas_find(gfx->atlases, item->image);
    const char *file = image || !gfx->scene ? NULL : ssr_scene_image_file(gfx->scene, item->image);
    SDL_Texture *texture = file ? ssr_gfx_texture(gfx, file) : NULL;
    if (!texture && !image) return;
    SDL_BlendMode mode = item->additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND;
    if (texture) {
        SDL_SetTextureBlendMode(texture, mode);
        draw_quad(gfx, texture, &p, 0, 0, 1, 1, 0, 0, 1, 1);
        return;
    }
    SDL_Texture *page = atlas_texture_for(gfx, image);
    if (page) SDL_SetTextureBlendMode(page, mode);
    draw_atlas_image(gfx, image, &p);
}

// Draws one line of text with its top left at (x, y), scaled.
static void draw_line(SSRGfx *gfx, FontSlot *slot, const char *text, float x, float y, float scaleX, float scaleY,
                      uint32_t colour) {
    static SSRGlyphQuad quads[MAX_QUADS];
    int count = ssr_font_layout(slot->font, text, 0, 0, quads, MAX_QUADS);
    if (count > MAX_QUADS) count = MAX_QUADS;
    SDL_Texture *atlas = atlas_texture(gfx, slot);
    if (!atlas) return;
    SDL_SetTextureColorMod(atlas, (colour >> 24) & 0xFF, (colour >> 16) & 0xFF, (colour >> 8) & 0xFF);
    SDL_SetTextureAlphaMod(atlas, colour & 0xFF);
    for (int i = 0; i < count; i++) {
        const SSRGlyphQuad *q = &quads[i];
        SDL_FRect source = {q->u0 * SSR_FONT_ATLAS_SIZE, q->v0 * SSR_FONT_ATLAS_SIZE, q->x1 - q->x0, q->y1 - q->y0};
        SDL_FRect place = {x + q->x0 * scaleX, y + q->y0 * scaleY, (q->x1 - q->x0) * scaleX, (q->y1 - q->y0) * scaleY};
        SDL_RenderTexture(gfx->renderer, atlas, &source, &place);
    }
    SDL_SetTextureAlphaMod(atlas, 255);
}

#define MAX_LINES 32

// Cuts the text into lines at '\n', and at spaces when a line is wider
// than width. Each line goes into lines as a new buffer of the text.
static int wrap_lines(FontSlot *slot, const char *text, float width, char lines[MAX_LINES][256]) {
    int count = 0;
    const char *p = text;
    while (*p && count < MAX_LINES) {
        char line[256] = "";
        size_t used = 0;
        const char *lastBreak = NULL;
        size_t lastBreakUsed = 0;
        while (*p && *p != '\n' && used + 1 < sizeof(line)) {
            line[used++] = *p;
            line[used] = 0;
            if (*p == ' ') {
                lastBreak = p;
                lastBreakUsed = used - 1;
            }
            if (width > 0 && ssr_font_measure(slot->font, line) > width && lastBreak) {
                line[lastBreakUsed] = 0;
                p = lastBreak + 1;
                break;
            }
            p++;
        }
        if (*p == '\n') p++;
        SDL_strlcpy(lines[count++], line, 256);
    }
    return count;
}

static void draw_text_item(SSRGfx *gfx, const SSRSceneItem *item) {
    if (!item->text || !item->text[0]) return;
    FontSlot *slot = &gfx->fonts[item->font];
    if (!slot->font) return;
    float scaleX = item->fontScaleX > 0 ? item->fontScaleX : 1;
    float scaleY = item->fontScaleY > 0 ? item->fontScaleY : 1;
    char lines[MAX_LINES][256];
    int count = wrap_lines(slot, item->text, item->wrap && item->w > 0 ? item->w / scaleX : 0, lines);
    // A line that is wider than its box gets smaller to fit it.
    if (item->w > 0) {
        float widest = 0;
        for (int i = 0; i < count; i++) {
            float width = ssr_font_measure(slot->font, lines[i]) * scaleX;
            if (width > widest) widest = width;
        }
        if (widest > item->w) {
            float fit = item->w / widest;
            scaleX *= fit;
            scaleY *= fit;
        }
    }
    float lineHeight = ssr_font_line_height(slot->font) * scaleY;
    float total = lineHeight * count;
    float y = item->y;
    if (item->alignY == 'M') y += (item->h - total) / 2;
    if (item->alignY == 'B') y += item->h - total;
    for (int i = 0; i < count; i++) {
        float width = ssr_font_measure(slot->font, lines[i]) * scaleX;
        float x = item->x;
        if (item->alignX == 'C') x += (item->w - width) / 2;
        if (item->alignX == 'R') x += item->w - width;
        draw_line(gfx, slot, lines[i], x + gfx->offsetX, y + lineHeight * i + gfx->offsetY, scaleX, scaleY,
                  item->colour);
    }
}

void ssr_gfx_set_offset(SSRGfx *gfx, float x, float y) {
    if (!gfx) return;
    gfx->offsetX = x;
    gfx->offsetY = y;
}

void ssr_gfx_draw_scene(SSRGfx *gfx, const SSRScene *scene) {
    if (!gfx || !scene) return;
    gfx->scene = scene;
    SDL_Rect clips[32];
    int clipCount = 0;
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        switch (item->kind) {
        case SSR_ITEM_IMAGE:
            draw_image_item(gfx, item);
            break;
        case SSR_ITEM_TEXT:
            draw_text_item(gfx, item);
            break;
        case SSR_ITEM_CLIP:
            if (clipCount < 32) {
                SDL_Rect rect = {(int)(item->x + gfx->offsetX), (int)(item->y + gfx->offsetY), (int)item->w, (int)item->h};
                clips[clipCount++] = rect;
                SDL_SetRenderClipRect(gfx->renderer, &rect);
            }
            break;
        case SSR_ITEM_UNCLIP:
            if (clipCount > 0) clipCount--;
            SDL_SetRenderClipRect(gfx->renderer, clipCount ? &clips[clipCount - 1] : NULL);
            break;
        case SSR_ITEM_AREA:
            break;
        }
    }
    SDL_SetRenderClipRect(gfx->renderer, NULL);
    gfx->scene = NULL;
}
