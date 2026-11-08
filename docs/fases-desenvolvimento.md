# 1. Fases de desenvolvimento

## Fase 0: Fundamentos (≈ 1 semana)
- [X] Definir divisão de tarefas da equipe (montador / simulador / GUI / infra).
- [X] Criar repositório e convenções (estilo C++, estilo Python, commits, branches).
- [X] Decidir o empacotamento da GUI: opção A (host) ou B (imagem única) ([comunicação simulador ↔ GUI](arquitetura-sistema.md)).
- [X] Escolher a biblioteca de GUI (PyQt/PySide ou Tkinter).
- [ ] "Hello world" da comunicação: simulador ecoando bytes pelo `stdin`/`stdout` e a GUI Python recebendo, dentro de `docker run -i`.

**Entrega:** repositório estruturado e eco de bytes simulador ↔ Python funcionando.

## Fase 1: Especificação (≈ 1 semana)
- [X] `docs/isa.md`: tabela de instruções, formatos de bits, E/S mapeada em memória, exemplos.
- [X] `docs/microarch.md`: diagrama do caminho de dados, mapa de registradores, formato da microinstrução.
- [X] `docs/memory.md`: caches, memória, árbitro, parâmetros.
- [X] `docs/protocol.md` + `protocol/protocol.yaml`: quadros, comandos, mensagens e layouts ([protocolo TX/RX](protocol.md)).

**Entrega:** documentos escritos e consistentes; aprovação do grupo ainda pendente. **Critério técnico:** o exemplo em [isa.md](isa.md) pode ser codificado à mão e tem resultado previsível.

## Fase 2: Simulador base: MIC-1 de 32 bits, sem pipeline nem cache (≈ 2 semanas)
Reproduzir o MIC-1 do PDF com palavra de 32 bits e 32 registradores, executando o **microprograma fornecido**.
- [X] Estruturas: banco de registradores (R0 = 0), A/B latches, AMUX, ALU (N/Z), shifter, MAR/MBR, memória principal, Memória de Controle, MPC.
- [X] Ciclo de relógio: leitura de A/B, ALU/shifter, escrita em C, acesso à memória, próximo MPC.
- [X] Transcrever o microprograma do PDF para um arquivo na notação do microasm (`tools/microcode/mic1.masm`).
- [X] Carregador de binário na memória principal (palavras little-endian, limite de 4096 palavras).
- [X] E/S mapeada em memória (`PROGRAM_OUT`, `HALTED`).
- [X] Modo texto de depuração: traço por ciclo no `stderr` (`--trace`).
- [X] Validar com programas de exemplo que usem as 23 instruções.

**Entrega:** os sete exemplos montam e executam corretamente. **Critério atendido:** `all_instructions.asm` percorre as 23 instruções e produz `PROGRAM_OUT=3`; `docker compose up` montou e executou esse exemplo nos dois containers.

## Fase 3: Montador e microasm (≈ 2 semanas, paralelo à Fase 2)
**Montador:**
- [X] Analisador léxico e sintático (instruções, rótulos, comentários, diretivas `.word`, `.org`).
- [X] Tabela de símbolos e **montagem em dois passes** (rótulos de desvio e de dados).
- [X] Pseudo-instruções `HALT`, `OUTI`, `OUTC` ([ISA](isa.md)).
- [X] Verificação de faixa (`x` até 4095, `y` até 255) e mensagens de erro com linha/coluna.
- [X] Geração de binário (`.bin`) e **listagem** (`.lst`: endereço ↔ código ↔ fonte).
- [X] Programas de exemplo: soma, fatorial, Fibonacci, laço com vetor, chamada de procedimento (`CALL`/`RETN`).

**Microasm:**
- [X] *Parser* da notação do PDF (ver [microasm](../tools/microcode/README.md)), mais a palavra-chave `ifetch`.
- [X] Empacotamento dos campos conforme o formato de 36 bits.
- [X] Geração da Memória de Controle (`ctrl.bin`) e da listagem `.mlst` (com a fase Busca/Identifica/Executa de cada linha).

**Entrega:** `assembler prog.asm -o prog.bin -l prog.lst` monta os exemplos; `python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o ctrl.bin -l ctrl.mlst` gera a Memória de Controle e sua listagem. O simulador atual incorpora a mesma ROM por meio de `tools/microcode/build_rom.py`.

