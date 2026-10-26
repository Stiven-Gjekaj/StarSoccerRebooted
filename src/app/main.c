// The app: imports the copy of the player, then shows the game.
// Start it with the path of an APK, XAPK, or IPA, or drop the file on the
// window.
// On iOS the app imports the first APK, XAPK, or IPA in its Documents
// folder, where Finder and the Files app can put it.
// Set SSR_SHOT to a path to save the first frame as a PNG and quit.
// Set SSR_PRESS to "x,y" to press that point of the screen after the first
// frame.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_audio.h"
#include "ssr_fs.h"
#include "ssr_gfx.h"
#include "ssr_import.h"
#include "ssr_scene.h"
#include "ssr_text.h"
#include "ssr_tweaks.h"
#include "ssr_ui.h"
#include "ssr_world.h"

// The game is portrait. The 2x images fill 640 x 960 pixels. On a taller
// screen the width stays 640 and the height grows, as in the game. The
// layouts place things from the bottom with @B.
#define SSR_WIDTH 640
#define SSR_MIN_HEIGHT 960

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SSRGfx *gfx;
    SSRWorld *world;
    SSRText *text;
    SSRTweaks *tweaks;
    // The screen that shows now, from NewGUI/Screens, and its last frame.
    SSRUiTree *screen;
    SSRScene *scene;
    SSRAudio *audio;
    // The height of the screen in logical pixels.
    int height;
    char gameDirectory[2048];
    char message[256];
} SSRApp;

static char *read_screen_file(void *context, const char *name, size_t *size) {
    const SSRApp *app = context;
    char asked[2400], path[2400];
    SDL_snprintf(asked, sizeof(asked), "%s/NewGUI/Screens/%s", app->gameDirectory, name);
    if (!ssr_fs_resolve(asked, path, sizeof(path))) return NULL;
    char *data = SDL_LoadFile(path, size);
    if (!data) return NULL;
    // The parser frees the buffer with free(), so it gets a copy.
    char *copy = malloc(*size + 1);
    if (copy) {
        memcpy(copy, data, *size);
        copy[*size] = 0;
    }
    SDL_free(data);
    return copy;
}

static const char *translate(void *context, const char *tag) {
    const SSRApp *app = context;
    return app->text ? ssr_text_get(app->text, tag, 0) : tag;
}

static int image_size(void *context, const char *name, const char *file, float *w, float *h) {
    const SSRApp *app = context;
    return ssr_gfx_image_size(app->gfx, name, file, w, h);
}

static void open_screen(SSRApp *app, const char *name) {
    ssr_ui_free(app->screen);
    app->screen = NULL;
    SSRUiFiles files = {read_screen_file, app};
    app->screen = ssr_ui_parse(name, &files);
    if (!app->screen) {
        SDL_Log("The screen %s is not there.", name);
        return;
    }
    for (int i = 0; i < ssr_ui_problem_count(app->screen); i++) SDL_Log("%s", ssr_ui_problem(app->screen, i));
}

static void load_game(SSRApp *app) {
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    ssr_audio_free(app->audio);
    app->audio = ssr_audio_create(app->gameDirectory);
    app->gfx = ssr_gfx_create(app->renderer, app->gameDirectory);
    char path[2300];
    SDL_snprintf(path, sizeof(path), "%s/MyData/%s", app->gameDirectory, SSR_WORLD_DEFAULT_SET);
    app->world = ssr_world_load(path);
    if (!app->world) SDL_snprintf(app->message, sizeof(app->message), "The import has no data set %s.", SSR_WORLD_DEFAULT_SET);
    ssr_text_free(app->text);
    app->text = NULL;
    SDL_snprintf(path, sizeof(path), "%s/MyData/Languages/en.txt", app->gameDirectory);
    app->text = ssr_text_load(path);
    const char *screen = SDL_getenv("SSR_SCREEN");
    open_screen(app, screen && *screen ? screen : "MainMenu.uis");
}

static void import_copy(SSRApp *app, const char *archive) {
    int files = 0;
    SSRSource source = ssr_import(archive, app->gameDirectory, &files);
    if (source == SSR_SOURCE_UNKNOWN) {
        SDL_snprintf(app->message, sizeof(app->message), "This file is not a copy of New Star Soccer.");
        return;
    }
    SDL_Log("Imported %d files from the %s.", files, ssr_source_name(source));
    app->message[0] = 0;
    load_game(app);
}

#ifdef SDL_PLATFORM_IOS
// Imports the first copy of the game in the Documents folder. Returns 1
// when there was one.
static int import_from_documents(SSRApp *app) {
    const char *documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (!documents) return 0;
    static const char *patterns[] = {"*.xapk", "*.apk", "*.ipa"};
    for (size_t i = 0; i < SDL_arraysize(patterns); i++) {
        int count = 0;
        char **names = SDL_GlobDirectory(documents, patterns[i], SDL_GLOB_CASEINSENSITIVE, &count);
        if (names && count > 0) {
            char path[2400];
            SDL_snprintf(path, sizeof(path), "%s%s", documents, names[0]);
            SDL_free(names);
            import_copy(app, path);
            return 1;
        }
        SDL_free(names);
    }
    return 0;
}
#endif

