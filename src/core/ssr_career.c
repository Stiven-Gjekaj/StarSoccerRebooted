#include "ssr_career.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

static void copy_name(char *out, size_t capacity, const char *text) {
    snprintf(out, capacity, "%s", text ? text : "");
}

// Removes the spaces at the two ends.
static void trim(char *text) {
    size_t start = 0, length = strlen(text);
    while (text[start] == ' ') start++;
    while (length > start && text[length - 1] == ' ') length--;
    memmove(text, text + start, length - start);
    text[length - start] = 0;
}

void ssr_career_create(SSRCareer *c, const char *first, const char *last, int gender, int hardMode,
                       int clubNationId, int wage, int goalBonus, int assistBonus) {
    memset(c, 0, sizeof(*c));
    c->sdate = 1;
    copy_name(c->firstName, sizeof(c->firstName), first);
    copy_name(c->lastName, sizeof(c->lastName), last);
    trim(c->firstName);
    trim(c->lastName);
    snprintf(c->fullName, sizeof(c->fullName), "%s %s", c->firstName, c->lastName);
    trim(c->fullName);
    copy_name(c->shortName, sizeof(c->shortName), c->lastName[0] ? c->lastName : c->firstName);
    c->gender = gender;
    c->hardMode = hardMode;
    c->clubNationId = clubNationId;
    c->wage = wage;
    c->goalBonus = goalBonus;
    c->assistBonus = assistBonus;
    // The values of the remote configuration of the game, when it is off.
    c->bank = 15;
    c->relationBoss = c->relationTeam = c->relationFans = 75;
    c->energy = 100;
    c->nrg[0] = c->nrg[1] = c->nrg[2] = 1;
    for (int i = 0; i < 10; i++) c->ratings[i] = 5;
    copy_name(c->gameVersion, sizeof(c->gameVersion), "v2.32");
}

int ssr_career_year(const SSRCareer *c) {
    return (c->sdate - 1) / 364 + 1;
}

int ssr_career_week(const SSRCareer *c) {
    return ((c->sdate - 1) % 364) / 7 + 1;
}

static void write_ints(FILE *file, const char *name, const int *values, int count) {
    fprintf(file, "%s=", name);
    for (int i = 0; i < count; i++) fprintf(file, i ? ",%d" : "%d", values[i]);
    fputc('\n', file);
}

static void write_floats(FILE *file, const char *name, const float *values, int count) {
    fprintf(file, "%s=", name);
    for (int i = 0; i < count; i++) fprintf(file, i ? ",%g" : "%g", values[i]);
    fputc('\n', file);
}

int ssr_career_save(const SSRCareer *c, const char *path) {
    if (!c || !path) return 0;
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.new", path);
    FILE *file = fopen(temp, "wb");
    if (!file) return 0;
    fprintf(file, "gameversion=%s\n", c->gameVersion);
    fprintf(file, "sdate=%d\n", c->sdate);
    fprintf(file, "firstname=%s\nlastname=%s\n", c->firstName, c->lastName);
    fprintf(file, "fullname=%s\nshortname=%s\n", c->fullName, c->shortName);
    fprintf(file, "gender=%d\nhardmode=%d\n", c->gender, c->hardMode);
    fprintf(file, "nationid=%d\nclubnationid=%d\nclubid=%d\n", c->nationId, c->clubNationId, c->clubId);
    fprintf(file, "bank=%d\nwage=%d\ngoalbonus=%d\nassistbonus=%d\n", c->bank, c->wage, c->goalBonus, c->assistBonus);
    fprintf(file, "energy=%g\n", c->energy);
    fprintf(file, "relationboss=%g\nrelationteam=%g\nrelationfans=%g\n", c->relationBoss, c->relationTeam,
            c->relationFans);
    write_ints(file, "technique", c->technique, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "pace", c->pace, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "vision", c->vision, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "setpieces", c->setPieces, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "power", c->power, SSR_CAREER_SKILL_LEVELS);
    write_ints(file, "nrg", c->nrg, 10);
    write_floats(file, "ratings", c->ratings, 10);
    fprintf(file, "apps=%d\ngoals=%d\nassists=%d\npasses=%d\n", c->apps, c->goals, c->assists, c->passes);
    fprintf(file, "retired=%d\n", c->retired);
    // A line ends the headline, so the save keeps its new lines as "\n".
    fputs("headline=", file);
    for (const char *p = c->headline; *p; p++) {
        if (*p == '\n') {
            fputs("\\n", file);
        } else if (*p == '\\') {
            fputs("\\\\", file);
        } else {
            fputc(*p, file);
        }
    }
    fputc('\n', file);
    int ok = !ferror(file);
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temp, path) != 0) {
        remove(temp);
        return 0;
    }
    return 1;
}

