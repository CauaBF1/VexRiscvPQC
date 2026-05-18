# ML-KEM-512 KAT no Murax/VexRiscv

Este documento descreve o que foi feito para validar a implementação bare-metal do ML-KEM-512 no Murax/VexRiscv usando KAT, isto é, Known Answer Test.

O objetivo do KAT é provar que o algoritmo executado dentro do SoC reproduz byte-a-byte uma saída de referência conhecida. Isso é mais forte do que apenas observar `ss_match=1`, porque `ss_match=1` mostra que encapsulamento e decapsulamento concordam entre si, mas não prova sozinho que os bytes produzidos são iguais aos de uma implementação de referência.

## Contexto inicial

A implementação usada no Murax veio de:

```text
/home/borgescaua/PQClean/crypto_kem/ml-kem-512/clean
```

Esses arquivos foram copiados para:

```text
kyber_implementation/
```

Para o benchmark funcional inicial, foi criado um `randombytes.c` simples baseado em `xorshift`. Ele é suficiente para bring-up, simulação e medição de ciclos, mas não é adequado como validação criptográfica formal. O motivo é que ele não reproduz o fluxo de aleatoriedade usado pelos vetores oficiais do NIST/PQClean.

Por isso foi criado um modo separado de KAT. Esse modo não substitui o benchmark normal. Ele existe apenas para validar corretude contra uma saída conhecida.

## Referência do PQClean

Dentro do PQClean, os arquivos relevantes para KAT são:

```text
/home/borgescaua/PQClean/test/crypto_kem/nistkat.c
/home/borgescaua/PQClean/test/common/nistkatrng.c
/home/borgescaua/PQClean/common/aes.c
/home/borgescaua/PQClean/common/aes.h
```

O `nistkat.c` é o programa de referência do PQClean para gerar o vetor KAT. Ele executa o fluxo:

```c
entropy_input[i] = i;      // 48 bytes: 0, 1, 2, ...
nist_kat_init(entropy_input, NULL, 256);
randombytes(seed, 48);
nist_kat_init(seed, NULL, 256);
crypto_kem_keypair(pk, sk);
crypto_kem_enc(ct, ss, pk);
crypto_kem_dec(ss_dec, ct, sk);
```

O `nistkatrng.c` implementa o RNG usado pelo KAT, baseado em AES-256-CTR-DRBG. Esse RNG depende de `aes.c` e `aes.h`.

## Verificação da referência no PC

Primeiro foi compilado o gerador KAT do próprio PQClean:

```bash
make -C /home/borgescaua/PQClean/test nistkat SCHEME=ml-kem-512 IMPLEMENTATION=clean
```

Depois foi executado o binário gerado:

```bash
/home/borgescaua/PQClean/bin/nistkat_ml-kem-512_clean | sha256sum
```

O hash obtido foi:

```text
c70041a761e01cd6426fa60e9fd6a4412c2be817386c8d0f3334898082512782
```

Esse valor bate com o campo `nistkat-sha256` presente em:

```text
/home/borgescaua/PQClean/crypto_kem/ml-kem-512/META.yml
```

Isso confirmou que o vetor gerado no PC era a referência correta do PQClean para `ml-kem-512/clean`.

## Decisão de implementação no Murax

Havia duas opções possíveis:

1. Portar o `nistkatrng.c` e também o `aes.c` para o firmware bare-metal.
2. Reproduzir no Murax o mesmo fluxo de bytes gerado pelo `nistkatrng.c` no PC.

Foi escolhida a segunda opção.

O motivo é prático: portar o AES-256-CTR-DRBG inteiro aumentaria o firmware apenas para alimentar o teste, sem validar o ML-KEM em si. Como o objetivo do KAT é verificar se `pk`, `sk`, `ct` e `ss` batem com a referência, basta garantir que o `randombytes` usado no Murax entregue exatamente os mesmos bytes que o NIST DRBG entregaria naquele vetor.

Assim, o Murax não precisa executar AES para o KAT. Ele apenas faz replay dos bytes oficiais já extraídos do fluxo NIST KAT.

## Arquivos criados ou alterados

Foram adicionados:

