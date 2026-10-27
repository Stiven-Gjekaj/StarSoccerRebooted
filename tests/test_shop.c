// Tests ssr_shop: the products of the lifestyle shop.

#include <string.h>

#include "ssr_shop.h"
#include "ssr_test.h"

static void test_lists_the_products_by_price(void) {
    int priced = 0;
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = ssr_shop_item(i);
        CHECK(item != NULL);
        if (!item) continue;
        priced += item->price > 0;
        if (i > 0) CHECK(ssr_shop_item(i - 1)->price <= item->price);
    }
    // c_TShopItem.CountAllItems counts the 60 products.
    CHECK(priced == SSR_SHOP_COUNT && ssr_shop_item(SSR_SHOP_COUNT) == NULL && ssr_shop_item(-1) == NULL);
    // The tattoo is the cheapest product, and the skyscraper the dearest.
    CHECK(strcmp(ssr_shop_item(0)->uid, "item_Tattoo") == 0);
    CHECK(strcmp(ssr_shop_item(SSR_SHOP_COUNT - 1)->uid, "property_SkyScraper") == 0);
    CHECK(ssr_shop_count_by_type(SSR_SHOP_VEHICLE) == 20);
    // The vehicles in the order of the price: the bicycle, then the hang
    // glider.
    CHECK(strcmp(ssr_shop_nth_of_type(SSR_SHOP_VEHICLE, 2)->uid, "vehicle_HangGlider") == 0);
    CHECK(ssr_shop_nth_of_type(SSR_SHOP_VEHICLE, 21) == NULL);
    const SSRShopItem *stable = ssr_shop_nth_of_type(SSR_SHOP_PROPERTY, 13);
    CHECK(stable && strcmp(stable->uid, "property_Stable") == 0 && stable->lifeType == SSR_LIFE_NONE);
}

static void test_wears_the_products(void) {
    int items[50] = {0}, vehicles[50] = {0}, properties[50] = {0};
    items[10] = 1;      // the tattoo needs no care
    vehicles[0] = 1;    // the bicycle, the cheapest with care
    properties[19] = 1; // the skyscraper, the dearest
    SSRRandom random = {SSR_RANDOM_START};
    ssr_shop_wear(&random, items, vehicles, properties);
    CHECK(items[10] == 1);
    // The skyscraper: 0.9 and 0.1 to 0.5, cut to 1.
    CHECK(properties[19] == 2);
    // The bicycle comes 57th or 58th of the 58 products with care, so it
    // has an f of 14.9 or 15.15.
    CHECK(vehicles[0] == 1 + 15);
    // Products that the player does not own do not wear.
    CHECK(items[0] == 0 && vehicles[1] == 0);
    // The list is in the order of the price again.
    CHECK(strcmp(ssr_shop_item(0)->uid, "item_Tattoo") == 0);
}

static void test_chooses_the_boots_of_the_shop(void) {
    int available[SSR_BOOT_COUNT + 1];
    // Year 1, age 16, no boots: the pure, the two stickers, the two boots
    // above 21, and each sixth other boot from the third.
    ssr_shop_boots_available(1, 0, 10, 26, available);
    int shown[SSR_BOOT_COUNT + 1] = {0};
    shown[1] = shown[10] = shown[22] = shown[23] = shown[4] = shown[11] = shown[17] = 1;
    for (int id = 1; id <= SSR_BOOT_COUNT; id++) CHECK(available[id] == shown[id]);
    // The boots that the player wears always show, a hard boot too.
    ssr_shop_boots_available(2, 25, 10, 26, available);
    CHECK(available[25] == 1 && available[1] == 0 && available[26] == 0);
    CHECK(ssr_shop_boot(21)->control == 1 && ssr_shop_boot(27) == NULL && ssr_shop_boot(0) == NULL);
}

int main(void) {
    test_chooses_the_boots_of_the_shop();
    test_wears_the_products();
    test_lists_the_products_by_price();
    return TEST_RESULT();
}
