#include "ssr_game.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ssr_aftermatch.h"
#include "ssr_ball.h"
#include "ssr_blackjack.h"
#include "ssr_career.h"
#include "ssr_casino.h"
#include "ssr_contract.h"
#include "ssr_ex.h"
#include "ssr_fs.h"
#include "ssr_honours.h"
#include "ssr_horses.h"
#include "ssr_match.h"
#include "ssr_pack.h"
#include "ssr_people.h"
#include "ssr_random.h"
#include "ssr_season.h"
#include "ssr_shop.h"
#include "ssr_sort.h"
#include "ssr_training.h"

#define MAX_OPS 2048
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
    // 1 for a nation, else a club.
    int level;
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
    // The installed data packs, the ids on the packs screen (the first is
    // the world of the game), the row that is chosen there, 1 when the
    // player asks to install a pack, and 1 when the app can open a file.
    char packsDirectory[2048];
    char packIds[33][48];
    int packCount, packRow, packInstallAsked, canInstallPacks;
    // 1 after the player chose another pack: the app loads the world again.
    int packChanged;
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
    // The match type of the ball of the match (c_TFixture.GetBallType): 1
    // the league, 2 the cup, 3 the continent, 4 the nation.
    int matchBallType;
    // The two teams of the match, for the kits (c_TMatch.m_team1, m_team2),
    // and the kit choice of the kit button.
    Team homeTeam, awayTeam;
    int kitNum;
    // The competition of the table that shows, the row that the player
    // chose in it (0 none), and the team of the club fixtures screen.
    int leagueComp, leagueSelected;
    // The squad screen: the club, and the list: 0 the squad, 1 the top
    // scorers of the league, 2 the player and the rivals.
    int squadClub, squadMode;
    // The honours screen: 0 the world, 1 the timeline of the career, 2 the
    // hall of fame.
    int honoursMode;
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
    // The retirement screen: the stage, the time of the last stage, the
    // values that show, and the score.
    int retireStage, retireTime, retireScore;
    float retireStats[9];
    // The casino: the stake level and the bet, the stake buttons that are
    // off (c_TScreen_Casino), and the fields of TPlayer that a load does not
    // keep: the bets of the visit, the matches until the next catch, the
    // date of the trip with the team mates, and the bets of that trip (-1
    // with no trip).
    int stakeLevel, bet, stakeUpOff, stakeDownOff;
    int betCount, caughtGambling, teammatesCasino, teammatesBetCount;
    // The games seed their numbers from the clock.
    SSRRandom casinoRandom;
    SSRSlotReel reels[3];
    int slotsSpinning;
    SSRRoulette roulette;
    SSRRouletteBet rouletteBet;
    int rouletteSpinning;
    // Blackjack: the cards in their places on the screen, and Hit and Hold
    // off while a card slides.
    SSRBlackjack blackjack;
    int playerCardsDealt, dealerCardsDealt, blackjackHitOff;
    // The race horses and the text of their names. The stable: the horse
    // ids of the cards. The race: the frame of the track, 1 when the places
    // are final, 1 in the race of the own horse, the places of the horses
    // and the badges on the screen, the runner of the player, and the
    // commentary.
    // The players of the world, the names for new players, the seed of
    // the world of the next new career, and the last day that the people
    // counted.
    SSRPeople *people;
    SSRNames *names;
    int nextWorldSeed;
    int peopleDate;
    // The turn of the news of the world in the ticker.
    int worldTurn;
    // 1 while the player plays a match with an injury.
    int inPain;
    // The champions and the top scorers of the past seasons.
    SSRHonours honours;
    SSRHorses horses;
    char *horseNames;
    int horsesLoaded;
    int ownedHorses[SSR_STABLE_SLOTS], saleHorses[SSR_STABLE_SLOTS];
    float raceFrame;
    int standingsReady, ownRace, gallopNext;
    float horseX[SSR_RUNNERS], horseY[SSR_RUNNERS];
    int badgePlace[SSR_RUNNERS];
    int myHorse, commentaryNext, commentaryPrev;
    // The bux of a stable message.
    int messageCash;
    int slot;
    int slotExists[SSR_CAREER_SLOTS];

    // The player profile (c_TScreen_ChooseClub).
    char firstName[SSR_CAREER_NAME];
    char lastName[SSR_CAREER_NAME];
    int gender;
    int hardMode;
    // The custom career: the age at the start, and the skill that each of
    // the five skills has at the least at the start (0: the trials give
    // the skills).
    int customAge, customSkill;
    // The position, the first club (0: the game chooses it, or an index
    // from 1 into customClubs), and the index of the first Bux.
    int customPosition, customClub, customBux;
    int customClubs[512];
    int customClubCount;
    // 0 no name, 1 the first name, 2 the last name takes the keyboard.
    int typing;
    // The seed that the player types on the world screen.
    char seedText[16];
    // The name of a club that the player types in the editor.
    char editText[64];

    // The nationality (c_TScreen_ChooseNation): 1 when the Ok shows full.
    int canPlay;

    // The star transition: the screen after it, and its frames.
    char afterTransition[64];
    float transitionFrames;
    // The layout of the overlay over the screen, and of the modal overlay
    // over it, or "".
    char overlay[48];
    char modal[48];
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
    int recovery, payWage, payGoals, payAssists, paySponsors;
    float propertyBoost;
    // The boosts of the agent and of the trainer, and 1 after a club match.
    int agentWage, agentGoals, agentAssists, agentSponsors, trainerBoost, clubMatch;
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
    // The sounds that the logic asks for, by their names in Sounds.txt
    // (AudioManager.Play).
    const char *sounds[16];
    int soundCount;
    // The splash message ("Success!", "Fail!") shows until this time, in
    // its layout (TQuickMessage).
    int splashEnd;
    char splashLayout[32];
    // The alerts at the bottom of the screen (TQuickMessage.CreateAlert).
    // Each starts when the one before it ends. id counts the alerts.
    struct {
        char layout[40];
        char text[160];
        int start, finish, id;
    } alerts[32];
    int alertCount, alertIds;
    // The furthest goal, pass, headed goal, and headed pass of the last
    // match, in pixels.
    float matchFurthest[4];
    // The post hits, the goals from a corner, and the last minute winner
    // of the last match.
    int matchPostHits, matchCornerGoals, matchLastMinuteWinner;

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
    // 1 while a career plays (the player of the game is not blank).
    int hasPlayer;
    // 1 after the check of the trainer in this match week
    // (c_TPlayer.donetrainerwarning, not in the save).
    int doneTrainerWarning;

    // The interview after a match (c_TScreen_Interview): 1 once a match
    // has had one, the words to repeat, the buttons, the decoys, the
    // sequence (a decoy is below 0), the word of the sequence, the time of
    // the next step, the state, and the sentence so far.
    // The contract talks: the statics of TContractOffer, the offer on the
    // screen, the relation of the new boss of each club (75 at the start,
    // not in the save), and the negotiate game.
    SSRContractState contract;
    SSRContractOffer offer;
    int *newBossRel;
    int newBossRelCount;
    int shirts[6];
    int negTurn, lastClick, timeClickedNeg, clickDelay, negotiateAccepted, proceedAt;
    // The desired transfer at the end of a season: the nation, the
    // division, and the club that the player chose.
    int desiredNation, desiredDivision, desiredClub;
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

// AudioManager.Play: the app plays the sound.
static void play_sound(SSRGame *game, const char *name) {
    if (game->soundCount < (int)(sizeof(game->sounds) / sizeof(game->sounds[0]))) {
        game->sounds[game->soundCount++] = name;
    }
}

// TQuickMessage.Create: the text of Menu.QuickMessage shows in the layout
// for ms.
static void show_splash(SSRGame *game, const char *layout, int ms) {
    snprintf(game->splashLayout, sizeof(game->splashLayout), "%s", layout);
    game->splashEnd = game->now + ms;
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

// bb_various.MyToUpper: the text in capitals in English. The other
// languages keep their text.
static void upper(const SSRGame *game, char *text) {
    if (game->settings.language > 0) return;
    for (; *text; text++) *text = (char)toupper((unsigned char)*text);
}

static void show_splash(SSRGame *game, const char *layout, int ms);
static void award(SSRGame *game, int id);

// A banner (TQuickMessage.InitBanner): the text goes to the tweak
// QuickMessage with MyToUpper, and the layout "splashmessage" shows it.
static void quick_message(SSRGame *game, const char *text, int ms) {
    char banner[256];
    snprintf(banner, sizeof(banner), "%s", text);
    upper(game, banner);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", banner);
    show_splash(game, "splashmessage", ms);
}

// TQuickMessage.CreateAlert: the text in capitals in the layout, for ms,
// after the alerts before it. A full queue drops the alert.
static void alert(SSRGame *game, const char *layout, const char *text, int ms) {
    if (game->alertCount >= (int)(sizeof(game->alerts) / sizeof(game->alerts[0]))) return;
    int start = game->now;
    for (int i = 0; i < game->alertCount; i++) {
        if (game->alerts[i].finish > start) start = game->alerts[i].finish;
    }
    int at = game->alertCount++;
    snprintf(game->alerts[at].layout, sizeof(game->alerts[at].layout), "%s", layout);
    snprintf(game->alerts[at].text, sizeof(game->alerts[at].text), "%s", text);
    upper(game, game->alerts[at].text);
    game->alerts[at].start = start;
    game->alerts[at].finish = start + ms;
    game->alerts[at].id = ++game->alertIds;
}

// message.dismiss: TQuickMessage.Dismiss. All the alerts move back, so that
// the first one ends now and the next one starts.
static void alert_dismiss(SSRGame *game) {
    if (game->alertCount == 0) return;
    int shift = game->alerts[0].finish - game->now;
    for (int i = 0; i < game->alertCount; i++) {
        game->alerts[i].start -= shift;
        game->alerts[i].finish -= shift;
    }
}

/* -------------------------------------------------------------- settings */

// The tweaks that the options screen reads (c_TScreen_Options.Update*).
static void set_option_tweaks(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsVolume", game->settings.volume);
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsDistance", game->settings.distance);
    ssr_tweaks_set_number(game->tweaks, "Menu", "SettingsReplays", game->settings.replays);
    if (game->settings.language >= 0) ssr_tweaks_set_number(game->tweaks, "Menu", "Language", game->settings.language);
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
    // A message over a message goes back to the screen under them both.
    if (strncmp(game->screen, "message", 7) != 0) {
        snprintf(game->messageReturn, sizeof(game->messageReturn), "%s", game->screen);
    }
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

/* --------------------------------------------------------- the packs */

// The packs screen: the world of the game first, then each installed
// pack. The row of the chosen pack shows its highlight.
static void packs_fill(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    char ids[32][48];
    int n = game->packsDirectory[0] ? ssr_pack_list(game->packsDirectory, ids, 32) : 0;
    game->packCount = n + 1;
    game->packIds[0][0] = 0;
    for (int i = 0; i < n; i++) snprintf(game->packIds[i + 1], 48, "%s", ids[i]);
    game->packRow = 0;
    add_op(game, SSR_OP_LIST_CLEAR, "PackList", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_START, "PackList", NULL, NULL, 82);
    for (int i = 0; i < game->packCount; i++) {
        char name[128], details[256];
        SSRPackInfo info;
        char directory[2200];
        snprintf(directory, sizeof(directory), "%s/%s", game->packsDirectory, game->packIds[i]);
        if (i == 0 || !ssr_pack_info(directory, &info)) {
            snprintf(name, sizeof(name), "%s", loc(game, "SSR_GameWorld"));
            snprintf(details, sizeof(details), "New Star Soccer %s", SSR_WORLD_DEFAULT_SET);
        } else {
            snprintf(name, sizeof(name), "%s", info.name);
            snprintf(details, sizeof(details), "%s", loc(game, "SSR_PackBy"));
            replace_all(details, sizeof(details), "$author", info.author);
            replace_all(details, sizeof(details), "$version", info.version);
        }
        add_op(game, SSR_OP_LIST_ADD, "PackList", "PackEntry", "", 0);
        add_op(game, SSR_OP_LIST_TEXT, "PackList", NULL, name, 1);
        add_op(game, SSR_OP_LIST_TEXT, "PackList", NULL, details, 2);
        if (strcmp(game->packIds[i], game->settings.pack) == 0) {
            add_op(game, SSR_OP_LIST_SUB, "PackList", "Highlight", NULL, 1);
            game->packRow = i;
        }
        add_op(game, SSR_OP_LIST_PARAM, "PackList", NULL, NULL, i);
    }
    ssr_tweaks_set(t, "Packs", "Title", loc(game, "SSR_World"));
    ssr_tweaks_set(t, "Packs", "SeedLabel", loc(game, "SSR_SeedLabel"));
    ssr_tweaks_set(t, "Packs", "ContinueLabel", loc(game, "SSR_ContinueLabel"));
    ssr_tweaks_set_number(t, "Packs", "CanInstall", game->canInstallPacks);
    ssr_tweaks_set_number(t, "Packs", "CanRemove", game->packRow > 0);
    ssr_tweaks_set_number(t, "Packs", "Scroll", 0);
}

static void packs_screen(SSRGame *game) {
    set_screen(game, "packs");
    packs_fill(game);
}

// packs.row_pack: the world reads the pack of the row. The app loads the
// world again.
static void packs_row(SSRGame *game) {
    int row = game->messageParam;
    if (row < 0 || row >= game->packCount || row == game->packRow) return;
    ssr_game_set_pack(game, game->packIds[row]);
    settings_changed(game);
    game->packChanged = 1;
    packs_fill(game);
}

static void packs_remove(SSRGame *game) {
    if (game->packRow <= 0) return;
    ssr_pack_remove(game->packsDirectory, game->packIds[game->packRow]);
    ssr_game_set_pack(game, "");
    settings_changed(game);
    game->packChanged = 1;
    packs_fill(game);
}

static void seed_done(SSRGame *game);
static void options(SSRGame *game);

static void packs_back(SSRGame *game) {
    if (game->typing == 3) seed_done(game);
    options(game);
}

static void packs_install(SSRGame *game) {
    game->packInstallAsked = 1;
}

// packs.inp_Seed: the player types the seed of the next world. Enter or a
// tap elsewhere keeps it. An empty seed gives a new world each career.
static void seed_start(SSRGame *game) {
    if (game->typing == 3) {
        seed_done(game);
        return;
    }
    game->typing = 3;
    game->seedText[0] = 0;
}

static void seed_done(SSRGame *game) {
    game->typing = 0;
    game->settings.nextWorldSeed = atoi(game->seedText);
    settings_changed(game);
}

static void continue_next(SSRGame *game) {
    if (game->typing == 3) seed_done(game);
    game->settings.continueWorld = !game->settings.continueWorld;
    settings_changed(game);
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

// c_TScreen_Options.ButtonLanguage. The screen shows the language of the
// tweak Menu.Language.
static void language_screen(SSRGame *game) {
    set_option_tweaks(game);
    set_screen(game, "language");
}

// c_TScreen_Language.ButtonOk and SetLanguage: the same language goes back
// to the options. A new language loads, and the main menu opens.
static void language_ok(SSRGame *game) {
    int language = (int)ssr_tweaks_number(game->tweaks, "Menu", "Language");
    if (language < 0 || language > 12) language = 0;
    if (language == game->settings.language) {
        options(game);
        return;
    }
    game->settings.language = language;
    settings_changed(game);
    main_menu(game);
}

/* ------------------------------------------------------------ save slots */

static void slot_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/career%d.txt", game->saveDirectory, slot + 1);
}

static void season_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/season%d.txt", game->saveDirectory, slot + 1);
}

static SSRPerson *mentee(SSRGame *game);
static void edit_done(SSRGame *game);

static void people_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/people%d.txt", game->saveDirectory, slot + 1);
}

static void honours_path(const SSRGame *game, int slot, char *out, size_t capacity) {
    snprintf(out, capacity, "%s/honours%d.txt", game->saveDirectory, slot + 1);
}

// A fixture of a club league or cup counts for the players of its clubs.
// A club with no squad gets one. The goals of the player of the user do
// not go to the people.
static void people_played(void *context, SSRSeason *s, const SSRSeasonComp *comp, const SSRFixture *f) {
    SSRGame *game = context;
    if (!game->people || !game->names || !game->world || comp->data->level != 0) return;
    if (f->sdate > game->peopleDate) {
        int days = f->sdate - game->peopleDate;
        ssr_people_pass_days(game->people, days);
        SSRCareer *c = &game->career;
        if (c->injury > 0 && c->injury <= days) {
            // The comeback of the player.
            alert(game, "bottommessagemoment", loc(game, "SSR_MomentFit"), 4000);
            ssr_career_add_moment(c, f->sdate, loc(game, "SSR_MomentFit"));
        }
        c->injury = c->injury > days ? c->injury - days : 0;
        game->peopleDate = f->sdate;
    }
    int teams[2] = {ssr_fixture_home_team(s, f), ssr_fixture_away_team(s, f)};
    int goals[2] = {f->score1, f->score2};
    int mine = f->compId == game->fixtureComp && f->sdate == game->fixtureDate;
    for (int k = 0; k < 2; k++) {
        if (!teams[k]) continue;
        ssr_people_make_squad(game->people, game->world, game->names, teams[k], ssr_date_year(f->sdate));
        int g = goals[k];
        if (mine && teams[k] == game->career.clubId) g = g > game->facts.goals ? g - game->facts.goals : 0;
        ssr_people_play(game->people, teams[k], g);
    }
    // A cup final makes the rivalry of the two clubs grow.
    if (teams[0] && teams[1] &&
        (ssr_season_won_final(s, comp, f, teams[0]) || ssr_season_won_final(s, comp, f, teams[1]))) {
        ssr_honours_add_rivalry(&game->honours, teams[0], teams[1], SSR_RIVALRY_FINAL);
    }
}

// The people of a new world: the squad of the club of the player, and
// three rivals of the same age in the league.
static void people_start(SSRGame *game) {
    SSRCareer *c = &game->career;
    ssr_people_free(game->people);
    if (c->worldSeed == 0) c->worldSeed = game->nextWorldSeed ? game->nextWorldSeed : 1;
    game->people = ssr_people_create(c->worldSeed);
    game->peopleDate = c->sdate;
    if (!game->people || !game->names || !game->world) return;
    int year = ssr_career_year(c);
    ssr_people_make_squad(game->people, game->world, game->names, c->clubId, year);
    int league = game->season ? ssr_season_actual_league(game->season, c->clubId) : 0;
    if (!league) {
        const SSRClub *club = ssr_world_club(game->world, c->clubId);
        league = club ? club->league : 0;
    }
    ssr_people_make_rivals(game->people, game->world, game->names, league, c->nationId, year - ssr_career_age(c), 3,
                           year);
}

// A new career continues the world of the last career: the players, the
// managers, the honours, and the rivalries of the career of another slot
// with the most years. The old player stays in the world as a legend, and
// the years of the world move back to the year of the new career. Returns
// 0 when no other slot has a world.
static int continue_world(SSRGame *game) {
    SSRCareer *c = &game->career;
    int best = -1, bestYear = 0;
    static SSRCareer old;
    char path[2200];
    for (int slot = 0; slot < SSR_CAREER_SLOTS; slot++) {
        if (slot == game->slot) continue;
        slot_path(game, slot, path, sizeof(path));
        if (!ssr_career_load(&old, path)) continue;
        people_path(game, slot, path, sizeof(path));
        FILE *file = fopen(path, "rb");
        if (!file) continue;
        fclose(file);
        if (ssr_career_year(&old) > bestYear) {
            bestYear = ssr_career_year(&old);
            best = slot;
        }
    }
    if (best < 0 || !game->world || !game->names) return 0;
    slot_path(game, best, path, sizeof(path));
    ssr_career_load(&old, path);
    people_path(game, best, path, sizeof(path));
    SSRPeople *people = ssr_people_load(path);
    if (!people) return 0;
    honours_path(game, best, path, sizeof(path));
    ssr_honours_load(&game->honours, path);
    int oldYear = ssr_career_year(&old);
    ssr_people_add_legend(people, old.firstName, old.lastName, old.nationId, oldYear - ssr_career_age(&old), oldYear,
                          old.apps, old.goals, old.assists);
    int shift = people->year - ssr_career_year(c);
    ssr_people_rebase(people, shift);
    for (int i = 0; i < game->honours.count; i++) game->honours.list[i].year -= shift;
    ssr_people_free(game->people);
    game->people = people;
    game->peopleDate = c->sdate;
    c->worldSeed = people->seed;
    // The club of the new player gets its squad, and new rivals start.
    int year = ssr_career_year(c);
    ssr_people_make_squad(people, game->world, game->names, c->clubId, year);
    int league = game->season ? ssr_season_actual_league(game->season, c->clubId) : 0;
    ssr_people_make_rivals(people, game->world, game->names, league, c->nationId, year - ssr_career_age(c), 3, year);
    char text[256];
    snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentLegend"));
    replace_all(text, sizeof(text), "$name", old.fullName);
    ssr_career_add_moment(c, c->sdate, text);
    return 1;
}

