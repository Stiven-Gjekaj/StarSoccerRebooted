// Tests ssr_atlas with a key file, a RLE page, and a PNG page that the
// test writes.

#include <stdlib.h>
#include <string.h>

#include "ssr_atlas.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"
#include "stb_image_write.h"

typedef struct {
    unsigned char data[4096];
    size_t size;
} Writer;

static void pad(Writer *w, size_t to) {
    while (w->size % to) w->data[w->size++] = 0;
}

static void put_int(Writer *w, int value) {
    pad(w, 4);
    for (int i = 0; i < 4; i++) w->data[w->size++] = (unsigned char)((unsigned)value >> (8 * i));
}

static void put_short(Writer *w, int value) {
    pad(w, 2);
    w->data[w->size++] = (unsigned char)(value & 0xFF);
    w->data[w->size++] = (unsigned char)((value >> 8) & 0xFF);
}

static void put_float(Writer *w, float value) {
    int bits;
    memcpy(&bits, &value, 4);
    put_int(w, bits);
}

static void put_string(Writer *w, const char *text) {
    size_t length = strlen(text);
    w->data[w->size++] = (unsigned char)length;
    memcpy(w->data + w->size, text, length);
    w->size += length;
    pad(w, 4);
}

static void write_bytes(const char *directory, const char *name, const void *data, size_t size) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    fwrite(data, 1, size, file);
    fclose(file);
}

static const unsigned char RED[4] = {255, 0, 0, 255};
static const unsigned char BLUE[4] = {0, 0, 255, 255};

static const char *write_atlases(void) {
    const char *directory = ssr_test_directory("atlas");
    Writer key = {{0}, 0};
    put_int(&key, 2);

    // Page 0: a RLE page with one packed 2 x 2 frame.
    put_string(&key, "RlePage");
    put_int(&key, 64);
    put_int(&key, 64);
    put_int(&key, 1);
    put_int(&key, 4);
    put_string(&key, "Frame0");
    put_short(&key, 2);
    put_short(&key, 2);
    put_short(&key, 2);
    put_short(&key, 2);

    // Page 1: a 4 x 4 PNG page with three images.
    put_string(&key, "PngPage");
    put_int(&key, 4);
    put_int(&key, 4);
    put_int(&key, 3);
    put_int(&key, 1);
    put_string(&key, "Plain");
    put_short(&key, 0);
    put_short(&key, 0);
    put_short(&key, 1);
    put_short(&key, 1);
    put_int(&key, 0);
    put_short(&key, 4);
    put_short(&key, 4);
    put_short(&key, 2);
    put_short(&key, 2);
    const float rect[8] = {0.25f, 0.25f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f, 0.75f};
    for (int i = 0; i < 8; i++) put_float(&key, rect[i]);

    put_int(&key, 2);
    put_string(&key, "Sliced");
    put_short(&key, 4);
    put_short(&key, 4);
    put_short(&key, 4);
    put_short(&key, 4);
    put_short(&key, 2);
    put_short(&key, 2);
    for (int i = 0; i < 12; i++) put_float(&key, (float)i);
    put_float(&key, -1.0f);

    // A stripped image with one strip of frame 0 at (2, 0), size 1 x 1. The
    // strip count is odd, so the alpha flag is a short.
    put_int(&key, 9);
    put_string(&key, "Stripped");
    put_short(&key, 1);
    put_short(&key, 0);
    put_short(&key, 0);
    put_short(&key, 0);
    put_short(&key, 3);
    put_short(&key, 0);
    put_short(&key, 1);
    put_short(&key, 1);
    put_short(&key, 4);
    put_short(&key, 4);
    for (int i = 0; i < 12; i++) put_float(&key, 0.5f);
    put_float(&key, -1.0f);

    // The cel chains, then the end. The second chain of "Plain" takes the
    // place of the first.
    put_string(&key, "Plain");
    put_string(&key, "Shadow");
    put_string(&key, "");
    put_string(&key, "Zulu");
    put_string(&key, "Zone");
    put_string(&key, "");
    put_string(&key, "Alpha");
    put_string(&key, "Arm");
    put_string(&key, "");
    put_string(&key, "Plain");
    put_string(&key, "Shadow");
    put_string(&key, "Gloss");
    put_string(&key, "");
    put_string(&key, "");
    write_bytes(directory, "Key.key", key.data, key.size);

    // The RLE page: red, blue / blue, blue.
    Writer rle = {{0}, 0};
    put_int(&rle, 2);
    memcpy(rle.data + rle.size, RED, 4);
    memcpy(rle.data + rle.size + 4, BLUE, 4);
    rle.size += 8;
    put_int(&rle, 1);
    put_int(&rle, 0);
    put_int(&rle, 2);
    const unsigned char bytes[8] = {0, 1, 2, 4, 1, 2, 2, 2};
    memcpy(rle.data + rle.size, bytes, 8);
    rle.size += 8;
    write_bytes(directory, "RlePage-quant.bin", rle.data, rle.size);

    // The PNG page is grey. The frame goes over it at (1, 1).
    unsigned char grey[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        grey[i * 4] = grey[i * 4 + 1] = grey[i * 4 + 2] = 128;
        grey[i * 4 + 3] = 255;
    }
    char path[1200];
    snprintf(path, sizeof(path), "%s/PngPage.png", directory);
    stbi_write_png(path, 4, 4, 4, grey, 16);
    return directory;
}

