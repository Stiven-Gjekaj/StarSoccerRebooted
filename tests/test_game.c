// Tests ssr_game: the messages of the screens and the settings file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_ball.h"
#include "ssr_game.h"
#include "ssr_pack.h"
#include "ssr_shop.h"
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
    // A message with a param after a space finds its route.
    CHECK(ssr_game_message(game, "mainmenu.btn_Career 3"));
    CHECK(!ssr_game_message(game, "no.such_message 3"));
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
    ssr_game_set_pack(game, "harbour");
    CHECK(ssr_game_save_settings(game, path));
    ssr_game_free(game);

    SSRTweaks *other = ssr_tweaks_create();
    game = ssr_game_create(other);
    CHECK(ssr_game_settings(game)->volume == 1);
    CHECK(ssr_game_load_settings(game, path));
    CHECK(ssr_game_settings(game)->volume == 0 && ssr_game_settings(game)->distance == 1);
    CHECK(strcmp(ssr_game_settings(game)->pack, "harbour") == 0);
    CHECK(ssr_tweaks_number(other, "Menu", "SettingsDistance") == 1);
    ssr_game_free(game);
    ssr_tweaks_free(other);
    ssr_tweaks_free(tweaks);
}

// The options open the language screen. Ok with a new language saves it
// and goes to the main menu. Ok with the same language goes back.
static void test_chooses_a_language(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRGame *game = ssr_game_create(tweaks);
    CHECK(ssr_game_settings(game)->language == -1);
    ssr_game_set_language(game, 6, 1);
    CHECK(ssr_game_settings(game)->language == 6 && ssr_game_settings(game)->distance == 1);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "Language") == 6);
    CHECK(ssr_game_take_settings_change(game));
    CHECK(ssr_game_message(game, "mainmenu.btn_Options"));
    CHECK(ssr_game_message(game, "options.btn_Language"));
    CHECK(strcmp(ssr_game_screen(game), "language") == 0);
    CHECK(ssr_game_message(game, "language.btn_Ok"));
    CHECK(strcmp(ssr_game_screen(game), "options") == 0);
    CHECK(!ssr_game_take_settings_change(game));
    ssr_game_message(game, "options.btn_Language");
    // The rows of language.uis set the tweak.
    ssr_tweaks_set_number(tweaks, "Menu", "Language", 10);
    ssr_game_message(game, "language.btn_Ok");
    CHECK(ssr_game_settings(game)->language == 10);
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);
    CHECK(ssr_game_take_settings_change(game));

    char path[1200];
    snprintf(path, sizeof(path), "%s/settings.txt", ssr_test_directory("game_language"));
    // The classic ratings and scores stay in the settings.
    ssr_game_message(game, "packs.btn_Realism");
    CHECK(ssr_game_save_settings(game, path));
    ssr_game_free(game);
    game = ssr_game_create(tweaks);
    CHECK(ssr_game_load_settings(game, path));
    CHECK(ssr_game_settings(game)->language == 10 && ssr_game_settings(game)->classicRealism == 1);
    // With no language, the game asks for one in English.
    ssr_game_free(game);
    game = ssr_game_create(tweaks);
    ssr_game_choose_language(game);
    CHECK(strcmp(ssr_game_screen(game), "language") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "Language") == 0);
    ssr_game_free(game);
    ssr_tweaks_free(tweaks);
}

// The Tag_NationId of the language, or NULL for none.
static const char *nationTag;

static const char *localise(void *context, const char *tag) {
    (void)context;
    if (strcmp(tag, "Tag_NationId") == 0) return nationTag;
    if (strcmp(tag, "SSR_TICKER_RIVAL") == 0) return "Rival $name of $club: $goals in $apps.";
    if (strcmp(tag, "CMESSAGE_TRIALSUCCESSMOBILE") == 0) return "Signed by $clubname ($labelname) in $leaguename.";
    if (strcmp(tag, "CMESSAGE_TRIALMOBILE1") == 0) return "Trial one.";
    if (strcmp(tag, "CMESSAGE_TRIALMOBILEFAIL4_DRIBBLING") == 0) return "Failed. Now dribble.";
    if (strcmp(tag, "CNEWS_FIRSTCONTRACTMOBILE") == 0) return "$clubname have signed $playername.";
    if (strcmp(tag, "help_newplayer") == 0) return "Welcome.";
    if (strcmp(tag, "CMESSAGE_DELETESLOT") == 0) return "Delete?";
    if (strcmp(tag, "sla_versus_RESKIN") == 0) return "vs.";
    if (strcmp(tag, "CNEWS_RETIREMENT") == 0) return "$years $num $age";
    return NULL;
}

static int take_ops(SSRGame *game, SSRUiOp *ops, int max) {
    int count = 0;
    while (count < max && ssr_game_take_op(game, &ops[count])) count++;
    return count;
}

static int has_op(const SSRUiOp *ops, int count, SSRUiOpKind kind, const char *ref, const char *text) {
    for (int i = 0; i < count; i++) {
        if (ops[i].kind == kind && strcmp(ops[i].ref, ref) == 0 && (!text || strcmp(ops[i].text, text) == 0)) return 1;
    }
    return 0;
}

// A new career from the main menu to the game menu, with a world that the
// test makes: one continent, two nations, a league in England.
// Plays a match from the game menu: the commentary, a chance of the
// player, and the end.
// Fails the five trials of a new career: each shot goes to the left, and
// the dribble goes off the screen. No skill gets a level.
static void fail_the_trials(SSRGame *game, SSRTweaks *tweaks) {
    for (int trial = 1; trial <= 5; trial++) {
        CHECK(strcmp(ssr_game_screen(game), "messagedefault") == 0);
        if (trial == 1) CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "Trial one.") == 0);
        if (trial == 4) CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "Failed. Now dribble.") == 0);
        ssr_game_message(game, "message.btn_Ok");
        CHECK(strcmp(ssr_game_screen(game), "matchminigame") == 0);
        const SSRTraining *t = ssr_game_training(game);
        CHECK(t && t->trial == trial && t->lives == 3 && ssr_game_chance(game) == &t->chance);
        char step[8];
        snprintf(step, sizeof(step), "%d", trial);
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MGTrialStep"), step) == 0);
        int lost = 0, splash = 0;
        for (int i = 0; i < 200000 && ssr_game_training(game); i++) {
            t = ssr_game_training(game);
            const SSRChance *c = &t->chance;
            if (t->lives < 3) lost = 1;
            if (ssr_game_splash(game)) splash = 1;
            if (t->instruction) {
                ssr_game_touch(game, 1, 320, 900);
                ssr_game_update(game, 1);
                ssr_game_touch(game, 0, 320, 900);
            } else if (c->state == SSR_CHANCE_AIM && c->kicking == 0) {
                ssr_game_touch(game, 1, c->ball.x, c->ball.y);
                ssr_game_update(game, 1);
                ssr_game_touch(game, 1, c->ball.x + 60, c->ball.y);
                ssr_game_update(game, 10);
                ssr_game_touch(game, 0, c->ball.x + 60, c->ball.y);
            } else if (c->state == SSR_CHANCE_TAP && !c->tap.touching && c->tap.tapX == 0) {
                ssr_game_touch(game, 1, c->tap.x, c->tap.y);
                ssr_game_update(game, 1);
                ssr_game_touch(game, 0, c->tap.x, c->tap.y);
            } else if (c->state == SSR_CHANCE_DRIBBLE && fabsf(c->dribbleX) < 0.1f) {
                ssr_game_touch(game, 1, 100, 900);
                ssr_game_update(game, 1);
                ssr_game_touch(game, 0, 600, 900);
            }
            ssr_game_update(game, 1);
        }
        CHECK(!ssr_game_training(game) && lost);
        // The drills of the gate, the dribble, and the dummy show "Fail!".
        if (trial == 1 || trial == 4 || trial == 5) {
            CHECK(splash && strcmp(ssr_tweaks_get(tweaks, "Menu", "QuickMessage"), "FAIL!") == 0);
        }
        CHECK(ssr_game_career(game)->technique[0] == 0 && ssr_game_career(game)->pace[0] == 0);
        // The end of the fifth trial gives the award of the trials.
        CHECK((ssr_game_career(game)->awards[1] > 0) == (trial == 5));
    }
}

// The replays that the matches of the test watched.
static int replays;

// The offer of a goal replay: Watch, then the replay plays the recorded
// frames in a loop until Exit.
static void watch_a_replay(SSRGame *game, SSRTweaks *tweaks) {
    const SSRChance *live = ssr_game_chance(game);
    int result = live->result;
    CHECK(result == SSR_RESULT_GOAL || result == SSR_RESULT_ASSIST);
    ssr_game_message(game, "matchchance.btn_WatchReplay");
    CHECK(ssr_game_replaying(game) == 2 && ssr_tweaks_number(tweaks, "WatchReplay", "Active") == 1);
    int length = (int)ssr_tweaks_number(tweaks, "WatchReplay", "Length");
    CHECK(length > 10);
    // The first replay waits at frame 0 for the help.
    if (ssr_tweaks_number(tweaks, "Player", "ReplayActive") < 3) {
        ssr_game_update(game, 5);
        CHECK(ssr_tweaks_number(tweaks, "WatchReplay", "Frame") == 0);
        ssr_tweaks_set_number(tweaks, "Player", "ReplayActive", 3);
        ssr_tweaks_set_number(tweaks, "WatchReplay", "Speed", 1);
    }
    const SSRChance *first = ssr_game_chance(game);
    CHECK(first != live && first->state != SSR_CHANCE_OVER);
    ssr_game_update(game, 5);
    CHECK(ssr_tweaks_number(tweaks, "WatchReplay", "Frame") == 5);
    // The last frame is the end of the chance. Then the replay starts again.
    ssr_tweaks_set_number(tweaks, "WatchReplay", "Frame", length);
    CHECK(ssr_game_chance(game)->state == SSR_CHANCE_OVER);
    ssr_game_update(game, 1);
    CHECK(ssr_tweaks_number(tweaks, "WatchReplay", "Frame") == 0);
    ssr_game_message(game, "watchreplay.exit");
    CHECK(ssr_game_replaying(game) == 0 && strcmp(ssr_game_screen(game), "match") == 0);
    replays++;
}

// The end of a season: the review, the table, the awards, and the next
// season.
static int seasons = 0, notPicked = 0, substitutes = 0, records = 0, squadSeen = 0, moments = 0, painAsked = 0;

static int transfers = 0;

// 1 when the game menu shows, after the test closes the messages that can
// come over it: the trainer, the agent, and the offers of the shop. A
// question gets No.
static int at_game_menu(SSRGame *game, SSRTweaks *tweaks) {
    for (int i = 0; i < 6 && strncmp(ssr_game_screen(game), "message", 7) == 0; i++) {
        int question = ssr_tweaks_number(tweaks, "Message", "ShowDec") == 1;
        ssr_game_message(game, question ? "message.btn_Cancel" : "message.btn_Ok");
        ssr_game_update(game, 130);
    }
    return strcmp(ssr_game_screen(game), "gamemenu") == 0;
}

// Yes to a transfer at the end of the season: a nation, a division, a
// club, and the contract. A season with no club that wants the player
// goes on with the club.
static int exBossOffers = 0;

static void transfer_at_season_end(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    // The old boss of the player, sacked by the club, wants the player at
    // the new club. The test says no, and the offer does not come again.
    if (strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_ExBossOffer") == 0) {
        exBossOffers++;
        CHECK(c->exBossClub != 0 && c->exBossClub != c->clubId);
        ssr_game_message(game, "message.btn_Cancel");
        CHECK(c->exBossClub == 0 && strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_SEASONENDTRANSFER") == 0);
    }
    // At the second end the star rating is 400: the limit is about 65, so
    // the club of strength 50 wants the player.
    if (seasons == 1) ((SSRCareer *)c)->starRating = 400;
    ssr_game_message(game, "message.btn_Ok");
    // A player of a low star rating: no club wants the player.
    if (strcmp(ssr_game_screen(game), "message") == 0) {
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_NOCLUBSINTERESTEDATALL") == 0);
        ssr_game_message(game, "message.btn_Ok");
        return;
    }
    CHECK(strcmp(ssr_game_screen(game), "desiredtransfer") == 0);
    SSRUiOp ops[256];
    int count = take_ops(game, ops, 256), division = 0;
    CHECK(has_op(ops, count, SSR_OP_LIST_ADD, "LeagueCombo", NULL));
    for (int i = 0; i < count; i++) {
        if (ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "DivisionCombo") == 0) division = ops[i].id;
    }
    int club = 0;
    if (division) {
        ssr_tweaks_set_number(tweaks, "Combo", "ChosenDesiredDivision", division);
        ssr_game_message(game, "desiredtransfer.cmb_Division");
        count = take_ops(game, ops, 256);
        for (int i = 0; i < count; i++) {
            if (ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "ClubCombo") == 0) club = ops[i].id;
        }
    }
    if (!club) {
        // Back asks to stay; Yes starts the new season.
        ssr_game_message(game, "desiredtransfer.btn_Back");
        CHECK(strcmp(ssr_game_screen(game), "message") == 0);
        ssr_game_message(game, "message.btn_Ok");
        return;
    }
    CHECK(club != c->clubId);
    ssr_tweaks_set_number(tweaks, "Combo", "ChosenDesiredClub", club);
    ssr_game_message(game, "desiredtransfer.cmb_Club");
    ssr_game_message(game, "desiredtransfer.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "contract") == 0 && ssr_tweaks_number(tweaks, "Negotiation", "IsNewClub") == 1);
    int wage = (int)ssr_tweaks_number(tweaks, "Negotiation", "Wage");
    ssr_game_message(game, "contract.btn_Accept");
    // An agent gets a fee: its message comes first.
    if (strcmp(ssr_game_screen(game), "message") == 0) ssr_game_message(game, "message.btn_Ok");
    CHECK(c->clubId == club && c->wage == wage);
    CHECK(c->awards[34] > 0);
    transfers++;
}

static int retireNow = 0, starManSum = 0;
static const char *careerPath = NULL;

// The retirement: No comes back to the awards. Yes shows the news, then
// the stats one at a time and the score.
static void retire(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    const SSRCareer *c = ssr_game_career(game);
    SSRUiOp ops[64];
    ssr_game_message(game, "playeroftheyear.btn_Retire");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_RETIREMENTCHECK CMESSAGE_LOSESTARBUX") == 0);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_screen(game), "playeroftheyear") == 0 && !c->retired);
    int count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_SHOW, "PlayButton", NULL) && has_op(ops, count, SSR_OP_SHOW, "RetireButton", NULL));
    CHECK(has_op(ops, count, SSR_OP_SHOW, ssr_career_age(c) <= 23 ? "TeamPlayerOfYear" : "TeamPlayerOfYearOld", NULL));
    int sdate = c->sdate;
    ssr_game_message(game, "playeroftheyear.btn_Retire");
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "webpagenews") == 0);
    count = take_ops(game, ops, 64);
    char news[64];
    snprintf(news, sizeof(news), "%d %d %d", ssr_career_year(c) - 1, c->goals + c->intGoals, ssr_career_age(c) - 1);
    CHECK(count == 1 && strcmp(ops[0].text, news) == 0);
    ssr_game_message(game, "webpage.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "retirement") == 0);
    count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_HIDE, "CareerPlayerofYear", NULL));
    CHECK(has_op(ops, count, SSR_OP_HIDE, "StarRating", NULL));
    CHECK(strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerScore"), "0") == 0);
    // Ten steps of one second: nine stats, a wait, and the score.
    int shown = 0, frames = 0;
    for (; frames < 12 * 60 && strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerScore"), "0") == 0; frames++) {
        ssr_game_update(game, 1);
        count = take_ops(game, ops, 64);
        for (int i = 0; i < count; i++) shown += ops[i].kind == SSR_OP_SHOW;
    }
    CHECK(shown == 9 && frames >= 10 * 60 && frames <= 11 * 60);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "CareerMatches") == c->apps + c->intApps);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "CareerStarMan") == c->starMan + c->intStarMan);
    CHECK(fabs(ssr_tweaks_number(tweaks, "PlayerStats", "CareerAverageRating") - c->totalRating / c->apps) < 1e-4);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "StarRating") == c->starRating / 100 + 1);
    // The score: the history of the seasons, the average, the medals, the
    // star level, and the bars. Each trophy of this career is a league
    // place (the test localise gives tags, so no place is "position_1").
    float skills = (ssr_career_skill(c->pace) + ssr_career_skill(c->power) + ssr_career_skill(c->technique) +
                    ssr_career_skill(c->vision) + ssr_career_skill(c->setPieces)) / 3.0f;
    float h = 0, hg = 0, ha = 0, hs = 0;
    for (int i = 0; i < c->historyCount; i++) {
        h += c->history[i].apps;
        hg += c->history[i].goals;
        ha += c->history[i].assists;
        hs += c->history[i].starMan;
    }
    CHECK(c->historyCount > 0 && h == c->apps);
    int medals = 0;
    for (int i = 0; i < c->trophyCount; i++) medals += c->trophies[i].medal > 0;
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "CareerMedals") == medals);
    int score = (int)(hg / h * 100.0f + 1.0f);
    score = (int)(score + ha / h * 50.0f + 1.0f);
    score = (int)(score + hs / h * 200.0f + 1.0f);
    score = (int)(score * (c->totalRating / c->apps + 1.0f) * 200.0f);
    score = (int)(score + medals * 250.0f);
    score += c->potyAwards * 250 + ssr_career_award_count(c) * 200 + (c->starRating / 100 + 1) * 200;
    float bars = skills / 100.0f + 1.0f + ssr_career_happiness(c) / 100.0f + ssr_career_lifestyle(c) / 100.0f;
    score = (int)(score * bars) * 10;
    char text[32];
    snprintf(text, sizeof(text), "%d,%03d", score / 1000, score % 1000);
    CHECK(score >= 1000 && score < 1000000 && strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerScore"), text) == 0);
    // Play keeps the score, and the career goes one day back.
    ssr_game_message(game, "retirement.btn_Play");
    CHECK(c->retired == score && c->sdate == sdate - 1);
    // The timeline ends with the testimonial match.
    CHECK(c->momentCount > 0 && strcmp(c->moments[c->momentCount - 1].text, "SSR_MomentTestimonial") == 0);
    CHECK(at_game_menu(game, tweaks));
    CHECK(ssr_tweaks_number(tweaks, "Menu", "DisableQuitButton") == 0);
    SSRCareer saved;
    CHECK(ssr_career_load(&saved, path) && saved.retired == score);
    // Play opens the retirement screen again; it saves nothing.
    ssr_game_message(game, "gamemenu.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "retirement") == 0);
    ssr_game_message(game, "retirement.btn_Play");
    CHECK(c->retired == score && c->sdate == sdate - 1);
    // The first retired career unlocks the hard mode, once.
    CHECK(!ssr_game_settings(game)->hardModeUnlocked);
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && ssr_game_settings(game)->hardModeUnlocked);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_HARDMODEUNLOCKED") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);
    ssr_game_message(game, "mainmenu.btn_Career");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "SaveSlot", "Slot0ButtonText"), "Retired") == 0);
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0 && c->retired == score);
}

