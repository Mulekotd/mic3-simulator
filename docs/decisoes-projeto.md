# 1. Decisões de projeto

## 1.1 Parâmetros base

| Parâmetro               | Valor                                                                     |
| ----------------------- | ------------------------------------------------------------------------- |
| Palavra de dados        | **32 bits**, complemento de dois                                          |
| Registradores           | **32** (R0 a R31), **R0 fixo em 0** (escritas em R0 são ignoradas)        |
| Instrução da ISA        | 1 palavra de 32 bits                                                      |
| Memória principal       | 4096 palavras de 32 bits (endereço de 12 bits, por palavra), configurável |
| Memória de Controle     | 256 palavras de **36 bits**                                               |
| PC inicial / SP inicial | PC = 0; SP = `0xFFC` (pilha cresce para baixo)                            |

## 1.2 ISA (macroinstruções)

A especificação da ISA está em [isa.md](isa.md).

## 1.3 Microarquitetura

**Mapa dos 32 registradores** (o MIC-1 original tem 16; aqui o banco cresce para 32 e os campos A/B/C passam de 4 para 5 bits). Proposta:

| Registrador | Nome             | Função                                                  |
| ----------- | ---------------- | ------------------------------------------------------- |
| R0          | ZERO             | constante 0; escrita ignorada                           |
| R1          | PC               | contador de programa                                    |
| R2          | AC               | acumulador                                              |
| R3          | SP               | ponteiro de pilha                                       |
| R4          | IR               | registrador de instrução                                |
| R5          | TIR              | cópia temporária do IR (para identificar o opcode)      |
| R6          | ONE              | constante +1 (somente leitura)                          |
| R7          | MONE             | constante −1 (somente leitura)                          |
| R8          | AMASK            | `0x00000FFF`, extrai o campo `x` (somente leitura)      |
| R9          | SMASK            | `0x000000FF`, extrai o campo `y` (somente leitura)      |
| R10 a R15   | A, B, C, D, E, F | temporários do microprograma (o fornecido usa só o `a`) |
| R16 a R31   | (livres)         | reservados; o microprograma fornecido não os usa        |

**Formato da microinstrução (36 bits):**

| Campo   | Bits  | Função                                                                                            |
| ------- | ----- | ------------------------------------------------------------------------------------------------- |
| AMUX    | 1     | 0 = A latch, 1 = MBR (entrada A da ALU)                                                           |
| COND    | 2     | 00 sem desvio, 01 se N, 10 se Z, 11 sempre                                                        |
| ALU     | 2     | 00 A+B, 01 A and B, 10 A, 11 not A                                                                |
| SH      | 2     | 00 sem deslocamento, 01 desloca à direita, 10 desloca à esquerda                                  |
| MBR     | 1     | carrega MBR com a saída do shifter                                                                |
| MAR     | 1     | carrega MAR com o B latch                                                                         |
| RD / WR | 1 + 1 | requisição de leitura / escrita na memória                                                        |
| ENC     | 1     | habilita escrita no registrador de destino                                                        |
| C       | 5     | registrador de destino (barramento C)                                                             |
| B       | 5     | registrador-fonte do barramento B                                                                 |
| A       | 5     | registrador-fonte do barramento A                                                                 |
| ADDR    | 8     | endereço do desvio                                                                                |
| **IF**  | 1     | **novo:** indica que o `RD` é uma busca de instrução (ver [memória e cache](decisoes-projeto.md)) |

> Codificações de COND, ALU e SH seguem o livro do Tanenbaum; confirmar com o material da disciplina, pois os PDFs anexados não as listam.

**Pipeline (estilo MIC-3):** (1) leitura para A/B; (2) ALU e shifter; (3) escrita via C e em MAR/MBR. Diretrizes:

- **Manter o microprograma fornecido sem alterações**, resolvendo os hazards **em hardware** (interlock com stall e, se houver tempo, forwarding).
- Desvios que dependem de N/Z podem gerar stalls, pois as flags saem da ALU.
- Leitura de memória usa 2 ciclos no caso base; misses de cache e MBR pendente exigem stall por interlock.

## 1.4 Memória e cache

- Memória principal com latência configurável (N ciclos) e handshake `req/ready`.
- **I-cache** e **D-cache**: direct-mapped, linha de 4 palavras, bloqueantes.
- D-cache: write-through + no-write-allocate (inicial).
- Árbitro de acesso à memória (prioridade para a D-cache).
- Parâmetros (tamanho, linha, latência) configuráveis via argumento ou comando `CONFIG`.
- **Roteamento I-cache × D-cache:** no MIC-1 a busca de instrução e o acesso a dados usam **o mesmo par MAR/MBR**. Para saber qual cache atender, a microinstrução ganha o bit **IF**, ligado nas linhas 0 e 1 do microprograma (`mar:=pc; rd;` e `pc:=pc+1; rd;`). `IF=1` vai para a I-cache; `IF=0` vai para a D-cache.
- **Região de E/S** (`0xFFD` a `0xFFF`): não cacheável; o acesso é tratado direto pelo simulador.
- **Coerência I/D:** não tratada. Assume-se que o programa não altera o próprio código.

## 1.5 Pontos a confirmar com o professor/grupo

1. **ISA de acumulador × "baseada em registradores".** A ISA do PDF não expõe registradores ao programador (só `ac`, `pc` e `sp`). O banco de 32 registradores fica como parte da **microarquitetura** (R16 a R31 sem uso). Confirmar se a ISA deve permanecer assim ou ser estendida.
2. **Largura do campo `x`.** O plano mantém **12 bits** (como no PDF). Alargar exige mudar só `AMASK`, o montador e o tamanho da memória, sem tocar no microprograma.
3. **Endereços de E/S e `HALT`** (`0xFFD` a `0xFFF`) e o **SP inicial** (`0xFFC`): proposta nossa, não consta nos PDFs.
4. **Codificação dos campos COND/ALU/SH** (ver [microarquitetura](decisoes-projeto.md)).
5. **Bit IF** para rotear I-cache/D-cache: proposta nossa; outra opção é separar por região de endereços.
6. **Empacotamento da GUI** (opção A ou B, [comunicação simulador ↔ GUI](arquitetura-sistema.md)).
