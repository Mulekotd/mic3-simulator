# Protocolo binário TX/RX, versão 1

Esta é a especificação da comunicação entre a GUI Raylib em C++ e o simulador. O executável usa este fluxo com `--protocol` por pipes ou `--serve-local /run/mic3/simulator.sock` por socket Unix; a CLI textual continua disponível com um arquivo `.bin`. No Compose, o serviço `simulator` usa o socket local e permanece ativo entre sessões da GUI. Os IDs, tipos e layouts canônicos estão em [protocol/protocol.yaml](../protocol/protocol.yaml), que gera os IDs e enums C++ por `tools/generate_protocol_cpp.py`.

## Transporte e quadro

No modo `--protocol`, RX é `stdin` do simulador e TX é `stdout`. No modo `--serve-local`, RX e TX compartilham um socket Unix acessível apenas aos processos com acesso ao volume local do Compose. Ambos são fluxos de bytes, sem TTY. Cada conexão tem uma sessão de simulação própria; ao desconectar e reconectar, a GUI carrega novamente o programa. Logs e diagnósticos vão para `stderr`. Todos os inteiros de mais de um byte usam **little-endian**; não há alinhamento nem padding. Endereços são índices de palavras de 32 bits.

```text
byte 0       bytes 1–2          bytes 3 em diante
TYPE (u8) | LEN (u16 LE) | PAYLOAD (LEN bytes)
```

`LEN` vale no máximo **512**. O receptor primeiro lê exatamente 3 bytes de cabeçalho e depois exatamente `LEN` bytes. Leituras curtas do fluxo não indicam fim de quadro; EOF no meio de um quadro é erro de sessão. Não há CRC: pipes e sockets Unix preservam a ordem e a integridade dos bytes. `TYPE` desconhecido com `LEN` válido produz `ERROR(unsupported, type)` após consumir o payload. `LEN > 512` ou EOF parcial produz `ERROR(malformed_frame, type)` quando possível e encerra a sessão, porque não há ressincronização. Um comando conhecido com tamanho incorreto recebe `ACK` com `status=bad_length`.

Todos os comandos conhecidos recebem um `ACK` (`status=0` para sucesso, valor não zero para NACK). O `ACK` pode preceder mensagens produzidas pelo comando. Não existem strings no fluxo; a GUI usa `.lst` e `.mlst` para mostrar nomes e código-fonte.

## Comandos RX

| TYPE   | Comando      | Payload, na ordem                                                         | Resposta principal                                   |
| ------ | ------------ | ------------------------------------------------------------------------- | ---------------------------------------------------- |
| `0x01` | `LOAD_BEGIN` | `start_word:u32`, `word_count:u32` (8 B)                                  | `ACK`                                                |
| `0x02` | `LOAD_DATA`  | `offset_words:u32`, `words:u32[n]`, 1 ≤ n ≤ 64 (4+4n B)                   | `ACK`                                                |
| `0x03` | `LOAD_END`   | vazio                                                                     | `ACK`, `STACK_STATE`                                 |
| `0x10` | `STEP_CYCLE` | vazio                                                                     | `ACK`, `CYCLE_STATE`, `STACK_STATE` se mudou, talvez `HALTED` |
| `0x11` | `STEP_INSTR` | vazio                                                                     | `ACK`, `STACK_STATE` se mudou, `CYCLE_STATE`, `RUN_STOPPED`, talvez `HALTED` |
| `0x12` | `RUN`        | `max_cycles:u32`, `report_mode:u8`, `report_every:u16` (7 B)              | `ACK`, eventos, `RUN_STOPPED`, `STATS`               |
| `0x13` | `PAUSE`      | vazio                                                                     | `ACK`, depois `RUN_STOPPED` do `RUN` ativo           |
| `0x14` | `RESET`      | vazio                                                                     | `ACK`, `STACK_STATE`                                 |
| `0x15` | `SET_BP`     | `pc_word:u32` (4 B)                                                       | `ACK`                                                |
| `0x16` | `CLR_BP`     | `pc_word:u32` (4 B)                                                       | `ACK`                                                |
| `0x17` | `STEP_BACK`  | vazio                                                                     | `ACK`, `REWOUND`, `CYCLE_STATE`, `STACK_STATE`, `STATS` |
| `0x20` | `READ_MEM`   | `start_word:u32`, `word_count:u16` (6 B)                                  | `ACK`, `MEM_DATA`                                    |
| `0x21` | `CONFIG`     | `i_lines:u16`, `d_lines:u16`, `line_words:u8`, `memory_latency:u16` (7 B) | `ACK`                                                |

