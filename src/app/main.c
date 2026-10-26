// The app: imports the copy of the player, then shows the game.
// Start it with the path of an APK, XAPK, or IPA, or drop the file on the
// window.
// Set SSR_SHOT to a path to save the first frame as a PNG and quit.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ssr_gfx.h"
#include "ssr_import.h"
#include "ssr_world.h"

// The game is portrait. The 2x images fill 640 x 960 pixels.
#define SSR_WIDTH 640
#define SSR_HEIGHT 960

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SSRGfx *gfx;
    SSRWorld *world;
    char gameDirectory[2048];
    char message[256];
} SSRApp;

static void load_game(SSRApp *app) {
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    app->gfx = ssr_gfx_create(app->renderer, app->gameDirectory);
    char path[2300];
    SDL_snprintf(path, sizeof(path), "%s/MyData/%s", app->gameDirectory, SSR_WORLD_DEFAULT_SET);
    app->world = ssr_world_load(path);
    if (!app->world) SDL_snprintf(app->message, sizeof(app->message), "The import has no data set %s.", SSR_WORLD_DEFAULT_SET);
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

SDL_AppResult SDL_AppInit(void **state, int argc, char *argv[]) {
    SSRApp *app = SDL_calloc(1, sizeof(SSRApp));
    if (!app) return SDL_APP_FAILURE;
    *state = app;

    SDL_SetAppMetadata("Star Soccer Rebooted", SSR_VERSION, "io.github.stivengjekaj.starsoccerrebooted");
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
        load_game(app);
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

    SDL_Texture *background = ssr_gfx_texture(app->gfx, "Images2x/Backgrounds/Default.jpg");
    if (background) {
        // The background is white. The game tints it.
        SDL_SetTextureColorMod(background, 34, 120, 52);
        SDL_RenderTexture(app->renderer, background, NULL, NULL);
    }
    SDL_Texture *logo = ssr_gfx_texture(app->gfx, "Images2x/NSS_Watermark.png");
    if (logo) {
        float width = 0, height = 0;
        SDL_GetTextureSize(logo, &width, &height);
        SDL_FRect place = {(SSR_WIDTH - width) / 2, 160, width, height};
        SDL_RenderTexture(app->renderer, logo, NULL, &place);
    }
    ssr_gfx_text(app->gfx, SSR_FONT_LARGE, "STAR SOCCER REBOOTED", SSR_WIDTH / 2.0f, 300, 0xFFFFFF, SSR_ALIGN_CENTER);
    if (app->world) {
        char line[128];
        SDL_snprintf(line, sizeof(line), "%d clubs in %d nations", app->world->clubCount, app->world->nationCount);
        ssr_gfx_text(app->gfx, SSR_FONT_MEDIUM, line, SSR_WIDTH / 2.0f, 420, 0xFFE066, SSR_ALIGN_CENTER);
        ssr_gfx_text(app->gfx, SSR_FONT_SMALL, "The 2025 data of your copy", SSR_WIDTH / 2.0f, 480, 0xDDEEDD,
                     SSR_ALIGN_CENTER);
    }
    if (!app->gfx) {
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
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    SDL_free(app);
}
