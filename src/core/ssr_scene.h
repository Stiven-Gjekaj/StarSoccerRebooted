// ssr_scene: makes the elements of a screen layout from its tree, as the
// game does (c_Gel), and runs them each frame: the animations, the
// converges, the movers, the emitters, and the touch. Each frame gives a
// list of things to draw, in order. The app draws the list. The scene does
// no drawing, so a test can check it.

#ifndef SSR_SCENE_H
#define SSR_SCENE_H

#include <stdint.h>

#include "ssr_spine.h"
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

typedef enum {
    // src ONE, dst ONE_MINUS_SRC_ALPHA: the default of the game.
    SSR_BLEND_ALPHA,
    // Additive(): dst ONE.
    SSR_BLEND_ADD,
    // Gloss(): src DST_COLOR, dst ONE.
    SSR_BLEND_GLOSS,
} SSRBlend;

typedef struct {
    SSRItemKind kind;
    // SSR_ITEM_IMAGE: the name of the image.
    const char *image;
    // SSR_ITEM_TEXT: the text, with the language already applied.
    const char *text;
    // The box on the screen before the rotation. A negative width or
    // height flips the image.
    float x, y, w, h;
    // The rotation in degrees, about the point (pivotX, pivotY). A positive
    // angle turns anticlockwise on the screen, as in the game.
    float angle;
    float pivotX, pivotY;
    // When quad is 1, the image goes on these four corners (top left, top
    // right, bottom right, bottom left, x and y each) in place of the box.
    int quad;
    float corners[8];
    // 0xRRGGBBAA.
    uint32_t colour;
    SSRBlend blend;
    // SSR_ITEM_TEXT: the font size (0 to 3) and the alignment: 'L', 'C', or
    // 'R', and 'T', 'M', or 'B'.
    int font;
    char alignX, alignY;
    // SSR_ITEM_TEXT: the scale of the font, and 1 when the text wraps to
    // the width of the box.
    float fontScaleX, fontScaleY;
    int wrap;
    // SSR_ITEM_TEXT: the slant of the glyphs, x += (y - baseline) * italic.
    // The game slants all text by -0.25 if Italicise() does not change it.
    float italic;
    // The index of the layer of the screen. The list is in draw order.
    int layer;
    const char *ref;
    // The message of a Click(message) modifier, and the sound that its
    // block plays with PlaySnd(name). NULL when there is none.
    const char *click;
    const char *clickSound;
    // The element of the item, for ssr_scene_click.
    const void *owner;
} SSRSceneItem;

typedef struct {
    // The screen of the device in the units of the layouts. The layouts
    // are for 640 x 960. The game fits that box in the screen and centres
    // it, so a tall screen has a negative top, for example -231 for
    // 640 x 1422. This is c_VirtualDisplay of the game.
    float left, top, width, height;
    // SetTweak() of a layout changes the tweaks.
    SSRTweaks *tweaks;
    // Gives the text of a language tag. NULL keeps the tag.
    const char *(*text)(void *context, const char *tag);
    // Gives the size of an image when a Draw: has no size. file is the file
    // of an Img: with the name, or NULL. NULL gives 0.
    int (*image_size)(void *context, const char *name, const char *file, float *w, float *h);
    // Gives the Spine file with the name, for example "S2GrassAnim", or NULL.
    const SSRSpine *(*spine)(void *context, const char *name);
    // Gives the text of a GS_ function of a layout, for example
    // "GS_PlayerClubName". NULL keeps the text of the line.
    const char *(*provide)(void *context, const char *name);
    void *context;
} SSRSceneEnv;

typedef struct SSRScene SSRScene;

// Makes the elements of the screen. The tree must stay until the scene is
// freed. The scene has no items before the first update.
SSRScene *ssr_scene_open(const SSRUiTree *tree, const SSRSceneEnv *env);

// Runs one frame, ms milliseconds after the frame before it, and makes the
// list of items. The first update gives 0.
void ssr_scene_update(SSRScene *scene, const SSRSceneEnv *env, int ms);

