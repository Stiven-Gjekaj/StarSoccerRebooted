#include "ssr_pack.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ssr_sha256.h"
#include "ssr_tsv.h"
#include "ssr_zip.h"

#ifdef _WIN32
#include <direct.h>
#define ssr_mkdir(path) _mkdir(path)
#define rmdir(path) _rmdir(path)
#else
#include <unistd.h>
#define ssr_mkdir(path) mkdir(path, 0755)
#endif

#define PACK_FILE "pack.txt"
// A pack file larger than this is not data of the game.
#define MAX_FILE_SIZE (16u * 1024u * 1024u)

static void copy_text(char *out, size_t capacity, const char *text, size_t length) {
    if (length >= capacity) length = capacity - 1;
    memcpy(out, text, length);
    out[length] = 0;
}

// An id has 1 to 47 small letters, digits, "-", and "_".
static int is_good_id(const char *id) {
    if (!*id) return 0;
    for (const char *p = id; *p; p++) {
        if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '-' || *p == '_')) return 0;
    }
    return 1;
}

int ssr_pack_parse(const char *text, SSRPackInfo *info) {
    if (!text || !info) return 0;
    memset(info, 0, sizeof(*info));
    const char *line = text;
    while (*line) {
        const char *end = line + strcspn(line, "\r\n");
        const char *equals = memchr(line, '=', (size_t)(end - line));
        if (equals) {
            size_t keyLength = (size_t)(equals - line);
            const char *value = equals + 1;
            size_t valueLength = (size_t)(end - value);
            if (keyLength == 2 && strncmp(line, "id", 2) == 0) {
                if (valueLength >= sizeof(info->id)) return 0;
                copy_text(info->id, sizeof(info->id), value, valueLength);
            } else if (keyLength == 4 && strncmp(line, "name", 4) == 0) {
                copy_text(info->name, sizeof(info->name), value, valueLength);
            } else if (keyLength == 6 && strncmp(line, "author", 6) == 0) {
                copy_text(info->author, sizeof(info->author), value, valueLength);
            } else if (keyLength == 7 && strncmp(line, "version", 7) == 0) {
                copy_text(info->version, sizeof(info->version), value, valueLength);
            } else if (keyLength == 6 && strncmp(line, "format", 6) == 0) {
                info->format = atoi(value);
            }
        }
        line = end;
        while (*line == '\r' || *line == '\n') line++;
    }
    return is_good_id(info->id) && info->name[0] && info->format >= 1 && info->format <= SSR_PACK_FORMAT;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    char *text = malloc(65537);
    size_t size = text ? fread(text, 1, 65536, file) : 0;
    fclose(file);
    if (text) text[size] = 0;
    return text;
}

int ssr_pack_info(const char *directory, SSRPackInfo *info) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", directory, PACK_FILE);
    char *text = read_file(path);
    int ok = ssr_pack_parse(text, info);
    free(text);
    return ok;
}

// Refuses a name that can write outside the pack, such as "../x" or "/x".
static int is_safe_name(const char *name) {
    if (!*name || name[0] == '/' || strchr(name, '\\') || strchr(name, ':')) return 0;
    const char *part = name;
    while (part) {
        if (part[0] == '.') return 0;
        part = strchr(part, '/');
        if (part) part++;
    }
    return 1;
}

// A pack holds text, images, layouts, and layout patches. It holds no
// code.
static int is_data_name(const char *name) {
    static const char *const KINDS[] = {".txt", ".png", ".jpg", ".uis", ".uih", ".patch"};
    const char *dot = strrchr(name, '.');
    if (!dot || strchr(dot, '/')) return 0;
    for (size_t i = 0; i < sizeof(KINDS) / sizeof(KINDS[0]); i++) {
        if (strcmp(dot, KINDS[i]) == 0) return 1;
    }
    return 0;
}

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

static int write_file(const char *path, const void *data, size_t size) {
    char made[4096];
    snprintf(made, sizeof(made), "%s", path);
    if (!make_parents(made)) return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

// Removes the directory and all that it holds.
static int remove_tree(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return remove(path) == 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) remove_tree(child);
        else remove(child);
    }
    closedir(dir);
    return rmdir(path) == 0;
}

// Finds pack.txt at the top of the archive or in one folder. prefix gets
// the folder with its "/", or "".
static int find_prefix(SSRZip *zip, char *prefix, size_t capacity) {
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        const char *name = ssr_zip_name(zip, i);
        if (strcmp(name, PACK_FILE) == 0) {
            prefix[0] = 0;
            return 1;
        }
    }
    for (int i = 0; i < ssr_zip_count(zip); i++) {
        const char *name = ssr_zip_name(zip, i);
        const char *slash = strchr(name, '/');
        if (!slash || strcmp(slash + 1, PACK_FILE) != 0) continue;
        size_t length = (size_t)(slash - name) + 1;
        if (length + 1 > capacity) return 0;
        memcpy(prefix, name, length);
        prefix[length] = 0;
        return 1;
    }
    return 0;
}

