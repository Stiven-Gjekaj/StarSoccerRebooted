// The app: imports the copy of the player, then shows the game.
// Start it with the path of an APK, XAPK, or IPA, or drop the file on the
// window.
// On iOS the app imports the first APK, XAPK, or IPA in its Documents
// folder, where Finder and the Files app can put it. On Android it imports
// the APK of the installed New Star Soccer.
// Set SSR_SHOT to a path to save the first frame as a PNG and quit. Set
// SSR_SHOT_MS to save a later frame, that many milliseconds after the first.
// Set SSR_PRESS to "x,y" to press that point of the screen 1.5 seconds
// after the first frame. More presses go after ';', 1.5 seconds apart, and
// "type:text" types the text. Set SSR_WINDOW to "WxH" for the size of the window in points.
// Set SSR_AUTOCHANCE to "dx,dy" to play each chance of the player with a
// drag of (dx, dy) from the ball. Set SSR_SHOT_TAP to save the frame of
// the first tap ball, and SSR_SHOT_DRIBBLE for the first dribble.
// Set SSR_MESSAGES to messages of the game, for example
// "mainmenu.btn_Career;saveslot.btn_Slot1", to send them 1.5 seconds apart
// as the presses go.
// Set SSR_AUTOMATCH to 1 to press Play each time that the match waits, to 2
// to also go on from the match stats, or to 3 to open the ratings from
// them, or to 4 to press Ok on each message box (the trials). Each of them
// also watches the replay of a goal. Set SSR_SHOT_SCREEN
// to a screen name to save the frame SSR_SHOT_MS after that screen opens,
// or SSR_SHOT_REPLAY to save it 1.5 s after the first replay starts.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_audio.h"
#include "ssr_fs.h"
#include "ssr_game.h"
#include "ssr_ticker.h"
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
    // The logic of the screens, and the screen that it showed last.
    SSRGame *game;
    int gameScreen;
    char settingsPath[2048];
    char saveDirectory[2048];
    int typing;
    // The screen that shows now, from NewGUI/Screens, and its last frame.
    SSRUiTree *screen;
    SSRScene *scene;
    // The ticker shell over the screen (ticker.uis).
    SSRUiTree *tickerScreen;
    SSRScene *tickerScene;
    SSRTicker *ticker;
    Uint64 tickerFrame;
    // A chance of the player: the pitch under the screen, the tap ball over
    // it, and the copies of the players and of the ball on the screen.
    SSRUiTree *pitchScreen, *tapScreen, *bannerScreen, *stadiumScreen, *stadiumStaticScreen;
    SSRScene *pitchScene, *tapScene, *bannerScene, *stadiumScene, *stadiumStaticScene;
    // The HUD of a chance (minigamehud.uis): the replay offer and controls.
    SSRScene *hudScene;
    SSRUiTree *hudScreen;
    char pitchName[64];
    SSRScene *viewScene;
    int playerCopies[SSR_PLAYERS_MAX];
    // The run arrow of each team-mate (c_MatchCaptainArrow).
    int runArrows[SSR_PLAYERS_MAX];
    // The trail of the ball (c_BallTrailFXParticles): 40 faint copies of
    // the ball, the time of each, and its scale at the start.
    int trail[40];
    Uint64 trailTime[40];
    float trailScale[40];
    // The views of the objects of a drill (c_ATraining).
    int objectCopies[SSR_TRAINING_OBJECTS];
    int objectCount;
    // The splash message of a drill (splashmessage.uis).
    SSRScene *splashScene;
    SSRUiTree *splashScreen;
    int ballCopy, arrowCopy;
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

