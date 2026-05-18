# README operacional: ML-KEM-512 no Murax/VexRiscv

Este README descreve o que está implementado atualmente para executar o ML-KEM-512, também conhecido como Kyber-512, dentro do SoC Murax com CPU VexRiscv. O foco aqui é uso prático: como compilar, como simular, como rodar benchmark, como rodar KAT e como interpretar a saída do `VMurax`.

## O que está implementado

O projeto possui uma aplicação bare-metal para o Murax que executa a implementação `clean` do ML-KEM-512 vinda do PQClean.

Diretório principal do algoritmo:

```text
kyber_implementation/
```

Firmware bare-metal do Murax:

```text
src/main/c/murax/crystal_kyber/
```

Arquivos principais do firmware:

```text
src/main/c/murax/crystal_kyber/src/main.c
src/main/c/murax/crystal_kyber/src/main_kat.c
src/main/c/murax/crystal_kyber/src/kat_randombytes.c
src/main/c/murax/crystal_kyber/src/kat_vectors.h
src/main/c/murax/crystal_kyber/src/support.c
src/main/c/murax/crystal_kyber/src/linker.ld
```

Existem dois modos principais:

- Benchmark normal: executa `keypair`, `encapsulation` e `decapsulation`, imprime prefixos dos buffers e ciclos medidos por `rdcycle`.
- KAT: executa o vetor conhecido do PQClean/NIST e valida byte-a-byte `seed`, `pk`, `sk`, `ct` e `ss`.

## Arquitetura alvo

O firmware é compilado para RISC-V 32-bit:

```text
rv32i_zicsr
ABI: ilp32
```

O sufixo `zicsr` é necessário porque o benchmark usa CSR de ciclo via:

```c
rdcycle
```

O SoC usado é o alvo Scala:

```text
vexriscv.demo.MuraxCrystalKyberWithRamInit
```

Esse alvo carrega o firmware gerado em:

```text
src/main/c/murax/crystal_kyber/build/crystal_kyber.hex
```

## Toolchain esperada

O Makefile está configurado por padrão para:

```text
RISCV_PATH ?= /opt/riscv-ricstar
RISCV_NAME ?= riscv32-none-elf
```

O compilador esperado é:

```text
/opt/riscv-ricstar/bin/riscv32-none-elf-gcc
```

Para conferir:

```bash
which riscv32-none-elf-gcc
```

No ambiente atual, o caminho esperado é:

```text
/opt/riscv-ricstar/bin/riscv32-none-elf-gcc
```

## Saídas geradas pelo firmware

Ao compilar o firmware, o Makefile gera:

```text
src/main/c/murax/crystal_kyber/build/crystal_kyber.elf
src/main/c/murax/crystal_kyber/build/crystal_kyber.hex
src/main/c/murax/crystal_kyber/build/crystal_kyber.bin
src/main/c/murax/crystal_kyber/build/crystal_kyber.asm
src/main/c/murax/crystal_kyber/build/crystal_kyber.v
src/main/c/murax/crystal_kyber/build/crystal_kyber.map
```

Função de cada arquivo:

- `.elf`: executável RISC-V com símbolos.
- `.hex`: imagem usada pelo Murax para inicializar a RAM.
- `.bin`: imagem binária bruta.
- `.asm`: disassembly gerado por `objdump`, útil para inspeção.
- `.v`: dump em formato Verilog do firmware.
- `.map`: mapa de linkedição, útil para ver uso de memória e símbolos.

## Variáveis principais do Makefile

O Makefile fica em:

```text
src/main/c/murax/crystal_kyber/makefile
```

Variáveis importantes:

```make
PROJ_NAME=crystal_kyber
DEBUG=no
BENCH=no
MULDIV=no
COMPRESSED=no
KAT ?= no
BENCH_ROUNDS ?= 2
RISCV_NAME ?= riscv32-none-elf
RISCV_PATH ?= /opt/riscv-ricstar
MABI=ilp32
MARCH := rv32i_zicsr
```

