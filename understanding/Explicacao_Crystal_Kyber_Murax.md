# Explicacao completa do alvo `c/murax/crystal_kyber`

Este documento explica o alvo bare-metal criado em `src/main/c/murax/crystal_kyber`, os arquivos dentro de `src/main/c/murax/crystal_kyber/src` e a alteracao feita em `src/main/scala/vexriscv/demo/Murax.scala` com o objeto `MuraxCrystalKyberWithRamInit`.

O objetivo desse conjunto de arquivos e permitir executar o algoritmo Crystal Kyber, atualmente exposto como ML-KEM-512 pelas funcoes `PQCLEAN_MLKEM512_CLEAN_*`, dentro de um SoC Murax baseado no processador VexRiscv. Para isso, foi necessario criar um firmware bare-metal, ligar esse firmware contra os codigos C do Kyber, definir um mapa de memoria compativel com o Murax e configurar a geracao do hardware para inicializar a RAM interna com o binario gerado.

## Visao geral do fluxo

O fluxo completo e:

1. O codigo C do Kyber fica em `kyber_implementation`.
2. O alvo `src/main/c/murax/crystal_kyber` compila esse codigo junto com um pequeno programa bare-metal.
3. O resultado da compilacao gera `build/crystal_kyber.elf`, `build/crystal_kyber.hex`, `build/crystal_kyber.bin`, `build/crystal_kyber.v` e `build/crystal_kyber.asm`.
4. O objeto Scala `MuraxCrystalKyberWithRamInit` instancia o SoC Murax com 128 KiB de RAM interna.
5. Essa RAM e inicializada com `src/main/c/murax/crystal_kyber/build/crystal_kyber.hex`.
6. Quando o VexRiscv sai do reset, ele busca instrucoes a partir de `0x80000000`, executa o startup em `crt.S` e chama `main()`.
7. O `main()` executa `keypair`, `encaps` e `decaps`, compara os segredos compartilhados e imprime resultados pela UART.

## Por que criar um alvo separado para Kyber

O Murax e um SoC pequeno, sem sistema operacional, sem libc completa e com memoria interna limitada. Um programa Kyber comum, compilado para Linux ou para um ambiente hosted, depende de servicos que nao existem diretamente no core, como inicializacao de processo, heap, funcoes padrao de memoria, saida de texto e encerramento do programa.

Por isso foi criado um alvo dedicado em `src/main/c/murax/crystal_kyber`:

- para compilar o Kyber como firmware bare-metal RISC-V;
- para controlar exatamente a ISA usada no binario, como `rv32i` ou `rv32im`;
- para fornecer um startup minimo em assembly;
- para definir a RAM em `0x80000000`, que e o endereco usado pelo Murax;
- para fornecer wrappers simples para UART, GPIO, timer e interrupcoes;
- para gerar um arquivo `.hex` carregavel diretamente na RAM do SoC durante a geracao RTL.

## `src/main/c/murax/crystal_kyber/README.md`

O `README.md` desse diretorio documenta o alvo de firmware. Ele deixa claro que o diretorio e um target bare-metal para integrar Crystal-Kyber com Murax.

O bloco de build informa:

```sh
make
```

Isso significa que a entrada principal do fluxo de software e o `makefile` local. Ele nao usa CMake, Meson ou SBT para compilar o C. O SBT entra depois, no lado Scala, para gerar o hardware.

Os artefatos listados sao:

- `build/crystal_kyber.elf`: executavel ELF RISC-V, usado para debug, mapa de simbolos e objdump;
- `build/crystal_kyber.hex`: imagem em Intel HEX usada para inicializar a RAM do Murax;
- `build/crystal_kyber.asm`: disassembly para inspecionar o codigo gerado.

O README tambem registra uma decisao importante: o alvo foi pensado inicialmente para `rv32i`, porque o Murax padrao e simples. Se o core for configurado com multiplicacao/divisao por hardware, o firmware pode ser compilado com `MULDIV=yes`.

## `makefile`

O `makefile` e o centro do build do firmware.

### Identidade do projeto

```make
PROJ_NAME=crystal_kyber
DEBUG=no
BENCH=no
MULDIV=no
COMPRESSED=no
```

`PROJ_NAME` define o nome base dos artefatos. Por isso os arquivos finais ficam como `crystal_kyber.elf`, `crystal_kyber.hex`, `crystal_kyber.bin`, `crystal_kyber.asm` e `crystal_kyber.v`.

`DEBUG`, `BENCH`, `MULDIV` e `COMPRESSED` sao chaves de configuracao:

- `DEBUG=yes` troca otimizacao por `-O0` e mais informacao de debug;
- `BENCH=yes` adiciona `-fno-inline`, util quando se quer medir funcoes separadamente;
- `MULDIV=yes` inclui a extensao `M` na arquitetura RISC-V;
- `COMPRESSED=yes` tenta incluir instrucoes comprimidas, extensao `C`.

### Localizacao do Kyber

```make
ROOT := ../../../../..
KYBER_DIR := $(ROOT)/kyber_implementation
```

`ROOT` sobe do diretorio `src/main/c/murax/crystal_kyber` ate a raiz do repositorio. `KYBER_DIR` aponta para a implementacao do Kyber que fica fora do target Murax.

Essa separacao e importante: o target Murax nao duplica o algoritmo. Ele reutiliza os arquivos C de `kyber_implementation` e so adiciona a camada bare-metal necessaria para rodar no VexRiscv.

### Coleta automatica dos fontes

