#include "ssr_gfx.h"

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
    return gfx;
}

void ssr_gfx_free(SSRGfx *gfx) {
    if (!gfx) return;
    for (int i = 0; i < gfx->textureCount; i++) SDL_DestroyTexture(gfx->textures[i].texture);
    for (int i = 0; i < SSR_FONT_COUNT; i++) {
        ssr_font_free(gfx->fonts[i].font);
        SDL_DestroyTexture(gfx->fonts[i].atlas);
    }
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
