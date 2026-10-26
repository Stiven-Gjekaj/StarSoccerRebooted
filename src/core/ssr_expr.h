// ssr_expr: the arithmetic of the screen layouts of the game, for example
// "[@B-960]*0.7+744" or "Max[?W?:?H?]+100".
//
// Numbers, + - * /, unary minus, and [ ] for groups. The comparisons
// =, ==, !=, <>, <, >, <=, and >= give 1 or 0. Max[a:b] and Min[a:b]
// take two values. sin(x) and cos(x) take degrees. "@name" and "%name" are
// variables, and "twk(Group,Name)" is a tweak value. The caller gives their
// values. A part that the evaluator does not know counts as 0.

#ifndef SSR_EXPR_H
#define SSR_EXPR_H

typedef struct {
    // Gives the value of "@name" or "%name". kind is '@' or '%'.
    double (*variable)(void *context, char kind, const char *name);
    // Gives the value of "twk(group,name)".
    double (*tweak)(void *context, const char *group, const char *name);
    void *context;
} SSRExprScope;

// Evaluates the text. scope can be NULL. ok gets 1 when the evaluator read
// all of the text, else 0. ok can be NULL.
double ssr_expr_eval(const char *text, const SSRExprScope *scope, int *ok);

#endif
