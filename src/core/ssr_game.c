#include "ssr_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

struct SSRGame {
    SSRTweaks *tweaks;
    SSRSettings settings;
    char screen[64];
    int version;
    int settingsChanged;
};

typedef void (*Handler)(SSRGame *game);

typedef struct {
    const char *message;
    Handler handler;
} Route;

// Opens the screen with the layout name.uis (c_TScreen.SetActive).
static void set_screen(SSRGame *game, const char *name) {
    snprintf(game->screen, sizeof(game->screen), "%s", name);
    game->version++;
}

// The tweaks that the options screen reads (c_TScreen_Options.Update*).
static void set_option_tweaks(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsVolume", game->settings.volume);
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsDistance", game->settings.distance);
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsReplays", game->settings.replays);
}

static void settings_changed(SSRGame *game) {
    game->settingsChanged = 1;
    set_option_tweaks(game);
}

static void main_menu(SSRGame *game) {
    set_screen(game, "mainmenu");
}

static void options(SSRGame *game) {
    set_option_tweaks(game);
    set_screen(game, "options");
}

static void career(SSRGame *game) {
    set_screen(game, "saveslot");
}

// Off, high, low, then off again.
static void volume(SSRGame *game) {
    game->settings.volume = (game->settings.volume + 1) % 3;
    settings_changed(game);
}

static void distance(SSRGame *game) {
    game->settings.distance = !game->settings.distance;
    settings_changed(game);
}

static void replays(SSRGame *game) {
    game->settings.replays = !game->settings.replays;
    settings_changed(game);
}

static void about(SSRGame *game) {
    set_screen(game, "about");
}

static const Route ROUTES[] = {
    {"mainmenu.btn_Career", career},
    {"mainmenu.btn_Options", options},
    {"options.btn_Volume", volume},
    {"options.btn_Distance", distance},
    {"options.btn_OfferReplay", replays},
    {"options.btn_About", about},
    {"options.btn_Back", main_menu},
    {"about.btn_Ok", options},
};

SSRGame *ssr_game_create(SSRTweaks *tweaks) {
    SSRGame *game = calloc(1, sizeof(SSRGame));
    if (!game) return NULL;
    game->tweaks = tweaks;
    game->settings.volume = 1;
    game->settings.replays = 1;
    set_option_tweaks(game);
    set_screen(game, "mainmenu");
    return game;
}

void ssr_game_free(SSRGame *game) {
    free(game);
}

int ssr_game_load_settings(SSRGame *game, const char *path) {
    if (!game || !path) return 0;
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[64];
        int value;
        if (sscanf(line, "%63[^=]=%d", key, &value) != 2) continue;
        if (strcmp(key, "volume") == 0 && value >= 0 && value <= 2) game->settings.volume = value;
        if (strcmp(key, "distance") == 0) game->settings.distance = value != 0;
        if (strcmp(key, "replays") == 0) game->settings.replays = value != 0;
    }
    fclose(file);
    set_option_tweaks(game);
    return 1;
}

int ssr_game_save_settings(const SSRGame *game, const char *path) {
    if (!game || !path) return 0;
    // The new file takes the place of the old one only when it is complete.
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return 0;
    int ok = fprintf(file, "volume=%d\ndistance=%d\nreplays=%d\n", game->settings.volume, game->settings.distance,
                     game->settings.replays) > 0;
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

const SSRSettings *ssr_game_settings(const SSRGame *game) {
    return game ? &game->settings : NULL;
}

int ssr_game_message(SSRGame *game, const char *message) {
    if (!game || !message) return 0;
    for (size_t i = 0; i < sizeof(ROUTES) / sizeof(ROUTES[0]); i++) {
        if (strcmp(ROUTES[i].message, message) == 0) {
            ROUTES[i].handler(game);
            return 1;
        }
    }
    return 0;
}

const char *ssr_game_text(SSRGame *game, const char *name) {
    (void)game;
    (void)name;
    return NULL;
}

const char *ssr_game_screen(const SSRGame *game) {
    return game ? game->screen : "";
}

int ssr_game_screen_version(const SSRGame *game) {
    return game ? game->version : 0;
}

int ssr_game_take_settings_change(SSRGame *game) {
    if (!game || !game->settingsChanged) return 0;
    game->settingsChanged = 0;
    return 1;
}
