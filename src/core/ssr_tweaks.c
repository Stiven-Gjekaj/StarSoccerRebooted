#include "ssr_tweaks.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key;
    char *value;
} Entry;

struct SSRTweaks {
    Entry *entries;
    int count;
    int capacity;
};

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
    if (!tweaks || !group || !name) return NULL;
    char *key = make_key(group, name);
    if (!key) return NULL;
    Entry *found = NULL;
    for (int i = 0; i < tweaks->count; i++) {
        if (strcmp(tweaks->entries[i].key, key) == 0) {
            found = &tweaks->entries[i];
            break;
        }
    }
    free(key);
    return found;
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
    free(tweaks);
}

void ssr_tweaks_set(SSRTweaks *tweaks, const char *group, const char *name, const char *value) {
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
    tweaks->entries[tweaks->count].key = key;
    tweaks->entries[tweaks->count].value = copy;
    tweaks->count++;
}

void ssr_tweaks_set_number(SSRTweaks *tweaks, const char *group, const char *name, double value) {
    char text[64];
    snprintf(text, sizeof(text), "%.10g", value);
    ssr_tweaks_set(tweaks, group, name, text);
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