// The text of a tag. A female player gets the female text of the language
// (c_TLocale with the gender column).
static const char *translate(void *context, const char *tag) {
    const SSRApp *app = context;
    const SSRCareer *career = ssr_game_career(app->game);
    return app->text ? ssr_text_get(app->text, tag, career && career->gender == 1) : tag;
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

// The volume of the options: off, high, or low (c_TScreen_Options).
static void apply_settings(SSRApp *app) {
    const SSRSettings *settings = ssr_game_settings(app->game);
    if (!settings) return;
    ssr_audio_set_volume(app->audio, settings->volume == 0 ? 0.0f : settings->volume == 2 ? 0.5f : 1.0f);
}

// Opens the screen that the logic chose, and saves the changed settings.
static void follow_game(SSRApp *app) {
    if (!app->game) return;
    if (ssr_game_take_settings_change(app->game)) {
        if (!ssr_game_save_settings(app->game, app->settingsPath)) SDL_Log("The settings were not saved.");
        apply_settings(app);
    }
    int version = ssr_game_screen_version(app->game);
    if (version == app->gameScreen) return;
    app->gameScreen = version;
    char name[80];
    SDL_snprintf(name, sizeof(name), "%s.uis", ssr_game_screen(app->game));
    open_screen(app, name);
}

static SSRSceneEnv scene_env(SSRApp *app);

/* ------------------------------------------------------- the chance */

// The pitch of a chance (c_MatchChance.p_GetChanceScreenName): "Pitch_",
// Pen, A, or M, the side letter, "_AM", and "_Rev" for a mirror. Side 0 is
// "a", side 1 is "b", and side 2 is "a" in a mirror. A corner or a wing
// cross is "c", in a mirror at an odd minute.
static void pitch_name(const SSRChance *chance, char *out, size_t capacity) {
    int type = chance->info.chanceType, side = chance->info.pitchSide;
    // A drill has its own pitch (c_TMatch.m_LoadPitchImages in training).
    switch (chance->info.training) {
    case 2:
        SDL_snprintf(out, capacity, "Pitch_Ma_AM.uis");
        return;
    case 3:
    case 4:
        SDL_snprintf(out, capacity, "Pitch_Ab_AM.uis");
        return;
    case 6:
        SDL_snprintf(out, capacity, "Pitch_Mb_AM.uis");
        return;
    case 8:
        SDL_snprintf(out, capacity, "Pitch_Prac_AM.uis");
        return;
    default:
        break;
    }
    if (type == 4) {
        SDL_snprintf(out, capacity, "Pitch_Pen_AM.uis");
        return;
    }
    if (type == 11) {
        // The dribble plays on the practice pitch (GSGetChanceScreenName).
        SDL_snprintf(out, capacity, "Pitch_Prac_AM.uis");
        return;
    }
    const char *kind = type == 1 || type == 9 ? "M" : "A";
    char letter = side == 1 ? 'b' : 'a';
    int mirror = side == 2;
    if (type == 7 || type == 8) {
        letter = 'c';
        mirror = chance->info.minute % 2 == 1;
    }
    SDL_snprintf(out, capacity, "Pitch_%s%c_AM%s.uis", kind, letter, mirror ? "_Rev" : "");
}

static void free_chance_view(SSRApp *app) {
    ssr_scene_free(app->pitchScene);
    ssr_ui_free(app->pitchScreen);
    ssr_scene_free(app->tapScene);
    ssr_ui_free(app->tapScreen);
    ssr_scene_free(app->bannerScene);
    ssr_ui_free(app->bannerScreen);
    ssr_scene_free(app->stadiumScene);
    ssr_ui_free(app->stadiumScreen);
    ssr_scene_free(app->stadiumStaticScene);
    ssr_ui_free(app->stadiumStaticScreen);
    ssr_scene_free(app->hudScene);
    ssr_ui_free(app->hudScreen);
    app->hudScene = NULL;
    app->hudScreen = NULL;
    ssr_scene_free(app->splashScene);
    ssr_ui_free(app->splashScreen);
    app->splashScene = NULL;
    app->splashScreen = NULL;
    app->pitchScene = app->tapScene = app->bannerScene = app->stadiumScene = app->stadiumStaticScene = NULL;
    app->pitchScreen = app->tapScreen = app->bannerScreen = app->stadiumScreen = app->stadiumStaticScreen = NULL;
    app->pitchName[0] = 0;
    app->viewScene = NULL;
}

static SSRScene *open_layer(SSRApp *app, const char *name, SSRUiTree **tree, SSRSceneEnv *env) {
    SSRUiFiles files = {read_screen_file, app};
    *tree = ssr_ui_parse(name, &files);
    return *tree ? ssr_scene_open(*tree, env) : NULL;
}

// The animation of a player: the kind, and one of 8 headings
// (c_AMatchPlayer.SetDirToAngle).
static void player_anim(const SSRMatchPlayer *p, char *out, size_t capacity) {
    int a = (int)p->direction;
    while (a < 0) a += 360;
    a %= 360;
    const char *heading = a > 338 || a < 23 ? "E"
                          : a < 67          ? "SE"
                          : a < 112         ? "S"
                          : a < 157         ? "SW"
                          : a < 202         ? "W"
                          : a < 247         ? "NW"
                          : a < 292         ? "N"
                                            : "NE";
    const char *kind = p->type == SSR_PLAYER_KEEPER ? (p->anim == SSR_ANIM_STAND ? "KStand" : "KRun")
                       : p->inWall                  ? "FWallStand"
                       : p->anim == SSR_ANIM_RUN    ? "FRun"
                                                    : "FStand";
    SDL_snprintf(out, capacity, "%s%s", kind, heading);
}

// The ball of a chance (c_ABall): the ball rises with z and grows, and it
// rolls in one of 8 headings at a speed by its power.
static void update_ball_view(SSRApp *app, const SSRChance *chance) {
    const SSRBall *b = &chance->ball;
    int id = app->ballCopy;
    ssr_scene_spawn_place(app->scene, id, NULL, b->x, b->y);
    ssr_scene_spawn_place(app->scene, id, "BallHook", 0, -b->z);
    ssr_scene_spawn_set(app->scene, id, "BallHook", SSR_GEL_SCALE,
                        (1 + b->z / 100) * (chance->info.chanceType == 11 ? 1.5f : 1));
    // GetVelDir: a heading near an axis, or a diagonal.
    float c = SDL_cosf(b->direction * SDL_PI_F / 180), s = SDL_sinf(b->direction * SDL_PI_F / 180);
    const char *roll;
    if (SDL_fabsf(c) - SDL_fabsf(s) > 0.5f) {
        roll = c > 0 ? "BallRollE" : "BallRollW";
    } else if (SDL_fabsf(s) - SDL_fabsf(c) > 0.5f) {
        roll = s > 0 ? "BallRollS" : "BallRollN";
    } else {
        roll = s > 0 ? (c > 0 ? "BallRollSE" : "BallRollSW") : (c > 0 ? "BallRollNE" : "BallRollNW");
    }
    ssr_scene_spawn_anim(app->scene, id, "BallAnim", roll);
    float speed = chance->kicking >= 2 ? SDL_max(0.2f, b->power / 5) : 0;
    if (b->power <= 0.015f) speed = 0;
    ssr_scene_spawn_set(app->scene, id, "BallAnim", SSR_GEL_ANIM_SPEED, speed);
    // The aim arrow (c_MatchArrow): it points the way of the shot, and grows
    // and goes from yellow to red with the power.
    int aiming = chance->state == SSR_CHANCE_AIM && chance->kicking == 1 && chance->aimPower > 0;
    ssr_scene_spawn_show(app->scene, app->arrowCopy, NULL, aiming);
    if (aiming) {
        float most = chance->setup.power * 0.4f + 60;
        float length = SDL_clamp(chance->aimPower * 1.3f, 10, most);
        ssr_scene_spawn_place(app->scene, app->arrowCopy, NULL, b->x, b->y);
        ssr_scene_spawn_set(app->scene, app->arrowCopy, NULL, SSR_GEL_ANGLE, -chance->aimDirection);
        ssr_scene_spawn_set(app->scene, app->arrowCopy, "Arrow#1", SSR_GEL_W, length + 20);
        float t = length / most;
        char colour[16];
        SDL_snprintf(colour, sizeof(colour), "FF%02X0090", (int)(255 * (1 - t)));
        ssr_scene_spawn_colour(app->scene, app->arrowCopy, "Arrow#1", colour);
    }
}

// The trail of a fast ball (c_BallTrailFXParticles): a copy of the ball in
// #FFFFFF18 comes each frame while power / 50 > 0.08 after the kick. It
// fades in 0.5 s and shrinks to nothing, and it goes after 600 ms
// [I: the remake makes one each frame, the game one each tick].
static void update_trail(SSRApp *app, const SSRChance *chance) {
    const SSRBall *b = &chance->ball;
    Uint64 now = SDL_GetTicks();
    int free = -1;
    for (int i = 0; i < 40; i++) {
        int id = app->trail[i];
        if (!app->trailTime[i]) {
            if (free < 0) free = i;
            continue;
        }
        float t = (float)(now - app->trailTime[i]);
        if (t > 600) {
            app->trailTime[i] = 0;
            ssr_scene_spawn_show(app->scene, id, NULL, 0);
            continue;
        }
        char colour[16];
        SDL_snprintf(colour, sizeof(colour), "FFFFFF%02X", (int)(0x18 * SDL_max(0.0f, 1 - t / 500)));
        ssr_scene_spawn_colour(app->scene, id, "BallHook", colour);
        ssr_scene_spawn_set(app->scene, id, "BallHook", SSR_GEL_SCALE, app->trailScale[i] * (1 - t / 600));
    }
    if (free < 0 || b->kicks == 0 || b->power / 50 <= 0.08f) return;
    int id = app->trail[free];
    app->trailTime[free] = now;
    // The scale of the template (0.6) times 1 + the scale of the ball.
    app->trailScale[free] = 0.6f * (2 + b->z / 100);
    ssr_scene_spawn_show(app->scene, id, NULL, 1);
    ssr_scene_spawn_place(app->scene, id, NULL, b->x, b->y);
    ssr_scene_spawn_place(app->scene, id, "BallHook", 0, -b->z);
    ssr_scene_spawn_colour(app->scene, id, "BallHook", "FFFFFF18");
    ssr_scene_spawn_set(app->scene, id, "BallHook", SSR_GEL_SCALE, app->trailScale[free]);
    ssr_scene_spawn_set(app->scene, id, "BallAnim", SSR_GEL_ANIM_SPEED, 0);
}

// MinigameHud.ShowWind: the wind of a chance, but not of a dribble. The
// blades point with the wind, and the colour goes from green to red at a
// speed of 5.
static void show_wind(SSRApp *app, const SSRChance *chance) {
    if (!app->hudScene) return;
    float speed = chance->setup.windSpeed;
    int shown = speed != 0 && chance->info.chanceType != 11 && chance->info.training != 8;
    ssr_scene_set_visible(app->hudScene, "WindIndicator", shown);
    if (!shown) return;
    ssr_scene_spawn_set(app->hudScene, 0, "WindIndicator#1", SSR_GEL_ANGLE, chance->setup.windDirection - 90);
    ssr_tweaks_set_number(app->tweaks, "HUD", "WindSpeed", speed);
    ssr_tweaks_set_number(app->tweaks, "HUD", "WindSpeedSpin", -speed);
    Uint32 lo = (Uint32)SDL_strtoul(ssr_tweaks_get(app->tweaks, "HUD", "WindIndicatorMin"), NULL, 16);
    Uint32 hi = (Uint32)SDL_strtoul(ssr_tweaks_get(app->tweaks, "HUD", "WindIndicatorMax"), NULL, 16);
    float t = SDL_clamp(speed / 5.0f, 0, 1);
    Uint32 mixed = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float a = (float)((lo >> shift) & 0xFF), b = (float)((hi >> shift) & 0xFF);
        mixed |= (Uint32)(a + (b - a) * t) << shift;
    }
    char colour[16];
    SDL_snprintf(colour, sizeof(colour), "%08X", (unsigned)mixed);
    ssr_tweaks_set(app->tweaks, "HUD", "WindIndicator", colour);
}

