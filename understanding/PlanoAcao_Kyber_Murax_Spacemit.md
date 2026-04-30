# Plano de Ação: Crystal-Kyber com benchmark em Murax/VexRisc-V e SpacemiT K1-X DEB1

## Objetivo

Executar uma implementação de Crystal-Kyber em um ambiente bare-metal baseado em VexRisc-V/Murax e produzir benchmarks comparáveis com uma execução em software na SpacemiT K1-X DEB1 rodando BianbuOS.

O objetivo prático não é só "rodar". É conseguir responder, com dados:

- qual implementação do algoritmo é viável em bare-metal no Murax
- quanto custa em ciclos no VexRisc-V/Murax
- quanto custa em tempo no K1-X com BianbuOS
- se vale a pena comparar "na placa" contra "no VexRisc-V" e em que termos essa comparação é justa

## Decisão estrutural

Há dois alvos de execução diferentes, e eles não devem ser confundidos:

- `K1-X + BianbuOS`: benchmark em software no processador da placa
- `Murax/VexRisc-V`: benchmark do mesmo algoritmo em um SoC RISC-V pequeno, inicialmente em simulação Verilator e depois, se fizer sentido, em hardware

Resumo prático:

- se a pergunta é "quanto esse algoritmo custa na minha placa hoje?", rode no `K1-X`
- se a pergunta é "quanto esse algoritmo custa em um SoC RISC-V pequeno que eu controlo e posso modificar?", rode no `Murax/VexRisc-V`

Comparar os dois faz sentido, mas como comparação de plataformas diferentes, não como números diretamente equivalentes.

## Recomendação de estratégia

Executar em três camadas:

1. `Referência funcional no K1-X com BianbuOS`
2. `Port bare-metal mínimo no Murax/VexRisc-V`
3. `Benchmark padronizado com mesma carga de trabalho nos dois lados`

Essa ordem reduz risco. Primeiro valida a implementação do algoritmo. Depois resolve o ambiente bare-metal. Só então mede.

## Hipóteses atuais

- já existe um scaffold bare-metal para Murax em [src/main/c/murax/crystal_kyber](</home/borgescaua/VexRiscvPQC/src/main/c/murax/crystal_kyber>)
- já existe um target Murax com preload da firmware em [Murax.scala](/home/borgescaua/VexRiscvPQC/src/main/scala/vexriscv/demo/Murax.scala:541)
- o `Murax` padrão é pequeno e limitado; para Kyber, memória e custo de multiplicações são pontos críticos
- ainda não existe código Crystal-Kyber dentro deste repositório

## Critério de sucesso

O trabalho só deve ser considerado concluído quando todos os itens abaixo estiverem verdadeiros:

- existe uma implementação do algoritmo integrada ao fluxo Murax
- a firmware bare-metal executa e imprime resultados verificáveis por UART
- existe ao menos um benchmark reproduzível em ciclos no VexRisc-V/Murax
- existe ao menos um benchmark reproduzível no K1-X sob BianbuOS
- existe um documento com metodologia, parâmetros e números coletados

## Fase 1: Escolha e saneamento da implementação

### Meta

Selecionar uma implementação que possa ser compilada:

- em userspace no `K1-X + BianbuOS`
- em bare-metal no `Murax/VexRisc-V`

### Ações

- localizar uma implementação de Crystal-Kyber em C, preferencialmente com:
  - dependência mínima de libc
  - código determinístico
  - separação clara entre `keygen`, `encaps`, `decaps`
  - suporte a build simples com `gcc/clang`
- evitar, na primeira integração, código que dependa fortemente de:
  - alocação dinâmica
  - syscalls
  - pthreads
  - APIs de temporização do SO
  - intrinsics específicos de x86/ARM
- registrar a origem do código e a variante exata:
  - `Kyber512`, `Kyber768` ou `Kyber1024`
  - versão de referência, otimizada, ou pqm4-like

### Entregáveis

- diretório da implementação escolhida
- nota curta com:
  - origem
  - licença
  - variante
  - dependências
  - risco de port para bare-metal

## Fase 2: Rodar primeiro no K1-X com BianbuOS

### Meta

Usar o `K1-X` como baseline funcional e de performance em software.

### Ações

- compilar a implementação em userspace no BianbuOS
- criar um binário de teste com:
  - vetores fixos
  - validação de corretude
  - benchmark repetido de:
    - `keygen`
    - `encaps`
    - `decaps`
- coletar:
  - tempo total por operação
  - média, mínimo e máximo
  - tamanho do binário final

### Medidas sugeridas

- usar `clock_gettime` no BianbuOS
- fixar entrada e número de iterações
- desabilitar prints dentro da região medida
- se houver ruído alto:
  - aumentar número de iterações
  - descartar warm-up inicial

### Entregáveis

- programa de benchmark para `K1-X`
- primeiro relatório de corretude
- primeira linha de base de performance

## Fase 3: Port bare-metal para Murax/VexRisc-V

### Meta

Substituir o stub atual por uma integração real da implementação em C.

### Ações

- copiar os fontes necessários para [src/main/c/murax/crystal_kyber/src](</home/borgescaua/VexRiscvPQC/src/main/c/murax/crystal_kyber/src>)
- adaptar includes e dependências para o ambiente bare-metal
- remover ou substituir:
  - chamadas de SO
  - alocação dinâmica, se existir
  - dependências de `stdio`, exceto o mínimo necessário