`LOAD_BEGIN` exige `start_word=0` e `word_count≤4096`. O simulador cria uma imagem temporária; `LOAD_DATA` envia offsets **sequenciais**, começando por zero, sem lacunas nem sobreposição. `LOAD_END` só confirma quando recebeu exatamente `word_count` palavras: então substitui a RAM, zera o restante, reinicia CPU/contadores e invalida caches. Uma carga incompleta recebe `load_incomplete` e preserva a imagem anterior. Cada palavra enviada usa 4 bytes little-endian. O carregador de arquivo `.bin` da CLI permanece como entrada separada até esta interface existir.

`RUN` exige `max_cycles≥1` e `report_every≥1`. `report_mode` vale `0` (completo: um `CYCLE_STATE` por ciclo), `1` (compacto: eventos de stall, miss, conclusão de macroinstrução e a cada `report_every` ciclos) ou `2` (só final: sem `CYCLE_STATE` durante `RUN`). `PROGRAM_OUT` e `HALTED` são enviados em todos os modos. Ao terminar por limite de ciclos, pausa, breakpoint ou parada do programa, `RUN` envia `RUN_STOPPED` e em seguida `STATS`. `PAUSE` é lido sem bloquear durante `RUN` e efetiva a pausa após o ciclo corrente. `STEP_CYCLE` executa um ciclo; `STEP_INSTR` executa até concluir uma macroinstrução e envia o estado final. Breakpoints são verificados **antes** da busca no PC indicado.

`STACK_STATE` é enviado após `LOAD_END` e `RESET`, após `STEP_BACK` e sempre que a visão da pilha muda durante avanço por ciclo, instrução ou `RUN`. Ele é independente de `report_mode`: mesmo o modo compacto informa mudanças da pilha. A mensagem descreve a RAM **após** o microciclo indicado. Quando `SP` desce antes de a escrita pendente terminar, uma posição pode aparecer com o valor antigo e ser atualizada em outra mensagem quando a RAM receber a escrita.

`RESET` restaura a imagem carregada e o estado inicial da CPU, zera contadores, invalida caches e remove breakpoints; preserva a configuração de cache/latência. `READ_MEM` aceita 1–64 palavras de RAM comum, desde que `start_word + word_count ≤ 0xFFD`; a região de E/S não é legível por esse comando. `CONFIG` aceita `i_lines` e `d_lines` como potências de dois entre 1 e 256, `line_words` em {1, 2, 4, 8} e `memory_latency` entre 1 e 65535 ciclos. Só é aceito com o simulador parado; invalida caches e zera os contadores de cache. [Memória e caches](memory.md) define os padrões e políticas.

`STEP_BACK` restaura o início da última macroinstrução executada, incluindo RAM, CPU e contadores. Se o simulador estiver no meio de uma instrução, volta ao início dela. O histórico guarda até 1024 inícios de instrução; `LOAD_END`, `RESET` e `CONFIG` o limpam. O comando exige execução parada e histórico disponível. A GUI remove as saídas posteriores ao `output_count` de `REWOUND`. O `CYCLE_STATE` seguinte mostra o estado restaurado.

## Mensagens TX

| TYPE   | Mensagem      | Payload                                                      | LEN  |
| ------ | ------------- | ------------------------------------------------------------ | ---- |
| `0x81` | `ACK`         | `command_type:u8`, `status:u8`                               | 2    |
| `0x90` | `CYCLE_STATE` | Estado de um ciclo, layout abaixo                            | 174  |
| `0x91` | `PROGRAM_OUT` | `kind:u8`, `value:u32`                                       | 5    |
| `0x92` | `MEM_DATA`    | `start_word:u32`, `word_count:u16`, `words:u32[count]`       | 6+4n |
| `0x93` | `HALTED`      | `cycles:u32`, `instructions:u32`                             | 8    |
| `0x94` | `STATS`       | Treze contadores `u32`, layout abaixo                        | 52   |
| `0x95` | `RUN_STOPPED` | `reason:u8`, `pc_word:u32`, `cycles:u32`, `instructions:u32` | 13   |
| `0x96` | `REWOUND`     | `output_count:u32`                                           | 4    |
| `0x97` | `STACK_STATE` | `cycle:u32`, `sp:u32`, `depth:u16`, `word_count:u8`, `words:u32[count]` | 11+4n |
| `0x9F` | `ERROR`       | `status:u8`, `offending_type:u8`                             | 2    |

