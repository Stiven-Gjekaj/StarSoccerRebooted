#include "ssr_game.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_aftermatch.h"
#include "ssr_career.h"
#include "ssr_fs.h"
#include "ssr_match.h"
#include "ssr_random.h"
#include "ssr_season.h"
#include "ssr_shop.h"
#include "ssr_sort.h"
#include "ssr_training.h"

#define MAX_OPS 512
// The game takes 11 letters for a name from the keyboard (c_TInputBox).
#define NAME_LIMIT 11
// England, the nation that the English game chooses first.
#define DEFAULT_NATION 62

// A colour of the game: r, g, b from 0 to 255 (c_GColour).
typedef struct {
    float r, g, b;
} Rgb;

// A team of a match: a club or a nation (c_TBase_Team).
typedef struct {
    int id;
    const char *tla;
    int strength;
    const SSRKit *home, *away;
    // c_TClub.LoadClub and c_TNation: the two badge colours and the style.
    Rgb badge1, badge2;
    int badgeStyle;
    // The style of the home shirt (m_kitstylehome), and 1 when the team
    // plays in its away kit (m_awaykit).
    int kitStyle;
    int awayKit;
} Team;

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
    // The param of the GUI message that came last (a row of a list).
    int messageParam;
    int returnState;
    // A message with a decision (c_TScreen_Message with decision): the
    // change of the relations that Yes and No make, and the time when the
    // function runs, 2000 ms after the choice.
    int decision;
    char messageText[64];
    int decisionEnd;

    // The career that plays now, and the slot that it uses.
    SSRCareer career;
    // The competitions of the career.
    SSRSeason *season;
    // The minute when the player comes on, or 0 (c_TPlayer.m_subtime).
    int subTime;
    // 1 when the last match went to extra time, and its penalties.
    int extraTime, pens[3];
    // The fixture of the match that plays now: its competition, date, and
    // sides. A fixture has no fixed place in its list.
    int fixtureComp, fixtureDate, fixtureHome, fixtureAway;
    // The two teams of the match, for the kits (c_TMatch.m_team1, m_team2),
    // and the kit choice of the kit button.
    Team homeTeam, awayTeam;
    int kitNum;
    // The competition of the table that shows, the row that the player
    // chose in it (0 none), and the team of the club fixtures screen.
    int leagueComp, leagueSelected;
    int fixturesTeam, fixturesLevel;
    // c_TScreen_League.m_flagContinueButton: the table shows Continue.
    int leagueContinue;
    // c_TPlayer.lastmatchclub >= 0: the match review waits.
    int reviewWaits;
    // c_TPlayer.wasPicked.
    int wasPicked;
    // The screens of the end of the season are open
    // (SeasonReview.m_screen != null).
    int seasonEndMode;
    // The player of the year screen: the stage, the time of the last stage,
    // and the awards.
    int potyStage, potyTime, potyCount;
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
    // The layout of the overlay over the screen, or "".
    char overlay[48];
    // The headline that the news page shows, and the screen after it.
    char news[512];
    char newsNext[32];
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

    // The screens after the match (c_TScreen_MatchStats and
    // c_TScreen_MatchReview): the facts and the result of the match, the
    // stage of the screen, the time of the last stage, the frames of the
    // result overlay, and the star level before the match.
    SSRAfterMatchFacts facts;
    SSRAfterMatch after;
    int statStage, statTime;
    float initDelay;
    int oldStarLevel;
    // The energy that the rest days give, and the pay of the match.
    int recovery, payWage, payGoals, payAssists;
    // The screen under the ratings screen (c_TScreen_Ratings.m_prevscreen).
    char ratingsReturn[64];

    // The goal replay: a copy of the chance for each tick that the game
    // records, 0 or the offer or the playback, and the time when the offer
    // goes on by itself.
    SSRChance *replay;
    int replayCount, replayCapacity;
    int replayMode;
    int offerEnd;

    // The drill of a trial of a new career, and the last banner that it
    // showed.
    SSRTraining *training;
    const char *trainingMessage;
    // The splash message ("Success!", "Fail!") shows until this time.
    int splashEnd;

    // The relationships screen (c_TScreen_Relationships): the national
    // view, and the bars whose button sends no click (Boss, Team, Fans,
    // Girlfriend, Sponsors).
    int showInternational;
    int relationOff[5];
    // The pairs game (c_TScreen_Pairs): the relation, the icons in the
    // order of the list of the game, the state of the clicks, the time of
    // the last step, the try, and 1 when the list is made.
    int pairsRelation;
    struct {
        int id, randNo, picked, imgId;
    } icons[16];
    int iconOrder[16];
    int pairsMade;
    int clicked, timeClicked, turn;

    // The dilemma (c_TScreen_Dilemma): the two relations, their images,
    // the national flags, the choice, the time of the end, and the day of
    // the last accident (a static of the game, not in the save).
    int dilemmaRelation[2], dilemmaImage[2], dilemmaInt[2];
    int dilemmaChoice, dilemmaChosen, dilemmaEnd;
    int lastAccident;

    // The interview after a match (c_TScreen_Interview): 1 once a match
    // has had one, the words to repeat, the buttons, the decoys, the
    // sequence (a decoy is below 0), the word of the sequence, the time of
    // the next step, the state, and the sentence so far.
    int interviewDone;
    int difficulty, buttonCount, buttons, decoys;
    int sequence[15];
    char words[15][64];
    int countdown, beepDone, ready, interviewFail;
    char sentence[512];
};

enum { REPLAY_NONE, REPLAY_OFFER, REPLAY_PLAY };
// 60 seconds of ticks.
#define REPLAY_MAX 3600

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

static void message_relations(SSRGame *game, int state);

static void close_message(SSRGame *game, int state) {
    game->returnState = state;
    if (game->decision) {
        // The bars move for 2000 ms, then the function runs.
        message_relations(game, state);
        game->decision = 0;
        game->decisionEnd = game->now + 2000;
        return;
    }
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

static void season_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/season%d.txt", game->saveDirectory, slot + 1);
}

// The season reads the date and the team of the player from the career,
// and gives back the date.
static void season_in(SSRGame *game) {
    if (!game->season) return;
    game->season->player.sdate = game->career.sdate;
    game->season->player.clubId = game->career.clubId;
    game->season->player.nationId = game->career.nationId;
    game->season->player.careerApps = game->career.apps;
}

static void season_out(SSRGame *game) {
    if (!game->season) return;
    game->career.sdate = game->season->player.sdate;
    // c_TCompetition.DoCupWinnerNews: the results show it next.
    SSRSeason *s = game->season;
    if (s->hasWinnerNews) {
        char *text = game->career.headlineResult;
        snprintf(text, sizeof(game->career.headlineResult), "%s", loc(game, "CRESULTNEWS_COMPETITIONWINNERS"));
        replace_all(text, sizeof(game->career.headlineResult), "$winningteam", s->winnerTla);
        replace_all(text, sizeof(game->career.headlineResult), "$competition", s->winnerCompetition);
        s->hasWinnerNews = 0;
    }
}

// A new season state for the career: no competition is set up.
static void new_season_state(SSRGame *game) {
    ssr_season_free(game->season);
    game->season = game->world ? ssr_season_create(game->world, &game->random) : NULL;
    season_in(game);
}

