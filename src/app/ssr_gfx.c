#include "ssr_gfx.h"

#include <math.h>

#include "ssr_atlas.h"
#include "ssr_font.h"
#include "ssr_fs.h"
#include "ssr_image.h"

// The Android "new" font set of Fonts.txt, for an import that has no
// Fonts.txt.
static const SSRFontSlot DEFAULT_FONTS[SSR_FONT_COUNT] = {
    {"Veriox-Rg.ttf", 31, -1, 6, 5, 32, 28, -26, {{0}}, 0},
    {"Veriox-Rg.ttf", 44, -1, 6, 5, 45, 40, -40, {{0}}, 0},
    {"Veriox-Rg.ttf", 60, -1, 6, 5, 59, 50, -48, {{0}}, 0},
    {"Veriox-Rg.ttf", 18, -1, 6, 5, 18, 16, -14, {{0}}, 0},
};

#define MAX_TEXTURES 512
// The screens that the remake can attach at once.
#define MAX_HOOKS 16
#define MAX_QUADS 1024

typedef struct {
    char name[256];
    SDL_Texture *texture;
} NamedTexture;

typedef struct {
    SSRFont *font;
    SSRFontSlot config;
    SDL_Texture *atlas;
    int atlasVersion;
    // The textures of the icons of the font, by their number from 1.
    SDL_Texture *icons[8];
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
    // The same pages with premultiplied alpha, for Gloss().
    SDL_Texture **glossPages;
    SDL_Texture **glossFrames;
    // The Img: files of the scene that draws now.
    const SSRScene *scene;
    const SSRTweaks *tweaks;
    float offsetX, offsetY;
    // The time of the scene that draws now, in ms.
    int gameMS;
    // The scenes that draw in the place of an element (AttachSubscreen),
    // and the clip of the element while one draws.
    struct {
        char ref[48];
        const SSRScene *scene;
    } hooks[MAX_HOOKS];
    int hasOuter;
    SDL_Rect outer;
    // The names of the art of the remake (art/<name>.png).
    char **art;
    int artCount;
    // The art of the data pack (<pack>/art/<name>.png). It comes before
    // the art of the remake.
    char packDirectory[2048];
    char **packArt;
    int packArtCount;
    // The hole in the ring of a shield (MapShieldFrameAll), measured once:
    // 0 not yet, 1 measured, -1 no hole. hole has a byte for each pixel of
    // the image of a crest: 1 where the crest shows. The ellipse of the
    // hole: the middle, and the map from a point of the image to the unit
    // circle: u = (dx - lean * dy) / width, v = dy / height. The map keeps
    // the rows of the crest level, and the sides of the crest lean with the
    // perspective of the ring.
    int holeState;
    unsigned char *hole;
    float holeX, holeY, holeWidth, holeHeight, holeLean;
};