// 1 after the test gave the first season great counts.
static int greatSeason = 0;

static void finish_season(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    int year = ssr_career_year(c);
    starManSum += c->seasonStarMan;
    CHECK(c->starMan == starManSum);
    // Star Man gives an award, and 10 of them another.
    CHECK((c->awards[8] > 0) == (c->starMan > 0) && (c->awards[9] > 0) == (c->starMan >= 10));
    CHECK(ssr_career_week(c) == 1 && ssr_tweaks_number(tweaks, "Menu", "DisableQuitButton") == 1);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "SeasonLeague"), "PRM") == 0);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "SeasonMatches") == c->seasonApps && c->seasonApps > 0);
    double position = ssr_tweaks_number(tweaks, "PlayerStats", "SeasonPosition");
    CHECK(position >= 1 && position <= 4);
    CHECK(ssr_game_splash(game));
    // The table opens from the review, and its Back comes back.
    ssr_game_message(game, "seasonreview.btn_League");
    CHECK(strcmp(ssr_game_screen(game), "league") == 0 && ssr_tweaks_number(tweaks, "Menu", "ShowLeaguePlay") == 1);
    ssr_game_message(game, "leaguefixtures.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "seasonreview") == 0);
    // The place goes into the trophy log, and the season into the stat
    // history, once, also when the review opens again.
    int rows = 0;
    for (int i = 0; i < c->historyCount; i++) rows += c->history[i].year == year - 1;
    int historyApps;
    ssr_career_history_totals(c, &historyApps, NULL, NULL, NULL);
    CHECK(rows == 1 && historyApps == c->apps);
    // The mean rating of the season goes into the form of the career.
    CHECK(c->careerForm[4] == (int)(c->seasonTotalRating / (float)c->seasonApps));
    const SSRTrophy *place = c->trophyCount ? &c->trophies[c->trophyCount - 1] : NULL;
    CHECK(place && place->year == year - 1 && strncmp(place->tournament, "PRM ", 4) == 0);
    CHECK(place && place->medal == (position == 1 ? 1 : 0));
    // The awards of the season: a title, the counts of the season, and
    // the year 10.
    if (position == 1) CHECK(c->awards[16] > 0);
    if (seasons == 0) CHECK(c->awards[19] > 0 && c->awards[122] > 0 && c->awards[20] > 0 && c->awards[21] > 0);
    if (seasons == 0) CHECK(c->awards[22] == 0 && c->awards[17] == 0);
    if (year - 1 == 10) CHECK(c->awards[50] > 0);
    // In the second season the player is the best player of the club.
    SSRCareer *best = (SSRCareer *)c;
    int fans = best->relationFans;
    if (seasons == 1) {
        best->seasonApps = 30;
        best->seasonStarMan = 20;
        best->seasonTotalRating = 30 * 8.0f;
        best->relationFans = 90;
    }
    // The awards come one at a time; the buttons show at the end.
    ssr_game_message(game, "seasonreview.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "playeroftheyear") == 0);
    SSRUiOp ops[64];
    int count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_HIDE, "PlayButton", NULL));
    int shown = 0;
    for (int i = 0; i < 40 * 60 && !shown; i++) {
        ssr_game_update(game, 1);
        count = take_ops(game, ops, 64);
        shown = has_op(ops, count, SSR_OP_SHOW, "PlayButton", NULL);
    }
    CHECK(shown);
    // A player with few matches wins no award. The best player of the club
    // wins the award of the club.
    CHECK((ssr_tweaks_number(tweaks, "SeasonAwards", "TeamPlayerOfYear") != 0) == (seasons == 1));
    if (seasons == 1) CHECK(c->awards[69] > 0 && c->awards[72] == 0 && c->awards[114] == 0);
    best->relationFans = fans;
    if (retireNow) {
        retire(game, tweaks, careerPath);
        retireNow = 2;
        return;
    }
    ssr_game_message(game, "playeroftheyear.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0);
    if (seasons < 2) {
        transfer_at_season_end(game, tweaks);
    } else {
        ssr_game_message(game, "message.btn_Cancel");
    }
    CHECK(strcmp(ssr_game_screen(game), "startransition") == 0);
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    // The new season starts the counts again, the day before its first
    // match.
    CHECK(ssr_career_year(c) == year && c->seasonApps == 0 && c->energy == 100);
    CHECK(ssr_career_week(c) == 6 && ssr_tweaks_number(tweaks, "Menu", "DisableQuitButton") == 0);
    seasons++;
}

// 1 after the test checked the ball of a chance.
static int ballChecked = 0;
static int winnerPages = 0, newsSeen = 0, dilemmasSeen = 0, interviews = 0, interviewsWon = 0, trainerMessages = 0;

// The interview after a match: the green buttons light one at a time, then
// the player repeats them. Every second interview the test makes a fault.
static void give_an_interview(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    int skill = c->interviewSkill, stars = c->starRating;
    int order[16], count = 0, go = 0;
    SSRUiOp ops[64];
    for (int f = 0; f < 2000 && !go; f++) {
        ssr_game_update(game, 1);
        int n = take_ops(game, ops, 64);
        for (int i = 0; i < n; i++) {
            int b;
            if (ops[i].kind == SSR_OP_SUB && ops[i].id == 1 && strcmp(ops[i].ref2, "CorrectOutline") == 0 &&
                sscanf(ops[i].ref, "Button%d", &b) == 1 && count < 16) {
                order[count++] = b;
            }
            if (ops[i].kind == SSR_OP_TEXT && strcmp(ops[i].ref, "Sentence") == 0 && strcmp(ops[i].text, "Go!") == 0) go = 1;
        }
    }
    CHECK(go && count >= 3 && count <= 5);
    int fault = interviews++ % 2 == 1;
    for (int i = 0; i < count; i++) {
        char message[32];
        int b = order[i];
        if (fault && i == count - 1) b = b % 9 + 1;
        snprintf(message, sizeof(message), "interview.btn_%d", b);
        ssr_game_message(game, message);
    }
    for (int f = 0; f < 400 && strcmp(ssr_game_screen(game), "interview") == 0; f++) ssr_game_update(game, 1);
    CHECK(strcmp(ssr_game_screen(game), "matchstats") == 0);
    // The press writes of the answers: the headline is an alert.
    const char *want = fault ? "SSR_PRESSBAD" : "SSR_PRESSGOOD";
    int pressSeen = 0;
    for (int k = 0; k < 16 && !pressSeen; k++) {
        int id = 0;
        const char *layout = ssr_game_alert(game, &id);
        if (!layout) break;
        pressSeen = strcmp(ssr_tweaks_get(tweaks, "Menu", "BottomQuickMessage"), want) == 0;
        if (!pressSeen) ssr_game_message(game, "message.dismiss");
    }
    CHECK(pressSeen);
    if (!fault) {
        interviewsWon++;
        CHECK(c->interviewSkill == skill + 1 && c->starRating == stars + 10);
    } else {
        CHECK(c->interviewSkill == skill && ssr_tweaks_number(tweaks, "Menu", "ZeroRelChange") == 1);
    }
}

// Play on the game menu: the news of the week comes first. The test
// answers it until the fixtures show.
static void press_play(SSRGame *game) {
    // The message of the trainer can show on the game menu after a match.
    for (int i = 0; i < 4 && strcmp(ssr_game_screen(game), "messagetrainer") == 0; i++) {
        trainerMessages++;
        ssr_game_message(game, "message.btn_Ok");
    }
    ssr_game_message(game, "gamemenu.btn_Play");
    for (int i = 0; i < 20 && strcmp(ssr_game_screen(game), "leaguefixtures") != 0; i++) {
        const char *screen = ssr_game_screen(game);
        newsSeen++;
        if (strncmp(screen, "message", 7) == 0) {
            ssr_game_message(game, "message.btn_Ok");
            // A decision runs its function 2000 ms after the choice.
            ssr_game_update(game, 130);
        } else if (strncmp(screen, "webpage", 7) == 0) {
            // The news of a cup winner comes first on the next page.
            SSRUiOp ops[16];
            int count = take_ops(game, ops, 16);
            winnerPages += has_op(ops, count, SSR_OP_TEXT, "MessageText", "PRM have won the CUP!");
            ssr_game_message(game, "webpage.btn_Ok");
        } else if (strcmp(screen, "dilemma") == 0) {
            dilemmasSeen++;
            ssr_game_update(game, 130);
            ssr_game_message(game, "dilemma.btn_relationship2");
            ssr_game_update(game, 160);
        } else if (strcmp(screen, "relationships") == 0) {
            ssr_game_message(game, "relationships.btn_Home");
        } else if (strcmp(screen, "casino") == 0) {
            // The team mates ask the player to the casino: no bet.
            ssr_game_message(game, "casino.btn_Home");
        } else if (strcmp(screen, "contract") == 0) {
            // An offer in the transfer window: the player stays.
            ssr_game_message(game, "contract.btn_Reject");
        } else if (strcmp(screen, "gamemenu") == 0) {
            ssr_game_message(game, "gamemenu.btn_Play");
        } else {
            break;
        }
    }
}

// Play on the results: the news of a cup winner shows first, once.
static void results_play(SSRGame *game, SSRTweaks *tweaks) {
    ssr_game_message(game, "leaguefixtures.btn_Play");
    if (strcmp(ssr_game_screen(game), "webpagenews") != 0) return;
    SSRUiOp ops[16];
    int count = take_ops(game, ops, 16);
    CHECK(has_op(ops, count, SSR_OP_TEXT, "MessageText", "PRM have won the CUP!"));
    CHECK(ssr_game_career(game)->headlineResult[0] == 0);
    winnerPages++;
    ssr_game_message(game, "webpage.btn_Ok");
    (void)tweaks;
}

