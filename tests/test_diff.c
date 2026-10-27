// Tests ssr_diff with images that the test makes.

#include <stdlib.h>
#include <string.h>

#include "ssr_diff.h"
#include "ssr_test.h"

static SSRImage solid(int width, int height, unsigned char r, unsigned char g, unsigned char b) {
    SSRImage image = {width, height, malloc((size_t)width * height * 4)};
    for (int i = 0; i < width * height; i++) {
        unsigned char *p = image.pixels + i * 4;
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = 255;
    }
    return image;
}

static void test_same_images_do_not_differ(void) {
    SSRImage a = solid(40, 40, 10, 200, 30), b = solid(40, 40, 10, 200, 30);
    SSRDiff diff;
    CHECK(ssr_diff_images(&a, &b, &diff, NULL));
    CHECK(diff.mean == 0 && diff.changed == 0 && diff.bands[0] == 0);
    ssr_image_free(&a);
    ssr_image_free(&b);
}

static void test_scales_the_reference(void) {
    // The reference has 3 times the pixels on each side. A 3x3 block of
    // one 0 and eight 45 has a mean of 40, the same as one pixel of 40.
    SSRImage reference = solid(60, 60, 0, 0, 0), image = solid(20, 20, 40, 40, 40);
    for (int i = 0; i < 60 * 60; i++) {
        int x = i % 60, y = i / 60;
        unsigned char v = (x % 3 == 0 && y % 3 == 0) ? 0 : 45;
        memset(reference.pixels + i * 4, v, 3);
    }
    SSRDiff diff;
    CHECK(ssr_diff_images(&reference, &image, &diff, NULL));
    CHECK(diff.changed == 0 && diff.mean == 0);
    ssr_image_free(&reference);
    ssr_image_free(&image);
}

static void test_finds_the_band_that_differs(void) {
    SSRImage reference = solid(20, 40, 0, 0, 0), image = solid(20, 40, 0, 0, 0);
    // The last 2 rows of 40 are white: they are the last band of 20.
    memset(image.pixels + 38 * 20 * 4, 255, 2 * 20 * 4);
    // A small change is not a difference.
    image.pixels[0] = SSR_DIFF_THRESHOLD;
    SSRDiff diff;
    SSRImage heat = {0};
    CHECK(ssr_diff_images(&reference, &image, &diff, &heat));
    CHECK(diff.changed == 40.0 / 800 && diff.bands[SSR_DIFF_BANDS - 1] == 1 && diff.bands[0] == 0);
    CHECK(heat.width == 20 && heat.height == 40);
    CHECK(heat.pixels && heat.pixels[39 * 20 * 4] == 255 && heat.pixels[39 * 20 * 4 + 1] == 0);
    ssr_image_free(&heat);
    ssr_image_free(&reference);
    ssr_image_free(&image);
}

static void test_refuses_an_empty_image(void) {
    SSRImage empty = {0}, image = solid(2, 2, 0, 0, 0);
    SSRDiff diff;
    CHECK(!ssr_diff_images(&empty, &image, &diff, NULL));
    ssr_image_free(&image);
}

int main(void) {
    test_same_images_do_not_differ();
    test_scales_the_reference();
    test_finds_the_band_that_differs();
    test_refuses_an_empty_image();
    return TEST_RESULT();
}
