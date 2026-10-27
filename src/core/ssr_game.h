// ssr_game: the logic of the screens, as the TScreen classes of the game
// have it. A Click(message) of a layout comes here. The logic changes the
// state of the game, sets the tweaks that the layouts read, and chooses the
// next screen. It draws nothing.

#ifndef SSR_GAME_H
#define SSR_GAME_H

#include "ssr_career.h"
#include "ssr_match.h"
#include "ssr_training.h"
#include "ssr_tweaks.h"
#include "ssr_world.h"

typedef struct SSRGame SSRGame;

// Gives the text of a language tag, or NULL to keep the tag.
typedef const char *(*SSRLocalise)(void *context, const char *tag);

// A change that the logic asks of the elements of the screen, as the code
// of the game does with its gadgets. The app gives it to the scene.
typedef enum {
    // Give the text to the element with ref.
    SSR_OP_TEXT,
    SSR_OP_SHOW,
    SSR_OP_HIDE,
    // Empty the list with ref.
    SSR_OP_LIST_CLEAR,
    // Add to the list with ref a copy of the template ref2, with the text
    // and the id.
    SSR_OP_LIST_ADD,
    // The element number id of the last entry of the list ref takes the
    // colour in text.
    SSR_OP_LIST_COLOUR,
    // The elements ref2 of the last entry of the list ref show when id is 1,
    // and take the text when it is not "".
    SSR_OP_LIST_SUB,
    // The AnimInstance elements ref play at the speed in text, and jump to
    // their end when id is 1.
    SSR_OP_ANIM,
    // The elements ref take the colour in text.
    SSR_OP_COLOUR,
    // The element number id of the last entry of the list ref takes the
    // text. "ref@n" names the entry n.
    SSR_OP_LIST_TEXT,
    // The clicks of the last entry of the list ref send the param id.
    SSR_OP_LIST_PARAM,
    // The next entry of the list ref goes at the y in id.
    SSR_OP_LIST_START,
    // The AnimInstance elements ref play the animation ref2 (their own one
    // when ref2 is "") from the frame id at the speed in text.
    SSR_OP_ANIM_PLAY,
    // The element number id of the elements ref takes the colour in text.
    SSR_OP_ELEMENT_COLOUR,
    // The element ref gets a copy of the template ref2.
    SSR_OP_HOOK,
    // The elements ref2 inside the element ref show when id is 1, and take
    // the text when it is not "".
    SSR_OP_SUB,
    // The list ref gets a copy of the template ref2 at the x in id.
    SSR_OP_LIST_ADD_AT,
    // The part text (the entry when "") of the last entry of the list ref
    // gets a copy of the template ref2.
    SSR_OP_ENTRY_CHILD,
    // The strvar of the part ref2 of the last entry of the list ref takes a
    // value. text is "name=value".
    SSR_OP_ENTRY_VAR,
    // The ProgressBar of the part ref2 of the last entry of the list ref
    // follows a tweak. text is "Group,Name".
    SSR_OP_ENTRY_BAR,
    // The first element ref goes to the x in id, in its parent.
    SSR_OP_SET_X,
    // The AnimInstance number id of the elements ref stops at the frame in
    // text.
    SSR_OP_ANIM_FRAME,
    // The property id (SSRGelProperty) of the first element ref takes the
    // number in text.
    SSR_OP_GEL,
} SSRUiOpKind;

typedef struct {
    SSRUiOpKind kind;
    char ref[48];
    char ref2[48];
    char text[512];
    int id;
} SSRUiOp;

typedef enum {
    SSR_KEY_BACKSPACE,
    SSR_KEY_ENTER,
} SSRKey;

// The options of the game (c_TPlayer.m_op*). They go into the settings
// file of the remake.
typedef struct {
    // 0 off, 1 high, 2 low.
    int volume;
    // 0 yards, 1 metres.
    int distance;
    // 1 when the game offers the replay of a goal.
    int replays;
    // 1 after the message that skills go down with no training. It shows
    // once for each install (global data 11 of the game).
    int doneLoseSkills;
    // 1 after the first career that retires. A new career can then play
    // the hard mode (global data 12 of the game).
    int hardModeUnlocked;
} SSRSettings;