static void play_a_match(SSRGame *game, SSRTweaks *tweaks) {
    SSRUiOp ops[256];
    int apps = ssr_game_career(game)->apps, bank = ssr_game_career(game)->bank;
    int goals = ssr_game_career(game)->goals, assists = ssr_game_career(game)->assists;
    int passes = ssr_game_career(game)->passes;
    float furthest[4];
    memcpy(furthest, ssr_game_career(game)->furthest, sizeof(furthest));
    float ratings[10];
    memcpy(ratings, ssr_game_career(game)->ratings, sizeof(ratings));
    // The first season has the counts of a great season, for the awards
    // of the season review.
    if (seasons == 0 && !greatSeason) {
        SSRCareer *great = (SSRCareer *)ssr_game_career(game);
        great->seasonGoals += 50;
        great->seasonPasses += 200;
        great->seasonAssists += 20;
        greatSeason = 1;
    }
    // Play shows the fixtures of the round of the player first.
    press_play(game);
    CHECK(strcmp(ssr_game_screen(game), "leaguefixtures") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "LeagueFixturesResults") == 0);
    int rows = take_ops(game, ops, 256);
    CHECK(has_op(ops, rows, SSR_OP_LIST_ADD, "FixtureList", ""));
    CHECK(has_op(ops, rows, SSR_OP_LIST_TEXT, "FixtureList", "vs."));
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Match", "LeagueName"), "PRM") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "LeagueType") == 1);
    ssr_game_message(game, "leaguefixtures.btn_Play");
    // An injured player rests in place of a match with pain.
    if (strcmp(ssr_game_screen(game), "message") == 0 &&
        strstr(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_PlayWithPain")) {
        painAsked++;
        ssr_game_message(game, "message.btn_Cancel");
    }
    // The boss can make the player a substitute, or leave the player out.
    if (strcmp(ssr_game_screen(game), "messageboss") == 0) {
        CHECK(strlen(ssr_tweaks_get(tweaks, "Menu", "MessageBody")) > 0);
        ssr_game_message(game, "message.btn_Ok");
        if (strcmp(ssr_game_screen(game), "leaguefixtures") == 0) {
            // Not picked: the results of the day show, with no match.
            CHECK(ssr_tweaks_number(tweaks, "Menu", "LeagueFixturesResults") == 1);
            CHECK(ssr_game_career(game)->apps == apps);
            notPicked++;
            results_play(game, tweaks);
            if (strcmp(ssr_game_screen(game), "league") == 0) ssr_game_message(game, "league.btn_Continue");
            if (strcmp(ssr_game_screen(game), "seasonreview") == 0) finish_season(game, tweaks);
            CHECK(at_game_menu(game, tweaks));
            return;
        }
        // A substitute comes on in the second half.
        substitutes++;
        const SSRMatch *bench = ssr_game_match(game);
        CHECK(bench && bench->setup.subTime >= 46 && bench->setup.subTime <= 65);
    }
    CHECK(strcmp(ssr_game_screen(game), "match") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Match", "MatchState") == 1);
    CHECK(ssr_tweaks_number(tweaks, "Match", "MatchCompetition") == 1);
    CHECK(strlen(ssr_tweaks_get(tweaks, "MatchColours", "Badge1Col1")) == 8);
    // The kit button goes round four kit pairs, and back to the first.
    char homeKit[16], awayKit[16];
    snprintf(homeKit, sizeof(homeKit), "%s", ssr_tweaks_get(tweaks, "TeamStrip", "HomeKitColour"));
    snprintf(awayKit, sizeof(awayKit), "%s", ssr_tweaks_get(tweaks, "TeamStrip", "AwayKitColour"));
    CHECK(strlen(homeKit) == 8 && strlen(awayKit) == 8);
    char plate[16];
    snprintf(plate, sizeof(plate), "%s", ssr_tweaks_get(tweaks, "MatchColours", "NameplateAwayCol1"));
    for (int i = 0; i < 4; i++) ssr_game_message(game, "leaguefixtures.btn_Kits");
    CHECK(strcmp(plate, ssr_tweaks_get(tweaks, "MatchColours", "NameplateAwayCol1")) == 0);
    CHECK(strcmp(homeKit, ssr_tweaks_get(tweaks, "TeamStrip", "HomeKitColour")) == 0);
    CHECK(strcmp(awayKit, ssr_tweaks_get(tweaks, "TeamStrip", "AwayKitColour")) == 0);
    CHECK(ssr_tweaks_kind(tweaks, "MatchColours", "MyTeamShirt1") == SSR_TWEAK_COLOUR);
    CHECK(strlen(ssr_tweaks_get(tweaks, "MatchColours", "OppTeamPants")) == 8);
    CHECK(strlen(ssr_tweaks_get(tweaks, "MatchColours", "NameplateHomeCol1")) == 8);
    const char *home = ssr_tweaks_get(tweaks, "Match", "HomeName");
    CHECK(strlen(home) == 3 && strcmp(home, ssr_tweaks_get(tweaks, "Match", "AwayName")) != 0);
    if (!ballChecked) {
        // The ball button opens the balls, with a tick on the ball of the
        // league. A tap on a ball picks it for the league, and closes.
        int before = ssr_game_career(game)->ballLeague;
        ssr_game_message(game, "btn_MatchBall");
        CHECK(strcmp(ssr_game_overlay(game), "matchballs") == 0);
        char owned[32];
        snprintf(owned, sizeof(owned), "MB_%d_Owned", before);
        CHECK(ssr_tweaks_number(tweaks, "Products", owned) == 1);
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Products", "MB_3_Speed"), "3") == 0);
        CHECK(ssr_tweaks_number(tweaks, "Menu", "BallSpecialEffect") == 0);
        ssr_game_message(game, "matchballs.btn_Back");
        CHECK(ssr_game_overlay(game)[0] == 0 && ssr_game_career(game)->ballLeague == before);
        ssr_game_message(game, "btn_MatchBall");
        ssr_tweaks_set(tweaks, "Menu", "ItemString", before == 3 ? "MB_5" : "MB_3");
        ssr_game_message(game, "chooseball.btn_OK");
        int picked = before == 3 ? 5 : 3;
        CHECK(ssr_game_overlay(game)[0] == 0);
        CHECK(ssr_game_career(game)->ballLeague == picked);
        CHECK(ssr_tweaks_number(tweaks, "Match", "MatchBall") == picked - 1);
    }
    ssr_game_message(game, "match.btn_Time");
    CHECK(ssr_tweaks_number(tweaks, "Match", "MatchSpeed") == 350);
    ssr_game_message(game, "match.btn_Play");
    CHECK(ssr_tweaks_number(tweaks, "Match", "MatchState") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Match", "MaximiseScreen") == 1);
    int kick = take_ops(game, ops, 64);
    CHECK(has_op(ops, kick, SSR_OP_ANIM, "NameBar", "1"));
    ssr_game_update(game, 1);
    int count = take_ops(game, ops, 64);
    CHECK(count >= 1 && has_op(ops, count, SSR_OP_LIST_ADD, "CommentaryTable", "Kick Off"));
    // The kick-off has no minute: its stopwatch hides.
    CHECK(has_op(ops, count, SSR_OP_LIST_SUB, "CommentaryTable", ""));
    int chances = 0, frames = 0;
    while (strcmp(ssr_game_screen(game), "matchstats") != 0 && frames < 200000) {
        frames++;
        ssr_game_update(game, 1);
        take_ops(game, ops, 64);
        if (ssr_game_replaying(game) == 1) watch_a_replay(game, tweaks);
        const SSRChance *chance = ssr_game_chance(game);
        if (chance && !ballChecked) {
            // The ball of the league gives its factors to the chance.
            int ball = ((SSRCareer *)ssr_game_career(game))->ballLeague;
            float speed, bounce, weight, curl;
            ssr_ball_type_factors(ball, &speed, &bounce, &weight, &curl);
            CHECK(chance->ball.speedFactor == speed && chance->ball.weightFactor == weight);
            CHECK(chance->ball.curlFactor == curl && chance->tap.bounce == bounce);
            CHECK(ssr_tweaks_number(tweaks, "Match", "MatchBall") == ball - 1);
            ballChecked = 1;
        }
        if (chance && chance->state == SSR_CHANCE_DRIBBLE) {
            // A dribble: swipe up the screen until the chance ends.
            chances++;
            CHECK(strcmp(ssr_game_screen(game), "matchminigame") == 0);
            for (int i = 0; i < 200 && chance->state == SSR_CHANCE_DRIBBLE; i++) {
                ssr_game_touch(game, 1, 320, 900);
                ssr_game_update(game, 1);
                ssr_game_touch(game, 0, 320, 720);
                for (int k = 0; k < 10; k++) ssr_game_update(game, 1);
            }
            CHECK(chance->state == SSR_CHANCE_OVER);
        }
        if (chance && chance->state == SSR_CHANCE_AIM && chance->kicking == 0) {
            chances++;
            CHECK(strcmp(ssr_game_screen(game), "matchminigame") == 0);
            // Aim at the middle of the goal, then tap the big ball at its
            // centre.
            float dx = chance->ball.x - 320, dy = chance->ball.y - 128;
            float d = sqrtf(dx * dx + dy * dy);
            float aimX = chance->ball.x + dx / d * 60, aimY = chance->ball.y + dy / d * 60;
            ssr_game_touch(game, 1, chance->ball.x, chance->ball.y);
            ssr_game_update(game, 1);
            ssr_game_touch(game, 1, aimX, aimY);
            ssr_game_update(game, 10);
            ssr_game_touch(game, 0, aimX, aimY);
            ssr_game_update(game, 1);
            CHECK(chance->state == SSR_CHANCE_TAP);
            // Wait for the tap ball to come near, then tap it.
            for (int i = 0; i < 600 && chance->state == SSR_CHANCE_TAP; i++) {
                ssr_game_touch(game, 0, 0, 0);
                ssr_game_update(game, 1);
                if (chance->tap.x > 100 && chance->tap.x < 540) {
                    ssr_game_touch(game, 1, chance->tap.x, chance->tap.y);
                    ssr_game_update(game, 1);
                }
            }
            ssr_game_touch(game, 0, 0, 0);
            // The chance ends with a banner of its result.
            for (int i = 0; i < 2000 && ssr_game_chance(game) && chance->state != SSR_CHANCE_OVER; i++) {
                ssr_game_update(game, 1);
            }
            ssr_game_update(game, 1);
            CHECK(ssr_game_banner(game));
            int outcome = (int)ssr_tweaks_number(tweaks, "Minigames", "Outcome");
            CHECK(outcome >= -3 && outcome <= 2);
        }
        if (ssr_tweaks_number(tweaks, "Match", "MatchState") == 2 || ssr_tweaks_number(tweaks, "Match", "MatchState") == 3) {
            ssr_game_message(game, "match.btn_Play");
        }
    }
    CHECK(strcmp(ssr_game_screen(game), "matchstats") == 0 && !ssr_game_match(game));
    // The first match of the career is the debut: an alert of a moment.
    if (apps == 0) {
        int id = 0;
        const char *layout = ssr_game_alert(game, &id);
        for (int k = 0; k < 64 && layout && strcmp(layout, "bottommessagemoment") != 0; k++) {
            ssr_game_message(game, "message.dismiss");
            layout = ssr_game_alert(game, &id);
        }
        CHECK(layout && strcmp(layout, "bottommessagemoment") == 0);
        moments++;
    }
    // The commentary scrolled to its last rows.
    CHECK(ssr_tweaks_number(tweaks, "Menu", "CommentaryWindowScroll") < -400);
    // The number of chances is random. A substitute can have only one.
    CHECK(chances >= 1 && ssr_game_career(game)->apps == apps + 1);
    // Each chance changes the rating of a skill. Later a rating can stay at
    // its end, 1 or 10, so the test asks only the first match.
    static int matches = 0;
    if (matches++ == 0) CHECK(memcmp(ratings, ssr_game_career(game)->ratings, sizeof(ratings)) != 0);

    // The match stats: the result overlay shows for 270 frames, then the
    // stats come one at a time.
    const SSRCareer *c = ssr_game_career(game);
    int outcome = (int)ssr_tweaks_number(tweaks, "Match", "Outcome");
    CHECK(outcome >= 1 && outcome <= 3);
    CHECK(ssr_tweaks_number(tweaks, "MatchStats", "Chances") == -1);
    float boss = c->relationBoss;
    ssr_game_update(game, 269);
    CHECK(ssr_tweaks_number(tweaks, "Match", "Outcome") == outcome);
    ssr_game_update(game, 2);
    CHECK(ssr_tweaks_number(tweaks, "Match", "Outcome") == 0);
    ssr_game_update(game, 1);
    CHECK(ssr_tweaks_number(tweaks, "MatchStats", "StatStage") == 1);
    // One chance can hold two mini-games: a pass, then a shot.
    CHECK(ssr_tweaks_number(tweaks, "MatchStats", "Chances") >= 1);
    CHECK(ssr_tweaks_number(tweaks, "MatchStats", "Goals") == -1);
    // 750 ms is 47 ticks of 16 ms.
    ssr_game_update(game, 48);
    CHECK(ssr_tweaks_number(tweaks, "MatchStats", "StatStage") == 2);
    int interviewed = 0;
    for (int i = 0; i < 2000 && ssr_tweaks_number(tweaks, "MatchStats", "StatStage") < 10; i++) {
        ssr_game_update(game, 1);
        if (strcmp(ssr_game_screen(game), "interview") == 0) {
            give_an_interview(game, tweaks);
            interviewed = 1;
        }
    }
    // The first two matches also give an interview, one won and one lost.
    if (!interviewed && interviews < 2) {
        ssr_game_message(game, "debug.interview");
        CHECK(strcmp(ssr_game_screen(game), "interview") == 0);
        give_an_interview(game, tweaks);
        interviewed = 1;
    }
    float rating = ssr_tweaks_number(tweaks, "MatchStats", "MatchRating");
    CHECK(rating >= 1 && rating <= 10);
    int mod = (int)ssr_tweaks_number(tweaks, "RelationshipMods", "Boss");
    if (!interviewed) CHECK(c->relationBoss == (boss + mod < 1 ? 1 : boss + mod > 100 ? 100 : boss + mod));
    CHECK(ssr_tweaks_number(tweaks, "Player", "StarRating") == c->starRating / 100 + 1);

    // The ratings screen opens over the stats, and Back comes back to them.
    ssr_game_message(game, "skills.btn_Ratings");
    CHECK(strcmp(ssr_game_screen(game), "ratings") == 0);
    for (int i = 0; i < 8; i++) {
        char name[40];
        snprintf(name, sizeof(name), "ShiftedRating%d", i + 1);
        double up = ssr_tweaks_number(tweaks, "Player", name);
        snprintf(name, sizeof(name), "NegativeShiftedRating%d", i + 1);
        double down = ssr_tweaks_number(tweaks, "Player", name);
        CHECK(fabs(5 + up - down - c->ratings[i]) < 1e-4 && (up == 0 || down == 0));
    }
    double type = ssr_tweaks_number(tweaks, "Player", "PlayerType");
    CHECK(type >= 0 && type <= 20 && strlen(ssr_tweaks_get(tweaks, "Player", "PlayerTypeString")) > 0);
    ssr_game_message(game, "ratings.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "matchstats") == 0);

    // The other matches of the round play, and the results show. Then the
    // table, and Continue gives the weekly update.
    int date = c->sdate;
    ssr_game_message(game, "matchstats.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "leaguefixtures") == 0 && c->sdate == date + 1);
    // The awards of the match (CheckAchievements): a goal, a hat trick,
    // five passes, an assist, and the goals of the career.
    if (c->goals > goals) CHECK(c->awards[2] > 0);
    if (c->goals >= goals + 3) CHECK(c->awards[3] > 0);
    if (c->passes >= passes + 5) CHECK(c->awards[4] > 0);
    if (c->assists > assists) CHECK(c->awards[6] > 0);
    CHECK((c->awards[23] > 0) == (c->goals >= 50));
    // The records: a pass of the match goes into the furthest pass of the
    // season, and the career keeps the best of all seasons. A new record
    // of the career gives an alert.
    if (c->passes > passes) CHECK(c->seasonFurthest[1] > 0);
    for (int i = 0; i < 4; i++) CHECK(c->seasonFurthest[i] <= c->furthest[i] && c->furthest[i] >= furthest[i]);
    if (c->furthest[1] > furthest[1]) {
        int id = 0;
        const char *layout = ssr_game_alert(game, &id);
        for (int k = 0; k < 64 && layout && strcmp(layout, "bottommessagerecord") != 0; k++) {
            ssr_game_message(game, "message.dismiss");
            layout = ssr_game_alert(game, &id);
        }
        CHECK(layout && strcmp(layout, "bottommessagerecord") == 0);
        CHECK(!strstr(ssr_tweaks_get(tweaks, "Menu", "BottomQuickMessage"), "$DIST"));
        records++;
    }
    CHECK(ssr_tweaks_number(tweaks, "Menu", "LeagueFixturesResults") == 1);
    rows = take_ops(game, ops, 256);
    CHECK(has_op(ops, rows, SSR_OP_LIST_ADD, "FixtureList", "") && !has_op(ops, rows, SSR_OP_LIST_TEXT, "FixtureList", "vs."));
    results_play(game, tweaks);
    CHECK(strcmp(ssr_game_screen(game), "league") == 0 && ssr_tweaks_number(tweaks, "Menu", "ShowLeaguePlay") == 1);
    rows = take_ops(game, ops, 256);
    int entries = 0;
    for (int i = 0; i < rows; i++) entries += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "TeamList") == 0;
    CHECK(entries == 4 && has_op(ops, rows, SSR_OP_LIST_SUB, "TeamList", NULL));
    // The squad of the club of the player, then the top scorers of the
    // league, then back to the table.
    if (!squadSeen) {
        squadSeen = 1;
        ssr_game_message(game, "league.btn_Squad");
        CHECK(strcmp(ssr_game_screen(game), "squad") == 0);
        CHECK(strlen(ssr_tweaks_get(tweaks, "Squad", "Title")) > 0);
        // The facts of the club, and its budget from its strength and its
        // titles.
        CHECK(strcmp(ssr_game_text(game, "GS_ClubInfo"), "SSR_ClubInfo") == 0);
        CHECK(ssr_game_club_budget(game, ssr_game_career(game)->clubId) > 0 && ssr_game_club_budget(game, 99999) == 0);
        rows = take_ops(game, ops, 256);
        int players = 0, scorers = 0;
        for (int i = 0; i < rows; i++) players += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "PlayerList") == 0;
        CHECK(players >= SSR_SQUAD_SIZE);
        // The editor: a tap on the name of the club, a new name, and Enter.
        ssr_game_message(game, "squad.inp_Name");
        CHECK(ssr_game_wants_text(game));
        ssr_game_key(game, SSR_KEY_BACKSPACE);
        ssr_game_type(game, "X");
        char edited[64];
        snprintf(edited, sizeof(edited), "%s", ssr_game_text(game, "GS_SquadTitle"));
        ssr_game_key(game, SSR_KEY_ENTER);
        CHECK(!ssr_game_wants_text(game) && strcmp(ssr_game_text(game, "GS_SquadTitle"), edited) == 0);
        CHECK(edited[strlen(edited) - 1] == 'X');
        // The editor: the club wears the next kit of the editor. The kits
        // go round.
        CHECK(ssr_tweaks_number(tweaks, "Squad", "CanKit") == 1);
        for (int i = 0; i < 12; i++) ssr_game_message(game, "squad.btn_Kit");
        ssr_game_message(game, "squad.btn_Mode");
        rows = take_ops(game, ops, 256);
        for (int i = 0; i < rows; i++) scorers += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "PlayerList") == 0;
        CHECK(scorers >= 1 && scorers <= 20 && strcmp(ssr_tweaks_get(tweaks, "Squad", "Title"), "PRM") == 0);
        CHECK(ssr_tweaks_number(tweaks, "Squad", "CanKit") == 0);
        // The editor: the list of the scorers names the league.
        ssr_game_message(game, "squad.inp_Name");
        CHECK(ssr_game_wants_text(game) && strcmp(ssr_game_text(game, "GS_SquadTitle"), "PRM") == 0);
        ssr_game_type(game, "X");
        ssr_game_key(game, SSR_KEY_ENTER);
        CHECK(strcmp(ssr_game_text(game, "GS_SquadTitle"), "PRMX") == 0);
        ssr_game_message(game, "squad.inp_Name");
        ssr_game_key(game, SSR_KEY_BACKSPACE);
        ssr_game_key(game, SSR_KEY_ENTER);
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Squad", "Title"), "PRM") == 0);
        // The third list: the player and the three rivals.
        ssr_game_message(game, "squad.btn_Mode");
        rows = take_ops(game, ops, 256);
        int rivalRows = 0;
        for (int i = 0; i < rows; i++) rivalRows += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "PlayerList") == 0;
        CHECK(rivalRows == 4);
        // The player changes position there, and back again.
        CHECK(ssr_tweaks_number(tweaks, "Squad", "CanPosition") == 1);
        int position = ssr_game_career(game)->position;
        ssr_game_message(game, "squad.btn_Position");
        CHECK(ssr_game_career(game)->position == (position + 1) % SSR_CAREER_POSITIONS);
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Squad", "PositionText"), "SSR_PosForward") != 0 || position == 2);
        for (int i = 1; i < SSR_CAREER_POSITIONS; i++) ssr_game_message(game, "squad.btn_Position");
        CHECK(ssr_game_career(game)->position == position);
        take_ops(game, ops, 256);
        ssr_game_message(game, "squad.btn_Back");
        CHECK(strcmp(ssr_game_screen(game), "league") == 0);
        take_ops(game, ops, 256);
    }
    // A row of the table, tapped two times, opens the fixtures of its team:
    // the six matches of the season, one of them played at least.
    ssr_game_message(game, "league.row_team 1");
    ssr_game_message(game, "league.row_team 1");
    CHECK(strcmp(ssr_game_screen(game), "clubfixtures") == 0);
    rows = take_ops(game, ops, 256);
    int fixtures = 0, played = 0;
    for (int i = 0; i < rows; i++) {
        if (ops[i].kind != SSR_OP_LIST_ADD || strcmp(ops[i].ref, "FixtureList") != 0) continue;
        fixtures++;
        played += strcmp(ops[i].ref2, "PendingFixture") != 0;
    }
    CHECK(fixtures >= 6 && played >= 1 && has_op(ops, rows, SSR_OP_TEXT, "FixtureTeamName", NULL));
    ssr_game_message(game, "league.btn_League");
    CHECK(strcmp(ssr_game_screen(game), "league") == 0);
    ssr_game_message(game, "league.btn_Continue");
    CHECK(strcmp(ssr_game_screen(game), "matchreview") == 0);
    CHECK(ssr_tweaks_number(tweaks, "MatchReview", "EarningsTotal") == -1);
    ssr_game_update(game, 26);
    CHECK(ssr_tweaks_number(tweaks, "Player", "Recovery") == 100 && c->energy == 100);
    ssr_game_message(game, "matchreview.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "matchreview") == 0);
    for (int i = 0; i < 2000 && ssr_tweaks_number(tweaks, "MatchStats", "StatStage") < 14; i++) {
        ssr_game_update(game, 1);
    }
    // The match pays the wage and the bonuses.
    CHECK(c->bank == bank + (int)ssr_tweaks_number(tweaks, "MatchReview", "EarningsTotal"));
    CHECK(ssr_tweaks_number(tweaks, "MatchReview", "EarningsTotal") >= c->wage);
    // Each property gives 1.5 energy, and each sponsor pays each week.
    int properties = 0, sponsors = 0;
    for (int i = 0; i < SSR_CAREER_ITEMS; i++) properties += c->properties[i] > 0;
    for (int i = 0; i < 10; i++) sponsors += c->sponsors[i];
    CHECK(fabs(ssr_tweaks_number(tweaks, "Player", "RecoveryBoostProperties") - 1.5 * properties) < 1e-4);
    CHECK(ssr_tweaks_number(tweaks, "MatchReview", "EarningsSponsors") == sponsors);
    ssr_game_message(game, "matchreview.btn_Play");
    if (strcmp(ssr_game_screen(game), "seasonreview") == 0) finish_season(game, tweaks);
    // The game menu can open with a message: the hint of the agent, or a
    // contract that ran out.
    for (int i = 0; i < 4 && strncmp(ssr_game_screen(game), "message", 7) == 0; i++) {
        ssr_game_message(game, "message.btn_Cancel");
    }
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Saves the career with full skills and energy, and loads it again.
static void make_a_star(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    for (int i = 0; i < SSR_CAREER_SKILL_LEVELS; i++) {
        c.technique[i] = c.power[i] = c.pace[i] = c.vision[i] = c.setPieces[i] = 3;
    }
    c.energy = 100;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Saves the career with half of the energy, and loads it again.
static void tire(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.energy = 50;
    // Quit saves the career, so the test writes its file after it.
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0 && ssr_game_career(game)->energy == 50);
}

// A player that the boss does not like is not picked: the message, then
// the results of the day with no match.
static void not_picked_by_the_boss(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.relationBoss = 5;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    int apps = ssr_game_career(game)->apps;
    press_play(game);
    if (strcmp(ssr_game_screen(game), "league") == 0) return;
    // The news of the week can change the relation: it is 5 at the match.
    ((SSRCareer *)ssr_game_career(game))->relationBoss = 5;
    ssr_game_message(game, "leaguefixtures.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "messageboss") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_NOTPICKEDRELATION_NEW") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "leaguefixtures") == 0 && ssr_game_career(game)->apps == apps);
    ssr_game_message(game, "leaguefixtures.btn_Play");
    if (strcmp(ssr_game_screen(game), "league") == 0) ssr_game_message(game, "league.btn_Continue");
    if (strcmp(ssr_game_screen(game), "seasonreview") == 0) finish_season(game, tweaks);
    CHECK(at_game_menu(game, tweaks));
}

// Saves the career after three activities, with two sponsors and the news
// of a cup winner, and loads it again.
static void busy(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    // The next match plays with NS-Rocket.
    c.ballLeague = 2;
    ballChecked = 0;
    c.spentTime = 3;
    c.sponsors[0] = c.sponsors[1] = 4;
    c.relationSponsors = 50;
    snprintf(c.headlineResult, sizeof(c.headlineResult), "PRM have won the CUP!");
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0 && ssr_game_career(game)->spentTime == 3);
}