int ssr_pack_install(const char *archive, const char *packs, SSRPackInfo *info) {
    SSRPackInfo local;
    if (!info) info = &local;
    memset(info, 0, sizeof(*info));
    SSRZip *zip = ssr_zip_open(archive);
    if (!zip) return 0;
    char prefix[512], name[600];
    int ok = find_prefix(zip, prefix, sizeof(prefix));
    if (ok) {
        snprintf(name, sizeof(name), "%s%s", prefix, PACK_FILE);
        size_t size = 0;
        unsigned char *data = ssr_zip_read(zip, name, &size);
        char *text = data ? realloc(data, size + 1) : NULL;
        if (text) text[size] = 0;
        else free(data);
        ok = text && ssr_pack_parse(text, info);
        free(text);
    }
    // Check all the names first, so that a bad archive writes nothing.
    size_t prefixLength = strlen(prefix);
    for (int i = 0; ok && i < ssr_zip_count(zip); i++) {
        const char *entry = ssr_zip_name(zip, i);
        size_t length = strlen(entry);
        if (length && entry[length - 1] == '/') continue;
        if (strncmp(entry, prefix, prefixLength) != 0) continue;
        const char *rest = entry + prefixLength;
        if (!is_safe_name(rest) || !is_data_name(rest)) ok = 0;
    }
    char dest[4096];
    if (ok) {
        ssr_mkdir(packs);
        snprintf(dest, sizeof(dest), "%s/%s", packs, info->id);
        remove_tree(dest);
    }
    for (int i = 0; ok && i < ssr_zip_count(zip); i++) {
        const char *entry = ssr_zip_name(zip, i);
        size_t length = strlen(entry);
        if (length && entry[length - 1] == '/') continue;
        if (strncmp(entry, prefix, prefixLength) != 0) continue;
        size_t size = 0;
        unsigned char *data = ssr_zip_read(zip, entry, &size);
        char path[4700];
        snprintf(path, sizeof(path), "%s/%s", dest, entry + prefixLength);
        ok = data && size <= MAX_FILE_SIZE && write_file(path, data, size);
        free(data);
    }
    ssr_zip_close(zip);
    return ok;
}

int ssr_pack_make(const char *packs, const char *id, const char *name) {
    if (!packs || !id || !is_good_id(id)) return 0;
    char path[4096];
    snprintf(path, sizeof(path), "%s", packs);
    ssr_mkdir(path);
    snprintf(path, sizeof(path), "%s/%s", packs, id);
    ssr_mkdir(path);
    snprintf(path, sizeof(path), "%s/%s/world", packs, id);
    ssr_mkdir(path);
    SSRPackInfo info;
    snprintf(path, sizeof(path), "%s/%s", packs, id);
    if (ssr_pack_info(path, &info)) return 1;
    snprintf(path, sizeof(path), "%s/%s/%s", packs, id, PACK_FILE);
    char text[256];
    int length = snprintf(text, sizeof(text), "id=%s\nname=%s\nauthor=You\nversion=1\nformat=1\n", id, name ? name : id);
    return write_file(path, text, (size_t)length);
}

int ssr_pack_remove(const char *packs, const char *id) {
    if (!id || !is_good_id(id)) return 0;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", packs, id);
    return remove_tree(path);
}

static int compare_ids(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int ssr_pack_index_parse(const char *text, SSRPackEntry *entries, int max) {
    SSRTsv *table = text ? ssr_tsv_parse(text, strlen(text)) : NULL;
    if (!table) return 0;
    int columns[8];
    static const char *const NAMES[8] = {"id", "name", "author", "version", "format", "size", "sha256", "url"};
    for (int k = 0; k < 8; k++) columns[k] = ssr_tsv_column(table, NAMES[k]);
    int n = 0;
    for (int row = 0; row < ssr_tsv_rows(table) && n < max; row++) {
        SSRPackEntry e;
        memset(&e, 0, sizeof(e));
        snprintf(e.id, sizeof(e.id), "%s", ssr_tsv_cell(table, row, columns[0]));
        snprintf(e.name, sizeof(e.name), "%s", ssr_tsv_cell(table, row, columns[1]));
        snprintf(e.author, sizeof(e.author), "%s", ssr_tsv_cell(table, row, columns[2]));
        snprintf(e.version, sizeof(e.version), "%s", ssr_tsv_cell(table, row, columns[3]));
        e.format = ssr_tsv_int(table, row, columns[4], 0);
        e.size = ssr_tsv_int(table, row, columns[5], 0);
        snprintf(e.sha256, sizeof(e.sha256), "%s", ssr_tsv_cell(table, row, columns[6]));
        snprintf(e.url, sizeof(e.url), "%s", ssr_tsv_cell(table, row, columns[7]));
        if (!is_good_id(e.id) || e.format < 1 || e.format > SSR_PACK_FORMAT || strncmp(e.url, "https://", 8) != 0 ||
            strlen(e.sha256) != 64 || e.size <= 0) {
            continue;
        }
        entries[n++] = e;
    }
    ssr_tsv_free(table);
    return n;
}

int ssr_pack_check_download(const SSRPackEntry *entry, const void *data, size_t size) {
    if (!entry || !data || (long)size != entry->size) return 0;
    char hex[65];
    ssr_sha256_hex(data, size, hex);
    return strcmp(hex, entry->sha256) == 0;
}

int ssr_pack_list(const char *packs, char ids[][48], int max) {
    DIR *dir = opendir(packs);
    if (!dir) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max) {
        if (!is_good_id(entry->d_name) || strlen(entry->d_name) >= 48) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", packs, entry->d_name);
        SSRPackInfo info;
        if (!ssr_pack_info(path, &info) || strcmp(info.id, entry->d_name) != 0) continue;
        snprintf(ids[count++], 48, "%s", entry->d_name);
    }
    closedir(dir);
    qsort(ids, (size_t)count, sizeof(ids[0]), compare_ids);
    return count;
}