Significado:

- `PROJ_NAME`: nome base dos artefatos gerados.
- `DEBUG=no`: usa otimização `-Os` e debug info básica `-g`.
- `DEBUG=yes`: usa `-O0 -g3`, útil para depuração, mas altera desempenho.
- `BENCH=no`: comportamento normal.
- `BENCH=yes`: adiciona `-fno-inline`, útil se quiser evitar inlining em medições específicas, mas altera desempenho.
- `MULDIV=no`: mantém o alvo sem extensão `M`.
- `MULDIV=yes`: adiciona `m` ao `-march`.
- `COMPRESSED=no`: mantém o alvo sem instruções comprimidas.
- `COMPRESSED=yes`: adiciona `ac` ao `-march`.
- `KAT=no`: compila o firmware de benchmark normal.
- `KAT=yes`: compila o firmware de validação KAT.
- `BENCH_ROUNDS`: número de rodadas executadas pelo benchmark normal.
- `RISCV_NAME`: prefixo da toolchain.
- `RISCV_PATH`: diretório onde está instalada a toolchain.
- `MABI=ilp32`: ABI RISC-V 32-bit.
- `MARCH=rv32i_zicsr`: ISA base usada no firmware.

## Significado das flags de comando

### `make -B`

A flag `-B` força o `make` a reconstruir os alvos, mesmo que os arquivos pareçam atualizados.

Exemplo:

```bash
make -B -C src/main/c/murax/crystal_kyber all
```

Sem `-B`, o `make` pode decidir que nada mudou e não recompilar. Com `-B`, ele recompila tudo que é necessário. Isso é útil neste projeto porque o firmware, o HEX carregado no Murax e o Verilator dependem de etapas encadeadas.

### `make -C`

A flag `-C` diz ao `make` para entrar em outro diretório antes de executar.

Exemplo:

```bash
make -C src/main/c/murax/crystal_kyber all
```

Isso equivale a:

```bash
cd src/main/c/murax/crystal_kyber
make all
```

mas sem mudar permanentemente o diretório do terminal.

### `all`

O alvo `all` do Makefile gera todos os artefatos principais:

```text
.elf
.hex
.bin
.asm
.v
```

### `KAT=yes`

Passa uma variável para o Makefile:

```bash
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
```

Com `KAT=yes`, o Makefile compila `main_kat.c` e `kat_randombytes.c`, removendo o `main.c` normal e o `randombytes.c` normal.

### `BENCH_ROUNDS=30`

Passa o número de rodadas do benchmark:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=30 all
```

Isso define:

```c
BENCH_ROUNDS=30
```

e o firmware executa 30 rodadas do fluxo ML-KEM.

### `sbt "runMain ..."`

Executa uma classe Scala pelo SBT:

```bash
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
```

Esse comando regenera o `Murax.v`, já com o firmware `crystal_kyber.hex` carregado como imagem inicial da RAM.

### `make -B -C src/test/cpp/murax compile`

Recompila o simulador Verilator do Murax:

```bash
make -B -C src/test/cpp/murax compile
```

Esse comando usa o `Murax.v` gerado pelo SpinalHDL/SBT e cria o executável:

```text
src/test/cpp/murax/obj_dir/VMurax
```

### `./obj_dir/VMurax`

Executa a simulação Verilator:

```bash
cd src/test/cpp/murax
./obj_dir/VMurax
```

O firmware envia mensagens pela UART simulada, e o `VMurax` imprime essas mensagens no terminal.

## Fluxo completo para benchmark normal

Use este fluxo para medir ciclos:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=2 all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

Para 30 rodadas:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=30 all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

Observação: 30 rodadas podem demorar bastante no Verilator. Para evitar travamento ou execução infinita após a linha `done`, pode-se usar o script Python de parada automática ou rodar blocos menores.

## Comando para rodar e parar ao aparecer `done`

Depois de compilar o Verilator:

```bash
cd src/test/cpp/murax
python3 -c 'import subprocess,sys; p=subprocess.Popen(["./obj_dir/VMurax"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1); 
for line in p.stdout:
    print(line, end="")
    if line.strip()=="done":
        p.terminate()
        break'
