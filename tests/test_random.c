// Tests ssr_random with values of the generator of the game, from the seed
// 1234.

#include "ssr_random.h"
#include "ssr_test.h"

static void test_gives_the_numbers_of_the_game(void) {
    SSRRandom r = {SSR_RANDOM_START};
    CHECK(ssr_rnd(&r) == 0.7143076658248901f);
    CHECK(ssr_rnd(&r) == 0.2070130705833435f);
    CHECK(ssr_rnd(&r) == 0.7495418190956116f);

    r.seed = SSR_RANDOM_START;
    const int dice[5] = {5, 2, 5, 3, 5};
    for (int i = 0; i < 5; i++) CHECK(ssr_rand(&r, 6) == dice[i]);
    const int range[5] = {0, -1, -1, -1, 0};
    for (int i = 0; i < 5; i++) CHECK(ssr_rand2(&r, -3, 3) == range[i]);
    CHECK(ssr_rand_float(&r, -1, 1) == 0.3883512020111084f);
    CHECK(ssr_rand_float(&r, -1, 1) == 0.5915662050247192f);
    CHECK(ssr_rand_float(&r, -1, 1) == -0.8788071870803833f);
    CHECK(r.seed == 260259477);
}

int main(void) {
    test_gives_the_numbers_of_the_game();
    return TEST_RESULT();
}
