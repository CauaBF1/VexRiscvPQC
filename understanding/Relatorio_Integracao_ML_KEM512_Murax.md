> Nota: este relatorio descreve a integracao antiga baseada em PQClean. O fluxo atual foi migrado para o submodule `external/mlkem-native`; veja `understanding/README_MLKEM512_Murax.md` e `understanding/MLKEM512_KAT_Murax.md`.

# Relatorio de Integracao do ML-KEM-512 ao SoC Murax/VexRiscv em Ambiente Bare-Metal

## 1. Objetivo

Este relatorio descreve o procedimento realizado para integrar uma implementacao de ML-KEM-512, proveniente do repositorio PQClean, ao SoC Murax baseado em VexRiscv. O objetivo da integracao foi executar o fluxo minimo de encapsulamento de chaves em ambiente bare-metal, sem sistema operacional e sem dependencia de biblioteca C hospedada.

O fluxo funcional integrado executa as tres rotinas principais do mecanismo KEM:

- geracao do par de chaves;
- encapsulamento;
- decapsulamento;
- comparacao entre os segredos compartilhados produzidos por encapsulamento e decapsulamento.

## 2. Origem e Organizacao da Implementacao Criptografica

Os arquivos da implementacao criptografica foram copiados a partir do diretorio:

```text
/home/borgescaua/PQClean/crypto_kem/ml-kem-512/clean
```

Esses arquivos foram colocados no diretorio local:

```text
kyber_implementation/
```

O conteudo copiado corresponde a variante `clean` do ML-KEM-512, mantendo a nomenclatura original do PQClean. A preservacao dos nomes originais, como `PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair`, evita modificacoes invasivas no codigo criptografico e facilita a rastreabilidade em relacao ao codigo de referencia.

Os arquivos principais presentes em `kyber_implementation/` incluem:

- `api.h`
- `kem.c` e `kem.h`
- `indcpa.c` e `indcpa.h`
- `poly.c` e `poly.h`
- `polyvec.c` e `polyvec.h`
- `ntt.c` e `ntt.h`
- `reduce.c` e `reduce.h`
- `cbd.c` e `cbd.h`
- `symmetric-shake.c` e `symmetric.h`
- `verify.c` e `verify.h`
- `params.h`

Tambem foram incorporadas dependencias comuns do PQClean:

- `fips202.c`
- `fips202.h`
- `compat.h`
- `randombytes.h`

Adicionalmente, foi criado o arquivo:

```text
kyber_implementation/randombytes.c
```

Esse arquivo implementa uma fonte deterministica de bytes pseudoaleatorios, necessaria porque o PQClean fornece a interface `randombytes`, mas espera que a aplicacao ou plataforma forneca sua implementacao concreta. A implementacao adotada utiliza um gerador `xorshift` simples, adequado para execucao funcional e reproducibilidade em ambiente experimental. Essa fonte nao deve ser considerada criptograficamente segura para uso produtivo.

## 3. Integracao com o Firmware Bare-Metal do Murax

O firmware de integracao esta localizado em:

```text
src/main/c/murax/crystal_kyber/
```

Esse diretorio contem o ambiente bare-metal usado para compilar o codigo C, gerar a imagem hexadecimal e permitir o preload da firmware na memoria interna do Murax.

### 3.1 Arquivo principal da aplicacao

O arquivo:

```text
src/main/c/murax/crystal_kyber/src/main.c
```

foi alterado para substituir o `stub` inicial por uma execucao real do ML-KEM-512.

Foram declarados buffers estaticos para:

- chave publica (`pk`);
- chave secreta (`sk`);
- texto cifrado (`ct`);
- segredo compartilhado gerado no encapsulamento (`ss1`);
- segredo compartilhado recuperado no decapsulamento (`ss2`).

A decisao de usar buffers estaticos reduz a pressao sobre a pilha e torna o uso de memoria mais previsivel, o que e relevante em ambientes bare-metal com RAM limitada.

