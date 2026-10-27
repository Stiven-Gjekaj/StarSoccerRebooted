#include "ssr_diff.h"

#include <stdlib.h>
#include <string.h>

// The mean of the pixels of the reference under the pixel x, y of a
// width x height image.
static void sample(const SSRImage *reference, int width, int height, int x, int y, int out[3]) {
    double sx = (double)reference->width / width, sy = (double)reference->height / height;
    int x0 = (int)(x * sx), x1 = (int)((x + 1) * sx), y0 = (int)(y * sy), y1 = (int)((y + 1) * sy);
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    if (x1 > reference->width) x1 = reference->width;
    if (y1 > reference->height) y1 = reference->height;
    long sum[3] = {0, 0, 0};
    long count = 0;
    for (int j = y0; j < y1; j++) {
        for (int i = x0; i < x1; i++) {
            const unsigned char *p = reference->pixels + ((size_t)j * reference->width + i) * 4;
            for (int c = 0; c < 3; c++) sum[c] += p[c];
            count++;
        }
    }
    for (int c = 0; c < 3; c++) out[c] = count ? (int)(sum[c] / count) : 0;
}

int ssr_diff_images(const SSRImage *reference, const SSRImage *image, SSRDiff *diff, SSRImage *heat) {
    if (!reference || !image || !diff || !reference->pixels || !image->pixels || reference->width <= 0 ||
        reference->height <= 0 || image->width <= 0 || image->height <= 0) {
        return 0;
    }
    memset(diff, 0, sizeof(*diff));
    int w = image->width, h = image->height;
    if (heat) {
        heat->width = w;
        heat->height = h;
        heat->pixels = malloc((size_t)w * h * 4);
        if (!heat->pixels) return 0;
    }
    long bandPixels[SSR_DIFF_BANDS] = {0}, bandChanged[SSR_DIFF_BANDS] = {0};
    double total = 0;
    long changed = 0;
    for (int y = 0; y < h; y++) {
        int band = y * SSR_DIFF_BANDS / h;
        for (int x = 0; x < w; x++) {
            int ref[3];
            sample(reference, w, h, x, y, ref);
            const unsigned char *p = image->pixels + ((size_t)y * w + x) * 4;
            int widest = 0;
            for (int c = 0; c < 3; c++) {
                int d = abs(p[c] - ref[c]);
                total += d;
                if (d > widest) widest = d;
            }
            int differs = widest > SSR_DIFF_THRESHOLD;
            changed += differs;
            bandPixels[band]++;
            bandChanged[band] += differs;
            if (heat) {
                unsigned char *q = heat->pixels + ((size_t)y * w + x) * 4;
                if (differs) {
                    q[0] = 255;
                    q[1] = q[2] = 0;
                } else {
                    for (int c = 0; c < 3; c++) q[c] = (unsigned char)(p[c] / 3);
                }
                q[3] = 255;
            }
        }
    }
    diff->mean = total / ((double)w * h * 3);
    diff->changed = (double)changed / ((double)w * h);
    for (int i = 0; i < SSR_DIFF_BANDS; i++) diff->bands[i] = bandPixels[i] ? (double)bandChanged[i] / bandPixels[i] : 0;
    return 1;
}
