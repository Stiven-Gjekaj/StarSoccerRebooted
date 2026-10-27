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
    // The skills: the score of each level of each skill. 0 is not trained.
    int technique[SSR_CAREER_SKILL_LEVELS];
    int pace[SSR_CAREER_SKILL_LEVELS];
    int vision[SSR_CAREER_SKILL_LEVELS];
    int setPieces[SSR_CAREER_SKILL_LEVELS];
    int power[SSR_CAREER_SKILL_LEVELS];
    // The energy drinks of each kind.
    int nrg[10];
    float ratings[10];
    int retired;
    // The news that the next screen shows, or "".
    char headline[512];
    char gameVersion[16];
} SSRCareer;

// The start of a new career (c_TPlayer.m_Create). The values come from
// the game: the bank, the relations, the cans, the ratings, and the first
// contract (wage, goal bonus, assist bonus).
void ssr_career_create(SSRCareer *career, const char *first, const char *last, int gender, int hardMode,
                       int clubNationId, int wage, int goalBonus, int assistBonus);

// The year and the week of the career, from 1.
int ssr_career_year(const SSRCareer *career);
int ssr_career_week(const SSRCareer *career);

// Writes the career to the file. The new file takes the place of the old
// one only when it is complete. Returns 1 when it worked.
int ssr_career_save(const SSRCareer *career, const char *path);

// Reads the career from the file. Returns 0 when the file is not there or
// has no club.
int ssr_career_load(SSRCareer *career, const char *path);

#endif