```text
src/main/c/murax/crystal_kyber/src/main_kat.c
src/main/c/murax/crystal_kyber/src/kat_randombytes.c
src/main/c/murax/crystal_kyber/src/kat_vectors.h
scripts/run_mlkem512_kat.py
```

Foi alterado:

```text
src/main/c/murax/crystal_kyber/makefile
```

## `kat_vectors.h`

O arquivo `kat_vectors.h` contém os vetores de referência extraídos do PQClean:

```text
kat_seed
kat_keypair_coins
kat_enc_coins
kat_pk
kat_sk
kat_ct
kat_ss
```

Esses vetores correspondem ao `count = 0` do NIST KAT para ML-KEM-512.

Os tamanhos esperados são:

```text
seed: 48 bytes
keypair_coins: 64 bytes
enc_coins: 32 bytes
pk: 800 bytes
sk: 1632 bytes
ct: 768 bytes
ss: 32 bytes
```

## `kat_randombytes.c`

O arquivo `kat_randombytes.c` substitui o `randombytes.c` normal quando o firmware é compilado com `KAT=yes`.

Ele implementa a mesma função esperada pelo PQClean:

```c
int randombytes(uint8_t *output, size_t n);
```

Em vez de usar `xorshift`, ele entrega os bytes de:

```text
kat_seed
kat_keypair_coins
kat_enc_coins
```

na mesma ordem em que o fluxo KAT precisa consumi-los.

Também existe controle de overflow:

```c
kat_randombytes_reset();
kat_randombytes_overflow();
```

Se o algoritmo pedir mais bytes do que o vetor KAT fornece, `random_stream_ok` vira `0`. Isso evita falso positivo caso o fluxo de chamadas mude ou algum consumo inesperado de aleatoriedade aconteça.

## `main_kat.c`

O arquivo `main_kat.c` é o firmware bare-metal específico do KAT. Ele faz:

```c
randombytes(seed, 48);
crypto_kem_keypair(pk, sk);
crypto_kem_enc(ct, ss1, pk);
crypto_kem_dec(ss2, ct, sk);
```

Depois compara byte-a-byte:

```text
seed == kat_seed
pk == kat_pk
sk == kat_sk
ct == kat_ct
ss1 == kat_ss
ss2 == kat_ss
```

O firmware imprime:

```text
seed_ret
keypair_ret
enc_ret
dec_ret
seed_match
pk_match
sk_match
ct_match
ss_match
random_stream_ok
kat_pass
```

O teste só passa se todos os retornos forem zero e todas as comparações forem `1`.

## Alteração no Makefile

Foi adicionado o parâmetro:

```make
KAT ?= no
```

Quando `KAT=yes`, o Makefile:

- remove `src/main.c` da compilação;
- usa `src/main_kat.c`;
- usa `src/kat_randombytes.c`;
- remove `kyber_implementation/randombytes.c`;
- adiciona `-DKAT_MODE=1`.

Isso evita conflito de símbolos, porque o firmware não pode ter dois `main()` nem duas implementações de `randombytes()`.

Comando para compilar o firmware KAT:

```bash
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
```

O firmware KAT compilado ocupou aproximadamente:

```text
73340 B de RAM / 128 KiB
```

Ou seja, permaneceu dentro da RAM configurada para o Murax.

## Erro encontrado

Na primeira tentativa, o KAT foi implementado usando um bloco contínuo de 96 bytes para alimentar o `randombytes` depois da `seed`.

A lógica inicial era equivalente a:

```text
seed: 48 bytes
coins: 96 bytes
```

Essa tentativa gerou o seguinte resultado no Verilator:

```text
seed_match=0x00000001
pk_match=0x00000001
sk_match=0x00000001
ct_match=0x00000000
ss_match=0x00000000
random_stream_ok=0x00000001
kat_pass=0x00000000
```

Interpretação do erro:

- `seed_match=1` mostrou que os primeiros 48 bytes estavam corretos.
- `pk_match=1` e `sk_match=1` mostraram que o `keypair` estava correto.
- `ct_match=0` e `ss_match=0` mostraram que o encapsulamento não estava recebendo os mesmos bytes aleatórios da referência.

