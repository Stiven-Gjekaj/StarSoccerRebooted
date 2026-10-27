// ssr_career: the player of a career (c_TPlayer of the game) and its save
// file. The save file is the format of the remake: one "name=value" a line,
// with the names of the fields of the game. A list is values with commas.

#ifndef SSR_CAREER_H
#define SSR_CAREER_H

// The game has three save slots.
#define SSR_CAREER_SLOTS 3
// The length of a name: the game takes 11 letters from the keyboard.
#define SSR_CAREER_NAME 32
#define SSR_CAREER_SKILL_LEVELS 100
// The places of the lifestyle items, the vehicles, and the properties.
// The game uses the first 20.
#define SSR_CAREER_ITEMS 50
// The lifestyle shop sells 60 products (c_TShopItem.CountAllItems).
#define SSR_CAREER_SHOP_PRODUCTS 60

// The relations of UpdateRelationship and GetRelationship.
typedef enum {
    SSR_RELATION_BOSS,
    SSR_RELATION_TEAM,
    SSR_RELATION_FANS,
    SSR_RELATION_PARTNER,
    SSR_RELATION_SPONSORS,
    SSR_RELATION_FRIENDS,
    SSR_RELATION_BOSS_INT,
    SSR_RELATION_TEAM_INT,
    SSR_RELATION_FANS_INT,
} SSRRelation;

typedef struct {
    // The day of the career: (year - 1) * 364 + (week - 1) * 7 + day
    // (c_TMyDate.sdate). Day 1 is the first day of year 1.
    int sdate;
    char firstName[SSR_CAREER_NAME];
    char lastName[SSR_CAREER_NAME];
    char fullName[SSR_CAREER_NAME * 2];
    char shortName[SSR_CAREER_NAME];
    // 0 male, 1 female.
    int gender;
    int hardMode;
    // The nation of the player, the nation of the league, and the club.
    int nationId;
    int clubNationId;
    int clubId;
    // Bux, the cash of the game.
    int bank;
    int wage;
    int goalBonus;
    int assistBonus;
    float energy;
    float relationBoss, relationTeam, relationFans;
    // The relations with the boss, the team, and the fans of the nation.
    // They start at 0, so a young player is not picked for the nation.
    float relationBossInt, relationTeamInt, relationFansInt;
    // The relation with the partner (0 is no partner), the sponsors, and
    // the friends (no screen uses the friends in 4.32).
    float relationPartner, relationSponsors, relationFriends;
    // The days of the last time with the friends and with the partner.
    float lastSpendTimeFriends, lastSpendTimePartner;
    // The relationship activities since the last match.
    int spentTime;
    // The income of each sponsor. 0 is no sponsor.
    int sponsors[10];
    // The lifestyle products: 0 is not owned. A value above 0 is the wear
    // of the product, and above 10000 the product is broken.
    int items[SSR_CAREER_ITEMS], vehicles[SSR_CAREER_ITEMS], properties[SSR_CAREER_ITEMS];
    // A shopping trip with the team-mates: -1 none, else the purchases of
    // the trip so far (c_TPlayer.teammatesshoppingcount).
    int teamShopping;
    // The gambling of the player, 0 to 100, and the number of bets of the
    // career (c_TPlayer.gambling and casinocount).
    int gambling, casinoCount;
    // The agent and the trainer of each level: the matches left plus 1,
    // or 0 (c_TPlayer.iapAgent1..3 and iapTrainer1..3).
    int agent[3], trainer[3];
    // The boots that the player wears (0 none) and their matches left, the
    // boots that ran out (-1 none), the boots before a trial, a trial,
    // the old pro boots of the last offer, a free trial, and the matches
    // with no boots.
    int bootId, bootUses, expiredBoots, currentBootId, currentBootUses, tryingBoots;
    int oldProBoots, tryBoots, noBootsMatches;
    // The days of the last contract talk, of the last signature, and of
    // the last hint of the agent.
    int lastNegotiations, lastTransferDate, lastAgentHint;
    // The captaincy: 0 none, 1 the club, 2 the nation, 3 both.
    int captain;
    // 1 until the game menu showed the tip of the contract.
    int showContractTip;
    // The match ball of each kind of match: league, cup, continental, and
    // international (0 is the default ball).
    int ballLeague, ballCup, ballContinent, ballInternational;
    // 1 before the first match of the career (c_TPlayer.m_debut).
    int debut;
    // The day of the last message of the nation that did not pick the
    // player (c_TPlayer.internationalnotpickedmsg).
    int intNotPickedMessage;
    // The skills: the score of each level of each skill. 0 is not trained.
    int technique[SSR_CAREER_SKILL_LEVELS];
    int pace[SSR_CAREER_SKILL_LEVELS];
    int vision[SSR_CAREER_SKILL_LEVELS];
    int setPieces[SSR_CAREER_SKILL_LEVELS];
    int power[SSR_CAREER_SKILL_LEVELS];
    // The energy drinks of each kind.
    int nrg[10];
    float ratings[10];
    // The matches, the goals, the assists, the passes, and the star man
    // awards of the career with the clubs.
    int apps, goals, assists, passes, starMan;
    // The counts of this season of the club, and of the career with the
    // nation (c_TPlayer.season* and int*).
    int seasonApps, seasonGoals, seasonAssists, seasonPasses, seasonHatTricks, seasonStarMan, seasonChances;
    float seasonTotalRating, totalRating;
    int intApps, intGoals, intAssists, intPasses, intHatTricks, intStarMan, intChances;
    float intTotalRating;
    // The ratings of the last five matches, the oldest first.
    int form[5], intForm[5];
    // The awards of the player of the year, and 1 after a continental final
    // won in this season.
    int potyAwards, wonMajorAward;
    // The star rating (c_TPlayer.m_starrating). The star level is
    // starRating / 100 + 1.
    int starRating;
    int retired;
    // The news that the next screen shows, or "".
    char headline[512];
    // The news of a cup winner that the results show next, or "".
    char headlineResult[256];
    // The race horses that the player owns: "id:energy:strength:colour#"
    // for each (THorse.GetHorsesOwned).
    char horses[2048];
    // 1 when the news of this match week is done: the next Play goes to
    // the match (c_TPlayer.newsdone).
    int newsDone;
    // The number of tips that the game menu showed (c_TPlayer.tipcount).
    int tipCount;
    // The skill of the player in interviews, and the day of the last one.
    int interviewSkill, lastInterview;
    char gameVersion[16];
} SSRCareer;

