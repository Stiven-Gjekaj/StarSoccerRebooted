// ssr_audio: plays the sounds of the import with SDL.

#ifndef SSR_AUDIO_H
#define SSR_AUDIO_H

typedef struct SSRAudio SSRAudio;

// Opens the audio device and reads the sound list of the import. Returns
// NULL when there is no audio device. A missing sound list plays nothing.
SSRAudio *ssr_audio_create(const char *gameDirectory);
void ssr_audio_free(SSRAudio *audio);

// Plays the sound with the name of Sounds/Sounds.txt, for example
// "UISelect". A name with more than one file plays one of them.
void ssr_audio_play(SSRAudio *audio, const char *name);
// Plays the sound at a part of the volume: 0 is off, 1 is full. A name such
// as "Sounds/HitNet" that is not in the list plays that file of the import,
// as g_LoadMySound does.
void ssr_audio_play_at(SSRAudio *audio, const char *name, float volume);

// The volume of all the sounds: 0 is off, 1 is full.
void ssr_audio_set_volume(SSRAudio *audio, float volume);

#endif