- usar UART apenas para:
  - banner
  - resultado de corretude
  - métricas finais
- manter uma rotina de benchmark com `rdcycle`

### Critérios de aceitação

- compila para `rv32`
- gera `.elf` e `.hex`
- inicializa no Murax
- executa pelo menos uma rotina real do Kyber sem travar

### Observação importante

Se a implementação for pesada demais para o Murax atual, o problema não é "o código está errado". Pode ser simplesmente:

- RAM insuficiente
- stack insuficiente
- custo alto de multiplicações em `rv32i`

## Fase 4: Ajuste de arquitetura do Murax

### Meta

Adaptar o SoC para que o benchmark seja realista.

### Decisões a validar

- `RAM`: `128 kB` pode ainda ser pouco; medir uso real
- `stack`: aumentar se necessário
- `rv32i` vs `rv32im`
- `MuraxConfig.default` vs `MuraxConfig.fast`

### Recomendação

Começar com:

- RAM maior
- benchmark com `MuraxConfig.fast`
- avaliar uma variante com multiplicação em hardware se o algoritmo depender fortemente disso

### Entregáveis

- tabela curta de configurações testadas
- motivo técnico para manter ou trocar a configuração

## Fase 5: Benchmark padronizado

### Meta

Produzir números comparáveis com metodologia consistente.

### Métricas mínimas

- ciclos por operação no `Murax/VexRisc-V`
- tempo por operação no `K1-X`
- tamanho do binário
- uso de RAM estimado

### Operações medidas

- `keygen`
- `encaps`
- `decaps`

### Regras de medição

- entradas fixas ou seed fixa
- sem logs dentro da janela de medição
- repetir múltiplas vezes
- registrar versão do compilador e flags

### Formato sugerido

| Plataforma | Configuração | Operação | Iterações | Métrica | Valor |
|---|---|---|---:|---|---:|
| Murax | rv32i / 128 kB | keygen | 100 | ciclos médios | ... |
| Murax | rv32i / 128 kB | encaps | 100 | ciclos médios | ... |
| Murax | rv32i / 128 kB | decaps | 100 | ciclos médios | ... |
| K1-X | BianbuOS userspace | keygen | 1000 | ns médios | ... |
| K1-X | BianbuOS userspace | encaps | 1000 | ns médios | ... |
| K1-X | BianbuOS userspace | decaps | 1000 | ns médios | ... |

## Fase 6: Decidir onde o benchmark "vale mais"

### Pergunta

`Rodo na placa ou dentro do VexRisc-V?`

### Resposta curta

Rode nos dois, mas com objetivos diferentes.

### Quando rodar no K1-X

- para validar a implementação original rapidamente
- para ter baseline de software real em uma placa funcional
- para comparação prática de throughput/latência

### Quando rodar no VexRisc-V/Murax

- para estudar custo em um núcleo pequeno e controlável
- para medir ciclos de forma direta
- para apoiar decisões de customização do hardware
- para preparar terreno para aceleradores ou instruções customizadas

### Quando a comparação é útil

Ela é útil se for apresentada como:

- `baseline de software em processador da placa`
- `baseline de software bare-metal em SoC VexRisc-V`

Ela não deve ser apresentada como disputa direta de performance entre plataformas equivalentes.

## Backlog técnico imediato

### Tarefa 1

Encontrar e escolher a implementação de Crystal-Kyber.

### Tarefa 2

Rodar essa implementação primeiro no `K1-X + BianbuOS`.

### Tarefa 3

Integrar os fontes ao scaffold em `src/main/c/murax/crystal_kyber/src`.

### Tarefa 4

Trocar `kyber_stub()` por chamadas reais.

### Tarefa 5

Medir uso de memória e validar se `128 kB` basta.

### Tarefa 6

Se necessário, criar uma variante Murax com:

- mais RAM
- configuração `fast`
- multiplicação em hardware

### Tarefa 7

Padronizar benchmark e registrar resultados em um CSV ou Markdown.

## Riscos principais

- implementação escolhida não portar limpo para bare-metal
- estouro de RAM/stack no Murax
- custo proibitivo em `rv32i` puro
- comparação injusta entre ambientes diferentes
- benchmark contaminado por UART, logs ou warm-up

## Mitigações

- escolher primeiro a implementação mais simples, não a mais otimizada
- validar funcionalidade no K1-X antes do port bare-metal
- usar seeds fixas e benchmark sem prints na região crítica
- medir memória cedo
- tratar `K1-X` e `Murax` como linhas de base distintas

## Próxima ação recomendada

Próximo passo único e correto:

- buscar uma implementação em `C` de Crystal-Kyber que compile sem dependências pesadas e testá-la primeiro no `K1-X + BianbuOS`

Só depois disso vale a pena fechar o port no `Murax/VexRisc-V`.

## Estado atual do repositório

- scaffold Murax para Kyber criado em [src/main/c/murax/crystal_kyber](</home/borgescaua/VexRiscvPQC/src/main/c/murax/crystal_kyber>)
- target Murax com preload criado em [Murax.scala](/home/borgescaua/VexRiscvPQC/src/main/scala/vexriscv/demo/Murax.scala:541)
- ainda falta integrar a implementação real do algoritmo
