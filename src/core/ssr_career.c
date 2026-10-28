#include "ssr_career.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

static void copy_name(char *out, size_t capacity, const char *text) {
    snprintf(out, capacity, "%s", text ? text : "");
}

// Removes the spaces at the two ends.
static void trim(char *text) {
    size_t start = 0, length = strlen(text);
    while (text[start] == ' ') start++;
    while (length > start && text[length - 1] == ' ') length--;
    memmove(text, text + start, length - start);
    text[length - start] = 0;
}

void ssr_career_create(SSRCareer *c, const char *first, const char *last, int gender, int hardMode,
                       int clubNationId, int wage, int goalBonus, int assistBonus) {
    memset(c, 0, sizeof(*c));
    c->sdate = 1;
    // The balls of the league, the cup, the continent, and the nation.
    c->ballLeague = 1;
    c->ballCup = 2;
    c->ballContinent = 3;
    c->ballInternational = 4;
    copy_name(c->firstName, sizeof(c->firstName), first);
    copy_name(c->lastName, sizeof(c->lastName), last);
    trim(c->firstName);
    trim(c->lastName);
    snprintf(c->fullName, sizeof(c->fullName), "%s %s", c->firstName, c->lastName);
    trim(c->fullName);
    copy_name(c->shortName, sizeof(c->shortName), c->lastName[0] ? c->lastName : c->firstName);
    c->gender = gender;
    c->hardMode = hardMode;
    c->clubNationId = clubNationId;
    c->wage = wage;
    c->goalBonus = goalBonus;
    c->assistBonus = assistBonus;
    // The values of the remote configuration of the game, when it is off.
    c->bank = 15;
    c->relationBoss = c->relationTeam = c->relationFans = 75;
    c->debut = 1;
    c->lastSpendTimeFriends = c->lastSpendTimePartner = 28;
    c->newsDone = 1;
    c->teamShopping = -1;
    c->expiredBoots = -1;
    c->showContractTip = 1;
    c->energy = 100;
    c->nrg[0] = c->nrg[1] = c->nrg[2] = 1;
    for (int i = 0; i < 10; i++) c->ratings[i] = 5;
    copy_name(c->gameVersion, sizeof(c->gameVersion), "v2.32");
}