// The date goes to the day before the first match of the player
// (c_TPlayer.SkipToDate in StartCareer and NewSeasonSetUp2).
static void skip_to_first_match(SSRGame *game) {
    season_in(game);
    SSRFixture *next = ssr_season_next_fixture(game->season);
    if (next) ssr_season_skip_to(game->season, next->sdate);
    season_out(game);
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
static void skill_tweaks(SSRGame *game);
static void choose_club(SSRGame *game);
static void star_transition(SSRGame *game, const char *after);
static void player_tweaks(SSRGame *game);
static void reset_progress(SSRGame *game);
static void rating_tweaks(SSRGame *game);

static void open_slot(SSRGame *game, int slot) {
    game->slot = slot;
    char path[2200];
    slot_path(game, slot, path, sizeof(path));
    if (game->slotExists[slot] && ssr_career_load(&game->career, path)) {
        new_season_state(game);
        season_path(game, slot, path, sizeof(path));
        if (game->season && !ssr_season_load(game->season, path)) {
            // A career from before the seasons gets its competitions now.
            ssr_season_set_up(game->season);
            skip_to_first_match(game);
        }
        // LoadCareer: the star transition, then the game menu.
        player_tweaks(game);
        reset_progress(game);
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
        season_path(game, game->slot, path, sizeof(path));
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
    // SetUpCompetitionsAll comes after the choice of the club (CSPEC 7).
    new_season_state(game);
    ssr_season_set_up(game->season);
    // The first of the five trials.
    message(game, "messagedefault", loc(game, "CMESSAGE_TRIALMOBILE1"), 0, 1, "DoTrial1");
}

// The text at the end of the trials, with the club and its league.
static void trial_success_text(SSRGame *game, char *text, size_t capacity) {
    const SSRClub *club = game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
    const SSRCompetition *comp = club ? ssr_world_competition(game->world, club->league) : NULL;
    snprintf(text, capacity, "%s", loc(game, "CMESSAGE_TRIALSUCCESSMOBILE"));
    replace_all(text, capacity, "$clubname", club ? club->name : "");
    replace_all(text, capacity, "$labelname", club ? club->tla : "");
    replace_all(text, capacity, "$leaguename", comp ? comp->tla : "");
}

// DoTrial1 to DoTrial5 (c_TScreen_Message): the hint of the trial, then the
// drill (SetUpTraining(type, 1, 3, trial)).
static void do_trial(SSRGame *game, int trial) {
    static const int TYPES[5] = {2, 3, 4, 5, 6};
    char step[8];
    snprintf(step, sizeof(step), "%d", trial);
    ssr_tweaks_set(game->tweaks, "Menu", "MGTrialStep", step);
    if (trial == 1) ssr_tweaks_set_number(game->tweaks, "Pitch", "TrialsLogo", 1);
    if (!game->training) game->training = calloc(1, sizeof(SSRTraining));
    if (!game->training) return;
    const SSRCareer *c = &game->career;
    SSRTrainingPlayer player = {.power = ssr_career_skill(c->power),
                                .technique = ssr_career_skill(c->technique),
                                .pace = ssr_career_skill(c->pace),
                                .starLevel = ssr_career_star_level(c)};
    ssr_training_start(game->training, TYPES[trial - 1], 1, 3, trial, &player, &game->random);
    game->trainingMessage = NULL;
    ssr_tweaks_set_number(game->tweaks, "Menu", "Lifecount", game->training->lives);
    ssr_tweaks_set_number(game->tweaks, "Training", "TrainingType", game->training->training);
    set_screen(game, "matchminigame");
}

static void call_do_trial1(SSRGame *game) {
    do_trial(game, 1);
}

static void call_do_trial2(SSRGame *game) {
    do_trial(game, 2);
}

static void call_do_trial3(SSRGame *game) {
    do_trial(game, 3);
}

static void call_do_trial4(SSRGame *game) {
    do_trial(game, 4);
}

static void call_do_trial5(SSRGame *game) {
    do_trial(game, 5);
}

// TrainingSuccess and TrainingFailure: a success writes the lives that are
// left into the first level of the skill. Then the next trial, or the end.
static void end_trial(SSRGame *game) {
    SSRTraining *t = game->training;
    int trial = t->trial, won = t->over == 1;
    if (won) {
        SSRCareer *c = &game->career;
        int *skills[5] = {c->technique, c->setPieces, c->power, c->pace, c->vision};
        int skill = ssr_training_skill(t->training);
        if (skill >= 0) skills[skill][0] = t->lives;
    }
    free(game->training);
    game->training = NULL;
    char text[1024];
    if (trial >= 5) {
        trial_success_text(game, text, sizeof(text));
        message(game, won ? "messagedefault" : "message", text, 0, 1, "TrialDone");
        return;
    }
    // The dribble drill has its own text (dodribbling is 1).
    char key[64], call[16];
    snprintf(key, sizeof(key), "CMESSAGE_TRIALMOBILE%s%d%s", won ? "" : "FAIL", trial + 1,
             trial + 1 == 4 ? "_DRIBBLING" : "");
    snprintf(call, sizeof(call), "DoTrial%d", trial + 1);
    message(game, "messagedefault", loc(game, key), 0, 1, call);
}

// One tick of a trial: the drill, its hint, its lives, and its banners.
static void trial_tick(SSRGame *game) {
    SSRTraining *t = game->training;
    SSRTapTouch touch = game->touch;
    touch.hit = touch.down && !game->touchWasDown;
    game->touchWasDown = touch.down;
    ssr_training_update(t, game->now, &touch, &game->random);
    // c_TrialHud.m_Update: the hint goes after the first aim.
    if (t->chance.kicking == 1) ssr_tweaks_set(game->tweaks, "Menu", "MGTrialStep", "1aim");
    ssr_tweaks_set_number(game->tweaks, "Menu", "Lifecount", t->lives);
    if (t->message && t->message != game->trainingMessage) {
        if (strcmp(t->message, "Success!") == 0 || strcmp(t->message, "Fail!") == 0) {
            // TQuickMessage "splashmessage": the text in capitals.
            char text[64];
            snprintf(text, sizeof(text), "%s", loc(game, t->message));
            for (char *p = text; *p; p++) *p = (char)toupper((unsigned char)*p);
            ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
            game->splashEnd = t->messageEnd;
        } else {
            // The banner of the result (scoreanim.uis).
            ssr_tweaks_set_number(game->tweaks, "Minigames", "Outcome", t->outcome);
            ssr_tweaks_set_number(game->tweaks, "Chance", "IsHead", 0);
            game->bannerEnd = t->messageEnd;
        }
    }
    game->trainingMessage = t->message;
    if (t->over) end_trial(game);
}

static void call_trial_done(SSRGame *game) {
    ssr_tweaks_set(game->tweaks, "Menu", "MGTrialStep", "0");
    ssr_tweaks_set_number(game->tweaks, "Pitch", "TrialsLogo", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "TrialCompleteIncrement", 1);
    message(game, "message", "", 0, 1, "StartCareer");
}

static void save_career(SSRGame *game) {
    if (!game->saveDirectory[0]) return;
    char path[2200];
    slot_path(game, game->slot, path, sizeof(path));
    ssr_career_save(&game->career, path);
    if (game->season) {
        season_path(game, game->slot, path, sizeof(path));
        ssr_season_save(game->season, path);
    }
}

static void webpage(SSRGame *game);

// StartCareer: the first skills, the first save, and the news.
static void call_start_career(SSRGame *game) {
    SSRCareer *c = &game->career;
    int *skills[5] = {c->technique, c->pace, c->vision, c->setPieces, c->power};
    for (int i = 0; i < 5; i++) {
        if (skills[i][0] == 0) skills[i][0] = 1;
    }
    skip_to_first_match(game);
    const SSRClub *club = game->world ? ssr_world_club(game->world, c->clubId) : NULL;
    snprintf(c->headline, sizeof(c->headline), "%s", loc(game, "CNEWS_FIRSTCONTRACTMOBILE"));
    replace_all(c->headline, sizeof(c->headline), "$clubname", club ? club->name : "");
    replace_all(c->headline, sizeof(c->headline), "$playername", c->fullName);
    replace_all(c->headline, sizeof(c->headline), "$lastname", c->shortName);
    save_career(game);
    player_tweaks(game);
    reset_progress(game);
    // The news page shows the headline once (c_TScreen_WebPage).
    snprintf(game->news, sizeof(game->news), "%s", c->headline);
    c->headline[0] = 0;
    star_transition(game, "webpagenews");
}

static void call_drink(SSRGame *game);
static void call_season_end_transfer(SSRGame *game);
static void call_play_to_date(SSRGame *game);
static void call_set_up_fixture(SSRGame *game);
static void call_debut_tired(SSRGame *game);
static void call_relation_screen(SSRGame *game);
static void call_girl_dumps_you(SSRGame *game);
static void call_meet_partner(SSRGame *game);
static void call_relationship_request(SSRGame *game);
static void call_vehicle_team(SSRGame *game);
static void call_vehicle_partner(SSRGame *game);
static void call_tip(SSRGame *game);
static void call_expired_item(SSRGame *game);
static void call_dump_partner(SSRGame *game);

static void run_call(SSRGame *game, const char *name) {
    static const struct {
        const char *name;
        CallFunction call;
    } CALLS[] = {
        {"DeleteCareer", call_delete_career},
        {"TrialDone", call_trial_done},
        {"DoTrial1", call_do_trial1},
        {"DoTrial2", call_do_trial2},
        {"DoTrial3", call_do_trial3},
        {"DoTrial4", call_do_trial4},
        {"DoTrial5", call_do_trial5},
        {"StartCareer", call_start_career},
        {"DrinkNRG", call_drink},
        {"SeasonEndTransfer", call_season_end_transfer},
        {"PlayToDate", call_play_to_date},
        {"SetUpFixture", call_set_up_fixture},
        {"DebutTired", call_debut_tired},
        {"DoRelationScreen", call_relation_screen},
        {"GirlDumpsYou", call_girl_dumps_you},
        {"MeetGirl", call_meet_partner},
        {"DoRelationshipRequest", call_relationship_request},
        {"VehicleBorrowedTeam", call_vehicle_team},
        {"VehicleBorrowedGirl", call_vehicle_partner},
        {"DoTip", call_tip},
        {"ExpiredItem", call_expired_item},
        {"DumpGirl", call_dump_partner},
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

// c_TScreen_WebPage.SetUpScreen: the result headline first, else the
// headline. Each shows once. The page of the style news has its own
// look. Ok goes to the next screen.
static void webpage_to(SSRGame *game, const char *next, int style) {
    SSRCareer *c = &game->career;
    if (c->headlineResult[0]) {
        snprintf(game->news, sizeof(game->news), "%s", c->headlineResult);
        c->headlineResult[0] = 0;
    } else if (c->headline[0]) {
        snprintf(game->news, sizeof(game->news), "%s", c->headline);
        c->headline[0] = 0;
    }
    snprintf(game->newsNext, sizeof(game->newsNext), "%s", next ? next : "");
    set_screen(game, style ? "webpagestyle" : "webpagenews");
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
    // UpdatePlayerBasicTweaks.
    ssr_tweaks_set_number(t, "Player", "Happiness", ssr_career_happiness(c));
    ssr_tweaks_set_number(t, "Player", "Lifestyle", ssr_career_lifestyle(c));
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

static void webpage_to(SSRGame *game, const char *next, int style);

static void game_menu(SSRGame *game) {
    // GameMenu.SetUpScreen: a headline that is left shows first.
    if (game->career.headline[0]) {
        webpage_to(game, "gamemenu", 0);
        return;
    }
    set_screen(game, "gamemenu");
    ssr_tweaks_set_number(game->tweaks, "Menu", "TabPos", 0);
    player_tweaks(game);
}

/* ------------------------------------------------------------ match */

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
    if (game->match->state == SSR_MATCH_HALF_TIME || game->match->state == SSR_MATCH_PENALTIES) state = 2;
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

static Rgb rgb(SSRColor c) {
    Rgb x = {(float)((c >> 16) & 0xFF), (float)((c >> 8) & 0xFF), (float)(c & 0xFF)};
    return x;
}

// c_GColour.Brightness.
static float brightness(Rgb c) {
    return c.r * 0.2989f + c.g * 0.587f + c.b * 0.114f;
}

// c_GColour.Distance.
static float colour_distance(Rgb a, Rgb b) {
    return (fabsf(a.r - b.r) + fabsf(a.g - b.g) * 2.0f + fabsf(a.b - b.b)) / 4.0f;
}

static void set_colour_tweak(SSRGame *game, const char *name, Rgb c, float alpha) {
    char value[16];
    int v[3] = {(int)(c.r + 0.5f), (int)(c.g + 0.5f), (int)(c.b + 0.5f)};
    for (int i = 0; i < 3; i++) v[i] = v[i] < 0 ? 0 : v[i] > 255 ? 255 : v[i];
    snprintf(value, sizeof(value), "%02X%02X%02X%02X", v[0], v[1], v[2], (int)(alpha * 255 + 0.5f));
    ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, value, SSR_TWEAK_COLOUR);
}

static int team_of(SSRGame *game, int level, int id, int strength, Team *team) {
    memset(team, 0, sizeof(*team));
    team->id = id;
    team->strength = strength;
    team->badgeStyle = id % 15;
    if (level == 1) {
        const SSRNation *n = ssr_world_nation(game->world, id);
        if (!n) return 0;
        team->tla = n->tla;
        team->home = &n->home;
        team->away = &n->away;
        team->badge1 = rgb(n->home.shirt1);
        team->badge2 = rgb(n->away.shirt1);
        team->kitStyle = n->homeKitType;
        return 1;
    }
    const SSRClub *c = ssr_world_club(game->world, id);
    if (!c) return 0;
    team->tla = c->tla;
    team->home = &c->home;
    team->away = &c->away;
    team->kitStyle = c->homeStyle;
    team->badge1 = rgb(c->home.shirt1);
    team->badge2 = rgb(c->home.shirt2);
    // A badge of one colour takes the colour of the away shirt.
    if (c->home.shirt1 == c->home.shirt2) team->badge2 = rgb(c->away.shirt1);
    return 1;
}

// c_AScreen_Match.m_SetUpTeamShields and m_SetUpTeamPlates: the badges and
// the name plates take the badge colours, with dark text on a bright plate.
static void team_plates(SSRGame *game, const Team *home, const Team *away, const Team *mine) {
    const Team *teams[2] = {home, away};
    char name[48];
    for (int i = 0; i < 2; i++) {
        snprintf(name, sizeof(name), "Badge%dCol1", i + 1);
        set_colour_tweak(game, name, teams[i]->badge1, 1);
        snprintf(name, sizeof(name), "Badge%dCol2", i + 1);
        set_colour_tweak(game, name, teams[i]->badge2, 1);
        snprintf(name, sizeof(name), "BadgeStyle%d", i + 1);
        ssr_tweaks_set_number(game->tweaks, "MatchColours", name, teams[i]->badgeStyle);
    }
    set_colour_tweak(game, "MyTeamBadge1Col1", mine->badge1, 1);
    set_colour_tweak(game, "MyTeamBadge1Col2", mine->badge2, 1);
    float bright = (float)ssr_tweaks_number(game->tweaks, "MatchColours", "BrightKit");
    Rgb col[2][2];
    for (int i = 0; i < 2; i++) {
        const Team *t = teams[i];
        int swap = brightness(t->badge1) > 200 && brightness(t->badge2) < brightness(t->badge1);
        col[i][0] = swap ? t->badge2 : t->badge1;
        col[i][1] = swap ? t->badge1 : t->badge2;
    }
    // Two plates of near the same colour: one gets darker or brighter.
    Rgb *h = &col[0][0], *a = &col[1][0];
    if (colour_distance(*h, *a) < 20) {
        Rgb *dark = brightness(*h) < brightness(*a) ? h : a, *other = dark == h ? a : h;
        if (brightness(*dark) > 50) {
            dark->r *= 0.6f;
            dark->g *= 0.6f;
            dark->b *= 0.6f;
        } else {
            other->r *= 1.3f;
            other->g *= 1.3f;
            other->b *= 1.3f;
        }
    }
    const char *sides[2] = {"Home", "Away"};
    for (int i = 0; i < 2; i++) {
        int light = brightness(col[i][0]) > bright;
        Rgb text = light ? (Rgb){51, 51, 51} : (Rgb){255, 255, 255};
        snprintf(name, sizeof(name), "Nameplate%sColText", sides[i]);
        set_colour_tweak(game, name, text, 1);
        snprintf(name, sizeof(name), "Nameplate%sColShad", sides[i]);
        ssr_tweaks_set_kind(game->tweaks, "MatchColours", name, light ? "0000001A000000FF" : "00000080000000FF",
                            SSR_TWEAK_COLOUR);
        snprintf(name, sizeof(name), "Nameplate%sCol1", sides[i]);
        set_colour_tweak(game, name, col[i][0], 1);
        snprintf(name, sizeof(name), "Nameplate%sCol2", sides[i]);
        set_colour_tweak(game, name, col[i][1], 1);
    }
}

// A shirt colour of a team in its kit now; black shows as dark grey
// (c_TBase_Team.GetShirtColour1 and 2).
static SSRColor shirt_colour(const Team *t, int second) {
    const SSRKit *kit = t->awayKit ? t->away : t->home;
    // The away kit of the game has one shirt colour.
    SSRColor c = second && !t->awayKit ? kit->shirt2 : kit->shirt1;
    return (c & 0xFFFFFF) == 0 ? 0x333333 : c;
}

static void strip_tweak(SSRGame *game, const char *name, SSRColor c) {
    char value[16];
    snprintf(value, sizeof(value), "%06XFF", (unsigned)(c & 0xFFFFFF));
    ssr_tweaks_set_kind(game->tweaks, "TeamStrip", name, value, SSR_TWEAK_COLOUR);
}

// c_AScreen_Match.m_UpdateKits: the kits of the kit button and the match.
// A team in its away kit keeps the style of the kit before.
static void update_kits(SSRGame *game) {
    const Team *teams[2] = {&game->homeTeam, &game->awayTeam};
    const char *sides[2] = {"Home", "Away"};
    char name[48];
    for (int i = 0; i < 2; i++) {
        snprintf(name, sizeof(name), "%sKitColour", sides[i]);
        strip_tweak(game, name, shirt_colour(teams[i], 0));
        snprintf(name, sizeof(name), "%sStripesColour", sides[i]);
        strip_tweak(game, name, shirt_colour(teams[i], 1));
        if (teams[i]->awayKit) continue;
        int style = teams[i]->kitStyle == 2 ? 1 : teams[i]->kitStyle == 8 ? 2 : teams[i]->kitStyle == 10 ? 3 : 0;
        snprintf(name, sizeof(name), "%sKitStyle", sides[i]);
        if (style) ssr_tweaks_set_number(game->tweaks, "TeamStrip", name, style);
    }
    int homeIsMine = game->match && game->match->setup.myTeam == 1;
    const Team *mine = homeIsMine ? &game->homeTeam : &game->awayTeam;
    const Team *other = homeIsMine ? &game->awayTeam : &game->homeTeam;
    kit_tweaks(game, "MyTeam", mine->awayKit ? mine->away : mine->home);
    kit_tweaks(game, "OppTeam", other->awayKit ? other->away : other->home);
}

// leaguefixtures.btn_Kits: the kits go round (home, away) = (0, 0),
// (0, 1), (1, 1), (1, 0).
static void kits_button(SSRGame *game) {
    if (!game->match) return;
    static const int KITS[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
    game->kitNum = (game->kitNum + 1) % 4;
    game->homeTeam.awayKit = KITS[game->kitNum][0];
    game->awayTeam.awayKit = KITS[game->kitNum][1];
    update_kits(game);
}

// c_TMatch.SetUpFixture: the match of the next fixture of the player.
static const SSRFixture *first_leg_of(const SSRSeasonComp *comp, const SSRFixture *f);

static void start_match(SSRGame *game) {
    season_in(game);
    SSRFixture *f = ssr_season_next_fixture(game->season);
    SSRSeasonComp *comp = f ? ssr_season_comp(game->season, f->compId) : NULL;
    if (!f || !comp) return;
    SSRPool *pool = &comp->pools[f->group - 1];
    SSRTableRow *homeRow = ssr_season_row(game->season, pool, f->home);
    SSRTableRow *awayRow = ssr_season_row(game->season, pool, f->away);
    if (!homeRow || !awayRow) return;
    Team home, away;
    int level = comp->data->level;
    if (!team_of(game, level, homeRow->teamId, homeRow->strength, &home) ||
        !team_of(game, level, awayRow->teamId, awayRow->strength, &away)) {
        return;
    }
    int mine = level == 1 ? game->career.nationId : game->career.clubId;
    int homeIsMine = home.id == mine;
    game->fixtureComp = f->compId;
    game->fixtureDate = f->sdate;
    game->fixtureHome = f->home;
    game->fixtureAway = f->away;
    free(game->match);
    game->match = calloc(1, sizeof(SSRMatch));
    if (!game->match) return;
    const SSRCareer *c = &game->career;
    SSRMatchSetup s = {0};
    s.homeName = home.tla;
    s.awayName = away.tla;
    s.homeStrength = (float)home.strength;
    s.awayStrength = (float)away.strength;
    s.myTeam = homeIsMine ? 1 : 2;
    s.playerName = c->shortName;
    s.focus = 2;
    s.workRate = 2;
    s.energy = c->energy;
    s.age = ssr_career_age(c);
    s.relationTeam = (int)c->relationTeam;
    s.relationBoss = (int)c->relationBoss;
    s.setPieces = ssr_career_skill(c->setPieces);
    s.vision = ssr_career_skill(c->vision);
    s.careerApps = c->apps;
    s.starLevel = ssr_career_star_level(c);
    // c_TPlayer sets m_dodribbling to 1 for each career, new or loaded.
    s.dribbling = 1;
    s.subTime = game->subTime;
    s.matchType = f->matchType;
    const SSRFixture *one = f->leg == 2 ? first_leg_of(comp, f) : NULL;
    s.firstLegHome = one ? one->score1 : 0;
    s.firstLegAway = one ? one->score2 : 0;
    ssr_match_start(game->match, &s, game->localise, game->context, &game->random);
    game->commentaryCount = 0;
    SSRTweaks *t = game->tweaks;
    // c_TScreen_LeagueFixtures.m_ButtonKits(true): the away team plays in
    // its away kit when the shirts of the two teams are the same.
    game->kitNum = 0;
    home.awayKit = 0;
    away.awayKit = 0;
    if (home.home->shirt1 == away.home->shirt1) {
        away.awayKit = 1;
        game->kitNum = 1;
    }
    game->homeTeam = home;
    game->awayTeam = away;
    update_kits(game);
    team_plates(game, &home, &away, homeIsMine ? &home : &away);
    ssr_tweaks_set(t, "Match", "HomeName", s.homeName);
    ssr_tweaks_set(t, "Match", "AwayName", s.awayName);
    ssr_tweaks_set(t, "Match", "LeagueName", comp->data->tla);
    // c_AScreen_Match.m_SetUpLeagueShield.
    ssr_tweaks_set_number(t, "Match", "MatchCompetition", ssr_season_comp_type(game->season, comp));
    ssr_tweaks_set_number(t, "Match", "AggHomeScore", -1);
    ssr_tweaks_set_number(t, "Match", "AggAwayScore", -1);
    ssr_tweaks_set_number(t, "Match", "MaximiseScreen", 0);
    set_screen(game, "match");
    match_tweaks(game);
    match_state(game);
}

// The star level and its progress, for the star bars (Player.StarRating
// and Player.StarRatingProgress).
static void star_tweaks(SSRGame *game) {
    const SSRCareer *c = &game->career;
    ssr_tweaks_set_number(game->tweaks, "Player", "StarRating", ssr_career_star_level(c));
    ssr_tweaks_set_number(game->tweaks, "Player", "StarRatingProgress", c->starRating % 100);
}

// GSResetProgressTweaks: the bars show the values as they are, with no
// animation from the values before.
static void reset_progress(SSRGame *game) {
    skill_tweaks(game);
    star_tweaks(game);
    rating_tweaks(game);
    static const char *NAMES[10] = {"Energy",    "BossRelationship", "TeamRelationship", "FansRelationship",
                                    "StarRatingProgress", "Technique", "Power", "Pace", "Vision", "SetPieces"};
    char name[64], shown[80];
    for (int i = 0; i < 26; i++) {
        if (i < 10) {
            snprintf(name, sizeof(name), "%s", NAMES[i]);
        } else {
            snprintf(name, sizeof(name), "%sShiftedRating%d", i < 18 ? "" : "Negative", (i - 10) % 8 + 1);
        }
        double value = ssr_tweaks_number(game->tweaks, "Player", name);
        snprintf(shown, sizeof(shown), "%s_Displayed", name);
        ssr_tweaks_set_number(game->tweaks, "Player", shown, value);
        snprintf(shown, sizeof(shown), "%s_Before", name);
        ssr_tweaks_set_number(game->tweaks, "Player", shown, value);
    }
}

// UpdateRelationship: a relation stays from 1 to 100.
static void change_relation(float *relation, float change) {
    float v = *relation + change;
    *relation = v < 1 ? 1 : v > 100 ? 100 : v;
}

// GSMatchStatsUpdateRelationships: a club match changes the relations with
// the club, and with the boss of the nation by the strength of the club
// against the nation. A match of the nation changes the relations with the
// nation.
static void after_relations(SSRGame *game) {
    SSRCareer *c = &game->career;
    SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    float boss = (float)game->after.boss;
    if (comp && comp->data->level == 1) {
        change_relation(&c->relationBossInt, boss);
        change_relation(&c->relationTeamInt, (float)game->after.team);
        change_relation(&c->relationFansInt, (float)game->after.fans);
        return;
    }
    change_relation(&c->relationBoss, boss);
    change_relation(&c->relationTeam, (float)game->after.team);
    change_relation(&c->relationFans, (float)game->after.fans);
    SSRSeasonClub *club = game->season ? ssr_season_club(game->season, c->clubId, 0) : NULL;
    const SSRNation *nation = ssr_world_nation(game->world, c->nationId);
    if (!club || !nation) return;
    float strength = (float)club->strength, of = (float)nation->strength;
    float part = strength >= of ? 1.0f : strength > of * 0.85f ? 0.5f : strength > of * 0.7f ? 0.25f
                                     : strength > of * 0.5f  ? 0.15f
                                                             : 0.0f;
    change_relation(&c->relationBossInt, boss * part);
}

// The five calls of UpdateStarRating at stage 9, in their order.
static void after_star_rating(SSRGame *game) {
    SSRCareer *c = &game->career;
    int changes[5] = {game->facts.goals, game->facts.assists, game->after.boss, game->after.team, game->after.fans};
    for (int i = 0; i < 5; i++) c->starRating = ssr_star_rating_add(c->starRating, changes[i]);
    star_tweaks(game);
}

// The end of the match (c_TMatch.EndMatch): the career keeps the stats, and
// the match stats screen opens (c_TScreen_MatchStats.SetUpScreen). The match
// has no fixture yet, so it is a league match of the club [I].
static void end_match(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRMatch *m = game->match;
    const SSRMatchSetup *s = &m->setup;
    int home = s->myTeam == 1;
    SSRAfterMatchFacts f = {0};
    f.goals = m->goals;
    f.assists = m->assists;
    f.passes = m->passes;
    f.chances = m->chances;
    f.dribbles = m->dribbles;
    f.shotsOnTarget = m->shotsOnTarget;
    f.score1 = m->score[1];
    f.score2 = m->score[2];
    f.myScore = home ? f.score1 : f.score2;
    f.opponentScore = home ? f.score2 : f.score1;
    f.home = home;
    f.homeStrength = s->homeStrength;
    f.awayStrength = s->awayStrength;
    f.subTime = s->subTime;
    f.starLevel = ssr_career_star_level(c);
    f.hardMode = c->hardMode;
    // The game never adds to m_workratetally, so GetOverallWorkRate is
    // always 0 in 4.32.
    f.workRate = 0;
    game->facts = f;
    ssr_aftermatch_rate(&f, &game->after);
    // The counts of the club, or of the nation (c_TScreen_MatchStats).
    SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    float rating = game->after.rating;
    int hatTrick = m->goals >= 3;
    if (comp && comp->data->level == 1) {
        c->intApps++;
        c->intGoals += m->goals;
        c->intAssists += m->assists;
        c->intPasses += m->passes;
        c->intHatTricks += hatTrick;
        c->intChances += m->chances;
        c->intStarMan += game->after.starMan;
        c->intTotalRating += rating;
        ssr_career_add_form(c->intForm, (int)rating);
    } else {
        c->apps++;
        c->seasonApps++;
        c->goals += m->goals;
        c->seasonGoals += m->goals;
        c->assists += m->assists;
        c->seasonAssists += m->assists;
        c->passes += m->passes;
        c->seasonPasses += m->passes;
        c->seasonHatTricks += hatTrick;
        c->seasonChances += m->chances;
        c->seasonStarMan += game->after.starMan;
        c->seasonTotalRating += rating;
        c->totalRating += rating;
        ssr_career_add_form(c->form, (int)rating);
    }
    c->energy = m->energy;
    c->debut = 0;
    game->extraTime = m->half >= 3;
    game->pens[1] = m->pens[1];
    game->pens[2] = m->pens[2];
    // UpdateRating: one change of a skill rating for each chance.
    for (int i = 0; i < m->ratingCount; i++) ssr_career_update_rating(c, m->ratingIndex[i], m->ratingSuccess[i]);
    free(game->match);
    game->match = NULL;

    SSRTweaks *t = game->tweaks;
    set_screen(game, "matchstats");
    // c_AScreen_MatchStats.ClearStats.
    ssr_tweaks_set_number(t, "MatchStats", "StatStage", 0);
    static const char *STATS[5] = {"Chances", "Goals", "Assists", "Passes", "MatchRating"};
    for (int i = 0; i < 5; i++) ssr_tweaks_set_number(t, "MatchStats", STATS[i], -1);
    static const char *RISES[4] = {"FansRelIncrease", "TeamRelIncrease", "BossRelIncrease", "StarRatingIncrease"};
    for (int i = 0; i < 4; i++) ssr_tweaks_set_number(t, "MatchStats", RISES[i], -10);
    ssr_tweaks_set_number(t, "Menu", "ZeroRelChange", 0);
    static const char *MODS[4] = {"Boss", "Team", "Fans", "Sponsors"};
    for (int i = 0; i < 4; i++) ssr_tweaks_set_number(t, "RelationshipMods", MODS[i], 0);
    ssr_tweaks_set_number(t, "MatchStats", "International", 0);
    // No competition plate until the season comes.
    ssr_tweaks_set_number(t, "Menu", "LeagueType", 0);
    ssr_tweaks_set(t, "Match", "LeagueName", "");
    ssr_tweaks_set_number(t, "Match", "Outcome", game->after.outcome);
    skill_tweaks(game);
    star_tweaks(game);
    game->initDelay = 270;
    game->statStage = 0;
    game->statTime = game->now;
    game->oldStarLevel = ssr_career_star_level(c);
    game->interviewDone = 0;
}

static int start_interview(SSRGame *game);

// One stage of the match stats screen (GSMatchStatsUpdate).
static void stats_stage(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    const SSRAfterMatchFacts *f = &game->facts;
    int stage = ++game->statStage;
    switch (stage) {
    case 1:
        ssr_tweaks_set_number(t, "MatchStats", "Chances", f->chances);
        break;
    case 2:
        ssr_tweaks_set_number(t, "MatchStats", "Goals", f->goals);
        break;
    case 3:
        ssr_tweaks_set_number(t, "MatchStats", "Assists", f->assists);
        break;
    case 4:
        ssr_tweaks_set_number(t, "MatchStats", "Passes", f->passes);
        break;
    case 5:
        ssr_tweaks_set_number(t, "MatchStats", "MatchRating", game->after.rating);
        break;
    case 6:
        // Stages 7 and 8 do not come.
        ssr_tweaks_set_number(t, "RelationshipMods", "Boss", game->after.boss);
        ssr_tweaks_set_number(t, "RelationshipMods", "Team", game->after.team);
        ssr_tweaks_set_number(t, "RelationshipMods", "Fans", game->after.fans);
        game->statStage = stage = 8;
        after_relations(game);
        skill_tweaks(game);
        ssr_tweaks_set_number(t, "MatchStats", "StatStage", stage);
        if (start_interview(game)) return;
        break;
    case 9:
        after_star_rating(game);
        ssr_tweaks_set_number(t, "MatchStats", "StarRatingIncrease", 0);
        break;
    case 10:
        // [I: the star shower and the cheer of a new level come with the
        // particles and the sounds.]
        game->oldStarLevel = ssr_career_star_level(&game->career);
        break;
    default:
        break;
    }
    if (stage <= 12) ssr_tweaks_set_number(t, "MatchStats", "StatStage", stage);
}

// The energy and the pay of the weekly update (c_TScreen_MatchReview).
static void review_stage(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    SSRCareer *c = &game->career;
    int stage = ++game->statStage;
    ssr_tweaks_set_number(t, "MatchStats", "StatStage", stage);
    switch (stage) {
    case 1: {
        float e = c->energy + (float)game->recovery;
        c->energy = e < 0 ? 0 : e > 100 ? 100 : e;
        ssr_tweaks_set_number(t, "Player", "Recovery", game->recovery);
        ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
        break;
    }
    case 2:
        ssr_tweaks_set_number(t, "Player", "RecoveryBoostTrainer", 0);
        break;
    case 3:
        ssr_tweaks_set_number(t, "Player", "RecoveryBoostProperties", 0);
        break;
    case 4:
        ssr_tweaks_set_number(t, "Player", "RecoveryTotal", game->recovery > 100 ? 100 : game->recovery);
        break;
    case 5:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsWage", game->payWage);
        break;
    case 6:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentWage", 0);
        break;
    case 7:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsBonusGoals", game->payGoals);
        break;
    case 8:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentBonusGoals", 0);
        break;
    case 9:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsBonusAssists", game->payAssists);
        break;
    case 10:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentBonusAssists", 0);
        break;
    case 11:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsSponsors", 0);
        break;
    case 12:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentSponsors", 0);
        break;
    case 13: {
        // GSPlayerAwardWages: the pay goes into the bank once.
        int total = game->payWage + game->payGoals + game->payAssists;
        c->bank += total;
        ssr_tweaks_set_number(t, "MatchReview", "EarningsTotal", total);
        ssr_tweaks_set_number(t, "Player", "Bux", c->bank);
        ssr_tweaks_set_number(t, "Player", "StarBux", c->bank);
        break;
    }
    default:
        break;
    }
}

// The stages of the two screens come at an interval: 750 or 400 ms, or
// 100 ms while the player touches the screen.
static void after_tick(SSRGame *game) {
    int stats = strcmp(game->screen, "matchstats") == 0;
    if (stats && game->initDelay > 0) {
        game->initDelay -= 1;
        if (game->initDelay <= 0) ssr_tweaks_set_number(game->tweaks, "Match", "Outcome", 0);
        return;
    }
    int interval = game->touch.down ? 100 : stats ? 750 : 400;
    if (game->statStage < 20 && game->now > game->statTime + interval) {
        game->statTime = game->now;
        if (stats) {
            stats_stage(game);
        } else {
            review_stage(game);
        }
    }
}

// UpdatePlayerRatings and UpdatePlayerType: each of the first eight
// ratings as the part over 5 and the part under 5, and the type of the
// player.
static void rating_tweaks(SSRGame *game) {
    const SSRCareer *c = &game->career;
    char name[64];
    for (int i = 0; i < 8; i++) {
        float r = c->ratings[i];
        snprintf(name, sizeof(name), "ShiftedRating%d", i + 1);
        ssr_tweaks_set_number(game->tweaks, "Player", name, r > 5 ? r - 5 : 0);
        snprintf(name, sizeof(name), "NegativeShiftedRating%d", i + 1);
        ssr_tweaks_set_number(game->tweaks, "Player", name, r < 5 ? 5 - r : 0);
    }
    const char *type = ssr_career_player_type(c);
    // GetRatingIndex: the last type that the news named, or 0 or 1
    // [I: the type now, as the remake has no rating news yet].
    int index = ssr_career_player_type_index(type);
    if (index < 0) index = c->ratings[0] + c->ratings[1] > c->ratings[2] + c->ratings[3] ? 0 : 1;
    ssr_tweaks_set_number(game->tweaks, "Player", "PlayerType", index);
    ssr_tweaks_set(game->tweaks, "Player", "PlayerTypeString", loc(game, type));
}

// skills.btn_Ratings: the ratings screen over the screen that is open
// (c_TScreen_Ratings.SetUpScreen).
static void ratings_open(SSRGame *game) {
    snprintf(game->ratingsReturn, sizeof(game->ratingsReturn), "%s", game->screen);
    rating_tweaks(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "RatingsScroll", 0);
    set_screen(game, "ratings");
}

// ratings.btn_Back: the screen under it comes back.
static void ratings_back(SSRGame *game) {
    set_screen(game, game->ratingsReturn[0] ? game->ratingsReturn : "gamemenu");
}

// matchstats.dismissanim: a tap ends the result overlay.
static void stats_dismiss(SSRGame *game) {
    game->initDelay = 0;
    ssr_tweaks_set_number(game->tweaks, "Match", "Outcome", 0);
}

static void leaguefixtures_screen(SSRGame *game, int results);
static void season_end(SSRGame *game);
static void season_review(SSRGame *game);

// matchstats.btn_Play: the stages that did not come run now. Then the
// result counts in the table (c_TPlayer.MyMatchPlayed), the other teams
// play up to the day of the match (PlayToDate), and the results show.
static void match_recovery(SSRGame *game);

static void stats_continue(SSRGame *game) {
    if (game->statStage < 6) after_relations(game);
    if (game->statStage < 9) after_star_rating(game);
    SSRCareer *c = &game->career;
    season_in(game);
    SSRFixture *f = ssr_season_find(game->season, game->fixtureComp, game->fixtureDate, game->fixtureHome,
                                    game->fixtureAway);
    if (f && game->extraTime) {
        ssr_season_my_result_after_extra_time(game->season, f, game->facts.score1, game->facts.score2,
                                              game->pens[1], game->pens[2]);
    } else if (f) {
        ssr_season_my_result(game->season, f, game->facts.score1, game->facts.score2);
    }
    // MatchRecovery comes first in PlayToDate, with the rest days to the
    // next match.
    match_recovery(game);
    game->recovery = ssr_energy_recovery(game->season ? ssr_season_rest_days(game->season) : 14, ssr_career_age(c));
    game->payWage = c->wage;
    game->payGoals = game->facts.goals * c->goalBonus;
    game->payAssists = game->facts.assists * c->assistBonus;
    int end = game->season ? ssr_season_play_to(game->season, game->fixtureDate) : 0;
    season_out(game);
    game->reviewWaits = 1;
    save_career(game);
    if (end) season_end(game);
    else leaguefixtures_screen(game, 1);
}

// c_AScreen_MatchReview: the weekly update after the match.
static void match_review(SSRGame *game) {
    SSRCareer *c = &game->career;
    set_screen(game, "matchreview");
    SSRTweaks *t = game->tweaks;
    ssr_tweaks_set_number(t, "Menu", "HideNRGShopTab", 1);
    ssr_tweaks_set_number(t, "Menu", "HideBootsShopTab", 1);
    ssr_tweaks_set_number(t, "Menu", "OwnedTrainer", 0);
    ssr_tweaks_set_number(t, "Menu", "OwnedAgent", 0);
    // c_AScreen_MatchReview.ClearTweakValues.
    static const char *EARNINGS[13] = {"Wage",
                                       "AgentWage",
                                       "RewardedAgentWage",
                                       "BonusGoals",
                                       "AgentBonusGoals",
                                       "RewardedAgentBonusGoals",
                                       "BonusAssists",
                                       "AgentBonusAssists",
                                       "RewardedAgentBonusAssists",
                                       "Sponsors",
                                       "AgentSponsors",
                                       "RewardedAgentSponsors",
                                       "Total"};
    char name[64];
    for (int i = 0; i < 13; i++) {
        snprintf(name, sizeof(name), "Earnings%s", EARNINGS[i]);
        ssr_tweaks_set_number(t, "MatchReview", name, -1);
    }
    static const char *RECOVERY[5] = {"Recovery", "RecoveryBoostTrainer", "RecoveryRewardedBoostTrainer",
                                      "RecoveryBoostProperties", "RecoveryTotal"};
    for (int i = 0; i < 5; i++) ssr_tweaks_set_number(t, "Player", RECOVERY[i], -1);
    ssr_tweaks_set_number(t, "MatchStats", "StatStage", 0);
    ssr_tweaks_set_number(t, "MatchReview", "ShowAgentRewardAd", 0);
    ssr_tweaks_set_number(t, "MatchReview", "ShowTrainerRewardAd", 0);
    ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
    game->statStage = 0;
    game->statTime = game->now;
}

// matchreview.btn_Play: the game menu, from stage 14 only, so that the
// energy does not come two times.
static void review_continue(SSRGame *game) {
    if (game->statStage < 14) return;
    ssr_tweaks_set_number(game->tweaks, "Menu", "HideNRGShopTab", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HideBootsShopTab", 0);
    game->reviewWaits = 0;
    save_career(game);
    if (game->seasonEndMode) season_review(game);
    else game_menu(game);
}

/* ------------------------------------------------- fixtures and tables */

// c_TTeamPool.GetStringTeamPosition: "1st", or "" before the first match.
static void position_string(SSRGame *game, SSRPool *pool, int teamId, char *out, size_t size) {
    SSRSeason *s = game->season;
    int played = 0;
    for (int i = 0; i < pool->count; i++) played += s->rows[pool->rows[i]].played > 0;
    out[0] = 0;
    if (!played) return;
    int position = ssr_season_position(s, pool, teamId);
    if (position == 0) {
        snprintf(out, size, "-");
    } else if (position < 33) {
        char tag[32];
        snprintf(tag, sizeof(tag), "position_%d", position);
        snprintf(out, size, "%s", loc(game, tag));
    } else {
        snprintf(out, size, "%d", position);
    }
}

// The first leg of a second leg, or NULL.
static const SSRFixture *first_leg_of(const SSRSeasonComp *comp, const SSRFixture *f) {
    for (int i = 0; i < comp->fixtureCount; i++) {
        const SSRFixture *x = &comp->fixtures[i];
        if (x->leg == 1 && x->group == f->group && x->home == f->away && x->away == f->home) return x;
    }
    return NULL;
}

// c_TFixture.GetStringArrayForLeague: the positions, the names, and the
// middle text of a row.
static void fixture_strings(SSRGame *game, SSRSeasonComp *comp, const SSRFixture *f, char out[5][64]) {
    SSRSeason *s = game->season;
    for (int i = 0; i < 5; i++) out[i][0] = 0;
    if (f->group < 1 || f->group > comp->poolCount) return;
    SSRPool *pool = &comp->pools[f->group - 1];
    SSRTableRow *home = ssr_season_row(s, pool, f->home), *away = ssr_season_row(s, pool, f->away);
    int homeTeam = home ? home->teamId : 0, awayTeam = away ? away->teamId : 0;
    if (comp->data->type != 1) {
        if (home) position_string(game, pool, homeTeam, out[0], 64);
        if (away) position_string(game, pool, awayTeam, out[4], 64);
    }
    // The sort of the table moves the rows, so the names come after it.
    home = ssr_season_row(s, pool, f->home);
    away = ssr_season_row(s, pool, f->away);
    if (home) snprintf(out[1], 64, "%s", home->name);
    if (away) snprintf(out[3], 64, "%s", away->name);
    if (f->result == 0) {
        snprintf(out[2], 64, "%s", loc(game, !out[1][0] || !out[3][0] ? "fixture_Bye" : "sla_versus_RESKIN"));
        return;
    }
    const char *mark = f->resultType == SSR_RESULT_EXTRA_TIME ? loc(game, "sla_extratime")
                       : f->resultType == SSR_RESULT_PENALTIES ? loc(game, "sla_penalty")
                       : f->resultType == SSR_RESULT_AWAY_GOALS ? loc(game, "sla_awaygoals")
                                                                 : "";
    int homeWon = ssr_fixture_winner(s, f) == f->home;
    char score[32];
    snprintf(score, sizeof(score), "%d - %d", f->score1, f->score2);
    if (f->leg == 2) {
        const SSRFixture *one = first_leg_of(comp, f);
        int aggHome = f->score1 + (one ? one->score2 : 0), aggAway = f->score2 + (one ? one->score1 : 0);
        snprintf(score, sizeof(score), "(%d) %d - %d (%d)", aggHome, f->score1, f->score2, aggAway);
    }
    if (homeWon) snprintf(out[2], 64, "%s%s", mark, score);
    else snprintf(out[2], 64, "%s%s", score, mark);
}

// 1 when the team of the side is the team of the player (c_TPlayer.OnTeam2).
static int is_my_team(const SSRGame *game, const SSRSeasonComp *comp, int teamId) {
    return teamId != 0 && teamId == (comp->data->level == 1 ? game->career.nationId : game->career.clubId);
}

// c_TCompetition.AddFixture: one row of a fixture list.
static void add_fixture_row(SSRGame *game, const char *list, SSRSeasonComp *comp, const SSRFixture *f) {
    char texts[5][64];
    fixture_strings(game, comp, f, texts);
    int mine = is_my_team(game, comp, ssr_fixture_home_team(game->season, f)) ||
               is_my_team(game, comp, ssr_fixture_away_team(game->season, f));
    const char *template_ = mine ? (f->result == 0 ? "PlayerCupFixture" : "PlayerCupFixturePlayed")
                                 : (f->result == 0 ? "CupFixture" : "CupFixturePlayed");
    add_op(game, SSR_OP_LIST_ADD, list, template_, "", 0);
    for (int i = 0; i < 5; i++) add_op(game, SSR_OP_LIST_TEXT, list, NULL, texts[i], i + 1);
    ssr_tweaks_set_number(game->tweaks, "Menu", "ShowPosHeadings", texts[0][0] || texts[4][0]);
}

static int compare_rows_of_list(const void *a, const void *b) {
    const SSRFixture *x = *(const SSRFixture *const *)a, *y = *(const SSRFixture *const *)b;
    if (x->sdate != y->sdate) return x->sdate < y->sdate ? -1 : 1;
    if (x->level != y->level) return x->level > y->level ? -1 : 1;
    if (x->round != y->round) return x->round < y->round ? -1 : 1;
    if (x->group != y->group) return x->group < y->group ? -1 : 1;
    if (x->result != y->result) return x->result > y->result ? -1 : 1;
    return x < y ? -1 : x > y;
}

// c_TScreen_LeagueFixtures.SetUpFixturesTable: the fixtures of the round.
static void fixtures_table(SSRGame *game, const char *list, int results, SSRSeasonComp *comp) {
    SSRSeason *s = game->season;
    ssr_tweaks_set(game->tweaks, "Match", "LeagueName", comp->data->tla);
    ssr_tweaks_set_number(game->tweaks, "Menu", "LeagueType", ssr_season_comp_type(s, comp));
    int round = results ? ssr_season_prev_round(comp) : ssr_season_next_round(comp);
    if (comp->data->type == 1 && !ssr_season_all_populated(s, comp)) return;
    const SSRFixture **rows = malloc((size_t)(comp->fixtureCount + 1) * sizeof(*rows));
    if (!rows) return;
    int n = 0;
    for (int i = 0; i < comp->fixtureCount; i++) {
        if (comp->data->type == 1 || comp->fixtures[i].round == round) rows[n++] = &comp->fixtures[i];
    }
    qsort(rows, (size_t)n, sizeof(*rows), compare_rows_of_list);
    add_op(game, SSR_OP_LIST_START, list, NULL, NULL,
           (int)ssr_tweaks_number(game->tweaks, "Menu", "LeagueFixturesYOffset"));
    for (int i = 0; i < n; i++) add_fixture_row(game, list, comp, rows[i]);
    free(rows);
}

// c_TScreen_LeagueFixtures.SetUpScreen: the round before the match, or the
// results after it.
static void leaguefixtures_screen(SSRGame *game, int results) {
    SSRTweaks *t = game->tweaks;
    set_screen(game, "leaguefixtures");
    ssr_tweaks_set_number(t, "Menu", "HomeButtonDisabled", 1);
    ssr_tweaks_set_number(t, "Menu", "ShowPlayEnergy", !results);
    ssr_tweaks_set_number(t, "Menu", "LeagueFixturesScroll", 0);
    ssr_tweaks_set_number(t, "Menu", "LeagueFixturesResults", results);
    if (!results) add_op(game, SSR_OP_SHOW, "BackButton", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "FixtureList", NULL, NULL, 0);
    season_in(game);
    SSRFixture *f = results ? ssr_season_prev_fixture(game->season) : ssr_season_next_fixture(game->season);
    if (f) {
        if (!results) ssr_season_skip_to(game->season, f->sdate);
        season_out(game);
        // The skip can move the fixtures in their list.
        f = results ? ssr_season_prev_fixture(game->season) : ssr_season_next_fixture(game->season);
    }
    SSRSeasonComp *comp = f ? ssr_season_comp(game->season, f->compId) : NULL;
    if (comp) fixtures_table(game, "FixtureList", results, comp);
    char week[64];
    snprintf(week, sizeof(week), "%s %d", loc(game, "Week"), ssr_career_week(&game->career));
    add_op(game, SSR_OP_TEXT, "TableWeek", NULL, week, 0);
    if (results) game->leagueContinue = 1;
}

static void league_screen(SSRGame *game, int compId);
static void league_home(SSRGame *game);
static void league_fixtures_button(SSRGame *game);

// The end of a PlayToDate: the season review at the new year, else the
// results.
static void after_play_to(SSRGame *game, int end) {
    season_out(game);
    if (end) season_end(game);
    else leaguefixtures_screen(game, 1);
}

// c_TPlayer.GetForm and GetInternationalForm: the mean of the last five
// ratings, or 5 before five matches.
static float form_of(const int *form, int apps) {
    if (apps < 5) return 5.0f;
    return (float)(form[0] + form[1] + form[2] + form[3] + form[4]) / 5.0f;
}

// GSPlayerUpdateSelectionStatus: 1 first team; 2 to 4 a substitute (energy,
// boss, form); 5 and 6 not picked (energy, boss); 7 to 11 the same for the
// nation.
static int selection_status(SSRGame *game, int international) {
    const SSRCareer *c = &game->career;
    if (c->retired) return 1;
    if (!international) {
        if (c->relationBoss < 10) return 6;
        if (c->energy < 25) return 5;
        if (form_of(c->form, c->apps) < 3.5f ||
            (c->seasonApps > 4 && c->form[3] < 5 && c->form[4] < 5 && c->relationBoss < 50)) {
            return 4;
        }
        if (c->relationBoss < 40) return 3;
        if (c->energy < 50) return 2;
        return 1;
    }
    if (c->relationBossInt < 50) return 11;
    if (c->energy < 25) return 10;
    if (form_of(c->intForm, c->intApps) < 4.0f ||
        (c->intApps > 4 && c->intForm[3] < 5 && c->intForm[4] < 5 && c->relationBossInt < 85) ||
        (c->seasonApps > 4 && c->form[3] < 5 && c->form[4] < 5 && c->relationBossInt < 65)) {
        return 9;
    }
    if (c->relationBossInt < 65) return 8;
    if (c->energy < 50) return 7;
    return 1;
}

// The player is not picked: the message, then the other teams play the day
// of the match (PlayToDate) while the player rests.
static void not_picked(SSRGame *game, const char *text) {
    SSRCareer *c = &game->career;
    game->wasPicked = 0;
    season_in(game);
    SSRFixture *f = ssr_season_next_fixture(game->season);
    game->messageData = f ? f->sdate : c->sdate;
    // c_TPlayer.UpdateNotPickedRecovery. The game pays no wage for it
    // (award_not_picked_wage is "NO").
    float energy = c->energy + (float)ssr_energy_recovery(ssr_season_rest_days(game->season), ssr_career_age(c));
    c->energy = energy > 100 ? 100 : energy;
    const char *shown = text && text[0] == 'C' && strncmp(text, "CMESSAGE", 8) == 0 ? loc(game, text) : text;
    message(game, "messageboss", shown, 0, 1, "PlayToDate");
}

// GSMatchRecovery, the start of PlayToDate: the relationship activities
// start again, and each sponsor takes a little of the relation.
static void match_recovery(SSRGame *game) {
    SSRCareer *c = &game->career;
    c->spentTime = 0;
    int n = ssr_career_count_sponsors(c);
    if (n > 0) ssr_career_update_relation(c, SSR_RELATION_SPONSORS, (float)n * -0.65f);
}

// The call "PlayToDate": the days up to the date in the message.
static void call_play_to_date(SSRGame *game) {
    match_recovery(game);
    season_in(game);
    after_play_to(game, ssr_season_play_to(game->season, game->messageData));
}

// The call "SetUpFixture": the match of the next fixture.
static void call_set_up_fixture(SSRGame *game) {
    start_match(game);
}

// The call "DebutTired": the boss gave a can; the player presses Play again.
static void call_debut_tired(SSRGame *game) {
    (void)game;
}

// GSPlayerPlayMatches: the other teams play first when the round of the
// player is not the next round, or the cup has no draw yet.
static void play_matches(SSRGame *game) {
    season_in(game);
    SSRFixture *f = ssr_season_next_fixture(game->season);
    if (!f) return;
    SSRSeasonComp *comp = ssr_season_comp(game->season, f->compId);
    if (!comp) return;
    int round = ssr_season_next_round(comp);
    if (round < f->round) {
        after_play_to(game, ssr_season_play_to(game->season, ssr_season_round_date(comp, round)));
        return;
    }
    if (ssr_fixture_home_team(game->season, f) < 1 || ssr_fixture_away_team(game->season, f) < 1) {
        after_play_to(game, ssr_season_play_to(game->season, ssr_season_round_date(comp, round) - 1));
        return;
    }
    game->wasPicked = 1;
    game->subTime = 0;
    // The end of GSPlayerPlayMatches: the next week has news again.
    game->career.newsDone = 0;
    int status = selection_status(game, f->level == 1);
    SSRCareer *c = &game->career;
    int star = ssr_career_star_level(c);
    const SSRNation *nation = ssr_world_nation(game->world, c->nationId);
    char text[512];
    switch (status) {
    case 1:
        // The bribe of the game takes Rand(20) for a club match [I: no bribe
        // in the remake yet].
        if (f->level == 0) ssr_rand(&game->random, 20);
        start_match(game);
        return;
    case 2:
    case 5:
        if (c->debut) {
            c->energy = 100;
            message(game, "messageboss", loc(game, "CMESSAGE_DEBUTTIRED"), 0, 1, "DebutTired");
            return;
        }
        if (status == 2) {
            static const float BOSS_SUB[4] = {0, -3, -5, -7};
            change_relation(&c->relationBoss, BOSS_SUB[star < 4 ? star - 1 : 3]);
            game->subTime = ssr_rand2(&game->random, 46, 65);
            message(game, "messageboss", loc(game, "CMESSAGE_SUBENERGY"), 0, 1, "SetUpFixture");
            return;
        }
        static const float BOSS_OUT[4] = {-3, -6, -9, -12};
        change_relation(&c->relationBoss, BOSS_OUT[star < 4 ? star - 1 : 3]);
        not_picked(game, "CMESSAGE_NOTPICKEDENERGY");
        return;
    case 3:
    case 4:
        game->subTime = ssr_rand2(&game->random, 46, 65);
        message(game, "messageboss", loc(game, status == 3 ? "CMESSAGE_SUBRELATION" : "CMESSAGE_SUBFORM"), 0, 1,
                "SetUpFixture");
        return;
    case 6:
        not_picked(game, "CMESSAGE_NOTPICKEDRELATION_NEW");
        return;
    case 7:
    case 8:
    case 9: {
        if (status == 7) change_relation(&c->relationBossInt, -7.5f);
        game->subTime = ssr_rand2(&game->random, 46, 65);
        static const char *const SUBS[3] = {"CMESSAGE_SUBENERGYINT", "CMESSAGE_SUBRELATIONINT", "CMESSAGE_SUBFORMINT"};
        snprintf(text, sizeof(text), "%s", loc(game, SUBS[status - 7]));
        replace_all(text, sizeof(text), "$team", nation ? nation->tla : "");
        message(game, "messageboss", text, 0, 1, "SetUpFixture");
        return;
    }
    case 10:
        change_relation(&c->relationBossInt, -15);
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NOTPICKEDENERGYINT"));
        replace_all(text, sizeof(text), "$team", nation ? nation->tla : "");
        not_picked(game, text);
        return;
    default:
        // A star hears about it, at most once in 30 days.
        game->wasPicked = 0;
        if (star > 3 && c->sdate > c->intNotPickedMessage + 30) {
            c->intNotPickedMessage = c->sdate;
            snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NOTPICKEDRELATIONINT"));
            replace_all(text, sizeof(text), "$team", nation ? nation->tla : "");
            not_picked(game, text);
            return;
        }
        not_picked(game, "");
        return;
    }
}

// leaguefixtures.btn_Play: the match, or after the results the table of a
// league or the way home.
static void leaguefixtures_play(SSRGame *game) {
    if (ssr_tweaks_number(game->tweaks, "Menu", "LeagueFixturesResults") == 0) {
        ssr_season_keep_positions(game->season);
        play_matches(game);
        return;
    }
    // The news of a cup winner shows before the tables.
    if (game->career.headlineResult[0]) {
        webpage_to(game, "leaguefixtures", 0);
        return;
    }
    season_in(game);
    SSRFixture *f = ssr_season_prev_fixture(game->season);
    SSRSeasonComp *comp = f ? ssr_season_comp(game->season, f->compId) : NULL;
    if (comp && (comp->data->type == 0 || comp->data->type == 4)) league_screen(game, comp->data->id);
    else league_home(game);
}

// c_TScreen_League.SetUpLeagueTable: the table of the group of the player,
// or the ties of a cup.
static void league_table(SSRGame *game, int compId) {
    SSRSeason *s = game->season;
    if (compId == 0) compId = ssr_season_actual_league(s, game->career.clubId);
    SSRSeasonComp *comp = ssr_season_comp(s, compId);
    add_op(game, SSR_OP_LIST_CLEAR, "TeamList", NULL, NULL, 0);
    game->leagueComp = compId;
    game->leagueSelected = 0;
    if (!comp || comp->poolCount == 0) return;
    ssr_tweaks_set(game->tweaks, "Match", "LeagueName", comp->data->tla);
    ssr_tweaks_set_number(game->tweaks, "Menu", "LeagueType", ssr_season_comp_type(s, comp));
    if (comp->data->type == 1) {
        fixtures_table(game, "TeamList", 0, comp);
        ssr_tweaks_set_number(game->tweaks, "Menu", "LeagueType", 2);
        return;
    }
    int mine = comp->data->level == 1 ? game->career.nationId : game->career.clubId;
    int group = comp->poolCount > 1 ? ssr_season_group_of(s, comp, mine) : 1;
    SSRPool *pool = &comp->pools[group > 0 ? group - 1 : 0];
    ssr_season_sort_pool(s, pool, comp->data->type == 2 || comp->data->type == 3 ? 16 : 4);
    add_op(game, SSR_OP_LIST_START, "TeamList", NULL, NULL, 52);
    for (int i = 0; i < pool->count; i++) {
        const SSRTableRow *row = &s->rows[pool->rows[i]];
        int position = i + 1, player = row->teamId == mine;
        add_op(game, SSR_OP_LIST_ADD, "TeamList", player ? "PlayerTeamEntry" : "TeamEntry", "", 0);
        char texts[10][32];
        snprintf(texts[0], 32, "%d", position);
        snprintf(texts[1], 32, "%s", row->name);
        int values[8] = {row->played, row->won,          row->drawn, row->lost, row->goalsFor, row->goalsAgainst,
                         row->goalsFor - row->goalsAgainst, row->points};
        for (int k = 0; k < 8; k++) snprintf(texts[k + 2], 32, "%d", values[k]);
        for (int k = 0; k < 10; k++) add_op(game, SSR_OP_LIST_TEXT, "TeamList", NULL, texts[k], k + 1);
        // One colour band at most.
        if (position == 1) add_op(game, SSR_OP_LIST_SUB, "TeamList", "Top", NULL, 1);
        else if (ssr_season_is_promotion_place(s, comp, position))
            add_op(game, SSR_OP_LIST_SUB, "TeamList", "Promotion", NULL, 1);
        else if (ssr_season_is_relegation_place(s, comp, position))
            add_op(game, SSR_OP_LIST_SUB, "TeamList", "Relegation", NULL, 1);
        if (player) {
            add_op(game, SSR_OP_LIST_SUB, "TeamList", "Highlight", NULL, 1);
            game->leagueSelected = position;
        }
        if (row->prevPosition >= 0) {
            const char *arrow = position < row->prevPosition ? "PosUp" : position > row->prevPosition ? "PosDown"
                                                                                                    : "PosSame";
            add_op(game, SSR_OP_LIST_SUB, "TeamList", arrow, NULL, 1);
        }
        add_op(game, SSR_OP_LIST_PARAM, "TeamList", NULL, NULL, position);
    }
}

// c_TScreen_League.ResetCombo: the competitions that the player can see.
static void league_list(SSRGame *game) {
    SSRSeason *s = game->season;
    add_op(game, SSR_OP_LIST_CLEAR, "SeeLeaguesCombo", NULL, NULL, 0);
    int year = ssr_career_year(&game->career);
    int actual = ssr_season_actual_league(s, game->career.clubId);
    const SSRNation *mine = ssr_world_nation(game->world, game->career.nationId);
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[s->compOrder[i]];
        const SSRCompetition *d = c->data;
        if (c->fixtureCount == 0) continue;
        const char *icon;
        if (d->level == 1) {
            int inYear = 0;
            for (int k = 0; k < c->fixtureCount; k++) inYear |= ssr_date_year(c->fixtures[k].sdate) == year;
            if (!(d->locale == 2 || (mine && d->based == mine->continent)) || !inYear) continue;
            icon = "IntCupIcon";
        } else if (d->level == 0) {
            icon = d->locale == 1 ? "IntLeagueIcon" : d->type == 1 ? "CupIcon" : "LeagueIcon";
        } else {
            continue;
        }
        int populated = ssr_season_all_populated(s, c);
        int listed = d->type == 1 ? populated && ssr_season_next_round(c) != 0
                     : d->type == 0 ? populated && c->startYear <= year + 1
                     : d->type == 4 ? populated
                                    : 0;
        if (!listed) continue;
        add_op(game, SSR_OP_LIST_ADD, "SeeLeaguesCombo", "LeagueTemplate", d->tla, d->id);
        add_op(game, SSR_OP_LIST_SUB, "SeeLeaguesCombo", icon, NULL, 1);
        if (d->id == actual) add_op(game, SSR_OP_LIST_SUB, "SeeLeaguesCombo", "CurrentOutline", NULL, 1);
        add_op(game, SSR_OP_LIST_PARAM, "SeeLeaguesCombo", NULL, NULL, d->id);
    }
}

// c_TScreen_League.SetUpScreen.
static void league_screen(SSRGame *game, int compId) {
    set_screen(game, "league");
    int blocked = game->leagueContinue || game->seasonEndMode;
    ssr_tweaks_set_number(game->tweaks, "Menu", "ShowLeaguePlay", blocked);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", blocked);
    if (game->seasonEndMode) {
        add_op(game, SSR_OP_SHOW, "BackButton", NULL, NULL, 0);
        add_op(game, SSR_OP_SHOW, "HelpBackgroundNoFixtures", NULL, NULL, 0);
        add_op(game, SSR_OP_HIDE, "HelpBackgroundFixtures", NULL, NULL, 0);
        add_op(game, SSR_OP_HIDE, "FixturesButton", NULL, NULL, 0);
    }
    ssr_tweaks_set_number(game->tweaks, "Menu", "LeagueFixturesScroll", 0);
    season_in(game);
    league_list(game);
    league_table(game, compId);
}

// The fixtures of a team in the competitions of its level, in the order of
// a fixture list (c_TClub.GetFixtureList and c_TNation.GetFixtureList).
static int team_fixtures(SSRGame *game, int teamId, int level, const SSRFixture **out, int max) {
    SSRSeason *s = game->season;
    int n = 0;
    const SSRClub *club = level == 0 ? ssr_world_club(game->world, teamId) : NULL;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *c = &s->comps[i];
        const SSRCompetition *d = c->data;
        if (d->level != level) continue;
        if (level == 0 && !((d->locale == 0 && club && d->based == club->nation) || d->locale == 1)) continue;
        int group = ssr_season_group_of(s, c, teamId);
        if (group == 0) continue;
        SSRPool *pool = &c->pools[group - 1];
        int id = 0;
        for (int r = 0; r < pool->count; r++) {
            if (s->rows[pool->rows[r]].teamId == teamId) id = s->rows[pool->rows[r]].id;
        }
        for (int k = 0; k < c->fixtureCount && n < max; k++) {
            const SSRFixture *f = &c->fixtures[k];
            if (f->group == group && (f->home == id || f->away == id)) out[n++] = f;
        }
    }
    qsort(out, (size_t)n, sizeof(*out), compare_rows_of_list);
    return n;
}

// c_TFixture.GetStringArrayForTeamId: the week, the competition, the other
// team, the result, and home or away.
static void team_fixture_strings(SSRGame *game, const SSRFixture *f, int teamId, char out[5][64]) {
    SSRSeason *s = game->season;
    for (int i = 0; i < 5; i++) out[i][0] = 0;
    int year = ssr_date_year(f->sdate);
    if (year != ssr_career_year(&game->career)) {
        snprintf(out[0], 64, "%.1s%d", loc(game, "Year"), year);
    } else {
        snprintf(out[0], 64, "%d", ssr_date_week(f->sdate));
    }
    SSRSeasonComp *comp = ssr_season_comp(s, f->compId);
    if (!comp) return;
    snprintf(out[1], 64, "%s", comp->data->tla);
    snprintf(out[2], 64, "-");
    SSRPool *pool = f->group >= 1 && f->group <= comp->poolCount ? &comp->pools[f->group - 1] : NULL;
    SSRTableRow *home = ssr_season_row(s, pool, f->home), *away = ssr_season_row(s, pool, f->away);
    int atHome = home && home->teamId == teamId;
    SSRTableRow *other = atHome ? away : home;
    if (other && other->teamId != teamId) snprintf(out[2], 64, "%s", other->name);
    if (comp->data->type == 1 && !ssr_season_all_populated(s, comp)) snprintf(out[2], 64, "-");
    snprintf(out[4], 64, "%s", loc(game, atHome ? "sla_Home" : "sla_Away"));
    if (f->result == 0) {
        if (f->matchType == SSR_FIXTURE_EXTRA_TIME && f->round == 2) snprintf(out[3], 64, "%s", loc(game, "Replay"));
        else if (f->leg > 0) snprintf(out[3], 64, "%s %d", loc(game, "Leg"), f->leg);
        return;
    }
    if (strcmp(out[2], "-") == 0) {
        snprintf(out[3], 64, "%s", loc(game, "fixture_Bye"));
        return;
    }
    char texts[5][64];
    fixture_strings(game, comp, f, texts);
    snprintf(out[3], 64, "%s", texts[2]);
}

// c_TScreen_ClubFixtures.SetUpScreen: the fixtures and results of a team.
static void club_fixtures(SSRGame *game, int teamId, int level) {
    set_screen(game, "clubfixtures");
    game->fixturesTeam = teamId;
    game->fixturesLevel = level;
    Team team;
    if (team_of(game, level, teamId, 0, &team)) {
        set_colour_tweak(game, "Badge1Col1", team.badge1, 1);
        set_colour_tweak(game, "Badge1Col2", team.badge2, 1);
        ssr_tweaks_set_number(game->tweaks, "MatchColours", "BadgeStyle1", team.badgeStyle);
        add_op(game, SSR_OP_TEXT, "FixtureTeamName", NULL, team.tla, 0);
    }
    add_op(game, SSR_OP_LIST_CLEAR, "FixtureList", NULL, NULL, 0);
    const SSRFixture *list[256];
    int n = team_fixtures(game, teamId, level, list, 256);
    int year = ssr_career_year(&game->career), rows = 0, lastPlayed = -1;
    for (int i = 0; i < n; i++) {
        const SSRFixture *f = list[i];
        if (ssr_date_year(f->sdate) > year + 1) continue;
        char texts[5][64];
        team_fixture_strings(game, f, teamId, texts);
        SSRSeasonComp *comp = ssr_season_comp(game->season, f->compId);
        int result = 0;
        if (f->result != 0 && comp) {
            int winner = ssr_fixture_winner(game->season, f);
            SSRPool *pool = &comp->pools[f->group - 1];
            SSRTableRow *row = ssr_season_row(game->season, pool, winner);
            result = winner == 0 ? 2 : row && row->teamId == teamId ? 1 : 3;
            lastPlayed = rows;
        }
        static const char *const TEMPLATES[4] = {"PendingFixture", "WonFixture", "DrawnFixture", "LostFixture"};
        add_op(game, SSR_OP_LIST_ADD, "FixtureList", TEMPLATES[result], "", 0);
        for (int k = 0; k < 5; k++) add_op(game, SSR_OP_LIST_TEXT, "FixtureList", NULL, texts[k], k + 1);
        rows++;
    }
    int start = lastPlayed - 5 > 0 ? lastPlayed - 5 : 0;
    ssr_tweaks_set_number(game->tweaks, "Menu", "LeagueFixturesScroll", -(float)(start * 52));
    if (lastPlayed < rows - 1) {
        char entry[64];
        snprintf(entry, sizeof(entry), "FixtureList@%d", lastPlayed + 1);
        add_op(game, SSR_OP_LIST_SUB, entry, "Highlight", NULL, 1);
    }
}

// league.row_team: a tap chooses a row; a second tap opens its fixtures.
static void league_row(SSRGame *game) {
    int row = game->messageParam;
    if (row == game->leagueSelected) {
        league_fixtures_button(game);
        return;
    }
    char entry[64];
    if (game->leagueSelected > 0) {
        snprintf(entry, sizeof(entry), "TeamList@%d", game->leagueSelected - 1);
        add_op(game, SSR_OP_LIST_SUB, entry, "Highlight", NULL, 0);
    }
    snprintf(entry, sizeof(entry), "TeamList@%d", row - 1);
    add_op(game, SSR_OP_LIST_SUB, entry, "Highlight", NULL, 1);
    game->leagueSelected = row;
}

// league.btn_Fixtures: the fixtures of the chosen team of the table, or of
// the club of the player.
static void league_fixtures_button(SSRGame *game) {
    SSRSeasonComp *comp = ssr_season_comp(game->season, game->leagueComp);
    int team = game->career.clubId, level = 0;
    if (comp && comp->data->type != 1 && comp->poolCount > 0) {
        int mine = comp->data->level == 1 ? game->career.nationId : game->career.clubId;
        int group = comp->poolCount > 1 ? ssr_season_group_of(game->season, comp, mine) : 1;
        SSRPool *pool = &comp->pools[group > 0 ? group - 1 : 0];
        int row = game->leagueSelected > 0 ? game->leagueSelected : 1;
        if (row <= pool->count) {
            team = game->season->rows[pool->rows[row - 1]].teamId;
            level = comp->data->level;
        }
    }
    club_fixtures(game, team, level);
}

// league.btn_League on the club fixtures: the table again.
static void fixtures_league(SSRGame *game) {
    league_screen(game, game->leagueComp);
}

static void game_league(SSRGame *game) {
    league_screen(game, 0);
}

// league.cmb_League: the table of the competition of the row.
static void league_combo(SSRGame *game) {
    league_table(game, game->messageParam);
}

// c_TScreen_League.ButtonHome: the end of the season when no match is
// left, else the game menu (with the match review first after a match).
static void league_home(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Combo", "SeeLeagues", 0);
    game->leagueContinue = 0;
    if (game->seasonEndMode) {
        ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 0);
        set_screen(game, "seasonreview");
        return;
    }
    season_in(game);
    if (!game->career.retired && !ssr_season_next_fixture(game->season)) {
        int end = ssr_season_play_to(game->season, ssr_date_make(1, 1, ssr_career_year(&game->career) + 1));
        season_out(game);
        if (end || !ssr_season_next_fixture(game->season)) {
            season_end(game);
            return;
        }
    }
    if (!game->wasPicked) save_career(game);
    if (game->reviewWaits) match_review(game);
    else game_menu(game);
}

// GSPlayerPlay: with no match left this year the table shows, else the
// fixtures of the next round.
static int play_news(SSRGame *game, const SSRFixture *next);

static void game_play(SSRGame *game) {
    season_in(game);
    SSRFixture *f = ssr_season_next_fixture(game->season);
    if (!f || ssr_date_year(f->sdate) > ssr_career_year(&game->career)) {
        league_screen(game, 0);
        return;
    }
    if (!game->career.newsDone && play_news(game, f)) {
        save_career(game);
        return;
    }
    leaguefixtures_screen(game, 0);
}

// c_TCompetition.IsTopDivision: 0 when the first place goes up to a
// league of the nation.
static int is_top_division(SSRGame *game, const SSRSeasonComp *comp) {
    for (int i = 0; comp && i < comp->placeCount; i++) {
        const SSRPromotion *p = &game->world->promotions[comp->places[i]];
        SSRSeasonComp *t = ssr_season_comp(game->season, p->promotionTo);
        if (p->place == 1 && t && t->data->locale == 0 && t->data->level == 0 && t->data->type == 0) return 0;
    }
    return 1;
}

// c_TScreen_SeasonReview.SetUpScreen: the place in the league and the
// counts of the season. The match review comes first after a match.
static void season_review(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    SSRCareer *c = &game->career;
    game->seasonEndMode = 1;
    if (game->reviewWaits) {
        match_review(game);
        return;
    }
    ssr_tweaks_set_number(t, "Menu", "DisableQuitButton", 1);
    set_screen(game, "seasonreview");
    season_in(game);
    SSRSeason *s = game->season;
    SSRSeasonComp *comp = ssr_season_comp(s, ssr_season_actual_league(s, c->clubId));
    SSRSeasonClub *club = ssr_season_club(s, c->clubId, 0);
    if (!comp || !club) return;
    int position = ssr_season_position(s, &comp->pools[0], c->clubId);
    char text[128], place[32];
    position_string(game, &comp->pools[0], c->clubId, place, sizeof(place));
    if (club->league < comp->data->id && comp->data->type != 4) {
        snprintf(text, sizeof(text), "%s", loc(game, "position_Promoted"));
    } else if (club->league > comp->data->id && comp->data->type != 4) {
        snprintf(text, sizeof(text), "%s", loc(game, "position_Relegated"));
    } else {
        snprintf(text, sizeof(text), "%s %s", loc(game, "position_Finished"), place);
    }
    // TQuickMessage "splashmessage" for 1500 ms.
    for (char *p = text; *p; p++) *p = (char)toupper((unsigned char)*p);
    ssr_tweaks_set(t, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 1500;
    // c_AScreen_SeasonReview.Init.
    ssr_tweaks_set(t, "PlayerStats", "SeasonClub", club->data->tla);
    ssr_tweaks_set(t, "PlayerStats", "SeasonLeague", comp->data->tla);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonPosition", position);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonWage", c->wage);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonMatches", c->seasonApps);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonGoals", c->seasonGoals);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonPasses", c->seasonPasses);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonHatTricks", c->seasonHatTricks);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonAssists", c->seasonAssists);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonStarMan", c->seasonStarMan);
    int apps = c->seasonApps;
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonAverageGoals", apps ? c->seasonGoals / apps : 0);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonAveragePasses", apps ? c->seasonPasses / apps : 0);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonAverageAssists", apps ? c->seasonAssists / apps : 0);
    ssr_tweaks_set_number(t, "PlayerStats", "SeasonAverageRating", apps ? c->seasonTotalRating / (float)apps : 0);
}

