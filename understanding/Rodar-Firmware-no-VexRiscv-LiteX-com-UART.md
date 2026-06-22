# Rodar firmware no VexRiscv do LiteX e usar UART

Este guia descreve o fluxo recomendado para executar um firmware bare-metal no VexRiscv instanciado pelo LiteX, usar a UART como console e preparar o projeto para medir ciclos e area antes de criar aceleradores em hardware.

O caso esperado e:

- FPGA com SoC LiteX.
- CPU `vexriscv`.
- Firmware C/C++ com o algoritmo rodando como software.
- Saida por UART fisica ou por `jtag_uart`.
- Medicao inicial por contador de ciclos e relatorios da ferramenta de sintese.

## 1. Escolha o target da placa

Use um target existente do `litex-boards` quando possivel. A forma geral e:

```bash
python3 -m litex_boards.targets.<target_da_placa> --help
```

Exemplos de opcoes importantes:

```bash
--cpu-type=vexriscv
--cpu-variant=lite
--cpu-variant=standard
--uart-name=serial
--uart-name=jtag_uart
--csr-csv=csr.csv
--build
--load
```

Para algoritmos como ML-KEM/Kyber, comece com `standard` se houver recursos suficientes na FPGA. Use `lite` apenas se precisar reduzir area.

## 2. Escolha a UART

### UART fisica

Use quando a placa tem USB-UART ou quando ha um adaptador externo conectado aos pinos seriais:

```bash
--uart-name=serial --uart-baudrate=115200
```

Depois de carregar o bitstream:

```bash
litex_term /dev/ttyUSBX
```

Para carregar um firmware pela UART:

```bash
litex_term /dev/ttyUSBX --kernel=firmware.bin
```

### UART por JTAG

Use quando voce nao quer ou nao tem um conversor USB-UART externo. Nesse modo, a UART do SoC passa pelo JTAG:

```bash
--uart-name=jtag_uart
```

Depois de carregar o bitstream, use o terminal compativel com o canal JTAG UART da sua instalacao LiteX/board. Se o target suporta `jtag_uart`, o firmware continua enxergando uma UART normal do ponto de vista do software; a diferenca fica no transporte entre FPGA e host.

### JTAGBone nao e console

`jtagbone` e uma ponte para ler/escrever o barramento do SoC pelo host. Ela e util para debug, leitura de registradores e automacao, mas nao substitui diretamente `printf`/`write` do firmware.

No SoC:

```python
self.add_jtagbone()
```

No host:

```bash
litex_server --jtag
litex_cli --regs
```

## 3. Gere o SoC LiteX com VexRiscv

Com UART fisica:

```bash
python3 -m litex_boards.targets.<target_da_placa> \
    --cpu-type=vexriscv \
    --cpu-variant=standard \
    --uart-name=serial \
    --uart-baudrate=115200 \
    --csr-csv=csr.csv \
    --build \
    --load
```

Com UART por JTAG:

```bash
python3 -m litex_boards.targets.<target_da_placa> \
    --cpu-type=vexriscv \
    --cpu-variant=standard \
    --uart-name=jtag_uart \
    --csr-csv=csr.csv \
    --build \
    --load
```

Se o firmware precisar de mais RAM integrada:

```bash
--integrated-main-ram-size=0x20000
```

ou outro tamanho adequado ao algoritmo.

## 4. Porte o firmware para o ambiente LiteX

Remova dependencias especificas do SoC antigo, por exemplo:

- Enderecos fixos da UART do Murax.
- Linker script especifico do Murax.
- Startup code especifico do Murax.
- Inicializacao manual de perifericos que o LiteX ja fornece.

No firmware LiteX, use os headers gerados pelo build:

```c
#include <generated/csr.h>
#include <generated/mem.h>
```

Para saida simples, comece com:

```c
#include <stdio.h>

int main(void)
{
    printf("Firmware iniciado\n");
    return 0;
}
```

Depois coloque as chamadas do algoritmo:

```c
int main(void)
{
    printf("ML-KEM inicio\n");

    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss, pk);
    crypto_kem_dec(ss2, ct, sk);

    printf("ML-KEM fim\n");
    return 0;
}
```