static void people_load(SSRGame *game, int slot) {
    char path[2200];
    people_path(game, slot, path, sizeof(path));
    ssr_people_free(game->people);
    game->people = game->saveDirectory[0] ? ssr_people_load(path) : NULL;
    game->peopleDate = game->career.sdate;
    if (!game->people) people_start(game);
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
    if (game->season) {
        game->season->played = people_played;
        game->season->playedContext = game;
    }
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
static void casino_new_player(SSRGame *game);
static void horses_for_career(SSRGame *game);

static void open_slot(SSRGame *game, int slot) {
    game->slot = slot;
    // The game keeps no flag of the check of the trainer in the save.
    game->doneTrainerWarning = 0;
    char path[2200];
    slot_path(game, slot, path, sizeof(path));
    if (game->slotExists[slot] && ssr_career_load(&game->career, path)) {
        game->hasPlayer = 1;
        casino_new_player(game);
        horses_for_career(game);
        new_season_state(game);
        season_path(game, slot, path, sizeof(path));
        if (game->season && !ssr_season_load(game->season, path)) {
            // A career from before the seasons gets its competitions now.
            ssr_season_set_up(game->season);
            skip_to_first_match(game);
        }
        people_load(game, slot);
        honours_path(game, slot, path, sizeof(path));
        ssr_honours_load(&game->honours, path);
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
        people_path(game, game->slot, path, sizeof(path));
        remove(path);
        honours_path(game, game->slot, path, sizeof(path));
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
    ssr_tweaks_set_number(game->tweaks, "Menu", "HardmodeUnlocked", game->settings.hardModeUnlocked);
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
        upper(game, name);
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
        upper(game, name);
        add_op(game, SSR_OP_LIST_ADD, "ContinentCombo", "ContinentTemplate", name, c->id);
    }
    int count = sorted_nations(world);
    for (int i = 0; i < count; i++) {
        const SSRNation *n = &world->nations[nation_order[i]];
        if (!nation_has_league(world, n->id)) continue;
        char name[128];
        snprintf(name, sizeof(name), "%s", n->name);
        upper(game, name);
        add_op(game, SSR_OP_LIST_ADD, "LeagueCombo", "LeagueTemplate", name, n->id);
    }
    // The default nation of the language (GetDefaultNation): the tag
    // Tag_NationId, or England.
    int chosen = atoi(loc(game, "Tag_NationId"));
    if (chosen < 1 || chosen > 209) chosen = DEFAULT_NATION;
    const SSRNation *nation = ssr_world_nation(world, chosen);
    int continent = nation ? nation->continent : 6;
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredContinent", continent);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredNation", chosen);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredLeague", chosen);
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
// The clubs with a league for GetProspectiveClubs, the weakest first. The
// clubs of the nation get the strengths of AverageOutStrengthAll. The
// caller frees the list.
static Prospect *prospect_list(SSRGame *game, int nation, int *outCount) {
    const SSRWorld *world = game->world;
    Prospect *list = malloc((size_t)world->clubCount * sizeof(Prospect));
    *outCount = 0;
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
    *outCount = count;
    return list;
}

static const SSRClub *first_club(SSRGame *game, int nation) {
    const SSRWorld *world = game->world;
    int count = 0;
    Prospect *list = prospect_list(game, nation, &count);
    if (!list) return NULL;

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

// The custom career (a screen of the remake): the age and the skills at
// the start. The first values are a career of the game: 16, and the skills
// of the trials.
#define CUSTOM_AGE_MIN 16
#define CUSTOM_AGE_MAX 32
#define CUSTOM_SKILL_STEP 10
#define CUSTOM_SKILL_MAX 70

// The Bux at the start of a custom career. The first is the Bux of 4.32.
static const int CUSTOM_BUX[4] = {15, 100, 500, 2000};

static int stronger_club(const void *a, const void *b) {
    const SSRClub *x = *(const SSRClub *const *)a, *y = *(const SSRClub *const *)b;
    if (x->strength != y->strength) return y->strength - x->strength;
    return strcmp(x->name, y->name);
}

static int league_duration(const SSRWorld *world, int league);

static void nation_ok(SSRGame *game) {
    if (!game->canPlay || !game->world) return;
    game->customAge = CUSTOM_AGE_MIN;
    game->customSkill = 0;
    game->customPosition = SSR_CAREER_FORWARD;
    game->customClub = 0;
    game->customBux = 0;
    // The clubs of the leagues of the nation, the strongest first.
    const SSRWorld *world = game->world;
    int nation = chosen(game, "ChosenDesiredLeague");
    const SSRClub *clubs[512];
    int n = 0;
    for (int i = 0; i < world->clubCount && n < 512; i++) {
        const SSRClub *c = &world->clubs[i];
        if (c->nation == nation && c->league > 0 && c->bTeamOf == 0 && league_duration(world, c->league) > 0) clubs[n++] = c;
    }
    qsort(clubs, (size_t)n, sizeof(clubs[0]), stronger_club);
    for (int i = 0; i < n; i++) game->customClubs[i] = clubs[i]->id;
    game->customClubCount = n;
    set_screen(game, "customcareer");
    ssr_tweaks_set(game->tweaks, "Custom", "Title", loc(game, "SSR_CustomTitle"));
    ssr_tweaks_set(game->tweaks, "Custom", "AgeLabel", loc(game, "SSR_CustomAge"));
    ssr_tweaks_set(game->tweaks, "Custom", "SkillLabel", loc(game, "SSR_CustomSkill"));
    ssr_tweaks_set(game->tweaks, "Custom", "PositionLabel", loc(game, "SSR_CustomPosition"));
    ssr_tweaks_set(game->tweaks, "Custom", "ClubLabel", loc(game, "SSR_CustomClub"));
    ssr_tweaks_set(game->tweaks, "Custom", "BuxLabel", loc(game, "SSR_CustomBux"));
}

static void custom_age(SSRGame *game) {
    game->customAge = game->customAge >= CUSTOM_AGE_MAX ? CUSTOM_AGE_MIN : game->customAge + 1;
}

static void custom_age_down(SSRGame *game) {
    game->customAge = game->customAge <= CUSTOM_AGE_MIN ? CUSTOM_AGE_MAX : game->customAge - 1;
}

static void custom_skill(SSRGame *game) {
    game->customSkill = game->customSkill >= CUSTOM_SKILL_MAX ? 0 : game->customSkill + CUSTOM_SKILL_STEP;
}

static void custom_skill_down(SSRGame *game) {
    game->customSkill = game->customSkill <= 0 ? CUSTOM_SKILL_MAX : game->customSkill - CUSTOM_SKILL_STEP;
}

static void custom_position(SSRGame *game) {
    game->customPosition = (game->customPosition + 1) % SSR_CAREER_POSITIONS;
}

static void custom_position_down(SSRGame *game) {
    game->customPosition = (game->customPosition + SSR_CAREER_POSITIONS - 1) % SSR_CAREER_POSITIONS;
}

static void custom_club(SSRGame *game) {
    game->customClub = (game->customClub + 1) % (game->customClubCount + 1);
}

static void custom_club_down(SSRGame *game) {
    game->customClub = (game->customClub + game->customClubCount) % (game->customClubCount + 1);
}

static void custom_bux(SSRGame *game) {
    game->customBux = (game->customBux + 1) % 4;
}

static void custom_bux_down(SSRGame *game) {
    game->customBux = (game->customBux + 3) % 4;
}

static void custom_ok(SSRGame *game) {
    if (!game->canPlay || !game->world) return;
    int league = chosen(game, "ChosenDesiredLeague");
    int wage = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingWage");
    int goalBonus = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingGoalBonus");
    int assistBonus = (int)ssr_tweaks_number(game->tweaks, "Player", "StartingAssistBonus");
    ssr_career_create(&game->career, game->firstName, game->lastName, game->gender, game->hardMode, league, wage,
                      goalBonus, assistBonus);
    game->hasPlayer = 1;
    casino_new_player(game);
    horses_for_career(game);
    game->career.nationId = chosen(game, "ChosenDesiredNation");
    game->career.startAge = game->customAge > CUSTOM_AGE_MIN ? game->customAge : 0;
    game->career.position = game->customPosition;
    game->career.bank = CUSTOM_BUX[game->customBux % 4];
    // LoadDataAndStartTrial: the game chooses the club. A custom career can
    // choose it.
    const SSRClub *club = first_club(game, league);
    if (game->customClub > 0 && game->customClub <= game->customClubCount) {
        const SSRClub *own = ssr_world_club(game->world, game->customClubs[game->customClub - 1]);
        if (own) club = own;
    }
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
        // The rows of the days of training: pace, power, technique,
        // vision, and set pieces.
        static const int ROWS[5] = {2, 4, 1, 0, 3};
        int skill = ssr_training_skill(t->training);
        if (skill >= 0) {
            skills[skill][0] = t->lives;
            c->lastTrained[ROWS[skill]] = c->deplete[ROWS[skill]] = c->sdate;
        }
    }
    free(game->training);
    game->training = NULL;
    char text[1024];
    if (trial >= 5) {
        // The end of the trials gives an award.
        award(game, 1);
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
            upper(game, text);
            ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
            show_splash(game, "splashmessage", t->messageEnd - game->now);
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

// GSPlayerUtility.UpdateRelationship: the relation changes, then a club
// relation, the sponsors, the partner, or the happiness at 100 unlocks its
// award.
static void update_relation(SSRGame *game, SSRRelation relation, float change) {
    SSRCareer *c = &game->career;
    ssr_career_update_relation(c, relation, change);
    if (ssr_career_relation(c, SSR_RELATION_BOSS) >= 100) award(game, 44);
    if (ssr_career_relation(c, SSR_RELATION_TEAM) >= 100) award(game, 45);
    if (ssr_career_relation(c, SSR_RELATION_FANS) >= 100) award(game, 46);
    if (ssr_career_relation(c, SSR_RELATION_SPONSORS) >= 100) award(game, 47);
    if (c->relationPartner >= 100) award(game, 48);
    if (ssr_career_happiness(c) >= 100) award(game, 49);
}

// c_TPlayer.CheckSkillAchievements, at each save: a skill at 60 with no
// boost of the boots, and all five.
static void skill_awards(SSRGame *game) {
    const SSRCareer *c = &game->career;
    int skills[5] = {ssr_career_skill(c->pace), ssr_career_skill(c->power), ssr_career_skill(c->technique),
                     ssr_career_skill(c->vision), ssr_career_skill(c->setPieces)};
    int all = 1;
    for (int i = 0; i < 5; i++) {
        if (skills[i] >= 60) award(game, 37 + i);
        else all = 0;
    }
    if (all) award(game, 42);
}

static void save_career(SSRGame *game) {
    skill_awards(game);
    if (game->horsesLoaded) ssr_horses_owned_text(&game->horses, game->career.horses, sizeof(game->career.horses));
    if (!game->saveDirectory[0]) return;
    char path[2200];
    slot_path(game, game->slot, path, sizeof(path));
    ssr_career_save(&game->career, path);
    if (game->season) {
        season_path(game, game->slot, path, sizeof(path));
        ssr_season_save(game->season, path);
    }
    if (game->people) {
        people_path(game, game->slot, path, sizeof(path));
        ssr_people_save(game->people, path);
    }
    honours_path(game, game->slot, path, sizeof(path));
    ssr_honours_save(&game->honours, path);
}

static void webpage(SSRGame *game);
static void signed_moment(SSRGame *game);

// StartCareer: the first skills, the first save, and the news.
static void call_start_career(SSRGame *game) {
    SSRCareer *c = &game->career;
    int *skills[5] = {c->technique, c->pace, c->vision, c->setPieces, c->power};
    for (int i = 0; i < 5; i++) {
        if (skills[i][0] == 0) skills[i][0] = 1;
        // A custom career trains the first levels of each skill.
        for (int level = 0; level < game->customSkill && level < SSR_CAREER_SKILL_LEVELS; level++) {
            if (skills[i][level] == 0) skills[i][level] = 1;
        }
    }
    game->customSkill = 0;
    skip_to_first_match(game);
    c->worldSeed = game->settings.nextWorldSeed;
    ssr_honours_clear(&game->honours);
    if (!(game->settings.continueWorld && continue_world(game))) people_start(game);
    signed_moment(game);
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
static void call_play_with_pain(SSRGame *game);
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
static void call_buy_property(SSRGame *game);
static void call_old_pro_boots(SSRGame *game);
static void call_sponsorship_offer(SSRGame *game);
static void call_change_staff(SSRGame *game);
static void call_change_boots(SSRGame *game);
static void call_reopen_staff(SSRGame *game);
static void call_agent_get(SSRGame *game);
static void call_negotiations_cancelled(SSRGame *game);
static void call_negotiation_accepted(SSRGame *game);
static void call_sign_new_contract(SSRGame *game);
static void call_sign_for_new_club(SSRGame *game);
static void call_confirm_transfer(SSRGame *game);
static void call_new_season(SSRGame *game);
static void call_new_season_same_club(SSRGame *game);
static void call_trainer_get(SSRGame *game);
static void call_boots_expired(SSRGame *game);
static void call_dump_partner(SSRGame *game);
static void call_retirement(SSRGame *game);
static void call_caught_gambling(SSRGame *game);
static void call_teammates_casino(SSRGame *game);
static void call_teammates_shopping(SSRGame *game);
static void call_racing_return(SSRGame *game);
static void call_train_horse(SSRGame *game);
static void call_sell_horse(SSRGame *game);

static void run_call(SSRGame *game, const char *name) {
    static const struct {
        const char *name;
        CallFunction call;
    } CALLS[] = {
        {"DeleteCareer", call_delete_career},
        {"PlayWithPain", call_play_with_pain},
        {"TrialDone", call_trial_done},
        {"DoTrial1", call_do_trial1},
        {"DoTrial2", call_do_trial2},
        {"DoTrial3", call_do_trial3},
        {"DoTrial4", call_do_trial4},
        {"DoTrial5", call_do_trial5},
        {"StartCareer", call_start_career},
        {"DrinkNRG", call_drink},
        {"SeasonEndTransfer", call_season_end_transfer},
        {"Retirement", call_retirement},
        {"CaughtGambling", call_caught_gambling},
        {"TeamMatesCasino", call_teammates_casino},
        {"TeamMatesShopping", call_teammates_shopping},
        {"TScreen_Racing.Return", call_racing_return},
        {"TrainHorse", call_train_horse},
        {"SellHorse", call_sell_horse},
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
        {"BuyProperty", call_buy_property},
        {"OldProBoots", call_old_pro_boots},
        {"SponsorshipOffer", call_sponsorship_offer},
        {"ChangeSoftAgentOrTrainer", call_change_staff},
        {"ChangeSoftBat", call_change_boots},
        {"ReopenAgentTrainerShop", call_reopen_staff},
        {"AgentGet", call_agent_get},
        {"NegotiationsCancelled", call_negotiations_cancelled},
        {"NegotiationAccepted", call_negotiation_accepted},
        {"SignNewContract", call_sign_new_contract},
        {"SignForNewClub", call_sign_for_new_club},
        {"ConfirmSeasonEndTransfer", call_confirm_transfer},
        {"NewSeasonNoClubsInterested", call_new_season},
        {"NewSeasonWithSameClub", call_new_season_same_club},
        {"TrainerGet", call_trainer_get},
        {"BootsExpired", call_boots_expired},
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

// c_TPlayer.GetSkills: the sum of the five skills / 3.
static float career_skills(const SSRCareer *c) {
    return (ssr_career_skill(c->pace) + ssr_career_skill(c->power) + ssr_career_skill(c->technique) +
            ssr_career_skill(c->vision) + ssr_career_skill(c->setPieces)) /
           3.0f;
}

// The tweaks of the game menu (c_TScreen_GameMenu.UpdatePanels and
// bb_tweakinterface.UpdatePlayerBasicTweaks).
static void player_tweaks(SSRGame *game) {
    const SSRCareer *c = &game->career;
    SSRTweaks *t = game->tweaks;
    char text[64];
    // The awards that the achievements screen shows as unlocked.
    for (int id = 1; id < SSR_CAREER_AWARDS; id++) {
        snprintf(text, sizeof(text), "%d", id);
        if (c->awards[id] > 0 || ssr_tweaks_kind(t, "Achievements", text) != SSR_TWEAK_NONE) {
            ssr_tweaks_set_number(t, "Achievements", text, c->awards[id] > 0);
        }
    }
    ssr_tweaks_set_number(t, "Player", "Gender", c->gender);
    ssr_tweaks_set_number(t, "Player", "Bux", c->bank);
    ssr_tweaks_set_number(t, "Player", "StarBux", c->bank);
    ssr_tweaks_set_number(t, "Player", "Age", ssr_career_age(c));
    ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
    // UpdatePlayerBasicTweaks.
    ssr_tweaks_set_number(t, "Player", "Happiness", ssr_career_happiness(c));
    ssr_tweaks_set_number(t, "Player", "Lifestyle", ssr_career_lifestyle(c));
    ssr_tweaks_set_number(t, "Player", "Skills", career_skills(c));
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

static int agent_hint(SSRGame *game);

// c_TPlayer.CheckAgentAndTrainer: a message when a contract runs out.
// Returns 1 when a message shows.
static int check_trainer_warning(SSRGame *game);

static int check_staff(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->retired) return 0;
    int agent = ssr_career_agent(c), trainer = ssr_career_trainer(c);
    int agentLeft = agent ? c->agent[agent - 1] : 0, trainerLeft = trainer ? c->trainer[trainer - 1] : 0;
    if (agentLeft == 1 && trainerLeft == 1) {
        for (int i = 0; i < 3; i++) c->agent[i] = c->trainer[i] = 0;
        game->messageData = agent;
        message(game, "message", loc(game, "CMESSAGE_AGENTANDTRAINEREXPIRED"), 1, 1, "AgentGet");
        return 1;
    }
    for (int i = 0; i < 3; i++) {
        if (c->agent[i] != 1) continue;
        c->agent[i] = 0;
        c->expiredBoots = -1;
        game->messageData = agent;
        message(game, "messageagent", loc(game, "CMESSAGE_AGENTEXPIRED"), 1, 1, "AgentGet");
        return 1;
    }
    for (int i = 0; i < 3; i++) {
        if (c->trainer[i] != 1) continue;
        c->trainer[i] = 0;
        c->expiredBoots = -1;
        game->messageData = trainer;
        message(game, "messagetrainer", loc(game, "CMESSAGE_TRAINEREXPIRED"), 1, 1, "TrainerGet");
        return 1;
    }
    if (agent_hint(game)) return 1;
    if (game->doneTrainerWarning) return 0;
    game->doneTrainerWarning = 1;
    return check_trainer_warning(game);
}

// GSBootsRenewMessage: the boots that ran out ask for a new pair.
static int check_boots(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRBoot *boot = ssr_shop_boot(c->expiredBoots);
    if (!boot) return 0;
    c->expiredBoots = -1;
    char text[512], name[64];
    snprintf(text, sizeof(text), "%s", loc(game, "TRY_BEFORE_BOOTS"));
    snprintf(name, sizeof(name), "Title_%s", boot->name);
    replace_all(text, sizeof(text), "$name", loc(game, name));
    message(game, "message", text, 1, 1, "BootsExpired");
    return 1;
}

static void game_menu(SSRGame *game) {
    // GameMenu.SetUpScreen: a headline that is left shows first.
    if (game->career.headline[0]) {
        webpage_to(game, "gamemenu", 0);
        return;
    }
    set_screen(game, "gamemenu");
    ssr_tweaks_set_number(game->tweaks, "Menu", "TabPos", 0);
    player_tweaks(game);
    if (!check_staff(game)) check_boots(game);
}

// GSGetPace, GetPower, and GetTechnique with the boots: the skill and the
// boost of the boots that the player wears, from 1 to 100. which is 0
// pace, 1 power, 2 technique.
static int boot_skill(const SSRCareer *c, int which) {
    const int *levels = which == 0 ? c->pace : which == 1 ? c->power : c->technique;
    int sum = 0;
    for (int i = 0; i < SSR_CAREER_SKILL_LEVELS; i++) sum += levels[i];
    const SSRBoot *boot = c->bootId > 0 ? ssr_shop_boot(c->bootId) : NULL;
    if (boot) sum += which == 0 ? boot->pace : which == 1 ? boot->power : boot->technique;
    return sum < 1 ? 1 : sum > 100 ? 100 : sum;
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

// The crest of a club in the chosen pack (crests/<id>.png) takes the place
// of the face of the shield of the team in the badge slot (1 or 2). The
// designs of the face go, and the face is white, so that the crest keeps
// its colours. A shield of slot 1 is a flipped image, so it gets the
// flipped crest.
static void team_crest(SSRGame *game, int slot, int level, int id) {
    char path[4096], name[48], image[48];
    int has = 0;
    if (level != 1 && game->packsDirectory[0] && game->settings.pack[0]) {
        snprintf(path, sizeof(path), "%s/%s/crests/%d.png", game->packsDirectory, game->settings.pack, id);
        FILE *file = fopen(path, "rb");
        has = file != NULL;
        if (file) fclose(file);
    }
    snprintf(name, sizeof(name), "HasCrest%d", slot);
    ssr_tweaks_set_number(game->tweaks, "MatchColours", name, has);
    snprintf(name, sizeof(name), "Crest%d", slot);
    if (has) {
        snprintf(image, sizeof(image), "%s%d", slot == 1 ? "CrestM_" : "Crest_", id);
    } else {
        snprintf(image, sizeof(image), "MapShieldColourGrey");
    }
    ssr_tweaks_set(game->tweaks, "MatchColours", name, image);
    if (has) {
        snprintf(name, sizeof(name), "Badge%dCol1", slot);
        set_colour_tweak(game, name, rgb(0xFFFFFF), 1);
    }
}

static int team_of(SSRGame *game, int level, int id, int strength, Team *team) {
    memset(team, 0, sizeof(*team));
    team->id = id;
    team->strength = strength;
    team->badgeStyle = id % 15;
    team->level = level;
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
        team_crest(game, i + 1, teams[i]->level, teams[i]->id);
    }
    set_colour_tweak(game, "MyTeamBadge1Col1", mine->badge1, 1);
    set_colour_tweak(game, "MyTeamBadge1Col2", mine->badge2, 1);
    float bright = (float)ssr_tweaks_number(game->tweaks, "MatchColours", "BrightKit");
    Rgb col[2][2];
    for (int i = 0; i < 2; i++) {
        const Team *t = teams[i];
        // [I: 4.32 colours the plates with the home shirts. A team in its
        // away kit gets the colours of that kit, so that two teams in the
        // same home colours have plates of two colours.]
        Rgb one = t->badge1, two = t->badge2;
        if (t->awayKit) {
            one = rgb(t->away->shirt1);
            two = rgb(t->away->shorts);
        }
        // The swap of a light first colour is a rule of the home plates.
        int swap = !t->awayKit && brightness(one) > 200 && brightness(two) < brightness(one);
        col[i][0] = swap ? two : one;
        col[i][1] = swap ? one : two;
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
    // The plates of the scores follow the kits.
    int mineHome = game->match->setup.myTeam == 1;
    team_plates(game, &game->homeTeam, &game->awayTeam, mineHome ? &game->homeTeam : &game->awayTeam);
}

// c_TMatch.SetUpFixture: the match of the next fixture of the player.
static const SSRFixture *first_leg_of(const SSRSeasonComp *comp, const SSRFixture *f);

// c_TFixture.GetBallType: the cup of a nation is 2, a competition of a
// continent 3, and a match of the nation 4. Another is the league, 1.
static int ball_type_of(const SSRCompetition *comp) {
    if (!comp) return 1;
    if (comp->level == 0 && comp->locale == 0) return comp->type == 1 ? 2 : 1;
    if (comp->level == 0 && comp->locale == 1) return 3;
    if (comp->level == 1) return 4;
    return 1;
}

// ABall.SetType: the layouts show the ball of the match.
static void match_ball_tweak(SSRGame *game) {
    int ball = ssr_career_selected_ball(&game->career, game->matchBallType);
    ssr_tweaks_set_number(game->tweaks, "Match", "MatchBall", ball - 1);
}

// The weeks of the winter of the year of the game, which starts in the
// summer: December to February in the north, June to August in the south.
#define WINTER_NORTH_FIRST 18
#define WINTER_NORTH_LAST 30
#define WINTER_SOUTH_FIRST 44
#define WINTER_SOUTH_LAST 4
// The latitude from which the rain of the winter is snow.
#define SNOW_LATITUDE 45
// A club under this strength has a worn pitch in the winter.
#define WORN_PITCH_STRENGTH 40

// The rounds at the end of a league season that make a big match.
#define MUST_WIN_ROUNDS 3

// A big match of the remake: a cup final, or a league match near the end of
// the season for a club in the top two places or the bottom three.
static int big_match(SSRGame *game, SSRSeasonComp *comp, const SSRFixture *f, SSRPool *pool, int tableId) {
    if (ssr_season_is_final(game->season, comp)) return SSR_BIG_FINAL;
    if (comp->data->type != 0 || pool->count < 6) return SSR_BIG_NONE;
    int left = 0;
    for (int k = 0; k < comp->fixtureCount; k++) {
        const SSRFixture *x = &comp->fixtures[k];
        left += x->group == f->group && x->result == 0 && (x->home == tableId || x->away == tableId);
    }
    if (left > MUST_WIN_ROUNDS) return SSR_BIG_NONE;
    int teamId = 0;
    for (int r = 0; r < pool->count; r++) {
        if (game->season->rows[pool->rows[r]].id == tableId) teamId = game->season->rows[pool->rows[r]].teamId;
    }
    int position = ssr_season_position(game->season, pool, teamId);
    return position >= 1 && (position <= 2 || position > pool->count - 3) ? SSR_BIG_MUST_WIN : SSR_BIG_NONE;
}

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
    game->matchBallType = ball_type_of(comp->data);
    match_ball_tweak(game);
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
    s.focus = ssr_career_focus(c);
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
    // The weather of the remake: rain is more likely far from the equator,
    // as in the training of 4.32. The numbers come from the seed of the
    // world, the day, and the ground, so no stream of numbers changes.
    // In the winter, the rain is snow far from the equator. The pitch of a
    // small club is worn in the winter.
    const SSRClub *ground = game->world ? ssr_world_club(game->world, ssr_fixture_home_team(game->season, f)) : NULL;
    int worn = 0;
    if (ground && f->level == 0) {
        SSRRandom r = {(int32_t)(c->worldSeed * 7919 + f->sdate * 104729 + ground->id * 31)};
        int far = (int)(ground->latitude < 0 ? -ground->latitude : ground->latitude);
        s.weather = ssr_rand(&r, 80) < far && ssr_rand(&r, 4) == 1;
        int week = ssr_date_week(f->sdate);
        int winter = ground->latitude >= 0 ? week >= WINTER_NORTH_FIRST && week <= WINTER_NORTH_LAST
                                           : week >= WINTER_SOUTH_FIRST || week <= WINTER_SOUTH_LAST;
        if (s.weather == 1 && winter && far >= SNOW_LATITUDE) s.weather = 2;
        worn = winter && ground->strength < WORN_PITCH_STRENGTH;
    }
    // The pitch of the game: 0 dry, 1 snow, 2 worn, 3 rain.
    ssr_tweaks_set_number(game->tweaks, "Pitch", "WeatherType", s.weather == 1 ? 3 : s.weather == 2 ? 1 : worn ? 2 : 0);
    s.bigMatch = big_match(game, comp, f, pool, homeIsMine ? f->home : f->away);
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

// A moment goes into the timeline of the career.
static void moment(SSRGame *game, const char *text) {
    ssr_career_add_moment(&game->career, game->career.sdate, text);
}

// The moment of a new club.
static void signed_moment(SSRGame *game) {
    const SSRClub *club = game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
    char text[256];
    snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentSigned"));
    replace_all(text, sizeof(text), "$club", club ? club->name : "");
    moment(game, text);
}

// A match can hurt the player: one match in 60, or one in 15 when the
// player ends it with less than 30 energy. The numbers come from the
// stream of the world, so the numbers of the career stay the same.
static void player_injury(SSRGame *game, float energy) {
    SSRCareer *c = &game->career;
    int pain = game->inPain;
    game->inPain = 0;
    if (!game->people) return;
    SSRRandom *r = &game->people->random;
    // A match with pain makes the injury worse one time in three.
    if (pain) {
        if (ssr_rand(r, 3) != 1) return;
        c->injury += ssr_rand2(r, 7, 21);
    } else {
        if (c->injury > 0 || ssr_rand(r, energy < 30 ? 15 : 60) != 1) return;
        c->injury = ssr_rand2(r, 7, 42);
    }
    char text[256], number[16];
    snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentInjury"));
    snprintf(number, sizeof(number), "%d", c->injury);
    replace_all(text, sizeof(text), "$num", number);
    alert(game, "bottommessagemoment", text, 4000);
    ssr_career_add_moment(c, c->sdate, text);
}

// The moments of a career after a match of the club: the debut, the first
// goal, and the matches and the goals of a milestone. Each opens an alert.
static void career_moments(SSRGame *game, int goalsBefore) {
    const SSRCareer *c = &game->career;
    static const int APPS[] = {50, 100, 200, 300, 400, 500};
    static const int GOALS[] = {25, 50, 100, 150, 200, 300};
    char text[256], number[16];
    if (c->apps == 1) {
        const SSRClub *club = game->world ? ssr_world_club(game->world, c->clubId) : NULL;
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentDebut"));
        replace_all(text, sizeof(text), "$club", club ? club->name : "");
        alert(game, "bottommessagemoment", text, 4000);
        moment(game, text);
    }
    if (goalsBefore == 0 && c->goals > 0) {
        alert(game, "bottommessagemoment", loc(game, "SSR_MomentFirstGoal"), 4000);
        moment(game, loc(game, "SSR_MomentFirstGoal"));
    }
    for (size_t i = 0; i < sizeof(APPS) / sizeof(APPS[0]); i++) {
        if (c->apps != APPS[i]) continue;
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentApps"));
        snprintf(number, sizeof(number), "%d", APPS[i]);
        replace_all(text, sizeof(text), "$num", number);
        alert(game, "bottommessagemoment", text, 4000);
        moment(game, text);
    }
    for (size_t i = 0; i < sizeof(GOALS) / sizeof(GOALS[0]); i++) {
        if (goalsBefore >= GOALS[i] || c->goals < GOALS[i]) continue;
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentGoals"));
        snprintf(number, sizeof(number), "%d", GOALS[i]);
        replace_all(text, sizeof(text), "$num", number);
        alert(game, "bottommessagemoment", text, 4000);
        moment(game, text);
    }
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
    // Match context: the fans care more about a derby or a match of a big
    // rivalry. The change of the relation of the fans is half as big again.
    if (game->season && game->world) {
        SSRFixture *fixture = ssr_season_find(game->season, game->fixtureComp, game->fixtureDate, game->fixtureHome,
                                              game->fixtureAway);
        SSRSeasonComp *comp = ssr_season_comp(game->season, game->fixtureComp);
        if (fixture && comp && comp->data->level == 0) {
            int a = ssr_fixture_home_team(game->season, fixture), b = ssr_fixture_away_team(game->season, fixture);
            if (ssr_world_is_derby(game->world, a, b) || ssr_honours_rivalry(&game->honours, a, b) >= SSR_RIVALRY_POINTS) {
                game->after.fans = game->after.fans * 3 / 2;
            }
        }
        // A big match counts more for the fans and the boss: two times in a
        // final, and half as much again in a match that the club must win.
        int big = game->match ? game->match->setup.bigMatch : SSR_BIG_NONE;
        if (big == SSR_BIG_FINAL) {
            game->after.fans *= 2;
            game->after.boss *= 2;
        } else if (big == SSR_BIG_MUST_WIN) {
            game->after.fans = game->after.fans * 3 / 2;
            game->after.boss = game->after.boss * 3 / 2;
        }
    }
    // The counts of the club, or of the nation (c_TScreen_MatchStats).
    SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    float rating = game->after.rating;
    int hatTrick = ssr_career_hat_tricks(m->goals);
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
        int goalsBefore = c->goals;
        c->apps++;
        c->seasonApps++;
        c->goals += m->goals;
        c->seasonGoals += m->goals;
        c->assists += m->assists;
        c->seasonAssists += m->assists;
        c->passes += m->passes;
        c->seasonPasses += m->passes;
        c->seasonHatTricks += hatTrick;
        c->hatTricks += hatTrick;
        c->seasonChances += m->chances;
        c->chances += m->chances;
        c->seasonStarMan += game->after.starMan;
        c->starMan += game->after.starMan;
        // TScreen_StarMan: Star Man, and 10, 25, and 50 of them.
        if (game->after.starMan > 0) {
            award(game, 8);
            if (c->starMan >= 10) award(game, 9);
            if (c->starMan >= 25) award(game, 10);
            if (c->starMan >= 50) award(game, 11);
        }
        c->seasonTotalRating += rating;
        c->totalRating += rating;
        ssr_career_add_form(c->form, (int)rating);
        career_moments(game, goalsBefore);
        player_injury(game, m->energy);
    }
    memcpy(game->matchFurthest, m->furthest, sizeof(game->matchFurthest));
    game->matchPostHits = m->postHits;
    game->matchCornerGoals = m->cornerGoals;
    game->matchLastMinuteWinner = m->lastMinuteWinner;
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
    case 2: {
        float e = c->energy + (float)game->trainerBoost;
        c->energy = e < 0 ? 0 : e > 100 ? 100 : e;
        ssr_tweaks_set_number(t, "Player", "RecoveryBoostTrainer", game->trainerBoost);
        ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
        break;
    }
    case 3: {
        // Each property gives 1.5 more energy.
        float e = c->energy + game->propertyBoost;
        c->energy = e < 0 ? 0 : e > 100 ? 100 : e;
        ssr_tweaks_set_number(t, "Player", "RecoveryBoostProperties", game->propertyBoost);
        ssr_tweaks_set_number(t, "Player", "Energy", c->energy);
        break;
    }
    case 4: {
        float total = (float)game->recovery + game->propertyBoost;
        int shown = (int)(total < 0 ? 0 : total > 100 ? 100 : total) + game->trainerBoost;
        shown = shown > 100 ? 100 : shown;
        ssr_tweaks_set_number(t, "Player", "RecoveryTotal", shown);
        break;
    }
    case 5:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsWage", game->payWage);
        break;
    case 6:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentWage", game->agentWage);
        break;
    case 7:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsBonusGoals", game->payGoals);
        break;
    case 8:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentBonusGoals", game->agentGoals);
        break;
    case 9:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsBonusAssists", game->payAssists);
        break;
    case 10:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentBonusAssists", game->agentAssists);
        break;
    case 11:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsSponsors", game->paySponsors);
        break;
    case 12:
        ssr_tweaks_set_number(t, "MatchReview", "EarningsAgentSponsors", game->agentSponsors);
        break;
    case 14: {
        // DepleteContracts: the agent counts club matches, the trainer all.
        int agent = ssr_career_agent(c), trainer = ssr_career_trainer(c);
        if (game->clubMatch && agent > 0) c->agent[agent - 1]--;
        if (trainer > 0) c->trainer[trainer - 1]--;
        break;
    }
    case 13: {
        // GSPlayerAwardWages: the pay goes into the bank once.
        int total = game->payWage + game->payGoals + game->payAssists + game->paySponsors + game->agentWage +
                    game->agentGoals + game->agentAssists + game->agentSponsors;
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
static int count_owned(const SSRCareer *c, SSRShopType type);

// max1 of the agent boosts: a part of a bux is 1.
static int agent_boost(float v) {
    return (int)(v > 0 && v < 1 ? 1 : v);
}

static const char *my_club_tla(SSRGame *game);

// c_TScreen_MatchStats.SetUpScreen: a won final goes into the trophy log:
// the cup, the continental cup, or the cup of the nations. The last two
// count as a major award. [I: the game logs it when the stats open; the
// remake logs it when they close.]
static void cup_trophy(SSRGame *game, const SSRFixture *f) {
    SSRCareer *c = &game->career;
    SSRSeasonComp *comp = game->season && f ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    if (!comp || comp->data->type != 1) return;
    int nation = comp->data->level == 1;
    if (!ssr_season_won_final(game->season, comp, f, nation ? c->nationId : c->clubId)) return;
    int medal = nation ? 5 : comp->data->locale == 0 ? 3 : comp->data->locale == 1 ? 4 : 0;
    if (!medal) return;
    const SSRNation *n = ssr_world_nation(game->world, c->nationId);
    const char *team = nation ? (n ? n->tla : "") : my_club_tla(game);
    char winner[64], text[96];
    snprintf(winner, sizeof(winner), "%s", loc(game, "Winner"));
    upper(game, winner);
    snprintf(text, sizeof(text), "%s %s", comp->data->tla, winner);
    ssr_career_add_trophy(c, ssr_career_year(c), team, text, medal);
    if (medal >= 4) c->wonMajorAward = 1;
}

static float form_of(const int *form, int apps);

// TScreen_MatchStats.CheckAchievements: the awards of a match. [I: the
// remake keeps no count of the hits of the post and of the goals from a
// corner, so the awards 115, 117, and 118 do not come yet.]
// GetStringDistance: pixels as yards or metres, with one decimal that the
// game cuts, not rounds. tla gives "yd" or "m", else "yards" or "metres".
static void distance_string(SSRGame *game, float pixels, int tla, char *out, size_t capacity) {
    float value = pixels / 19.2f;
    if (game->settings.distance) value *= 0.9144f;
    const char *tag = game->settings.distance ? (tla ? "tla_Metres" : "Metres") : (tla ? "tla_Yards" : "Yards");
    char unit[32];
    snprintf(unit, sizeof(unit), "%s", loc(game, tag));
    for (char *p = unit; *p; p++) *p = (char)tolower((unsigned char)*p);
    int tenths = (int)(value * 10.0f + 0.0001f);
    snprintf(out, capacity, "%d.%d %s", tenths / 10, tenths % 10, unit);
}

// c_TScreen_MatchStats.CheckRecords and CheckInternationalRecords: the
// furthest values of the match go into the season and the career, or into
// the career with the nation. A new record of the career gives an alert.
static void match_records(SSRGame *game, int nation) {
    SSRCareer *c = &game->career;
    static const char *const TAGS[4] = {"CMESSAGE_NEWFURTHESTGOAL", "CMESSAGE_NEWFURTHESTPASS",
                                        "CMESSAGE_NEWFURTHESTHEADEDGOAL", "CMESSAGE_NEWFURTHESTHEADEDPASS"};
    // The game checks the goal, the headed goal, the pass, the headed pass.
    static const int ORDER[4] = {0, 2, 1, 3};
    for (int k = 0; k < 4; k++) {
        int i = ORDER[k];
        float value = game->matchFurthest[i];
        if (nation) {
            if (value > c->intFurthest[i]) c->intFurthest[i] = value;
            continue;
        }
        if (value > c->seasonFurthest[i]) c->seasonFurthest[i] = value;
        if (value <= c->furthest[i]) continue;
        c->furthest[i] = value;
        char text[256], dist[48];
        snprintf(text, sizeof(text), "%s", loc(game, TAGS[i]));
        distance_string(game, value, 0, dist, sizeof(dist));
        replace_all(text, sizeof(text), "$dist", dist);
        alert(game, "bottommessagerecord", text, 5000);
        play_sound(game, "CAchievement");
    }
}

static void match_awards(SSRGame *game, SSRFixture *f) {
    SSRCareer *c = &game->career;
    const SSRAfterMatchFacts *m = &game->facts;
    // A hat trick counts at the third, the sixth, and the ninth goal.
    int hatTricks = ssr_career_hat_tricks(m->goals);
    if (m->passes >= 5) award(game, 4);
    if (m->passes >= 10) award(game, 5);
    if (m->assists > 0) award(game, 6);
    if (m->assists >= 3) award(game, 7);
    if (game->matchCornerGoals > 0) award(game, 115);
    if (m->dribbles >= 3) award(game, 116);
    if (game->matchPostHits > 0) award(game, 117);
    if (game->matchPostHits >= 3) award(game, 118);
    if (game->matchLastMinuteWinner) award(game, 79);
    // A rating at 10, from the short pass to the dribble.
    for (int i = 0; i < 8; i++) {
        if (c->ratings[i] >= 10) award(game, 101 + i);
    }
    SSRSeasonComp *comp = game->season && f ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    int won = m->myScore > m->opponentScore;
    int margin = m->home ? m->score1 - m->score2 : m->score2 - m->score1;
    if (comp && comp->data->level == 0) {
        match_records(game, 0);
        if (m->goals > 0) award(game, 2);
        if (hatTricks > 0) award(game, 3);
        if (form_of(c->form, c->apps) >= 10) award(game, 12);
        if (c->goals >= 50) award(game, 23);
        if (c->goals >= 100) award(game, 24);
        if (comp->data->locale == 1) {
            award(game, 58);
            if (m->goals > 0) award(game, 59);
            if (hatTricks > 0) award(game, 60);
        }
        if (won) {
            int final = ssr_season_won_final(game->season, comp, f, c->clubId);
            if (comp->data->type == 0) {
                if (comp->data->locale == 0) award(game, 13);
                else if (comp->data->locale == 1) award(game, 61);
            } else if (comp->data->type == 1 && comp->data->locale == 0) {
                award(game, 14);
                if (final) award(game, 15);
            } else if (comp->data->type == 1 && comp->data->locale == 1) {
                award(game, 61);
                if (final) {
                    award(game, 62);
                    if (ssr_career_history_apps(c, my_club_tla(game)) + c->seasonApps == c->apps) award(game, 124);
                }
            }
            if (margin > 4) award(game, 18);
        }
        // CheckTotalAppsForCurrentClub.
        int clubApps = ssr_career_history_apps(c, my_club_tla(game)) + c->seasonApps;
        if (clubApps > 100) award(game, 76);
        if (clubApps > 250) award(game, 77);
        if (clubApps > 500) award(game, 78);
    } else if (comp && comp->data->level == 1) {
        match_records(game, 1);
        award(game, 51);
        if (m->goals > 0) award(game, 52);
        if (hatTricks > 0) award(game, 53);
        if (c->intGoals >= 50) award(game, 56);
        if (c->intGoals >= 100) award(game, 57);
        if (won) {
            award(game, 54);
            if (comp->data->type == 1 && ssr_season_won_final(game->season, comp, f, c->nationId)) {
                award(game, 55);
                if (comp->data->id == 60) {
                    award(game, 75);
                    if (c->captain == 2 || c->captain == 3) award(game, 83);
                }
            }
            // [I: 4.32 compares the winner of 5 goals with the club here, so
            // the award 18 never comes from a match of the nation.]
        }
    }
    // CheckTotalAppsClubAndCountry.
    int apps = c->apps + c->intApps;
    if (apps > 250) award(game, 109);
    if (apps > 500) award(game, 110);
    if (apps > 1000) award(game, 111);
    if (hatTricks > 1) award(game, 112);
}

static void stats_continue(SSRGame *game) {
    if (game->statStage < 6) after_relations(game);
    if (game->statStage < 9) after_star_rating(game);
    SSRCareer *c = &game->career;
    // c_TPlayer.WearBoots: a match uses the boots.
    if (c->bootId > 0) c->bootUses--;
    if (c->bootUses <= 0) {
        c->expiredBoots = c->bootId;
        c->bootId = 0;
    }
    season_in(game);
    SSRFixture *f = ssr_season_find(game->season, game->fixtureComp, game->fixtureDate, game->fixtureHome,
                                    game->fixtureAway);
    if (f && game->extraTime) {
        ssr_season_my_result_after_extra_time(game->season, f, game->facts.score1, game->facts.score2,
                                              game->pens[1], game->pens[2]);
    } else if (f) {
        ssr_season_my_result(game->season, f, game->facts.score1, game->facts.score2);
    }
    cup_trophy(game, f);
    if (game->statStage < 11) match_awards(game, f);
    // MatchRecovery comes first in PlayToDate, with the rest days to the
    // next match.
    match_recovery(game);
    game->recovery = ssr_energy_recovery(game->season ? ssr_season_rest_days(game->season) : 14, ssr_career_age(c));
    game->payWage = c->wage;
    game->payGoals = game->facts.goals * c->goalBonus;
    game->payAssists = game->facts.assists * c->assistBonus;
    game->propertyBoost = 30.0f / (float)ssr_shop_count_by_type(SSR_SHOP_PROPERTY) * (float)count_owned(c, SSR_SHOP_PROPERTY);
    // GetSponsorAmount: the sponsors pay for a club match only. [I: the
    // boost of a captain comes with the captaincy.]
    game->paySponsors = 0;
    SSRSeasonComp *played = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    if (!(played && played->data->level == 1)) {
        game->payGoals = game->facts.goals * c->goalBonus;
        for (int i = 0; i < 10; i++) game->paySponsors += c->sponsors[i];
        // GetCaptaincySponsorBoost: a captain earns more from the sponsors.
        if (ssr_career_count_sponsors(c) > 0 && c->captain >= 1 && c->captain <= 3) {
            float f = (float)game->paySponsors / 100.0f * (float)(c->captain * 10);
            game->paySponsors += (int)(f >= 1 ? f : 1);
        }
    } else {
        // A match of the nation pays the wage only.
        game->payGoals = game->payAssists = 0;
    }
    // The agent adds 10, 20, or 30 in a hundred to the pay of a club match,
    // and at least 1 (GetAgentWageBoost and the others).
    static const int AGENT_PART[4] = {0, 10, 20, 30};
    static const int TRAINER_BOOST[4] = {0, 5, 15, 30};
    float part = (float)AGENT_PART[ssr_career_agent(c)];
    game->clubMatch = !(played && played->data->level == 1);
    game->agentWage = game->agentGoals = game->agentAssists = game->agentSponsors = 0;
    if (game->clubMatch && part > 0) {
        game->agentWage = agent_boost((float)c->wage / 100.0f * part);
        if (game->facts.goals > 0) game->agentGoals = agent_boost((float)(c->goalBonus * game->facts.goals) / 100.0f * part);
        if (c->assistBonus > 0 && game->facts.assists > 0) {
            game->agentAssists = agent_boost((float)(c->assistBonus * game->facts.assists) / 100.0f * part);
        }
        if (ssr_career_count_sponsors(c) > 0) game->agentSponsors = agent_boost((float)game->paySponsors / 100.0f * part);
    }
    game->trainerBoost = TRAINER_BOOST[ssr_career_trainer(c)];
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
    game->doneTrainerWarning = 0;
    c->gambling = c->gambling - 5 < 0 ? 0 : c->gambling - 5;
    if (game->caughtGambling > 0) game->caughtGambling--;
    c->spentTime = 0;
    int n = ssr_career_count_sponsors(c);
    if (n > 0) update_relation(game, SSR_RELATION_SPONSORS, (float)n * -0.65f);
    if (game->horsesLoaded) ssr_horses_recover(&game->horses, &game->random);
    // TShopItem.UpdateLife: the products wear.
    ssr_shop_wear(&game->random, c->items, c->vehicles, c->properties);
}

// The call "PlayToDate": the days up to the date in the message.
static void call_play_to_date(SSRGame *game) {
    match_recovery(game);
    season_in(game);
    after_play_to(game, ssr_season_play_to(game->season, game->messageData));
}

// The call "PlayWithPain": Ok plays the match with the injury, and Cancel
// rests.
static void call_play_with_pain(SSRGame *game) {
    if (game->returnState == 1) {
        game->inPain = 1;
        start_match(game);
        return;
    }
    char text[256], number[16];
    snprintf(text, sizeof(text), "%s", loc(game, "SSR_Injured"));
    snprintf(number, sizeof(number), "%d", game->career.injury);
    replace_all(text, sizeof(text), "$num", number);
    not_picked(game, text);
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
    if (c->injury > 0 && !c->retired) {
        // The player chooses: play with pain, or rest.
        char text[256], number[16];
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_PlayWithPain"));
        snprintf(number, sizeof(number), "%d", c->injury);
        replace_all(text, sizeof(text), "$num", number);
        message(game, "message", text, 1, 1, "PlayWithPain");
        return;
    }
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
        team_crest(game, 1, level, teamId);
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
/* ---------------------------------------------------------- the squad */

static const char *const POSITIONS[SSR_POS_COUNT] = {"GK", "DF", "MF", "FW"};

static int by_position(const void *a, const void *b) {
    const SSRPerson *p = *(SSRPerson *const *)a, *q = *(SSRPerson *const *)b;
    if (p->position != q->position) return p->position - q->position;
    if (p->skill != q->skill) return q->skill - p->skill;
    return p->id - q->id;
}

// The most goals of the season first, then the most assists.
static int by_season(const void *a, const void *b) {
    const SSRPerson *p = *(SSRPerson *const *)a, *q = *(SSRPerson *const *)b;
    if (p->seasonGoals != q->seasonGoals) return q->seasonGoals - p->seasonGoals;
    if (p->seasonAssists != q->seasonAssists) return q->seasonAssists - p->seasonAssists;
    return q->seasonApps - p->seasonApps;
}

// A row of the squad list: the position, the name, the age or the club,
// the skill or the matches, and the goals.
static void squad_row(SSRGame *game, const SSRPerson *p, const char *club, int year) {
    char texts[7][96];
    snprintf(texts[0], 96, "%s", POSITIONS[p->position]);
    if (club) snprintf(texts[1], 96, "%c. %s, %s", p->first[0], p->last, club);
    else snprintf(texts[1], 96, "%c. %s", p->first[0], p->last);
    int values[5] = {year - p->born, p->skill, p->seasonApps, p->seasonGoals, p->seasonAssists};
    for (int k = 0; k < 5; k++) snprintf(texts[k + 2], 96, "%d", values[k]);
    add_op(game, SSR_OP_LIST_ADD, "PlayerList", p->rival ? "RivalEntry" : "PlayerEntry", "", 0);
    for (int k = 0; k < 7; k++) add_op(game, SSR_OP_LIST_TEXT, "PlayerList", NULL, texts[k], k + 1);
}

// The squad of the club, the best of each position first, or the top
// scorers of the league of the club.
static void squad_fill(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    add_op(game, SSR_OP_LIST_CLEAR, "PlayerList", NULL, NULL, 0);
    ssr_tweaks_set_number(t, "Squad", "Scroll", 0);
    const SSRClub *club = game->world ? ssr_world_club(game->world, game->squadClub) : NULL;
    int year = ssr_career_year(&game->career);
    SSRPerson *list[64];
    int n = 0;
    if (game->people && game->names && club) {
        ssr_people_make_squad(game->people, game->world, game->names, club->id, year);
    }
    SSRPerson me;
    if (game->squadMode == 1) {
        SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->leagueComp) : NULL;
        int clubs[64], count = 0;
        if (comp && comp->poolCount > 0 && comp->data->level == 0) {
            for (int g = 0; g < comp->poolCount; g++) {
                for (int i = 0; i < comp->pools[g].count && count < 64; i++) {
                    clubs[count++] = game->season->rows[comp->pools[g].rows[i]].teamId;
                }
            }
        }
        n = ssr_people_top_scorers(game->people, clubs, count, list, 20);
        ssr_tweaks_set(t, "Squad", "Title", comp ? comp->data->tla : "");
        ssr_tweaks_set(t, "Squad", "ModeText", loc(game, "SSR_Rivals"));
    } else if (game->squadMode == 2) {
        // The player of the user, as a row of the same kind, with the mean
        // of the five skills.
        const SSRCareer *c = &game->career;
        memset(&me, 0, sizeof(me));
        snprintf(me.first, sizeof(me.first), "%s", c->firstName);
        snprintf(me.last, sizeof(me.last), "%s", c->lastName);
        me.club = c->clubId;
        me.born = year - ssr_career_age(c);
        me.position = c->position == SSR_CAREER_HOLDING ? SSR_POS_MIDFIELDER : SSR_POS_ATTACKER;
        me.skill = (ssr_career_skill(c->technique) + ssr_career_skill(c->pace) + ssr_career_skill(c->vision) +
                    ssr_career_skill(c->setPieces) + ssr_career_skill(c->power)) / 5;
        me.seasonApps = c->seasonApps;
        me.seasonGoals = c->seasonGoals;
        me.seasonAssists = c->seasonAssists;
        list[n++] = &me;
        for (int i = 0; game->people && i < game->people->count && n < 64; i++) {
            SSRPerson *p = &game->people->list[i];
            if (p->rival && !p->retired) list[n++] = p;
        }
        qsort(list, (size_t)n, sizeof(list[0]), by_season);
        ssr_tweaks_set(t, "Squad", "Title", loc(game, "SSR_Rivals"));
        ssr_tweaks_set(t, "Squad", "ModeText", loc(game, "SSR_Squad"));
    } else {
        n = ssr_people_squad(game->people, club ? club->id : 0, list, 64);
        if (n > 64) n = 64;
        qsort(list, (size_t)n, sizeof(list[0]), by_position);
        ssr_tweaks_set(t, "Squad", "Title", club ? club->name : "");
        ssr_tweaks_set(t, "Squad", "ModeText", loc(game, "SSR_Scorers"));
    }
    // The rivals show the player, and the player can change position there.
    static const char *const POSITIONS[SSR_CAREER_POSITIONS] = {"SSR_PosForward", "SSR_PosStriker", "SSR_PosHolding"};
    ssr_tweaks_set_number(t, "Squad", "CanPosition", game->squadMode == 2);
    ssr_tweaks_set(t, "Squad", "PositionText", loc(game, POSITIONS[game->career.position % SSR_CAREER_POSITIONS]));
    add_op(game, SSR_OP_LIST_START, "PlayerList", NULL, NULL, 52);
    for (int i = 0; i < n; i++) {
        // The lists of other clubs show the full name of the club. A long
        // name moves from side to side in its box.
        const SSRClub *of = game->squadMode ? ssr_world_club(game->world, list[i]->club) : NULL;
        squad_row(game, list[i], of ? of->name : NULL, year);
    }
}

// squad.btn_Position: the player plays in the next position. The focus of
// the next match follows it.
static void squad_position(SSRGame *game) {
    if (game->squadMode != 2) return;
    game->career.position = (game->career.position + 1) % SSR_CAREER_POSITIONS;
    save_career(game);
    squad_fill(game);
}

// league.btn_Squad: the squad of the chosen team of the table, or of the
// club of the player.
static void league_squad(SSRGame *game) {
    SSRSeasonComp *comp = ssr_season_comp(game->season, game->leagueComp);
    int team = game->career.clubId;
    if (comp && comp->data->type != 1 && comp->data->level == 0 && comp->poolCount > 0) {
        int group = comp->poolCount > 1 ? ssr_season_group_of(game->season, comp, game->career.clubId) : 1;
        SSRPool *pool = &comp->pools[group > 0 ? group - 1 : 0];
        int row = game->leagueSelected;
        if (row > 0 && row <= pool->count) team = game->season->rows[pool->rows[row - 1]].teamId;
    }
    game->squadClub = team;
    game->squadMode = 0;
    set_screen(game, "squad");
    squad_fill(game);
}

static void squad_mode(SSRGame *game) {
    if (game->typing == 4) edit_done(game);
    game->squadMode = (game->squadMode + 1) % 3;
    squad_fill(game);
}

// squad.btn_Honours: the champions and the top scorers, the newest
// season first. A title of the club of the player shows its highlight.
static const char *club_name(SSRGame *game, int id);

static int by_career_goals(const void *a, const void *b) {
    const SSRPerson *p = *(SSRPerson *const *)a, *q = *(SSRPerson *const *)b;
    if (p->goals != q->goals) return q->goals - p->goals;
    return p->apps - q->apps;
}

// The hall of fame: the 30 players of the world with the most goals with
// the clubs, and the player of the user among them.
static void hall_of_fame(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    ssr_tweaks_set(t, "Honours", "Title", loc(game, "SSR_HallOfFame"));
    ssr_tweaks_set(t, "Honours", "Column", loc(game, "Goals"));
    ssr_tweaks_set(t, "Honours", "ModeText", loc(game, "SSR_Honours"));
    int n = game->people ? game->people->count : 0;
    SSRPerson **all = malloc(sizeof(SSRPerson *) * (size_t)(n + 1));
    if (!all) return;
    SSRPerson me;
    memset(&me, 0, sizeof(me));
    const SSRCareer *c = &game->career;
    snprintf(me.first, sizeof(me.first), "%s", c->firstName);
    snprintf(me.last, sizeof(me.last), "%s", c->lastName);
    me.club = c->clubId;
    me.goals = c->goals;
    me.apps = c->apps;
    me.assists = c->assists;
    me.id = -1;
    int count = 0;
    all[count++] = &me;
    for (int i = 0; i < n; i++) {
        if (game->people->list[i].goals > 0) all[count++] = &game->people->list[i];
    }
    qsort(all, (size_t)count, sizeof(SSRPerson *), by_career_goals);
    for (int i = 0; i < count && i < 30; i++) {
        const SSRPerson *p = all[i];
        char name[160], line[256], number[16];
        if (p->retired) snprintf(name, sizeof(name), "%d. %s %s (%s)", i + 1, p->first, p->last, loc(game, "SSR_Retired"));
        else snprintf(name, sizeof(name), "%d. %s %s, %s", i + 1, p->first, p->last, club_name(game, p->club));
        snprintf(line, sizeof(line), "%s", loc(game, "SSR_FameRow"));
        snprintf(number, sizeof(number), "%d", p->goals);
        replace_all(line, sizeof(line), "$goals", number);
        snprintf(number, sizeof(number), "%d", p->apps);
        replace_all(line, sizeof(line), "$apps", number);
        snprintf(number, sizeof(number), "%d", p->assists);
        replace_all(line, sizeof(line), "$assists", number);
        add_op(game, SSR_OP_LIST_ADD, "HonourList", "HonourEntry", "", 0);
        add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, name, 1);
        add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, line, 2);
        if (p->id == -1 || p->rival) add_op(game, SSR_OP_LIST_SUB, "HonourList", "Highlight", NULL, 1);
    }
    free(all);
}

static void record_row(SSRGame *game, const char *title, const char *detail, int highlight) {
    add_op(game, SSR_OP_LIST_ADD, "HonourList", "HonourEntry", "", 0);
    add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, title, 1);
    add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, detail, 2);
    if (highlight) add_op(game, SSR_OP_LIST_SUB, "HonourList", "Highlight", NULL, 1);
}

// The text of a tag with the names and the numbers in it.
static void record_text(SSRGame *game, char *out, size_t capacity, const char *tag, const char *name,
                        const char *club, int number, int year) {
    char value[16];
    snprintf(out, capacity, "%s", loc(game, tag));
    replace_all(out, capacity, "$name", name ? name : "");
    replace_all(out, capacity, "$club", club ? club : "");
    replace_all(out, capacity, "$league", name ? name : "");
    replace_all(out, capacity, "$nation", name ? name : "");
    snprintf(value, sizeof(value), "%d", number);
    replace_all(out, capacity, "$n", value);
    snprintf(value, sizeof(value), "%d", year);
    replace_all(out, capacity, "$year", value);
}

// The records of the world: for each league, the club with the most titles
// and the most goals of a scorer in a season; the titles of the club and of
// the nation of the player; and the scorer with the most goals.
static void records(SSRGame *game) {
    const SSRHonours *h = &game->honours;
    const SSRClub *mine = game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
    const SSRCompetition *myLeague = mine ? ssr_world_competition(game->world, mine->league) : NULL;
    char title[256], detail[256];
    // The leagues of the honours, the league of the player first.
    const char *leagues[64];
    int leagueCount = 0;
    if (myLeague) leagues[leagueCount++] = myLeague->tla;
    for (int i = 0; i < h->count && leagueCount < 64; i++) {
        int seen = 0;
        for (int k = 0; k < leagueCount && !seen; k++) seen = strcmp(leagues[k], h->list[i].league) == 0;
        if (!seen) leagues[leagueCount++] = h->list[i].league;
    }
    for (int l = 0; l < leagueCount; l++) {
        int bestClub = 0, bestTitles = 0, bestGoals = -1;
        const SSRHonour *top = NULL;
        for (int i = 0; i < h->count; i++) {
            const SSRHonour *a = &h->list[i];
            if (strcmp(a->league, leagues[l]) != 0) continue;
            int titles = 0;
            for (int k = 0; k < h->count; k++) titles += strcmp(h->list[k].league, leagues[l]) == 0 && h->list[k].club == a->club;
            if (titles > bestTitles) {
                bestTitles = titles;
                bestClub = a->club;
            }
            if (a->scorer[0] && a->goals > bestGoals) {
                bestGoals = a->goals;
                top = a;
            }
        }
        record_text(game, title, sizeof(title), "SSR_RecordLeagueTitles", leagues[l], NULL, 0, 0);
        if (bestClub) record_text(game, detail, sizeof(detail), "SSR_RecordTitlesOf", NULL, club_name(game, bestClub),
                                  bestTitles, 0);
        else snprintf(detail, sizeof(detail), "%s", loc(game, "SSR_RecordNone"));
        record_row(game, title, detail, bestClub && bestClub == game->career.clubId);
        if (top) {
            record_text(game, title, sizeof(title), "SSR_RecordSeasonGoals", leagues[l], NULL, 0, 0);
            record_text(game, detail, sizeof(detail), "SSR_RecordScorer", top->scorer, top->scorerClub, top->goals,
                        top->year);
            record_row(game, title, detail, strcmp(top->scorer, game->career.fullName) == 0);
        }
    }
    // The titles of the club and of the nation of the player.
    if (mine) {
        record_text(game, title, sizeof(title), "SSR_RecordClubTitles", NULL, mine->name, 0, 0);
        record_text(game, detail, sizeof(detail), "SSR_RecordTitles", NULL, NULL, ssr_honours_titles(h, mine->id), 0);
        record_row(game, title, detail, 1);
        const SSRNation *nation = ssr_world_nation(game->world, mine->nation);
        int titles = 0;
        for (int i = 0; i < h->count; i++) {
            const SSRClub *c = ssr_world_club(game->world, h->list[i].club);
            titles += c && c->nation == mine->nation;
        }
        record_text(game, title, sizeof(title), "SSR_RecordNationTitles", nation ? nation->name : "", NULL, 0, 0);
        record_text(game, detail, sizeof(detail), "SSR_RecordTitles", NULL, NULL, titles, 0);
        record_row(game, title, detail, 0);
    }
    // The scorer of the world with the most goals in a career.
    const SSRPerson *best = NULL;
    for (int i = 0; game->people && i < game->people->count; i++) {
        const SSRPerson *p = &game->people->list[i];
        if (!best || p->goals > best->goals) best = p;
    }
    int myGoals = game->career.goals + game->career.intGoals;
    snprintf(title, sizeof(title), "%s", loc(game, "SSR_RecordWorldGoals"));
    if (best && best->goals > myGoals) {
        char name[96];
        snprintf(name, sizeof(name), "%s %s", best->first, best->last);
        record_text(game, detail, sizeof(detail), "SSR_RecordGoals", name, NULL, best->goals, 0);
        record_row(game, title, detail, best->rival);
    } else {
        record_text(game, detail, sizeof(detail), "SSR_RecordGoals", game->career.fullName, NULL, myGoals, 0);
        record_row(game, title, detail, 1);
    }
}

static void honours_fill(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    int career = game->honoursMode == 1;
    // The title of each mode, and the name of the next mode on the button.
    static const char *const TITLES[4] = {"SSR_Honours", "Career", "SSR_HallOfFame", "SSR_Records"};
    ssr_tweaks_set(t, "Honours", "Title", loc(game, TITLES[game->honoursMode % 4]));
    char column[128], seed[16];
    snprintf(column, sizeof(column), "%s", loc(game, career ? "Year" : "SSR_WorldSeedOf"));
    snprintf(seed, sizeof(seed), "%d", game->career.worldSeed);
    replace_all(column, sizeof(column), "$seed", seed);
    ssr_tweaks_set(t, "Honours", "Column", column);
    ssr_tweaks_set(t, "Honours", "ModeText", loc(game, TITLES[(game->honoursMode + 1) % 4]));
    ssr_tweaks_set_number(t, "Honours", "Scroll", 0);
    add_op(game, SSR_OP_LIST_CLEAR, "HonourList", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_START, "HonourList", NULL, NULL, 82);
    if (game->honoursMode == 2) {
        hall_of_fame(game);
        return;
    }
    if (game->honoursMode == 3) {
        records(game);
        return;
    }
    if (career) {
        // The timeline: the newest moment first.
        const SSRCareer *c = &game->career;
        for (int i = c->momentCount - 1; i >= 0; i--) {
            const SSRMoment *m = &c->moments[i];
            char when[128];
            snprintf(when, sizeof(when), "%s %d, %s %d", loc(game, "Year"), ssr_date_year(m->sdate), loc(game, "Week"),
                     ssr_date_week(m->sdate));
            add_op(game, SSR_OP_LIST_ADD, "HonourList", "HonourEntry", "", 0);
            add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, m->text, 1);
            add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, when, 2);
        }
        return;
    }
    for (int i = game->honours.count - 1; i >= 0; i--) {
        const SSRHonour *h = &game->honours.list[i];
        char title[256], scorer[256], number[16];
        snprintf(title, sizeof(title), "%s", loc(game, "SSR_HonourTitle"));
        snprintf(number, sizeof(number), "%d", h->year);
        replace_all(title, sizeof(title), "$year", number);
        replace_all(title, sizeof(title), "$league", h->league);
        replace_all(title, sizeof(title), "$club", h->clubName);
        if (h->lastDay) {
            size_t n = strlen(title);
            snprintf(title + n, sizeof(title) - n, " %s", loc(game, "SSR_LastDay"));
        }
        scorer[0] = 0;
        if (h->scorer[0]) {
            snprintf(scorer, sizeof(scorer), "%s", loc(game, "SSR_HonourScorer"));
            replace_all(scorer, sizeof(scorer), "$name", h->scorer);
            replace_all(scorer, sizeof(scorer), "$club", h->scorerClub);
            snprintf(number, sizeof(number), "%d", h->goals);
            replace_all(scorer, sizeof(scorer), "$goals", number);
        }
        add_op(game, SSR_OP_LIST_ADD, "HonourList", "HonourEntry", "", 0);
        add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, title, 1);
        add_op(game, SSR_OP_LIST_TEXT, "HonourList", NULL, scorer, 2);
        if (h->club == game->career.clubId) add_op(game, SSR_OP_LIST_SUB, "HonourList", "Highlight", NULL, 1);
    }
}

// squad.btn_Honours: the champions and the top scorers, the newest
// season first. A title of the club of the player shows its highlight.
static void honours_screen(SSRGame *game) {
    set_screen(game, "honours");
    game->honoursMode = 0;
    honours_fill(game);
}

static void honours_mode(SSRGame *game) {
    game->honoursMode = (game->honoursMode + 1) % 4;
    honours_fill(game);
}

static void honours_back(SSRGame *game) {
    set_screen(game, "squad");
    squad_fill(game);
}

// The editor: the name of the club of the squad screen goes into the
// world now, and into the data pack "custom" for the next start. The world
// reads the pack "custom" from then on.
static void edit_done(SSRGame *game) {
    game->typing = 0;
    if (!game->editText[0] || !game->world) return;
    // The world belongs to the app, but the editor changes it in place.
    SSRWorld *world = (SSRWorld *)game->world;
    if (!ssr_world_set_club_name(world, game->squadClub, game->editText, NULL)) return;
    ssr_tweaks_set(game->tweaks, "Squad", "Title", game->editText);
    if (!game->packsDirectory[0]) return;
    if (!ssr_pack_make(game->packsDirectory, "custom", "My edits")) return;
    char path[2300];
    snprintf(path, sizeof(path), "%s/custom/world/Clubs.txt", game->packsDirectory);
    if (ssr_world_save_clubs(world, path) && strcmp(game->settings.pack, "custom") != 0) {
        ssr_game_set_pack(game, "custom");
        settings_changed(game);
    }
}

static void squad_edit(SSRGame *game) {
    if (game->typing == 4) {
        edit_done(game);
        return;
    }
    const SSRClub *club = game->world ? ssr_world_club(game->world, game->squadClub) : NULL;
    if (!club || game->squadMode != 0) return;
    game->typing = 4;
    snprintf(game->editText, sizeof(game->editText), "%s", club->name);
}

static void squad_back(SSRGame *game) {
    if (game->typing == 4) edit_done(game);
    league_screen(game, game->leagueComp);
}

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
static int check_transfer_window(SSRGame *game);
static int news_gambling(SSRGame *game);

static void retirement_screen(SSRGame *game);

static void game_play(SSRGame *game) {
    // A retired player sees the retirement screen again.
    if (game->career.retired) {
        retirement_screen(game);
        return;
    }
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
// The awards of the season review (c_TScreen_SeasonReview.Update).
static void season_awards(SSRGame *game, SSRSeasonComp *comp, SSRSeasonClub *club, int position, int year) {
    SSRCareer *c = &game->career;
    if (club->league < comp->data->id && comp->data->type != 4) award(game, 17);
    if (position == 1 && club->league >= comp->data->id && comp->data->type != 4) {
        award(game, 16);
        char text[256];
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentTitle"));
        replace_all(text, sizeof(text), "$league", comp->data->tla);
        replace_all(text, sizeof(text), "$club", club->data->name);
        moment(game, text);
        if (c->captain == 1 || c->captain == 3) award(game, 82);
        if (ssr_career_history_apps(c, club->data->tla) + c->seasonApps == c->apps) award(game, 123);
    }
    if (c->seasonGoals >= 20) award(game, 19);
    if (c->seasonGoals >= 50) award(game, 122);
    if (c->seasonPasses >= 200) award(game, 20);
    if (c->seasonAssists >= 20) award(game, 21);
    if (c->seasonStarMan >= 20) award(game, 22);
    if (year + 1 == 10) award(game, 50);
    const SSRTableRow *row = ssr_season_row(game->season, &comp->pools[0], c->clubId);
    if (row) {
        if (row->lost == 0) award(game, 120);
        if (row->played == row->won) award(game, 121);
    }
    if (position == 1 && club->league >= comp->data->id) {
        if (ssr_career_count_medal_teams(c, 1) >= 3) award(game, 113);
        if (ssr_career_count_medals(c, 3, year) > 0) award(game, 125);
    }
}

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
    upper(game, text);
    ssr_tweaks_set(t, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 1500);
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
    // The place of the club in the trophy log, and the row of the season
    // in the stat history.
    int year = ssr_career_year(c) - 1;
    char trophy[64];
    snprintf(trophy, sizeof(trophy), "%s %s", comp->data->tla, place);
    int medal = position != 1 ? 0 : club->league < comp->data->id ? 2 : 1;
    ssr_career_add_trophy(c, year, club->data->tla, trophy, medal);
    season_awards(game, comp, club, position, year);
    // [I: the game adds a row each time that the screen opens. The remake
    // adds one row for the season.]
    const SSRStatRow *last = c->historyCount ? &c->history[c->historyCount - 1] : NULL;
    if (!last || last->year != year || strcmp(last->club, club->data->tla) != 0) {
        ssr_career_add_history(c, year, club->data->tla);
        // AddCareerForm: the mean rating of the season.
        ssr_career_add_form(c->careerForm, apps ? (int)(c->seasonTotalRating / (float)apps) : 0);
    }
}

// The end of the season: PlayToDate reached the new year.
// The champion and the top scorer of each league of the season go into
// the honours.
static void keep_honours(SSRGame *game) {
    SSRSeason *s = game->season;
    if (!s || !game->world) return;
    int year = ssr_career_year(&game->career) - 1;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *comp = &s->comps[i];
        if (comp->data->level != 0 || comp->data->type != 0 || comp->poolCount == 0 || comp->pools[0].count == 0) continue;
        SSRPool *pool = &comp->pools[0];
        ssr_season_sort_pool(s, pool, 4);
        SSRHonour h;
        memset(&h, 0, sizeof(h));
        h.year = year;
        snprintf(h.league, sizeof(h.league), "%s", comp->data->tla);
        h.club = s->rows[pool->rows[0]].teamId;
        int before = s->rows[pool->rows[0]].prevPosition;
        h.lastDay = before > 1;
        const SSRClub *club = ssr_world_club(game->world, h.club);
        snprintf(h.clubName, sizeof(h.clubName), "%s", club ? club->name : "");
        int clubs[64], n = 0;
        for (int k = 0; k < pool->count && n < 64; k++) clubs[n++] = s->rows[pool->rows[k]].teamId;
        SSRPerson *top = NULL;
        if (game->people && ssr_people_top_scorers(game->people, clubs, n, &top, 1)) {
            snprintf(h.scorer, sizeof(h.scorer), "%s %s", top->first, top->last);
            const SSRClub *of = ssr_world_club(game->world, top->club);
            snprintf(h.scorerClub, sizeof(h.scorerClub), "%s", of ? of->name : "");
            h.goals = top->seasonGoals;
        }
        // The player of the user can be the top scorer.
        if (game->career.seasonGoals > h.goals && ssr_season_actual_league(s, game->career.clubId) == comp->data->id) {
            snprintf(h.scorer, sizeof(h.scorer), "%s", game->career.fullName);
            const SSRClub *mine = ssr_world_club(game->world, game->career.clubId);
            snprintf(h.scorerClub, sizeof(h.scorerClub), "%s", mine ? mine->name : "");
            h.goals = game->career.seasonGoals;
        }
        ssr_honours_add(&game->honours, &h);
        // The first two clubs raced for the title: their rivalry grows, and
        // more after a close race.
        const SSRTableRow *first = &s->rows[pool->rows[0]], *second = &s->rows[pool->rows[1 < pool->count ? 1 : 0]];
        if (first != second) {
            ssr_honours_add_rivalry(&game->honours, first->teamId, second->teamId,
                                    first->points - second->points <= 3 ? 3 : 2);
        }
    }
}

