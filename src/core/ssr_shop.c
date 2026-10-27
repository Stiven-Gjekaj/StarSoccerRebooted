#include "ssr_shop.h"

#include <stddef.h>

#include "ssr_sort.h"

// The products in the order of c_TShopItem.SetUp.
static const SSRShopItem PRODUCTS[SSR_SHOP_COUNT] = {
    {SSR_SHOP_ITEM, 1, "item_Phone", 10, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 2, "item_GamesConsole", 15, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 3, "item_MusicPlayer", 20, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 4, "item_Tablet", 40, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 5, "item_TV", 35, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 6, "item_DesignerSuit", 50, SSR_LIFE_REPAIR},
    {SSR_SHOP_ITEM, 7, "item_SilverChain", 65, SSR_LIFE_CLEAN},
    {SSR_SHOP_ITEM, 8, "item_GoldRing", 70, SSR_LIFE_CLEAN},
    {SSR_SHOP_ITEM, 9, "item_Earrings", 75, SSR_LIFE_CLEAN},
    {SSR_SHOP_ITEM, 10, "item_Watch", 80, SSR_LIFE_CLEAN},
    {SSR_SHOP_ITEM, 11, "item_Tattoo", 5, SSR_LIFE_NONE},
    {SSR_SHOP_ITEM, 12, "item_PoolTable", 25, SSR_LIFE_REPAIR},
    {SSR_SHOP_ITEM, 13, "item_ArcadeMachine", 30, SSR_LIFE_REPAIR},
    {SSR_SHOP_ITEM, 14, "item_PinballTable", 45, SSR_LIFE_REPAIR},
    {SSR_SHOP_ITEM, 15, "item_Laptop", 55, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 16, "item_Jukebox", 60, SSR_LIFE_REPAIR},
    {SSR_SHOP_ITEM, 17, "item_HomeCinema", 85, SSR_LIFE_UPGRADE},
    {SSR_SHOP_ITEM, 18, "item_ArtSculpture", 90, SSR_LIFE_RESTORE},
    {SSR_SHOP_ITEM, 19, "item_AntiqueFurniture", 100, SSR_LIFE_RESTORE},
    {SSR_SHOP_ITEM, 20, "item_Painting", 150, SSR_LIFE_RESTORE},
    {SSR_SHOP_VEHICLE, 1, "vehicle_Bicycle", 10, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 2, "vehicle_Scooter", 30, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 3, "vehicle_SmallCar", 50, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 4, "vehicle_Motorbike", 80, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 5, "vehicle_SUV", 90, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 6, "vehicle_SailingBoat", 60, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 7, "vehicle_SportsCar", 110, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 8, "vehicle_Helicopter", 150, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 9, "vehicle_Yacht", 175, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 10, "vehicle_PrivateJet", 200, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 11, "vehicle_HangGlider", 20, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 12, "vehicle_JetSki", 40, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 13, "vehicle_FishingBoat", 70, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 14, "vehicle_HotAirBalloon", 100, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 15, "vehicle_HoverCraft", 120, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 16, "vehicle_BigRig", 130, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 17, "vehicle_Limo", 140, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 18, "vehicle_Tank", 250, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 19, "vehicle_F1Car", 500, SSR_LIFE_REPAIR},
    {SSR_SHOP_VEHICLE, 20, "vehicle_FighterJet", 1000, SSR_LIFE_REPAIR},
    {SSR_SHOP_PROPERTY, 1, "property_Apartment", 25, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 2, "property_House", 75, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 3, "property_TownHouse", 150, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 4, "property_Cottage", 200, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 5, "property_Stable", 700, SSR_LIFE_NONE},
    {SSR_SHOP_PROPERTY, 6, "property_HolidayVilla", 300, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 7, "property_SkiChalet", 500, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 8, "property_Mansion", 800, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 9, "property_Castle", 1000, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 10, "property_PrivateIsland", 1500, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 11, "property_Bungalow", 50, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 12, "property_Cabin", 100, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 13, "property_BeachHouse", 250, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 14, "property_Cafe", 400, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 15, "property_Windmill", 650, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 16, "property_Farm", 600, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 17, "property_Vineyard", 900, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 18, "property_Hotel", 1250, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 19, "property_ShoppingMall", 2500, SSR_LIFE_RESTORE},
    {SSR_SHOP_PROPERTY, 20, "property_SkyScraper", 5000, SSR_LIFE_RESTORE},
};

static int order[SSR_SHOP_COUNT];
static int ordered;

// Mode 35 of c_TShopItem.Compare: the price, then the id.
static int compare_price(void *context, int a, int b) {
    (void)context;
    if (PRODUCTS[a].price != PRODUCTS[b].price) return PRODUCTS[a].price - PRODUCTS[b].price;
    return PRODUCTS[a].id - PRODUCTS[b].id;
}

static void make_order(void) {
    if (ordered) return;
    for (int i = 0; i < SSR_SHOP_COUNT; i++) order[i] = i;
    ssr_sort(order, SSR_SHOP_COUNT, compare_price, NULL, 0);
    ordered = 1;
}

