// Tests ssr_scene with layouts that the test holds in memory.

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_scene.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static const char *layout;

static char *read_layout(void *context, const char *name, size_t *size) {
    (void)context;
    (void)name;
    *size = strlen(layout);
    char *copy = malloc(*size + 1);
    memcpy(copy, layout, *size + 1);
    return copy;
}

static const char *translate(void *context, const char *tag) {
    (void)context;
    if (strcmp(tag, "MENU_PLAY") == 0) return "Play";
    if (strcmp(tag, "nrg_Cans") == 0) return "Cans";
    if (strcmp(tag, "Consent_TEXT_ATT_2") == 0) return "Second screen";
    return tag;
}

static int image_size(void *context, const char *name, const char *file, float *w, float *h) {
    (void)context;
    if (file && strcmp(file, "NewGUI/Big.png") == 0) {
        *w = 100;
        *h = 50;
        return 1;
    }
    if (strcmp(name, "Glint") != 0) return 0;
    *w = 40;
    *h = 20;
    return 1;
}

static SSRScene *build(const char *text, SSRTweaks *tweaks, SSRUiTree **tree) {
    layout = text;
    SSRUiFiles files = {read_layout, NULL};
    *tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = {.left = 0, .top = 0, .width = 640, .height = 1136, .tweaks = tweaks, .text = translate,
                       .image_size = image_size};
    return ssr_scene_build(*tree, &env);
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static void test_places_images_with_origins_and_variables(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("Set:(@n=[@B-960]*0.5)\n"
                            "Draw:(Title_Sky,@L,@T,@W,@H)\n"
                            "PushOrigin:(100,@n)\n"
                            "Draw:(Glint,10,20,0,10,,#FF000080)\n"
                            "\tAdditive()\n"
                            "Draw:(Glint,0,0,40,20)\n"
                            "\tHandle(20,10)\n"
                            "\tScale(2,2)\n"
                            "\tAngle(45)\n"
                            "PopOrigin\n"
                            "DrawLTRB:(Glint,10,10,30,50)\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 4);
    const SSRSceneItem *sky = ssr_scene_item(scene, 0);
    CHECK(sky && strcmp(sky->image, "Title_Sky") == 0 && sky->w == 640 && sky->h == 1136);

    const SSRSceneItem *glint = ssr_scene_item(scene, 1);
    CHECK(glint && near(glint->x, 110) && near(glint->y, 88 + 20));
    CHECK(glint && near(glint->w, 20) && near(glint->h, 10));
    CHECK(glint && glint->colour == 0xFF000080 && glint->blend == SSR_BLEND_ADD);

    const SSRSceneItem *turned = ssr_scene_item(scene, 2);
    CHECK(turned && near(turned->x, 100 - 40) && near(turned->y, 88 - 20));
    CHECK(turned && near(turned->w, 80) && near(turned->h, 40));
    CHECK(turned && near(turned->pivotX, 100) && near(turned->pivotY, 88) && near(turned->angle, 45));

    const SSRSceneItem *ltrb = ssr_scene_item(scene, 3);
    CHECK(ltrb && near(ltrb->x, 10) && near(ltrb->w, 20) && near(ltrb->h, 40));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_shows_items_by_tweak(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Pitch", "WeatherType", 2);
    const char *text = "Draw:(Rain,0,0,10,10)\n"
                       "\tShowIf(Pitch,WeatherType,2)\n"
                       "{\n"
                       "\tDraw:(Drops,0,0,10,10)\n"
                       "}\n"
                       "Draw:(Sun,0,0,10,10)\n"
                       "\tShowIf(Pitch,WeatherType,0)\n"
                       "\tOrShowIf(Pitch,WeatherType,2)\n"
                       "Draw:(Snow,0,0,10,10)\n"
                       "\tShowIfNot(Pitch,WeatherType,2)\n"
                       "Draw:(Hidden,0,0,10,10) $HIDE\n"
                       "Draw:(Storm,0,0,10,10)\n"
                       "\tShowIf(Pitch,WeatherType,1,2)\n";
    SSRUiTree *tree;
    SSRScene *scene = build(text, tweaks, &tree);
    // ShowIf with two numbers shows for the range from the first to the second.
    CHECK(ssr_scene_count(scene) == 4 && strcmp(ssr_scene_item(scene, 3)->image, "Storm") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 0)->image, "Rain") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 1)->image, "Drops") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 2)->image, "Sun") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);

    ssr_tweaks_set_number(tweaks, "Pitch", "WeatherType", 0);
    scene = build(text, tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 2);
    CHECK(strcmp(ssr_scene_item(scene, 0)->image, "Sun") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 1)->image, "Snow") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_text_areas_layers_and_clips(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("UnsortedLayer:(AfterGloss,1)\n"
                            "Img:(Pitch,NewGUI/Pitch.png)\n"
                            "Img:(Big,NewGUI/Big.png)\n"
                            "Draw:(Big,0,0)\n"
                            "Draw:(Front,0,0,10,10)\n"
                            "\tLayer(AfterGloss)\n"
                            "Rect:(20,30,200,80) ref:PlayButton\n"
                            "{\n"
                            "\tTextLoc:(MENU_PLAY,20,30,200,80,L,T,#00FF00FF)\n"
                            "\t\tFontSize(2)\n"
                            "\tText:(\"0\",0,0,10,10)\n"
                            "}\n"
                            "Clip:(0,0,50,50)\n"
                            "{\n"
                            "\tDraw:(Inside,0,0,10,10)\n"
                            "\tDraw:(Late,0,0,10,10)\n"
                            "\t\tLayer(AfterGloss)\n"
                            "}\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 7);
    const SSRSceneItem *big = ssr_scene_item(scene, 0);
    CHECK(big && big->w == 100 && big->h == 50);
    const SSRSceneItem *area = ssr_scene_item(scene, 1);
    CHECK(area && area->kind == SSR_ITEM_AREA && strcmp(area->ref, "PlayButton") == 0);
    const SSRSceneItem *label = ssr_scene_item(scene, 2);
    CHECK(label && label->kind == SSR_ITEM_TEXT && strcmp(label->text, "Play") == 0);
    CHECK(label && label->font == 2 && label->alignX == 'L' && label->alignY == 'T' && label->colour == 0x00FF00FF);
    CHECK(strcmp(ssr_scene_item(scene, 3)->text, "0") == 0);
    // The item in the clip keeps the box of the clip.
    const SSRSceneItem *inside = ssr_scene_item(scene, 4);
    CHECK(strcmp(inside->image, "Inside") == 0 && inside->clipped && inside->clipW == 50 && inside->clipH == 50);
    CHECK(!ssr_scene_item(scene, 3)->clipped);
    // The items of a later layer draw last. The sort keeps the clip.
    CHECK(strcmp(ssr_scene_item(scene, 5)->image, "Front") == 0);
    const SSRSceneItem *late = ssr_scene_item(scene, 6);
    CHECK(strcmp(late->image, "Late") == 0 && late->clipped && late->clipW == 50);

    CHECK(strcmp(ssr_scene_image_file(scene, "Pitch"), "NewGUI/Pitch.png") == 0);
    CHECK(ssr_scene_image_file(scene, "Other") == NULL);
    CHECK(strcmp(ssr_scene_hit(scene, 100, 50), "PlayButton") == 0);
    CHECK(ssr_scene_hit(scene, 500, 500) == NULL);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_text_expressions_and_set(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Consent", "ATTFlowScreen", 2);
    ssr_tweaks_set_number(tweaks, "Consent", "RequestLocalConsent", 0);
    SSRUiTree *tree;
    SSRScene *scene = build("Rect:(0,0,10,10) ref:Hidden\n"
                            "\tset(gel.vis,twk(Consent,RequestLocalConsent))\n"
                            "{\n"
                            "\tText:(Inside,0,0,10,10)\n"
                            "}\n"
                            "TextBox:(\"Screen \"::twk(Consent,ATTFlowScreen),20,44,550,400)\n"
                            "\tFontScale(0.55,0.5)\n"
                            "TextBox:(MENU_PLAY,0,0,100,0)\n"
                            "Text:(Old,0,0,10,10)\n"
                            "\tset(gel.text,loc(\"Consent_TEXT_ATT_\"::twk(Consent,ATTFlowScreen)))\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 3);
    const SSRSceneItem *joined = ssr_scene_item(scene, 0);
    CHECK(joined && strcmp(joined->text, "Screen 2") == 0 && joined->wrap);
    CHECK(joined && joined->fontScaleX == 0.55f && joined->fontScaleY == 0.5f);
    CHECK(strcmp(ssr_scene_item(scene, 1)->text, "Play") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 2)->text, "Second screen") == 0 && !ssr_scene_item(scene, 2)->wrap);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_clicks_and_their_sounds(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("Rect:(0,0,200,100)\n"
                            "\tClick(menu.play)\n"
                            "\t<\n"
                            "\t\tPlaySnd(UISelect)\n"
                            "\t>\n"
                            "Draw:(Glint,300,0,40,20) ref:Shine\n"
                            "\tClick()\n"
                            "Draw:(Glint,0,200,40,20) ref:Decoration\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 3);
    CHECK(ssr_scene_hit_item(scene, 10, 210) == NULL);
    CHECK(strcmp(ssr_scene_hit(scene, 10, 210), "Decoration") == 0);
    const SSRSceneItem *play = ssr_scene_hit_item(scene, 50, 50);
    CHECK(play && strcmp(play->click, "menu.play") == 0 && strcmp(play->clickSound, "UISelect") == 0);
    CHECK(play && play->ref == NULL);
    const SSRSceneItem *shine = ssr_scene_hit_item(scene, 310, 10);
    CHECK(shine && strcmp(shine->click, "") == 0 && shine->clickSound == NULL);
    CHECK(strcmp(ssr_scene_hit(scene, 310, 10), "Shine") == 0);
    CHECK(ssr_scene_hit_item(scene, 250, 50) == NULL);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_tiles(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("Tile:(TitleGrass01,-5,700,139,254,,#404040FF)\n"
                            "\tCopies(3,2)\n"
                            "\tSpacing(104,10)\n"
                            "Tile:(NoCopies,0,0,10,10)\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 6);
    const SSRSceneItem *last = ssr_scene_item(scene, 5);
    CHECK(last && near(last->x, -5 + 2 * 104) && near(last->y, 710) && near(last->w, 139));
    CHECK(last && last->colour == 0x404040FF && strcmp(last->image, "TitleGrass01") == 0);
    CHECK(near(ssr_scene_item(scene, 1)->x, 99) && near(ssr_scene_item(scene, 1)->y, 700));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_sets_a_variable_many_times(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    char text[8192] = "";
    // More sets than the store has places for, of one name.
    for (int i = 0; i < 100; i++) {
        char line[64];
        snprintf(line, sizeof(line), "Set:(@n=%d)\n", i);
        strcat(text, line);
    }
    strcat(text, "Draw:(Last,0,@n,10,10)\n");
    SSRUiTree *tree;
    SSRScene *scene = build(text, tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 1 && near(ssr_scene_item(scene, 0)->y, 99));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_panels_of_the_screen_and_of_origins(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    layout = "Draw:(Top,@L,@T,@W,@H)\n"
             "PushOrigin:(306,431-[@T*0.8],200,100)\n"
             "Draw:(Inside,@L,@T,@W,@H)\n"
             "PushOrigin:(Left,Bottom)\n"
             "Draw:(Corner,0,0,10,10)\n"
             "PopOrigin\n"
             "PopOrigin\n"
             "Draw:(After,@C,@M,1,1)\n";
    SSRUiFiles files = {read_layout, NULL};
    SSRUiTree *tree = ssr_ui_parse("Screen.uis", &files);
    // A 640 x 1422 screen: the 640 x 960 box is in the middle.
    SSRSceneEnv env = {.left = 0, .top = -231, .width = 640, .height = 1422, .tweaks = tweaks, .text = translate,
                       .image_size = image_size};
    SSRScene *scene = ssr_scene_build(tree, &env);
    CHECK(ssr_scene_count(scene) == 4);
    const SSRSceneItem *top = ssr_scene_item(scene, 0);
    CHECK(top && near(top->y, -231) && near(top->h, 1422));
    const SSRSceneItem *inside = ssr_scene_item(scene, 1);
    CHECK(inside && near(inside->x, 306) && near(inside->y, 431 + 231 * 0.8f));
    CHECK(inside && near(inside->w, 200) && near(inside->h, 100));
    const SSRSceneItem *corner = ssr_scene_item(scene, 2);
    CHECK(corner && near(corner->x, 0) && near(corner->y, -231 + 1422));
    const SSRSceneItem *after = ssr_scene_item(scene, 3);
    CHECK(after && near(after->x, 320) && near(after->y, -231 + 711));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// The draws of a 3 x 2 crowd from the seed of the game. They come from a
// copy of c_CrowdPacket.Draw in Python, with the same tweaks.
static void test_draws_a_crowd(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Crowd", "Active", 1);
    ssr_tweaks_set_number(tweaks, "Crowd", "TrouserColCount", 2);
    ssr_tweaks_set(tweaks, "Crowd", "TrouserCol0", "808080FF");
    ssr_tweaks_set(tweaks, "Crowd", "TrouserCol1", "806020FF");
    ssr_tweaks_set_number(tweaks, "Crowd", "HatLikelihood", 0.3);
    ssr_tweaks_set_number(tweaks, "Crowd", "HomeAwayBias", 0.7);
    const char *sides[2] = {"HomeCrowd", "AwayCrowd"};
    const char *home[2] = {"FF0000FF", "00FF00FF"};
    const char *away[2] = {"0000FFFF", "FFFF00FF"};
    for (int i = 0; i < 2; i++) {
        ssr_tweaks_set_number(tweaks, sides[i], "Density", i ? 1.0 : 0.8);
        ssr_tweaks_set_number(tweaks, sides[i], "StripChance", 0.7);
        ssr_tweaks_set_number(tweaks, sides[i], "AwayStripChance", i ? 0.7 : 0.2);
        ssr_tweaks_set(tweaks, sides[i], "ShirtColHome", home[i]);
        ssr_tweaks_set(tweaks, sides[i], "ShirtColAway", away[i]);
        for (int k = 0; k < 20; k++) {
            char name[16], value[16];
            snprintf(name, sizeof(name), "SkinCol%d", k);
            snprintf(value, sizeof(value), "%02X%02X%02XFF", k * 10, k * 10, k * 10);
            ssr_tweaks_set(tweaks, sides[i], name, value);
        }
    }
    SSRUiTree *tree;
    SSRScene *scene = build("Crowd:(0,0,20,40)\n"
                            "\tHandle(10,40)\n"
                            "\tSpacing(30,50)\n"
                            "\tCopies(3,2)\n",
                            tweaks, &tree);
    static const struct {
        const char *image;
        float x, y, angle;
        unsigned int rgb;
    } expected[20] = {
        {"CrowdTrousers1", 0, 0, 0, 0x808080}, {"CrowdShirt", 0, 0, 2, 0x6E94A6},
        {"CrowdHead1", 0, 0, 2, 0x646464},     {"CrowdArms3", 0, 0, 2, 0x646464},
        {"CrowdTrousers1", 59, 0, 0, 0x806020}, {"CrowdShirt", 59, 0, 1, 0xFFFF00},
        {"CrowdHead1", 59, 1, 1, 0x5A5A5A},    {"CrowdArms2", 59, 0, 1, 0x5A5A5A},
        {"CrowdTrousers1", 0, 50, 0, 0x808080}, {"CrowdShirt", 0, 50, 3, 0x0000FF},
        {"CrowdHead2", 0, 50, 3, 0x141414},    {"CrowdArms4", 0, 50, 3, 0x141414},
        {"CrowdTrousers1", 29, 50, 0, 0x806020}, {"CrowdShirt", 29, 50, 0, 0x227C22},
        {"CrowdHead1", 29, 51, 0, 0x5A5A5A},   {"CrowdArms2", 29, 50, 0, 0x5A5A5A},
        {"CrowdTrousers1", 60, 50, 0, 0x806020}, {"CrowdShirt", 60, 50, -3, 0xFF0000},
        {"CrowdHead2", 60, 51, -3, 0x0A0A0A},  {"CrowdArms4", 60, 50, -3, 0x0A0A0A},
    };
    CHECK(ssr_scene_count(scene) == 20);
    for (int i = 0; i < 20 && i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        CHECK(strcmp(item->image, expected[i].image) == 0);
        CHECK(near(item->pivotX, expected[i].x) && near(item->pivotY, expected[i].y));
        CHECK(near(item->x, expected[i].x - 10) && near(item->y, expected[i].y - 40));
        CHECK(near(item->angle, expected[i].angle));
        CHECK(item->colour == (expected[i].rgb << 8 | 0xFF));
    }
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_definitions_do_not_draw(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("Animation:(600,Loop,0) ref:Glow\n"
                            "{\n"
                            "\tDuring:(0,299)\n"
                            "\t{\n"
                            "\t\tDraw:(Glow,0,0,100,100)\n"
                            "\t\tSequence:(Coin#,1,2)\n"
                            "\t}\n"
                            "}\n"
                            "Draw:(Template,0,0,10,10) ref:Particle\n"
                            "{\n"
                            "\tDraw:(Inside,0,0,10,10)\n"
                            "} Template\n"
                            "Draw:(Shown,0,0,10,10)\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 1 && strcmp(ssr_scene_item(scene, 0)->image, "Shown") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static SSRSceneEnv test_env(SSRTweaks *tweaks) {
    SSRSceneEnv env = {.left = 0, .top = 0, .width = 640, .height = 960, .tweaks = tweaks, .text = translate,
                       .image_size = image_size};
    return env;
}

static const SSRSceneItem *find_image(const SSRScene *scene, const char *image) {
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (item->image && strcmp(item->image, image) == 0) return item;
    }
    return NULL;
}

// The buttons of the menu wait for their Delay, then slide in from the left
// at a rate of 0.2 a frame. Press() pulls the button down by 8.
static void test_converges_after_a_delay_and_on_press(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "PushOrigin:(100,200)\n"
             "Rect:(0,0,200,80)\n"
             "\tPress()\n"
             "\t<\n"
             "\t\tConvergeTo(0,8)\n"
             "\t>\n"
             "\tNoPress()\n"
             "\t<\n"
             "\t\tConvergeTo(0,0)\n"
             "\t>\n"
             "{\n"
             "\tRect:(0,0,200,80)\n"
             "\t{\n"
             "\t\tDraw:(Button,0,0,200,80)\n"
             "\t}\n"
             "\t\tDelay(0.1)\n"
             "\t\tConvergeFrom(-640,0)\n"
             "}\n"
             "PopOrigin\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    const SSRSceneItem *button = find_image(scene, "Button");
    CHECK(button && near(button->x, 100 - 640) && near(button->y, 200));
    // 0, 17, 17, 17, 17, 17, 17: the counter is 102 ms at the seventh frame,
    // so the first step is there.
    for (int i = 0; i < 5; i++) ssr_scene_update(scene, &env, 17);
    button = find_image(scene, "Button");
    CHECK(button && near(button->x, -540));
    ssr_scene_update(scene, &env, 17);
    button = find_image(scene, "Button");
    CHECK(button && near(button->x, -540 + 640 * 0.2f));
    for (int i = 0; i < 200; i++) ssr_scene_update(scene, &env, 17);
    button = find_image(scene, "Button");
    CHECK(button && fabsf(button->x - 100) < 0.01f && fabsf(button->y - 200) < 0.01f);

    // A touch on the button. Press() sees it in the next frame.
    ssr_scene_touch(scene, 1, 150, 230);
    ssr_scene_update(scene, &env, 17);
    ssr_scene_update(scene, &env, 17);
    button = find_image(scene, "Button");
    CHECK(button && near(button->y, 200 + 8 * 0.2f));
    for (int i = 0; i < 100; i++) ssr_scene_update(scene, &env, 17);
    CHECK(fabsf(find_image(scene, "Button")->y - 208) < 0.01f);
    ssr_scene_touch(scene, 0, 150, 230);
    for (int i = 0; i < 100; i++) ssr_scene_update(scene, &env, 17);
    CHECK(fabsf(find_image(scene, "Button")->y - 200) < 0.01f);
    CHECK(ssr_scene_time(scene) == 408 * 17);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// The Glow of the main menu: 0 to 299 turns to 240 degrees, gets half the
// size about its centre, and goes from grey to white.
static void test_plays_an_animation(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "Animation:(600,Loop,0) ref:Glow\n"
             "{\n"
             "\tDuring:(0,299)\n"
             "\t{\n"
             "\t\tDraw:(Glow,0,0,100,100,,#80808000)\n"
             "\t\t\tAdditive()\n"
             "\t\t\tTweenAngle(240,0,0)\n"
             "\t\t\tTweenSize(50,50,50,50,-1,-1)\n"
             "\t\t\tTweenCol(#FFFFFFFF)\n"
             "\t}\n"
             "\tDuring:(300,599)\n"
             "\t{\n"
             "\t\tDraw:(Glow2,0,0,50,50,241,#FFFFFFFF)\n"
             "\t}\n"
             "}\n"
             "AnimInstance:(40,160,Glow) ref:GlowInstance\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    const SSRSceneItem *glow = find_image(scene, "Glow");
    CHECK(ssr_scene_count(scene) == 1 && glow);
    // The code can stop an animation, and send it to its end.
    ssr_scene_anim(scene, "GlowInstance", 0, 1);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "Glow2") && !find_image(scene, "Glow"));
    ssr_scene_free(scene);
    scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    glow = find_image(scene, "Glow");
    CHECK(glow && near(glow->w, 100) && near(glow->pivotX, 40) && near(glow->x, -10) && near(glow->angle, 0));
    CHECK(glow && glow->colour == 0x80808000 && glow->blend == SSR_BLEND_ADD);
    // Frames of 16 ms are one frame of the game each. Frame 299: t is 1.
    for (int i = 0; i < 299; i++) ssr_scene_update(scene, &env, 16);
    glow = find_image(scene, "Glow");
    CHECK(glow && near(glow->w, 50) && near(glow->x, 15) && near(glow->angle, -120));
    CHECK(glow && glow->colour == 0xFFFFFFFF);
    // Frame 150: the size eases with smoothstep, the angle does not.
    ssr_scene_free(scene);
    scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    for (int i = 0; i < 150; i++) ssr_scene_update(scene, &env, 16);
    glow = find_image(scene, "Glow");
    float t = 150.0f / 299;
    CHECK(glow && fabsf(glow->w - (100 - 50 * (3 * t * t - 2 * t * t * t))) < 0.01f);
    CHECK(glow && fabsf(glow->angle - 240 * t) < 0.01f);
    for (int i = 0; i < 150; i++) ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "Glow2") && !find_image(scene, "Glow"));
    // The loop starts again at 600.
    for (int i = 0; i < 300; i++) ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "Glow") && !find_image(scene, "Glow2"));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// A colour of 16 digits mixes with the colour of the parent. A gel with no
// colour takes the colour of the parent.
static void test_mixes_colours_and_orders_layers(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    SSRScene *scene = build("UnsortedLayer:(Top,2)\n"
                            "SortedLayer:(Sorted,1)\n"
                            "Rect:(0,0,10,10,,#80402080)\n"
                            "{\n"
                            "\tDraw:(Plain,0,0,10,10)\n"
                            "\tDraw:(Multiply,0,0,10,10,,#FFFFFFFFFFFFFFFF)\n"
                            "\tDraw:(OwnRgb,0,0,10,10,,#FF0000FF000000FF)\n"
                            "\tDraw:(Replace,0,0,10,10,,#00FF00FF)\n"
                            "\t\tLayer(Top)\n"
                            "}\n"
                            "Draw:(Deep,0,0,10,10)\n"
                            "\tLayer(Sorted)\n"
                            "\tDepth(5)\n"
                            "Draw:(Shallow,0,0,10,10)\n"
                            "\tLayer(Sorted)\n"
                            "\tDepth(2)\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 6);
    CHECK(find_image(scene, "Plain")->colour == 0x80402080);
    CHECK(find_image(scene, "Multiply")->colour == 0x80402080);
    CHECK(find_image(scene, "OwnRgb")->colour == 0xFF000080);
    CHECK(find_image(scene, "Replace")->colour == 0x00FF00FF);
    CHECK(strcmp(ssr_scene_item(scene, 3)->image, "Shallow") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 4)->image, "Deep") == 0);
    CHECK(strcmp(ssr_scene_item(scene, 5)->image, "Replace") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_set(tweaks, "StadiumCol", "SeatColHome", "7a1e98FF");
    scene = build("Draw:(Seats,0,0,10,10,,#AAAAAAFF)\n"
                  "\tColReadout(StadiumCol,SeatColHome)\n",
                  tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 1 && ssr_scene_item(scene, 0)->colour == 0x7A1E98FF);
    // A cel tweak of 8 digits replaces the colour. One of 16 digits mixes.
    CHECK(ssr_scene_mix_colour("#FF00FFFF", 0x80808080) == 0xFF00FFFF);
    CHECK(ssr_scene_mix_colour("#FFFFFFFF000000FF", 0x80808080) == 0xFFFFFF80);
    CHECK(ssr_scene_mix_colour("bad", 0x12345678) == 0x12345678);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// An emitter makes copies of its template with values from Range tweaks,
// and each copy goes away at the end of its Lifetime.
static void test_emits_particles(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    char path[1200];
    snprintf(path, sizeof(path), "%s/TweakValues.txt", ssr_test_directory("scene_emitter"));
    FILE *file = fopen(path, "wb");
    fputs("Category:(Stars)\n"
          "\tRange:(Rate,Rate,0.1,0.1,1)\n"
          "\tRange:(Lifetime,Life,0.25,0.25,1)\n"
          "\tRange:(XVel,X Vel,2,2,1)\n"
          "\tRange:(YVel,Y Vel,0,0,1)\n"
          "EndCategory\n",
          file);
    fclose(file);
    CHECK(ssr_tweaks_load(tweaks, path) == 4);
    SSRUiTree *tree;
    layout = "Draw:(Star,0,0,10,10) ref:StarTemplate\n"
             "\tEmitPlacer()\n"
             "\tEmitMover()\n"
             "\tEmitLifetime()\n"
             "{\n"
             "} Template\n"
             "Emitter:(Stars,StarTemplate) ref:Burst\n"
             "Instance:(Burst,300,400)\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    CHECK(ssr_scene_count(scene) == 0);
    // 100 ms a star: after 6 frames of 17 ms the first one is there.
    for (int i = 0; i < 6; i++) ssr_scene_update(scene, &env, 17);
    CHECK(ssr_scene_count(scene) == 1);
    const SSRSceneItem *star = find_image(scene, "Star");
    CHECK(star && near(star->x, 302) && near(star->y, 400));
    ssr_scene_update(scene, &env, 17);
    CHECK(near(find_image(scene, "Star")->x, 304));
    // A star lives 250 ms, so at most three are there.
    for (int i = 0; i < 60; i++) ssr_scene_update(scene, &env, 17);
    CHECK(ssr_scene_count(scene) >= 2 && ssr_scene_count(scene) <= 3);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static const char *provide(void *context, const char *name) {
    (void)context;
    return strcmp(name, "GS_PlayerClubName") == 0 ? "LEE" : NULL;
}

static const SSRSceneItem *find_text(const SSRScene *scene, const char *text) {
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (item->text && strcmp(item->text, text) == 0) return item;
    }
    return NULL;
}

// The texts that come from the state of the game, the conditions, and the
// actions of a click.
static void test_readouts_conditions_and_click_actions(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Player", "SponsorIncome1", 15);
    ssr_tweaks_set_number(tweaks, "Player", "Energy", 7.25);
    ssr_tweaks_set(tweaks, "SaveSlot", "Slot0Data", "Save Slot 1");
    ssr_tweaks_set_number(tweaks, "Mods", "Boss", 2);
    ssr_tweaks_set_number(tweaks, "Mods", "Team", -1);
    ssr_tweaks_set_number(tweaks, "Player", "Cans", 3);
    ssr_tweaks_set_number(tweaks, "Menu", "OwnedBoot", 1);
    ssr_tweaks_set_number(tweaks, "Menu", "FocusedItem", 2);
    SSRUiTree *tree;
    layout = "Text:(\"00\",0,0,100,20)\n"
             "\tReadout(Player,SponsorIncome1,* )\n"
             "Text:(\"0\",0,30,100,20)\n"
             "\tReadout(Player,Energy,,,,2)\n"
             "Text:(\"45'\",0,200,100,20)\n"
             "\tReadout(Player,SponsorIncome1,,')\n"
             "Text:(Old,0,60,100,20)\n"
             "\tStringReadout(SaveSlot,Slot0Data)\n"
             "Text:(Boss,300,300,100,20)\n"
             "\tSignedReadoutLoc(Mods,Boss,MENU_PLAY)\n"
             "Text:(Team,300,330,100,20)\n"
             "\tSignedReadoutLoc(Mods,Team,MENU_PLAY)\n"
             "Text:(Zero,300,360,100,20)\n"
             "\tZeroSignedReadoutLoc(Always,0,MENU_PLAY)\n"
             "Text:(Cans,300,390,100,20)\n"
             "\tReadout(Player,Cans,, can, cans)\n"
             "Text:(Club,0,90,100,20)\n"
             "\tGS_PlayerClubName()\n"
             "Rect:(0,0,10,10)\n"
             "\tSelector(Menu,OwnedBoot)\n"
             "{\n"
             "\tDraw:(Boot0,0,0,10,10)\n"
             "\tDraw:(Boot1,0,0,10,10)\n"
             "}\n"
             "Draw:(Focus,0,0,10,10,,#000000FF)\n"
             "\tif(twk(Menu,FocusedItem)=2)\n"
             "\t<\n"
             "\t\tColour(#FF0000FF)\n"
             "\t\tConvergeCol(#FF0000FF,Instant)\n"
             "\t>\n"
             "\telse\n"
             "\t<\n"
             "\t\tConvergeCol(#0000FFFF,1)\n"
             "\t>\n"
             "Rect:(200,0,100,100)\n"
             "\tClick(menu.next)\n"
             "\t<\n"
             "\t\tPlaySnd(UISelect)\n"
             "\t\tSetTweak(Combo,ShowNations,1)\n"
             "\t\tIncrementTweak(Menu,Page,0,1)\n"
             "\t>\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    env.provide = provide;
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    CHECK(find_text(scene, "* 15"));
    CHECK(find_text(scene, "7.25"));
    CHECK(find_text(scene, "15'"));
    CHECK(find_text(scene, "Save Slot 1"));
    CHECK(find_text(scene, "Play+2") && find_text(scene, "Play-1") && find_text(scene, "Play+0"));
    CHECK(find_text(scene, "3 cans"));
    ssr_tweaks_set_number(tweaks, "Player", "Cans", 1);
    ssr_scene_update(scene, &env, 0);
    CHECK(find_text(scene, "1 can"));
    CHECK(find_text(scene, "LEE"));
    CHECK(find_image(scene, "Boot1") && !find_image(scene, "Boot0"));
    CHECK(find_image(scene, "Focus")->colour == 0xFF0000FF);
    ssr_tweaks_set_number(tweaks, "Menu", "FocusedItem", 0);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "Focus")->colour == 0x0000FFFF);

    const SSRSceneItem *next = ssr_scene_hit_item(scene, 250, 50);
    CHECK(next && strcmp(ssr_scene_click(scene, &env, next), "menu.next") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Combo", "ShowNations") == 1);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "Page") == 1);
    CHECK(strcmp(ssr_scene_take_sound(scene), "UISelect") == 0 && ssr_scene_take_sound(scene) == NULL);
    ssr_scene_click(scene, &env, next);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "Page") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// The code fills a list from a template, as c_ACombo does, and changes
// the text and the visibility of the elements with a ref.
static void test_fills_lists_and_changes_elements(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "Rect:(0,0,410,100) ref:NationTemplate\n"
             "\tClick(choosenation.cmb_Nation)\n"
             "\t<\n"
             "\t\tSetTweak(Combo,ChosenDesiredNation,0) ref:ID\n"
             "\t>\n"
             "{\n"
             "\tDraw:(GreenButton,0,0,410,100)\n"
             "\tTextLoc:(Nation,0,0,410,100) ref:NationTemplate\n"
             "} Template\n"
             "PushOrigin:(100,200)\n"
             "Rect:(0,0,0,0) ref:NationCombo\n"
             "PopOrigin\n"
             "Text:(Choose,0,0,100,20) ref:NationText\n"
             "Draw:(PlayBall,0,0,10,10) ref:PlayButton $HIDE\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    CHECK(ssr_scene_list_add(scene, "NationCombo", "NationTemplate", "ENGLAND", 62));
    CHECK(ssr_scene_list_add(scene, "NationCombo", "NationTemplate", "WALES", 63));
    CHECK(ssr_scene_set_text(scene, "NationText", "England") == 1);
    CHECK(ssr_scene_set_visible(scene, "PlayButton", 1) == 1);
    CHECK(ssr_scene_set_colour(scene, "PlayButton", "FFFFFF55") == 1);
    CHECK(!ssr_scene_list_add(scene, "NoList", "NationTemplate", "X", 1));
    ssr_scene_update(scene, &env, 0);
    CHECK(find_text(scene, "England") && find_image(scene, "PlayBall"));
    CHECK(find_image(scene, "PlayBall")->colour == 0xFFFFFF55);
    const SSRSceneItem *wales = find_text(scene, "WALES");
    CHECK(wales && near(wales->x, 110) && near(wales->y, 300));
    CHECK(find_text(scene, "ENGLAND") && near(find_text(scene, "ENGLAND")->y, 200));
    const SSRSceneItem *hit = ssr_scene_hit_item(scene, 150, 350);
    CHECK(hit && strcmp(ssr_scene_click(scene, &env, hit), "choosenation.cmb_Nation") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Combo", "ChosenDesiredNation") == 63);
    // The code colours the parts of the last entry, and changes its
    // sub-elements.
    ssr_scene_list_colour(scene, "NationCombo", 1, "FF0000FF");
    ssr_scene_list_sub(scene, "NationCombo", "NationTemplate", NULL, 1);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "GreenButton"));
    ssr_scene_list_sub(scene, "NationCombo", "NationTemplate", "Cymru", 1);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_text(scene, "Cymru") && find_text(scene, "Cymru")->colour == 0xFF0000FF);
    ssr_scene_list_colour(scene, "NationCombo", 1, "00FF00FF");
    ssr_scene_update(scene, &env, 16);
    CHECK(find_text(scene, "Cymru")->colour == 0x00FF00FF);
    ssr_scene_list_sub(scene, "NationCombo", "NationTemplate", NULL, 0);
    ssr_scene_update(scene, &env, 16);
    CHECK(!find_text(scene, "Cymru") && find_text(scene, "ENGLAND"));
    ssr_scene_list_clear(scene, "NationCombo");
    ssr_scene_update(scene, &env, 16);
    CHECK(!find_text(scene, "WALES"));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// The doodads of the expression language: set, add, if with ".:", the
// variables of a gel, and the own variables of each copy of a template.
static void test_runs_expressions(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_kind(tweaks, "Player", "Cans", "3", SSR_TWEAK_FLOAT);
    ssr_tweaks_set_kind(tweaks, "Player", "StarBux", "1234", SSR_TWEAK_FLOAT);
    SSRUiTree *tree;
    layout = "Text:(Old,0,0,100,20)\n"
             "\tset(gel.text,fmt(twk(Player,Cans),X)::\" \"::loc(nrg_Cans))\n"
             "Text:(Count,0,30,100,20)\n"
             "\tfloat(%c,%t,%n)\n"
             "\t.set(%c,twk(Player,StarBux))\n"
             "\t.add(%n,5)\n"
             "\tif(%c != twk(Player,StarBux))\n"
             "\t<\n"
             "\t\t.:set(%t,%t+1)\n"
             "\t>\n"
             "\tset(gel.text,\"Count \"::fmt(%t,X)::\"/\"::fmt(%n,X))\n"
             "Rect:(0,0,10,10)\n"
             "\tset(gel.vis,twk(Player,Cans)>5)\n"
             "{\n"
             "\tDraw:(Hidden,0,0,10,10)\n"
             "}\n"
             "Rect:(0,0,100,20) ref:Row\n"
             "\tfloat(%k)\n"
             "\tinit(%k,7)\n"
             "{\n"
             "\tText:(Row,0,0,100,20) ref:Row\n"
             "\t\tadd(%k,1)\n"
             "\t\tset(gel.text,\"Row \"::fmt(%k,X))\n"
             "} Template\n"
             "Rect:(0,0,0,0) ref:Rows\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    CHECK(ssr_scene_list_add(scene, "Rows", "Row", NULL, 0));
    CHECK(ssr_scene_list_add(scene, "Rows", "Row", NULL, 0));
    ssr_scene_update(scene, &env, 0);
    CHECK(find_text(scene, "3 Cans"));
    CHECK(find_text(scene, "Count 0/5"));
    CHECK(!find_image(scene, "Hidden"));
    // Each copy adds to its own %k.
    int rows = 0;
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (item->text && strcmp(item->text, "Row 8") == 0) rows++;
    }
    CHECK(rows == 2);

    ssr_tweaks_set_number(tweaks, "Player", "StarBux", 1300);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_text(scene, "Count 1/5"));
    ssr_scene_update(scene, &env, 16);
    CHECK(find_text(scene, "Count 1/5"));
    ssr_tweaks_set_number(tweaks, "Player", "StarBux", 1234);
    ssr_scene_update(scene, &env, 16);
    ssr_tweaks_set_number(tweaks, "Player", "StarBux", 1300);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_text(scene, "Count 2/5"));
    ssr_tweaks_set_number(tweaks, "Player", "Cans", 6);
    ssr_scene_update(scene, &env, 16);
    CHECK(find_image(scene, "Hidden") && find_text(scene, "6 Cans"));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// The code moves copies of a template, plays their animations, and a gel
// lower on the screen draws over a gel higher up (YDepth).
static void test_moves_copies_and_sorts_by_y(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "SortedLayer:(Minigame,3)\n"
             "Animation:(90,Loop) ref:RunA\n"
             "{\n"
             "\tDuring:(0,90)\n"
             "\t{\n"
             "\t\tDraw:(ImageA,0,0,10,10)\n"
             "\t}\n"
             "}\n"
             "Animation:(90,Loop) ref:RunB\n"
             "{\n"
             "\tDuring:(0,90)\n"
             "\t{\n"
             "\t\tDraw:(ImageB,0,0,10,10)\n"
             "\t}\n"
             "}\n"
             "Rect:(0,0,0,0) ref:Pitch\n"
             "Rect:(0,0) $HIDE ref:Player\n"
             "{\n"
             "\tAnimInstance:(0,-30,RunA) ref:PlayerAnim\n"
             "\t\tLayer(Minigame)\n"
             "\t\tYDepth(25)\n"
             "}\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    int low = ssr_scene_spawn(scene, "Pitch", "Player", 0, NULL, 0);
    int high = ssr_scene_spawn(scene, "Pitch", "Player", 0, NULL, 0);
    CHECK(low && high && low != high);
    ssr_scene_spawn_place(scene, low, NULL, 100, 500);
    ssr_scene_spawn_place(scene, high, NULL, 200, 300);
    ssr_scene_spawn_anim(scene, high, "PlayerAnim", "RunB");
    ssr_scene_update(scene, &env, 0);
    const SSRSceneItem *a = find_image(scene, "ImageA"), *b = find_image(scene, "ImageB");
    CHECK(a && b && b < a);
    CHECK(b && near(b->x, 200) && near(b->y, 270) && a && near(a->x, 100) && near(a->y, 470));
    // The copy moves past the other one, and draws over it now.
    ssr_scene_spawn_place(scene, high, NULL, 200, 700);
    ssr_scene_spawn_show(scene, low, "PlayerAnim", 0);
    ssr_scene_update(scene, &env, 16);
    CHECK(!find_image(scene, "ImageA") && find_image(scene, "ImageB"));
    ssr_scene_spawn_show(scene, low, "PlayerAnim", 1);
    ssr_scene_update(scene, &env, 16);
    a = find_image(scene, "ImageA");
    b = find_image(scene, "ImageB");
    CHECK(a && b && a < b);
    // The code scales, turns, and colours a part of a copy.
    ssr_scene_spawn_set(scene, low, "PlayerAnim#0", SSR_GEL_SCALE, 2);
    ssr_scene_spawn_colour(scene, low, "PlayerAnim", "FF000080");
    ssr_scene_update(scene, &env, 16);
    a = find_image(scene, "ImageA");
    CHECK(a && near(a->w, 20) && a->colour == 0xFF000080);
    ssr_scene_spawn_set(scene, low, "PlayerAnim", SSR_GEL_SCALE, 1);
    // The code moves an element of the screen by its ref.
    ssr_scene_place(scene, "Pitch", 10, 20, 2);
    ssr_scene_update(scene, &env, 16);
    b = find_image(scene, "ImageB");
    CHECK(b && near(b->x, 10 + 2 * 200) && near(b->y, 20 + 2 * 670));
    // The id 0 changes an element of the screen by its ref: a turn moves
    // the image, and a turn back puts it back.
    ssr_scene_spawn_set(scene, 0, "Pitch", SSR_GEL_ANGLE, 90);
    ssr_scene_update(scene, &env, 16);
    b = find_image(scene, "ImageB");
    CHECK(b && !near(b->x, 10 + 2 * 200));
    ssr_scene_spawn_set(scene, 0, "Pitch", SSR_GEL_ANGLE, 0);
    ssr_scene_update(scene, &env, 16);
    b = find_image(scene, "ImageB");
    CHECK(b && near(b->x, 10 + 2 * 200) && near(b->y, 20 + 2 * 670));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// A Sheet: puts its children on a plane in 3D, with the perspective of the
// game. The corners of the pitch land where c_GelSheet puts them.
static void test_projects_a_sheet(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "PushOrigin:(50,50)\n"
             "Sheet:(-593,-606,1980,-30,0,0,20)\n"
             "{\n"
             "\tDraw:(Grass,-50,-50,1610,1610)\n"
             "}\n"
             "PopOrigin\n"
             "Sheet:(0,0,0,0,0,0,0,1,1,1,$PITCH)\n"
             "\tset(gel.vis,false)\n"
             "{\n"
             "\tDraw:(Flat,10,20,30,40)\n"
             "}\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    const SSRSceneItem *grass = find_image(scene, "Grass");
    CHECK(grass && grass->projected);
    // The PushOrigin moves the child on the plane, so it starts at (0, 0).
    static const float SCREEN[8] = {-223.53f, -75.44f, 1252.15f, -75.44f, 1890.78f, 1697.55f, -595.90f, 1697.55f};
    for (int k = 0; grass && k < 4; k++) {
        const float *c = grass->camera + k * 3;
        float sx = 320 + grass->focal * c[0] / c[2], sy = 480 + grass->focal * c[1] / c[2];
        CHECK(fabsf(sx - SCREEN[k * 2]) < 0.05f && fabsf(sy - SCREEN[k * 2 + 1]) < 0.05f);
    }
    // A sheet that renders into a surface draws its children in place, each
    // frame, with no perspective.
    const SSRSceneItem *flat = find_image(scene, "Flat");
    CHECK(flat && !flat->projected && near(flat->x, 10) && near(flat->y, 20));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// CloneTweak gives each copy its own kit: the items keep the cel colours
// of the moment that their gel draws.
static void test_clones_kit_colours(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_kind(tweaks, "MatchColours", "MyShirt", "FF0000FF", SSR_TWEAK_COLOUR);
    ssr_tweaks_set_kind(tweaks, "MatchColours", "OppShirt", "0000FFFF", SSR_TWEAK_COLOUR);
    SSRUiTree *tree;
    layout = "Rect:(0,0,0,0) ref:Pitch\n"
             "Rect:(0,0) $HIDE ref:Player\n"
             "{\n"
             "\tDraw:(Body,0,0,10,10)\n"
             "\t\tCloneTweak(MatchColours,OppShirt,CelColours,Shirt) ref:Shirt\n"
             "}\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    int mine = ssr_scene_spawn(scene, "Pitch", "Player", 0, NULL, 0);
    int theirs = ssr_scene_spawn(scene, "Pitch", "Player", 0, NULL, 0);
    ssr_scene_spawn_clone(scene, mine, "Shirt", "MatchColours", "MyShirt");
    ssr_scene_spawn_place(scene, theirs, NULL, 100, 0);
    ssr_scene_update(scene, &env, 0);
    int red = 0, blue = 0;
    for (int i = 0; i < ssr_scene_count(scene); i++) {
        const SSRSceneItem *item = ssr_scene_item(scene, i);
        if (!item->image || strcmp(item->image, "Body") != 0) continue;
        CHECK(item->cels != NULL);
        if (item->cels && strstr(item->cels, "Shirt=FF0000FF;")) red += item->x < 50;
        if (item->cels && strstr(item->cels, "Shirt=0000FFFF;")) blue += item->x >= 50;
    }
    CHECK(red == 1 && blue == 1);
    // The tweak keeps the last value, as in the game.
    CHECK(strcmp(ssr_tweaks_get(tweaks, "CelColours", "Shirt"), "0000FFFF") == 0);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// DoIf runs its block while the tweak is in its range, or has its text.
static void test_runs_a_block_by_a_tweak(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Menu", "Maximise", 0);
    ssr_tweaks_set_kind(tweaks, "Menu", "Tab", "Shop", SSR_TWEAK_STRING);
    SSRUiTree *tree;
    layout = "Draw:(Box,0,0,10,10)\n"
             "\tDoIf(Menu,Maximise,1)\n"
             "\t<\n"
             "\t\tConvergeTo(0,-100,Instant)\n"
             "\t>\n"
             "\tDoIf(Menu,Maximise,0)\n"
             "\t<\n"
             "\t\tConvergeTo(0,0,Instant)\n"
             "\t>\n"
             "Draw:(Tab,0,0,10,10)\n"
             "\tDoIfNot(Menu,Tab,Shop)\n"
             "\t<\n"
             "\t\tConvergeTo(50,0,Instant)\n"
             "\t>\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    CHECK(near(find_image(scene, "Box")->y, 0) && near(find_image(scene, "Tab")->x, 0));
    ssr_tweaks_set_number(tweaks, "Menu", "Maximise", 1);
    ssr_tweaks_set(tweaks, "Menu", "Tab", "Kits");
    ssr_scene_update(scene, &env, 16);
    CHECK(near(find_image(scene, "Box")->y, -100) && near(find_image(scene, "Tab")->x, 50));
    ssr_tweaks_set_number(tweaks, "Menu", "Maximise", 0);
    ssr_scene_update(scene, &env, 16);
    CHECK(near(find_image(scene, "Box")->y, 0));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// A drag on a swipe panel scrolls the list under it, and the list keeps
// the speed of the drag, then slows down and stops at its ends.
// The rows of a list can be in a child of the list: they count too.
static void test_scrolls_rows_in_a_child(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Combo", "Scroll", -2000);
    SSRUiTree *tree;
    layout = "Rect:(0,0,0,0)\n"
             "\tScrollingList(Combo,Scroll,300,Combo,ScrollVel)\n"
             "{\n"
             "\tRect:(0,0,400,72) ref:Table\n"
             "\t{\n"
             "\t\tDraw:(Row,0,0,400,72)\n"
             "\t\tDraw:(Row,0,928,400,72)\n"
             "\t}\n"
             "}\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    // The lowest place is 300 - 1000, and the list may go 64 past it.
    float scroll = (float)ssr_tweaks_number(tweaks, "Combo", "Scroll");
    CHECK(scroll < -700 && scroll > -764);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

static void test_scrolls_a_list_by_a_swipe(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    SSRUiTree *tree;
    layout = "Rect:(0,0,400,300)\n"
             "\tSwipePanel(Combo,Scroll,Combo,ScrollVel)\n"
             "{\n"
             "\tRect:(0,0,0,0) ref:List\n"
             "\t\tScrollingList(Combo,Scroll,300,Combo,ScrollVel)\n"
             "\t{\n"
             "\t\tDraw:(Entry,0,0,400,1000)\n"
             "\t}\n"
             "}\n";
    SSRUiFiles files = {read_layout, NULL};
    tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    CHECK(near(find_image(scene, "Entry")->y, 0));
    ssr_scene_touch(scene, 1, 200, 200);
    ssr_scene_update(scene, &env, 16);
    ssr_scene_update(scene, &env, 16);
    ssr_scene_touch(scene, 1, 200, 150);
    ssr_scene_update(scene, &env, 16);
    CHECK(ssr_scene_dragged(scene));
    CHECK(near((float)ssr_tweaks_number(tweaks, "Combo", "Scroll"), -50));
    // The touch goes up while it moves: the list keeps the speed. The list
    // moves in the same frame, then slows by 0.95.
    ssr_scene_touch(scene, 0, 200, 150);
    ssr_scene_update(scene, &env, 16);
    CHECK(near(find_image(scene, "Entry")->y, -100));
    CHECK(near((float)ssr_tweaks_number(tweaks, "Combo", "ScrollVel"), -47.5f));
    for (int i = 0; i < 400; i++) ssr_scene_update(scene, &env, 16);
    float at = (float)ssr_tweaks_number(tweaks, "Combo", "Scroll");
    CHECK(at < -100 && at > -710);
    // A long throw stops at the end of the list: 300 - 1000 = -700.
    ssr_tweaks_set_number(tweaks, "Combo", "ScrollVel", -500);
    for (int i = 0; i < 400; i++) ssr_scene_update(scene, &env, 16);
    CHECK(fabsf((float)ssr_tweaks_number(tweaks, "Combo", "Scroll") + 700) < 1);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// Embed:(Name) draws nothing. DrawEmbedded:(Name) draws the children of
// the embed that showed last, as the agent panel of the game menu does.
static void test_draws_embedded_content(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "Menu", "OwnedAgent", 1);
    SSRUiTree *tree;
    SSRScene *scene = build("PushOrigin:(100,0)\n"
                            "DrawEmbedded:(AgentEmbed)\n"
                            "PopOrigin\n"
                            "Rect:(0,0,0,0)\n"
                            "\tSelector(Menu,OwnedAgent)\n"
                            "{\n"
                            "\tEmbed:(AgentEmbed)\n"
                            "\t{\n"
                            "\t\tDraw:(NoAgent,10,0,10,10)\n"
                            "\t}\n"
                            "\tEmbed:(AgentEmbed)\n"
                            "\t{\n"
                            "\t\tDraw:(Agent1,10,0,10,10)\n"
                            "\t}\n"
                            "}\n",
                            tweaks, &tree);
    // The first frame has no embed yet when the DrawEmbedded draws.
    CHECK(ssr_scene_count(scene) == 0);
    SSRSceneEnv env = test_env(tweaks);
    ssr_scene_update(scene, &env, 16);
    CHECK(ssr_scene_count(scene) == 1 && find_image(scene, "Agent1") && near(find_image(scene, "Agent1")->x, 10));
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

// A bar follows its tweak (c_ProgressBar), and AnimateProgress moves the
// shown value from the value before to the new value after its delay.
static void test_progress_bars(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    ssr_tweaks_set_number(tweaks, "P", "Fill", 25);
    ssr_tweaks_set_number(tweaks, "P", "Rel", 50);
    ssr_tweaks_set_number(tweaks, "P", "Rel_Before", 20);
    ssr_tweaks_set_number(tweaks, "P", "Rel_Displayed", 20);
    layout = "Draw:(Fill,0,0,200,20)\n"
             "\tProgressBar(P,Fill,0,100,20)\n"
             "Draw:(Rel,0,40,200,20)\n"
             "\tAnimateProgress(P,Rel,H,O,B,20,200,10,0,0,0,100)\n"
             "\tProgressBar(P,Rel_Displayed,0,100,20)\n";
    SSRUiFiles files = {read_layout, NULL};
    SSRUiTree *tree = ssr_ui_parse("Screen.uis", &files);
    SSRSceneEnv env = test_env(tweaks);
    SSRScene *scene = ssr_scene_open(tree, &env);
    ssr_scene_update(scene, &env, 0);
    // 20 + (200 - 20) * 0.25 = 65.
    CHECK(fabsf(find_image(scene, "Fill")->w - 65) < 0.01f);
    CHECK(fabsf(find_image(scene, "Rel")->w - 56) < 0.01f);
    // The value waits 10 frames, then moves.
    for (int i = 0; i < 9; i++) ssr_scene_update(scene, &env, 16);
    CHECK(ssr_tweaks_number(tweaks, "P", "Rel_Displayed") == 20);
    CHECK(ssr_tweaks_number(tweaks, "P", "Rel_Before") == 50);
    for (int i = 0; i < 5; i++) ssr_scene_update(scene, &env, 16);
    double shown = ssr_tweaks_number(tweaks, "P", "Rel_Displayed");
    CHECK(shown > 20 && shown < 50);
    for (int i = 0; i < 200; i++) ssr_scene_update(scene, &env, 16);
    CHECK(ssr_tweaks_number(tweaks, "P", "Rel_Displayed") == 50);
    CHECK(fabsf(find_image(scene, "Rel")->w - 110) < 0.01f);
    // A value over the max fills the bar and no more.
    ssr_tweaks_set_number(tweaks, "P", "Fill", 140);
    ssr_scene_update(scene, &env, 16);
    CHECK(fabsf(find_image(scene, "Fill")->w - 200) < 0.01f);
    ssr_scene_free(scene);
    ssr_ui_free(tree);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_draws_embedded_content();
    test_scrolls_a_list_by_a_swipe();
    test_scrolls_rows_in_a_child();
    test_fills_lists_and_changes_elements();
    test_readouts_conditions_and_click_actions();
    test_converges_after_a_delay_and_on_press();
    test_projects_a_sheet();
    test_clones_kit_colours();
    test_runs_a_block_by_a_tweak();
    test_moves_copies_and_sorts_by_y();
    test_runs_expressions();
    test_plays_an_animation();
    test_mixes_colours_and_orders_layers();
    test_emits_particles();
    test_definitions_do_not_draw();
    test_draws_a_crowd();
    test_panels_of_the_screen_and_of_origins();
    test_sets_a_variable_many_times();
    test_tiles();
    test_clicks_and_their_sounds();
    test_text_expressions_and_set();
    test_places_images_with_origins_and_variables();
    test_shows_items_by_tweak();
    test_text_areas_layers_and_clips();
    test_progress_bars();
    return TEST_RESULT();
}