static void read_ints(const char *text, int *values, int count) {
    for (int i = 0; i < count && *text; i++) {
        char *end;
        values[i] = (int)strtol(text, &end, 10);
        text = *end == ',' ? end + 1 : end;
    }
}

static void read_floats(const char *text, float *values, int count) {
    for (int i = 0; i < count && *text; i++) {
        char *end;
        values[i] = strtof(text, &end);
        text = *end == ',' ? end + 1 : end;
    }
}

int ssr_career_load(SSRCareer *c, const char *path) {
    if (!c || !path) return 0;
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    memset(c, 0, sizeof(*c));
    static char line[4096];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = 0;
        const char *key = line, *value = equals + 1;
        int number = atoi(value);
        float real = strtof(value, NULL);
        if (strcmp(key, "gameversion") == 0) copy_name(c->gameVersion, sizeof(c->gameVersion), value);
        else if (strcmp(key, "sdate") == 0) c->sdate = number;
        else if (strcmp(key, "firstname") == 0) copy_name(c->firstName, sizeof(c->firstName), value);
        else if (strcmp(key, "lastname") == 0) copy_name(c->lastName, sizeof(c->lastName), value);
        else if (strcmp(key, "fullname") == 0) copy_name(c->fullName, sizeof(c->fullName), value);
        else if (strcmp(key, "shortname") == 0) copy_name(c->shortName, sizeof(c->shortName), value);
        else if (strcmp(key, "gender") == 0) c->gender = number;
        else if (strcmp(key, "hardmode") == 0) c->hardMode = number;
        else if (strcmp(key, "nationid") == 0) c->nationId = number;
        else if (strcmp(key, "clubnationid") == 0) c->clubNationId = number;
        else if (strcmp(key, "clubid") == 0) c->clubId = number;
        else if (strcmp(key, "bank") == 0) c->bank = number;
        else if (strcmp(key, "wage") == 0) c->wage = number;
        else if (strcmp(key, "goalbonus") == 0) c->goalBonus = number;
        else if (strcmp(key, "assistbonus") == 0) c->assistBonus = number;
        else if (strcmp(key, "energy") == 0) c->energy = real;
        else if (strcmp(key, "relationboss") == 0) c->relationBoss = real;
        else if (strcmp(key, "relationteam") == 0) c->relationTeam = real;
        else if (strcmp(key, "relationfans") == 0) c->relationFans = real;
        else if (strcmp(key, "technique") == 0) read_ints(value, c->technique, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "pace") == 0) read_ints(value, c->pace, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "vision") == 0) read_ints(value, c->vision, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "setpieces") == 0) read_ints(value, c->setPieces, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "power") == 0) read_ints(value, c->power, SSR_CAREER_SKILL_LEVELS);
        else if (strcmp(key, "nrg") == 0) read_ints(value, c->nrg, 10);
        else if (strcmp(key, "ratings") == 0) read_floats(value, c->ratings, 10);
        else if (strcmp(key, "apps") == 0) c->apps = number;
        else if (strcmp(key, "goals") == 0) c->goals = number;
        else if (strcmp(key, "assists") == 0) c->assists = number;
        else if (strcmp(key, "passes") == 0) c->passes = number;
        else if (strcmp(key, "retired") == 0) c->retired = number;
        else if (strcmp(key, "headline") == 0) {
            size_t used = 0;
            for (const char *p = value; *p && used + 1 < sizeof(c->headline); p++) {
                if (p[0] == '\\' && p[1] == 'n') {
                    c->headline[used++] = '\n';
                    p++;
                } else if (p[0] == '\\' && p[1] == '\\') {
                    c->headline[used++] = '\\';
                    p++;
                } else {
                    c->headline[used++] = *p;
                }
            }
            c->headline[used] = 0;
        }
    }
    fclose(file);
    return c->clubId != 0;
}

int ssr_career_skill(const int *levels) {
    int sum = 0;
    for (int i = 0; i < SSR_CAREER_SKILL_LEVELS; i++) sum += levels[i];
    return sum < 1 ? 1 : sum > 100 ? 100 : sum;
}
