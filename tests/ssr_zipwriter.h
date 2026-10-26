// ssr_zipwriter: writes small zip archives for the tests.
// A test builds the archive that it needs. It does not read a copy of the
// game.

#ifndef SSR_ZIPWRITER_H
#define SSR_ZIPWRITER_H

#include <stddef.h>

typedef struct {
    const char *name;
    const void *data;
    size_t size;
    int deflate; // 1 stores the data deflated, 0 stores it as it is.
} SSRZipFile;

// Writes the files to a new archive at path. Returns 1 on success.
int ssr_zipwriter_write(const char *path, const SSRZipFile *files, int count);

// Makes a new empty directory for a test and returns its path.
// The path stays valid until the next call.
const char *ssr_test_directory(const char *name);

#endif
