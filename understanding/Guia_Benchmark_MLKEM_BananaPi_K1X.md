# Guia de benchmark ML-KEM-512 na Banana Pi K1-X/X60

Este guia serve para executar o `mlkem-native` na Banana Pi K1-X/X60 e comparar o desempenho de diferentes conjuntos de instrucoes antes de decidir o que vale implementar ou aproximar no FPGA.

O teste mede o processador Linux da Banana Pi. Ele nao roda no Murax/VexRiscv do FPGA. Use os resultados como indicacao de quais classes de instrucao/aceleracao podem valer a pena.

## Contexto da placa testada

Na placa atual, os comandos retornaram:

```text
model: spacemit k1-x deb1 board
cpu: Spacemit(R) X60
linux: 6.6.36 riscv64
gcc: 13.2.0
isa: rv64imafdcv_zicbom_zicboz_zicntr_zicond_zicsr_zifencei_zihintpause_zihpm_zfh_zfhmin_zca_zcd_zba_zbb_zbc_zbs_zkt_zve32f_zve32x_zve64d_zve64f_zve64x_zvfh_zvfhmin_zvkt_sscofpmf_sstc_svinval_svnapot_svpbmt
```

Extensoes importantes para estes testes:

- `rv64gc`: baseline geral de Linux RISC-V 64-bit, sem as extensoes extras isoladas abaixo.
- `Zba/Zbb/Zbc/Zbs`: bit manipulation escalar. E mais parecido com adicionar instrucoes extras simples em um core FPGA.
- `Zicond`: operacoes condicionais inteiras. Pode reduzir branches em alguns codigos.
- `V/RVV`: vetor RISC-V. Deu ganho grande na Banana Pi, mas nao passa diretamente para o Murax atual.

## Preparar ambiente

Na Banana Pi:

```bash
cd VexRiscvPQC/external/mlkem-native

sudo apt update
sudo apt install -y build-essential make gcc python3 binutils linux-perf

cat /proc/device-tree/model
uname -a
lscpu
grep -m1 '^isa' /proc/cpuinfo
gcc --version

make clean
make host_info AUTO=1
```

O `host_info` deve mostrar:

```text
RVV: Host yes Compiler yes
```

No terminal pode aparecer como checkmarks em vez de `yes`.

## Regra importante

Sempre rode `make clean` entre configuracoes. O Makefile guarda variaveis como `OPT`, `AUTO` e `CYCLES` em `test/build/config.mk`; sem limpar, voce pode misturar builds.

Para isolar extensoes escalares, use:

```text
OPT=0 AUTO=0
```

Para testar o backend nativo RVV do `mlkem-native`, use:

```text
OPT=1 AUTO=1
```

ou `OPT=1 AUTO=0` com `CFLAGS` explicito.

## Criar diretorio de logs

```bash
mkdir -p ../../understanding/benchmarks/banana_k1x
```

Se preferir nao gravar no repo enquanto testa, use `/tmp/banana_k1x_logs`.

## Teste 1: baseline C puro

Este e o ponto de comparacao principal.

```bash
make clean
make run_func_512 OPT=0 AUTO=0 CFLAGS="-march=rv64gc -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/01_func_rv64gc_opt0.log

sudo make run_bench_512 CYCLES=PERF OPT=0 AUTO=0 CFLAGS="-march=rv64gc -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/01_bench_rv64gc_opt0.log
```

Resultado ja medido nesta placa:

```text
keypair cycles = 239774
encaps cycles  = 344336
decaps cycles  = 467939
```

## Teste 2: C puro com Zba/Zbb/Zbc/Zbs

Este teste tenta medir o ganho de bit manipulation escalar sem ativar RVV.

```bash
make clean
make run_func_512 OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zba_zbb_zbc_zbs -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/02_func_rv64gc_zb_opt0.log

sudo make run_bench_512 CYCLES=PERF OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zba_zbb_zbc_zbs -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/02_bench_rv64gc_zb_opt0.log
```

Depois veja se o compilador realmente usou instrucoes dessas extensoes:

```bash
riscv64-linux-gnu-objdump -d test/build/mlkem512/bin/bench_mlkem512 \
  | grep -E '\b(sh1add|sh2add|sh3add|andn|orn|xnor|clz|ctz|cpop|rol|ror|rev8|clmul|clmulh|clmulr|bset|bclr|bext|binv)\b' \
  | tee ../../understanding/benchmarks/banana_k1x/02_objdump_zb_used.log
```

Se o `grep` quase nao imprimir linhas, o compilador nao encontrou muitas oportunidades no C do ML-KEM, e o ganho esperado sera pequeno.

## Teste 3: C puro com Zicond

A placa anuncia `zicond`. Este teste isola essa extensao.

```bash
make clean
make run_func_512 OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/03_func_rv64gc_zicond_opt0.log

sudo make run_bench_512 CYCLES=PERF OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/03_bench_rv64gc_zicond_opt0.log
```

Se o GCC reclamar de `zicond`, pule este teste. Ele e util, mas nao essencial.

## Teste 4: C puro com Zb + Zicond

Este teste mede as extensoes escalares juntas.

```bash
make clean
make run_func_512 OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zba_zbb_zbc_zbs_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/04_func_rv64gc_zb_zicond_opt0.log

sudo make run_bench_512 CYCLES=PERF OPT=0 AUTO=0 CFLAGS="-march=rv64gc_zba_zbb_zbc_zbs_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/04_bench_rv64gc_zb_zicond_opt0.log
```

## Teste 5: backend nativo automatico do mlkem-native