// The objects of a drill at their places. A line goes when the ball cuts
// it, a hit pole and a hit dummy change colour, and a cone that falls goes
// [I: the remake does not play the fall of a cone].
static void update_objects_view(SSRApp *app) {
    const SSRTraining *drill = ssr_game_training(app->game);
    if (!drill) return;
    for (int i = 0; i < app->objectCount && i < drill->objectCount; i++) {
        const SSRTrainingObject *o = &drill->objects[i];
        int id = app->objectCopies[i];
        ssr_scene_spawn_place(app->scene, id, NULL, o->x, o->y);
        switch (o->type) {
        case SSR_OBJECT_LINE:
            // c_ALine: the template is one unit wide.
            ssr_scene_spawn_set(app->scene, id, NULL, SSR_GEL_SCALEX, (float)o->x2 - o->x);
            ssr_scene_spawn_show(app->scene, id, NULL, o->alive);
            break;
        case SSR_OBJECT_CONE:
            ssr_scene_spawn_show(app->scene, id, NULL, o->alive);
            break;
        case SSR_OBJECT_POLE:
            ssr_scene_spawn_colour(app->scene, id, "Colour", o->alive ? "FFFFFFFF" : "FF0000FF");
            break;
        case SSR_OBJECT_DUMMY:
            if (!o->alive) {
                const char *name = drill->training == 6 ? "ColHittedDummyPositive" : "ColHittedDummyNegative";
                ssr_scene_spawn_colour(app->scene, id, "DummyColour", ssr_tweaks_get(app->tweaks, "Training", name));
            }
            break;
        }
    }
}

