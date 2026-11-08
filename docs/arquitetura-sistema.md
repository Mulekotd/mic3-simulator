# 1. Arquitetura do sistema

## 1.1 Componentes

| Componente  | Tecnologia sugerida                                   | Responsabilidade                                                                                                            |
| ----------- | ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `assembler` | C++ (ou Python, se permitido)                         | Léxico/sintaxe, tabela de símbolos, 2 passes, geração de binário e listagem (`.lst`)                                        |
| `microasm`  | C++ ou Python (script)                                | Lê o microprograma, gera a Memória de Controle e a listagem (`.mlst`) (ver [microasm](arquitetura-sistema.md))              |
| `simulator` | C++17                                                 | Modelo ciclo-a-ciclo da CPU, caches, memória, camada TX/RX (stdin/stdout)                                                   |
| `gui`       | Python (PyQt/PySide ou Tkinter)                       | Visualização e controle (step, run, reset, breakpoints); decodifica os quadros TX/RX                                        |
| `docker`    | Dockerfiles (e, opcionalmente, compose só para build) | Imagens do `assembler`, do `simulator` e, se necessário, da GUI (ver [comunicação simulador ↔ GUI](arquitetura-sistema.md)) |

## 1.2 Comunicação simulador ↔ GUI

**Requisito:** nada de rede. O simulador funciona como um dispositivo com um **canal RX** (recebe comandos) e um **canal TX** (transmite estado e saída), ambos como **fluxo de bytes**.

**Canal escolhido:** pipes (`stdin` = RX, `stdout` = TX). Funcionam no host ou com `docker run -i`, sem rede. Logs usam `stderr` para não misturar bytes com o protocolo.

**Como funciona:** a GUI inicia o simulador como processo filho e conversa pelos pipes.

```python
sim = subprocess.Popen(
    ["docker", "run", "-i", "--rm",
     "-v", "./shared:/shared", "mic3-sim", "--ctrl", "/shared/ctrl.bin"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    bufsize=0)
# GUI -> simulador (RX):  sim.stdin.write(frame)
# simulador -> GUI (TX):  sim.stdout.read(n)
```

**Regras para não ter dor de cabeça:**

1. **Nunca usar `-t`** no `docker run` (o TTY altera bytes, como `\n`, e faz eco).
2. O simulador escreve **somente quadros do protocolo** no `stdout`; todo log vai para `stderr`.
3. **Descarregar o buffer** (`fflush`) após cada quadro enviado.
4. A GUI lê o `stdout` em uma **thread separada**, para a interface não travar e o pipe nunca encher.
5. Durante `RUN`, o simulador verifica o RX **sem bloquear** a cada N ciclos, para atender `PAUSE`.
6. Se o simulador rodar nativamente no Windows, colocar `stdin`/`stdout` em **modo binário**.

**Empacotamento:** por padrão, simulador e montador ficam em Docker e a GUI roda no host, iniciando o simulador com `docker run -i`. Se a GUI também precisar de container, usar imagem única e os mesmos pipes.

No modo de linha de comando atual, o simulador recebe `programa.bin` como argumento. O binário contém palavras de 32 bits little-endian, carregadas a partir do endereço 0; a carga por `LOAD_*` será usada na futura interface TX/RX.
O script `tools/program_loader.py` valida o arquivo binário e inicia esse executável. A transferência via `LOAD_*` substituirá essa forma de inicialização quando o protocolo RX estiver implementado.

## 1.3 Protocolo binário TX/RX (rascunho)

**Convenções:** campos multibyte em **little-endian**; tamanhos fixos; sem strings (a GUI traduz códigos em texto). Como pipes entregam os bytes de forma confiável, **não há CRC nem ressincronização**: se um quadro vier inválido (tipo desconhecido, `LEN` absurdo), o simulador responde `ERROR` e a GUI reinicia a sessão.

**Quadro (frame):**

```
| TYPE (1B) | LEN (2B) | PAYLOAD (LEN bytes) |
```

**Comandos (GUI → simulador, canal RX do simulador):**

| TYPE            | Nome                | Payload                                                              |
| --------------- | ------------------- | -------------------------------------------------------------------- |
| `0x01`          | `LOAD_BEGIN`        | endereço inicial (4B), nº de palavras (4B)                           |
| `0x02`          | `LOAD_DATA`         | offset em palavras (4B), palavras de 32 bits (até 64 por quadro)     |
| `0x03`          | `LOAD_END`          | (vazio); o simulador confere a contagem                              |
| `0x10`          | `STEP_CYCLE`        | (vazio): executa 1 ciclo                                             |
| `0x11`          | `STEP_INSTR`        | (vazio): até concluir 1 instrução da ISA                             |
| `0x12`          | `RUN`               | máx. de ciclos (4B), modo de relato (1B: completo/compacto/só final) |
| `0x13`          | `PAUSE`             | (vazio)                                                              |
| `0x14`          | `RESET`             | (vazio)                                                              |
| `0x15` / `0x16` | `SET_BP` / `CLR_BP` | endereço de PC (4B)                                                  |
| `0x20`          | `READ_MEM`          | endereço (4B), nº de palavras (2B)                                   |
| `0x21`          | `CONFIG`            | parâmetros de cache/latência (campos fixos)                          |

