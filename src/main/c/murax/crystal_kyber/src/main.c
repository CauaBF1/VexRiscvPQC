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
#ifdef FPGA_LED_ONLY
  (void)str;
#else
  while (*str) {
    uart_write(UART, *str);
    str++;
  }
#endif
}

static void println(const char *str) {
  print(str);
#ifndef FPGA_LED_ONLY
  uart_write(UART, '\n');
#endif
}

static void print_hex32(uint32_t value) {
#ifdef FPGA_LED_ONLY
  (void)value;
#else
  for (int i = 7; i >= 0; i--) {
    uint32_t digit = (value >> (i * 4)) & 0xF;
    uart_write(UART, digit < 10 ? ('0' + digit) : ('A' + digit - 10));
  }
#endif
}

static void print_hex8(uint8_t value) {
#ifdef FPGA_LED_ONLY
  (void)value;
#else
  uint8_t high = (value >> 4) & 0xF;
  uint8_t low = value & 0xF;

  uart_write(UART, high < 10 ? ('0' + high) : ('A' + high - 10));
  uart_write(UART, low < 10 ? ('0' + low) : ('A' + low - 10));
#endif
}

static void print_bytes_prefix(const char *label, const uint8_t *buffer, uint32_t count) {
  print(label);
  print("=");

  for (uint32_t i = 0; i < count; i++) {
    print_hex8(buffer[i]);
  }

#ifndef FPGA_LED_ONLY
  uart_write(UART, '\n');
#endif
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
#ifndef FPGA_LED_ONLY
  uart_write(UART, '\n');
#endif
}

static void print_cycles(const char *label, uint32_t cycles) {
  print(label);
  print("=0x");
  print_hex32(cycles);
#ifndef FPGA_LED_ONLY
  uart_write(UART, '\n');
#endif
}

static void run_round(uint32_t round) {
  print("round=");
  print_hex32(round);
#ifndef FPGA_LED_ONLY
  uart_write(UART, '\n');
#endif

  GPIO_A->OUTPUT = 0x00000002;
  uint32_t start_keypair = read_cycle();
  int keypair_ret = mlkem_keypair(pk, sk);
  uint32_t end_keypair = read_cycle();

  GPIO_A->OUTPUT = 0x00000004;
  uint32_t start_enc = read_cycle();
  int enc_ret = mlkem_enc(ct, ss1, pk);
  uint32_t end_enc = read_cycle();

  GPIO_A->OUTPUT = 0x00000008;
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

  GPIO_A->OUTPUT = (keypair_ret == 0 && enc_ret == 0 && dec_ret == 0 && shared_secret_matches())
                         ? 0x0000000F
                         : 0x00000008;
}

void main(void) {
  GPIO_A->OUTPUT_ENABLE = 0x0000000F;
  GPIO_A->OUTPUT = 0x00000001;

  println("Murax ML-KEM-512 start");

  for (uint32_t round = 1; round <= BENCH_ROUNDS; round++) {
    run_round(round);
  }

  println("done");

#ifdef FPGA_LED_ONLY
  while (1) {
    asm volatile("" ::: "memory");
  }
#else
  while (1) {
    GPIO_A->OUTPUT ^= 0x0000000F;
    for (volatile uint32_t i = 0; i < 200000; i++) {
      asm volatile("" ::: "memory");
    }
  }
#endif
}

void irqCallback(void) {
}