float ssr_career_update_rating(SSRCareer *c, int index, int success) {
    // The fuse parameters of 4.32 for a success and for a fault.
    static const float SUCCESS[9] = {0.5f, 1, 1.5f, 1.5f, 1, 1, 1, 1, 1};
    static const float FAULT[9] = {-2, -2, -1.5f, -1.5f, -1, -1, -1, -1.5f, -1};
    if (index < 0 || index > 8) return 0;
    float r = c->ratings[index];
    int level = (int)r;
    static const float SCALE[9] = {0.1f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    float scale = level >= 1 && level <= 8 ? SCALE[level] : 0.1f;
    float change = (success ? SUCCESS[index] : FAULT[index]) * scale;
    float v = r + change;
    c->ratings[index] = v < 1 ? 1 : v > 10 ? 10 : v;
    return change;
}

static const char *const TYPES[21] = {
    "playerdesc_Midfielder",
    "playerdesc_Forward",
    "playerdesc_ShortPassing",
    "playerdesc_LongPassing",
    "playerdesc_LongPassingCrossing",
    "playerdesc_Passing",
    "playerdesc_PassingAssists",
    "playerdesc_Finishing",
    "playerdesc_LongShots",
    "playerdesc_FinishingLongShots",
    "playerdesc_ShootingHeading",
    "playerdesc_PassingShootingAssists",
    "playerdesc_Heading",
    "playerdesc_Crossing",
    "playerdesc_CrossingDribbling",
    "playerdesc_SetPieces",
    "playerdesc_Dribbling",
    "playerdesc_DribblingPace",
    "playerdesc_MaxRating",
    "playerdesc_NearMaxRating",
    "playerdesc_NearMaxRatingYoung",
};

int ssr_career_player_type_index(const char *tag) {
    for (int i = 0; tag && i < 21; i++) {
        if (strcmp(tag, TYPES[i]) == 0) return i;
    }
    return -1;
}

// m_GetBasicDescription: the type of one rating.
static const char *basic_type(int index) {
    static const char *const BASIC[8] = {"playerdesc_ShortPassing", "playerdesc_LongPassing", "playerdesc_Finishing",
                                         "playerdesc_LongShots",    "playerdesc_Heading",     "playerdesc_Crossing",
                                         "playerdesc_SetPieces",    "playerdesc_Dribbling"};
    return index >= 0 && index < 8 ? BASIC[index] : "playerdesc_NoRating";
}

// The form of the bytecode (jadx -m fallback). The normal decompile mixes
// up the branches of crossing and dribbling. [I: the career has no season
// yet, so the assists of the season are the assists of the career.]
const char *ssr_career_player_type(const SSRCareer *c) {
    const float *r = c->ratings;
    float sum = 0;
    int top = 0, best = 0;
    for (int i = 0; i <= 7; i++) {
        sum += r[i];
        if (r[i] > (float)top) {
            top = (int)r[i];
            best = i;
        }
    }
    if (top < 7) return r[0] + r[1] > r[2] + r[3] ? "playerdesc_Midfielder" : "playerdesc_Forward";
    // The second and the third rating with the same value as the top.
    int second = -1, third = -1;
    for (int i = 0; i <= 7; i++) {
        if (i != best && r[i] == (float)top) {
            second = i;
            break;
        }
    }
    for (int i = 0; i <= 7; i++) {
        if (i != best && i != second && r[i] == (float)top) {
            third = i;
            break;
        }
    }
    const char *type = basic_type(best);
    if (best == 0 && second == 1) {
        type = "playerdesc_Passing";
        if ((float)c->assists > (float)c->apps * 0.6f) {
            type = third == 2 || third == 3 ? "playerdesc_PassingShootingAssists" : "playerdesc_PassingAssists";
        }
    } else if (best == 2 && second == 3) {
        type = third == 4 ? "playerdesc_ShootingHeading" : "playerdesc_FinishingLongShots";
    } else if (best == 1 && second == 5 && r[1] == r[5]) {
        type = "playerdesc_LongPassingCrossing";
    } else if (best == 5 && second == 7) {
        type = "playerdesc_CrossingDribbling";
    } else if (best == 7 && ssr_career_skill(c->pace) > 50) {
        type = "playerdesc_DribblingPace";
    }
    if ((best == 0 || best == 1) && (second == 2 || second == 3) && r[0] + r[1] < r[2] + r[3]) {
        type = basic_type(second);
    }
    if (sum > 79) return "playerdesc_MaxRating";
    if (sum > 69) return ssr_career_age(c) < 19 ? "playerdesc_NearMaxRatingYoung" : "playerdesc_NearMaxRating";
    return type;
}

int ssr_career_star_level(const SSRCareer *c) {
    return c->starRating / 100 + 1;
}

int ssr_career_focus(const SSRCareer *c) {
    (void)c;
    return 2;
}

int ssr_career_age(const SSRCareer *c) {
    return ssr_career_year(c) + (c->startAge > 0 ? c->startAge : 16) - 1;
}

int ssr_career_year(const SSRCareer *c) {
    return (c->sdate - 1) / 364 + 1;
}

int ssr_career_week(const SSRCareer *c) {
    return ((c->sdate - 1) % 364) / 7 + 1;
}

static void write_ints(FILE *file, const char *name, const int *values, int count) {
    fprintf(file, "%s=", name);
    for (int i = 0; i < count; i++) fprintf(file, i ? ",%d" : "%d", values[i]);
    fputc('\n', file);
}

// A line ends a text, so the save keeps its new lines as "\n".
static void write_text(FILE *file, const char *name, const char *text) {
    fprintf(file, "%s=", name);
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            fputs("\\n", file);
        } else if (*p == '\\') {
            fputs("\\\\", file);
        } else {
            fputc(*p, file);
        }
    }
    fputc('\n', file);
}

static void read_text(const char *value, char *out, size_t capacity) {
    size_t used = 0;
    for (const char *p = value; *p && used + 1 < capacity; p++) {
        if (p[0] == '\\' && p[1] == 'n') {
            out[used++] = '\n';
            p++;
        } else if (p[0] == '\\' && p[1] == '\\') {
            out[used++] = '\\';
            p++;
        } else {
            out[used++] = *p;
        }
    }
    out[used] = 0;
}

static void write_floats(FILE *file, const char *name, const float *values, int count) {
    fprintf(file, "%s=", name);
    for (int i = 0; i < count; i++) fprintf(file, i ? ",%g" : "%g", values[i]);
    fputc('\n', file);
}

int ssr_career_agent(const SSRCareer *c) {
    return c->agent[2] > 0 ? 3 : c->agent[1] > 0 ? 2 : c->agent[0] > 0 ? 1 : 0;
}

int ssr_career_trainer(const SSRCareer *c) {
    return c->trainer[2] > 0 ? 3 : c->trainer[1] > 0 ? 2 : c->trainer[0] > 0 ? 1 : 0;
}

static float *relation_field(SSRCareer *c, SSRRelation relation) {
    switch (relation) {
    case SSR_RELATION_BOSS: return &c->relationBoss;
    case SSR_RELATION_TEAM: return &c->relationTeam;
    case SSR_RELATION_FANS: return &c->relationFans;
    case SSR_RELATION_PARTNER: return &c->relationPartner;
    case SSR_RELATION_SPONSORS: return &c->relationSponsors;
    case SSR_RELATION_FRIENDS: return &c->relationFriends;
    case SSR_RELATION_BOSS_INT: return &c->relationBossInt;
    case SSR_RELATION_TEAM_INT: return &c->relationTeamInt;
    case SSR_RELATION_FANS_INT: return &c->relationFansInt;
    }
    return NULL;
}

