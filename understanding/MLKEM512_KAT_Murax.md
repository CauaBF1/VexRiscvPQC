# KAT ML-KEM-512 no Murax com mlkem-native

Este documento descreve o fluxo atual de validação KAT do ML-KEM-512 no Murax/VexRiscv. O KAT não depende mais de PQClean e não usa mais replay de `randombytes`. A referência agora vem do submodule `external/mlkem-native`.

## Objetivo

O objetivo do KAT é provar que o firmware bare-metal executado no Murax gera exatamente os mesmos bytes esperados para ML-KEM-512 em um vetor conhecido:

- chave pública `pk`;
- chave secreta `sk`;
- ciphertext `ct`;
- shared secret `ss`.

Quando `kat_pass=0x00000001`, o Murax/VexRiscv executou o algoritmo e reproduziu byte-a-byte o vetor usado como referência.

## Fonte da referência

A referência vem do submodule:

```text
external/mlkem-native/
```

O gerador usado é criado pelo próprio `mlkem-native`:

```text
external/mlkem-native/test/build/mlkem512/bin/gen_KAT512
```

O script do projeto que automatiza isso é:

```text
scripts/generate_mlkem512_kat_vectors.py
```

Esse script executa:

```bash
make -C external/mlkem-native kat_512
```

Depois roda o binário `gen_KAT512`, calcula o SHA-256 da saída completa e compara com o valor esperado do `mlkem-native`:

```text
0a2d61707a68c0cac7b2a5005def19994e4e2a25cf8dc512b1254cbaa25473c0
```

Se o hash não bater, o script falha e não aceita os vetores como referência.

## Arquivo gerado

O arquivo gerado para o firmware é:

```text
src/main/c/murax/crystal_kyber/src/kat_vectors.h
```

Ele contém:

```c
kat_keypair_coins
kat_enc_coins
kat_pk
kat_sk
kat_ct
kat_ss
```

`kat_pk`, `kat_sk`, `kat_ct` e `kat_ss` são extraídos do primeiro vetor produzido por `gen_KAT512`.

`kat_keypair_coins` e `kat_enc_coins` são derivados no script Python para alimentar as APIs determinísticas do `mlkem-native`:

```c
mlkem_keypair_derand(pk, sk, kat_keypair_coins);
mlkem_enc_derand(ct, ss1, pk, kat_enc_coins);
mlkem_dec(ss2, ct, sk);
```

Esse modelo elimina a necessidade de interceptar `randombytes`. O KAT passa a controlar diretamente os bytes de entrada usados por keypair e encapsulation. O arquivo `randombytes.c` ainda pode ser linkado como símbolo de fallback exigido por `mlkem_native.c`, mas ele não define o vetor KAT.

## Firmware KAT

O firmware específico de KAT é:

```text
src/main/c/murax/crystal_kyber/src/main_kat.c
```

Ele executa este fluxo:

1. chama `mlkem_keypair_derand` com `kat_keypair_coins`;
2. chama `mlkem_enc_derand` com `kat_enc_coins`;
3. chama `mlkem_dec` com o ciphertext gerado;
4. compara `pk`, `sk`, `ct` e `ss` com os arrays de `kat_vectors.h`;
5. imprime uma métrica por linha pela UART.

O arquivo antigo `kat_randombytes.c` não faz parte do fluxo atual. O KAT não usa replay de `randombytes`; ele usa as APIs determinísticas do `mlkem-native`.

## Execução automática

Para gerar os vetores, compilar o firmware, regenerar o Murax, compilar o Verilator e validar a saída:

```bash
python3 scripts/run_mlkem512_kat.py
```

Esse script faz:

1. gera `kat_vectors.h` a partir do `mlkem-native`;
2. compila o firmware com `KAT=yes`;
3. executa `sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"`;
4. executa `make -B -C src/test/cpp/murax compile`;
5. roda `src/test/cpp/murax/obj_dir/VMurax`;
6. para a simulação quando encontra `done`;
7. valida automaticamente os campos esperados.

Para não regenerar os vetores e usar o `kat_vectors.h` já existente:

```bash
python3 scripts/run_mlkem512_kat.py --skip-reference
```

Para não recompilar e rodar apenas o `VMurax` existente:

```bash
python3 scripts/run_mlkem512_kat.py --skip-reference --skip-build
```

## Execução manual

Fluxo manual completo:

```bash
python3 scripts/generate_mlkem512_kat_vectors.py
make -B -C src/main/c/murax/crystal_kyber KAT=yes all
sbt "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"
make -B -C src/test/cpp/murax compile
cd src/test/cpp/murax
./obj_dir/VMurax
```

A simulação fica rodando após imprimir `done`, porque o firmware entra em loop infinito no final. Para automação, use `scripts/run_mlkem512_kat.py`, que encerra o processo quando recebe `done`.

## Saída esperada

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
```

## Significado dos campos

`keypair_ret=0x00000000` significa que `mlkem_keypair_derand` retornou sucesso.

`enc_ret=0x00000000` significa que `mlkem_enc_derand` retornou sucesso.

`dec_ret=0x00000000` significa que `mlkem_dec` retornou sucesso.

`pk_match=0x00000001` significa que a chave pública gerada no Murax é idêntica ao vetor de referência.

`sk_match=0x00000001` significa que a chave secreta gerada no Murax é idêntica ao vetor de referência.

`ct_match=0x00000001` significa que o ciphertext gerado no Murax é idêntico ao vetor de referência.

`ss_match=0x00000001` significa que o shared secret do encapsulamento e o shared secret recuperado na decapsulação batem com o vetor de referência.

`kat_pass=0x00000001` é o resultado global. Ele só vale `1` quando todos os retornos são zero e todas as comparações byte-a-byte passam.

## Dependências atuais

O KAT atual depende de:

```text
external/mlkem-native/
scripts/generate_mlkem512_kat_vectors.py
scripts/run_mlkem512_kat.py
src/main/c/murax/crystal_kyber/src/main_kat.c
src/main/c/murax/crystal_kyber/src/kat_vectors.h
src/main/c/murax/crystal_kyber/src/mlkem_native_murax_config.h
```

O KAT atual não depende de:

```text
PQClean
kyber_implementation
kat_randombytes.c
```
