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

// The boots of the shop (c_SoccerProducts, the free version). Ids 24 to 26
// are bought with real money in the game, so the remake shows them only
// when the player wears them (the old pro boots).
#define SSR_BOOT_COUNT 26

typedef struct {
    int id;
    // The name, for example "NS-Pure". The title is the tag "Title_" + name.
    const char *name;
    int price, uses, pace, power, technique;
    // 0 none, 1 control, 2 swerve, 3 both.
    int control;
    int hard;
} SSRBoot;

// The boots with the id from 1, or NULL.
const SSRBoot *ssr_shop_boot(int id);

// Product_Boot.UpdateAvailability: available[id] is 1 for each boot that
// the shop shows. A sixth of the other soft boots shows, by the age.
// popular and value are the ids of the boots of the stickers.
void ssr_shop_boots_available(int year, int bootId, int popular, int value, int available[SSR_BOOT_COUNT + 1]);

#endif
