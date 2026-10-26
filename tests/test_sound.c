// Tests ssr_sound with a sound list that the test writes. The decoders are
// third party code. The test checks only that a file which is not a sound
// fails.

#include <stdlib.h>
#include <string.h>

#include "ssr_sound.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static void write_text(const char *directory, const char *name, const char *text) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    fputs(text, file);
    fclose(file);
}

static void test_reads_the_sound_list(void) {
    const char *directory = ssr_test_directory("sounds");
    char sub[1200];
    snprintf(sub, sizeof(sub), "mkdir -p '%s/Sounds'", directory);
    CHECK(system(sub) == 0);
    write_text(directory, "Sounds/Sounds.txt",
               "// A comment\r\n"
               "Snd:(NotFound,Sounds/Beep)\r\n"
               "// Snd:(Old,Sounds/Old)\r\n"
               "Snd:(AudienceCheer,Sounds/CrowdCheer_0)\r\n"
               "Snd:(AudienceCheer,Sounds/CrowdCheer_1)\r\n"
               "Snd:(Missing,Sounds/NoFile)\r\n");
    write_text(directory, "Sounds/Beep.ogg", "x");
    write_text(directory, "Sounds/CrowdCheer_0.mp3", "x");
    write_text(directory, "Sounds/CrowdCheer_1.ogg", "x");

    SSRSounds *sounds = ssr_sounds_load(directory);
    CHECK(sounds != NULL);
    CHECK(ssr_sounds_count(sounds, "NotFound") == 1);
    CHECK(strcmp(ssr_sounds_file(sounds, "NotFound", 0), "Sounds/Beep.ogg") == 0);
    CHECK(ssr_sounds_count(sounds, "AudienceCheer") == 2);
    CHECK(strcmp(ssr_sounds_file(sounds, "AudienceCheer", 0), "Sounds/CrowdCheer_0.mp3") == 0);
    CHECK(strcmp(ssr_sounds_file(sounds, "AudienceCheer", 1), "Sounds/CrowdCheer_1.ogg") == 0);
    CHECK(ssr_sounds_count(sounds, "Old") == 0);
    CHECK(ssr_sounds_count(sounds, "Missing") == 0);
    CHECK(ssr_sounds_file(sounds, "AudienceCheer", 2) == NULL);

    // The files hold no sound.
    char path[1200];
    SSRPcm pcm;
    snprintf(path, sizeof(path), "%s/Sounds/Beep.ogg", directory);
    CHECK(!ssr_sound_decode(path, &pcm) && pcm.samples == NULL);
    snprintf(path, sizeof(path), "%s/Sounds/CrowdCheer_0.mp3", directory);
    CHECK(!ssr_sound_decode(path, &pcm) && pcm.samples == NULL);
    CHECK(!ssr_sound_decode("/nonexistent/sound.wav", &pcm));
    ssr_sounds_free(sounds);

    CHECK(ssr_sounds_load("/nonexistent") == NULL);
}

int main(void) {
    test_reads_the_sound_list();
    return TEST_RESULT();
}
