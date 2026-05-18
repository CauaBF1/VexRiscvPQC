#include <stddef.h>
#include <stdint.h>

#include "kat_vectors.h"
#include "randombytes.h"

static size_t kat_random_offset = 0;
static int kat_random_overflow = 0;

void kat_randombytes_reset(void) {
  kat_random_offset = 0;
  kat_random_overflow = 0;
}

int kat_randombytes_overflow(void) {
  return kat_random_overflow;
}

int randombytes(uint8_t *output, size_t n) {
  const size_t seed_size = sizeof(kat_seed);
  const size_t keypair_coins_size = sizeof(kat_keypair_coins);
  const size_t enc_coins_size = sizeof(kat_enc_coins);
  const size_t stream_size = seed_size + keypair_coins_size + enc_coins_size;

  for (size_t i = 0; i < n; i++) {
    if (kat_random_offset >= stream_size) {
      output[i] = 0;
      kat_random_overflow = 1;
    } else if (kat_random_offset < seed_size) {
      output[i] = kat_seed[kat_random_offset];
    } else if (kat_random_offset < seed_size + keypair_coins_size) {
      output[i] = kat_keypair_coins[kat_random_offset - seed_size];
    } else {
      output[i] = kat_enc_coins[kat_random_offset - seed_size - keypair_coins_size];
    }

    kat_random_offset++;
  }

  return kat_random_overflow ? -1 : 0;
}
