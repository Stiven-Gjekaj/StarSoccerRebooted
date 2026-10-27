#include "ssr_tweaks.h"

#include "ssr_fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key;
    char *value;
    SSRTweakKind kind;
    // A Range: tweak keeps its ends.
    int range;
    double min, max;
} Entry;

struct SSRTweaks {
    Entry *entries;
    int count;
    int capacity;
    // An open hash table of entry index + 1, or 0 for an empty place. It
    // has a power of two places, and at least twice as many as entries.
    int *table;
    int tableSize;
};

// FNV-1a of the key, in lower case. The key is "group\x1fname".
static unsigned int hash_key(const char *group, const char *name) {
    unsigned int h = 2166136261u;
    for (const char *p = group; *p; p++) h = (h ^ (unsigned char)tolower((unsigned char)*p)) * 16777619u;
    h = (h ^ 0x1fu) * 16777619u;
    for (const char *p = name; *p; p++) h = (h ^ (unsigned char)tolower((unsigned char)*p)) * 16777619u;
    return h;
}

// 1 when the stored key is the key of group and name, in any case.
static int same_key(const char *key, const char *group, const char *name) {
    for (; *group; group++, key++) {
        if (*key != tolower((unsigned char)*group)) return 0;
    }
    if (*key++ != '\x1f') return 0;
    for (; *name; name++, key++) {
        if (*key != tolower((unsigned char)*name)) return 0;
    }
    return *key == 0;
}

static void table_insert(SSRTweaks *tweaks, int index, unsigned int hash) {
    unsigned int mask = (unsigned int)tweaks->tableSize - 1;
    unsigned int at = hash & mask;
    while (tweaks->table[at]) at = (at + 1) & mask;
    tweaks->table[at] = index + 1;
}

// The key is "group\x1fname". The case of the letters does not matter.
static char *make_key(const char *group, const char *name) {
    size_t a = strlen(group), b = strlen(name);
    char *key = malloc(a + b + 2);
    if (!key) return NULL;
    for (size_t i = 0; i < a; i++) key[i] = (char)tolower((unsigned char)group[i]);
    key[a] = '\x1f';
    for (size_t i = 0; i < b; i++) key[a + 1 + i] = (char)tolower((unsigned char)name[i]);
    key[a + b + 1] = 0;
    return key;
}

static Entry *find(const SSRTweaks *tweaks, const char *group, const char *name) {
    if (!tweaks || !group || !name || !tweaks->tableSize) return NULL;
    unsigned int mask = (unsigned int)tweaks->tableSize - 1;
    for (unsigned int at = hash_key(group, name) & mask; tweaks->table[at]; at = (at + 1) & mask) {
        Entry *entry = &tweaks->entries[tweaks->table[at] - 1];
        if (same_key(entry->key, group, name)) return entry;
    }
    return NULL;
}

SSRTweaks *ssr_tweaks_create(void) {
    return calloc(1, sizeof(SSRTweaks));
}

void ssr_tweaks_free(SSRTweaks *tweaks) {
    if (!tweaks) return;
    for (int i = 0; i < tweaks->count; i++) {
        free(tweaks->entries[i].key);
        free(tweaks->entries[i].value);
    }
    free(tweaks->entries);
    free(tweaks->table);
    free(tweaks);
}

static void set_value(SSRTweaks *tweaks, const char *group, const char *name, const char *value, SSRTweakKind kind);

void ssr_tweaks_set(SSRTweaks *tweaks, const char *group, const char *name, const char *value) {
    set_value(tweaks, group, name, value, SSR_TWEAK_STRING);
}

void ssr_tweaks_set_kind(SSRTweaks *tweaks, const char *group, const char *name, const char *value,
                         SSRTweakKind kind) {
    set_value(tweaks, group, name, value, kind);
    Entry *entry = find(tweaks, group, name);
    if (entry) entry->kind = kind;
}

SSRTweakKind ssr_tweaks_kind(const SSRTweaks *tweaks, const char *group, const char *name) {
    Entry *entry = find(tweaks, group, name);
    return entry ? entry->kind : SSR_TWEAK_NONE;
}

// A new tweak takes the kind. A tweak that is there keeps its kind.
static void set_value(SSRTweaks *tweaks, const char *group, const char *name, const char *value, SSRTweakKind kind) {
    if (!tweaks || !group || !name) return;
    char *copy = malloc(strlen(value ? value : "") + 1);
    if (!copy) return;
    strcpy(copy, value ? value : "");
    Entry *entry = find(tweaks, group, name);
    if (entry) {
        free(entry->value);
        entry->value = copy;
        return;
    }
    if (tweaks->count == tweaks->capacity) {
        int capacity = tweaks->capacity ? tweaks->capacity * 2 : 64;
        Entry *entries = realloc(tweaks->entries, (size_t)capacity * sizeof(Entry));
        if (!entries) {
            free(copy);
            return;
        }
        tweaks->entries = entries;
        tweaks->capacity = capacity;
    }
    char *key = make_key(group, name);
    if (!key) {
        free(copy);
        return;
    }
    memset(&tweaks->entries[tweaks->count], 0, sizeof(Entry));
    tweaks->entries[tweaks->count].kind = kind;
    tweaks->entries[tweaks->count].key = key;
    tweaks->entries[tweaks->count].value = copy;
    tweaks->count++;
    if (tweaks->count * 2 > tweaks->tableSize) {
        int size = tweaks->tableSize ? tweaks->tableSize * 2 : 256;
        int *table = calloc((size_t)size, sizeof(int));
        if (!table) return;
        free(tweaks->table);
        tweaks->table = table;
        tweaks->tableSize = size;
        for (int i = 0; i < tweaks->count; i++) {
            const char *k = tweaks->entries[i].key;
            unsigned int h = 2166136261u;
            for (const char *q = k; *q; q++) h = (h ^ (unsigned char)*q) * 16777619u;
            table_insert(tweaks, i, h);
        }
    } else {
        table_insert(tweaks, tweaks->count - 1, hash_key(group, name));
    }
}