void ssr_career_update_relation(SSRCareer *c, SSRRelation relation, float change) {
    float *field = c ? relation_field(c, relation) : NULL;
    if (!field) return;
    float v = *field + change;
    *field = v < 1 ? 1 : v > 100 ? 100 : v;
}

float ssr_career_relation(const SSRCareer *c, SSRRelation relation) {
    const float *field = c ? relation_field((SSRCareer *)c, relation) : NULL;
    return field ? *field : 0;
}

int ssr_career_count_sponsors(const SSRCareer *c) {
    int n = 0;
    for (int i = 0; i < 10; i++) n += c->sponsors[i] > 0;
    return n;
}

float ssr_career_happiness(const SSRCareer *c) {
    // GetEffectiveRelation casts each relation to int, but not the partner.
    float sum = (float)((int)c->relationBoss + (int)c->relationTeam + (int)c->relationFans);
    sum += c->relationPartner * 2;
    sum += (float)(int)c->relationSponsors;
    return sum / 6.0f;
}

float ssr_career_lifestyle(const SSRCareer *c) {
    int n = 0;
    for (int k = 0; k < SSR_CAREER_ITEMS; k++) {
        n += c->items[k] > 0;
        n += c->vehicles[k] > 0;
        n += c->properties[k] > 0;
    }
    return 100.0f / SSR_CAREER_SHOP_PRODUCTS * (float)n;
}

int ssr_career_hat_tricks(int goals) {
    return goals >= 9 ? 3 : goals / 3;
}

void ssr_career_add_form(int *form, int rating) {
    if (!form) return;
    for (int i = 0; i < 4; i++) form[i] = form[i + 1];
    form[4] = rating;
}

void ssr_career_reset_season(SSRCareer *c) {
    if (!c) return;
    c->seasonApps = c->seasonGoals = c->seasonAssists = c->seasonPasses = 0;
    c->seasonHatTricks = c->seasonStarMan = c->seasonChances = 0;
    c->seasonTotalRating = 0;
    memset(c->seasonFurthest, 0, sizeof(c->seasonFurthest));
    c->wonMajorAward = 0;
    c->headlineResult[0] = 0;
    c->energy = 100;
    // The boss and the team forget a tenth of the relation.
    c->relationBoss -= c->relationBoss * 0.1f;
    c->relationTeam -= c->relationTeam * 0.1f;
    c->relationBossInt -= c->relationBossInt * 0.1f;
    c->relationTeamInt -= c->relationTeamInt * 0.1f;
}

