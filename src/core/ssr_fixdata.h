// ssr_fixdata: the fixture tables of the game (c_TFixData). A league
// table gives the pairs of each round of a league of a size. A cup table
// gives the order of the pairs of the first round of a cup.

#ifndef SSR_FIXDATA_H
#define SSR_FIXDATA_H

typedef struct {
    // The number of teams, and 1 for a cup table.
    int size;
    int cup;
    const short *values;
    int count;
} SSRFixTable;

extern const SSRFixTable SSR_FIX_TABLES[];
extern const int SSR_FIX_TABLE_COUNT;

#endif
