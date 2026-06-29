#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <generated/csr.h>

#include "kat_vectors.h"
#include "mlkem_native.h"

int litex_mlkem_kat_status(void);

static uint8_t pk[MLKEM512_PUBLICKEYBYTES];
static uint8_t sk[MLKEM512_SECRETKEYBYTES];
static uint8_t ct[MLKEM512_CIPHERTEXTBYTES];
static uint8_t ss1[MLKEM512_BYTES];
static uint8_t ss2[MLKEM512_BYTES];

static int bytes_equal(const uint8_t *lhs, const uint8_t *rhs, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (lhs[i] != rhs[i])
			return 0;
	}
	return 1;
}

static size_t first_diff(const uint8_t *lhs, const uint8_t *rhs, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (lhs[i] != rhs[i])
			return i;
	}
	return n;
}

static void report_match(const char *name, const uint8_t *got,
                         const uint8_t *expected, size_t n)
{
	size_t diff = first_diff(got, expected, n);

	printf("[MLKEM] %s match: %s\n", name, diff == n ? "ok" : "fail");
	if (diff != n) {
		printf("[MLKEM] %s first_diff=%lu got=0x%02x exp=0x%02x\n",
		       name,
		       (unsigned long)diff,
		       got[diff],
		       expected[diff]);
	}
}

static void status(uint32_t value)
{
#ifdef CSR_HEX_STATUS_BASE
	hex_status_value_write(value & 0x00ffffffu);
#endif
	printf("[MLKEM] status=0x%06lx\n", (unsigned long)(value & 0x00ffffffu));
}

static void benchmark_timer_start(void)
{
	timer0_en_write(0);
	timer0_reload_write(0);
	timer0_load_write(0xffffffffu);
	timer0_en_write(1);
}

static uint32_t benchmark_timer_value(void)
{
	timer0_update_value_write(1);
	return timer0_value_read();
}

static uint32_t benchmark_timer_stop(void)
{
	uint32_t value = benchmark_timer_value();
	timer0_en_write(0);
	return value;
}

static void report_cycles(const char *name, uint32_t cycles)
{
	printf("[MLKEM] bench_%s_cycles=%lu\n", name, (unsigned long)cycles);
}

int litex_mlkem_kat_status(void)
{
	int pass = 1;

	printf("[MLKEM] ML-KEM-512 KAT start\n");
	printf("[MLKEM] keypair coins[0..3]=%02x %02x %02x %02x\n",
	       kat_keypair_coins[0], kat_keypair_coins[1],
	       kat_keypair_coins[2], kat_keypair_coins[3]);
	printf("[MLKEM] enc coins[0..3]=%02x %02x %02x %02x\n",
	       kat_enc_coins[0], kat_enc_coins[1],
	       kat_enc_coins[2], kat_enc_coins[3]);
	printf("[MLKEM] bench_clock_hz=%lu\n", (unsigned long)CONFIG_CLOCK_FREQUENCY);

	status(0x000001); /* start */
	benchmark_timer_start();
	uint32_t start_keypair = benchmark_timer_value();
	int keypair_ret = mlkem_keypair_derand(pk, sk, kat_keypair_coins);
	uint32_t cycles_keypair = start_keypair - benchmark_timer_stop();
	status(keypair_ret == 0 ? 0x000002 : 0xee0002);
	printf("[MLKEM] keypair: %s\n", keypair_ret == 0 ? "ok" : "fail");
	report_cycles("keypair", cycles_keypair);

	benchmark_timer_start();
	uint32_t start_enc = benchmark_timer_value();
	int enc_ret = mlkem_enc_derand(ct, ss1, pk, kat_enc_coins);
	uint32_t cycles_enc = start_enc - benchmark_timer_stop();
	status(enc_ret == 0 ? 0x000003 : 0xee0003);
	printf("[MLKEM] encaps: %s\n", enc_ret == 0 ? "ok" : "fail");
	report_cycles("encaps", cycles_enc);

	benchmark_timer_start();
	uint32_t start_dec = benchmark_timer_value();
	int dec_ret = mlkem_dec(ss2, ct, sk);
	uint32_t cycles_dec = start_dec - benchmark_timer_stop();
	status(dec_ret == 0 ? 0x000004 : 0xee0004);
	printf("[MLKEM] decaps: %s\n", dec_ret == 0 ? "ok" : "fail");
	report_cycles("decaps", cycles_dec);

	int ss_self_match = bytes_equal(ss1, ss2, sizeof(ss1));
	status(ss_self_match ? 0x000009 : 0xee0009);
	printf("[MLKEM] ss self-match: %s\n", ss_self_match ? "ok" : "fail");
	if (!ss_self_match)
		report_match("ss self", ss1, ss2, sizeof(ss1));

	int pk_match = bytes_equal(pk, kat_pk, sizeof(pk));
	status(pk_match ? 0x000005 : 0xee0005);
	report_match("pk", pk, kat_pk, sizeof(pk));

	int sk_match = bytes_equal(sk, kat_sk, sizeof(sk));
	status(sk_match ? 0x000006 : 0xee0006);
	report_match("sk", sk, kat_sk, sizeof(sk));

	int ct_match = bytes_equal(ct, kat_ct, sizeof(ct));
	status(ct_match ? 0x000007 : 0xee0007);
	report_match("ct", ct, kat_ct, sizeof(ct));

	int ss_match = bytes_equal(ss1, kat_ss, sizeof(ss1)) &&
	               bytes_equal(ss2, kat_ss, sizeof(ss2));
	status(ss_match ? 0x000008 : 0xee0008);
	report_match("ss", ss1, kat_ss, sizeof(ss1));

	pass &= keypair_ret == 0;
	pass &= enc_ret == 0;
	pass &= dec_ret == 0;
	pass &= ss_self_match;
	pass &= pk_match;
	pass &= sk_match;
	pass &= ct_match;
	pass &= ss_match;

	status(pass ? 0x00f00d : 0x00eeee);
	printf("[MLKEM] KAT %s\n", pass ? "PASS" : "FAIL");
	return pass;
}
