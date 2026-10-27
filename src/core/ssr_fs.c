#include "ssr_fs.h"

#include <string.h>

#ifdef _WIN32

// Windows ignores the case of names.
int ssr_fs_resolve(const char *path, char *out, size_t capacity) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    if (strlen(path) + 1 > capacity) return 0;
    strcpy(out, path);
    return 1;
}

#else

#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

// Finds each part of the path in its directory, in any case. A system can
// refuse to list a directory above the app, as Android does for /data.
// Then the part counts as it is, when it is there.
int ssr_fs_resolve(const char *path, char *out, size_t capacity) {
    if (!path || !out || capacity == 0) return 0;
    size_t length = strlen(path);
    if (length + 1 > capacity) return 0;
    out[0] = 0;
    size_t used = 0;
    const char *p = path;
    if (*p == '/') {
        out[used++] = '/';
        out[used] = 0;
        while (*p == '/') p++;
    }
    while (*p) {
        const char *end = strchr(p, '/');
        size_t partLength = end ? (size_t)(end - p) : strlen(p);
        char part[256];
        if (partLength == 0 || partLength >= sizeof(part)) return 0;
        memcpy(part, p, partLength);
        part[partLength] = 0;

        DIR *dir = opendir(used ? out : ".");
        const char *found = NULL;
        if (!dir) {
            char asIs[4096];
            struct stat info;
            snprintf(asIs, sizeof(asIs), "%s%s%s", out, used && out[used - 1] != '/' ? "/" : "", part);
            if (stat(asIs, &info) != 0 || strlen(asIs) + 1 > capacity) return 0;
            strcpy(out, asIs);
            used = strlen(out);
            p += partLength;
            while (*p == '/') p++;
            continue;
        }
        struct dirent *entry;
        char name[256];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, part) == 0) {
                found = part;
                break;
            }
            if (!found && strcasecmp(entry->d_name, part) == 0) {
                strncpy(name, entry->d_name, sizeof(name) - 1);
                name[sizeof(name) - 1] = 0;
                found = name;
            }
        }
        if (found) {
            size_t foundLength = strlen(found);
            int slash = used > 0 && out[used - 1] != '/';
            if (used + slash + foundLength + 1 > capacity) found = NULL;
            if (found) {
                if (slash) out[used++] = '/';
                memcpy(out + used, found, foundLength + 1);
                used += foundLength;
            }
        }
        closedir(dir);
        if (!found) return 0;
        p += partLength;
        while (*p == '/') p++;
    }
    return 1;
}

#endif

FILE *ssr_fs_open(const char *path, const char *mode) {
    FILE *file = fopen(path, mode);
    if (file || !path) return file;
    char real[4096];
    return ssr_fs_resolve(path, real, sizeof(real)) ? fopen(real, mode) : NULL;
}
