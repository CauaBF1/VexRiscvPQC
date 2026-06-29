# Integração ML-KEM-512 no LiteX para Sipeed Tang Primer 20K

Esta pasta contém os arquivos necessários para adicionar o teste/benchmark ML-KEM-512 ao BIOS do LiteX e executá-lo na **Sipeed Tang Primer 20K**.

O fluxo validado executa o ML-KEM-512 automaticamente no boot do BIOS e imprime o resultado pela UART:

```text
[MLKEM] keypair: ok
[MLKEM] encaps: ok
[MLKEM] decaps: ok
[MLKEM] KAT PASS
```

Também imprime métricas de ciclos usando o `timer0` do LiteX.

## Estrutura desta pasta

```text
litex_sipeed_mlkem_integration/
├── README.md
├── bios/
│   ├── Makefile
│   ├── kat_vectors.h
│   ├── mlkem_litex.c
│   └── mlkem_native_litex_config.h
├── patches/
│   └── main.c.patch
└── scripts/
    └── install_into_litex.sh
```

Arquivos principais:

- `bios/mlkem_litex.c`: executa `keypair`, `encaps`, `decaps`, valida KAT e imprime benchmark pela UART.
- `bios/mlkem_native_litex_config.h`: configuração bare-metal do `mlkem-native` para LiteX/RISC-V.
- `bios/kat_vectors.h`: vetores KAT ML-KEM-512 usados para validação byte-a-byte.
- `bios/Makefile`: Makefile do BIOS já modificado para compilar `mlkem_litex.o` e `mlkem_native.o`.
- `patches/main.c.patch`: mostra onde chamar `litex_mlkem_kat_status()` no boot do BIOS.

## Pré-requisitos

No repositório principal, o `mlkem-native` precisa existir em:

```text
external/mlkem-native/
```

Se necessário:

```bash
git submodule update --init --recursive
```

Também é necessário ter:

- Python 3 e virtualenv;
- toolchain RISC-V usada pelo LiteX;
- Gowin EDA instalado;
- `openFPGALoader`;
- acesso à placa via FTDI/JTAG;
- acesso à UART, normalmente `/dev/ttyUSB1` no vlab.

## Baixar/preparar o LiteX

A estrutura validada foi:

```text
~/VexRiscvPQC/
├── external/mlkem-native/
├── litex/
│   ├── litex/
│   ├── litex-boards/
│   ├── migen/
│   └── pythondata-*/
└── litex-env/
```

Se o diretório `litex/` ainda não existir, um fluxo típico é:

```bash
cd ~/VexRiscvPQC
python3 -m venv litex-env
source litex-env/bin/activate
mkdir -p litex
cd litex
wget https://raw.githubusercontent.com/enjoy-digital/litex/master/litex_setup.py
python3 litex_setup.py --init --install
```

Se o vlab já tem o diretório `litex/` pronto, apenas ative o ambiente:

```bash
cd ~/VexRiscvPQC
source litex-env/bin/activate
cd litex
```

Configure o `PYTHONPATH` estando dentro de `~/VexRiscvPQC/litex`:

```bash
export PYTHONPATH=$PWD/litex:$PWD/litex-boards
```

Teste os imports:

```bash
python3 -c "from litex import get_data_mod, RemoteClient; print('ok')"
```

## Instalar os arquivos no LiteX

Estando em `~/VexRiscvPQC`, copie os arquivos para o BIOS do LiteX:

```bash
cp litex_sipeed_mlkem_integration/bios/Makefile \
  litex/litex/litex/soc/software/bios/Makefile

cp litex_sipeed_mlkem_integration/bios/mlkem_litex.c \
  litex/litex/litex/soc/software/bios/mlkem_litex.c

cp litex_sipeed_mlkem_integration/bios/mlkem_native_litex_config.h \
  litex/litex/litex/soc/software/bios/mlkem_native_litex_config.h

cp litex_sipeed_mlkem_integration/bios/kat_vectors.h \
  litex/litex/litex/soc/software/bios/kat_vectors.h
```

Alternativamente, use o script:

```bash
litex_sipeed_mlkem_integration/scripts/install_into_litex.sh ~/VexRiscvPQC/litex
```

O script copia os arquivos do BIOS, mas ainda exige que o `main.c` já contenha a chamada ao ML-KEM.

## Alterar o `main.c` do BIOS

Edite:

```text
litex/litex/litex/soc/software/bios/main.c
```

Adicione a declaração perto dos includes globais:

```c
int litex_mlkem_kat_status(void);
```

Depois de `uart_init();`, chame o teste ML-KEM:

```c
#ifdef CSR_UART_BASE
	uart_init();
#endif

	(void)litex_mlkem_kat_status();
```

O patch de referência está em:

```text
litex_sipeed_mlkem_integration/patches/main.c.patch
```

## Configurar Gowin no vlab

No terminal de build/load:

