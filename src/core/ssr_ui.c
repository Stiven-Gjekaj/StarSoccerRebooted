#include "ssr_ui.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 64
#define MAX_IMPORT_DEPTH 16
#define MAX_MACRO_DEPTH 32

typedef struct {
    char *key;
    char *value;
} Pair;

typedef struct {
    Pair *items;
    int count;
    int capacity;
} PairList;

typedef struct {
    char *name;
    char **params;
    int paramCount;
    char *body;
    size_t bodyLength;
    size_t bodyCapacity;
} Macro;

typedef struct {
    SSRUiNode *owner;
    // 0: the children of owner. 1: the children of the last modifier of
    // owner.
    int modifierBlock;
    SSRUiNode *target;
} Frame;

struct SSRUiTree {
    SSRUiNode *root;
    PairList constants;
    Macro *macros;
    int macroCount;
    char **problems;
    int problemCount;
    char **fileNames;
    int fileNameCount;
};

typedef struct {
    SSRUiTree *tree;
    const SSRUiFiles *files;
    Frame frames[MAX_DEPTH];
    int frameCount;
    // The If: stack. Each level is active or not.
    int ifActive[MAX_DEPTH];
    int ifTaken[MAX_DEPTH];
    int ifCount;
    // The macro that the parser collects now, or NULL.
    Macro *collecting;
    int importDepth;
    int macroDepth;
    const char *file;
    int line;
    // PushParent() keeps the element that takes the next block and the next
    // modifiers. PopParent() gives it back: then it takes them in place of
    // the last element of its block, until a new element comes there.
    SSRUiNode *pushed[MAX_DEPTH];
    int pushedCount;
    SSRUiNode *forced;
    SSRUiNode *forcedIn;
} Parser;

static char *copy_text(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, text, length);
    copy[length] = 0;
    return copy;
}

static void problem(Parser *p, const char *format, ...) {
    char message[512];
    int used = snprintf(message, sizeof(message), "%s:%d: ", p->file ? p->file : "?", p->line);
    va_list args;
    va_start(args, format);
    if (used > 0 && (size_t)used < sizeof(message)) vsnprintf(message + used, sizeof(message) - (size_t)used, format, args);
    va_end(args);
    char **problems = realloc(p->tree->problems, (size_t)(p->tree->problemCount + 1) * sizeof(char *));
    if (!problems) return;
    p->tree->problems = problems;
    p->tree->problems[p->tree->problemCount] = copy_text(message, strlen(message));
    if (p->tree->problems[p->tree->problemCount]) p->tree->problemCount++;
}

static void push_child(SSRUiNode ***list, int *count, SSRUiNode *node) {
    SSRUiNode **grown = realloc(*list, (size_t)(*count + 1) * sizeof(SSRUiNode *));
    if (!grown) return;
    *list = grown;
    (*list)[(*count)++] = node;
}

static void free_node(SSRUiNode *node) {
    if (!node) return;
    for (int i = 0; i < node->argCount; i++) free(node->args[i]);
    free(node->args);
    for (int i = 0; i < node->childCount; i++) free_node(node->children[i]);
    free(node->children);
    for (int i = 0; i < node->modifierCount; i++) free_node(node->modifiers[i]);
    free(node->modifiers);
    free(node->ref);
    free(node->raw);
    free(node->name);
    free(node);
}

static const char *pair_get(const PairList *list, const char *key, size_t keyLength) {
    for (int i = list->count - 1; i >= 0; i--) {
        if (strlen(list->items[i].key) == keyLength && strncmp(list->items[i].key, key, keyLength) == 0) {
            return list->items[i].value;
        }
    }
    return NULL;
}

static void pair_set(PairList *list, const char *key, const char *value) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].key, key) == 0) {
            char *copy = copy_text(value, strlen(value));
            if (!copy) return;
            free(list->items[i].value);
            list->items[i].value = copy;
            return;
        }
    }
    if (list->count == list->capacity) {
        int capacity = list->capacity ? list->capacity * 2 : 32;
        Pair *items = realloc(list->items, (size_t)capacity * sizeof(Pair));
        if (!items) return;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count].key = copy_text(key, strlen(key));
    list->items[list->count].value = copy_text(value, strlen(value));
    list->count++;
}