const SSRShopItem *ssr_shop_item(int index) {
    if (index < 0 || index >= SSR_SHOP_COUNT) return NULL;
    make_order();
    return &PRODUCTS[order[index]];
}

static int *product_value(int *items, int *vehicles, int *properties, const SSRShopItem *item) {
    int *a = item->type == SSR_SHOP_ITEM ? items : item->type == SSR_SHOP_VEHICLE ? vehicles : properties;
    return &a[item->id - 1];
}

void ssr_shop_wear(SSRRandom *random, int *items, int *vehicles, int *properties) {
    make_order();
    // The list goes to the dearest first, then back. Each sort starts from
    // the order before it, so two products of one price can change places.
    ssr_sort(order, SSR_SHOP_COUNT, compare_price, NULL, 1);
    float f = 0.9f;
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = &PRODUCTS[order[i]];
        if (item->lifeType == SSR_LIFE_NONE) continue;
        int *v = product_value(items, vehicles, properties, item);
        // Rnd2(0.1, 0.5) is Rnd3(0.4) + 0.1.
        if (*v > 0) *v += (int)(ssr_rnd(random) * (0.5f - 0.1f) + 0.1f + f);
        f += 0.25f;
    }
    ssr_sort(order, SSR_SHOP_COUNT, compare_price, NULL, 0);
}

int ssr_shop_count_by_type(SSRShopType type) {
    int n = 0;
    for (int i = 0; i < SSR_SHOP_COUNT; i++) n += PRODUCTS[i].type == type;
    return n;
}

const SSRShopItem *ssr_shop_nth_of_type(SSRShopType type, int n) {
    for (int i = 0; i < SSR_SHOP_COUNT; i++) {
        const SSRShopItem *item = ssr_shop_item(i);
        if (item->type == type && --n == 0) return item;
    }
    return NULL;
}

static const SSRBoot BOOTS[SSR_BOOT_COUNT] = {
    {1, "NS-Pure", 3, 5, 5, 5, 5, 0, 0},           {2, "NS-Mercury", 5, 7, 10, 10, 5, 0, 0},
    {3, "NS-Geo", 5, 7, 5, 10, 10, 0, 0},          {4, "NS-Carnivore", 5, 7, 10, 5, 10, 0, 0},
    {5, "NS-Tempo", 10, 10, 10, 10, 10, 0, 0},     {6, "NS-Lema", 15, 10, 15, 15, 10, 0, 0},
    {7, "NS-Viper", 15, 10, 10, 15, 15, 0, 0},     {8, "NS-Neptune", 15, 10, 15, 10, 15, 0, 0},
    {9, "NS-Raptor", 20, 15, 15, 15, 15, 0, 0},    {10, "NS-Cortex", 30, 15, 20, 20, 15, 0, 0},
    {11, "NS-Hunter", 30, 15, 15, 20, 20, 0, 0},   {12, "NS-Bullet", 30, 15, 20, 15, 20, 0, 0},
    {13, "NS-Strike", 40, 20, 20, 20, 20, 0, 0},   {14, "NS-Prey", 50, 20, 25, 25, 20, 0, 0},
    {15, "NS-Cobra", 50, 20, 20, 25, 25, 0, 0},    {16, "NS-Dynamic", 50, 20, 25, 20, 25, 0, 0},
    {17, "NS-Uzi", 100, 25, 25, 25, 25, 0, 0},     {18, "NS-Punk", 150, 30, 30, 30, 30, 0, 0},
    {19, "NS-Falcon", 200, 35, 35, 35, 35, 0, 0},  {20, "NS-Instinct", 250, 40, 40, 40, 40, 0, 0},
    {21, "NS-CNTRL_Mk1", 250, 15, 10, 10, 10, 1, 0}, {22, "NS-CNTRL_Mk2", 400, 30, 20, 20, 20, 1, 0},
    {23, "NS-SWRV_Mk1", 500, 45, 30, 30, 30, 2, 0},  {24, "NS-CNTRL_Mk3", 0, 100, 40, 40, 40, 1, 1},
    {25, "NS-SWRV_Mk2", 0, 100, 40, 40, 40, 2, 1},   {26, "NS-SWRV+CNTRL", 0, 150, 40, 40, 40, 3, 1},
};

const SSRBoot *ssr_shop_boot(int id) {
    return id >= 1 && id <= SSR_BOOT_COUNT ? &BOOTS[id - 1] : NULL;
}

void ssr_shop_boots_available(int year, int bootId, int popular, int value, int available[SSR_BOOT_COUNT + 1]) {
    // GetAge is the year plus 15.
    int a = (year + 15) % 6;
    available[0] = 0;
    for (int id = 1; id <= SSR_BOOT_COUNT; id++) {
        available[id] = 0;
        if ((id == 1 && year == 1) || id == bootId) {
            available[id] = 1;
            continue;
        }
        const SSRBoot *boot = &BOOTS[id - 1];
        // A hard boot has no price in the remake: it shows only when worn.
        if (boot->hard) continue;
        if (id > 21 || id == popular || id == value) {
            available[id] = 1;
            continue;
        }
        if (a == 0) available[id] = 1;
        if (++a > 5) a = 0;
    }
}