```make
APP_C_SRCS := $(wildcard src/*.c)
APP_CPP_SRCS := $(wildcard src/*.cpp)
APP_ASM_SRCS := $(wildcard src/*.S)
KYBER_C_SRCS := $(wildcard $(KYBER_DIR)/*.c)
```

Essas linhas procuram automaticamente:

- arquivos C locais em `src/*.c`, como `main.c` e `support.c`;
- arquivos C++ locais, caso sejam adicionados depois;
- arquivos assembly locais em `src/*.S`, como `crt.S`;
- todos os `.c` da implementacao Kyber.

Assim, ao adicionar um novo `.c` em `kyber_implementation`, ele entra no build sem precisar editar manualmente a lista de fontes.

### Diretorio de build, includes e linker script

```make
OBJDIR = build

INC  = -I./src -I$(KYBER_DIR)
LIBS =
LIBSINC = -L$(OBJDIR)
LDSCRIPT = ./src/linker.ld
```

`OBJDIR` define onde os objetos e artefatos serao colocados.

`INC` adiciona dois caminhos de include:

- `./src`, para headers do Murax como `murax.h`, `uart.h` e `gpio.h`;
- `$(KYBER_DIR)`, para headers do Kyber como `api.h`.

`LDSCRIPT` aponta para o script de linkagem que define onde o codigo, dados, heap e stack ficam dentro da RAM do Murax.

### Escolha do toolchain

```make
SIFIVE_GCC_PACK ?= yes
```

Por padrao, o makefile assume um pacote estilo SiFive em `/opt/riscv` com prefixo `riscv64-unknown-elf`.

Quando `SIFIVE_GCC_PACK=yes`:

```make
RISCV_NAME ?= riscv64-unknown-elf
RISCV_PATH ?= /opt/riscv/
```

Quando `SIFIVE_GCC_PACK=no`, o makefile espera toolchains RV32 separados:

```make
RISCV_NAME ?= riscv32-unknown-elf
```

e escolhe `/opt/riscv32i/` ou `/opt/riscv32im/` conforme `MULDIV`.

Essa parte existe porque ha duas formas comuns de instalar compiladores RISC-V:

- um GCC `riscv64-unknown-elf` multilib, capaz de gerar tambem `rv32i/ilp32`;
- toolchains separados, um para RV32I e outro para RV32IM.

Para este projeto, o ponto critico e que o compilador precisa conseguir gerar binarios `elf32-littleriscv` com ABI `ilp32`.

### ABI e arquitetura

```make
MABI=ilp32
MARCH := rv32i
```

`MABI=ilp32` define a ABI de 32 bits: inteiros, longs e ponteiros de 32 bits. Isso combina com um core VexRiscv RV32.

`MARCH=rv32i` define a ISA base. O codigo e compilado para o conjunto inteiro basico RISC-V de 32 bits.

Quando `MULDIV=yes`:

```make
MARCH := $(MARCH)m
```

A extensao `M` adiciona multiplicacao e divisao por hardware. Kyber usa muitas multiplicacoes em reducoes modulares e NTT; portanto, um core com `M` tende a ser bem mais rapido. Sem `M`, o compilador chama rotinas de software de `libgcc`, como `__mulsi3`.

Quando `COMPRESSED=yes`:

```make
MARCH := $(MARCH)ac
```

A intencao e adicionar instrucoes atomicas e comprimidas. Para um alvo Murax pequeno, o mais relevante seria normalmente `C`, porque reduz tamanho de codigo. A extensao `A` so faz sentido se o core tiver suporte atomico.

Observacao importante: toolchains RISC-V modernos podem exigir que CSR seja declarado explicitamente, por exemplo `rv32izicsr`, porque `crt.S` usa instrucoes como `csrw`. Se o build falhar em `csrw mie,a0` ou `csrw mstatus,a0`, a arquitetura precisa incluir `zicsr`.

### Flags de compilacao

```make
CFLAGS += -march=$(MARCH) -mabi=$(MABI) -DNDEBUG
LDFLAGS += -march=$(MARCH) -mabi=$(MABI)
```

Essas flags garantem que todos os arquivos C, assembly e a linkagem usem a mesma ISA e ABI.

`-DNDEBUG` desativa asserts condicionados por `assert.h`, comum em builds finais ou embarcados.

O bloco de debug:

```make
ifeq ($(DEBUG),yes)
	CFLAGS += -g3 -O0
endif
```

usa `-O0` para facilitar debug e `-g3` para informacoes detalhadas.

O bloco normal:

```make
ifeq ($(DEBUG),no)
	CFLAGS += -g -Os
endif
```

usa `-Os`, otimizacao para tamanho. Essa escolha e coerente com Murax, porque a RAM interna e limitada e o firmware inteiro fica dentro dela.

O bloco de benchmark:

```make
ifeq ($(BENCH),yes)
	CFLAGS += -fno-inline
endif
```

impede inlining. Isso piora desempenho, mas deixa a medicao por funcao mais clara no disassembly e em traces.

### Ferramentas usadas

```make
RISCV_OBJCOPY = $(RISCV_PATH)/bin/$(RISCV_NAME)-objcopy
RISCV_OBJDUMP = $(RISCV_PATH)/bin/$(RISCV_NAME)-objdump
RISCV_CC=$(RISCV_PATH)/bin/$(RISCV_NAME)-gcc
```

Essas variaveis montam os caminhos para compilador, objcopy e objdump. O firmware nao usa o compilador nativo da maquina; ele usa um cross-compiler RISC-V.

### Flags bare-metal

```make
CFLAGS += -MD -fstrict-volatile-bitfields -fno-strict-aliasing -ffreestanding
```

