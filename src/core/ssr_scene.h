// ssr_scene: turns the tree of a screen layout into a list of things to
// draw, in order. The app draws the list. The scene does no drawing, so a
// test can check it.

#ifndef SSR_SCENE_H
#define SSR_SCENE_H

#include <stdint.h>

#include "ssr_tweaks.h"
#include "ssr_ui.h"

typedef enum {
    SSR_ITEM_IMAGE,
    SSR_ITEM_TEXT,
    // An area that the player can press. It draws nothing.
    SSR_ITEM_AREA,
    SSR_ITEM_CLIP,
    SSR_ITEM_UNCLIP,
} SSRItemKind;

typedef struct {
    SSRItemKind kind;
    // SSR_ITEM_IMAGE: the name of the image.
    const char *image;
    // SSR_ITEM_TEXT: the text, with the language already applied.
    const char *text;
    // The box on the screen before the rotation. A negative width or
    // height flips the image.
    float x, y, w, h;
    // The rotation in degrees, about the point (pivotX, pivotY).
    float angle;
    float pivotX, pivotY;
    // 0xRRGGBBAA.
    uint32_t colour;
    int additive;
    // The image repeats to fill the box.
    int tile;
    // SSR_ITEM_TEXT: the font size (0 to 3) and the alignment: 'L', 'C', or
    // 'R', and 'T', 'M', or 'B'.
    int font;
    char alignX, alignY;
    // SSR_ITEM_TEXT: the scale of the font, and 1 when the text wraps to
    // the width of the box.
    float fontScaleX, fontScaleY;
    int wrap;
    int layer;
    const char *ref;
} SSRSceneItem;

typedef struct {
    // The size of the screen in logical pixels, for example 640 x 960.
    float width, height;
    const SSRTweaks *tweaks;
    // Gives the text of a language tag. NULL keeps the tag.
    const char *(*text)(void *context, const char *tag);
    // Gives the size of an image when a Draw: has no size. NULL gives 0.
    int (*image_size)(void *context, const char *name, float *w, float *h);
    void *context;
} SSRSceneEnv;

typedef struct SSRScene SSRScene;

SSRScene *ssr_scene_build(const SSRUiTree *tree, const SSRSceneEnv *env);
void ssr_scene_free(SSRScene *scene);

int ssr_scene_count(const SSRScene *scene);
const SSRSceneItem *ssr_scene_item(const SSRScene *scene, int index);

// The file of an image that the layout names with Img:(Name,Path), or NULL.
const char *ssr_scene_image_file(const SSRScene *scene, const char *name);

// The ref of the last item with a ref at the point, or NULL.
const char *ssr_scene_hit(const SSRScene *scene, float x, float y);

#endif
