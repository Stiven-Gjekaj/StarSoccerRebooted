#include "ssr_sound.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif
#include "stb_vorbis.c"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

typedef struct {
    char *name;
    char *file;
} Entry;

struct SSRSounds {
    Entry *entries;
    int count;
};

static int exists(const char *path) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static void add(SSRSounds *sounds, const char *name, size_t nameLength, const char *file) {
    Entry *grown = realloc(sounds->entries, (size_t)(sounds->count + 1) * sizeof(Entry));
    if (!grown) return;
    sounds->entries = grown;
    Entry *entry = &grown[sounds->count];
    entry->name = malloc(nameLength + 1);
    entry->file = malloc(strlen(file) + 1);
    if (!entry->name || !entry->file) {
        free(entry->name);
        free(entry->file);
        return;
    }
    memcpy(entry->name, name, nameLength);
    entry->name[nameLength] = 0;
    strcpy(entry->file, file);
    sounds->count++;
}

SSRSounds *ssr_sounds_load(const char *gameDirectory) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/Sounds/Sounds.txt", gameDirectory);
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return NULL;
    SSRSounds *sounds = calloc(1, sizeof(SSRSounds));
    char line[1024];
    while (sounds && fgets(line, sizeof(line), file)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (strncmp(p, "Snd:(", 5) != 0) continue;
        p += 5;
        char *comma = strchr(p, ',');
        char *end = comma ? strchr(comma, ')') : NULL;
        if (!comma || !end) continue;
        *end = 0;
        char *stem = comma + 1;
        while (isspace((unsigned char)*stem)) stem++;
        // The list gives no extension. The import has an OGG or an MP3.
        static const char *extensions[] = {".ogg", ".mp3", ".wav"};
        for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
            char relative[1024], full[5200];
            snprintf(relative, sizeof(relative), "%s%s", stem, extensions[i]);
            snprintf(full, sizeof(full), "%s/%s", gameDirectory, relative);
            if (exists(full)) {
                add(sounds, p, (size_t)(comma - p), relative);
                break;
            }
        }
    }
    fclose(file);
    return sounds;
}

void ssr_sounds_free(SSRSounds *sounds) {
    if (!sounds) return;
    for (int i = 0; i < sounds->count; i++) {
        free(sounds->entries[i].name);
        free(sounds->entries[i].file);
    }
    free(sounds->entries);
    free(sounds);
}

int ssr_sounds_count(const SSRSounds *sounds, const char *name) {
    if (!sounds || !name) return 0;
    int count = 0;
    for (int i = 0; i < sounds->count; i++) count += strcmp(sounds->entries[i].name, name) == 0;
    return count;
}

const char *ssr_sounds_file(const SSRSounds *sounds, const char *name, int index) {
    if (!sounds || !name) return NULL;
    for (int i = 0; i < sounds->count; i++) {
        if (strcmp(sounds->entries[i].name, name) == 0 && index-- == 0) return sounds->entries[i].file;
    }
    return NULL;
}

static int ends_with(const char *text, const char *end) {
    size_t a = strlen(text), b = strlen(end);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++) {
        if (tolower((unsigned char)text[a - b + i]) != end[i]) return 0;
    }
    return 1;
}

int ssr_sound_decode(const char *asked, SSRPcm *out) {
    memset(out, 0, sizeof(*out));
    char path[4096];
    if (!ssr_fs_resolve(asked, path, sizeof(path))) return 0;
    if (ends_with(path, ".ogg")) {
        int channels = 0, rate = 0;
        short *samples = NULL;
        int frames = stb_vorbis_decode_filename(path, &channels, &rate, &samples);
        if (frames <= 0 || !samples) {
            free(samples);
            return 0;
        }
        out->channels = channels;
        out->rate = rate;
        out->samples = samples;
        out->frames = frames;
        return 1;
    }
    if (ends_with(path, ".mp3")) {
        drmp3_config config;
        drmp3_uint64 frames = 0;
        drmp3_int16 *samples = drmp3_open_file_and_read_pcm_frames_s16(path, &config, &frames, NULL);
        if (!samples || frames == 0) {
            drmp3_free(samples, NULL);
            return 0;
        }
        // The caller frees the samples with free().
        size_t bytes = (size_t)frames * config.channels * sizeof(int16_t);
        out->samples = malloc(bytes);
        if (!out->samples) {
            drmp3_free(samples, NULL);
            return 0;
        }
        memcpy(out->samples, samples, bytes);
        drmp3_free(samples, NULL);
        out->channels = (int)config.channels;
        out->rate = (int)config.sampleRate;
        out->frames = (int)frames;
        return 1;
    }
    return 0;
}

void ssr_pcm_free(SSRPcm *pcm) {
    if (!pcm) return;
    free(pcm->samples);
    memset(pcm, 0, sizeof(*pcm));
}