// The circle of a team-mate that the captain can send on a run, and the
// arrow of its run (c_MatchCaptainArrow.Update). They show until the kick.
static void update_run_arrow(SSRApp *app, const SSRChance *chance, int i) {
    const SSRMatchPlayer *p = &chance->players.list[i];
    int before = chance->state == SSR_CHANCE_AIM && chance->kicking == 0;
    ssr_scene_spawn_show(app->scene, app->playerCopies[i], "PlayerCircle", p->canMakeRun && before);
    float dx = (float)p->makeRunX - p->x, dy = (float)p->makeRunY - p->y, d2 = dx * dx + dy * dy;
    int shown = p->canMakeRun && before && p->makeRunDistance > 0 && d2 >= 1;
    int id = app->runArrows[i];
    ssr_scene_spawn_show(app->scene, id, NULL, shown);
    if (!shown) return;
    float most = (float)SSR_MAKE_RUN_MAX;
    float length = SDL_clamp((d2 / most + SDL_sqrtf(d2)) * 0.3f * 1.3f, 10, most);
    ssr_scene_spawn_place(app->scene, id, NULL, p->x, p->y);
    ssr_scene_spawn_set(app->scene, id, NULL, SSR_GEL_ANGLE, -SDL_atan2f(dy, dx) * 180 / SDL_PI_F);
    ssr_scene_spawn_set(app->scene, id, "TacticArrow#1", SSR_GEL_W, length + 20);
}

