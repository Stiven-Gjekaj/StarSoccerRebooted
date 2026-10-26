// ssr_gfx: draws the images and the text of the import with SDL.

#ifndef SSR_GFX_H
#define SSR_GFX_H

#include <SDL3/SDL.h>
#include <stdint.h>

#include "ssr_scene.h"

// The four sizes of the "new" font set of the game (Fonts/Fonts.txt).
typedef enum {
    SSR_FONT_SMALL,
    SSR_FONT_MEDIUM,
    SSR_FONT_LARGE,
    SSR_FONT_TINY,
    SSR_FONT_COUNT,
} SSRFontSize;

typedef enum {
    SSR_ALIGN_LEFT,
    SSR_ALIGN_CENTER,
    SSR_ALIGN_RIGHT,
} SSRAlign;

typedef struct SSRGfx SSRGfx;

// Opens the fonts and the image atlases of the import in gameDirectory. A
// font or an atlas that is not there draws nothing.
SSRGfx *ssr_gfx_create(SDL_Renderer *renderer, const char *gameDirectory);
void ssr_gfx_free(SSRGfx *gfx);

// The texture of an image of the import, for example
// "Images2x/NSS_Watermark.png". The gfx keeps it. Returns NULL when the
// image is not there.
SDL_Texture *ssr_gfx_texture(SSRGfx *gfx, const char *name);

// Draws the text. x is the left, the centre, or the right of the text, as
// align tells. y is the top of the first line. color is 0xRRGGBB.
void ssr_gfx_text(SSRGfx *gfx, SSRFontSize size, const char *text, float x, float y, uint32_t color,
                  SSRAlign align);
float ssr_gfx_text_width(SSRGfx *gfx, SSRFontSize size, const char *text);
float ssr_gfx_line_height(SSRGfx *gfx, SSRFontSize size);

// The size of an image: of the file of the import when file is not NULL,
// else of the image of the atlases with the name. Returns 0 when the image
// is not there.
int ssr_gfx_image_size(SSRGfx *gfx, const char *name, const char *file, float *w, float *h);

// Moves everything that the gfx draws by (x, y), from the units of the
// layouts to the units of the window.
void ssr_gfx_set_offset(SSRGfx *gfx, float x, float y);

// Draws each item of the scene.
void ssr_gfx_draw_scene(SSRGfx *gfx, const SSRScene *scene);

#endif
