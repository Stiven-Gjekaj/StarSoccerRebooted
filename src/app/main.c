// The app: imports the copy of the player, then shows the game.
// Start it with the path of an APK, XAPK, or IPA, or drop the file on the
// window.
// On iOS the app imports the first APK, XAPK, or IPA in its Documents
// folder, where Finder and the Files app can put it. On Android it imports
// the APK of the installed New Star Soccer.
// Set SSR_SHOT to a path to save the first frame as a PNG and quit. Set
// SSR_SHOT_MS to save a later frame, that many milliseconds after the first.
// Set SSR_PRESS to "x,y" to press that point of the screen after the first
// frame. Set SSR_WINDOW to "WxH" for the size of the window in points.

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
#include "ssr_spine.h"
#include "ssr_text.h"
#include "ssr_tweaks.h"
#include "ssr_ui.h"
#include "ssr_world.h"

// The layouts are for a portrait box of 640 x 960. The game fits the box
// in the screen and centres it (c_VirtualDisplay). The rest of the screen
// is outside the box: a tall screen has a negative top.
#define SSR_WIDTH 640
#define SSR_HEIGHT 960

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
    // The Spine files that the screens use, and when the screen opened.
    struct {
        char name[64];
        SSRSpine *spine;
    } spines[16];
    int spineCount;
    Uint64 lastFrame;
    // The screen in the units of the layouts.
    float left, top, width, height;
    char gameDirectory[2048];
    char message[256];
} SSRApp;

// Reads a layout file: from the layouts of the remake first, then from the
// import. A layout of the remake takes the place of the one of the game.
static char *read_screen_file(void *context, const char *name, size_t *size) {
    const SSRApp *app = context;
    char asked[2400], path[2400];
    const char *base = SDL_GetBasePath();
    SDL_snprintf(asked, sizeof(asked), "%slayouts/%s", base ? base : "", name);
    if (!ssr_fs_resolve(asked, path, sizeof(path))) {
        SDL_snprintf(asked, sizeof(asked), "%s/NewGUI/Screens/%s", app->gameDirectory, name);
        if (!ssr_fs_resolve(asked, path, sizeof(path))) return NULL;
    }
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

static const SSRSpine *spine_file(void *context, const char *name) {
    SSRApp *app = context;
    for (int i = 0; i < app->spineCount; i++) {
        if (SDL_strcmp(app->spines[i].name, name) == 0) return app->spines[i].spine;
    }
    if (app->spineCount >= (int)SDL_arraysize(app->spines)) return NULL;
    char path[2400];
    SDL_snprintf(path, sizeof(path), "%s/Spine/%s.spinary", app->gameDirectory, name);
    SSRSpine *spine = ssr_spine_load(path);
    // A missing file is kept too, so that the app does not look for it again.
    SDL_strlcpy(app->spines[app->spineCount].name, name, sizeof(app->spines[0].name));
    app->spines[app->spineCount++].spine = spine;
    return spine;
}

static void open_screen(SSRApp *app, const char *name) {
    ssr_scene_free(app->scene);
    app->scene = NULL;
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
    ssr_gfx_set_offset(app->gfx, -app->left, -app->top);
    char path[2300];
    SDL_snprintf(path, sizeof(path), "%s/MyData/%s", app->gameDirectory, SSR_WORLD_DEFAULT_SET);
    app->world = ssr_world_load(path);
    if (!app->world) SDL_snprintf(app->message, sizeof(app->message), "The import has no data set %s.", SSR_WORLD_DEFAULT_SET);
    // The tweaks start with the values of the game.
    SDL_snprintf(path, sizeof(path), "%s/Tweaks/TweakValues.txt", app->gameDirectory);
    if (ssr_tweaks_load(app->tweaks, path) < 0) SDL_Log("The import has no Tweaks/TweakValues.txt.");
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

#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>

// Imports the APK of the installed New Star Soccer. Returns 1 when the game
// is installed.
static int import_installed_game(SSRApp *app) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return 0;
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, type, "installedGamePath", "()Ljava/lang/String;");
    jstring path = method ? (jstring)(*env)->CallStaticObjectMethod(env, type, method) : NULL;
    int found = 0;
    if (path) {
        const char *text = (*env)->GetStringUTFChars(env, path, NULL);
        if (text) {
            import_copy(app, text);
            found = 1;
            (*env)->ReleaseStringUTFChars(env, path, text);
        }
        (*env)->DeleteLocalRef(env, path);
    }
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
    return found;
}
#endif

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

