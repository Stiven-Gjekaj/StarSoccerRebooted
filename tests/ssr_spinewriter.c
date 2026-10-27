// ssr_spinewriter: writes a small .spinary file for the tests.

#include "ssr_spinewriter.h"

#include <string.h>

typedef struct {
    unsigned char data[4096];
    size_t size;
} Writer;

static void pad(Writer *w, size_t to) {
    while (w->size % to) w->data[w->size++] = 0xCD;
}

static void put_int(Writer *w, int value) {
    pad(w, 4);
    for (int i = 0; i < 4; i++) w->data[w->size++] = (unsigned char)((unsigned)value >> (8 * i));
}

static void put_short(Writer *w, int value) {
    pad(w, 2);
    w->data[w->size++] = (unsigned char)(value & 0xFF);
    w->data[w->size++] = (unsigned char)((value >> 8) & 0xFF);
}

static void put_float(Writer *w, float value) {
    int bits;
    memcpy(&bits, &value, 4);
    put_int(w, bits);
}

static void put_string(Writer *w, const char *text) {
    size_t length = strlen(text);
    w->data[w->size++] = (unsigned char)length;
    memcpy(w->data + w->size, text, length);
    w->size += length;
    pad(w, 4);
}

static void put_bone(Writer *w, int parent, float x, float y, float rotation, const char *name) {
    put_short(w, parent);
    put_short(w, 3);
    put_float(w, 10);
    put_float(w, x);
    put_float(w, y);
    put_float(w, rotation);
    put_float(w, 1);
    put_float(w, 1);
    put_string(w, name);
}

size_t ssr_spine_writer_build(unsigned char *out) {
    Writer w = {{0}, 0};
    put_string(&w, "Test");
    put_float(&w, 100);
    put_float(&w, 100);
    put_int(&w, 2);
    put_bone(&w, -1, 100, 0, 0, "root");
    put_bone(&w, 0, 10, 0, 0, "child");
    put_int(&w, 2);
    const char *slots[2] = {"Leaf", "Unused"};
    for (int i = 0; i < 2; i++) {
        put_string(&w, slots[i]);
        put_int(&w, 1);
        for (int c = 0; c < 4; c++) put_float(&w, 1);
        put_string(&w, "leaf");
        put_int(&w, 0);
    }
    put_int(&w, 1);
    put_string(&w, "default");
    put_int(&w, 2);
    for (int i = 0; i < 2; i++) {
        put_int(&w, 1);
        put_string(&w, "leaf");
        put_string(&w, "NewGUI/TitleScreen/Title_Grass01");
        const float values[11] = {5, 0, 1, 1, 0, 40, 20, 1, 1, 1, 1};
        for (int v = 0; v < 11; v++) put_float(&w, values[v]);
        put_int(&w, 0);
        put_int(&w, 0);
    }
    put_int(&w, 0);
    put_int(&w, 0);

    put_int(&w, 1);
    put_float(&w, 2);
    put_string(&w, "Sway");
    put_int(&w, 2);
    // Rotate the child from key 1: 0 then 90 then 90.
    put_int(&w, 6);
    put_int(&w, 1);
    put_int(&w, 1);
    put_int(&w, 3);
    put_float(&w, 0);
    put_float(&w, 90);
    put_float(&w, 90);
    put_int(&w, 0x600DF00D);
    // The colour of slot 0 from 0.5 seconds: opaque red.
    put_int(&w, 9);
    put_int(&w, 0);
    put_float(&w, 0.5f);
    put_int(&w, 1);
    put_int(&w, (int)0xFF0000FFu);
    put_int(&w, 0x600DF00D);
    put_int(&w, 2);
    put_int(&w, 0);
    put_int(&w, 1);
    put_int(&w, 1);
    put_int(&w, 0);
    memcpy(out, w.data, w.size);
    return w.size;
}
