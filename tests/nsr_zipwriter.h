// nsr_zipwriter: writes small zip archives for the tests.
// A test builds the archive that it needs. It does not read a copy of the
// game.

#ifndef NSR_ZIPWRITER_H
#define NSR_ZIPWRITER_H

#include <stddef.h>

typedef struct {
    const char *name;
    const void *data;
    size_t size;
    int deflate; // 1 stores the data deflated, 0 stores it as it is.
} NSRZipFile;

// Writes the files to a new archive at path. Returns 1 on success.
int nsr_zipwriter_write(const char *path, const NSRZipFile *files, int count);

// Makes a new empty directory for a test and returns its path.
// The path stays valid until the next call.
const char *nsr_test_directory(const char *name);

#endif
