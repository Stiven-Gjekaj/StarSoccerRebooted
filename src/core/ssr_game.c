#include "ssr_game.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_career.h"
#include "ssr_fs.h"
#include "ssr_match.h"
#include "ssr_random.h"

#define MAX_OPS 512
// The game takes 11 letters for a name from the keyboard (c_TInputBox).
#define NAME_LIMIT 11
// England, the nation that the English game chooses first.
#define DEFAULT_NATION 62

struct SSRGame {
    SSRTweaks *tweaks;
    SSRSettings settings;
    char screen[64];
    int version;
    int settingsChanged;

    const SSRWorld *world;
    SSRLocalise localise;
    void *context;
    char saveDirectory[2048];
    SSRRandom random;

    SSRUiOp ops[MAX_OPS];
    int opCount;

    // The message box (c_TScreen_Message): the screen under it and the
    // function that runs when it closes.
    char messageReturn[64];
    char messageCall[64];
    // The number that goes with the call (c_TScreen_Message intdata).
    int messageData;
    int returnState;

    // The career that plays now, and the slot that it uses.
    SSRCareer career;
    int slot;
    int slotExists[SSR_CAREER_SLOTS];

    // The player profile (c_TScreen_ChooseClub).
    char firstName[SSR_CAREER_NAME];
    char lastName[SSR_CAREER_NAME];
    int gender;
    int hardMode;
    // 0 no name, 1 the first name, 2 the last name takes the keyboard.
    int typing;

    // The nationality (c_TScreen_ChooseNation): 1 when the Ok shows full.
    int canPlay;

    // The star transition: the screen after it, and its frames.
    char afterTransition[64];
    float transitionFrames;
    // The headline that the news page shows.
    char news[512];
    char shown[256];

    // The match that plays now, and the chance of the player in it.
    SSRMatch *match;
    SSRChance *chance;
    // The milliseconds of the game, the ticks of 60 a second that are due,
    // and the time when an ended chance goes back to the match.
    int now;
    float ticks;
    int chanceEnd;
    // The banner of the result of a chance shows until this time.
    int bannerEnd;
    SSRTapTouch touch;
    int touchWasDown;
    // The rows of the commentary. The screen shows them again when it
    // comes back after a chance (m_commentaryCache).
    SSRMatchLine commentary[256];
    int commentaryCount;

    // The boost screen (c_TScreen_ChooseBoost): 1 skills or 2 relations, a
    // relation pass after the skill pass, the bolts of each pass, and the
    // rows that the player can still choose.
    int boostType, boostBoth, bolt1, bolt2;
    int boostOn[5];
};

typedef void (*Handler)(SSRGame *game);

typedef struct {
    const char *message;
    Handler handler;
} Route;

static const char *loc(SSRGame *game, const char *tag) {
    const char *text = game->localise ? game->localise(game->context, tag) : NULL;
    return text ? text : tag;
}

static void add_op(SSRGame *game, SSRUiOpKind kind, const char *ref, const char *ref2, const char *text, int id) {
    if (game->opCount >= MAX_OPS) return;
    SSRUiOp *op = &game->ops[game->opCount++];
    memset(op, 0, sizeof(*op));
    op->kind = kind;
    snprintf(op->ref, sizeof(op->ref), "%s", ref ? ref : "");
    snprintf(op->ref2, sizeof(op->ref2), "%s", ref2 ? ref2 : "");
    snprintf(op->text, sizeof(op->text), "%s", text ? text : "");
    op->id = id;
}

// Opens the screen with the layout name.uis (c_TScreen.SetActive). The
// changes for the old screen go away.
static void set_screen(SSRGame *game, const char *name) {
    snprintf(game->screen, sizeof(game->screen), "%s", name);
    game->version++;
    game->opCount = 0;
    ssr_tweaks_set(game->tweaks, "Menu", "CurrentScreen", name);
}

static void replace_all(char *text, size_t capacity, const char *from, const char *to) {
    char out[1024] = "";
    const char *p = text;
    size_t fromLength = strlen(from);
    while (*p) {
        const char *at = strstr(p, from);
        if (!at) {
            strncat(out, p, sizeof(out) - strlen(out) - 1);
            break;
        }
        strncat(out, p, (size_t)(at - p) < sizeof(out) - strlen(out) - 1 ? (size_t)(at - p) : sizeof(out) - strlen(out) - 1);
        strncat(out, to, sizeof(out) - strlen(out) - 1);
        p = at + fromLength;
    }
    snprintf(text, capacity, "%s", out);
}

static void upper(char *text) {
    for (; *text; text++) *text = (char)toupper((unsigned char)*text);
}

/* -------------------------------------------------------------- settings */

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

/* --------------------------------------------------------------- message */

typedef void (*CallFunction)(SSRGame *game);

static void run_call(SSRGame *game, const char *name);

// A message box (c_TScreen_Message.SetUpScreen). It takes the place of the
// screen. Ok and Cancel go back to the screen, then the function runs. A
// box with no text runs the function at once.
static void message(SSRGame *game, const char *layout, const char *text, int cancel, int ok, const char *call) {
    snprintf(game->messageCall, sizeof(game->messageCall), "%s", call ? call : "");
    if (!text || !*text) {
        game->returnState = 1;
        run_call(game, game->messageCall);
        return;
    }
    snprintf(game->messageReturn, sizeof(game->messageReturn), "%s", game->screen);
    ssr_tweaks_set(game->tweaks, "Menu", "MessageTitle", "");
    ssr_tweaks_set(game->tweaks, "Menu", "MessageBody", text);
    ssr_tweaks_set_number(game->tweaks, "Message", "ShowAcc", ok);
    ssr_tweaks_set_number(game->tweaks, "Message", "ShowDec", cancel);
    ssr_tweaks_set(game->tweaks, "Message", "Type", layout);
    set_screen(game, layout);
}

static void close_message(SSRGame *game, int state) {
    game->returnState = state;
    char call[64];
    snprintf(call, sizeof(call), "%s", game->messageCall);
    game->messageCall[0] = 0;
    set_screen(game, game->messageReturn);
    run_call(game, call);
}

static void message_ok(SSRGame *game) {
    close_message(game, 1);
}

static void message_cancel(SSRGame *game) {
    close_message(game, 0);
}

/* ------------------------------------------------------------- the menus */

static void main_menu(SSRGame *game) {
    set_screen(game, "mainmenu");
}

static void options(SSRGame *game) {
    set_option_tweaks(game);
    set_screen(game, "options");
}

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

/* ------------------------------------------------------------ save slots */

static void slot_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/career%d.txt", game->saveDirectory, slot + 1);
}

// The text of a date as "YY-WW" (c_TMyDate.GetString).
static void date_text(const SSRCareer *c, char *out, size_t capacity) {
    snprintf(out, capacity, "%02d-%02d", ssr_career_year(c) % 100, ssr_career_week(c));
}

