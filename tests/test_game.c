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

static void finish_season(SSRGame *game, SSRTweaks *tweaks) {
    const SSRCareer *c = ssr_game_career(game);
    int year = ssr_career_year(c);
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
    ssr_game_message(game, "playeroftheyear.btn_Play");
    CHECK(strcmp(ssr_game_screen(game), "message") == 0);
    ssr_game_message(game, "message.btn_Cancel");
    CHECK(strcmp(ssr_game_screen(game), "startransition") == 0);
    ssr_game_update(game, 60);
    CHECK(strcmp(ssr_game_screen(game), "gamemenu") == 0);
    // The new season starts the counts again, the day before its first
    // match.
    CHECK(ssr_career_year(c) == year && c->seasonApps == 0 && c->energy == 100);
    CHECK(ssr_career_week(c) == 6 && ssr_tweaks_number(tweaks, "Menu", "DisableQuitButton") == 0);
    seasons++;
}

static void play_a_match(SSRGame *game, SSRTweaks *tweaks) {
    SSRUiOp ops[256];
    int apps = ssr_game_career(game)->apps, bank = ssr_game_career(game)->bank;
    float ratings[10];
    memcpy(ratings, ssr_game_career(game)->ratings, sizeof(ratings));
    // Play shows the fixtures of the round of the player first.
    ssr_game_message(game, "gamemenu.btn_Play");
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
            ssr_game_message(game, "leaguefixtures.btn_Play");
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
    CHECK(chances >= 2 && ssr_game_career(game)->apps == apps + 1);
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
    for (int i = 0; i < 2000 && ssr_tweaks_number(tweaks, "MatchStats", "StatStage") < 10; i++) {
        ssr_game_update(game, 1);
    }
    float rating = ssr_tweaks_number(tweaks, "MatchStats", "MatchRating");
    CHECK(rating >= 1 && rating <= 10);
    int mod = (int)ssr_tweaks_number(tweaks, "RelationshipMods", "Boss");
    CHECK(c->relationBoss == (boss + mod < 1 ? 1 : boss + mod > 100 ? 100 : boss + mod));
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
    ssr_game_message(game, "leaguefixtures.btn_Play");
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
    ssr_game_message(game, "matchreview.btn_Play");
    if (strcmp(ssr_game_screen(game), "seasonreview") == 0) finish_season(game, tweaks);
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
    ssr_game_message(game, "gamemenu.btn_Play");
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

// Saves the career after three activities and with two sponsors, and
// loads it again.
static void busy(SSRGame *game, const char *path) {
    SSRCareer c = *ssr_game_career(game);
    c.spentTime = 3;
    c.sponsors[0] = c.sponsors[1] = 4;
    c.relationSponsors = 50;
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
    // A match starts the activities again, and each sponsor takes 0.65.
    busy(game, path);
    play_a_match(game, tweaks);
    CHECK(ssr_game_career(game)->spentTime == 0 && fabsf(ssr_game_career(game)->relationSponsors - 48.7f) < 1e-4f);
    spend_time(game, tweaks, path);
    // Six rounds make a season: the career played to the next one.
    for (int i = 0; i < 12 && seasons == 0; i++) play_a_match(game, tweaks);
    CHECK(seasons >= 1);
    ssr_game_message(game, "gamemenu.btn_Quit");

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
