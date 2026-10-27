// Tests ssr_game: the messages of the screens and the settings file.

#include <stdio.h>
#include <string.h>

#include "ssr_game.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static void test_goes_between_screens(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRGame *game = ssr_game_create(tweaks);
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);
    int version = ssr_game_screen_version(game);
    CHECK(ssr_game_message(game, "mainmenu.btn_Options"));
    CHECK(strcmp(ssr_game_screen(game), "options") == 0 && ssr_game_screen_version(game) == version + 1);
    CHECK(ssr_game_message(game, "options.btn_About"));
    CHECK(strcmp(ssr_game_screen(game), "about") == 0);
    CHECK(ssr_game_message(game, "about.btn_Ok"));
    CHECK(ssr_game_message(game, "options.btn_Back"));
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);
    CHECK(ssr_game_message(game, "mainmenu.btn_Career"));
    CHECK(strcmp(ssr_game_screen(game), "saveslot") == 0);
    CHECK(!ssr_game_message(game, "no.such_message"));
    ssr_game_free(game);
    ssr_tweaks_free(tweaks);
}

// The options go round and set the tweaks that options.uis shows.
static void test_changes_and_saves_the_options(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRGame *game = ssr_game_create(tweaks);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "SettingsVolume") == 1);
    CHECK(!ssr_game_take_settings_change(game));
    ssr_game_message(game, "options.btn_Volume");
    CHECK(ssr_tweaks_number(tweaks, "Menu", "SettingsVolume") == 2);
    ssr_game_message(game, "options.btn_Volume");
    CHECK(ssr_tweaks_number(tweaks, "Menu", "SettingsVolume") == 0);
    ssr_game_message(game, "options.btn_Distance");
    CHECK(ssr_tweaks_number(tweaks, "Menu", "SettingsDistance") == 1);
    CHECK(ssr_game_take_settings_change(game) && !ssr_game_take_settings_change(game));

    char path[1200];
    snprintf(path, sizeof(path), "%s/settings.txt", ssr_test_directory("game_settings"));
    remove(path);
    CHECK(!ssr_game_load_settings(game, path));
    CHECK(ssr_game_save_settings(game, path));
    ssr_game_free(game);

    SSRTweaks *other = ssr_tweaks_create();
    game = ssr_game_create(other);
    CHECK(ssr_game_settings(game)->volume == 1);
    CHECK(ssr_game_load_settings(game, path));
    CHECK(ssr_game_settings(game)->volume == 0 && ssr_game_settings(game)->distance == 1);
    CHECK(ssr_tweaks_number(other, "Menu", "SettingsDistance") == 1);
    ssr_game_free(game);
    ssr_tweaks_free(other);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_goes_between_screens();
    test_changes_and_saves_the_options();
    return TEST_RESULT();
}
