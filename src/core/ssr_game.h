// ssr_game: the logic of the screens, as the TScreen classes of the game
// have it. A Click(message) of a layout comes here. The logic changes the
// state of the game, sets the tweaks that the layouts read, and chooses the
// next screen. It draws nothing.

#ifndef SSR_GAME_H
#define SSR_GAME_H

#include "ssr_career.h"
#include "ssr_match.h"
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

// Grows by 1 each time that the screen changes. A screen that opens again
// counts too.
int ssr_game_screen_version(const SSRGame *game);

// 1 when the settings changed since the last call. The app saves them then.
int ssr_game_take_settings_change(SSRGame *game);

// The match that plays now, or NULL, and the chance of the player in it,
// or NULL. The screen "matchchance" shows the chance.
const SSRMatch *ssr_game_match(const SSRGame *game);
const SSRChance *ssr_game_chance(const SSRGame *game);

// The touch of the player in the units of the layouts, for a chance.
void ssr_game_touch(SSRGame *game, int down, float x, float y);

#endif
