// ssr_shop: the products of the lifestyle shop (c_TShopItem): the items,
// the vehicles, and the properties, with their prices and the kind of care
// that each one needs.

#ifndef SSR_SHOP_H
#define SSR_SHOP_H

#include "ssr_random.h"

#define SSR_SHOP_COUNT 60

typedef enum {
    SSR_SHOP_ITEM = 1,
    SSR_SHOP_VEHICLE = 2,
    SSR_SHOP_PROPERTY = 3,
} SSRShopType;

// The care that a product needs when it wears out (lifetype).
typedef enum {
    SSR_LIFE_NONE,
    SSR_LIFE_REPAIR,
    SSR_LIFE_UPGRADE,
    SSR_LIFE_CLEAN,
    SSR_LIFE_RESTORE,
    SSR_LIFE_REPLACE,
} SSRLifeType;

typedef struct {
    SSRShopType type;
    // The id from 1. The place in the array of the player is id - 1.
    int id;
    // The language tag of the name, for example "item_Phone".
    const char *uid;
    int price;
    SSRLifeType lifeType;
} SSRShopItem;

// The products in the order of the list of the game (m_glist): sorted by
// the price, low first, with the quicksort of the game.
const SSRShopItem *ssr_shop_item(int index);

// c_TShopItem.CountItemsByType.
int ssr_shop_count_by_type(SSRShopType type);

// The product number n (from 1) of the type, in the order of the list
// (GetRandomItemByType without the random number). NULL when there is
// none.
const SSRShopItem *ssr_shop_nth_of_type(SSRShopType type, int n);

// c_TShopItem.UpdateLife after a match: each owned product that needs care
// wears. The dearest product wears 1 a match, the cheapest about 15. The
// arrays are the items, the vehicles, and the properties of the player.
void ssr_shop_wear(SSRRandom *random, int *items, int *vehicles, int *properties);

#endif