// The end of the season: PlayToDate reached the new year.
static void season_end(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 0);
    season_review(game);
}

static void season_review_league(SSRGame *game) {
    league_screen(game, 0);
}

// c_TPlayer.AwardPOTY: the awards of the season need 20 matches, 15 star
// man awards, and an average rating of 7.
static int award_poty(SSRGame *game, int type) {
    SSRCareer *c = &game->career;
    int apps = c->seasonApps;
    if (apps < 20 || c->seasonStarMan < 15) return 0;
    float average = c->seasonTotalRating / (float)apps;
    if (average < 7.0f) return 0;
    SSRSeason *s = game->season;
    SSRSeasonComp *comp = ssr_season_comp(s, ssr_season_actual_league(s, c->clubId));
    int position = comp ? ssr_season_position(s, &comp->pools[0], c->clubId) : 99;
    if (comp && comp->data->type == 4) position = 99;
    int top = is_top_division(game, comp), won = 0;
    switch (type) {
    case 1:
        won = c->relationFans > 80 && (average > 7.0f || c->seasonGoals + c->seasonAssists > 80);
        break;
    case 2:
        won = ssr_career_age(c) < 24 && position < 9 && ((top && average >= 8.0f) || average > 7.0f);
        break;
    case 3:
        won = position < 4 && ((top && average >= 8.5f) || (!top && average >= 7.5f));
        break;
    case 4:
        won = c->wonMajorAward && average >= 9.0f;
        break;
    }
    if (won) c->potyAwards++;
    return won;
}