- `-MD` gera arquivos `.d` de dependencia;
- `-fstrict-volatile-bitfields` ajuda acessos a registradores mapeados em memoria;
- `-fno-strict-aliasing` evita otimizacoes agressivas baseadas em aliasing;
- `-ffreestanding` informa ao compilador que nao existe ambiente hosted padrao.

As flags de linkagem:

```make
LDFLAGS += -nostdlib -lgcc -mcmodel=medany -nostartfiles -ffreestanding -Wl,-Bstatic,-T,$(LDSCRIPT),-Map,$(OBJDIR)/$(PROJ_NAME).map,--print-memory-usage
```

significam:

- `-nostdlib`: nao linkar libc e startup padrao;
- `-lgcc`: ainda usar rotinas auxiliares do GCC, como multiplicacao ou shifts de 64 bits quando necessario;
- `-mcmodel=medany`: modelo de codigo adequado para enderecos nao necessariamente proximos de zero;
- `-nostartfiles`: nao usar `crt0` do toolchain;
- `-T linker.ld`: usar o mapa de memoria local;
- `-Map`: gerar o arquivo `.map`;
- `--print-memory-usage`: mostrar consumo de memoria no fim da linkagem.

### Conversao de fontes em objetos

```make
APP_C_OBJS := $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(APP_C_SRCS))
APP_CPP_OBJS := $(patsubst src/%.cpp,$(OBJDIR)/src/%.o,$(APP_CPP_SRCS))
APP_ASM_OBJS := $(patsubst src/%.S,$(OBJDIR)/src/%.o,$(APP_ASM_SRCS))
KYBER_C_OBJS := $(patsubst $(KYBER_DIR)/%.c,$(OBJDIR)/kyber/%.o,$(KYBER_C_SRCS))
```

Essas regras transformam caminhos de fonte em caminhos de objeto. Por exemplo:

- `src/main.c` vira `build/src/main.o`;
- `src/support.c` vira `build/src/support.o`;
- `src/crt.S` vira `build/src/crt.o`;
- `kyber_implementation/kem.c` vira `build/kyber/kem.o`.

Separar `build/src` e `build/kyber` deixa claro quais objetos pertencem ao firmware Murax e quais pertencem ao algoritmo Kyber.

### Alvo principal

```make
all: $(OBJDIR)/$(PROJ_NAME).elf $(OBJDIR)/$(PROJ_NAME).hex $(OBJDIR)/$(PROJ_NAME).bin $(OBJDIR)/$(PROJ_NAME).asm $(OBJDIR)/$(PROJ_NAME).v
```

O alvo `all` gera todos os formatos necessarios:

- ELF para debug;
- HEX para inicializacao de RAM;
- BIN para imagem bruta;
- ASM para inspecao humana;
- V para formato Verilog memory init.

### Linkagem

```make
$(OBJDIR)/%.elf: $(OBJS) | $(OBJDIR)
	$(RISCV_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBSINC) $(LIBS)
```

Essa regra pega todos os objetos e gera o ELF final. O `$@` e o arquivo de saida, `$^` e a lista de dependencias.

### Geracao dos formatos finais

```make
%.hex: %.elf
	$(RISCV_OBJCOPY) -O ihex $^ $@
```

Gera Intel HEX. Este e o arquivo referenciado pelo Scala.

```make
%.bin: %.elf
	$(RISCV_OBJCOPY) -O binary $^ $@
```

Gera binario puro.

```make
%.v: %.elf
	$(RISCV_OBJCOPY) -O verilog $^ $@
```

Gera formato Verilog memory.

```make
%.asm: %.elf
	$(RISCV_OBJDUMP) -S -d $^ > $@
```

Gera disassembly com codigo fonte intercalado quando possivel.

### Regras de compilacao

As regras para `.c`, `.cpp` e `.S` criam o diretorio necessario, compilam o objeto e, no caso de C, tambem geram um `.disasm` intermediario em assembly.

O tratamento especial de Kyber:

```make
$(OBJDIR)/kyber/%.o: $(KYBER_DIR)/%.c
```

existe porque os fontes Kyber estao fora do diretorio `src`. A regra compila esses arquivos com os mesmos includes e flags usados pelo firmware.

### Limpeza

O alvo `clean` remove objetos, dependencias e artefatos principais. Ele preserva a estrutura geral do repositorio e nao apaga fontes.

## `src/main.c`

`main.c` e o harness de execucao do Kyber dentro do Murax.

### Includes

```c
#include <stdint.h>

#include "api.h"
#include "murax.h"
```

`stdint.h` fornece tipos de tamanho fixo, como `uint8_t` e `uint32_t`.

`api.h` vem de `kyber_implementation` e declara as constantes e funcoes publicas do ML-KEM-512.

`murax.h` agrega os headers de periféricos do SoC, como UART e GPIO, e define os enderecos mapeados em memoria.

### Buffers globais

```c
static uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];
static uint8_t ct[PQCLEAN_MLKEM512_CLEAN_CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss1[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];
static uint8_t ss2[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];
```

Esses buffers armazenam:

- `pk`: chave publica;
- `sk`: chave secreta;
- `ct`: ciphertext;
- `ss1`: segredo compartilhado gerado na encapsulacao;
- `ss2`: segredo compartilhado recuperado na decapsulacao.

Eles sao `static` globais para ficarem em memoria global, normalmente `.bss`, em vez de consumir stack. Isso e importante porque Kyber usa buffers relativamente grandes e a stack do firmware e limitada.

