#include "ssr_tsv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SSRTsv {
    char *text;
    // cells[row * columns + column]. Row 0 is the header row.
    const char **cells;
    int columns;
    int rows;
};

static int count_cells(const char *line) {
    int cells = 1;
    for (; *line; line++) {
        if (*line == '\t') cells++;
    }
    return cells;
}

SSRTsv *ssr_tsv_parse(const char *text, size_t size) {
    if (!text) return NULL;
    if (size >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) {
        text += 3;
        size -= 3;
    }
    SSRTsv *table = calloc(1, sizeof(SSRTsv));
    if (!table) return NULL;
    table->text = malloc(size + 1);
    if (!table->text) {
        free(table);
        return NULL;
    }
    memcpy(table->text, text, size);
    table->text[size] = 0;

    // Cut the text into lines. Each line ends at LF. A CR before it goes.
    int lineCount = 0;
    for (size_t i = 0; i < size; i++) {
        if (table->text[i] == '\n') lineCount++;
    }
    char **lines = calloc((size_t)lineCount + 1, sizeof(char *));
    if (!lines) {
        ssr_tsv_free(table);
        return NULL;
    }
    int used = 0;
    char *line = table->text;
    while (line && *line) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\r') line[--length] = 0;
        if (length > 0) lines[used++] = line;
        line = end ? end + 1 : NULL;
    }
    if (used == 0) {
        free(lines);
        ssr_tsv_free(table);
        return NULL;
    }

    table->columns = count_cells(lines[0]);
    table->rows = used - 1;
    table->cells = calloc((size_t)used * (size_t)table->columns, sizeof(char *));
    if (!table->cells) {
        free(lines);
        ssr_tsv_free(table);
        return NULL;
    }
    for (int row = 0; row < used; row++) {
        char *cell = lines[row];
        for (int column = 0; column < table->columns && cell; column++) {
            char *tab = strchr(cell, '\t');
            if (tab) *tab = 0;
            table->cells[row * table->columns + column] = cell;
            cell = tab ? tab + 1 : NULL;
        }
    }
    free(lines);
    return table;
}

SSRTsv *ssr_tsv_load(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    SSRTsv *table = NULL;
    if (fseek(file, 0, SEEK_END) == 0) {
        long length = ftell(file);
        char *text = length >= 0 ? malloc((size_t)length + 1) : NULL;
        if (text && fseek(file, 0, SEEK_SET) == 0 && fread(text, 1, (size_t)length, file) == (size_t)length) {
            table = ssr_tsv_parse(text, (size_t)length);
        }
        free(text);
    }
    fclose(file);
    return table;
}

void ssr_tsv_free(SSRTsv *table) {
    if (!table) return;
    free(table->cells);
    free(table->text);
    free(table);
}

int ssr_tsv_rows(const SSRTsv *table) {
    return table ? table->rows : 0;
}

static int same_name(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    }
    return *a == *b;
}

int ssr_tsv_column(const SSRTsv *table, const char *name) {
    if (!table || !name) return -1;
    for (int column = 0; column < table->columns; column++) {
        const char *header = table->cells[column];
        if (header && same_name(header, name)) return column;
    }
    return -1;
}

const char *ssr_tsv_cell(const SSRTsv *table, int row, int column) {
    if (!table || row < 0 || row >= table->rows || column < 0 || column >= table->columns) return "";
    const char *cell = table->cells[(row + 1) * table->columns + column];
    return cell ? cell : "";
}

int ssr_tsv_int(const SSRTsv *table, int row, int column, int fallback) {
    const char *cell = ssr_tsv_cell(table, row, column);
    char *end = NULL;
    long value = strtol(cell, &end, 10);
    if (end == cell || *end) return fallback;
    return (int)value;
}

double ssr_tsv_double(const SSRTsv *table, int row, int column, double fallback) {
    const char *cell = ssr_tsv_cell(table, row, column);
    char *end = NULL;
    double value = strtod(cell, &end);
    if (end == cell || *end) return fallback;
    return value;
}
