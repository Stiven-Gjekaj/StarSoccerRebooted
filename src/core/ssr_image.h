// ssr_image: decodes the PNG and JPG images of the game into RGBA pixels.

#ifndef SSR_IMAGE_H
#define SSR_IMAGE_H

#include <stddef.h>

typedef struct {
    int width;
    int height;
    // Four bytes for each pixel: red, green, blue, alpha. Rows go from the
    // top to the bottom.
    unsigned char *pixels;
} SSRImage;

// Decodes the image in data. Returns 1 on success. On failure, image is
// empty and the result is 0.
int ssr_image_decode(const void *data, size_t size, SSRImage *image);

// Reads and decodes the image file at path. Returns 1 on success.
int ssr_image_load(const char *path, SSRImage *image);

// Frees the pixels and empties the image.
void ssr_image_free(SSRImage *image);

#endif
