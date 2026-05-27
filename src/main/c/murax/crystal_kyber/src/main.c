#include <stdint.h>

#include "mlkem_native.h"
#include "murax.h"

#ifndef BENCH_ROUNDS
#define BENCH_ROUNDS 2u
#endif

static uint8_t pk[MLKEM512_PUBLICKEYBYTES];
static uint8_t sk[MLKEM512_SECRETKEYBYTES];
static uint8_t ct[MLKEM512_CIPHERTEXTBYTES];
static uint8_t ss1[MLKEM512_BYTES];
static uint8_t ss2[MLKEM512_BYTES];

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

static void print_hex8(uint8_t value) {
  uint8_t high = (value >> 4) & 0xF;
  uint8_t low = value & 0xF;

  uart_write(UART, high < 10 ? ('0' + high) : ('A' + high - 10));
  uart_write(UART, low < 10 ? ('0' + low) : ('A' + low - 10));
}

static void print_bytes_prefix(const char *label, const uint8_t *buffer, uint32_t count) {
  print(label);
  print("=");

  for (uint32_t i = 0; i < count; i++) {
    print_hex8(buffer[i]);
  }

  uart_write(UART, '\n');
}

static inline uint32_t read_cycle(void) {
  uint32_t value;
  asm volatile("rdcycle %0" : "=r"(value));
  return value;
}

static int shared_secret_matches(void) {
  for (uint32_t i = 0; i < MLKEM512_BYTES; i++) {
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

static void run_round(uint32_t round) {
  print("round=");
  print_hex32(round);
  uart_write(UART, '\n');

  uint32_t start_keypair = read_cycle();
  int keypair_ret = mlkem_keypair(pk, sk);
  uint32_t end_keypair = read_cycle();

  uint32_t start_enc = read_cycle();
  int enc_ret = mlkem_enc(ct, ss1, pk);
  uint32_t end_enc = read_cycle();

  uint32_t start_dec = read_cycle();
  int dec_ret = mlkem_dec(ss2, ct, sk);
  uint32_t end_dec = read_cycle();

  print_result("keypair_ret", keypair_ret);
  print_result("enc_ret", enc_ret);
  print_result("dec_ret", dec_ret);
  print_result("ss_match", shared_secret_matches());

  print_bytes_prefix("pk_prefix", pk, 8);
  print_bytes_prefix("ct_prefix", ct, 8);
  print_bytes_prefix("ss1_prefix", ss1, 8);
  print_bytes_prefix("ss2_prefix", ss2, 8);

  print_cycles("cycles_keypair", end_keypair - start_keypair);
  print_cycles("cycles_enc", end_enc - start_enc);
  print_cycles("cycles_dec", end_dec - start_dec);
}

void main(void) {
  GPIO_A->OUTPUT_ENABLE = 0x0000000F;
  GPIO_A->OUTPUT = 0x00000001;

  println("Murax ML-KEM-512 start");

  for (uint32_t round = 1; round <= BENCH_ROUNDS; round++) {
    run_round(round);
  }

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