static void update_chance_view(SSRApp *app, SSRSceneEnv *env, int ms) {
    const SSRChance *chance = ssr_game_chance(app->game);
    if (!chance || SDL_strcmp(ssr_game_screen(app->game), "matchminigame") != 0 || !app->scene) {
        if (app->pitchScene || app->tapScene) free_chance_view(app);
        return;
    }
    char name[64];
    pitch_name(chance, name, sizeof(name));
    if (SDL_strcmp(name, app->pitchName) != 0) {
        ssr_scene_free(app->pitchScene);
        ssr_ui_free(app->pitchScreen);
        app->pitchScene = open_layer(app, name, &app->pitchScreen, env);
        SDL_snprintf(app->pitchName, sizeof(app->pitchName), "%s", name);
    }
    if (!app->tapScene) {
        // The tap ball shows in front of the stadium of the home team
        // (c_MatchTapBallChance): "Stadium" + size + "AM", and its Static
        // part under it.
        const SSRMatch *match = ssr_game_match(app->game);
        char stadium[64];
        SDL_snprintf(stadium, sizeof(stadium), "Stadium%dAMStatic.uis", match ? match->stadiumSize : 0);
        app->stadiumStaticScene = open_layer(app, stadium, &app->stadiumStaticScreen, env);
        SDL_snprintf(stadium, sizeof(stadium), "Stadium%dAM.uis", match ? match->stadiumSize : 0);
        app->stadiumScene = open_layer(app, stadium, &app->stadiumScreen, env);
        app->tapScene = open_layer(app, "tapball.uis", &app->tapScreen, env);
        // The HUD of a career chance has no training part (MinigameHud
        // HideTraining).
        app->hudScene = open_layer(app, "minigamehud.uis", &app->hudScreen, env);
        // A drill shows its lives, with no quit button (ShowTraining).
        int training = ssr_game_training(app->game) != NULL;
        if (app->hudScene) ssr_scene_set_visible(app->hudScene, "TrainingHUD", training);
        if (app->hudScene) ssr_scene_set_visible(app->hudScene, "QuitButton", 0);
        // A match chance shows the names and the score (ShowGame).
        if (app->hudScene) ssr_scene_set_visible(app->hudScene, "GameHUD", !training);
        show_wind(app, chance);
    }
    if (app->viewScene != app->scene) {
        // A new screen: the copies of the players and of the ball.
        app->viewScene = app->scene;
        for (int i = 0; i < chance->players.count; i++) {
            int id = ssr_scene_spawn(app->scene, "MatchPitch", "Player", 0, NULL, 0);
            app->playerCopies[i] = id;
            ssr_scene_spawn_show(app->scene, id, "PlayerCircle", 0);
            ssr_scene_spawn_show(app->scene, id, "PlayerCalling", 0);
            ssr_scene_spawn_show(app->scene, id, "OffsideFlag", 0);
            // The team-mates wear the kit of the player, the others the kit of
            // the other team, and the keeper a keeper shirt.
            const SSRMatchPlayer *p = &chance->players.list[i];
            const char *team = p->type == SSR_PLAYER_ATTACKER ? "MyTeam" : "OppTeam";
            char shirt[32], pants[32];
            SDL_snprintf(shirt, sizeof(shirt), "%sShirt1", team);
            SDL_snprintf(pants, sizeof(pants), "%sPants", team);
            if (p->type == SSR_PLAYER_KEEPER) SDL_snprintf(shirt, sizeof(shirt), "KeeperShirt1");
            ssr_scene_spawn_clone(app->scene, id, "Shirt", "MatchColours", shirt);
            ssr_scene_spawn_clone(app->scene, id, "Sleeves", "MatchColours", shirt);
            ssr_scene_spawn_clone(app->scene, id, "Stripes", "MatchColours", shirt);
            ssr_scene_spawn_clone(app->scene, id, "Hoops", "MatchColours", shirt);
            ssr_scene_spawn_clone(app->scene, id, "Pants", "MatchColours", pants);
            app->runArrows[i] = ssr_scene_spawn(app->scene, "MatchPitch", "TacticArrow", 0, NULL, 0);
        }
        for (int i = 0; i < 40; i++) {
            app->trail[i] = ssr_scene_spawn(app->scene, "MatchPitch", "Ball", 0, NULL, 0);
            app->trailTime[i] = 0;
            ssr_scene_spawn_show(app->scene, app->trail[i], NULL, 0);
            ssr_scene_spawn_show(app->scene, app->trail[i], "Ball", 0);
            ssr_scene_spawn_show(app->scene, app->trail[i], "BallTip", 0);
        }
        // The objects of a drill (c_ATraining.m_CreateItems).
        const SSRTraining *drill = ssr_game_training(app->game);
        app->objectCount = drill ? drill->objectCount : 0;
        for (int i = 0; i < app->objectCount; i++) {
            const SSRTrainingObject *o = &drill->objects[i];
            const char *name = o->type == SSR_OBJECT_POLE    ? "Pole"
                               : o->type == SSR_OBJECT_CONE  ? "ConeOneStripe"
                               : o->type == SSR_OBJECT_DUMMY ? "Dummy"
                               : o->finishLine               ? "Line"
                               : o->tall                     ? "HoloLineTall"
                                                             : "HoloLine";
            app->objectCopies[i] = ssr_scene_spawn(app->scene, "MatchPitch", name, 0, NULL, 0);
        }
        app->ballCopy = ssr_scene_spawn(app->scene, "MatchPitch", "Ball", 0, NULL, 0);
        ssr_scene_spawn_show(app->scene, app->ballCopy, "BallTip", 0);
        app->arrowCopy = ssr_scene_spawn(app->scene, "MatchPitch", "Arrow", 0, NULL, 0);
    }
    for (int i = 0; i < chance->players.count && i < SSR_PLAYERS_MAX; i++) {
        const SSRMatchPlayer *p = &chance->players.list[i];
        char anim[32];
        player_anim(p, anim, sizeof(anim));
        // The game adds z to y here (c_AMatchPlayer.p_Update). A dribble
        // shows the players at 1.5 times their size.
        ssr_scene_spawn_place(app->scene, app->playerCopies[i], NULL, p->x, p->y + p->z);
        if (chance->info.chanceType == 11) ssr_scene_spawn_set(app->scene, app->playerCopies[i], NULL, SSR_GEL_SCALE, 1.5f);
        ssr_scene_spawn_anim(app->scene, app->playerCopies[i], "PlayerAnim", anim);
        update_run_arrow(app, chance, i);
        // The run plays at the speed of the player (c_AMatchPlayer): half the
        // speed times Match.RunSpeedFactor.
        SSRPlayerBall view = ssr_chance_player_ball(chance);
        float factor = (float)ssr_tweaks_number(app->tweaks, "Match", "RunSpeedFactor");
        if (p->anim == SSR_ANIM_RUN || p->anim == SSR_ANIM_RUN_GK) {
            ssr_scene_spawn_set(app->scene, app->playerCopies[i], "PlayerAnim", SSR_GEL_ANIM_SPEED,
                                ssr_player_speed(p, &view) / 2 * (factor > 0 ? factor : 3.5f));
        } else {
            ssr_scene_spawn_set(app->scene, app->playerCopies[i], "PlayerAnim", SSR_GEL_ANIM_SPEED, 1);
        }
    }
    update_ball_view(app, chance);
    update_trail(app, chance);
    update_objects_view(app);
    ssr_scene_update(app->pitchScene, env, ms);
    if (app->hudScene) ssr_scene_update(app->hudScene, env, ms);
    // The banner of the result (c_TQuickMessage "scoreanim") starts again
    // for each chance.
    if (ssr_game_banner(app->game)) {
        if (!app->bannerScene) app->bannerScene = open_layer(app, "scoreanim.uis", &app->bannerScreen, env);
        ssr_scene_update(app->bannerScene, env, ms);
    } else if (app->bannerScene) {
        ssr_scene_free(app->bannerScene);
        ssr_ui_free(app->bannerScreen);
        app->bannerScene = NULL;
        app->bannerScreen = NULL;
    }
    // The splash message of a drill (TQuickMessage "splashmessage").
    if (ssr_game_splash(app->game)) {
        if (!app->splashScene) app->splashScene = open_layer(app, "splashmessage.uis", &app->splashScreen, env);
        ssr_scene_update(app->splashScene, env, ms);
    } else if (app->splashScene) {
        ssr_scene_free(app->splashScene);
        ssr_ui_free(app->splashScreen);
        app->splashScene = NULL;
        app->splashScreen = NULL;
    }
    if (app->tapScene) {
        const SSRTapBall *tap = &chance->tap;
        ssr_scene_place(app->tapScene, "TapBall", tap->x, tap->y, tap->depth);
        ssr_scene_update(app->tapScene, env, ms);
        ssr_scene_update(app->stadiumStaticScene, env, ms);
        ssr_scene_update(app->stadiumScene, env, ms);
    }
}

// Gives the changes that the logic asks for to the elements of the screen.
static void apply_ops(SSRApp *app) {
    SSRUiOp op;
    while (app->scene && ssr_game_take_op(app->game, &op)) {
        switch (op.kind) {
        case SSR_OP_TEXT:
            ssr_scene_set_text(app->scene, op.ref, op.text);
            break;
        case SSR_OP_SHOW:
        case SSR_OP_HIDE:
            ssr_scene_set_visible(app->scene, op.ref, op.kind == SSR_OP_SHOW);
            if (app->hudScene) ssr_scene_set_visible(app->hudScene, op.ref, op.kind == SSR_OP_SHOW);
            break;
        case SSR_OP_LIST_CLEAR:
            ssr_scene_list_clear(app->scene, op.ref);
            break;
        case SSR_OP_COLOUR:
            ssr_scene_set_colour(app->scene, op.ref, op.text);
            break;
        case SSR_OP_ANIM:
            ssr_scene_anim(app->scene, op.ref, (float)SDL_atof(op.text), op.id);
            break;
        case SSR_OP_LIST_COLOUR:
            ssr_scene_list_colour(app->scene, op.ref, op.id, op.text);
            break;
        case SSR_OP_LIST_SUB:
            ssr_scene_list_sub(app->scene, op.ref, op.ref2, op.text[0] ? op.text : NULL, op.id);
            break;
        case SSR_OP_LIST_ADD:
            ssr_scene_list_add(app->scene, op.ref, op.ref2, op.text, op.id);
            break;
        }
    }
}

