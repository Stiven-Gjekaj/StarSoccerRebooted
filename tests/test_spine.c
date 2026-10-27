// Tests ssr_spine with a small .spinary file that the test builds.

#include <math.h>
#include <string.h>

#include "ssr_spine.h"
#include "ssr_test.h"

typedef struct {
    unsigned char data[4096];
    size_t size;
} Writer;

static void pad(Writer *w, size_t to) {
    while (w->size % to) w->data[w->size++] = 0xCD;
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

static void put_bone(Writer *w, int parent, float x, float y, float rotation, const char *name) {
    put_short(w, parent);
    put_short(w, 3);
    put_float(w, 10);
    put_float(w, x);
    put_float(w, y);
    put_float(w, rotation);
    put_float(w, 1);
    put_float(w, 1);
    put_string(w, name);
}

// Two bones: a root at (100, 0) and a child at (10, 0). Two slots on the
// child: one with the image, and one that the animation does not use. One
// animation of 2 seconds turns the child by 90 degrees from frame 2 on, and
// sets the colour of the slot at 0.5 seconds.
static size_t build(unsigned char *out) {
    Writer w = {{0}, 0};
    put_string(&w, "Test");
    put_float(&w, 100);
    put_float(&w, 100);
    put_int(&w, 2);
    put_bone(&w, -1, 100, 0, 0, "root");
    put_bone(&w, 0, 10, 0, 0, "child");
    put_int(&w, 2);
    const char *slots[2] = {"Leaf", "Unused"};
    for (int i = 0; i < 2; i++) {
        put_string(&w, slots[i]);
        put_int(&w, 1);
        for (int c = 0; c < 4; c++) put_float(&w, 1);
        put_string(&w, "leaf");
        put_int(&w, 0);
    }
    put_int(&w, 1);
    put_string(&w, "default");
    put_int(&w, 2);
    for (int i = 0; i < 2; i++) {
        put_int(&w, 1);
        put_string(&w, "leaf");
        put_string(&w, "NewGUI/TitleScreen/Title_Grass01");
        const float values[11] = {5, 0, 1, 1, 0, 40, 20, 1, 1, 1, 1};
        for (int v = 0; v < 11; v++) put_float(&w, values[v]);
        put_int(&w, 0);
        put_int(&w, 0);
    }
    put_int(&w, 0);
    put_int(&w, 0);

    put_int(&w, 1);
    put_float(&w, 2);
    put_string(&w, "Sway");
    put_int(&w, 2);
    // Rotate the child from key 1: 0 then 90 then 90.
    put_int(&w, 6);
    put_int(&w, 1);
    put_int(&w, 1);
    put_int(&w, 3);
    put_float(&w, 0);
    put_float(&w, 90);
    put_float(&w, 90);
    put_int(&w, 0x600DF00D);
    // The colour of slot 0 from 0.5 seconds: opaque red.
    put_int(&w, 9);
    put_int(&w, 0);
    put_float(&w, 0.5f);
    put_int(&w, 1);
    put_int(&w, (int)0xFF0000FFu);
    put_int(&w, 0x600DF00D);
    put_int(&w, 2);
    put_int(&w, 0);
    put_int(&w, 1);
    put_int(&w, 1);
    put_int(&w, 0);
    memcpy(out, w.data, w.size);
    return w.size;
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static void test_reads_the_file(void) {
    unsigned char data[4096];
    size_t size = build(data);
    SSRSpine *spine = ssr_spine_parse(data, size);
    CHECK(spine != NULL);
    CHECK(ssr_spine_bone_count(spine) == 2 && ssr_spine_slot_count(spine) == 2);
    CHECK(ssr_spine_animation_count(spine) == 1 && ssr_spine_animation(spine, "Sway") == 0);
    CHECK(ssr_spine_animation(spine, "Missing") == -1);
    CHECK(near(ssr_spine_duration(spine, 0), 2));
    ssr_spine_free(spine);

    // A file with a byte more, or a byte less, is damaged.
    CHECK(ssr_spine_parse(data, size - 1) == NULL);
    data[size] = 0;
    CHECK(ssr_spine_parse(data, size + 1) == NULL);
}

static void test_poses_the_animation(void) {
    unsigned char data[4096];
    size_t size = build(data);
    SSRSpine *spine = ssr_spine_parse(data, size);
    SSRSpineDraw draws[4];

    // At the start, the child has no turn. Only the used slot draws. The
    // image is at root (100) + child (10) + attachment (5) on x.
    CHECK(ssr_spine_pose(spine, 0, 0, 1, draws, 4) == 1);
    CHECK(strcmp(draws[0].image, "Title_Grass01") == 0);
    CHECK(strcmp(draws[0].file, "NewGUI/TitleScreen/Title_Grass01.png") == 0);
    CHECK(near(draws[0].m[4], 115) && near(draws[0].m[5], 0));
    CHECK(near(draws[0].m[0], 1) && near(draws[0].m[3], 1));
    CHECK(near(draws[0].a, 1) && near(draws[0].g, 1));

    // At frame 4 (key 2), the child turns 90 degrees anticlockwise in y-up
    // space. The attachment then goes up the screen.
    CHECK(ssr_spine_pose(spine, 0, 4.0f / 60, 1, draws, 4) == 1);
    CHECK(near(draws[0].m[4], 110) && near(draws[0].m[5], -5));
    CHECK(near(draws[0].m[0], 0) && near(draws[0].m[1], -1));

    // At frame 3 (between key 1 and key 2), the turn is the mean: 45.
    CHECK(ssr_spine_pose(spine, 0, 3.0f / 60, 1, draws, 4) == 1);
    CHECK(near(draws[0].m[0], cosf(3.14159265f / 4)));

    // From 0.5 seconds, the colour timeline makes the slot red.
    CHECK(ssr_spine_pose(spine, 0, 0.75f, 1, draws, 4) == 1);
    CHECK(near(draws[0].r, 1) && near(draws[0].g, 0) && near(draws[0].a, 1));

    // A loop starts again after 2 seconds. Without a loop, the pose stays.
    CHECK(ssr_spine_pose(spine, 0, 2.0f + 1.0f / 60, 1, draws, 4) == 1);
    CHECK(near(draws[0].m[0], 1));
    CHECK(ssr_spine_pose(spine, 0, 5, 0, draws, 4) == 1);
    CHECK(near(draws[0].m[0], 0));
    ssr_spine_free(spine);
}

// A gel can follow the bone of a slot (c_Slot): the bone of the slot
// "Leaf" is the child, at 110 on x, and it turns with the child.
static void test_gives_the_bone_of_a_slot(void) {
    unsigned char data[4096];
    size_t size = build(data);
    SSRSpine *spine = ssr_spine_parse(data, size);
    float m[6], rgba[4];
    CHECK(ssr_spine_slot(spine, 0, 0, 1, "Leaf", m, rgba) == 1);
    CHECK(near(m[4], 110) && near(m[5], 0) && near(m[0], 1));
    CHECK(ssr_spine_slot(spine, 0, 4.0f / 60, 1, "Leaf", m, rgba) == 1);
    CHECK(near(m[0], 0) && near(m[1], -1) && near(m[4], 110));
    CHECK(ssr_spine_slot(spine, 0, 0.75f, 1, "Leaf", m, rgba) == 1);
    CHECK(near(rgba[0], 1) && near(rgba[1], 0) && near(rgba[3], 1));
    CHECK(ssr_spine_slot(spine, 0, 0, 1, "Nothing", m, rgba) == 0);
    ssr_spine_free(spine);
}

int main(void) {
    test_reads_the_file();
    test_poses_the_animation();
    test_gives_the_bone_of_a_slot();
    return TEST_RESULT();
}