// The managers of the clubs in the relegation places, or in the bottom
// three of a league with no relegation, can go. A new manager of the club
// of the player starts with a boss relation of 50.
static void judge_managers(SSRGame *game) {
    SSRSeason *s = game->season;
    if (!s || !game->people || !game->names) return;
    int bottom[256], n = 0;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *comp = &s->comps[i];
        if (comp->data->level != 0 || comp->data->type != 0 || comp->poolCount == 0) continue;
        SSRPool *pool = &comp->pools[0];
        ssr_season_sort_pool(s, pool, 4);
        for (int k = 0; k < pool->count && n < 256; k++) {
            int place = k + 1;
            int low = ssr_season_is_relegation_place(s, comp, place) || place > pool->count - 3;
            if (low) bottom[n++] = s->rows[pool->rows[k]].teamId;
        }
    }
    int year = ssr_career_year(&game->career);
    ssr_people_judge_managers(game->people, game->world, game->names, bottom, n, year);
    for (int k = 0; k < game->people->sackedCount; k++) {
        if (game->people->sacked[k] != game->career.clubId) continue;
        const SSRManager *m = ssr_people_manager(game->people, game->career.clubId);
        game->career.relationBoss = 50;
        char text[256], name[64];
        snprintf(name, sizeof(name), "%s %s", m->first, m->last);
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentManager"));
        replace_all(text, sizeof(text), "$name", name);
        moment(game, text);
    }
}

static void season_end(SSRGame *game) {
    keep_honours(game);
    judge_managers(game);
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
    if (won) {
        c->potyAwards++;
        // The award goes into the trophy log, with the tla of the club for
        // the club award and of the league for the others.
        static const char *const TAGS[5] = {"poty_ClubPOTY", "poty_YoungPOTY", "poty_LeaguePOTY", "poty_DivisionPOTY",
                                            "poty_WorldPOTY"};
        int kind = type == 1 ? 0 : type == 2 ? 1 : type == 3 ? (top ? 2 : 3) : 4;
        const SSRSeasonClub *mine = ssr_season_club(s, c->clubId, 0);
        const char *team = type == 1 ? (mine ? mine->data->tla : "") : comp ? comp->data->tla : "";
        ssr_career_add_trophy(c, ssr_career_year(c) - 1, team, loc(game, TAGS[kind]), 6 + kind);
        char text[256];
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentAward"));
        replace_all(text, sizeof(text), "$award", loc(game, TAGS[kind]));
        moment(game, text);
        // m_ach: the club 69, the young 68, the division 70 (and 71 in the
        // top division), the world 72. Three world awards give 114.
        award(game, type == 1 ? 69 : type == 2 ? 68 : type == 3 ? 70 : 72);
        if (type == 3 && top) award(game, 71);
        if (ssr_career_count_medals(c, 10, 0) >= 3) award(game, 114);
    }
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
    // A new market of horses for the year. [I: the year of the date at this
    // call.]
    const SSRCareer *c = &game->career;
    if (game->horsesLoaded) {
        ssr_horses_reset_for_sale(&game->horses, ssr_career_year(c) + (int)strlen(c->fullName) + c->clubNationId);
    }
    ssr_tweaks_set_number(t, "Menu", "DisableQuitButton", 0);
    skip_to_first_match(game);
    // ResetSeasonStats: the clock of the loss of each skill starts again,
    // a week more or less from today.
    for (int r = 0; r < 5; r++) game->career.deplete[r] = game->career.sdate + ssr_rand2(&game->random, -7, 7);
    ssr_tweaks_set_number(t, "Menu", "CanSavePlayer", 1);
    ssr_career_reset_season(&game->career);
    if (game->people) {
        ssr_people_new_season(game->people, game->world, game->names, ssr_career_year(&game->career));
        // A good team spirit helps the young player that the player
        // mentors: 3 more skill points each season.
        SSRPerson *young = mentee(game);
        if (young && game->career.relationTeam >= 60) {
            young->skill = young->skill + 3 > young->potential ? young->potential : young->skill + 3;
        }
        // A transfer between two clubs makes their rivalry grow.
        for (int i = 0; i < game->people->moveCount; i++) {
            ssr_honours_add_rivalry(&game->honours, game->people->moves[i][1], game->people->moves[i][2], 1);
        }
    }
    save_career(game);
    player_tweaks(game);
    star_transition(game, "gamemenu");
}

// playeroftheyear.btn_Play: GSSeasonEndTransfer asks about a transfer.
static void poty_play(SSRGame *game) {
    message(game, "message", loc(game, "CMESSAGE_SEASONENDTRANSFER"), 1, 1, "SeasonEndTransfer");
}

static void do_news(SSRGame *game, const char *tag, const char *team, const char *team2);
static const char *my_club_tla(SSRGame *game);

// playeroftheyear.btn_Retire: c_TScreen_PlayerOfTheYear.ButtonRetire.
static void poty_retire(SSRGame *game) {
    char text[1024];
    snprintf(text, sizeof(text), "%s %s", loc(game, "CMESSAGE_RETIREMENTCHECK"), loc(game, "CMESSAGE_LOSESTARBUX"));
    message(game, "message", text, 1, 1, "Retirement");
}

static void replace_number(char *text, size_t capacity, const char *name, int value) {
    char number[16];
    snprintf(number, sizeof(number), "%d", value);
    replace_all(text, capacity, name, number);
}

// The call "Retirement": Yes is c_TPlayer.Retire, the news and then the
// retirement screen. No comes back to the awards as they were.
static void call_retirement(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState != 1) {
        for (int award = 0; award < 4; award++) {
            if (award != 1 || ssr_career_age(c) <= 23) show_award_row(game, award);
        }
        add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
        add_op(game, SSR_OP_SHOW, "RetireButton", NULL, NULL, 0);
        return;
    }
    do_news(game, "CNEWS_RETIREMENT", my_club_tla(game), NULL);
    replace_number(c->headline, sizeof(c->headline), "$years", ssr_career_year(c) - 1);
    replace_number(c->headline, sizeof(c->headline), "$num", c->goals + c->intGoals);
    replace_number(c->headline, sizeof(c->headline), "$age", ssr_career_age(c) - 1);
    webpage_to(game, "retirement", 0);
}

static const char *const RETIRE_STATS[9] = {"CareerMatches", "CareerAverageGoals", "CareerAverageAssists",
                                            "CareerStarMan", "CareerAverageRating", "CareerMedals",
                                            "CareerPlayerOfYear", "CareerAwards", "StarRating"};

// The ref of the row of a stat. The row of the awards has its own spelling.
static const char *retire_ref(const char *name) {
    return strcmp(name, "CareerPlayerOfYear") == 0 ? "CareerPlayerofYear" : name;
}

// GetStringNumberWithCommas: 1234567 is "1,234,567".
static void number_with_commas(int value, char *out, size_t capacity) {
    char digits[16], text[24];
    snprintf(digits, sizeof(digits), "%d", value < 0 ? -value : value);
    size_t n = strlen(digits), o = 0;
    if (value < 0) text[o++] = '-';
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) text[o++] = ',';
        text[o++] = digits[i];
    }
    text[o] = 0;
    snprintf(out, capacity, "%s", text);
}

// c_AScreen_Retirement.SetScore: the score with commas.
static void retirement_score(SSRGame *game, int score) {
    char text[24];
    number_with_commas(score, text, sizeof(text));
    ssr_tweaks_set(game->tweaks, "PlayerStats", "CareerScore", text);
}

// c_TScreen_Retirement.SetUpScreen: the stats of the career and the New
// Star Soccer score.
static void retirement_screen(SSRGame *game) {
    SSRCareer *c = &game->career;
    set_screen(game, "retirement");
    game->retireStage = 0;
    game->retireTime = game->now;
    int apps = c->apps + c->intApps;
    float goals = (float)(c->goals + c->intGoals), assists = (float)(c->assists + c->intAssists);
    if (goals > 0) goals /= apps;
    if (assists > 0) assists /= apps;
    float average = c->apps > 0 ? c->totalRating / c->apps : 0;
    int star = ssr_career_star_level(c), medals = 0, achievements = ssr_career_award_count(c);
    float bonus = 0, history = 0, historyGoals = 0, historyAssists = 0, historyStarMan = 0;
    // The medals: a league title, a trophy with a medal, or a final that
    // was won. A medal before the year 25 and a medal of the nation give a
    // bonus each.
    char first[64], winner[64], winnerCaps[64];
    snprintf(first, sizeof(first), " %s", loc(game, "position_1"));
    snprintf(winner, sizeof(winner), "%s", loc(game, "Winner"));
    snprintf(winnerCaps, sizeof(winnerCaps), "%s", winner);
    for (char *p = winnerCaps; *p; p++) *p = (char)toupper((unsigned char)*p);
    const SSRNation *nation = ssr_world_nation(game->world, c->nationId);
    for (int i = 0; i < c->trophyCount; i++) {
        const SSRTrophy *t = &c->trophies[i];
        size_t n = strlen(t->tournament), k = strlen(first);
        int title = n >= k && strcmp(t->tournament + n - k, first) == 0;
        if (!(title || t->medal > 0 || strstr(t->tournament, winnerCaps) || strstr(t->tournament, winner))) continue;
        medals++;
        if (t->year < 25) bonus += 1;
        if (nation && nation->tla && strcmp(t->team, nation->tla) == 0) bonus += 1;
    }
    // The sums of the history before the year 25.
    for (int i = 0; i < c->historyCount; i++) {
        const SSRStatRow *h = &c->history[i];
        if (h->year >= 25) continue;
        history += (float)h->apps;
        historyGoals += (float)h->goals;
        historyAssists += (float)h->assists;
        historyStarMan += (float)h->starMan;
    }
    if (history < 1) history = 1;
    float stats[9] = {(float)apps, goals, assists, (float)(c->starMan + c->intStarMan), average, (float)medals,
                      (float)c->potyAwards, (float)achievements, (float)star};
    memcpy(game->retireStats, stats, sizeof(stats));
    int score = (int)(historyGoals / history * 100.0f + 1.0f);
    score = (int)(score + historyAssists / history * 50.0f + 1.0f);
    score = (int)(score + historyStarMan / history * 200.0f + 1.0f);
    score = (int)(score * (average + 1.0f) * 200.0f);
    score = (int)(score + bonus * 250.0f);
    score += c->potyAwards * 250;
    score += achievements * 200;
    score += (star < 1 ? 1 : star > 100 ? 100 : star) * 200;
    score = (int)(score * (career_skills(c) / 100.0f + 1.0f + ssr_career_happiness(c) / 100.0f +
                           ssr_career_lifestyle(c) / 100.0f));
    game->retireScore = score * 10;
    // The bars of the screen read the live tweaks of the player.
    ssr_tweaks_set_number(game->tweaks, "Player", "Happiness", ssr_career_happiness(c));
    ssr_tweaks_set_number(game->tweaks, "Player", "Lifestyle", ssr_career_lifestyle(c));
    ssr_tweaks_set_number(game->tweaks, "Player", "Skills", career_skills(c));
    // c_AScreen_Retirement.HideStats.
    retirement_score(game, 0);
    for (int i = 0; i < 9; i++) {
        add_op(game, SSR_OP_HIDE, retire_ref(RETIRE_STATS[i]), NULL, NULL, 0);
    }
}

// c_TScreen_Retirement.Update: one stat each stage, then the score.
static void retirement_tick(SSRGame *game) {
    int step = game->touchWasDown ? 100 : 1000;
    if (game->retireStage >= 20 || game->now <= game->retireTime + step) return;
    game->retireTime = game->now;
    int stage = ++game->retireStage;
    if (stage >= 1 && stage <= 9) {
        const char *name = RETIRE_STATS[stage - 1];
        ssr_tweaks_set_number(game->tweaks, "PlayerStats", name, game->retireStats[stage - 1]);
        play_sound(game, "CStat");
        add_op(game, SSR_OP_SHOW, retire_ref(name), NULL, NULL, 0);
        // The modern look has no bars 10 to 12.
        if (stage == 9) game->retireStage = 12;
    } else if (stage == 13) {
        play_sound(game, "AudienceCheer");
        retirement_score(game, game->retireScore);
    }
}

// retirement.btn_Play: c_TScreen_Retirement.ButtonPlay. The first visit
// keeps the score and saves the career one day back.
static void retirement_play(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (!c->retired) {
        ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 1);
        // The testimonial match: the fans of the last club say goodbye. A
        // star with good fans fills more seats.
        char text[256], number[16];
        snprintf(text, sizeof(text), "%s", loc(game, "SSR_MomentTestimonial"));
        replace_all(text, sizeof(text), "$club", club_name(game, c->clubId));
        snprintf(number, sizeof(number), "%d", 500 * ssr_career_star_level(c) + (int)c->relationFans * 100);
        replace_all(text, sizeof(text), "$num", number);
        moment(game, text);
        c->retired = game->retireScore;
        c->sdate--;
        save_career(game);
    }
    ssr_tweaks_set_number(game->tweaks, "Menu", "DisableQuitButton", 0);
    game->seasonEndMode = 0;
    game_menu(game);
}

static void desired_transfer_screen(SSRGame *game, int nation);

// The call "SeasonEndTransfer": No keeps the club, and Yes looks for a
// new club.
static void call_season_end_transfer(SSRGame *game) {
    if (game->returnState != 1) {
        new_season(game);
        return;
    }
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    desired_transfer_screen(game, club ? club->nation : game->career.clubNationId);
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
    SSRChanceSetup s = ssr_match_chance_setup(game->match, boot_skill(c, 2), boot_skill(c, 1), 1);
    s.pace = boot_skill(c, 0);
    s.careerGoals = (float)c->goals;
    s.careerPasses = (float)c->passes;
    // c_TBall_Type.UpdateMatchBallImages: the factors of the ball of the
    // match.
    int ball = ssr_career_selected_ball(&game->career, game->matchBallType);
    ssr_ball_type_factors(ball, &s.ballSpeed, &s.ballBounce, &s.ballWeight, &s.ballCurl);
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
                // A star that the skill takes is a training of the skill.
                c->lastTrained[row] = c->deplete[row] = c->sdate;
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

static void retirement_screen(SSRGame *game);

// c_TScreen_WebPage.ButtonOk: the screen after the news.
static void webpage_ok(SSRGame *game) {
    char next[32];
    snprintf(next, sizeof(next), "%s", game->newsNext);
    game->newsNext[0] = 0;
    if (!next[0] || strcmp(next, "gamemenu") == 0) game_menu(game);
    else if (strcmp(next, "relationships") == 0) relationships_screen(game, 0, 0);
    else if (strcmp(next, "leaguefixtures") == 0) leaguefixtures_play(game);
    else if (strcmp(next, "retirement") == 0) retirement_screen(game);
    else set_screen(game, next);
}

static void relationships(SSRGame *game) {
    relationships_screen(game, 1, 0);
}

/* ---------------------------------------------------- the achievements */

// c_TPlayer.CheckAchievement: a locked award unlocks, with an alert and a
// sound. An award gives nothing else.
static void award(SSRGame *game, int id) {
    if (!ssr_career_unlock_award(&game->career, id)) return;
    char tag[48];
    snprintf(tag, sizeof(tag), "%d", id);
    ssr_tweaks_set_number(game->tweaks, "Achievements", tag, 1);
    snprintf(tag, sizeof(tag), "CACHIEVEMENTMOBILE_%d", id);
    alert(game, "bottommessageachievement", loc(game, tag), 4000);
    play_sound(game, "CAchievement");
}

// c_ASection_Awards.SetUp: the share of the awards with a text that the
// player has.
static void award_share(SSRGame *game) {
    const SSRCareer *c = &game->career;
    int all = 0, done = 0;
    for (int id = 1; id < SSR_CAREER_AWARDS; id++) {
        char tag[48];
        snprintf(tag, sizeof(tag), "CACHIEVEMENTMOBILE_%d", id);
        const char *text = loc(game, tag);
        if (strcmp(text, tag) == 0 || strchr(text, '@')) continue;
        all++;
        done += c->awards[id] > 0;
    }
    int share = all > 0 ? done * 100 / all : 0;
    char text[128];
    ssr_tweaks_set_number(game->tweaks, "Player", "AchievementsComplete", share);
    snprintf(text, sizeof(text), "%s%d%%", loc(game, "ACHIEVEMENTS_CAPS_SPACEAFTER"), share);
    ssr_tweaks_set(game->tweaks, "Player", "AchievementsCompleteString", text);
}

// Player.GetStringForm: the ratings of the last matches, the oldest first,
// with "-" between them.
static void form_string(const int *form, int apps, char *out, size_t capacity) {
    size_t n = 0;
    out[0] = 0;
    for (int i = apps < 5 ? 5 - apps : 0; i <= 4 && n < capacity; i++) {
        n += (size_t)snprintf(out + n, capacity - n, "%s%d", n ? "-" : "", form[i]);
    }
}

static void player_stat(SSRGame *game, const char *name, float season, float career, float international) {
    char key[64];
    snprintf(key, sizeof(key), "Season%s", name);
    ssr_tweaks_set_number(game->tweaks, "PlayerStats", key, season);
    snprintf(key, sizeof(key), "Career%s", name);
    ssr_tweaks_set_number(game->tweaks, "PlayerStats", key, career);
    snprintf(key, sizeof(key), "International%s", name);
    ssr_tweaks_set_number(game->tweaks, "PlayerStats", key, international);
}

static void player_string_stat(SSRGame *game, const char *name, const char *season, const char *career,
                               const char *international) {
    char key[64];
    snprintf(key, sizeof(key), "Season%s", name);
    ssr_tweaks_set(game->tweaks, "PlayerStats", key, season);
    snprintf(key, sizeof(key), "Career%s", name);
    ssr_tweaks_set(game->tweaks, "PlayerStats", key, career);
    snprintf(key, sizeof(key), "International%s", name);
    ssr_tweaks_set(game->tweaks, "PlayerStats", key, international);
}

// GetStringCareerForm: the ratings above 0, or the last four when the
// text is longer than 12 characters.
static void career_form_string(const int *form, char *out, size_t capacity) {
    char text[64] = "", part[80];
    if (form[4] > 0) snprintf(text, sizeof(text), "%d", form[4]);
    for (int i = 3; i >= 0; i--) {
        if (form[i] <= 0) continue;
        snprintf(part, sizeof(part), "%d-%s", form[i], text);
        snprintf(text, sizeof(text), "%s", part);
    }
    if (strlen(text) <= 12) snprintf(out, capacity, "%s", text);
    else snprintf(out, capacity, "%d-%d-%d-%d", form[1], form[2], form[3], form[4]);
}

// TweakInterface.UpdatePlayerStats.
static void update_player_stats(SSRGame *game) {
    const SSRCareer *c = &game->career;
    player_stat(game, "Matches", c->seasonApps, c->apps, c->intApps);
    player_stat(game, "Goals", c->seasonGoals, c->goals, c->intGoals);
    player_stat(game, "HatTricks", c->seasonHatTricks, c->hatTricks, c->intHatTricks);
    player_stat(game, "Passes", c->seasonPasses, c->passes, c->intPasses);
    player_stat(game, "Assists", c->seasonAssists, c->assists, c->intAssists);
    player_stat(game, "StarMan", c->seasonStarMan, c->starMan, c->intStarMan);
    static const char *const FURTHEST[4] = {"FurthestGoal", "FurthestPass", "FurthestHeadedGoal", "FurthestHeadedPass"};
    for (int i = 0; i < 4; i++) {
        char season[48], career[48], international[48];
        distance_string(game, c->seasonFurthest[i], 1, season, sizeof(season));
        distance_string(game, c->furthest[i], 1, career, sizeof(career));
        distance_string(game, c->intFurthest[i], 1, international, sizeof(international));
        player_string_stat(game, FURTHEST[i], season, career, international);
    }
    player_stat(game, "AverageRating", c->seasonApps > 0 ? c->seasonTotalRating / (float)c->seasonApps : 0,
                c->apps > 0 ? c->totalRating / (float)c->apps : 0,
                c->intApps > 0 ? c->intTotalRating / (float)c->intApps : 0);
    char season[64], international[64];
    form_string(c->form, c->seasonApps, season, sizeof(season));
    form_string(c->intForm, c->intApps, international, sizeof(international));
    char career[64];
    career_form_string(c->careerForm, career, sizeof(career));
    player_string_stat(game, "Form", season, career, international);
}

// c_TScreen_Achievements.SetUpScreen and c_AScreen_Achievements.OnActive:
// the stats of the career, the trophies, the clubs, and the awards.
static void achievements_screen(SSRGame *game) {
    const SSRCareer *c = &game->career;
    set_screen(game, "achievements");
    update_player_stats(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "ShowGamecenter", 0);
    award_share(game);
    // c_ASection_Trophies: the newest first.
    add_op(game, SSR_OP_LIST_CLEAR, "TrophyHook", NULL, NULL, 0);
    if (c->trophyCount == 0) add_op(game, SSR_OP_LIST_ADD, "TrophyHook", "NoRecords", "", 0);
    for (int i = c->trophyCount - 1; i >= 0; i--) {
        const SSRTrophy *t = &c->trophies[i];
        char year[16];
        snprintf(year, sizeof(year), "%d", t->year);
        add_op(game, SSR_OP_LIST_ADD, "TrophyHook", "TrophiesStatsListItem", "", 0);
        add_op(game, SSR_OP_LIST_TEXT, "TrophyHook", NULL, year, 1);
        add_op(game, SSR_OP_LIST_TEXT, "TrophyHook", NULL, t->team, 2);
        add_op(game, SSR_OP_LIST_TEXT, "TrophyHook", NULL, t->tournament, 3);
    }
    // c_ASection_ClubStats: the part of the season now, then the history,
    // the newest first. A row of year 0 does not show.
    add_op(game, SSR_OP_LIST_CLEAR, "ClubStats", NULL, NULL, 0);
    int apps, goals, assists;
    ssr_career_history_totals(c, &apps, &goals, &assists, NULL);
    for (int i = c->historyCount; i >= 0; i--) {
        SSRStatRow row;
        if (i == c->historyCount) {
            memset(&row, 0, sizeof(row));
            row.year = ssr_career_year(c);
            snprintf(row.club, sizeof(row.club), "%s", my_club_tla(game));
            row.apps = c->apps - apps;
            row.goals = c->goals - goals;
            row.assists = c->assists - assists;
        } else {
            row = c->history[i];
            if (row.year == 0) continue;
        }
        char numbers[4][16];
        snprintf(numbers[0], sizeof(numbers[0]), "%d", row.year);
        snprintf(numbers[1], sizeof(numbers[1]), "%d", row.apps);
        snprintf(numbers[2], sizeof(numbers[2]), "%d", row.assists);
        snprintf(numbers[3], sizeof(numbers[3]), "%d", row.goals);
        add_op(game, SSR_OP_LIST_ADD, "ClubStats", "ClubStatsListItem", "", 0);
        add_op(game, SSR_OP_LIST_TEXT, "ClubStats", NULL, numbers[0], 1);
        add_op(game, SSR_OP_LIST_TEXT, "ClubStats", NULL, row.club, 2);
        add_op(game, SSR_OP_LIST_TEXT, "ClubStats", NULL, numbers[1], 3);
        add_op(game, SSR_OP_LIST_TEXT, "ClubStats", NULL, numbers[2], 4);
        add_op(game, SSR_OP_LIST_TEXT, "ClubStats", NULL, numbers[3], 5);
    }
}

// c_TScreen_Achievements.ButtonHome: back to the relationships.
static void achievements_home(SSRGame *game) {
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
    play_sound(game, "CError");
    char text[256], number[16];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_SPENTTOOMUCHTIME"));
    snprintf(number, sizeof(number), "%d", MAX_RELATION_ACTIVITIES);
    replace_all(text, sizeof(text), "$num", number);
    if (strchr(text, '@')) return;
    message(game, "message", text, 0, 1, NULL);
}

static void nrg_shop(SSRGame *game);

// GSPlayerNotEnoughEnergy: the NRG shop when the player has no can, else
// the overlay "upsellnrg" that offers the next can. The remake has no
// adverts, so the button of the free NRG stays hidden.
static void not_enough_energy(SSRGame *game, const char *activity, float need) {
    play_sound(game, "CError");
    float v = need - game->career.energy;
    ssr_tweaks_set_number(game->tweaks, "WatchAdNRG", "RefillAmount", v < 1 ? 1 : v);
    ssr_tweaks_set_number(game->tweaks, "WatchAdNRG", "ShowNRGAdReward", 0);
    ssr_tweaks_set(game->tweaks, "WatchAdNRG", "AdShowedFor", activity);
    if (next_can(&game->career) < 0) {
        nrg_shop(game);
    } else {
        snprintf(game->overlay, sizeof(game->overlay), "upsellnrg");
    }
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
            play_sound(game, "CError");
            message(game, partner_screen(game), loc(game, text), 0, 1, "DoRelationScreen");
            return;
        }
    }
    if (bar == 4 && c->relationSponsors == 0) {
        play_sound(game, "CError");
        message(game, "messagesponsor", loc(game, "CMESSAGE_NOSPONSORS"), 0, 1, "DoRelationScreen");
        return;
    }
    if (!activity_allowed(game)) {
        // The partner and the sponsors play the error two times.
        if (bar >= 3) play_sound(game, "CError");
        return;
    }
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
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 2000);
}

