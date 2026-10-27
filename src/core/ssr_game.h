// ssr_game: the logic of the screens, as the TScreen classes of the game
// have it. A Click(message) of a layout comes here. The logic changes the
// state of the game, sets the tweaks that the layouts read, and chooses the
// next screen. It draws nothing.

#ifndef SSR_GAME_H
#define SSR_GAME_H

#include "ssr_tweaks.h"

typedef struct SSRGame SSRGame;

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

// Reads and writes the settings file. Reading a file that is not there
// keeps the defaults. Returns 1 when it worked.
int ssr_game_load_settings(SSRGame *game, const char *path);
int ssr_game_save_settings(const SSRGame *game, const char *path);
const SSRSettings *ssr_game_settings(const SSRGame *game);

// Handles the message of a Click. Returns 1 when the game knows it.
int ssr_game_message(SSRGame *game, const char *message);

// The name of the layout of the screen to show, for example "options"
// for options.uis.
const char *ssr_game_screen(const SSRGame *game);

// Grows by 1 each time that the screen changes. A screen that opens again
// counts too.
int ssr_game_screen_version(const SSRGame *game);

// 1 when the settings changed since the last call. The app saves them then.
int ssr_game_take_settings_change(SSRGame *game);

#endif
