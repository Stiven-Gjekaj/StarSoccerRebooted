// layoutscan: opens each layout of an import and counts the layouts that
// use each modifier that the scene does not know.
// Usage: layoutscan <game directory>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_scene.h"
#include "ssr_tweaks.h"
#include "ssr_ui.h"

static char screens[4096];

static char *read_file(void *context, const char *name, size_t *size) {
    (void)context;
    char path[8192];
    snprintf(path, sizeof(path), "%s/%s", screens, name);
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *data = malloc((size_t)length + 1);
    if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(file);
    if (data) {
        data[length] = 0;
        *size = (size_t)length;
    }
    return data;
}

static const char *text(void *context, const char *tag) {
    (void)context;
    return tag;
}

static int image_size(void *context, const char *name, const char *file, float *w, float *h) {
    (void)context;
    (void)name;
    (void)file;
    *w = *h = 64;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: layoutscan <game directory>\n");
        return 1;
    }
    snprintf(screens, sizeof(screens), "%s/NewGUI/Screens", argv[1]);
    DIR *dir = opendir(screens);
    if (!dir) return 1;
    char names[256][64];
    int counts[256] = {0}, kinds = 0, layouts = 0;
    SSRTweaks *tweaks = ssr_tweaks_create();
    for (struct dirent *e; (e = readdir(dir)) != NULL;) {
        size_t n = strlen(e->d_name);
        if (n < 5 || strcmp(e->d_name + n - 4, ".uis") != 0) continue;
        SSRUiFiles files = {read_file, NULL};
        SSRUiTree *tree = ssr_ui_parse(e->d_name, &files);
        if (!tree) continue;
        SSRSceneEnv env = {.width = 640, .height = 960, .tweaks = tweaks, .text = text, .image_size = image_size};
        SSRScene *scene = ssr_scene_build(tree, &env);
        layouts++;
        for (int i = 0; i < ssr_scene_unknown_count(scene); i++) {
            const char *u = ssr_scene_unknown(scene, i);
            int k = 0;
            while (k < kinds && strcmp(names[k], u) != 0) k++;
            if (k == kinds && kinds < 256) snprintf(names[kinds++], sizeof(names[0]), "%s", u);
            if (k < 256) counts[k]++;
        }
        ssr_scene_free(scene);
        ssr_ui_free(tree);
    }
    closedir(dir);
    printf("%d layouts\n", layouts);
    for (int k = 0; k < kinds; k++) printf("%4d %s\n", counts[k], names[k]);
    ssr_tweaks_free(tweaks);
    return 0;
}
