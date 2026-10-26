#include "ssr_audio.h"

#include <SDL3/SDL.h>

#include "ssr_sound.h"

#define VOICES 8
#define MAX_CACHED 128

typedef struct {
    char file[256];
    SSRPcm pcm;
} Cached;

struct SSRAudio {
    SDL_AudioDeviceID device;
    SDL_AudioStream *voices[VOICES];
    int nextVoice;
    SSRSounds *sounds;
    char gameDirectory[2048];
    Cached cache[MAX_CACHED];
    int cacheCount;
};

SSRAudio *ssr_audio_create(const char *gameDirectory) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return NULL;
    SSRAudio *audio = SDL_calloc(1, sizeof(SSRAudio));
    if (!audio) return NULL;
    audio->device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!audio->device) {
        SDL_Log("No audio device: %s", SDL_GetError());
        SDL_free(audio);
        return NULL;
    }
    SDL_strlcpy(audio->gameDirectory, gameDirectory, sizeof(audio->gameDirectory));
    audio->sounds = ssr_sounds_load(gameDirectory);
    return audio;
}

void ssr_audio_free(SSRAudio *audio) {
    if (!audio) return;
    for (int i = 0; i < VOICES; i++) SDL_DestroyAudioStream(audio->voices[i]);
    SDL_CloseAudioDevice(audio->device);
    for (int i = 0; i < audio->cacheCount; i++) ssr_pcm_free(&audio->cache[i].pcm);
    ssr_sounds_free(audio->sounds);
    SDL_free(audio);
}

static const SSRPcm *pcm_for(SSRAudio *audio, const char *file) {
    for (int i = 0; i < audio->cacheCount; i++) {
        if (SDL_strcmp(audio->cache[i].file, file) == 0) return &audio->cache[i].pcm;
    }
    if (audio->cacheCount >= MAX_CACHED) return NULL;
    char path[2400];
    SDL_snprintf(path, sizeof(path), "%s/%s", audio->gameDirectory, file);
    Cached *slot = &audio->cache[audio->cacheCount];
    if (!ssr_sound_decode(path, &slot->pcm)) return NULL;
    SDL_strlcpy(slot->file, file, sizeof(slot->file));
    audio->cacheCount++;
    return &slot->pcm;
}

void ssr_audio_play(SSRAudio *audio, const char *name) {
    if (!audio || !name) return;
    int count = ssr_sounds_count(audio->sounds, name);
    if (count == 0) return;
    const char *file = ssr_sounds_file(audio->sounds, name, (int)SDL_rand(count));
    const SSRPcm *pcm = file ? pcm_for(audio, file) : NULL;
    if (!pcm) return;

    // The voices take turns. A new sound stops the oldest one.
    int index = audio->nextVoice;
    audio->nextVoice = (audio->nextVoice + 1) % VOICES;
    SDL_AudioSpec spec = {SDL_AUDIO_S16, pcm->channels, pcm->rate};
    if (audio->voices[index]) {
        SDL_ClearAudioStream(audio->voices[index]);
        SDL_SetAudioStreamFormat(audio->voices[index], &spec, NULL);
    } else {
        audio->voices[index] = SDL_CreateAudioStream(&spec, NULL);
        if (!audio->voices[index] || !SDL_BindAudioStream(audio->device, audio->voices[index])) return;
    }
    SDL_PutAudioStreamData(audio->voices[index], pcm->samples, pcm->frames * pcm->channels * (int)sizeof(int16_t));
}
