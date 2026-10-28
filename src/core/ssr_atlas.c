#include "ssr_atlas.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"

typedef struct {
    unsigned char *data;
    size_t size;
    int paletteSize;
    const unsigned char *palette;
    int frameCount;
    const unsigned char *starts;
    const unsigned char *words;
    int loaded;
} RlePage;

typedef struct {
    const char *base;
    const char **names;
    int count;
} CelGroup;

struct SSRAtlases {
    char *directory;
    SSRAtlasPage *pages;
    int pageCount;
    SSRAtlasImage *images;
    int imageCount;
    // The image indexes, sorted by name.
    int *byName;
    RlePage *rle;
    char **strings;
    int stringCount;
    // The cel groups of the key files: a base image and its cels.
    CelGroup *cels;
    int celCount;
    // The cel group indexes, sorted by base. Of the groups with the same
    // base, the last group of the key files comes first.
    int *celsByBase;
};

// The key reader. Each value starts at a multiple of its size from the
// start of the file.
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t at;
    int ok;
} Reader;

static void align(Reader *r, size_t to) {
    r->at = (r->at + to - 1) & ~(to - 1);
}

static int32_t read_int(Reader *r) {
    align(r, 4);
    if (r->at + 4 > r->size) {
        r->ok = 0;
        return 0;
    }
    const unsigned char *p = r->data + r->at;
    r->at += 4;
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
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

static float read_float(Reader *r) {
    int32_t bits = read_int(r);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static const char *keep(SSRAtlases *a, const char *text, size_t length) {
    char *copy = malloc(length + 1);
    char **strings = realloc(a->strings, (size_t)(a->stringCount + 1) * sizeof(char *));
    if (!copy || !strings) {
        free(copy);
        if (strings) a->strings = strings;
        return "";
    }
    memcpy(copy, text, length);
    copy[length] = 0;
    a->strings = strings;
    a->strings[a->stringCount++] = copy;
    return copy;
}

// A string is a signed length byte and its bytes. The next value starts
// at a multiple of 4.
static const char *read_string(Reader *r, SSRAtlases *a) {
    if (r->at >= r->size) {
        r->ok = 0;
        return "";
    }
    int length = (signed char)r->data[r->at++];
    if (length < 0) length = 0;
    if (r->at + (size_t)length > r->size) {
        r->ok = 0;
        return "";
    }
    const char *text = length ? keep(a, (const char *)r->data + r->at, (size_t)length) : "";
    r->at += (size_t)length;
    align(r, 4);
    return text;
}

static void read_shapes(Reader *r, SSRAtlasImage *image) {
    for (;;) {
        float first = read_float(r);
        if (!r->ok || first == -1.0f) return;
        float *shapes = realloc(image->shapes, (size_t)(image->shapeCount + 1) * 12 * sizeof(float));
        if (!shapes) {
            r->ok = 0;
            return;
        }
        image->shapes = shapes;
        float *shape = shapes + image->shapeCount * 12;
        shape[0] = first;
        for (int i = 1; i < 12; i++) shape[i] = read_float(r);
        image->shapeCount++;
    }
}

static void add_unpack(SSRAtlasImage *image, SSRAtlasUnpack unpack) {
    SSRAtlasUnpack *list = realloc(image->unpacks, (size_t)(image->unpackCount + 1) * sizeof(SSRAtlasUnpack));
    if (!list) return;
    image->unpacks = list;
    image->unpacks[image->unpackCount++] = unpack;
}

static SSRAtlasUnpack read_unpack(Reader *r) {
    SSRAtlasUnpack u = {0};
    u.sourcePage = read_short(r);
    u.sourceFrame = read_short(r);
    u.x = read_short(r);
    u.y = read_short(r);
    u.keepAlpha = read_int(r);
    return u;
}

static void read_image(Reader *r, SSRAtlases *a, SSRAtlasImage *image) {
    image->flags = read_int(r);
    image->name = read_string(r, a);
    if (image->flags & 2) {
        image->kind = SSR_ATLAS_SLICED;
        if (image->flags & 1) add_unpack(image, read_unpack(r));
        image->originalWidth = read_short(r);
        image->originalHeight = read_short(r);
        image->trimWidth = read_short(r);
        image->trimHeight = read_short(r);
        image->fixedWidth = read_short(r);
        image->fixedHeight = read_short(r);
        read_shapes(r, image);
    } else if (image->flags & 8) {
        image->kind = SSR_ATLAS_STRIPPED;
        if (image->flags & 1) {
            int count = read_short(r);
            int source = read_short(r);
            int keepAlpha = count & 1 ? read_short(r) : read_int(r);
            for (int i = 0; i < count && r->ok; i++) {
                SSRAtlasUnpack u = {source, 0, 0, 0, 0, 0, keepAlpha};
                u.sourceFrame = read_short(r);
                u.x = read_short(r);
                u.y = read_short(r);
                u.width = read_short(r);
                u.height = read_short(r);
                add_unpack(image, u);
            }
        }
        image->originalWidth = read_short(r);
        image->originalHeight = read_short(r);
        read_shapes(r, image);
    } else if (image->flags & 4) {
        image->kind = SSR_ATLAS_PACKED;
        image->originalWidth = read_short(r);
        image->originalHeight = read_short(r);
        image->trimWidth = read_short(r);
        image->trimHeight = read_short(r);
    } else {
        image->kind = SSR_ATLAS_PLAIN;
        if (image->flags & 1) add_unpack(image, read_unpack(r));
        image->originalWidth = read_short(r);
        image->originalHeight = read_short(r);
        image->trimWidth = read_short(r);
        image->trimHeight = read_short(r);
        for (int i = 0; i < 8; i++) image->rect[i] = read_float(r);
    }
    // A plain or sliced unpack is as big as the trimmed image.
    if (image->kind != SSR_ATLAS_STRIPPED) {
        for (int i = 0; i < image->unpackCount; i++) {
            image->unpacks[i].width = image->trimWidth;
            image->unpacks[i].height = image->trimHeight;
        }
    }
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *file = ssr_fs_open(path, "rb");
    if (!file) return NULL;
    unsigned char *data = NULL;
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) length = ftell(file);
    if (length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
        data = malloc((size_t)length + 1);
        if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
            free(data);
            data = NULL;
        }
    }
    fclose(file);
    if (data) *size = (size_t)length;
    return data;
}