## Causa do erro

A causa foi o funcionamento interno do NIST AES-CTR-DRBG usado pelo PQClean.

O erro foi assumir que os 64 bytes usados pelo `keypair` e os 32 bytes usados pelo `encapsulation` poderiam ser extraídos como um único bloco contínuo de 96 bytes:

```c
randombytes(coins, 96);
```

Mas esse não é o comportamento real do KAT.

No fluxo real, o PQClean chama:

```c
randombytes(coins, 64);  // dentro de crypto_kem_keypair
randombytes(coins, 32);  // dentro de crypto_kem_enc
```

O NIST DRBG atualiza seu estado ao final de cada chamada de `randombytes`. Portanto:

```text
randombytes(96)
```

não produz o mesmo fluxo que:

```text
randombytes(64)
randombytes(32)
```

Mesmo que a soma dos bytes seja 96, os bytes finais são diferentes porque existe atualização de estado entre as duas chamadas.

Por isso `pk` e `sk` estavam corretos, mas `ct` e `ss` falhavam.

## Correção aplicada

A correção foi separar os vetores aleatórios em duas partes:

```text
kat_keypair_coins: 64 bytes
kat_enc_coins: 32 bytes
```

Depois, `kat_randombytes.c` foi ajustado para entregar os bytes na ordem correta:

```text
kat_seed
kat_keypair_coins
kat_enc_coins
```

Com isso, o replay passou a respeitar as chamadas reais de `randombytes` feitas pelo ML-KEM:

```text
1. randombytes(seed, 48)
2. randombytes(..., 64) durante keypair
3. randombytes(..., 32) durante encapsulation
```

Após essa correção, o resultado passou a ser:

```text
seed_match=0x00000001
pk_match=0x00000001
sk_match=0x00000001
ct_match=0x00000001
ss_match=0x00000001
random_stream_ok=0x00000001
kat_pass=0x00000001
```

## Fluxo manual de execução

Para executar manualmente:

```bash
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

Saída esperada:

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

## Automação com Python

Foi criado o script:

```text
scripts/run_mlkem512_kat.py
```

Uso completo:

```bash
python3 scripts/run_mlkem512_kat.py
```

O script executa automaticamente:

- compilação/verificação do NIST KAT do PQClean no PC;
- validação do hash SHA-256 do KAT;
- compilação do firmware Murax com `KAT=yes`;
- regeneração do `Murax.v`;
- recompilação do Verilator;
- execução do simulador até a linha `done`;
- validação automática dos campos impressos pelo firmware.

O log é salvo em:

```text
understanding/benchmarks/mlkem512_kat.log
```

Para apenas validar uma simulação já compilada:

```bash
python3 scripts/run_mlkem512_kat.py --skip-build --skip-reference
```

Resultado observado no script:

```text
nistkat_sha256=c70041a761e01cd6426fa60e9fd6a4412c2be817386c8d0f3334898082512782
kat_pass=0x00000001
KAT validation passed: kat_pass=0x00000001
```

## Interpretação final

O resultado:

```text
kat_pass=0x00000001
```

significa que o Murax/VexRiscv executou o ML-KEM-512 e reproduziu byte-a-byte os valores oficiais do vetor NIST KAT `count = 0` do PQClean.

As comparações mais importantes são:

- `pk_match=1`: a chave pública gerada no Murax bate com a referência.
- `sk_match=1`: a chave secreta gerada no Murax bate com a referência.
- `ct_match=1`: o ciphertext gerado no Murax bate com a referência.
- `ss_match=1`: o segredo compartilhado gerado e recuperado no Murax bate com a referência.
- `random_stream_ok=1`: o firmware não consumiu bytes aleatórios além do previsto.

Essa validação é independente do `randombytes.c` usado no benchmark normal. Portanto, o fluxo recomendado é:

```text
KAT: validar corretude byte-a-byte contra referência oficial.
Benchmark: medir ciclos usando o firmware normal.
```

Para voltar ao benchmark normal, compile sem `KAT=yes`:

```bash
make -B -C src/main/c/murax/crystal_kyber BENCH_ROUNDS=2 all
```
