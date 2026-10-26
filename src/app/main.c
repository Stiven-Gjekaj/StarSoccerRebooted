// The app: imports the copy of the player, then shows the game.
// Start it with the path of an APK, XAPK, or IPA, or drop the file on the
// window.
// Set SSR_SHOT to a path to save the first frame as a PNG and quit.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ssr_import.h"

// The game is portrait. The 2x images fill 640 x 960 pixels.
#define SSR_WIDTH 640
#define SSR_HEIGHT 960

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logo;
    char gameDirectory[2048];
    char message[256];
} SSRApp;

static void load_logo(SSRApp *app) {
    char path[2300];
    SDL_snprintf(path, sizeof(path), "%s/Images2x/NSS_Watermark.png", app->gameDirectory);
    SDL_Surface *surface = SDL_LoadPNG(path);
    if (!surface) {
        SDL_snprintf(app->message, sizeof(app->message), "The import has no logo: %s", SDL_GetError());
        return;
    }
    app->logo = SDL_CreateTextureFromSurface(app->renderer, surface);
    SDL_DestroySurface(surface);
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
    load_logo(app);
}

SDL_AppResult SDL_AppInit(void **state, int argc, char *argv[]) {
    SSRApp *app = SDL_calloc(1, sizeof(SSRApp));
    if (!app) return SDL_APP_FAILURE;
    *state = app;

    SDL_SetAppMetadata("Star Soccer Rebooted", "0.1.0", "io.github.stivengjekaj.starsoccerrebooted");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("Star Soccer Rebooted", SSR_WIDTH * 3 / 4, SSR_HEIGHT * 3 / 4,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &app->window, &app->renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(app->renderer, SSR_WIDTH, SSR_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(app->renderer, 1);

    char *pref = SDL_GetPrefPath("StivenGjekaj", "StarSoccerRebooted");
    if (!pref) return SDL_APP_FAILURE;
    SDL_snprintf(app->gameDirectory, sizeof(app->gameDirectory), "%sgame", pref);
    SDL_free(pref);

    if (argc > 1) {
        import_copy(app, argv[1]);
    } else if (ssr_import_is_done(app->gameDirectory)) {
        load_logo(app);
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
    SSRApp *app = state;
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data) import_copy(app, event->drop.data);
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

    if (app->logo) {
        float width = 0, height = 0;
        SDL_GetTextureSize(app->logo, &width, &height);
        SDL_FRect place = {(SSR_WIDTH - width) / 2, (SSR_HEIGHT - height) / 2, width, height};
        SDL_RenderTexture(app->renderer, app->logo, NULL, &place);
    } else {
        draw_centered_text(app->renderer, SSR_HEIGHT / 2 - 12, "Drop your copy of New Star Soccer here.");
        draw_centered_text(app->renderer, SSR_HEIGHT / 2 + 4, "The app reads an APK, an XAPK, or an IPA.");
    }
    if (app->message[0]) draw_centered_text(app->renderer, SSR_HEIGHT - 40, app->message);

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
    if (app->logo) SDL_DestroyTexture(app->logo);
    SDL_free(app);
}