static const char *const POTY_NAMES[4] = {"TeamPlayerOfYear", "LeagueYoungPlayerOfYear", "LeaguePlayerOfYear",
                                          "WorldPlayerOfYear"};

// The row of an award shows; a player over 23 has the rows "Old".
static void show_award_row(SSRGame *game, int award) {
    char ref[64];
    int young = ssr_career_age(&game->career) <= 23;
    snprintf(ref, sizeof(ref), "%s%s", POTY_NAMES[award], young ? "" : "Old");
    add_op(game, SSR_OP_SHOW, ref, NULL, NULL, 0);
}

// seasonreview.btn_Play: c_TScreen_PlayerOfTheYear.SetUpScreen.
static void player_of_the_year(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    set_screen(game, "playeroftheyear");
    game->potyStage = 0;
    game->potyTime = game->now;
    game->potyCount = 0;
    season_in(game);
    SSRSeasonComp *comp = ssr_season_comp(game->season, ssr_season_actual_league(game->season, game->career.clubId));
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    ssr_tweaks_set(t, "SeasonAwards", "LeagueName", comp ? comp->data->tla : "");
    ssr_tweaks_set(t, "SeasonAwards", "TeamName", club ? club->tla : "");
    for (int i = 0; i < 4; i++) ssr_tweaks_set_number(t, "SeasonAwards", POTY_NAMES[i], 0);
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    add_op(game, SSR_OP_HIDE, "RetireButton", NULL, NULL, 0);
}