// c_TScreen_SaveSlot.SetUpScreen: the label and the button of each slot.
static void save_slots(SSRGame *game) {
    for (int slot = 0; slot < SSR_CAREER_SLOTS; slot++) {
        char data[128], button[64], name[32], path[2200];
        snprintf(data, sizeof(data), "%s %d", loc(game, "Save Slot"), slot + 1);
        snprintf(button, sizeof(button), "%s", loc(game, "New Career"));
        game->slotExists[slot] = 0;
        SSRCareer c;
        slot_path(game, slot, path, sizeof(path));
        if (game->saveDirectory[0] && ssr_career_load(&c, path)) {
            const SSRClub *club = game->world ? ssr_world_club(game->world, c.clubId) : NULL;
            char date[16];
            date_text(&c, date, sizeof(date));
            if (c.retired) {
                snprintf(data, sizeof(data), "%s - %d", c.fullName, ssr_career_year(&c));
                snprintf(button, sizeof(button), "%s", loc(game, "Retired"));
            } else if (club) {
                snprintf(data, sizeof(data), "%s - %s - %s", c.fullName, club->tla, date);
                snprintf(button, sizeof(button), "%s", loc(game, "Load Career"));
            }
            game->slotExists[slot] = club != NULL;
        }
        snprintf(name, sizeof(name), "Slot%dData", slot);
        ssr_tweaks_set(game->tweaks, "SaveSlot", name, data);
        snprintf(name, sizeof(name), "Slot%dButtonText", slot);
        ssr_tweaks_set(game->tweaks, "SaveSlot", name, button);
        snprintf(name, sizeof(name), "Slot%dExist", slot);
        ssr_tweaks_set_number(game->tweaks, "SaveSlot", name, game->slotExists[slot]);
    }
}

static void save_slot_screen(SSRGame *game) {
    set_screen(game, "saveslot");
    save_slots(game);
}

static void game_menu(SSRGame *game);
static void choose_club(SSRGame *game);
static void star_transition(SSRGame *game, const char *after);
static void player_tweaks(SSRGame *game);

static void open_slot(SSRGame *game, int slot) {
    game->slot = slot;
    char path[2200];
    slot_path(game, slot, path, sizeof(path));
    if (game->slotExists[slot] && ssr_career_load(&game->career, path)) {
        // LoadCareer: the star transition, then the game menu.
        player_tweaks(game);
        star_transition(game, "gamemenu");
        return;
    }
    // DoNewCareer: the profile, with the help box over it.
    choose_club(game);
    message(game, "messagedefault", loc(game, "help_newplayer"), 0, 1, "");
}

static void slot0(SSRGame *game) {
    open_slot(game, 0);
}

static void slot1(SSRGame *game) {
    open_slot(game, 1);
}

static void slot2(SSRGame *game) {
    open_slot(game, 2);
}

static void delete_slot(SSRGame *game, int slot) {
    if (!game->slotExists[slot]) return;
    game->slot = slot;
    message(game, "message", loc(game, "CMESSAGE_DELETESLOT"), 1, 1, "DeleteCareer");
}

static void delete0(SSRGame *game) {
    delete_slot(game, 0);
}

static void delete1(SSRGame *game) {
    delete_slot(game, 1);
}

static void delete2(SSRGame *game) {
    delete_slot(game, 2);
}

static void call_delete_career(SSRGame *game) {
    if (game->returnState == 1) {
        char path[2200];
        slot_path(game, game->slot, path, sizeof(path));
        remove(path);
    }
    save_slot_screen(game);
}

static void career(SSRGame *game) {
    save_slot_screen(game);
}

/* ---------------------------------------------------------- the profile */

static void set_profile_tweaks(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Player", "Gender", game->gender);
    ssr_tweaks_set_number(game->tweaks, "Player", "Hardmode", game->hardMode);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HardmodeUnlocked", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "FocusedItem", game->typing);
}

static void choose_club(SSRGame *game) {
    set_screen(game, "chooseclub");
    game->firstName[0] = game->lastName[0] = 0;
    game->typing = 0;
    set_profile_tweaks(game);
}

static void focus_first(SSRGame *game) {
    game->typing = 1;
    set_profile_tweaks(game);
}

static void focus_last(SSRGame *game) {
    game->typing = 2;
    set_profile_tweaks(game);
}

static void gender(SSRGame *game) {
    game->typing = 0;
    game->gender = !game->gender;
    set_profile_tweaks(game);
}

// Hard mode needs a career that ended (TPlayer.m_hardmodeunlocked). The
// remake has no ended career yet, so the button keeps it off.
static void difficulty(SSRGame *game) {
    game->hardMode = 0;
    set_profile_tweaks(game);
}

static void choose_nation(SSRGame *game);

static void profile_ok(SSRGame *game) {
    game->typing = 0;
    set_profile_tweaks(game);
    if (!game->firstName[0] && !game->lastName[0]) {
        // The game gives the name "The Kid". The player must press again.
        snprintf(game->firstName, sizeof(game->firstName), "The");
        snprintf(game->lastName, sizeof(game->lastName), "Kid");
        return;
    }
    choose_nation(game);
}

/* ------------------------------------------------------ the nationality */

static int nation_has_league(const SSRWorld *world, int nation) {
    for (int i = 0; i < world->competitionCount; i++) {
        const SSRCompetition *c = &world->competitions[i];
        if (c->locale == 0 && c->based == nation && c->duration > 1) return 1;
    }
    return 0;
}

static int continent_has_nations(const SSRWorld *world, int continent) {
    for (int i = 0; i < world->nationCount; i++) {
        if (world->nations[i].continent == continent) return 1;
    }
    return 0;
}

// The nations in the order of their TLA, then their id (TNation.SortListBy
// 2). The order is in game->order.
static int nation_order[1024];
static const SSRWorld *sort_world;

static int compare_tla(const void *a, const void *b) {
    const SSRNation *x = &sort_world->nations[*(const int *)a], *y = &sort_world->nations[*(const int *)b];
    int order = strcmp(x->tla, y->tla);
    return order ? order : (x->id > y->id) - (x->id < y->id);
}

static int sorted_nations(const SSRWorld *world) {
    int count = world->nationCount < 1024 ? world->nationCount : 1024;
    for (int i = 0; i < count; i++) nation_order[i] = i;
    sort_world = world;
    qsort(nation_order, (size_t)count, sizeof(int), compare_tla);
    return count;
}

static int chosen(SSRGame *game, const char *name) {
    return (int)ssr_tweaks_number(game->tweaks, "Combo", name);
}

static void check_enable_play(SSRGame *game) {
    if (chosen(game, "ChosenDesiredNation") == 0) return;
    game->canPlay = 1;
    if (chosen(game, "ChosenDesiredLeague") != 0) add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
}

