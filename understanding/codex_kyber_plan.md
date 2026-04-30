Below is a **Codex-ready execution plan**. It is written so Codex can **generate all required files, structure, and glue code** to run PQClean Kyber on a **VexRiscv (SpinalHDL) baremetal SoC**. No testing phase is included—only what must exist to run.

------

# 🧠 STRATEGIC OVERVIEW

You are not “running PQClean”—you are building a **full baremetal runtime + integration stack**.

Minimum system:

```
[SpinalHDL SoC (Murax/VexRiscv)]
        ↓
[Boot + runtime (crt0 + linker)]
        ↓
[Kyber (PQClean)]
        ↓
[HAL: RNG + SHA3 + IO]
```

------

# 🎯 OBJECTIVE FOR CODEX

Generate a **complete minimal firmware environment** that:

1. boots on VexRiscv (baremetal)
2. links PQClean Kyber
3. runs one KEM flow
4. requires no OS / libc

------

# 📦 TASK BREAKDOWN FOR CODEX

## 1. Project Structure

Codex should create:

```
kyber-vexriscv/
├── hw/                    # optional spinal integration
├── sw/
│   ├── src/
│   │   ├── main.c
│   │   ├── startup.S
│   │   ├── linker.ld
│   │   ├── hal/
│   │   │   ├── rng.c
│   │   │   ├── uart.c
│   │   │   ├── platform.h
│   │   ├── crypto/
│   │   │   ├── kyber/     # PQClean extracted
│   │   │   ├── sha3/      # fips202
│   ├── Makefile
```

------

## 2. CPU / SoC assumptions (must be explicit)

Codex must assume:

- RV32IM (VexRiscv typical config) VexRiscv
- no OS
- memory mapped RAM
- UART at known address

VexRiscv:

- plugin-based CPU architecture (important for extensions later) ([GitHub](https://github.com/spinalhdl/vexriscv?utm_source=chatgpt.com))
- supports baremetal execution ([SpinalHDL](https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Introduction/index.html?utm_source=chatgpt.com))

------

## 3. Startup code (MANDATORY)

Codex must generate:

### `startup.S`

Responsibilities:

- set stack pointer
- zero `.bss`
- copy `.data`
- jump to `main`

Example requirements:

```asm
- define _start
- initialize sp from linker symbol
- call main
- infinite loop after return
```

------

## 4. Linker Script

### `linker.ld`

Must define:

- FLASH / RAM regions
- `.text`, `.data`, `.bss`, `.stack`

Critical constraint:

- Kyber uses large buffers → stack must be explicit

------

## 5. PQClean Integration

Codex must:

### Step A — extract Kyber

Copy only:

```
crypto_kem/kyber512/clean/
```

### Step B — namespace compatibility

Keep PQClean naming (no modification)

### Step C — include dependencies

From PQClean common:

- `fips202.c` (SHA3)
- `randombytes.h` (replace impl)

PQClean explicitly requires you to provide:

- RNG
- SHA3/AES if needed ([GitHub](https://github.com/pqclean/PQClean?utm_source=chatgpt.com))

------

## 6. HAL (Hardware Abstraction Layer)

Codex must implement:

------

### (A) RNG

File: `hal/rng.c`

Minimal version:

```c
int randombytes(uint8_t *buf, size_t len) {
    static uint32_t seed = 0x12345678;
    for (size_t i = 0; i < len; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        buf[i] = seed & 0xFF;
    }
    return 0;
}
```

Purpose:

- deterministic → acceptable for now

------

### (B) UART (debug visibility)

File: `hal/uart.c`

Functions:

```c
void uart_putc(char c);
void uart_print(const char *s);
```

Mapped to Murax UART base address

------

### (C) platform.h

Defines:

```c
#define UART_BASE 0xXXXXXXXX
#define RAM_BASE  0xXXXXXXXX
```

------

## 7. Main program

Codex must generate:

### `main.c`

```c
int main() {
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss1[CRYPTO_BYTES];
    uint8_t ss2[CRYPTO_BYTES];

    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss1, pk);
    crypto_kem_dec(ss2, ct, sk);

    while(1);
}
```

NO printf, NO malloc

------

## 8. Build System

### `Makefile`

Must:

- use `riscv64-unknown-elf-gcc`
- flags:

```bash
-march=rv32im
-mabi=ilp32
-O2
-ffreestanding
-nostdlib
```

- include:
  - startup.S
  - PQClean files
  - SHA3
  - HAL

Output:

```
kyber.elf
kyber.bin
```

------

## 9. Memory + Constraints (critical for Codex)

Codex must respect:

### Stack risk

Kyber is heavy:

- large polynomial buffers
- deep call chains

👉 enforce:

- static allocation where possible

------

### Performance insight (important context)

Kyber bottlenecks:

- NTT
- Keccak (SHA3 dominates runtime) ([cs.ru.nl](https://www.cs.ru.nl/masters-theses/2020/D_Greconici___KYBER_on_RISC-V.pdf?utm_source=chatgpt.com))

👉 no optimization needed now, but architecture must allow it later

------

## 10. Optional (but high-value for future)

Codex can scaffold:

- cycle counter read (`mcycle`)
- GPIO toggle for profiling
- hooks for future hardware accelerators

------

# ⚠️ RISKS CODEX MUST HANDLE

## 1. Missing libc

→ must not use:

- printf
- malloc
- memset (unless implemented)

------

## 2. Alignment issues

→ use `uint32_t` aligned buffers

------

## 3. Large binary size

→ remove unused PQClean files

------

## 4. Toolchain mismatch

→ enforce RV32 build flags

------

# 🚀 FINAL INSTRUCTION FOR CODEX

Codex should:

1. create full project structure
2. integrate PQClean Kyber clean version
3. implement minimal HAL (RNG + UART)
4. generate startup + linker
5. produce working ELF for VexRiscv

------

# 📊 STRATEGIC EVALUATION

### Strengths of this plan

- minimal complexity
- reproducible
- aligned with FPGA constraints

### Weaknesses

- no security (weak RNG)
- poor performance (software SHA3)

### Opportunities

- hardware SHA3 accelerator
- custom VexRiscv plugin (NTT)

### Risks

- stack overflow
- memory exhaustion
- integration friction with Murax bus

------

# NEXT HIGH-IMPACT STEP

After Codex generates this:

→ integrate **cycle benchmarking (mcycle CSR)**
→ this directly supports your IC deliverable (performance evaluation)

If needed, next step can be:

- Murax memory map + UART address extraction
- or VexRiscv plugin for accelerating Kyber (NTT or SHA3)