`PROGRAM_OUT.kind=0` interpreta `value` como inteiro de 32 bits com sinal; `kind=1` usa apenas o byte baixo como caractere. Escrita em `0xFFF` gera o primeiro tipo, em `0xFFE` gera o segundo; escrita em `0xFFD` gera `HALTED`. `HALTED` significa parada pela pseudo-instrução `HALT`, não pausa ou limite de ciclos. `RUN_STOPPED.reason` vale 0 limite, 1 pausa, 2 breakpoint, 3 HALT ou 4 macroinstrução concluída (`STEP_INSTR`). `MEM_DATA` repete o endereço e a contagem solicitados e retorna as palavras na ordem crescente.

`STACK_STATE.depth` é `0xFFC - SP` quando `SP ≤ 0xFFC`; acima disso, é zero e a GUI mostra `SP` fora da região. `words[0]` é a palavra no endereço `SP`, seguida de endereços crescentes, até 32 palavras do topo. `word_count` pode ser zero. A pilha não guarda cópia própria: a mensagem é uma inspeção da RAM e do registrador `SP`. O painel da GUI mostra o topo, permite rolar pelas 32 palavras recebidas e indica a profundidade total; valores mais profundos continuam acessíveis por `READ_MEM`.

### `CYCLE_STATE` (174 bytes)

O quadro representa o estado **após** a borda do ciclo indicado. `mpc` e `microword` identificam a micro-operação executada nesse ciclo; `regs`, `mar`, `mbr` e flags são os valores resultantes. Os 28 bits altos de `microword:u64` são zero.

| Campo                                                         | Tipo                     | Observação                                                        |
| ------------------------------------------------------------- | ------------------------ | ----------------------------------------------------------------- |
| `cycle`                                                       | `u32`                    | Ciclos desde a última carga ou `RESET`                            |
| `mpc`, `microword`                                            | `u16`, `u64`             | Endereço da micro-operação e palavra de 36 bits                   |
| `flags`                                                       | `u8`                     | Bit 0 = N; bit 1 = Z; demais zero                                 |
| `mar`, `mbr`                                                  | `u32`, `u32`             | Interface de memória                                              |
| `pipe_valid`                                                  | `u8`                     | Bits 0, 1 e 2: leitura, execução, escrita                         |
| `pipe_uop[3]`                                                 | 3 × `u16`                | MPC em cada estágio; `0xFFFF` para estágio vazio                  |
| `stall`                                                       | `u8`                     | 0 nenhum; 1 RAW; 2 desvio; 3 miss I; 4 miss D; 5 memória pendente |
| `regs[32]`                                                    | 32 × `u32`               | Banco R0–R31 em ordem                                             |
| `icache_result`, `icache_index`, `icache_tag`                 | `u8`, `u16`, `u32`       | Resultado, índice e tag do acesso I                               |
| `dcache_result`, `dcache_index`, `dcache_tag`, `dcache_flags` | `u8`, `u16`, `u32`, `u8` | Resultado, índice, tag e bit 0 = escrita                          |

Resultados de cache: 0 nenhum, 1 hit, 2 miss, 3 bypass. Índice e tag são zero quando não houve acesso. No modelo sem pipeline, `pipe_valid=0` e `pipe_uop` contém três `0xFFFF`. Os campos de cache também são zero quando caches não estão habilitadas. `dcache_flags` não possui bit dirty, porque a política é write-through.

### `STATS` (52 bytes)

Treze `u32`, nesta ordem: `cycles`, `instructions`, `icache_accesses`, `icache_hits`, `icache_misses`, `dcache_accesses`, `dcache_hits`, `dcache_misses`, `stall_raw`, `stall_branch`, `stall_icache`, `stall_dcache`, `stall_memory`. Contadores de 32 bits retornam a zero ao ultrapassar `0xFFFFFFFF`; a GUI calcula CPI e taxas de acerto. Hits e misses contam acessos da CPU, não cada palavra de preenchimento. Um ciclo parado deve incrementar uma única categoria de stall, segundo a causa que bloqueou o avanço naquele ciclo.

### Códigos de status

`0` sucesso; `1` tamanho incorreto; `2` estado inválido; `3` endereço fora da faixa; `4` valor inválido; `5` comando não suportado; `6` carga incompleta; `7` falha de E/S; `8` quadro malformado. O valor exato e os nomes constam em `protocol.yaml`. Comandos de execução exigem programa carregado e simulador parado; `PAUSE` exige `RUN` ativo. `READ_MEM` e `CONFIG` só são aceitos com execução parada.

## Exemplo de quadro

`STEP_CYCLE` sem payload é `10 00 00` em hexadecimal. Um `ACK` de sucesso é `81 02 00 10 00`: `TYPE=0x81`, `LEN=2`, comando `0x10`, status `0`. Um `LOAD_BEGIN` de uma palavra no endereço zero é `01 08 00 00 00 00 00 01 00 00 00`.
