# ML-KEM-512 no Murax/VexRiscv

## Dependências necessárias

Instale ou tenha disponível:

- `git`
- `make`
- `python3`
- `sbt`
- Java compatível com SBT, por exemplo OpenJDK 17
- `verilator`
- `g++`
- toolchain bare-metal RISC-V com suporte a `rv32i/ilp32`

A toolchain usada atualmente no Makefile é:

```text
/opt/riscv-elf-multilib/bin/riscv64-unknown-elf-gcc
```

O Makefile usa por padrão:

```make
RISCV_NAME ?= riscv64-unknown-elf
RISCV_PATH ?= /home/borgescaua/opt/riscv-elf-multilib
MARCH := rv32i_zicsr
MABI := ilp32
```

Para conferir se a toolchain tem o multilib necessário:

```bash
riscv64-unknown-elf-gcc -print-multi-lib
```

Deve existir uma entrada compatível com `rv32i/ilp32`.

## Preparar o repositório

Depois de clonar ou dar `git pull`, inicialize o submodule do `mlkem-native`:

```bash
git submodule update --init --recursive
```

O algoritmo usado pelo firmware vem de:

```text
external/mlkem-native/
```

O diretório antigo `kyber_implementation/` não é mais usado.

## Executar o benchmark normal

Esse modo roda `keypair`, `encapsulation` e `decapsulation`, imprime prefixos de `pk`, `ct`, `ss1`, `ss2` e mede ciclos com `rdcycle`.

Compile o firmware:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=2 all
```

Gere o Verilog do Murax com o firmware embutido:

```bash
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
```

Compile o simulador Verilator:

```bash
make -B -C src/test/cpp/murax compile
```

Execute:

```bash
cd src/test/cpp/murax
./obj_dir/VMurax
```

Saída esperada, em formato resumido:

```text
BOOT
Murax ML-KEM-512 start
round=00000001
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
ss_match=0x00000001
pk_prefix=...
ct_prefix=...
ss1_prefix=...
ss2_prefix=...
cycles_keypair=0x...
cycles_enc=0x...
cycles_dec=0x...
done
```

`ss_match=0x00000001` indica que o shared secret encapsulado e o decapsulado bateram naquela rodada.

Para mudar o número de rodadas:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=30 all
```

## Executar o KAT

O KAT valida byte-a-byte contra vetores gerados pelo submodule `mlkem-native`. Ele não depende de PQClean.

Comando recomendado:

```bash
python3 scripts/run_mlkem512_kat.py
```

Esse script faz automaticamente:

- gera `src/main/c/murax/crystal_kyber/src/kat_vectors.h` a partir de `external/mlkem-native`;
- compila o firmware com `KAT=yes`;
- executa o SBT para gerar `Murax.v`;
- compila o Verilator;
- roda `VMurax`;
- encerra quando recebe `done`;
- valida a saída.

Saída esperada:

```text
BOOT
Murax ML-KEM-512 KAT start
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
pk_match=0x00000001
sk_match=0x00000001
ct_match=0x00000001
ss_match=0x00000001
kat_pass=0x00000001
done
KAT validation passed: kat_pass=0x00000001
```

`kat_pass=0x00000001` significa que todos os retornos foram sucesso e `pk`, `sk`, `ct` e `ss` bateram byte-a-byte com o vetor de referência.

## Comandos úteis

Limpar build do firmware:

```bash
make -C src/main/c/murax/crystal_kyber clean
```

Limpar build do Verilator:

```bash
make -C src/test/cpp/murax clean
```

Regenerar apenas os vetores KAT:

```bash
python3 scripts/generate_mlkem512_kat_vectors.py
```

Rodar o KAT sem regenerar os vetores:

```bash
python3 scripts/run_mlkem512_kat.py --skip-reference
```

Rodar o KAT usando o binário Verilator já compilado:

```bash
python3 scripts/run_mlkem512_kat.py --skip-reference --skip-build
```
