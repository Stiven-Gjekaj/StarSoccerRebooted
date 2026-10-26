// Tests ssr_expr with the forms that the screen layouts use.

#include <math.h>
#include <string.h>

#include "ssr_expr.h"
#include "ssr_test.h"

static double variable(void *context, char kind, const char *name) {
    (void)context;
    if (kind == '@' && strcmp(name, "B") == 0) return 1136;
    if (kind == '@' && strcmp(name, "W") == 0) return 640;
    if (kind == '%' && strcmp(name, "xPos") == 0) return 200;
    return 0;
}

static double tweak(void *context, const char *group, const char *name) {
    (void)context;
    if (strcmp(group, "Player") == 0 && strcmp(name, "Energy") == 0) return 80;
    return 0;
}

static int near(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static void test_evaluates_arithmetic(void) {
    int ok = 0;
    CHECK(near(ssr_expr_eval("1+2*3", NULL, &ok), 7) && ok);
    CHECK(near(ssr_expr_eval("[1+2]*3", NULL, &ok), 9) && ok);
    CHECK(near(ssr_expr_eval("290-10", NULL, &ok), 280) && ok);
    CHECK(near(ssr_expr_eval("-15.5", NULL, &ok), -15.5) && ok);
    CHECK(near(ssr_expr_eval("10/4", NULL, &ok), 2.5) && ok);
    CHECK(near(ssr_expr_eval("10/0", NULL, &ok), 0) && ok);
    CHECK(near(ssr_expr_eval("Max[300:450]+100", NULL, &ok), 550) && ok);
    CHECK(near(ssr_expr_eval("Min[300:450]", NULL, &ok), 300) && ok);
    CHECK(near(ssr_expr_eval("sin(90)+5", NULL, &ok), 6) && ok);
}

static void test_uses_the_scope(void) {
    SSRExprScope scope = {variable, tweak, NULL};
    int ok = 0;
    CHECK(near(ssr_expr_eval("[@B-960]*0.7+744", &scope, &ok), 176 * 0.7 + 744) && ok);
    CHECK(near(ssr_expr_eval("@W/2", &scope, &ok), 320) && ok);
    CHECK(near(ssr_expr_eval("[%xPos-115]", &scope, &ok), 85) && ok);
    CHECK(near(ssr_expr_eval("[twk(Player,Energy)-20]/20", &scope, &ok), 3) && ok);
    CHECK(near(ssr_expr_eval("twk(Menu, Missing)", &scope, &ok), 0) && ok);
}

static void test_reports_text_that_it_cannot_read(void) {
    int ok = 1;
    ssr_expr_eval("", NULL, &ok);
    CHECK(!ok);
    ssr_expr_eval("12abc", NULL, &ok);
    CHECK(!ok);
    ssr_expr_eval("[1+2", NULL, &ok);
    CHECK(!ok);
    ssr_expr_eval("Glint", NULL, &ok);
    CHECK(!ok);
}

int main(void) {
    test_evaluates_arithmetic();
    test_uses_the_scope();
    test_reports_text_that_it_cannot_read();
    return TEST_RESULT();
}
