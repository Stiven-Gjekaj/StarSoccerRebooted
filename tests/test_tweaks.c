// Tests ssr_tweaks.

#include <math.h>
#include <string.h>

#include "ssr_test.h"
#include "ssr_tweaks.h"
#include "ssr_zipwriter.h"

static void test_sets_and_gets_values(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Player", "Energy"), "") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Player", "Energy") == 0);

    ssr_tweaks_set_number(tweaks, "Player", "Energy", 80);
    CHECK(ssr_tweaks_number(tweaks, "player", "ENERGY") == 80);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Player", "Energy"), "80") == 0);

    ssr_tweaks_set(tweaks, "Menu", "ItemString", "Boots");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Boots") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Menu", "ItemString") == 0);
    ssr_tweaks_set(tweaks, "Menu", "ItemString", "Balls");
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Menu", "ItemString"), "Balls") == 0);
    ssr_tweaks_free(tweaks);
}

static void test_compares_values(void) {
    SSRTweaks *tweaks = ssr_tweaks_create();
    CHECK(ssr_tweaks_equals(tweaks, "Pitch", "WeatherType", "0"));
    CHECK(!ssr_tweaks_equals(tweaks, "Pitch", "WeatherType", "1"));
    ssr_tweaks_set(tweaks, "Pitch", "WeatherType", "1.0");
    CHECK(ssr_tweaks_equals(tweaks, "Pitch", "WeatherType", "1"));
    ssr_tweaks_set(tweaks, "Menu", "ItemString", "Boots");
    CHECK(ssr_tweaks_equals(tweaks, "Menu", "ItemString", "Boots"));
    CHECK(!ssr_tweaks_equals(tweaks, "Menu", "ItemString", "boots"));
    ssr_tweaks_set_number(tweaks, "Minigames", "Outcome", -2);
    CHECK(ssr_tweaks_equals(tweaks, "Minigames", "Outcome", "-2"));
    ssr_tweaks_free(tweaks);
}

static void test_loads_the_start_values(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/TweakValues.txt", ssr_test_directory("tweaks"));
    FILE *file = fopen(path, "wb");
    fputs("Version:(196)\r\n"
          "Float:(Outside,No category,5,1)\r\n"
          "Category:(Crowd) NOLOCAL\r\n"
          "\tFloat:(Active,Crowd Density,1,1)\r\n"
          "\tFloat:(HatLikelihood,Hat Likelihood,30,0.01) // a comment\r\n"
          "\tColour:(TrouserCol0,Trouser Col 0,808080FF)\r\n"
          "\tVar:(SetupState,Setup state,2,1)\r\n"
          "\tString:(Cohort,Name of active cohort,None)\r\n"
          "\tString:(CohortURL,Optional direct cohort url,)\r\n"
          "\tRange:(YStart,Y Start,-40,40,1)\r\n"
          "\tRange:(Rate,Rate,0.2,0.3,10)\r\n"
          "\tLinearBuff:(A-B,X.Y,1,10,Z.W,0,100,HRLR)\r\n"
          "EndCategory\r\n",
          file);
    fclose(file);
    SSRTweaks *tweaks = ssr_tweaks_create();
    CHECK(ssr_tweaks_load(tweaks, path) == 8);
    double min = 0, max = 0;
    CHECK(ssr_tweaks_range(tweaks, "Crowd", "Rate", &min, &max) && fabs(min - 2) < 1e-9 && fabs(max - 3) < 1e-9);
    CHECK(!ssr_tweaks_range(tweaks, "Crowd", "Active", &min, &max));
    CHECK(ssr_tweaks_kind(tweaks, "Crowd", "Active") == SSR_TWEAK_FLOAT);
    CHECK(ssr_tweaks_kind(tweaks, "Crowd", "TrouserCol0") == SSR_TWEAK_COLOUR);
    CHECK(ssr_tweaks_kind(tweaks, "Crowd", "Cohort") == SSR_TWEAK_STRING);
    CHECK(ssr_tweaks_kind(tweaks, "Crowd", "Rate") == SSR_TWEAK_RANGE);
    CHECK(ssr_tweaks_kind(tweaks, "Crowd", "Missing") == SSR_TWEAK_NONE);
    ssr_tweaks_set_number(tweaks, "Code", "Count", 1);
    ssr_tweaks_set(tweaks, "Code", "Count", "2");
    CHECK(ssr_tweaks_kind(tweaks, "Code", "Count") == SSR_TWEAK_FLOAT);
    ssr_tweaks_set(tweaks, "Code", "Text", "a");
    CHECK(ssr_tweaks_kind(tweaks, "Code", "Text") == SSR_TWEAK_STRING);
    CHECK(ssr_tweaks_number(tweaks, "Crowd", "Active") == 1);
    CHECK(ssr_tweaks_number(tweaks, "Crowd", "HatLikelihood") > 0.2999 &&
          ssr_tweaks_number(tweaks, "Crowd", "HatLikelihood") < 0.3001);
    CHECK(ssr_tweaks_colour(tweaks, "Crowd", "TrouserCol0", 0) == 0x808080FF);
    CHECK(ssr_tweaks_colour(tweaks, "Crowd", "Cohort", 7) == 7);
    CHECK(ssr_tweaks_number(tweaks, "Crowd", "SetupState") == 2);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "Crowd", "Cohort"), "None") == 0);
    CHECK(ssr_tweaks_number(tweaks, "Crowd", "YStart") == -40);
    CHECK(strcmp(ssr_tweaks_get(tweaks, "", "Outside"), "") == 0);
    CHECK(ssr_tweaks_load(tweaks, "/nonexistent/TweakValues.txt") == -1);
    ssr_tweaks_free(tweaks);
}

int main(void) {
    test_loads_the_start_values();
    test_sets_and_gets_values();
    test_compares_values();
    return TEST_RESULT();
}