static const unsigned char *pixel(const SSRImage *image, int x, int y) {
    return image->pixels + ((size_t)y * image->width + x) * 4;
}

static void test_reads_the_key(void) {
    const char *keys[] = {"Key.key"};
    SSRAtlases *atlases = ssr_atlas_load(write_atlases(), keys, 1);
    CHECK(atlases != NULL);
    if (!atlases) return;
    CHECK(ssr_atlas_page_count(atlases) == 2 && ssr_atlas_image_count(atlases) == 4);
    CHECK(ssr_atlas_page(atlases, 0)->rle == 1 && ssr_atlas_page(atlases, 1)->rle == 0);
    CHECK(ssr_atlas_page(atlases, 1)->width == 4 && ssr_atlas_page(atlases, 1)->imageCount == 3);

    const char *cels[4];
    CHECK(ssr_atlas_cels(atlases, "Plain", cels, 4) == 2 && strcmp(cels[1], "Gloss") == 0);
    CHECK(ssr_atlas_cels(atlases, "Alpha", cels, 4) == 1 && strcmp(cels[0], "Arm") == 0);
    CHECK(ssr_atlas_cels(atlases, "Zulu", cels, 4) == 1 && strcmp(cels[0], "Zone") == 0);
    CHECK(ssr_atlas_cels(atlases, "Sliced", cels, 4) == 0);
    CHECK(ssr_atlas_cels(atlases, "Zz", cels, 4) == 0);
    const SSRAtlasImage *plain = ssr_atlas_find(atlases, "Plain");
    CHECK(plain && plain->kind == SSR_ATLAS_PLAIN && plain->page == 1 && plain->frame == 0);
    CHECK(plain && plain->unpackCount == 1 && plain->unpacks[0].x == 1 && plain->unpacks[0].width == 2);
    CHECK(plain && plain->rect[4] == 0.75f && plain->trimWidth == 2);

    const SSRAtlasImage *sliced = ssr_atlas_find(atlases, "Sliced");
    CHECK(sliced && sliced->kind == SSR_ATLAS_SLICED && sliced->shapeCount == 1 && sliced->shapes[11] == 11.0f);
    CHECK(sliced && sliced->fixedWidth == 2);

    const SSRAtlasImage *stripped = ssr_atlas_find(atlases, "Stripped");
    CHECK(stripped && stripped->kind == SSR_ATLAS_STRIPPED && stripped->unpackCount == 1);
    CHECK(stripped && stripped->unpacks[0].x == 3 && stripped->unpacks[0].height == 1);
    CHECK(stripped && stripped->shapeCount == 1 && stripped->shapes[0] == 0.5f);

    CHECK(ssr_atlas_find(atlases, "Frame0")->kind == SSR_ATLAS_PACKED);
    CHECK(ssr_atlas_find(atlases, "Missing") == NULL);
    ssr_atlas_free(atlases);
}

static void test_decodes_the_pages(void) {
    const char *keys[] = {"Key.key"};
    SSRAtlases *atlases = ssr_atlas_load(write_atlases(), keys, 1);
    if (!atlases) {
        CHECK(atlases != NULL);
        return;
    }
    SSRImage frame;
    CHECK(ssr_atlas_frame_pixels(atlases, 0, 0, &frame));
    CHECK(frame.width == 2 && frame.height == 2);
    CHECK(memcmp(pixel(&frame, 0, 0), RED, 4) == 0 && memcmp(pixel(&frame, 1, 0), BLUE, 4) == 0);
    CHECK(memcmp(pixel(&frame, 0, 1), BLUE, 4) == 0 && memcmp(pixel(&frame, 1, 1), BLUE, 4) == 0);
    ssr_image_free(&frame);

    SSRImage page;
    CHECK(ssr_atlas_page_pixels(atlases, 1, &page));
    CHECK(page.width == 4 && page.height == 4);
    CHECK(memcmp(pixel(&page, 1, 1), RED, 4) == 0 && memcmp(pixel(&page, 2, 2), BLUE, 4) == 0);
    // The edge of the frame goes one pixel out.
    CHECK(memcmp(pixel(&page, 0, 0), RED, 4) == 0 && memcmp(pixel(&page, 3, 3), BLUE, 4) == 0);
    // The strip is the top left pixel of the frame, at (3, 0).
    CHECK(memcmp(pixel(&page, 3, 0), RED, 4) == 0);
    ssr_image_free(&page);

    CHECK(!ssr_atlas_page_pixels(atlases, 0, &page));
    CHECK(!ssr_atlas_frame_pixels(atlases, 1, 0, &frame));
    ssr_atlas_free(atlases);
}