Os tamanhos nao sao numeros fixos escritos manualmente. Eles vem de `api.h`, evitando erro caso os parametros do ML-KEM mudem.

### Impressao pela UART

```c
static void print(const char *str) {
  while (*str) {
    uart_write(UART, *str);
    str++;
  }
}
```

`print` percorre uma string C ate encontrar `'\0'`. Cada caractere e enviado para `uart_write`.

`UART` e um ponteiro para o registrador UART mapeado em `0xF0010000`. O envio nao usa `printf`, porque nao existe libc completa no ambiente bare-metal.

```c
static void println(const char *str) {
  print(str);
  uart_write(UART, '\n');
}
```

`println` reutiliza `print` e adiciona quebra de linha. Isso simplifica mensagens de status.

### Impressao hexadecimal

```c
static void print_hex32(uint32_t value) {
  for (int i = 7; i >= 0; i--) {
    uint32_t digit = (value >> (i * 4)) & 0xF;
    uart_write(UART, digit < 10 ? ('0' + digit) : ('A' + digit - 10));
  }
}
```

Essa funcao imprime um valor de 32 bits como 8 digitos hexadecimais.

O loop comeca em `i = 7`, que representa o nibble mais significativo, e termina em `i = 0`, o nibble menos significativo.

`(value >> (i * 4)) & 0xF` extrai um nibble. Se o valor for de 0 a 9, escreve `'0' + digit`; se for de 10 a 15, escreve `'A' + digit - 10`.

Isso substitui `printf("%08x")`, que seria pesado demais e dependeria de biblioteca C.

### Leitura do contador de ciclos

```c
static inline uint32_t read_cycle(void) {
  uint32_t value;
  asm volatile("rdcycle %0" : "=r"(value));
  return value;
}
```

`read_cycle` usa a instrucao RISC-V `rdcycle` para ler o contador de ciclos.

Ela serve para medir o custo aproximado de:

- geracao de par de chaves;
- encapsulacao;
- decapsulacao.

O retorno e `uint32_t`, entao a medicao assume que a diferenca entre inicio e fim cabe em 32 bits. Para execucoes muito longas, poderia haver overflow, mas para uma medicao simples no Murax isso e aceitavel.

### Comparacao dos segredos

```c
static int shared_secret_matches(void) {
  for (uint32_t i = 0; i < PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES; i++) {
    if (ss1[i] != ss2[i]) {
      return 0;
    }
  }

  return 1;
}
```

Depois de encapsular e decapsular, o segredo produzido por `crypto_kem_enc` deve ser igual ao segredo recuperado por `crypto_kem_dec`.

Essa funcao compara `ss1` e `ss2` byte por byte. Retorna `1` se todos os bytes sao iguais e `0` se encontrar diferenca.

Ela e um teste funcional basico: se `ss_match=0`, o fluxo Kyber falhou.

### Impressao de resultados

```c
static void print_result(const char *label, int value) {
  print(label);
  print("=0x");
  print_hex32((uint32_t)value);
  uart_write(UART, '\n');
}
```

Essa funcao imprime pares `nome=valor` em hexadecimal. Ela e usada para os codigos de retorno e para o resultado da comparacao do segredo.

```c
static void print_cycles(const char *label, uint32_t cycles) {
  print(label);
  print("=0x");
  print_hex32(cycles);
  uart_write(UART, '\n');
}
```

Essa funcao faz o mesmo para contagens de ciclos.

Separar `print_result` e `print_cycles` deixa o `main()` mais legivel e torna a saida UART padronizada.

### Corpo do `main`

```c
void main(void) {
  GPIO_A->OUTPUT_ENABLE = 0x0000000F;
  GPIO_A->OUTPUT = 0x00000001;
```

O firmware configura os quatro bits inferiores do GPIO como saida e coloca o bit 0 em nivel alto. Isso ajuda a observar atividade em LEDs ou sinais externos, dependendo da placa/simulacao.

```c
  println("Murax ML-KEM-512 start");
```

Mensagem inicial pela UART. Serve como marcador de que o boot funcionou, a UART esta acessivel e o firmware chegou ao `main()`.

```c
  uint32_t start_keypair = read_cycle();
  int keypair_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
  uint32_t end_keypair = read_cycle();
```

Esse bloco mede e executa a geracao do par de chaves. A funcao preenche `pk` e `sk`. O valor retornado e guardado em `keypair_ret`.

```c
  uint32_t start_enc = read_cycle();
  int enc_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(ct, ss1, pk);
  uint32_t end_enc = read_cycle();
```

Esse bloco executa a encapsulacao. Ele usa a chave publica `pk`, gera o ciphertext `ct` e produz o segredo compartilhado `ss1`.

```c
  uint32_t start_dec = read_cycle();
  int dec_ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss2, ct, sk);
  uint32_t end_dec = read_cycle();
```

Esse bloco executa a decapsulacao. Ele usa o ciphertext `ct` e a chave secreta `sk` para recuperar o segredo `ss2`.

```c
  print_result("keypair_ret", keypair_ret);
  print_result("enc_ret", enc_ret);
  print_result("dec_ret", dec_ret);
  print_result("ss_match", shared_secret_matches());
```

Essas linhas imprimem os codigos de retorno e se os segredos coincidem. Em uma execucao correta, espera-se retorno zero para as operacoes e `ss_match=1`.

```c
  print_cycles("cycles_keypair", end_keypair - start_keypair);
  print_cycles("cycles_enc", end_enc - start_enc);
  print_cycles("cycles_dec", end_dec - start_dec);
```