// c_TScreen_PlayerOfTheYear.Update: one award each stage.
static void poty_tick(SSRGame *game) {
    int step = game->touchWasDown ? 250 : 1750;
    if (game->potyStage >= 20 || game->now <= game->potyTime + step) return;
    game->potyTime = game->now;
    int stage = ++game->potyStage;
    int young = ssr_career_age(&game->career) <= 23;
    if (stage == 1 || stage == 3 || stage == 5 || stage == 7) {
        show_award_row(game, (stage - 1) / 2);
    } else if (stage == 2 || stage == 4 || stage == 6 || stage == 8) {
        int award = stage / 2 - 1;
        int won = award_poty(game, award + 1);
        ssr_tweaks_set_number(game->tweaks, "SeasonAwards", POTY_NAMES[award], won);
        game->potyCount += won;
        // A player over 23 has no young award.
        if (stage == 2 && !young) game->potyStage += 2;
    } else if (stage == 9) {
        add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
        add_op(game, SSR_OP_SHOW, "RetireButton", NULL, NULL, 0);
    }
}

// c_TPlayer.NewSeason and the calls NewSeasonSetUp1 to 4.
static void new_season(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    game->seasonEndMode = 0;
    season_in(game);
    ssr_season_new_season(game->season);
    ssr_tweaks_set_number(t, "Menu", "DisableQuitButton", 0);
    skip_to_first_match(game);
    ssr_tweaks_set_number(t, "Menu", "CanSavePlayer", 1);
    ssr_career_reset_season(&game->career);
    save_career(game);
    player_tweaks(game);
    star_transition(game, "gamemenu");
}

// playeroftheyear.btn_Play: GSSeasonEndTransfer asks about a transfer.
static void poty_play(SSRGame *game) {
    message(game, "message", loc(game, "CMESSAGE_SEASONENDTRANSFER"), 1, 1, "SeasonEndTransfer");
}

