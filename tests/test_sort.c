// Tests ssr_sort: the quicksort of Monkey X.

#include "ssr_sort.h"
#include "ssr_test.h"

static int by_key(void *context, int a, int b) {
    const int *keys = context;
    return keys[a] - keys[b];
}

static void test_sorts_as_the_game_does(void) {
    int keys[4] = {3, 1, 2, 1};
    int items[4] = {0, 1, 2, 3};
    ssr_sort(items, 4, by_key, keys, 0);
    // The sort is not stable: item 3 comes before item 1, which has the
    // same key.
    CHECK(items[0] == 3 && items[1] == 1 && items[2] == 2 && items[3] == 0);
    int down[4] = {0, 1, 2, 3};
    ssr_sort(down, 4, by_key, keys, 1);
    CHECK(keys[down[0]] == 3 && keys[down[1]] == 2 && keys[down[2]] == 1 && keys[down[3]] == 1);
    // No items and one item do not change.
    int one[1] = {7};
    ssr_sort(one, 1, by_key, keys, 0);
    ssr_sort(NULL, 0, by_key, keys, 0);
    CHECK(one[0] == 7);
}

int main(void) {
    test_sorts_as_the_game_does();
    return TEST_RESULT();
}
