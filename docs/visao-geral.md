# 1. Visão geral

**Objetivo:** desenvolver uma microarquitetura completa composta por:

1. **Montador:** converte programas em assembly para a ISA fornecida (23 macroinstruções).
2. **Simulador:** modela a microarquitetura e executa o microprograma ciclo a ciclo.
3. **GUI:** exibe de forma amigável a execução ciclo a ciclo (estágios do pipeline, registradores, caches, microinstrução atual).
4. **Empacotamento:** simulador e montador em Docker; comunicação simulador ↔ GUI por **TX/RX sobre stdin/stdout**.

**Fluxo de uso:**

```
programa.asm --[Montador]--> programa.bin --> Memória Principal
                                                                  |
Microprograma --[Microasm]--> Memória de Controle                 |
                                    |                             |
GUI (Python) <==== TX/RX (bytes, stdin/stdout) ====> [Simulador C++]
```

## Premissa sobre as memórias

| Memória                                 | O que guarda                                                | Como é carregada                                               |
| --------------------------------------- | ----------------------------------------------------------- | -------------------------------------------------------------- |
| **Memória de Controle** (control store) | As **microinstruções** (microprograma que interpreta a ISA) | Gerada pelo microasm e carregada na inicialização do simulador |
| **Memória Principal**                   | Programa, dados e pilha                                     | Arquivo `.bin` na CLI; futuramente, comando `LOAD_*` da GUI    |

As caches (I-cache e D-cache) ficam entre a CPU e a Memória Principal, ou seja, tratam instruções e dados do programa do usuário. A busca de microinstruções na Memória de Controle **não passa pelas caches**.