// Replaces each "?NAME?" that params or the constants know. Returns a new
// buffer.
static char *substitute(const char *text, const PairList *params, const PairList *constants) {
    size_t capacity = strlen(text) + 64;
    char *out = malloc(capacity);
    if (!out) return NULL;
    size_t length = 0;
    const char *p = text;
    while (*p) {
        const char *value = NULL;
        size_t skip = 1;
        if (*p == '?') {
            const char *end = strchr(p + 1, '?');
            if (end && end > p + 1) {
                size_t keyLength = (size_t)(end - p) + 1;
                if (params) value = pair_get(params, p, keyLength);
                if (!value && constants) value = pair_get(constants, p, keyLength);
                if (value) skip = keyLength;
            }
        }
        const char *piece = value ? value : p;
        size_t pieceLength = value ? strlen(value) : 1;
        if (length + pieceLength + 1 > capacity) {
            capacity = (length + pieceLength + 1) * 2;
            char *grown = realloc(out, capacity);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        memcpy(out + length, piece, pieceLength);
        length += pieceLength;
        p += skip;
    }
    out[length] = 0;
    return out;
}

static Macro *find_macro(SSRUiTree *tree, const char *name) {
    for (int i = tree->macroCount - 1; i >= 0; i--) {
        if (strcmp(tree->macros[i].name, name) == 0) return &tree->macros[i];
    }
    return NULL;
}

static int if_active(const Parser *p) {
    return p->ifCount == 0 || p->ifActive[p->ifCount - 1];
}

static Frame *top(Parser *p) {
    return &p->frames[p->frameCount - 1];
}

static SSRUiNode *last_element(Parser *p) {
    SSRUiNode *target = top(p)->target;
    if (p->forced && p->forcedIn == target) return p->forced;
    for (int i = target->childCount - 1; i >= 0; i--) {
        if (target->children[i]->flags & SSR_UI_ELEMENT) return target->children[i];
    }
    return NULL;
}

static void open_block(Parser *p, int modifierBlock) {
    if (p->frameCount >= MAX_DEPTH) {
        problem(p, "the blocks go too deep");
        return;
    }
    SSRUiNode *owner = last_element(p);
    SSRUiNode *target = owner;
    if (modifierBlock && owner && owner->modifierCount > 0) target = owner->modifiers[owner->modifierCount - 1];
    if (!target) {
        // A block with no element before it gets a group of its own.
        target = calloc(1, sizeof(SSRUiNode));
        if (!target) return;
        target->name = copy_text("Group", 5);
        target->flags = SSR_UI_ELEMENT;
        target->file = p->file;
        target->line = p->line;
        push_child(&top(p)->target->children, &top(p)->target->childCount, target);
    }
    if (!modifierBlock && !(target->flags & SSR_UI_BLOCK)) {
        target->flags |= SSR_UI_BLOCK;
        target->blockAt = target->modifierCount;
    }
    Frame *frame = &p->frames[p->frameCount++];
    frame->owner = owner;
    frame->modifierBlock = modifierBlock;
    frame->target = target;
}

static void close_block(Parser *p, int modifierBlock) {
    if (p->frameCount <= 1 || top(p)->modifierBlock != modifierBlock) {
        problem(p, "a '%c' closes no block", modifierBlock ? '>' : '}');
        return;
    }
    p->frameCount--;
}

// Splits the text between the brackets of a line into arguments. The text
// can hold () and [] pairs. The line can end before the last ')'.
static int read_args(const char **cursor, char ***args) {
    const char *p = *cursor;
    int depth = 0, count = 0, quoted = 0;
    char **list = NULL;
    const char *start = p;
    for (;; p++) {
        char c = *p;
        // A bracket or a comma in quotes is a part of the text.
        if (c == '"') quoted = !quoted;
        if (quoted && c) continue;
        int end = c == 0 || (c == ')' && depth == 0);
        if (end || (c == ',' && depth == 0)) {
            const char *a = start, *b = p;
            while (a < b && isspace((unsigned char)*a)) a++;
            while (b > a && isspace((unsigned char)b[-1])) b--;
            char **grown = realloc(list, (size_t)(count + 1) * sizeof(char *));
            if (!grown) break;
            list = grown;
            list[count++] = copy_text(a, (size_t)(b - a));
            start = p + 1;
            if (end) break;
            continue;
        }
        if (c == '(' || c == '[') depth++;
        if ((c == ')' || c == ']') && depth > 0) depth--;
    }
    *cursor = *p ? p + 1 : p;
    // "Name()" has no arguments.
    if (count == 1 && list[0][0] == 0) {
        free(list[0]);
        count = 0;
    }
    *args = list;
    return count;
}

static void parse_text(Parser *p, const char *text, const PairList *params);

static void import_file(Parser *p, const char *name) {
    if (p->importDepth >= MAX_IMPORT_DEPTH) {
        problem(p, "the imports go too deep at %s", name);
        return;
    }
    size_t size = 0;
    char *text = p->files && p->files->read ? p->files->read(p->files->context, name, &size) : NULL;
    if (!text) {
        problem(p, "cannot import %s", name);
        return;
    }
    char *source = copy_text(text, size);
    free(text);
    if (!source) return;
    char **names = realloc(p->tree->fileNames, (size_t)(p->tree->fileNameCount + 1) * sizeof(char *));
    if (names) {
        p->tree->fileNames = names;
        names[p->tree->fileNameCount] = copy_text(name, strlen(name));
        if (names[p->tree->fileNameCount]) p->tree->fileNameCount++;
    }
    const char *savedFile = p->file;
    int savedLine = p->line;
    p->file = names ? names[p->tree->fileNameCount - 1] : name;
    p->importDepth++;
    parse_text(p, source, NULL);
    p->importDepth--;
    p->file = savedFile;
    p->line = savedLine;
    free(source);
}

static void call_macro(Parser *p, Macro *macro, char **args, int argCount) {
    if (p->macroDepth >= MAX_MACRO_DEPTH) {
        problem(p, "the macros go too deep at %s", macro->name);
        return;
    }
    PairList params = {0};
    for (int i = 0; i < macro->paramCount; i++) pair_set(&params, macro->params[i], i < argCount ? args[i] : "");
    p->macroDepth++;
    int savedLine = p->line;
    parse_text(p, macro->body ? macro->body : "", &params);
    p->line = savedLine;
    p->macroDepth--;
    for (int i = 0; i < params.count; i++) {
        free(params.items[i].key);
        free(params.items[i].value);
    }
    free(params.items);
}

static void start_macro(Parser *p, char **args, int argCount) {
    if (argCount < 1 || !args[0][0]) {
        problem(p, "a macro has no name");
        return;
    }
    Macro *macros = realloc(p->tree->macros, (size_t)(p->tree->macroCount + 1) * sizeof(Macro));
    if (!macros) return;
    p->tree->macros = macros;
    Macro *macro = &macros[p->tree->macroCount++];
    memset(macro, 0, sizeof(*macro));
    macro->name = copy_text(args[0], strlen(args[0]));
    macro->paramCount = argCount - 1;
    macro->params = calloc((size_t)(argCount > 1 ? argCount - 1 : 1), sizeof(char *));
    for (int i = 1; i < argCount && macro->params; i++) macro->params[i - 1] = copy_text(args[i], strlen(args[i]));
    p->collecting = macro;
}

static void collect_line(Macro *macro, const char *line, size_t length) {
    if (macro->bodyLength + length + 2 > macro->bodyCapacity) {
        size_t capacity = (macro->bodyLength + length + 2) * 2;
        char *grown = realloc(macro->body, capacity);
        if (!grown) return;
        macro->body = grown;
        macro->bodyCapacity = capacity;
    }
    memcpy(macro->body + macro->bodyLength, line, length);
    macro->bodyLength += length;
    macro->body[macro->bodyLength++] = '\n';
    macro->body[macro->bodyLength] = 0;
}

// "If:(?A?)" is true when the text is not empty, not "0", and has no "?"
// name that nothing replaced. "If:(?A?=B)" compares the two texts.
static int condition(const char *text) {
    const char *equals = strchr(text, '=');
    if (equals) {
        size_t left = (size_t)(equals - text);
        return strlen(equals + 1) == left && strncmp(text, equals + 1, left) == 0;
    }
    if (!*text || strcmp(text, "0") == 0) return 0;
    return !(text[0] == '?' && text[strlen(text) - 1] == '?');
}

static void define_constant(Parser *p, char **args, int argCount) {
    if (argCount < 1) return;
    // The value can have commas, so the arguments go back together.
    size_t length = 0;
    for (int i = 0; i < argCount; i++) length += strlen(args[i]) + 1;
    char *all = calloc(1, length + 1);
    if (!all) return;
    for (int i = 0; i < argCount; i++) {
        if (i) strcat(all, ",");
        strcat(all, args[i]);
    }
    char *equals = strchr(all, '=');
    if (!equals) {
        problem(p, "a constant has no '='");
    } else {
        *equals = 0;
        pair_set(&p->tree->constants, all, equals + 1);
    }
    free(all);
}

static void free_args(char **args, int count) {
    if (!args) return;
    for (int i = 0; i < count; i++) free(args[i]);
    free(args);
}

static int is_word(const char *text, const char *word) {
    size_t length = strlen(word);
    return strncmp(text, word, length) == 0 && !isalnum((unsigned char)text[length]) && text[length] != '_';
}

// These lines change the state of the parser in the original, so they stay
// in their place and are not modifiers of the element before them.
static int is_command(const char *name) {
    static const char *commands[] = {"PopOrigin", "PushParent", "PopParent", "ParseStop", "Stop"};
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(name, commands[i]) == 0) return 1;
    }
    return 0;
}