## 5. Meça ciclos no VexRiscv

Para medir ciclos diretamente no firmware:

```c
static inline unsigned int rdcycle(void)
{
    unsigned int value;
    __asm__ volatile ("rdcycle %0" : "=r"(value));
    return value;
}
```

Use em cada parte do algoritmo:

```c
unsigned int start, end;

start = rdcycle();
crypto_kem_keypair(pk, sk);
end = rdcycle();
printf("keypair cycles: %u\n", end - start);

start = rdcycle();
crypto_kem_enc(ct, ss, pk);
end = rdcycle();
printf("enc cycles: %u\n", end - start);

start = rdcycle();
crypto_kem_dec(ss2, ct, sk);
end = rdcycle();
printf("dec cycles: %u\n", end - start);
```

Se o compilador reclamar da instrucao `rdcycle`, ajuste as flags RISC-V para incluir suporte a CSR/counters, por exemplo `rv32i_zicsr`, sem mudar o objetivo arquitetural do firmware: ele continua sendo um alvo RV32I com o suporte necessario para ler contadores.

## 6. Compile o firmware

O firmware deve ser compilado com a toolchain RISC-V usada pelo LiteX e com os headers gerados no build do SoC. Em um fluxo LiteX comum, o build gera diretorios como:

```text
build/<nome_do_target>/software/include/generated/
build/<nome_do_target>/software/include/base/
```

O firmware precisa enxergar esses includes e linkar para o mapa de memoria gerado pelo LiteX.

A estrategia mais simples e criar o firmware a partir de um exemplo bare-metal LiteX ja existente e substituir o `main.c` pelo seu codigo. Assim voce reaproveita:

- Startup code.
- Linker script.
- Headers gerados.
- Suporte basico de UART/console.
- Definicoes de memoria.

## 7. Carregue e execute o firmware

Com UART fisica:

```bash
litex_term /dev/ttyUSBX --kernel=firmware.bin
```

Com `jtag_uart`, use o terminal/ponte indicado pelo target. O ponto importante e que o firmware continua usando a UART do LiteX; o transporte fisico passa pelo JTAG.

Ao iniciar, espere ver algo como:

```text
Firmware iniciado
ML-KEM inicio
keypair cycles: ...
enc cycles: ...
dec cycles: ...
ML-KEM fim
```

## 8. Gere o baseline de area

Depois de confirmar que o firmware roda, salve os relatorios de sintese/place-and-route da ferramenta FPGA:

- LUTs/ALMs.
- FFs.
- BRAMs.
- DSPs.
- Frequencia maxima.
- Uso de memoria.

Esse e o baseline sem acelerador.

## 9. So depois crie aceleradores

Nao comece acelerando o algoritmo inteiro. Primeiro descubra onde estao os ciclos. Para ML-KEM/Kyber, candidatos comuns sao:

- NTT.
- inverse NTT.
- multiplicacao polinomial.
- Keccak/SHAKE.
- amostragem/rejection sampling.
- compress/decompress.

O acelerador deve entrar como periferico LiteX, acessado por CSR ou Wishbone:

```text
VexRiscv -> CSR/Wishbone -> acelerador -> resultado
```

Fluxo de software esperado:

```c
accel_input_write(...);
accel_start_write(1);

while (!accel_done_read())
    ;

result = accel_output_read();
```

Compare sempre:

```text
ciclos antes
ciclos depois
LUTs antes
LUTs depois
BRAMs antes
BRAMs depois
Fmax antes
Fmax depois
```

## 10. Checklist recomendado

1. Gerar SoC LiteX com `--cpu-type=vexriscv`.
2. Escolher `--uart-name=serial` ou `--uart-name=jtag_uart`.
3. Carregar bitstream.
4. Rodar um `printf("hello")`.
5. Portar o firmware ML-KEM.
6. Confirmar `keypair`, `enc` e `dec`.
7. Medir ciclos com `rdcycle`.
8. Salvar relatorio de area baseline.
9. Identificar a funcao mais cara.
10. Criar acelerador pequeno e mensuravel.
11. Comparar ciclos e area.