// Starts at the main menu. The game keeps the tweaks, and sets the tweaks
// of each screen in them.
SSRGame *ssr_game_create(SSRTweaks *tweaks);
void ssr_game_free(SSRGame *game);

// The data of the game, the text of the language, and the directory of
// the save files. The game keeps the pointers.
void ssr_game_set_data(SSRGame *game, const SSRWorld *world, SSRLocalise localise, void *context,
                       const char *saveDirectory);

// The seed of the random numbers. The game takes the clock when it picks
// the first club, so the app gives the time.
void ssr_game_set_seed(SSRGame *game, int seed);

// The career that plays now.
const SSRCareer *ssr_game_career(const SSRGame *game);

// Reads and writes the settings file. Reading a file that is not there
// keeps the defaults. Returns 1 when it worked.
int ssr_game_load_settings(SSRGame *game, const char *path);
int ssr_game_save_settings(const SSRGame *game, const char *path);
const SSRSettings *ssr_game_settings(const SSRGame *game);

// Handles the message of a Click. Returns 1 when the game knows it.
int ssr_game_message(SSRGame *game, const char *message);

// The text of a GS_ function of a layout, for example "GS_ShowNameF", or
// NULL when the game does not know the name.
const char *ssr_game_text(SSRGame *game, const char *name);

// 1 when a name box takes the keyboard. The app sends the text that the
// player types, and Backspace and Enter.
int ssr_game_wants_text(const SSRGame *game);
void ssr_game_type(SSRGame *game, const char *text);
void ssr_game_key(SSRGame *game, SSRKey key);

// Runs the logic of the frames that went past, for example the star
// transition.
void ssr_game_update(SSRGame *game, float frames);

// The next change for the elements of the screen. Returns 0 when there is
// none. A new screen drops the changes for the screen before it.
int ssr_game_take_op(SSRGame *game, SSRUiOp *op);

// The name of the layout of the screen to show, for example "options"
// for options.uis.
const char *ssr_game_screen(const SSRGame *game);

// The layout of the overlay over the screen (a GShell like the shop), or
// "" when none shows. It takes the touches first.
const char *ssr_game_overlay(const SSRGame *game);

// The layout of the modal overlay over the overlay (c_ModalOverlay: the
// repair box, the purchase animation), or "". It takes the touches first.
const char *ssr_game_modal(const SSRGame *game);

// Grows by 1 each time that the screen changes. A screen that opens again
// counts too.
int ssr_game_screen_version(const SSRGame *game);

// 1 when the settings changed since the last call. The app saves them then.
int ssr_game_take_settings_change(SSRGame *game);

// The match that plays now, or NULL, and the chance of the player in it,
// or NULL. The screen "matchminigame" shows the chance.
const SSRMatch *ssr_game_match(const SSRGame *game);
const SSRChance *ssr_game_chance(const SSRGame *game);

// The drill of a trial of a new career, or NULL. ssr_game_chance gives its
// ball while it runs.
const SSRTraining *ssr_game_training(const SSRGame *game);

// The goal replay: 0 none, 1 the offer shows, 2 the replay plays. While it
// plays, ssr_game_chance gives the recorded frame (WatchReplay.Frame).
int ssr_game_replaying(const SSRGame *game);

// 1 while the banner of the result of a chance shows (scoreanim.uis, with
// the tweaks Minigames:Outcome and Chance:IsHead).
int ssr_game_banner(const SSRGame *game);

// 1 while the splash message of a drill shows (splashmessage.uis, with the
// tweak Menu:QuickMessage).
int ssr_game_splash(const SSRGame *game);
// The layout of the splash message, for example "splashmessagebux".
const char *ssr_game_splash_layout(const SSRGame *game);

// The touch of the player in the units of the layouts, for a chance.
void ssr_game_touch(SSRGame *game, int down, float x, float y);

#endif