// The answer: No keeps the club [I: the transfer screens come with the
// life of the player; until then Yes also keeps the club].
static void call_season_end_transfer(SSRGame *game) {
    new_season(game);
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

// One frame of the replay (RecordReplayFrame): the ticks of the aim drag,
// the tap ball, the flight, and the end until the offer.
static void record_replay(SSRGame *game) {
    const SSRChance *c = game->chance;
    if (game->replayMode != REPLAY_NONE) return;
    if (c->state == SSR_CHANCE_AIM && c->kicking != 1) return;
    if (game->replayCount == game->replayCapacity) {
        if (game->replayCapacity >= REPLAY_MAX) return;
        int capacity = game->replayCapacity ? game->replayCapacity * 2 : 256;
        if (capacity > REPLAY_MAX) capacity = REPLAY_MAX;
        SSRChance *more = realloc(game->replay, (size_t)capacity * sizeof(SSRChance));
        if (!more) return;
        game->replay = more;
        game->replayCapacity = capacity;
    }
    game->replay[game->replayCount++] = *c;
}

// The end of a chance: the match goes on, and the commentary comes back.
static void end_chance(SSRGame *game) {
    if (game->replayMode != REPLAY_NONE) {
        ssr_tweaks_set_number(game->tweaks, "WatchReplay", "Active", 0);
        game->replayMode = REPLAY_NONE;
    }
    game->replayCount = 0;
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

// c_MatchChance.m_ShowWatchReplay: the offer after a goal or an assist of
// the player, when the option is on. The first offer waits; later offers go
// on by themselves after SplashMessage.DelayWatchReplay seconds.
static int offer_replay(SSRGame *game) {
    SSRChanceResult r = game->chance->result;
    if (r != SSR_RESULT_GOAL && r != SSR_RESULT_ASSIST) return 0;
    if (!game->settings.replays || game->replayCount == 0) return 0;
    SSRTweaks *t = game->tweaks;
    if (ssr_tweaks_number(t, "Menu", "ReplaysEnabled") != 1) return 0;
    game->replayMode = REPLAY_OFFER;
    if (ssr_tweaks_number(t, "Player", "ReplayActive") == 0) {
        ssr_tweaks_set_number(t, "Player", "ReplayActive", 1);
        game->offerEnd = 0;
    } else {
        game->offerEnd = game->now + (int)(ssr_tweaks_number(t, "SplashMessage", "DelayWatchReplay") * 1000);
    }
    add_op(game, SSR_OP_SHOW, "ReplayHud", NULL, "", 0);
    add_op(game, SSR_OP_SHOW, "WatchReplay", NULL, "", 0);
    add_op(game, SSR_OP_HIDE, "ReplayOptions", NULL, "", 0);
    return 1;
}

// matchchance.btn_WatchReplay: the playback (c_TMatch.m_StartReplay).
static void watch_replay(SSRGame *game) {
    if (game->replayMode != REPLAY_OFFER) return;
    SSRTweaks *t = game->tweaks;
    game->replayMode = REPLAY_PLAY;
    add_op(game, SSR_OP_HIDE, "WatchReplay", NULL, "", 0);
    add_op(game, SSR_OP_SHOW, "ReplayOptions", NULL, "", 0);
    ssr_tweaks_set_number(t, "WatchReplay", "Frame", 0);
    ssr_tweaks_set_number(t, "WatchReplay", "Speed", 1);
    ssr_tweaks_set_number(t, "WatchReplay", "Length", game->replayCount - 1);
    ssr_tweaks_set_number(t, "WatchReplay", "Loop", 1);
    ssr_tweaks_set_number(t, "WatchReplay", "Active", 1);
    ssr_tweaks_set_number(t, "WatchReplay", "Paused", 0);
    // The first replay waits behind the help of the controls.
    if (ssr_tweaks_number(t, "Player", "ReplayActive") < 3) ssr_tweaks_set_number(t, "WatchReplay", "Speed", 0);
}

// matchchance.btn_SkipReplay and watchreplay.exit.
static void skip_replay(SSRGame *game) {
    if (game->replayMode == REPLAY_NONE || !game->chance) return;
    add_op(game, SSR_OP_HIDE, "ReplayHud", NULL, "", 0);
    end_chance(game);
}

// One tick of the playback (c_TMatch.m_UpdateReplay): the frame goes on by
// the speed, and wraps when the loop is on.
static void replay_tick(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    float frame = (float)ssr_tweaks_number(t, "WatchReplay", "Frame");
    float length = (float)ssr_tweaks_number(t, "WatchReplay", "Length");
    int loop = ssr_tweaks_number(t, "WatchReplay", "Loop") == 1;
    if (ssr_tweaks_number(t, "WatchReplay", "Paused") == 0) frame += (float)ssr_tweaks_number(t, "WatchReplay", "Speed");
    if (frame < 0) {
        if (loop) {
            frame = length;
        } else {
            frame = 0;
            ssr_tweaks_set_number(t, "WatchReplay", "Speed", 0);
        }
    } else if (frame > length) {
        if (loop) {
            frame = 0;
        } else {
            frame = length;
            ssr_tweaks_set_number(t, "WatchReplay", "Speed", 0);
        }
    }
    frame = frame < 0 ? 0 : frame > length ? length : frame;
    ssr_tweaks_set_number(t, "WatchReplay", "Frame", frame);
}

static void start_chance(SSRGame *game) {
    free(game->chance);
    game->chance = calloc(1, sizeof(SSRChance));
    if (!game->chance) return;
    const SSRCareer *c = &game->career;
    SSRChanceSetup s = ssr_match_chance_setup(game->match, ssr_career_skill(c->technique), ssr_career_skill(c->power), 1);
    s.pace = ssr_career_skill(c->pace);
    s.careerGoals = (float)c->goals;
    s.careerPasses = (float)c->passes;
    ssr_chance_start(game->chance, &s, &game->random);
    game->chanceEnd = 0;
    // InitChance: a new recording, and no replay.
    game->replayCount = 0;
    game->replayMode = REPLAY_NONE;
    ssr_tweaks_set_number(game->tweaks, "WatchReplay", "Speed", 1);
    ssr_tweaks_set_number(game->tweaks, "WatchReplay", "Active", 0);
    set_screen(game, "matchminigame");
}

static void match_tick(SSRGame *game) {
    if (game->chance) {
        if (game->replayMode == REPLAY_PLAY) {
            replay_tick(game);
            return;
        }
        if (game->replayMode == REPLAY_OFFER) {
            if (game->offerEnd && game->now >= game->offerEnd) skip_replay(game);
            return;
        }
        SSRTapTouch touch = game->touch;
        touch.hit = touch.down && !game->touchWasDown;
        game->touchWasDown = touch.down;
        ssr_chance_update(game->chance, game->now, &touch, &game->random);
        record_replay(game);
        if (game->chance->state == SSR_CHANCE_OVER) {
            if (game->chanceEnd == 0) show_banner(game);
            if (game->now >= game->chanceEnd && !offer_replay(game)) end_chance(game);
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


/* ---------------------------------------------------- the relationships */

// The relations of the five bars of the screen.
static const SSRRelation BAR_RELATIONS[5] = {SSR_RELATION_BOSS, SSR_RELATION_TEAM, SSR_RELATION_FANS,
                                             SSR_RELATION_PARTNER, SSR_RELATION_SPONSORS};
static const char *const BAR_BUTTONS[5] = {"BossButton", "TeamButton", "FansButton", "GirlfriendButton",
                                           "SponsorsButton"};
// bb_.g_fuseparam_maxrelationshipsperday.
#define MAX_RELATION_ACTIVITIES 3
#define ACTIVITY_ENERGY 35.0f

// GetPartnerMessageScreen.
static const char *partner_screen(const SSRGame *game) {
    return game->career.gender == 0 ? "messagegirlfriend" : "messageboyfriend";
}

// A message box with the bar of its relation (SetMessageScreen with one
// bar).
static void message_bar(SSRGame *game, const char *layout, const char *text, const char *call) {
    message(game, layout, text, 0, 1, call);
    add_op(game, SSR_OP_SHOW, "OneBar", NULL, NULL, 0);
}

// UpdatePlayerRelationships: the bars of the relations. With instant, the
// bars show the values at once.
static void relation_tweaks(SSRGame *game, int instant) {
    static const char *NAMES[8] = {"BossRelationship",       "TeamRelationship",    "FansRelationship",
                                   "GirlfriendRelationship", "SponsorsRelationship", "IntBossRelationship",
                                   "IntTeamRelationship",    "IntFansRelationship"};
    static const SSRRelation OF[8] = {SSR_RELATION_BOSS,     SSR_RELATION_TEAM,     SSR_RELATION_FANS,
                                      SSR_RELATION_PARTNER,  SSR_RELATION_SPONSORS, SSR_RELATION_BOSS_INT,
                                      SSR_RELATION_TEAM_INT, SSR_RELATION_FANS_INT};
    const SSRCareer *c = &game->career;
    char name[64];
    for (int i = 0; i < 8; i++) {
        float v = ssr_career_relation(c, OF[i]);
        ssr_tweaks_set_number(game->tweaks, "Player", NAMES[i], v);
        if (!instant) continue;
        snprintf(name, sizeof(name), "%s_Displayed", NAMES[i]);
        ssr_tweaks_set_number(game->tweaks, "Player", name, v);
    }
    ssr_tweaks_set_number(game->tweaks, "Player", "SponsorCount", ssr_career_count_sponsors(c));
    for (int i = 0; i < 10; i++) {
        snprintf(name, sizeof(name), "SponsorIncome%d", i + 1);
        ssr_tweaks_set_number(game->tweaks, "Player", name, c->sponsors[i]);
    }
}

// c_AScreen_Relationships.DisableButton and EnableButton. A disabled
// button sends no click.
static void relation_button(SSRGame *game, int bar, int on, int disable) {
    add_op(game, SSR_OP_COLOUR, BAR_BUTTONS[bar], NULL, on ? "FFFFFFFF" : "FFFFFF80", 0);
    game->relationOff[bar] = on ? 0 : disable ? 1 : game->relationOff[bar];
}

// c_AScreen_Relationships.UpdateButtons, in its order.
static void relation_buttons(SSRGame *game) {
    const SSRCareer *c = &game->career;
    for (int i = 0; i < 3; i++) relation_button(game, i, !game->showInternational, 1);
    ssr_tweaks_set_number(game->tweaks, "Menu", "InternationalBoss", game->showInternational);
    if (c->spentTime >= MAX_RELATION_ACTIVITIES) {
        for (int i = 0; i < 5; i++) relation_button(game, i, 0, 0);
    }
    if (c->relationPartner == 0) {
        relation_button(game, 3, 0, 0);
        add_op(game, SSR_OP_HIDE, "DumpGirlfriendButton", NULL, NULL, 0);
    } else {
        relation_button(game, 3, 1, 0);
    }
    relation_button(game, 4, ssr_career_count_sponsors(c) > 0, 0);
    for (int i = 0; i < 5; i++) {
        if (ssr_career_relation(c, BAR_RELATIONS[i]) >= 100) relation_button(game, i, 0, 1);
    }
    if (c->retired) {
        for (int i = 0; i < 5; i++) relation_button(game, i, 0, 0);
    }
}

// c_TScreen_Relationships.UpdateRelationships.
static void update_relationships(SSRGame *game, int instant, int international) {
    const SSRCareer *c = &game->career;
    game->showInternational = international;
    relation_buttons(game);
    static const char *TITLES[5] = {"Boss", "Team", "Fans", "Girlfriend", "Sponsors"};
    static const char *TAGS[5] = {"BOSS_CAPS_SPACEAFTER", "TEAM_CAPS_SPACEAFTER", "FANS_CAPS_SPACEAFTER",
                                  "GIRLFRIEND_CAPS_SPACEAFTER", "SPONSORS_CAPS_SPACEAFTER"};
    static const SSRRelation INT[3] = {SSR_RELATION_BOSS_INT, SSR_RELATION_TEAM_INT, SSR_RELATION_FANS_INT};
    const SSRNation *nation = ssr_world_nation(game->world, c->nationId);
    const char *tla = nation && nation->tla ? nation->tla : "";
    char name[64], text[128];
    for (int i = 0; i < 5; i++) {
        snprintf(name, sizeof(name), "%sRelationship_Title", TITLES[i]);
        snprintf(text, sizeof(text), "%s %d", loc(game, TAGS[i]), (int)ssr_career_relation(c, BAR_RELATIONS[i]));
        ssr_tweaks_set(game->tweaks, "Player", name, text);
        if (i >= 3) continue;
        snprintf(name, sizeof(name), "Int%sRelationship_Title", TITLES[i]);
        snprintf(text, sizeof(text), "%s %s %d", tla, loc(game, TAGS[i]), (int)ssr_career_relation(c, INT[i]));
        ssr_tweaks_set(game->tweaks, "Player", name, text);
    }
    relation_tweaks(game, instant);
}

// c_TScreen_Relationships.SetUpScreen.
static void relationships_screen(SSRGame *game, int instant, int international) {
    set_screen(game, "relationships");
    update_relationships(game, instant, international);
}

// c_TScreen_WebPage.ButtonOk: the screen after the news.
static void webpage_ok(SSRGame *game) {
    char next[32];
    snprintf(next, sizeof(next), "%s", game->newsNext);
    game->newsNext[0] = 0;
    if (!next[0] || strcmp(next, "gamemenu") == 0) game_menu(game);
    else if (strcmp(next, "relationships") == 0) relationships_screen(game, 0, 0);
    else if (strcmp(next, "leaguefixtures") == 0) leaguefixtures_play(game);
    else set_screen(game, next);
}

static void relationships(SSRGame *game) {
    relationships_screen(game, 1, 0);
}

static void relationships_home(SSRGame *game) {
    game->showInternational = 0;
    update_relationships(game, 1, 0);
    game_menu(game);
}

static void relationships_international(SSRGame *game) {
    update_relationships(game, 1, !game->showInternational);
}

// The call "DoRelationScreen".
static void call_relation_screen(SSRGame *game) {
    relationships_screen(game, 0, game->showInternational);
}

static void spent_too_much_time(SSRGame *game) {
    char text[256], number[16];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_SPENTTOOMUCHTIME"));
    snprintf(number, sizeof(number), "%d", MAX_RELATION_ACTIVITIES);
    replace_all(text, sizeof(text), "$num", number);
    if (strchr(text, '@')) return;
    message(game, "message", text, 0, 1, NULL);
}

// GSPlayerNotEnoughEnergy: the remake has no adverts, so the NRG overlay
// offers only the cans.
static void not_enough_energy(SSRGame *game, const char *activity, float need) {
    float v = need - game->career.energy;
    ssr_tweaks_set_number(game->tweaks, "WatchAdNRG", "RefillAmount", v < 1 ? 1 : v);
    ssr_tweaks_set_number(game->tweaks, "WatchAdNRG", "ShowNRGAdReward", 0);
    ssr_tweaks_set(game->tweaks, "WatchAdNRG", "AdShowedFor", activity);
    // [I: the NRG overlay comes with the shops of section 3.]
    message(game, "message", loc(game, "CMESSAGE_NOTENOUGHENERGY"), 0, 1, NULL);
}

static void start_pairs(SSRGame *game, int relation);

// The checks of an activity that all the bars share, after the checks of
// the bar. Returns 1 when the activity starts.
static int activity_allowed(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->spentTime >= MAX_RELATION_ACTIVITIES) {
        spent_too_much_time(game);
        return 0;
    }
    if (c->energy < ACTIVITY_ENERGY) {
        not_enough_energy(game, "Relationship", ACTIVITY_ENERGY);
        return 0;
    }
    return 1;
}

static void activity(SSRGame *game, int bar) {
    SSRCareer *c = &game->career;
    if (game->relationOff[bar] || c->retired) return;
    if (bar == 3) {
        const char *text = NULL;
        if (ssr_career_lifestyle(c) < 20.0f) text = "CMESSAGE_NOGIRLFRIENDLIFESTYLE";
        else if (c->relationPartner == 0) text = "CMESSAGE_NOGIRLFRIEND";
        if (text) {
            message(game, partner_screen(game), loc(game, text), 0, 1, "DoRelationScreen");
            return;
        }
    }
    if (bar == 4 && c->relationSponsors == 0) {
        message(game, "messagesponsor", loc(game, "CMESSAGE_NOSPONSORS"), 0, 1, "DoRelationScreen");
        return;
    }
    if (!activity_allowed(game)) return;
    // The time with the partner counts before the game, win or lose.
    if (bar == 3) c->lastSpendTimePartner = (float)c->sdate;
    start_pairs(game, bar);
    c->energy -= ACTIVITY_ENERGY;
    c->spentTime++;
}

static void activity_boss(SSRGame *game) {
    activity(game, 0);
}

static void activity_team(SSRGame *game) {
    activity(game, 1);
}

static void activity_fans(SSRGame *game) {
    activity(game, 2);
}

static void activity_partner(SSRGame *game) {
    activity(game, 3);
}

static void activity_sponsors(SSRGame *game) {
    activity(game, 4);
}

// The end of the relationship: a question, then the call "DumpGirl".
static void dump_partner(SSRGame *game) {
    message(game, partner_screen(game), loc(game, "CMESSAGE_GIRLFRIENDEND"), 1, 1, "DumpGirl");
}

static void call_dump_partner(SSRGame *game) {
    if (game->returnState == 0) {
        relationships_screen(game, 1, 0);
        return;
    }
    // [I: DoNews("CNEWS_GIRLFRIENDDUMPED") comes with the news of section
    // 6.]
    game->career.relationPartner = 0;
    relationships_screen(game, 0, 0);
}


/* ------------------------------------------------------------ the dilemma */

// The names of the dilemma images, from 1 (c_AScreen_Dilemma).
static const char *const DILEMMA_IMAGES[17] = {"",          "Beach",      "Boss",   "Bowling",  "Cinema", "Fans",
                                               "GoKart",    "Golf",       "Gym",    "Hospital", "Nightclub",
                                               "Physio",    "Pub",        "Restaurant", "Swimming", "Shopping",
                                               "Training"};

// The image of a dilemma of the relation: one random number a relation.
static int dilemma_image(SSRGame *game, int relation) {
    static const int BOSS[3] = {2, 11, 16};
    static const int TEAM[9] = {3, 4, 6, 7, 8, 10, 12, 14, 16};
    static const int FANS[2] = {5, 9};
    static const int PARTNER[6] = {1, 4, 10, 12, 13, 15};
    static const int SPONSORS[2] = {2, 7};
    switch (relation) {
    case 0: return BOSS[ssr_rand(&game->random, 3) - 1];
    case 1: return TEAM[ssr_rand(&game->random, 9) - 1];
    case 2: return FANS[ssr_rand(&game->random, 2) - 1];
    case 3: return PARTNER[ssr_rand(&game->random, 6) - 1];
    default: return SPONSORS[ssr_rand(&game->random, 2) - 1];
    }
}

// c_AScreen_Dilemma.SetItem: the template of the relation and the image in
// the hook.
static void dilemma_item(SSRGame *game, const char *hook, int relation, int image, int international) {
    static const char *ENTITY[5] = {"Boss", "Team", "Fans", "Girlfriend", "Sponsor"};
    int base = relation >= 6 ? relation - 6 : relation;
    char name[64];
    snprintf(name, sizeof(name), "%sDilemma%s%s", DILEMMA_IMAGES[image], international ? "Int" : "", ENTITY[base]);
    add_op(game, SSR_OP_HOOK, hook, name, NULL, 0);
    if (!international) return;
    static const char *TAGS[3] = {"BOSS_CAPS", "TEAM_CAPS", "FANS_CAPS"};
    const SSRNation *nation = ssr_world_nation(game->world, game->career.nationId);
    char text[64];
    snprintf(text, sizeof(text), "%s (%.3s)", loc(game, TAGS[base]), nation && nation->tla ? nation->tla : "");
    add_op(game, SSR_OP_SUB, hook, "BarName", text, 1);
}

// c_TScreen_Dilemma.SetUpScreen.
static void dilemma_screen(SSRGame *game) {
    SSRCareer *c = &game->career;
    set_screen(game, "dilemma");
    int *r = game->dilemmaRelation;
    r[0] = ssr_rand2(&game->random, 0, 3);
    while (c->relationPartner == 0 && r[0] == 3) r[0] = ssr_rand2(&game->random, 0, 3);
    do {
        r[1] = ssr_rand2(&game->random, 0, 3);
    } while (r[1] == r[0] || (r[1] == 3 && c->relationPartner <= 0));
    for (int i = 0; i < 2; i++) {
        if (r[i] == 2 && ssr_career_count_sponsors(c) > 0) r[i] = ssr_rand(&game->random, 2) == 1 ? 2 : 4;
    }
    game->dilemmaImage[0] = dilemma_image(game, r[0]);
    do {
        game->dilemmaImage[1] = dilemma_image(game, r[1]);
    } while (game->dilemmaImage[1] == game->dilemmaImage[0]);
    // A player that the nation picks for its next match chooses for the
    // national relations.
    game->dilemmaInt[0] = game->dilemmaInt[1] = 0;
    season_in(game);
    const SSRFixture *next = game->season ? ssr_season_next_fixture(game->season) : NULL;
    if (next && c->intApps > 0 &&
        (ssr_fixture_home_team(game->season, next) == c->nationId ||
         ssr_fixture_away_team(game->season, next) == c->nationId)) {
        int status = selection_status(game, 1);
        if (status == 1 || (status >= 7 && status <= 9)) {
            for (int i = 0; i < 2; i++) {
                if (r[i] <= 2) {
                    r[i] += 6;
                    game->dilemmaInt[i] = 1;
                }
            }
        }
    }
    relation_tweaks(game, 1);
    dilemma_item(game, "TopHook", r[0], game->dilemmaImage[0], game->dilemmaInt[0]);
    dilemma_item(game, "BottomHook", r[1], game->dilemmaImage[1], game->dilemmaInt[1]);
    game->dilemmaChosen = 0;
    game->dilemmaChoice = 0;
    game->dilemmaEnd = 0;
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Dilemma!"));
    upper(text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 2000;
}

// The fuse parameter dilemma_not_chosen_decay of 4.32.
#define DILEMMA_DECAY -10.0f

static void choose_dilemma(SSRGame *game, int item) {
    if (game->now < game->splashEnd || game->dilemmaChosen) return;
    SSRCareer *c = &game->career;
    int chosen = game->dilemmaRelation[item], other = game->dilemmaRelation[1 - item];
    if (chosen == 3) c->lastSpendTimePartner = (float)c->sdate;
    ssr_career_update_relation(c, (SSRRelation)chosen, 5.0f);
    ssr_career_update_relation(c, (SSRRelation)other, DILEMMA_DECAY);
    game->dilemmaChoice = game->dilemmaImage[item];
    relation_tweaks(game, 0);
    add_op(game, SSR_OP_SUB, item == 0 ? "TopHook" : "BottomHook", "Selected", NULL, 1);
    add_op(game, SSR_OP_SUB, item == 0 ? "BottomHook" : "TopHook", "NotSelected", NULL, 1);
    // c_ActionAfterDilemma runs 2500 ms later.
    game->dilemmaEnd = game->now + 2500;
    game->dilemmaChosen = 1;
}

static void dilemma_top(SSRGame *game) {
    choose_dilemma(game, 0);
}

static void dilemma_bottom(SSRGame *game) {
    choose_dilemma(game, 1);
}

// c_TPlayer.DepleteSkillByLevels: the top levels of the skill go.
static int deplete_skill(int *levels, int count) {
    int lost = 0;
    for (int n = 0; n < count; n++) {
        int k = SSR_CAREER_SKILL_LEVELS - 1;
        while (k > 0 && levels[k] <= 0) k--;
        if (k > 0) {
            lost += levels[k];
            levels[k] = 0;
        }
    }
    return lost;
}

// c_TScreen_Dilemma.CheckAccident and DepleteSkill: the place of the
// choice can cost the player a skill.
static void dilemma_accident(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->sdate < game->lastAccident + 28) return;
    int a = ssr_rand(&game->random, 10);
    if (ssr_rand(&game->random, a) != 1) return;
    int skill = ssr_rand(&game->random, 5);
    // The rows of skill_levels are pace, power, technique, vision, and set
    // pieces, as the numbers 1 to 5 of the game.
    int *levels = skill_levels(c, skill - 1);
    if (ssr_career_skill(levels) < 4) return;
    static const char *PLACES[17] = {"",          "BEACH", "",          "BOWLING", "CINEMA",   "", "GOKART",
                                     "GOLF",      "GYM",   "HOSPITAL",  "NIGHTCLUB", "",     "PUB", "RESTAURANT",
                                     "SWIMMING",  "SHOPPING", ""};
    int choice = game->dilemmaChoice;
    if (choice < 1 || choice > 16 || !PLACES[choice][0]) return;
    char tag[48], text[512];
    snprintf(tag, sizeof(tag), "CMESSAGE_DILEMMA%s%d", PLACES[choice], skill);
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    if (!text[0] || strchr(text, '@')) return;
    game->lastAccident = c->sdate;
    int age = ssr_career_age(c);
    int lost = deplete_skill(levels, age <= 23 ? 1 : age <= 32 ? 2 : 3);
    skill_tweaks(game);
    // [I: the bar of the skill in the message needs a bar source that the
    // code can change.]
    message(game, "messagedefault", text, 0, 1, NULL);
    (void)lost;
}

static void dilemma_tick(SSRGame *game) {
    if (!game->dilemmaEnd || game->now < game->dilemmaEnd) return;
    game->dilemmaEnd = 0;
    save_career(game);
    game_menu(game);
    dilemma_accident(game);
}


/* ------------------------------------------------------------- the news */

// A message with a decision and the bars of the energy and of a relation
// (SetMessageScreen with two bars and a decision).
static void message_decision(SSRGame *game, const char *layout, const char *text, const char *call, int data,
                             const char *name) {
    message(game, layout, text, 1, 1, call);
    add_op(game, SSR_OP_SHOW, "TwoBars", NULL, NULL, 0);
    game->decision = 1;
    game->messageData = data;
    snprintf(game->messageText, sizeof(game->messageText), "%s", name ? name : "");
}

// c_TScreen_Message.UpdateRelationships: Yes and No of a decision.
static void message_relations(SSRGame *game, int state) {
    SSRCareer *c = &game->career;
    const char *call = game->messageCall;
    if (strcmp(call, "DoRelationshipRequest") == 0) {
        if (state == 1) {
            ssr_career_update_relation(c, (SSRRelation)game->messageData, 5.0f);
            set_energy(game, c->energy - 25);
        } else {
            ssr_career_update_relation(c, (SSRRelation)game->messageData, -10.0f);
        }
    } else if (strcmp(call, "VehicleBorrowedTeam") == 0) {
        ssr_career_update_relation(c, SSR_RELATION_TEAM, state == 1 ? 5.0f : -15.0f);
    } else if (strcmp(call, "VehicleBorrowedGirl") == 0) {
        ssr_career_update_relation(c, SSR_RELATION_PARTNER, state == 1 ? 5.0f : -15.0f);
    }
    relation_tweaks(game, 0);
    player_tweaks(game);
}

static void decision_tick(SSRGame *game) {
    if (game->now < game->decisionEnd) return;
    game->decisionEnd = 0;
    char call[64];
    snprintf(call, sizeof(call), "%s", game->messageCall);
    game->messageCall[0] = 0;
    set_screen(game, game->messageReturn);
    run_call(game, call);
}

// c_TPlayer.DoNews.
static void do_news(SSRGame *game, const char *tag, const char *team, const char *team2) {
    SSRCareer *c = &game->career;
    char *text = c->headline;
    size_t capacity = sizeof(c->headline);
    snprintf(text, capacity, "%s", loc(game, tag));
    if (team) replace_all(text, capacity, "$clubname", team);
    if (team2) {
        replace_all(text, capacity, "$opposingclubname", team2);
        replace_all(text, capacity, "$offerclubname", team2);
    }
    replace_all(text, capacity, "$playername", c->fullName);
    replace_all(text, capacity, "$lastname", c->shortName);
}

static const char *my_club_tla(SSRGame *game) {
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    return club && club->tla ? club->tla : "";
}

// GSCheckNewsGetDumped.
static int news_get_dumped(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (!(c->relationPartner > 0 && c->relationPartner < 5)) return 0;
    c->relationPartner = 0;
    message(game, partner_screen(game), loc(game, "CMESSAGE_GIRLFRIENDDUMPSYOU"), 0, 1, "GirlDumpsYou");
    return 1;
}

static void call_girl_dumps_you(SSRGame *game) {
    do_news(game, ssr_rand(&game->random, 2) == 1 ? "CNEWS_GIRLFRIENDDUMPSYOU" : "CNEWS_GIRLFRIENDDUMPED", NULL, NULL);
    webpage_to(game, "relationships", 0);
}

// GSCheckNewsGirlUnhappy: the partner wants time after 35 days.
static int news_partner_unhappy(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->relationPartner <= 0 || (float)c->sdate <= c->lastSpendTimePartner + 35) return 0;
    if (c->relationPartner < 31) {
        ssr_career_update_relation(c, SSR_RELATION_PARTNER, -5);
        if (c->relationTeam > c->relationBoss && c->relationTeam > c->relationFans) {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGETEAM", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            ssr_career_update_relation(c, SSR_RELATION_TEAM, -c->relationTeam / 2.0f);
        } else if (c->relationFans > c->relationBoss && c->relationFans > c->relationTeam) {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGEFANS", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            ssr_career_update_relation(c, SSR_RELATION_FANS, -c->relationFans / 2.0f);
        } else {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGEBOSS", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            ssr_career_update_relation(c, SSR_RELATION_BOSS, -c->relationBoss / 2.0f);
        }
    } else {
        ssr_career_update_relation(c, SSR_RELATION_PARTNER, -10);
        relation_tweaks(game, 0);
        message_bar(game, partner_screen(game), loc(game, "CMESSAGE_LONGTIMEGIRLFRIEND"), "DoRelationScreen");
    }
    c->lastSpendTimePartner = (float)c->sdate;
    return 1;
}

static int *product_array(SSRCareer *c, SSRShopType type) {
    return type == SSR_SHOP_ITEM ? c->items : type == SSR_SHOP_VEHICLE ? c->vehicles : c->properties;
}

// c_TPlayer.CheckNewsItemExpiry: a product breaks at a wear of 1000 and goes
// after 1100 more.
static int news_item_expiry(SSRGame *game) {
    SSRCareer *c = &game->career;
    static const char *CARE[6] = {NULL, "CMESSAGE_ITEMREPAIR", "CMESSAGE_ITEMUPGRADE", "CMESSAGE_ITEMCLEAN",
                                  "CMESSAGE_ITEMRESTORE", "CMESSAGE_ITEMREPLACE"};
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = ssr_shop_item(i);
        int *v = &product_array(c, item->type)[item->id - 1];
        char text[512];
        if (*v > 11100) {
            *v = 0;
            snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_ITEMEXPIRED"));
            char name[64];
            snprintf(name, sizeof(name), "%s", loc(game, item->uid));
            upper(name);
            replace_all(text, sizeof(text), "$item", name);
        } else if (*v > 1000 && *v <= 10000) {
            *v += 10000;
            const char *tag = item->type == SSR_SHOP_VEHICLE ? CARE[1] : item->type == SSR_SHOP_PROPERTY ? CARE[4]
                                                                                                           : CARE[item->lifeType];
            if (item->type == SSR_SHOP_ITEM && item->lifeType == SSR_LIFE_NONE) return 0;
            snprintf(text, sizeof(text), "%s", tag ? loc(game, tag) : "");
            char name[64];
            snprintf(name, sizeof(name), "%s", loc(game, item->uid));
            replace_all(text, sizeof(text), "$item", name);
        } else {
            continue;
        }
        game->messageData = item->type;
        message(game, "message", text, 0, 1, "ExpiredItem");
        return 1;
    }
    return 0;
}

// GSCheckNewsGetGirlfriend.
static int news_get_partner(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->relationPartner != 0) return 0;
    if (ssr_career_lifestyle(c) < 20) {
        char text[512];
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_GIRLFRIENDNOTIMPRESSED"));
        if (ssr_rand(&game->random, 2) == 1) {
            ssr_career_update_relation(c, SSR_RELATION_TEAM, -5);
            relation_tweaks(game, 0);
            size_t n = strlen(text);
            snprintf(text + n, sizeof(text) - n, " %s", loc(game, "CMESSAGE_TEAMMATESRIBBING"));
            message_bar(game, "messageteam", text, NULL);
        } else {
            message(game, partner_screen(game), text, 0, 1, NULL);
        }
        return 1;
    }
    message(game, partner_screen(game), loc(game, "CMESSAGE_GIRLFRIENDMEETMOBILE"), 1, 1, "MeetGirl");
    return 1;
}

static void call_meet_partner(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState != 1) {
        leaguefixtures_screen(game, 0);
        return;
    }
    c->relationPartner = 60;
    c->lastSpendTimePartner = (float)c->sdate;
    relationships_screen(game, 0, 0);
}

