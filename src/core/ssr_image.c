#include "ssr_image.h"

#include <limits.h>
#include <string.h>

// The game has only PNG and JPG images.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int keep(unsigned char *pixels, int width, int height, SSRImage *image) {
    if (!pixels) return 0;
    image->width = width;
    image->height = height;
    image->pixels = pixels;
    return 1;
}

int ssr_image_decode(const void *data, size_t size, SSRImage *image) {
    memset(image, 0, sizeof(*image));
    if (!data || size == 0 || size > INT_MAX) return 0;
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
    return keep(pixels, width, height, image);
}

int ssr_image_load(const char *path, SSRImage *image) {
    memset(image, 0, sizeof(*image));
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 4);
    return keep(pixels, width, height, image);
}

void ssr_image_free(SSRImage *image) {
    if (!image) return;
    stbi_image_free(image->pixels);
    memset(image, 0, sizeof(*image));
}