// The fuse parameter dilemma_not_chosen_decay of 4.32.
#define DILEMMA_DECAY -10.0f

static void choose_dilemma(SSRGame *game, int item) {
    if (game->now < game->splashEnd || game->dilemmaChosen) return;
    SSRCareer *c = &game->career;
    int chosen = game->dilemmaRelation[item], other = game->dilemmaRelation[1 - item];
    if (chosen == 3) c->lastSpendTimePartner = (float)c->sdate;
    update_relation(game, (SSRRelation)chosen, 5.0f);
    update_relation(game, (SSRRelation)other, DILEMMA_DECAY);
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

// The names of the skills, as the tweaks of the group Player and the text
// tags have them.
static const char *const SKILL_NAMES[5] = {"Pace", "Power", "Technique", "Vision", "SetPieces"};

// AScreen_Message.SetUpSkillBar: the bar of the message shows the skill,
// out of 60, and its name with the change.
static void skill_bar(SSRGame *game, const char *skill, int change) {
    add_op(game, SSR_OP_SHOW, "OneBar", NULL, NULL, 0);
    char source[64], text[96];
    snprintf(source, sizeof(source), "Player,%s", skill);
    add_op(game, SSR_OP_BAR_SOURCE, "BarAPDoodad", NULL, source, 60);
    add_op(game, SSR_OP_BAR_SOURCE, "BarPBDoodad", NULL, source, 60);
    add_op(game, SSR_OP_BAR_SOURCE, "ClippedBarPBDoodad", NULL, source, 60);
    if (change != 0) {
        snprintf(text, sizeof(text), "%s %d", loc(game, skill), change);
    } else {
        snprintf(text, sizeof(text), "%s", loc(game, skill));
    }
    add_op(game, SSR_OP_TEXT, "BarText", NULL, text, 0);
}

static int deplete_skill(int *levels, int count);

// GetLastTrainedLimit: the days that a skill keeps with no training. The
// first year has no loss.
static int last_trained_limit(const SSRCareer *c) {
    int year = ssr_career_year(c);
    if (year == 1) return 999;
    int weeks = 16 - year / 2;
    return (weeks < 2 ? 2 : weeks > 16 ? 16 : weeks) * 7;
}

// The message of the trainer with the bars of the skills: one bar for one
// skill, else the list SkillsDepletion (AScreen_Message.SetUpBar).
static void trainer_message(SSRGame *game, const char *text, const int flags[5]) {
    message(game, "messagetrainer", text, 0, 1, NULL);
    int count = 0, last = 0;
    for (int r = 0; r < 5; r++) {
        if (flags[r]) {
            count++;
            last = r;
        }
    }
    if (count == 1) {
        skill_bar(game, SKILL_NAMES[last], 0);
        return;
    }
    add_op(game, SSR_OP_SHOW, "SkillsDepletion", NULL, NULL, 0);
    int height = (int)ssr_tweaks_number(game->tweaks, "Message", "SkillBarHeight");
    if (height <= 0) height = 65;
    for (int r = 0, k = 0; r < 5; r++) {
        if (!flags[r]) continue;
        char source[64];
        snprintf(source, sizeof(source), "Player,%s", SKILL_NAMES[r]);
        add_op(game, SSR_OP_LIST_START, "SkillsScroll", NULL, NULL, k++ * height);
        add_op(game, SSR_OP_LIST_ADD_AT, "SkillsScroll", "SkillItem", "", 0);
        add_op(game, SSR_OP_ENTRY_BAR, "SkillsScroll", "SkillItemBar", source, 60);
        add_op(game, SSR_OP_LIST_SUB, "SkillsScroll", "SkillItemText", loc(game, SKILL_NAMES[r]), 1);
    }
}

// CheckSkillDeplete: with no trainer, a skill that waited too long loses
// its top level. With a trainer, a skill loses only after a warning.
static int check_skill_deplete(SSRGame *game) {
    SSRCareer *c = &game->career;
    int trainer = ssr_career_trainer(c) > 0, limit = last_trained_limit(c), flags[5] = {0}, any = 0;
    for (int r = 0; r < 5; r++) {
        int start = c->deplete[r] < 0 ? -c->deplete[r] : c->deplete[r];
        if (!((trainer && c->deplete[r] < 0) || (!trainer && c->sdate > start + limit))) continue;
        if (deplete_skill(skill_levels(c, r), 1) > 0) {
            c->deplete[r] = c->sdate;
            flags[r] = 1;
            any = 1;
        }
    }
    if (!any) return 0;
    c->expiredBoots = -1;
    skill_tweaks(game);
    char text[1024];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_LASTTRAINED"));
    if (!trainer) {
        size_t n = strlen(text);
        snprintf(text + n, sizeof(text) - n, "|(%s)", loc(game, "CMESSAGE_LASTTRAINED2"));
    }
    trainer_message(game, text, flags);
    save_career(game);
    return 1;
}

// ShowTrainerWarning: the trainer warns of each skill that waited too
// long. The sign of its day marks the warning.
static int show_trainer_warning(SSRGame *game) {
    SSRCareer *c = &game->career;
    int limit = last_trained_limit(c), flags[5] = {0}, any = 0;
    for (int r = 0; r < 5; r++) {
        if (c->sdate > c->deplete[r] + limit && skill_levels(c, r)[1] > 0) {
            c->deplete[r] = -c->deplete[r];
            flags[r] = 1;
            any = 1;
        }
    }
    if (any) trainer_message(game, loc(game, "CMESSAGE_TRAINERWARNING"), flags);
    return any;
}

// CheckTrainerWarning: once each match week, on the game menu after the
// match. A skill with no day of training starts its clock now, a week
// more or less.
static int check_trainer_warning(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->newsDone) return 0;
    for (int r = 0; r < 5; r++) {
        if (c->lastTrained[r] != 0) continue;
        int day = c->sdate + ssr_rand2(&game->random, -7, 7);
        c->lastTrained[r] = c->deplete[r] = day;
    }
    if (check_skill_deplete(game)) return 1;
    return ssr_career_trainer(c) > 0 && show_trainer_warning(game);
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
    message(game, "messagedefault", text, 0, 1, NULL);
    skill_bar(game, SKILL_NAMES[skill - 1], -lost);
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
            update_relation(game, (SSRRelation)game->messageData, 5.0f);
            set_energy(game, c->energy - 25);
        } else {
            update_relation(game, (SSRRelation)game->messageData, -10.0f);
        }
    } else if (strcmp(call, "VehicleBorrowedTeam") == 0) {
        update_relation(game, SSR_RELATION_TEAM, state == 1 ? 5.0f : -15.0f);
    } else if (strcmp(call, "VehicleBorrowedGirl") == 0) {
        update_relation(game, SSR_RELATION_PARTNER, state == 1 ? 5.0f : -15.0f);
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
        update_relation(game, SSR_RELATION_PARTNER, -5);
        if (c->relationTeam > c->relationBoss && c->relationTeam > c->relationFans) {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGETEAM", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            update_relation(game, SSR_RELATION_TEAM, -c->relationTeam / 2.0f);
        } else if (c->relationFans > c->relationBoss && c->relationFans > c->relationTeam) {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGEFANS", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            update_relation(game, SSR_RELATION_FANS, -c->relationFans / 2.0f);
        } else {
            do_news(game, "CMESSAGE_GIRLFRIENDRAMPAGEBOSS", my_club_tla(game), NULL);
            webpage_to(game, "relationships", 0);
            update_relation(game, SSR_RELATION_BOSS, -c->relationBoss / 2.0f);
        }
    } else {
        update_relation(game, SSR_RELATION_PARTNER, -10);
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
            upper(game, name);
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
            update_relation(game, SSR_RELATION_TEAM, -5);
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
    award(game, 43);
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
    update_relation(game, SSR_RELATION_BOSS, d);
    update_relation(game, SSR_RELATION_BOSS_INT, d);
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
    update_relation(game, r, d);
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


static void shop_price(char *out, size_t capacity, int price);

// c_TPlayer.CheckOldProBoots: an ex pro gives the player old boots, once
// after 8 matches and once after 24.
static int news_old_pro_boots(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->bootId > 0) return 0;
    if (c->tryBoots > 0) c->tryBoots = 0;
    // OldProBoots.OldProBoots1Index and OldProBoots2Index.
    int first = 21, second = 25, id = 0;
    if (c->oldProBoots == first) id = c->apps > 24 ? second : 0;
    else if (c->oldProBoots != second && c->apps > 8) id = first;
    const SSRBoot *boot = ssr_shop_boot(id);
    if (!boot) return 0;
    c->oldProBoots = id;
    char text[512], name[64];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_OLDPROBOOTS"));
    snprintf(name, sizeof(name), "Title_%s", boot->name);
    replace_all(text, sizeof(text), "$name", loc(game, name));
    replace_all(text, sizeof(text), "$num", "5");
    game->messageData = id;
    message(game, "message", text, 1, 1, "OldProBoots");
    return 1;
}

static void call_old_pro_boots(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState == 1) {
        c->bootId = game->messageData;
        c->bootUses = 5;
        c->tryBoots = 1;
        ssr_tweaks_set(game->tweaks, "Products", "PurchaseOverlayMessage", loc(game, "Item_Received"));
        snprintf(game->modal, sizeof(game->modal), "PurchaseAnim");
    }
    save_career(game);
}

static float club_strength(SSRGame *game) {
    SSRSeasonClub *club = game->season ? ssr_season_club(game->season, game->career.clubId, 0) : NULL;
    return club ? (float)club->strength : 0;
}

// The awards of the armband of the club or of the nation, and of an
// armband at 18 or younger.
static void captain_award(SSRGame *game, int nation) {
    award(game, nation ? 81 : 80);
    if (ssr_career_age(&game->career) <= 18) award(game, 119);
}

static void captain_message(SSRGame *game, const char *tag, const char *team) {
    char text[512];
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    replace_all(text, sizeof(text), "$teamname", team);
    message(game, "messageboss", text, 0, 1, NULL);
}

// c_TPlayer.CheckCaptaincy: the club and the nation make the player
// captain, or take the armband away.
static int news_captaincy(SSRGame *game, const SSRFixture *next) {
    SSRCareer *c = &game->career;
    int level = ssr_career_star_level(c), year = ssr_career_year(c);
    if (next->level == 0) {
        const char *team = my_club_tla(game);
        if (c->captain != 1 && c->captain != 3) {
            // The matches for the club: the history of the club and the
            // matches of the career, as the game adds them.
            if (c->seasonApps <= 4 || ssr_career_history_apps(c, team) + c->apps < 25) return 0;
            float avg = c->seasonTotalRating / (float)c->seasonApps;
            int tell = year > 1 && c->seasonApps % 20 == 0;
            const char *no = level < (int)club_strength(game) / 6 ? "CMESSAGE_NOTCAPTAIN_STARRATING"
                             : avg < 7.0f                          ? "CMESSAGE_NOTCAPTAIN_RATING"
                             : (int)c->relationBoss < 80 || (int)c->relationTeam < 80 || (int)c->relationFans < 80
                                 ? "CMESSAGE_NOTCAPTAIN_RELATIONSHIPS"
                                 : NULL;
            if (no) {
                if (!tell) return 0;
                captain_message(game, no, team);
                return 1;
            }
            c->captain = c->captain == 2 ? 3 : 1;
            captain_message(game, "CMESSAGE_CAPTAINCY", team);
            captain_award(game, 0);
            return 1;
        }
        if (c->seasonApps <= 4) return 0;
        const char *tag = NULL;
        if (c->seasonTotalRating / (float)c->seasonApps < 6.0f) tag = "CMESSAGE_CAPTAINCYSTRIP_RATING";
        else if ((int)c->relationBoss < 60 || (int)c->relationTeam < 60 || (int)c->relationFans < 60) {
            tag = "CMESSAGE_CAPTAINCYSTRIP_RELATIONSHIPS";
        }
        if (!tag) return 0;
        c->captain = c->captain == 3 ? 2 : 0;
        captain_message(game, tag, team);
        save_career(game);
        return 1;
    }
    const SSRNation *nation = ssr_world_nation(game->world, c->nationId);
    const char *team = nation ? nation->name : "";
    if (c->captain != 2 && c->captain != 3) {
        if (c->intApps <= 19) return 0;
        float avg = c->intTotalRating / (float)c->intApps;
        int tell = year > 1 && c->intApps % 12 == 0;
        const char *no = nation && level < nation->strength / 5 ? "CMESSAGE_NOTCAPTAIN_STARRATING"
                         : avg < 7.0f                            ? "CMESSAGE_NOTCAPTAIN_RATING"
                         : (int)c->relationBossInt < 90 || (int)c->relationTeamInt < 90 || (int)c->relationFansInt < 90
                             ? "CMESSAGE_NOTCAPTAIN_RELATIONSHIPS"
                             : NULL;
        if (no) {
            if (!tell) return 0;
            captain_message(game, no, team);
            return 1;
        }
        c->captain = c->captain == 1 ? 3 : 2;
        captain_message(game, "CMESSAGE_CAPTAINCY", team);
        captain_award(game, 1);
        save_career(game);
        return 1;
    }
    const char *tag = NULL;
    if (c->intTotalRating / (float)(c->intApps ? c->intApps : 1) < 6.0f) tag = "CMESSAGE_CAPTAINCYSTRIP_RATING";
    else if ((int)c->relationBossInt < 70 || (int)c->relationTeamInt < 70 || (int)c->relationFansInt < 70) {
        tag = "CMESSAGE_CAPTAINCYSTRIP_RELATIONSHIPS";
    }
    if (!tag) return 0;
    c->captain = c->captain == 3 ? 1 : 0;
    captain_message(game, tag, team);
    save_career(game);
    return 1;
}

// GetStringSponsorName.
static const char *sponsor_name(SSRGame *game, int i) {
    static const char *NAMES[10] = {"sponsor_Boots", "sponsor_SportsDrink", "sponsor_SportsClothing",
                                    "sponsor_CasualClothing", "sponsor_Food", "sponsor_Cosmetics", "sponsor_Watch",
                                    "sponsor_Electronics", "sponsor_Jewelry", "sponsor_Car"};
    return i >= 0 && i < 10 ? loc(game, NAMES[i]) : "";
}

// GSCheckSponsorship: a sponsor leaves, or a new one makes an offer.
static int news_sponsorship(SSRGame *game) {
    SSRCareer *c = &game->career;
    int n = ssr_career_count_sponsors(c);
    char text[512], name[64];
    if (n > 0 && c->relationSponsors < 20) {
        for (int i = 9; i >= 0; i--) {
            if (c->sponsors[i] <= 0) continue;
            snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_SPONSORCANCEL"));
            snprintf(name, sizeof(name), "%s", sponsor_name(game, i));
            upper(game, name);
            replace_all(text, sizeof(text), "$sponsor", name);
            c->sponsors[i] = 0;
            if (ssr_career_count_sponsors(c) == 0) c->relationSponsors = 0;
            else update_relation(game, SSR_RELATION_SPONSORS, 30);
            game_menu(game);
            message(game, "messagesponsor", text, 0, 1, NULL);
            return 1;
        }
        return 0;
    }
    if (c->seasonApps % 10 > 0) return 0;
    if (ssr_career_lifestyle(c) < (float)((n - 2) * 10)) {
        message(game, "messagesponsor", loc(game, "CMESSAGE_SPONSORSNOTIMPRESSEDLIFESTYLE"), 0, 1, NULL);
        return 1;
    }
    if (c->relationFans < (float)((n - 2) * 10)) {
        message(game, "messagesponsor", loc(game, "CMESSAGE_SPONSORSNOTIMPRESSEDFANS"), 0, 1, NULL);
        return 1;
    }
    // OfferSponsorship: the first free slot that the star level reaches.
    int level = ssr_career_star_level(c), rating = c->starRating % 100;
    for (int i = 0; i < 10; i++) {
        if (!(c->sponsors[i] == 0 && level > i && rating > 50)) continue;
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_SPONSOROFFERMOBILE"));
        snprintf(name, sizeof(name), "%s", sponsor_name(game, i));
        upper(game, name);
        replace_all(text, sizeof(text), "$sponsor", name);
        char cash[32];
        shop_price(cash, sizeof(cash), i / 2 + 1);
        replace_all(text, sizeof(text), "$cash", cash);
        relation_tweaks(game, 1);
        game->messageData = i;
        message(game, "messagesponsor", text, 1, 1, "SponsorshipOffer");
        return 1;
    }
    return 0;
}

static void call_sponsorship_offer(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState != 1) return;
    if (c->relationSponsors == 0) c->relationSponsors = 50;
    else update_relation(game, SSR_RELATION_SPONSORS, 25);
    int i = game->messageData;
    if (i >= 0 && i < 10) c->sponsors[i] = i / 2 + 1;
    relationships_screen(game, 0, 0);
    // A sponsor, and all ten.
    award(game, 35);
    if (ssr_career_count_sponsors(c) == 10) award(game, 36);
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
    // [I: the Facebook box (b) is not in the remake.]
    if (news_old_pro_boots(game)) {
        c->newsDone = 1;
        return 1;
    }
    if (!game->settings.doneLoseSkills && c->apps > 3) {
        game->settings.doneLoseSkills = 1;
        game->settingsChanged = 1;
        c->newsDone = 1;
        message(game, "message", loc(game, "CMESSAGE_LOSESKILLS"), 0, 1, NULL);
        return 1;
    }
    if (check_transfer_window(game) || news_captaincy(game, next) || news_sponsorship(game) || news_gambling(game) ||
        news_get_dumped(game) || news_partner_unhappy(game) || news_item_expiry(game)) {
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

static void lifestyle_shop(SSRGame *game, int type);

// The call "ExpiredItem": the lifestyle shop on the tab of the product.
static void call_expired_item(SSRGame *game) {
    lifestyle_shop(game, game->messageData);
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
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 1500);
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
    // A captain of the team of the match has interviews more often.
    SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, game->fixtureComp) : NULL;
    int level = comp ? comp->data->level : 0;
    int captain = (level == 0 && (c->captain == 1 || c->captain == 3)) || (level == 1 && (c->captain == 2 || c->captain == 3));
    int n = captain ? 4 : 8, gap = captain ? 13 : 21;
    if (game->interviewDone || ssr_rand(&game->random, n) != 1 || c->sdate <= c->lastInterview + gap) return 0;
    if (game->after.boss <= 0 || game->after.team <= 0 || game->after.fans <= 0) return 0;
    game->interviewDone = 1;
    c->lastInterview = c->sdate;
    interview_screen(game);
    return 1;
}

// c_TScreen_Interview.ButtonOk: back to the match stats.
// The press of the remake: the answers of the interview make the headline
// of the next day. Good words before a derby or a rivalry match also make
// the rivalry grow, because the other club hears them.
static void press(SSRGame *game, int good) {
    SSRCareer *c = &game->career;
    char text[256];
    snprintf(text, sizeof(text), "%s", loc(game, good ? "SSR_PressGood" : "SSR_PressBad"));
    replace_all(text, sizeof(text), "$name", c->shortName);
    alert(game, "bottommessagemoment", text, 4000);
    if (!good || !game->season || !game->world) return;
    SSRFixture *f = ssr_season_find(game->season, game->fixtureComp, game->fixtureDate, game->fixtureHome,
                                    game->fixtureAway);
    SSRSeasonComp *comp = ssr_season_comp(game->season, game->fixtureComp);
    if (!f || !comp || comp->data->level != 0) return;
    int a = ssr_fixture_home_team(game->season, f), b = ssr_fixture_away_team(game->season, f);
    if (ssr_world_is_derby(game->world, a, b) || ssr_honours_rivalry(&game->honours, a, b) >= SSR_RIVALRY_POINTS) {
        ssr_honours_add_rivalry(&game->honours, a, b, 1);
    }
}

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
        update_relation(game, national ? SSR_RELATION_BOSS_INT : SSR_RELATION_BOSS, -a);
        update_relation(game, national ? SSR_RELATION_TEAM_INT : SSR_RELATION_TEAM, -b);
        update_relation(game, national ? SSR_RELATION_FANS_INT : SSR_RELATION_FANS, -f);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-a);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-b);
        c->starRating = ssr_star_rating_add(c->starRating, (int)-f);
        game->after.boss = game->after.team = game->after.fans = 0;
        static const char *MODS[3] = {"Boss", "Team", "Fans"};
        for (int i = 0; i < 3; i++) ssr_tweaks_set_number(game->tweaks, "RelationshipMods", MODS[i], 0);
        ssr_tweaks_set_number(game->tweaks, "Menu", "ZeroRelChange", 1);
        skill_tweaks(game);
    }
    press(game, !game->interviewFail);
    star_tweaks(game);
    ssr_tweaks_set_number(game->tweaks, "MatchStats", "StatStage", game->statStage);
}

static void interview_end(SSRGame *game, int success) {
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, success ? "Success!" : "Fail!"));
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 1000);
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


/* -------------------------------------------------------- the lifestyle */

// The products of the tabs of the lifestyle overlay.
static const char *const SHOP_TABS[3] = {"Items", "Vehicles", "Property"};

static SSRShopType shop_type(const char *tab) {
    if (strcmp(tab, "Vehicles") == 0) return SSR_SHOP_VEHICLE;
    if (strcmp(tab, "Property") == 0) return SSR_SHOP_PROPERTY;
    return SSR_SHOP_ITEM;
}

static const SSRShopItem *shop_product(const char *uid) {
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = ssr_shop_item(i);
        if (uid && strcmp(item->uid, uid) == 0) return item;
    }
    return NULL;
}

// c_Product_Item.Owned: 0 not owned, else the condition from 1 to 100.
static int product_owned(const SSRCareer *c, const SSRShopItem *item) {
    int v = product_array((SSRCareer *)c, item->type)[item->id - 1];
    if (v == 0) return 0;
    int w = v >= 10000 ? v - 10000 : v;
    float owned = (1100.0f - (float)w) * 0.09090909f;
    return (int)(owned < 1.0f ? 1.0f : owned);
}

// GetLifeLeft: 5 new to 0 broken.
static int product_life_left(const SSRCareer *c, const SSRShopItem *item) {
    int v = product_array((SSRCareer *)c, item->type)[item->id - 1];
    if (v >= 10000) return 0;
    return v > 1000 ? 1 : v > 750 ? 2 : v > 500 ? 3 : v > 250 ? 4 : 5;
}

// c_Product_Lifestyle.GetRepairPrice.
static float repair_price(const SSRCareer *c, const SSRShopItem *item) {
    if (item->lifeType == SSR_LIFE_NONE) return 0;
    static const float PART[5] = {80, 70, 45, 25, 10};
    int life = product_life_left(c, item);
    if (life >= 5) return 0;
    float price = PART[life] * ((float)item->price / 100.0f);
    return price < 1 ? 1 : price;
}

static int count_owned(const SSRCareer *c, SSRShopType type) {
    int n = 0;
    for (int i = 0; i < SSR_CAREER_ITEMS; i++) n += product_array((SSRCareer *)c, type)[i] > 0;
    return n;
}

// The price in the shop: the star and the number.
static void shop_price(char *out, size_t capacity, int price) {
    snprintf(out, capacity, "\xE2\x98\x85%d", price);
}

