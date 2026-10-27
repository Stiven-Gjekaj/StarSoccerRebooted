// Tests ssr_contract: the contract offers and the transfer value.

#include <string.h>

#include "ssr_contract.h"
#include "ssr_test.h"

// A career at star level 3 with skills of 30 each: the skills are 50.
static SSRCareer make_player(void) {
    SSRCareer c;
    ssr_career_create(&c, "A", "B", 0, 0, 62, 1, 1, 0);
    c.clubId = 100;
    c.starRating = 250;
    for (int i = 0; i < 30; i++) c.pace[i] = c.power[i] = c.technique[i] = c.vision[i] = c.setPieces[i] = 1;
    return c;
}

static void test_makes_the_offers(void) {
    SSRCareer c = make_player();
    SSRContractState state = {0};
    SSRContractOffer offer;
    CHECK(ssr_contract_skills(&c) == 50);
    // The base offer: 3 + 0.5 * 3, cut to 4; the goal bonus 3 / 4 + 1.
    ssr_contract_offer(&state, &c, 100, 0, &offer);
    CHECK(offer.wage == 4 && offer.goalBonus == 1 && offer.assistBonus == 0 && state.offerCount == 0);
    // Another club: the first offer is the base, the second and the third
    // give more, the fourth is the base again.
    ssr_contract_offer(&state, &c, 101, 0, &offer);
    CHECK(offer.wage == 4 && offer.goalBonus == 1 && offer.clubId == 101);
    ssr_contract_offer(&state, &c, 101, 0, &offer);
    CHECK(offer.wage == 4 && offer.goalBonus == 2 && offer.assistBonus == 1);
    ssr_contract_offer(&state, &c, 101, 0, &offer);
    CHECK(offer.goalBonus == 3 && offer.assistBonus == 2);
    ssr_contract_offer(&state, &c, 101, 0, &offer);
    CHECK(offer.goalBonus == 1 && offer.assistBonus == 0);
    // At the end of the season each offer is a first one.
    ssr_contract_offer(&state, &c, 101, 1, &offer);
    CHECK(state.offerCount == 1 && offer.endOfSeason == 1);
    // A player with no wage gets the first contract.
    c.wage = 0;
    ssr_contract_offer(&state, &c, 100, 0, &offer);
    CHECK(offer.wage == 1 && offer.goalBonus == 1 && offer.assistBonus == 0);
    // A negotiation that raises the wage is a success.
    ssr_contract_increase(&offer, 0, 1, 0);
    CHECK(!offer.negotiationSuccess && offer.goalBonus == 2);
    ssr_contract_increase(&offer, 2, 1, 1);
    CHECK(offer.negotiationSuccess && offer.wage == 3 && offer.assistBonus == 1);
}

static void test_values_the_player(void) {
    SSRCareer c = make_player();
    c.starRating = 50;
    c.apps = 10;
    c.goals = 2;
    c.passes = 25;
    c.assists = 1;
    c.totalRating = 70;
    // Experience 18 + 60; (7 * 2 + 7) * 10 * 100, and 78; at star level 1.
    CHECK(ssr_contract_transfer_value(&c) == 21000);
}

int main(void) {
    test_makes_the_offers();
    test_values_the_player();
    return TEST_RESULT();
}
