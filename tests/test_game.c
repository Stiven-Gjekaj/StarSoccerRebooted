// Tests ssr_game: the messages of the screens and the settings file.

#include <math.h>
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

static const char *localise(void *context, const char *tag) {
    (void)context;
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
static int seasons = 0, notPicked = 0, substitutes = 0;

static int transfers = 0;

// Yes to a transfer at the end of the season: a nation, a division, a
// club, and the contract. A season with no club that wants the player
// goes on with the club.
static void transfer_at_season_end(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
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
    // The score: 3 * (average + 1) * 200, the star level, and the bars.
    float skills = (ssr_career_skill(c->pace) + ssr_career_skill(c->power) + ssr_career_skill(c->technique) +
                    ssr_career_skill(c->vision) + ssr_career_skill(c->setPieces)) / 3.0f;
    int score = (int)(3 * (c->totalRating / c->apps + 1.0f) * 200.0f);
    score += c->potyAwards * 250 + (c->starRating / 100 + 1) * 200;
    float bars = skills / 100.0f + 1.0f + ssr_career_happiness(c) / 100.0f + ssr_career_lifestyle(c) / 100.0f;
    score = (int)(score * bars) * 10;
    char text[32];
    snprintf(text, sizeof(text), "%d,%03d", score / 1000, score % 1000);
    CHECK(score >= 1000 && score < 1000000 && strcmp(ssr_tweaks_get(tweaks, "PlayerStats", "CareerScore"), text) == 0);
    // Play keeps the score, and the career goes one day back.
    ssr_game_message(game, "retirement.btn_Play");
    CHECK(c->retired == score && c->sdate == sdate - 1);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
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

static void finish_season(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    int year = ssr_career_year(c);
    starManSum += c->seasonStarMan;
    CHECK(c->starMan == starManSum);
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
    // A player with few matches wins no award.
    CHECK(ssr_tweaks_number(tweaks, "SeasonAwards", "TeamPlayerOfYear") == 0);
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

static int winnerPages = 0, newsSeen = 0, dilemmasSeen = 0, interviews = 0, interviewsWon = 0;

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
    ssr_game_message(game, "gamemenu.btn_Play");
    for (int i = 0; i < 20 && strcmp(ssr_game_screen(game), "leaguefixtures") != 0; i++) {
        const char *screen = ssr_game_screen(game);
        newsSeen++;
        if (strncmp(screen, "message", 7) == 0) {
            ssr_game_message(game, "message.btn_Ok");
            // A decision runs its function 2000 ms after the choice.
            ssr_game_update(game, 130);
        } else if (strncmp(screen, "webpage", 7) == 0) {
            ssr_game_message(game, "webpage.btn_Ok");
        } else if (strcmp(screen, "dilemma") == 0) {
            dilemmasSeen++;
            ssr_game_update(game, 130);
            ssr_game_message(game, "dilemma.btn_relationship2");
            ssr_game_update(game, 160);
        } else if (strcmp(screen, "relationships") == 0) {
            ssr_game_message(game, "relationships.btn_Home");
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
    float ratings[10];
    memcpy(ratings, ssr_game_career(game)->ratings, sizeof(ratings));
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
            CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
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
    for (int i = 0; i < 4; i++) ssr_game_message(game, "leaguefixtures.btn_Kits");
    CHECK(strcmp(homeKit, ssr_tweaks_get(tweaks, "TeamStrip", "HomeKitColour")) == 0);
    CHECK(strcmp(awayKit, ssr_tweaks_get(tweaks, "TeamStrip", "AwayKitColour")) == 0);
    CHECK(ssr_tweaks_kind(tweaks, "MatchColours", "MyTeamShirt1") == SSR_TWEAK_COLOUR);
    CHECK(strlen(ssr_tweaks_get(tweaks, "MatchColours", "OppTeamPants")) == 8);
    CHECK(strlen(ssr_tweaks_get(tweaks, "MatchColours", "NameplateHomeCol1")) == 8);
    const char *home = ssr_tweaks_get(tweaks, "Match", "HomeName");
    CHECK(strlen(home) == 3 && strcmp(home, ssr_tweaks_get(tweaks, "Match", "AwayName")) != 0);
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
    CHECK(ssr_tweaks_number(tweaks, "Menu", "LeagueFixturesResults") == 1);
    rows = take_ops(game, ops, 256);
    CHECK(has_op(ops, rows, SSR_OP_LIST_ADD, "FixtureList", "") && !has_op(ops, rows, SSR_OP_LIST_TEXT, "FixtureList", "vs."));
    results_play(game, tweaks);
    CHECK(strcmp(ssr_game_screen(game), "league") == 0 && ssr_tweaks_number(tweaks, "Menu", "ShowLeaguePlay") == 1);
    rows = take_ops(game, ops, 256);
    int entries = 0;
    for (int i = 0; i < rows; i++) entries += ops[i].kind == SSR_OP_LIST_ADD && strcmp(ops[i].ref, "TeamList") == 0;
    CHECK(entries == 4 && has_op(ops, rows, SSR_OP_LIST_SUB, "TeamList", NULL));
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
    ssr_game_message(game, "leaguefixtures.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "messageboss") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CMESSAGE_NOTPICKEDRELATION_NEW") == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "leaguefixtures") == 0 && ssr_game_career(game)->apps == apps);
    ssr_game_message(game, "leaguefixtures.btn_Play");
    if (strcmp(ssr_game_screen(game), "league") == 0) ssr_game_message(game, "league.btn_Continue");
    if (strcmp(ssr_game_screen(game), "seasonreview") == 0) finish_season(game, tweaks);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
}

// Saves the career after three activities, with two sponsors and the news
// of a cup winner, and loads it again.
static void busy(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
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
    // No partner and no sponsors: a message, and no activity.
    ssr_game_message(game, "relationships.btn_Girlfriend");
    CHECK(strcmp(ssr_game_screen(game), "messageboyfriend") == 0);
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
    CHECK(card_faces(game, faces) == 16);
    // The cards turn and move before the player can click.
    ssr_game_message(game, "pairs.btn_0");
    ssr_game_update(game, 60 * 4);
    click_two(game, faces, 1);
    ssr_game_update(game, 90);
    CHECK(strcmp(ssr_game_screen(game), "messageboss") == 0 && c->relationBoss == 55);
    CHECK(strncmp(ssr_tweaks_get(tweaks, "Menu", "MessageBody"), "CDILEMMA_BOSS", 13) == 0);
    ssr_game_message(game, "message.btn_Ok");
    CHECK(strcmp(ssr_game_screen(game), "relationships") == 0);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Player", "BossRelationship_Title"), "BOSS_CAPS_SPACEAFTER 55") == 0);

    // A lost game: two misses, then the screen comes back with no change.
    ssr_game_message(game, "relationships.btn_Team");
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
    // The energy is too low for a third activity.
    ssr_game_message(game, "relationships.btn_Fans");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0 && c->spentTime == 2);
    ssr_game_message(game, "message.btn_Ok");
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

// The lifestyle shop: a purchase, too little cash, the question of the
// first property, a repair, and the end of a trip with the team-mates.
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
    ssr_game_message(game, "message.btn_Ok");
    ssr_game_message(game, "PurchaseAnim.Close");
    // Boots: the first pair, then a question for a second pair.
    ssr_game_message(game, "shop.tab.boots");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Boot_10") == 0);
    ssr_game_message(game, "shopoverlay.buy");
    CHECK(now->bootId == 10 && now->bootUses == 15 && now->bank == 110);
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
    ssr_game_message(game, "PurchaseAnim.Close");
    // The energy is 60, so the can goes at once.
    ssr_game_message(game, "UpsellMsg.DrinkNRG");
    CHECK(now->nrg[0] == 0 && now->energy == 100 && ssr_game_overlay(game)[0] == 0);
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
    snprintf(path, sizeof(path), "%s/career1.txt", directory);
    remove(path);
    careerPath = path;
    SSRGame *game = ssr_game_create(tweaks);
    ssr_game_set_data(game, &world, localise, NULL, directory);
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

    ssr_game_message(game, "choosenation.btn_Ok");
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
    go_shopping(game, tweaks, path);
    hire_and_kit_out(game, tweaks, path);
    get_old_boots_and_a_sponsor(game, tweaks, path);
    renew_the_contract(game, tweaks, path);
    // Six rounds make a season: the career played to the next one.
    for (int i = 0; i < 24 && seasons < 2; i++) play_a_match(game, tweaks);
    // At the second end the star rating is near 400: the limit is about
    // 65, so the club of strength 50 wants the player.
    CHECK(seasons >= 2 && transfers == 1);
    // The weeks had news: tips, and a dilemma or other news.
    CHECK(newsSeen > 0 && ssr_game_career(game)->tipCount > 0);
    CHECK(interviews >= 2 && interviewsWon >= 1);
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
    ssr_game_free(game);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_starts_a_new_career();
    test_goes_between_screens();
    test_changes_and_saves_the_options();
    return TEST_RESULT();
}
