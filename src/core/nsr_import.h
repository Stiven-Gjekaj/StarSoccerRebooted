// nsr_import: copies the assets of New Star Soccer from the copy of the
// player into the data directory of the app.

#ifndef NSR_IMPORT_H
#define NSR_IMPORT_H

typedef enum {
    NSR_SOURCE_UNKNOWN,
    NSR_SOURCE_APK,
} NSRSource;

// Copies each asset in the archive into the directory dest. The tree in
// dest is the same as the tree under "assets/monkey/" in the APK. dest
// gets the file "import.txt" last, when all the assets are there.
// Returns the kind of the archive, or NSR_SOURCE_UNKNOWN when the archive
// holds no assets or a copy fails. files gets the number of copied files.
NSRSource nsr_import(const char *archive, const char *dest, int *files);

// Returns 1 when dest holds a complete import.
int nsr_import_is_done(const char *dest);

// The name of the source, for example "apk".
const char *nsr_source_name(NSRSource source);

#endif