// Fits the 640 x 960 box of the layouts in the window and centres it, as
// c_VirtualDisplay.UpdateVirtualDisplay of the game does.
static void fit_screen(SSRApp *app) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(app->renderer, &w, &h);
    if (w <= 0 || h <= 0) {
        w = SSR_WIDTH;
        h = SSR_HEIGHT;
    }
    float scale = (float)h / w > (float)SSR_HEIGHT / SSR_WIDTH ? (float)w / SSR_WIDTH : (float)h / SSR_HEIGHT;
    app->width = w / scale;
    app->height = h / scale;
    app->left = -((w - SSR_WIDTH * scale) * 0.5f) / scale;
    app->top = -((h - SSR_HEIGHT * scale) * 0.5f) / scale;
    SDL_SetRenderLogicalPresentation(app->renderer, (int)(app->width + 0.5f), (int)(app->height + 0.5f),
                                     SDL_LOGICAL_PRESENTATION_STRETCH);
    ssr_gfx_set_offset(app->gfx, -app->left, -app->top);
    // The layout of a screen depends on the size, so the screen opens again.
    ssr_scene_free(app->scene);
    app->scene = NULL;
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
    int windowW = SSR_WIDTH * 3 / 4, windowH = SSR_HEIGHT * 3 / 4;
    const char *size = SDL_getenv("SSR_WINDOW");
    if (size && SDL_sscanf(size, "%dx%d", &windowW, &windowH) != 2) {
        windowW = SSR_WIDTH * 3 / 4;
        windowH = SSR_HEIGHT * 3 / 4;
    }
    if (!SDL_CreateWindowAndRenderer("Star Soccer Rebooted", windowW, windowH,
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
#ifdef SDL_PLATFORM_ANDROID
    else if (!import_installed_game(app)) {
        SDL_snprintf(app->message, sizeof(app->message), "Install New Star Soccer from Google Play first.");
    }
#endif
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
    SSRApp *app = state;
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) fit_screen(app);
    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data) import_copy(app, event->drop.data);
    int mouse = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP ||
                event->type == SDL_EVENT_MOUSE_MOTION;
    int finger = event->type == SDL_EVENT_FINGER_DOWN || event->type == SDL_EVENT_FINGER_UP ||
                 event->type == SDL_EVENT_FINGER_MOTION;
    // A touch makes mouse events too. The app takes the touch only.
    if (mouse && event->type != SDL_EVENT_MOUSE_MOTION && event->button.which == SDL_TOUCH_MOUSEID) mouse = 0;
    if (mouse && event->type == SDL_EVENT_MOUSE_MOTION && event->motion.which == SDL_TOUCH_MOUSEID) mouse = 0;
    if (mouse || finger) {
        SDL_ConvertEventToRenderCoordinates(app->renderer, event);
        float x, y;
        int down;
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            x = event->motion.x;
            y = event->motion.y;
            down = (event->motion.state & SDL_BUTTON_LMASK) != 0;
        } else if (mouse) {
            x = event->button.x;
            y = event->button.y;
            down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        } else {
            x = event->tfinger.x;
            y = event->tfinger.y;
            down = event->type != SDL_EVENT_FINGER_UP;
        }
        ssr_scene_touch(app->scene, down, x + app->left, y + app->top);
        // A click is when the touch goes up on the element, as in the game.
        int up = event->type == SDL_EVENT_MOUSE_BUTTON_UP || event->type == SDL_EVENT_FINGER_UP;
        const SSRSceneItem *item = up ? ssr_scene_hit_item(app->scene, x + app->left, y + app->top) : NULL;
        if (item) {
            SDL_Log("Pressed %s (%s)", item->ref ? item->ref : "-", item->click ? item->click : "-");
            ssr_audio_play(app->audio, item->clickSound ? item->clickSound : "UISelect");
        }
    }
    return SDL_APP_CONTINUE;
}

static void draw_centered_text(const SSRApp *app, float y, const char *text) {
    float width = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_RenderDebugText(app->renderer, (app->width - width) / 2, y, text);
}

SDL_AppResult SDL_AppIterate(void *state) {
    SSRApp *app = state;
    SDL_SetRenderDrawColor(app->renderer, 34, 120, 52, 255);
    SDL_RenderClear(app->renderer);
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);

    if (app->screen) {
        SSRSceneEnv env = {
            .left = app->left,
            .top = app->top,
            .width = app->width,
            .height = app->height,
            .tweaks = app->tweaks,
            .text = translate,
            .image_size = image_size,
            .spine = spine_file,
            .context = app,
        };
        Uint64 now = SDL_GetTicks();
        int ms = 0;
        if (!app->scene) {
            app->scene = ssr_scene_open(app->screen, &env);
        } else {
            ms = (int)(now - app->lastFrame);
        }
        app->lastFrame = now;
        ssr_scene_update(app->scene, &env, ms);
        ssr_gfx_set_tweaks(app->gfx, app->tweaks);
        ssr_gfx_draw_scene(app->gfx, app->scene);
    }
    if (!app->gfx) {
        draw_centered_text(app, app->height / 2 - 12, "Drop your copy of New Star Soccer here.");
        draw_centered_text(app, app->height / 2 + 4, "The app reads an APK, an XAPK, or an IPA.");
#ifdef SDL_PLATFORM_IOS
        draw_centered_text(app, app->height / 2 + 20, "Put the file in Documents with Finder, then restart.");
#endif
    }
    if (app->message[0]) draw_centered_text(app, app->height - 40, app->message);

    const char *press = SDL_getenv("SSR_PRESS");
    static int pressed = 0;
    if (press && *press && !pressed && app->scene) {
        pressed = 1;
        float x = 0, y = 0;
        if (SDL_sscanf(press, "%f,%f", &x, &y) == 2) {
            const SSRSceneItem *item = ssr_scene_hit_item(app->scene, x + app->left, y + app->top);
            SDL_Log("Pressed %s (%s)", item && item->ref ? item->ref : "-", item && item->click ? item->click : "-");
            if (item) ssr_audio_play(app->audio, item->clickSound ? item->clickSound : "UISelect");
        }
    }

    const char *shot = SDL_getenv("SSR_SHOT");
    const char *shotMs = SDL_getenv("SSR_SHOT_MS");
    static Uint64 firstFrame = 0;
    if (!firstFrame) firstFrame = SDL_GetTicks();
    int wait = shotMs ? SDL_atoi(shotMs) : 0;
    if (shot && *shot && SDL_GetTicks() - firstFrame >= (Uint64)wait) {
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
    for (int i = 0; i < app->spineCount; i++) ssr_spine_free(app->spines[i].spine);
    ssr_ui_free(app->screen);
    ssr_text_free(app->text);
    ssr_tweaks_free(app->tweaks);
    ssr_audio_free(app->audio);
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    SDL_free(app);
}