// Saves the career with full energy, no activities, and no sponsors, and
// loads it again.
static void rest(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.energy = 100;
    c.spentTime = 0;
    c.relationBoss = 50;
    c.relationSponsors = 0;
    for (int i = 0; i < 10; i++) c.sponsors[i] = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// The faces of the cards of a new pairs game, from the ops that show them.
static int card_faces(SSRGame *game, char faces[16][48]) {
    SSRUiOp ops[128];
    int count = take_ops(game, ops, 128), found = 0;
    for (int i = 0; i < count; i++) {
        int k;
        if (ops[i].kind != SSR_OP_ANIM_PLAY || !ops[i].ref2[0] || sscanf(ops[i].ref, "Button%d", &k) != 1) continue;
        if (k < 1 || k > 16) continue;
        snprintf(faces[k - 1], 48, "%s", ops[i].ref2);
        found++;
    }
    return found;
}

// Clicks two cards: a pair when same is 1, else two different faces.
static void click_two(SSRGame *game, char faces[16][48], int same) {
    for (int a = 0; a < 16; a++) {
        for (int b = a + 1; b < 16; b++) {
            if ((strcmp(faces[a], faces[b]) == 0) != same) continue;
            char message[32];
            snprintf(message, sizeof(message), "pairs.btn_%d", a);
            ssr_game_message(game, message);
            snprintf(message, sizeof(message), "pairs.btn_%d", b);
            ssr_game_message(game, message);
            return;
        }
    }
}

// The Life screen: the bars, a won and a lost pairs game, and the limit
// of three activities between matches.
static void spend_time(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    rest(game, path);
    const SSRCareer *c = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Player", "BossRelationship_Title"), "BOSS_CAPS_SPACEAFTER 50") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Player", "BossRelationship_Displayed") == 50);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "InternationalBoss") == 0);
    // No partner and no sponsors: a message with the error sound, and no
    // activity.
    while (ssr_game_take_sound(game)) {
    }
    ssr_game_message(game, "relationships.btn_Girlfriend");
    CHECK(strcmp(ssr_game_screen(game), "messageboyfriend") == 0);
    const char *sound = ssr_game_take_sound(game);
    CHECK(sound && strcmp(sound, "CError") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && c->spentTime == 0);
    ssr_game_message(game, "relationships.btn_Sponsors");
    CHECK(strcmp(ssr_game_screen(game), "messagesponsor") == 0);
    ssr_game_message(game, "message.btn_Ok");
    // The national view turns off the club bars.
    ssr_game_message(game, "relationships.btn_International");
    CHECK(ssr_tweaks_number(tweaks, "Menu", "InternationalBoss") == 1);
    ssr_game_message(game, "relationships.btn_Boss");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && c->spentTime == 0);
    ssr_game_message(game, "relationships.btn_International");

    // A won game: the boss likes the player 5 more.
    char faces[16][48] = {{0}};
    SSRUiOp drop;
    while (ssr_game_take_op(game, &drop)) {
    }
    ssr_game_message(game, "relationships.btn_Boss");
    CHECK(strcmp(ssr_game_screen(game), "pairs") == 0 && c->energy == 65 && c->spentTime == 1);
    // The banner is in capitals in English (TQuickMessage.InitBanner).
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "QuickMessage"), "FIND A PAIR!") == 0);
    CHECK(card_faces(game, faces) == 16);
    // The cards turn and move before the player can click.
    ssr_game_message(game, "pairs.btn_0");
    ssr_game_update(game, 60 * 4);
    while (ssr_game_take_sound(game)) {
    }
    click_two(game, faces, 1);
    ssr_game_update(game, 90);
    CHECK(strcmp(ssr_game_screen(game), "messageboss") == 0 && c->relationBoss == 55);
    // Each card plays the deal sound, then the pair plays the success.
    int deals = 0, success = 0;
    while ((sound = ssr_game_take_sound(game)) != NULL) {
        deals += strcmp(sound, "DealCard") == 0;
        success += strcmp(sound, "CSuccess") == 0;
    }
    CHECK(deals == 2 && success == 1);
    CHECK(strncmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CDILEMMA_BOSS", 13) == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Player", "BossRelationship_Title"), "BOSS_CAPS_SPACEAFTER 55") == 0);

    // A lost game: two misses, then the screen comes back with no change.
    // In German the banner keeps its text.
    ssr_game_set_language(game, 6, 0);
    ssr_game_message(game, "relationships.btn_Team");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "QuickMessage"), "Find a pair!") == 0);
    ssr_game_set_language(game, 0, 0);
    float team = c->relationTeam;
    CHECK(card_faces(game, faces) == 16 && c->spentTime == 2 && c->energy == 30);
    ssr_game_update(game, 60 * 4);
    click_two(game, faces, 0);
    ssr_game_update(game, 90);
    CHECK(strcmp(ssr_game_screen(game), "pairs") == 0);
    click_two(game, faces, 0);
    ssr_game_update(game, 90);
    ssr_game_update(game, 70);
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && c->relationTeam == team);
    // The energy is too low for a third activity. With no can, the NRG
    // shop opens (GSPlayerNotEnoughEnergy).
    SSRCareer *own = (SSRCareer *)c;
    for (int i = 0; i < 5; i++) own->nrg[i] = 0;
    ssr_game_message(game, "relationships.btn_Fans");
    CHECK(strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0 && c->spentTime == 2);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ShopTab"), "NRG") == 0);
    ssr_game_message(game, "UpsellMsg.Close");
    // With a can, the overlay offers it, and the can fills the energy.
    own->nrg[0] = 1;
    ssr_game_message(game, "relationships.btn_Fans");
    CHECK(strcmp(ssr_game_overlay(game), "upsellnrg") == 0);
    ssr_game_message(game, "UpsellMsg.DrinkNRG");
    CHECK(ssr_game_overlay(game)[0] == 0 && c->nrg[0] == 0 && c->energy == 100);
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// The relation of a dilemma template, from the end of its name.
static float dilemma_relation(const SSRCareer *c, const char *name) {
    const char *at = strstr(name, "Dilemma");
    if (!at) return -1;
    at += 7;
    if (strcmp(at, "Boss") == 0) return c->relationBoss;
    if (strcmp(at, "Team") == 0) return c->relationTeam;
    if (strcmp(at, "Fans") == 0) return c->relationFans;
    if (strcmp(at, "Sponsor") == 0) return c->relationSponsors;
    if (strcmp(at, "Girlfriend") == 0) return c->relationPartner;
    return -1;
}

// A dilemma: two cards of two relations. The choice gives one 5 and takes
// 10 from the other, then the game menu comes back.
static void choose_a_dilemma(SSRGame *game, const char *path) {
    rest(game, path);
    const SSRCareer *c = ssr_game_career(game);
    SSRUiOp drop;
    while (ssr_game_take_op(game, &drop)) {
    }
    // The club relations and the sponsors at 96: the card of the choice
    // goes to 100 and unlocks its award (UpdateRelationship).
    SSRCareer *live = (SSRCareer *)c;
    live->relationBoss = live->relationTeam = live->relationFans = live->relationSponsors = 96;
    ssr_game_message(game, "debug.dilemma");
    CHECK(strcmp(ssr_game_screen(game), "dilemma") == 0 && ssr_game_splash(game));
    SSRUiOp ops[64];
    int count = take_ops(game, ops, 64);
    char top[48] = "", bottom[48] = "";
    for (int i = 0; i < count; i++) {
        if (ops[i].kind != SSR_OP_HOOK) continue;
        if (strcmp(ops[i].ref, "TopHook") == 0) snprintf(top, sizeof(top), "%s", ops[i].ref2);
        if (strcmp(ops[i].ref, "BottomHook") == 0) snprintf(bottom, sizeof(bottom), "%s", ops[i].ref2);
    }
    float a = dilemma_relation(c, top), b = dilemma_relation(c, bottom);
    CHECK(a >= 0 && b >= 0 && strcmp(top, bottom) != 0);
    // The player has no partner, so no card is of the partner.
    CHECK(!strstr(top, "Girlfriend") && !strstr(bottom, "Girlfriend"));
    // The cards do not take a click while the splash shows.
    ssr_game_message(game, "dilemma.btn_relationship1");
    CHECK(dilemma_relation(c, top) == a);
    ssr_game_update(game, 130);
    ssr_game_message(game, "dilemma.btn_relationship1");
    CHECK(dilemma_relation(c, top) == (a + 5 > 100 ? 100 : a + 5));
    CHECK(dilemma_relation(c, bottom) == (b - 10 < 1 ? 1 : b - 10));
    static const char *const CARDS[4] = {"DilemmaBoss", "DilemmaTeam", "DilemmaFans", "DilemmaSponsor"};
    for (int k = 0; k < 4; k++) {
        if (strstr(top, CARDS[k])) CHECK(c->awards[44 + k] > 0);
        else CHECK(c->awards[44 + k] == 0);
    }
    count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_SUB, "TopHook", NULL) && has_op(ops, count, SSR_OP_SUB, "BottomHook", NULL));
    // A second click does nothing.
    ssr_game_message(game, "dilemma.btn_relationship2");
    CHECK(dilemma_relation(c, top) == (a + 5 > 100 ? 100 : a + 5));
    ssr_game_update(game, 160);
    // An accident can cost a skill. Its message comes over the game menu.
    if (strcmp(ssr_game_screen(game), "messagedefault") == 0) ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Loads the career c from the save file, with no news done.
static void load_career(SSRGame *game, SSRCareer *c, const char *path) {
    c->newsDone = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
}

// A skill with no training: with no trainer it loses its top level after
// the days of the year. With a trainer, the trainer warns first, and the
// next check takes the level.
static void lose_a_skill_with_no_training(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    while (strncmp(ssr_game_screen(game), "message", 7) == 0) ssr_game_message(game, "message.btn_Ok");
    static SSRCareer c, before;
    before = *ssr_game_career(game);
    c = before;
    c.sdate = 364 * 2 + 50;
    for (int i = 0; i < 3; i++) c.trainer[i] = c.agent[i] = 0;
    for (int r = 0; r < 5; r++) c.lastTrained[r] = c.deplete[r] = c.sdate;
    for (int k = 0; k < SSR_CAREER_SKILL_LEVELS; k++) c.pace[k] = k < 6 ? 3 : 0;
    // Year 3 keeps a skill 105 days.
    c.deplete[0] = c.sdate - 106;
    c.lastAgentHint = c.sdate;
    load_career(game, &c, path);
    const SSRCareer *now = ssr_game_career(game);
    CHECK(strcmp(ssr_game_screen(game), "messagetrainer") == 0);
    CHECK(strstr(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_LASTTRAINED|(CMESSAGE_LASTTRAINED2)") != NULL);
    CHECK(now->pace[5] == 0 && now->pace[4] == 3 && now->deplete[0] == now->sdate && now->power[1] == c.power[1]);
    SSRUiOp ops[64];
    int count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_SHOW, "OneBar", NULL) && has_op(ops, count, SSR_OP_BAR_SOURCE, "BarPBDoodad", "Player,Pace"));
    ssr_game_message(game, "message.btn_Ok");

    // A trainer warns of two skills: no level goes yet.
    c = *ssr_game_career(game);
    c.trainer[0] = 20;
    c.deplete[1] = c.sdate - 106;
    c.deplete[3] = c.sdate - 106;
    for (int k = 0; k < 4; k++) c.power[k] = c.vision[k] = 3;
    load_career(game, &c, path);
    CHECK(strcmp(ssr_game_screen(game), "messagetrainer") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_TRAINERWARNING") == 0);
    CHECK(now->deplete[1] == -(c.sdate - 106) && now->deplete[3] < 0 && now->power[3] == 3);
    count = take_ops(game, ops, 64);
    int rows = 0;
    for (int i = 0; i < count; i++) rows += ops[i].kind == SSR_OP_LIST_ADD_AT && strcmp(ops[i].ref, "SkillsScroll") == 0;
    CHECK(has_op(ops, count, SSR_OP_SHOW, "SkillsDepletion", NULL) && rows == 2);
    ssr_game_message(game, "message.btn_Ok");
    // The next check takes the top level of each warned skill.
    c = *ssr_game_career(game);
    int power = 0, vision = 0, powerNow = 0, visionNow = 0;
    for (int k = 0; k < SSR_CAREER_SKILL_LEVELS; k++) {
        power += c.power[k];
        vision += c.vision[k];
    }
    load_career(game, &c, path);
    CHECK(strcmp(ssr_game_screen(game), "messagetrainer") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_LASTTRAINED") == 0);
    for (int k = 0; k < SSR_CAREER_SKILL_LEVELS; k++) {
        powerNow += now->power[k];
        visionNow += now->vision[k];
    }
    CHECK(powerNow < power && visionNow < vision);
    CHECK(now->deplete[1] == now->sdate && now->deplete[3] == now->sdate);
    ssr_game_message(game, "message.btn_Ok");
    // The career goes on from where it was.
    load_career(game, &before, path);
    CHECK(at_game_menu(game, tweaks));
}

// The tickers of the career: each kind has its rule, and most count the
// requests since they showed. Before a career and after a quit, none
// shows.
static void read_the_tickers(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    while (strncmp(ssr_game_screen(game), "message", 7) == 0) ssr_game_message(game, "message.btn_Ok");
    static SSRCareer c, before;
    before = *ssr_game_career(game);
    c = before;
    for (int i = 0; i < 10; i++) c.nrg[i] = 0;
    c.nrg[0] = 2;
    c.relationBoss = 80;
    c.relationSponsors = 25;
    for (int i = 0; i < 10; i++) c.sponsors[i] = 0;
    c.sponsors[3] = 5;
    load_career(game, &c, path);
    at_game_menu(game, tweaks);
    int count = 0;
    char text[256] = "";
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_LOW_NRG, &count) == 1);
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_LOW_NRG, &count) == 2);
    ssr_game_ticker_text(game, SSR_TICKER_LOW_NRG, &count, text, sizeof(text));
    CHECK(count == 0 && strcmp(text, "TICKER_LOW_NRG") == 0);
    // The next match: 0 the first time after it showed, then 1000.
    count = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_NEXT_MATCH, &count) == 0);
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_NEXT_MATCH, &count) == 1000);
    count = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_HIGH_BOSS, &count) == 1);
    // The sponsors from 20 to 30 keep the last text.
    count = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_LOW_SPONSORS, &count) == 1);
    snprintf(text, sizeof(text), "Old text");
    ssr_game_ticker_text(game, SSR_TICKER_LOW_SPONSORS, &count, text, sizeof(text));
    CHECK(strcmp(text, "Old text") == 0);
    ((SSRCareer *)ssr_game_career(game))->relationSponsors = 15;
    ssr_game_ticker_text(game, SSR_TICKER_LOW_SPONSORS, &count, text, sizeof(text));
    CHECK(strcmp(text, "TICKER_LOW_SPONSORS") == 0);
    // Three cans, and a boss under 80 as an int: no ticker.
    ((SSRCareer *)ssr_game_career(game))->nrg[1] = 1;
    ((SSRCareer *)ssr_game_career(game))->relationBoss = 79.9f;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_LOW_NRG, &count) == -1);
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_HIGH_BOSS, &count) == -1);
    // After a quit the player is blank.
    ssr_game_message(game, "gamemenu.btn_Quit");
    for (int kind = 1; kind < SSR_TICKER_KINDS; kind++) CHECK(ssr_game_ticker_priority(game, kind, &count) == -1);
    load_career(game, &before, path);
    CHECK(at_game_menu(game, tweaks));
}