// Handles one statement. line has its comment and its outer spaces gone.
static void parse_statement(Parser *p, const char *line) {
    unsigned flags = 0;
    for (;; line++) {
        if (*line == '.') {
            flags |= SSR_UI_DOT;
        } else if (*line == '!') {
            flags |= SSR_UI_BANG;
        } else if (*line == ':') {
            flags |= SSR_UI_DOT | SSR_UI_REARM;
        } else {
            break;
        }
    }
    while (*line == '{' || *line == '}' || *line == '<' || *line == '>') {
        if (line[0] == '{' && line[1] == '{') {
            // "{{" is one block. Its children are in the box of the element.
            open_block(p, 0);
            if (top(p)->owner) top(p)->owner->flags |= SSR_UI_BOXED;
            line++;
        } else if (line[0] == '}' && line[1] == '}') {
            close_block(p, 0);
            line++;
        } else if (*line == '{') {
            open_block(p, 0);
        } else if (*line == '}') {
            close_block(p, 0);
        } else if (*line == '<') {
            open_block(p, 1);
        } else if (*line == '>') {
            close_block(p, 1);
        }
        line++;
        while (isspace((unsigned char)*line)) line++;
    }
    if (!*line) return;
    if (strncmp(line, "$HIDE", 5) == 0) {
        SSRUiNode *element = last_element(p);
        if (element) element->flags |= SSR_UI_HIDE;
        return;
    }
    if (!isalpha((unsigned char)*line) && *line != '_') {
        problem(p, "cannot read \"%.40s\"", line);
        return;
    }
    const char *nameStart = line;
    while (isalnum((unsigned char)*line) || *line == '_') line++;
    size_t nameLength = (size_t)(line - nameStart);
    if (*line == ':') {
        flags |= SSR_UI_ELEMENT;
        line++;
    }
    char **args = NULL;
    int argCount = 0;
    char *raw = NULL;
    if (*line == '(') {
        line++;
        const char *start = line;
        argCount = read_args(&line, &args);
        const char *end = start;
        int depth = 0, quoted = 0;
        while (*end && !(*end == ')' && depth == 0 && !quoted)) {
            if (*end == '"') quoted = !quoted;
            if (quoted) {
                end++;
                continue;
            }
            if (*end == '(' || *end == '[') depth++;
            if ((*end == ')' || *end == ']') && depth > 0) depth--;
            end++;
        }
        raw = copy_text(start, (size_t)(end - start));
    }
    char *ref = NULL;
    const char *refAt = strstr(line, "ref:");
    if (refAt) {
        const char *a = refAt + 4;
        while (*a == ' ' || *a == '\t') a++;
        const char *b = a;
        while (*b && !isspace((unsigned char)*b)) b++;
        if (b > a) ref = copy_text(a, (size_t)(b - a));
    }
    if (strstr(line, "$HIDE")) flags |= SSR_UI_HIDE;
    char *name = copy_text(nameStart, nameLength);
    if (!name) {
        free_args(args, argCount);
        free(ref);
        free(raw);
        return;
    }

    if (strcmp(name, "Import") == 0) {
        if (argCount > 0) import_file(p, args[0]);
    } else if (strcmp(name, "Macro") == 0) {
        start_macro(p, args, argCount);
    } else if (strcmp(name, "Const") == 0 || strcmp(name, "Define") == 0) {
        define_constant(p, args, argCount);
    } else if (strncmp(name, "M_", 2) == 0 && find_macro(p->tree, name)) {
        call_macro(p, find_macro(p->tree, name), args, argCount);
    } else if (strcmp(name, "EndM") == 0) {
        problem(p, "an EndM with no Macro");
    } else {
        SSRUiNode *node = calloc(1, sizeof(SSRUiNode));
        if (node) {
            node->name = name;
            name = NULL;
            node->flags = flags;
            node->args = args;
            node->argCount = argCount;
            node->raw = raw;
            raw = NULL;
            node->ref = ref;
            node->file = p->file;
            node->line = p->line;
            args = NULL;
            argCount = 0;
            ref = NULL;
            SSRUiNode *owner = (flags & SSR_UI_ELEMENT) || is_command(node->name) ? NULL : last_element(p);
            if (strcmp(node->name, "PushParent") == 0 && p->pushedCount < MAX_DEPTH) {
                p->pushed[p->pushedCount++] = last_element(p);
            } else if (strcmp(node->name, "PopParent") == 0 && p->pushedCount > 0) {
                p->forced = p->pushed[--p->pushedCount];
                p->forcedIn = top(p)->target;
            } else if ((flags & SSR_UI_ELEMENT) && p->forcedIn == top(p)->target) {
                p->forced = NULL;
            }
            if (owner) {
                push_child(&owner->modifiers, &owner->modifierCount, node);
            } else {
                push_child(&top(p)->target->children, &top(p)->target->childCount, node);
            }
        }
    }
    free(name);
    free_args(args, argCount);
    free(ref);
    free(raw);
}