int ssr_career_save(const SSRCareer *c, const char *path) {
    if (!c || !path) return 0;
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return 0;
    fprintf(file, "gameversion=%s\n", c->gameVersion);
    fprintf(file, "sdate=%d\n", c->sdate);
    fprintf(file, "firstname=%s\nlastname=%s\n", c->firstName, c->lastName);
    fprintf(file, "fullname=%s\nshortname=%s\n", c->fullName, c->shortName);
    fprintf(file, "gender=%d\nhardmode=%d\n", c->gender, c->hardMode);
    fprintf(file, "nationid=%d\nclubnationid=%d\nclubid=%d\n", c->nationId, c->clubNationId, c->clubId);
    fprintf(file, "bank=%d\nwage=%d\ngoalbonus=%d\nassistbonus=%d\n", c->bank, c->wage, c->goalBonus, c->assistBonus);
    fprintf(file, "energy=%g\n", c->energy);
    fprintf(file, "relationboss=%g\nrelationteam=%g\nrelationfans=%g\n", c->relationBoss, c->relationTeam,
            c->relationFans);
    fprintf(file, "relationbossint=%g\nrelationteamint=%g\nrelationfansint=%g\n", c->relationBossInt,
            c->relationTeamInt, c->relationFansInt);
    fprintf(file, "relationgirlfriend=%g\nrelationsponsors=%g\nrelationfriends=%g\n", c->relationPartner,
            c->relationSponsors, c->relationFriends);
    fprintf(file, "lastspendtimefriends=%g\nlastspendtimegirlfriend=%g\n", c->lastSpendTimeFriends,
            c->lastSpendTimePartner);
    fprintf(file, "spenttime=%d\n", c->spentTime);
    write_ints(file, "sponsors", c->sponsors, 10);
    write_ints(file, "items", c->items, SSR_CAREER_ITEMS);
    write_ints(file, "vehicles", c->vehicles, SSR_CAREER_ITEMS);
    write_ints(file, "properties", c->properties, SSR_CAREER_ITEMS);
    fprintf(file, "teammatesshoppingcount=%d\n", c->teamShopping);
    fprintf(file, "gambling=%d\ncasinocount=%d\n", c->gambling, c->casinoCount);
    write_ints(file, "agents", c->agent, 3);
    write_ints(file, "trainers", c->trainer, 3);
    int boots[9] = {c->bootId,       c->bootUses,   c->expiredBoots,   c->currentBootId, c->currentBootUses,
                    c->tryingBoots,  c->oldProBoots, c->tryBoots,      c->noBootsMatches};
    write_ints(file, "boots", boots, 9);
    int balls[4] = {c->ballLeague, c->ballCup, c->ballContinent, c->ballInternational};
    write_ints(file, "balls", balls, 4);
    int contract[5] = {c->lastNegotiations, c->lastTransferDate, c->lastAgentHint, c->captain, c->showContractTip};
    write_ints(file, "contract", contract, 5);
    fprintf(file, "debut=%d\nintnotpickedmsg=%d\n", c->debut, c->intNotPickedMessage);
    write_ints(file, "technique", c->technique, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "pace", c->pace, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "vision", c->vision, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "setpieces", c->setPieces, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "power", c->power, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "lasttrained", c->lastTrained, 5);
    write_ints(file, "deplete", c->deplete, 5);
    write_ints(file, "nrg", c->nrg, 10);
    write_floats(file, "ratings", c->ratings, 10);
    fprintf(file, "apps=%d\ngoals=%d\nassists=%d\npasses=%d\nstarman=%d\n", c->apps, c->goals, c->assists, c->passes,
            c->starMan);
    fprintf(file, "chances=%d\n", c->chances);
    int season[7] = {c->seasonApps,      c->seasonGoals,   c->seasonAssists, c->seasonPasses,
                     c->seasonHatTricks, c->seasonStarMan, c->seasonChances};
    write_ints(file, "season", season, 7);
    int national[7] = {c->intApps, c->intGoals, c->intAssists, c->intPasses, c->intHatTricks, c->intStarMan, c->intChances};
    write_ints(file, "international", national, 7);
    fprintf(file, "seasontotalrating=%g\ntotalrating=%g\ninttotalrating=%g\n", c->seasonTotalRating, c->totalRating,
            c->intTotalRating);
    write_ints(file, "form", c->form, 5);
    write_ints(file, "intform", c->intForm, 5);
    write_ints(file, "careerform", c->careerForm, 5);
    fprintf(file, "hattricks=%d\n", c->hatTricks);
    write_floats(file, "seasonfurthest", c->seasonFurthest, 4);
    write_floats(file, "furthest", c->furthest, 4);
    write_floats(file, "intfurthest", c->intFurthest, 4);
    fprintf(file, "potyawards=%d\nwonmajoraward=%d\n", c->potyAwards, c->wonMajorAward);
    fprintf(file, "worldseed=%d\n", c->worldSeed);
    if (c->startAge) fprintf(file, "startage=%d\n", c->startAge);
    if (c->position) fprintf(file, "role=%d\n", c->position);
    if (c->loanFrom) fprintf(file, "loanfrom=%d\n", c->loanFrom);
    if (c->charityYear) fprintf(file, "charityyear=%d\n", c->charityYear);
    if (c->businessStake) fprintf(file, "businessstake=%d\n", c->businessStake);
    if (c->tutorials) fprintf(file, "tutorials=%d\n", c->tutorials);
    if (c->trainingAttempts) fprintf(file, "trainingattempts=%d\n", c->trainingAttempts);
    if (c->exBoss[0]) {
        write_text(file, "exboss", c->exBoss);
        fprintf(file, "exbossclub=%d\n", c->exBossClub);
    }
    fprintf(file, "injury=%d\n", c->injury);
    fprintf(file, "starrating=%d\n", c->starRating);
    fprintf(file, "retired=%d\n", c->retired);
    fprintf(file, "newsdone=%d\ntipcount=%d\n", c->newsDone, c->tipCount);
    fprintf(file, "interviewskill=%d\nlastinterview=%d\n", c->interviewSkill, c->lastInterview);
    write_text(file, "headline", c->headline);
    write_text(file, "headlineresult", c->headlineResult);
    write_text(file, "horses", c->horses);
    // One line for each entry, as a long career passes the length of a
    // line (GetTrophyLog and GetStatHistory join them with "#").
    for (int i = 0; i < c->trophyCount; i++) {
        const SSRTrophy *t = &c->trophies[i];
        fprintf(file, "trophy=%d:%s:%s:%d\n", t->year, t->tournament, t->team, t->medal);
    }
    for (int i = 0; i < c->historyCount; i++) {
        const SSRStatRow *h = &c->history[i];
        fprintf(file, "history=%d:%s:%d:%d:%d:%d\n", h->year, h->club, h->apps, h->goals, h->assists, h->starMan);
    }
    for (int id = 0; id < SSR_CAREER_AWARDS; id++) {
        if (c->awards[id] > 0) fprintf(file, "award=%d:%d\n", id, c->awards[id]);
    }
    for (int i = 0; i < c->momentCount; i++) {
        char value[160];
        snprintf(value, sizeof(value), "%d:%s", c->moments[i].sdate, c->moments[i].text);
        write_text(file, "moment", value);
    }
    int ok = !ferror(file);
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

static void read_ints(const char *text, int *values, int count) {
    for (int i = 0; i < count && *text; i++) {
        char *end;
        values[i] = (int)strtol(text, &end, 10);
        text = *end == ',' ? end + 1 : end;
    }
}

static void read_floats(const char *text, float *values, int count) {
    for (int i = 0; i < count && *text; i++) {
        char *end;
        values[i] = strtof(text, &end);
        text = *end == ',' ? end + 1 : end;
    }
}

static void read_trophy(SSRCareer *c, const char *value);
static void read_history(SSRCareer *c, const char *value);

int ssr_career_load(SSRCareer *c, const char *path) {
    if (!c || !path) return 0;
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    memset(c, 0, sizeof(*c));
    // An older save has no shopping trip and no boots that ran out.
    c->teamShopping = -1;
    c->expiredBoots = -1;
    static char line[4096];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = 0;
        const char *key = line, *value = equals + 1;
        int number = atoi(value);
        float real = strtof(value, NULL);
        if (strcmp(key, "gameversion") == 0) copy_name(c->gameVersion, sizeof(c->gameVersion), value);
        else if (strcmp(key, "sdate") == 0) c->sdate = number;
        else if (strcmp(key, "firstname") == 0) copy_name(c->firstName, sizeof(c->firstName), value);
        else if (strcmp(key, "lastname") == 0) copy_name(c->lastName, sizeof(c->lastName), value);
        else if (strcmp(key, "fullname") == 0) copy_name(c->fullName, sizeof(c->fullName), value);
        else if (strcmp(key, "shortname") == 0) copy_name(c->shortName, sizeof(c->shortName), value);
        else if (strcmp(key, "gender") == 0) c->gender = number;
        else if (strcmp(key, "hardmode") == 0) c->hardMode = number;
        else if (strcmp(key, "nationid") == 0) c->nationId = number;
        else if (strcmp(key, "clubnationid") == 0) c->clubNationId = number;
        else if (strcmp(key, "clubid") == 0) c->clubId = number;
        else if (strcmp(key, "bank") == 0) c->bank = number;
        else if (strcmp(key, "wage") == 0) c->wage = number;
        else if (strcmp(key, "goalbonus") == 0) c->goalBonus = number;
        else if (strcmp(key, "assistbonus") == 0) c->assistBonus = number;
        else if (strcmp(key, "energy") == 0) c->energy = real;
        else if (strcmp(key, "relationboss") == 0) c->relationBoss = real;
        else if (strcmp(key, "relationteam") == 0) c->relationTeam = real;
        else if (strcmp(key, "relationfans") == 0) c->relationFans = real;
        else if (strcmp(key, "relationbossint") == 0) c->relationBossInt = real;
        else if (strcmp(key, "relationteamint") == 0) c->relationTeamInt = real;
        else if (strcmp(key, "relationfansint") == 0) c->relationFansInt = real;
        else if (strcmp(key, "relationgirlfriend") == 0) c->relationPartner = real;
        else if (strcmp(key, "relationsponsors") == 0) c->relationSponsors = real;
        else if (strcmp(key, "relationfriends") == 0) c->relationFriends = real;
        else if (strcmp(key, "lastspendtimefriends") == 0) c->lastSpendTimeFriends = real;
        else if (strcmp(key, "lastspendtimegirlfriend") == 0) c->lastSpendTimePartner = real;
        else if (strcmp(key, "spenttime") == 0) c->spentTime = number;
        else if (strcmp(key, "sponsors") == 0) read_ints(value, c->sponsors, 10);
        else if (strcmp(key, "items") == 0) read_ints(value, c->items, SSR_CAREER_ITEMS);
        else if (strcmp(key, "vehicles") == 0) read_ints(value, c->vehicles, SSR_CAREER_ITEMS);
        else if (strcmp(key, "properties") == 0) read_ints(value, c->properties, SSR_CAREER_ITEMS);
        else if (strcmp(key, "teammatesshoppingcount") == 0) c->teamShopping = number;
        else if (strcmp(key, "gambling") == 0) c->gambling = number;
        else if (strcmp(key, "casinocount") == 0) c->casinoCount = number;
        else if (strcmp(key, "agents") == 0) read_ints(value, c->agent, 3);
        else if (strcmp(key, "trainers") == 0) read_ints(value, c->trainer, 3);
        else if (strcmp(key, "boots") == 0) {
            int v[9] = {0, 0, -1, 0, 0, 0, 0, 0, 0};
            read_ints(value, v, 9);
            c->bootId = v[0];
            c->bootUses = v[1];
            c->expiredBoots = v[2];
            c->currentBootId = v[3];
            c->currentBootUses = v[4];
            c->tryingBoots = v[5];
            c->oldProBoots = v[6];
            c->tryBoots = v[7];
            c->noBootsMatches = v[8];
        } else if (strcmp(key, "contract") == 0) {
            int v[5] = {0, 0, 0, 0, 1};
            read_ints(value, v, 5);
            c->lastNegotiations = v[0];
            c->lastTransferDate = v[1];
            c->lastAgentHint = v[2];
            c->captain = v[3];
            c->showContractTip = v[4];
        } else if (strcmp(key, "balls") == 0) {
            int v[4] = {0};
            read_ints(value, v, 4);
            c->ballLeague = v[0];
            c->ballCup = v[1];
            c->ballContinent = v[2];
            c->ballInternational = v[3];
        }
        else if (strcmp(key, "debut") == 0) c->debut = number;
        else if (strcmp(key, "intnotpickedmsg") == 0) c->intNotPickedMessage = number;
        else if (strcmp(key, "technique") == 0) read_ints(value, c->technique, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "pace") == 0) read_ints(value, c->pace, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "vision") == 0) read_ints(value, c->vision, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "setpieces") == 0) read_ints(value, c->setPieces, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "power") == 0) read_ints(value, c->power, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "lasttrained") == 0) read_ints(value, c->lastTrained, 5);
        else if (strcmp(key, "deplete") == 0) read_ints(value, c->deplete, 5);
        else if (strcmp(key, "nrg") == 0) read_ints(value, c->nrg, 10);
        else if (strcmp(key, "ratings") == 0) read_floats(value, c->ratings, 10);
        else if (strcmp(key, "apps") == 0) c->apps = number;
        else if (strcmp(key, "goals") == 0) c->goals = number;
        else if (strcmp(key, "assists") == 0) c->assists = number;
        else if (strcmp(key, "passes") == 0) c->passes = number;
        else if (strcmp(key, "starman") == 0) c->starMan = number;
        else if (strcmp(key, "chances") == 0) c->chances = number;
        else if (strcmp(key, "season") == 0) {
            int v[7] = {0};
            read_ints(value, v, 7);
            c->seasonApps = v[0];
            c->seasonGoals = v[1];
            c->seasonAssists = v[2];
            c->seasonPasses = v[3];
            c->seasonHatTricks = v[4];
            c->seasonStarMan = v[5];
            c->seasonChances = v[6];
        } else if (strcmp(key, "international") == 0) {
            int v[7] = {0};
            read_ints(value, v, 7);
            c->intApps = v[0];
            c->intGoals = v[1];
            c->intAssists = v[2];
            c->intPasses = v[3];
            c->intHatTricks = v[4];
            c->intStarMan = v[5];
            c->intChances = v[6];
        } else if (strcmp(key, "seasontotalrating") == 0) c->seasonTotalRating = real;
        else if (strcmp(key, "totalrating") == 0) c->totalRating = real;
        else if (strcmp(key, "inttotalrating") == 0) c->intTotalRating = real;
        else if (strcmp(key, "form") == 0) read_ints(value, c->form, 5);
        else if (strcmp(key, "intform") == 0) read_ints(value, c->intForm, 5);
        else if (strcmp(key, "careerform") == 0) read_ints(value, c->careerForm, 5);
        else if (strcmp(key, "hattricks") == 0) c->hatTricks = number;
        else if (strcmp(key, "seasonfurthest") == 0) read_floats(value, c->seasonFurthest, 4);
        else if (strcmp(key, "furthest") == 0) read_floats(value, c->furthest, 4);
        else if (strcmp(key, "intfurthest") == 0) read_floats(value, c->intFurthest, 4);
        else if (strcmp(key, "potyawards") == 0) c->potyAwards = number;
        else if (strcmp(key, "wonmajoraward") == 0) c->wonMajorAward = number;
        else if (strcmp(key, "worldseed") == 0) c->worldSeed = number;
        else if (strcmp(key, "startage") == 0) c->startAge = number;
        else if (strcmp(key, "loanfrom") == 0) c->loanFrom = number;
        else if (strcmp(key, "charityyear") == 0) c->charityYear = number;
        else if (strcmp(key, "businessstake") == 0) c->businessStake = number;
        else if (strcmp(key, "tutorials") == 0) c->tutorials = number;
        else if (strcmp(key, "trainingattempts") == 0) c->trainingAttempts = number;
        else if (strcmp(key, "exboss") == 0) read_text(value, c->exBoss, sizeof(c->exBoss));
        else if (strcmp(key, "exbossclub") == 0) c->exBossClub = number;
        else if (strcmp(key, "role") == 0 && number >= 0 && number < SSR_CAREER_POSITIONS) c->position = number;
        // A save of 0.27 had a striker (1) or a holding midfielder (2).
        else if (strcmp(key, "position") == 0) c->position = number == 2 ? SSR_CAREER_MIDFIELDER : SSR_CAREER_FORWARD;
        else if (strcmp(key, "injury") == 0) c->injury = number;
        else if (strcmp(key, "starrating") == 0) c->starRating = number;
        else if (strcmp(key, "retired") == 0) c->retired = number;
        else if (strcmp(key, "newsdone") == 0) c->newsDone = number;
        else if (strcmp(key, "tipcount") == 0) c->tipCount = number;
        else if (strcmp(key, "interviewskill") == 0) c->interviewSkill = number;
        else if (strcmp(key, "lastinterview") == 0) c->lastInterview = number;
        else if (strcmp(key, "headline") == 0) read_text(value, c->headline, sizeof(c->headline));
        else if (strcmp(key, "headlineresult") == 0) read_text(value, c->headlineResult, sizeof(c->headlineResult));
        else if (strcmp(key, "horses") == 0) read_text(value, c->horses, sizeof(c->horses));
        else if (strcmp(key, "trophy") == 0) read_trophy(c, value);
        else if (strcmp(key, "history") == 0) read_history(c, value);
        else if (strcmp(key, "moment") == 0) {
            const char *text = strchr(value, ':');
            if (text && c->momentCount < SSR_CAREER_MOMENTS) {
                SSRMoment *m = &c->moments[c->momentCount++];
                m->sdate = atoi(value);
                read_text(text + 1, m->text, sizeof(m->text));
            }
        } else if (strcmp(key, "award") == 0) {
            int id = atoi(value);
            const char *day = strchr(value, ':');
            if (id >= 0 && id < SSR_CAREER_AWARDS && day) c->awards[id] = atoi(day + 1);
        }
    }
    fclose(file);
    // StatHistory.StoreOldStats: a career from before the history gets
    // one row of year 0 with the counts before this season.
    if (ssr_career_year(c) > 1 && c->historyCount == 0) {
        SSRStatRow *h = &c->history[c->historyCount++];
        memset(h, 0, sizeof(*h));
        snprintf(h->club, sizeof(h->club), "-");
        h->apps = c->apps - c->seasonApps;
        h->goals = c->goals - c->seasonGoals;
        h->assists = c->assists - c->seasonAssists;
        h->starMan = c->starMan - c->seasonStarMan;
    }
    return c->clubId != 0;
}

