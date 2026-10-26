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

#endif
