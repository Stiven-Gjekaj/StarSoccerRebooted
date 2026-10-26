// Tests ssr_image with images that the test writes.

#include <stdlib.h>
#include <string.h>

#include "ssr_image.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"
#include "stb_image_write.h"

static void append(void *context, void *data, int size) {
    unsigned char **cursor = context;
    memcpy(*cursor, data, (size_t)size);
    *cursor += size;
}

static int near(int a, int b) {
    return abs(a - b) <= 6;
}

static void test_decodes_a_jpg(void) {
    unsigned char rgb[16 * 8 * 3];
    for (int i = 0; i < 16 * 8; i++) {
        rgb[i * 3] = 200;
        rgb[i * 3 + 1] = 40;
        rgb[i * 3 + 2] = 20;
    }
    unsigned char file[8192];
    unsigned char *cursor = file;
    CHECK(stbi_write_jpg_to_func(append, &cursor, 16, 8, 3, rgb, 100));

    SSRImage image;
    CHECK(ssr_image_decode(file, (size_t)(cursor - file), &image));
    CHECK(image.width == 16 && image.height == 8);
    unsigned char *last = image.pixels + (16 * 8 - 1) * 4;
    CHECK(near(last[0], 200) && near(last[1], 40) && near(last[2], 20));
    CHECK(last[3] == 255);
    ssr_image_free(&image);
    CHECK(image.pixels == NULL && image.width == 0);
}

static void test_loads_a_png_with_alpha(void) {
    const unsigned char rgba[2 * 2 * 4] = {
        255, 0, 0, 255, 0, 255, 0, 128,
        0, 0, 255, 0, 255, 255, 255, 255,
    };
    char path[1100];
    snprintf(path, sizeof(path), "%s/tile.png", ssr_test_directory("png"));
    CHECK(stbi_write_png(path, 2, 2, 4, rgba, 2 * 4));

    SSRImage image;
    CHECK(ssr_image_load(path, &image));
    CHECK(image.width == 2 && image.height == 2);
    CHECK(image.pixels && memcmp(image.pixels, rgba, sizeof(rgba)) == 0);
    ssr_image_free(&image);
}

static void test_refuses_data_that_is_not_an_image(void) {
    SSRImage image;
    CHECK(!ssr_image_decode("not an image", 12, &image));
    CHECK(image.pixels == NULL);
    char path[1100];
    snprintf(path, sizeof(path), "%s/missing.png", ssr_test_directory("missing"));
    CHECK(!ssr_image_load(path, &image));
}

int main(void) {
    test_decodes_a_jpg();
    test_loads_a_png_with_alpha();
    test_refuses_data_that_is_not_an_image();
    return TEST_RESULT();
}