// Splits text at ':' into parts, up to max. Returns the number of parts.
static int split_parts(char *text, char **parts, int max) {
    int n = 0;
    for (char *p = text; p && n < max;) {
        parts[n++] = p;
        p = strchr(p, ':');
        if (p) *p++ = 0;
    }
    return n;
}

// LoadTrophyLog: the year and the tournament, and the team and the medal
// when they are there.
static void read_trophy(SSRCareer *c, const char *value) {
    if (c->trophyCount >= SSR_CAREER_TROPHIES) return;
    char copy[256], *parts[4];
    snprintf(copy, sizeof(copy), "%s", value);
    int n = split_parts(copy, parts, 4);
    if (n < 2) return;
    SSRTrophy *t = &c->trophies[c->trophyCount++];
    memset(t, 0, sizeof(*t));
    t->year = atoi(parts[0]);
    snprintf(t->tournament, sizeof(t->tournament), "%s", parts[1]);
    if (n > 2) snprintf(t->team, sizeof(t->team), "%s", parts[2]);
    if (n > 3) t->medal = atoi(parts[3]);
}

// LoadStatHistory: a row needs all six parts.
static void read_history(SSRCareer *c, const char *value) {
    if (c->historyCount >= SSR_CAREER_HISTORY) return;
    char copy[256], *parts[6];
    snprintf(copy, sizeof(copy), "%s", value);
    if (split_parts(copy, parts, 6) < 6) return;
    SSRStatRow *h = &c->history[c->historyCount++];
    memset(h, 0, sizeof(*h));
    h->year = atoi(parts[0]);
    snprintf(h->club, sizeof(h->club), "%s", parts[1]);
    h->apps = atoi(parts[2]);
    h->goals = atoi(parts[3]);
    h->assists = atoi(parts[4]);
    h->starMan = atoi(parts[5]);
}

