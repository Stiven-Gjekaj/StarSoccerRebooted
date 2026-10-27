#include "ssr_spine.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SENTINEL 0x600DF00D

typedef struct {
    int parent;
    int flags;
    float x, y, rotation, scaleX, scaleY;
} Bone;

typedef struct {
    char *name;
    int bone;
    float r, g, b, a;
    char *attachment;
} Slot;

typedef struct {
    int skin;
    int slot;
    char *key;
    char *image;
    char *file;
    float x, y, scaleX, scaleY, rotation;
} Attachment;

// The game files have only the simple timelines (types 6 to 9). The other
// types are read and kept out of the pose.
typedef struct {
    int type;
    int index;
    int startKey;
    float startSeconds;
    float *values;
    int count;
} Timeline;

typedef struct {
    char *name;
    float duration;
    Timeline *timelines;
    int timelineCount;
    int *usedBones;
    int usedBoneCount;
    int *usedSlots;
    int usedSlotCount;
} Animation;

struct SSRSpine {
    Bone *bones;
    int boneCount;
    Slot *slots;
    int slotCount;
    Attachment *attachments;
    int attachmentCount;
    int defaultSkin;
    Animation *animations;
    int animationCount;
};

// The reader of c_PeePok: little-endian values, each at a multiple of its
// size from the start of the file.
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t at;
    int ok;
} Reader;

static void align(Reader *r, size_t to) {
    r->at = (r->at + to - 1) & ~(to - 1);
}