void ssr_tweaks_set_number(SSRTweaks *tweaks, const char *group, const char *name, double value) {
    char text[64];
    snprintf(text, sizeof(text), "%.10g", value);
    set_value(tweaks, group, name, text, SSR_TWEAK_FLOAT);
}

const char *ssr_tweaks_get(const SSRTweaks *tweaks, const char *group, const char *name) {
    Entry *entry = find(tweaks, group, name);
    return entry ? entry->value : "";
}

static int to_number(const char *text, double *value) {
    char *end;
    *value = strtod(text, &end);
    return end != text && *end == 0;
}

double ssr_tweaks_number(const SSRTweaks *tweaks, const char *group, const char *name) {
    double value;
    return to_number(ssr_tweaks_get(tweaks, group, name), &value) ? value : 0;
}

int ssr_tweaks_equals(const SSRTweaks *tweaks, const char *group, const char *name, const char *text) {
    const char *value = ssr_tweaks_get(tweaks, group, name);
    if (!*value) value = "0";
    double a, b;
    if (to_number(value, &a) && to_number(text, &b)) return a == b;
    return strcmp(value, text) == 0;
}

// Splits the text between the brackets at the commas. Returns the number
// of parts.
static int split_args(char *text, char **parts, int max) {
    char *open = strchr(text, '(');
    char *close = open ? strrchr(open, ')') : NULL;
    if (!open || !close) return 0;
    *close = 0;
    int count = 0;
    char *p = open + 1;
    while (count < max) {
        char *comma = strchr(p, ',');
        if (comma) *comma = 0;
        while (isspace((unsigned char)*p)) p++;
        char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) *--end = 0;
        parts[count++] = p;
        if (!comma) break;
        p = comma + 1;
    }
    return count;
}

int ssr_tweaks_load(SSRTweaks *tweaks, const char *path) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return -1;
    char line[1024], group[128] = "";
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        char *comment = strstr(line, "//");
        if (comment) *comment = 0;
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        char *parts[16];
        if (strncmp(p, "Category:(", 10) == 0) {
            int n = split_args(p, parts, 16);
            snprintf(group, sizeof(group), "%s", n > 0 ? parts[0] : "");
        } else if (strncmp(p, "EndCategory", 11) == 0) {
            group[0] = 0;
        } else if (!group[0]) {
            continue;
        } else if (strncmp(p, "Float:(", 7) == 0 || strncmp(p, "Var:(", 5) == 0) {
            int n = split_args(p, parts, 16);
            if (n < 3) continue;
            double value = strtod(parts[2], NULL), scale = n >= 4 ? strtod(parts[3], NULL) : 1;
            ssr_tweaks_set_number(tweaks, group, parts[0], value * scale);
            Entry *entry = find(tweaks, group, parts[0]);
            if (entry) entry->kind = SSR_TWEAK_FLOAT;
            count++;
        } else if (strncmp(p, "Range:(", 7) == 0) {
            int n = split_args(p, parts, 16);
            if (n < 3) continue;
            double scale = n >= 5 ? strtod(parts[4], NULL) : 1;
            double min = strtod(parts[2], NULL) * scale, max = n >= 4 ? strtod(parts[3], NULL) * scale : min;
            ssr_tweaks_set_number(tweaks, group, parts[0], min);
            Entry *entry = find(tweaks, group, parts[0]);
            if (entry) {
                entry->kind = SSR_TWEAK_RANGE;
                entry->range = 1;
                entry->min = min;
                entry->max = max;
            }
            count++;
        } else if (strncmp(p, "Colour:(", 8) == 0 || strncmp(p, "String:(", 8) == 0) {
            int n = split_args(p, parts, 16);
            if (n < 1) continue;
            ssr_tweaks_set_kind(tweaks, group, parts[0], n >= 3 ? parts[2] : "",
                                p[0] == 'C' ? SSR_TWEAK_COLOUR : SSR_TWEAK_STRING);
            count++;
        }
    }
    fclose(file);
    return count;
}

int ssr_tweaks_range(const SSRTweaks *tweaks, const char *group, const char *name, double *min, double *max) {
    Entry *entry = find(tweaks, group, name);
    if (!entry || !entry->range) return 0;
    *min = entry->min;
    *max = entry->max;
    return 1;
}

unsigned int ssr_tweaks_colour(const SSRTweaks *tweaks, const char *group, const char *name, unsigned int fallback) {
    const char *text = ssr_tweaks_get(tweaks, group, name);
    if (*text == '#') text++;
    size_t length = strlen(text);
    if (length != 6 && length != 8) return fallback;
    unsigned int value = 0;
    for (size_t i = 0; i < length; i++) {
        char c = (char)tolower((unsigned char)text[i]);
        int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0) return fallback;
        value = (value << 4) | (unsigned int)digit;
    }
    return length == 6 ? (value << 8) | 0xFF : value;
}