void ssr_career_add_trophy(SSRCareer *c, int year, const char *team, const char *tournament, int medal) {
    if (!c || !tournament) return;
    for (int i = 0; i < c->trophyCount; i++) {
        if (c->trophies[i].year == year && strcmp(c->trophies[i].tournament, tournament) == 0) return;
    }
    if (c->trophyCount >= SSR_CAREER_TROPHIES) return;
    SSRTrophy *t = &c->trophies[c->trophyCount++];
    memset(t, 0, sizeof(*t));
    t->year = year;
    t->medal = medal;
    snprintf(t->tournament, sizeof(t->tournament), "%s", tournament);
    snprintf(t->team, sizeof(t->team), "%s", team ? team : "");
}

int ssr_career_unlock_award(SSRCareer *c, int id) {
    if (!c || id < 0 || id >= SSR_CAREER_AWARDS || c->awards[id] > 0) return 0;
    c->awards[id] = c->sdate > 0 ? c->sdate : 1;
    return 1;
}

int ssr_career_award_count(const SSRCareer *c) {
    int count = 0;
    for (int id = 0; c && id < SSR_CAREER_AWARDS; id++) count += c->awards[id] > 0;
    return count;
}

int ssr_career_count_medals(const SSRCareer *c, int medal, int year) {
    int n = 0;
    for (int i = 0; c && i < c->trophyCount; i++) {
        n += c->trophies[i].medal == medal && (year == 0 || c->trophies[i].year == year);
    }
    return n;
}

