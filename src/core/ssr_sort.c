#include "ssr_sort.h"

static int partition(int *items, int low, int high, int middle, SSRSortCompare compare, void *context,
                     int descending) {
    int pivot = items[middle];
    items[middle] = items[high];
    items[high] = pivot;
    int store = low;
    for (int i = low; i < high; i++) {
        int c = compare(context, items[i], pivot);
        if (descending ? c >= 0 : c <= 0) {
            int t = items[i];
            items[i] = items[store];
            items[store] = t;
            store++;
        }
    }
    int t = items[store];
    items[store] = items[high];
    items[high] = t;
    return store;
}

static void quicksort(int *items, int low, int high, SSRSortCompare compare, void *context, int descending) {
    if (high <= low) return;
    int p = partition(items, low, high, low + (high - low) / 2, compare, context, descending);
    quicksort(items, low, p - 1, compare, context, descending);
    quicksort(items, p + 1, high, compare, context, descending);
}

void ssr_sort(int *items, int count, SSRSortCompare compare, void *context, int descending) {
    if (items && compare && count > 1) quicksort(items, 0, count - 1, compare, context, descending);
}