O programa executa, em sequencia:

```c
PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(ct, ss1, pk);
PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss2, ct, sk);
```

Apos a execucao, o firmware compara `ss1` e `ss2`. O valor `ss_match=1` indica que o segredo compartilhado produzido pelo encapsulamento coincide com o segredo recuperado pelo decapsulamento, validando funcionalmente o fluxo KEM.

O firmware tambem imprime mensagens por UART para indicar:

- inicio da execucao;
- codigo de retorno de cada funcao;
- resultado da comparacao dos segredos;
- campos inicialmente destinados a medicao de ciclos.

## 4. Configuracao de Memoria e Linker Script

O arquivo:

```text
src/main/c/murax/crystal_kyber/src/linker.ld
```

foi configurado para uma regiao de RAM de 128 KiB iniciando em `0x80000000`, coerente com o alvo Murax utilizado para esta integracao.

Foram definidos explicitamente:

- stack de 32 KiB;
- heap de 16 KiB.

Essas reservas sao necessarias porque a implementacao do ML-KEM-512 utiliza buffers temporarios consideraveis e porque a implementacao `fips202.c` incorporada a partir do PQClean faz uso de alocacao dinamica para contextos SHAKE/SHA3.

## 5. Suporte Bare-Metal a Rotinas de Biblioteca C

O arquivo:

```text
src/main/c/murax/crystal_kyber/src/support.c
```

foi criado para suprir funcoes normalmente fornecidas por uma biblioteca C em ambiente hospedado. Como o firmware e compilado com `-nostdlib`, essas rotinas precisam existir localmente.

Foram implementadas:

- `memcpy`;
- `memset`;
- `memcmp`;
- `malloc`;
- `free`;
- `exit`.

### 5.1 Implementacao de `malloc` e `free`

A primeira necessidade de heap surgiu porque `fips202.c`, proveniente do PQClean, utiliza `malloc` e `free` para alocar e liberar contextos internos de SHAKE/SHA3. Em ambiente bare-metal, nao ha alocador padrao disponivel.

Foi implementado um alocador simples baseado nos simbolos `_heap_start` e `_heap_end` definidos no linker script. O ponteiro global `heap_current` avanca a cada chamada de `malloc`.

Posteriormente, o comportamento de `free()` foi ajustado. A versao inicial apenas ignorava a liberacao, o que poderia esgotar o heap durante chamadas repetidas de SHAKE/SHA3. A versao atual armazena um pequeno cabecalho antes de cada bloco alocado, contendo o tamanho total da alocacao. Com isso, `free()` consegue recuperar memoria quando a liberacao ocorre em ordem LIFO, isto e, quando o ultimo bloco alocado e o primeiro a ser liberado.

Essa abordagem e suficiente para o padrao de uso observado nos contextos do `fips202.c`, sem introduzir a complexidade de um alocador geral com lista livre.

### 5.2 Criacao de `exit`

O arquivo `fips202.c` chama `exit(111)` em caminhos de erro relacionados a falhas de alocacao. Como a firmware e linkada sem biblioteca C padrao, a ausencia de `exit` causava erro de link.

Foi adicionada uma implementacao bare-metal de `exit(int status)` que ignora o codigo de saida e entra em um loop infinito. Esse comportamento e apropriado para o ambiente atual, pois nao existe sistema operacional para receber um codigo de retorno de processo.

## 6. Configuracao da Toolchain

O firmware bare-metal precisa ser compilado por uma toolchain cruzada RISC-V, pois o binario gerado deve conter instrucoes RISC-V de 32 bits e nao codigo nativo da maquina host.

No ambiente utilizado, a toolchain encontrada foi:

```text
/opt/riscv-ricstar/bin/riscv32-none-elf-gcc
```

Por isso, o `makefile` em:

```text
src/main/c/murax/crystal_kyber/makefile
```

foi ajustado para usar:

```make
RISCV_NAME ?= riscv32-none-elf
RISCV_PATH ?= /opt/riscv-ricstar
```