static int load_key(SSRAtlases *a, const unsigned char *data, size_t size) {
    Reader r = {data, size, 0, 1};
    int pages = read_int(&r);
    if (!r.ok || pages < 0 || pages > 100000) return 0;
    SSRAtlasPage *grown = realloc(a->pages, (size_t)(a->pageCount + pages) * sizeof(SSRAtlasPage));
    if (!grown) return 0;
    a->pages = grown;
    int base = a->pageCount;
    for (int p = 0; p < pages && r.ok; p++) {
        SSRAtlasPage *page = &a->pages[a->pageCount++];
        memset(page, 0, sizeof(*page));
        page->name = read_string(&r, a);
        page->width = read_int(&r);
        page->height = read_int(&r);
        int count = read_int(&r);
        if (!r.ok || count < 0 || count > 1000000) return 0;
        SSRAtlasImage *images = realloc(a->images, (size_t)(a->imageCount + count) * sizeof(SSRAtlasImage));
        if (!images && count) return 0;
        if (images) a->images = images;
        page->firstImage = a->imageCount;
        page->imageCount = count;
        for (int i = 0; i < count && r.ok; i++) {
            SSRAtlasImage *image = &a->images[a->imageCount++];
            memset(image, 0, sizeof(*image));
            image->page = base + p;
            image->frame = i;
            read_image(&r, a, image);
        }
    }
    // The cel groups follow: "Base, cel1, cel2, ..., ''". The image of a
    // cel is "cel_Base".
    for (;;) {
        const char *base = read_string(&r, a);
        if (!r.ok || !*base) break;
        CelGroup group = {base, NULL, 0};
        for (;;) {
            const char *cel = read_string(&r, a);
            if (!r.ok || !*cel) break;
            const char **names = realloc(group.names, (size_t)(group.count + 1) * sizeof(char *));
            if (!names) break;
            group.names = names;
            group.names[group.count++] = cel;
        }
        CelGroup *groups = realloc(a->cels, (size_t)(a->celCount + 1) * sizeof(CelGroup));
        if (!groups) {
            free(group.names);
            break;
        }
        a->cels = groups;
        a->cels[a->celCount++] = group;
    }
    return r.ok;
}

static const SSRAtlases *sort_context;

static int compare_names(const void *a, const void *b) {
    return strcmp(sort_context->images[*(const int *)a].name, sort_context->images[*(const int *)b].name);
}

static int compare_bases(const void *a, const void *b) {
    int i = *(const int *)a, j = *(const int *)b;
    int order = strcmp(sort_context->cels[i].base, sort_context->cels[j].base);
    return order != 0 ? order : j - i;
}