```bash
export GOWIN_HOME=/var/local/Gowin_V1.9.10.03_Education_linux/IDE
export PATH=$GOWIN_HOME/bin:$PATH
export LD_PRELOAD=/lib/x86_64-linux-gnu/libfreetype.so.6
```

Teste:

```bash
which gw_sh
```

## Abrir UART

Em outro terminal SSH:

```bash
cd ~/VexRiscvPQC/litex
source ../litex-env/bin/activate
export PYTHONPATH=$PWD/litex:$PWD/litex-boards
```

Abra a UART:

```bash
python3 -m litex.tools.litex_term /dev/ttyUSB1 --speed 115200
```

Se o `litex_term` corromper a saída, use modo raw:

```bash
python3 -m serial.tools.miniterm /dev/ttyUSB1 115200 --raw
```

## Build e load para Sipeed Tang Primer 20K

No terminal de build/load:

```bash
cd ~/VexRiscvPQC/litex
source ../litex-env/bin/activate
export PYTHONPATH=$PWD/litex:$PWD/litex-boards

export GOWIN_HOME=/var/local/Gowin_V1.9.10.03_Education_linux/IDE
export PATH=$GOWIN_HOME/bin:$PATH
export LD_PRELOAD=/lib/x86_64-linux-gnu/libfreetype.so.6

rm -rf build/sipeed_tang_primer_20k

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

## Configurações usadas

Configuração validada:

```text
CPU: VexRiscv_Min
Clock: 48 MHz
ROM: 48 KiB
SRAM: 32 KiB
MAIN RAM: 256 B
UART: serial, 115200 baud
BIOS console: disable
BIOS LTO: enabled
```

Opções importantes:

- `--cpu-variant=minimal`: cabe no Tang Primer 20K.
- `--integrated-sram-size=0x8000`: 32 KiB de SRAM, necessário para evitar falha por stack/memória no ML-KEM.
- `--integrated-main-ram-size=0x100`: evita ativar DDR/LiteDRAM no target LiteX.
- `--bios-stack-margin=0x4000`: exige margem de stack adequada no link do BIOS.
- `--bios-console=disable`: reduz uso de ROM/SRAM, mas o ML-KEM ainda imprime antes do final do BIOS.

## Configuração do mlkem-native

O arquivo `mlkem_native_litex_config.h` usa:

```c
#define MLK_CONFIG_PARAMETER_SET 512
#define MLK_CONFIG_NAMESPACE_PREFIX mlkem
#define MLK_CONFIG_INTERNAL_API_QUALIFIER static
#define MLK_CONFIG_NO_ASM
#define MLK_CONFIG_NO_RANDOMIZED_API
#define MLK_CONFIG_CUSTOM_ZEROIZE
```

Motivos:

- `MLK_CONFIG_PARAMETER_SET 512`: seleciona ML-KEM-512.
- `MLK_CONFIG_NO_ASM`: usa C puro, adequado ao RISC-V bare-metal do LiteX.
- `MLK_CONFIG_NO_RANDOMIZED_API`: remove APIs com RNG interno; o KAT usa entrada determinística via `coins`.
- `MLK_CONFIG_CUSTOM_ZEROIZE`: necessário porque `NO_ASM` desativa a implementação padrão de zeroização.

## Saída esperada

Exemplo resumido:

```text
[MLKEM] ML-KEM-512 KAT start
[MLKEM] keypair: ok
[MLKEM] bench_keypair_cycles=12639106
[MLKEM] encaps: ok
[MLKEM] bench_encaps_cycles=17816772
[MLKEM] decaps: ok
[MLKEM] bench_decaps_cycles=25247537
[MLKEM] ss self-match: ok
[MLKEM] pk match: ok
[MLKEM] sk match: ok
[MLKEM] ct match: ok
[MLKEM] ss match: ok
[MLKEM] KAT PASS
```

Com clock de 48 MHz, os valores medidos foram aproximadamente:

```text
keypair: 12,639,106 ciclos = 263.31 ms
encaps:  17,816,772 ciclos = 371.18 ms
decaps:  25,247,537 ciclos = 525.99 ms
```

## Problemas comuns

### ImportError: cannot import name get_data_mod ou RemoteClient

O `PYTHONPATH` está errado. Se você está em `~/VexRiscvPQC/litex`, use:

```bash
export PYTHONPATH=$PWD/litex:$PWD/litex-boards
```

Não use `$PWD/litex/litex` se já estiver dentro da pasta `litex/`.

### KAT falha com SRAM menor

Use:

```text
--integrated-sram-size=0x8000
```

SRAM menor pode causar corrupção de memória/stack durante `decaps`.

### `rdcycle` retorna zero

A variante `VexRiscv_Min` pode não expor contador de ciclos. Por isso o benchmark usa `timer0` do LiteX.

### `litex_term` perde conexão ou corrompe saída

Use:

```bash
python3 -m serial.tools.miniterm /dev/ttyUSB1 115200 --raw
```
