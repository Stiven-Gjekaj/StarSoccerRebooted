// ssr_sound: the sounds of the game. Sounds/Sounds.txt names each sound,
// for example "Snd:(UISelect,Sounds/NSC_Select)". A name can have more than
// one file, and the game picks one of them. The file is an OGG in an APK
// and an MP3 in an IPA.

#ifndef SSR_SOUND_H
#define SSR_SOUND_H

#include <stdint.h>

typedef struct SSRSounds SSRSounds;

// Reads the list of sounds in the directory of the import. Returns NULL
// when there is no Sounds/Sounds.txt.
SSRSounds *ssr_sounds_load(const char *gameDirectory);
void ssr_sounds_free(SSRSounds *sounds);

// The number of files of the name, and the path of one of them, relative
// to the directory of the import, for example "Sounds/NSC_Select.ogg".
int ssr_sounds_count(const SSRSounds *sounds, const char *name);
const char *ssr_sounds_file(const SSRSounds *sounds, const char *name, int index);

typedef struct {
    int channels;
    int rate;
    // channels samples for each frame, one after the other.
    int16_t *samples;
    int frames;
} SSRPcm;

// Decodes an OGG or an MP3 file. Returns 1 on success.
int ssr_sound_decode(const char *path, SSRPcm *out);
void ssr_pcm_free(SSRPcm *pcm);

#endif
