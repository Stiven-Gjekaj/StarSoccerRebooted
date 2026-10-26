// Tests ssr_tweaks.

#include <string.h>

#include "ssr_test.h"
#include "ssr_tweaks.h"

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

int main(void) {
    test_sets_and_gets_values();
    test_compares_values();
    return TEST_RESULT();
}
