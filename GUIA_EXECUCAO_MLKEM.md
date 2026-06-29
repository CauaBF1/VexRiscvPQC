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
- toolchain bare-metal RISC-V com suporte a `rv32i/ilp32` (Usar `./configure --prefix=/opt/riscv-gnu-toolchain --enable-multilib --enable-qemu-system`)

A toolchain usada atualmente no Makefile é:

```text
/opt/riscv-gnu-toolchain/bin/riscv64-unknown-elf-gcc
```

O Makefile usa por padrão:

```make
RISCV_NAME ?= riscv64-unknown-elf
RISCV_PATH ?= /opt/riscv-gnu-toolchain
MARCH := rv32i_zicsr
MABI := ilp32
```

Para conferir se a toolchain tem o multilib necessário:

```bash
riscv64-unknown-elf-gcc --print-multi-lib
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

## Rodar em FPGA

### Como rodar usando o vlab com LiteX/Tang Primer 20K

Este fluxo é separado do alvo Murax/DE10-Standard. Ele usa o LiteX no diretório `litex/`, a placa **Sipeed Tang Primer 20K** e imprime o KAT/benchmark pela UART.

Entre no repositório, ative o ambiente Python e vá para o checkout do LiteX:

```bash
cd ~/VexRiscvPQC
source litex-env/bin/activate
cd litex
```

Configure o `PYTHONPATH` para esse diretório. Como o `cd` está em `~/VexRiscvPQC/litex`, use exatamente:

```bash
export PYTHONPATH=$PWD/litex:$PWD/litex-boards
```

Teste se os imports do LiteX estão corretos:

```bash
python3 -c "from litex import get_data_mod, RemoteClient; print('ok')"
```

Em um terminal separado, abra a UART. A porta validada no vlab foi `/dev/ttyUSB1`:

```bash
python3 -m litex.tools.litex_term /dev/ttyUSB1 --speed 115200
```

Se o `litex_term` corromper a saída ou perder conexão durante reset/load, use modo raw:

```bash
python3 -m serial.tools.miniterm /dev/ttyUSB1 115200 --raw
```

No terminal de build/load, configure o Gowin:

```bash
export GOWIN_HOME=/var/local/Gowin_V1.9.10.03_Education_linux/IDE
export PATH=$GOWIN_HOME/bin:$PATH
export LD_PRELOAD=/lib/x86_64-linux-gnu/libfreetype.so.6
```

Opcionalmente confira se o `gw_sh` está no `PATH`:

```bash
which gw_sh
```

Limpe o build anterior da Tang Primer 20K:

```bash
rm -rf build/sipeed_tang_primer_20k
```

Compile e carregue o bitstream:

```bash
python3 -m litex_boards.targets.sipeed_tang_primer_20k \
  --cpu-type=vexriscv \
  --cpu-variant=minimal \
  --uart-name=serial \
  --bios-console=disable \
  --bios-lto \
  --bios-no-ansi \
  --bios-stack-margin=0x4000 \
  --integrated-rom-size=0xc000 \
  --integrated-sram-size=0x8000 \
  --integrated-main-ram-size=0x100 \
  --build \
  --load
```

Observações importantes:

- Cada terminal SSH tem ambiente separado. Exporte o `PYTHONPATH` no terminal da UART e no terminal do build, se os dois forem usados.
- `--integrated-main-ram-size=0x100` evita ativar a DDR/LiteDRAM do alvo LiteX. Usar `0x0` ativa o caminho com DRAM.
- `--integrated-sram-size=0x8000` deixa margem suficiente para o ML-KEM-512 e para a stack no Tang Primer 20K.
- A saída esperada pela UART deve terminar com `KAT PASS`.

Exemplo de métricas esperadas, com clock de 48 MHz:

```text
[MLKEM] bench_clock_hz=48000000
[MLKEM] bench_keypair_cycles=...
[MLKEM] bench_encaps_cycles=...
[MLKEM] bench_decaps_cycles=...
[MLKEM] KAT PASS
```

Para converter ciclos em tempo:

```text
tempo_segundos = cycles / 48000000
tempo_ms = cycles * 1000 / 48000000
```

Esperado na UART:

```text
[MLKEM] ML-KEM-512 KAT start
[MLKEM] keypair coins[0..3]=99 c8 fd b4
[MLKEM] enc coins[0..3]=b9 11 21 76
[MLKEM] bench_clock_hz=48000000
[MLKEM] status=0x000001
[MLKEM] status=0x000002
[MLKEM] keypair: ok
[MLKEM] bench_keypair_cycles=12639106
[MLKEM] status=0x000003
[MLKEM] encaps: ok
[MLKEM] bench_encaps_cycles=17816772
[MLKEM] status=0x000004
[MLKEM] decaps: ok
[MLKEM] bench_decaps_cycles=25247537
[MLKEM] status=0x000009
[MLKEM] ss self-match: ok
[MLKEM] status=0x000005
[MLKEM] pk match: ok
[MLKEM] status=0x000006
[MLKEM] sk match: ok
[MLKEM] status=0x000007
[MLKEM] ct match: ok
[MLKEM] status=0x000008
[MLKEM] ss match: ok
[MLKEM] status=0x00f00d
[MLKEM] KAT PASS

        __   _ __      _  __
       / /  (_) /____ | |/_/
      / /__/ / __/ -_)>  <
     /____/_/\__/\__/_/|_|

   Build your hardware, easily!

 (c) Copyright 2012-2026 Enjoy-Digital
 (c) Copyright 2007-2015 M-Labs

 BIOS built on Jun 29 2026 09:09:07
 BIOS CRC passed (4163b97e)

 LiteX git sha1: 2245d34a1

--================ SoC =================--
CPU:		VexRiscv_Min @ 48MHz
BUS:		wishbone 32-bit data/32-bit addr
CSR:		32-bit data big ordering
ROM:		48.0KiB
SRAM:		32.0KiB
MAIN RAM:	256B

--=========== Initialization ===========--
Memtest at 0x40000000 (256B)...
  Write: 0x40000000-0x40000100 256B   
   Read: 0x40000000-0x40000100 256B   
Memtest OK
Memspeed at 0x40000000 (Sequential, 256B)...
  Write speed: 23.3MiB/s
   Read speed: 21.8MiB/s
--================ Boot ================--
Booting from serial...
Press Q or ESC to abort boot completely.
sL5DdSMmkekro
Timeout
No boot medium found

--========= Done (No Console) ==========--


```

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