static void combo_continent(SSRGame *game) {
    const SSRWorld *world = game->world;
    if (!world) return;
    int continent = chosen(game, "ChosenDesiredContinent");
    add_op(game, SSR_OP_LIST_CLEAR, "NationCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "NationText", NULL, loc(game, "Choose Nation"), 0);
    const SSRContinent *c = ssr_world_continent(world, continent);
    if (c) add_op(game, SSR_OP_TEXT, "ContinentText", NULL, c->name, 0);
    int count = sorted_nations(world);
    for (int i = 0; i < count; i++) {
        const SSRNation *n = &world->nations[nation_order[i]];
        if (n->continent != continent) continue;
        char name[128];
        snprintf(name, sizeof(name), "%s", n->name);
        upper(name);
        add_op(game, SSR_OP_LIST_ADD, "NationCombo", "NationTemplate", name, n->id);
    }
    game->canPlay = 0;
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
}

static void combo_nation(SSRGame *game) {
    const SSRNation *n = game->world ? ssr_world_nation(game->world, chosen(game, "ChosenDesiredNation")) : NULL;
    if (n) add_op(game, SSR_OP_TEXT, "NationText", NULL, n->name, 0);
    check_enable_play(game);
}

static void combo_league(SSRGame *game) {
    const SSRNation *n = game->world ? ssr_world_nation(game->world, chosen(game, "ChosenDesiredLeague")) : NULL;
    if (n) add_op(game, SSR_OP_TEXT, "LeagueText", NULL, n->name, 0);
    check_enable_play(game);
}

static void choose_nation(SSRGame *game) {
    set_screen(game, "choosenation");
    const SSRWorld *world = game->world;
    if (!world) return;
    for (int i = 0; i < world->continentCount; i++) {
        const SSRContinent *c = &world->continents[i];
        if (!continent_has_nations(world, c->id)) continue;
        char name[128];
        snprintf(name, sizeof(name), "%s", c->name);
        upper(name);
        add_op(game, SSR_OP_LIST_ADD, "ContinentCombo", "ContinentTemplate", name, c->id);
    }
    int count = sorted_nations(world);
    for (int i = 0; i < count; i++) {
        const SSRNation *n = &world->nations[nation_order[i]];
        if (!nation_has_league(world, n->id)) continue;
        char name[128];
        snprintf(name, sizeof(name), "%s", n->name);
        upper(name);
        add_op(game, SSR_OP_LIST_ADD, "LeagueCombo", "LeagueTemplate", name, n->id);
    }
    // The default nation of the language: England for English.
    const SSRNation *nation = ssr_world_nation(world, DEFAULT_NATION);
    int continent = nation ? nation->continent : 6;
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredContinent", continent);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredNation", DEFAULT_NATION);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredLeague", DEFAULT_NATION);
    combo_continent(game);
    combo_nation(game);
    combo_league(game);
}

/* ----------------------------------------------------- the first club */

typedef struct {
    const SSRClub *club;
    int strength;
    int level;
} Prospect;

static int compare_level(const void *a, const void *b) {
    const Prospect *x = a, *y = b;
    if (x->level != y->level) return x->level < y->level ? -1 : 1;
    return (x->club->strength < y->club->strength) - (x->club->strength > y->club->strength);
}

static int compare_strength(const void *a, const void *b) {
    const Prospect *x = a, *y = b;
    if (x->strength != y->strength) return x->strength < y->strength ? -1 : 1;
    return (x->club->id > y->club->id) - (x->club->id < y->club->id);
}

static int league_duration(const SSRWorld *world, int league) {
    const SSRCompetition *c = ssr_world_competition(world, league);
    return c ? c->duration : 0;
}

// TContractOffer.GetProspectiveClubs(0, nation, 1, 3) for a new career: the
// clubs of the nation get strengths in steps from the best down to 15, then
// the game takes a random league and a weak club in it.
static const SSRClub *first_club(SSRGame *game, int nation) {
    const SSRWorld *world = game->world;
    Prospect *list = malloc((size_t)world->clubCount * sizeof(Prospect));
    if (!list) return NULL;
    int count = 0, own = 0;
    for (int i = 0; i < world->clubCount; i++) {
        const SSRClub *club = &world->clubs[i];
        if (club->league == 0) continue;
        const SSRCompetition *league = ssr_world_competition(world, club->league);
        list[count].club = club;
        list[count].strength = club->strength;
        list[count].level = league ? league->level : 99;
        count++;
    }
    // TClub.AverageOutStrengthAll: the clubs of the nation go first, in the
    // order of their league, and get even steps of strength. The lowest is
    // 15, as in the game.
    Prospect *mine = malloc((size_t)(count ? count : 1) * sizeof(Prospect));
    if (!mine) {
        free(list);
        return NULL;
    }
    int high = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].club->nation != nation) continue;
        mine[own++] = list[i];
        if (list[i].club->strength > high) high = list[i].club->strength;
    }
    qsort(mine, (size_t)own, sizeof(Prospect), compare_level);
    float step = own ? (float)(high - 15) / (float)own : 0, f = (float)high;
    for (int i = 0; i < own; i++) {
        mine[i].strength = (int)f;
        f -= step;
    }
    for (int i = 0; i < count; i++) {
        if (list[i].club->nation != nation) continue;
        for (int k = 0; k < own; k++) {
            if (mine[k].club == list[i].club) list[i].strength = mine[k].strength;
        }
    }
    free(mine);
    qsort(list, (size_t)count, sizeof(Prospect), compare_strength);

    int league = 0;
    for (int i = 0; i < count; i++) {
        int r = ssr_rand(&game->random, 2);
        const SSRClub *club = list[i].club;
        if (r != 1 && club->nation == nation && league_duration(world, club->league) > 0) {
            league = club->league;
            break;
        }
    }
    const SSRClub *picked = NULL;
    for (int i = 0; i < count && !picked; i++) {
        const SSRClub *club = list[i].club;
        if (list[i].strength <= 20 || club->nation != nation) continue;
        if (league != 0 && club->league != league) continue;
        if (ssr_rand(&game->random, 6) == 1 && league_duration(world, club->league) > 0) picked = club;
    }
    for (int i = 0; i < count && !picked; i++) {
        const SSRClub *club = list[i].club;
        if (club->nation == nation && league_duration(world, club->league) > 0) picked = club;
    }
    free(list);
    return picked;
}

/* --------------------------------------------------------- the career */

static void nation_ok(SSRGame *game) {
    if (!game->canPlay || !game->world) return;
    int league = chosen(game, "ChosenDesiredLeague");
    int wage = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingWage");
    int goalBonus = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingGoalBonus");
    int assistBonus = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingAssistBonus");
    ssr_career_create(&game->career, game->firstName, game->lastName, game->gender, game->hardMode, league, wage,
                      goalBonus, assistBonus);
    game->career.nationId = chosen(game, "ChosenDesiredNation");
    // LoadDataAndStartTrial: the game chooses the club.
    const SSRClub *club = first_club(game, league);
    if (!club) return;
    game->career.clubId = club->id;
    // The trials need the match (0.5). Until then the player goes through
    // them with no score, so the skills start at 1.
    const SSRCompetition *comp = ssr_world_competition(game->world, club->league);
    char text[1024];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_TRIALSUCCESSMOBILE"));
    replace_all(text, sizeof(text), "$clubname", club->name);
    replace_all(text, sizeof(text), "$labelname", club->tla);
    replace_all(text, sizeof(text), "$leaguename", comp ? comp->tla : "");
    message(game, "messagedefault", text, 0, 1, "TrialDone");
}