Essas linhas imprimem o numero de ciclos de cada etapa. Isso permite comparar configuracoes do VexRiscv, por exemplo RV32I puro contra RV32IM com multiplicador/divisor.

```c
  println("done");
```

Marca o fim da execucao do teste Kyber.

```c
  while (1) {
    GPIO_A->OUTPUT ^= 0x0000000F;
    for (volatile uint32_t i = 0; i < 200000; i++) {
      asm volatile("" ::: "memory");
    }
  }
}
```

Depois do teste, o firmware entra em loop infinito. Ele alterna os quatro bits inferiores do GPIO e faz um atraso simples.

Esse loop impede que o programa "termine", porque em bare-metal nao ha sistema operacional para retornar. Tambem gera atividade observavel no GPIO.

O `volatile` no contador evita que o compilador remova o loop de atraso. O `asm volatile("" ::: "memory")` funciona como barreira minima para impedir otimizacoes que eliminariam completamente o corpo do loop.

### Callback de interrupcao

```c
void irqCallback(void) {
}
```

O startup assembly chama `irqCallback` quando ocorre uma trap/interrupcao. Aqui a funcao esta vazia porque o teste Kyber nao depende de interrupcoes.

Mesmo vazia, ela precisa existir para satisfazer a referencia feita em `crt.S`.

## `crt.S`

`crt.S` e o codigo de inicializacao. Ele substitui o startup padrao de uma libc.

### Simbolos globais

```asm
.global crtStart
.global main
.global irqCallback
```

Essas diretivas informam ao montador/linker que os simbolos podem ser referenciados por outros arquivos.

`crtStart` e o ponto de entrada definido no linker script. `main` e a funcao C chamada depois da inicializacao. `irqCallback` e a funcao C chamada ao tratar interrupcoes.

### Secao inicial

```asm
    .section .start_jump,"ax",@progbits
crtStart:
  lui x2, %hi(crtInit)
  addi x2, x2, %lo(crtInit)
  jalr x1,x2
  nop
```

Esse bloco fica na secao `.start_jump`, que o linker coloca no inicio da RAM. Ele carrega o endereco de `crtInit` em `x2` e desvia para la com `jalr`.

No RISC-V, `x2` tambem e o registrador `sp`. Aqui ele e usado temporariamente antes da stack real ser configurada.

### Entrada de trap

```asm
.global trap_entry
.align 5
trap_entry:
```

`trap_entry` e a rotina para interrupcoes/excecoes. O alinhamento ajuda a cumprir requisitos ou convencoes de endereco para handler.

### Salvamento de registradores

O bloco com varios `sw` salva registradores na stack:

```asm
  sw x1,  - 1*4(sp)
  sw x5,  - 2*4(sp)
  ...
  sw x31, -16*4(sp)
  addi sp,sp,-16*4
```

Ele reserva espaco para 16 registradores e salva os registradores volateis que podem ser alterados durante o tratamento.

O ajuste do `sp` acontece depois dos stores, usando offsets negativos em relacao ao valor antigo de `sp`.

### Chamada para C

```asm
  call irqCallback
```

Chama a funcao C de tratamento de interrupcao. No firmware atual, essa funcao existe, mas esta vazia.

### Restauracao e retorno

```asm
  lw x1, 15*4(sp)
  ...
  lw x31, 0*4(sp)
  addi sp,sp,16*4
  mret
```

Os registradores sao restaurados e `mret` retorna da trap em modo maquina.

### Inicializacao de `gp` e `sp`

```asm
crtInit:
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop
  la sp, _stack_start
```

`gp` e o global pointer usado pela ABI RISC-V para acesso eficiente a dados pequenos.

`sp` e inicializado com `_stack_start`, simbolo definido no linker script. A partir daqui o codigo C pode usar stack.

### Zeragem da `.bss`

```asm
bss_init:
  la a0, _bss_start
  la a1, _bss_end
bss_loop:
  beq a0,a1,bss_done
  sw zero,0(a0)
  add a0,a0,4
  j bss_loop
bss_done:
```

Variaveis globais `static` sem inicializador, como os buffers `pk`, `sk`, `ct`, `ss1` e `ss2`, ficam na `.bss`. Em C, elas devem iniciar zeradas.

Como nao ha runtime padrao, o proprio `crt.S` percorre a regiao `.bss` e escreve zero de 4 em 4 bytes.

### Inicializacao de construtores

```asm
ctors_init:
  la a0, _ctors_start
  addi sp,sp,-4
ctors_loop:
  la a1, _ctors_end
  beq a0,a1,ctors_done
  lw a3,0(a0)
  add a0,a0,4
  sw a0,0(sp)
  jalr a3
  lw a0,0(sp)
  j ctors_loop
ctors_done:
  addi sp,sp,4
```

Esse bloco percorre a lista de construtores, como `.init_array` e `.ctors`. Para C puro isso quase nao e usado, mas deixa o startup compativel com codigo C++ ou inicializadores especiais.

### Habilitacao de interrupcoes

```asm
  li a0, 0x880
  csrw mie,a0
  li a0, 0x1808
  csrw mstatus,a0
```

Essas instrucoes escrevem nos CSRs `mie` e `mstatus` para configurar interrupcoes em modo maquina.

Como sao instrucoes CSR, toolchains modernos podem exigir `zicsr` no `-march`. Sem isso, o assembler pode rejeitar `csrw`.

### Chamada do programa principal

```asm
  call main
infinitLoop:
  j infinitLoop
```

Depois da inicializacao, chama `main`. Se `main` algum dia retornar, o codigo entra em loop infinito. Isso e necessario porque nao existe processo nem sistema operacional para receber um `return`.