SSRGfx *ssr_gfx_create(SDL_Renderer *renderer, const char *gameDirectory) {
    SSRGfx *gfx = SDL_calloc(1, sizeof(SSRGfx));
    if (!gfx) return NULL;
    gfx->renderer = renderer;
    SDL_strlcpy(gfx->gameDirectory, gameDirectory, sizeof(gfx->gameDirectory));
    char path[2300];
    SSRFontSlot slots[SSR_FONT_COUNT];
    SDL_memcpy(slots, DEFAULT_FONTS, sizeof(slots));
    SDL_snprintf(path, sizeof(path), "%s/Fonts/Fonts.txt", gameDirectory);
    char *fonts = SDL_LoadFile(path, NULL);
    if (fonts && ssr_font_read_set(fonts, "new", slots, SSR_FONT_COUNT) < SSR_FONT_COUNT) {
        SDL_Log("Fonts.txt has less than %d fonts in the set \"new\".", SSR_FONT_COUNT);
        SDL_memcpy(slots, DEFAULT_FONTS, sizeof(slots));
    }
    SDL_free(fonts);
    for (int i = 0; i < SSR_FONT_COUNT; i++) {
        SDL_snprintf(path, sizeof(path), "%s/Fonts/%s", gameDirectory, slots[i].file);
        gfx->fonts[i].font = ssr_font_load(path, slots[i].size);
        ssr_font_set_spacing(gfx->fonts[i].font, slots[i].spacing);
        // The images of Inject: lines, for example the star of the bux.
        for (int k = 0; k < slots[i].iconCount; k++) {
            char file[128];
            SDL_snprintf(file, sizeof(file), "Fonts/%s", slots[i].icons[k].file);
            float w = 0, h = 0;
            SDL_Texture *icon = ssr_gfx_texture(gfx, file);
            if (!icon || !SDL_GetTextureSize(icon, &w, &h)) continue;
            int n = ssr_font_add_icon(gfx->fonts[i].font, slots[i].icons[k].code, (int)w, (int)h,
                                      slots[i].icons[k].bearingX, slots[i].icons[k].bearingY,
                                      slots[i].icons[k].advance);
            if (n > 0 && n <= 8) gfx->fonts[i].icons[n - 1] = icon;
        }
        gfx->fonts[i].config = slots[i];
        gfx->fonts[i].atlasVersion = -1;
    }
    // The 2x screen uses the high resolution keys.
    SDL_snprintf(path, sizeof(path), "%s/NewGUI/Atlases", gameDirectory);
    const char *keys[] = {"Key.key", "CharactersHiRes.key"};
    gfx->atlases = ssr_atlas_load(path, keys, 2);
    if (gfx->atlases) {
        gfx->pageTextures = SDL_calloc((size_t)ssr_atlas_page_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
        gfx->frameTextures = SDL_calloc((size_t)ssr_atlas_image_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
        gfx->glossPages = SDL_calloc((size_t)ssr_atlas_page_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
        gfx->glossFrames = SDL_calloc((size_t)ssr_atlas_image_count(gfx->atlases) + 1, sizeof(SDL_Texture *));
    } else {
        SDL_Log("The atlases of the import are not there.");
    }
    const char *base = SDL_GetBasePath();
    SDL_snprintf(path, sizeof(path), "%sart", base ? base : "");
    gfx->art = SDL_GlobDirectory(path, "*.png", 0, &gfx->artCount);
    return gfx;
}

void ssr_gfx_set_language(SSRGfx *gfx, const char *code) {
    if (!gfx || !code) return;
    char path[2400], resolved[2400];
    SDL_snprintf(path, sizeof(path), "%s/Fonts/Glyphs/modern_%s_glyphs.bin", gfx->gameDirectory, code);
    size_t tableSize = 0;
    void *table = ssr_fs_resolve(path, resolved, sizeof(resolved)) ? SDL_LoadFile(resolved, &tableSize) : NULL;
    if (!table) SDL_Log("The import has no %s.", path);
    // SDL reads the fonts of the app, so that it finds them in the assets of
    // an Android APK too.
    const char *base = SDL_GetBasePath();
    size_t sizes[2] = {0, 0};
    void *fallbacks[2];
    SDL_snprintf(path, sizeof(path), "%sfonts/Fallback.ttf", base ? base : "");
    fallbacks[0] = SDL_LoadFile(path, &sizes[0]);
    if (!fallbacks[0]) SDL_Log("The app has no %s.", path);
    SDL_snprintf(path, sizeof(path), "%sfonts/Fallback-%s.ttf", base ? base : "", code);
    fallbacks[1] = SDL_LoadFile(path, &sizes[1]);
    for (int i = 0; i < SSR_FONT_COUNT; i++) {
        SSRFont *font = gfx->fonts[i].font;
        if (!font) continue;
        // The slots of the set are the sizes of face 0, in their order.
        ssr_font_set_glyph_table(font, table, tableSize, 0, i);
        ssr_font_clear_fallbacks(font);
        for (int k = 0; k < 2; k++) {
            if (fallbacks[k]) ssr_font_add_fallback(font, fallbacks[k], sizes[k]);
        }
    }
    for (int k = 0; k < 2; k++) SDL_free(fallbacks[k]);
    SDL_free(table);
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
        for (int i = 0; i < ssr_atlas_page_count(gfx->atlases); i++) SDL_DestroyTexture(gfx->glossPages[i]);
        for (int i = 0; i < ssr_atlas_image_count(gfx->atlases); i++) SDL_DestroyTexture(gfx->glossFrames[i]);
    }
    SDL_free(gfx->pageTextures);
    SDL_free(gfx->frameTextures);
    SDL_free(gfx->glossPages);
    SDL_free(gfx->glossFrames);
    ssr_atlas_free(gfx->atlases);
    SDL_free(gfx->art);
    SDL_free(gfx->packArt);
    SDL_free(gfx->hole);
    SDL_free(gfx);
}

static SDL_Texture *texture_from_pixels(SDL_Renderer *renderer, int width, int height, void *pixels) {
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    SDL_Texture *texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
    SDL_DestroySurface(surface);
    return texture;
}

// Loads an image of the remake. SDL reads the file, so that the app also
// finds it in the assets of an Android APK.
static int load_own_image(const char *path, SSRImage *image) {
    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    int ok = data && ssr_image_decode(data, size, image);
    SDL_free(data);
    return ok;
}

// Loads the image at the path once, and keeps it by the key. own is 1 for
// an image of the remake.
static SDL_Texture *cached_texture(SSRGfx *gfx, const char *key, const char *path, int own) {
    for (int i = 0; i < gfx->textureCount; i++) {
        if (SDL_strcmp(gfx->textures[i].name, key) == 0) return gfx->textures[i].texture;
    }
    SSRImage image;
    SDL_Texture *texture = NULL;
    if (own ? load_own_image(path, &image) : ssr_image_load(path, &image)) {
        texture = texture_from_pixels(gfx->renderer, image.width, image.height, image.pixels);
        ssr_image_free(&image);
    }
    // A missing image is kept too, so that the gfx does not look for it
    // again each frame.
    if (gfx->textureCount < MAX_TEXTURES) {
        NamedTexture *slot = &gfx->textures[gfx->textureCount++];
        SDL_strlcpy(slot->name, key, sizeof(slot->name));
        slot->texture = texture;
    }
    return texture;
}

SDL_Texture *ssr_gfx_texture(SSRGfx *gfx, const char *name) {
    if (!gfx || !name) return NULL;
    char path[2400];
    SDL_snprintf(path, sizeof(path), "%s/%s", gfx->gameDirectory, name);
    return cached_texture(gfx, name, path, 0);
}

// The art of the remake (art/<name>.png) takes the place of the atlas
// image with the name. It fills the full box of the original image.
static SDL_Texture *own_art(SSRGfx *gfx, const char *name) {
    if (!name || !name[0]) return NULL;
    char file[256];
    SDL_snprintf(file, sizeof(file), "%s.png", name);
    for (int i = 0; i < gfx->packArtCount; i++) {
        if (SDL_strcmp(gfx->packArt[i], file) != 0) continue;
        char key[256], path[2400];
        SDL_snprintf(key, sizeof(key), "pack:%s", name);
        SDL_snprintf(path, sizeof(path), "%s/art/%s", gfx->packDirectory, file);
        return cached_texture(gfx, key, path, 1);
    }
    int found = 0;
    for (int i = 0; i < gfx->artCount && !found; i++) found = SDL_strcmp(gfx->art[i], file) == 0;
    if (!found) return NULL;
    char key[256], path[2400];
    SDL_snprintf(key, sizeof(key), "art:%s", name);
    const char *base = SDL_GetBasePath();
    SDL_snprintf(path, sizeof(path), "%sart/%s", base ? base : "", file);
    return cached_texture(gfx, key, path, 1);
}

// The image of a crest has the shape of the box of a shield, at two times
// its size, so that the crest goes on the face of the shield.
#define CREST_W 420
#define CREST_H 570

static void draw_image_item(SSRGfx *gfx, const SSRSceneItem *item);

// Measures the hole in the ring of a shield: the app draws the ring
// (MapShieldFrameAll) into the box of a crest, and fills the clear pixels
// from the middle. The ellipse of the hole comes from the moments of the
// fill, so that it follows the perspective of the ring. The hole grows by
// a few pixels under the ring, so that no gap shows at its edge.
static void measure_hole(SSRGfx *gfx) {
    gfx->holeState = -1;
    SDL_Texture *target = SDL_CreateTexture(gfx->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, CREST_W,
                                            CREST_H);
    if (!target) return;
    SDL_Texture *old = SDL_GetRenderTarget(gfx->renderer);
    float offsetX = gfx->offsetX, offsetY = gfx->offsetY;
    SDL_SetRenderTarget(gfx->renderer, target);
    SDL_SetRenderDrawColor(gfx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(gfx->renderer);
    gfx->offsetX = gfx->offsetY = 0;
    SSRSceneItem ring;
    SDL_zero(ring);
    ring.kind = SSR_ITEM_IMAGE;
    ring.image = "MapShieldFrameAll";
    ring.w = CREST_W;
    ring.h = CREST_H;
    ring.colour = 0xFFFFFFFF;
    draw_image_item(gfx, &ring);
    SDL_Surface *read = SDL_RenderReadPixels(gfx->renderer, NULL);
    SDL_SetRenderTarget(gfx->renderer, old);
    gfx->offsetX = offsetX;
    gfx->offsetY = offsetY;
    SDL_DestroyTexture(target);
    SDL_Surface *frame = read ? SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32) : NULL;
    SDL_DestroySurface(read);
    if (!frame || frame->w != CREST_W || frame->h != CREST_H) {
        SDL_DestroySurface(frame);
        return;
    }
    unsigned char *fill = SDL_calloc(CREST_W * CREST_H, 1);
    int *stack = SDL_malloc(sizeof(int) * CREST_W * CREST_H);
    int closed = fill && stack;
    const unsigned char *pixels = frame->pixels;
#define CLEAR(x, y) (pixels[(size_t)(y) * frame->pitch + (size_t)(x) * 4 + 3] < 128)
    // The fill starts at the clear pixel nearest the middle on its row.
    int start = -1;
    for (int d = 0; closed && d < CREST_W / 2 && start < 0; d++) {
        if (CLEAR(CREST_W / 2 + d, CREST_H / 2)) start = CREST_H / 2 * CREST_W + CREST_W / 2 + d;
        else if (CLEAR(CREST_W / 2 - d, CREST_H / 2)) start = CREST_H / 2 * CREST_W + CREST_W / 2 - d;
    }
    int top = 0;
    if (start >= 0 && closed) {
        stack[top++] = start;
        fill[start] = 1;
    }
    double n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    while (top > 0 && closed) {
        int k = stack[--top], x = k % CREST_W, y = k / CREST_W;
        // A fill that comes to the edge of the box is not in a ring.
        if (x == 0 || y == 0 || x == CREST_W - 1 || y == CREST_H - 1) closed = 0;
        n++;
        sx += x;
        sy += y;
        sxx += (double)x * x;
        syy += (double)y * y;
        sxy += (double)x * y;
        const int next[4] = {k - 1, k + 1, k - CREST_W, k + CREST_W};
        for (int i = 0; i < 4 && closed; i++) {
            int nx = next[i] % CREST_W, ny = next[i] / CREST_W;
            if (!fill[next[i]] && CLEAR(nx, ny)) {
                fill[next[i]] = 1;
                stack[top++] = next[i];
            }
        }
    }
#undef CLEAR
    SDL_free(stack);
    SDL_DestroySurface(frame);
    if (!closed || n < 1000) {
        SDL_free(fill);
        return;
    }
    // The ellipse with the same moments as the hole. A full ellipse with
    // the covariance C is d * inverse(4 C) * d = 1. Its map to the unit
    // circle that keeps the rows level comes from that matrix M:
    // width = 1 / sqrt(M11), lean = -M12 / M11, and
    // height = 1 / sqrt(M22 - M12 * M12 / M11).
    double mx = sx / n, my = sy / n;
    double cxx = 4 * (sxx / n - mx * mx), cyy = 4 * (syy / n - my * my), cxy = 4 * (sxy / n - mx * my);
    double det = cxx * cyy - cxy * cxy;
    if (det <= 0) {
        SDL_free(fill);
        return;
    }
    double m11 = cyy / det, m22 = cxx / det, m12 = -cxy / det;
    gfx->holeX = (float)mx;
    gfx->holeY = (float)my;
    gfx->holeWidth = (float)(1 / SDL_sqrt(m11));
    gfx->holeLean = (float)(-m12 / m11);
    gfx->holeHeight = (float)(1 / SDL_sqrt(m22 - m12 * m12 / m11));
    // The hole grows by 4 pixels under the ring.
    gfx->hole = SDL_calloc(CREST_W * CREST_H, 1);
    if (!gfx->hole) {
        SDL_free(fill);
        return;
    }
    for (int y = 0; y < CREST_H; y++) {
        for (int x = 0; x < CREST_W; x++) {
            int in = 0;
            for (int dy = -4; dy <= 4 && !in; dy++) {
                for (int dx = -4; dx <= 4 && !in; dx++) {
                    int px = x + dx, py = y + dy;
                    in = px >= 0 && py >= 0 && px < CREST_W && py < CREST_H && dx * dx + dy * dy <= 16 &&
                         fill[py * CREST_W + px];
                }
            }
            gfx->hole[y * CREST_W + x] = (unsigned char)in;
        }
    }
    SDL_free(fill);
    gfx->holeState = 1;
}

// The crest of a club in the pack: "Crest_<id>", or "CrestM_<id>" for the
// flipped crest of a flipped shield. The image of crests/<id>.png fills the
// hole in the ring of the shield. It lies on the ellipse of the hole as on
// a dome: the middle of the crest comes near, its edge bends away, and the
// light falls from the top. Returns NULL when the image is not a crest, is
// not there, or the ring has no hole.
static SDL_Texture *crest_texture(SSRGfx *gfx, const char *name) {
    int flip = SDL_strncmp(name, "CrestM_", 7) == 0;
    if (!flip && SDL_strncmp(name, "Crest_", 6) != 0) return NULL;
    if (!gfx->packDirectory[0]) return NULL;
    for (int i = 0; i < gfx->textureCount; i++) {
        if (SDL_strcmp(gfx->textures[i].name, name) == 0) return gfx->textures[i].texture;
    }
    if (gfx->holeState == 0) measure_hole(gfx);
    char path[2400];
    SDL_snprintf(path, sizeof(path), "%s/crests/%d.png", gfx->packDirectory, SDL_atoi(name + (flip ? 7 : 6)));
    SSRImage source;
    SDL_Texture *texture = NULL;
    unsigned char *pixels = gfx->holeState == 1 && load_own_image(path, &source) ? SDL_calloc(CREST_W * CREST_H, 4)
                                                                                  : NULL;
    if (pixels) {
        const float side = (float)(source.width < source.height ? source.width : source.height);
        for (int y = 0; y < CREST_H; y++) {
            for (int x = 0; x < CREST_W; x++) {
                if (!gfx->hole[y * CREST_W + x]) continue;
                unsigned char *to = pixels + ((size_t)y * CREST_W + x) * 4;
                // u and v go from -1 to 1 across the ellipse of the hole.
                float dx = x + 0.5f - gfx->holeX, dy = y + 0.5f - gfx->holeY;
                float u = (dx - gfx->holeLean * dy) / gfx->holeWidth, v = dy / gfx->holeHeight;
                float r = SDL_sqrtf(u * u + v * v), rc = SDL_min(r, 1.0f);
                // On a dome, the point at r on the screen is at the angle
                // asin(r) from the front, so the crest bends at the edge.
                float bend = r > 0 ? SDL_asinf(rc) / (SDL_PI_F / 2) / r : 1;
                float su = u * bend, sv = v * bend;
                int sx = (int)(source.width / 2.0f + (flip ? -su : su) * side / 2);
                int sy = (int)(source.height / 2.0f + sv * side / 2);
                sx = SDL_clamp(sx, 0, source.width - 1);
                sy = SDL_clamp(sy, 0, source.height - 1);
                const unsigned char *from = source.pixels + ((size_t)sy * source.width + sx) * 4;
                // The crest lies on white. The light falls from the top left
                // onto the dome.
                float nz = SDL_sqrtf(SDL_max(0.0f, 1 - rc * rc));
                float light = SDL_clamp(0.72f + 0.28f * (0.8f * nz - 0.35f * v - 0.2f * u), 0.0f, 1.0f);
                float alpha = from[3] / 255.0f;
                for (int c = 0; c < 3; c++) to[c] = (unsigned char)((from[c] * alpha + 255 * (1 - alpha)) * light + 0.5f);
                to[3] = 255;
            }
        }
        texture = texture_from_pixels(gfx->renderer, CREST_W, CREST_H, pixels);
        SDL_free(pixels);
        ssr_image_free(&source);
    }
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
    SDL_SetTextureColorMod(atlas, 255, 255, 255);
}

// The texture of a PNG page, or of the frame of a RLE page that holds the
// image.
// The game keeps its textures with premultiplied alpha. A blend that does
// not use the alpha (Gloss) needs the colours that way too.
static void premultiply(SSRImage *pixels) {
    size_t count = (size_t)pixels->width * (size_t)pixels->height;
    for (size_t i = 0; i < count; i++) {
        unsigned char *p = pixels->pixels + i * 4;
        for (int c = 0; c < 3; c++) p[c] = (unsigned char)((p[c] * p[3] + 127) / 255);
    }
}

// The patches of the remake (art/<name>.patch.png) change only some pixels
// of an atlas image. They go on the PNG page before it goes to the GPU.
static void patch_page(SSRGfx *gfx, int page, SSRImage *pixels) {
    static const char SUFFIX[] = ".patch.png";
    const size_t suffix = sizeof(SUFFIX) - 1;
    // The patches of the remake first, then the patches of the pack.
    for (int i = 0; i < gfx->artCount + gfx->packArtCount; i++) {
        int fromPack = i >= gfx->artCount;
        const char *file = fromPack ? gfx->packArt[i - gfx->artCount] : gfx->art[i];
        size_t length = SDL_strlen(file);
        if (length <= suffix || SDL_strcmp(file + length - suffix, SUFFIX) != 0) continue;
        char name[256];
        SDL_snprintf(name, sizeof(name), "%.*s", (int)(length - suffix), file);
        const SSRAtlasImage *image = ssr_atlas_find(gfx->atlases, name);
        if (!image || image->page != page) continue;
        char path[2400];
        const char *base = SDL_GetBasePath();
        if (fromPack) SDL_snprintf(path, sizeof(path), "%s/art/%s", gfx->packDirectory, file);
        else SDL_snprintf(path, sizeof(path), "%sart/%s", base ? base : "", file);
        SSRImage patch;
        if (!load_own_image(path, &patch)) continue;
        if (!ssr_atlas_patch(pixels, image, &patch)) SDL_Log("The patch %s does not fit its image.", file);
        ssr_image_free(&patch);
    }
}

// The texture of a PNG page, or of the frame of a RLE page that holds the
// image. With premultiplied, the copy with premultiplied alpha.
static SDL_Texture *atlas_texture_kind(SSRGfx *gfx, const SSRAtlasImage *image, int premultiplied) {
    const SSRAtlasPage *page = ssr_atlas_page(gfx->atlases, image->page);
    if (!page) return NULL;
    SSRImage pixels;
    if (page->rle) {
        int index = page->firstImage + image->frame;
        SDL_Texture **slot = premultiplied ? &gfx->glossFrames[index] : &gfx->frameTextures[index];
        if (*slot) return *slot;
        if (!ssr_atlas_frame_pixels(gfx->atlases, image->page, image->frame, &pixels)) return NULL;
        if (premultiplied) premultiply(&pixels);
        *slot = texture_from_pixels(gfx->renderer, pixels.width, pixels.height, pixels.pixels);
        ssr_image_free(&pixels);
        return *slot;
    }
    SDL_Texture **slot = premultiplied ? &gfx->glossPages[image->page] : &gfx->pageTextures[image->page];
    if (*slot) return *slot;
    if (!ssr_atlas_page_pixels(gfx->atlases, image->page, &pixels)) return NULL;
    patch_page(gfx, image->page, &pixels);
    if (premultiplied) premultiply(&pixels);
    *slot = texture_from_pixels(gfx->renderer, pixels.width, pixels.height, pixels.pixels);
    ssr_image_free(&pixels);
    return *slot;
}

static SDL_Texture *atlas_texture_for(SSRGfx *gfx, const SSRAtlasImage *image) {
    return atlas_texture_kind(gfx, image, 0);
}

static SDL_Texture *own_art(SSRGfx *gfx, const char *name);

// The atlases hold the images by the names of the Img: lines. An image of
// the remake that is not in the atlases has the size of its file. The file of
// an Img: line is only there when the atlases do not have the name.
int ssr_gfx_image_size(SSRGfx *gfx, const char *name, const char *file, float *w, float *h) {
    if (!gfx) return 0;
    const SSRAtlasImage *image = ssr_atlas_find(gfx->atlases, name);
    if (image) {
        *w = (float)image->originalWidth;
        *h = (float)image->originalHeight;
        return 1;
    }
    SDL_Texture *texture = own_art(gfx, name);
    if (texture) return SDL_GetTextureSize(texture, w, h);
    texture = file ? ssr_gfx_texture(gfx, file) : NULL;
    return texture && SDL_GetTextureSize(texture, w, h);
}

typedef struct {
    const SSRSceneItem *item;
    SDL_FColor colour;
    // 1 for the premultiplied pages of Gloss().
    int premultiplied;
    float cosA, sinA;
    float offsetX, offsetY;
    // The fade of the alpha (SSRSceneItem.fade) from the y0 to the y1 of
    // the drawn rect, in parts of the box.
    float fade, fadeY0, fadeY1;
} Placement;

// A point of the box (as parts of the box) on the screen, after the
// rotation about the pivot.
static SDL_FPoint place(const Placement *p, float x, float y) {
    const SSRSceneItem *item = p->item;
    if (item->projected) {
        // A point of the plane of a sheet: the corners mix in the space of
        // the camera, then the perspective divides by z (c_GelSheet).
        const float *c = item->camera;
        float w[4] = {(1 - x) * (1 - y), x * (1 - y), x * y, (1 - x) * y};
        float X = 0, Y = 0, Z = 0;
        for (int k = 0; k < 4; k++) {
            X += w[k] * c[k * 3];
            Y += w[k] * c[k * 3 + 1];
            Z += w[k] * c[k * 3 + 2];
        }
        if (Z < 1) Z = 1;
        SDL_FPoint out = {320 + item->focal * X / Z + p->offsetX, 480 + item->focal * Y / Z + p->offsetY};
        return out;
    }
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
    if (p->fade > 0 && p->fadeY1 != p->fadeY0) {
        float t = (y - p->fadeY0) / (p->fadeY1 - p->fadeY0);
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        out.color.a *= 1 + (p->fade - 1) * t;
    }
    out.tex_coord.x = u;
    out.tex_coord.y = v;
    return out;
}

static void draw_quad(SSRGfx *gfx, SDL_Texture *texture, const Placement *p, float x0, float y0, float x1, float y1,
                      float u0, float v0, float u1, float v1) {
    if (p->item->projected) {
        // The renderer maps the texture in a straight line between the
        // points. A grid of 8 x 8 cells keeps the perspective of the texture
        // near the game, which maps it with perspective.
        enum { CELLS = 8 };
        SDL_Vertex grid[(CELLS + 1) * (CELLS + 1)];
        int indices[CELLS * CELLS * 6], count = 0;
        for (int j = 0; j <= CELLS; j++) {
            for (int i = 0; i <= CELLS; i++) {
                float a = (float)i / CELLS, b = (float)j / CELLS;
                grid[j * (CELLS + 1) + i] = vertex(p, x0 + (x1 - x0) * a, y0 + (y1 - y0) * b, u0 + (u1 - u0) * a,
                                                   v0 + (v1 - v0) * b);
            }
        }
        for (int j = 0; j < CELLS; j++) {
            for (int i = 0; i < CELLS; i++) {
                int k = j * (CELLS + 1) + i;
                int quad[6] = {k, k + 1, k + CELLS + 2, k, k + CELLS + 2, k + CELLS + 1};
                for (int q = 0; q < 6; q++) indices[count++] = quad[q];
            }
        }
        SDL_RenderGeometry(gfx->renderer, texture, grid, (CELLS + 1) * (CELLS + 1), indices, count);
        return;
    }
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
    SDL_Texture *texture = atlas_texture_kind(gfx, image, p->premultiplied);
    if (!texture) return;
    const SSRAtlasPage *page = ssr_atlas_page(gfx->atlases, image->page);
    const SSRSceneItem *item = p->item;
    if (image->kind == SSR_ATLAS_PLAIN) {
        const float *r = image->rect;
        float u0 = r[2], v0 = r[3], u1 = r[6], v1 = r[7];
        if (page->rle) {
            // The frame has a texture of its own, so the coordinates are
            // the whole texture. The key can give the box from the bottom
            // up (y0 > y1), so the top of the frame goes to the top of the
            // box.
            u0 = r[0] <= r[4] ? 0.0f : 1.0f;
            v0 = r[1] <= r[5] ? 0.0f : 1.0f;
            u1 = 1 - u0;
            v1 = 1 - v0;
        }
        Placement faded = *p;
        faded.fadeY0 = r[1];
        faded.fadeY1 = r[5];
        draw_quad(gfx, texture, &faded, r[0], r[1], r[4], r[5], u0, v0, u1, v1);
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

// The colour of the cel that CloneTweak gave the item ("Cel=RRGGBBAA;").
// Returns 0 when the item has no colour for the cel.
static int item_cel(const SSRSceneItem *item, const char *cel, char *out, size_t capacity) {
    size_t length = SDL_strlen(cel);
    for (const char *p = item->cels; p && *p;) {
        const char *end = SDL_strchr(p, ';');
        if (!end) break;
        if (SDL_strncmp(p, cel, length) == 0 && p[length] == '=') {
            SDL_snprintf(out, capacity, "%.*s", (int)(end - p - length - 1), p + length + 1);
            return 1;
        }
        p = end + 1;
    }
    return 0;
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
    p.fade = item->fade;
    p.fadeY0 = 0;
    p.fadeY1 = 1;

    SDL_Texture *texture = crest_texture(gfx, item->image);
    const SSRAtlasImage *image = texture ? NULL : ssr_atlas_find(gfx->atlases, item->image);
    if (!texture) texture = own_art(gfx, item->image);
    if (texture) image = NULL;
    const char *file = image || texture || !gfx->scene ? NULL : ssr_scene_image_file(gfx->scene, item->image);
    if (file) texture = ssr_gfx_texture(gfx, file);
    if (!texture && !image) return;
    SDL_BlendMode mode = SDL_BLENDMODE_BLEND;
    if (item->blend == SSR_BLEND_ADD) mode = SDL_BLENDMODE_ADD;
    if (item->blend == SSR_BLEND_COPY) mode = SDL_BLENDMODE_NONE;
    // Gloss() adds the image times the colour under it (DST_COLOR, ONE).
    // The game premultiplies the alpha, so the image and its colour carry
    // it.
    p.premultiplied = 0;
    if (item->blend == SSR_BLEND_GLOSS) {
        mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                          SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        p.premultiplied = image != NULL;
        p.colour.r *= p.colour.a;
        p.colour.g *= p.colour.a;
        p.colour.b *= p.colour.a;
    }
    if (texture) {
        SDL_SetTextureBlendMode(texture, mode);
        draw_quad(gfx, texture, &p, 0, 0, 1, 1, 0, 0, 1, 1);
        return;
    }
    SDL_Texture *page = atlas_texture_kind(gfx, image, p.premultiplied);
    if (page) SDL_SetTextureBlendMode(page, mode);
    draw_atlas_image(gfx, image, &p);
    // A shadow draws the image alone (c_GImage.Render2).
    if (item->fade > 0) return;
    // The cels of the image draw over it, each in the colour of its tweak
    // CelColours:cel on the colour of the item (c_GImage).
    const char *cels[16];
    int celCount = ssr_atlas_cels(gfx->atlases, item->image, cels, 16);
    for (int i = 0; i < celCount && i < 16; i++) {
        char name[256];
        SDL_snprintf(name, sizeof(name), "%s_%s", cels[i], item->image);
        const SSRAtlasImage *cel = ssr_atlas_find(gfx->atlases, name);
        if (!cel) continue;
        const char *tweak = gfx->tweaks ? ssr_tweaks_get(gfx->tweaks, "CelColours", cels[i]) : "";
        char own[24];
        if (item_cel(item, cels[i], own, sizeof(own))) tweak = own;
        char text[24];
        SDL_snprintf(text, sizeof(text), "#%s", tweak[0] == '#' ? tweak + 1 : tweak);
        uint32_t colour = ssr_scene_mix_colour(text, item->colour);
        p.colour.r = ((colour >> 24) & 0xFF) / 255.0f;
        p.colour.g = ((colour >> 16) & 0xFF) / 255.0f;
        p.colour.b = ((colour >> 8) & 0xFF) / 255.0f;
        p.colour.a = (colour & 0xFF) / 255.0f;
        SDL_Texture *celPage = atlas_texture_for(gfx, cel);
        if (celPage) SDL_SetTextureBlendMode(celPage, mode);
        draw_atlas_image(gfx, cel, &p);
    }
}

void ssr_gfx_set_tweaks(SSRGfx *gfx, const SSRTweaks *tweaks) {
    if (gfx) gfx->tweaks = tweaks;
}

// Draws one line of text with its top left at (x, y), scaled. The glyphs
// lean by italic: x += (y - baseline) * italic, as c_FontRenderer does.
static void draw_line(SSRGfx *gfx, FontSlot *slot, const char *text, float x, float y, float scaleX, float scaleY,
                      uint32_t colour, float italic, int shadow) {
    static SSRGlyphQuad quads[MAX_QUADS];
    int count = ssr_font_layout(slot->font, text, 0, 0, quads, MAX_QUADS);
    if (count > MAX_QUADS) count = MAX_QUADS;
    SDL_Texture *atlas = atlas_texture(gfx, slot);
    if (!atlas) return;
    SDL_FColor c = {((colour >> 24) & 0xFF) / 255.0f, ((colour >> 16) & 0xFF) / 255.0f,
                    ((colour >> 8) & 0xFF) / 255.0f, (colour & 0xFF) / 255.0f};
    float baseline = ssr_font_ascent(slot->font);
    for (int i = 0; i < count; i++) {
        const SSRGlyphQuad *q = &quads[i];
        SDL_Texture *texture = atlas;
        SDL_FColor tint = c;
        if (q->icon) {
            // An icon keeps its own colours. Its shadow takes the colour of
            // the shadow.
            texture = q->icon <= 8 ? slot->icons[q->icon - 1] : NULL;
            if (!texture) continue;
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            if (!shadow) tint.r = tint.g = tint.b = 1;
        }
        float xs[4] = {q->x0, q->x1, q->x1, q->x0}, ys[4] = {q->y0, q->y0, q->y1, q->y1};
        float us[4] = {q->u0, q->u1, q->u1, q->u0}, vs[4] = {q->v0, q->v0, q->v1, q->v1};
        SDL_Vertex v[4];
        for (int k = 0; k < 4; k++) {
            v[k].position.x = x + (xs[k] + (ys[k] - baseline) * italic) * scaleX;
            v[k].position.y = y + ys[k] * scaleY;
            v[k].color = tint;
            v[k].tex_coord.x = us[k];
            v[k].tex_coord.y = vs[k];
        }
        static const int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(gfx->renderer, texture, v, 4, indices, 6);
    }
}

#define MAX_LINES 32

// Cuts the text into lines at '\n' and at '|', the line break of the texts
// of the game, and at spaces when a line is wider than width. Each line goes into lines as a new buffer of the text.
static int wrap_lines(FontSlot *slot, const char *text, float width, char lines[MAX_LINES][256]) {
    int count = 0;
    const char *p = text;
    while (*p && count < MAX_LINES) {
        char line[256] = "";
        size_t used = 0;
        const char *lastBreak = NULL;
        size_t lastBreakUsed = 0;
        while (*p && *p != '\n' && *p != '|' && used + 1 < sizeof(line)) {
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
        if (*p == '\n' || *p == '|') p++;
        SDL_strlcpy(lines[count++], line, 256);
    }
    return count;
}

// A line that is wider than its box moves from side to side in it, at the
// time of the game (c_GelText.DoBounceScroll): 300 units each 6 seconds.
static float bounce_scroll(int gameMS, float overflow) {
    const float distance = 300, period = 6000;
    float at = (float)(gameMS % (int)period) * distance * 2 / period;
    if (at > distance) at = distance * 2 - at;
    at -= distance * 0.5f;
    float half = overflow * 0.5f < 20 ? 20 : overflow * 0.5f;
    // A long line moves further, so that all of it shows.
    if (half > 120) at = at * half / 120;
    return at;
}

// Draws a text as c_GelTextFR and c_GelTextBoxFR do. The font gives the
// line height and the distances from the box to the baseline. A TextBox
// cuts its text into lines that fit the width of the box.
static void draw_text_item(SSRGfx *gfx, const SSRSceneItem *item) {
    if (!item->text || !item->text[0]) return;
    FontSlot *slot = &gfx->fonts[item->font];
    if (!slot->font) return;
    const SSRFontSlot *config = &slot->config;
    float scaleX = item->fontScaleX > 0 ? item->fontScaleX : 1;
    float scaleY = item->fontScaleY > 0 ? item->fontScaleY : 1;
    char lines[MAX_LINES][256];
    int count = wrap_lines(slot, item->text, item->wrap && item->w > 0 ? item->w / scaleX : 0, lines);
    // The height of a line of the font, in units of the font.
    float fontHeight = ssr_font_line_height(slot->font);
    float lineHeight = fontHeight * scaleY;
    float ascent = ssr_font_ascent(slot->font) * scaleY;
    float rightLean = lineHeight * 0.5f * item->italic;
    float lean = item->italic * lineHeight * 0.2f;
    // LineSpacing() of a TextBox moves the lines apart. The block of lines
    // keeps its middle, or its bottom (c_GelTextBoxFR).
    float spacing = item->wrap && item->lineSpacing > 0 ? item->lineSpacing : 1;
    float extra = (float)(count - 1) * (spacing - 1) * fontHeight;
    for (int i = 0; i < count; i++) {
        // The baseline of the line, from the top of the box.
        float below = (float)i * fontHeight * spacing;
        float baseline;
        if (item->alignY == 'T') {
            baseline = scaleY * (below + config->padTop);
        } else if (item->alignY == 'B') {
            baseline = item->h + scaleY * (below - (float)count * fontHeight - extra - config->padBottom);
        } else if (item->wrap) {
            // c_GelTextBoxFR: the lines start half of their height above
            // the middle of the box. The middle padding then gives the
            // baseline. The game divides the padding by the scale before
            // it scales the lines, so the padding keeps its full size.
            baseline = item->h * 0.5f + scaleY * (below - (float)count * fontHeight * 0.5f - extra * 0.5f) +
                       config->padMiddle;
        } else {
            baseline = (item->h - lineHeight) * 0.5f + scaleY * config->padMiddle;
        }
        float width = ssr_font_measure(slot->font, lines[i]) * scaleX;
        float x;
        int scroll = !item->wrap && item->w > 0 && width > item->w - item->padding * 2;
        if (scroll) {
            float centre = (item->w - width) / 2 + bounce_scroll(gfx->gameMS, width - (item->w - item->padding * 2));
            float low = item->w - item->padding + rightLean - width, high = item->padding;
            x = centre < low ? low : centre > high ? high : centre;
        } else if (item->alignX == 'L') {
            x = item->padding + config->padLeft;
        } else if (item->alignX == 'R') {
            x = item->w - (item->padding + config->padRight) + rightLean - width;
        } else {
            x = (item->w - width) / 2;
        }
        x += item->x + lean;
        SDL_Rect outer;
        int clipped = SDL_RenderClipEnabled(gfx->renderer);
        if (clipped) SDL_GetRenderClipRect(gfx->renderer, &outer);
        if (scroll) {
            // The line shows only inside the padding of its box.
            float left = item->x + item->padding + gfx->offsetX;
            float right = item->x + item->w + rightLean - item->padding + gfx->offsetX;
            SDL_Rect rect = {(int)left, -100000, (int)(right - left), 200000};
            if (clipped) SDL_GetRectIntersection(&rect, &outer, &rect);
            SDL_SetRenderClipRect(gfx->renderer, &rect);
        }
        // draw_line takes the top of the line. The shadow draws first, in
        // units of the font (c_CachedTextPacketFR).
        float left = x + gfx->offsetX, top = item->y + baseline - ascent + gfx->offsetY;
        if (item->shadow) {
            draw_line(gfx, slot, lines[i], left + item->shadowX * scaleX, top + item->shadowY * scaleY, scaleX, scaleY,
                      item->shadowColour, item->italic, 1);
        }
        draw_line(gfx, slot, lines[i], left, top, scaleX, scaleY, item->colour, item->italic, 0);
        if (scroll) SDL_SetRenderClipRect(gfx->renderer, clipped ? &outer : NULL);
    }
}

void ssr_gfx_set_pack(SSRGfx *gfx, const char *directory) {
    if (!gfx) return;
    SDL_free(gfx->packArt);
    gfx->packArt = NULL;
    gfx->packArtCount = 0;
    SDL_strlcpy(gfx->packDirectory, directory ? directory : "", sizeof(gfx->packDirectory));
    if (!directory || !directory[0]) return;
    char path[2100];
    SDL_snprintf(path, sizeof(path), "%s/art", directory);
    gfx->packArt = SDL_GlobDirectory(path, "*.png", 0, &gfx->packArtCount);
}

void ssr_gfx_set_offset(SSRGfx *gfx, float x, float y) {
    if (!gfx) return;
    gfx->offsetX = x;
    gfx->offsetY = y;
}

void ssr_gfx_attach(SSRGfx *gfx, const char *ref, const SSRScene *scene) {
    if (!gfx || !ref) return;
    for (int i = 0; i < MAX_HOOKS; i++) {
        if (!scene && gfx->hooks[i].scene && SDL_strcmp(gfx->hooks[i].ref, ref) == 0) gfx->hooks[i].scene = NULL;
    }
    if (!scene) return;
    for (int i = 0; i < MAX_HOOKS; i++) {
        if (gfx->hooks[i].scene) continue;
        SDL_strlcpy(gfx->hooks[i].ref, ref, sizeof(gfx->hooks[i].ref));
        gfx->hooks[i].scene = scene;
        return;
    }
}

// The scene of the hook draws at the place of the area, inside its clip.
static void draw_hook(SSRGfx *gfx, const SSRSceneItem *area, const SSRScene *sub) {
    const SSRScene *scene = gfx->scene;
    int gameMS = gfx->gameMS, hasOuter = gfx->hasOuter;
    SDL_Rect outer = gfx->outer;
    float x = gfx->offsetX, y = gfx->offsetY;
    if (area->clipped) {
        SDL_Rect rect = {(int)(area->clipX + x), (int)(area->clipY + y), (int)area->clipW, (int)area->clipH};
        if (gfx->hasOuter) SDL_GetRectIntersection(&rect, &gfx->outer, &rect);
        gfx->outer = rect;
        gfx->hasOuter = 1;
    }
    gfx->offsetX = x + area->x;
    gfx->offsetY = y + area->y;
    ssr_gfx_draw_scene(gfx, sub);
    gfx->offsetX = x;
    gfx->offsetY = y;
    gfx->hasOuter = hasOuter;
    gfx->outer = outer;
    gfx->scene = scene;
    gfx->gameMS = gameMS;
}

void ssr_gfx_draw_scene(SSRGfx *gfx, const SSRScene *scene) {
    if (!gfx || !scene) return;
    gfx->scene = scene;
    gfx->gameMS = ssr_scene_time(scene);
    // The clip changes only when the next item has another clip. A scene
    // in a hook stays inside the clip of the hook.
    int clipped = -1;
    SDL_Rect clip = {0, 0, 0, 0};
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (item->kind == SSR_ITEM_AREA) {
            // The scenes of the hook draw in the order that they came.
            for (int h = 0; item->ref && h < MAX_HOOKS; h++) {
                const SSRScene *sub = gfx->hooks[h].scene;
                if (!sub || sub == scene || SDL_strcmp(gfx->hooks[h].ref, item->ref) != 0) continue;
                draw_hook(gfx, item, sub);
                clipped = -1;
            }
            continue;
        }
        SDL_Rect rect = {(int)(item->clipX + gfx->offsetX), (int)(item->clipY + gfx->offsetY), (int)item->clipW,
                         (int)item->clipH};
        int on = item->clipped || gfx->hasOuter;
        if (gfx->hasOuter) {
            if (item->clipped) SDL_GetRectIntersection(&rect, &gfx->outer, &rect);
            else rect = gfx->outer;
        }
        if (on != clipped || (on && SDL_memcmp(&rect, &clip, sizeof(rect)) != 0)) {
            clipped = on;
            clip = rect;
            SDL_SetRenderClipRect(gfx->renderer, on ? &clip : NULL);
        }
        if (item->kind == SSR_ITEM_IMAGE) {
            draw_image_item(gfx, item);
        } else if (item->kind == SSR_ITEM_TEXT) {
            draw_text_item(gfx, item);
        }
    }
    SDL_SetRenderClipRect(gfx->renderer, gfx->hasOuter ? &gfx->outer : NULL);
    gfx->scene = NULL;
}
