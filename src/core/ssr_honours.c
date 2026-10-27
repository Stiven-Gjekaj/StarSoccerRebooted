#include "ssr_honours.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ssr_honours_add(SSRHonours *h, const SSRHonour *honour) {
    if (!h || !honour) return 0;
    for (int i = 0; i < h->count; i++) {
        if (h->list[i].year == honour->year && strcmp(h->list[i].league, honour->league) == 0) {
            h->list[i] = *honour;
            return 1;
        }
    }
    if (h->count == h->capacity) {
        int capacity = h->capacity ? h->capacity * 2 : 32;
        SSRHonour *grown = realloc(h->list, sizeof(SSRHonour) * (size_t)capacity);
        if (!grown) return 0;
        h->list = grown;
        h->capacity = capacity;
    }
    h->list[h->count++] = *honour;
    return 1;
}

void ssr_honours_clear(SSRHonours *h) {
    if (!h) return;
    free(h->rivalries);
    free(h->list);
    memset(h, 0, sizeof(*h));
}

static SSRRivalry *find_rivalry(const SSRHonours *h, int a, int b) {
    for (int i = 0; h && i < h->rivalryCount; i++) {
        SSRRivalry *r = &h->rivalries[i];
        if ((r->a == a && r->b == b) || (r->a == b && r->b == a)) return r;
    }
    return NULL;
}

int ssr_honours_add_rivalry(SSRHonours *h, int a, int b, int points) {
    if (!h || a == b || a <= 0 || b <= 0) return 0;
    SSRRivalry *r = find_rivalry(h, a, b);
    if (!r) {
        if (h->rivalryCount == h->rivalryCapacity) {
            int capacity = h->rivalryCapacity ? h->rivalryCapacity * 2 : 32;
            SSRRivalry *grown = realloc(h->rivalries, sizeof(SSRRivalry) * (size_t)capacity);
            if (!grown) return 0;
            h->rivalries = grown;
            h->rivalryCapacity = capacity;
        }
        r = &h->rivalries[h->rivalryCount++];
        r->a = a < b ? a : b;
        r->b = a < b ? b : a;
        r->points = 0;
    }
    r->points += points;
    return r->points;
}

int ssr_honours_rivalry(const SSRHonours *h, int a, int b) {
    const SSRRivalry *r = find_rivalry(h, a, b);
    return r ? r->points : 0;
}

int ssr_honours_titles(const SSRHonours *h, int club) {
    int n = 0;
    for (int i = 0; h && i < h->count; i++) n += h->list[i].club == club;
    return n;
}

// A name in the file has no tab and no line end.
static void clean(char *text) {
    for (char *p = text; *p; p++) {
        if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
    }
}

int ssr_honours_save(const SSRHonours *h, const char *path) {
    if (!h || !path) return 0;
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return 0;
    int ok = fprintf(file, "ssrhonours\t1\n") > 0;
    for (int i = 0; ok && i < h->count; i++) {
        SSRHonour e = h->list[i];
        clean(e.league);
        clean(e.clubName);
        clean(e.scorer);
        clean(e.scorerClub);
        ok = fprintf(file, "%d\t%s\t%d\t%s\t%s\t%s\t%d\t%d\n", e.year, e.league, e.club, e.clubName,
                     e.scorer[0] ? e.scorer : "-", e.scorerClub[0] ? e.scorerClub : "-", e.goals, e.lastDay) > 0;
    }
    for (int i = 0; ok && i < h->rivalryCount; i++) {
        ok = fprintf(file, "R\t%d\t%d\t%d\n", h->rivalries[i].a, h->rivalries[i].b, h->rivalries[i].points) > 0;
    }
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

int ssr_honours_load(SSRHonours *h, const char *path) {
    ssr_honours_clear(h);
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file) return 0;
    char line[512];
    int version = 0;
    if (!fgets(line, sizeof(line), file) || sscanf(line, "ssrhonours\t%d", &version) != 1 || version != 1) {
        fclose(file);
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        int a, b, points;
        if (line[0] == 'R' && sscanf(line, "R\t%d\t%d\t%d", &a, &b, &points) == 3) {
            ssr_honours_add_rivalry(h, a, b, points);
            continue;
        }
        SSRHonour e;
        memset(&e, 0, sizeof(e));
        // A file from before the last day has 7 columns.
        if (sscanf(line, "%d\t%15[^\t]\t%d\t%63[^\t]\t%63[^\t]\t%63[^\t]\t%d\t%d", &e.year, e.league, &e.club, e.clubName,
                   e.scorer, e.scorerClub, &e.goals, &e.lastDay) < 7) {
            continue;
        }
        if (strcmp(e.scorer, "-") == 0) e.scorer[0] = 0;
        if (strcmp(e.scorerClub, "-") == 0) e.scorerClub[0] = 0;
        ssr_honours_add(h, &e);
    }
    fclose(file);
    return 1;
}
