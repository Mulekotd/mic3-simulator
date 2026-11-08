# 1. Estrutura sugerida do repositório

```text
├── docs/                  especificações e plano
├── protocol/              definição compartilhada do protocolo
├── assembler/             montador da ISA
├── microasm/              montador do microcódigo
├── simulator/             CPU, memória, cache e TX/RX
├── gui/                   interface Python
├── tools/                 cliente de depuração
├── shared/                arquivos trocados com os containers
└── examples/              programas de demonstração
```

---

# 2. Riscos e mitigação

| Risco                                                                      | Impacto                            | Mitigação                                                                                                               |
| -------------------------------------------------------------------------- | ---------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| ISA/microprograma fornecidos divergem da nossa microarquitetura de 32 bits | Microprograma não roda como no PDF | Manter o opcode nos 4 bits mais altos e `AMASK`/`SMASK` como constantes ([ISA](isa.md)); validar na Fase 2 |
| Microprograma do MIC-1 sofre com hazards no pipeline                       | Resultados errados ou CPI alto     | Interlock em hardware (stall) em vez de alterar o microcódigo                                                           |
| "Qualquer programa assembly" é irrestrito                                  | Escopo infinito                    | Definir a gramática do dialeto aceito                                                                                   |
| GUI atrasa por depender do simulador                                       | Gargalo no final                   | Desenvolver a GUI com quadros gravados (modo _replay_) a partir da Fase 1                                               |
| Protocolo diverge entre C++ e Python (endianness, tamanhos)                | Bugs difíceis de achar             | `protocol.yaml` como fonte única, com geração de código                                                                 |
| Bytes corrompidos no pipe (TTY, buffer, logs no `stdout`)                  | GUI mostra lixo ou trava           | Sem `-t`, `fflush`, logs só no `stderr`, modo binário, thread de leitura                                                |
| GUI desktop em Docker (display)                                            | Atraso de infraestrutura           | Opção A (GUI no host) como padrão                                                                                       |
| Prazo curto                                                                | Entrega incompleta                 | Ordem de corte: forwarding → modo compacto → D-cache (manter só I-cache)                                                |

---

# 3. Divisão sugerida de responsabilidades (equipe de 3 a 4)

| Papel                      | Foco principal                                  |
| -------------------------- | ----------------------------------------------- |
| **A: ISA/Montador**        | Especificação da ISA, montador, microasm        |
| **B: Simulador núcleo**    | CPU, microprograma, pipeline, hazards           |
| **C: Memória/Cache/Infra** | Caches, memória, árbitro, Docker                |
| **D: GUI/Protocolo**       | Protocolo TX/RX (ambos os lados), GUI em Python |

Com 3 pessoas, junte **C** e **D** ou distribua a infra entre todos.

---

# 4. Cronograma resumido

| Semana | Atividade                                                        |
| ------ | ---------------------------------------------------------------- |
| 1      | Fase 0 (fundamentos)                                             |
| 2      | Fase 1 (especificação)                                           |
| 3 a 4  | Fase 2 (simulador base) e Fase 3 (montador/microasm) em paralelo |
| 5 a 6  | Fase 4 (pipeline)                                                |
| 7 a 8  | Fase 5 (caches); início da GUI com quadros gravados              |
| 9      | Fase 6 (camada TX/RX)                                            |
| 9 a 10 | Fase 7 (GUI em Python)                                           |
| 11     | Fase 8 (integração, relatório, ensaio)                           |

Ajuste conforme o prazo da disciplina; a ordem importa mais que as durações.

---

# 5. Critérios de pronto (Definition of Done)

- [ ] Simulador e montador funcionam em Docker; a GUI se comunica com o simulador por stdin/stdout (opção A ou B da [comunicação simulador ↔ GUI](arquitetura-sistema.md)).
- [ ] Um programa `.asm` arbitrário dentro do dialeto documentado é montado sem intervenção manual.
- [ ] O binário é carregado manualmente na memória principal (`LOAD_*`) e executado passo a passo.
- [ ] A GUI mostra, a cada ciclo: microinstrução, estágios do pipeline, registradores, caches e stalls.
- [ ] A saída final do programa é exibida (`PROGRAM_OUT`).
- [ ] Resultados idênticos entre configurações (sem/com pipeline, sem/com cache).
- [ ] Relatório com CPI e taxa de acerto comparando as configurações.
- [ ] Documentação de ISA, microarquitetura, protocolo TX/RX e uso.