// The achievements: the stats of the career, the trophy log with the
// newest first, and the clubs with the part of the season first.
static void look_at_achievements(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    SSRUiOp drop;
    while (ssr_game_take_op(game, &drop)) {
    }
    // The form of the career shows the seasons with a rating, or the last
    // four of a long text.
    SSRCareer *form = (SSRCareer *)c;
    int kept[5];
    memcpy(kept, form->careerForm, sizeof(kept));
    int forms[2][5] = {{0, 0, 6, 7, 8}, {10, 10, 10, 10, 9}};
    const char *texts[2] = {"6-7-8", "10-10-10-9"};
    float pass = form->furthest[1], headed = form->seasonFurthest[3];
    form->furthest[1] = 640;
    form->seasonFurthest[3] = 0;
    for (int k = 0; k < 2; k++) {
        memcpy(form->careerForm, forms[k], sizeof(kept));
        ssr_game_message(game, "relationships.btn_Achievements");
        CHECK(strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerForm"), texts[k]) == 0);
        // 640 pixels are 33.33 yards; the game cuts the text to 33.3.
        CHECK(strncmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerFurthestPass"), "33.3 ", 5) == 0);
        CHECK(strncmp(ssr_tweaks_get(tweaks, "PlayerStats", "SeasonFurthestHeadedPass"), "0.0 ", 4) == 0);
        ssr_game_message(game, "achievements.btn_Home");
    }
    memcpy(form->careerForm, kept, sizeof(kept));
    form->furthest[1] = pass;
    form->seasonFurthest[3] = headed;
    while (ssr_game_take_op(game, &drop)) {
    }
    ssr_game_message(game, "relationships.btn_Achievements");
    CHECK(strcmp(ssr_game_screen(game), "achievements") == 0);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "CareerMatches") == c->apps);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "CareerHatTricks") == c->hatTricks);
    CHECK(ssr_tweaks_number(tweaks, "PlayerStats", "InternationalGoals") == c->intGoals);
    static SSRUiOp ops[2048];
    int count = take_ops(game, ops, 2048), trophies = 0, clubs = 0, first = -1, firstClub = -1;
    for (int i = 0; i < count; i++) {
        if (ops[i].kind != SSR_OP_LIST_ADD) continue;
        if (strcmp(ops[i].ref, "TrophyHook") == 0 && trophies++ == 0) first = i;
        if (strcmp(ops[i].ref, "ClubStats") == 0 && clubs++ == 0) firstClub = i;
    }
    CHECK(c->trophyCount > 0 && trophies == c->trophyCount);
    char year[16];
    snprintf(year, sizeof(year), "%d", c->trophies[c->trophyCount - 1].year);
    CHECK(first >= 0 && ops[first + 1].kind == SSR_OP_LIST_TEXT && strcmp(ops[first + 1].text, year) == 0);
    CHECK(strcmp(ops[first + 3].text, c->trophies[c->trophyCount - 1].tournament) == 0);
    int rows = 0;
    for (int i = 0; i < c->historyCount; i++) rows += c->history[i].year > 0;
    CHECK(clubs == rows + 1);
    snprintf(year, sizeof(year), "%d", ssr_career_year(c));
    CHECK(firstClub >= 0 && strcmp(ops[firstClub + 1].text, year) == 0);
    ssr_game_message(game, "achievements.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// An accident after a dilemma takes the top level of a skill. The message
// shows the bar of the skill out of 60, with the loss (SetUpSkillBar).
static void an_accident_costs_a_skill(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    int *rows[5] = {c.pace, c.power, c.technique, c.vision, c.setPieces};
    for (int i = 0; i < 5; i++) rows[i][1] += 10;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    int found = 0;
    for (int seed = 1; seed <= 400 && !found; seed++) {
        rest(game, path);
        ssr_game_set_seed(game, seed);
        ssr_game_message(game, "debug.dilemma");
        ssr_game_update(game, 130);
        ssr_game_message(game, "dilemma.btn_relationship1");
        SSRUiOp drop;
        while (ssr_game_take_op(game, &drop)) {
        }
        ssr_game_update(game, 160);
        if (strcmp(ssr_game_screen(game), "messagedefault") != 0) continue;
        found = 1;
        static SSRUiOp ops[1024];
        int count = take_ops(game, ops, 1024), bars = 0, text = 0;
        for (int i = 0; i < count; i++) {
            if (ops[i].kind == SSR_OP_BAR_SOURCE) bars += strncmp(ops[i].text, "Player,", 7) == 0 && ops[i].id == 60;
            if (ops[i].kind == SSR_OP_TEXT && strcmp(ops[i].ref, "BarText") == 0) text = strstr(ops[i].text, " -") != NULL;
        }
        CHECK(has_op(ops, count, SSR_OP_SHOW, "OneBar", NULL) && bars == 3 && text);
        ssr_game_message(game, "message.btn_Ok");
    }
    CHECK(found);
}

// The lifestyle shop: a purchase, too little cash, the question of the
// first property, a repair, and the end of a trip with the team-mates.
// The casino: the stake, and eleven spins of the slots. Each tenth bet
// costs 5 energy, and the bets of the visit cost energy at the end.
static void go_to_the_casino(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 100;
    c.energy = 100;
    c.gambling = 0;
    c.casinoCount = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Casino");
    CHECK(strcmp(ssr_game_screen(game), "casino") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == 1);
    CHECK(ssr_tweaks_number(tweaks, "Casino", "TeammatesBetCount") == -1);
    ssr_game_message(game, "casino.btn_Slots");
    CHECK(strcmp(ssr_game_screen(game), "slots") == 0);
    SSRUiOp ops[256];
    int count = take_ops(game, ops, 256), frames = 0;
    for (int i = 0; i < count; i++) frames += ops[i].kind == SSR_OP_ANIM_FRAME;
    CHECK(frames == 24 && has_op(ops, count, SSR_OP_ANIM_FRAME, "ReelAnims3", NULL));
    // The stake does not go under level 1. Two levels up is 5 bux.
    ssr_game_message(game, "casino.btn_StakeDown");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == 1);
    ssr_game_message(game, "casino.btn_StakeUp");
    ssr_game_message(game, "casino.btn_StakeUp");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == 5);
    while (ssr_game_take_sound(game)) {
    }
    int bank = 100, wins = 0;
    for (int spin = 1; spin <= 11; spin++) {
        float energy = now->energy;
        ssr_game_message(game, "slots.btn_Play");
        CHECK(now->bank == bank - 5 && now->gambling == spin && now->casinoCount == spin);
        // The eleventh bet comes after ten: it costs 5 energy.
        CHECK(now->energy == (spin == 11 ? energy - 5 : energy));
        // The stake cannot change, and a second press does nothing.
        ssr_game_message(game, "casino.btn_StakeUp");
        ssr_game_message(game, "slots.btn_Play");
        CHECK(now->bank == bank - 5 && ssr_tweaks_number(tweaks, "Casino", "Stake") == 5);
        double outcome = 0;
        for (int f = 0; f < 4 * 60 && outcome == 0; f++) {
            ssr_tweaks_set_number(tweaks, "Casino", "CasinoOutcome", 0);
            ssr_game_update(game, 1);
            outcome = ssr_tweaks_number(tweaks, "Casino", "CasinoOutcome");
        }
        CHECK(outcome == 1 || outcome == -1);
        // The sounds: the arm, a stop for each reel, then the result.
        const char *sounds[8] = {0};
        int heard = 0;
        for (const char *sound; (sound = ssr_game_take_sound(game)) != NULL;) {
            if (heard < 8) sounds[heard] = sound;
            heard++;
        }
        CHECK(heard >= 5 && strcmp(sounds[0], "SlotsPlay") == 0 && strcmp(sounds[3], "SlotsItemLock") == 0);
        // The first win unlocks the award of the slots, with its sound first
        // (CheckAchievement comes before the sound of the win).
        int after = sounds[4] && strcmp(sounds[4], "CAchievement") == 0 ? 5 : 4;
        CHECK(after == 4 || (outcome == 1 && now->awards[32] > 0));
        CHECK(sounds[after] && strcmp(sounds[after], outcome == 1 ? "SlotsYouWin" : "SlotsYouLose") == 0);
        int win = (int)ssr_tweaks_number(tweaks, "Casino", "Winnings");
        CHECK(outcome == 1 ? win == 15 || win == 35 : win == 0);
        if (win > 0) {
            wins++;
            CHECK(strncmp(ssr_tweaks_get(tweaks, "Menu", "QuickMessage"), "BET_YOUWON \xE2\x98\x85", 14) == 0);
            CHECK(strcmp(ssr_game_splash_layout(game), "splashmessagebux") == 0);
        }
        bank = bank - 5 + win;
        CHECK(now->bank == bank);
    }
    CHECK(wins > 0);
    // Back: the one bet after the reset costs 1 more energy.
    float energy = now->energy;
    ssr_game_message(game, "slots.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "casino") == 0);
    ssr_game_message(game, "casino.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && now->energy == energy - 1);
    // A low bank takes the stake down.
    ssr_game_message(game, "relationships.btn_Casino");
    ssr_game_message(game, "casino.btn_Slots");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == (bank >= 5 ? 5 : bank >= 2 ? 2 : 1));
    ssr_game_message(game, "slots.btn_Quit");
    ssr_game_message(game, "casino.btn_Back");
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);

    // A player under 18 who gambles can be caught: 1 bet in 20. The test
    // makes the player 17 for it, and gives the day back after it.
    c = *ssr_game_career(game);
    int day = c.sdate;
    if (ssr_career_age(&c) >= 18) c.sdate = 364 + 10;
    CHECK(ssr_career_age(&c) < 18);
    c.bank = 1000;
    c.gambling = 30;
    c.relationBoss = 80;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Casino");
    ssr_game_message(game, "casino.btn_Slots");
    int caught = 0, gambling = 0;
    for (int spin = 0; spin < 300 && !caught; spin++) {
        gambling = now->gambling;
        bank = now->bank;
        ssr_game_message(game, "slots.btn_Play");
        caught = strcmp(ssr_game_screen(game), "message") == 0;
        for (int f = 0; f < 4 * 60 && !caught && ssr_tweaks_number(tweaks, "Menu", "HomeButtonDisabled") == 1; f++) {
            ssr_game_update(game, 1);
        }
    }
    CHECK(caught && strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_GAMBLINGTOOYOUNG") == 0);
    // The bet is not taken.
    CHECK(now->bank == bank && now->gambling == gambling);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "webpagenews") == 0);
    CHECK(now->gambling == gambling / 2 && fabsf(now->relationBoss - 80 * 0.75f) < 1e-4f);
    ssr_game_message(game, "webpage.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    if (day != now->sdate) {
        c = *ssr_game_career(game);
        c.sdate = day;
        ssr_game_message(game, "gamemenu.btn_Quit");
        CHECK(ssr_career_save(&c, path));
        ssr_game_message(game, "mainmenu.btn_Career");
        ssr_game_message(game, "saveslot.btn_Slot0");
        ssr_game_update(game, 60);
        for (int i = 0; i < 4 && strncmp(ssr_game_screen(game), "message", 7) == 0; i++) {
            ssr_game_message(game, "message.btn_Ok");
        }
    }
}

// Roulette: three bets on red. Back is off while the wheel turns.
static void play_roulette(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 500;
    c.energy = 100;
    c.gambling = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Casino");
    ssr_game_message(game, "casino.btn_Roulette");
    CHECK(strcmp(ssr_game_screen(game), "roulette") == 0);
    SSRUiOp ops[256];
    ssr_game_update(game, 1);
    int count = take_ops(game, ops, 256);
    CHECK(has_op(ops, count, SSR_OP_GEL, "Wheel", NULL) && has_op(ops, count, SSR_OP_GEL, "Ball", NULL));
    // The stake is 10 bux.
    for (int i = 0; i < 4; i++) ssr_game_message(game, "casino.btn_StakeUp");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == 10);
    // A tap ends each alert of the awards before, one at a time.
    for (int i = 0; i < 40 && ssr_game_alert(game, NULL); i++) ssr_game_message(game, "message.dismiss");
    CHECK(ssr_game_alert(game, NULL) == NULL);
    int bank = 500;
    for (int spin = 1; spin <= 3; spin++) {
        ssr_game_message(game, "roulette.btn_Red");
        CHECK(now->bank == bank - 10 && now->gambling == 2 * spin);
        CHECK(ssr_tweaks_number(tweaks, "Menu", "HomeButtonDisabled") == 1);
        // A second bet and Back do nothing while the wheel turns.
        ssr_game_message(game, "roulette.btn_Black");
        ssr_game_message(game, "roulette.btn_Quit");
        CHECK(now->bank == bank - 10 && strcmp(ssr_game_screen(game), "roulette") == 0);
        int frames = 0;
        ssr_tweaks_set_number(tweaks, "Casino", "CasinoOutcome", 0);
        while (ssr_tweaks_number(tweaks, "Casino", "CasinoOutcome") == 0 && frames < 10 * 60) {
            ssr_game_update(game, 1);
            frames++;
        }
        // A spin takes 3 to 5 seconds.
        CHECK(frames >= 180 && frames <= 300);
        int win = (int)ssr_tweaks_number(tweaks, "Casino", "Winnings");
        CHECK(win == 0 || win == 20);
        const char *text = ssr_tweaks_get(tweaks, "Menu", "QuickMessage");
        CHECK(strncmp(text, win ? "BET_YOUWON" : "BET_YOULOST", win ? 10 : 11) == 0);
        CHECK(strcmp(ssr_game_splash_layout(game), win ? "splashmessagebux" : "splashmessage") == 0);
        bank = bank - 10 + win;
        CHECK(now->bank == bank && ssr_tweaks_number(tweaks, "Menu", "HomeButtonDisabled") == 0);
    }
    ssr_game_message(game, "roulette.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "casino") == 0);
    ssr_game_message(game, "casino.btn_Back");
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Blackjack: six hands, the player holds at once. The test sends the
// messages of the slides, as the layout does.
static void play_blackjack(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 500;
    c.energy = 100;
    c.gambling = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Casino");
    ssr_game_message(game, "casino.btn_BlackJack");
    CHECK(strcmp(ssr_game_screen(game), "blackjack") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Casino", "PlayerBJTotal"), "?") == 0);
    for (int i = 0; i < 4; i++) ssr_game_message(game, "casino.btn_StakeUp");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "Stake") == 10);
    int bank = 500, outcomes[3] = {0};
    SSRUiOp ops[512];
    // Six hands, then more until a tie came, so that the sounds of each
    // result get a check.
    int hands = 0;
    for (int hand = 1; hand <= 6 || (outcomes[1] == 0 && hand <= 60); hand++) {
        hands = hand;
        float energy = now->energy;
        ssr_game_message(game, "blackjack.btn_Play");
        CHECK(now->bank == bank - 10);
        if (hand <= 6) {
            CHECK(now->gambling == 3 * hand);
            // The fifth bet comes after four of weight 3: it costs 5 energy.
            CHECK(now->energy == (hand == 5 ? energy - 5 : energy));
        }
        // Back does nothing during a hand.
        ssr_game_message(game, "blackjack.btn_Quit");
        CHECK(strcmp(ssr_game_screen(game), "blackjack") == 0);
        ssr_tweaks_set_number(tweaks, "Casino", "CasinoOutcome", 99);
        int shown = 0, next = 0, dealt = 0, cashIn = 0, tape = 0;
        for (int f = 0; f < 20 * 60 && !next; f++) {
            ssr_game_update(game, 1);
            for (const char *sound; (sound = ssr_game_take_sound(game)) != NULL;) cashIn |= strcmp(sound, "CCashIn") == 0;
            int count = take_ops(game, ops, 512);
            for (int i = 0; i < count; i++) {
                if (ops[i].kind == SSR_OP_HOOK && ops[i].ref2[0]) {
                    dealt++;
                    if (strncmp(ops[i].ref, "PlayerCard", 10) == 0) ssr_game_message(game, "blackjack.playerCardSlid");
                    else if (strcmp(ops[i].ref, "DealerCardToFlip") == 0) ssr_game_message(game, "blackjack.dealerCardFlipped");
                    else ssr_game_message(game, "blackjack.dealerCardSlid");
                }
                if (ops[i].kind == SSR_OP_SHOW && strcmp(ops[i].ref, "HoldButton") == 0) shown = 1;
                if (ops[i].kind == SSR_OP_SHOW && strcmp(ops[i].ref, "PlayButton") == 0) next = 1;
                // A win hides the ticker tape, then shows it: a new tape.
                if (strcmp(ops[i].ref, "TickerTapeShower") == 0) {
                    if (ops[i].kind == SSR_OP_HIDE) tape = 1;
                    else if (ops[i].kind == SSR_OP_SHOW && tape == 1) tape = 2;
                }
            }
            if (shown) {
                shown = 0;
                ssr_game_message(game, "blackjack.btn_Hold");
            }
        }
        CHECK(next && dealt >= 4);
        double outcome = ssr_tweaks_number(tweaks, "Casino", "CasinoOutcome");
        int winnings = (int)ssr_tweaks_number(tweaks, "Casino", "Winnings");
        CHECK(outcome == 1 ? winnings == 20 : outcome == 0 ? winnings == 10 : outcome == -1 && winnings == 0);
        // Only a win plays the cash sound. A tie gives the bet back quietly.
        CHECK(cashIn == (outcome == 1));
        CHECK((tape == 2) == (outcome == 1));
        // The first win unlocks the award of blackjack, with an alert.
        if (outcome == 1 && outcomes[2] == 0) {
            CHECK(now->awards[33] > 0 && ssr_tweaks_number(tweaks, "Achievements", "33") == 1);
            const char *layout = ssr_game_alert(game, NULL);
            CHECK(layout && strcmp(layout, "bottommessageachievement") == 0);
            ssr_game_message(game, "message.dismiss");
        }
        outcomes[(int)outcome + 1]++;
        bank = bank - 10 + winnings;
        CHECK(now->bank == bank);
        // The totals show a number now.
        CHECK(strcmp(ssr_tweaks_get(tweaks, "Casino", "PlayerBJTotal"), "?") != 0);
    }
    CHECK(outcomes[1] > 0 && outcomes[2] > 0);
    // Back: six bets of 3, with the reset at the fifth, leave 6: 3 energy.
    float energy = now->energy;
    ssr_game_message(game, "blackjack.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "casino") == 0);
    ssr_game_message(game, "casino.btn_Back");
    if (hands == 6) CHECK(now->energy == energy - 3);
    // An alert at the bottom tells the energy of the bets. A tap on it
    // ends it.
    int alertId = 0;
    const char *layout = ssr_game_alert(game, &alertId);
    CHECK(layout && strcmp(layout, "bottommessageenergy") == 0 && alertId > 0);
    char lost[16];
    snprintf(lost, sizeof(lost), " -%d", (int)(energy - now->energy + 0.5f));
    CHECK(strstr(ssr_tweaks_get(tweaks, "Menu", "BottomQuickMessage"), lost) != NULL);
    ssr_game_update(game, 60);
    CHECK(ssr_game_alert(game, NULL) != NULL);
    ssr_game_message(game, "message.dismiss");
    CHECK(ssr_game_alert(game, NULL) == NULL);
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Runs a race to the result screen. Returns the number of updates.
static int run_race(SSRGame *game) {
    CHECK(strcmp(ssr_game_screen(game), "RacingMiniGame") == 0);
    int ticks = 0;
    while (strcmp(ssr_game_screen(game), "RacingMiniGame") == 0 && ticks < 2000) {
        ssr_game_update(game, 1);
        ticks++;
    }
    CHECK(strcmp(ssr_game_screen(game), "racingresults") == 0);
    return ticks;
}

// The races: a bet on a horse, then a horse of the player from the market,
// one more star for it, its race for a prize, and its sale.
static void go_to_the_races(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 5000;
    c.energy = 100;
    c.gambling = 0;
    c.properties[4] = 1;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Casino");
    ssr_game_message(game, "casino.btn_Racing");
    CHECK(strcmp(ssr_game_screen(game), "racing") == 0);
    CHECK(ssr_tweaks_number(tweaks, "HorseRacing", "SelectedHorse") == 0);
    CHECK(strncmp(ssr_tweaks_get(tweaks, "HorseRacing", "Horse1Name"), "Horse ", 6) == 0);
    // Race with no horse: a message, then a new field.
    ssr_game_message(game, "racing.btn_Race");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_SELECTHORSE") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "racing") == 0 && now->bank == 5000);
    // A bet of 10 on the second card.
    for (int i = 0; i < 4; i++) ssr_game_message(game, "casino.btn_StakeUp");
    const char *odds = ssr_tweaks_get(tweaks, "HorseRacing", "Horse2Odds");
    int k = atoi(odds);
    char text[32];
    snprintf(text, sizeof(text), "%d", k * 10);
    CHECK(k >= 2 && k <= 12 && strcmp(ssr_tweaks_get(tweaks, "HorseRacing", "Horse2Return"), text) == 0);
    ssr_tweaks_set_number(tweaks, "HorseRacing", "SelectedHorse", 2);
    ssr_game_message(game, "racing.btn_Race");
    CHECK(now->bank == 4990 && now->gambling == 3);
    // The race ends at frame 1350 of the track: about 7 seconds.
    int ticks = run_race(game);
    CHECK(ticks >= 425 && ticks <= 432);
    int seen[7] = {0};
    for (int n = 1; n <= 6; n++) {
        snprintf(text, sizeof(text), "Horse%dPlace", n);
        int place = (int)ssr_tweaks_number(tweaks, "HorseRacing", text);
        CHECK(place >= 1 && place <= 6);
        seen[place]++;
    }
    for (int p = 1; p <= 6; p++) CHECK(seen[p] == 1);
    int mine = (int)ssr_tweaks_number(tweaks, "HorseRacing", "Horse2Place");
    CHECK(ssr_tweaks_number(tweaks, "HorseRacing", "SelectedHorseResult") == mine);
    CHECK(now->bank == (mine == 1 ? 4990 + 10 * k : 4990));
    int bank = now->bank;
    // The market: buy the first horse for sale.
    ssr_game_message(game, "racingresults.btn_Stable");
    CHECK(strcmp(ssr_game_screen(game), "mystable") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Stable_Owned", "Count") == 0);
    int forSale = (int)ssr_tweaks_number(tweaks, "Stable_ForSale", "Count");
    CHECK(forSale > 0);
    int strength = (int)ssr_tweaks_number(tweaks, "Stable_ForSale", "0_Strength");
    const int prices[6] = {0, 100, 250, 500, 1000, 2500};
    char horse[48];
    snprintf(horse, sizeof(horse), "%s", ssr_tweaks_get(tweaks, "Stable_ForSale", "0_Name"));
    ssr_game_message(game, "horsesforsale.btn_Buy");
    CHECK(now->bank == bank - prices[strength]);
    bank = now->bank;
    CHECK(ssr_tweaks_number(tweaks, "Stable_Owned", "Count") == 1);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Stable_Owned", "0_Name"), horse) == 0);
    CHECK(ssr_tweaks_number(tweaks, "Stable_ForSale", "Count") == forSale - 1);
    CHECK(strchr(now->horses, ':') != NULL);
    // One more star, for the cost of the training.
    if (strength < 5) {
        const int costs[5] = {0, 150, 250, 500, 1500};
        ssr_game_message(game, "mystable.btn_Train");
        CHECK(strcmp(ssr_game_screen(game), "message") == 0);
        ssr_game_message(game, "message.btn_Ok");
        CHECK(now->bank == bank - costs[strength] && ssr_tweaks_number(tweaks, "Stable_Owned", "0_Strength") == strength + 1);
        strength++;
        bank = now->bank;
    }
    // The race of the own horse: no stake, a prize for the first three.
    ssr_game_message(game, "mystable.btn_Race");
    CHECK(now->bank == bank && now->gambling == 3);
    run_race(game);
    int place = 0;
    for (int n = 1; n <= 6; n++) {
        if (ssr_tweaks_number(tweaks, "HorseRacing", "SelectedHorse") != n) continue;
        snprintf(text, sizeof(text), "Horse%dPlace", n);
        place = (int)ssr_tweaks_number(tweaks, "HorseRacing", text);
    }
    const int prizes[7] = {0, 100, 50, 25, 0, 0, 0};
    CHECK(place >= 1 && now->bank == bank + prizes[place]);
    bank = now->bank;
    // Next goes back to the stable after the own race. Sell for half.
    ssr_game_message(game, "racingresults.btn_Next");
    CHECK(strcmp(ssr_game_screen(game), "mystable") == 0);
    ssr_game_message(game, "mystable.btn_SellHorse");
    ssr_game_message(game, "message.btn_Ok");
    CHECK(now->bank == bank + prices[strength] / 2 && ssr_tweaks_number(tweaks, "Stable_Owned", "Count") == 0);
    ssr_game_message(game, "mystable.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "racing") == 0);
    ssr_game_message(game, "racing.btn_Quit");
    ssr_game_message(game, "casino.btn_Back");
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    // The tests after this one have no stable.
    c = *ssr_game_career(game);
    c.properties[4] = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
}

static void go_shopping(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 40;
    c.vehicles[0] = 900;
    c.teamShopping = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_tweaks_set(tweaks, "Products", "ItemsOrder", "item_ArtSculpture_L|item_Tattoo|item_Phone");
    ssr_tweaks_set(tweaks, "Products", "VehiclesOrder", "vehicle_Bicycle_L|vehicle_Scooter");
    ssr_tweaks_set(tweaks, "Products", "PropertyOrder", "property_Apartment_L");
    ssr_game_message(game, "gamemenu.btn_Life");
    ssr_game_message(game, "relationships.btn_Shop");
    CHECK(strcmp(ssr_game_overlay(game), "LifestyleShopOverlay") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ShopTab"), "Items") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "item_ArtSculpture") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemsOwned"), "0/20") == 0);
    SSRUiOp ops[256];
    int count = take_ops(game, ops, 256);
    int buttons = 0;
    for (int i = 0; i < count; i++) buttons += ops[i].kind == SSR_OP_LIST_ADD_AT;
    CHECK(buttons == 3 && has_op(ops, count, SSR_OP_ENTRY_CHILD, "LifestyleList", NULL));
    // The sculpture costs 90: too much.
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bank == 40 && now->items[17] == 0 && ssr_game_splash(game));
    // The tattoo costs 5.
    ssr_tweaks_set(tweaks, "Menu", "ItemString", "item_Tattoo");
    ssr_game_message(game, "shopoverlay.onItemSelected");
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bank == 35 && now->items[10] == 1 && now->teamShopping == 1);
    CHECK(strcmp(ssr_game_modal(game), "PurchaseAnim") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemsOwned"), "1/20") == 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    CHECK(ssr_game_modal(game)[0] == 0);
    // The bicycle has a wear of 900, so 2 of its 5 lives are left: the
    // repair costs 45 a hundredth of the price, 4.5, cut to 4.
    ssr_game_message(game, "shop.tab.vehicles");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "vehicle_Bicycle") == 0);
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(strcmp(ssr_game_modal(game), "repairmenu") == 0);
    ssr_game_message(game, "Repair.Confirm");
    CHECK(now->vehicles[0] == 1 && now->bank == 31 && now->teamShopping == 3 && ssr_game_modal(game)[0] == 0);
    // The first property asks first. No keeps the cash.
    ssr_game_message(game, "shop.tab.property");
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && ssr_game_overlay(game)[0] == 0);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_overlay(game), "LifestyleShopOverlay") == 0 && now->properties[0] == 0 && now->bank == 31);
    // Yes buys it.
    ssr_game_message(game, "shopoverlay.buy");
    ssr_game_message(game, "message.btn_Ok");
    CHECK(now->properties[0] == 1 && now->bank == 6);
    // Not all the items, vehicles, or properties: no award.
    CHECK(now->awards[27] == 0 && now->awards[28] == 0 && now->awards[29] == 0 && now->awards[30] == 0);
    // The trip ends: three purchases give the team 5.
    float team = now->relationTeam;
    ssr_game_message(game, "UpsellMsg.CloseLifestyle");
    CHECK(ssr_game_overlay(game)[0] == 0 && strcmp(ssr_game_screen(game), "relationships") == 0);
    CHECK(now->teamShopping == -1 && now->relationTeam == (team + 5 > 100 ? 100 : team + 5));
    ssr_game_message(game, "relationships.btn_Home");
}