static void call_trial_done(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Menu", "TrialCompleteIncrement", 1);
    message(game, "message", "", 0, 1, "StartCareer");
}

static void save_career(SSRGame *game) {
    if (!game->saveDirectory[0]) return;
    char path[2200];
    slot_path(game, game->slot, path, sizeof(path));
    ssr_career_save(&game->career, path);
}

static void webpage(SSRGame *game);

// StartCareer: the first skills, the first save, and the news.
static void call_start_career(SSRGame *game) {
    SSRCareer *c = &game->career;
    int *skills[5] = {c->technique, c->pace, c->vision, c->setPieces, c->power};
    for (int i = 0; i < 5; i++) {
        if (skills[i][0] == 0) skills[i][0] = 1;
    }
    const SSRClub *club = game->world ? ssr_world_club(game->world, c->clubId) : NULL;
    snprintf(c->headline, sizeof(c->headline), "%s", loc(game, "CNEWS_FIRSTCONTRACTMOBILE"));
    replace_all(c->headline, sizeof(c->headline), "$clubname", club ? club->name : "");
    replace_all(c->headline, sizeof(c->headline), "$playername", c->fullName);
    replace_all(c->headline, sizeof(c->headline), "$lastname", c->shortName);
    save_career(game);
    player_tweaks(game);
    // The news page shows the headline once (c_TScreen_WebPage).
    snprintf(game->news, sizeof(game->news), "%s", c->headline);
    c->headline[0] = 0;
    star_transition(game, "webpagenews");
}

static void call_drink(SSRGame *game);

static void run_call(SSRGame *game, const char *name) {
    static const struct {
        const char *name;
        CallFunction call;
    } CALLS[] = {
        {"DeleteCareer", call_delete_career},
        {"TrialDone", call_trial_done},
        {"StartCareer", call_start_career},
        {"DrinkNRG", call_drink},
    };
    for (size_t i = 0; i < sizeof(CALLS) / sizeof(CALLS[0]); i++) {
        if (strcmp(CALLS[i].name, name) == 0) {
            CALLS[i].call(game);
            return;
        }
    }
}

/* ----------------------------------------------- the news and the menu */

static void webpage(SSRGame *game) {
    set_screen(game, "webpagenews");
    add_op(game, SSR_OP_TEXT, "MessageText", NULL, game->news, 0);
}

// The star transition (c_TScreen_StarTransition) shows for 55 frames, then
// the screen under it comes back.
static void star_transition(SSRGame *game, const char *after) {
    snprintf(game->afterTransition, sizeof(game->afterTransition), "%s", after);
    game->transitionFrames = 0;
    set_screen(game, "startransition");
}

// The tweaks of the game menu (c_TScreen_GameMenu.UpdatePanels and
// bb_tweakinterface.UpdatePlayerBasicTweaks).
static void player_tweaks(SSRGame *game) {
    const SSRCareer *c = &game->career;
    SSRTweaks *t = game->tweaks;
    char text[64];
    ssr_tweaks_set_number(t, "Player", "Gender", c->gender);
    ssr_tweaks_set_number(t, "Player", "Bux", c->bank);
    ssr_tweaks_set_number(t, "Player", "StarBux", c->bank);
    ssr_tweaks_set_number(t, "Player", "Age", ssr_career_year(c) + 15);
    ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
    ssr_tweaks_set_number(t, "Player", "EarningsWage", c->wage);
    ssr_tweaks_set_number(t, "Player", "EarningsBonusGoals", c->goalBonus);
    ssr_tweaks_set_number(t, "Player", "EarningsBonusAssists", c->assistBonus);
    snprintf(text, sizeof(text), "%d", c->wage);
    ssr_tweaks_set(t, "Menu", "GameMenuEarningsWage", text);
    snprintf(text, sizeof(text), "%d", c->goalBonus);
    ssr_tweaks_set(t, "Menu", "GameMenuEarningsGoalBonus", text);
    snprintf(text, sizeof(text), "%d", c->assistBonus);
    ssr_tweaks_set(t, "Menu", "GameMenuEarningsAssistBonus", text);
    int cans = 0, next = -1;
    for (int i = 0; i < 5; i++) {
        cans += c->nrg[i];
        if (next < 0 && c->nrg[i] > 0) next = i;
    }
    ssr_tweaks_set_number(t, "Player", "Cans", cans);
    ssr_tweaks_set_number(t, "Menu", "NextCanType", next + 1);
    ssr_tweaks_set_number(t, "Menu", "CanSavePlayer", 1);
}

static void game_menu(SSRGame *game) {
    set_screen(game, "gamemenu");
    ssr_tweaks_set_number(game->tweaks, "Menu", "TabPos", 0);
    player_tweaks(game);
}

/* ------------------------------------------------------------ match */

static const SSRClub *player_club(const SSRGame *game) {
    return game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
}

// The other team: the clubs of the league in turn [I: the fixtures of the
// season come in 0.6].
static const SSRClub *opponent(const SSRGame *game, const SSRClub *mine) {
    const SSRClub *clubs[64];
    int count = ssr_world_league_clubs(game->world, mine->league, clubs, 64);
    if (count > 64) count = 64;
    int self = 0;
    for (int i = 0; i < count; i++) {
        if (clubs[i] == mine) self = i;
    }
    if (count < 2) return mine;
    int next = (self + 1 + game->career.apps % (count - 1)) % count;
    return clubs[next];
}

static void match_tweaks(SSRGame *game) {
    const SSRMatch *m = game->match;
    SSRTweaks *t = game->tweaks;
    ssr_tweaks_set_number(t, "Match", "HomeScore", m->score[1]);
    ssr_tweaks_set_number(t, "Match", "AwayScore", m->score[2]);
    ssr_tweaks_set_number(t, "Match", "MatchTime", m->minute);
    ssr_tweaks_set_number(t, "Match", "MatchSpeed", m->textSpeed);
    ssr_tweaks_set_number(t, "Match", "WorkRate", m->setup.workRate);
    ssr_tweaks_set_number(t, "Match", "Focus", m->setup.focus);
    ssr_tweaks_set_number(t, "Match", "Substitute", m->setup.subTime < 0 ? 1 : 0);
    ssr_tweaks_set_number(t, "Player", "Energy", m->energy);
}

// The Match.MatchState of the layout: 1 before the kick-off, 0 while the
// clock runs, 2 at half time, 3 at the end.
static void match_state(SSRGame *game) {
    int state = 0;
    if (game->match->state == SSR_MATCH_PREMATCH) state = 1;
    if (game->match->state == SSR_MATCH_HALF_TIME) state = 2;
    if (game->match->state == SSR_MATCH_FULL_TIME) state = 3;
    ssr_tweaks_set_number(game->tweaks, "Match", "MatchState", state);
    ssr_tweaks_set_number(game->tweaks, "Menu", "MatchScreenFooter", state == 0 ? -1 : 0);
    // While the clock runs, the commentary fills the screen
    // (c_AScreen_Match.m_ButtonPlay and m_DoHalfEnd).
    ssr_tweaks_set_number(game->tweaks, "Match", "MaximiseScreen", state == 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "MaximiseCommentary", state == 0);
}

