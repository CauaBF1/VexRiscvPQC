# Benchmark ML-KEM-512 no Murax/VexRiscv

Configuração:
- Firmware bare-metal compilado com `BENCH_ROUNDS=10`.
- Simulação Verilator do alvo `MuraxCrystalKyberWithRamInit`.
- ISA alvo do firmware: `rv32i_zicsr`, ABI `ilp32`.
- Total: 3 blocos de 10 rodadas, 30 rodadas completas.
- Validação funcional: 30 ocorrências de `ss_match=0x00000001`.
- Desvio padrão reportado: amostral (`n - 1`).

Logs:
- `mlkem10_block1.log`
- `mlkem10_block2.log`
- `mlkem10_block3.log`

| Operação | n | Média (ciclos) | Mínimo | Máximo | Desvio padrão | Média (Mciclos) |
|---|---:|---:|---:|---:|---:|---:|
| Keypair | 30 | 13906285.60 | 13857308 | 13989868 | 35482.60 | 13.906286 |
| Encapsulation | 30 | 19090368.40 | 19035923 | 19171059 | 36605.32 | 19.090368 |
| Decapsulation | 30 | 26541597.40 | 26504420 | 26608486 | 30131.87 | 26.541597 |

Observação: como o `randombytes.c` bare-metal usa uma fonte determinística para reprodutibilidade no simulador, os mesmos 10 casos se repetem em cada bloco quando o firmware reinicia. As estatísticas acima representam 30 execuções do fluxo no simulador, mas apenas 10 entradas distintas por reinicialização.
