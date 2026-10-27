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

int main(void) {
    test_lists_the_products_by_price();
    return TEST_RESULT();
}