// A click: the actions of the block of the Click() run, its sounds play,
// and the logic gets the message.
static void click_in(SSRApp *app, SSRScene *scene, const SSRSceneItem *item) {
    SSRSceneEnv env = scene_env(app);
    const char *message = ssr_scene_click(scene, &env, item);
    SDL_Log("Pressed %s (%s)", item->ref ? item->ref : "-", message ? message : "-");
    const char *sound;
    while ((sound = ssr_scene_take_sound(scene)) != NULL) ssr_audio_play(app->audio, sound);
    if (message && *message && !ssr_game_message(app->game, message)) SDL_Log("No logic for %s yet.", message);
    follow_game(app);
}

static void click(SSRApp *app, const SSRSceneItem *item) {
    click_in(app, app->scene, item);
}

static float ticker_measure(void *context, const char *text) {
    SSRApp *app = context;
    return ssr_gfx_text_width(app->gfx, SSR_FONT_MEDIUM, text);
}

// The ticker of the game (bb_soccer_tickers.InitialiseTickers): the welcome
// texts. The texts of the career come with the parts that they tell of.
static void load_ticker(SSRApp *app) {
    ssr_scene_free(app->tickerScene);
    app->tickerScene = NULL;
    ssr_ui_free(app->tickerScreen);
    ssr_ticker_free(app->ticker);
    SSRUiFiles files = {read_screen_file, app};
    app->tickerScreen = ssr_ui_parse("ticker.uis", &files);
    app->ticker = ssr_ticker_create(ticker_measure, app);
    for (int i = 1; i <= 6; i++) {
        char tag[32];
        SDL_snprintf(tag, sizeof(tag), "TICKER_NEWPLAYER_%d", i);
        ssr_ticker_add(app->ticker, translate(app, tag), 0);
    }
    ssr_tweaks_set_number(app->tweaks, "Shell", "Ticker", 1);
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
    ssr_game_free(app->game);
    app->game = ssr_game_create(app->tweaks);
    ssr_game_set_data(app->game, app->world, translate, app, app->saveDirectory);
    // SSR_SEED fixes the random numbers, so that a test run is the same each time.
    const char *seed = SDL_getenv("SSR_SEED");
    ssr_game_set_seed(app->game, seed && *seed ? SDL_atoi(seed) : (int)SDL_GetTicks());
    ssr_game_load_settings(app->game, app->settingsPath);
    apply_settings(app);
    app->gameScreen = ssr_game_screen_version(app->game);
    load_ticker(app);
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
    ssr_scene_free(app->tickerScene);
    app->tickerScene = NULL;
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
    SDL_snprintf(app->settingsPath, sizeof(app->settingsPath), "%ssettings.txt", pref);
    SDL_snprintf(app->saveDirectory, sizeof(app->saveDirectory), "%ssaves", pref);
    SDL_CreateDirectory(app->saveDirectory);
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
    if (event->type == SDL_EVENT_TEXT_INPUT) ssr_game_type(app->game, event->text.text);
    if (event->type == SDL_EVENT_KEY_DOWN && ssr_game_wants_text(app->game)) {
        if (event->key.key == SDLK_BACKSPACE) ssr_game_key(app->game, SSR_KEY_BACKSPACE);
        if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER) ssr_game_key(app->game, SSR_KEY_ENTER);
    }
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
        if (app->hudScene) ssr_scene_touch(app->hudScene, down, x + app->left, y + app->top);
        ssr_game_touch(app->game, down, x + app->left, y + app->top);
        // A click is when the touch goes up on the element, as in the game.
        // A touch that moved a list is a swipe, not a click.
        int up = (event->type == SDL_EVENT_MOUSE_BUTTON_UP || event->type == SDL_EVENT_FINGER_UP) &&
                 !ssr_scene_dragged(app->scene);
        // The HUD of a chance is on top, so it takes the click first.
        const SSRSceneItem *hud = up && app->hudScene && ssr_game_chance(app->game)
                                      ? ssr_scene_hit_item(app->hudScene, x + app->left, y + app->top)
                                      : NULL;
        const SSRSceneItem *item = up && !hud ? ssr_scene_hit_item(app->scene, x + app->left, y + app->top) : NULL;
        if (hud) click_in(app, app->hudScene, hud);
        if (item) click(app, item);
    }
    return SDL_APP_CONTINUE;
}

static void draw_centered_text(const SSRApp *app, float y, const char *text) {
    float width = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_RenderDebugText(app->renderer, (app->width - width) / 2, y, text);
}

// The ticker shell shows while the tweak Shell,Ticker is 1 (ShowShell).
static void draw_ticker(SSRApp *app, const SSRSceneEnv *env) {
    if (!app->tickerScreen || ssr_tweaks_number(app->tweaks, "Shell", "Ticker") == 0) return;
    Uint64 now = SDL_GetTicks();
    int ms = 0;
    if (!app->tickerScene) {
        app->tickerScene = ssr_scene_open(app->tickerScreen, env);
        ssr_ticker_start(app->ticker, app->tickerScene);
    } else {
        ms = (int)(now - app->tickerFrame);
    }
    app->tickerFrame = now;
    ssr_scene_update(app->tickerScene, env, ms);
    ssr_ticker_update(app->ticker, app->tickerScene);
    ssr_gfx_draw_scene(app->gfx, app->tickerScene);
}

static const char *provide(void *context, const char *name) {
    SSRApp *app = context;
    return ssr_game_text(app->game, name);
}

