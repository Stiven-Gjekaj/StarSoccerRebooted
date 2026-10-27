#include "ssr_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_tsv.h"

typedef struct {
    const char *tag;
    const char *male;
    const char *female;
} SSRTextEntry;

struct SSRText {
    SSRTsv *table;
    SSRTextEntry *entries;
    int count;
    // The texts that ssr_text_rebrand makes.
    char **owned;
    int ownedCount;
};

static int compare_entries(const void *a, const void *b) {
    return strcmp(((const SSRTextEntry *)a)->tag, ((const SSRTextEntry *)b)->tag);
}

SSRText *ssr_text_load(const char *path) {
    SSRTsv *table = ssr_tsv_load(path);
    int tagColumn = ssr_tsv_column(table, "Tag");
    if (!table || tagColumn < 0) {
        ssr_tsv_free(table);
        return NULL;
    }
    SSRText *text = calloc(1, sizeof(SSRText));
    int rows = ssr_tsv_rows(table);
    if (text) text->entries = calloc(rows ? (size_t)rows : 1, sizeof(SSRTextEntry));
    if (!text || !text->entries) {
        free(text);
        ssr_tsv_free(table);
        return NULL;
    }
    text->table = table;
    // The text is in the column after the tag. The female text is next.
    for (int row = 0; row < rows; row++) {
        const char *tag = ssr_tsv_cell(table, row, tagColumn);
        if (!*tag) continue;
        SSRTextEntry *entry = &text->entries[text->count++];
        entry->tag = tag;
        entry->male = ssr_tsv_cell(table, row, tagColumn + 1);
        entry->female = ssr_tsv_cell(table, row, tagColumn + 2);
    }
    qsort(text->entries, (size_t)text->count, sizeof(SSRTextEntry), compare_entries);
    return text;
}

void ssr_text_free(SSRText *text) {
    if (!text) return;
    for (int i = 0; i < text->ownedCount; i++) free(text->owned[i]);
    free(text->owned);
    free(text->entries);
    ssr_tsv_free(text->table);
    free(text);
}

int ssr_text_count(const SSRText *text) {
    return text ? text->count : 0;
}

const char *ssr_text_get(const SSRText *text, const char *tag, int female) {
    if (!text || !tag) return tag ? tag : "";
    SSRTextEntry key = {tag, NULL, NULL};
    const SSRTextEntry *entry = bsearch(&key, text->entries, (size_t)text->count, sizeof(SSRTextEntry),
                                        compare_entries);
    if (!entry) return tag;
    if (female && *entry->female) return entry->female;
    return entry->male;
}

static const struct {
    const char *from, *to;
    // 1 when the name must be at the start of a word, 2 when it must be a
    // full word.
    int word;
} RENAMES[] = {{"New Star Soccer", "Star Soccer Rebooted", 0}, {"NS-", "SSR-", 1}, {"NSS", "SSR", 2}};

static const char *const KEEP[] = {"Credits_", "CMESSAGE_CREDITS", "GWL_"};

// A copy of s with the names of the remake, or NULL when s does not change.
static char *rename_text(const char *s) {
    size_t length = strlen(s), capacity = length * 2 + 1;
    char *out = malloc(capacity);
    if (!out) return NULL;
    size_t o = 0;
    int changed = 0;
    for (size_t i = 0; i < length;) {
        int done = 0;
        for (size_t r = 0; r < sizeof(RENAMES) / sizeof(RENAMES[0]) && !done; r++) {
            size_t n = strlen(RENAMES[r].from);
            if (strncmp(s + i, RENAMES[r].from, n) != 0) continue;
            if (RENAMES[r].word && i > 0 && isalnum((unsigned char)s[i - 1])) continue;
            if (RENAMES[r].word == 2 && isalnum((unsigned char)s[i + n])) continue;
            size_t m = strlen(RENAMES[r].to);
            if (o + m + 1 > capacity) break;
            memcpy(out + o, RENAMES[r].to, m);
            o += m;
            i += n;
            changed = done = 1;
        }
        if (done) continue;
        if (o + 2 > capacity) break;
        out[o++] = s[i++];
    }
    out[o] = 0;
    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

int ssr_text_rebrand(SSRText *text) {
    if (!text) return 0;
    if (!text->owned) text->owned = calloc((size_t)text->count * 2 + 1, sizeof(char *));
    if (!text->owned) return 0;
    int changes = 0;
    for (int i = 0; i < text->count; i++) {
        SSRTextEntry *entry = &text->entries[i];
        int keep = 0;
        for (size_t k = 0; k < sizeof(KEEP) / sizeof(KEEP[0]); k++) {
            keep |= strncmp(entry->tag, KEEP[k], strlen(KEEP[k])) == 0;
        }
        if (keep) continue;
        const char **texts[2] = {&entry->male, &entry->female};
        int entryChanged = 0;
        for (int t = 0; t < 2; t++) {
            char *renamed = rename_text(*texts[t]);
            if (!renamed) continue;
            if (text->ownedCount >= text->count * 2) {
                free(renamed);
                continue;
            }
            text->owned[text->ownedCount++] = renamed;
            *texts[t] = renamed;
            entryChanged = 1;
        }
        changes += entryChanged;
    }
    return changes;
}

size_t ssr_text_format(char *out, size_t capacity, const char *template, const char *const *names,
                       const char *const *values, int count) {
    size_t length = 0;
#define PUT(c)                                                                                              \
    do {                                                                                                    \
        if (length + 1 < capacity) out[length] = (c);                                                       \
        length++;                                                                                           \
    } while (0)
    for (const char *p = template ? template : ""; *p;) {
        int best = -1;
        size_t bestLength = 0;
        if (*p == '$') {
            for (int i = 0; i < count; i++) {
                size_t nameLength = strlen(names[i]);
                if (nameLength > bestLength && strncmp(p + 1, names[i], nameLength) == 0) {
                    best = i;
                    bestLength = nameLength;
                }
            }
        }
        if (best < 0) {
            PUT(*p);
            p++;
            continue;
        }
        for (const char *v = values[best]; *v; v++) PUT(*v);
        p += 1 + bestLength;
    }
#undef PUT
    if (capacity > 0) out[length < capacity ? length : capacity - 1] = 0;
    return length;
}
