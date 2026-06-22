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

O fluxo FPGA implementado neste repositório é para a **DE10-Standard** usando o Murax/VexRiscv e saída por **LEDs**. Ele não usa UART para imprimir texto na placa.

Na FPGA, o firmware é compilado com:

```make
FPGA_LED_ONLY=yes
```

Com essa flag, as chamadas de impressão são desativadas no firmware. A validação do resultado é feita pelos LEDs `LEDR[3:0]`, conectados ao `GPIO_A[3:0]` do Murax.

Para obter saída textual com `pk_prefix`, `ct_prefix`, `ss_match` e ciclos, use a simulação Verilator descrita na seção "Executar o benchmark normal". A execução FPGA atual serve para confirmar visualmente que o firmware roda até o fim e que o ML-KEM terminou com sucesso.

### DE10-Standard com LEDs

O alvo da DE10-Standard está em:

```text
scripts/Murax/de10_standard/
```

Ele gera:

```text
scripts/Murax/de10_standard/output_files/MuraxDe10Standard.sof
```

O wrapper da placa conecta:

```text
CLOCK_50  -> clock principal do Murax
KEY0      -> reset assíncrono, ativo em nível baixo
LEDR[3:0] -> GPIO_A[3:0]
LEDR[9:4] -> 0
```

O UART do Murax não é exportado nesse alvo. O sinal `uart_rxd` fica preso em nível idle e `uart_txd` não é ligado a pino externo.

Antes de compilar, confira que o submodule do `mlkem-native` existe:

```bash
git submodule update --init --recursive
```

Compile o firmware, gere o RTL e rode a síntese/implementação no Quartus:

```bash
make -C scripts/Murax/de10_standard build \
  RISCV_PATH=/home/borgescaua/opt/riscv-elf-multilib \
  RISCV_NAME=riscv64-unknown-elf \
  BENCH_ROUNDS=1
```

Se a toolchain estiver em outro caminho, ajuste apenas `RISCV_PATH`. Por exemplo:

```bash
make -C scripts/Murax/de10_standard build \
  RISCV_PATH=/usr \
  RISCV_NAME=riscv64-unknown-elf \
  BENCH_ROUNDS=1
```

Se o Quartus não estiver no `PATH`, passe os binários explicitamente:

```bash
make -C scripts/Murax/de10_standard build \
  RISCV_PATH=/home/borgescaua/opt/riscv-elf-multilib \
  RISCV_NAME=riscv64-unknown-elf \
  QUARTUS_SH=/caminho/para/quartus_sh
```

Grave a FPGA:

```bash
make -C scripts/Murax/de10_standard program
```

Se houver mais de um cabo JTAG, escolha o cabo com `CABLE`:

```bash
jtagconfig
make -C scripts/Murax/de10_standard program CABLE=1
```

Na DE10-Standard, a cadeia JTAG normalmente mostra primeiro o HPS e depois a FPGA. Por isso o Makefile programa o `.sof` no índice `@2` e pula o HPS com:

```make
HPS_DEVICE ?= SOCVHPS
FPGA_DEVICE_INDEX ?= 2
```

### Significado dos LEDs

Durante a execução normal:

```text
LEDR[0] = 1    firmware iniciou
LEDR[1] = 1    keypair em execução
LEDR[2] = 1    encapsulation em execução
LEDR[3] = 1    decapsulation em execução ou falha final
LEDR[3:0] = 1111    execução terminou com sucesso
```

O estado final esperado é:

```text
LEDR[3:0] = 1111
```

Isso significa que:

```text
mlkem_keypair retornou 0
mlkem_enc retornou 0
mlkem_dec retornou 0
ss1 e ss2 são iguais
```

Se o estado final ficar em:

```text
LEDR[3:0] = 1000
```

então houve falha em algum retorno ou o shared secret não bateu.

### Rodar KAT na FPGA

Para gravar o firmware KAT na DE10-Standard em vez do benchmark normal:

```bash
make -C scripts/Murax/de10_standard kat-program \
  RISCV_PATH=/home/borgescaua/opt/riscv-elf-multilib \
  RISCV_NAME=riscv64-unknown-elf
```

Esse comando:

- gera `kat_vectors.h` a partir do submodule `external/mlkem-native`;
- compila o firmware com `KAT=yes`;
- compila com `FPGA_LED_ONLY=yes`;
- regenera o RTL do Murax para a DE10-Standard;
- roda o Quartus;
- grava o `.sof` na FPGA.

Como a saída da FPGA é por LEDs, não há impressão do `kat_pass` na placa. O estado final esperado também é:

```text
LEDR[3:0] = 1111
```

No KAT, esse estado indica que `pk`, `sk`, `ct` e `ss` bateram byte-a-byte com os vetores gerados.

### Outros alvos FPGA

O repositório também contém diretórios antigos para Arty A7 e iCE40 HX8K. Eles servem como referência de fluxo FPGA, mas não são o caminho pronto para o ML-KEM-512 atual.

O firmware atual usa 128 KiB de RAM no Murax. Portanto, qualquer outro alvo precisa ser adaptado para usar o firmware `crystal_kyber.hex`, RAM suficiente e o mapeamento de saída desejado.

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