// Handles the If:, Else, and EndIf lines. Returns 1 when the line was one
// of them.
static int parse_condition(Parser *p, const char *line, const PairList *params) {
    if (is_word(line, "EndIf")) {
        if (p->ifCount > 0) {
            p->ifCount--;
        } else {
            problem(p, "an EndIf with no If");
        }
        return 1;
    }
    if (is_word(line, "Else")) {
        if (p->ifCount == 0) {
            problem(p, "an Else with no If");
            return 1;
        }
        int outer = p->ifCount < 2 || p->ifActive[p->ifCount - 2];
        p->ifActive[p->ifCount - 1] = outer && !p->ifTaken[p->ifCount - 1];
        p->ifTaken[p->ifCount - 1] = 1;
        return 1;
    }
    if (strncmp(line, "If:(", 4) == 0) {
        if (p->ifCount >= MAX_DEPTH) {
            problem(p, "the Ifs go too deep");
            return 1;
        }
        int outer = if_active(p);
        int value = 0;
        if (outer) {
            char *text = substitute(line + 4, params, &p->tree->constants);
            if (text) {
                const char *cursor = text;
                char **args = NULL;
                int count = read_args(&cursor, &args);
                value = count > 0 && condition(args[0]);
                free_args(args, count);
                free(text);
            }
        }
        p->ifActive[p->ifCount] = outer && value;
        p->ifTaken[p->ifCount] = value;
        p->ifCount++;
        return 1;
    }
    return 0;
}