// GSCheckNewsGirlScandal.
static int news_partner_scandal(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->relationPartner <= 1 || ssr_rand(&game->random, 12) != 1) return 0;
    int k = ssr_rand(&game->random, 10);
    int high = ssr_rand(&game->random, 2) == 1;
    char tag[48];
    snprintf(tag, sizeof(tag), "CNEWS_GIRLSCANDAL%s%d", high ? "HIGH" : "LOW", k);
    float d = high ? -20.0f : -10.0f;
    ssr_career_update_relation(c, SSR_RELATION_BOSS, d);
    ssr_career_update_relation(c, SSR_RELATION_BOSS_INT, d);
    do_news(game, tag, my_club_tla(game), NULL);
    webpage_to(game, "relationships", 0);
    return 1;
}

// GSCheckNewsEnergyRequest: a relation asks for a quarter of the energy.
static int news_energy_request(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->energy < 25) return 0;
    static const char *REL_TWO[5] = {"BossRelationship", "TeamRelationship", "FansRelationship",
                                     "GirlfriendRelationship", "SponsorsRelationship"};
    char tag[48] = "";
    const char *layout = NULL;
    int w = 0;
    while (!tag[0]) {
        w = ssr_rand2(&game->random, 0, 4);
        if (w == 0) {
            snprintf(tag, sizeof(tag), "CMESSAGE_BOSSREQUEST%d", ssr_rand(&game->random, 5));
            layout = "messageboss";
        } else if (w == 1) {
            snprintf(tag, sizeof(tag), "CMESSAGE_TEAMREQUEST%d", ssr_rand(&game->random, 5));
            layout = "messageteam";
        } else if (w == 2) {
            snprintf(tag, sizeof(tag), "CMESSAGE_FANSREQUEST%d", ssr_rand(&game->random, 5));
            layout = "messagefans";
        } else if (w == 3 && c->relationPartner > 0) {
            snprintf(tag, sizeof(tag), "CMESSAGE_GIRLREQUEST%d", ssr_rand(&game->random, 5));
            layout = partner_screen(game);
        } else if (w == 4 && ssr_career_count_sponsors(c) > 0) {
            snprintf(tag, sizeof(tag), "CMESSAGE_SPONSORREQUEST%d", ssr_rand(&game->random, 10));
            layout = "messagesponsor";
        }
    }
    char text[512];
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    replace_all(text, sizeof(text), "$percent", "25");
    relation_tweaks(game, 1);
    message_decision(game, layout, text, "DoRelationshipRequest", w, REL_TWO[w]);
    return 1;
}

static void call_relationship_request(SSRGame *game) {
    game_menu(game);
}

// GSCheckNewsHappiness: a low relation makes the news, and costs stars.
static int news_happiness(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->apps < 15) return 0;
    int k = ssr_rand(&game->random, 3);
    int limit = ssr_rand(&game->random, 60);
    int which = ssr_rand(&game->random, 5);
    const char *tag = NULL;
    if (which == 1 && (int)c->relationBoss < limit) tag = "CNEWS_RELATIONSHIPBOSS";
    else if (which == 2 && (int)c->relationTeam < limit) tag = "CNEWS_RELATIONSHIPTEAM";
    else if (which == 3 && (int)c->relationFans < limit) tag = "CNEWS_RELATIONSHIPFANS";
    else if (which == 4 && c->relationPartner > 0 && c->relationPartner < (float)limit) tag = "CNEWS_RELATIONSHIPGIRL";
    else if (which == 5 && c->relationSponsors > 0 && (int)c->relationSponsors < limit) tag = "CNEWS_RELATIONSHIPSPONSORS";
    if (!tag && ssr_career_happiness(c) < (float)ssr_rand(&game->random, 50)) tag = "CNEWS_RELATIONSHIPLOW";
    if (!tag) return 0;
    char full[48];
    snprintf(full, sizeof(full), "%s%d", tag, k);
    do_news(game, full, my_club_tla(game), NULL);
    c->starRating = ssr_star_rating_add(c->starRating, -10);
    webpage_to(game, "gamemenu", 0);
    return 1;
}

// GSCheckNewsStyle: the press looks at the lifestyle of the player.
static int news_style(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->apps < 10) return 0;
    if (ssr_career_lifestyle(c) < 7) {
        char tag[32];
        snprintf(tag, sizeof(tag), "CNEWS_STYLELOW%d", ssr_rand(&game->random, 3));
        do_news(game, tag, my_club_tla(game), NULL);
        webpage_to(game, "gamemenu", 1);
        c->starRating = ssr_star_rating_add(c->starRating, -5);
        return 1;
    }
    static const char *TAGS[3] = {"CNEWS_STYLEGADGET", "CNEWS_STYLEVEHICLE", "CNEWS_STYLEPROPERTY"};
    static const char *KEYS[3] = {"$item", "$vehicle", "$property"};
    int t = ssr_rand(&game->random, 3);
    SSRShopType type = (SSRShopType)t;
    const SSRShopItem *item = ssr_shop_nth_of_type(type, ssr_rand(&game->random, ssr_shop_count_by_type(type)));
    if (!item || product_array(c, type)[item->id - 1] <= 0) return 0;
    do_news(game, TAGS[t - 1], my_club_tla(game), NULL);
    replace_all(c->headline, sizeof(c->headline), KEYS[t - 1], loc(game, item->uid));
    webpage_to(game, "gamemenu", 1);
    return 1;
}

// GSCheckNewsBorrowVehicle: a team-mate or the partner asks for a vehicle.
static int news_borrow_vehicle(SSRGame *game) {
    SSRCareer *c = &game->career;
    int any = 0;
    for (int i = 0; i < SSR_CAREER_ITEMS; i++) any |= c->vehicles[i] > 0;
    if (!any) return 0;
    const SSRShopItem *v = NULL;
    for (int i = 0; i < 10; i++) {
        v = ssr_shop_nth_of_type(SSR_SHOP_VEHICLE, ssr_rand(&game->random, ssr_shop_count_by_type(SSR_SHOP_VEHICLE)));
        if (v && c->vehicles[v->id - 1] > 0 && c->vehicles[v->id - 1] < 1000) break;
    }
    if (!v || c->vehicles[v->id - 1] == 0) return 0;
    char text[512];
    const char *name = loc(game, v->uid);
    int partner = !(c->relationPartner <= 1 || ssr_rand(&game->random, 2) != 1);
    snprintf(text, sizeof(text), "%s", loc(game, partner ? "CMESSAGE_VEHICLEBORROWGIRL" : "CMESSAGE_VEHICLEBORROWTEAM"));
    replace_all(text, sizeof(text), "$vehicle", name);
    relation_tweaks(game, 1);
    message_decision(game, partner ? partner_screen(game) : "messageteam", text,
                     partner ? "VehicleBorrowedGirl" : "VehicleBorrowedTeam", v->id, NULL);
    // The picture of the vehicle in the box (ImageHook).
    char image[64];
    snprintf(image, sizeof(image), "%s_Image", v->uid);
    add_op(game, SSR_OP_HOOK, "ImageHook", image, NULL, 0);
    add_op(game, SSR_OP_SHOW, "ImageHook", NULL, NULL, 0);
    return 1;
}

// The function after a vehicle loan: the vehicle can come back damaged.
static void vehicle_returned(SSRGame *game, int partner) {
    SSRCareer *c = &game->career;
    int yes = game->returnState == 1;
    int q = ssr_rand(&game->random, 4);
    float d = q == 1 ? -10.0f : yes ? 5.0f : -15.0f;
    SSRRelation r = partner ? SSR_RELATION_PARTNER : SSR_RELATION_TEAM;
    ssr_career_update_relation(c, r, d);
    relationships_screen(game, partner, 0);
    if (!(yes && q == 1)) return;
    int *v = &c->vehicles[game->messageData - 1];
    *v += ssr_rand2(&game->random, 250, 850);
    if (*v > 1100) *v = 1100;
    relation_tweaks(game, 0);
    message_bar(game, partner ? partner_screen(game) : "messageteam",
                loc(game, partner ? "CMESSAGE_VEHICLEDAMAGEDGIRL2" : "CMESSAGE_VEHICLEDAMAGEDTEAM2"), NULL);
}

static void call_vehicle_team(SSRGame *game) {
    vehicle_returned(game, 0);
}

static void call_vehicle_partner(SSRGame *game) {
    vehicle_returned(game, 1);
}

static int compare_club_news(void *context, int a, int b) {
    const int *randNo = context;
    return randNo[a] - randNo[b];
}

// c_TPlayer.CheckNewsRandomClubNews: a club of the league of the player
// gets stronger or weaker.
static int news_random_club(SSRGame *game) {
    SSRSeason *s = game->season;
    if (!s) return 1;
    int n = s->clubListCount;
    int *order = malloc(sizeof(int) * (size_t)(n > 0 ? n : 1));
    int *randNo = malloc(sizeof(int) * (size_t)(n > 0 ? n : 1));
    if (!order || !randNo) {
        free(order);
        free(randNo);
        return 1;
    }
    for (int i = 0; i < n; i++) {
        order[i] = i;
        randNo[i] = (int)(ssr_rnd(&game->random) * 9999.0f);
    }
    ssr_sort(order, n, compare_club_news, randNo, 0);
    const SSRSeasonClub *mine = ssr_season_club(s, game->career.clubId, 0);
    SSRSeasonClub *club = NULL;
    for (int i = 0; i < n && mine; i++) {
        SSRSeasonClub *c = &s->clubs[s->clubList[order[i]]];
        if (c->data->id != game->career.clubId && c->league == mine->league) {
            club = c;
            break;
        }
    }
    free(order);
    free(randNo);
    int k = ssr_rand(&game->random, 15);
    if (!club) return 1;
    int boost = ssr_rand(&game->random, 2) == 1;
    club->strength += boost ? 3 : -3;
    club->strength = club->strength < 10 ? 10 : club->strength > 95 ? 95 : club->strength;
    char tag[48];
    snprintf(tag, sizeof(tag), "CNEWS_RANDOMCLUB%s%d", boost ? "BOOST" : "CRISIS", k);
    do_news(game, tag, club->data->tla, NULL);
    webpage_to(game, "gamemenu", 0);
    return 1;
}

// The random news of the week, by the round of the next match.
static int random_news(SSRGame *game, const SSRFixture *next) {
    SSRCareer *c = &game->career;
    int r = next->round % 3;
    if (r == 0) {
        if (ssr_rand(&game->random, 2) == 1) {
            return c->relationPartner == 0 ? news_get_partner(game) : news_partner_scandal(game);
        }
        return news_energy_request(game);
    }
    if (r == 1) {
        if (ssr_rand(&game->random, 2) == 1 && news_happiness(game)) return 1;
        c->newsDone = 1;
        dilemma_screen(game);
        return 1;
    }
    switch (ssr_rand(&game->random, 3)) {
    case 1: return news_style(game);
    case 2: return news_borrow_vehicle(game);
    default: return news_random_club(game);
    }
}

// GSPlayerPlay: the news of the week comes before the match.
static int play_news(SSRGame *game, const SSRFixture *next) {
    SSRCareer *c = &game->career;
    // [I: the old pro boots (a), the transfer window (e), the captaincy
    // (f), the sponsors (g), and the gambling (h) come with their parts.
    // The Facebook box (b) is not in the remake.]
    if (!game->settings.doneLoseSkills && c->apps > 3) {
        game->settings.doneLoseSkills = 1;
        game->settingsChanged = 1;
        c->newsDone = 1;
        message(game, "message", loc(game, "CMESSAGE_LOSESKILLS"), 0, 1, NULL);
        return 1;
    }
    if (news_get_dumped(game) || news_partner_unhappy(game) || news_item_expiry(game)) {
        c->newsDone = 1;
        return 1;
    }
    if (random_news(game, next)) {
        c->newsDone = 1;
        return 1;
    }
    c->newsDone = 1;
    // The pitch pack offer is a purchase, so the remake goes on to the tip.
    if (c->tipCount < 30) {
        c->tipCount++;
        char tag[32], title[64];
        snprintf(tag, sizeof(tag), "CMOBILE_TIP%d", c->tipCount);
        snprintf(title, sizeof(title), "%s!", loc(game, "help_Tip"));
        message(game, "message", loc(game, tag), 0, 1, "DoTip");
        ssr_tweaks_set(game->tweaks, "Menu", "MessageTitle", title);
        return 1;
    }
    return 0;
}

static void call_tip(SSRGame *game) {
    leaguefixtures_screen(game, 0);
}

// The call "ExpiredItem": the lifestyle shop on the tab of the product
// [I: the shop comes with its part; the game menu shows until then].
static void call_expired_item(SSRGame *game) {
    game_menu(game);
}


/* ---------------------------------------------------------- the interview */

static void interview_button(char *out, size_t capacity, int i) {
    snprintf(out, capacity, "Button%d", i);
}

static void interview_outline(SSRGame *game, int i, const char *outline, int visible) {
    char ref[16];
    interview_button(ref, sizeof(ref), i);
    add_op(game, SSR_OP_SUB, ref, outline, NULL, visible);
}

static void interview_sentence(SSRGame *game, const char *text) {
    add_op(game, SSR_OP_TEXT, "Sentence", NULL, text, 0);
}

// The text with its first letter in capitals.
static void capitalise(char *out, size_t capacity, const char *text) {
    snprintf(out, capacity, "%s", text);
    if (out[0]) out[0] = (char)toupper((unsigned char)out[0]);
}

// c_TScreen_Interview.SetUpScreen. The debug console of the game opens it
// with "forcescreen interview".
static void interview_screen(SSRGame *game) {
    SSRCareer *c = &game->career;
    set_screen(game, "interview");
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Interview!"));
    upper(text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 1500;
    if (c->interviewSkill < 3) c->interviewSkill = 3;
    int skill = c->interviewSkill;
    game->difficulty = skill < 3 ? 3 : skill > 5 ? 5 : skill;
    game->buttons = skill > 8 ? 15 : skill > 5 ? 12 : 9;
    int decoys = skill - game->difficulty;
    game->decoys = decoys < 0 ? 0 : decoys > 3 ? 3 : decoys;
    game->ready = 0;
    game->interviewFail = 0;
    game->sentence[0] = 0;
    interview_sentence(game, loc(game, "interview_Instrucs"));
    game->countdown = game->now + 2500;
    game->buttonCount = 1;
    game->beepDone = 0;
    char ref[16], shown[64], tag[32];
    for (int i = 1; i <= 15; i++) {
        if (i <= 3) {
            snprintf(tag, sizeof(tag), "CLICHE_%d", i);
            snprintf(game->words[i - 1], sizeof(game->words[i - 1]), "%s", loc(game, tag));
        } else {
            // Each word differs from the words before it.
            for (;;) {
                snprintf(tag, sizeof(tag), "CLICHE_%d", ssr_rand2(&game->random, 4, 25));
                snprintf(game->words[i - 1], sizeof(game->words[i - 1]), "%s", loc(game, tag));
                capitalise(shown, sizeof(shown), game->words[i - 1]);
                int same = 0;
                for (int k = 0; k < i - 1 && !same; k++) {
                    char other[64];
                    capitalise(other, sizeof(other), game->words[k]);
                    same = strcmp(shown, other) == 0;
                }
                if (!same) break;
            }
        }
        capitalise(shown, sizeof(shown), game->words[i - 1]);
        interview_button(ref, sizeof(ref), i);
        add_op(game, SSR_OP_TEXT, ref, NULL, shown, 0);
        add_op(game, i <= game->buttons ? SSR_OP_SHOW : SSR_OP_HIDE, ref, NULL, NULL, 0);
        add_op(game, SSR_OP_ELEMENT_COLOUR, ref, NULL, "FFFFFFFF", 0);
    }
    int decoysLeft = game->decoys;
    for (int j = 0; j < game->buttons; j++) {
        int unique;
        do {
            game->sequence[j] = ssr_rand(&game->random, game->buttons);
            unique = 1;
            for (int k = 0; k < j; k++) unique &= game->sequence[k] != game->sequence[j];
        } while (!unique);
        if (ssr_rand(&game->random, 3) == 1 && decoysLeft > 0) {
            decoysLeft--;
            game->sequence[j] = -game->sequence[j];
        }
    }
}

// GSMatchStatsUpdateRelationships, its end: the press wants a word after
// some matches with gains for the boss, the team, and the fans.
static int start_interview(SSRGame *game) {
    SSRCareer *c = &game->career;
    // [I: the captain gets an interview more often; the captaincy comes
    // with its part.]
    int n = 8, gap = 21;
    if (game->interviewDone || ssr_rand(&game->random, n) != 1 || c->sdate <= c->lastInterview + gap) return 0;
    if (game->after.boss <= 0 || game->after.team <= 0 || game->after.fans <= 0) return 0;
    game->interviewDone = 1;
    c->lastInterview = c->sdate;
    interview_screen(game);
    return 1;
}

// c_TScreen_Interview.ButtonOk: back to the match stats.
static void interview_ok(SSRGame *game) {
    SSRCareer *c = &game->career;
    set_screen(game, "matchstats");
    if (!game->interviewFail) {
        // InterviewSuccess.
        c->starRating = ssr_star_rating_add(c->starRating, 10);
    } else {
        // InterviewFail: the gains of the match go.
        float a = fabsf((float)game->after.boss), b = fabsf((float)game->after.team), f = fabsf((float)game->after.fans);
        SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
        int national = comp && comp->data->level == 1;
        ssr_career_update_relation(c, national ? SSR_RELATION_BOSS_INT : SSR_RELATION_BOSS, -a);
        ssr_career_update_relation(c, national ? SSR_RELATION_TEAM_INT : SSR_RELATION_TEAM, -b);
        ssr_career_update_relation(c, national ? SSR_RELATION_FANS_INT : SSR_RELATION_FANS, -f);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-a);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-b);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-f);
        game->after.boss = game->after.team = game->after.fans = 0;
        static const char *MODS[3] = {"Boss", "Team", "Fans"};
        for (int i = 0; i < 3; i++) ssr_tweaks_set_number(game->tweaks, "RelationshipMods", MODS[i], 0);
        ssr_tweaks_set_number(game->tweaks, "Menu", "ZeroRelChange", 1);
        skill_tweaks(game);
    }
    star_tweaks(game);
    ssr_tweaks_set_number(game->tweaks, "MatchStats", "StatStage", game->statStage);
}

