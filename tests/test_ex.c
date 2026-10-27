// Tests ssr_ex with the examples of the layouts. The test builds its own
// tweaks, variables, and gel.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ssr_ex.h"
#include "ssr_test.h"

typedef struct {
    float values[32];
    char text[256];
    SSRExColour colour;
    char slot[64];
    SSRExColour slotColour;
    int slotMask;
    char shell[64], shellScreen[64];
    SSRExVar vars[8];
    int varCount;
} Fixture;

static Fixture fx;

static const char *localise(void *context, const char *tag) {
    (void)context;
    static const char *TABLE[][2] = {
        {"ENERGY_CAPS_SPACEAFTER", "ENERGY "}, {"nrg_Cans", "Cans"},          {"contract_Length", "$num weeks"},
        {"iap_AgentBoostWage", "+$percent wage"}, {"iap_Title", "Buy $num"}, {"Hi-Score", "Best"},
        {"tab_Items", "items"},                {"Consent_TEXT_ATT_6", "ATT six"},
    };
    for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
        if (strcmp(TABLE[i][0], tag) == 0) return TABLE[i][1];
    }
    return NULL;
}

static SSRExVar *find_var(void *context, const char *name) {
    Fixture *f = context;
    for (int i = 0; i < f->varCount; i++) {
        if (strcmp(f->vars[i].name, name) == 0) return &f->vars[i];
    }
    return NULL;
}

static float simple_var(void *context, char letter) {
    (void)context;
    return letter == 'W' ? 640 : letter == 'H' ? 1136 : 0;
}

static float get(void *context, void *gel, SSRGelProperty property) {
    (void)gel;
    return ((Fixture *)context)->values[property];
}

static void set(void *context, void *gel, SSRGelProperty property, float value) {
    (void)gel;
    ((Fixture *)context)->values[property] = value;
}

static const char *get_text(void *context, void *gel, SSRGelProperty property) {
    (void)gel;
    (void)property;
    return ((Fixture *)context)->text;
}

static void set_text(void *context, void *gel, SSRGelProperty property, const char *text) {
    (void)gel;
    (void)property;
    snprintf(((Fixture *)context)->text, sizeof(fx.text), "%s", text);
}

static SSRExColour get_colour(void *context, void *gel) {
    (void)gel;
    return ((Fixture *)context)->colour;
}

static void set_colour(void *context, void *gel, SSRGelProperty property, const char *slot, SSRExColour colour,
                       int mask) {
    (void)gel;
    Fixture *f = context;
    if (property == SSR_GEL_SLOTCOL) {
        snprintf(f->slot, sizeof(f->slot), "%s", slot ? slot : "");
        f->slotColour = colour;
        f->slotMask = mask;
    } else {
        f->colour = colour;
    }
}

static void set_shell(void *context, const char *shell, const char *screen) {
    Fixture *f = context;
    snprintf(f->shell, sizeof(f->shell), "%s", shell);
    snprintf(f->shellScreen, sizeof(f->shellScreen), "%s", screen);
}

static SSRExHost host;

static void add_var(const char *name, SSRExVarKind kind, float value, const char *text) {
    SSRExVar *v = &fx.vars[fx.varCount++];
    memset(v, 0, sizeof(*v));
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->kind = kind;
    v->f = value;
    snprintf(v->s, sizeof(v->s), "%s", text ? text : "");
}

static void set_float(const char *group, const char *name, double value) {
    char text[64];
    snprintf(text, sizeof(text), "%g", value);
    ssr_tweaks_set_kind(host.tweaks, group, name, text, SSR_TWEAK_FLOAT);
}