int ssr_career_count_medal_teams(const SSRCareer *c, int medal) {
    int n = 0;
    for (int i = 0; c && i < c->trophyCount; i++) {
        if (c->trophies[i].medal != medal) continue;
        int seen = 0;
        for (int k = 0; k < i && !seen; k++) {
            seen = c->trophies[k].medal == medal && strcmp(c->trophies[k].team, c->trophies[i].team) == 0;
        }
        n += !seen;
    }
    return n;
}

void ssr_career_history_totals(const SSRCareer *c, int *apps, int *goals, int *assists, int *starMan) {
    int a = 0, g = 0, s = 0, m = 0;
    for (int i = 0; c && i < c->historyCount; i++) {
        a += c->history[i].apps;
        g += c->history[i].goals;
        s += c->history[i].assists;
        m += c->history[i].starMan;
    }
    if (apps) *apps = a;
    if (goals) *goals = g;
    if (assists) *assists = s;
    if (starMan) *starMan = m;
}

int ssr_career_history_apps(const SSRCareer *c, const char *club) {
    int n = 0;
    for (int i = 0; c && club && i < c->historyCount; i++) {
        if (strcmp(c->history[i].club, club) == 0) n += c->history[i].apps;
    }
    return n;
}

void ssr_career_add_history(SSRCareer *c, int year, const char *club) {
    // [I: the game looks for a row with the name of the club, but it
    // keeps the tla, so it adds a row each time. The remake does the
    // same.]
    if (!c || c->historyCount >= SSR_CAREER_HISTORY) return;
    int apps, goals, assists, starMan;
    ssr_career_history_totals(c, &apps, &goals, &assists, &starMan);
    SSRStatRow *h = &c->history[c->historyCount++];
    memset(h, 0, sizeof(*h));
    h->year = year;
    snprintf(h->club, sizeof(h->club), "%s", club ? club : "");
    h->apps = c->apps - apps;
    h->goals = c->goals - goals;
    h->assists = c->assists - assists;
    h->starMan = c->starMan - starMan;
}

