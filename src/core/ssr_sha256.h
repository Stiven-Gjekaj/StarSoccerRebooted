// ssr_sha256: the SHA-256 of a buffer (FIPS 180-4). The pack browser checks
// a download with it.

#ifndef SSR_SHA256_H
#define SSR_SHA256_H

#include <stddef.h>

// Writes the SHA-256 of the data as 64 small hex digits and a 0 into hex.
void ssr_sha256_hex(const void *data, size_t size, char hex[65]);

#endif