static void setup(void) {
    if (host.tweaks) ssr_tweaks_free(host.tweaks);
    memset(&fx, 0, sizeof(fx));
    fx.colour = (SSRExColour){255, 255, 255, 1, 0, 0, 0, 0};
    memset(&host, 0, sizeof(host));
    host.tweaks = ssr_tweaks_create();
    host.context = &fx;
    host.localise = localise;
    host.find_var = find_var;
    host.simple_var = simple_var;
    host.get = get;
    host.set = set;
    host.get_text = get_text;
    host.set_text = set_text;
    host.get_colour = get_colour;
    host.set_colour = set_colour;
    host.set_shell = set_shell;
    host.frameTime = 1;
    set_float("Player", "Energy", 75);
    set_float("Player", "StarBux", 1234);
    set_float("Player", "Cans", 3);
    set_float("Player", "TeamStatusReason", 5);
    set_float("AdSpotSponsors", "ShowStarMan", 1);
    set_float("AdSpotSponsors", "UseStarmanTabletCL", 0);
    ssr_tweaks_set_kind(host.tweaks, "AdSpotSponsors", "SMStarBackColourCL", "#CCCCCCFF", SSR_TWEAK_COLOUR);
    set_float("Classic_Match", "RenderPitch", 1);
    set_float("Classic_Match", "MatchX", 12);
    set_float("AwayCrowd", "Density", 0.8);
    set_float("HomeCrowd", "Density", 0.2);
    set_float("Negotiation", "WageDiff", 250);
    set_float("Negotiation", "TotalDiff", -40);
    set_float("Negotiation", "GoalBonusDiff", 0);
    ssr_tweaks_set_kind(host.tweaks, "Menu", "ItemString", "Shop_Boots_12", SSR_TWEAK_STRING);
    set_float("Menu", "ShowContractTip", 2);
    set_float("Menu", "GameMenuHelpIncrement", 0);
    ssr_tweaks_set_kind(host.tweaks, "Menu", "ItemsOwned", "1/10", SSR_TWEAK_STRING);
    ssr_tweaks_set_kind(host.tweaks, "Menu", "ShopTab", "tab_Items", SSR_TWEAK_STRING);
    set_float("Match", "MatchState", 1);
    set_float("MatchReview", "EarningsTotal", 75);
    set_float("Casino", "Winnings", 120);
    set_float("WatchReplay", "Frame", 50);
    set_float("WatchReplay", "Length", 200);
    set_float("Consent", "ATTFlowScreen", 6);
    ssr_tweaks_set_kind(host.tweaks, "Settings", "VerNum", "4.32", SSR_TWEAK_STRING);
    ssr_tweaks_set_kind(host.tweaks, "Cloud", "CohortTag", "B", SSR_TWEAK_STRING);
    set_float("Agents", "Agent1Duration", 10);
    set_float("Agents", "Agent1WagePercent", 15);
    ssr_tweaks_set_kind(host.tweaks, "SelProduct", "Title", "iap_Title", SSR_TWEAK_STRING);
    ssr_tweaks_set_kind(host.tweaks, "SelProduct", "Amount", "500", SSR_TWEAK_STRING);
    ssr_tweaks_set_range(host.tweaks, "SlotShowerEmitter", "Count", 0, 0);
    set_float("Stable_ForSale", "3_Price", 1500);
}

// Parses the phrase and runs the doodad op on its items. Returns the
// number of items.
static int run(const char *op, const char *phrase) {
    SSRExNode *items[8] = {0};
    int count = ssr_ex_parse(phrase, &host, items, 8);
    if (count >= 2) ssr_ex_operate(op, items[0], items[1], count >= 3 ? items[2] : NULL, &host);
    for (int i = 0; i < count; i++) ssr_ex_free(items[i]);
    return count;
}

static int condition(const char *phrase) {
    SSRExNode *items[8] = {0};
    int count = ssr_ex_parse(phrase, &host, items, 8);
    int value = count >= 1 && ssr_ex_bool(items[0], &host);
    for (int i = 0; i < count; i++) ssr_ex_free(items[i]);
    return value;
}

