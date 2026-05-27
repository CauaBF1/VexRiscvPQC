#include <stddef.h>
#include <stdint.h>

#include "kat_vectors.h"
#include "mlkem_native.h"
#include "murax.h"


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

static int bytes_equal(const uint8_t *lhs, const uint8_t *rhs, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (lhs[i] != rhs[i]) {
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

static int run_kat(void) {
  int pass = 1;

  int keypair_ret = mlkem_keypair_derand(pk, sk, kat_keypair_coins);
  int enc_ret = mlkem_enc_derand(ct, ss1, pk, kat_enc_coins);
  int dec_ret = mlkem_dec(ss2, ct, sk);

  int pk_match = bytes_equal(pk, kat_pk, sizeof(pk));
  int sk_match = bytes_equal(sk, kat_sk, sizeof(sk));
  int ct_match = bytes_equal(ct, kat_ct, sizeof(ct));
  int ss_match = bytes_equal(ss1, kat_ss, sizeof(ss1)) && bytes_equal(ss2, kat_ss, sizeof(ss2));

  pass &= keypair_ret == 0;
  pass &= enc_ret == 0;
  pass &= dec_ret == 0;
  pass &= pk_match;
  pass &= sk_match;
  pass &= ct_match;
  pass &= ss_match;

  print_result("keypair_ret", keypair_ret);
  print_result("enc_ret", enc_ret);
  print_result("dec_ret", dec_ret);
  print_result("pk_match", pk_match);
  print_result("sk_match", sk_match);
  print_result("ct_match", ct_match);
  print_result("ss_match", ss_match);
  print_result("kat_pass", pass);

  return pass;
}

void main(void) {
  GPIO_A->OUTPUT_ENABLE = 0x0000000F;
  GPIO_A->OUTPUT = 0x00000001;

  println("Murax ML-KEM-512 KAT start");
  (void)run_kat();
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