// Sets the logical height from the shape of the window.
static void fit_screen(SSRApp *app) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(app->renderer, &w, &h);
    app->height = SSR_MIN_HEIGHT;
    if (w > 0 && h * SSR_WIDTH > w * SSR_MIN_HEIGHT) app->height = h * SSR_WIDTH / w;
    SDL_SetRenderLogicalPresentation(app->renderer, SSR_WIDTH, app->height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

SDL_AppResult SDL_AppInit(void **state, int argc, char *argv[]) {
    SSRApp *app = SDL_calloc(1, sizeof(SSRApp));
    if (!app) return SDL_APP_FAILURE;
    *state = app;

    SDL_SetAppMetadata("Star Soccer Rebooted", SSR_VERSION, "io.github.stivengjekaj.starsoccerrebooted");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("Star Soccer Rebooted", SSR_WIDTH * 3 / 4, SSR_MIN_HEIGHT * 3 / 4,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &app->window, &app->renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    fit_screen(app);
    SDL_SetRenderVSync(app->renderer, 1);

    char *pref = SDL_GetPrefPath("StivenGjekaj", "StarSoccerRebooted");
    if (!pref) return SDL_APP_FAILURE;
    SDL_snprintf(app->gameDirectory, sizeof(app->gameDirectory), "%sgame", pref);
    SDL_free(pref);
    app->tweaks = ssr_tweaks_create();

    if (argc > 1) {
        import_copy(app, argv[1]);
    } else if (ssr_import_is_done(app->gameDirectory)) {
        load_game(app);
    }
#ifdef SDL_PLATFORM_IOS
    else {
        import_from_documents(app);
    }
#endif
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
    SSRApp *app = state;
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) fit_screen(app);
    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data) import_copy(app, event->drop.data);
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_FINGER_DOWN) {
        SDL_ConvertEventToRenderCoordinates(app->renderer, event);
        float x = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? event->button.x : event->tfinger.x;
        float y = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? event->button.y : event->tfinger.y;
        const SSRSceneItem *item = ssr_scene_hit_item(app->scene, x, y);
        if (item) {
            SDL_Log("Pressed %s (%s)", item->ref ? item->ref : "-", item->click ? item->click : "-");
            ssr_audio_play(app->audio, item->clickSound ? item->clickSound : "UISelect");
        }
    }
    return SDL_APP_CONTINUE;
}

static void draw_centered_text(SDL_Renderer *renderer, float y, const char *text) {
    float width = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_RenderDebugText(renderer, (SSR_WIDTH - width) / 2, y, text);
}

SDL_AppResult SDL_AppIterate(void *state) {
    SSRApp *app = state;
    SDL_SetRenderDrawColor(app->renderer, 34, 120, 52, 255);
    SDL_RenderClear(app->renderer);
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);

    if (app->screen) {
        SSRSceneEnv env = {SSR_WIDTH, (float)app->height, app->tweaks, translate, image_size, app};
        ssr_scene_free(app->scene);
        app->scene = ssr_scene_build(app->screen, &env);
        ssr_gfx_draw_scene(app->gfx, app->scene);
    }
    if (!app->gfx) {
        draw_centered_text(app->renderer, app->height / 2 - 12, "Drop your copy of New Star Soccer here.");
        draw_centered_text(app->renderer, app->height / 2 + 4, "The app reads an APK, an XAPK, or an IPA.");
#ifdef SDL_PLATFORM_IOS
        draw_centered_text(app->renderer, app->height / 2 + 20, "Put the file in Documents with Finder, then restart.");
#endif
    }
    if (app->message[0]) draw_centered_text(app->renderer, app->height - 40, app->message);

    const char *press = SDL_getenv("SSR_PRESS");
    static int pressed = 0;
    if (press && *press && !pressed && app->scene) {
        pressed = 1;
        float x = 0, y = 0;
        if (SDL_sscanf(press, "%f,%f", &x, &y) == 2) {
            const SSRSceneItem *item = ssr_scene_hit_item(app->scene, x, y);
            SDL_Log("Pressed %s (%s)", item && item->ref ? item->ref : "-", item && item->click ? item->click : "-");
            if (item) ssr_audio_play(app->audio, item->clickSound ? item->clickSound : "UISelect");
        }
    }

    const char *shot = SDL_getenv("SSR_SHOT");
    if (shot && *shot) {
        SDL_Surface *frame = SDL_RenderReadPixels(app->renderer, NULL);
        bool saved = frame && SDL_SavePNG(frame, shot);
        SDL_DestroySurface(frame);
        if (!saved) SDL_Log("The frame was not saved: %s", SDL_GetError());
        return saved ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
    }
    SDL_RenderPresent(app->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *state, SDL_AppResult result) {
    (void)result;
    SSRApp *app = state;
    if (!app) return;
    ssr_scene_free(app->scene);
    ssr_ui_free(app->screen);
    ssr_text_free(app->text);
    ssr_tweaks_free(app->tweaks);
    ssr_audio_free(app->audio);
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    SDL_free(app);
}