static int text_is(const char *phrase, const char *expected) {
    setup();
    run("set", phrase);
    if (strcmp(fx.text, expected) != 0) {
        printf("  %s gives \"%s\", not \"%s\"\n", phrase, fx.text, expected);
        return 0;
    }
    return 1;
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

static void test_folds_constants(void) {
    setup();
    SSRExNode *items[4];
    CHECK(ssr_ex_parse("(gel.vis,@W>640)", &host, items, 4) == 2);
    CHECK(ssr_ex_is_const(items[1]) && !ssr_ex_bool(items[1], &host));
    ssr_ex_free(items[0]);
    ssr_ex_free(items[1]);
    CHECK(ssr_ex_parse("(gel.vis,@H>960)", &host, items, 4) == 2);
    CHECK(ssr_ex_is_const(items[1]) && ssr_ex_bool(items[1], &host));
    ssr_ex_free(items[0]);
    ssr_ex_free(items[1]);
    CHECK(condition("(@W=640)"));
    CHECK(!condition("(@W!=640)"));
    run("set", "(gel.y,-128)");
    CHECK(fx.values[SSR_GEL_Y] == -128);
    run("set", "(gel.y,@T+05)");
    CHECK(fx.values[SSR_GEL_Y] == 5);
    run("set", "(gel.x,@L+13)");
    CHECK(fx.values[SSR_GEL_X] == 13);
}

static void test_has_no_precedence(void) {
    setup();
    run("set", "(gel.x,[@W/2]-[3*40/2]+(40*[twk(Menu,GameMenuHelpIncrement)-1]))");
    CHECK(fx.values[SSR_GEL_X] == 220);
    fx.values[SSR_GEL_H] = 100;
    run("set", "(gel.hy,(gel.h / 400.0) * 200)");
    CHECK(fx.values[SSR_GEL_HY] == 50);
    run("set", "(gel.x,115+[[twk(WatchReplay,Frame)/twk(WatchReplay,Length)]*495])");
    CHECK(near(fx.values[SSR_GEL_X], 238.75f));
    add_var("%xPos", SSR_VAR_FLOAT, 362.5f, NULL);
    add_var("%r", SSR_VAR_FLOAT, 0, NULL);
    add_var("%x", SSR_VAR_FLOAT, 3, NULL);
    const char *frame = "(twk(WatchReplay,Frame),min(1,max(0,[[%xPos-115]/495]))*twk(WatchReplay,Length))";
    run("set", frame);
    CHECK(ssr_tweaks_number(host.tweaks, "WatchReplay", "Frame") == 100);
    fx.vars[0].f = 0;
    run("set", frame);
    CHECK(ssr_tweaks_number(host.tweaks, "WatchReplay", "Frame") == 0);
    run("set", "(%r,2+3*4)");
    CHECK(fx.vars[1].f == 20);
    run("set", "(%r,2+min(1,5)*4)");
    CHECK(fx.vars[1].f == 6);
    run("set", "(%r,0<%x<5)");
    CHECK(fx.vars[1].f == 1);
    fx.vars[2].f = 7;
    run("set", "(%r,0<%x<5)");
    CHECK(fx.vars[1].f == 0);
    CHECK(text_is("(gel.text,a b)", "a b"));
}

static void test_reads_tweaks(void) {
    setup();
    run("set", "(gel.x,twk(Classic_Match,MatchX))");
    CHECK(fx.values[SSR_GEL_X] == 12);
    CHECK(condition("(twk(Classic_Match,RenderPitch)>0)"));
    const char *crowd = "(twk(AwayCrowd,Density)>0.3 or(twk(HomeCrowd,Density)>0.3))";
    CHECK(condition(crowd));
    set_float("AwayCrowd", "Density", 0.2);
    CHECK(!condition(crowd));
    add_var("%bux", SSR_VAR_FLOAT, 10, NULL);
    CHECK(condition("(twk(MatchReview,EarningTotal)-%bux<=0.1)"));
    fx.vars[0].f = -5;
    CHECK(!condition("(twk(MatchReview,EarningTotal)-%bux<=0.1)"));
    CHECK(condition("(twk(AdSpotSponsors,ShowStarMan)=1)"));
    CHECK(!condition("(twk(AdSpotSponsors,ShowStarMan)=1 and(twk(AdSpotSponsors,UseStarmanTabletCL)=1))"));
    run("set", "(gel.colour,twk(AdSpotSponsors,SMStarBackColourCL))");
    CHECK(fx.colour.r == 204 && fx.colour.g == 204 && fx.colour.b == 204 && fx.colour.a == 1 && fx.colour.ma == 0);
    run("set", "(twk(SlotShowerEmitter,Count)(0.5),15)");
    double min = 0, max = 0;
    CHECK(ssr_tweaks_range(host.tweaks, "SlotShowerEmitter", "Count", &min, &max) && min == 15 && max == 15);
    set_float("HomeCrowd", "Density", 0.5);
    fx.values[SSR_GEL_VIS] = 1;
    run("set", "(gel.vis,twk(HomeCrowd,Density))");
    CHECK(fx.values[SSR_GEL_VIS] == 0);
}

static int fmt_is(float value, const char *phrase, const char *expected) {
    setup();
    add_var("%c", SSR_VAR_FLOAT, value, NULL);
    run("set", phrase);
    if (strcmp(fx.text, expected) != 0) {
        printf("  %s with %g gives \"%s\", not \"%s\"\n", phrase, value, fx.text, expected);
        return 0;
    }
    return 1;
}

static void test_makes_text(void) {
    CHECK(fmt_is(1234, "(gel.text,fmt(%c,X))", "1234"));
    CHECK(fmt_is(1233.9f, "(gel.text,fmt(%c,X))", "1233"));
    CHECK(text_is("(gel.text,loc(ENERGY_CAPS_SPACEAFTER)::fmt(twk(Player,Energy)+0.5,XX)::\"%\")", "ENERGY 75%"));
    CHECK(fmt_is(7.2f, "(gel.text,loc(ENERGY_CAPS_SPACEAFTER)::fmt(%c+0.5,XX)::\"%\")", "ENERGY 07%"));
    CHECK(text_is("(gel.text,\"(\"::fmt(twk(Negotiation,WageDiff),ZX)::\")\")", "(+250)"));
    CHECK(text_is("(gel.text,\"(\"::fmt(twk(Negotiation,TotalDiff),ZX)::\")\")", "(-40)"));
    CHECK(text_is("(gel.text,\"(\"::fmt(twk(Negotiation,GoalBonusDiff),ZX)::\")\")", "(+0)"));
    CHECK(fmt_is(-12.7f, "(gel.text,\"* \"::fmt(%c,UX))", "* 12"));
    CHECK(fmt_is(2.96f, "(gel.text,fmt(%c,X.X))", "2.9"));
    CHECK(fmt_is(3.14159f, "(gel.text,fmt(%c,X.X))", "3.1"));
    CHECK(text_is("(gel.text,fmt(twk(Player,Cans),X)::\" \"::loc(nrg_Cans))", "3 Cans"));
    CHECK(text_is("(gel.text,loc(contract_Length) rep($num, fmt(twk(Agents,Agent1Duration),X)))", "10 weeks"));
    CHECK(text_is("(gel.text,loc(iap_AgentBoostWage) rep($percent, fmt(twk(Agents,Agent1WagePercent),X)::\"%\"))",
                  "+15% wage"));
    CHECK(text_is("(gel.text,loc(twk(SelProduct,Title)) rep($num,twk(SelProduct,Amount)))", "Buy 500"));
    CHECK(text_is("(gel.text,\"* \"::twk(Stable_ForSale,3_Price))", "* 1500"));
    setup();
    set_float("Stable_ForSale", "3_Price", 12.5);
    run("set", "(gel.text,\"* \"::twk(Stable_ForSale,3_Price))");
    CHECK(strcmp(fx.text, "* 12.5") == 0);
    CHECK(text_is("(gel.text,twk(Settings,VerNum) :: \"_\" :: twk(Cloud,CohortTag))", "4.32_B"));
    CHECK(text_is("(gel.text,twk(Menu,ItemsOwned) :: \" \" :: loc(twk(Menu,ShopTab)))", "1/10 items"));
    CHECK(text_is("(gel.text,loc(\"Consent_TEXT_ATT_\"::twk(Consent,ATTFlowScreen)))", "ATT six"));
    setup();
    set_float("Menu", "HiScore", 7);
    run("set", "(gel.text,loc(Hi-Score)::\": \"::twk(Menu,HiScore))");
    CHECK(strcmp(fx.text, "Best: 7") == 0);
    CHECK(text_is("(gel.text,loc(Hi-Score)::\": \"::twk(Menu,HiScore))", "Best: ::"));
    setup();
    CHECK(run("set", "(gel.text,\"x\"::twk(No,Such))") == 3);
    CHECK(strcmp(fx.text, "x") == 0);
    CHECK(fmt_is(5, "(gel.text,%c)", "5.0"));
    CHECK(text_is("(gel.text,lerp(1,2,0.5))", ""));
}

static void test_tests_lists_and_booleans(void) {
    setup();
    fx.values[SSR_GEL_VIS] = 0;
    run("set", "(gel.vis(twk(Match,MatchState) oneof(0,1,2)))");
    CHECK(fx.values[SSR_GEL_VIS] == 1);
    set_float("Match", "MatchState", 3);
    run("set", "(gel.vis(twk(Match,MatchState) oneof(0,1,2)))");
    CHECK(fx.values[SSR_GEL_VIS] == 0);
    run("set", "(gel.vis(twk(Player,TeamStatusReason) oneof(1,5)) and(twk(Menu,GameMenuHelpIncrement)=0))");
    CHECK(fx.values[SSR_GEL_VIS] == 1);
    run("set", "(gel.vis((twk(Player,TeamStatusReason) oneof(3,4)) or (twk(Menu,GameMenuHelpIncrement) oneof(7,8))))");
    CHECK(fx.values[SSR_GEL_VIS] == 0);
    add_var("%hashed_uid", SSR_VAR_STRING, 0, "Boots_12");
    CHECK(condition("(twk(Menu,ItemString) endswith(%hashed_uid))"));
    fx.vars[0].s[0] = 0;
    CHECK(condition("(twk(Menu,ItemString) endswith(%hashed_uid))"));
    snprintf(fx.vars[0].s, sizeof(fx.vars[0].s), "Balls");
    CHECK(!condition("(twk(Menu,ItemString) endswith(%hashed_uid))"));
    CHECK(condition("(not(imgdownloaded(CLSMNewspaperHeader) and(twk(AdSpotSponsors,ShowStarMan)=1)))"));
    CHECK(condition("(not(imgdownloaded(CLWNHeaderBG)))"));
    CHECK(!condition("((twk(Player,Energy)>=20) and (twk(Player,Energy) < 40))"));
    CHECK(condition("(twk(Casino,Winnings) >= 100 and(twk(Casino,Winnings) < 150))"));
    add_var("%c", SSR_VAR_FLOAT, 1000, NULL);
    CHECK(condition("(%c != twk(Player,StarBux))"));
    add_var("%amt", SSR_VAR_FLOAT, 20, NULL);
    CHECK(condition("((%amt) >= 20 and(%amt < 50))"));
    CHECK(condition("(true)"));
    CHECK(!condition("(twk(Menu,ShowContractTip)!=2)"));
}

static void test_changes_colours_and_values(void) {
    setup();
    set_float("Player", "Energy", 10);
    run("set", "(gel.colour,lerp(#E42719FF,#E4B219FF,min(1,[twk(Player,Energy)/20])))");
    CHECK(fx.colour.r == 228 && near(fx.colour.g, 108.5f) && fx.colour.b == 25 && fx.colour.a == 1);
    set_float("Player", "Energy", 75);
    run("set", "(gel.colour,lerp(#E42719FF,#E4B219FF,min(1,[twk(Player,Energy)/20])))");
    CHECK(fx.colour.r == 228 && fx.colour.g == 178 && fx.colour.b == 25);

    fx.values[SSR_GEL_SCALEX] = 2;
    run("lerp", "(gel.scalex,1,0.2)");
    CHECK(near(fx.values[SSR_GEL_SCALEX], 1.8f));

    fx.colour = (SSRExColour){255, 255, 255, 1, 0, 0, 0, 0};
    host.frameTime = 2;
    run("frlerp", "(gel.colour,#20000000,0.11)");
    CHECK(fabsf(fx.colour.r - 208.638f) < 0.01f && fabsf(fx.colour.g - 201.986f) < 0.01f && near(fx.colour.a, 0.7921f));
    host.frameTime = 1;

    add_var("%timer", SSR_VAR_FLOAT, 0.5f, NULL);
    run("add", "(%timer,FrameTime*0.08)");
    CHECK(near(fx.vars[0].f, 0.58f));
    fx.vars[0].f = 0.5f;
    run("set", "(gel.scalex,lerp(1,1.3,min(1,max(0,%timer))))");
    CHECK(near(fx.values[SSR_GEL_SCALEX], 1.15f));

    fx.colour = (SSRExColour){255, 255, 255, 1, 0, 0, 0, 0};
    run("lerp", "(gel.colour,#808080FFFFFFFF00,0.1)");
    CHECK(near(fx.colour.r, 242.3f) && fx.colour.a == 1 && fx.colour.mr == 0);

    run("set", "(gel.slotcol.rgba(MatchBall_Patches),#00000000000000FF)");
    CHECK(strcmp(fx.slot, "MatchBall_Patches") == 0 && fx.slotMask == 240 && fx.slotColour.ma == 1);
    run("set", "(gel.slotcol.RGBA(Tri01),#FF0000FF)");
    CHECK(strcmp(fx.slot, "Tri01") == 0 && fx.slotMask == 15 && fx.slotColour.r == 255);
    run("set", "(gel.slotcol(MatchBall_Patches),#FFFFFF00)");
    CHECK(fx.slotMask == 255 && fx.slotColour.a == 0);

    run("set", "(shell(Overlay1),\"\")");
    CHECK(strcmp(fx.shell, "Overlay1") == 0 && fx.shellScreen[0] == 0);
    run("set", "(gel.scale,1)");
    CHECK(fx.values[SSR_GEL_SCALE] == 1);
}

static void test_writes_floats_as_java(void) {
    char text[32];
    ssr_ex_java_float(5, text, sizeof(text));
    CHECK(strcmp(text, "5.0") == 0);
    ssr_ex_java_float(0.58f, text, sizeof(text));
    CHECK(strcmp(text, "0.58") == 0);
    ssr_ex_java_float(-12.5f, text, sizeof(text));
    CHECK(strcmp(text, "-12.5") == 0);
    ssr_ex_java_float(1e7f, text, sizeof(text));
    CHECK(strcmp(text, "1.0E7") == 0);
    ssr_ex_java_float(0.0001f, text, sizeof(text));
    CHECK(strcmp(text, "1.0E-4") == 0);
    ssr_ex_java_float(0.1f + 0.2f, text, sizeof(text));
    CHECK(strcmp(text, "0.3") == 0);
}

int main(void) {
    test_folds_constants();
    test_has_no_precedence();
    test_reads_tweaks();
    test_makes_text();
    test_tests_lists_and_booleans();
    test_changes_colours_and_values();
    test_writes_floats_as_java();
    ssr_tweaks_free(host.tweaks);
    return TEST_RESULT();
}