Este teste ativa o backend nativo do `mlkem-native`. Na K1-X/X60, isso usa RVV para partes aritmeticas quando o runtime aceita.

```bash
make clean
make run_func_512 OPT=1 AUTO=1 \
  | tee ../../understanding/benchmarks/banana_k1x/05_func_auto_opt1.log

sudo make run_bench_512 CYCLES=PERF OPT=1 AUTO=1 \
  | tee ../../understanding/benchmarks/banana_k1x/05_bench_auto_opt1.log

sudo make run_bench_components_512 CYCLES=PERF OPT=1 AUTO=1 \
  | tee ../../understanding/benchmarks/banana_k1x/05_components_auto_opt1.log
```

Resultado ja medido nesta placa:

```text
keypair cycles = 172431
encaps cycles  = 184787
decaps cycles  = 236005
```

Ganho contra o baseline C puro:

```text
keypair: 1.39x, reducao de 28.1%
encaps:  1.86x, reducao de 46.3%
decaps:  1.98x, reducao de 49.6%
```

O `bench_components` confirmou uso nativo:

```text
mlk_ntt_native cycles=3329
mlk_intt_native cycles=3498
mlk_rej_uniform_native cycles=1197
```

## Teste 6: RVV explicito

Este teste evita depender da deteccao automatica e passa RVV diretamente ao GCC.

```bash
make clean
make run_func_512 OPT=1 AUTO=0 CFLAGS="-march=rv64gcv -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/06_func_rv64gcv_opt1.log

sudo make run_bench_512 CYCLES=PERF OPT=1 AUTO=0 CFLAGS="-march=rv64gcv -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/06_bench_rv64gcv_opt1.log

sudo make run_bench_components_512 CYCLES=PERF OPT=1 AUTO=0 CFLAGS="-march=rv64gcv -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/06_components_rv64gcv_opt1.log
```

O resultado deve ser parecido com `OPT=1 AUTO=1`.

## Teste 7: RVV + extensoes escalares

Este teste mostra se existe ganho adicional alem do backend RVV.

```bash
make clean
make run_func_512 OPT=1 AUTO=0 CFLAGS="-march=rv64gcv_zba_zbb_zbc_zbs_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/07_func_rv64gcv_zb_zicond_opt1.log

sudo make run_bench_512 CYCLES=PERF OPT=1 AUTO=0 CFLAGS="-march=rv64gcv_zba_zbb_zbc_zbs_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/07_bench_rv64gcv_zb_zicond_opt1.log

sudo make run_bench_components_512 CYCLES=PERF OPT=1 AUTO=0 CFLAGS="-march=rv64gcv_zba_zbb_zbc_zbs_zicond -mabi=lp64d" \
  | tee ../../understanding/benchmarks/banana_k1x/07_components_rv64gcv_zb_zicond_opt1.log
```

Se o GCC reclamar de `zicond`, rode sem ele:

```bash
CFLAGS="-march=rv64gcv_zba_zbb_zbc_zbs -mabi=lp64d"
```

## Extrair resultados dos logs

Depois de rodar os testes:

```bash
grep -H -E 'keypair cycles|encaps cycles|decaps cycles' \
  ../../understanding/benchmarks/banana_k1x/*bench*.log
```

Preencha a tabela:

| Teste | Configuracao | OPT | AUTO | keypair | encaps | decaps | Observacao |
|---|---|---:|---:|---:|---:|---:|---|
| 1 | `rv64gc` | 0 | 0 | 239774 | 344336 | 467939 | baseline C puro |
| 2 | `rv64gc_zba_zbb_zbc_zbs` | 0 | 0 |  |  |  | escalares bitmanip |
| 3 | `rv64gc_zicond` | 0 | 0 |  |  |  | opcional |
| 4 | `rv64gc_zba_zbb_zbc_zbs_zicond` | 0 | 0 |  |  |  | escalares juntas |
| 5 | automatico | 1 | 1 | 172431 | 184787 | 236005 | backend RVV ativo |
| 6 | `rv64gcv` | 1 | 0 |  |  |  | RVV explicito |
| 7 | `rv64gcv_zba_zbb_zbc_zbs_zicond` | 1 | 0 |  |  |  | RVV + escalares |

## Como decidir o melhor desempenho

Use o menor numero de ciclos para cada operacao:

- melhor `keypair`: menor `keypair cycles`
- melhor `encaps`: menor `encaps cycles`
- melhor `decaps`: menor `decaps cycles`

Calcule ganho contra o baseline:

```text
ganho = baseline / resultado
reducao_percentual = (baseline - resultado) * 100 / baseline
```

Exemplo com RVV automatico ja medido:

```text
keypair: 239774 / 172431 = 1.39x
encaps:  344336 / 184787 = 1.86x
decaps:  467939 / 236005 = 1.98x
```

## Interpretacao para FPGA

- `OPT=0` com `rv64gc` e `rv64gc_zba_zbb_zbc_zbs` ajuda a estimar o valor de instrucoes escalares extras.
- `OPT=1` com RVV mostra potencial de aceleracao vetorial, mas nao e diretamente portavel para o Murax atual.
- Se `Zba/Zbb/Zbc/Zbs` quase nao mudarem o resultado, priorize aceleradores especificos de ML-KEM em vez de implementar bitmanip generico.
- O resultado RVV indica que NTT/INTT, base multiplication e rejection sampling sao bons candidatos para aceleracao.

Para o Murax/VexRiscv deste repo, mantenha a comparacao separada: o fluxo bare-metal atual continua em `src/main/c/murax/crystal_kyber`, com alvo conceitual RV32I/Zicsr.