// c_GSShopping.BuildButton for a lifestyle product.
static void shop_button(SSRGame *game, const SSRShopItem *item, int large) {
    const SSRCareer *c = &game->career;
    const char *list = "LifestyleList";
    const char *L = large ? "Large" : "";
    char name[96], text[96];
    snprintf(name, sizeof(name), "ShopButton%s", large ? "Large" : "Small");
    add_op(game, SSR_OP_LIST_ADD_AT, list, name, NULL, 20);
    snprintf(text, sizeof(text), "hashed_uid=%s", item->uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, "ItemButton", text, 0);
    snprintf(text, sizeof(text), "info_hashed_uid=%s", item->uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, "InfoButton", text, 0);
    snprintf(name, sizeof(name), "ShopSelected%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    snprintf(text, sizeof(text), "sel_uid=%s", item->uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, name, text, 0);
    snprintf(name, sizeof(name), "%s%s_Image", item->uid, large ? "_L" : "");
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    snprintf(name, sizeof(name), "ShopTitle%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    add_op(game, SSR_OP_LIST_SUB, list, "Title", loc(game, item->uid), 1);
    add_op(game, SSR_OP_LIST_SUB, list, "TitleShadow", loc(game, item->uid), 1);
    snprintf(name, sizeof(name), "ShopDescription%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    if (item->type == SSR_SHOP_PROPERTY) {
        snprintf(text, sizeof(text), "%s        %s", loc(game, "LIFESTYLE_BONUS"), loc(game, "PROPERTY_RECOVERY"));
    } else {
        snprintf(text, sizeof(text), "%s", loc(game, "LIFESTYLE_BONUS"));
    }
    add_op(game, SSR_OP_LIST_SUB, list, "Description", text, 1);
    int owned = product_owned(c, item);
    if (owned == 0) {
        snprintf(name, sizeof(name), "ShopPrice%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        shop_price(text, sizeof(text), item->price);
        add_op(game, SSR_OP_LIST_SUB, list, "Price", text, 1);
        add_op(game, SSR_OP_LIST_SUB, list, "PriceShadow", text, 1);
    } else if (item->lifeType != SSR_LIFE_NONE) {
        snprintf(name, sizeof(name), "ShopOwnedBar%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        snprintf(text, sizeof(text), "Products,%s_Owned", item->uid);
        add_op(game, SSR_OP_ENTRY_BAR, list, "ProgressBar", text, 0);
        add_op(game, SSR_OP_LIST_SUB, list, "UsesRemaining", loc(game, "ITEM_CONDITION"), 1);
        if (owned <= 25) {
            snprintf(name, sizeof(name), "ShopFixIcon%s", L);
            add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        }
    } else {
        snprintf(name, sizeof(name), "ShopOwnedTick%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    }
    snprintf(name, sizeof(name), "ShopUnselected%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    snprintf(text, sizeof(text), "unsel_uid=%s", item->uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, name, text, 0);
    snprintf(text, sizeof(text), "%s_Owned", item->uid);
    ssr_tweaks_set_number(game->tweaks, "Products", text, owned);
}

static void shop_selected(SSRGame *game);

// c_ShopOverlay.RebuildShop for the lifestyle tabs.
static void rebuild_shop(SSRGame *game, int select) {
    SSRTweaks *t = game->tweaks;
    const SSRCareer *c = &game->career;
    const char *tab = ssr_tweaks_get(t, "Menu", "ShopTab");
    SSRShopType type = shop_type(tab);
    static const char *TITLES[3] = {"Items", "Vehicles", "Properties"};
    ssr_tweaks_set(t, "Menu", "ShopTitle", loc(game, TITLES[type - 1]));
    char text[600], name[48];
    snprintf(text, sizeof(text), "%d/%d", count_owned(c, type), ssr_shop_count_by_type(type));
    ssr_tweaks_set(t, "Menu", "ItemsOwned", text);
    player_tweaks(game);
    // BuildShopContent: the order of the products comes from a tweak.
    add_op(game, SSR_OP_LIST_CLEAR, "LifestyleList", NULL, NULL, 0);
    snprintf(name, sizeof(name), "%sOrder", SHOP_TABS[type - 1]);
    snprintf(text, sizeof(text), "%s", ssr_tweaks_get(t, "Products", name));
    char value[64], popular[64];
    snprintf(name, sizeof(name), "%s_Value", SHOP_TABS[type - 1]);
    snprintf(value, sizeof(value), "%s", ssr_tweaks_get(t, "Products", name));
    snprintf(name, sizeof(name), "%s_Popular", SHOP_TABS[type - 1]);
    snprintf(popular, sizeof(popular), "%s", ssr_tweaks_get(t, "Products", name));
    int first = 1;
    for (char *entry = strtok(text, "|"); entry; entry = strtok(NULL, "|")) {
        size_t n = strlen(entry);
        int large = n > 2 && strcmp(entry + n - 2, "_L") == 0;
        if (large) entry[n - 2] = 0;
        const SSRShopItem *item = shop_product(entry);
        if (!item) continue;
        shop_button(game, item, large);
        if (select && first) ssr_tweaks_set(t, "Menu", "ItemString", item->uid);
        first = 0;
        const char *sticker = strcmp(value, entry) == 0 ? "ShopBestValue" : strcmp(popular, entry) == 0 ? "ShopMostPopular" : NULL;
        if (sticker) {
            snprintf(name, sizeof(name), "%s%sInst", sticker, large ? "_L" : "");
            add_op(game, SSR_OP_ENTRY_CHILD, "LifestyleList", name, "", 0);
        }
    }
    shop_selected(game);
}

// c_ShopOverlay.OnItemSelected: the text of the buy button.
static void shop_selected(SSRGame *game) {
    SSRTweaks *t = game->tweaks;
    const SSRShopItem *item = shop_product(ssr_tweaks_get(t, "Menu", "ItemString"));
    const char *text = loc(game, "BUY");
    if (item && product_owned(&game->career, item) > 0) {
        static const char *CARE[6] = {NULL, "item_Repair_caps", "item_Upgrade_caps", "item_Clean_caps",
                                      "item_Restore_caps", "item_Replace_caps"};
        int life = product_life_left(&game->career, item);
        if (life == 5) text = "";
        else if (item->lifeType != SSR_LIFE_NONE) text = loc(game, CARE[item->lifeType]);
    }
    ssr_tweaks_set(t, "Menu", "BuyButtonString", text);
}

// ShopLifestyle.SetUpScreen: the lifestyle overlay on the tab of the type.
static void lifestyle_shop(SSRGame *game, int type) {
    snprintf(game->overlay, sizeof(game->overlay), "LifestyleShopOverlay");
    ssr_tweaks_set(game->tweaks, "Menu", "ShopTab", SHOP_TABS[(type >= 1 && type <= 3 ? type : 1) - 1]);
    rebuild_shop(game, 1);
}

static void shop_from_relationships(SSRGame *game) {
    lifestyle_shop(game, 1);
}

static void shop_tab(SSRGame *game, int type) {
    ssr_tweaks_set(game->tweaks, "Menu", "ShopTab", SHOP_TABS[type - 1]);
    rebuild_shop(game, 1);
}

static void shop_tab_items(SSRGame *game) {
    shop_tab(game, 1);
}

static void shop_tab_vehicles(SSRGame *game) {
    shop_tab(game, 2);
}

static void shop_tab_property(SSRGame *game) {
    shop_tab(game, 3);
}

// NotEnoughCash: the remake has no bux shop, so the splash of the classic
// look shows.
static void not_enough_cash(SSRGame *game, int shortBy) {
    play_sound(game, "CError");
    ssr_tweaks_set_number(game->tweaks, "Menu", "ItemInfoHelpIncrement", 0);
    if (shortBy <= 0) return;
    ssr_tweaks_set_number(game->tweaks, "Menu", "BuxShort", shortBy);
    quick_message(game, loc(game, "CMESSAGE_NOTENOUGHCASHSHORT"), 1500);
}

// UpdateBank: the bank does not go under 0.
static void update_bank(SSRGame *game, int change) {
    SSRCareer *c = &game->career;
    c->bank = c->bank + change < 0 ? 0 : c->bank + change;
    // UpdateBank: the award of 10,000 bux, for any change.
    if (c->bank >= 10000) award(game, 67);
    player_tweaks(game);
}

// bb_GSProducts.OnItemPurchased: the pop-up of the purchase.
static void item_purchased(SSRGame *game, const char *text) {
    rebuild_shop(game, 0);
    ssr_tweaks_set(game->tweaks, "Products", "PurchaseOverlayMessage", text && text[0] ? text : loc(game, "Item purchased"));
    snprintf(game->modal, sizeof(game->modal), "PurchaseAnim");
}

// c_Product_Item.Buy.
static void buy_product(SSRGame *game, const SSRShopItem *item) {
    SSRCareer *c = &game->career;
    update_bank(game, -item->price);
    product_array(c, item->type)[item->id - 1] += 1;
    // MadePurchaseCheck: the awards of all the items, all the vehicles,
    // all the properties, and all three.
    int all[4] = {0, 1, 1, 1};
    for (int type = SSR_SHOP_ITEM; type <= SSR_SHOP_PROPERTY; type++) {
        for (int i = 0; i < ssr_shop_count_by_type((SSRShopType)type); i++) all[type] &= product_array(c, type)[i] > 0;
    }
    if (all[SSR_SHOP_ITEM]) award(game, 27);
    if (all[SSR_SHOP_VEHICLE]) award(game, 28);
    if (all[SSR_SHOP_PROPERTY]) award(game, 29);
    if (all[SSR_SHOP_ITEM] && all[SSR_SHOP_VEHICLE] && all[SSR_SHOP_PROPERTY]) award(game, 30);
    save_career(game);
    ssr_tweaks_set_number(game->tweaks, "Player", "Lifestyle", ssr_career_lifestyle(c));
    if (c->teamShopping > -1) c->teamShopping += item->type;
    item_purchased(game, "");
}

static void repair_check(SSRGame *game, const SSRShopItem *item);

// shopoverlay.buy: TryBuy of the product.
static void shop_buy(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->retired) {
        quick_message(game, loc(game, "Retired"), 1500);
        return;
    }
    const SSRShopItem *item = shop_product(ssr_tweaks_get(game->tweaks, "Menu", "ItemString"));
    if (!item) return;
    if (product_array(c, item->type)[item->id - 1] > 0) {
        repair_check(game, item);
        return;
    }
    if (c->bank < item->price) {
        not_enough_cash(game, item->price - c->bank);
        return;
    }
    if (item->type == SSR_SHOP_PROPERTY && count_owned(c, SSR_SHOP_PROPERTY) == 0) {
        // The first property asks first.
        game->overlay[0] = 0;
        char text[512], price[32];
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_PROPERTYCHECK"));
        shop_price(price, sizeof(price), item->price);
        replace_all(text, sizeof(text), "$price", price);
        game->messageData = item->id;
        message(game, "message", text, 1, 1, "BuyProperty");
        return;
    }
    buy_product(game, item);
}

static void call_buy_property(SSRGame *game) {
    const SSRShopItem *item = ssr_shop_nth_of_type(SSR_SHOP_PROPERTY, 1);
    for (int i = 1; i <= ssr_shop_count_by_type(SSR_SHOP_PROPERTY); i++) {
        const SSRShopItem *p = ssr_shop_nth_of_type(SSR_SHOP_PROPERTY, i);
        if (p->id == game->messageData) item = p;
    }
    lifestyle_shop(game, 3);
    if (game->returnState == 1 && item) buy_product(game, item);
}

// UpsellMsg.CloseLifestyle: the overlay goes, and a shopping trip ends.
static void shop_close_lifestyle(SSRGame *game) {
    SSRCareer *c = &game->career;
    game->overlay[0] = 0;
    game->modal[0] = 0;
    ssr_tweaks_set_number(game->tweaks, "Menu", "ItemInfoHelpIncrement", 0);
    if (c->teamShopping == 0) {
        update_relation(game, SSR_RELATION_TEAM, -5);
    } else if (c->teamShopping > 0) {
        int gain = c->teamShopping * 2;
        update_relation(game, SSR_RELATION_TEAM, (float)(gain < 1 ? 1 : gain > 5 ? 5 : gain));
    }
    c->teamShopping = -1;
    relationships_screen(game, 0, 0);
}

static void purchase_close(SSRGame *game) {
    game->modal[0] = 0;
}

// c_Product_Lifestyle.DoRepairCheck: the repair box.
static void repair_check(SSRGame *game, const SSRShopItem *item) {
    const SSRCareer *c = &game->career;
    if (product_life_left(c, item) == 5 || item->lifeType == SSR_LIFE_NONE) return;
    static const char *TYPES[6] = {"", "REPAIR", "UPGRADE", "CLEAN", "RESTORE", "REPLACE"};
    static const char *NAMES[6] = {"", "item_Repair", "item_Upgrade", "item_Clean", "item_Restore", "item_Replace"};
    static const char *ALL[4] = {"", "CMESSAGE_REPAIRALLITEMS", "CMESSAGE_REPAIRALLVEHICLES", "CMESSAGE_REPAIRALLPROPERTY"};
    char text[512], price[32], tag[48], name[64];
    int all = 0;
    for (int i = 1; i <= ssr_shop_count_by_type(item->type); i++) {
        all = (int)((float)all + repair_price(c, ssr_shop_nth_of_type(item->type, i)));
    }
    text[0] = 0;
    if (count_owned(c, item->type) > 1) {
        snprintf(text, sizeof(text), "%s", loc(game, ALL[item->type]));
        shop_price(price, sizeof(price), all);
        replace_all(text, sizeof(text), "$price", price);
    }
    ssr_tweaks_set(game->tweaks, "Products", "RepairAllString", text);
    snprintf(tag, sizeof(tag), "CMESSAGE_ITEMCHECK%s", TYPES[item->lifeType]);
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    shop_price(price, sizeof(price), (int)repair_price(c, item));
    replace_all(text, sizeof(text), "$price", price);
    snprintf(name, sizeof(name), "%s", loc(game, item->uid));
    upper(game, name);
    replace_all(text, sizeof(text), "$item", name);
    ssr_tweaks_set(game->tweaks, "Products", "RepairItemString", text);
    ssr_tweaks_set(game->tweaks, "Products", "RepairTypeString", loc(game, NAMES[item->lifeType]));
    snprintf(game->modal, sizeof(game->modal), "repairmenu");
}

static void repair_cancel(SSRGame *game) {
    game->modal[0] = 0;
}

static void repair_confirm(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRShopItem *item = shop_product(ssr_tweaks_get(game->tweaks, "Menu", "ItemString"));
    if (item) {
        float r = repair_price(c, item);
        if ((float)c->bank < r) {
            not_enough_cash(game, (int)(r - (float)c->bank));
        } else {
            update_bank(game, -(int)r);
            product_array(c, item->type)[item->id - 1] = 1;
            if (c->teamShopping > -1) c->teamShopping += item->type;
            save_career(game);
        }
    }
    game->modal[0] = 0;
    rebuild_shop(game, 0);
}

static void repair_all(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRShopItem *item = shop_product(ssr_tweaks_get(game->tweaks, "Menu", "ItemString"));
    if (item) {
        int price = 0;
        for (int i = 1; i <= ssr_shop_count_by_type(item->type); i++) {
            price = (int)((float)price + repair_price(c, ssr_shop_nth_of_type(item->type, i)));
        }
        if (c->bank < price) {
            not_enough_cash(game, price - c->bank);
        } else {
            int *a = product_array(c, item->type);
            for (int i = 0; i < SSR_CAREER_ITEMS; i++) {
                if (a[i] > 0) a[i] = 1;
            }
            if (c->teamShopping > -1) c->teamShopping += 3;
            update_bank(game, -price);
            save_career(game);
        }
    }
    game->modal[0] = 0;
    rebuild_shop(game, 0);
}


/* ------------------------------------------------------- the skills shop */

// The contract of an agent or a trainer of each level: the matches plus 1.
static const int STAFF_PERIOD[3] = {51, 101, 151};

// The kinds of products of the skills overlay, from the uid.
enum { PRODUCT_NONE, PRODUCT_AGENT, PRODUCT_TRAINER, PRODUCT_BOOT, PRODUCT_NRG };

static int product_kind(const char *uid, int *id) {
    static const struct {
        const char *prefix;
        int kind;
    } KINDS[4] = {{"Agent_", PRODUCT_AGENT}, {"Trainer_", PRODUCT_TRAINER}, {"Boot_", PRODUCT_BOOT}, {"NRG_", PRODUCT_NRG}};
    for (int i = 0; uid && i < 4; i++) {
        size_t n = strlen(KINDS[i].prefix);
        if (strncmp(uid, KINDS[i].prefix, n) == 0) {
            *id = atoi(uid + n);
            return KINDS[i].kind;
        }
    }
    return PRODUCT_NONE;
}

// GetNRGPrice for the cans that the remake sells, 1 to 4.
static int nrg_price(const SSRCareer *c, int id) {
    int level = ssr_career_star_level(c);
    level = level < 1 ? 1 : level > 80 ? 80 : level;
    static const int PER[4] = {3, 5, 5, 7};
    return id >= 1 && id <= 4 ? PER[id - 1] * level - 1 : 1337;
}

// The price of a product in bux, or -1 when it has none (a purchase with
// real money in the game).
static int skill_price(const SSRCareer *c, int kind, int id) {
    switch (kind) {
    case PRODUCT_AGENT:
    case PRODUCT_TRAINER: return id == 1 ? ssr_career_star_level(c) * 5 + 25 : -1;
    case PRODUCT_BOOT: {
        const SSRBoot *boot = ssr_shop_boot(id);
        return boot && !boot->hard ? boot->price : -1;
    }
    case PRODUCT_NRG: return id >= 1 && id <= 4 ? nrg_price(c, id) : -1;
    default: return -1;
    }
}

// Owned() of a product of the skills overlay: the bar of the contract or of
// the boots from 1 to 100, the count of the cans, or 0.
static int skill_owned(const SSRCareer *c, int kind, int id) {
    if (kind == PRODUCT_AGENT || kind == PRODUCT_TRAINER) {
        int level = kind == PRODUCT_AGENT ? ssr_career_agent(c) : ssr_career_trainer(c);
        if (level != id) return 0;
        int left = kind == PRODUCT_AGENT ? c->agent[id - 1] : c->trainer[id - 1];
        float v = 100.0f / (float)STAFF_PERIOD[id - 1] * (float)left;
        return (int)(v < 1 ? 1 : v);
    }
    if (kind == PRODUCT_BOOT) {
        const SSRBoot *boot = ssr_shop_boot(id);
        if (!boot || c->bootId != id) return 0;
        float v = 100.0f / (float)boot->uses * (float)c->bootUses;
        return (int)(v < 1 ? 1 : v);
    }
    if (kind == PRODUCT_NRG) return id >= 1 && id <= 4 ? c->nrg[id - 1] : id <= 6 ? c->nrg[4] : 0;
    return 0;
}

// MatchesRemaining of a product.
static int skill_matches_left(const SSRCareer *c, int kind, int id) {
    if (kind == PRODUCT_AGENT) return c->agent[id - 1] - 1;
    if (kind == PRODUCT_TRAINER) return c->trainer[id - 1] - 1;
    if (kind == PRODUCT_BOOT) return c->bootUses;
    return 0;
}

// BuildButton for a product of the skills overlay.
static void skills_button(SSRGame *game, const char *uid, int large) {
    const SSRCareer *c = &game->career;
    const char *list = "SkillsList";
    const char *L = large ? "Large" : "";
    int id = 0, kind = product_kind(uid, &id);
    char name[96], text[160];
    snprintf(name, sizeof(name), "ShopButton%s", large ? "Large" : "Small");
    add_op(game, SSR_OP_LIST_ADD_AT, list, name, NULL, 20);
    snprintf(text, sizeof(text), "hashed_uid=%s", uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, "ItemButton", text, 0);
    snprintf(text, sizeof(text), "info_hashed_uid=%s", uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, "InfoButton", text, 0);
    snprintf(name, sizeof(name), "ShopSelected%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    snprintf(text, sizeof(text), "sel_uid=%s", uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, name, text, 0);
    snprintf(name, sizeof(name), "%s%s_Image", uid, large ? "_L" : "");
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    // The title and the text of the product.
    char title[96] = "", desc[160] = "";
    const SSRBoot *boot = kind == PRODUCT_BOOT ? ssr_shop_boot(id) : NULL;
    if (kind == PRODUCT_AGENT || kind == PRODUCT_TRAINER) {
        snprintf(name, sizeof(name), "iap_%s%d", kind == PRODUCT_AGENT ? "Agent" : "Trainer", id);
        snprintf(title, sizeof(title), "%s", loc(game, name));
        snprintf(name, sizeof(name), "iap_%s%d_desc", kind == PRODUCT_AGENT ? "Agent" : "Trainer", id);
        snprintf(desc, sizeof(desc), "%s", loc(game, name));
    } else if (boot) {
        snprintf(name, sizeof(name), "Title_%s", boot->name);
        snprintf(title, sizeof(title), "%s", loc(game, name));
    } else if (kind == PRODUCT_NRG) {
        snprintf(name, sizeof(name), "NRG_%d_Title", id);
        snprintf(title, sizeof(title), "%s", loc(game, name));
        snprintf(name, sizeof(name), "NRG_%d_Description", id);
        snprintf(desc, sizeof(desc), "%s", loc(game, name));
        snprintf(text, sizeof(text), "%d", skill_owned(c, kind, id));
        replace_all(desc, sizeof(desc), "$num", text);
    }
    snprintf(name, sizeof(name), "ShopTitle%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    add_op(game, SSR_OP_LIST_SUB, list, "Title", title, 1);
    add_op(game, SSR_OP_LIST_SUB, list, "TitleShadow", title, 1);
    if (boot) {
        snprintf(name, sizeof(name), "ShopSkills%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        snprintf(text, sizeof(text), "%d", boot->pace);
        add_op(game, SSR_OP_LIST_SUB, list, "Pace", text, 1);
        snprintf(text, sizeof(text), "%d", boot->power);
        add_op(game, SSR_OP_LIST_SUB, list, "Power", text, 1);
        snprintf(text, sizeof(text), "%d", boot->technique);
        add_op(game, SSR_OP_LIST_SUB, list, "Technique", text, 1);
        snprintf(text, sizeof(text), "%d", boot->uses);
        add_op(game, SSR_OP_LIST_SUB, list, "Uses", text, 1);
    } else {
        snprintf(name, sizeof(name), "ShopDescription%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        add_op(game, SSR_OP_LIST_SUB, list, "Description", desc, 1);
    }
    int owned = skill_owned(c, kind, id), consumable = kind == PRODUCT_NRG;
    int price = skill_price(c, kind, id);
    if (owned == 0 || consumable) {
        snprintf(name, sizeof(name), "ShopPrice%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        if (price == 0) snprintf(text, sizeof(text), "%s", loc(game, "shop_Free"));
        else if (price > 0) shop_price(text, sizeof(text), price);
        else text[0] = 0;
        add_op(game, SSR_OP_LIST_SUB, list, "Price", text, 1);
        add_op(game, SSR_OP_LIST_SUB, list, "PriceShadow", text, 1);
    }
    if (owned != 0 && !consumable) {
        snprintf(name, sizeof(name), "ShopOwnedBar%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
        snprintf(text, sizeof(text), "Products,%s_Owned", uid);
        add_op(game, SSR_OP_ENTRY_BAR, list, "ProgressBar", text, 0);
        snprintf(text, sizeof(text), "%d %s", skill_matches_left(c, kind, id), loc(game, "Item_Uses"));
        add_op(game, SSR_OP_LIST_SUB, list, "UsesRemaining", text, 1);
    }
    if (boot && (boot->control == 1 || boot->control == 3)) {
        snprintf(name, sizeof(name), "ShopControlIcon%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    }
    if (boot && boot->control >= 2) {
        snprintf(name, sizeof(name), "ShopSwerveIcon%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    }
    snprintf(name, sizeof(name), "ShopUnselected%s", L);
    add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    snprintf(text, sizeof(text), "unsel_uid=%s", uid);
    add_op(game, SSR_OP_ENTRY_VAR, list, name, text, 0);
    if (owned > 0 && kind != PRODUCT_NRG && skill_matches_left(c, kind, id) <= 3) {
        snprintf(name, sizeof(name), "ShopExpiringWarning%s", L);
        add_op(game, SSR_OP_ENTRY_CHILD, list, name, "ItemButton", 0);
    }
    if (kind == PRODUCT_NRG) {
        add_op(game, SSR_OP_ENTRY_CHILD, list, "ShopInventory", "ItemButton", 0);
        snprintf(text, sizeof(text), "%d", owned);
        add_op(game, SSR_OP_LIST_SUB, list, "Inventory", text, 1);
        snprintf(text, sizeof(text), "inv_uid=%s", uid);
        add_op(game, SSR_OP_ENTRY_VAR, list, "InventoryParent", text, 0);
        add_op(game, SSR_OP_ENTRY_VAR, list, "InventoryAnim", text, 0);
    }
    snprintf(text, sizeof(text), "%s_Owned", uid);
    ssr_tweaks_set_number(game->tweaks, "Products", text, owned);
}

// Available() of a product of the skills overlay in the remake.
static int skill_available(SSRGame *game, int kind, int id, const int *boots) {
    const SSRCareer *c = &game->career;
    if (kind == PRODUCT_BOOT) return id >= 1 && id <= SSR_BOOT_COUNT && boots[id];
    // The products for real money and adverts show only when owned.
    if (skill_price(c, kind, id) < 0) return skill_owned(c, kind, id) > 0;
    return kind != PRODUCT_NONE;
}

static void skills_selected(SSRGame *game);

// RebuildShop for the tabs of the skills overlay.
static void rebuild_skills(SSRGame *game, int select) {
    SSRTweaks *t = game->tweaks;
    const SSRCareer *c = &game->career;
    const char *tab = ssr_tweaks_get(t, "Menu", "ShopTab");
    const char *order = "StaffOrder", *title = "Hire_Staff", *value = "Staff_Value", *popular = "Staff_Popular";
    if (strcmp(tab, "Boots") == 0) {
        order = "BootOrder";
        title = "Get_Boots";
        value = "Boot_Value";
        popular = "Boot_Popular";
    } else if (strcmp(tab, "NRG") == 0) {
        order = "NRGOrder";
        title = "Get_NRG";
        value = "NRG_Value";
        popular = "NRG_Popular";
    }
    ssr_tweaks_set(t, "Menu", "ShopTitle", loc(game, title));
    player_tweaks(game);
    int boots[SSR_BOOT_COUNT + 1], k = 0;
    const char *pop = ssr_tweaks_get(t, "Products", "Boot_Popular"), *val = ssr_tweaks_get(t, "Products", "Boot_Value");
    ssr_shop_boots_available(ssr_career_year(c), c->bootId, product_kind(pop, &k) ? k : 10,
                             product_kind(val, &k) ? k : 26, boots);
    char text[1024], valueUid[64], popularUid[64], name[64];
    snprintf(valueUid, sizeof(valueUid), "%s", ssr_tweaks_get(t, "Products", value));
    snprintf(popularUid, sizeof(popularUid), "%s", ssr_tweaks_get(t, "Products", popular));
    snprintf(text, sizeof(text), "%s", ssr_tweaks_get(t, "Products", order));
    add_op(game, SSR_OP_LIST_CLEAR, "SkillsList", NULL, NULL, 0);
    // UpdateBootOrder: the worn boots come first, large.
    char worn[16] = "";
    if (strcmp(tab, "Boots") == 0 && c->bootId > 0) {
        snprintf(worn, sizeof(worn), "Boot_%d", c->bootId);
        skills_button(game, worn, 1);
        if (select) ssr_tweaks_set(t, "Menu", "ItemString", worn);
        select = 0;
    }
    for (char *entry = strtok(text, "|"); entry; entry = strtok(NULL, "|")) {
        size_t n = strlen(entry);
        int large = n > 2 && strcmp(entry + n - 2, "_L") == 0;
        if (large) entry[n - 2] = 0;
        int id = 0, kind = product_kind(entry, &id);
        if (strcmp(entry, worn) == 0 || !skill_available(game, kind, id, boots)) continue;
        // A boot that is large in the order is small when other boots are
        // worn.
        if (worn[0] && kind == PRODUCT_BOOT) large = 0;
        skills_button(game, entry, large);
        if (select) ssr_tweaks_set(t, "Menu", "ItemString", entry);
        select = 0;
        const char *sticker = strcmp(valueUid, entry) == 0 ? "ShopBestValue" : strcmp(popularUid, entry) == 0 ? "ShopMostPopular" : NULL;
        if (sticker) {
            snprintf(name, sizeof(name), "%s%sInst", sticker, large ? "_L" : "");
            add_op(game, SSR_OP_ENTRY_CHILD, "SkillsList", name, "", 0);
        }
    }
    skills_selected(game);
}

static void skills_selected(SSRGame *game) {
    int id = 0, kind = product_kind(ssr_tweaks_get(game->tweaks, "Menu", "ItemString"), &id);
    int price = skill_price(&game->career, kind, id);
    ssr_tweaks_set(game->tweaks, "Menu", "BuyButtonString", price >= 0 ? loc(game, "BUY") : "HIDE");
}

// ShopOverlay.Create for the skills overlay on a tab.
static void skills_shop(SSRGame *game, const char *tab) {
    if (game->career.retired) return;
    snprintf(game->overlay, sizeof(game->overlay), "SkillsShopOverlay");
    ssr_tweaks_set(game->tweaks, "Menu", "ShopTab", tab);
    rebuild_skills(game, 1);
}

static void staff_shop(SSRGame *game) {
    skills_shop(game, "Staff");
}

static void boots_shop(SSRGame *game) {
    skills_shop(game, "Boots");
}

static void nrg_shop(SSRGame *game) {
    skills_shop(game, "NRG");
}

static void skills_tab(SSRGame *game, const char *tab) {
    ssr_tweaks_set(game->tweaks, "Menu", "ShopTab", tab);
    rebuild_skills(game, 1);
}

static void shop_tab_staff(SSRGame *game) {
    skills_tab(game, "Staff");
}

static void shop_tab_boots(SSRGame *game) {
    skills_tab(game, "Boots");
}

static void shop_tab_nrg(SSRGame *game) {
    skills_tab(game, "NRG");
}

// The purchase pop-up over the skills overlay.
static void skill_purchased(SSRGame *game) {
    if (game->overlay[0]) rebuild_skills(game, 0);
    ssr_tweaks_set(game->tweaks, "Products", "PurchaseOverlayMessage", loc(game, "Item purchased"));
    snprintf(game->modal, sizeof(game->modal), "PurchaseAnim");
}

// c_Product_Agent.Buy and c_Product_Trainer.Buy, level 1.
static void buy_staff(SSRGame *game, int kind) {
    SSRCareer *c = &game->career;
    update_bank(game, -skill_price(c, kind, 1));
    int *staff = kind == PRODUCT_AGENT ? c->agent : c->trainer;
    staff[0] = staff[1] = staff[2] = 0;
    staff[0] = STAFF_PERIOD[0];
    award(game, kind == PRODUCT_AGENT ? 73 : 74);
    save_career(game);
    game->overlay[0] = 0;
    message(game, kind == PRODUCT_AGENT ? "messageagent" : "messagetrainer",
            loc(game, kind == PRODUCT_AGENT ? "CMESSAGE_AGENTBOOST" : "CMESSAGE_TRAINERBOOST"), 0, 1,
            "ReopenAgentTrainerShop");
    skill_purchased(game);
}

// c_Product_Boot_Soft.Buy.
static void buy_boots(SSRGame *game, int id) {
    SSRCareer *c = &game->career;
    const SSRBoot *boot = ssr_shop_boot(id);
    if (!boot) return;
    c->bootId = id;
    c->bootUses = boot->uses;
    update_bank(game, -boot->price);
    award(game, 26);
    if (boot->control > 0) award(game, 66);
    save_career(game);
    // [I: the information screen of the control and swerve boots comes
    // with the tutorials.]
    skills_shop(game, "Boots");
    skill_purchased(game);
}

// c_Product_NRG_Soft.Buy: the overlay stays.
static void buy_nrg(SSRGame *game, int id) {
    SSRCareer *c = &game->career;
    int price = nrg_price(c, id);
    if (price > 0) {
        update_bank(game, -price);
        award(game, 25);
    }
    c->nrg[id - 1]++;
    if (c->teamShopping > -1) c->teamShopping++;
    save_career(game);
    player_tweaks(game);
    if (price > 0) skill_purchased(game);
}

// shopoverlay.buy on the skills overlay.
static void skills_buy(SSRGame *game) {
    SSRCareer *c = &game->career;
    const char *uid = ssr_tweaks_get(game->tweaks, "Menu", "ItemString");
    int id = 0, kind = product_kind(uid, &id), price = skill_price(c, kind, id);
    if (price < 0) return;
    if (c->bank < price) {
        not_enough_cash(game, price - c->bank);
        return;
    }
    game->messageData = kind * 100 + id;
    if (kind == PRODUCT_AGENT || kind == PRODUCT_TRAINER) {
        int level = kind == PRODUCT_AGENT ? ssr_career_agent(c) : ssr_career_trainer(c);
        if (level == 0) {
            buy_staff(game, kind);
            return;
        }
        game->overlay[0] = 0;
        message(game, kind == PRODUCT_AGENT ? "messageagent" : "messagetrainer",
                loc(game, kind == PRODUCT_AGENT ? "CMESSAGE_ALREADYGOTAGENT" : "CMESSAGE_ALREADYGOTTRAINER"), 1, 1,
                "ChangeSoftAgentOrTrainer");
    } else if (kind == PRODUCT_BOOT) {
        if (c->bootId == 0) {
            buy_boots(game, id);
            return;
        }
        game->overlay[0] = 0;
        char text[512], number[16];
        snprintf(text, sizeof(text), "%s", loc(game, c->bootId == id ? "MESSAGE_ALREADY_OWN_BOOTS" : "CMESSAGE_BOOTSHAVEMATCHES"));
        snprintf(number, sizeof(number), "%d", c->bootUses);
        replace_all(text, sizeof(text), "$num", number);
        message(game, "message", text, 1, 1, "ChangeSoftBat");
    } else if (kind == PRODUCT_NRG) {
        buy_nrg(game, id);
    }
}

// shopoverlay.buy: the product of the overlay that shows.
static void overlay_buy(SSRGame *game) {
    if (strcmp(game->overlay, "SkillsShopOverlay") == 0) skills_buy(game);
    else shop_buy(game);
}

static void overlay_item_selected(SSRGame *game) {
    if (strcmp(game->overlay, "SkillsShopOverlay") == 0) skills_selected(game);
    else shop_selected(game);
}

static void call_change_staff(SSRGame *game) {
    int kind = game->messageData / 100;
    if (game->returnState == 1) buy_staff(game, kind);
    else staff_shop(game);
}

static void call_change_boots(SSRGame *game) {
    if (game->returnState == 1) buy_boots(game, game->messageData % 100);
    else boots_shop(game);
}

static void call_reopen_staff(SSRGame *game) {
    staff_shop(game);
}

// UpsellMsg.Close.
static void shop_close(SSRGame *game) {
    game->overlay[0] = 0;
    game->modal[0] = 0;
    ssr_tweaks_set_number(game->tweaks, "Menu", "ItemInfoHelpIncrement", 0);
    player_tweaks(game);
}

// UpsellMsg.DrinkNRG: ShopOverlay.ButtonDrinkNRG.
static void shop_drink(SSRGame *game) {
    if (strcmp(game->overlay, "upsellnrg") == 0) {
        int next = next_can(&game->career);
        shop_close(game);
        drink(game, next);
        return;
    }
    int id = 0, kind = product_kind(ssr_tweaks_get(game->tweaks, "Menu", "ItemString"), &id);
    int can = kind == PRODUCT_NRG ? (id >= 1 && id <= 4 ? id - 1 : 4) : 0;
    SSRCareer *c = &game->career;
    if (c->nrg[can] < 1) return;
    if (c->energy >= 100) {
        quick_message(game, loc(game, "CMESSAGE_ENERGYFULL"), 1500);
        return;
    }
    shop_close(game);
    if (c->energy <= 75 || game->match) {
        drink(game, can);
        return;
    }
    game->messageData = can;
    message(game, "message", loc(game, "CMESSAGE_DRINKNRGCHECK"), 1, 1, "DrinkNRG");
}


// The calls "AgentGet", "TrainerGet", and "BootsExpired": Yes opens the shop.
static void call_agent_get(SSRGame *game) {
    if (game->returnState != 1) return;
    game_menu(game);
    staff_shop(game);
    ssr_tweaks_set(game->tweaks, "Menu", "ItemString", "Agent_1");
    skills_selected(game);
}

static void call_trainer_get(SSRGame *game) {
    if (game->returnState != 1) return;
    game_menu(game);
    staff_shop(game);
    ssr_tweaks_set(game->tweaks, "Menu", "ItemString", "Trainer_1");
    skills_selected(game);
}

static void call_boots_expired(SSRGame *game) {
    if (game->returnState != 1) return;
    game_menu(game);
    boots_shop(game);
}


/* ------------------------------------------------------------ contracts */

// TClub.newbossrel: the relation that the boss of the club gives a new
// contract.
static int *new_boss_rel(SSRGame *game, int clubId) {
    if (!game->world || clubId < 0) return NULL;
    if (clubId >= game->newBossRelCount) {
        int count = clubId + 64;
        int *grown = realloc(game->newBossRel, sizeof(int) * (size_t)count);
        if (!grown) return NULL;
        for (int i = game->newBossRelCount; i < count; i++) grown[i] = 75;
        game->newBossRel = grown;
        game->newBossRelCount = count;
    }
    return &game->newBossRel[clubId];
}

// GetOffer with setboss: the boss of the club of the player gives the
// relation of now.
static void get_offer(SSRGame *game, int clubId, int endOfSeason) {
    if (clubId == game->career.clubId) {
        int *rel = new_boss_rel(game, clubId);
        if (rel) *rel = (int)game->career.relationBoss;
    }
    ssr_contract_offer(&game->contract, &game->career, clubId, endOfSeason, &game->offer);
}

static const SSRClub *offer_club(SSRGame *game) {
    return ssr_world_club(game->world, game->offer.clubId);
}

// The figures of the offer against the contract of now (UpdateInstructs).
static void offer_tweaks(SSRGame *game, int wage, int goals, int assists) {
    SSRTweaks *t = game->tweaks;
    const SSRCareer *c = &game->career;
    ssr_tweaks_set_number(t, "Negotiation", "Wage", wage);
    ssr_tweaks_set_number(t, "Negotiation", "GoalBonus", goals);
    ssr_tweaks_set_number(t, "Negotiation", "AssistBonus", assists);
    ssr_tweaks_set_number(t, "Negotiation", "Total", wage + goals + assists);
    ssr_tweaks_set_number(t, "Negotiation", "WageDiff", wage - c->wage);
    ssr_tweaks_set_number(t, "Negotiation", "GoalBonusDiff", goals - c->goalBonus);
    ssr_tweaks_set_number(t, "Negotiation", "AssistBonusDiff", assists - c->assistBonus);
    ssr_tweaks_set_number(t, "Negotiation", "TotalDiff", wage + goals + assists - c->wage - c->goalBonus - c->assistBonus);
}

// c_TScreen_Contract.SetUpScreen.
static void contract_screen(SSRGame *game, int animate) {
    SSRTweaks *t = game->tweaks;
    const SSRClub *club = offer_club(game);
    int *rel = new_boss_rel(game, game->offer.clubId);
    set_screen(game, "contract");
    offer_tweaks(game, game->offer.wage, game->offer.goalBonus, game->offer.assistBonus);
    char text[256];
    snprintf(text, sizeof(text), "%s", loc(game, "NEGOTIATION_OFFER_3"));
    replace_all(text, sizeof(text), "$clubname", club ? club->name : "");
    ssr_tweaks_set(t, "Negotiation", "Offer", text);
    ssr_tweaks_set_number(t, "Player", "NewBossRelationship", rel ? *rel : 75);
    if (animate) ssr_tweaks_set_number(t, "Player", "NewBossRelationship_Displayed", rel ? *rel : 75);
    ssr_tweaks_set_number(t, "Negotiation", "IsNewClub", game->offer.clubId != game->career.clubId);
    // The date of the letter is the date of the device.
    time_t now = time(NULL);
    struct tm *day = localtime(&now);
    char d[8], m[8], y[8];
    snprintf(d, sizeof(d), "%02d", day ? day->tm_mday : 1);
    snprintf(m, sizeof(m), "%02d", day ? day->tm_mon + 1 : 1);
    snprintf(y, sizeof(y), "%02d", day ? day->tm_year % 100 : 0);
    snprintf(text, sizeof(text), "%s", loc(game, "Contract_Date"));
    replace_all(text, sizeof(text), "$D", d);
    replace_all(text, sizeof(text), "$M", m);
    replace_all(text, sizeof(text), "$Y", y);
    ssr_tweaks_set(t, "Menu", "DateString", text);
    int done = game->offer.negotiationSuccess;
    add_op(game, done ? SSR_OP_HIDE : SSR_OP_SHOW, "EnabledNegotiateButton", NULL, NULL, 0);
    add_op(game, done ? SSR_OP_SHOW : SSR_OP_HIDE, "DisabledNegotiateButton", NULL, NULL, 0);
}

// A message of the boss that ends the talk before it starts.
static void no_talk(SSRGame *game, const char *tag, float boss) {
    relationships_screen(game, 1, 0);
    update_relation(game, SSR_RELATION_BOSS, boss);
    relation_tweaks(game, 0);
    message_bar(game, "messageboss", loc(game, tag), "NegotiationsCancelled");
}

// c_TScreen_GameMenu.ButtonContract: the talk with the boss of the club.
static void game_contract(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->retired) return;
    if (c->sdate < c->lastNegotiations + 56) {
        no_talk(game, "CMESSAGE_NOBONUSTOOSOON", -5);
        return;
    }
    if ((int)c->relationBoss < 25) {
        message(game, "messageboss", loc(game, "CMESSAGE_NOBONUSRELATIONSHIPBOSS"), 0, 1, NULL);
        return;
    }
    if ((int)c->relationTeam < 25) {
        no_talk(game, "CMESSAGE_NOBONUSRELATIONSHIPTEAM", -5);
        return;
    }
    if ((int)c->relationFans < 25) {
        no_talk(game, "CMESSAGE_NOBONUSRELATIONSHIPFANS", -5);
        return;
    }
    get_offer(game, c->clubId, 0);
    if (c->wage > game->offer.wage) {
        no_talk(game, "CMESSAGE_NOBONUSLUCRATIVE", -5);
        return;
    }
    if (game->offer.wage == c->wage) {
        no_talk(game, "CMESSAGE_NOBONUSSTARRATING", -3);
        return;
    }
    contract_screen(game, 1);
}

static void call_negotiations_cancelled(SSRGame *game) {
    game_menu(game);
}

static void call_negotiation_accepted(SSRGame *game) {
    add_op(game, SSR_OP_HIDE, "EnabledNegotiateButton", NULL, NULL, 0);
    add_op(game, SSR_OP_SHOW, "DisabledNegotiateButton", NULL, NULL, 0);
}


// c_TContractOffer.SignContract.
static void sign_contract(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRContractOffer *o = &game->offer;
    const SSRClub *club = offer_club(game);
    int *rel = new_boss_rel(game, o->clubId);
    c->wage = o->wage;
    c->goalBonus = o->goalBonus;
    c->assistBonus = o->assistBonus;
    c->lastTransferDate = c->lastNegotiations = c->sdate;
    game->contract.doneRumour = 0;
    int fee = ssr_career_agent(c) * o->wage;
    char text[600], cash[32];
    text[0] = 0;
    if (o->endOfSeason) {
        if (c->captain == 1) c->captain = 0;
        else if (c->captain == 3) c->captain = 2;
        if (fee > 0) {
            snprintf(text, sizeof(text), " %s", loc(game, "CMESSAGE_AGENTBOOST2"));
            shop_price(cash, sizeof(cash), fee);
            replace_all(text, sizeof(text), "$num", cash);
            update_bank(game, fee);
        }
        game->messageData = rel ? *rel : 75;
        message(game, "message", text, 0, 1, "ConfirmSeasonEndTransfer");
        return;
    }
    if (o->clubId == c->clubId) {
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_TRANSFERSAMECLUB"));
        if (ssr_career_agent(c) > 0) {
            size_t n = strlen(text);
            snprintf(text + n, sizeof(text) - n, " %s", loc(game, "CMESSAGE_AGENTBOOST"));
        }
        replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
        if (rel) c->relationBoss = (float)*rel;
        game_menu(game);
        message(game, "message", text, 0, 1, "SignNewContract");
    } else {
        // A transfer in the window. The stat history gets the row of the
        // old club first.
        int old = c->clubId;
        ssr_career_add_history(c, ssr_career_year(c), my_club_tla(game));
        c->clubId = o->clubId;
        c->debut = 1;
        if (rel) c->relationBoss = (float)*rel;
        c->relationTeam = (float)ssr_rand2(&game->random, 35, 50);
        c->relationFans = (float)ssr_rand2(&game->random, 50, 60);
        if (c->captain == 1) c->captain = 0;
        else if (c->captain == 3) c->captain = 2;
        game_menu(game);
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_TRANSFERNEWCLUBMOBILE"));
        replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
        if (fee > 0) {
            size_t n = strlen(text);
            snprintf(text + n, sizeof(text) - n, " %s", loc(game, "CMESSAGE_AGENTBOOST2"));
            shop_price(cash, sizeof(cash), fee);
            replace_all(text, sizeof(text), "$num", cash);
            update_bank(game, fee);
        }
        game->messageData = old;
        message(game, "message", text, 0, 1, "SignForNewClub");
        award(game, 34);
        signed_moment(game);
    }
    save_career(game);
    relation_tweaks(game, 1);
    player_tweaks(game);
}

static void call_sign_new_contract(SSRGame *game) {
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    do_news(game, "CNEWS_RENEWCONTRACTMOBILE", club ? club->tla : "", NULL);
    char wage[32];
    shop_price(wage, sizeof(wage), game->career.wage);
    replace_all(game->career.headline, sizeof(game->career.headline), "$wage", wage);
    webpage_to(game, "gamemenu", 0);
}

static void call_sign_for_new_club(SSRGame *game) {
    const SSRClub *old = ssr_world_club(game->world, game->messageData);
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    char tag[40];
    snprintf(tag, sizeof(tag), "CNEWS_TRANSFERMOBILE%d", ssr_rand(&game->random, 10));
    do_news(game, tag, old ? old->tla : "", club ? club->tla : "");
    webpage_to(game, "gamemenu", 0);
}

static void contract_accept(SSRGame *game) {
    sign_contract(game);
}

static void negotiate_screen(SSRGame *game);
static void desired_transfer_screen(SSRGame *game, int nation);

static void contract_reject(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->offer.endOfSeason) {
        desired_transfer_screen(game, game->desiredNation);
        return;
    }
    if (game->offer.clubId == c->clubId) {
        c->lastNegotiations = c->sdate;
        int *rel = new_boss_rel(game, c->clubId);
        if (rel) c->relationBoss = (float)*rel;
    }
    game_menu(game);
    save_career(game);
}

// c_TContractOffer.DoNegotiation.
static void contract_negotiate(SSRGame *game) {
    int *rel = new_boss_rel(game, game->offer.clubId);
    const SSRClub *club = offer_club(game);
    char text[512];
    if (game->offer.negotiationSuccess) {
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NONEGOTIATING"));
        replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
        message(game, "message", text, 0, 1, "NegotiationAccepted");
    } else if (rel && *rel >= 40) {
        negotiate_screen(game);
    } else if (game->offer.clubId != game->career.clubId && !game->offer.endOfSeason) {
        message(game, "message", loc(game, "CMESSAGE_NEGOTIATIONSCANCELLED"), 0, 1, "NegotiationsCancelled");
        if (rel) *rel = 0;
    } else {
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NONEGOTIATING"));
        replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
        message(game, "message", text, 0, 1, NULL);
    }
}

/* ------------------------------------------------------------ negotiate */

static const int CARD_XS[5] = {10, 130, 250, 370, 490};

static void negotiate_buttons(SSRGame *game, int show) {
    int n = game->shirts[game->negTurn];
    add_op(game, show && n > 1 ? SSR_OP_SHOW : SSR_OP_HIDE, "LowerButton", NULL, NULL, 0);
    add_op(game, show && n < 11 ? SSR_OP_SHOW : SSR_OP_HIDE, "HigherButton", NULL, NULL, 0);
    add_op(game, show && game->negTurn > 1 ? SSR_OP_SHOW : SSR_OP_HIDE, "AcceptButton", NULL, NULL, 0);
}

// The figures of the offer with the increase of the turn (UpdateInstrucs).
static void negotiate_increase(const SSRGame *game, int *w, int *g, int *a) {
    int t = game->negTurn;
    *w = t == 99 ? 2 : t >= 3 && t <= 4 ? 1 : 0;
    *g = t == 99 || t == 4 ? 1 : 0;
    *a = t == 99 ? 1 : 0;
}

static void negotiate_instructions(SSRGame *game) {
    int w, g, a;
    negotiate_increase(game, &w, &g, &a);
    if (game->negTurn == 100) w = g = a = 0;
    offer_tweaks(game, game->offer.wage + w, game->offer.goalBonus + g, game->offer.assistBonus + a);
}

// c_TScreen_Negotiate.SetUpScreen.
static void negotiate_screen(SSRGame *game) {
    set_screen(game, "negotiate");
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Negotiate!"));
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 1500);
    game->negotiateAccepted = 0;
    game->negTurn = 1;
    game->lastClick = 0;
    game->proceedAt = 0;
    for (int i = 0; i < 6; i++) game->shirts[i] = 0;
    for (int card = 1; card <= 5; card++) {
        int same;
        do {
            if (card == 1 && ssr_career_star_level(&game->career) == 1) {
                game->shirts[1] = ssr_rand(&game->random, 2) == 1 ? 1 : 11;
            } else {
                game->shirts[card] = ssr_rand(&game->random, 11);
            }
            same = 0;
            for (int k = 1; k < card; k++) same |= game->shirts[k] == game->shirts[card];
        } while (same);
    }
    for (int card = 1; card <= 5; card++) {
        snprintf(text, sizeof(text), "Card%d", card);
        ssr_tweaks_set_number(game->tweaks, "Negotiation", text, card == 1 ? game->shirts[1] : 0);
    }
    game->clickDelay = 3000;
    const SSRClub *club = offer_club(game);
    char offer[256];
    snprintf(offer, sizeof(offer), "%s", loc(game, "NEGOTIATION_OFFER_3"));
    replace_all(offer, sizeof(offer), "$clubname", club ? club->name : "");
    add_op(game, SSR_OP_TEXT, "TeamOfferText", NULL, offer, 0);
    negotiate_buttons(game, 1);
    negotiate_instructions(game);
}

// ProceedToContract.
static void negotiate_proceed(SSRGame *game) {
    if (game->negTurn <= 99) {
        int w, g, a;
        negotiate_increase(game, &w, &g, &a);
        ssr_contract_increase(&game->offer, w, g, a);
    }
    contract_screen(game, 0);
}

static void negotiate_guess(SSRGame *game, int up) {
    if (game->negotiateAccepted || game->negTurn > 5 || game->now < game->splashEnd) return;
    if (game->lastClick != 0) return;
    game->negTurn++;
    game->lastClick = up ? 1 : -1;
    game->timeClickedNeg = game->now;
    negotiate_buttons(game, 0);
    // RevealCard: the shirt of the turn shows.
    int n = game->shirts[game->negTurn];
    char name[16];
    snprintf(name, sizeof(name), "Card%d", game->negTurn);
    ssr_tweaks_set_number(game->tweaks, "Negotiation", name, n);
    game->clickDelay = n * (500 - n * 20);
    if (game->negTurn < 5) add_op(game, SSR_OP_SET_X, "GuessButtons", NULL, NULL, CARD_XS[game->negTurn]);
}

static void negotiate_higher(SSRGame *game) {
    negotiate_guess(game, 1);
}

static void negotiate_lower(SSRGame *game) {
    negotiate_guess(game, 0);
}

static void negotiate_accept(SSRGame *game) {
    if (game->negotiateAccepted) return;
    game->negotiateAccepted = 1;
    negotiate_buttons(game, 0);
    negotiate_proceed(game);
}

// c_TScreen_Negotiate.Update and CheckFailOrSuccess.
static void negotiate_tick(SSRGame *game) {
    if (game->proceedAt) {
        if (game->now >= game->proceedAt) {
            game->proceedAt = 0;
            negotiate_proceed(game);
        }
        return;
    }
    if (game->negTurn > 1 && game->negTurn <= 5 && game->lastClick != 0 && game->now > game->timeClickedNeg + game->clickDelay) {
        int now = game->shirts[game->negTurn], before = game->shirts[game->negTurn - 1];
        int right = (game->lastClick < 0 && now < before) || (game->lastClick > 0 && now > before);
        game->lastClick = 0;
        char text[32];
        if (!right) {
            // Fail: the boss likes the player less.
            game->negTurn = 100;
            int *rel = new_boss_rel(game, game->offer.clubId);
            if (rel) *rel -= ssr_rand2(&game->random, 10, 20);
            negotiate_instructions(game);
            snprintf(text, sizeof(text), "%s", loc(game, "Fail!"));
            upper(game, text);
            ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
            show_splash(game, "splashmessage", 1000);
            game->proceedAt = game->now + 1000;
            return;
        }
        negotiate_instructions(game);
        if (game->negTurn == 5) {
            game->negTurn = 99;
            negotiate_instructions(game);
            add_op(game, SSR_OP_HIDE, "Shirts", NULL, NULL, 0);
            add_op(game, SSR_OP_SHOW, "StarShowerAnim", NULL, NULL, 0);
            add_op(game, SSR_OP_SHOW, "SigAnimContainer", NULL, NULL, 0);
            add_op(game, SSR_OP_SHOW, "StampAnimContainer", NULL, NULL, 0);
            snprintf(text, sizeof(text), "%s", loc(game, "Success!"));
            upper(game, text);
            ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
            show_splash(game, "splashmessage", 1000);
            game->proceedAt = game->now + 3000;
            return;
        }
        negotiate_buttons(game, 1);
    }
}



/* ------------------------------------------------------ transfer window */

// GetProspectiveClubs during a season: up to max stronger clubs of the
// nation that want the player. The game seeds its random numbers from the
// clock here.
static int window_clubs(SSRGame *game, int nation, int max, const SSRClub **out) {
    const SSRWorld *world = game->world;
    const SSRCareer *c = &game->career;
    int thr = (c->starRating + 200) / 10;
    thr = thr < 10 ? 10 : thr > 90 ? 90 : thr;
    int count = 0, n = 0;
    Prospect *list = prospect_list(game, nation, &count);
    if (!list) return 0;
    game->random.seed = game->now;
    int mine = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].club->id == c->clubId) mine = list[i].strength;
    }
    for (int i = 0; i < count; i++) {
        const SSRClub *club = list[i].club;
        if (ssr_rand(&game->random, 2) != 1 && club->nation == nation && league_duration(world, club->league) > 0) break;
    }
    for (int i = 0; i < count && n < max; i++) {
        const SSRClub *club = list[i].club;
        if (list[i].strength <= thr || club->nation != nation || club->id == c->clubId) continue;
        int r = ssr_rand(&game->random, 2);
        if (r == 1 && (list[i].strength > mine + 10 || list[i].strength > 89) && league_duration(world, club->league) > 0) {
            out[n++] = club;
        }
    }
    free(list);
    return n;
}

// TransferWindowOpenDate: the day of the match after the half of the
// season of the club.
static int window_open_date(SSRGame *game) {
    const SSRFixture *fixtures[128];
    int n = game->season ? team_fixtures(game, game->career.clubId, 0, fixtures, 128) : 0;
    int half = n / 2;
    return half < n ? fixtures[half]->sdate : 0;
}

// GSCheckTransferWindow: the window opens, closes, and brings offers.
static int check_transfer_window(SSRGame *game) {
    SSRCareer *c = &game->career;
    SSRContractState *w = &game->contract;
    int open = window_open_date(game);
    if (open == 0 || c->sdate < open) {
        w->windowOpen = 0;
        return 0;
    }
    if (!w->windowOpen) {
        if (c->sdate > open + 28) return 0;
        w->windowOpen = 1;
        if (c->sdate > open + 7) return 0;
        message(game, "message", loc(game, "CMESSAGE_TRANSFERWINDOWOPEN"), 0, 1, NULL);
        w->offerClubId = 0;
        return 1;
    }
    if (c->sdate > open + 28) {
        w->windowOpen = 0;
        char text[512];
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_TRANSFERWINDOWCLOSED"));
        if (ssr_rand(&game->random, 3) > 1) {
            relationships_screen(game, 1, 0);
            size_t n = strlen(text);
            if (c->relationTeam < 60) {
                snprintf(text + n, sizeof(text) - n, " %s", loc(game, "CMESSAGE_TRANSFERSBOOST"));
                update_relation(game, SSR_RELATION_TEAM, (100 - c->relationTeam) * 0.2f);
            } else {
                snprintf(text + n, sizeof(text) - n, " %s", loc(game, "CMESSAGE_TRANSFERSDRAIN"));
                update_relation(game, SSR_RELATION_TEAM, -c->relationTeam * 0.1f);
            }
            relation_tweaks(game, 0);
            message_bar(game, "messageteam", text, "DoRelationScreen");
        } else {
            message(game, "message", text, 0, 1, NULL);
        }
        return 1;
    }
    if (c->sdate < c->lastTransferDate + 84) return 0;
    const SSRClub *clubs[3];
    int n = window_clubs(game, c->clubNationId, 3, clubs);
    if (n == 0) return 0;
    if (c->sdate <= open + 14 && !w->doneRumour) {
        w->doneRumour = 1;
        char list[256];
        snprintf(list, sizeof(list), "%s", clubs[0]->tla);
        for (int k = 1; k < n; k++) {
            size_t used = strlen(list);
            snprintf(list + used, sizeof(list) - used, "%s%s", k < n - 1 ? ", " : " & ", clubs[k]->tla);
        }
        do_news(game, "CMESSAGE_TRANSFERRUMOUR", NULL, NULL);
        replace_all(c->headline, sizeof(c->headline), "$clubs", list);
        webpage_to(game, "gamemenu", 0);
        return 1;
    }
    for (int k = 0; k < n; k++) {
        if (ssr_rand(&game->random, 2) != 1) continue;
        get_offer(game, clubs[k]->id, 0);
        contract_screen(game, 1);
        char text[512];
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_TRANSFEROFFERMOBILE"));
        replace_all(text, sizeof(text), "$clubname", my_club_tla(game));
        replace_all(text, sizeof(text), "$offerclubname", clubs[k]->tla);
        message(game, "message", text, 0, 1, NULL);
        return 1;
    }
    return 0;
}

// The hint of the agent at the game menu: the club would pay more.
static int agent_hint(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (ssr_career_agent(c) == 0 || c->sdate <= c->lastAgentHint + 27 || c->sdate < c->lastNegotiations + 56) return 0;
    get_offer(game, c->clubId, 0);
    if (game->offer.wage <= c->wage) return 0;
    const char *tag = (int)c->relationBoss < 25   ? "CMESSAGE_AGENTHINTLOWBOSS"
                      : (int)c->relationTeam < 25 ? "CMESSAGE_AGENTHINTLOWTEAM"
                      : (int)c->relationFans < 25 ? "CMESSAGE_AGENTHINTLOWFANS"
                                                  : "CMESSAGE_AGENTHINT";
    message(game, "messageagent", loc(game, tag), 0, 1, NULL);
    c->lastAgentHint = c->sdate;
    return 1;
}

/* ----------------------------------------------------- desired transfer */

// GetRatingLimit: the strength of the clubs that want the player.
static int rating_limit(const SSRCareer *c) {
    int v = (c->starRating + 200) / 10;
    return (v < 10 ? 10 : v > 100 ? 100 : v) + 5;
}

static const SSRCompetition *world_comp(const SSRWorld *w, int id) {
    for (int i = 0; i < w->competitionCount; i++) {
        if (w->competitions[i].id == id) return &w->competitions[i];
    }
    return NULL;
}

static int compare_club_strength(void *context, int a, int b) {
    const SSRWorld *w = context;
    return w->clubs[a].strength - w->clubs[b].strength;
}

// The clubs of the nation, the strongest first (SortListBy 11).
static int nation_clubs(const SSRWorld *w, int nation, int *out) {
    int n = 0;
    for (int i = 0; i < w->clubCount; i++) {
        if (w->clubs[i].nation == nation) out[n++] = i;
    }
    ssr_sort(out, n, compare_club_strength, (void *)w, 1);
    return n;
}

static void desired_hint(SSRGame *game) {
    char text[256];
    if (game->desiredClub > 0) {
        const SSRClub *club = ssr_world_club(game->world, game->desiredClub);
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NEGOTIATEWITH"));
        replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
    } else {
        snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_CLUBSINTERESTED"));
    }
    add_op(game, SSR_OP_TEXT, "HintText", NULL, text, 0);
}

// ComboLeague: the divisions of the nation with clubs that want the player.
static void desired_league(SSRGame *game) {
    const SSRWorld *w = game->world;
    int nation = (int)ssr_tweaks_number(game->tweaks, "Combo", "ChosenDesiredLeague");
    const SSRNation *n = ssr_world_nation(w, nation);
    if (!n) return;
    game->desiredNation = nation;
    add_op(game, SSR_OP_TEXT, "LeagueText", NULL, n->name, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "DivisionCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "ClubCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "DivisionText", NULL, loc(game, "Choose Division"), 0);
    add_op(game, SSR_OP_TEXT, "ClubText", NULL, loc(game, "Choose Club"), 0);
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    game->desiredDivision = game->desiredClub = 0;
    int limit = rating_limit(&game->career);
    int *clubs = malloc(sizeof(int) * (size_t)(w->clubCount + 1));
    if (!clubs) return;
    int count = nation_clubs(w, nation, clubs);
    for (int i = 0; i < w->competitionCount; i++) {
        const SSRCompetition *comp = &w->competitions[i];
        int found = 0;
        for (int k = 0; k < count && !found; k++) {
            const SSRClub *club = &w->clubs[clubs[k]];
            found = club->league == comp->id && club->strength < limit;
        }
        if (found && comp->level == 0 && comp->locale == 0 && comp->type == 0 && comp->based == nation &&
            comp->duration > 0) {
            add_op(game, SSR_OP_LIST_ADD, "DivisionCombo", "DivisionTemplate", comp->tla, comp->id);
        }
    }
    free(clubs);
    desired_hint(game);
}

// ComboDivision: the clubs of the division that want the player.
static void desired_division(SSRGame *game) {
    const SSRWorld *w = game->world;
    int division = (int)ssr_tweaks_number(game->tweaks, "Combo", "ChosenDesiredDivision");
    const SSRCompetition *comp = world_comp(w, division);
    if (!comp) return;
    game->desiredDivision = division;
    game->desiredClub = 0;
    add_op(game, SSR_OP_TEXT, "DivisionText", NULL, comp->tla, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "ClubCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "ClubText", NULL, loc(game, "Choose Club"), 0);
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    int limit = rating_limit(&game->career);
    int *clubs = malloc(sizeof(int) * (size_t)(w->clubCount + 1));
    if (!clubs) return;
    int count = nation_clubs(w, game->desiredNation, clubs);
    for (int k = 0; k < count; k++) {
        const SSRClub *club = &w->clubs[clubs[k]];
        if (club->league != division || club->strength >= limit || club->id == game->career.clubId) continue;
        char text[64];
        const SSRCompetition *cont = club->continentalCompetition > 0 ? world_comp(w, club->continentalCompetition) : NULL;
        if (cont) snprintf(text, sizeof(text), "%s (%s)", club->tla, cont->tla);
        else snprintf(text, sizeof(text), "%s", club->tla);
        add_op(game, SSR_OP_LIST_ADD, "ClubCombo", "ClubTemplate", text, club->id);
    }
    free(clubs);
    desired_hint(game);
}

// ComboClub.
static void desired_club(SSRGame *game) {
    int id = (int)ssr_tweaks_number(game->tweaks, "Combo", "ChosenDesiredClub");
    const SSRClub *club = ssr_world_club(game->world, id);
    game->desiredClub = club ? id : 0;
    if (club) {
        add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
        add_op(game, SSR_OP_TEXT, "ClubText", NULL, club->tla, 0);
    }
    desired_hint(game);
}

// c_TScreen_DesiredTransfer.SetUpScreen.
static void desired_transfer_screen(SSRGame *game, int nation) {
    const SSRWorld *w = game->world;
    set_screen(game, "desiredtransfer");
    game->desiredNation = nation;
    game->desiredDivision = game->desiredClub = 0;
    add_op(game, SSR_OP_LIST_CLEAR, "LeagueCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "DivisionCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_LIST_CLEAR, "ClubCombo", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "LeagueText", NULL, loc(game, "Choose League"), 0);
    add_op(game, SSR_OP_TEXT, "DivisionText", NULL, loc(game, "Choose Division"), 0);
    add_op(game, SSR_OP_TEXT, "ClubText", NULL, loc(game, "Choose Club"), 0);
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    add_op(game, SSR_OP_TEXT, "HintText", NULL, loc(game, "Choose League"), 0);
    int limit = rating_limit(&game->career), any = 0;
    for (int i = 0; i < w->nationCount; i++) {
        const SSRNation *n = &w->nations[i];
        int ok = 0;
        for (int k = 0; k < w->clubCount && !ok; k++) {
            const SSRClub *club = &w->clubs[k];
            const SSRCompetition *comp = club->nation == n->id ? world_comp(w, club->league) : NULL;
            ok = comp && club->strength < limit + 5 && comp->duration > 0;
        }
        if (!ok) continue;
        add_op(game, SSR_OP_LIST_ADD, "LeagueCombo", "LeagueTemplate", n->name, n->id);
        any = 1;
    }
    if (!any) {
        message(game, "message", loc(game, "CMESSAGE_NOCLUBSINTERESTEDATALL"), 0, 1, "NewSeasonNoClubsInterested");
        return;
    }
    ssr_tweaks_set_number(game->tweaks, "Combo", "ChosenDesiredLeague", nation);
    desired_league(game);
}

static void desired_ok(SSRGame *game) {
    if (!game->desiredClub) return;
    get_offer(game, game->desiredClub, 1);
    contract_screen(game, 0);
}

static void desired_back(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "Combo", "ShowDesiredLeagues", 0);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ShowDesiredDivisions", 0);
    ssr_tweaks_set_number(game->tweaks, "Combo", "ShowDesiredClubs", 0);
    const SSRClub *club = ssr_world_club(game->world, game->career.clubId);
    char text[512];
    snprintf(text, sizeof(text), "%s", loc(game, "CMESSAGE_NEWSEASONWITHSAMECLUB"));
    replace_all(text, sizeof(text), "$clubname", club ? club->tla : "");
    message(game, "message", text, 1, 1, "NewSeasonWithSameClub");
}

static void new_season(SSRGame *game);

// The call "ConfirmSeasonEndTransfer": the new club for the new season.
static void call_confirm_transfer(SSRGame *game) {
    SSRCareer *c = &game->career;
    const SSRClub *club = offer_club(game);
    if (club) {
        c->clubNationId = club->nation;
        c->clubId = club->id;
    }
    c->relationBoss = (float)game->messageData;
    c->relationTeam = (float)ssr_rand2(&game->random, 35, 50);
    c->relationFans = (float)ssr_rand2(&game->random, 50, 60);
    new_season(game);
    // NewSeasonSetUp4 after a transfer: the award of a transfer.
    award(game, 34);
    signed_moment(game);
}

static void call_new_season(SSRGame *game) {
    new_season(game);
}

static void call_new_season_same_club(SSRGame *game) {
    if (game->returnState == 1) new_season(game);
}

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
    quick_message(game, loc(game, "Find a pair!"), 1500);
    // AScreen_Pairs.Init: each card shows its face.
    char ref[16];
    for (int k = 0; k < 16; k++) {
        card_ref(ref, sizeof(ref), k);
        add_op(game, SSR_OP_ANIM_PLAY, ref, CARD_FACES[relation][game->icons[game->iconOrder[k]].imgId - 1], "0", 0);
    }
}

static void flip_cards_down(SSRGame *game) {
    // [I: the game names "Card_Dealt_1"; the remake plays its set DealCard.]
    play_sound(game, "DealCard");
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
    play_sound(game, "KeyboardWhoosh");
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
    play_sound(game, "CSuccess");
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Success!"));
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 4000);
    update_relation(game, BAR_RELATIONS[r], 5.0f);
    relation_tweaks(game, 0);
    char tag[32];
    snprintf(tag, sizeof(tag), "%s%d", TAGS[r], imgId);
    save_career(game);
    // The screen goes, so the next game makes the list again.
    game->pairsMade = 0;
    message_bar(game, SCREENS[r] ? SCREENS[r] : partner_screen(game), loc(game, tag), "DoRelationScreen");
}

static void pairs_fail(SSRGame *game) {
    play_sound(game, "CError");
    char text[64];
    snprintf(text, sizeof(text), "%s", loc(game, "Fail!"));
    upper(game, text);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessage", 1000);
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
    play_sound(game, "DealCard");
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

/* ---------------------------------------------------------- the casino */

// The fields of TPlayer that a new career or a load starts again, and the
// stake of the casino (c_TScreen_MainMenu).
static void casino_new_player(SSRGame *game) {
    game->stakeLevel = game->bet = 1;
    game->betCount = game->caughtGambling = game->teammatesCasino = 0;
    game->teammatesBetCount = -1;
}

static void set_teammates_bet_count(SSRGame *game, int count) {
    game->teammatesBetCount = count;
    ssr_tweaks_set_number(game->tweaks, "Casino", "TeammatesBetCount", count);
}

static void add_energy(SSRCareer *c, float change) {
    float e = c->energy + change;
    c->energy = e < 0 ? 0 : e > 100 ? 100 : e;
}

// c_TScreen_Casino.UpdateTitlePanel: the stake, and the stake buttons on
// at the levels that can change.
static void update_title_panel(SSRGame *game) {
    game->bet = ssr_casino_stake(game->stakeLevel);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Stake", game->bet);
    game->stakeDownOff = game->stakeLevel == 1;
    game->stakeUpOff = game->stakeLevel == SSR_CASINO_STAKE_LEVELS;
    add_op(game, SSR_OP_ELEMENT_COLOUR, "DecreaseBet", NULL, game->stakeDownOff ? "FFFFFF55" : "FFFFFFFF", 0);
    add_op(game, SSR_OP_ELEMENT_COLOUR, "IncreaseBet", NULL, game->stakeUpOff ? "FFFFFF55" : "FFFFFFFF", 0);
}

// DisableStake: the stake cannot change while a game runs.
static void disable_stake(SSRGame *game) {
    game->stakeUpOff = game->stakeDownOff = 1;
    add_op(game, SSR_OP_ELEMENT_COLOUR, "DecreaseBet", NULL, "FFFFFF55", 0);
    add_op(game, SSR_OP_ELEMENT_COLOUR, "IncreaseBet", NULL, "FFFFFF55", 0);
}

// CheckStakeAmount (EnableStake): the stake goes down to the bank.
static void check_stake(SSRGame *game) {
    game->stakeLevel = ssr_casino_check_stake(game->stakeLevel, game->career.bank);
    update_title_panel(game);
}

static void runners_table(SSRGame *game);

// The stake buttons; the cards of the racing screen show the new returns.
static void stake_up(SSRGame *game) {
    if (game->stakeUpOff) return;
    if (game->stakeLevel < SSR_CASINO_STAKE_LEVELS) game->stakeLevel++;
    update_title_panel(game);
    if (strcmp(game->screen, "racing") == 0) runners_table(game);
}

static void stake_down(SSRGame *game) {
    if (game->stakeDownOff) return;
    if (game->stakeLevel > 1) game->stakeLevel--;
    update_title_panel(game);
    if (strcmp(game->screen, "racing") == 0) runners_table(game);
}

// c_TScreen_Casino.SetUpScreen.
static void casino_screen(SSRGame *game) {
    set_screen(game, "casino");
    ssr_tweaks_set_number(game->tweaks, "Casino", "TeammatesBetCount", game->teammatesBetCount);
    update_title_panel(game);
}

// c_TScreen_Casino.ButtonBack: the end of a trip with the team mates, and
// the energy that the bets of the visit cost. [I: the alerts of the energy
// come with the alerts of the game.]
static void casino_back(SSRGame *game) {
    SSRCareer *c = &game->career;
    int count = game->teammatesBetCount;
    if (count == 0) {
        game->teammatesCasino = 0;
        update_relation(game, SSR_RELATION_TEAM, -10);
    } else if (count > 0) {
        int gain = (int)((float)count * 0.5f);
        update_relation(game, SSR_RELATION_TEAM, (float)(gain < 1 ? 1 : gain > 5 ? 5 : gain));
    }
    set_teammates_bet_count(game, -1);
    // The bets cost energy, and an alert tells how much.
    if (game->betCount > 0) {
        int energy = game->betCount / 3 + 1;
        add_energy(c, (float)-energy);
        char text[64];
        snprintf(text, sizeof(text), "%s -%d", loc(game, "Energy"), energy);
        alert(game, "bottommessageenergy", text, 2800);
    }
    game->betCount = 0;
    player_tweaks(game);
    relationships_screen(game, count == -1, 0);
}

// casino.btn_Home: ButtonBack, then the game menu.
static void casino_home(SSRGame *game) {
    casino_back(game);
    game_menu(game);
}

// c_TPlayer.CatchGambling: a player under 18 can be caught.
static void catch_gambling(SSRGame *game) {
    message(game, "message", loc(game, "CMESSAGE_GAMBLINGTOOYOUNG"), 0, 1, "CaughtGambling");
}

// The call "CaughtGambling": the relations go down and the news tells it.
static void call_caught_gambling(SSRGame *game) {
    SSRCareer *c = &game->career;
    game->caughtGambling = 10;
    c->gambling = (int)((float)c->gambling * 0.5f);
    c->relationBoss = ssr_career_relation(c, SSR_RELATION_BOSS) * 0.75f;
    c->relationFans = ssr_career_relation(c, SSR_RELATION_FANS) * 0.9f;
    c->relationSponsors = ssr_career_relation(c, SSR_RELATION_SPONSORS) * 0.75f;
    do_news(game, "CNEWS_GAMBLINGTOOYOUNG2", my_club_tla(game), NULL);
    replace_number(c->headline, sizeof(c->headline), "$age", ssr_career_age(c));
    webpage_to(game, "relationships", 0);
}

// c_TPlayer.Bet (GSPlayerBet): returns 1 when the bet is taken. weight is
// what the bet adds to the gambling.
static int casino_bet(SSRGame *game, int amount, int weight) {
    SSRCareer *c = &game->career;
    if (amount > c->bank) {
        not_enough_cash(game, amount - c->bank);
        check_stake(game);
        return 0;
    }
    if (c->energy < 1.0f) {
        not_enough_energy(game, "Casino", 1.0f);
        return 0;
    }
    if (ssr_career_age(c) < 18 && c->gambling > 20 && game->caughtGambling == 0 &&
        ssr_rand(&game->casinoRandom, 20) == 1) {
        catch_gambling(game);
        return 0;
    }
    // Each tenth bet costs energy. [I: the alert comes with the alerts.]
    if (game->betCount > 9) {
        game->betCount = 0;
        add_energy(c, -5);
    }
    update_bank(game, -amount);
    if (game->teammatesBetCount > -1) set_teammates_bet_count(game, game->teammatesBetCount + 1);
    c->casinoCount++;
    game->betCount += weight;
    c->gambling = c->gambling + weight > 100 ? 100 : c->gambling + weight;
    save_career(game);
    return 1;
}

// The message of a result: "YOU WON" or "YOU LOST" and the bux
// (TQuickMessage).
static void casino_message(SSRGame *game, const char *tag, int amount) {
    char text[128], cash[24];
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    upper(game, text);
    number_with_commas(amount, cash, sizeof(cash));
    size_t n = strlen(text);
    snprintf(text + n, sizeof(text) - n, " \xE2\x98\x85%s", cash);
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    // A win shows in the layout with the bux.
    show_splash(game, strcmp(tag, "bet_YouWon") == 0 ? "splashmessagebux" : "splashmessage", 1500);
}

/* ----------------------------------------------------------- the horses */

// The bux as a text: "★1,250" (GetStringCash with the star).
static void cash_star(int value, char *out, size_t capacity) {
    char number[24];
    number_with_commas(value, number, sizeof(number));
    snprintf(out, capacity, "\xE2\x98\x85%s", number);
}

// THorse.LoadData and LoadHorsesOwned for a new or a loaded career, then
// the market of the year. [I: the remake keeps its career numbers; the
// game seeds them from the clock here.]
static void horses_for_career(SSRGame *game) {
    const SSRCareer *c = &game->career;
    game->horsesLoaded = 0;
    if (!game->horseNames) return;
    ssr_horses_load(&game->horses, game->horseNames);
    ssr_horses_load_owned(&game->horses, c->horses);
    ssr_horses_reset_for_sale(&game->horses, ssr_career_year(c) + (int)strlen(c->fullName) + c->clubNationId);
    game->horsesLoaded = 1;
}

// UpdateHorseTweaks: the cards of the stable and of the market, in id
// order. With select, the first card of each is selected.
static void horse_tweaks(SSRGame *game, int select) {
    SSRTweaks *t = game->tweaks;
    SSRHorses *h = &game->horses;
    ssr_horses_sort_by_id(h);
    int owned = 0, sale = 0;
    char name[48], text[32];
    for (int k = 0; k < h->count; k++) {
        const SSRHorse *horse = &h->horses[h->list[k]];
        const char *group = horse->owned ? "Stable_Owned" : "Stable_ForSale";
        int *n = horse->owned ? &owned : &sale;
        if ((!horse->owned && !horse->forsale) || *n >= SSR_STABLE_SLOTS) continue;
        (horse->owned ? game->ownedHorses : game->saleHorses)[*n] = horse->id;
        snprintf(name, sizeof(name), "%d_Name", *n);
        ssr_tweaks_set(t, group, name, horse->name);
        snprintf(name, sizeof(name), "%d_Colour", *n);
        snprintf(text, sizeof(text), "%.8s", horse->colour);
        ssr_tweaks_set(t, group, name, text);
        snprintf(name, sizeof(name), "%d_Strength", *n);
        ssr_tweaks_set_number(t, group, name, horse->strength);
        snprintf(name, sizeof(name), "%d_Price", *n);
        int price = ssr_horses_price(horse->strength);
        number_with_commas(horse->owned ? (int)((float)price * 0.5f) : price, text, sizeof(text));
        ssr_tweaks_set(t, group, name, text);
        if (horse->owned) {
            snprintf(name, sizeof(name), "%d_Energy", *n);
            ssr_tweaks_set_number(t, group, name, horse->energy);
        }
        (*n)++;
    }
    for (int k = owned; k < SSR_STABLE_SLOTS; k++) game->ownedHorses[k] = 0;
    for (int k = sale; k < SSR_STABLE_SLOTS; k++) game->saleHorses[k] = 0;
    for (int g = 0; g < 2; g++) {
        const char *group = g ? "Stable_ForSale" : "Stable_Owned";
        for (int k = g ? sale : owned; k < SSR_STABLE_SLOTS; k++) {
            snprintf(name, sizeof(name), "%d_Name", k);
            ssr_tweaks_set(t, group, name, "NONE");
            snprintf(name, sizeof(name), "%d_Colour", k);
            ssr_tweaks_set(t, group, name, "FFFFFFFF");
            snprintf(name, sizeof(name), "%d_Strength", k);
            ssr_tweaks_set_number(t, group, name, g ? 0 : 1);
            snprintf(name, sizeof(name), "%d_Price", k);
            ssr_tweaks_set(t, group, name, "1");
        }
    }
    ssr_tweaks_set_number(t, "Stable_ForSale", "Count", sale);
    ssr_tweaks_set_number(t, "Stable_Owned", "Count", owned);
    if (select) {
        ssr_tweaks_set(t, "Stable_ForSale", "SelectedItem", "0");
        ssr_tweaks_set(t, "Stable_Owned", "SelectedItem", "0");
    }
}

// The horse of the selected card of a list, or NULL.
static SSRHorse *selected_horse(SSRGame *game, int forSale) {
    const char *item = ssr_tweaks_get(game->tweaks, forSale ? "Stable_ForSale" : "Stable_Owned", "SelectedItem");
    int k = item ? atoi(item) : 0;
    if (k < 0 || k >= SSR_STABLE_SLOTS) return NULL;
    return ssr_horses_by_id(&game->horses, (forSale ? game->saleHorses : game->ownedHorses)[k]);
}

// MyStable.SetUpScreen.
static void stable_screen(SSRGame *game) {
    set_screen(game, "mystable");
    horse_tweaks(game, 1);
}

static void racing_screen(SSRGame *game);

// The call "TScreen_Racing.Return": a new field of runners.
static void call_racing_return(SSRGame *game) {
    racing_screen(game);
}

// Racing.ButtonStable: the stable needs the property Stable.
static void racing_stable(SSRGame *game) {
    if (game->career.properties[4] == 0) {
        message(game, "message", loc(game, "CMESSAGE_NOSTABLEMOBILE"), 0, 1, "TScreen_Racing.Return");
        return;
    }
    stable_screen(game);
}

// Racing.SetUpRunnersTable: the cards of the six runners.
static void runners_table(SSRGame *game) {
    SSRHorses *h = &game->horses;
    SSRTweaks *t = game->tweaks;
    int y[SSR_RUNNERS], maxOdds = 0, minOdds = 99;
    ssr_horses_card_order(h, y);
    char name[48], text[32];
    for (int i = 0; i < h->runnerCount; i++) {
        const SSRHorse *r = &h->horses[h->runners[i]];
        int n = i + 1;
        for (int s = 1; s <= 5; s++) {
            snprintf(name, sizeof(name), "Horse%dStar%dRating", n, s);
            add_op(game, s <= r->strength ? SSR_OP_SHOW : SSR_OP_HIDE, name, NULL, NULL, 0);
        }
        snprintf(name, sizeof(name), "Horse%d", n);
        add_op(game, SSR_OP_GEL, name, NULL, "12", SSR_GEL_X);
        snprintf(text, sizeof(text), "%d", y[i]);
        add_op(game, SSR_OP_GEL, name, NULL, text, SSR_GEL_Y);
        snprintf(name, sizeof(name), "Horse%dName", n);
        ssr_tweaks_set(t, "HorseRacing", name, r->name);
        snprintf(name, sizeof(name), "Horse%dReturn", n);
        number_with_commas(r->odds * game->bet, text, sizeof(text));
        ssr_tweaks_set(t, "HorseRacing", name, text);
        snprintf(name, sizeof(name), "Horse%dOdds", n);
        snprintf(text, sizeof(text), "%d/1", r->odds);
        ssr_tweaks_set(t, "HorseRacing", name, text);
        snprintf(name, sizeof(name), "Horse%dColour", n);
        snprintf(text, sizeof(text), "%.8s", r->colour);
        ssr_tweaks_set(t, "HorseRacing", name, text);
        if (r->odds > maxOdds) maxOdds = r->odds;
        if (r->odds < minOdds) minOdds = r->odds;
    }
    for (int i = 0; i < h->runnerCount; i++) {
        const SSRHorse *r = &h->horses[h->runners[i]];
        snprintf(name, sizeof(name), "Horse%dLongShot", i + 1);
        add_op(game, r->odds >= maxOdds ? SSR_OP_SHOW : SSR_OP_HIDE, name, NULL, NULL, 0);
        snprintf(name, sizeof(name), "Horse%dFavourite", i + 1);
        add_op(game, r->odds <= minOdds ? SSR_OP_SHOW : SSR_OP_HIDE, name, NULL, NULL, 0);
    }
}

// Racing.SetUpRace: a new field, and no horse is selected.
static void setup_race(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "HorseRacing", "SelectedHorse", 0);
    ssr_horses_select_runners(&game->horses, -1, &game->casinoRandom);
    runners_table(game);
}

// casino.btn_Racing: Racing.SetUpScreen. The numbers take a new seed from
// the clock. Each visit makes a new field.
static void racing_screen(SSRGame *game) {
    ssr_tweaks_set_number(game->tweaks, "HorseRacing", "SelectedHorse", 1);
    game->casinoRandom.seed = game->now;
    set_screen(game, "racing");
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", 0);
    if (game->horses.runnerCount == 0) setup_race(game);
    update_title_panel(game);
    setup_race(game);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
}

// The places of the horses on the screen at the start (RacingMiniGame).
static const float HORSE_START[SSR_RUNNERS][2] = {{350, 60}, {282, 120}, {214, 180},
                                                  {146, 240}, {82, 300},  {15, 360}};

// TRacing.SetUp: the race screen ("horserace" in the game, the layout
// RacingMiniGame.uis). The track animation runs at 3.15 frames in each
// update.
static void race_setup(SSRGame *game, int own) {
    SSRHorses *h = &game->horses;
    set_screen(game, "RacingMiniGame");
    game->raceFrame = 0;
    game->standingsReady = 0;
    game->ownRace = own;
    game->gallopNext = 0;
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
    game->myHorse = -1;
    for (int i = 0; i < h->runnerCount; i++) {
        const SSRHorse *r = &h->horses[h->runners[i]];
        if (r->owned || r->betamount) game->myHorse = i;
        game->horseX[i] = HORSE_START[i][0];
        game->horseY[i] = HORSE_START[i][1];
        game->badgePlace[i] = -1;
        char name[32];
        snprintf(name, sizeof(name), "Horse%dPlace", i + 1);
        ssr_tweaks_set_number(game->tweaks, "HorseRacing", name, 0);
    }
    // HorseRaceCommentaryEngine.New.
    add_op(game, SSR_OP_TEXT, "CommentaryText", NULL, loc(game, "HORSERACING_01"), 0);
    game->commentaryNext = game->now + 1650;
    game->commentaryPrev = -1;
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", 1);
    add_op(game, SSR_OP_ANIM_PLAY, "RacetrackAnimInst", NULL, "3.15", 0);
}

// racing.btn_Race: the bet on the selected horse, then the race.
static void racing_race(SSRGame *game) {
    int n = (int)ssr_tweaks_number(game->tweaks, "HorseRacing", "SelectedHorse");
    if (n < 1 || n > game->horses.runnerCount) {
        message(game, "message", loc(game, "CMESSAGE_SELECTHORSE"), 0, 1, "TScreen_Racing.Return");
        return;
    }
    if (!casino_bet(game, game->bet, 3)) return;
    game->horses.horses[game->horses.runners[n - 1]].betamount = game->bet;
    race_setup(game, 0);
}

// HorseRaceCommentaryEngine.Update: a line each 1650 ms for the horse of
// the player.
static void race_commentary(SSRGame *game) {
    static const char *const DOWN3[7] = {NULL, NULL, NULL, "07", "07", "17", "17"};
    static const char *const DOWN2[7] = {NULL, NULL, NULL, "16", "06", "15", "06"};
    static const char *const DOWN1[7] = {NULL, NULL, "16", "09", "09", "09", "03"};
    static const char *const SAME[7] = {NULL, "19", "13", "14", "14", "20", "08"};
    static const char *const UP1[7] = {NULL, "02", "10", "04", "04", "04", "04"};
    static const char *const UP2[7] = {NULL, "02", "10", "11", "11", NULL, NULL};
    static const char *const UP3[7] = {NULL, "02", "10", "20", NULL, NULL, NULL};
    if (game->myHorse < 0 || game->commentaryNext > game->now) return;
    const SSRHorse *r = &game->horses.horses[game->horses.runners[game->myHorse]];
    int cur = r->currentPosition < 0 ? 0 : r->currentPosition > 6 ? 6 : r->currentPosition;
    const char *line = NULL;
    if (r->raceposition > 0) {
        line = cur == 1 ? "05" : "22";
    } else {
        int d = game->commentaryPrev - cur;
        if (d <= -3 && d >= -5) line = DOWN3[cur];
        else if (d == -2) line = DOWN2[cur];
        else if (d == -1) line = DOWN1[cur];
        else if (d == 0) line = SAME[cur];
        else if (d == 1) line = UP1[cur];
        else if (d == 2) line = UP2[cur];
        else if (d >= 3 && d <= 5) line = UP3[cur];
    }
    if (line) {
        char tag[32], text[256], num[16];
        snprintf(tag, sizeof(tag), "HORSERACING_%s", line);
        snprintf(text, sizeof(text), "%s", loc(game, tag));
        snprintf(num, sizeof(num), "%d", (int)ssr_tweaks_number(game->tweaks, "HorseRacing", "SelectedHorse"));
        replace_all(text, sizeof(text), "$num", num);
        add_op(game, SSR_OP_TEXT, "CommentaryText", NULL, text, 0);
    }
    game->commentaryPrev = cur;
    game->commentaryNext = game->now + 1650;
}

static void race_result(SSRGame *game);

// TRacing.Update: the horses run, the camera follows the leader, and the
// places become final at frame 1200. The race ends at frame 1350.
static void race_tick(SSRGame *game) {
    SSRHorses *h = &game->horses;
    float moves[SSR_RUNNERS] = {0};
    if (ssr_horses_race_step(h, &game->casinoRandom, moves)) play_sound(game, "RacingSwitchPos");
    // UpdateGallop: two gallop loops, 890 ms apart.
    if (game->now > game->gallopNext) {
        play_sound(game, "AmbRacing");
        game->gallopNext = game->now + 890;
    }
    float lead = 0;
    for (int i = 0; i < h->runnerCount; i++) {
        game->horseX[i] += moves[i] * 0.66f;
        game->horseY[i] += 0.2f * (moves[i] * 0.66f);
    }
    for (int i = 0; i < h->runnerCount; i++) {
        if (game->horseX[i] > 350 && game->horseX[i] - 350 > lead) lead = game->horseX[i] - 350;
    }
    if (lead > 0) {
        for (int i = 0; i < h->runnerCount; i++) {
            game->horseX[i] -= lead;
            game->horseY[i] += 0.2f * -lead;
        }
    }
    char name[32], value[32];
    for (int i = 0; i < h->runnerCount; i++) {
        snprintf(name, sizeof(name), "Horse%d", i + 1);
        snprintf(value, sizeof(value), "%.3f", game->horseX[i]);
        add_op(game, SSR_OP_GEL, name, NULL, value, SSR_GEL_X);
        snprintf(value, sizeof(value), "%.3f", game->horseY[i]);
        add_op(game, SSR_OP_GEL, name, NULL, value, SSR_GEL_Y);
    }
    int frozen = game->raceFrame >= 1200;
    if (game->raceFrame >= 1350) {
        race_result(game);
        return;
    }
    int freeze = frozen && !game->standingsReady;
    ssr_horses_race_places(h, freeze);
    if (!frozen || freeze) {
        for (int i = 0; i < h->runnerCount; i++) {
            snprintf(name, sizeof(name), "Horse%dPlace", i + 1);
            ssr_tweaks_set_number(game->tweaks, "HorseRacing", name, h->horses[h->runners[i]].currentPosition);
        }
    }
    if (freeze) game->standingsReady = 1;
    // The badges under the track go to their places, the leader on the
    // right.
    for (int i = 0; i < h->runnerCount; i++) {
        const SSRHorse *r = &h->horses[h->runners[i]];
        if (r->raceposition != 0 && game->badgePlace[i] >= 0) continue;
        if (r->currentPosition == game->badgePlace[i]) continue;
        game->badgePlace[i] = r->currentPosition;
        snprintf(name, sizeof(name), "Horse%dBadge", i + 1);
        snprintf(value, sizeof(value), "%d", (6 - r->currentPosition) * 85 + 5);
        add_op(game, SSR_OP_CONVERGE_X, name, NULL, value, 0);
    }
    race_commentary(game);
    game->raceFrame += 3.15f;
}

// TRacing.EndRace and RacingResult.SetUpScreen: the betting slip, and the
// money. A bet pays the stake times the odds; the own horse pays 100, 50,
// or 25.
static void race_result(SSRGame *game) {
    SSRHorses *h = &game->horses;
    SSRTweaks *t = game->tweaks;
    set_screen(game, "racingresults");
    char name[32], text[128];
    int own = -1, win = 0, myPlace = 0;
    for (int i = 0; i < h->runnerCount; i++) {
        const SSRHorse *r = &h->horses[h->runners[i]];
        snprintf(name, sizeof(name), "Horse%dPlace", i + 1);
        ssr_tweaks_set_number(t, "HorseRacing", name, r->raceposition);
    }
    for (int p = 1; p <= h->runnerCount; p++) {
        for (int i = 0; i < h->runnerCount; i++) {
            const SSRHorse *r = &h->horses[h->runners[i]];
            if (r->raceposition != p) continue;
            if (r->betamount > 0) {
                if (p == 1) win = r->betamount * r->odds;
                myPlace = p;
            } else if (r->owned) {
                myPlace = p;
                own = i;
            }
            snprintf(name, sizeof(name), "Place%dHorse", p);
            ssr_tweaks_set(t, "HorseRacing", name, r->name);
        }
    }
    if (own >= 0) {
        int place = h->horses[h->runners[own]].raceposition;
        win = ssr_horses_prize(place);
        if (win == 0) {
            char position[32];
            snprintf(position, sizeof(position), "position_%d", place);
            snprintf(text, sizeof(text), "%s %s", loc(game, "Finished"), loc(game, position));
            quick_message(game, text, 1500);
        }
    }
    // Awards: the first prize of the own horse, and a bet that won.
    if (own >= 0 && win > 0 && win == ssr_horses_prize(1)) award(game, 65);
    if (own < 0 && win > 0) award(game, 63);
    if (win > 0) {
        update_bank(game, win);
        save_career(game);
        play_sound(game, "CCashIn");
        play_sound(game, "CSuccess");
        ssr_tweaks_set_number(t, "HorseRacing", "Winnings", win);
        snprintf(text, sizeof(text), "%d", win);
        ssr_tweaks_set(t, "HorseRacing", "WinningsStr", text);
        ssr_tweaks_set_number(t, "Casino", "CasinoOutcome", 1);
        ssr_tweaks_set_number(t, "Casino", "Winnings", win);
        casino_message(game, "bet_YouWon", win);
    } else {
        play_sound(game, "CError");
        ssr_tweaks_set_number(t, "HorseRacing", "Winnings", 0);
        if (own < 0) {
            ssr_tweaks_set_number(t, "Casino", "CasinoOutcome", -1);
            ssr_tweaks_set_number(t, "Casino", "Winnings", 0);
            casino_message(game, "bet_YouLost", game->bet);
            show_splash(game, "splashmessagebux", 1500);
        }
    }
    ssr_tweaks_set_number(t, "HorseRacing", "SelectedHorseResult", myPlace);
    snprintf(name, sizeof(name), "horse_placed_%d", myPlace);
    add_op(game, SSR_OP_TEXT, "HintText", NULL, loc(game, name), 0);
    check_stake(game);
}

// racingresults.btn_Next: after the own race the stable, else a new field.
static void results_next(SSRGame *game) {
    if (game->ownRace) racing_stable(game);
    else racing_screen(game);
}

// racing.btn_Quit: back to the casino.
static void racing_quit(SSRGame *game) {
    casino_screen(game);
}

// horsesforsale.btn_Buy: the stable holds 50 horses.
static void stable_buy(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (ssr_horses_count_owned(&game->horses) > 49) {
        play_sound(game, "CError");
        message(game, "message", loc(game, "CMESSAGE_OWN_TOO_MANY_HORSES"), 0, 1, NULL);
        return;
    }
    SSRHorse *h = selected_horse(game, 1);
    if (!h) return;
    int price = ssr_horses_price(h->strength);
    if (c->bank < price) {
        not_enough_cash(game, price - c->bank);
        return;
    }
    play_sound(game, "CCashOut");
    update_bank(game, -price);
    h->owned = 1;
    h->forsale = 0;
    save_career(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "StableTabPos", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "StableScroll", 0);
    award(game, 64);
    horse_tweaks(game, 1);
}

// The text of a stable message with the bux in "$cash".
static void stable_ask(SSRGame *game, const char *tag, int cash, int horse, const char *call) {
    char text[512], amount[24];
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    cash_star(cash, amount, sizeof(amount));
    replace_all(text, sizeof(text), "$cash", amount);
    game->messageData = horse;
    game->messageCash = cash;
    message(game, "message", text, 1, 1, call);
}

// mystable.btn_Train: one more star for the cost.
static void stable_train(SSRGame *game) {
    SSRHorse *h = selected_horse(game, 0);
    if (ssr_horses_count_owned(&game->horses) == 0 || !h) {
        play_sound(game, "CError");
        message(game, "message", loc(game, "CMESSAGE_NO_HORSE_AVAILABLE"), 0, 1, NULL);
        return;
    }
    int cost = ssr_horses_train_cost(h->strength);
    if (cost == 0) {
        play_sound(game, "CError");
        message(game, "message", loc(game, "CMESSAGE_HORSE_FULLY_TRAINED"), 0, 1, NULL);
        return;
    }
    stable_ask(game, "CMESSAGE_TRAINHORSE", cost, h->id, "TrainHorse");
}

// The call "TrainHorse".
static void call_train_horse(SSRGame *game) {
    if (game->returnState != 1) return;
    SSRCareer *c = &game->career;
    int cost = game->messageCash;
    if (c->bank < cost) {
        stable_screen(game);
        not_enough_cash(game, cost - c->bank);
        return;
    }
    SSRHorse *h = ssr_horses_by_id(&game->horses, game->messageData);
    if (!h) return;
    h->strength++;
    play_sound(game, "CCashOut");
    update_bank(game, -cost);
    save_career(game);
    stable_screen(game);
    horse_tweaks(game, 0);
}

// mystable.btn_SellHorse: half of the price.
static void stable_sell(SSRGame *game) {
    SSRHorse *h = selected_horse(game, 0);
    if (ssr_horses_count_owned(&game->horses) == 0 || !h) {
        play_sound(game, "CError");
        message(game, "message", loc(game, "CMESSAGE_NO_HORSE_AVAILABLE_SELL"), 0, 1, NULL);
        return;
    }
    stable_ask(game, "CMESSAGE_SELLHORSE", (int)((float)ssr_horses_price(h->strength) * 0.5f), h->id, "SellHorse");
}

// The call "SellHorse": the horse leaves the market until the next year.
static void call_sell_horse(SSRGame *game) {
    if (game->returnState != 1) return;
    SSRHorse *h = ssr_horses_by_id(&game->horses, game->messageData);
    if (!h) return;
    h->owned = h->forsale = 0;
    play_sound(game, "CCashIn");
    update_bank(game, game->messageCash);
    save_career(game);
    horse_tweaks(game, 1);
    stable_screen(game);
}

// mystable.btn_Race: the own horse races for a prize, with no stake.
static void stable_race(SSRGame *game) {
    SSRHorse *h = selected_horse(game, 0);
    if (ssr_horses_count_owned(&game->horses) == 0 || !h) {
        play_sound(game, "CError");
        message(game, "message", loc(game, "CMESSAGE_NO_HORSE_AVAILABLE_RACE"), 0, 1, NULL);
        return;
    }
    if (h->energy < 30) {
        message(game, "message", loc(game, "CMESSAGE_HORSETIRED"), 0, 1, NULL);
        return;
    }
    SSRHorses *horses = &game->horses;
    ssr_horses_select_runners(horses, h->id - 1, &game->casinoRandom);
    for (int i = 0; i < horses->runnerCount; i++) {
        if (horses->horses[horses->runners[i]].owned) {
            ssr_tweaks_set_number(game->tweaks, "HorseRacing", "SelectedHorse", i + 1);
            break;
        }
    }
    race_setup(game, 1);
}

/* -------------------------------------------------------- the blackjack */

static void blackjack_card_dealt(SSRGame *game, int player, int flip);

// AScreen_BlackJack.GetCardRef: the template of a card, "QueenOfHeart",
// or "FlippedCard" for a card face down.
static void playing_card_ref(const SSRCard *card, char *out, size_t capacity) {
    static const char *const RANKS[13] = {"Ace", "Two",   "Three", "Four", "Five",  "Six", "Seven",
                                          "Eight", "Nine", "Ten",   "Jack", "Queen", "King"};
    static const char *const SUITS[4] = {"Heart", "Diamond", "Club", "Spade"};
    if (card->desflip < 1.0f) snprintf(out, capacity, "FlippedCard");
    else snprintf(out, capacity, "%sOf%s", RANKS[card->num - 1], SUITS[card->suit]);
}

// ShowScores: the totals over the card places.
static void blackjack_scores(SSRGame *game, const char *player, const char *dealer) {
    ssr_tweaks_set(game->tweaks, "Casino", "PlayerBJTotal", player);
    ssr_tweaks_set(game->tweaks, "Casino", "BankerBJTotal", dealer);
}

// HideContextButtons and ShowContextButtons: Play and Back.
static void blackjack_context(SSRGame *game, int show) {
    add_op(game, show ? SSR_OP_SHOW : SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    add_op(game, show ? SSR_OP_SHOW : SSR_OP_HIDE, "BackButton", NULL, NULL, 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", !show);
}

// The banner of a hand (TQuickMessage "splashmessagecards").
static void blackjack_banner(SSRGame *game, const char *tag) {
    char text[128];
    snprintf(text, sizeof(text), "%s", loc(game, tag));
    upper(game, text);
    if (strcmp(tag, "bet_YouWon") == 0) {
        char cash[24];
        number_with_commas(game->bet * 2, cash, sizeof(cash));
        size_t n = strlen(text);
        snprintf(text + n, sizeof(text) - n, " \xE2\x98\x85%s", cash);
    }
    ssr_tweaks_set(game->tweaks, "Menu", "QuickMessage", text);
    show_splash(game, "splashmessagecards", 1500);
}

// TickerTapeShower.CloneDisposable().Show(): each win shows a new ticker
// tape. The hide and the show start its emitters again.
static void show_ticker_tape(SSRGame *game) {
    add_op(game, SSR_OP_HIDE, "TickerTapeShower", NULL, NULL, 0);
    add_op(game, SSR_OP_SHOW, "TickerTapeShower", NULL, NULL, 0);
}

// Win, Lose, and Tie of TScreen_BlackJack.
static void blackjack_result(SSRGame *game, int result) {
    int back = result == 1 ? game->bet * 2 : result == 0 ? game->bet : 0;
    // A tie gives the bet back with no sound and no save (Tie).
    if (result == 1) {
        play_sound(game, "CSuccess");
        play_sound(game, "CCashIn");
    }
    if (result == 2) play_sound(game, "CError");
    if (back > 0) update_bank(game, back);
    if (result == 1) {
        save_career(game);
        award(game, 33);
    }
    check_stake(game);
    if (result == 1) show_ticker_tape(game);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", result == 1 ? 1 : result == 0 ? 0 : -1);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", back);
}

// The things that the rules ask the screen to show (AScreen_BlackJack).
static void blackjack_events(SSRGame *game) {
    SSRBlackjack *bj = &game->blackjack;
    char ref[32], slot[32];
    for (int i = 0; i < bj->eventCount; i++) {
        const SSRBlackjackEvent *e = &bj->events[i];
        const SSRCard *card = e->card >= 0 ? &bj->cards[e->card] : NULL;
        switch (e->kind) {
        case SSR_BJ_DEALT:
            // Hit: a copy of the card slides into its place.
            if (!card || card->posinhand == 0) break;
            playing_card_ref(card, ref, sizeof(ref));
            snprintf(slot, sizeof(slot), "%sCard%d", card->owner == SSR_OWNER_DEALER ? "Dealer" : "Player",
                     card->posinhand);
            add_op(game, SSR_OP_HOOK, slot, ref, NULL, 1);
            add_op(game, SSR_OP_ANIM_PLAY, slot, NULL, "1", 0);
            play_sound(game, "DealCard");
            game->blackjackHitOff = 1;
            break;
        case SSR_BJ_FLIP:
            // FlipDealerCard: the face turns up over the hole card.
            playing_card_ref(card, ref, sizeof(ref));
            play_sound(game, "DealCard");
            add_op(game, SSR_OP_HOOK, "DealerCardToFlip", ref, NULL, 1);
            add_op(game, SSR_OP_SHOW, "DealerCardToFlip", NULL, NULL, 0);
            add_op(game, SSR_OP_HIDE, "DealerCard2", NULL, NULL, 0);
            add_op(game, SSR_OP_ANIM_PLAY, "DealerCardToFlip", NULL, "1", 0);
            blackjack_card_dealt(game, 0, 1);
            break;
        case SSR_BJ_BANNER:
            blackjack_banner(game, e->tag);
            break;
        case SSR_BJ_BUTTONS:
            add_op(game, e->value ? SSR_OP_SHOW : SSR_OP_HIDE, "HitButton", NULL, NULL, 0);
            add_op(game, e->value ? SSR_OP_SHOW : SSR_OP_HIDE, "HoldButton", NULL, NULL, 0);
            break;
        case SSR_BJ_RESULT:
            blackjack_result(game, e->value);
            break;
        case SSR_BJ_NEXT:
            // ResetCards, then Play and Back show again.
            for (int k = 1; k <= 5; k++) {
                snprintf(slot, sizeof(slot), "DealerCard%d", k);
                add_op(game, SSR_OP_HOOK, slot, "", NULL, 0);
                snprintf(slot, sizeof(slot), "PlayerCard%d", k);
                add_op(game, SSR_OP_HOOK, slot, "", NULL, 0);
            }
            add_op(game, SSR_OP_SHOW, "DealerCard2", NULL, NULL, 0);
            add_op(game, SSR_OP_HOOK, "DealerCardToFlip", "", NULL, 0);
            add_op(game, SSR_OP_HIDE, "DealerCardToFlip", NULL, NULL, 0);
            game->playerCardsDealt = game->dealerCardsDealt = 0;
            game->blackjackHitOff = 0;
            blackjack_context(game, 1);
            break;
        }
    }
    bj->eventCount = 0;
}

// The total of the first count cards of a hand, as the screen shows it.
// The soft total takes an ace as 11 when the total before it is 10 or
// less, and never takes it back.
static void shown_total(const SSRBlackjack *bj, const int *hand, int count, char *out, size_t capacity,
                        const char *or_) {
    int hard = 0, soft = 0;
    for (int i = 0; i < count; i++) {
        int v = bj->cards[hand[i]].num;
        v = v < 1 ? 1 : v > 10 ? 10 : v;
        hard += v;
        soft += v == 1 && soft <= 10 ? 11 : v;
    }
    if (soft <= 0) snprintf(out, capacity, "?");
    else if (hard != soft) snprintf(out, capacity, "%d %s %d", hard, or_, soft);
    else snprintf(out, capacity, "%d", soft);
}

// AScreen_BlackJack.CardDealt: a card is in its place, so its value shows.
// The hole card stays hidden.
static void blackjack_card_dealt(SSRGame *game, int player, int flip) {
    const SSRBlackjack *bj = &game->blackjack;
    if (!flip) {
        if (player) game->playerCardsDealt++;
        else game->dealerCardsDealt++;
    }
    int shown = game->dealerCardsDealt;
    if (shown == 2 && !flip) shown = 1;
    int p = game->playerCardsDealt < bj->playerCount ? game->playerCardsDealt : bj->playerCount;
    int d = shown < bj->dealerCount ? shown : bj->dealerCount;
    char ptext[32], dtext[32];
    shown_total(bj, bj->player, p, ptext, sizeof(ptext), loc(game, "or"));
    shown_total(bj, bj->dealer, d, dtext, sizeof(dtext), loc(game, "or"));
    blackjack_scores(game, ptext, dtext);
    game->blackjackHitOff = 0;
}

static void blackjack_player_slid(SSRGame *game) {
    blackjack_card_dealt(game, 1, 0);
}

static void blackjack_dealer_slid(SSRGame *game) {
    blackjack_card_dealt(game, 0, 0);
}

// casino.btn_BlackJack: BlackJack.SetUpScreen. The numbers take a new seed
// from the clock.
static void blackjack_screen(SSRGame *game) {
    game->casinoRandom.seed = game->now;
    set_screen(game, "blackjack");
    add_op(game, SSR_OP_HIDE, "TickerTapeShower", NULL, NULL, 0);
    add_op(game, SSR_OP_HIDE, "HitButton", NULL, NULL, 0);
    add_op(game, SSR_OP_HIDE, "HoldButton", NULL, NULL, 0);
    game->playerCardsDealt = game->dealerCardsDealt = 0;
    game->blackjackHitOff = 0;
    blackjack_scores(game, "?", "?");
    game->splashEnd = game->now;
    ssr_blackjack_setup(&game->blackjack, &game->casinoRandom);
    game->blackjack.buttons = 0;
    check_stake(game);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
}

// blackjack.btn_Play: the bet, then the deal.
static void blackjack_play(SSRGame *game) {
    SSRBlackjack *bj = &game->blackjack;
    if (bj->turn == SSR_BJ_PLAYER || bj->turn == SSR_BJ_DEALER) return;
    if (!casino_bet(game, game->bet, 3)) return;
    disable_stake(game);
    // Reset clears the messages, the energy alert of the bet too.
    game->splashEnd = game->now;
    ssr_blackjack_play(bj, &game->casinoRandom);
    blackjack_events(game);
    blackjack_context(game, 0);
}

// blackjack.btn_Hit: a card for the player. The button is off while a
// card slides.
static void blackjack_hit(SSRGame *game) {
    if (game->blackjack.turn != SSR_BJ_PLAYER || game->blackjackHitOff) return;
    ssr_blackjack_play(&game->blackjack, &game->casinoRandom);
    blackjack_events(game);
}

static void blackjack_hold(SSRGame *game) {
    if (game->blackjackHitOff) return;
    ssr_blackjack_hold(&game->blackjack);
    blackjack_events(game);
}

// blackjack.btn_Quit: Back is hidden while a hand runs. The next visit
// makes a new deck.
static void blackjack_quit(SSRGame *game) {
    if (game->blackjack.turn != SSR_BJ_IDLE) return;
    ssr_blackjack_clear(&game->blackjack);
    casino_screen(game);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
}

static void blackjack_tick(SSRGame *game) {
    ssr_blackjack_update(&game->blackjack, &game->casinoRandom, game->now < game->splashEnd);
    blackjack_events(game);
}

/* --------------------------------------------------------- the roulette */

// The wheel turns and the ball moves on the screen (SetElementAngle and
// SetElementPosition). The wheel image is 83 degrees round from the pockets.
static void roulette_place(SSRGame *game) {
    const SSRRoulette *r = &game->roulette;
    char value[32];
    snprintf(value, sizeof(value), "%.4f", r->rot + 83.0f);
    add_op(game, SSR_OP_GEL, "Wheel", NULL, value, SSR_GEL_ANGLE);
    snprintf(value, sizeof(value), "%.4f", r->x);
    add_op(game, SSR_OP_GEL, "Ball", NULL, value, SSR_GEL_X);
    snprintf(value, sizeof(value), "%.4f", r->y);
    add_op(game, SSR_OP_GEL, "Ball", NULL, value, SSR_GEL_Y);
}

// TScreen_Roulette.UpdateButtons: the bets and Back are off while the
// wheel turns.
static void roulette_buttons(SSRGame *game, int on) {
    static const char *const BUTTONS[7] = {"BackButton",  "GreenButton",  "RedButton",   "BlackButton",
                                           "Button1to12", "Button13to24", "Button25to36"};
    for (int i = 0; i < 7; i++) add_op(game, SSR_OP_ELEMENT_COLOUR, BUTTONS[i], NULL, on ? "FFFFFFFF" : "FFFFFF55", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", !on);
    game->rouletteSpinning = !on;
}

// casino.btn_Roulette: TScreen_Roulette.SetUpScreen.
static void roulette_screen(SSRGame *game) {
    game->casinoRandom.seed = game->now;
    set_screen(game, "roulette");
    add_op(game, SSR_OP_HIDE, "TickerTapeShower", NULL, NULL, 0);
    roulette_buttons(game, 1);
    check_stake(game);
    ssr_roulette_setup(&game->roulette, 545);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
    roulette_place(game);
}

// TScreen_Roulette.ButtonBet: the bet, then the spin.
static void roulette_bet(SSRGame *game, SSRRouletteBet bet) {
    if (game->rouletteSpinning || !casino_bet(game, game->bet, 2)) return;
    game->rouletteBet = bet;
    disable_stake(game);
    roulette_buttons(game, 0);
    ssr_roulette_spin(&game->roulette, &game->casinoRandom);
    play_sound(game, "RouletteSpin");
}

static void roulette_green(SSRGame *game) {
    roulette_bet(game, SSR_BET_GREEN);
}

static void roulette_red(SSRGame *game) {
    roulette_bet(game, SSR_BET_RED);
}

static void roulette_black(SSRGame *game) {
    roulette_bet(game, SSR_BET_BLACK);
}

static void roulette_seg1(SSRGame *game) {
    roulette_bet(game, SSR_BET_1TO12);
}

static void roulette_seg2(SSRGame *game) {
    roulette_bet(game, SSR_BET_13TO24);
}

static void roulette_seg3(SSRGame *game) {
    roulette_bet(game, SSR_BET_25TO36);
}

// TRoulette.GetResult, then ResetGame.
static void roulette_result(SSRGame *game) {
    int win = ssr_roulette_prize(game->roulette.ballNum, game->rouletteBet, game->bet);
    if (win > 0) {
        play_sound(game, "CCashIn");
        play_sound(game, "CSuccess");
        update_bank(game, win);
        save_career(game);
        casino_message(game, "bet_YouWon", win);
        award(game, 31);
        show_ticker_tape(game);
    } else {
        play_sound(game, "CError");
        casino_message(game, "bet_YouLost", game->bet);
    }
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", win > 0 ? 1 : -1);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", win);
    game->casinoRandom.seed = game->now;
    roulette_buttons(game, 1);
    check_stake(game);
    ssr_roulette_setup(&game->roulette, 545);
}

// TRoulette.Update: the wheel and the ball move in each update, also with
// no spin.
static void roulette_tick(SSRGame *game) {
    int result = ssr_roulette_update(&game->roulette, &game->casinoRandom);
    if (game->roulette.hit) play_sound(game, "RouletteBallHit");
    if (game->roulette.drop) play_sound(game, "RouletteBallDrop");
    if (result) roulette_result(game);
    roulette_place(game);
}

// roulette.btn_Quit: Back is off while the wheel turns.
static void roulette_quit(SSRGame *game) {
    if (game->rouletteSpinning) return;
    casino_screen(game);
}

/* ----------------------------------------------------------- the slots */

// AScreen_Slots.SetByOldUIPos: the frames of the eight icons of a reel.
// [I: the reel animation loops, so a frame goes round 96.]
static void slots_reel_frames(SSRGame *game, int r) {
    int f = ssr_slots_frame(&game->reels[r]);
    char ref[16], frame[16];
    snprintf(ref, sizeof(ref), "ReelAnims%d", r + 1);
    for (int i = 0; i < SSR_SLOTS_SYMBOLS; i++) {
        snprintf(frame, sizeof(frame), "%d", ((12 * i + f + 8) % 96 + 96) % 96);
        add_op(game, SSR_OP_ANIM_FRAME, ref, NULL, frame, i);
    }
}

// TScreen_Slots.Reset: the numbers take a new seed from the clock.
static void slots_reset(SSRGame *game) {
    game->slotsSpinning = 0;
    game->casinoRandom.seed = game->now;
    add_op(game, SSR_OP_SHOW, "PlayButton", NULL, NULL, 0);
    add_op(game, SSR_OP_SHOW, "BackButton", NULL, NULL, 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", 0);
}

// casino.btn_Slots: TScreen_Slots.SetUpScreen.
static void slots_screen(SSRGame *game) {
    set_screen(game, "slots");
    add_op(game, SSR_OP_HIDE, "TickerTapeShower", NULL, NULL, 0);
    slots_reset(game);
    for (int r = 0; r < 3; r++) {
        ssr_slots_create(&game->reels[r], r, &game->casinoRandom);
        slots_reel_frames(game, r);
    }
    check_stake(game);
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", 0);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", 0);
}

// slots.btn_Play: the bet, then the reels turn.
static void slots_play(SSRGame *game) {
    if (game->slotsSpinning || !casino_bet(game, game->bet, 1)) return;
    disable_stake(game);
    play_sound(game, "SlotsPlay");
    for (int r = 0; r < 3; r++) ssr_slots_spin(&game->reels[r], r + 1, game->now, &game->casinoRandom);
    game->slotsSpinning = 1;
    add_op(game, SSR_OP_HIDE, "PlayButton", NULL, NULL, 0);
    add_op(game, SSR_OP_HIDE, "BackButton", NULL, NULL, 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "HomeButtonDisabled", 1);
}

// TScreen_Slots.DoPrize.
static void slots_prize(SSRGame *game) {
    int win = ssr_slots_prize(game->reels, game->bet);
    if (win > 0) award(game, 32);
    play_sound(game, win > 0 ? "SlotsYouWin" : "SlotsYouLose");
    if (win > 0) {
        play_sound(game, "CCashIn");
        update_bank(game, win);
        save_career(game);
        casino_message(game, "bet_YouWon", win);
        // ShowPrizeAnim: the title flashes, and the ticker tape falls.
        add_op(game, SSR_OP_SHOW, "TitleAnim", NULL, NULL, 0);
        add_op(game, SSR_OP_ANIM_PLAY, "TitleAnim", NULL, "1", 0);
        show_ticker_tape(game);
    }
    ssr_tweaks_set_number(game->tweaks, "Casino", "CasinoOutcome", win > 0 ? 1 : -1);
    ssr_tweaks_set_number(game->tweaks, "Casino", "Winnings", win);
    check_stake(game);
}

// TScreen_Slots.Update: the reels turn, and the prize comes when all stop.
static void slots_tick(SSRGame *game) {
    int all = 1;
    for (int r = 0; r < 3; r++) {
        SSRSlotReel *reel = &game->reels[r];
        int moves = reel->yVel > 0;
        if (ssr_slots_update(reel, game->now)) play_sound(game, "SlotsItemLock");
        if (moves) slots_reel_frames(game, r);
        all &= reel->stopped;
    }
    if (all && game->slotsSpinning) {
        slots_prize(game);
        slots_reset(game);
    }
}

// slots.btn_Quit: the back button hides while the reels turn.
static void slots_quit(SSRGame *game) {
    if (game->slotsSpinning) return;
    casino_screen(game);
}

// GSCheckNewsGambling: the boss hears of a trip with the team mates, the
// team mates ask the player out, or the news tells of a gambling addict.
static int news_gambling(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (c->sdate > game->teammatesCasino + 14) game->teammatesCasino = 0;
    if (game->teammatesCasino > 0 && c->sdate > game->teammatesCasino && ssr_rand(&game->random, 4) == 1) {
        game->teammatesCasino = 0;
        relationships_screen(game, 1, 0);
        update_relation(game, SSR_RELATION_BOSS, -(ssr_career_relation(c, SSR_RELATION_BOSS) * 0.2f));
        update_relation(game, SSR_RELATION_TEAM, -(ssr_career_relation(c, SSR_RELATION_TEAM) * 0.05f));
        update_relation(game, SSR_RELATION_BOSS_INT,
                                   -(ssr_career_relation(c, SSR_RELATION_BOSS_INT) * 0.2f));
        update_relation(game, SSR_RELATION_TEAM_INT,
                                   -(ssr_career_relation(c, SSR_RELATION_TEAM_INT) * 0.05f));
        relation_tweaks(game, 0);
        message_bar(game, "messageboss", loc(game, "CMESSAGE_TEAMMATESCASINOBOSS"), "DoRelationScreen");
        return 1;
    }
    if (game->teammatesCasino == 0 && c->gambling == 0 && ssr_career_week(c) % 4 == 0 &&
        ssr_rand(&game->random, 2) == 1) {
        if (ssr_rand(&game->random, 2) == 1) {
            message(game, "messageteam", loc(game, "CMESSAGE_TEAMMATESCASINO"), 1, 1, "TeamMatesCasino");
        } else {
            message(game, "messageteam", loc(game, "CMESSAGE_TEAMMATESSHOPPING"), 1, 1, "TeamMatesShopping");
        }
        return 1;
    }
    if (c->gambling <= 35) return 0;
    // The partner hears of it, or the boss and the fans read it. [I: the
    // remake has no Twitter text.]
    if (ssr_rand(&game->random, 2) == 1 && c->relationPartner > 0) {
        update_relation(game, SSR_RELATION_PARTNER, -c->relationPartner * 0.4f);
        do_news(game, "CNEWS_GAMBLINGADDICTGIRL", NULL, NULL);
    } else {
        static const SSRRelation HIT[4] = {SSR_RELATION_BOSS, SSR_RELATION_FANS, SSR_RELATION_BOSS_INT,
                                           SSR_RELATION_FANS_INT};
        for (int i = 0; i < 4; i++) {
            update_relation(game, HIT[i], -(ssr_career_relation(c, HIT[i]) * 0.4f));
        }
        do_news(game, "CNEWS_GAMBLINGADDICTBOSS", my_club_tla(game), NULL);
    }
    webpage_to(game, "relationships", 0);
    c->gambling = 0;
    return 1;
}

// The call "TeamMatesCasino": Yes goes to the casino with the team mates.
static void call_teammates_casino(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState != 1) {
        update_relation(game, SSR_RELATION_TEAM, -5);
        relationships_screen(game, 0, 0);
        return;
    }
    game->teammatesCasino = c->sdate;
    set_teammates_bet_count(game, 0);
    casino_screen(game);
}

// The call "TeamMatesShopping": Yes opens the shop of the items.
static void call_teammates_shopping(SSRGame *game) {
    SSRCareer *c = &game->career;
    if (game->returnState != 1) {
        update_relation(game, SSR_RELATION_TEAM, -5);
        relationships_screen(game, 0, 0);
        return;
    }
    c->teamShopping = 0;
    relationships_screen(game, 0, 0);
    lifestyle_shop(game, 1);
}

// c_TScreen_GameMenu.ButtonQuit: the first retired career unlocks the
// hard mode. The message shows over the main menu.
static void quit_career(SSRGame *game) {
    save_career(game);
    // GameMenu.ButtonQuit: the player of the game is blank.
    game->hasPlayer = 0;
    ssr_tweaks_set_number(game->tweaks, "Menu", "CanSavePlayer", 0);
    main_menu(game);
    if (game->career.retired && !game->settings.hardModeUnlocked) {
        game->settings.hardModeUnlocked = 1;
        game->settingsChanged = 1;
        message(game, "message", loc(game, "CMESSAGE_HARDMODEUNLOCKED"), 0, 1, NULL);
    }
}

// The type of ball of the fixture of the match, or the league with no
// fixture (c_TScreen_ChooseMatchBall.ButtonOK).
static int match_ball_type(const SSRGame *game) {
    return game->matchBallType >= 1 && game->matchBallType <= 4 ? game->matchBallType : 1;
}

// c_Product_Ball: the tweaks of the nine balls of the overlay. The balls of
// SpecialEffect (7 to 9) are a purchase in 4.32. BallSpecialEffect 0 keeps
// them hidden.
static void match_ball_products(SSRGame *game) {
    static const char *NAMES[] = {"NS_Pro", "NS_Rocket", "NS_Arrow", "NS_Bender", "NS_Bouncer",
                                  "NS_Floater", "NS_SFX", "NS_SFX", "NS_SFX"};
    static const char *ATTRIBUTES[] = {"Speed", "Bounce", "Weight", "Curl"};
    int selected = ssr_career_selected_ball(&game->career, match_ball_type(game));
    char name[48], value[16];
    for (int ball = 1; ball <= SSR_BALL_TYPES; ball++) {
        int attributes[4] = {0};
        ssr_ball_type_attributes(ball, attributes);
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof(name), "MB_%d_%s", ball, ATTRIBUTES[i]);
            snprintf(value, sizeof(value), "%d", attributes[i]);
            ssr_tweaks_set(game->tweaks, "Products", name, value);
        }
        snprintf(name, sizeof(name), "MB_%d_Owned", ball);
        ssr_tweaks_set_number(game->tweaks, "Products", name, ball == selected);
        snprintf(name, sizeof(name), "MB_%d_DisplayString", ball);
        ssr_tweaks_set(game->tweaks, "Products", name, loc(game, NAMES[ball - 1]));
    }
    snprintf(value, sizeof(value), "MB_%d", selected);
    ssr_tweaks_set(game->tweaks, "Menu", "ItemString", value);
    ssr_tweaks_set_number(game->tweaks, "Menu", "BallSpecialEffect", 0);
    ssr_tweaks_set_number(game->tweaks, "Menu", "MatchBallScroll", 0);
}

// btn_MatchBall: c_TScreen_MatchBalls.SetUpScreen opens the overlay.
static void match_balls_open(SSRGame *game) {
    match_ball_products(game);
    snprintf(game->overlay, sizeof(game->overlay), "matchballs");
}

// chooseball.btn_OK: the ball of the item becomes the ball of this type of
// match. A ball costs nothing.
static void match_ball_pick(SSRGame *game) {
    const char *item = ssr_tweaks_get(game->tweaks, "Menu", "ItemString");
    int ball = strncmp(item, "MB_", 3) == 0 ? atoi(item + 3) : 0;
    if (ball >= 1 && ball <= SSR_BALL_TYPES) {
        ssr_career_set_selected_ball(&game->career, ball, match_ball_type(game));
    }
    match_ball_tweak(game);
    save_career(game);
    ssr_tweaks_set_number(game->tweaks, "Menu", "GameMenuHelpIncrement", 0);
    game->overlay[0] = 0;
}

// matchballs.btn_Back: c_TScreen_MatchBalls.ButtonBack.
static void match_balls_back(SSRGame *game) {
    save_career(game);
    game->overlay[0] = 0;
}

static const Route ROUTES[] = {
    {"gamemenu.btn_Life", relationships},
    {"relationships.btn_Home", relationships_home},
    {"relationships.btn_Achievements", achievements_screen},
    {"achievements.btn_Home", achievements_home},
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
    {"gamemenu.btn_Contract", game_contract},
    {"desiredtransfer.btn_Ok", desired_ok},
    {"desiredtransfer.btn_Back", desired_back},
    {"desiredtransfer.cmb_League", desired_league},
    {"desiredtransfer.cmb_Division", desired_division},
    {"desiredtransfer.cmb_Club", desired_club},
    {"contract.btn_Accept", contract_accept},
    {"contract.btn_Reject", contract_reject},
    {"contract.btn_Negotiate", contract_negotiate},
    {"negotiate.btn_Higher", negotiate_higher},
    {"negotiate.btn_Lower", negotiate_lower},
    {"negotiate.btn_Accept", negotiate_accept},
    {"relationships.btn_Shop", shop_from_relationships},
    {"shop.tab.items", shop_tab_items},
    {"shop.tab.vehicles", shop_tab_vehicles},
    {"shop.tab.property", shop_tab_property},
    {"shopoverlay.onItemSelected", overlay_item_selected},
    {"shopoverlay.buy", overlay_buy},
    {"gamemenu.btn_Shop", staff_shop},
    {"gamemenu.btn_BuyBoots", boots_shop},
    {"shopoverlay.open_nrg", nrg_shop},
    {"shop.tab.staff", shop_tab_staff},
    {"shop.tab.boots", shop_tab_boots},
    {"shop.tab.nrg", shop_tab_nrg},
    {"UpsellMsg.Close", shop_close},
    {"message.dismiss", alert_dismiss},
    {"UpsellMsg.DrinkNRG", shop_drink},
    {"UpsellMsg.CloseLifestyle", shop_close_lifestyle},
    {"PurchaseAnim.Close", purchase_close},
    {"Repair.Cancel", repair_cancel},
    {"Repair.Confirm", repair_confirm},
    {"Repair.All", repair_all},
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
    {"options.btn_Language", language_screen},
    {"language.btn_Ok", language_ok},
    {"options.btn_Back", main_menu},
    {"options.btn_Packs", packs_screen},
    {"packs.btn_Back", packs_back},
    {"packs.row_pack", packs_row},
    {"packs.btn_Remove", packs_remove},
    {"packs.btn_Install", packs_install},
    {"packs.inp_Seed", seed_start},
    {"packs.btn_Continue", continue_next},
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
    {"customcareer.btn_Age", custom_age},
    {"customcareer.btn_AgeDown", custom_age_down},
    {"customcareer.btn_Skill", custom_skill},
    {"customcareer.btn_SkillDown", custom_skill_down},
    {"customcareer.btn_Ok", custom_ok},
    {"customcareer.btn_Position", custom_position},
    {"customcareer.btn_PositionDown", custom_position_down},
    {"customcareer.btn_Club", custom_club},
    {"customcareer.btn_ClubDown", custom_club_down},
    {"customcareer.btn_Bux", custom_bux},
    {"customcareer.btn_BuxDown", custom_bux_down},
    {"customcareer.btn_Back", choose_nation},
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
    {"league.btn_Squad", league_squad},
    {"squad.btn_Mode", squad_mode},
    {"squad.btn_Back", squad_back},
    {"squad.inp_Name", squad_edit},
    {"squad.btn_Honours", honours_screen},
    {"squad.btn_Position", squad_position},
    {"honours.btn_Back", honours_back},
    {"honours.btn_Mode", honours_mode},
    {"league.row_team", league_row},
    {"league.cmb_League", league_combo},
    {"seasonreview.btn_League", season_review_league},
    {"seasonreview.btn_Play", player_of_the_year},
    {"playeroftheyear.btn_Play", poty_play},
    {"playeroftheyear.btn_Retire", poty_retire},
    {"relationships.btn_Casino", casino_screen},
    {"casino.btn_Back", casino_back},
    {"casino.btn_Home", casino_home},
    {"casino.btn_Slots", slots_screen},
    {"casino.btn_Racing", racing_screen},
    {"racing.btn_Quit", racing_quit},
    {"racing.btn_Race", racing_race},
    {"racing.btn_Stable", racing_stable},
    {"racingresults.btn_Next", results_next},
    {"racingresults.btn_Quit", racing_screen},
    {"racingresults.btn_Stable", racing_stable},
    {"mystable.btn_Quit", racing_screen},
    {"mystable.btn_Train", stable_train},
    {"mystable.btn_SellHorse", stable_sell},
    {"mystable.btn_Race", stable_race},
    {"horsesforsale.btn_Buy", stable_buy},
    {"casino.btn_BlackJack", blackjack_screen},
    {"blackjack.btn_Play", blackjack_play},
    {"blackjack.btn_Hit", blackjack_hit},
    {"blackjack.btn_Hold", blackjack_hold},
    {"blackjack.btn_Quit", blackjack_quit},
    {"blackjack.playerCardSlid", blackjack_player_slid},
    {"blackjack.dealerCardSlid", blackjack_dealer_slid},
    {"blackjack.dealerCardFlipped", blackjack_dealer_slid},
    {"casino.btn_Roulette", roulette_screen},
    {"roulette.btn_Quit", roulette_quit},
    {"roulette.btn_Green", roulette_green},
    {"roulette.btn_Red", roulette_red},
    {"roulette.btn_Black", roulette_black},
    {"roulette.btn_Seg1", roulette_seg1},
    {"roulette.btn_Seg2", roulette_seg2},
    {"roulette.btn_Seg3", roulette_seg3},
    {"casino.btn_StakeUp", stake_up},
    {"casino.btn_StakeDown", stake_down},
    {"slots.btn_Play", slots_play},
    {"slots.btn_Quit", slots_quit},
    {"retirement.btn_Play", retirement_play},
    {"match.btn_Play", match_play},
    {"btn_MatchBall", match_balls_open},
    {"chooseball.btn_OK", match_ball_pick},
    {"matchballs.btn_Back", match_balls_back},
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
    game->settings.language = -1;
    game->random.seed = SSR_RANDOM_START;
    casino_new_player(game);
    set_option_tweaks(game);
    set_screen(game, "mainmenu");
    return game;
}

void ssr_game_free(SSRGame *game) {
    if (!game) return;
    free(game->newBossRel);
    free(game->match);
    free(game->chance);
    free(game->replay);
    free(game->training);
    ssr_season_free(game->season);
    ssr_people_free(game->people);
    ssr_names_free(game->names);
    ssr_honours_clear(&game->honours);
    free(game->horseNames);
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

void ssr_game_set_horse_names(SSRGame *game, const char *text) {
    if (!game) return;
    free(game->horseNames);
    game->horseNames = NULL;
    if (!text) return;
    size_t n = strlen(text);
    game->horseNames = malloc(n + 1);
    if (game->horseNames) memcpy(game->horseNames, text, n + 1);
}

void ssr_game_set_names(SSRGame *game, const char *text) {
    if (!game) return;
    ssr_names_free(game->names);
    game->names = ssr_names_parse(text);
}

int ssr_game_add_players(SSRGame *game, const char *text) {
    return game ? ssr_names_add_players(game->names, text) : 0;
}

void ssr_game_set_world_seed(SSRGame *game, int seed) {
    if (game) game->nextWorldSeed = seed;
}

SSRPeople *ssr_game_people(SSRGame *game) {
    return game ? game->people : NULL;
}

const SSRHonours *ssr_game_honours(const SSRGame *game) {
    return game ? &game->honours : NULL;
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
        if (strncmp(line, "pack=", 5) == 0) {
            char *text = line + 5;
            text[strcspn(text, "\r\n")] = 0;
            snprintf(game->settings.pack, sizeof(game->settings.pack), "%s", text);
            continue;
        }
        if (sscanf(line, "%63[^=]=%d", key, &value) != 2) continue;
        if (strcmp(key, "volume") == 0 && value >= 0 && value <= 2) game->settings.volume = value;
        if (strcmp(key, "distance") == 0) game->settings.distance = value != 0;
        if (strcmp(key, "replays") == 0) game->settings.replays = value != 0;
        if (strcmp(key, "doneloseskills") == 0) game->settings.doneLoseSkills = value != 0;
        if (strcmp(key, "hardmodeunlocked") == 0) game->settings.hardModeUnlocked = value != 0;
        if (strcmp(key, "language") == 0 && value >= -1 && value <= 12) game->settings.language = value;
        if (strcmp(key, "worldseed") == 0 && value >= 0) game->settings.nextWorldSeed = value;
        if (strcmp(key, "continueworld") == 0) game->settings.continueWorld = value != 0;
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
    int ok = fprintf(file, "volume=%d\ndistance=%d\nreplays=%d\ndoneloseskills=%d\nhardmodeunlocked=%d\nlanguage=%d\n",
                     game->settings.volume, game->settings.distance, game->settings.replays,
                     game->settings.doneLoseSkills, game->settings.hardModeUnlocked, game->settings.language) > 0;
    ok = fprintf(file, "pack=%s\n", game->settings.pack) > 0 && ok;
    ok = fprintf(file, "worldseed=%d\ncontinueworld=%d\n", game->settings.nextWorldSeed,
                 game->settings.continueWorld) > 0 && ok;
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

void ssr_game_set_pack(SSRGame *game, const char *id) {
    if (!game) return;
    snprintf(game->settings.pack, sizeof(game->settings.pack), "%s", id ? id : "");
}

/* ------------------------------------------------------------ the ticker */

// The rule of most tickers: the requests since it last showed.
static int ticker_counter(int *count) {
    return ++*count;
}

static int total_nrg(const SSRCareer *c) {
    int n = 0;
    for (int i = 0; i < 10; i++) n += c->nrg[i];
    return n;
}

// The first skill that the trainer would warn of, in the order of
// Ticker_Skill_Degrade (pace, power, set pieces, technique, vision), or -1.
static int degrading_skill(const SSRCareer *c) {
    static const int ROWS[5] = {0, 1, 4, 2, 3};
    int limit = last_trained_limit(c);
    for (int i = 0; i < 5; i++) {
        int r = ROWS[i], start = c->deplete[r] < 0 ? -c->deplete[r] : c->deplete[r];
        if (c->sdate > start + limit && skill_levels((SSRCareer *)c, r)[1] > 0) return i;
    }
    return -1;
}

// The first product, in the order of the shop, that wears too much.
static const SSRShopItem *worn_item(const SSRCareer *c) {
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = ssr_shop_item(i);
        if (!item) continue;
        const int *wear = item->type == SSR_SHOP_ITEM ? c->items : item->type == SSR_SHOP_VEHICLE ? c->vehicles : c->properties;
        if (wear[item->id - 1] > 1000) return item;
    }
    return NULL;
}

// A rival of the player at a club. turn picks one in turn.
static SSRPerson *world_rival(SSRGame *game, int turn) {
    SSRPerson *rivals[8];
    int n = 0;
    for (int i = 0; game->people && i < game->people->count && n < 8; i++) {
        SSRPerson *p = &game->people->list[i];
        if (p->rival && !p->retired && p->club) rivals[n++] = p;
    }
    return n ? rivals[(turn < 0 ? 0 : turn) % n] : NULL;
}

// The top scorer of the league of the player.
static SSRPerson *world_scorer(SSRGame *game) {
    if (!game->people || !game->season) return NULL;
    SSRSeasonComp *comp = ssr_season_comp(game->season, ssr_season_actual_league(game->season, game->career.clubId));
    if (!comp || comp->poolCount == 0 || comp->data->level != 0) return NULL;
    int clubs[64], n = 0;
    for (int i = 0; i < comp->pools[0].count && n < 64; i++) clubs[n++] = game->season->rows[comp->pools[0].rows[i]].teamId;
    SSRPerson *top = NULL;
    return ssr_people_top_scorers(game->people, clubs, n, &top, 1) ? top : NULL;
}

// 1 when the next match of the player is a club match between two clubs
// near each other. home and away get the clubs.
// The longest run of the league of the player now: a club with 5 wins or
// 4 defeats in a row in its last league matches. run gets the length,
// with a minus for defeats. Returns the club, or 0.
static int form_run(SSRGame *game, int *run) {
    SSRSeason *s = game->season;
    SSRSeasonComp *comp = s ? ssr_season_comp(s, ssr_season_actual_league(s, game->career.clubId)) : NULL;
    *run = 0;
    if (!comp || comp->data->type != 0 || comp->poolCount == 0) return 0;
    int best = 0, bestRun = 0;
    SSRPool *pool = &comp->pools[0];
    for (int r = 0; r < pool->count; r++) {
        int team = s->rows[pool->rows[r]].id, streak = 0;
        // The fixtures go by date; the run counts back from the last one.
        for (int k = comp->fixtureCount - 1; k >= 0; k--) {
            const SSRFixture *f = &comp->fixtures[k];
            if (f->result != 1 || (f->home != team && f->away != team)) continue;
            int mine = f->home == team ? f->score1 : f->score2, theirs = f->home == team ? f->score2 : f->score1;
            int sign = mine > theirs ? 1 : mine < theirs ? -1 : 0;
            if (sign == 0 || (streak != 0 && (streak > 0) != (sign > 0))) break;
            streak += sign;
        }
        int strong = streak >= 5 || streak <= -4;
        if (strong && abs(streak) > abs(bestRun)) {
            best = s->rows[pool->rows[r]].teamId;
            bestRun = streak;
        }
    }
    *run = bestRun;
    return best;
}

// The league of the player and its first pool, sorted as a table, or NULL.
static SSRSeasonComp *my_table(SSRGame *game, SSRPool **pool) {
    SSRSeason *s = game->season;
    SSRSeasonComp *comp = s ? ssr_season_comp(s, ssr_season_actual_league(s, game->career.clubId)) : NULL;
    if (!comp || comp->data->type != 0 || comp->poolCount == 0 || comp->pools[0].count < 4) return NULL;
    *pool = &comp->pools[0];
    ssr_season_sort_pool(s, *pool, 4);
    return comp;
}

// The rounds before the table can show a crisis.
#define CRISIS_ROUNDS 8

// A crisis: one of the three strongest clubs of the league of the player in
// the bottom four after some rounds. Returns the club, or 0. position gets
// its place in the table.
static int crisis(SSRGame *game, int *position) {
    SSRPool *pool = NULL;
    if (!my_table(game, &pool)) return 0;
    const SSRTableRow *rows = game->season->rows;
    int strongest[3] = {0, 0, 0};
    for (int r = 0; r < pool->count; r++) {
        const SSRTableRow *row = &rows[pool->rows[r]];
        for (int k = 0; k < 3; k++) {
            const SSRTableRow *other = strongest[k] ? &rows[strongest[k] - 1] : NULL;
            if (!other || row->strength > other->strength) {
                for (int m = 2; m > k; m--) strongest[m] = strongest[m - 1];
                strongest[k] = pool->rows[r] + 1;
                break;
            }
        }
    }
    for (int r = pool->count - 1; r >= pool->count - 4 && r >= 0; r--) {
        int index = pool->rows[r];
        if (rows[index].played < CRISIS_ROUNDS) continue;
        for (int k = 0; k < 3; k++) {
            if (strongest[k] == index + 1) {
                *position = r + 1;
                return rows[index].teamId;
            }
        }
    }
    return 0;
}

// The skill from which a young player of a small club makes a rumour.
#define RUMOUR_SKILL 60

// A rumour: the best player of 23 or younger at a club of the league of the
// player that is not one of the three strongest, linked with the strongest
// club that is not his own. Returns the player, or NULL. big gets the club.
static SSRPerson *rumour(SSRGame *game, int *big) {
    SSRPool *pool = NULL;
    if (!game->people || !my_table(game, &pool)) return NULL;
    const SSRTableRow *rows = game->season->rows;
    int clubs[64], strengths[64], n = 0;
    for (int r = 0; r < pool->count && n < 64; r++) {
        clubs[n] = rows[pool->rows[r]].teamId;
        strengths[n++] = rows[pool->rows[r]].strength;
    }
    int year = ssr_career_year(&game->career);
    SSRPerson *best = NULL;
    int bestClub = 0;
    for (int i = 0; i < game->people->count; i++) {
        SSRPerson *p = &game->people->list[i];
        if (p->retired || year - p->born > 23 || p->skill < RUMOUR_SKILL || p->club == game->career.clubId) continue;
        int at = -1, stronger = 0;
        for (int k = 0; k < n; k++) {
            if (clubs[k] == p->club) at = k;
        }
        if (at < 0) continue;
        for (int k = 0; k < n; k++) stronger += strengths[k] > strengths[at];
        if (stronger < 3) continue;
        if (!best || p->skill > best->skill) {
            best = p;
            bestClub = at;
        }
    }
    if (!best) return NULL;
    int top = -1;
    for (int k = 0; k < n; k++) {
        if (k != bestClub && (top < 0 || strengths[k] > strengths[top])) top = k;
    }
    *big = top >= 0 ? clubs[top] : 0;
    return *big ? best : NULL;
}

// The record of goals in a season of the league of the player, from the
// honours, and the top scorer of the league now. Returns 1 when the top
// scorer is near the record (3 goals or less) or past it.
static int record_chase(SSRGame *game, char *name, size_t capacity, int *goals, int *record, const char **league) {
    SSRPool *pool = NULL;
    SSRSeasonComp *comp = my_table(game, &pool);
    if (!comp || !game->people) return 0;
    *record = 0;
    for (int i = 0; i < game->honours.count; i++) {
        const SSRHonour *h = &game->honours.list[i];
        if (strcmp(h->league, comp->data->tla) == 0 && h->goals > *record) *record = h->goals;
    }
    if (*record < 5) return 0;
    int clubs[64], n = 0;
    for (int r = 0; r < pool->count && n < 64; r++) clubs[n++] = game->season->rows[pool->rows[r]].teamId;
    SSRPerson *top = NULL;
    *goals = 0;
    if (ssr_people_top_scorers(game->people, clubs, n, &top, 1)) {
        snprintf(name, capacity, "%s %s", top->first, top->last);
        *goals = top->seasonGoals;
    }
    if (game->career.seasonGoals > *goals) {
        snprintf(name, capacity, "%s", game->career.fullName);
        *goals = game->career.seasonGoals;
    }
    *league = comp->data->tla;
    return *goals > 0 && *goals >= *record - 3;
}

// The team mate with the most goals of the season, or NULL.
static SSRPerson *strike_partner(SSRGame *game) {
    int club = game->career.clubId;
    SSRPerson *top = NULL;
    return game->people && ssr_people_top_scorers(game->people, &club, 1, &top, 1) ? top : NULL;
}

// The young player that the player mentors: the youngest team mate of 19
// or less with a skill to reach of 70 or more.
static SSRPerson *mentee(SSRGame *game) {
    SSRPerson *squad[64], *best = NULL;
    int n = game->people ? ssr_people_squad(game->people, game->career.clubId, squad, 64) : 0;
    int year = ssr_career_year(&game->career);
    for (int i = 0; i < n && i < 64; i++) {
        if (year - squad[i]->born > 19 || squad[i]->potential < 70 || squad[i]->rival) continue;
        if (!best || squad[i]->born > best->born) best = squad[i];
    }
    return best;
}

// A small club in the last eight of a cup of the nation of the club of the
// player: a club still in the cup with a strength 25 under the mean of the
// others. comp gets the cup. Returns the club, or 0.
static int cup_run(SSRGame *game, SSRSeasonComp **cup) {
    SSRSeason *s = game->season;
    const SSRClub *mine = game->world ? ssr_world_club(game->world, game->career.clubId) : NULL;
    if (!s || !mine) return 0;
    for (int i = 0; i < s->compCount; i++) {
        SSRSeasonComp *comp = &s->comps[i];
        if (comp->data->type != 1 || comp->data->level != 0 || comp->data->based != mine->nation) continue;
        int last = 0;
        for (int k = 0; k < comp->fixtureCount; k++) {
            const SSRFixture *f = &comp->fixtures[k];
            if (f->round >= 1 && ssr_fixture_home_team(s, f) > 0 && ssr_fixture_away_team(s, f) > 0 && f->sdate > last) {
                last = f->sdate;
            }
        }
        int teams[16], n = 0, fixtures = 0;
        for (int k = 0; k < comp->fixtureCount; k++) {
            const SSRFixture *f = &comp->fixtures[k];
            if (f->sdate != last) continue;
            fixtures++;
            int home = ssr_fixture_home_team(s, f), away = ssr_fixture_away_team(s, f);
            int winner = f->result == 1 ? ssr_fixture_winner(s, f) : 0;
            if (n < 16 && (!winner || winner == f->home)) teams[n++] = home;
            if (n < 16 && (!winner || winner == f->away)) teams[n++] = away;
        }
        if (fixtures == 0 || fixtures > 4 || n < 2) continue;
        int small = 0, total = 0;
        for (int k = 0; k < n; k++) {
            const SSRClub *c = ssr_world_club(game->world, teams[k]);
            if (!c) continue;
            total += c->strength;
            const SSRClub *best = small ? ssr_world_club(game->world, small) : NULL;
            if (!best || c->strength < best->strength) small = c->id;
        }
        const SSRClub *c = ssr_world_club(game->world, small);
        if (!c || c->strength + 25 > (total - c->strength) / (n - 1)) continue;
        *cup = comp;
        return small;
    }
    return 0;
}

// 1 for a derby, 2 for a match of a big rivalry.
static int next_derby(SSRGame *game, int *home, int *away) {
    SSRFixture *f = game->season ? ssr_season_next_fixture(game->season) : NULL;
    SSRSeasonComp *comp = f ? ssr_season_comp(game->season, f->compId) : NULL;
    if (!f || !comp || comp->data->level != 0) return 0;
    *home = ssr_fixture_home_team(game->season, f);
    *away = ssr_fixture_away_team(game->season, f);
    if (ssr_world_is_derby(game->world, *home, *away)) return 1;
    return ssr_honours_rivalry(&game->honours, *home, *away) >= SSR_RIVALRY_POINTS ? 2 : 0;
}

// The league table of the player after 3 matches or more: the first, the
// second, and the last row. Returns the competition, or NULL.
static SSRSeasonComp *table_story(SSRGame *game, const SSRTableRow **rows) {
    SSRSeason *s = game->season;
    SSRSeasonComp *comp = s ? ssr_season_comp(s, ssr_season_actual_league(s, game->career.clubId)) : NULL;
    if (!comp || comp->data->type != 0 || comp->poolCount == 0 || comp->pools[0].count < 3) return NULL;
    SSRPool *pool = &comp->pools[0];
    ssr_season_sort_pool(s, pool, 4);
    rows[0] = &s->rows[pool->rows[0]];
    rows[1] = &s->rows[pool->rows[1]];
    rows[2] = &s->rows[pool->rows[pool->count - 1]];
    return rows[0]->played >= 3 ? comp : NULL;
}

static void person_name(const SSRPerson *p, char *out, size_t capacity) {
    snprintf(out, capacity, "%s %s", p->first, p->last);
}

static const char *club_name(SSRGame *game, int id) {
    const SSRClub *club = game->world ? ssr_world_club(game->world, id) : NULL;
    return club ? club->name : "";
}

int ssr_game_ticker_priority(SSRGame *game, int kind, int *count) {
    if (!game || !count || !game->hasPlayer) return -1;
    const SSRCareer *c = &game->career;
    int club = c->clubId != 0;
    switch (kind) {
    case SSR_TICKER_NEXT_MATCH:
        season_in(game);
        if (!club || !game->season || !ssr_season_next_fixture(game->season)) return -1;
        return ++*count < 2 ? 0 : 1000;
    case SSR_TICKER_LOW_NRG:
        return club && total_nrg(c) <= 2 ? ticker_counter(count) : -1;
    case SSR_TICKER_LOW_AGENT:
    case SSR_TICKER_LOW_TRAINER: {
        int level = kind == SSR_TICKER_LOW_AGENT ? ssr_career_agent(c) : ssr_career_trainer(c);
        const int *left = kind == SSR_TICKER_LOW_AGENT ? c->agent : c->trainer;
        int weeks = (level ? left[level - 1] : 0) - 1;
        return club && weeks > 0 && weeks <= 5 ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_LOW_BOOT:
        return club && c->bootId != 0 && c->bootUses <= 5 ? ticker_counter(count) : -1;
    case SSR_TICKER_LOW_BOSS:
        return club && (int)c->relationBoss <= 30 ? ticker_counter(count) : -1;
    case SSR_TICKER_LOW_FANS:
        return club && (int)c->relationFans <= 30 ? ticker_counter(count) : -1;
    case SSR_TICKER_LOW_TEAM:
        return club && (int)c->relationTeam <= 30 ? ticker_counter(count) : -1;
    case SSR_TICKER_LOW_SPONSORS:
        return club && ssr_career_count_sponsors(c) > 0 && (int)c->relationSponsors <= 30 ? ticker_counter(count)
                                                                                           : -1;
    case SSR_TICKER_SKILL_DEGRADE:
        return club && ssr_career_trainer(c) > 0 && degrading_skill(c) >= 0 ? ticker_counter(count) : -1;
    case SSR_TICKER_LIFESTYLE_DEGRADE:
        return worn_item(c) ? ticker_counter(count) : -1;
    case SSR_TICKER_CAREER_STAT:
        return c->apps >= 5 ? ticker_counter(count) : -1;
    case SSR_TICKER_HIGH_BOSS:
        return (int)c->relationBoss >= 80 ? ticker_counter(count) : -1;
    case SSR_TICKER_HIGH_FANS:
        return (int)c->relationFans >= 80 ? ticker_counter(count) : -1;
    case SSR_TICKER_HIGH_TEAM:
        return (int)c->relationTeam >= 80 ? ticker_counter(count) : -1;
    case SSR_TICKER_HIGH_PARTNER:
        return (int)c->relationPartner >= 80 ? ticker_counter(count) : -1;
    case SSR_TICKER_HIGH_SPONSORS:
        return club && ssr_career_count_sponsors(c) > 0 && (int)c->relationSponsors >= 80 ? ticker_counter(count)
                                                                                           : -1;
    case SSR_TICKER_AGENT_HINT: {
        if (!club || ssr_career_agent(c) == 0 || c->sdate < c->lastNegotiations + 56) return -1;
        // GetOffer of the own club: the count of the offers goes to 0.
        SSRContractOffer offer;
        ssr_contract_offer(&game->contract, c, c->clubId, 0, &offer);
        if (offer.wage <= c->wage || (int)c->relationBoss < 25 || (int)c->relationTeam < 25 ||
            (int)c->relationFans < 25) {
            return -1;
        }
        return ticker_counter(count);
    }
    case SSR_TICKER_WORLD_RIVAL:
        return club && world_rival(game, *count) ? ticker_counter(count) : -1;
    case SSR_TICKER_WORLD_SCORER:
        return club && world_scorer(game) ? ticker_counter(count) : -1;
    case SSR_TICKER_WORLD_MOVE:
        return club && game->people && game->people->moveCount > 0 ? ticker_counter(count) : -1;
    case SSR_TICKER_WORLD_DERBY: {
        season_in(game);
        int home = 0, away = 0;
        return club && next_derby(game, &home, &away) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_TABLE: {
        season_in(game);
        const SSRTableRow *rows[3];
        return club && table_story(game, rows) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_MANAGER:
        return club && game->people && game->people->sackedCount > 0 ? ticker_counter(count) : -1;
    case SSR_TICKER_WORLD_CUPRUN: {
        season_in(game);
        SSRSeasonComp *cup = NULL;
        return club && cup_run(game, &cup) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_PARTNER:
        return club && strike_partner(game) ? ticker_counter(count) : -1;
    case SSR_TICKER_WORLD_MENTEE:
        return club && mentee(game) ? ticker_counter(count) : -1;
    case SSR_TICKER_INJURY:
        // As the next match: first after one turn, then each time.
        return club && c->injury > 0 ? (++*count < 2 ? 0 : 1000) : -1;
    case SSR_TICKER_WORLD_FORM: {
        season_in(game);
        int run = 0;
        return club && form_run(game, &run) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_CRISIS: {
        season_in(game);
        int position = 0;
        return club && crisis(game, &position) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_RUMOUR: {
        season_in(game);
        int big = 0;
        return club && rumour(game, &big) ? ticker_counter(count) : -1;
    }
    case SSR_TICKER_WORLD_RECORD: {
        season_in(game);
        char name[96];
        int goals = 0, record = 0;
        const char *league = "";
        return club && record_chase(game, name, sizeof(name), &goals, &record, &league) ? ticker_counter(count) : -1;
    }
    default:
        return -1;
    }
}

void ssr_game_ticker_text(SSRGame *game, int kind, int *count, char *text, size_t capacity) {
    if (!game || !count || !text || capacity == 0) return;
    const SSRCareer *c = &game->career;
    char number[16];
    *count = 0;
    switch (kind) {
    case SSR_TICKER_NEXT_MATCH: {
        season_in(game);
        SSRFixture *f = game->season ? ssr_season_next_fixture(game->season) : NULL;
        SSRSeasonComp *comp = f ? ssr_season_comp(game->season, f->compId) : NULL;
        if (!f || !comp) return;
        char names[5][64];
        fixture_strings(game, comp, f, names);
        snprintf(text, capacity, "%s", loc(game, "TICKER_NEXTMATCH"));
        snprintf(number, sizeof(number), "%d", ssr_date_week(f->sdate));
        replace_all(text, capacity, "$week", number);
        replace_all(text, capacity, "$hometeam", names[1]);
        replace_all(text, capacity, "$awayteam", names[3]);
        replace_all(text, capacity, "$competition", comp->data->tla);
        break;
    }
    case SSR_TICKER_LOW_NRG: {
        int n = total_nrg(c);
        snprintf(text, capacity, "%s", loc(game, n > 0 ? "TICKER_LOW_NRG" : "TICKER_EMPTY_NRG"));
        snprintf(number, sizeof(number), "%d", n);
        replace_all(text, capacity, "$num", number);
        break;
    }
    case SSR_TICKER_LOW_AGENT:
    case SSR_TICKER_LOW_TRAINER: {
        int agent = kind == SSR_TICKER_LOW_AGENT;
        int level = agent ? ssr_career_agent(c) : ssr_career_trainer(c);
        const int *left = agent ? c->agent : c->trainer;
        snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_STAFF"));
        snprintf(number, sizeof(number), "%d", (level ? left[level - 1] : 0) - 1);
        replace_all(text, capacity, "$num", number);
        // The game puts the English word, not a text of the language.
        replace_all(text, capacity, "$staff", agent ? "Agent" : "Trainer");
        break;
    }
    case SSR_TICKER_LOW_BOOT:
        snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_BOOTS"));
        snprintf(number, sizeof(number), "%d", c->bootUses);
        replace_all(text, capacity, "$num", number);
        // [I: the game shows no name of the boots: its m_title is empty.]
        replace_all(text, capacity, "$boot", "");
        break;
    case SSR_TICKER_LOW_BOSS:
        snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_BOSS"));
        break;
    case SSR_TICKER_LOW_FANS:
        snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_FANS"));
        break;
    case SSR_TICKER_LOW_TEAM:
        snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_TEAM"));
        break;
    case SSR_TICKER_LOW_SPONSORS:
        // From 20 to 30 the ticker keeps its last text, as the game does.
        if (ssr_career_count_sponsors(c) > 0 && c->relationSponsors < 20.0f) {
            for (int i = 9; i >= 0; i--) {
                if (c->sponsors[i] <= 0) continue;
                char name[128];
                snprintf(name, sizeof(name), "%s", sponsor_name(game, i));
                upper(game, name);
                snprintf(text, capacity, "%s", loc(game, "TICKER_LOW_SPONSORS"));
                replace_all(text, capacity, "$sponsor", name);
                break;
            }
        }
        break;
    case SSR_TICKER_SKILL_DEGRADE: {
        static const char *const TAGS[5] = {"SKILL_PACE", "SKILL_POWER", "SKILL_SETPIECES", "SKILL_TECHNIQUE",
                                            "SKILL_VISION"};
        int skill = degrading_skill(c);
        snprintf(text, capacity, "%s", loc(game, "TICKER_SKILL_DEGRADE_1"));
        replace_all(text, capacity, "$skill", skill >= 0 ? loc(game, TAGS[skill]) : "DEBUG SKILL");
        break;
    }
    case SSR_TICKER_LIFESTYLE_DEGRADE: {
        const SSRShopItem *item = worn_item(c);
        char tag[48], name[128];
        snprintf(tag, sizeof(tag), "TICKER_LIFESTYLE_DEGRADE_%d", ssr_rand2(&game->random, 1, 3));
        snprintf(name, sizeof(name), "%s", item ? loc(game, item->uid) : "DebugItem");
        upper(game, name);
        snprintf(text, capacity, "%s", loc(game, tag));
        replace_all(text, capacity, "$item", name);
        break;
    }
    case SSR_TICKER_CAREER_STAT: {
        int assists = ssr_rand2(&game->random, 1, 2) == 1;
        snprintf(text, capacity, "%s", loc(game, assists ? "TICKER_CAREERSTAT_ASSISTS" : "TICKER_CAREERSTAT_CHANCES"));
        snprintf(number, sizeof(number), "%d", assists ? c->assists : c->chances);
        replace_all(text, capacity, "$num", number);
        break;
    }
    case SSR_TICKER_HIGH_BOSS:
        snprintf(text, capacity, "%s", loc(game, "TICKER_HIGH_BOSS"));
        break;
    case SSR_TICKER_HIGH_FANS:
        snprintf(text, capacity, "%s", loc(game, "TICKER_HIGH_FANS"));
        break;
    case SSR_TICKER_HIGH_TEAM:
        snprintf(text, capacity, "%s", loc(game, "TICKER_HIGH_TEAM"));
        break;
    case SSR_TICKER_HIGH_PARTNER:
        snprintf(text, capacity, "%s", loc(game, "TICKER_HIGH_GIRLFRIEND"));
        break;
    case SSR_TICKER_HIGH_SPONSORS:
        snprintf(text, capacity, "%s", loc(game, "TICKER_HIGH_SPONSORS"));
        break;
    case SSR_TICKER_AGENT_HINT:
        snprintf(text, capacity, "%s", loc(game, "TICKER_CONTRACT"));
        break;
    case SSR_TICKER_WORLD_RIVAL:
    case SSR_TICKER_WORLD_SCORER: {
        int rival = kind == SSR_TICKER_WORLD_RIVAL;
        const SSRPerson *p = rival ? world_rival(game, game->worldTurn++) : world_scorer(game);
        if (!p) return;
        char name[64], value[16];
        person_name(p, name, sizeof(name));
        snprintf(text, capacity, "%s", loc(game, rival ? "SSR_TICKER_RIVAL" : "SSR_TICKER_TOPSCORER"));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$club", club_name(game, p->club));
        snprintf(value, sizeof(value), "%d", p->seasonGoals);
        replace_all(text, capacity, "$goals", value);
        snprintf(value, sizeof(value), "%d", p->seasonApps);
        replace_all(text, capacity, "$apps", value);
        SSRSeasonComp *comp = game->season ? ssr_season_comp(game->season, ssr_season_actual_league(game->season, c->clubId)) : NULL;
        replace_all(text, capacity, "$league", comp ? comp->data->tla : "");
        break;
    }
    case SSR_TICKER_WORLD_CUPRUN: {
        season_in(game);
        SSRSeasonComp *cup = NULL;
        int small = cup_run(game, &cup);
        if (!small) return;
        snprintf(text, capacity, "%s", loc(game, "SSR_TICKER_CUPRUN"));
        replace_all(text, capacity, "$club", club_name(game, small));
        replace_all(text, capacity, "$cup", cup->data->tla);
        break;
    }
    case SSR_TICKER_WORLD_PARTNER:
    case SSR_TICKER_WORLD_MENTEE: {
        int partner = kind == SSR_TICKER_WORLD_PARTNER;
        const SSRPerson *p = partner ? strike_partner(game) : mentee(game);
        if (!p) return;
        char name[64];
        person_name(p, name, sizeof(name));
        snprintf(text, capacity, "%s", loc(game, partner ? "SSR_TICKER_PARTNER" : "SSR_TICKER_MENTEE"));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$club", club_name(game, p->club));
        snprintf(number, sizeof(number), "%d", p->seasonGoals);
        replace_all(text, capacity, "$goals", number);
        snprintf(number, sizeof(number), "%d", ssr_career_year(c) - p->born);
        replace_all(text, capacity, "$age", number);
        break;
    }
    case SSR_TICKER_INJURY:
        snprintf(text, capacity, "%s", loc(game, "SSR_Injured"));
        snprintf(number, sizeof(number), "%d", c->injury);
        replace_all(text, capacity, "$num", number);
        break;
    case SSR_TICKER_WORLD_FORM: {
        season_in(game);
        int run = 0, team = form_run(game, &run);
        if (!team) return;
        snprintf(text, capacity, "%s", loc(game, run > 0 ? "SSR_TICKER_GOODRUN" : "SSR_TICKER_BADRUN"));
        replace_all(text, capacity, "$club", club_name(game, team));
        snprintf(number, sizeof(number), "%d", abs(run));
        replace_all(text, capacity, "$num", number);
        break;
    }
    case SSR_TICKER_WORLD_CRISIS: {
        season_in(game);
        int position = 0, team = crisis(game, &position);
        if (!team) return;
        snprintf(text, capacity, "%s", loc(game, "SSR_TICKER_CRISIS"));
        replace_all(text, capacity, "$club", club_name(game, team));
        snprintf(number, sizeof(number), "%d", position);
        replace_all(text, capacity, "$num", number);
        break;
    }
    case SSR_TICKER_WORLD_RUMOUR: {
        season_in(game);
        int big = 0;
        const SSRPerson *p = rumour(game, &big);
        if (!p) return;
        char name[64];
        snprintf(name, sizeof(name), "%s %s", p->first, p->last);
        snprintf(text, capacity, "%s", loc(game, "SSR_TICKER_RUMOUR"));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$club", club_name(game, p->club));
        replace_all(text, capacity, "$big", club_name(game, big));
        break;
    }
    case SSR_TICKER_WORLD_RECORD: {
        season_in(game);
        char name[96];
        int goals = 0, record = 0;
        const char *league = "";
        if (!record_chase(game, name, sizeof(name), &goals, &record, &league)) return;
        snprintf(text, capacity, "%s", loc(game, goals > record ? "SSR_TICKER_RECORDBROKEN" : "SSR_TICKER_RECORDCHASE"));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$league", league);
        snprintf(number, sizeof(number), "%d", goals);
        replace_all(text, capacity, "$num", number);
        snprintf(number, sizeof(number), "%d", record);
        replace_all(text, capacity, "$record", number);
        break;
    }
    case SSR_TICKER_WORLD_MANAGER: {
        SSRPeople *people = game->people;
        if (!people || people->sackedCount == 0) return;
        int club = people->sacked[game->worldTurn++ % people->sackedCount];
        const SSRManager *m = ssr_people_manager(people, club);
        if (!m) return;
        static const char *const STYLES[SSR_STYLE_COUNT] = {"SSR_StyleBalanced", "SSR_StyleYouth", "SSR_StyleAttacking"};
        char name[64];
        snprintf(name, sizeof(name), "%s %s", m->first, m->last);
        snprintf(text, capacity, "%s", loc(game, "SSR_TICKER_MANAGER"));
        replace_all(text, capacity, "$club", club_name(game, club));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$style", loc(game, STYLES[m->style % SSR_STYLE_COUNT]));
        break;
    }
    case SSR_TICKER_WORLD_TABLE: {
        season_in(game);
        const SSRTableRow *rows[3];
        SSRSeasonComp *comp = table_story(game, rows);
        if (!comp) return;
        int gap = rows[0]->points - rows[1]->points;
        snprintf(text, capacity, "%s", loc(game, gap > 0 ? "SSR_TICKER_TITLERACE" : "SSR_TICKER_TITLELEVEL"));
        replace_all(text, capacity, "$leader", club_name(game, rows[0]->teamId));
        replace_all(text, capacity, "$second", club_name(game, rows[1]->teamId));
        replace_all(text, capacity, "$bottom", club_name(game, rows[2]->teamId));
        replace_all(text, capacity, "$league", comp->data->tla);
        snprintf(number, sizeof(number), "%d", gap);
        replace_all(text, capacity, "$gap", number);
        snprintf(number, sizeof(number), "%d", rows[0]->played);
        replace_all(text, capacity, "$played", number);
        break;
    }
    case SSR_TICKER_WORLD_DERBY: {
        season_in(game);
        int home = 0, away = 0, kind = next_derby(game, &home, &away);
        if (!kind) return;
        snprintf(text, capacity, "%s", loc(game, kind == 1 ? "SSR_TICKER_DERBY" : "SSR_TICKER_RIVALRY"));
        replace_all(text, capacity, "$home", club_name(game, home));
        replace_all(text, capacity, "$away", club_name(game, away));
        snprintf(number, sizeof(number), "%d", (int)(ssr_world_club_distance(game->world, home, away) + 0.5));
        replace_all(text, capacity, "$km", number);
        break;
    }
    case SSR_TICKER_WORLD_MOVE: {
        SSRPeople *people = game->people;
        if (!people || people->moveCount == 0) return;
        const int *move = people->moves[game->worldTurn++ % people->moveCount];
        const SSRPerson *p = ssr_people_find(people, move[0]);
        if (!p) return;
        char name[64];
        person_name(p, name, sizeof(name));
        snprintf(text, capacity, "%s", loc(game, "SSR_TICKER_MOVE"));
        replace_all(text, capacity, "$name", name);
        replace_all(text, capacity, "$from", club_name(game, move[1]));
        replace_all(text, capacity, "$club", club_name(game, move[2]));
        break;
    }
    default:
        break;
    }
}

void ssr_game_set_language(SSRGame *game, int language, int metres) {
    if (!game || language < 0 || language > 12) return;
    game->settings.language = language;
    game->settings.distance = metres != 0;
    settings_changed(game);
}

void ssr_game_choose_language(SSRGame *game) {
    if (!game) return;
    // c_GameEngine.SetUpScreens: the text is English until the choice.
    ssr_tweaks_set_number(game->tweaks, "Menu", "Language", 0);
    set_screen(game, "language");
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
    if (strcmp(name, "GS_CustomAge") == 0) {
        snprintf(game->shown, sizeof(game->shown), "%d", game->customAge);
        return game->shown;
    }
    if (strcmp(name, "GS_CustomPosition") == 0) {
        static const char *const TAGS[SSR_CAREER_POSITIONS] = {"SSR_PosForward", "SSR_PosStriker", "SSR_PosHolding"};
        return loc(game, TAGS[game->customPosition % SSR_CAREER_POSITIONS]);
    }
    if (strcmp(name, "GS_CustomClub") == 0) {
        if (game->customClub == 0 || game->customClub > game->customClubCount) return loc(game, "SSR_CustomSkillTrials");
        return club_name(game, game->customClubs[game->customClub - 1]);
    }
    if (strcmp(name, "GS_CustomBux") == 0) {
        snprintf(game->shown, sizeof(game->shown), "%d", CUSTOM_BUX[game->customBux % 4]);
        return game->shown;
    }
    if (strcmp(name, "GS_CustomSkill") == 0) {
        if (game->customSkill == 0) return loc(game, "SSR_CustomSkillTrials");
        snprintf(game->shown, sizeof(game->shown), "%d", game->customSkill);
        return game->shown;
    }
    if (strcmp(name, "GS_WorldSeed") == 0) {
        if (game->typing == 3) return game->seedText;
        if (game->settings.nextWorldSeed == 0) return loc(game, "SSR_SeedRandom");
        snprintf(game->shown, sizeof(game->shown), "%d", game->settings.nextWorldSeed);
        return game->shown;
    }
    if (strcmp(name, "GS_SquadTitle") == 0) {
        if (game->typing == 4) return game->editText;
        return ssr_tweaks_get(game->tweaks, "Squad", "Title");
    }
    if (strcmp(name, "GS_ContinueWorld") == 0) return loc(game, game->settings.continueWorld ? "SSR_ContinueYes" : "SSR_ContinueNo");
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
    if (game->typing == 4) {
        // The name of a club: up to 30 bytes, with no tab.
        for (const char *p = text; *p; p++) {
            size_t length = strlen(game->editText);
            if ((unsigned char)*p >= 32 && length < 30) {
                game->editText[length] = *p;
                game->editText[length + 1] = 0;
            }
        }
        return;
    }
    if (game->typing == 3) {
        // The seed of a world: up to 9 digits.
        for (const char *p = text; *p; p++) {
            size_t length = strlen(game->seedText);
            if (*p >= '0' && *p <= '9' && length < 9) {
                game->seedText[length] = *p;
                game->seedText[length + 1] = 0;
            }
        }
        return;
    }
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
    if (game->typing == 4) {
        size_t length = strlen(game->editText);
        while (length > 0 && ((unsigned char)game->editText[length - 1] & 0xC0) == 0x80) length--;
        if (key == SSR_KEY_BACKSPACE && length > 0) game->editText[length - 1] = 0;
        if (key == SSR_KEY_ENTER) edit_done(game);
        return;
    }
    if (game->typing == 3) {
        size_t length = strlen(game->seedText);
        if (key == SSR_KEY_BACKSPACE && length > 0) game->seedText[length - 1] = 0;
        if (key == SSR_KEY_ENTER) seed_done(game);
        return;
    }
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
    } else if (strcmp(game->screen, "RacingMiniGame") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "RacingMiniGame") == 0) {
            game->ticks -= 1;
            game->now += 16;
            race_tick(game);
        }
    } else if (strcmp(game->screen, "blackjack") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            blackjack_tick(game);
        }
    } else if (strcmp(game->screen, "roulette") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            roulette_tick(game);
        }
    } else if (strcmp(game->screen, "slots") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            slots_tick(game);
        }
    } else if (strcmp(game->screen, "retirement") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1) {
            game->ticks -= 1;
            game->now += 16;
            retirement_tick(game);
        }
    } else if (strcmp(game->screen, "interview") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "interview") == 0) {
            game->ticks -= 1;
            game->now += 16;
            interview_tick(game);
        }
    } else if (strcmp(game->screen, "negotiate") == 0) {
        game->ticks += frames;
        while (game->ticks >= 1 && strcmp(game->screen, "negotiate") == 0) {
            game->ticks -= 1;
            game->now += 16;
            negotiate_tick(game);
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

const char *ssr_game_modal(const SSRGame *game) {
    return game ? game->modal : "";
}

const char *ssr_game_screen(const SSRGame *game) {
    return game ? game->screen : "";
}

int ssr_game_screen_version(const SSRGame *game) {
    return game ? game->version : 0;
}

void ssr_game_set_packs_directory(SSRGame *game, const char *directory, int canInstall) {
    if (!game) return;
    snprintf(game->packsDirectory, sizeof(game->packsDirectory), "%s", directory ? directory : "");
    game->canInstallPacks = canInstall != 0;
}

int ssr_game_take_pack_change(SSRGame *game) {
    if (!game || !game->packChanged) return 0;
    game->packChanged = 0;
    return 1;
}

int ssr_game_take_pack_install(SSRGame *game) {
    if (!game || !game->packInstallAsked) return 0;
    game->packInstallAsked = 0;
    return 1;
}

void ssr_game_show_packs(SSRGame *game) {
    if (game) packs_screen(game);
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

const char *ssr_game_alert(SSRGame *game, int *id) {
    if (id) *id = 0;
    if (!game) return NULL;
    int kept = 0;
    for (int i = 0; i < game->alertCount; i++) {
        if (game->alerts[i].finish > game->now) game->alerts[kept++] = game->alerts[i];
    }
    game->alertCount = kept;
    if (kept == 0 || game->alerts[0].start > game->now) return NULL;
    ssr_tweaks_set(game->tweaks, "Menu", "BottomQuickMessage", game->alerts[0].text);
    if (id) *id = game->alerts[0].id;
    return game->alerts[0].layout;
}

int ssr_game_splash(const SSRGame *game) {
    return game && game->now < game->splashEnd;
}

const char *ssr_game_take_sound(SSRGame *game) {
    if (!game || game->soundCount == 0) return NULL;
    const char *sound = game->sounds[0];
    memmove(game->sounds, game->sounds + 1, (size_t)(game->soundCount - 1) * sizeof(const char *));
    game->soundCount--;
    return sound;
}

const char *ssr_game_splash_layout(const SSRGame *game) {
    return game && game->splashLayout[0] ? game->splashLayout : "splashmessage";
}