static void interview_end(SSRGame *game, int success) {
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, success ? "Success!" : "Fail!"));
    upper(text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 1000;
    if (success) game->career.interviewSkill++;
    else game->interviewFail = 1;
    game->ready = game->now > 1 ? game->now : 2;
}

// c_TScreen_Interview.ButtonAddText.
static void interview_click(SSRGame *game, int i) {
    if (!game->ready || game->ready > 1 || game->buttonCount < 0 || game->buttonCount > 5) return;
    if (i < 1 || i > game->buttons) return;
    int want = game->sequence[game->buttonCount - 1];
    want = want < 0 ? -want : want;
    char ref[16];
    interview_button(ref, sizeof(ref), i);
    size_t used = strlen(game->sentence);
    if (i == want) {
        interview_outline(game, i, "CorrectOutline", 1);
        const char *w = game->words[i - 1];
        if (game->buttonCount == 1) {
            char first[64];
            capitalise(first, sizeof(first), w);
            snprintf(game->sentence + used, sizeof(game->sentence) - used, "%s", first);
        } else if (game->buttonCount == game->difficulty) {
            snprintf(game->sentence + used, sizeof(game->sentence) - used, ", %s.", w);
        } else {
            snprintf(game->sentence + used, sizeof(game->sentence) - used, ", %s", w);
        }
        interview_sentence(game, game->sentence);
        if (game->buttonCount == game->difficulty) {
            game->buttonCount++;
            interview_end(game, 1);
            return;
        }
        game->buttonCount++;
        return;
    }
    add_op(game, SSR_OP_ELEMENT_COLOUR, ref, NULL, "FF0000FF", 0);
    snprintf(game->sentence + used, sizeof(game->sentence) - used, "...");
    interview_sentence(game, game->sentence);
    game->buttonCount = -game->buttonCount;
    interview_end(game, 0);
}

// c_TScreen_Interview.Update.
static void interview_tick(SSRGame *game) {
    int t = game->now;
    if (!game->ready) {
        int b = game->sequence[game->buttonCount - 1];
        int at = b < 0 ? -b : b;
        if (t > game->countdown + 1000 && t < game->countdown + 1500 && !game->beepDone) {
            game->beepDone = 1;
            interview_outline(game, at, b < 0 ? "IncorrectOutline" : "CorrectOutline", 1);
            if (game->buttonCount == 1) interview_sentence(game, "");
        } else if (t > game->countdown + 1500) {
            game->beepDone = 0;
            interview_outline(game, at, "CorrectOutline", 0);
            interview_outline(game, at, "IncorrectOutline", 0);
            game->countdown = t;
            if (b < 0) {
                for (int k = game->buttonCount - 1; k < 14; k++) game->sequence[k] = game->sequence[k + 1];
            } else {
                game->buttonCount++;
            }
            if (game->buttonCount > game->difficulty) {
                interview_sentence(game, loc(game, "Go!"));
                game->buttonCount = 1;
                game->ready = 1;
            }
        }
        return;
    }
    // After a fault the right button flashes.
    if (game->interviewFail && t >= game->splashEnd && game->buttonCount < 0) {
        int b = game->sequence[-game->buttonCount - 1];
        interview_outline(game, b < 0 ? -b : b, "CorrectOutline", t % 1000 >= 500);
    }
    if (game->ready > 1 && (t > game->ready + 3000 || game->touch.down)) interview_ok(game);
}

#define WORD(n)                                                                                                        \
    static void word##n(SSRGame *game) {                                                                               \
        interview_click(game, n);                                                                                      \
    }
WORD(1)
WORD(2)
WORD(3)
WORD(4)
WORD(5)
WORD(6)
WORD(7)
WORD(8)
WORD(9)
WORD(10)
WORD(11)
WORD(12)
WORD(13)
WORD(14)
WORD(15)
#undef WORD

/* ------------------------------------------------------------ the pairs */

// The face of each card by relation and image id (c_<X>CardAnimations).
static const char *const CARD_FACES[5][4] = {
    {"BatAnim", "ChairAnim", "BananaAnim", "ShirtAnim"},
    {"BowlingAnim", "GolfAnim", "BatAnim", "BallAnim"},
    {"PenAnim", "DiscoAnim", "ShirtAnim", "CameraAnim"},
    {"RoseAnim", "PrezzieAnim", "CocktailAnim", "HolidayAnim"},
    {"PhoneAnim", "CashAnim", "ChairAnim", "CameraAnim"},
};
// The places of the cards on the grid, in row order.
static const int CARD_X[4] = {22, 172, 322, 472};
static const int CARD_Y[4] = {224, 389, 556, 723};

static int compare_icons(void *context, int a, int b) {
    const SSRGame *game = context;
    return game->icons[a].randNo - game->icons[b].randNo;
}

static void card_ref(char *out, size_t capacity, int k) {
    snprintf(out, capacity, "Button%d", k + 1);
}

// Plays the flip of card k from the frame at the speed.
static void flip_card(SSRGame *game, int k, int frame, const char *speed) {
    char ref[16];
    card_ref(ref, sizeof(ref), k);
    add_op(game, SSR_OP_ANIM_PLAY, ref, NULL, speed, frame);
}

static void card_position(SSRGame *game, int n, int x, int y) {
    char name[32];
    snprintf(name, sizeof(name), "Button%d_X", n);
    ssr_tweaks_set_number(game->tweaks, "Pairs", name, x);
    snprintf(name, sizeof(name), "Button%d_Y", n);
    ssr_tweaks_set_number(game->tweaks, "Pairs", name, y);
}

// c_TScreen_Pairs.SetUpScreen.
static void start_pairs(SSRGame *game, int relation) {
    // TPair_Icon.CreateAll: the list in id order, once until the screen
    // goes.
    if (!game->pairsMade) {
        for (int i = 0; i < 16; i++) {
            game->icons[i].id = i;
            game->iconOrder[i] = i;
        }
        game->pairsMade = 1;
    }
    set_screen(game, "pairs");
    // AScreen_Pairs.LoadItems: a tint for each card (element 2, the
    // instance, of the gadget), and the places.
    static const char *TINTS[4] = {"0000FFFF", "008000FF", "AA0000FF", "BB8000FF"};
    for (int n = 1; n <= 16; n++) {
        char name[16], ref[16];
        snprintf(name, sizeof(name), "CardColour%d", ssr_rand2(&game->random, 0, 3) + 1);
        const char *tint = ssr_tweaks_get(game->tweaks, "Pairs", name);
        card_ref(ref, sizeof(ref), n - 1);
        add_op(game, SSR_OP_ELEMENT_COLOUR, ref, NULL, tint && tint[0] ? tint : TINTS[name[10] - '1'], 2);
        card_position(game, n, CARD_X[(n - 1) % 4], CARD_Y[(n - 1) / 4]);
    }
    game->pairsRelation = relation;
    // TPair_Icon.SetUp: the image ids 1 to 4 four times, the back, and a
    // random number for the sort.
    for (int i = 0, j = 1; i < 16; i++, j = j % 4 + 1) {
        int k = game->iconOrder[i];
        game->icons[k].randNo = ssr_rand(&game->random, 99);
        game->icons[k].picked = 0;
        ssr_rand2(&game->random, 0, 3);
        game->icons[k].imgId = j;
    }
    ssr_sort(game->iconOrder, 16, compare_icons, game, 0);
    for (int i = 0; i < 16; i++) game->icons[game->iconOrder[i]].id = i;
    game->clicked = -2;
    game->timeClicked = game->now;
    // The game seeds its random numbers from the clock here.
    game->random.seed = game->now;
    game->turn = 0;
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", loc(game, "Find a pair!"));
    game->splashEnd = game->now + 1500;
    // AScreen_Pairs.Init: each card shows its face.
    char ref[16];
    for (int k = 0; k < 16; k++) {
        card_ref(ref, sizeof(ref), k);
        add_op(game, SSR_OP_ANIM_PLAY, ref, CARD_FACES[relation][game->icons[game->iconOrder[k]].imgId - 1], "0", 0);
    }
}

static void flip_cards_down(SSRGame *game) {
    for (int k = 0; k < 16; k++) flip_card(game, k, 0, "1");
}

static int compare_shuffle(void *context, int a, int b) {
    (void)a;
    (void)b;
    return ssr_rand2(&((SSRGame *)context)->random, 2, -1);
}

static int compare_places(void *context, int a, int b) {
    const int *randNo = context;
    return randNo[a] - randNo[b];
}

// NewButtonPositions and AScreen_Pairs.Shuffle.
static void shuffle_cards(SSRGame *game) {
    int randNo[16], places[16];
    for (int i = 0; i < 16; i++) {
        randNo[i] = ssr_rand(&game->random, 99);
        places[i] = i;
    }
    // The sort of the classic buttons uses no random numbers and moves no
    // card of the modern screen.
    ssr_sort(places, 16, compare_places, randNo, 0);
    for (int i = 0; i < 16; i++) places[i] = i;
    ssr_sort(places, 16, compare_shuffle, game, 0);
    for (int n = 1; n <= 16; n++) {
        int p = places[n - 1];
        card_position(game, n, CARD_X[p % 4], CARD_Y[p / 4]);
    }
}

static void pairs_success(SSRGame *game, int imgId) {
    static const char *SCREENS[5] = {"messageboss", "messageteam", "messagefans", NULL, "messagesponsor"};
    static const char *TAGS[5] = {"CDILEMMA_BOSS", "CDILEMMA_TEAM", "CDILEMMA_FANS", "CDILEMMA_GIRL",
                                  "CDILEMMA_SPONSORS"};
    int r = game->pairsRelation;
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Success!"));
    upper(text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 4000;
    ssr_career_update_relation(&game->career, BAR_RELATIONS[r], 5.0f);
    relation_tweaks(game, 0);
    char tag[32];
    snprintf(tag, sizeof(tag), "%s%d", TAGS[r], imgId);
    save_career(game);
    // The screen goes, so the next game makes the list again.
    game->pairsMade = 0;
    message_bar(game, SCREENS[r] ? SCREENS[r] : partner_screen(game), loc(game, tag), "DoRelationScreen");
}

static void pairs_fail(SSRGame *game) {
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Fail!"));
    upper(text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    game->splashEnd = game->now + 1000;
    save_career(game);
    // c_ActionAfterPairsFail: the relationships screen after 1000 ms.
    game->clicked = 3;
    game->timeClicked = game->now;
}

// c_TScreen_Pairs.CheckSuccess.
static void check_pairs(SSRGame *game) {
    game->turn++;
    int first = 0, second = 0;
    for (int i = 0; i < 16; i++) {
        int k = game->iconOrder[i];
        if (!game->icons[k].picked) continue;
        if (!first) first = game->icons[k].imgId;
        else if (!second) second = game->icons[k].imgId;
    }
    if (first == second) {
        pairs_success(game, first);
        return;
    }
    if (game->turn == 2) {
        pairs_fail(game);
        return;
    }
    game->clicked = 0;
    for (int i = 0; i < 16; i++) game->icons[i].picked = 0;
    flip_cards_down(game);
}

// c_TScreen_Pairs.Update.
static void pairs_tick(SSRGame *game) {
    if (game->clicked == -2 && game->now > game->timeClicked + 2000) {
        game->clicked = -1;
        game->timeClicked = game->now;
        flip_cards_down(game);
    } else if (game->clicked == -1 && game->now > game->timeClicked + 1000) {
        game->clicked = 0;
        game->timeClicked = game->now;
        shuffle_cards(game);
    } else if (game->clicked == 2 && game->now > game->timeClicked + 1200) {
        check_pairs(game);
    } else if (game->clicked == 3 && game->now > game->timeClicked + 1000) {
        relationships_screen(game, 0, 0);
    }
}

// c_TScreen_Pairs.ClickCard.
static void click_card(SSRGame *game, int k) {
    if (game->clicked < 0 || game->clicked >= 2 || k < 0 || k > 15) return;
    int icon = game->iconOrder[k];
    if (game->icons[icon].picked) return;
    game->icons[icon].picked = 1;
    flip_card(game, k, 40, "-1");
    game->clicked++;
    if (game->clicked == 2) game->timeClicked = game->now;
}

#define CARD(n)                                                                                                        \
    static void card##n(SSRGame *game) {                                                                               \
        click_card(game, n);                                                                                           \
    }
CARD(0)
CARD(1)
CARD(2)
CARD(3)
CARD(4)
CARD(5)
CARD(6)
CARD(7)
CARD(8)
CARD(9)
CARD(10)
CARD(11)
CARD(12)
CARD(13)
CARD(14)
CARD(15)
#undef CARD

static void quit_career(SSRGame *game) {
    save_career(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 0);
    main_menu(game);
}

static const Route ROUTES[] = {
    {"gamemenu.btn_Life", relationships},
    {"relationships.btn_Home", relationships_home},
    {"relationships.btn_International", relationships_international},
    {"relationships.btn_Boss", activity_boss},
    {"relationships.btn_Team", activity_team},
    {"relationships.btn_Fans", activity_fans},
    {"relationships.btn_Girlfriend", activity_partner},
    {"relationships.btn_Sponsors", activity_sponsors},
    {"relationships.btn_DumpGirl", dump_partner},
    {"interview.btn_1", word1},
    {"interview.btn_2", word2},
    {"interview.btn_3", word3},
    {"interview.btn_4", word4},
    {"interview.btn_5", word5},
    {"interview.btn_6", word6},
    {"interview.btn_7", word7},
    {"interview.btn_8", word8},
    {"interview.btn_9", word9},
    {"interview.btn_10", word10},
    {"interview.btn_11", word11},
    {"interview.btn_12", word12},
    {"interview.btn_13", word13},
    {"interview.btn_14", word14},
    {"interview.btn_15", word15},
    {"interview.btn_ok", interview_ok},
    {"debug.interview", interview_screen},
    {"debug.dilemma", dilemma_screen},
    {"dilemma.btn_relationship1", dilemma_top},
    {"dilemma.btn_relationship2", dilemma_bottom},
    {"pairs.btn_0", card0},
    {"pairs.btn_1", card1},
    {"pairs.btn_2", card2},
    {"pairs.btn_3", card3},
    {"pairs.btn_4", card4},
    {"pairs.btn_5", card5},
    {"pairs.btn_6", card6},
    {"pairs.btn_7", card7},
    {"pairs.btn_8", card8},
    {"pairs.btn_9", card9},
    {"pairs.btn_10", card10},
    {"pairs.btn_11", card11},
    {"pairs.btn_12", card12},
    {"pairs.btn_13", card13},
    {"pairs.btn_14", card14},
    {"pairs.btn_15", card15},
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
    {"webpage.btn_Ok", webpage_ok},
    {"gamemenu.btn_Quit", quit_career},
    {"gamemenu.btn_Play", game_play},
    {"gamemenu.btn_League", game_league},
    {"leaguefixtures.btn_Play", leaguefixtures_play},
    {"leaguefixtures.btn_Back", league_home},
    {"league.btn_Continue", league_home},
    {"league.btn_Home", league_home},
    {"league.btn_League", fixtures_league},
    {"league.btn_Fixtures", league_fixtures_button},
    {"league.row_team", league_row},
    {"league.cmb_League", league_combo},
    {"seasonreview.btn_League", season_review_league},
    {"seasonreview.btn_Play", player_of_the_year},
    {"playeroftheyear.btn_Play", poty_play},
    {"match.btn_Play", match_play},
    {"leaguefixtures.btn_Kits", kits_button},
    {"match.btn_Time", match_speed},
    {"match.btn_WorkRate", match_work_rate},
    {"match.btn_Focus", match_focus},
    {"match.btn_MaximiseCommentary", maximise_commentary},
    {"match.btn_NRG", match_drink},
    {"matchstats.dismissanim", stats_dismiss},
    {"matchstats.btn_Play", stats_continue},
    {"matchreview.btn_Play", review_continue},
    {"skills.btn_Ratings", ratings_open},
    {"matchchance.btn_WatchReplay", watch_replay},
    {"matchchance.btn_SkipReplay", skip_replay},
    {"watchreplay.exit", skip_replay},
    {"ratings.btn_Back", ratings_back},
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
    free(game->replay);
    free(game->training);
    ssr_season_free(game->season);
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
        if (strcmp(key, "doneloseskills") == 0) game->settings.doneLoseSkills = value != 0;
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
    int ok = fprintf(file, "volume=%d\ndistance=%d\nreplays=%d\ndoneloseskills=%d\n", game->settings.volume,
                     game->settings.distance, game->settings.replays, game->settings.doneLoseSkills) > 0;
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
    // A row of a list sends its param after a space (c_GGadget param).
    char name[128];
    snprintf(name, sizeof(name), "%s", message);
    char *space = strchr(name, ' ');
    game->messageParam = space ? atoi(space + 1) : 0;
    if (space) *space = 0;
    for (size_t i = 0; i < sizeof(ROUTES) / sizeof(ROUTES[0]); i++) {
        if (strcmp(ROUTES[i].message, name) == 0) {
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

static void decision_tick(SSRGame *game);

void ssr_game_update(SSRGame *game, float frames) {
    if (!game) return;
    if (game->decisionEnd) {
        game->ticks += frames;
        while (game->ticks >= 1 && game->decisionEnd) {
            game->ticks -= 1;
            game->now += 16;
            decision_tick(game);
        }
        return;
    }
    if (game->match) {
        // The match runs in ticks of 60 a second, as the game does.
        game->ticks += frames;
        while (game->ticks >= 1 && game->match) {
            game->ticks -= 1;
            game->now += 16;
            match_tick(game);
        }
    } else if (game->training && strcmp(game->screen, "matchminigame") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && game->training) {
            game->ticks -= 1;
            game->now += 16;
            trial_tick(game);
        }
    } else if (strcmp(game->screen, "seasonreview") == 0 || strcmp(game->screen, "playeroftheyear") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            if (strcmp(game->screen, "playeroftheyear") == 0) poty_tick(game);
        }
    } else if (strcmp(game->screen, "interview") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "interview") == 0) {
            game->ticks -= 1;
            game->now += 16;
            interview_tick(game);
        }
    } else if (strcmp(game->screen, "dilemma") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "dilemma") == 0) {
            game->ticks -= 1;
            game->now += 16;
            dilemma_tick(game);
        }
    } else if (strcmp(game->screen, "pairs") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "pairs") == 0) {
            game->ticks -= 1;
            game->now += 16;
            pairs_tick(game);
        }
    } else if (strcmp(game->screen, "matchstats") == 0 || strcmp(game->screen, "matchreview") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            after_tick(game);
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

const char *ssr_game_overlay(const SSRGame *game) {
    return game ? game->overlay : "";
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

const SSRTraining *ssr_game_training(const SSRGame *game) {
    return game ? game->training : NULL;
}

const SSRChance *ssr_game_chance(const SSRGame *game) {
    if (!game) return NULL;
    if (game->training) return &game->training->chance;
    // The playback shows the copy of the frame (int)WatchReplay.Frame.
    if (game->replayMode == REPLAY_PLAY && game->replayCount > 0) {
        int i = (int)ssr_tweaks_number(game->tweaks, "WatchReplay", "Frame");
        i = i < 0 ? 0 : i >= game->replayCount ? game->replayCount - 1 : i;
        return &game->replay[i];
    }
    return game->chance;
}

int ssr_game_replaying(const SSRGame *game) {
    return game ? game->replayMode : 0;
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

int ssr_game_splash(const SSRGame *game) {
    return game && game->now < game->splashEnd;
}