// ScrollToBottom: the last row of the commentary shows. The list shows 400
// units of its rows of 72.
static void scroll_commentary(SSRGame *game) {
    float lowest = 400.0f - 72.0f * (float)game->commentaryCount;
    ssr_tweaks_set_number(game->tweaks, "Menu", "CommentaryWindowScroll", lowest < 0 ? lowest : 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "CommentaryWindowScrollVel", 0);
}

static void maximise_commentary(SSRGame *game) {
    if (game->match && game->match->state != SSR_MATCH_PREMATCH) {
        ssr_tweaks_set_number(game->tweaks, "Menu", "MaximiseCommentary", 1);
    }
}

// The colours of a kit in the MatchColours tweaks: MyTeam or OppTeam, with
// Shirt1, Shirt2, and Pants.
static void kit_tweaks(SSRGame *game, const char *team, const SSRKit *kit) {
    char name[32], value[16];
    const SSRColor colours[3] = {kit->shirt1, kit->shirt2, kit->shorts};
    const char *parts[3] = {"Shirt1", "Shirt2", "Pants"};
    for (int i = 0; i < 3; i++) {
        snprintf(name, sizeof(name), "%s%s", team, parts[i]);
        snprintf(value, sizeof(value), "%06XFF", (unsigned)(colours[i] & 0xFFFFFF));
        ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, value, SSR_TWEAK_COLOUR);
    }
}

static float brightness(SSRColor c) {
    return (float)(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF)) / 3.0f;
}

// c_AScreen_Match.m_SetUpTeamPlates: the plates and the commentary rows take
// the colours of the teams, with dark text on a bright plate. The kit
// colours stand for the badge colours [I].
static void team_plates(SSRGame *game, const char *side, const SSRKit *kit) {
    SSRColor first = kit->shirt1, second = kit->shirt2;
    if (brightness(first) > 200 && brightness(second) < brightness(first)) {
        first = kit->shirt2;
        second = kit->shirt1;
    }
    int bright = brightness(first) > (float)ssr_tweaks_number(game->tweaks, "MatchColours", "BrightKit");
    char name[48], value[16];
    snprintf(name, sizeof(name), "Nameplate%sCol1", side);
    snprintf(value, sizeof(value), "%06XFF", (unsigned)(first & 0xFFFFFF));
    ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, value, SSR_TWEAK_COLOUR);
    snprintf(name, sizeof(name), "Nameplate%sCol2", side);
    snprintf(value, sizeof(value), "%06XFF", (unsigned)(second & 0xFFFFFF));
    ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, value, SSR_TWEAK_COLOUR);
    snprintf(name, sizeof(name), "Nameplate%sColText", side);
    ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, bright ? "333333FF" : "FFFFFFFF", SSR_TWEAK_COLOUR);
}

static void start_match(SSRGame *game) {
    const SSRClub *mine = player_club(game);
    if (!mine) return;
    const SSRClub *other = opponent(game, mine);
    int home = game->career.apps % 2 == 0;
    free(game->match);
    game->match = calloc(1, sizeof(SSRMatch));
    if (!game->match) return;
    const SSRCareer *c = &game->career;
    SSRMatchSetup s = {0};
    s.homeName = home ? mine->tla : other->tla;
    s.awayName = home ? other->tla : mine->tla;
    s.homeStrength = (float)(home ? mine->strength : other->strength);
    s.awayStrength = (float)(home ? other->strength : mine->strength);
    s.myTeam = home ? 1 : 2;
    s.playerName = c->shortName;
    s.focus = 2;
    s.workRate = 2;
    s.energy = c->energy;
    s.age = ssr_career_year(c) + 15;
    s.relationTeam = (int)c->relationTeam;
    s.relationBoss = (int)c->relationBoss;
    s.setPieces = ssr_career_skill(c->setPieces);
    s.vision = ssr_career_skill(c->vision);
    s.careerApps = c->apps;
    s.starLevel = 1;
    ssr_match_start(game->match, &s, game->localise, game->context, &game->random);
    game->commentaryCount = 0;
    SSRTweaks *t = game->tweaks;
    // Each team plays in its kit for the ground [I: the clash of kits].
    kit_tweaks(game, "MyTeam", home ? &mine->home : &mine->away);
    kit_tweaks(game, "OppTeam", home ? &other->away : &other->home);
    team_plates(game, "Home", home ? &mine->home : &other->home);
    team_plates(game, "Away", home ? &other->away : &mine->away);
    ssr_tweaks_set(t, "Match", "HomeName", s.homeName);
    ssr_tweaks_set(t, "Match", "AwayName", s.awayName);
    ssr_tweaks_set_number(t, "Match", "AggHomeScore", -1);
    ssr_tweaks_set_number(t, "Match", "AggAwayScore", -1);
    ssr_tweaks_set_number(t, "Match", "MaximiseScreen", 0);
    set_screen(game, "match");
    match_tweaks(game);
    match_state(game);
}

// The end of the match: the career keeps the stats, and the game menu
// opens [I: the match stats and the ratings screens come later].
static void end_match(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRMatch *m = game->match;
    c->apps++;
    c->goals += m->goals;
    c->assists += m->assists;
    c->passes += m->passes;
    c->energy = m->energy;
    // The pay of the match (c_TScreen_MatchReview): the wage and the bonuses
    // [I: the sponsors and the agent come later].
    int goals = m->goals * c->goalBonus, assists = m->assists * c->assistBonus;
    int total = c->wage + goals + assists;
    c->bank += total;
    ssr_tweaks_set_number(game->tweaks, "MatchReview", "EarningsWage", c->wage);
    ssr_tweaks_set_number(game->tweaks, "MatchReview", "EarningsBonusGoals", goals);
    ssr_tweaks_set_number(game->tweaks, "MatchReview", "EarningsBonusAssists", assists);
    ssr_tweaks_set_number(game->tweaks, "MatchReview", "EarningsTotal", total);
    free(game->match);
    game->match = NULL;
    save_career(game);
    game_menu(game);
}

static void match_play(SSRGame *game) {
    if (!game->match) return;
    int kickOff = game->match->state == SSR_MATCH_PREMATCH;
    if (ssr_match_play(game->match, game->now)) {
        end_match(game);
        return;
    }
    // The name plates slide together and show the score
    // (c_AScreen_Match.m_ButtonPlay).
    if (kickOff) add_op(game, SSR_OP_ANIM, "NameBar", NULL, "1", 0);
    match_state(game);
}

static void match_speed(SSRGame *game) {
    if (!game->match) return;
    int *speed = &game->match->textSpeed;
    *speed = *speed == 1200 ? 800 : *speed == 800 ? 350 : 1200;
    match_tweaks(game);
}

