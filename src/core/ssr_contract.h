// ssr_contract: the contract offers of the clubs (c_TContractOffer) and the
// transfer value of the player.

#ifndef SSR_CONTRACT_H
#define SSR_CONTRACT_H

#include "ssr_career.h"

typedef struct {
    int clubId;
    // 1 for an offer of the transfer at the end of a season.
    int endOfSeason;
    int wage, goalBonus, assistBonus;
    // 1 after a negotiation that raised the wage.
    int negotiationSuccess;
} SSRContractOffer;

// The statics of c_TContractOffer: they are not in the save.
typedef struct {
    // The number of offers in a row from one club, and that club.
    int offerCount, offerClubId;
    // The transfer window shows as open, and its rumour was told.
    int windowOpen, doneRumour;
} SSRContractState;

// c_TContractOffer.GetOffer: the offer of the club. The club of the player
// gives the base offer. Another club gives more on its second and third
// offer in a row.
void ssr_contract_offer(SSRContractState *state, const SSRCareer *career, int clubId, int endOfSeason,
                        SSRContractOffer *offer);

// c_TContractOffer.IncreaseOffer.
void ssr_contract_increase(SSRContractOffer *offer, int wage, int goalBonus, int assistBonus);

// c_TPlayer.GetSkills: the five skills divided by 3.
float ssr_contract_skills(const SSRCareer *career);

// c_TPlayer.GetTransferValue, in thousands of bux.
int ssr_contract_transfer_value(const SSRCareer *career);

#endif