// The touch of the player, in the units of the layouts. Press() sees it in
// the next update.
void ssr_scene_touch(SSRScene *scene, int down, float x, float y);

// Opens the screen and runs its first frame.
SSRScene *ssr_scene_build(const SSRUiTree *tree, const SSRSceneEnv *env);
void ssr_scene_free(SSRScene *scene);

// The time of the scene in milliseconds, as the game counts it.
int ssr_scene_time(const SSRScene *scene);

int ssr_scene_count(const SSRScene *scene);
const SSRSceneItem *ssr_scene_item(const SSRScene *scene, int index);

// The file of an image that the layout names with Img:(Name,Path), or NULL.
const char *ssr_scene_image_file(const SSRScene *scene, const char *name);

// The colour text (#RRGGBBAA, or 16 digits with the mix factors) on the
// parent colour 0xRRGGBBAA, as the colour stack of the game mixes them.
// Returns the parent when the text is not a colour.
uint32_t ssr_scene_mix_colour(const char *colour, uint32_t parent);

// The ref of the last item with a ref at the point, or NULL.
const char *ssr_scene_hit(const SSRScene *scene, float x, float y);

// The last item with a click at the point, or NULL.
const SSRSceneItem *ssr_scene_hit_item(const SSRScene *scene, float x, float y);

// Runs the "< >" block of the Click() of the item: SetTweak, IncrementTweak,
// PlaySnd, and the other actions. Returns the message of the click, or
// NULL when the item has no click.
const char *ssr_scene_click(SSRScene *scene, const SSRSceneEnv *env, const SSRSceneItem *item);

// 1 when the last touch moved a swipe panel by more than 10 units. Then
// the touch is not a click, as in the game.
int ssr_scene_dragged(const SSRScene *scene);

// The next sound that a click asked for, or NULL when there is none.
const char *ssr_scene_take_sound(SSRScene *scene);

// Gives the first text of each element with the ref this text, as the code
// of the game does with the gadgets of a screen. Returns the number of
// elements.
int ssr_scene_set_text(SSRScene *scene, const char *ref, const char *text);

// Shows or hides each element with the ref. Returns the number of
// elements.
int ssr_scene_set_visible(SSRScene *scene, const char *ref, int visible);

// The lists that the code fills (c_ACombo). list is the ref of the element
// that holds the entries. An entry is a copy of the template with the ref
// template_: it goes below the entry before it, shows the text, and its
// SetTweak with the ref ID gives the id. Returns 1 when it worked.
void ssr_scene_list_clear(SSRScene *scene, const char *list);

// Adds to the element with the ref parent a copy of the template, at x,
// with the text in its first text. A width more than 0 is the width of the
// copy and of its text (c_GGadget.SetElementWidth). Returns an id for the
// copy, or 0.
int ssr_scene_spawn(SSRScene *scene, const char *parent, const char *template_, float x, const char *text,
                    float width);
// The place of the copy in its parent: it moves with its Mover.
float ssr_scene_spawn_x(const SSRScene *scene, int id);
void ssr_scene_spawn_remove(SSRScene *scene, int id);
int ssr_scene_list_add(SSRScene *scene, const char *list, const char *template_, const char *text, int id);

// Moves the first element with the ref to (x, y) in its parent, at the
// scale.
void ssr_scene_place(SSRScene *scene, const char *ref, float x, float y, float scale);

// Moves the copy with the id, or the element with the ref inside it, to
// (x, y) in its parent. ref can be NULL.
void ssr_scene_spawn_place(SSRScene *scene, int id, const char *ref, float x, float y);
// Plays the animation on the AnimInstance with the ref inside the copy.
void ssr_scene_spawn_anim(SSRScene *scene, int id, const char *ref, const char *anim);
// Shows or hides the copy, or the element with the ref inside it.
void ssr_scene_spawn_show(SSRScene *scene, int id, const char *ref, int visible);

#endif