static void parse_text(Parser *p, const char *text, const PairList *params) {
    int startIfCount = p->ifCount;
    int line = 0;
    const char *at = text;
    while (*at) {
        const char *end = strchr(at, '\n');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        line++;
        if (!p->macroDepth) p->line = line;
        char *raw = copy_text(at, length);
        at += length + (end ? 1 : 0);
        if (!raw) continue;
        size_t rawLength = strlen(raw);
        if (rawLength && raw[rawLength - 1] == '\r') raw[--rawLength] = 0;

        // A macro keeps its lines as they are, up to EndM.
        if (p->collecting) {
            const char *trimmed = raw;
            while (isspace((unsigned char)*trimmed)) trimmed++;
            if (is_word(trimmed, "EndM")) {
                p->collecting = NULL;
            } else {
                collect_line(p->collecting, raw, rawLength);
            }
            free(raw);
            continue;
        }

        // A "//" ends the line, but not in "http://".
        for (char *c = raw; (c = strstr(c, "//")) != NULL; c += 2) {
            if (c > raw && c[-1] == ':') continue;
            *c = 0;
            break;
        }
        char *start = raw;
        while (isspace((unsigned char)*start)) start++;
        char *stop = start + strlen(start);
        while (stop > start && isspace((unsigned char)stop[-1])) *--stop = 0;

        if (*start && !parse_condition(p, start, params) && if_active(p)) {
            char *expanded = substitute(start, params, &p->tree->constants);
            if (expanded) parse_statement(p, expanded);
            free(expanded);
        }
        free(raw);
    }
    if (p->ifCount > startIfCount && !p->macroDepth) {
        problem(p, "an If has no EndIf");
        p->ifCount = startIfCount;
    }
}