// The skills shop: an agent, a trainer, boots, and a can.
static void hire_and_kit_out(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bank = 200;
    c.starRating = 50;
    c.agent[0] = c.trainer[0] = 0;
    c.bootId = 0;
    c.nrg[0] = 0;
    c.energy = 60;
    // Pace at 60 and power at 59: a save unlocks the award of pace only.
    memset(c.pace, 0, sizeof(c.pace));
    memset(c.power, 0, sizeof(c.power));
    c.pace[0] = 60;
    c.power[0] = 59;
    memset(c.awards, 0, sizeof(c.awards));
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    const SSRCareer *now = ssr_game_career(game);
    ssr_tweaks_set(tweaks, "Products", "StaffOrder", "Agent_1_L|Agent_3|Trainer_1");
    ssr_tweaks_set(tweaks, "Products", "BootOrder", "Boot_10_L|Boot_23|Boot_1|Boot_2");
    ssr_tweaks_set(tweaks, "Products", "NRGOrder", "NRG_7_L|NRG_1|NRG_2");
    ssr_game_message(game, "gamemenu.btn_Shop");
    CHECK(strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Agent_1") == 0);
    SSRUiOp ops[512];
    int count = take_ops(game, ops, 512), buttons = 0;
    for (int i = 0; i < count; i++) buttons += ops[i].kind == SSR_OP_LIST_ADD_AT;
    // The agent of level 3 is a purchase with real money: it does not show.
    CHECK(buttons == 2);
    // The agent costs 5 for each star level and 25.
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bank == 170 && now->agent[0] == 51 && ssr_career_agent(now) == 1);
    CHECK(now->awards[73] > 0 && now->awards[74] == 0);
    CHECK(now->awards[37] > 0 && now->awards[38] == 0 && now->awards[42] == 0);
    CHECK(strcmp(ssr_game_screen(game), "messageagent") == 0 && ssr_game_overlay(game)[0] == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    // A second agent asks first. No keeps the first one.
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(strcmp(ssr_game_screen(game), "messageagent") == 0 && now->bank == 170);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0);
    ssr_tweaks_set(tweaks, "Menu", "ItemString", "Trainer_1");
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bank == 140 && now->trainer[0] == 51 && strcmp(ssr_game_screen(game), "messagetrainer") == 0);
    CHECK(now->awards[74] > 0);
    ssr_game_message(game, "message.btn_Ok");
    ssr_game_message(game, "PurchaseAnim.Close");
    // Boots: the first pair, then a question for a second pair.
    ssr_game_message(game, "shop.tab.boots");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Boot_10") == 0);
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bootId == 10 && now->bootUses == 15 && now->bank == 110);
    // The awards of boots, and of boots with control or swerve.
    CHECK(now->awards[26] > 0 && (now->awards[66] > 0) == (ssr_shop_boot(10)->control > 0));
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Boot_10") == 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    ssr_tweaks_set(tweaks, "Menu", "ItemString", "Boot_1");
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && now->bootId == 10);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(now->bootId == 1 && now->bootUses == 5 && now->bank == 107);
    ssr_game_message(game, "PurchaseAnim.Close");
    // A can of NRG Classic costs 2 at star level 1, and the overlay stays.
    ssr_game_message(game, "shop.tab.nrg");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "NRG_1") == 0);
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->nrg[0] == 1 && now->bank == 105 && strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0);
    CHECK(now->awards[25] > 0 && now->awards[67] == 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    // The energy is 60, so the can goes at once.
    ssr_game_message(game, "UpsellMsg.DrinkNRG");
    CHECK(now->nrg[0] == 0 && now->energy == 100 && ssr_game_overlay(game)[0] == 0);
    // A bank of 10,000 after a change gives an award, a spend too.
    ((SSRCareer *)now)->bank = 10002;
    ssr_game_message(game, "gamemenu.btn_Shop");
    ssr_game_message(game, "shop.tab.nrg");
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bank == 10000 && now->awards[67] > 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    ssr_game_message(game, "UpsellMsg.Close");
    ((SSRCareer *)now)->bank = 105;
    // A match with the agent and the trainer: each counts one match, and
    // the agent adds a tenth of the wage, at least 1.
    int apps = now->apps;
    play_a_match(game, tweaks);
    if (now->apps == apps + 1) {
        CHECK(now->agent[0] == 50 && now->trainer[0] == 50);
        CHECK(ssr_tweaks_number(tweaks, "MatchReview", "EarningsAgentWage") >= 1);
        CHECK(ssr_tweaks_number(tweaks, "Player", "RecoveryBoostTrainer") == 5);
    }
    // The last match of the agent: a message, and Yes opens the shop.
    c = *ssr_game_career(game);
    c.agent[0] = 1;
    c.trainer[0] = 30;
    c.expiredBoots = 1;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "messageagent") == 0 && now->agent[0] == 0 && now->expiredBoots == -1);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_overlay(game), "SkillsShopOverlay") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Agent_1") == 0);
    ssr_game_message(game, "UpsellMsg.Close");
    // Boots that ran out ask for a new pair.
    c = *ssr_game_career(game);
    c.expiredBoots = 1;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && now->expiredBoots == -1);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0 && ssr_game_overlay(game)[0] == 0);
}

// Saves the career with no news done this week, and loads it again.
static SSRCareer *news_week(SSRGame *game, const char *path, SSRCareer *c) {
    c->newsDone = 0;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    for (int i = 0; i < 4 && strcmp(ssr_game_screen(game), "gamemenu") != 0; i++) ssr_game_message(game, "message.btn_Cancel");
    return (SSRCareer *)ssr_game_career(game);
}