static SSRSceneEnv scene_env(SSRApp *app) {
    SSRSceneEnv env = {
        .left = app->left,
        .top = app->top,
        .width = app->width,
        .height = app->height,
        .tweaks = app->tweaks,
        .text = translate,
        .image_size = image_size,
        .spine = spine_file,
        .provide = provide,
        .context = app,
    };
    return env;
}

// SSR_AUTOCHANCE="dx,dy" plays each chance of the player for a test: it
// touches the ball, drags by (dx, dy), lets go, and taps the tap ball at its
// centre when the ball is near the middle of the screen. Each step takes
// some milliseconds, so that the ticks of the game see it.
static void auto_chance(SSRApp *app) {
    const char *setting = SDL_getenv("SSR_AUTOCHANCE");
    const SSRChance *chance = ssr_game_chance(app->game);
    static Uint64 start = 0, tapStart = 0;
    float dx = 0, dy = 0;
    if (!setting || SDL_sscanf(setting, "%f,%f", &dx, &dy) != 2 || !chance) {
        start = 0;
        return;
    }
    Uint64 now = SDL_GetTicks();
    if (!start) start = now;
    // An aim that did not start (the touch only ended the instruction of a
    // drill) starts again.
    if (chance->state == SSR_CHANCE_AIM && chance->kicking == 0 && now - start > 800) start = now;
    Uint64 t = now - start;
    if (chance->state != SSR_CHANCE_TAP) tapStart = 0;
    if (chance->state == SSR_CHANCE_AIM) {
        if (t < 200) {
            ssr_game_touch(app->game, 1, chance->ball.x, chance->ball.y);
        } else if (t < 600) {
            ssr_game_touch(app->game, 1, chance->ball.x + dx, chance->ball.y + dy);
        } else {
            ssr_game_touch(app->game, 0, chance->ball.x + dx, chance->ball.y + dy);
        }
    } else if (chance->state == SSR_CHANCE_DRIBBLE) {
        // A swipe up the screen each 600 ms.
        int down = (t % 600) < 100;
        ssr_game_touch(app->game, down, 320, down ? 900 : 720);
    } else if (chance->state == SSR_CHANCE_TAP) {
        const SSRTapBall *tap = &chance->tap;
        // The tap waits 1.5 s, so that a frame can show the tap ball.
        if (!tapStart) tapStart = now;
        int near = tap->x > 200 && tap->x < 440 && now - tapStart > 1500;
        ssr_game_touch(app->game, near && (t / 100) % 2 == 0, tap->x, tap->y);
    } else {
        ssr_game_touch(app->game, 0, 0, 0);
    }
}

// SSR_AUTOMATCH presses the Play button of the match when the match waits
// (Match.MatchState 1, 2, or 3), and with 2 the Continue button of the match
// stats 3 s after it shows. With 3 it opens the ratings from the match stats
// one time. The presses come 1.5 s apart.
static void auto_match(SSRApp *app) {
    const char *setting = SDL_getenv("SSR_AUTOMATCH");
    if (!setting || !*setting) return;
    static Uint64 last = 0, statsReady = 0;
    static int ratingsOpened = 0;
    Uint64 now = SDL_GetTicks();
    if (now - last < 1500) return;
    const char *screen = ssr_game_screen(app->game);
    const char *message = NULL;
    int state = (int)ssr_tweaks_number(app->tweaks, "Match", "MatchState");
    if (ssr_game_replaying(app->game) == 1) {
        message = "matchchance.btn_WatchReplay";
    } else if (SDL_atoi(setting) == 4 && (strcmp(screen, "messagedefault") == 0 || strcmp(screen, "message") == 0)) {
        message = "message.btn_Ok";
    } else if (strcmp(screen, "match") == 0 && state >= 1 && state <= 3) {
        message = "match.btn_Play";
    } else if (strcmp(screen, "matchstats") == 0 && SDL_atoi(setting) == 2 &&
               ssr_tweaks_number(app->tweaks, "MatchStats", "StatStage") >= 10) {
        if (!statsReady) statsReady = now;
        if (now - statsReady >= 3000) message = "matchstats.btn_Play";
    } else if (strcmp(screen, "matchstats") == 0 && SDL_atoi(setting) == 3 && !ratingsOpened &&
               ssr_tweaks_number(app->tweaks, "MatchStats", "StatStage") >= 10) {
        ratingsOpened = 1;
        message = "skills.btn_Ratings";
    }
    if (!message) return;
    last = now;
    SDL_Log("Auto press %s", message);
    ssr_game_message(app->game, message);
    follow_game(app);
}