static uint32_t raw32(Reader *r) {
    if (r->at + 4 > r->size) {
        r->ok = 0;
        return 0;
    }
    const unsigned char *p = r->data + r->at;
    r->at += 4;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int32_t read_int(Reader *r) {
    align(r, 4);
    return (int32_t)raw32(r);
}

static float read_float(Reader *r) {
    uint32_t bits = (align(r, 4), raw32(r));
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int read_short(Reader *r) {
    align(r, 2);
    if (r->at + 2 > r->size) {
        r->ok = 0;
        return 0;
    }
    const unsigned char *p = r->data + r->at;
    r->at += 2;
    return (int16_t)(p[0] | p[1] << 8);
}

// A signed length byte and its bytes, with no alignment after them.
static char *read_raw_string(Reader *r) {
    if (r->at >= r->size) {
        r->ok = 0;
        return NULL;
    }
    int length = (signed char)r->data[r->at++];
    if (length < 0) length = 0;
    if (r->at + (size_t)length > r->size) {
        r->ok = 0;
        return NULL;
    }
    char *text = malloc((size_t)length + 1);
    if (!text) {
        r->ok = 0;
        return NULL;
    }
    memcpy(text, r->data + r->at, (size_t)length);
    text[length] = 0;
    r->at += (size_t)length;
    return text;
}

static char *read_string(Reader *r) {
    char *text = read_raw_string(r);
    align(r, 4);
    return text;
}

static float *read_floats(Reader *r, int *count) {
    *count = read_int(r);
    if (!r->ok || *count < 0 || (size_t)*count * 4 > r->size - r->at) {
        r->ok = 0;
        *count = 0;
        return NULL;
    }
    float *values = malloc((size_t)(*count ? *count : 1) * sizeof(float));
    if (!values) {
        r->ok = 0;
        return NULL;
    }
    for (int i = 0; i < *count; i++) values[i] = read_float(r);
    return values;
}

static int *read_ints(Reader *r, int *count) {
    *count = read_int(r);
    if (!r->ok || *count < 0 || (size_t)*count * 4 > r->size - r->at) {
        r->ok = 0;
        *count = 0;
        return NULL;
    }
    int *values = malloc((size_t)(*count ? *count : 1) * sizeof(int));
    if (!values) {
        r->ok = 0;
        return NULL;
    }
    for (int i = 0; i < *count; i++) values[i] = read_int(r);
    return values;
}

// The list of attachment names: a count with no alignment before it, the
// strings with no alignment between them, then the alignment.
static void skip_names(Reader *r) {
    int count = (int32_t)raw32(r);
    for (int i = 0; i < count && r->ok; i++) free(read_raw_string(r));
    align(r, 4);
}

// The part of the path after the last '/', without the extension.
static char *strip_path(const char *path) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *dot = strrchr(name, '.');
    size_t length = dot ? (size_t)(dot - name) : strlen(name);
    char *out = malloc(length + 1);
    if (!out) return NULL;
    memcpy(out, name, length);
    out[length] = 0;
    return out;
}

// The attachments of a slot are a tree of a Monkey StringMap, before its
// branches.
static void read_attachments(Reader *r, SSRSpine *spine, int skin, int slot, int depth) {
    if (depth > 64 || read_int(r) == 0 || !r->ok) return;
    char *key = read_string(r);
    char *path = read_string(r);
    float values[11];
    for (int i = 0; i < 11; i++) values[i] = read_float(r);
    Attachment *grown = realloc(spine->attachments, (size_t)(spine->attachmentCount + 1) * sizeof(Attachment));
    if (!grown || !key || !path) {
        free(key);
        free(path);
        r->ok = 0;
        return;
    }
    spine->attachments = grown;
    Attachment *a = &grown[spine->attachmentCount++];
    a->skin = skin;
    a->slot = slot;
    a->key = key;
    a->image = strip_path(path);
    size_t length = strlen(path);
    a->file = malloc(length + 5);
    if (a->file) snprintf(a->file, length + 5, "%s.png", path);
    free(path);
    a->x = values[0];
    a->y = values[1];
    a->scaleX = values[2];
    a->scaleY = values[3];
    a->rotation = values[4];
    read_attachments(r, spine, skin, slot, depth + 1);
    read_attachments(r, spine, skin, slot, depth + 1);
}

static void read_timeline(Reader *r, Timeline *t) {
    memset(t, 0, sizeof(*t));
    t->type = read_int(r);
    int count = 0;
    switch (t->type) {
    case 0:
    case 1:
    case 2:
    case 3:
        t->index = read_int(r);
        free(read_floats(r, &count));
        free(read_floats(r, &count));
        break;
    case 4:
        t->index = read_int(r);
        free(read_floats(r, &count));
        skip_names(r);
        break;
    case 5: {
        free(read_floats(r, &count));
        for (int i = 0; i < count && r->ok; i++) {
            read_int(r);
            read_int(r);
            read_float(r);
            free(read_string(r));
        }
        break;
    }
    case 6:
    case 7:
    case 8:
        t->index = read_int(r);
        t->startKey = read_int(r);
        t->values = read_floats(r, &t->count);
        break;
    case 9: {
        t->index = read_int(r);
        t->startSeconds = read_float(r);
        int *colours = read_ints(r, &t->count);
        t->values = malloc((size_t)(t->count ? t->count : 1) * sizeof(float));
        if (colours && t->values) memcpy(t->values, colours, (size_t)t->count * sizeof(int));
        free(colours);
        break;
    }
    default:
        r->ok = 0;
        return;
    }
    if ((uint32_t)read_int(r) != SENTINEL) r->ok = 0;
}

SSRSpine *ssr_spine_parse(const unsigned char *data, size_t size) {
    Reader r = {data, size, 0, 1};
    SSRSpine *spine = calloc(1, sizeof(SSRSpine));
    if (!spine) return NULL;
    free(read_string(&r));
    read_float(&r);
    read_float(&r);

    spine->boneCount = read_int(&r);
    if (!r.ok || spine->boneCount < 0 || spine->boneCount > 4096) goto bad;
    spine->bones = calloc((size_t)spine->boneCount + 1, sizeof(Bone));
    if (!spine->bones) goto bad;
    for (int i = 0; i < spine->boneCount && r.ok; i++) {
        Bone *b = &spine->bones[i];
        b->parent = read_short(&r);
        b->flags = read_short(&r);
        read_float(&r);
        b->x = read_float(&r);
        b->y = read_float(&r);
        b->rotation = read_float(&r);
        b->scaleX = read_float(&r);
        b->scaleY = read_float(&r);
        free(read_string(&r));
        if (b->parent >= i) r.ok = 0;
    }

    spine->slotCount = read_int(&r);
    if (!r.ok || spine->slotCount < 0 || spine->slotCount > 4096) goto bad;
    spine->slots = calloc((size_t)spine->slotCount + 1, sizeof(Slot));
    if (!spine->slots) goto bad;
    for (int i = 0; i < spine->slotCount && r.ok; i++) {
        Slot *s = &spine->slots[i];
        s->name = read_string(&r);
        s->bone = read_int(&r);
        s->r = read_float(&r);
        s->g = read_float(&r);
        s->b = read_float(&r);
        s->a = read_float(&r);
        s->attachment = read_string(&r);
        read_int(&r);
        if (s->bone < 0 || s->bone >= spine->boneCount) r.ok = 0;
    }

    int skins = read_int(&r);
    if (!r.ok || skins < 0 || skins > 256) goto bad;
    for (int skin = 0; skin < skins && r.ok; skin++) {
        free(read_string(&r));
        int slots = read_int(&r);
        for (int slot = 0; slot < slots && r.ok; slot++) read_attachments(&r, spine, skin, slot, 0);
    }
    spine->defaultSkin = read_int(&r);

    int events = read_int(&r);
    if (!r.ok || events < 0 || events > 4096) goto bad;
    for (int i = 0; i < events && r.ok; i++) {
        free(read_string(&r));
        read_int(&r);
        read_float(&r);
        free(read_string(&r));
    }

    spine->animationCount = read_int(&r);
    if (!r.ok || spine->animationCount < 0 || spine->animationCount > 1024) goto bad;
    spine->animations = calloc((size_t)spine->animationCount + 1, sizeof(Animation));
    if (!spine->animations) goto bad;
    for (int i = 0; i < spine->animationCount && r.ok; i++) {
        Animation *a = &spine->animations[i];
        a->duration = read_float(&r);
        a->name = read_string(&r);
        a->timelineCount = read_int(&r);
        if (!r.ok || a->timelineCount < 0 || a->timelineCount > 65536) goto bad;
        a->timelines = calloc((size_t)a->timelineCount + 1, sizeof(Timeline));
        if (!a->timelines) goto bad;
        for (int t = 0; t < a->timelineCount && r.ok; t++) read_timeline(&r, &a->timelines[t]);
        a->usedBones = read_ints(&r, &a->usedBoneCount);
        a->usedSlots = read_ints(&r, &a->usedSlotCount);
    }
    if (!r.ok || r.at != size) goto bad;
    return spine;
bad:
    ssr_spine_free(spine);
    return NULL;
}

SSRSpine *ssr_spine_load(const char *path) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return NULL;
    SSRSpine *spine = NULL;
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) length = ftell(file);
    unsigned char *data = length > 0 ? malloc((size_t)length) : NULL;
    if (data && fseek(file, 0, SEEK_SET) == 0 && fread(data, 1, (size_t)length, file) == (size_t)length) {
        spine = ssr_spine_parse(data, (size_t)length);
    }
    free(data);
    fclose(file);
    return spine;
}