static void match_work_rate(SSRGame *game) {
    if (!game->match) return;
    SSRMatchSetup *s = &game->match->setup;
    s->workRate = game->match->energy <= 1 ? 1 : s->workRate % 3 + 1;
    match_tweaks(game);
}

static void match_focus(SSRGame *game) {
    if (!game->match) return;
    game->match->setup.focus = game->match->setup.focus % 3 + 1;
    match_tweaks(game);
}

// c_AScreen_Match.m_AddCommentary: a row of the colours of the team, with
// the minute on its stopwatch.
static void show_line(SSRGame *game, const SSRMatchLine *line) {
    static const char *TEMPLATES[] = {"Comment", "HomeComment", "AwayComment"};
    static const char *GOALS[] = {"Comment", "HomeGoalComment", "AwayGoalComment"};
    static const char *SIDES[] = {"", "Home", "Away"};
    int team = line->team >= 0 && line->team <= 2 ? line->team : 0;
    add_op(game, SSR_OP_LIST_ADD, "CommentaryTable", line->goal ? GOALS[team] : TEMPLATES[team], line->text, 0);
    if (team != 0) {
        char name[48];
        snprintf(name, sizeof(name), "Nameplate%sColText", SIDES[team]);
        add_op(game, SSR_OP_LIST_COLOUR, "CommentaryTable", NULL, ssr_tweaks_get(game->tweaks, "MatchColours", name), 1);
        snprintf(name, sizeof(name), "Nameplate%sCol1", SIDES[team]);
        add_op(game, SSR_OP_LIST_COLOUR, "CommentaryTable", NULL, ssr_tweaks_get(game->tweaks, "MatchColours", name), 2);
    }
    char minute[16] = "";
    if (line->minute[0]) snprintf(minute, sizeof(minute), "%s'", line->minute);
    add_op(game, SSR_OP_LIST_SUB, "CommentaryTable", "TimeStamp", minute, minute[0] != 0);
}

// The commentary rows go into the table of the screen.
static void match_lines(SSRGame *game) {
    SSRMatchLine line;
    int added = 0;
    while (ssr_match_take_line(game->match, &line)) {
        show_line(game, &line);
        if (game->commentaryCount < 256) game->commentary[game->commentaryCount++] = line;
        added = 1;
    }
    if (added) scroll_commentary(game);
}

// c_MatchChance.m_ShowBanner: the code of the result for scoreanim.uis,
// and the wait before the match goes on (SplashMessage.Delay*SplashMsg).
static void show_banner(SSRGame *game) {
    const SSRChance *c = game->chance;
    int code = -2, wait = 1500;
    switch (c->result) {
    case SSR_RESULT_GOAL:
        code = 0;
        wait = 1000;
        break;
    case SSR_RESULT_ASSIST:
        code = 1;
        wait = 1000;
        break;
    case SSR_RESULT_PASS:
        code = 2;
        wait = 1000;
        break;
    case SSR_RESULT_SAVED:
        code = -1;
        wait = 1000;
        break;
    case SSR_RESULT_OFFSIDE:
        code = -3;
        wait = 1000;
        break;
    default:
        break;
    }
    ssr_tweaks_set_number(game->tweaks, "Minigames", "Outcome", code);
    ssr_tweaks_set_number(game->tweaks, "Chance", "IsHead", c->info.chanceType == 9 || c->info.chanceType == 10);
    // The quick message shows for 2 seconds.
    game->bannerEnd = game->now + 2000;
    game->chanceEnd = game->now + wait;
}

static void start_chance(SSRGame *game) {
    free(game->chance);
    game->chance = calloc(1, sizeof(SSRChance));
    if (!game->chance) return;
    const SSRCareer *c = &game->career;
    SSRChanceSetup s = ssr_match_chance_setup(game->match, ssr_career_skill(c->technique), ssr_career_skill(c->power), 1);
    s.careerGoals = (float)c->goals;
    s.careerPasses = (float)c->passes;
    ssr_chance_start(game->chance, &s, &game->random);
    game->chanceEnd = 0;
    set_screen(game, "matchminigame");
}

static void match_tick(SSRGame *game) {
    if (game->chance) {
        SSRTapTouch touch = game->touch;
        touch.hit = touch.down && !game->touchWasDown;
        game->touchWasDown = touch.down;
        ssr_chance_update(game->chance, game->now, &touch, &game->random);
        if (game->chance->state == SSR_CHANCE_OVER) {
            if (game->chanceEnd == 0) show_banner(game);
            if (game->now >= game->chanceEnd) {
                ssr_match_chance_done(game->match, game->chance, game->now, &game->random);
                free(game->chance);
                game->chance = NULL;
                set_screen(game, "match");
                for (int i = 0; i < game->commentaryCount; i++) show_line(game, &game->commentary[i]);
                scroll_commentary(game);
                add_op(game, SSR_OP_ANIM, "NameBar", NULL, "1", 1);
                match_state(game);
                match_tweaks(game);
            }
        }
        return;
    }
    SSRMatchEvent event = ssr_match_update(game->match, game->now, &game->random);
    match_lines(game);
    match_tweaks(game);
    if (event == SSR_MATCH_EVENT_CHANCE) start_chance(game);
    if (event == SSR_MATCH_EVENT_HALF_TIME || event == SSR_MATCH_EVENT_FULL_TIME) match_state(game);
}

/* ------------------------------------------------------------ drinks */

// GetNextAvailableNRG: the first kind of can that the player has, or -1.
static int next_can(const SSRCareer *c) {
    for (int i = 0; i < 5; i++) {
        if (c->nrg[i] > 0) return i;
    }
    return -1;
}

static int *skill_levels(SSRCareer *c, int row) {
    int *rows[5] = {c->pace, c->power, c->technique, c->vision, c->setPieces};
    return rows[row];
}

static float *relation(SSRCareer *c, int row) {
    float *rows[3] = {&c->relationBoss, &c->relationTeam, &c->relationFans};
    return row < 3 ? rows[row] : NULL;
}

// The skills and the relations that the boost screen shows.
static void skill_tweaks(SSRGame *game) {
    SSRCareer *c = &game->career;
    static const char *SKILLS[5] = {"Pace", "Power", "Technique", "Vision", "SetPieces"};
    for (int i = 0; i < 5; i++) {
        ssr_tweaks_set_number(game->tweaks, "Player", SKILLS[i], ssr_career_skill(skill_levels(c, i)) + 0.5);
    }
    ssr_tweaks_set_number(game->tweaks, "Player", "BossRelationship", c->relationBoss);
    ssr_tweaks_set_number(game->tweaks, "Player", "TeamRelationship", c->relationTeam);
    ssr_tweaks_set_number(game->tweaks, "Player", "FansRelationship", c->relationFans);
    ssr_tweaks_set_number(game->tweaks, "Player", "GirlfriendRelationship", 0);
    ssr_tweaks_set_number(game->tweaks, "Player", "SponsorsRelationship", 0);
}

