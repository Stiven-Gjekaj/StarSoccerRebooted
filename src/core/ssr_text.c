#include "ssr_text.h"

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
