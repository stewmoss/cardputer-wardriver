#ifndef MURMURHASH3_H
#define MURMURHASH3_H

#include <stdint.h>

// MurmurHash3 was written by Austin Appleby and placed in the public domain.
// murmurHash3_64() returns the lower 64 bits of MurmurHash3_x86_128.
uint64_t murmurHash3_64(const void *key, int len, uint32_t seed);

#endif // MURMURHASH3_H