## Fase 4: Pipeline (≈ 2 semanas)
- [ ] Latches entre estágios com sinal de congelamento (`enable`).
- [ ] Propagação dos campos de controle da microinstrução pelos estágios.
- [ ] Detecção de hazard RAW + **stall** (o microprograma continua o mesmo).
- [ ] **Forwarding** da saída da ALU (se houver tempo).
- [ ] Hazard de controle: parar a busca da próxima microinstrução até N/Z serem conhecidos.
- [ ] Conferir que os programas de exemplo dão **o mesmo resultado** da Fase 2.

**Entrega:** simulador com pipeline. **Critério:** mesmo resultado funcional que a versão sem pipeline.

## Fase 5: Memória e caches (≈ 2 semanas)
- [ ] Modelo de memória principal com latência configurável e handshake.
- [ ] Bit **IF** na microinstrução e roteamento I-cache × D-cache.
- [ ] **I-cache** (dados, tags, validade, FSM de miss, preenchimento da linha).
- [ ] **D-cache** (leitura/escrita, política de escrita escolhida) e região de E/S não cacheável.
- [ ] Árbitro I-cache/D-cache.
- [ ] Interlock de MBR pendente e stall global por miss.
- [ ] Contadores: acessos, hits, misses, ciclos parados, ciclos e instruções (para o CPI).
- [ ] Conferir que os programas de exemplo continuam dando o mesmo resultado.

**Entrega:** simulador completo (pipeline + caches). **Critério:** resultados idênticos aos da Fase 4; estatísticas de cache coerentes.

## Fase 6: Camada TX/RX do simulador (≈ 1 semana)
- [ ] Leitura do `stdin` e escrita no `stdout` em modo binário, com `fflush` após cada quadro.
- [ ] Codificador/decodificador de quadros (`TYPE | LEN | PAYLOAD`).
- [ ] Comandos `LOAD_*`, `STEP_CYCLE`, `STEP_INSTR`, `RUN`, `PAUSE`, `RESET`, breakpoints, `READ_MEM`, `CONFIG`; leitura não bloqueante do RX durante `RUN`.
- [ ] Emissão de `CYCLE_STATE` por ciclo, com os modos *completo*, *compacto* e *só final*.
- [ ] Mensagens `PROGRAM_OUT`, `HALTED`, `STATS`, `ERROR`, `ACK`/`NACK`.
- [ ] Geração do cabeçalho C++ a partir de `protocol/protocol.yaml`.
- [ ] Verificação manual com um pequeno cliente Python (`tools/tx_rx_client.py`): carregar um `.bin`, executar e receber a saída.

**Entrega:** simulador controlável por bytes, dentro do container. **Critério:** o cliente carrega um `.bin`, executa e recebe `PROGRAM_OUT` e `HALTED` corretos.

## Fase 7: GUI em Python (≈ 2 semanas, pode iniciar com quadros gravados após a Fase 1)
- [ ] Módulo de protocolo em Python (gerado do `protocol.yaml`) e inicialização do simulador via `subprocess` (`docker run -i` ou binário local).
- [ ] **Thread de leitura** separada da thread da interface (sinais/filas), para a GUI não travar em `RUN`.
- [ ] Painéis de pipeline, microinstrução (`.mlst`), registradores, caches, memória (`READ_MEM`) e assembly (`.lst`), com destaque das mudanças do ciclo.
- [ ] Controles de execução, breakpoints, carga via `LOAD_*`, saída e métricas.
- [ ] Modo *replay* de quadros para desenvolver a GUI sem o simulador.

**Entrega:** execução ciclo a ciclo de forma amigável. **Critério:** um usuário novo consegue carregar e executar um programa sem ler o código-fonte.

## Fase 8: Integração, Docker final e documentação (≈ 1 semana)
- [ ] Imagens Docker finais e documentação do fluxo ponta a ponta (montar → carregar → executar).
- [ ] Demonstração com 3 a 5 programas representativos.
- [ ] Relatório de desempenho: CPI e taxa de acerto **com e sem cache/pipeline** (justifica o projeto).
- [ ] README, manual de ISA, manual do montador/microasm, manual do protocolo, manual da GUI.
- [ ] Ensaio da apresentação.
