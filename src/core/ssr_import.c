#include "ssr_import.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define ssr_mkdir(path) _mkdir(path)
#else
#define ssr_mkdir(path) mkdir(path, 0755)
#endif

#include "ssr_zip.h"

#define SSR_MARKER "import.txt"
#define SSR_APK_PREFIX "assets/monkey/"
#define SSR_XAPK_GAME "com.newstargames.newstarsoccer.apk"
#define SSR_XAPK_TEMP ".import.apk"

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

// Refuses a name that can write outside dest, such as "../x" or "/x".
static int is_safe_name(const char *name) {
    if (!*name || name[0] == '/' || strchr(name, '\\') || strchr(name, ':')) return 0;
    const char *part = name;
    while (part) {
        if (strncmp(part, "..", 2) == 0 && (part[2] == '/' || part[2] == 0)) return 0;
        part = strchr(part, '/');
        if (part) part++;
    }
    return 1;
}

// Makes each directory in the path of the file. The last part is the file.
static int make_parents(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        int status = ssr_mkdir(path);
        *p = '/';
        if (status != 0 && errno != EEXIST) return 0;
    }
    return 1;
}

static int write_file(const char *dest, const char *name, const void *data, size_t size) {
    size_t length = strlen(dest) + strlen(name) + 2;
    char *path = malloc(length);
    if (!path) return 0;
    snprintf(path, length, "%s/%s", dest, name);
    int ok = make_parents(path);
    FILE *file = ok ? fopen(path, "wb") : NULL;
    if (file) {
        ok = fwrite(data, 1, size, file) == size;
        if (fclose(file) != 0) ok = 0;
    } else {
        ok = 0;
    }
    free(path);
    return ok;
}

static void remove_marker(const char *dest) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dest, SSR_MARKER);
    remove(path);
}

// Copies each entry under prefix into dest, without the prefix.
static int copy_tree(SSRZip *zip, const char *prefix, const char *dest) {
    size_t prefixLength = strlen(prefix);
    // Check all the names first, so that a bad archive writes nothing.
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        const char *name = ssr_zip_name(zip, i);
        if (starts_with(name, prefix) && name[prefixLength] && !is_safe_name(name + prefixLength)) return -1;
    }
    int copied = 0;
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        const char *name = ssr_zip_name(zip, i);
        if (!starts_with(name, prefix)) continue;
        const char *rest = name + prefixLength;
        size_t restLength = strlen(rest);
        if (restLength == 0 || rest[restLength - 1] == '/') continue;
        size_t size = 0;
        unsigned char *data = ssr_zip_read(zip, name, &size);
        if (!data) return -1;
        int ok = write_file(dest, rest, data, size);
        free(data);
        if (!ok) return -1;
        copied++;
    }
    return copied;
}

static int write_marker(const char *dest, SSRSource source, int copied) {
    char marker[64];
    int length = snprintf(marker, sizeof(marker), "source=%s\nfiles=%d\n", ssr_source_name(source), copied);
    return write_file(dest, SSR_MARKER, marker, (size_t)length);
}

static int has_prefix(SSRZip *zip, const char *prefix) {
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        if (starts_with(ssr_zip_name(zip, i), prefix)) return 1;
    }
    return 0;
}

// Finds the prefix "Payload/<name>.app/data/" of an IPA. Returns 1 and
// fills prefix when the archive has it.
static int find_ipa_prefix(SSRZip *zip, char *prefix, size_t capacity) {
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        const char *name = ssr_zip_name(zip, i);
        if (!starts_with(name, "Payload/")) continue;
        const char *app = name + strlen("Payload/");
        const char *slash = strchr(app, '/');
        if (!slash || slash - app < 5 || strncmp(slash - 4, ".app", 4) != 0) continue;
        if (!starts_with(slash, "/data/")) continue;
        size_t length = (size_t)(slash - name) + strlen("/data/");
        if (length + 1 > capacity) return 0;
        memcpy(prefix, name, length);
        prefix[length] = 0;
        return 1;
    }
    return 0;
}

SSRSource ssr_import(const char *archive, const char *dest, int *files) {
    if (files) *files = 0;
    SSRZip *zip = ssr_zip_open(archive);
    if (!zip) return SSR_SOURCE_UNKNOWN;

    SSRSource source = SSR_SOURCE_UNKNOWN;
    const char *prefix = NULL;
    char ipaPrefix[1024];
    if (has_prefix(zip, SSR_APK_PREFIX)) {
        source = SSR_SOURCE_APK;
        prefix = SSR_APK_PREFIX;
    } else if (find_ipa_prefix(zip, ipaPrefix, sizeof(ipaPrefix))) {
        source = SSR_SOURCE_IPA;
        prefix = ipaPrefix;
    }

    if (!prefix) {
        // An XAPK holds the APK of the game and the APKs of the languages.
        size_t size = 0;
        unsigned char *apk = ssr_zip_read(zip, SSR_XAPK_GAME, &size);
        ssr_zip_close(zip);
        if (!apk) return SSR_SOURCE_UNKNOWN;
        int ok = write_file(dest, SSR_XAPK_TEMP, apk, size);
        free(apk);
        if (!ok) return SSR_SOURCE_UNKNOWN;
        char inner[4096];
        snprintf(inner, sizeof(inner), "%s/%s", dest, SSR_XAPK_TEMP);
        int copied = 0;
        SSRSource innerSource = ssr_import(inner, dest, &copied);
        remove(inner);
        if (innerSource != SSR_SOURCE_APK) return SSR_SOURCE_UNKNOWN;
        if (!write_marker(dest, SSR_SOURCE_XAPK, copied)) return SSR_SOURCE_UNKNOWN;
        if (files) *files = copied;
        return SSR_SOURCE_XAPK;
    }

    int copied = -1;
    if (prefix) {
        remove_marker(dest);
        copied = copy_tree(zip, prefix, dest);
    }
    ssr_zip_close(zip);
    if (copied <= 0) return SSR_SOURCE_UNKNOWN;

    if (!write_marker(dest, source, copied)) return SSR_SOURCE_UNKNOWN;
    if (files) *files = copied;
    return source;
}

int ssr_import_is_done(const char *dest) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dest, SSR_MARKER);
    struct stat info;
    return stat(path, &info) == 0;
}

const char *ssr_source_name(SSRSource source) {
    switch (source) {
    case SSR_SOURCE_APK:
        return "apk";
    case SSR_SOURCE_XAPK:
        return "xapk";
    case SSR_SOURCE_IPA:
        return "ipa";
    default:
        return "unknown";
    }
}
