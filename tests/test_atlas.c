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

    // One cel chain, then the end.
    put_string(&key, "Plain");
    put_string(&key, "Shadow");
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
    CHECK(ssr_atlas_cels(atlases, "Plain", cels, 4) == 1 && strcmp(cels[0], "Shadow") == 0);
    CHECK(ssr_atlas_cels(atlases, "Sliced", cels, 4) == 0);
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

int main(void) {
    test_reads_the_key();
    test_decodes_the_pages();
    test_refuses_a_damaged_key();
    return TEST_RESULT();
}