void ssr_spine_free(SSRSpine *spine) {
    if (!spine) return;
    for (int i = 0; i < spine->slotCount; i++) {
        free(spine->slots[i].name);
        free(spine->slots[i].attachment);
    }
    free(spine->slots);
    free(spine->bones);
    for (int i = 0; i < spine->attachmentCount; i++) {
        free(spine->attachments[i].key);
        free(spine->attachments[i].image);
        free(spine->attachments[i].file);
    }
    free(spine->attachments);
    for (int i = 0; i < spine->animationCount; i++) {
        Animation *a = &spine->animations[i];
        for (int t = 0; t < a->timelineCount; t++) free(a->timelines[t].values);
        free(a->timelines);
        free(a->usedBones);
        free(a->usedSlots);
        free(a->name);
    }
    free(spine->animations);
    free(spine);
}

int ssr_spine_bone_count(const SSRSpine *spine) {
    return spine ? spine->boneCount : 0;
}

int ssr_spine_slot_count(const SSRSpine *spine) {
    return spine ? spine->slotCount : 0;
}

int ssr_spine_animation_count(const SSRSpine *spine) {
    return spine ? spine->animationCount : 0;
}

int ssr_spine_animation(const SSRSpine *spine, const char *name) {
    if (!spine || !name) return -1;
    for (int i = 0; i < spine->animationCount; i++) {
        if (spine->animations[i].name && strcmp(spine->animations[i].name, name) == 0) return i;
    }
    return -1;
}

float ssr_spine_duration(const SSRSpine *spine, int animation) {
    if (!spine || animation < 0 || animation >= spine->animationCount) return 0;
    return spine->animations[animation].duration;
}

// The value of a simple timeline at frame f of 60 each second. A key is
// two frames. An odd frame between two keys takes the mean of them.
static int simple_value(const Timeline *t, int frame, int width, float *out) {
    int keys = t->count / width;
    int key = frame >> 1;
    if (keys <= 0 || key < t->startKey) return 0;
    int last = t->startKey + keys - 1;
    if (key >= last) {
        for (int i = 0; i < width; i++) out[i] = t->values[(last - t->startKey) * width + i];
        return 1;
    }
    for (int i = 0; i < width; i++) {
        float a = t->values[(key - t->startKey) * width + i];
        if (frame & 1) {
            float b = t->values[(key + 1 - t->startKey) * width + i];
            a = (a + b) * 0.5f;
        }
        out[i] = a;
    }
    return 1;
}

