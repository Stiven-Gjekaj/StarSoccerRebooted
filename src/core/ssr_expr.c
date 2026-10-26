#include "ssr_expr.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    const char *p;
    const SSRExprScope *scope;
    int ok;
} Parser;

static double expression(Parser *ps);

static void skip_spaces(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t') ps->p++;
}

static int accept(Parser *ps, char c) {
    skip_spaces(ps);
    if (*ps->p != c) return 0;
    ps->p++;
    return 1;
}

static int is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '.';
}

// Reads a name into out. Returns its length.
static size_t read_name(Parser *ps, char *out, size_t capacity) {
    size_t length = 0;
    while (is_name_char(*ps->p)) {
        if (length + 1 < capacity) out[length] = *ps->p;
        length++;
        ps->p++;
    }
    if (capacity) out[length < capacity ? length : capacity - 1] = 0;
    return length;
}

// Reads the text up to the next ',' or ')' at this depth, for the
// arguments of twk().
static void read_argument(Parser *ps, char *out, size_t capacity) {
    size_t length = 0;
    skip_spaces(ps);
    while (*ps->p && *ps->p != ',' && *ps->p != ')') {
        if (length + 1 < capacity) out[length++] = *ps->p;
        ps->p++;
    }
    while (length > 0 && (out[length - 1] == ' ' || out[length - 1] == '\t')) length--;
    out[length] = 0;
}

static double function(Parser *ps, const char *name) {
    if (strcmp(name, "twk") == 0) {
        char group[64], key[64];
        read_argument(ps, group, sizeof(group));
        if (!accept(ps, ',')) ps->ok = 0;
        read_argument(ps, key, sizeof(key));
        if (!accept(ps, ')')) ps->ok = 0;
        return ps->scope && ps->scope->tweak ? ps->scope->tweak(ps->scope->context, group, key) : 0;
    }
    double value = expression(ps);
    if (!accept(ps, ')')) ps->ok = 0;
    if (strcmp(name, "sin") == 0) return sin(value * M_PI / 180);
    if (strcmp(name, "cos") == 0) return cos(value * M_PI / 180);
    if (strcmp(name, "abs") == 0) return fabs(value);
    if (strcmp(name, "int") == 0) return floor(value);
    ps->ok = 0;
    return 0;
}

static double primary(Parser *ps) {
    skip_spaces(ps);
    char c = *ps->p;
    if (c == '[') {
        ps->p++;
        double value = expression(ps);
        if (!accept(ps, ']')) ps->ok = 0;
        return value;
    }
    if (c == '-') {
        ps->p++;
        return -primary(ps);
    }
    if (c == '+') {
        ps->p++;
        return primary(ps);
    }
    if (isdigit((unsigned char)c) || c == '.') {
        char *end;
        double value = strtod(ps->p, &end);
        ps->p = end;
        return value;
    }
    if (c == '@' || c == '%') {
        ps->p++;
        char name[64];
        if (!read_name(ps, name, sizeof(name))) ps->ok = 0;
        return ps->scope && ps->scope->variable ? ps->scope->variable(ps->scope->context, c, name) : 0;
    }
    if (isalpha((unsigned char)c)) {
        char name[32];
        read_name(ps, name, sizeof(name));
        if (strcmp(name, "Max") == 0 || strcmp(name, "Min") == 0) {
            if (!accept(ps, '[')) {
                ps->ok = 0;
                return 0;
            }
            double a = expression(ps);
            if (!accept(ps, ':')) ps->ok = 0;
            double b = expression(ps);
            if (!accept(ps, ']')) ps->ok = 0;
            return name[1] == 'a' ? (a > b ? a : b) : (a < b ? a : b);
        }
        if (accept(ps, '(')) return function(ps, name);
    }
    ps->ok = 0;
    return 0;
}

static double term(Parser *ps) {
    double value = primary(ps);
    for (;;) {
        if (accept(ps, '*')) {
            value *= primary(ps);
        } else if (accept(ps, '/')) {
            double divisor = primary(ps);
            value = divisor != 0 ? value / divisor : 0;
        } else {
            return value;
        }
    }
}

static double expression(Parser *ps) {
    double value = term(ps);
    for (;;) {
        if (accept(ps, '+')) {
            value += term(ps);
        } else if (accept(ps, '-')) {
            value -= term(ps);
        } else {
            return value;
        }
    }
}

double ssr_expr_eval(const char *text, const SSRExprScope *scope, int *ok) {
    Parser ps = {text ? text : "", scope, 1};
    skip_spaces(&ps);
    if (!*ps.p) {
        if (ok) *ok = 0;
        return 0;
    }
    double value = expression(&ps);
    skip_spaces(&ps);
    if (*ps.p) ps.ok = 0;
    if (ok) *ok = ps.ok;
    return value;
}