static void disable_row(SSRGame *game, int row) {
    static const char *SKILL[5] = {"SkillButton1", "SkillButton2", "SkillButton3", "SkillButton4", "SkillButton5"};
    static const char *REL[5] = {"RelButton1", "RelButton2", "RelButton3", "RelButton4", "RelButton5"};
    game->boostOn[row] = 0;
    add_op(game, SSR_OP_COLOUR, game->boostType == 2 ? REL[row] : SKILL[row], NULL, "ffffff55", 0);
}

// UpdateProgressBars: a full row cannot take more. The player has no
// girlfriend and no sponsors yet [I]. Ok shows when no row is left.
static void boost_rows(SSRGame *game) {
    SSRCareer *c = &game->career;
    for (int i = 0; i < 5; i++) {
        float percent;
        if (game->boostType == 2) {
            percent = i < 3 ? *relation(c, i) : 0;
            if (i >= 3 && game->boostOn[i]) disable_row(game, i);
        } else {
            // In float, 60 * 1.6666666 is 100 (c_TScreen_ChooseBoost).
            percent = (float)ssr_career_skill(skill_levels(c, i)) * 1.6666666f;
            if (percent > 100) percent = 100;
        }
        if (percent == 100.0f && game->boostOn[i]) disable_row(game, i);
    }
    int left = 0;
    for (int i = 0; i < 5; i++) left += game->boostOn[i];
    if (left == 0) add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
    skill_tweaks(game);
}

static void boost_screen(SSRGame *game, int type, int both, int bolt1, int bolt2) {
    game->boostType = type;
    game->boostBoth = both;
    game->bolt1 = bolt1;
    game->bolt2 = bolt2;
    for (int i = 0; i < 5; i++) game->boostOn[i] = 1;
    set_screen(game, "chooseboost");
    add_op(game, type == 2 ? SSR_OP_SHOW : SSR_OP_HIDE, "Relationships", NULL, NULL, 0);
    add_op(game, type == 2 ? SSR_OP_HIDE : SSR_OP_SHOW, "Skills", NULL, NULL, 0);
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "HintText", NULL, loc(game, type == 2 ? "Choose Relationship Boost" : "Choose Skill Boost"),
           0);
    boost_rows(game);
}

static void back_from_drink(SSRGame *game) {
    if (game->match) {
        // c_TScreen_Match.Restore: the match screen with its commentary.
        set_screen(game, "match");
        for (int i = 0; i < game->commentaryCount; i++) show_line(game, &game->commentary[i]);
        scroll_commentary(game);
        add_op(game, SSR_OP_ANIM, "NameBar", NULL, "1", 1);
        match_state(game);
        match_tweaks(game);
    } else {
        game_menu(game);
    }
}

static void set_energy(SSRGame *game, float energy) {
    game->career.energy = energy;
    if (game->match) game->match->energy = energy;
}

// c_TPlayer.p_DrinkNRG: the can goes, then its effect.
static void drink(SSRGame *game, int can) {
    SSRCareer *c = &game->career;
    if (can < 0 || can > 4 || c->nrg[can] < 1) return;
    c->nrg[can]--;
    save_career(game);
    switch (can) {
    case 0:
        set_energy(game, 100);
        back_from_drink(game);
        break;
    case 1:
        boost_screen(game, 1, 0, 0, 0);
        break;
    case 2:
        boost_screen(game, 2, 0, 0, 0);
        break;
    case 3:
        boost_screen(game, 1, 1, 0, 0);
        break;
    default:
        boost_screen(game, 1, 1, 3, 3);
        break;
    }
}

static void call_drink(SSRGame *game) {
    if (game->returnState == 1) drink(game, game->messageData);
}

// The Drink NRG button of the game menu (c_TScreen_GameMenu).
static void drink_button(SSRGame *game) {
    int can = next_can(&game->career);
    if (can < 0) return;
    if (game->career.energy >= 100) {
        message(game, "messagedefault", loc(game, "CMESSAGE_ENERGYFULL"), 0, 1, NULL);
    } else if (game->career.energy > 75) {
        game->messageData = can;
        message(game, "message", loc(game, "CMESSAGE_DRINKNRGCHECK"), 1, 1, "DrinkNRG");
    } else {
        drink(game, can);
    }
}

// The NRG button of a match drinks the next can at once [I: the game
// opens the shop on that can].
static void match_drink(SSRGame *game) {
    const SSRMatch *m = game->match;
    if (!m || m->minute >= 120 || m->state == SSR_MATCH_FULL_TIME || !ssr_match_on_pitch(m)) return;
    int can = next_can(&game->career);
    if (can < 0 || m->energy >= 100) return;
    drink(game, can);
}

// ChooseBoost(k): a skill gets stars, or a relation gets points.
static void choose_boost(SSRGame *game, int row) {
    if (!game->boostOn[row]) return;
    SSRCareer *c = &game->career;
    if (game->boostType == 1) {
        int *levels = skill_levels(c, row);
        int stars = game->bolt1 > 0 ? 3 : 1;
        for (int e = 0; e < 20 && stars > 0; e++) {
            while (stars > 0 && levels[e] < 3) {
                levels[e]++;
                stars--;
            }
        }
        if (game->bolt1 > 0) {
            disable_row(game, row);
            if (--game->bolt1 == 0) {
                for (int i = 0; i < 5; i++) disable_row(game, i);
            }
        } else {
            for (int i = 0; i < 5; i++) disable_row(game, i);
        }
    } else {
        float *r = relation(c, row);
        float amount = game->bolt2 > 0 ? 9.0f : 3.0f;
        if (r) {
            *r += amount;
            if (*r < 1) *r = 1;
            if (*r > 100) *r = 100;
        }
        if (game->bolt2 > 0) {
            disable_row(game, row);
            if (--game->bolt2 == 0) {
                for (int i = 0; i < 5; i++) disable_row(game, i);
            }
        } else {
            for (int i = 0; i < 5; i++) disable_row(game, i);
        }
    }
    boost_rows(game);
}

static void boost1(SSRGame *game) {
    choose_boost(game, 0);
}

static void boost2(SSRGame *game) {
    choose_boost(game, 1);
}

static void boost3(SSRGame *game) {
    choose_boost(game, 2);
}

static void boost4(SSRGame *game) {
    choose_boost(game, 3);
}

static void boost5(SSRGame *game) {
    choose_boost(game, 4);
}

static void boost_ok(SSRGame *game) {
    if (game->boostBoth) {
        boost_screen(game, 2, 0, game->bolt1, game->bolt2);
        return;
    }
    set_energy(game, 100);
    save_career(game);
    back_from_drink(game);
}