## `linker.ld`

`linker.ld` define o layout do firmware na memoria do Murax.

### Formato e entrada

```ld
OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(crtStart)
```

O firmware e ELF RISC-V little-endian de 32 bits. O ponto de entrada e `crtStart`, definido em `crt.S`.

### Memoria RAM

```ld
MEMORY {
  RAM (rwx): ORIGIN = 0x80000000, LENGTH = 128k
}
```

O Murax mapeia a RAM principal em `0x80000000`. O tamanho foi definido como 128 KiB para acomodar o firmware Kyber, stack e heap.

Essa escolha precisa bater com o hardware Scala. Por isso o objeto `MuraxCrystalKyberWithRamInit` tambem usa `onChipRamSize = 128 kB`.

### Tamanho de stack e heap

```ld
_stack_size = DEFINED(_stack_size) ? _stack_size : 32k;
_heap_size = DEFINED(_heap_size) ? _heap_size : 16k;
```

Se o usuario nao definir outros tamanhos no link, a stack fica com 32 KiB e o heap com 16 KiB.

Kyber pode usar buffers temporarios relativamente grandes. Reservar stack maior reduz risco de overflow. O heap existe para atender `malloc`, usado por algumas rotinas auxiliares, especialmente no SHAKE incremental.

### Vetor/startup

```ld
._vector ORIGIN(RAM): {
  *crt.o(.start_jump);
  *crt.o(.text);
} > RAM
```

Essa secao coloca o jump inicial e o texto de `crt.o` no inicio da RAM. Assim, quando o core inicia em `0x80000000`, ele encontra o startup.

### Heap

```ld
._user_heap (NOLOAD): {
  . = ALIGN(8);
  PROVIDE(end = .);
  PROVIDE(_end = .);
  PROVIDE(_heap_start = .);
  . = . + _heap_size;
  . = ALIGN(8);
  PROVIDE(_heap_end = .);
} > RAM
```

Essa secao reserva espaco para heap sem gravar conteudo no arquivo final, por isso `NOLOAD`.

`_heap_start` e `_heap_end` sao usados por `support.c` para implementar `malloc`.

### Stack

```ld
._stack (NOLOAD): {
  . = ALIGN(16);
  PROVIDE(_stack_end = .);
  . = . + _stack_size;
  . = ALIGN(16);
  PROVIDE(_stack_start = .);
} > RAM
```

Reserva a stack. `_stack_start` e usado por `crt.S` para inicializar `sp`.

O alinhamento de 16 bytes segue uma pratica segura para ABI e chamadas de funcao.

### Dados inicializados

```ld
.data : {
  *(.rdata)
  *(.rodata .rodata.*)
  *(.gnu.linkonce.r.*)
  *(.data .data.*)
  *(.gnu.linkonce.d.*)
  . = ALIGN(8);
  PROVIDE(__global_pointer$ = . + 0x800);
  *(.sdata .sdata.*)
  *(.gnu.linkonce.s.*)
  . = ALIGN(8);
  *(.srodata.cst16)
  *(.srodata.cst8)
  *(.srodata.cst4)
  *(.srodata.cst2)
  *(.srodata .srodata.*)
} > RAM
```

Aqui entram dados inicializados, constantes e dados pequenos. O simbolo `__global_pointer$` e criado para o registrador `gp`.

### `.bss`

```ld
.bss (NOLOAD) : {
  . = ALIGN(4);
  _bss_start = .;
  *(.sbss*)
  *(.gnu.linkonce.sb.*)
  *(.bss .bss.*)
  *(.gnu.linkonce.b.*)
  *(COMMON)
  . = ALIGN(4);
  _bss_end = .;
} > RAM
```

Variaveis globais nao inicializadas entram aqui. O startup usa `_bss_start` e `_bss_end` para zerar essa regiao.

### `.rodata`, `.noinit`, `.memory` e `.ctors`

`.rodata` guarda constantes. `.noinit` guarda dados que nao devem ser inicializados. `.memory` recebe texto restante. `.ctors` guarda listas de construtores e define `END_OF_SW_IMAGE`.

Esse layout e simples e concentra tudo na RAM interna, o que combina com a configuracao Murax sem memoria externa.

## `support.c`

`support.c` fornece uma mini-runtime C para o firmware.

### Includes e simbolos externos

```c
#include <stddef.h>
#include <stdint.h>

extern uint8_t _heap_start;
extern uint8_t _heap_end;
```

`stddef.h` fornece `size_t`. `stdint.h` fornece tipos de tamanho fixo.

`_heap_start` e `_heap_end` sao definidos no linker script. Eles delimitam a regiao usada por `malloc`.

### Estado do heap

```c
static uintptr_t heap_current = 0;
```

`heap_current` guarda o proximo endereco livre do heap. Comeca em zero para indicar que o heap ainda nao foi inicializado.

### Alinhamento

```c
static uintptr_t align_up(uintptr_t value, uintptr_t alignment) {
  return (value + alignment - 1u) & ~(alignment - 1u);
}
```

Essa funcao arredonda um endereco para cima ate o proximo multiplo de `alignment`. Isso evita retornar ponteiros desalinhados em `malloc`.

### `memcpy`

```c
void *memcpy(void *dest, const void *src, size_t n)
```

Copia `n` bytes de `src` para `dest`. Kyber e o proprio compilador podem gerar chamadas para `memcpy`. Como `-nostdlib` remove libc, a funcao precisa existir no firmware.

### `memset`