static void test_refuses_a_damaged_key(void) {
    const char *directory = ssr_test_directory("badatlas");
    const unsigned char bytes[6] = {5, 0, 0, 0, 3, 'A'};
    write_bytes(directory, "Key.key", bytes, sizeof(bytes));
    const char *keys[] = {"Key.key"};
    CHECK(ssr_atlas_load(directory, keys, 1) == NULL);
    const char *missing[] = {"Missing.key"};
    CHECK(ssr_atlas_load(directory, missing, 1) == NULL);
}

// A box of 4 x 4 with the left column trimmed. The page holds the other
// 3 x 4 pixels at (2, 2), from the bottom up as the keys of the game do.
static void test_patches_an_image(void) {
    unsigned char pagePixels[8 * 8 * 4];
    memset(pagePixels, 7, sizeof(pagePixels));
    SSRImage page = {8, 8, pagePixels};
    SSRAtlasImage image = {0};
    image.kind = SSR_ATLAS_PLAIN;
    image.originalWidth = image.originalHeight = 4;
    const float rect[8] = {0.25f, 1.0f, 0.25f, 0.75f, 1.0f, 0.0f, 0.625f, 0.25f};
    memcpy(image.rect, rect, sizeof(rect));
    unsigned char patchPixels[4 * 4 * 4];
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            unsigned char *p = patchPixels + (y * 4 + x) * 4;
            p[0] = (unsigned char)(x * 10 + y);
            p[1] = p[2] = 0;
            p[3] = x == 2 && y == 1 ? 0 : 255;
        }
    }
    SSRImage patch = {4, 4, patchPixels};
    CHECK(ssr_atlas_patch(&page, &image, &patch));
    // The page pixel (2, 2) is the box pixel (1, 0), the top of the box.
    CHECK(pagePixels[(2 * 8 + 2) * 4] == 10 && pagePixels[(2 * 8 + 2) * 4 + 3] == 255);
    CHECK(pagePixels[(5 * 8 + 4) * 4] == 33);
    // The clear pixel of the patch keeps the page pixel.
    CHECK(pagePixels[(3 * 8 + 3) * 4] == 7);
    // The pixels around the image stay.
    CHECK(pagePixels[(2 * 8 + 1) * 4] == 7 && pagePixels[(6 * 8 + 2) * 4] == 7 && pagePixels[(2 * 8 + 5) * 4] == 7);
    // A patch of another size, or an image that is not plain, takes none.
    SSRImage small = {2, 2, patchPixels};
    CHECK(!ssr_atlas_patch(&page, &image, &small));
    image.kind = SSR_ATLAS_SLICED;
    CHECK(!ssr_atlas_patch(&page, &image, &patch));
}

// A stripped image: two triangles put the 4 x 4 box on the page at (2, 2),
// turned a quarter. The x of the box goes down the page, and the y of the
// box goes to the left.
static void test_patches_a_stripped_image(void) {
    unsigned char pagePixels[8 * 8 * 4];
    memset(pagePixels, 7, sizeof(pagePixels));
    SSRImage page = {8, 8, pagePixels};
    float shapes[24] = {
        0, 0, 0.75f, 0.25f, 1, 0, 0.75f, 0.75f, 1, 1, 0.25f, 0.75f,
        0, 0, 0.75f, 0.25f, 1, 1, 0.25f, 0.75f, 0, 1, 0.25f, 0.25f,
    };
    SSRAtlasImage image = {0};
    image.kind = SSR_ATLAS_STRIPPED;
    image.originalWidth = image.originalHeight = 4;
    image.shapes = shapes;
    image.shapeCount = 2;
    unsigned char patchPixels[4 * 4 * 4];
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            unsigned char *p = patchPixels + (y * 4 + x) * 4;
            p[0] = (unsigned char)(x * 10 + y);
            p[1] = p[2] = 0;
            p[3] = x == 2 && y == 1 ? 0 : 255;
        }
    }
    SSRImage patch = {4, 4, patchPixels};
    CHECK(ssr_atlas_patch(&page, &image, &patch));
    // The box pixel (1, 0) is at the page pixel (5, 3), and (3, 3) at (2, 5).
    CHECK(pagePixels[(3 * 8 + 5) * 4] == 10 && pagePixels[(3 * 8 + 5) * 4 + 3] == 255);
    CHECK(pagePixels[(5 * 8 + 2) * 4] == 33);
    // The clear pixel (2, 1) of the patch keeps the page pixel (4, 4).
    CHECK(pagePixels[(4 * 8 + 4) * 4] == 7);
    // The pixels around the image stay.
    CHECK(pagePixels[(2 * 8 + 1) * 4] == 7 && pagePixels[(6 * 8 + 6) * 4] == 7 && pagePixels[(1 * 8 + 3) * 4] == 7);
}

int main(void) {
    test_reads_the_key();
    test_patches_an_image();
    test_patches_a_stripped_image();
    test_decodes_the_pages();
    test_refuses_a_damaged_key();
    return TEST_RESULT();
}
