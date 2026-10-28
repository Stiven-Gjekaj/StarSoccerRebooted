// ssr_import: copies the assets of New Star Soccer from the copy of the
// player into the data directory of the app.

#ifndef SSR_IMPORT_H
#define SSR_IMPORT_H

typedef enum {
    SSR_SOURCE_UNKNOWN,
    SSR_SOURCE_APK,
    SSR_SOURCE_XAPK,
    SSR_SOURCE_IPA,
} SSRSource;

// Copies each asset in the archive into the directory dest. The tree in
// dest is the same as the tree under "assets/monkey/" in an APK, or under
// "Payload/<name>.app/data/" in an IPA. An XAPK, an APKM, or an APKS
// holds the APK of the game. dest
// gets the file "import.txt" last, when all the assets are there.
// Returns the kind of the archive, or SSR_SOURCE_UNKNOWN when the archive
// holds no assets or a copy fails. files gets the number of copied files.
SSRSource ssr_import(const char *archive, const char *dest, int *files);

// Returns 1 when dest holds a complete import.
int ssr_import_is_done(const char *dest);

// The name of the source, for example "apk".
const char *ssr_source_name(SSRSource source);

#endif
