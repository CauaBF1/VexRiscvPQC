#include <stdint.h>

#include "api.h"
#include "murax.h"

static uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];
static uint8_t ct[PQCLEAN_MLKEM512_CLEAN_CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss1[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];
static uint8_t ss2[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];

static void print(const char *str) {
  while (*str) {
    uart_write(UART, *str);
    str++;
  }
}

static void println(const char *str) {
  print(str);
  uart_write(UART, '\n');
}

static void print_hex32(uint32_t value) {
  for (int i = 7; i >= 0; i--) {
    uint32_t digit = (value >> (i * 4)) & 0xF;
    uart_write(UART, digit < 10 ? ('0' + digit) : ('A' + digit - 10));
  }
}

static inline uint32_t read_cycle(void) {
  uint32_t value;
  asm volatile("rdcycle %0" : "=r"(value));
  return value;
}

static int shared_secret_matches(void) {
  for (uint32_t i = 0; i < PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES; i++) {
    if (ss1[i] != ss2[i]) {
      return 0;
    }
  }

  return 1;
}

static void print_result(const char *label, int value) {
  print(label);
  print("=0x");
  print_hex32((uint32_t)value);
  uart_write(UART, '\n');
}

static void print_cycles(const char *label, uint32_t cycles) {
  print(label);
  print("=0x");
  print_hex32(cycles);
  uart_write(UART, '\n');
}

void main(void) {
  GPIO_A->OUTPUT_ENABLE = 0x0000000F;
  GPIO_A->OUTPUT = 0x00000001;

  println("Murax ML-KEM-512 start");

  uint32_t start_keypair = read_cycle();
  int keypair_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
  uint32_t end_keypair = read_cycle();

  uint32_t start_enc = read_cycle();
  int enc_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(ct, ss1, pk);
  uint32_t end_enc = read_cycle();

  uint32_t start_dec = read_cycle();
  int dec_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss2, ct, sk);
  uint32_t end_dec = read_cycle();

  print_result("keypair_ret", keypair_ret);
  print_result("enc_ret", enc_ret);
  print_result("dec_ret", dec_ret);
  print_result("ss_match", shared_secret_matches());

  print_cycles("cycles_keypair", end_keypair - start_keypair);
  print_cycles("cycles_enc", end_enc - start_enc);
  print_cycles("cycles_dec", end_dec - start_dec);

  println("done");

  while (1) {
    GPIO_A->OUTPUT ^= 0x0000000F;
    for (volatile uint32_t i = 0; i < 200000; i++) {
      asm volatile("" ::: "memory");
    }
  }
}

void irqCallback(void) {
}