SSRAtlases *ssr_atlas_load(const char *directory, const char *const *keys, int keyCount) {
    SSRAtlases *a = calloc(1, sizeof(SSRAtlases));
    if (!a) return NULL;
    a->directory = malloc(strlen(directory) + 1);
    if (!a->directory) {
        free(a);
        return NULL;
    }
    strcpy(a->directory, directory);
    for (int k = 0; k < keyCount; k++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", directory, keys[k]);
        size_t size = 0;
        unsigned char *data = read_file(path, &size);
        int ok = data && load_key(a, data, size);
        free(data);
        if (!ok) {
            ssr_atlas_free(a);
            return NULL;
        }
    }
    a->rle = calloc(a->pageCount ? (size_t)a->pageCount : 1, sizeof(RlePage));
    a->byName = malloc((a->imageCount ? (size_t)a->imageCount : 1) * sizeof(int));
    a->celsByBase = malloc((a->celCount ? (size_t)a->celCount : 1) * sizeof(int));
    if (!a->rle || !a->byName || !a->celsByBase) {
        ssr_atlas_free(a);
        return NULL;
    }
    for (int i = 0; i < a->pageCount; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s-quant.bin", directory, a->pages[i].name);
        FILE *file = ssr_fs_open(path, "rb");
        a->pages[i].rle = file != NULL;
        if (file) fclose(file);
    }
    for (int i = 0; i < a->imageCount; i++) a->byName[i] = i;
    sort_context = a;
    qsort(a->byName, (size_t)a->imageCount, sizeof(int), compare_names);
    for (int i = 0; i < a->celCount; i++) a->celsByBase[i] = i;
    qsort(a->celsByBase, (size_t)a->celCount, sizeof(int), compare_bases);
    sort_context = NULL;
    return a;
}

void ssr_atlas_free(SSRAtlases *a) {
    if (!a) return;
    for (int i = 0; i < a->imageCount; i++) {
        free(a->images[i].shapes);
        free(a->images[i].unpacks);
    }
    free(a->images);
    free(a->pages);
    if (a->rle) {
        for (int i = 0; i < a->pageCount; i++) free(a->rle[i].data);
    }
    free(a->rle);
    free(a->byName);
    free(a->celsByBase);
    for (int i = 0; i < a->celCount; i++) free(a->cels[i].names);
    free(a->cels);
    for (int i = 0; i < a->stringCount; i++) free(a->strings[i]);
    free(a->strings);
    free(a->directory);
    free(a);
}

int ssr_atlas_page_count(const SSRAtlases *a) {
    return a ? a->pageCount : 0;
}

const SSRAtlasPage *ssr_atlas_page(const SSRAtlases *a, int index) {
    return a && index >= 0 && index < a->pageCount ? &a->pages[index] : NULL;
}

int ssr_atlas_image_count(const SSRAtlases *a) {
    return a ? a->imageCount : 0;
}

const SSRAtlasImage *ssr_atlas_image(const SSRAtlases *a, int index) {
    return a && index >= 0 && index < a->imageCount ? &a->images[index] : NULL;
}

int ssr_atlas_cels(const SSRAtlases *a, const char *base, const char **names, int max) {
    if (!a || !base) return 0;
    // The first group with the base in the sorted index: the last group
    // of the key files.
    int low = 0, high = a->celCount;
    while (low < high) {
        int middle = (low + high) / 2;
        if (strcmp(a->cels[a->celsByBase[middle]].base, base) < 0) low = middle + 1;
        else high = middle;
    }
    if (low >= a->celCount) return 0;
    const CelGroup *group = &a->cels[a->celsByBase[low]];
    if (strcmp(group->base, base) != 0) return 0;
    int n = group->count < max ? group->count : max;
    for (int k = 0; k < n; k++) names[k] = group->names[k];
    return group->count;
}

const SSRAtlasImage *ssr_atlas_find(const SSRAtlases *a, const char *name) {
    if (!a || !name) return NULL;
    int low = 0, high = a->imageCount - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        int order = strcmp(a->images[a->byName[middle]].name, name);
        if (order == 0) return &a->images[a->byName[middle]];
        if (order < 0) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return NULL;
}

