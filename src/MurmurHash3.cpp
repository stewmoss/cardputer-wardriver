// MurmurHash3 was written by Austin Appleby and placed in the public domain.
// The author hereby disclaims copyright to this source code.
//
// Adapted from the reference C++ implementation:
//   https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp

#include "MurmurHash3.h"
#include <cstring>

static inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

static inline uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static void MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out)
{
    const uint8_t *data = (const uint8_t *)key;
    const int nblocks = len / 16;

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    const uint32_t c1 = 0x239b961b;
    const uint32_t c2 = 0xab0e9789;
    const uint32_t c3 = 0x38b34ae5;
    const uint32_t c4 = 0xa1e38b93;

    // body
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 16);

    for (int i = -nblocks; i; i++)
    {
        uint32_t k1, k2, k3, k4;
        memcpy(&k1, &blocks[i * 4 + 0], sizeof(uint32_t));
        memcpy(&k2, &blocks[i * 4 + 1], sizeof(uint32_t));
        memcpy(&k3, &blocks[i * 4 + 2], sizeof(uint32_t));
        memcpy(&k4, &blocks[i * 4 + 3], sizeof(uint32_t));

        k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
        h1 = rotl32(h1, 19); h1 += h2; h1 = h1 * 5 + 0x561ccd1b;

        k2 *= c2; k2 = rotl32(k2, 16); k2 *= c3; h2 ^= k2;
        h2 = rotl32(h2, 17); h2 += h3; h2 = h2 * 5 + 0x0bcaa747;

        k3 *= c3; k3 = rotl32(k3, 17); k3 *= c4; h3 ^= k3;
        h3 = rotl32(h3, 15); h3 += h4; h3 = h3 * 5 + 0x96cd1c35;

        k4 *= c4; k4 = rotl32(k4, 18); k4 *= c1; h4 ^= k4;
        h4 = rotl32(h4, 13); h4 += h1; h4 = h4 * 5 + 0x32ac3b17;
    }

    // tail
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);
    uint32_t k1 = 0, k2 = 0, k3 = 0, k4 = 0;

    switch (len & 15)
    {
    case 15: k4 ^= (uint32_t)tail[14] << 16; // fall through
    case 14: k4 ^= (uint32_t)tail[13] << 8;  // fall through
    case 13: k4 ^= (uint32_t)tail[12] << 0;
        k4 *= c4; k4 = rotl32(k4, 18); k4 *= c1; h4 ^= k4; // fall through
    case 12: k3 ^= (uint32_t)tail[11] << 16; // fall through
    case 11: k3 ^= (uint32_t)tail[10] << 8;  // fall through
    case 10: k3 ^= (uint32_t)tail[9]  << 0;
        k3 *= c3; k3 = rotl32(k3, 17); k3 *= c4; h3 ^= k3; // fall through
    case  9: k2 ^= (uint32_t)tail[8]  << 16; // fall through
    case  8: k2 ^= (uint32_t)tail[7]  << 8;  // fall through
    case  7: k2 ^= (uint32_t)tail[6]  << 0;
        k2 *= c2; k2 = rotl32(k2, 16); k2 *= c3; h2 ^= k2; // fall through
    case  6: k1 ^= (uint32_t)tail[5]  << 16; // fall through
    case  5: k1 ^= (uint32_t)tail[4]  << 8;  // fall through
    case  4: k1 ^= (uint32_t)tail[3]  << 24; // fall through
    case  3: k1 ^= (uint32_t)tail[2]  << 16; // fall through
    case  2: k1 ^= (uint32_t)tail[1]  << 8;  // fall through
    case  1: k1 ^= (uint32_t)tail[0]  << 0;
        k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
    }

    // finalization
    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    h1 = fmix32(h1); h2 = fmix32(h2); h3 = fmix32(h3); h4 = fmix32(h4);
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    ((uint32_t *)out)[0] = h1;
    ((uint32_t *)out)[1] = h2;
    ((uint32_t *)out)[2] = h3;
    ((uint32_t *)out)[3] = h4;
}

uint64_t murmurHash3_64(const void *key, int len, uint32_t seed)
{
    uint32_t hash[4];
    MurmurHash3_x86_128(key, len, seed, hash);
    return ((uint64_t)hash[1] << 32) | (uint64_t)hash[0]; // lower 64 bits
}
