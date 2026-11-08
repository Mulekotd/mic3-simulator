# 1. Arquitetura do sistema

## 1.1 Componentes

| Componente  | Tecnologia sugerida                                   | Responsabilidade                                                                                                            |
| ----------- | ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `assembler` | C++                                                   | Léxico/sintaxe, tabela de símbolos, 2 passes, geração de binário e listagem (`.lst`)                                        |
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

## 1.3 Protocolo binário TX/RX

O contrato completo de quadros, comandos, mensagens, tamanhos, códigos e respostas está em [protocol.md](protocol.md). Os layouts canônicos ficam em [protocol/protocol.yaml](../protocol/protocol.yaml), fonte para a geração dos módulos C++ e Python na fase 6.

Cada quadro usa `TYPE:u8 | LEN:u16 | PAYLOAD`, com inteiros little-endian e payload de até 512 bytes. A GUI usa as listagens `.lst` e `.mlst` para traduzir endereços e palavras em texto; o fluxo TX/RX não carrega strings. `LOAD_*` envia uma imagem binária, `STEP_*` e `RUN` controlam a execução, e `CYCLE_STATE`, `PROGRAM_OUT`, `HALTED` e `STATS` relatam o resultado. O simulador atual ainda opera apenas pela CLI textual.