// The start of a new career (c_TPlayer.m_Create). The values come from
// the game: the bank, the relations, the cans, the ratings, and the first
// contract (wage, goal bonus, assist bonus).
void ssr_career_create(SSRCareer *career, const char *first, const char *last, int gender, int hardMode,
                       int clubNationId, int wage, int goalBonus, int assistBonus);

// The value of a skill: the sum of its levels, from 1 to 100
// (GSPlayerUtility.GetSkill).
int ssr_career_skill(const int *levels);

// c_TPlayer.UpdateRating: the rating of a skill (0 short passing, 1 long
// passing, 2 finishing, 3 long shots, 4 heading, 5 crossing, 6 set pieces,
// 7 dribbling, 8 defence) goes up after a success and down after a fault.
// A high rating moves less. Returns the change.
float ssr_career_update_rating(SSRCareer *career, int index, int success);

// c_TPlayer.GetRatingDescription: the language tag of the type of the
// player from the first eight ratings, for example "playerdesc_Finishing".
const char *ssr_career_player_type(const SSRCareer *career);

// The number of a player type tag (c_TPlayer.CheckRatingNews), for the icon
// of the type: 0 midfielder to 20 young and near the max. -1 for a tag that
// is not a type.
int ssr_career_player_type_index(const char *tag);

// The star level of the career, from 1 (c_TPlayer.GetStarLevel).
int ssr_career_star_level(const SSRCareer *career);

// The age of the player: 16 in the first year (c_TPlayer.GetAge).
int ssr_career_age(const SSRCareer *career);

// The year and the week of the career, from 1.
int ssr_career_year(const SSRCareer *career);
int ssr_career_week(const SSRCareer *career);

// c_TPlayer.AddForm and AddInternationalForm: the newest rating goes in at
// the end.
void ssr_career_add_form(int *form, int rating);

// GSPlayerUtility.GotAgent and GotTrainer: the level of the agent or of the
// trainer, 3 to 1, or 0.
int ssr_career_agent(const SSRCareer *career);
int ssr_career_trainer(const SSRCareer *career);

// GSPlayerUtility.UpdateRelationship: the relation changes by the amount
// and stays from 1 to 100.
void ssr_career_update_relation(SSRCareer *career, SSRRelation relation, float change);

// c_TPlayer.GetRelationship: the value of the relation, or 0.
float ssr_career_relation(const SSRCareer *career, SSRRelation relation);

// c_TPlayer.CountSponsors: the number of sponsors.
int ssr_career_count_sponsors(const SSRCareer *career);

// c_TPlayer.GetHappiness: the club relations, the partner two times, and
// the sponsors, divided by 6.
float ssr_career_happiness(const SSRCareer *career);

// c_TPlayer.GetLifestyle: the part of the 60 products of the shop that the
// player owns, from 0 to 100. A broken product counts.
float ssr_career_lifestyle(const SSRCareer *career);

// c_TPlayer.ResetSeasonStats: the counts of the season start again.
void ssr_career_reset_season(SSRCareer *career);

// Writes the career to the file. The new file takes the place of the old
// one only when it is complete. Returns 1 when it worked.
int ssr_career_save(const SSRCareer *career, const char *path);

// Reads the career from the file. Returns 0 when the file is not there or
// has no club.
int ssr_career_load(SSRCareer *career, const char *path);

#endif