typedef struct {
    float x, y, rotation, scaleX, scaleY;
    float m00, m01, m10, m11, wx, wy, wsx, wsy, wrot;
} Pose;

static const Attachment *find_attachment(const SSRSpine *spine, int slot, const char *key) {
    if (!key || !*key) return NULL;
    int skin = spine->defaultSkin >= 0 ? spine->defaultSkin : 0;
    for (int i = 0; i < spine->attachmentCount; i++) {
        const Attachment *a = &spine->attachments[i];
        if (a->skin == skin && a->slot == slot && strcmp(a->key, key) == 0) return a;
    }
    return NULL;
}

// The bones and the slot colours of the animation at the time. The caller
// frees the three arrays. Returns 0 when there is no memory.
static int make_pose(const SSRSpine *spine, int animation, float time, int loop, Pose **posed, float **coloured,
                     char **usedSlots) {
    const Animation *anim = &spine->animations[animation];
    float t = time < 0 ? 0 : time;
    if (anim->duration > 0) t = loop ? fmodf(t, anim->duration) : (t > anim->duration ? anim->duration : t);

    Pose *pose = calloc((size_t)spine->boneCount + 1, sizeof(Pose));
    float *colours = calloc((size_t)spine->slotCount * 4 + 4, sizeof(float));
    char *used = calloc((size_t)spine->slotCount + 1, 1);
    if (!pose || !colours || !used) {
        free(pose);
        free(colours);
        free(used);
        return 0;
    }
    for (int i = 0; i < spine->boneCount; i++) {
        const Bone *b = &spine->bones[i];
        pose[i].x = b->x;
        pose[i].y = b->y;
        pose[i].rotation = b->rotation;
        pose[i].scaleX = b->scaleX;
        pose[i].scaleY = b->scaleY;
    }
    for (int i = 0; i < spine->slotCount; i++) {
        colours[i * 4 + 0] = spine->slots[i].r;
        colours[i * 4 + 1] = spine->slots[i].g;
        colours[i * 4 + 2] = spine->slots[i].b;
        colours[i * 4 + 3] = spine->slots[i].a;
    }
    for (int i = 0; i < anim->usedSlotCount; i++) {
        if (anim->usedSlots[i] >= 0 && anim->usedSlots[i] < spine->slotCount) used[anim->usedSlots[i]] = 1;
    }

    int frame = (int)(t * 60);
    for (int i = 0; i < anim->timelineCount; i++) {
        const Timeline *tl = &anim->timelines[i];
        float v[2];
        if (tl->type >= 6 && tl->type <= 8 && (tl->index < 0 || tl->index >= spine->boneCount)) continue;
        Pose *p = tl->type <= 8 && tl->index >= 0 && tl->index < spine->boneCount ? &pose[tl->index] : NULL;
        if (tl->type == 6 && simple_value(tl, frame, 1, v)) {
            p->rotation = spine->bones[tl->index].rotation + v[0];
        } else if (tl->type == 7 && simple_value(tl, frame, 2, v)) {
            p->x = spine->bones[tl->index].x + v[0];
            p->y = spine->bones[tl->index].y + v[1];
        } else if (tl->type == 8 && simple_value(tl, frame, 2, v)) {
            // The game adds the key to the setup scale.
            p->scaleX = spine->bones[tl->index].scaleX + v[0] - 1;
            p->scaleY = spine->bones[tl->index].scaleY + v[1] - 1;
        } else if (tl->type == 9 && tl->count > 0 && tl->index >= 0 && tl->index < spine->slotCount) {
            int sample = (int)((t - tl->startSeconds) * 60);
            if (sample < 0) continue;
            if (sample >= tl->count) sample = tl->count - 1;
            uint32_t c;
            memcpy(&c, &tl->values[sample], sizeof(c));
            colours[tl->index * 4 + 0] = (c & 0xFF) / 255.0f;
            colours[tl->index * 4 + 1] = ((c >> 8) & 0xFF) / 255.0f;
            colours[tl->index * 4 + 2] = ((c >> 16) & 0xFF) / 255.0f;
            colours[tl->index * 4 + 3] = ((c >> 24) & 0xFF) / 255.0f;
        }
    }

    // The world transforms, y up. A parent comes before its children.
    for (int i = 0; i < spine->boneCount; i++) {
        Pose *p = &pose[i];
        const Bone *b = &spine->bones[i];
        if (b->parent >= 0) {
            const Pose *q = &pose[b->parent];
            p->wx = q->m00 * p->x + q->m01 * p->y + q->wx;
            p->wy = q->m10 * p->x + q->m11 * p->y + q->wy;
            p->wsx = (b->flags & 2) ? q->wsx * p->scaleX : p->scaleX;
            p->wsy = (b->flags & 2) ? q->wsy * p->scaleY : p->scaleY;
            p->wrot = (b->flags & 1) ? q->wrot + p->rotation : p->rotation;
        } else {
            p->wx = p->x;
            p->wy = p->y;
            p->wsx = p->scaleX;
            p->wsy = p->scaleY;
            p->wrot = p->rotation;
        }
        float radians = p->wrot * (float)M_PI / 180;
        float c = cosf(radians), s = sinf(radians);
        p->m00 = c * p->wsx;
        p->m01 = -s * p->wsy;
        p->m10 = s * p->wsx;
        p->m11 = c * p->wsy;
    }

    *posed = pose;
    *coloured = colours;
    *usedSlots = used;
    return 1;
}

