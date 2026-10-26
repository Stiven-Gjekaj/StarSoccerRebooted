// Tests ssr_scene with layouts that the test holds in memory.

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_scene.h"
#include "ssr_test.h"

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
                            "\tScale(2)\n"
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
    CHECK(glint && glint->colour == 0xFF000080 && glint->additive);

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
                       "Draw:(Hidden,0,0,10,10) $HIDE\n";
    SSRUiTree *tree;
    SSRScene *scene = build(text, tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 3);
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
    SSRScene *scene = build("Img:(Pitch,NewGUI/Pitch.png)\n"
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
                            "}\n",
                            tweaks, &tree);
    CHECK(ssr_scene_count(scene) == 8);
    const SSRSceneItem *big = ssr_scene_item(scene, 0);
    CHECK(big && big->w == 100 && big->h == 50);
    const SSRSceneItem *area = ssr_scene_item(scene, 1);
    CHECK(area && area->kind == SSR_ITEM_AREA && strcmp(area->ref, "PlayButton") == 0);
    const SSRSceneItem *label = ssr_scene_item(scene, 2);
    CHECK(label && label->kind == SSR_ITEM_TEXT && strcmp(label->text, "Play") == 0);
    CHECK(label && label->font == 2 && label->alignX == 'L' && label->alignY == 'T' && label->colour == 0x00FF00FF);
    CHECK(strcmp(ssr_scene_item(scene, 3)->text, "0") == 0);
    CHECK(ssr_scene_item(scene, 4)->kind == SSR_ITEM_CLIP && ssr_scene_item(scene, 6)->kind == SSR_ITEM_UNCLIP);
    // The item of a later layer draws last.
    CHECK(strcmp(ssr_scene_item(scene, 7)->image, "Front") == 0);

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

int main(void) {
    test_panels_of_the_screen_and_of_origins();
    test_sets_a_variable_many_times();
    test_tiles();
    test_clicks_and_their_sounds();
    test_text_expressions_and_set();
    test_places_images_with_origins_and_variables();
    test_shows_items_by_tweak();
    test_text_areas_layers_and_clips();
    return TEST_RESULT();
}