static void quit_career(SSRGame *game) {
    save_career(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 0);
    main_menu(game);
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
    {"message.btn_Ok", message_ok},
    {"message.btn_Cancel", message_cancel},
    {"saveslot.btn_Slot0", slot0},
    {"saveslot.btn_Slot1", slot1},
    {"saveslot.btn_Slot2", slot2},
    {"saveslot.btn_DeleteSlot0", delete0},
    {"saveslot.btn_DeleteSlot1", delete1},
    {"saveslot.btn_DeleteSlot2", delete2},
    {"saveslot.btn_Back", main_menu},
    {"chooseclub.inp_NameF_HIT", focus_first},
    {"chooseclub.inp_NameL_HIT", focus_last},
    {"chooseclub.btn_Gender", gender},
    {"chooseclub.btn_Difficulty", difficulty},
    {"chooseclub.btn_Back", main_menu},
    {"chooseclub.btn_Ok", profile_ok},
    {"choosenation.cmb_Continent", combo_continent},
    {"choosenation.cmb_Nation", combo_nation},
    {"choosenation.cmb_League", combo_league},
    {"choosenation.btn_Ok", nation_ok},
    {"webpage.btn_Ok", game_menu},
    {"gamemenu.btn_Quit", quit_career},
    {"gamemenu.btn_Play", start_match},
    {"match.btn_Play", match_play},
    {"match.btn_Time", match_speed},
    {"match.btn_WorkRate", match_work_rate},
    {"match.btn_Focus", match_focus},
    {"match.btn_MaximiseCommentary", maximise_commentary},
    {"match.btn_NRG", match_drink},
    {"gamemenu.btn_DrinkNRG", drink_button},
    {"chooseboost.btn_1", boost1},
    {"chooseboost.btn_2", boost2},
    {"chooseboost.btn_3", boost3},
    {"chooseboost.btn_4", boost4},
    {"chooseboost.btn_5", boost5},
    {"chooseboost.btn_Ok", boost_ok},
};

/* -------------------------------------------------------------- the API */

SSRGame *ssr_game_create(SSRTweaks *tweaks) {
    SSRGame *game = calloc(1, sizeof(SSRGame));
    if (!game) return NULL;
    game->tweaks = tweaks;
    game->settings.volume = 1;
    game->settings.replays = 1;
    game->random.seed = SSR_RANDOM_START;
    set_option_tweaks(game);
    set_screen(game, "mainmenu");
    return game;
}

void ssr_game_free(SSRGame *game) {
    if (!game) return;
    free(game->match);
    free(game->chance);
    free(game);
}

void ssr_game_set_data(SSRGame *game, const SSRWorld *world, SSRLocalise localise, void *context,
                       const char *saveDirectory) {
    if (!game) return;
    game->world = world;
    game->localise = localise;
    game->context = context;
    snprintf(game->saveDirectory, sizeof(game->saveDirectory), "%s", saveDirectory ? saveDirectory : "");
}

void ssr_game_set_seed(SSRGame *game, int seed) {
    if (game) game->random.seed = seed;
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

const SSRCareer *ssr_game_career(const SSRGame *game) {
    return game ? &game->career : NULL;
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

// The GS_ functions of the layouts (c_ShowNameF, c_GS_PlayerClubName).
const char *ssr_game_text(SSRGame *game, const char *name) {
    if (!game || !name) return NULL;
    if (strcmp(name, "GS_ShowNameF") == 0) {
        return game->firstName[0] || game->typing == 1 ? game->firstName : loc(game, "FIRSTNAME_ENTRY_INSTRUCTIONS");
    }
    if (strcmp(name, "GS_ShowNameL") == 0) {
        return game->lastName[0] || game->typing == 2 ? game->lastName : loc(game, "LASTNAME_ENTRY_INSTRUCTIONS");
    }
    if (strcmp(name, "GS_PlayerClubName") == 0) {
        const SSRClub *club = game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
        return club ? club->tla : loc(game, "Club");
    }
    if (strcmp(name, "GS_PlayerFullName") == 0) {
        snprintf(game->shown, sizeof(game->shown), "%s", game->career.fullName);
        return game->shown;
    }
    return NULL;
}

int ssr_game_wants_text(const SSRGame *game) {
    return game && game->typing != 0;
}

void ssr_game_type(SSRGame *game, const char *text) {
    if (!game || !game->typing || !text) return;
    char *name = game->typing == 1 ? game->firstName : game->lastName;
    for (const char *p = text; *p; p++) {
        // A comma or a semicolon would break the save of the game.
        if (*p == ',' || *p == ';' || (unsigned char)*p < 32) continue;
        size_t length = strlen(name);
        // A letter of UTF-8 has more bytes. Count the letters.
        int letters = 0;
        for (const char *q = name; *q; q++) letters += ((unsigned char)*q & 0xC0) != 0x80;
        if (((unsigned char)*p & 0xC0) != 0x80 && letters >= NAME_LIMIT) break;
        if (length + 1 >= SSR_CAREER_NAME) break;
        name[length] = *p;
        name[length + 1] = 0;
        // The first letter is a capital, as the keyboard of the phone does.
        if (length == 0) name[0] = (char)toupper((unsigned char)name[0]);
    }
}

void ssr_game_key(SSRGame *game, SSRKey key) {
    if (!game || !game->typing) return;
    char *name = game->typing == 1 ? game->firstName : game->lastName;
    if (key == SSR_KEY_BACKSPACE) {
        size_t length = strlen(name);
        // Removes the last letter, with all the bytes of its UTF-8.
        while (length > 0 && ((unsigned char)name[length - 1] & 0xC0) == 0x80) length--;
        if (length > 0) length--;
        name[length] = 0;
    } else if (key == SSR_KEY_ENTER) {
        // Enter on the first name moves to the last name (c_NameFInputBox).
        game->typing = game->typing == 1 ? 2 : 0;
        set_profile_tweaks(game);
    }
}

void ssr_game_update(SSRGame *game, float frames) {
    if (!game) return;
    if (game->match) {
        // The match runs in ticks of 60 a second, as the game does.
        game->ticks += frames;
        while (game->ticks >= 1 && game->match) {
            game->ticks -= 1;
            game->now += 16;
            match_tick(game);
        }
    }
    if (strcmp(game->screen, "startransition") == 0) {
        game->transitionFrames += frames;
        if (game->transitionFrames > 55) {
            char after[64];
            snprintf(after, sizeof(after), "%s", game->afterTransition);
            if (strcmp(after, "webpagenews") == 0) {
                webpage(game);
            } else if (strcmp(after, "gamemenu") == 0) {
                game_menu(game);
            } else {
                set_screen(game, after);
            }
        }
    }
}

int ssr_game_take_op(SSRGame *game, SSRUiOp *op) {
    if (!game || game->opCount == 0 || !op) return 0;
    *op = game->ops[0];
    memmove(game->ops, game->ops + 1, (size_t)(game->opCount - 1) * sizeof(SSRUiOp));
    game->opCount--;
    return 1;
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

const SSRMatch *ssr_game_match(const SSRGame *game) {
    return game ? game->match : NULL;
}

const SSRChance *ssr_game_chance(const SSRGame *game) {
    return game ? game->chance : NULL;
}

void ssr_game_touch(SSRGame *game, int down, float x, float y) {
    if (!game) return;
    game->touch.down = down;
    game->touch.x = x;
    game->touch.y = y;
}

int ssr_game_banner(const SSRGame *game) {
    return game && game->now < game->bannerEnd;
}