// The bone matrix of a slot, y down, in the order of SSRSpineDraw.m.
static void bone_matrix(const Pose *p, float m[6]) {
    m[0] = p->m00;
    m[1] = -p->m10;
    m[2] = -p->m01;
    m[3] = p->m11;
    m[4] = p->wx;
    m[5] = -p->wy;
}

int ssr_spine_pose(const SSRSpine *spine, int animation, float time, int loop, SSRSpineDraw *out, int max) {
    if (!spine || animation < 0 || animation >= spine->animationCount) return 0;
    Pose *pose;
    float *colours;
    char *used;
    if (!make_pose(spine, animation, time, loop, &pose, &colours, &used)) return 0;
    int count = 0;
    for (int i = 0; i < spine->slotCount; i++) {
        if (!used[i] || colours[i * 4 + 3] < 0.005f) continue;
        const Attachment *a = find_attachment(spine, i, spine->slots[i].attachment);
        if (!a || !a->image) continue;
        // The attachment matrix, y down.
        float radians = a->rotation * (float)M_PI / 180;
        float c = cosf(radians), s = sinf(radians);
        float aix = a->scaleX * c, aiy = -a->scaleX * s, ajx = a->scaleY * s, ajy = a->scaleY * c;
        float atx = a->x, aty = -a->y;
        float bone[6];
        bone_matrix(&pose[spine->slots[i].bone], bone);
        float b00 = bone[0], b10 = bone[1], b01 = bone[2], b11 = bone[3], btx = bone[4], bty = bone[5];
        if (count < max && out) {
            SSRSpineDraw *d = &out[count];
            d->image = a->image;
            d->file = a->file;
            d->slot = spine->slots[i].name;
            d->m[0] = b00 * aix + b01 * aiy;
            d->m[1] = b10 * aix + b11 * aiy;
            d->m[2] = b00 * ajx + b01 * ajy;
            d->m[3] = b10 * ajx + b11 * ajy;
            d->m[4] = b00 * atx + b01 * aty + btx;
            d->m[5] = b10 * atx + b11 * aty + bty;
            d->r = colours[i * 4 + 0];
            d->g = colours[i * 4 + 1];
            d->b = colours[i * 4 + 2];
            d->a = colours[i * 4 + 3];
        }
        count++;
    }
    free(pose);
    free(colours);
    free(used);
    return count;
}

int ssr_spine_slot(const SSRSpine *spine, int animation, float time, int loop, const char *slot, float m[6],
                   float rgba[4]) {
    if (!spine || !slot || animation < 0 || animation >= spine->animationCount) return 0;
    int index = -1;
    for (int i = 0; i < spine->slotCount && index < 0; i++) {
        if (spine->slots[i].name && strcmp(spine->slots[i].name, slot) == 0) index = i;
    }
    if (index < 0) return 0;
    Pose *pose;
    float *colours;
    char *used;
    if (!make_pose(spine, animation, time, loop, &pose, &colours, &used)) return 0;
    bone_matrix(&pose[spine->slots[index].bone], m);
    for (int k = 0; k < 4; k++) rgba[k] = colours[index * 4 + k];
    free(pose);
    free(colours);
    free(used);
    return 1;
}
