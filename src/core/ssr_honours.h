// ssr_honours: the history of the world of a career. Each season gives
// each league its champion and its top scorer. The list keeps them, so
// that the world remembers them.

#ifndef SSR_HONOURS_H
#define SSR_HONOURS_H

typedef struct {
    // The year of the season (ssr_career_year), the tla of the league, and
    // the champion.
    int year;
    char league[16];
    int club;
    char clubName[64];
    // The top scorer, the club of the top scorer, and the goals. The name
    // is "" when no player scored.
    char scorer[64];
    char scorerClub[64];
    int goals;
} SSRHonour;

typedef struct {
    SSRHonour *list;
    int count, capacity;
} SSRHonours;

// Adds the honour. A second honour of the same year and league takes the
// place of the first. Returns 1 on success.
int ssr_honours_add(SSRHonours *honours, const SSRHonour *honour);
void ssr_honours_clear(SSRHonours *honours);
// The number of titles of the club.
int ssr_honours_titles(const SSRHonours *honours, int club);

// Keeps the honours in a text file, one honour a line. Returns 1 on
// success.
int ssr_honours_save(const SSRHonours *honours, const char *path);
// Reads the file into honours, after a clear. Returns 0 when the file is
// not there.
int ssr_honours_load(SSRHonours *honours, const char *path);

#endif