```c
void *memset(void *dest, int value, size_t n)
```

Preenche `n` bytes com um valor. Tambem e comum o compilador gerar chamadas para `memset`, alem de bibliotecas C usarem essa funcao internamente.

### `memcmp`

```c
int memcmp(const void *lhs, const void *rhs, size_t n)
```

Compara dois buffers byte a byte. Retorna zero se forem iguais, negativo ou positivo se encontrar diferenca.

Kyber usa comparacoes de buffers em verificacao e decapsulacao.

### `malloc`

```c
void *malloc(size_t size)
```

Implementa um alocador linear simples:

1. Se `size == 0`, retorna `0`.
2. Se o heap ainda nao foi usado, inicializa `heap_current` com `_heap_start`.
3. Alinha o endereco atual.
4. Calcula o fim da alocacao.
5. Se passar de `_heap_end`, retorna `0`.
6. Atualiza `heap_current` e retorna o endereco inicial.

Esse `malloc` nao reutiliza memoria. Para esse firmware, isso e aceitavel porque o programa executa um fluxo curto e previsivel.

### `free`

```c
void free(void *ptr) {
  (void)ptr;
}
```

`free` nao faz nada. Isso combina com o `malloc` linear. A funcao existe para satisfazer codigo que chama `free`, mas nao ha reaproveitamento de blocos.

## Headers de perifericos

### `murax.h`

`murax.h` agrega os headers e define os enderecos base dos perifericos:

```c
#define GPIO_A ((Gpio_Reg *)(0xF0000000))
#define TIMER_PRESCALER ((Prescaler_Reg *)0xF0020000)
#define TIMER_INTERRUPT ((InterruptCtrl_Reg *)0xF0020010)
#define TIMER_A ((Timer_Reg *)0xF0020040)
#define TIMER_B ((Timer_Reg *)0xF0020050)
#define UART ((Uart_Reg *)(0xF0010000))
```

Esses enderecos correspondem ao mapa de memoria APB do Murax. O firmware acessa perifericos escrevendo e lendo diretamente structs `volatile` nesses enderecos.

### `gpio.h`

```c
typedef struct {
  volatile uint32_t INPUT;
  volatile uint32_t OUTPUT;
  volatile uint32_t OUTPUT_ENABLE;
} Gpio_Reg;
```

Define os registradores GPIO:

- `INPUT`: leitura dos pinos;
- `OUTPUT`: valor escrito nos pinos de saida;
- `OUTPUT_ENABLE`: mascara que define quais bits sao saida.

O `main.c` usa `OUTPUT_ENABLE` e `OUTPUT` para sinalizar atividade.

### `uart.h`

```c
typedef struct {
  volatile uint32_t DATA;
  volatile uint32_t STATUS;
  volatile uint32_t CLOCK_DIVIDER;
  volatile uint32_t FRAME_CONFIG;
} Uart_Reg;
```

Define os registradores da UART. `DATA` e usado para enviar bytes. `STATUS` informa disponibilidade de escrita e ocupacao de leitura. `CLOCK_DIVIDER` e `FRAME_CONFIG` configuram baudrate e formato do frame.

```c
static inline uint32_t uart_writeAvailability(Uart_Reg *reg) {
  return (reg->STATUS >> 16) & 0xFF;
}
```

Extrai do `STATUS` quantas posicoes de escrita estao disponiveis.

```c
static inline void uart_write(Uart_Reg *reg, uint32_t data) {
  while (uart_writeAvailability(reg) == 0) {
  }
  reg->DATA = data;
}
```

Espera ate a UART aceitar escrita e entao grava o byte em `DATA`. Esse polling e simples e adequado para um teste bare-metal.

### `timer.h`

Define `Timer_Reg` com:

- `CLEARS_TICKS`;
- `LIMIT`;
- `VALUE`.

Tambem fornece `timer_init`, que zera ticks e valor. O teste Kyber usa `rdcycle` para medir ciclos, entao o timer nao e essencial nesse momento, mas o header mantem compatibilidade com o ambiente Murax.

### `prescaler.h`

Define `Prescaler_Reg` com o registrador `LIMIT`. A funcao `prescaler_init` esta vazia, provavelmente porque o controle real e feito em outro ponto ou porque esse firmware nao usa o prescaler.

### `interrupt.h`

Define `InterruptCtrl_Reg` com:

- `PENDINGS`: interrupcoes pendentes;
- `MASKS`: mascara de habilitacao.

`interruptCtrl_init` desabilita mascaras e limpa pendencias escrevendo `0xFFFFFFFF`.

O firmware Kyber nao depende de interrupcoes, mas o startup tem suporte basico para traps.

## Alteracao Scala: `MuraxCrystalKyberWithRamInit`

O objeto esta em `src/main/scala/vexriscv/demo/Murax.scala`:

```scala
object MuraxCrystalKyberWithRamInit{
  def main(args: Array[String]) {
    SpinalVerilog(Murax(MuraxConfig.default.copy(
      onChipRamSize = 128 kB,
      onChipRamHexFile = "src/main/c/murax/crystal_kyber/build/crystal_kyber.hex"
    )))
  }
}
```

### Por que essa alteracao foi feita

O Murax ja tinha exemplos que inicializam RAM com programas simples, como `muraxDemo.hex`. Para rodar Kyber, era necessario criar uma variante que:

- aumentasse a RAM interna;
- carregasse o firmware Kyber em vez do demo padrao;
- mantivesse o restante da configuracao Murax padrao para reduzir o escopo da mudanca.

