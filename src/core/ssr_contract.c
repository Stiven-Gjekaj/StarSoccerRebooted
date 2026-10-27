#include "ssr_contract.h"

#include <string.h>

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

float ssr_contract_skills(const SSRCareer *c) {
    int sum = ssr_career_skill(c->pace) + ssr_career_skill(c->power) + ssr_career_skill(c->technique) +
              ssr_career_skill(c->vision) + ssr_career_skill(c->setPieces);
    return (float)sum / 3.0f;
}

void ssr_contract_offer(SSRContractState *state, const SSRCareer *c, int clubId, int endOfSeason,
                        SSRContractOffer *offer) {
    memset(offer, 0, sizeof(*offer));
    offer->clubId = clubId;
    offer->endOfSeason = endOfSeason;
    if (clubId == c->clubId) {
        state->offerCount = 0;
    } else {
        state->offerCount = endOfSeason || clubId != state->offerClubId ? 1 : state->offerCount + 1;
        state->offerClubId = clubId;
    }
    if (c->wage == 0) {
        offer->wage = offer->goalBonus = 1;
        return;
    }
    // cap_wage_atlevel of 4.32.
    int level = clampi(ssr_career_star_level(c), 1, 60);
    float s = ssr_contract_skills(c);
    int w = (int)((float)level + (s * 0.01f) * (float)level);
    int g = level / 4, a = level / 5;
    if (state->offerCount == 2) {
        offer->wage = w + w / 10;
        offer->goalBonus = g + 2;
        offer->assistBonus = a + 1;
    } else if (state->offerCount == 3) {
        offer->wage = w + w / 5;
        offer->goalBonus = g + 3;
        offer->assistBonus = a + 2;
    } else {
        offer->wage = w;
        offer->goalBonus = g + 1;
        offer->assistBonus = a;
    }
}

void ssr_contract_increase(SSRContractOffer *offer, int wage, int goalBonus, int assistBonus) {
    if (wage > 0) offer->negotiationSuccess = 1;
    offer->wage += wage;
    offer->goalBonus += goalBonus;
    offer->assistBonus += assistBonus;
}

int ssr_contract_transfer_value(const SSRCareer *c) {
    float season = c->seasonApps ? c->seasonTotalRating / (float)c->seasonApps : 0;
    float career = c->apps ? c->totalRating / (float)c->apps : 0;
    float national = c->intApps ? c->intTotalRating / (float)c->intApps : 0;
    // The national part counts the passes and the assists of the career,
    // as the game does.
    int experience = clampi(c->apps + c->goals + c->passes / 5 + c->assists +
                                (c->intApps + c->intGoals + c->passes / 5 + c->assists) * 10,
                            1, 1000);
    float v = career * 2 + national * 5;
    v += c->seasonApps < 10 ? career : season;
    v *= 10;
    v = v * (ssr_contract_skills(c) * 2.0f);
    v += (float)experience;
    int level = clampi(ssr_career_star_level(c), 1, 100);
    return (int)((float)(((int)(v * (float)level)) / 1000) * 1000.0f);
}