```

Esse comando executa `VMurax`, imprime a saída no terminal e encerra o processo quando o firmware imprime:

```text
done
```

## Saída do benchmark normal

Exemplo de saída:

```text
BOOT
Murax ML-KEM-512 start
round=00000001
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
ss_match=0x00000001
pk_prefix=0ED44902DA2DF3F3
ct_prefix=2CD4558045559D6D
ss1_prefix=2ABA253D50DC878B
ss2_prefix=2ABA253D50DC878B
cycles_keypair=0x00D432EA
cycles_enc=0x0123443C
cycles_dec=0x0195176A
done
```

## Significado de cada métrica do benchmark

### `BOOT`

Mensagem do ambiente de simulação indicando que o SoC iniciou.

### `Murax ML-KEM-512 start`

Mensagem inicial do firmware normal. Confirma que o firmware de benchmark, não o KAT, está rodando.

### `round=00000001`

Número da rodada atual em hexadecimal, impresso com 8 dígitos.

Exemplos:

```text
round=00000001
round=00000002
round=0000000A
```

`0000000A` significa rodada 10.

### `keypair_ret`

Código de retorno de:

```c
PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk)
```

Valor esperado:

```text
keypair_ret=0x00000000
```

Interpretação:

- `0`: geração de chave pública e chave secreta executou sem erro.
- diferente de `0`: erro na chamada de keypair.

### `enc_ret`

Código de retorno de:

```c
PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(ct, ss1, pk)
```

Valor esperado:

```text
enc_ret=0x00000000
```

Interpretação:

- `0`: encapsulamento executou sem erro.
- diferente de `0`: erro no encapsulamento.

### `dec_ret`

Código de retorno de:

```c
PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss2, ct, sk)
```

Valor esperado:

```text
dec_ret=0x00000000
```

Interpretação:

- `0`: decapsulamento executou sem erro.
- diferente de `0`: erro no decapsulamento.

### `ss_match`

Resultado da comparação entre os dois segredos compartilhados:

```text
ss1 == ss2
```

Onde:

- `ss1`: segredo produzido pelo encapsulamento.
- `ss2`: segredo recuperado pelo decapsulamento.

Valor esperado:

```text
ss_match=0x00000001
```

Interpretação:

- `1`: o fluxo KEM funcionou para aquela rodada.
- `0`: encapsulamento e decapsulamento não concordaram, indicando falha funcional.

### `pk_prefix`

Primeiros 8 bytes da chave pública `pk`, impressos em hexadecimal.

Exemplo:

```text
pk_prefix=0ED44902DA2DF3F3
```

Uso:

- Verificar visualmente que a chave pública muda entre rodadas.
- Mostrar que o firmware não está imprimindo sempre o mesmo buffer fixo.
- Dar evidência parcial do conteúdo gerado sem imprimir os 800 bytes completos da chave pública.

### `ct_prefix`

Primeiros 8 bytes do ciphertext `ct`, impresso em hexadecimal.

Exemplo:

```text
ct_prefix=2CD4558045559D6D
```

Uso:

- Verificar visualmente que o encapsulamento gerou um ciphertext.
- Confirmar que diferentes rodadas tendem a produzir diferentes ciphertexts.
- Evitar imprimir os 768 bytes completos do ciphertext.

### `ss1_prefix`

Primeiros 8 bytes do segredo compartilhado produzido pelo encapsulamento.

Exemplo:

```text
ss1_prefix=2ABA253D50DC878B
```

Esse valor vem de:

```c
crypto_kem_enc(ct, ss1, pk)
```

### `ss2_prefix`

Primeiros 8 bytes do segredo compartilhado recuperado pelo decapsulamento.

Exemplo:

```text
ss2_prefix=2ABA253D50DC878B
```

Esse valor vem de:

```c
crypto_kem_dec(ss2, ct, sk)
```

Para uma rodada correta:

```text
ss1_prefix == ss2_prefix
ss_match == 1
```

### `cycles_keypair`

Quantidade de ciclos do VexRiscv gastos em:

```c
crypto_kem_keypair(pk, sk)
```

Exemplo:

```text
cycles_keypair=0x00D432EA
```

O valor é hexadecimal. Para converter para decimal:

```bash
printf "%d\n" 0x00D432EA
```

### `cycles_enc`

Quantidade de ciclos gastos no encapsulamento:

```c
crypto_kem_enc(ct, ss1, pk)
```

Exemplo:

```text
cycles_enc=0x0123443C
```

### `cycles_dec`

Quantidade de ciclos gastos no decapsulamento:

```c
crypto_kem_dec(ss2, ct, sk)
```

Exemplo:

```text
cycles_dec=0x0195176A
```

### `done`

Mensagem final indicando que o firmware terminou todas as rodadas configuradas.

Depois de imprimir `done`, o firmware entra em um loop infinito piscando GPIO. Por isso, na simulação, é normal precisar interromper o `VMurax` manualmente ou usar um wrapper que pare quando detectar `done`.

## Como converter ciclos para decimal

Exemplo:

```bash
printf "%d\n" 0x00D432EA
```

Resultado aproximado:

```text
13906666
```

Para converter para milhões de ciclos:

```text
13906666 ciclos = 13.906666 Mciclos
```

## Como transformar ciclos em tempo

Se a frequência do clock for conhecida:

```text
tempo_segundos = ciclos / frequencia_hz
```

Exemplo com 12 MHz:

```text
tempo = 13906666 / 12000000
tempo = 1.158888 s
```

Para artigo, normalmente é melhor reportar ciclos, porque ciclos independem da frequência final escolhida. Se também reportar tempo, informe explicitamente a frequência usada.

## Benchmark de 30 rodadas

Já foi usado o fluxo com 30 rodadas para calcular estatísticas.

Resumo salvo em:

```text
understanding/benchmarks/mlkem30_stats.md
```

Resultados obtidos:

```text
Keypair:       média 13.90628560 Mciclos
Encapsulation: média 19.09036840 Mciclos
Decapsulation: média 26.54159740 Mciclos
```

Observação importante: o `randombytes.c` normal é determinístico. Portanto, quando o firmware reinicia, a sequência de entradas se repete. Isso é aceitável para benchmark reprodutível, mas deve ser descrito como pseudoaleatório determinístico, não como aleatoriedade real.

## Fluxo completo para KAT

Use o KAT para validar corretude contra vetor oficial:

```bash
python3 scripts/run_mlkem512_kat.py
```

Esse script faz tudo:

- compila o gerador KAT do PQClean no PC;
- verifica o SHA-256 do KAT oficial;
- compila o firmware com `KAT=yes`;
- regenera o `Murax.v`;
- recompila o Verilator;
- executa o `VMurax`;
- valida automaticamente a saída.

## Fluxo manual para KAT

Se quiser executar manualmente:

```bash
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