// The gambling news: a gambling addict, then the team mates ask the
// player to the casino.
static void gambling_news(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.gambling = 40;
    c.relationPartner = 0;
    c.relationBoss = c.relationFans = 50;
    c.lastTransferDate = c.sdate;
    c.seasonApps = 1;
    c.oldProBoots = 1;
    const SSRCareer *now = NULL;
    for (int i = 0; i < 6; i++) {
        SSRCareer week = c;
        now = news_week(game, path, &week);
        ssr_game_message(game, "gamemenu.btn_Play");
        if (strcmp(ssr_game_screen(game), "webpagenews") == 0) break;
        for (int k = 0; k < 4 && strcmp(ssr_game_screen(game), "gamemenu") != 0; k++) {
            ssr_game_message(game, "message.btn_Ok");
        }
    }
    CHECK(strcmp(ssr_game_screen(game), "webpagenews") == 0);
    SSRUiOp ops[64];
    int count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_TEXT, "MessageText", "CNEWS_GAMBLINGADDICTBOSS"));
    CHECK(now->gambling == 0 && fabsf(now->relationBoss - 30) < 1e-4f && fabsf(now->relationFans - 30) < 1e-4f);
    ssr_game_message(game, "webpage.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    ssr_game_message(game, "relationships.btn_Home");

    // The team mates ask in a week that 4 divides, with no gambling.
    c = *ssr_game_career(game);
    c.gambling = 0;
    c.sdate += (4 - ssr_career_week(&c) % 4) % 4 * 7;
    c.lastTransferDate = c.sdate;
    c.seasonApps = 1;
    c.relationTeam = 50;
    int asked = 0;
    for (int i = 0; i < 40 && !asked; i++) {
        SSRCareer week = c;
        now = news_week(game, path, &week);
        ssr_game_message(game, "gamemenu.btn_Play");
        const char *body = ssr_tweaks_get(tweaks, "Menu", "MessageBody");
        asked = strcmp(ssr_game_screen(game), "messageteam") == 0 && strcmp(body, "CMESSAGE_TEAMMATESCASINO") == 0;
        if (asked) break;
        // No to the shopping costs 5 team.
        if (strcmp(ssr_game_screen(game), "messageteam") == 0) {
            CHECK(strcmp(body, "CMESSAGE_TEAMMATESSHOPPING") == 0);
            ssr_game_message(game, "message.btn_Cancel");
            CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && fabsf(now->relationTeam - 45) < 1e-4f);
            ssr_game_message(game, "relationships.btn_Home");
        }
        for (int k = 0; k < 4 && strcmp(ssr_game_screen(game), "gamemenu") != 0; k++) {
            ssr_game_message(game, "message.btn_Ok");
        }
    }
    CHECK(asked);
    // Yes: the casino with the team mates. One bet gives the team 1.
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "casino") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Casino", "TeammatesBetCount") == 0);
    ssr_game_message(game, "casino.btn_Slots");
    ssr_game_message(game, "slots.btn_Play");
    CHECK(ssr_tweaks_number(tweaks, "Casino", "TeammatesBetCount") == 1);
    for (int f = 0; f < 4 * 60 && ssr_tweaks_number(tweaks, "Menu", "HomeButtonDisabled") == 1; f++) {
        ssr_game_update(game, 1);
    }
    ssr_game_message(game, "slots.btn_Quit");
    ssr_game_message(game, "casino.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0 && fabsf(now->relationTeam - 51) < 1e-4f);
    CHECK(ssr_tweaks_number(tweaks, "Casino", "TeammatesBetCount") == -1);
    ssr_game_message(game, "relationships.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// The club makes the player captain: a good season, 25 matches for the
// club, and good relations. The armband gives its award, and one more at
// 18 or younger (CheckCaptaincy).
static void become_captain(SSRGame *game, const char *path) {
    static SSRCareer before;
    before = *ssr_game_career(game);
    SSRCareer c = before;
    c.captain = 0;
    c.seasonApps = 10;
    c.apps = 30;
    c.seasonTotalRating = 85;
    c.starRating = 9900;
    c.relationBoss = c.relationTeam = c.relationFans = 90;
    c.awards[80] = c.awards[119] = 0;
    // The news before it in the chain: the old boots came, and the
    // transfer window just closed.
    c.oldProBoots = 21;
    c.lastTransferDate = c.sdate;
    // Other news can take the week. The test tries again.
    const SSRCareer *now = NULL;
    for (int i = 0; i < 4; i++) {
        SSRCareer week = c;
        now = news_week(game, path, &week);
        ssr_game_message(game, "gamemenu.btn_Play");
        if (now->captain == 1) break;
        for (int k = 0; k < 4 && strcmp(ssr_game_screen(game), "gamemenu") != 0; k++) {
            if (strcmp(ssr_game_screen(game), "contract") == 0) ssr_game_message(game, "contract.btn_Reject");
            else if (strncmp(ssr_game_screen(game), "webpage", 7) == 0) ssr_game_message(game, "webpage.btn_Ok");
            else ssr_game_message(game, "message.btn_Ok");
        }
    }
    CHECK(now && now->captain == 1 && strcmp(ssr_game_screen(game), "messageboss") == 0);
    CHECK(now->awards[80] > 0 && (now->awards[119] > 0) == (ssr_career_age(now) <= 18));
    ssr_game_message(game, "message.btn_Ok");
    // The other tests go on with the career from before.
    news_week(game, path, &before);
}

// The news before a match: the old boots of an ex pro, then a sponsor.
static void get_old_boots_and_a_sponsor(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.bootId = 0;
    c.oldProBoots = 0;
    c.apps = 9;
    const SSRCareer *now = news_week(game, path, &c);
    ssr_game_message(game, "gamemenu.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && now->oldProBoots == 21 && now->newsDone == 1);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(now->bootId == 21 && now->bootUses == 5 && strcmp(ssr_game_modal(game), "PurchaseAnim") == 0);
    ssr_game_message(game, "PurchaseAnim.Close");
    // A player of star level 3 with no sponsors gets the first offer.
    c = *ssr_game_career(game);
    c.starRating = 260;
    c.seasonApps = 0;
    c.relationSponsors = 0;
    for (int i = 0; i < 10; i++) c.sponsors[i] = 0;
    c.captain = 0;
    c.relationFans = 50;
    // A transfer offer in the window comes first, so the test has one just
    // now.
    c.lastTransferDate = c.sdate;
    // News that comes before it in the chain, as the transfer window, can
    // take the week. The test tries again.
    for (int i = 0; i < 4; i++) {
        SSRCareer week = c;
        now = news_week(game, path, &week);
        ssr_game_message(game, "gamemenu.btn_Play");
        if (strcmp(ssr_game_screen(game), "messagesponsor") == 0) break;
        for (int k = 0; k < 4 && strcmp(ssr_game_screen(game), "gamemenu") != 0; k++) {
            if (strcmp(ssr_game_screen(game), "contract") == 0) ssr_game_message(game, "contract.btn_Reject");
            else if (strncmp(ssr_game_screen(game), "webpage", 7) == 0) ssr_game_message(game, "webpage.btn_Ok");
            else ssr_game_message(game, "message.btn_Ok");
        }
    }
    CHECK(strcmp(ssr_game_screen(game), "messagesponsor") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(now->sponsors[0] == 1 && now->relationSponsors == 50 && strcmp(ssr_game_screen(game), "relationships") == 0);
    CHECK(now->awards[35] > 0 && now->awards[36] == 0);
    ssr_game_message(game, "relationships.btn_Home");
    (void)tweaks;
}

// The talk with the boss of the club: the contract, a turn of the
// negotiate game, and the signature.
static void renew_the_contract(SSRGame *game, SSRTweaks *tweaks, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.starRating = 400;
    c.relationBoss = c.relationTeam = c.relationFans = 80;
    c.lastNegotiations = 0;
    c.wage = 1;
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(ssr_career_save(&c, path));
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    if (strcmp(ssr_game_screen(game), "gamemenu") != 0) ssr_game_message(game, "message.btn_Cancel");
    const SSRCareer *now = ssr_game_career(game);
    ssr_game_message(game, "gamemenu.btn_Contract");
    CHECK(strcmp(ssr_game_screen(game), "contract") == 0 && ssr_tweaks_number(tweaks, "Negotiation", "IsNewClub") == 0);
    int wage = (int)ssr_tweaks_number(tweaks, "Negotiation", "Wage");
    CHECK(wage > 1 && ssr_tweaks_number(tweaks, "Negotiation", "WageDiff") == wage - 1);
    ssr_game_message(game, "contract.btn_Negotiate");
    CHECK(strcmp(ssr_game_screen(game), "negotiate") == 0);
    // The guess of the second shirt, after the splash.
    int first = (int)ssr_tweaks_number(tweaks, "Negotiation", "Card1");
    CHECK(first >= 1 && first <= 11);
    ssr_game_update(game, 100);
    ssr_game_message(game, first <= 6 ? "negotiate.btn_Higher" : "negotiate.btn_Lower");
    int second = (int)ssr_tweaks_number(tweaks, "Negotiation", "Card2");
    CHECK(second >= 1 && second <= 11 && second != first);
    ssr_game_update(game, 60 * 4);
    int right = first <= 6 ? second > first : second < first;
    if (!right) {
        // A wrong guess goes back to the contract with the same offer.
        CHECK(strcmp(ssr_game_screen(game), "contract") == 0);
        CHECK(ssr_tweaks_number(tweaks, "Negotiation", "Wage") == wage);
    } else {
        CHECK(strcmp(ssr_game_screen(game), "negotiate") == 0);
        ssr_game_message(game, "negotiate.btn_Accept");
        CHECK(strcmp(ssr_game_screen(game), "contract") == 0);
    }
    wage = (int)ssr_tweaks_number(tweaks, "Negotiation", "Wage");
    ssr_game_message(game, "contract.btn_Accept");
    CHECK(now->wage == wage && now->lastNegotiations == now->sdate && strcmp(ssr_game_screen(game), "message") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "webpagenews") == 0);
    ssr_game_message(game, "webpage.btn_Ok");
    // A second talk is too soon.
    ssr_game_message(game, "gamemenu.btn_Contract");
    CHECK(strcmp(ssr_game_screen(game), "messageboss") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Drinks the next can after a match: a question when the energy is above
// 75, then the boost screen of a skill can, and full energy at the end.
static void drink_a_can(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    CHECK(c->energy < 100);
    int cans = 0;
    for (int i = 0; i < 5; i++) cans += c->nrg[i];
    int skills = c->nrg[0] == 0 && c->nrg[1] > 0;
    int pace = c->pace[0];
    float energy = c->energy;
    ssr_game_message(game, "gamemenu.btn_DrinkNRG");
    if (energy > 75) {
        CHECK(strcmp(ssr_game_screen(game), "message") == 0);
        ssr_game_message(game, "message.btn_Ok");
    }
    int after = 0;
    for (int i = 0; i < 5; i++) after += c->nrg[i];
    CHECK(after == cans - 1);
    if (skills) {
        CHECK(strcmp(ssr_game_screen(game), "chooseboost") == 0);
        ssr_game_message(game, "chooseboost.btn_1");
        // One star goes to the first drill of Pace.
        CHECK(c->pace[0] == pace + 1);
        CHECK(ssr_tweaks_number(tweaks, "Player", "Pace") >= 1);
        // A second choice does nothing: one can, one star.
        int power = ssr_career_skill(c->power);
        ssr_game_message(game, "chooseboost.btn_2");
        CHECK(ssr_career_skill(c->power) == power);
        ssr_game_message(game, "chooseboost.btn_Ok");
    }
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0 && c->energy == 100);
    // Full energy: a message, and no can goes.
    ssr_game_message(game, "gamemenu.btn_DrinkNRG");
    int full = 0;
    for (int i = 0; i < 5; i++) full += c->nrg[i];
    CHECK(full == after);
    if (after > 0) {
        CHECK(strcmp(ssr_game_screen(game), "messagedefault") == 0);
        ssr_game_message(game, "message.btn_Ok");
    }
}

static void test_starts_a_new_career(void) {
    SSRContinent continents[1] = {{6, "Europe", "EUR", "UEFA", "UEFA", 1}};
    SSRNation nations[2] = {0};
    nations[0].id = 62;
    nations[0].name = "England";
    nations[0].tla = "ENG";
    nations[0].continent = 6;
    nations[1].id = 63;
    nations[1].name = "Wales";
    nations[1].tla = "WAL";
    nations[1].continent = 6;
    SSRCompetition competitions[1] = {0};
    competitions[0].id = 2270;
    competitions[0].tla = "PRM";
    competitions[0].based = 62;
    competitions[0].duration = 38;
    // A national league of four clubs that meet two times, on Saturdays
    // from week 6.
    competitions[0].startYear = 1;
    competitions[0].startWeek = 6;
    competitions[0].primaryMatchday = 6;
    competitions[0].secondaryMatchday = 99;
    competitions[0].groups = 1;
    competitions[0].rounds = 2;
    competitions[0].status = 1;
    SSRClub clubs[4] = {0};
    const char *names[4] = {"Big FC", "Mid FC", "Small FC", "Tiny FC"};
    const char *tlas[4] = {"BIG", "MID", "SML", "TNY"};
    for (int i = 0; i < 4; i++) {
        clubs[i].id = 100 + i;
        clubs[i].name = names[i];
        clubs[i].tla = tlas[i];
        clubs[i].nation = 62;
        clubs[i].league = 2270;
        clubs[i].strength = 90 - i * 20;
    }
    SSRWorld world = {0};
    world.continents = continents;
    world.continentCount = 1;
    world.nations = nations;
    world.nationCount = 2;
    world.competitions = competitions;
    world.competitionCount = 1;
    world.clubs = clubs;
    world.clubCount = 4;

    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Player", "StartingWage", 1);
    ssr_tweaks_set_number(tweaks, "Player", "StartingGoalBonus", 1);
    ssr_tweaks_set_number(tweaks, "Menu", "ReplaysEnabled", 1);
    ssr_tweaks_set_number(tweaks, "SplashMessage", "DelayWatchReplay", 25);
    const char *directory = ssr_test_directory("game_career");
    char path[1200];
    snprintf(path, sizeof(path), "%s/people1.txt", directory);
    remove(path);
    snprintf(path, sizeof(path), "%s/honours1.txt", directory);
    remove(path);
    snprintf(path, sizeof(path), "%s/career1.txt", directory);
    remove(path);
    careerPath = path;
    SSRGame *game = ssr_game_create(tweaks);
    ssr_game_set_data(game, &world, localise, NULL, directory);
    // This career checks the rules of 4.32: the classic ratings and scores.
    CHECK(ssr_game_settings(game)->classicRealism == 0);
    ssr_game_message(game, "packs.btn_Realism");
    CHECK(ssr_game_settings(game)->classicRealism == 1);
    // The names of 210 race horses, after a header line.
    static char horses[4096];
    size_t used = (size_t)snprintf(horses, sizeof(horses), "Header\r\n");
    for (int i = 1; i <= 210; i++) used += (size_t)snprintf(horses + used, sizeof(horses) - used, "Horse %03d\r\n", i);
    ssr_game_set_horse_names(game, horses);
    ssr_game_set_names(game, "first test Ann Bob Cid\nlast test Lake Hill Moor\n");
    ssr_game_set_world_seed(game, 99);
    SSRUiOp ops[64];

    ssr_game_message(game, "mainmenu.btn_Career");
    CHECK(strcmp(ssr_game_screen(game), "saveslot") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "SaveSlot", "Slot0Data"), "Save Slot 1") == 0);
    CHECK(ssr_tweaks_number(tweaks, "SaveSlot", "Slot0Exist") == 0);
    ssr_game_message(game, "saveslot.btn_Slot0");
    CHECK(strcmp(ssr_game_screen(game), "messagedefault") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "Welcome.") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Message", "ShowAcc") == 1 && ssr_tweaks_number(tweaks, "Message", "ShowDec") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "chooseclub") == 0);

    // The name boxes.
    CHECK(strcmp(ssr_game_text(game, "GS_ShowNameF"), "FIRSTNAME_ENTRY_INSTRUCTIONS") == 0);
    CHECK(!ssr_game_wants_text(game));
    ssr_game_message(game, "chooseclub.inp_NameF_HIT");
    CHECK(ssr_game_wants_text(game) && ssr_tweaks_number(tweaks, "Menu", "FocusedItem") == 1);
    ssr_game_type(game, "billy,;");
    ssr_game_type(game, "x");
    ssr_game_key(game, SSR_KEY_BACKSPACE);
    CHECK(strcmp(ssr_game_text(game, "GS_ShowNameF"), "Billy") == 0);
    ssr_game_key(game, SSR_KEY_ENTER);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "FocusedItem") == 2);
    ssr_game_type(game, "Abcdefghijklmnop");
    CHECK(strcmp(ssr_game_text(game, "GS_ShowNameL"), "Abcdefghijk") == 0);
    ssr_game_message(game, "chooseclub.btn_Gender");
    CHECK(ssr_tweaks_number(tweaks, "Player", "Gender") == 1 && !ssr_game_wants_text(game));
    ssr_game_message(game, "chooseclub.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "choosenation") == 0);

    // The lists: Europe, England and Wales, and England as a league.
    int count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_LIST_ADD, "ContinentCombo", "EUROPE"));
    CHECK(has_op(ops, count, SSR_OP_LIST_ADD, "NationCombo", "WALES"));
    CHECK(has_op(ops, count, SSR_OP_LIST_ADD, "LeagueCombo", "ENGLAND"));
    CHECK(!has_op(ops, count, SSR_OP_LIST_ADD, "LeagueCombo", "WALES"));
    CHECK(has_op(ops, count, SSR_OP_TEXT, "NationText", "England"));
    CHECK(has_op(ops, count, SSR_OP_SHOW, "PlayButton", NULL));
    ssr_tweaks_set_number(tweaks, "Combo", "ChosenDesiredNation", 63);
    ssr_game_message(game, "choosenation.cmb_Nation");
    count = take_ops(game, ops, 64);
    CHECK(has_op(ops, count, SSR_OP_TEXT, "NationText", "Wales"));
    // A language with the tag Tag_NationId starts on its nation.
    nationTag = "63";
    ssr_game_message(game, "chooseclub.btn_Ok");
    CHECK(ssr_tweaks_number(tweaks, "Combo", "ChosenDesiredNation") == 63);
    nationTag = "0";
    ssr_game_message(game, "chooseclub.btn_Ok");
    CHECK(ssr_tweaks_number(tweaks, "Combo", "ChosenDesiredNation") == 62);
    nationTag = NULL;
    ssr_tweaks_set_number(tweaks, "Combo", "ChosenDesiredNation", 63);
    ssr_game_message(game, "choosenation.cmb_Nation");
    take_ops(game, ops, 64);

    ssr_game_message(game, "choosenation.btn_Ok");
    // The custom career starts as a career of the game.
    CHECK(strcmp(ssr_game_screen(game), "customcareer") == 0);
    CHECK(strcmp(ssr_game_text(game, "GS_CustomAge"), "16") == 0);
    CHECK(strcmp(ssr_game_text(game, "GS_CustomSkill"), "SSR_CustomSkillTrials") == 0);
    ssr_game_message(game, "customcareer.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "messagedefault") == 0);
    const SSRCareer *career = ssr_game_career(game);
    CHECK(career->nationId == 63 && career->clubNationId == 62 && career->gender == 1);
    CHECK(career->clubId >= 100 && career->clubId <= 103 && career->wage == 1);
    fail_the_trials(game, tweaks);
    // After a failed last trial the text shows in the plain message box.
    CHECK(strcmp(ssr_game_screen(game), "message") == 0);
    CHECK(strstr(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "in PRM.") != NULL);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "startransition") == 0);
    CHECK(career->technique[0] == 1 && career->power[0] == 1);
    // The date is the day before the first match: Saturday of week 6.
    CHECK(career->sdate == 5 * 7 + 6 - 1);
    char seasonPath[1200];
    snprintf(seasonPath, sizeof(seasonPath), "%s/season1.txt", directory);
    FILE *seasonFile = fopen(seasonPath, "r");
    CHECK(seasonFile != NULL);
    if (seasonFile) fclose(seasonFile);
    // The header of the news page shows the player already.
    CHECK(ssr_tweaks_number(tweaks, "Player", "Age") == 16 && ssr_tweaks_number(tweaks, "Player", "Bux") == 15);
    CHECK(ssr_tweaks_number(tweaks, "Player", "StarBux") == 15);
    // The bars start at the values of the career, with no animation.
    CHECK(ssr_tweaks_number(tweaks, "Player", "BossRelationship_Displayed") == career->relationBoss);
    CHECK(ssr_tweaks_number(tweaks, "Player", "BossRelationship_Before") == career->relationBoss);
    ssr_game_update(game, 30);
    CHECK(strcmp(ssr_game_screen(game), "startransition") == 0);
    ssr_game_update(game, 30);
    CHECK(strcmp(ssr_game_screen(game), "webpagenews") == 0);
    count = take_ops(game, ops, 64);
    CHECK(count == 1 && ops[0].kind == SSR_OP_TEXT && strstr(ops[0].text, "have signed Billy Abcdefghijk."));
    ssr_game_message(game, "webpage.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Player", "Bux") == 15 && ssr_tweaks_number(tweaks, "Player", "Age") == 16);
    CHECK(strcmp(ssr_game_text(game, "GS_PlayerClubName"), world.clubs[career->clubId - 100].tla) == 0);
    // The skills bar: the sum of the five skills / 3.
    float skills = (ssr_career_skill(career->pace) + ssr_career_skill(career->power) +
                    ssr_career_skill(career->technique) + ssr_career_skill(career->vision) +
                    ssr_career_skill(career->setPieces)) / 3.0f;
    CHECK(skills > 0 && fabs(ssr_tweaks_number(tweaks, "Player", "Skills") - skills) < 1e-4);
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);

    // The slot shows the career, and it loads again.
    ssr_game_message(game, "mainmenu.btn_Career");
    CHECK(ssr_tweaks_number(tweaks, "SaveSlot", "Slot0Exist") == 1);
    CHECK(strstr(ssr_tweaks_get(tweaks, "SaveSlot", "Slot0Data"), "Billy Abcdefghijk - ") != NULL);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "SaveSlot", "Slot0ButtonText"), "Load Career") == 0);
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    // The first can gives energy. After a second match, the next can is a
    // skill boost.
    play_a_match(game, tweaks);
    tire(game, path);
    drink_a_can(game, tweaks);
    play_a_match(game, tweaks);
    tire(game, path);
    drink_a_can(game, tweaks);
    // A star scores: the test watches the replay of a goal.
    make_a_star(game, path);
    for (int i = 0; i < 40 && replays == 0; i++) {
        play_a_match(game, tweaks);
        make_a_star(game, path);
    }
    CHECK(replays > 0);
    not_picked_by_the_boss(game, tweaks, path);
    // A match starts the activities again, and each sponsor takes 0.65. The
    // news of a cup winner shows on the results, once.
    busy(game, path);
    CHECK(winnerPages == 0);
    play_a_match(game, tweaks);
    CHECK(ssr_game_career(game)->spentTime == 0 && fabsf(ssr_game_career(game)->relationSponsors - 48.7f) < 1e-4f);
    CHECK(winnerPages == 1);
    spend_time(game, tweaks, path);
    choose_a_dilemma(game, path);
    an_accident_costs_a_skill(game, path);
    look_at_achievements(game, tweaks);
    lose_a_skill_with_no_training(game, tweaks, path);
    read_the_tickers(game, tweaks, path);
    go_to_the_casino(game, tweaks, path);
    play_roulette(game, tweaks, path);
    play_blackjack(game, tweaks, path);
    go_to_the_races(game, tweaks, path);
    go_shopping(game, tweaks, path);
    hire_and_kit_out(game, tweaks, path);
    get_old_boots_and_a_sponsor(game, tweaks, path);
    become_captain(game, path);
    renew_the_contract(game, tweaks, path);
    // Six rounds make a season: the career played to the next one.
    for (int i = 0; i < 24 && seasons < 2; i++) play_a_match(game, tweaks);
    CHECK(seasons >= 2 && transfers == 1);
    // The offer of the old boss comes once at most.
    CHECK(exBossOffers <= 1);
    // The stories of the news: a crisis, a rumour, and a record. A story
    // that shows has all its names and numbers.
    int stories[3] = {SSR_TICKER_WORLD_CRISIS, SSR_TICKER_WORLD_RUMOUR, SSR_TICKER_WORLD_RECORD};
    for (int k = 0; k < 3; k++) {
        int storyCount = 0;
        char story[256] = "";
        if (ssr_game_ticker_priority(game, stories[k], &storyCount) < 0) continue;
        ssr_game_ticker_text(game, stories[k], &storyCount, story, sizeof(story));
        CHECK(story[0] && strchr(story, '$') == NULL);
    }
    // The world has players: the clubs of the matches have squads that
    // played and scored, and three rivals of the same age.
    SSRPeople *people = ssr_game_people(game);
    CHECK(people && people->seed == 99 && ssr_game_career(game)->worldSeed == 99);
    int rivals = 0, squads = 0, apps = 0, goals = 0;
    for (int i = 0; people && i < people->count; i++) {
        const SSRPerson *p = &people->list[i];
        rivals += p->rival;
        apps += p->apps;
        goals += p->goals;
        squads += p->club == ssr_game_career(game)->clubId && !p->retired;
        CHECK(p->first[0] && p->last[0]);
    }
    CHECK(rivals == 3 && squads >= SSR_SQUAD_SIZE && apps > 0 && goals > 0);
    // The race for the title made a rivalry of the first two clubs.
    const SSRHonours *honours = ssr_game_honours(game);
    CHECK(honours->count >= 2 && honours->rivalryCount >= 1 && honours->rivalries[0].points >= 2);
    // An injured player is not picked, and the injury heals with the days.
    ((SSRCareer *)ssr_game_career(game))->injury = 2;
    int notPickedBefore = notPicked;
    play_a_match(game, tweaks);
    CHECK(painAsked == 1 && notPicked == notPickedBefore + 1 && ssr_game_career(game)->injury == 0);
    const SSRCareer *healed = ssr_game_career(game);
    CHECK(healed->momentCount > 0 && strcmp(healed->moments[healed->momentCount - 1].text, "SSR_MomentFit") == 0);
    // Each club of the league has a manager.
    CHECK(people && people->managerCount == 4 && ssr_people_manager(people, ssr_game_career(game)->clubId));
    char peoplePath[1200];
    snprintf(peoplePath, sizeof(peoplePath), "%s/people1.txt", directory);
    SSRPeople *saved = ssr_people_load(peoplePath);
    CHECK(saved && saved->count > 0);
    ssr_people_free(saved);
    // The weeks had news: a dilemma or other news, and a tip in a week
    // with no other news.
    for (int i = 0; i < 40 && ssr_game_career(game)->tipCount == 0; i++) play_a_match(game, tweaks);
    CHECK(newsSeen > 0 && ssr_game_career(game)->tipCount > 0);
    // In a season, the ticker tells of a rival and of the top scorer of the league.
    int turns = 0;
    char news[256];
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_WORLD_RIVAL, &turns) > 0);
    ssr_game_ticker_text(game, SSR_TICKER_WORLD_RIVAL, &turns, news, sizeof(news));
    CHECK(strncmp(news, "Rival ", 6) == 0 && strchr(news, '$') == NULL);
    CHECK(strstr(news, "Ann ") || strstr(news, "Bob ") || strstr(news, "Cid "));
    turns = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_WORLD_SCORER, &turns) > 0);
    turns = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_WORLD_PARTNER, &turns) > 0);
    ssr_game_ticker_text(game, SSR_TICKER_WORLD_PARTNER, &turns, news, sizeof(news));
    CHECK(strcmp(news, "SSR_TICKER_PARTNER") == 0);
    turns = 0;
    CHECK(ssr_game_ticker_priority(game, SSR_TICKER_WORLD_TABLE, &turns) > 0);
    ssr_game_ticker_text(game, SSR_TICKER_WORLD_TABLE, &turns, news, sizeof(news));
    CHECK(strncmp(news, "SSR_TICKER_TITLE", 16) == 0);
    // The honours keep the champion of each past season of the league.
    ssr_game_message(game, "gamemenu.btn_League");
    ssr_game_message(game, "league.btn_Squad");
    ssr_game_message(game, "squad.btn_Honours");
    CHECK(strcmp(ssr_game_screen(game), "honours") == 0);
    SSRUiOp honourOps[256];
    int honourCount = take_ops(game, honourOps, 256), honourRows = 0;
    for (int i = 0; i < honourCount; i++) {
        honourRows += honourOps[i].kind == SSR_OP_LIST_ADD && strcmp(honourOps[i].ref, "HonourList") == 0;
    }
    CHECK(honourRows >= 2);
    // The timeline of the career: the first club, the debut, and more.
    ssr_game_message(game, "honours.btn_Mode");
    honourCount = take_ops(game, honourOps, 256);
    int momentRows = 0;
    for (int i = 0; i < honourCount; i++) {
        momentRows += honourOps[i].kind == SSR_OP_LIST_ADD && strcmp(honourOps[i].ref, "HonourList") == 0;
    }
    CHECK(momentRows >= 3 && momentRows == ssr_game_career(game)->momentCount);
    // The hall of fame: the scorers of the world and the player.
    ssr_game_message(game, "honours.btn_Mode");
    honourCount = take_ops(game, honourOps, 256);
    int fameRows = 0;
    for (int i = 0; i < honourCount; i++) {
        fameRows += honourOps[i].kind == SSR_OP_LIST_ADD && strcmp(honourOps[i].ref, "HonourList") == 0;
    }
    CHECK(fameRows >= 2 && fameRows <= 30);
    // The records: the titles of each league with a champion, the club and
    // the nation of the player, and the most goals of a career.
    ssr_game_message(game, "honours.btn_Mode");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Honours", "Title"), "SSR_Records") == 0);
    honourCount = take_ops(game, honourOps, 256);
    CHECK(has_op(honourOps, honourCount, SSR_OP_LIST_TEXT, "HonourList", "SSR_RecordLeagueTitles"));
    CHECK(has_op(honourOps, honourCount, SSR_OP_LIST_TEXT, "HonourList", "SSR_RecordSeasonGoals"));
    CHECK(has_op(honourOps, honourCount, SSR_OP_LIST_TEXT, "HonourList", "SSR_RecordClubTitles"));
    CHECK(has_op(honourOps, honourCount, SSR_OP_LIST_TEXT, "HonourList", "SSR_RecordNationTitles"));
    CHECK(has_op(honourOps, honourCount, SSR_OP_LIST_TEXT, "HonourList", "SSR_RecordWorldGoals"));
    ssr_game_message(game, "honours.btn_Mode");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Honours", "Title"), "SSR_Honours") == 0);
    ssr_game_message(game, "honours.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "squad") == 0);
    ssr_game_message(game, "squad.btn_Back");
    ssr_game_message(game, "league.btn_Home");
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    CHECK(interviews >= 2 && interviewsWon >= 1);
    // A pass of the first match is a record of the career.
    CHECK(records >= 1 && squadSeen && moments == 1);
    gambling_news(game, tweaks, path);
    // The player retires at the end of the next season.
    retireNow = 1;
    for (int i = 0; i < 24 && retireNow == 1; i++) play_a_match(game, tweaks);
    CHECK(retireNow == 2);
    // A second quit shows no message.
    ssr_game_message(game, "gamemenu.btn_Quit");
    CHECK(strcmp(ssr_game_screen(game), "mainmenu") == 0);

    // Delete: Cancel keeps it, Ok deletes it.
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_DeleteSlot0");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_screen(game), "saveslot") == 0 && ssr_tweaks_number(tweaks, "SaveSlot", "Slot0Exist") == 1);
    ssr_game_message(game, "saveslot.btn_DeleteSlot0");
    ssr_game_message(game, "message.btn_Ok");
    CHECK(ssr_tweaks_number(tweaks, "SaveSlot", "Slot0Exist") == 0);
    seasonFile = fopen(seasonPath, "r");
    CHECK(seasonFile == NULL);
    if (seasonFile) fclose(seasonFile);
    CHECK(ssr_people_load(peoplePath) == NULL);

    // A custom career: the player starts at 20, with 30 in each skill.
    ssr_game_message(game, "saveslot.btn_Slot0");
    ssr_game_message(game, "message.btn_Ok");
    ssr_game_message(game, "chooseclub.btn_Ok");
    ssr_game_message(game, "chooseclub.btn_Ok");
    ssr_game_message(game, "choosenation.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "customcareer") == 0);
    // The values go round at each end.
    ssr_game_message(game, "customcareer.btn_AgeDown");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomAge"), "32") == 0);
    ssr_game_message(game, "customcareer.btn_Age");
    for (int i = 0; i < 4; i++) ssr_game_message(game, "customcareer.btn_Age");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomAge"), "20") == 0);
    ssr_game_message(game, "customcareer.btn_SkillDown");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomSkill"), "70") == 0);
    ssr_game_message(game, "customcareer.btn_Skill");
    for (int i = 0; i < 3; i++) ssr_game_message(game, "customcareer.btn_Skill");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomSkill"), "30") == 0);
    // A striker at the strongest club of the league, with 100 Bux.
    CHECK(strcmp(ssr_game_text(game, "GS_CustomPosition"), "SSR_PosForward") == 0);
    ssr_game_message(game, "customcareer.btn_PositionDown");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomPosition"), "SSR_PosHolding") == 0);
    ssr_game_message(game, "customcareer.btn_Position");
    ssr_game_message(game, "customcareer.btn_Position");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomPosition"), "SSR_PosStriker") == 0);
    CHECK(strcmp(ssr_game_text(game, "GS_CustomClub"), "SSR_CustomSkillTrials") == 0);
    ssr_game_message(game, "customcareer.btn_ClubDown");
    ssr_game_message(game, "customcareer.btn_Club");
    ssr_game_message(game, "customcareer.btn_Club");
    int strongest = 0;
    for (int i = 0; i < world.clubCount; i++) {
        if (!strongest || world.clubs[i].strength > ssr_world_club(&world, strongest)->strength) strongest = world.clubs[i].id;
    }
    CHECK(strcmp(ssr_game_text(game, "GS_CustomClub"), ssr_world_club(&world, strongest)->name) == 0);
    ssr_game_message(game, "customcareer.btn_BuxDown");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomBux"), "2000") == 0);
    ssr_game_message(game, "customcareer.btn_Bux");
    ssr_game_message(game, "customcareer.btn_Bux");
    CHECK(strcmp(ssr_game_text(game, "GS_CustomBux"), "100") == 0);
    ssr_game_message(game, "customcareer.btn_Ok");
    fail_the_trials(game, tweaks);
    ssr_game_message(game, "message.btn_Ok");
    career = ssr_game_career(game);
    CHECK(strcmp(ssr_game_screen(game), "startransition") == 0 && career->startAge == 20);
    CHECK(career->position == SSR_CAREER_STRIKER && career->clubId == strongest && career->bank >= 100);
    CHECK(ssr_career_age(career) == 20 && ssr_tweaks_number(tweaks, "Player", "Age") == 20);
    CHECK(ssr_career_skill(career->technique) >= 30 && ssr_career_skill(career->power) >= 30);
    CHECK(ssr_career_skill(career->pace) >= 30 && ssr_career_skill(career->vision) >= 30);
    CHECK(ssr_career_skill(career->setPieces) >= 30);
    // Contract drama: a young player with few matches goes on loan to the
    // one club that is 8 to 30 points weaker, and comes back after a season.
    ssr_game_update(game, 60);
    ssr_game_message(game, "playeroftheyear.btn_Play");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_LoanOffer") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(career->loanFrom == strongest && career->clubId == 101);
    ssr_game_update(game, 60);
    ssr_game_message(game, "playeroftheyear.btn_Play");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_LoanEnds") == 0);
    CHECK(career->loanFrom == 0 && career->clubId == strongest);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_SEASONENDTRANSFER") == 0);
    // A key player: the club refuses to sell, and the player has not the
    // Bux of the release clause.
    SSRCareer *key = (SSRCareer *)career;
    key->seasonApps = 12;
    key->seasonTotalRating = 12 * 8.0f;
    key->relationBoss = 80;
    key->bank = 0;
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_RefuseToSell") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_ClauseNoBux") == 0);
    ssr_game_message(game, "message.btn_Ok");
    ssr_game_update(game, 60);
    // Charity: a rich player gives a fifth of the Bux, and the fans like it.
    key->bank = 1000;
    key->seasonApps = 7;
    key->newsDone = 0;
    float fansBefore = key->relationFans;
    int found = 0;
    for (int i = 0; i < 6 && !found; i++) {
        ssr_game_message(game, "gamemenu.btn_Play");
        found = strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_CharityAsk") == 0;
        if (!found) ssr_game_message(game, "message.btn_Cancel");
    }
    CHECK(found);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(key->bank == 800 && key->relationFans > fansBefore && key->charityYear == ssr_career_year(key));
    // Business: a quarter of the Bux buys a share that pays at the end of
    // each season, from a loss of a fifth to a profit of two fifths.
    key->bank = 4000;
    key->seasonApps = 15;
    key->newsDone = 0;
    found = 0;
    for (int i = 0; i < 6 && !found; i++) {
        ssr_game_message(game, "gamemenu.btn_Play");
        found = strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "SSR_BusinessOffer") == 0;
        if (!found) ssr_game_message(game, "message.btn_Cancel");
    }
    CHECK(found);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(key->bank == 3000 && key->businessStake == 1000);
    ssr_game_message(game, "playeroftheyear.btn_Play");
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(key->bank >= 2800 && key->bank <= 3400 && key->businessStake == 1000);
    // A second career continues the world of the first. The first player
    // did not retire, so the player plays on at the club.
    char firstName[64];
    snprintf(firstName, sizeof(firstName), "%s", career->firstName);
    int firstClub = career->clubId;
    ssr_game_message(game, "gamemenu.btn_Quit");
    ssr_game_message(game, "packs.btn_Continue");
    CHECK(ssr_game_settings(game)->continueWorld == 1);
    ssr_game_message(game, "mainmenu.btn_Career");
    ssr_game_message(game, "saveslot.btn_Slot1");
    ssr_game_message(game, "message.btn_Ok");
    ssr_game_message(game, "chooseclub.btn_Ok");
    ssr_game_message(game, "chooseclub.btn_Ok");
    ssr_game_message(game, "choosenation.btn_Ok");
    ssr_game_message(game, "customcareer.btn_Ok");
    fail_the_trials(game, tweaks);
    ssr_game_message(game, "message.btn_Ok");
    SSRPeople *world2 = ssr_game_people(game);
    int formers = 0;
    for (int i = 0; world2 && i < world2->count; i++) {
        const SSRPerson *p = &world2->list[i];
        if (p->rival == SSR_FORMER_PLAYER) formers += !p->retired && p->club == firstClub && strcmp(p->first, firstName) == 0;
    }
    CHECK(formers == 1);
    career = ssr_game_career(game);
    CHECK(career->momentCount >= 1 && strcmp(career->moments[0].text, "SSR_MomentFormer") == 0);
    ssr_game_free(game);
    ssr_tweaks_free(tweaks);
}

