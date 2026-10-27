// ssr_spine: the skeletal animations of the game (Spine/*.spinary), for
// example the grass of the main menu. The format is the one of the Spine
// 2.1 runtime of the game for Monkey X. docs/formats.md describes it.

#ifndef SSR_SPINE_H
#define SSR_SPINE_H

#include <stddef.h>

typedef struct SSRSpine SSRSpine;

// Reads a .spinary file. Returns NULL when the file is not there or is
// damaged.
SSRSpine *ssr_spine_load(const char *path);
SSRSpine *ssr_spine_parse(const unsigned char *data, size_t size);
void ssr_spine_free(SSRSpine *spine);

int ssr_spine_bone_count(const SSRSpine *spine);
int ssr_spine_slot_count(const SSRSpine *spine);
int ssr_spine_animation_count(const SSRSpine *spine);
// The index of the animation with the name, or -1.
int ssr_spine_animation(const SSRSpine *spine, const char *name);
float ssr_spine_duration(const SSRSpine *spine, int animation);

// One image of a pose. The image is drawn with its centre at (0, 0) of its
// own space, at its own size. m maps a point (x, y) of that space to the
// space of the Spine element, y down: (m[0]x + m[2]y + m[4],
// m[1]x + m[3]y + m[5]).
typedef struct {
    // The name of the image in the atlases, for example "Title_Grass01".
    const char *image;
    // The file of the image when the atlases do not have the name, for
    // example "NewGUI/TitleScreen/Title_Grass01.png".
    const char *file;
    float m[6];
    float r, g, b, a;
} SSRSpineDraw;

// Puts the images of the animation at the time (in seconds) into out, up
// to max, in the order of drawing. A looping animation repeats. Another
// animation stays at its last pose. Returns the number of images.
int ssr_spine_pose(const SSRSpine *spine, int animation, float time, int loop, SSRSpineDraw *out, int max);

// The bone of the slot with the name at the time, as m of SSRSpineDraw,
// and the colour of the slot. Returns 0 when the slot is not there.
int ssr_spine_slot(const SSRSpine *spine, int animation, float time, int loop, const char *slot, float m[6],
                   float rgba[4]);

#endif