## Saída do KAT

Exemplo:

```text
BOOT
Murax ML-KEM-512 NIST KAT start
seed_ret=0x00000000
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
seed_match=0x00000001
pk_match=0x00000001
sk_match=0x00000001
ct_match=0x00000001
ss_match=0x00000001
random_stream_ok=0x00000001
kat_pass=0x00000001
done
```

## Significado de cada métrica do KAT

### `Murax ML-KEM-512 NIST KAT start`

Confirma que o firmware KAT está rodando, não o firmware normal de benchmark.

### `seed_ret`

Retorno da chamada:

```c
randombytes(seed, 48)
```

Valor esperado:

```text
seed_ret=0x00000000
```

### `keypair_ret`, `enc_ret`, `dec_ret`

Mesma interpretação do benchmark normal: retornos das três chamadas principais do ML-KEM.

Todos devem ser:

```text
0x00000000
```

### `seed_match`

Compara a `seed` usada no Murax com a `seed` oficial do vetor KAT.

Valor esperado:

```text
seed_match=0x00000001
```

### `pk_match`

Compara a chave pública gerada no Murax com a chave pública oficial do vetor KAT.

Valor esperado:

```text
pk_match=0x00000001
```

### `sk_match`

Compara a chave secreta gerada no Murax com a chave secreta oficial do vetor KAT.