Esse ajuste permite que os comandos de compilacao, `objcopy` e `objdump` sejam resolvidos corretamente como:

```text
/opt/riscv-ricstar/bin/riscv32-none-elf-gcc
/opt/riscv-ricstar/bin/riscv32-none-elf-objcopy
/opt/riscv-ricstar/bin/riscv32-none-elf-objdump
```

## 7. Uso de `rv32i_zicsr`

O alvo definido para o SoC foi mantido como RV32I, isto e, sem extensao de multiplicacao/divisao em hardware. Entretanto, foi necessario usar a string de arquitetura:

```text
rv32i_zicsr
```

Essa extensao explicita nao altera o objetivo de executar em um nucleo RV32I sem multiplicacao. Ela apenas informa ao assembler que instrucoes de acesso a registradores de controle e status, como `csrw`, sao validas.

O motivo pratico para essa alteracao foi que o codigo de inicializacao `crt.S` utiliza instrucoes CSR para configurar registradores como `mie` e `mstatus`. Toolchains RISC-V mais recentes exigem que a extensao `zicsr` seja declarada explicitamente quando tais instrucoes sao usadas.

Sem essa alteracao, a montagem falhava com erro indicando que a instrucao `csrw` requer a extensao `zicsr`.

## 8. Alteracao no Gerador Murax

O arquivo:

```text
src/main/scala/vexriscv/demo/Murax.scala
```

foi estendido com o objeto:

```scala
MuraxCrystalKyberWithRamInit
```

Esse objeto gera uma instancia do Murax com:

- 128 KiB de RAM interna;
- preload da imagem hexadecimal `crystal_kyber.hex`.

A configuracao relevante e:

```scala
onChipRamSize = 128 kB
onChipRamHexFile = "src/main/c/murax/crystal_kyber/build/crystal_kyber.hex"
```

Com isso, a firmware ML-KEM-512 e carregada diretamente na RAM interna do SoC durante a geracao do RTL, permitindo sua execucao no simulador Verilator.

## 9. Artefatos Gerados

Apos a compilacao, foram gerados os seguintes artefatos em:

```text
src/main/c/murax/crystal_kyber/build/
```

Principais arquivos:

- `crystal_kyber.elf`
- `crystal_kyber.hex`
- `crystal_kyber.bin`
- `crystal_kyber.asm`
- `crystal_kyber.v`

O arquivo `crystal_kyber.hex` e o artefato usado pelo gerador Murax para inicializar a memoria interna do SoC.

Durante a compilacao, o uso reportado de RAM foi aproximadamente 69 KiB de uma regiao total de 128 KiB, ou cerca de 53% da memoria configurada.

## 10. Validacao Funcional Realizada

O fluxo executado foi:

```sh
make -B -C src/main/c/murax/crystal_kyber all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
timeout 240s ./obj_dir/VMurax
```

