#include "nsr_zipwriter.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void put16(FILE *file, uint16_t value) {
    fputc(value & 0xff, file);
    fputc(value >> 8, file);
}

static void put32(FILE *file, uint32_t value) {
    put16(file, (uint16_t)(value & 0xffff));
    put16(file, (uint16_t)(value >> 16));
}

// Packs data as raw deflate data with no zlib header, as a zip entry holds it.
static unsigned char *deflate_raw(const void *data, size_t size, size_t *packedSize) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return NULL;
    }
    uLong bound = deflateBound(&stream, (uLong)size);
    unsigned char *out = malloc(bound);
    if (!out) {
        deflateEnd(&stream);
        return NULL;
    }
    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = out;
    stream.avail_out = (uInt)bound;
    int status = deflate(&stream, Z_FINISH);
    *packedSize = stream.total_out;
    deflateEnd(&stream);
    if (status != Z_STREAM_END) {
        free(out);
        return NULL;
    }
    return out;
}

int nsr_zipwriter_write(const char *path, const NSRZipFile *files, int count) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    uint32_t *offsets = calloc((size_t)count + 1, sizeof(uint32_t));
    uint32_t *packedSizes = calloc((size_t)count + 1, sizeof(uint32_t));
    uint32_t *crcs = calloc((size_t)count + 1, sizeof(uint32_t));
    int ok = offsets && packedSizes && crcs;

    for (int i = 0; ok && i < count; i++) {
        const NSRZipFile *entry = &files[i];
        size_t packedSize = entry->size;
        unsigned char *packed = NULL;
        if (entry->deflate) {
            packed = deflate_raw(entry->data, entry->size, &packedSize);
            if (!packed) {
                ok = 0;
                break;
            }
        }
        offsets[i] = (uint32_t)ftell(file);
        packedSizes[i] = (uint32_t)packedSize;
        crcs[i] = (uint32_t)crc32(0, entry->data, (uInt)entry->size);
        uint16_t nameLength = (uint16_t)strlen(entry->name);

        put32(file, 0x04034b50);
        put16(file, 20);
        put16(file, 0);
        put16(file, entry->deflate ? 8 : 0);
        put32(file, 0);
        put32(file, crcs[i]);
        put32(file, packedSizes[i]);
        put32(file, (uint32_t)entry->size);
        put16(file, nameLength);
        put16(file, 0);
        fwrite(entry->name, 1, nameLength, file);
        fwrite(packed ? packed : entry->data, 1, packedSize, file);
        free(packed);
    }

    uint32_t directoryOffset = (uint32_t)ftell(file);
    for (int i = 0; ok && i < count; i++) {
        const NSRZipFile *entry = &files[i];
        uint16_t nameLength = (uint16_t)strlen(entry->name);
        put32(file, 0x02014b50);
        put16(file, 20);
        put16(file, 20);
        put16(file, 0);
        put16(file, entry->deflate ? 8 : 0);
        put32(file, 0);
        put32(file, crcs[i]);
        put32(file, packedSizes[i]);
        put32(file, (uint32_t)entry->size);
        put16(file, nameLength);
        put16(file, 0);
        put16(file, 0);
        put16(file, 0);
        put16(file, 0);
        put32(file, 0);
        put32(file, offsets[i]);
        fwrite(entry->name, 1, nameLength, file);
    }
    uint32_t directorySize = (uint32_t)ftell(file) - directoryOffset;

    put32(file, 0x06054b50);
    put16(file, 0);
    put16(file, 0);
    put16(file, (uint16_t)count);
    put16(file, (uint16_t)count);
    put32(file, directorySize);
    put32(file, directoryOffset);
    put16(file, 0);

    free(offsets);
    free(packedSizes);
    free(crcs);
    if (fclose(file) != 0) ok = 0;
    return ok;
}

const char *nsr_test_directory(const char *name) {
    static char path[1024];
    const char *base = getenv("TMPDIR");
    if (!base || !*base) base = "/tmp";
    snprintf(path, sizeof(path), "%s/nsr-test-%s-%ld", base, name, (long)rand());
    char command[1200];
    snprintf(command, sizeof(command), "rm -rf '%s' && mkdir -p '%s'", path, path);
    if (system(command) != 0) return NULL;
    return path;
}