SSRUiTree *ssr_ui_parse(const char *name, const SSRUiFiles *files) {
    SSRUiTree *tree = calloc(1, sizeof(SSRUiTree));
    if (!tree) return NULL;
    tree->root = calloc(1, sizeof(SSRUiNode));
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    tree->root->name = copy_text("Screen", 6);
    Parser p = {0};
    p.tree = tree;
    p.files = files;
    p.frames[0].target = tree->root;
    p.frameCount = 1;

    size_t size = 0;
    char *text = files && files->read ? files->read(files->context, name, &size) : NULL;
    if (!text) {
        ssr_ui_free(tree);
        return NULL;
    }
    free(text);
    import_file(&p, name);
    if (p.collecting) problem(&p, "the macro %s has no EndM", p.collecting->name);
    if (p.frameCount > 1) problem(&p, "%d blocks are not closed", p.frameCount - 1);
    return tree;
}

void ssr_ui_free(SSRUiTree *tree) {
    if (!tree) return;
    free_node(tree->root);
    for (int i = 0; i < tree->constants.count; i++) {
        free(tree->constants.items[i].key);
        free(tree->constants.items[i].value);
    }
    free(tree->constants.items);
    for (int i = 0; i < tree->macroCount; i++) {
        Macro *m = &tree->macros[i];
        for (int j = 0; j < m->paramCount; j++) free(m->params ? m->params[j] : NULL);
        free(m->params);
        free(m->body);
        free(m->name);
    }
    free(tree->macros);
    for (int i = 0; i < tree->problemCount; i++) free(tree->problems[i]);
    free(tree->problems);
    for (int i = 0; i < tree->fileNameCount; i++) free(tree->fileNames[i]);
    free(tree->fileNames);
    free(tree);
}

const SSRUiNode *ssr_ui_root(const SSRUiTree *tree) {
    return tree ? tree->root : NULL;
}

int ssr_ui_problem_count(const SSRUiTree *tree) {
    return tree ? tree->problemCount : 0;
}

const char *ssr_ui_problem(const SSRUiTree *tree, int index) {
    if (!tree || index < 0 || index >= tree->problemCount) return "";
    return tree->problems[index];
}

const char *ssr_ui_constant(const SSRUiTree *tree, const char *name) {
    if (!tree || !name) return NULL;
    return pair_get(&tree->constants, name, strlen(name));
}