A simulacao Verilator inicializou o SoC, executou a firmware e produziu a seguinte saida funcional:

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
cycles_keypair=0x00000000
cycles_enc=0x00000000
cycles_dec=0x00000000
round=00000002
keypair_ret=0x00000000
enc_ret=0x00000000
dec_ret=0x00000000
ss_match=0x00000001
pk_prefix=04057F74C371DD4B
ct_prefix=4A2B285A662F095D
ss1_prefix=40BDC502B4D863BD
ss2_prefix=40BDC502B4D863BD
cycles_keypair=0x00000000
cycles_enc=0x00000000
cycles_dec=0x00000000
done
```

Os retornos iguais a zero indicam que as chamadas principais do ML-KEM-512 foram executadas com sucesso. O campo `ss_match=0x00000001` indica que o segredo compartilhado obtido por decapsulamento corresponde ao segredo produzido pelo encapsulamento.

Portanto, a integracao funcional do ML-KEM-512 ao Murax bare-metal foi concluida com sucesso.

### 10.1 Interpretacao dos Prefixos Impressos

Para aumentar a confianca de que o algoritmo foi realmente executado, a firmware imprime os primeiros 8 bytes de quatro buffers importantes em cada rodada:

- `pk_prefix`: prefixo da chave publica gerada por `crypto_kem_keypair`;
- `ct_prefix`: prefixo do texto cifrado gerado por `crypto_kem_enc`;
- `ss1_prefix`: prefixo do segredo compartilhado produzido pelo encapsulamento;
- `ss2_prefix`: prefixo do segredo compartilhado recuperado pelo decapsulamento.

Esses prefixos nao sao usados como teste criptografico completo, mas fornecem evidencia operacional importante. Eles mostram que os buffers foram preenchidos com dados nao triviais e que o fluxo nao apenas retornou codigos de sucesso sem produzir saidas.

Na primeira rodada, a saida relevante foi:

```text
pk_prefix=0ED44902DA2DF3F3
ct_prefix=2CD4558045559D6D
ss1_prefix=2ABA253D50DC878B
ss2_prefix=2ABA253D50DC878B
```

Na segunda rodada, a saida foi:

```text
pk_prefix=04057F74C371DD4B
ct_prefix=4A2B285A662F095D
ss1_prefix=40BDC502B4D863BD
ss2_prefix=40BDC502B4D863BD
```

Ha tres observacoes importantes:

- dentro de cada rodada, `ss1_prefix` e `ss2_prefix` sao iguais;
- entre as rodadas, os prefixos mudam;
- os prefixos de `pk` e `ct` tambem mudam entre as rodadas.

A igualdade entre `ss1_prefix` e `ss2_prefix` dentro de uma mesma rodada confirma visualmente o mesmo resultado indicado por `ss_match=0x00000001`: o segredo produzido por encapsulamento foi recuperado corretamente por decapsulamento.

A diferenca entre as rodadas mostra que o firmware nao esta apenas imprimindo valores fixos ou buffers estaticos nao atualizados. Como o `randombytes.c` implementado usa um gerador deterministico com estado interno, cada chamada avanca esse estado. Portanto, a segunda rodada deve produzir uma nova chave publica, um novo texto cifrado e um novo segredo compartilhado. Foi exatamente isso que ocorreu.

Assim, mesmo que a contagem de ciclos ainda nao esteja funcional, a execucao criptografica esta funcionalmente demonstrada pelos seguintes sinais combinados:

- as tres funcoes principais retornaram `0`;
- `ss_match` retornou `1` nas duas rodadas;
- `ss1_prefix` e `ss2_prefix` coincidiram dentro de cada rodada;
- os prefixos mudaram entre a primeira e a segunda rodada;
- a firmware chegou ate `done`.

Portanto, o problema atual esta restrito a instrumentacao de desempenho. Ele nao invalida a integracao funcional do ML-KEM-512 no Murax bare-metal.

## 11. Contratempo Atual: Medicao Funcional de Ciclos

Embora o fluxo criptografico esteja funcional, a medicao de ciclos ainda nao esta operacional. O firmware utiliza a instrucao `rdcycle` para tentar medir os ciclos consumidos por `keypair`, `enc` e `dec`, mas os valores impressos foram:

```text
cycles_keypair=0x00000000
cycles_enc=0x00000000
cycles_dec=0x00000000
```

Isso indica que, na configuracao atual do Murax/VexRiscv ou do ambiente de simulacao, o contador lido por `rdcycle` nao esta fornecendo uma contagem util.

Assim, o principal contratempo restante e implementar uma medicao funcional de ciclos. As proximas alternativas tecnicas sao:

- utilizar o temporizador memory-mapped existente no Murax;
- habilitar ou ajustar o contador de ciclos no VexRiscv;
- instrumentar a simulacao Verilator para medir intervalos de execucao por outro mecanismo.

Essa pendencia nao impede a execucao funcional do ML-KEM-512, mas impede a obtencao de metricas quantitativas confiaveis de desempenho no estado atual.
