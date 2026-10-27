// Tests ssr_spine with a small .spinary file that the test builds.

#include <math.h>
#include <string.h>

#include "ssr_spine.h"
#include "ssr_spinewriter.h"
#include "ssr_test.h"

static int near(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static void test_reads_the_file(void) {
    unsigned char data[4096];
    size_t size = ssr_spine_writer_build(data);
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
    size_t size = ssr_spine_writer_build(data);
    SSRSpine *spine = ssr_spine_parse(data, size);
    SSRSpineDraw draws[4];

    // At the start, the child has no turn. Only the used slot draws. The
    // image is at root (100) + child (10) + attachment (5) on x.
    CHECK(ssr_spine_pose(spine, 0, 0, 1, draws, 4) == 1);
    CHECK(strcmp(draws[0].image, "Title_Grass01") == 0);
    CHECK(strcmp(draws[0].file, "NewGUI/TitleScreen/Title_Grass01.png") == 0);
    CHECK(draws[0].slot && strcmp(draws[0].slot, "Leaf") == 0);
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
    size_t size = ssr_spine_writer_build(data);
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
