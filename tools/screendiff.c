// screendiff: compares a screenshot of the remake with a screenshot of the
// original game, and writes an image of the pixels that differ.
//
//     screendiff reference.png remake.png [heat.png]

#include <stdio.h>

#include "ssr_diff.h"
#include "stb_image_write.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: screendiff reference.png remake.png [heat.png]\n");
        return 2;
    }
    SSRImage reference, image, heat = {0};
    if (!ssr_image_load(argv[1], &reference) || !ssr_image_load(argv[2], &image)) {
        fprintf(stderr, "screendiff: cannot read the images\n");
        return 2;
    }
    SSRDiff diff;
    if (!ssr_diff_images(&reference, &image, &diff, argc > 3 ? &heat : NULL)) {
        fprintf(stderr, "screendiff: an image is empty\n");
        return 2;
    }
    printf("mean %.2f changed %.2f%%\n", diff.mean, diff.changed * 100);
    for (int i = 0; i < SSR_DIFF_BANDS; i++) {
        printf("band %2d (rows %4d-%4d): %5.1f%%\n", i, i * image.height / SSR_DIFF_BANDS,
               (i + 1) * image.height / SSR_DIFF_BANDS - 1, diff.bands[i] * 100);
    }
    if (argc > 3 && !stbi_write_png(argv[3], heat.width, heat.height, 4, heat.pixels, heat.width * 4)) {
        fprintf(stderr, "screendiff: cannot write %s\n", argv[3]);
        return 2;
    }
    ssr_image_free(&heat);
    ssr_image_free(&reference);
    ssr_image_free(&image);
    return 0;
}