int ssr_career_skill(const int *levels) {
    int sum = 0;
    for (int i = 0; i < SSR_CAREER_SKILL_LEVELS; i++) sum += levels[i];
    return sum < 1 ? 1 : sum > 100 ? 100 : sum;
}

int ssr_career_selected_ball(SSRCareer *c, int type) {
    // A field of 0 takes the ball of the type. [I: for the nation the
    // game gives 0 and changes the ball of the continent; the remake
    // gives ball 4.]
    int *fields[4] = {&c->ballLeague, &c->ballCup, &c->ballContinent, &c->ballInternational};
    if (type < 1 || type > 4) return 0;
    if (*fields[type - 1] == 0) *fields[type - 1] = type;
    return *fields[type - 1];
}

void ssr_career_set_selected_ball(SSRCareer *c, int ball, int type) {
    int *fields[4] = {&c->ballLeague, &c->ballCup, &c->ballContinent, &c->ballInternational};
    if (type < 1 || type > 4) return;
    *fields[type - 1] = ball == 0 ? 1 : ball;
}

void ssr_career_add_moment(SSRCareer *c, int sdate, const char *text) {
    if (!c || !text) return;
    if (c->momentCount == SSR_CAREER_MOMENTS) {
        memmove(c->moments, c->moments + 1, sizeof(SSRMoment) * (SSR_CAREER_MOMENTS - 1));
        c->momentCount--;
    }
    SSRMoment *m = &c->moments[c->momentCount++];
    m->sdate = sdate;
    snprintf(m->text, sizeof(m->text), "%s", text);
}
