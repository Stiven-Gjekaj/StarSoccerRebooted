// nsr_zip: reads the files in a zip archive, such as an APK.
// It reads stored and deflated entries. It does not write archives.

#ifndef NSR_ZIP_H
#define NSR_ZIP_H

#include <stddef.h>

typedef struct NSRZip NSRZip;

// Opens the archive at path and reads its central directory.
// Returns NULL when the file is not a zip archive.
NSRZip *nsr_zip_open(const char *path);
void nsr_zip_close(NSRZip *zip);

// The number of entries, and the name of the entry at index.
int nsr_zip_count(const NSRZip *zip);
const char *nsr_zip_name(const NSRZip *zip, int index);

// Reads the entry with the full name, for example
// "assets/monkey/Images/Backgrounds/Default.jpg". Returns a buffer that the caller
// frees with free(), or NULL when the entry is not there or is damaged.
// The buffer has one extra zero byte after the data, so that text can be
// read as a C string. size gets the size of the data without that byte.
unsigned char *nsr_zip_read(NSRZip *zip, const char *name, size_t *size);

#endif
