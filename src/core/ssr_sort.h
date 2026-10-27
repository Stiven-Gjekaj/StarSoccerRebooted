// ssr_sort: the quicksort of Monkey X (bb_collections.DepQuickSort). It is
// not stable, and a compare can take random numbers, so the remake sorts
// with the same steps as the game.

#ifndef SSR_SORT_H
#define SSR_SORT_H

// Gives below 0, 0, or above 0 for the items a and b.
typedef int (*SSRSortCompare)(void *context, int a, int b);

// Sorts the items. The pivot is the middle item. An ascending sort keeps
// the items that compare <= 0 with the pivot on the left, and a
// descending sort the items that compare >= 0.
void ssr_sort(int *items, int count, SSRSortCompare compare, void *context, int descending);

#endif
