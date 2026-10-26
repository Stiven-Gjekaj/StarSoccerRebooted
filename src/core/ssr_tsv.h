// ssr_tsv: reads the tab-separated text files of the game, for example
// "MyData/v2025b/Clubs.txt". The first row holds the names of the columns.
// The reader accepts a UTF-8 byte order mark and CRLF or LF line ends.

#ifndef SSR_TSV_H
#define SSR_TSV_H

#include <stddef.h>

typedef struct SSRTsv SSRTsv;

// Reads the text. The table keeps its own copy. Returns NULL when there is
// no header row or no memory.
SSRTsv *ssr_tsv_parse(const char *text, size_t size);

// Reads the file at path. Returns NULL when the file is not there.
SSRTsv *ssr_tsv_load(const char *path);

void ssr_tsv_free(SSRTsv *table);

// The number of rows after the header row. An empty line is not a row.
int ssr_tsv_rows(const SSRTsv *table);

// The index of the column with the name, or -1. The match ignores case.
int ssr_tsv_column(const SSRTsv *table, const char *name);

// The text of a cell. Returns "" when the row or the column is not there.
const char *ssr_tsv_cell(const SSRTsv *table, int row, int column);

// The cell as a number. Returns fallback when the cell is empty or is not
// a number.
int ssr_tsv_int(const SSRTsv *table, int row, int column, int fallback);
double ssr_tsv_double(const SSRTsv *table, int row, int column, double fallback);

#endif