SDL_AppResult SDL_AppIterate(void *state) {
    SSRApp *app = state;
    SDL_SetRenderDrawColor(app->renderer, 34, 120, 52, 255);
    SDL_RenderClear(app->renderer);
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);

    if (app->screen) {
        SSRSceneEnv env = scene_env(app);
        Uint64 now = SDL_GetTicks();
        int ms = 0;
        if (!app->scene) {
            app->scene = ssr_scene_open(app->screen, &env);
        } else {
            ms = (int)(now - app->lastFrame);
        }
        app->lastFrame = now;
        apply_ops(app);
        ssr_scene_update(app->scene, &env, ms);
        ssr_game_update(app->game, ms * 60 / 1000.0f);
        follow_game(app);
        // The keyboard of the phone shows while a name box has the focus.
        int typing = ssr_game_wants_text(app->game);
        if (typing != app->typing) {
            app->typing = typing;
            if (typing) {
                SDL_StartTextInput(app->window);
            } else {
                SDL_StopTextInput(app->window);
            }
        }
        update_chance_view(app, &env, ms);
        ssr_gfx_set_tweaks(app->gfx, app->tweaks);
        const SSRChance *chance = ssr_game_chance(app->game);
        if (app->tapScene && chance && chance->state == SSR_CHANCE_TAP) {
            // The tap ball takes the whole screen: the stadium, then the ball.
            if (app->stadiumStaticScene) ssr_gfx_draw_scene(app->gfx, app->stadiumStaticScene);
            if (app->stadiumScene) ssr_gfx_draw_scene(app->gfx, app->stadiumScene);
            ssr_gfx_draw_scene(app->gfx, app->tapScene);
        } else {
            if (app->pitchScene) ssr_gfx_draw_scene(app->gfx, app->pitchScene);
            ssr_gfx_draw_scene(app->gfx, app->scene);
        }
        if (app->bannerScene) ssr_gfx_draw_scene(app->gfx, app->bannerScene);
        if (app->splashScene) ssr_gfx_draw_scene(app->gfx, app->splashScene);
        if (app->hudScene && chance) ssr_gfx_draw_scene(app->gfx, app->hudScene);
        draw_ticker(app, &env);
    }
    if (!app->gfx) {
        draw_centered_text(app, app->height / 2 - 12, "Drop your copy of New Star Soccer here.");
        draw_centered_text(app, app->height / 2 + 4, "The app reads an APK, an XAPK, or an IPA.");
#ifdef SDL_PLATFORM_IOS
        draw_centered_text(app, app->height / 2 + 20, "Put the file in Documents with Finder, then restart.");
#endif
    }
    if (app->message[0]) draw_centered_text(app, app->height - 40, app->message);

    // The presses come 1.5 s apart, from 1.5 s after the first frame, when
    // the buttons are in. A press "type:text" types the text.
    const char *press = SDL_getenv("SSR_PRESS");
    static int pressed = 0;
    static Uint64 firstFrame = 0;
    if (!firstFrame) firstFrame = SDL_GetTicks();
    if (press && *press && app->scene && SDL_GetTicks() - firstFrame >= (Uint64)(pressed + 1) * 1500) {
        const char *at = press;
        for (int i = 0; i < pressed && at; i++) {
            at = SDL_strchr(at, ';');
            if (at) at++;
        }
        pressed++;
        float x = 0, y = 0;
        char text[64];
        if (at && SDL_sscanf(at, "type:%63[^;]", text) == 1) {
            ssr_game_type(app->game, text);
        } else if (at && SDL_sscanf(at, "%f,%f", &x, &y) == 2) {
            const SSRSceneItem *item = ssr_scene_hit_item(app->scene, x + app->left, y + app->top);
            if (item) click(app, item);
            if (!item) SDL_Log("Pressed nothing at %g,%g", x, y);
        }
    }

    // The messages go 1.5 s apart, as the presses do.
    const char *messages = SDL_getenv("SSR_MESSAGES");
    static int sent = 0;
    if (messages && *messages && app->scene && SDL_GetTicks() - firstFrame >= (Uint64)(sent + 1) * 1500) {
        const char *at = messages;
        for (int i = 0; i < sent && at; i++) {
            at = SDL_strchr(at, ';');
            if (at) at++;
        }
        sent++;
        if (at && *at) {
            char message[96];
            SDL_snprintf(message, sizeof(message), "%.*s", (int)strcspn(at, ";"), at);
            SDL_Log("Message %s", message);
            if (!ssr_game_message(app->game, message)) SDL_Log("No logic for %s.", message);
            follow_game(app);
        }
    }

    auto_chance(app);
    auto_match(app);

    const char *shot = SDL_getenv("SSR_SHOT");
    const char *shotMs = SDL_getenv("SSR_SHOT_MS");
    int wait = shotMs ? SDL_atoi(shotMs) : 0;
    // SSR_SHOT_TAP saves the frame 1 s after the first tap ball shows, and
    // SSR_SHOT_DRIBBLE 1 s after the first dribble starts, in place of a time.
    static Uint64 stateSeen = 0;
    const SSRChance *shotChance = ssr_game_chance(app->game);
    int wanted = SDL_getenv("SSR_SHOT_TAP") ? SSR_CHANCE_TAP : SDL_getenv("SSR_SHOT_DRIBBLE") ? SSR_CHANCE_DRIBBLE : -1;
    if (shotChance && (int)shotChance->state == wanted && !stateSeen) stateSeen = SDL_GetTicks();
    int stateReady = stateSeen && SDL_GetTicks() - stateSeen >= 1000;
    if (wanted >= 0) wait = stateReady ? 0 : 1 << 30;
    static Uint64 replaySeen = 0;
    if (SDL_getenv("SSR_SHOT_REPLAY")) {
        if (!replaySeen && ssr_game_replaying(app->game) == 2) replaySeen = SDL_GetTicks();
        wait = replaySeen && SDL_GetTicks() - replaySeen >= 1500 ? 0 : 1 << 30;
    }
    const char *shotScreen = SDL_getenv("SSR_SHOT_SCREEN");
    static Uint64 screenSeen = 0;
    if (shotScreen && *shotScreen) {
        if (!screenSeen && strcmp(ssr_game_screen(app->game), shotScreen) == 0) screenSeen = SDL_GetTicks();
        wait = screenSeen && SDL_GetTicks() - screenSeen >= (Uint64)wait ? 0 : 1 << 30;
    }
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
    free_chance_view(app);
    ssr_scene_free(app->scene);
    for (int i = 0; i < app->spineCount; i++) ssr_spine_free(app->spines[i].spine);
    ssr_ui_free(app->screen);
    ssr_text_free(app->text);
    ssr_game_free(app->game);
    ssr_scene_free(app->tickerScene);
    ssr_ui_free(app->tickerScreen);
    ssr_ticker_free(app->ticker);
    ssr_tweaks_free(app->tweaks);
    ssr_audio_free(app->audio);
    ssr_gfx_free(app->gfx);
    ssr_world_free(app->world);
    SDL_free(app);
}