Sem essa alteracao, o hardware gerado nao saberia que deve carregar `crystal_kyber.hex`, e o core executaria outro firmware ou uma RAM vazia.

### `object MuraxCrystalKyberWithRamInit`

Em Scala, `object` define um singleton. Aqui ele funciona como uma aplicacao executavel pelo SBT:

```sh
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
```

Esse comando chama o metodo `main` do objeto e gera Verilog via SpinalHDL.

### `def main(args: Array[String])`

Esse e o ponto de entrada da aplicacao Scala. O array `args` existe por convencao, mesmo que nao seja usado.

### `SpinalVerilog(...)`

`SpinalVerilog` e a chamada que transforma a descricao SpinalHDL em Verilog. Tudo que esta dentro dela descreve o hardware a ser gerado.

### `Murax(...)`

`Murax` instancia o SoC. Ele inclui o VexRiscv, barramentos, RAM interna, ponte APB e perifericos como GPIO, timer e UART.

### `MuraxConfig.default.copy(...)`

`MuraxConfig.default` pega a configuracao padrao do Murax. `copy(...)` cria uma copia alterando apenas os campos passados.

Essa abordagem e boa porque evita duplicar toda a configuracao. A alteracao fica restrita aos aspectos necessarios para Kyber.

### `onChipRamSize = 128 kB`

Define a RAM interna do Murax com 128 KiB.

Isso precisa corresponder ao linker script:

```ld
RAM (rwx): ORIGIN = 0x80000000, LENGTH = 128k
```

Se o hardware tiver menos RAM que o linker script, o firmware pode acessar enderecos inexistentes. Se o linker script usar menos RAM que o hardware, parte da memoria ficara inutilizada.

Kyber precisa de mais memoria do que os exemplos simples de Murax. Por isso a RAM foi aumentada em relacao ao padrao.

### `onChipRamHexFile = ".../crystal_kyber.hex"`

Esse campo informa o arquivo que sera usado para inicializar o conteudo da RAM.

O caminho:

```scala
"src/main/c/murax/crystal_kyber/build/crystal_kyber.hex"
```

aponta para o artefato gerado pelo `makefile`. Isso conecta o mundo software ao mundo hardware:

- o makefile gera o `.hex`;
- o Scala inclui o `.hex` na RAM gerada;
- o VexRiscv executa esse conteudo ao iniciar.

## Relacao entre firmware, linker e hardware

Tres partes precisam estar coerentes:

1. `linker.ld` define RAM em `0x80000000` com 128 KiB.
2. `MuraxCrystalKyberWithRamInit` cria uma RAM de 128 KiB no Murax.
3. `makefile` gera `crystal_kyber.hex`, que e carregado nessa RAM.

Se qualquer uma dessas partes divergir, o programa pode falhar de formas dificeis de depurar:

- boot em endereco errado;
- stack fora da RAM;
- heap sobrepondo codigo ou dados;
- imagem maior que a RAM;
- core executando firmware antigo.

## Saida esperada pela UART

Quando o firmware executa corretamente, a UART deve mostrar mensagens no estilo:

```text
Murax ML-KEM-512 start
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
ss_match=0x00000001
cycles_keypair=0x........
cycles_enc=0x........
cycles_dec=0x........
done
```

Os valores de ciclo variam conforme configuracao do core, frequencia, memoria, flags de compilacao e presenca da extensao `M`.

## Pontos de atencao

### Toolchain RV32

O firmware precisa de um toolchain capaz de gerar `rv32*/ilp32`. Usar um `riscv64-unknown-elf-gcc` so funciona se ele for multilib e tiver bibliotecas RV32. Caso contrario, a compilacao pode ate gerar objetos RV32, mas a linkagem falha ao tentar usar uma `libgcc` de 64 bits.

### Extensao `zicsr`

Como `crt.S` usa `csrw`, toolchains recentes podem exigir `zicsr` em `-march`. Nesse caso, `rv32i` deve virar algo como `rv32izicsr`, e `rv32im` deve virar uma forma aceita pelo GCC instalado, como `rv32im_zicsr`.

### Multiplicacao por hardware

Kyber funciona em RV32I puro, mas multiplicacoes serao implementadas por software via `libgcc`. Com extensao `M`, o desempenho melhora e a dependencia de rotinas como `__mulsi3` diminui.

### Entropia

O arquivo `kyber_implementation/randombytes.c` usa um gerador deterministico simples. Isso e suficiente para bring-up e simulacao, mas nao e seguro para uso criptografico real. Em hardware real, sera necessario substituir por uma fonte de entropia adequada, como TRNG ou DRBG corretamente semeado.

### Heap e `exit`

O target fornece `malloc` e `free`, mas nao fornece libc completa. Se alguma parte do Kyber ou SHAKE chamar outras funcoes padrao, sera preciso adicionar stubs ou implementacoes minimas em `support.c`.

## Resumo

O codigo em `c/murax/crystal_kyber` cria a ponte entre o algoritmo Kyber em C e o core VexRiscv dentro do SoC Murax. O makefile compila o firmware bare-metal, `crt.S` inicializa o ambiente minimo, `linker.ld` posiciona tudo na RAM do Murax, `support.c` substitui partes essenciais da libc, os headers descrevem os perifericos mapeados em memoria e `main.c` executa o fluxo ML-KEM-512 completo.

A alteracao Scala `MuraxCrystalKyberWithRamInit` fecha a integracao ao gerar um Murax com 128 KiB de RAM e pre-carregar essa RAM com `crystal_kyber.hex`. Com isso, o VexRiscv pode iniciar diretamente executando o firmware Kyber.
