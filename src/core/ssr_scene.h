// ssr_scene: makes the elements of a screen layout from its tree, as the
// game does (c_Gel), and runs them each frame: the animations, the
// converges, the movers, the emitters, and the touch. Each frame gives a
// list of things to draw, in order. The app draws the list. The scene does
// no drawing, so a test can check it.

#ifndef SSR_SCENE_H
#define SSR_SCENE_H

#include <stdint.h>

#include "ssr_ex.h"
#include "ssr_spine.h"
#include "ssr_tweaks.h"
#include "ssr_ui.h"

typedef enum {
    SSR_ITEM_IMAGE,
    SSR_ITEM_TEXT,
    // An area that the player can press. It draws nothing.
    SSR_ITEM_AREA,
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
    // When projected is 1, the image is on a Sheet: with perspective. Its
    // corners (top left, top right, bottom right, bottom left) are in the
    // space of the camera, x, y, z for each. A point goes to the screen at
    // (320 + focal * x / z, 480 + focal * y / z).
    int projected;
    float camera[12];
    float focal;
    // 0xRRGGBBAA.
    uint32_t colour;
    // The alpha at the far edge of the image, as a part of the alpha of the
    // colour. The edge at the y0 of the image keeps the full alpha. 0 when
    // the alpha does not fade. A shadow of c_RenderShadow fades, and it
    // draws the image with no cels.
    float fade;
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
    // SSR_ITEM_TEXT: 1 when the text draws a shadow first, moved by
    // shadowX and shadowY units of the font, in shadowColour (0xRRGGBBAA).
    int shadow;
    float shadowX, shadowY;
    uint32_t shadowColour;
    // The index of the layer of the screen. The list is in draw order.
    int layer;
    const char *ref;
    // The message of a Click(message) modifier, and the sound that its
    // block plays with PlaySnd(name). NULL when there is none.
    const char *click;
    const char *clickSound;
    // The element of the item, for ssr_scene_click.
    const void *owner;
    // The cel colours of CloneTweak for the item, "Cel=RRGGBBAA;", or NULL.
    // A cel that is not there takes the tweak CelColours:Cel.
    const char *cels;
    // 1 when a Clip: around the item cuts it to the box of the clip, on
    // the screen.
    int clipped;
    float clipX, clipY, clipW, clipH;
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

// The modifiers of the layout that the scene does not know: it skips them.
// Each name comes once.
int ssr_scene_unknown_count(const SSRScene *scene);
const char *ssr_scene_unknown(const SSRScene *scene, int index);

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

// The next message of a Message() line that ran in an update, not in a
// click, for example when an animation is nearly over. NULL when there is
// none.
const char *ssr_scene_take_message(SSRScene *scene);

// Gives the first text of each element with the ref this text, as the code
// of the game does with the gadgets of a screen. Returns the number of
// elements.
int ssr_scene_set_text(SSRScene *scene, const char *ref, const char *text);

// Shows or hides each element with the ref. Returns the number of
// elements.
int ssr_scene_set_visible(SSRScene *scene, const char *ref, int visible);

// Gives the colour "RRGGBBAA" to each element with the ref
// (c_GGadget.SetColour). Returns the number of elements.
int ssr_scene_set_colour(SSRScene *scene, const char *ref, const char *colour);
// Only the element number index (from 0) of the elements with the ref takes
// the colour, as an element of a gadget of the game. Returns 1 when the
// element is there.
int ssr_scene_set_element_colour(SSRScene *scene, const char *ref, int index, const char *colour);

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

// The last entry of the list: the element with the number takes the colour
// "RRGGBBAA" (c_GGadget.SetElementColour). The elements are the gels with
// the ref of the entry, in the order of the file. 0 is the entry.
void ssr_scene_list_colour(SSRScene *scene, const char *list, int element, const char *colour);
// The last entry of the list: the element with the number takes the text
// (c_GGadget.SetElementText). The numbers are those of ssr_scene_list_colour.
void ssr_scene_list_text(SSRScene *scene, const char *list, int element, const char *text);
// The clicks of the last entry send their message with the param after a
// space, for example "league.row_team 7" (c_GGadget param).
void ssr_scene_list_param(SSRScene *scene, const char *list, int param);
// The next entry of the list goes at the y.
void ssr_scene_list_start(SSRScene *scene, const char *list, float y);
// In the calls on the last entry, "list@n" names the entry n, from 0.
// The last entry of the list: each gel with the ref shows or hides, and a
// text with the ref takes the text when it is not NULL.
void ssr_scene_list_sub(SSRScene *scene, const char *list, const char *ref, const char *text, int visible);

// The builders of an entry of a list, as the game makes the buttons of a
// shop (c_GTemplate.CreateDisposable and AddLocalChild). The list gets a
// copy of the template at x, under the entry before it.
int ssr_scene_list_add_at(SSRScene *scene, const char *list, const char *template_, float x);
// The part ref of the last entry of the list (the entry itself when ref is
// NULL or "") gets a copy of the template, at the place of the template.
int ssr_scene_entry_child(SSRScene *scene, const char *list, const char *ref, const char *template_);
// The strvar %name of the part ref of the last entry takes the value.
int ssr_scene_entry_var(SSRScene *scene, const char *list, const char *ref, const char *name, const char *value);
// The ProgressBar of the part ref of the last entry follows the tweak
// group,name (c_ProgressBar.SetSourceTweakValue).
int ssr_scene_entry_bar(SSRScene *scene, const char *list, const char *ref, const char *group, const char *name);

// The first element with the ref hook gets a copy of the template in the
// place of its children, at its origin (c_GGadget.Clone into a hook). An
// empty template clears the hook. Returns 1 when both are there.
int ssr_scene_set_hook(SSRScene *scene, const char *hook, const char *template_);
// The elements with the ref sub inside the first element with the ref show
// when visible is 1, and a text among them takes the text when it is not
// NULL.
void ssr_scene_sub(SSRScene *scene, const char *ref, const char *sub, const char *text, int visible);

// The AnimInstance elements with the ref play at the speed, and jump to the
// end of their animation when toEnd is 1 (c_GelAnimInstance).
void ssr_scene_anim(SSRScene *scene, const char *ref, float speed, int toEnd);
// The AnimInstance elements with the ref play the animation (or their own
// one when anim is NULL) from the frame at the speed. A speed below 0 plays
// the animation back to its start.
void ssr_scene_anim_play(SSRScene *scene, const char *ref, const char *anim, float frame, float speed);
// The AnimInstance number index (from 0) of the AnimInstance elements with
// the ref stops at the frame (c_GelAnimInstance.SetFrame). Returns 1 when
// the instance is there.
int ssr_scene_anim_frame(SSRScene *scene, const char *ref, int index, float frame);

// Moves the first element with the ref to (x, y) in its parent, at the
// scale.
void ssr_scene_place(SSRScene *scene, const char *ref, float x, float y, float scale);

// The calls of a copy take its id. The id 0 is the whole screen, so that
// the code can change an element of the screen by its ref.
// Moves the copy with the id, or the element with the ref inside it, to
// (x, y) in its parent. ref can be NULL.
void ssr_scene_spawn_place(SSRScene *scene, int id, const char *ref, float x, float y);
// Plays the animation on the AnimInstance with the ref inside the copy.
void ssr_scene_spawn_anim(SSRScene *scene, int id, const char *ref, const char *anim);
// The CloneTweak doodads with the ref in the copy take their value from the
// tweak group,name.
void ssr_scene_spawn_clone(SSRScene *scene, int id, const char *ref, const char *group, const char *name);
// Sets a property of the copy, or of the element ref inside it: "ref" is
// the first gel with the ref, "ref#n" is the element n of the ref (as
// c_GGadget.GetElement), and NULL is the copy.
void ssr_scene_spawn_set(SSRScene *scene, int id, const char *ref, SSRGelProperty property, float value);
// The colour "RRGGBBAA" of the copy, or of the element ref inside it.
void ssr_scene_spawn_colour(SSRScene *scene, int id, const char *ref, const char *colour);
// Shows or hides the copy, or the element with the ref inside it.
void ssr_scene_spawn_show(SSRScene *scene, int id, const char *ref, int visible);

#endif
