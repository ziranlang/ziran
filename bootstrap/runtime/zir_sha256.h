#ifndef ZIR_SHA256_H
#define ZIR_SHA256_H

#include <stddef.h>
#include <stdint.h>

/* SHA-256 (FIPS 180-4), for content keys such as native code caches. */
typedef struct Sha256 {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} Sha256;

void Sha256Begin(Sha256 *hash);
void Sha256Add(Sha256 *hash, const void *data, size_t size);
void Sha256End(Sha256 *hash, unsigned char digest[32]);

#endif