static uint32_t word_at(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static RlePage *rle_page(SSRAtlases *a, int page) {
    if (page < 0 || page >= a->pageCount || !a->pages[page].rle) return NULL;
    RlePage *rle = &a->rle[page];
    if (rle->loaded) return rle->data ? rle : NULL;
    rle->loaded = 1;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s-quant.bin", a->directory, a->pages[page].name);
    rle->data = read_file(path, &rle->size);
    if (!rle->data || rle->size < 8) goto bad;
    // The palette, the frame count, the start of each frame in words, and
    // then the words.
    rle->paletteSize = (int)word_at(rle->data);
    size_t at = 4 + (size_t)rle->paletteSize * 4;
    if (rle->paletteSize <= 0 || rle->paletteSize > 255 || at + 4 > rle->size) goto bad;
    rle->palette = rle->data + 4;
    rle->frameCount = (int)word_at(rle->data + at);
    at += 4;
    if (rle->frameCount < 0 || at + ((size_t)rle->frameCount + 1) * 4 > rle->size) goto bad;
    rle->starts = rle->data + at;
    rle->words = rle->starts + ((size_t)rle->frameCount + 1) * 4;
    if ((size_t)(rle->words - rle->data) + (size_t)word_at(rle->starts + (size_t)rle->frameCount * 4) * 4 > rle->size) {
        goto bad;
    }
    return rle;
bad:
    free(rle->data);
    rle->data = NULL;
    return NULL;
}

// Writes the frame into pixels, a buffer of width x height RGBA pixels.
// A byte under the palette size is a colour, and it goes count times. A
// byte above it sets the count for the next colour. The byte equal to it
// ends the row.
static int decode_frame(const RlePage *rle, int frame, unsigned char *pixels, int width, int height) {
    if (frame < 0 || frame >= rle->frameCount) return 0;
    uint32_t start = word_at(rle->starts + (size_t)frame * 4);
    uint32_t end = word_at(rle->starts + ((size_t)frame + 1) * 4);
    if (end < start) return 0;
    const unsigned char *p = rle->words + (size_t)start * 4;
    const unsigned char *stop = rle->words + (size_t)end * 4;
    int x = 0, y = 0, count = 1;
    for (; p < stop; p++) {
        int b = *p;
        if (b < rle->paletteSize) {
            const unsigned char *colour = rle->palette + b * 4;
            for (int i = 0; i < count; i++, x++) {
                if (x < width && y < height) memcpy(pixels + ((size_t)y * width + x) * 4, colour, 4);
            }
            count = 1;
        } else {
            count = b - rle->paletteSize;
            if (count == 0) {
                x = 0;
                y++;
                count = 1;
            }
        }
    }
    return 1;
}

int ssr_atlas_frame_pixels(SSRAtlases *a, int page, int frame, SSRImage *out) {
    memset(out, 0, sizeof(*out));
    RlePage *rle = a ? rle_page(a, page) : NULL;
    if (!rle || frame < 0 || frame >= a->pages[page].imageCount) return 0;
    const SSRAtlasImage *image = &a->images[a->pages[page].firstImage + frame];
    int width = image->trimWidth, height = image->trimHeight;
    if (width <= 0 || height <= 0) return 0;
    out->pixels = calloc((size_t)width * height, 4);
    if (!out->pixels) return 0;
    out->width = width;
    out->height = height;
    if (!decode_frame(rle, frame, out->pixels, width, height)) {
        ssr_image_free(out);
        return 0;
    }
    return 1;
}

// Puts a decoded frame into the page at (x, y). The edge pixels of the
// frame also go one pixel out, so that filtering at the edge does not take
// the pixels around it.
static void paste(SSRImage *page, const SSRImage *frame, int x, int y) {
    for (int fy = -1; fy <= frame->height; fy++) {
        int sy = fy < 0 ? 0 : fy >= frame->height ? frame->height - 1 : fy;
        int py = y + fy;
        if (py < 0 || py >= page->height) continue;
        for (int fx = -1; fx <= frame->width; fx++) {
            int sx = fx < 0 ? 0 : fx >= frame->width ? frame->width - 1 : fx;
            int px = x + fx;
            if (px < 0 || px >= page->width) continue;
            memcpy(page->pixels + ((size_t)py * page->width + px) * 4,
                   frame->pixels + ((size_t)sy * frame->width + sx) * 4, 4);
        }
    }
}

int ssr_atlas_page_pixels(SSRAtlases *a, int page, SSRImage *out) {
    memset(out, 0, sizeof(*out));
    if (!a || page < 0 || page >= a->pageCount || a->pages[page].rle) return 0;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.png", a->directory, a->pages[page].name);
    if (!ssr_image_load(path, out)) return 0;
    const SSRAtlasPage *p = &a->pages[page];
    for (int i = 0; i < p->imageCount; i++) {
        const SSRAtlasImage *image = &a->images[p->firstImage + i];
        for (int u = 0; u < image->unpackCount; u++) {
            const SSRAtlasUnpack *unpack = &image->unpacks[u];
            RlePage *rle = rle_page(a, unpack->sourcePage);
            if (!rle || unpack->width <= 0 || unpack->height <= 0) continue;
            SSRImage frame = {unpack->width, unpack->height, calloc((size_t)unpack->width * unpack->height, 4)};
            if (frame.pixels && decode_frame(rle, unpack->sourceFrame, frame.pixels, frame.width, frame.height)) {
                paste(out, &frame, unpack->x, unpack->y);
            }
            free(frame.pixels);
        }
    }
    return 1;
}

// Copies the pixel of the patch at the place (bx, by) of the box, as parts
// of the box, to the page pixel (px, py), when its alpha is above 0.
static void patch_pixel(SSRImage *page, int px, int py, const SSRImage *patch, float bx, float by) {
    int sx = (int)floorf(bx * patch->width), sy = (int)floorf(by * patch->height);
    if (sx < 0 || sx >= patch->width || sy < 0 || sy >= patch->height) return;
    const unsigned char *from = patch->pixels + ((size_t)sy * patch->width + sx) * 4;
    if (from[3] > 0) memcpy(page->pixels + ((size_t)py * page->width + px) * 4, from, 4);
}

// A stripped image: each page pixel in a triangle takes the place in the
// box that the triangle gives it.
static void patch_stripped(SSRImage *page, const SSRAtlasImage *image, const SSRImage *patch) {
    for (int i = 0; i < image->shapeCount; i++) {
        const float *t = image->shapes + i * 12;
        float ax = t[2] * page->width, ay = t[3] * page->height;
        float bx = t[6] * page->width, by = t[7] * page->height;
        float cx = t[10] * page->width, cy = t[11] * page->height;
        float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
        if (fabsf(area) < 1e-6f) continue;
        int x0 = (int)floorf(fminf(ax, fminf(bx, cx))), x1 = (int)ceilf(fmaxf(ax, fmaxf(bx, cx)));
        int y0 = (int)floorf(fminf(ay, fminf(by, cy))), y1 = (int)ceilf(fmaxf(ay, fmaxf(by, cy)));
        for (int py = y0 < 0 ? 0 : y0; py < y1 && py < page->height; py++) {
            for (int px = x0 < 0 ? 0 : x0; px < x1 && px < page->width; px++) {
                float x = px + 0.5f, y = py + 0.5f;
                float wb = ((x - ax) * (cy - ay) - (cx - ax) * (y - ay)) / area;
                float wc = ((bx - ax) * (y - ay) - (x - ax) * (by - ay)) / area;
                float wa = 1 - wb - wc;
                if (wa < -1e-4f || wb < -1e-4f || wc < -1e-4f) continue;
                patch_pixel(page, px, py, patch, wa * t[0] + wb * t[4] + wc * t[8], wa * t[1] + wb * t[5] + wc * t[9]);
            }
        }
    }
}

int ssr_atlas_patch(SSRImage *page, const SSRAtlasImage *image, const SSRImage *patch) {
    if (!page || !image || !patch) return 0;
    if (image->kind != SSR_ATLAS_PLAIN && image->kind != SSR_ATLAS_STRIPPED) return 0;
    if (patch->width != image->originalWidth || patch->height != image->originalHeight) return 0;
    if (image->kind == SSR_ATLAS_STRIPPED) {
        patch_stripped(page, image, patch);
        return 1;
    }
    const float *r = image->rect;
    float u0 = r[2], v0 = r[3], u1 = r[6], v1 = r[7];
    if (u0 == u1 || v0 == v1) return 0;
    int px0 = (int)floorf(fminf(u0, u1) * page->width), px1 = (int)ceilf(fmaxf(u0, u1) * page->width);
    int py0 = (int)floorf(fminf(v0, v1) * page->height), py1 = (int)ceilf(fmaxf(v0, v1) * page->height);
    for (int py = py0 < 0 ? 0 : py0; py < py1 && py < page->height; py++) {
        // The centre of the page pixel, as a part of the box.
        float by = r[1] + ((py + 0.5f) / page->height - v0) / (v1 - v0) * (r[5] - r[1]);
        int sy = (int)floorf(by * patch->height);
        if (sy < 0 || sy >= patch->height) continue;
        for (int px = px0 < 0 ? 0 : px0; px < px1 && px < page->width; px++) {
            float bx = r[0] + ((px + 0.5f) / page->width - u0) / (u1 - u0) * (r[4] - r[0]);
            int sx = (int)floorf(bx * patch->width);
            if (sx < 0 || sx >= patch->width) continue;
            const unsigned char *from = patch->pixels + ((size_t)sy * patch->width + sx) * 4;
            if (from[3] > 0) memcpy(page->pixels + ((size_t)py * page->width + px) * 4, from, 4);
        }
    }
    return 1;
}