// The packs screen lists the world of the game and the installed packs. A
// tap chooses a pack, and Remove removes it.
static void test_chooses_a_pack(void) {
    char root[1024], archive[1200], packs[1200];
    snprintf(root, sizeof(root), "%s", ssr_test_directory("game_packs"));
    snprintf(archive, sizeof(archive), "%s/harbour.zip", root);
    snprintf(packs, sizeof(packs), "%s/packs", root);
    static const char INFO[] = "id=harbour\nname=Harbour\nauthor=Ann\nversion=2\nformat=1\n";
    SSRZipFile files[] = {{"pack.txt", INFO, sizeof(INFO) - 1, 0}};
    CHECK(ssr_zipwriter_write(archive, files, 1) && ssr_pack_install(archive, packs, NULL));
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRGame *game = ssr_game_create(tweaks);
    ssr_game_set_packs_directory(game, packs, 1);
    ssr_game_message(game, "options.btn_Packs");
    CHECK(strcmp(ssr_game_screen(game), "packs") == 0 && ssr_tweaks_number(tweaks, "Packs", "CanInstall") == 1);
    SSRUiOp ops[64];
    int count = take_ops(game, ops, 64), rows = 0;
    for (int i = 0; i < count; i++) rows += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "PackList") == 0;
    CHECK(rows == 2 && has_op(ops, count, SSR_OP_LIST_TEXT, "PackList", "Harbour"));
    CHECK(ssr_tweaks_number(tweaks, "Packs", "CanRemove") == 0);
    ssr_game_message(game, "packs.row_pack 1");
    CHECK(strcmp(ssr_game_settings(game)->pack, "harbour") == 0 && ssr_game_take_settings_change(game));
    CHECK(ssr_game_take_pack_change(game) && !ssr_game_take_pack_change(game));
    CHECK(ssr_tweaks_number(tweaks, "Packs", "CanRemove") == 1);
    ssr_game_message(game, "packs.btn_Install");
    CHECK(ssr_game_take_pack_install(game) && !ssr_game_take_pack_install(game));
    ssr_game_message(game, "packs.btn_Remove");
    char ids[4][48];
    CHECK(ssr_game_settings(game)->pack[0] == 0 && ssr_pack_list(packs, ids, 4) == 0);
    // The seed of the next world: digits only, and Enter keeps it.
    CHECK(strcmp(ssr_game_text(game, "GS_WorldSeed"), "SSR_SeedRandom") == 0);
    ssr_game_message(game, "packs.inp_Seed");
    CHECK(ssr_game_wants_text(game));
    ssr_game_type(game, "12a3");
    CHECK(strcmp(ssr_game_text(game, "GS_WorldSeed"), "123") == 0);
    ssr_game_key(game, SSR_KEY_BACKSPACE);
    ssr_game_type(game, "45");
    ssr_game_key(game, SSR_KEY_ENTER);
    CHECK(!ssr_game_wants_text(game) && ssr_game_settings(game)->nextWorldSeed == 1245);
    // The ratings and the scores: real first, then classic.
    CHECK(strcmp(ssr_game_text(game, "GS_Realism"), "SSR_RealismReal") == 0);
    ssr_game_message(game, "packs.btn_Realism");
    CHECK(strcmp(ssr_game_text(game, "GS_Realism"), "SSR_RealismClassic") == 0);
    ssr_game_message(game, "packs.btn_Realism");
    ssr_game_message(game, "packs.btn_Back");
    CHECK(strcmp(ssr_game_screen(game), "options") == 0);
    ssr_game_free(game);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_starts_a_new_career();
    test_goes_between_screens();
    test_changes_and_saves_the_options();
    test_chooses_a_language();
    test_chooses_a_pack();
    return TEST_RESULT();
}