Valor esperado:

```text
sk_match=0x00000001
```

### `ct_match`

Compara o ciphertext gerado no Murax com o ciphertext oficial do vetor KAT.

Valor esperado:

```text
ct_match=0x00000001
```

### `ss_match`

No KAT, esse campo valida duas coisas:

- `ss1` gerado pelo encapsulamento bate com o `kat_ss` oficial.
- `ss2` recuperado pelo decapsulamento também bate com o `kat_ss` oficial.

Valor esperado:

```text
ss_match=0x00000001
```

### `random_stream_ok`

Indica se o firmware consumiu exatamente a quantidade esperada de bytes pseudoaleatórios do vetor KAT.

Valor esperado:

```text
random_stream_ok=0x00000001
```

Se for `0`, significa que o algoritmo pediu mais bytes do que o replay KAT fornece, ou seja, o fluxo de chamadas não corresponde ao esperado.

### `kat_pass`

Resultado final do KAT.

Valor esperado:

```text
kat_pass=0x00000001
```

Interpretação:

- `1`: todos os retornos foram zero, todos os vetores bateram e o fluxo de randombytes foi correto.
- `0`: alguma comparação falhou.

## Diferença entre benchmark e KAT

Benchmark normal:

- Usa `main.c`.
- Usa `kyber_implementation/randombytes.c`.
- Mede ciclos com `rdcycle`.
- Imprime `cycles_keypair`, `cycles_enc` e `cycles_dec`.
- Serve para desempenho.

KAT:

- Usa `main_kat.c`.
- Usa `kat_randombytes.c`.
- Usa vetores fixos oficiais do PQClean.
- Compara byte-a-byte `seed`, `pk`, `sk`, `ct` e `ss`.
- Serve para corretude.

## Limpeza do build

Para limpar o firmware:

```bash
make -C src/main/c/murax/crystal_kyber clean
```

O `clean` remove objetos e artefatos principais dentro de:

```text
src/main/c/murax/crystal_kyber/build/
```

Para forçar build limpo completo, use:

```bash
make -C src/main/c/murax/crystal_kyber clean
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=2 all
```

## Comandos mais usados

Benchmark normal com 2 rodadas:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=2 all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

Benchmark normal com 30 rodadas:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=30 all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

KAT automatizado:

```bash
python3 scripts/run_mlkem512_kat.py
```

KAT manual:

```bash
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

## Como saber se funcionou

Benchmark funcionando:

```text
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
ss_match=0x00000001
cycles_keypair=0x...
cycles_enc=0x...
cycles_dec=0x...
done
```

KAT funcionando:

```text
seed_match=0x00000001
pk_match=0x00000001
sk_match=0x00000001
ct_match=0x00000001
ss_match=0x00000001
random_stream_ok=0x00000001
kat_pass=0x00000001
done
```

Se `ss_match=1` no benchmark, o fluxo KEM funcionou naquela execução. Se `kat_pass=1` no KAT, a implementação no Murax bateu byte-a-byte com o vetor oficial do PQClean.
