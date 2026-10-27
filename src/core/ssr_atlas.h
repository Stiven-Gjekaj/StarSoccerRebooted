// ssr_atlas: the image atlases of the game (NewGUI/Atlases). A key file
// lists the pages and the images on each page. A page is a PNG file, or a
// "-quant.bin" file of palette RLE frames. Some images of a PNG page come
// from the frames of a RLE page. docs/formats.md describes the files.

#ifndef SSR_ATLAS_H
#define SSR_ATLAS_H

#include "ssr_image.h"

typedef enum {
    SSR_ATLAS_PLAIN,
    SSR_ATLAS_SLICED,
    SSR_ATLAS_STRIPPED,
    SSR_ATLAS_PACKED,
} SSRAtlasKind;

// A frame of a RLE page that goes into a PNG page at (x, y).
typedef struct {
    int sourcePage;
    int sourceFrame;
    int x, y;
    int width, height;
    int keepAlpha;
} SSRAtlasUnpack;

typedef struct {
    const char *name;
    int page;
    int frame;
    int flags;
    SSRAtlasKind kind;
    int originalWidth, originalHeight;
    int trimWidth, trimHeight;
    // SSR_ATLAS_PLAIN: x0, y0, u0, v0, x1, y1, u1, v1. x and y are parts
    // of the drawn size. u and v are parts of the page size, from the top
    // left.
    float rect[8];
    // SSR_ATLAS_SLICED: the size that does not stretch.
    int fixedWidth, fixedHeight;
    // SSR_ATLAS_SLICED: quads of 12 floats (sx0, sy0, ox0, oy0, u0, v0, sx1,
    // sy1, ox1, oy1, u1, v1). SSR_ATLAS_STRIPPED: triangles of 12 floats
    // (x, y, u, v three times).
    float *shapes;
    int shapeCount;
    SSRAtlasUnpack *unpacks;
    int unpackCount;
} SSRAtlasImage;

typedef struct {
    const char *name;
    int width, height;
    int firstImage;
    int imageCount;
    // 1 when the page is a "-quant.bin" file.
    int rle;
} SSRAtlasPage;

typedef struct SSRAtlases SSRAtlases;

// Reads the key files in the directory, in order. The pages of all the
// files share one list. Returns NULL when a key file is not there or is
// damaged.
SSRAtlases *ssr_atlas_load(const char *directory, const char *const *keys, int keyCount);
void ssr_atlas_free(SSRAtlases *atlases);

int ssr_atlas_page_count(const SSRAtlases *atlases);
const SSRAtlasPage *ssr_atlas_page(const SSRAtlases *atlases, int index);
int ssr_atlas_image_count(const SSRAtlases *atlases);
const SSRAtlasImage *ssr_atlas_image(const SSRAtlases *atlases, int index);

// The image with the name, or NULL.
const SSRAtlasImage *ssr_atlas_find(const SSRAtlases *atlases, const char *name);

// The cels of a base image, for example "Seats" for the base "Stand": the
// image "Seats_Stand" draws over the base in the colour of the tweak
// CelColours:Seats. Puts up to max names in names and returns the number.
int ssr_atlas_cels(const SSRAtlases *atlases, const char *base, const char **names, int max);

// The pixels of a PNG page, with the RLE frames that it gets put in.
// Returns 1 on success.
int ssr_atlas_page_pixels(SSRAtlases *atlases, int page, SSRImage *out);

// The pixels of one frame of a RLE page. Returns 1 on success.
int ssr_atlas_frame_pixels(SSRAtlases *atlases, int page, int frame, SSRImage *out);

// Puts a patch on a plain or a stripped image of a PNG page. The patch has
// the size of the box of the image (originalWidth x originalHeight). Each
// pixel of the patch with an alpha above 0 takes the place of the page
// pixel under it. The other page pixels stay. Returns 1 when the image can
// take a patch.
int ssr_atlas_patch(SSRImage *page, const SSRAtlasImage *image, const SSRImage *patch);

#endif
