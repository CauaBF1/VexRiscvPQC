#include <stddef.h>
#include <stdint.h>

#include "randombytes.h"

static uint32_t rng_state = 0x12345678u;

int randombytes(uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++) {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
        buf[i] = (uint8_t)(rng_state & 0xFF);
    }
    return 0;
}