**Mensagens (simulador → GUI, canal TX do simulador):**

| TYPE   | Nome          | Conteúdo                                                                              |
| ------ | ------------- | ------------------------------------------------------------------------------------- |
| `0x81` | `ACK`/`NACK`  | código do comando (1B), status (1B)                                                   |
| `0x90` | `CYCLE_STATE` | estado do ciclo (layout abaixo)                                                       |
| `0x91` | `PROGRAM_OUT` | saída produzida pelo programa (ver [ISA](isa.md), E/S mapeada em memória)             |
| `0x92` | `MEM_DATA`    | endereço (4B), palavras lidas                                                         |
| `0x93` | `HALTED`      | ciclos (4B), instruções (4B)                                                          |
| `0x94` | `STATS`       | acessos/hits/misses das caches, ciclos parados (inteiros; o CPI é calculado pela GUI) |
| `0x9F` | `ERROR`       | código de erro (1B)                                                                   |

**Layout de `CYCLE_STATE` (rascunho; fixar em `docs/protocol.md`):**

| Campo         | Tamanho           | Observação                                                            |
| ------------- | ----------------- | --------------------------------------------------------------------- |
| `cycle`       | 4B                | contador de ciclos                                                    |
| `mpc`         | 2B                | endereço da microinstrução atual na Memória de Controle               |
| `microword`   | 8B                | palavra bruta da microinstrução (36 bits); a GUI decodifica os campos |
| `flags`       | 1B                | bit 0 = N, bit 1 = Z                                                  |
| `mar` / `mbr` | 4B + 4B           | registradores de interface com a memória                              |
| `pipe_valid`  | 1B                | um bit por estágio (bolha = 0)                                        |
| `pipe_uop[3]` | 3×2B              | endereço da microinstrução em cada estágio                            |
| `stall`       | 1B                | 0 = nenhum, 1 = RAW, 2 = desvio, 3 = miss I-cache, 4 = miss D-cache…  |
| `regs[32]`    | 32×4B             | R0 a R31 (PC, AC, SP e IR já estão aqui)                              |
| `icache_ev`   | 1B + 2B + 4B      | resultado (nenhum/hit/miss), índice, tag                              |
| `dcache_ev`   | 1B + 2B + 4B + 1B | resultado, índice, tag, flags (dirty/write)                           |

São cerca de 174 bytes por ciclo. Em `RUN`, o modo _compacto_ envia `CYCLE_STATE` só em eventos relevantes (stall, miss, fim de instrução) ou a cada N ciclos; o modo _só final_ envia apenas `HALTED`/`STATS`. O modo _completo_ (todo ciclo) fica para `STEP_*`.

**Texto legível sem strings no protocolo:** o simulador manda apenas `mpc`/`microword`; a **GUI** mostra o texto da microinstrução lendo a listagem do microasm (`.mlst`: endereço → microinstrução simbólica). O mesmo vale para a instrução assembly corrente, via `.lst` do montador.

**Fonte única do protocolo:** manter a definição (códigos, tamanhos, layout) em **um arquivo** (`protocol/protocol.yaml`) e gerar dele o cabeçalho C++ e o módulo Python, para simulador e GUI nunca divergirem.

## 1.4 O que é o microasm e para que serve

O **montador** converte assembly da ISA em código de máquina para a Memória Principal. O **microasm** converte o microprograma simbólico em palavras da Memória de Controle e gera uma listagem `.mlst` para a GUI.

**A entrada do microasm é o próprio microprograma**, por exemplo:

```
0: mar:=pc; rd;
1: pc:=pc + 1; rd;
2: ir:=mbr; if n then goto 28;
3: tir:=lshift(ir + ir); if n then goto 19;
```

Exemplo do que ele faz com a linha 1 (`pc:=pc + 1; rd;`):

| AMUX        | COND            | ALU      | SH  | MBR | MAR | RD  | WR  | ENC | C       | B       | A       | ADDR | IF  |
| ----------- | --------------- | -------- | --- | --- | --- | --- | --- | --- | ------- | ------- | ------- | ---- | --- |
| 0 (A latch) | 00 (sem desvio) | 00 (A+B) | 00  | 0   | 0   | 1   | 0   | 1   | R1 (PC) | R6 (+1) | R1 (PC) | 0    | 1   |

O microasm interpreta atribuições, operações de memória e desvios; codifica os campos da microinstrução de 36 bits e gera `ctrl.bin` e `.mlst`.

O campo `IF` não existe na notação do PDF (ver [memória e cache](decisoes-projeto.md)); o microasm aceita uma palavra-chave extra (ex.: `ifetch`) para marcá-lo nas linhas 0 e 1.

**O microasm é uma ferramenta de apoio**; o requisito obrigatório é o montador da ISA.
