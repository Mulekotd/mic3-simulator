# Memória, caches e E/S

Esta página especifica o modelo de memória. O simulador implementa RAM de 4096 palavras, carga de `.bin`, E/S mapeada e latência configurável; caches e árbitro permanecem planejados. Os endereços abaixo são **índices de palavras de 32 bits**.

Veja também os [esquemáticos vetoriais da memória e da conexão CPU–RAM](esquematicos.md).

## Mapa e parâmetros

| Faixa           | Uso                                                         |
| --------------- | ----------------------------------------------------------- |
| `0x000`–`0xFFB` | RAM para código, dados e pilha                              |
| `0xFFC`         | RAM comum, mas bypass de cache por pertencer à última linha |
| `0xFFD`         | Escrita: `HALTED`                                           |
| `0xFFE`         | Escrita: `PROGRAM_OUT` de caractere (byte baixo)            |
| `0xFFF`         | Escrita: `PROGRAM_OUT` de inteiro com sinal                 |

`SP` começa em `0xFFC` e a primeira operação de empilhar escreve em `0xFFB`. A RAM continua tendo 4096 palavras; os três endereços de E/S não armazenam dados. Leituras de E/S e acessos fora de `0x000`–`0xFFF` são erros de endereço. O carregador de `.bin` e `LOAD_*` rejeitam imagens que excedam 4096 palavras; instruções nessas três posições são desaconselhadas porque os endereços são reservados à E/S.

| Parâmetro        | Padrão   | Valores aceitos                      |
| ---------------- | -------- | ------------------------------------ |
| `ram_words`      | 4096     | Fixo pela ISA de endereço de 12 bits |
| `line_words`     | 4        | Potência de 2: 1, 2, 4 ou 8          |
| `i_lines`        | 16       | Potência de 2: 1 a 256               |
| `d_lines`        | 16       | Potência de 2: 1 a 256               |
| `memory_latency` | 2 ciclos | Inteiro de 1 a 65535                 |

Os quatro parâmetros variáveis são aceitos por `CONFIG` no [protocolo](protocol.md) com a execução parada. `memory_latency` já afeta as transações de RAM; os parâmetros das caches ficam armazenados, mas não afetam a execução enquanto a Fase 5 não estiver implementada. A capacidade prevista de cada cache é `lines × line_words × 4` bytes. Com os padrões, cada cache terá 64 palavras, ou 256 bytes.

## RAM e handshake

A memória principal aceita uma transação por vez. O controlador apresenta `req` com endereço, tipo e eventual dado; após aceitar o pedido na borda `t`, mantém a operação pendente e sinaliza `ready` na borda `t + memory_latency`. Para a configuração padrão, uma operação solicitada em `t` termina em `t+2`. A resposta de leitura entrega uma palavra de 32 bits; a escrita torna-se visível no mesmo `ready`. Um pedido não pode ser alterado durante a espera. O pipeline congela quando depende da resposta, e o mesmo pedido não é reenviado após um stall.

A Memória de Controle (256 × 36 bits) é separada da RAM de programa; não usa este handshake nem passa pelas caches. A [microarquitetura](microarch.md) define seu formato.

## I-cache e D-cache

Ambas são **direct-mapped**, bloqueantes, com linhas de `line_words` palavras. Para um endereço de palavra `a`, `block = floor(a / line_words)`, `index = block mod lines`, `tag = floor(block / lines)` e `offset = a mod line_words`. Com `line_words=4` e `lines=16`, os bits do endereço são `tag[11:6] | index[5:2] | offset[1:0]`. Cada linha guarda tag, validade e dados. `RESET` invalida todas as linhas.

- A **I-cache** atende apenas `RD` marcado com `IF=1`. Ela é somente leitura. Hit entrega a palavra no ciclo de consulta; miss bloqueia a CPU, busca a linha inteira da RAM e só então entrega a palavra solicitada.
- A **D-cache** atende `RD` com `IF=0` e `WR`. Leitura em miss também preenche a linha inteira. Escrita usa **write-through**: em hit atualiza a linha e a RAM; em miss escreve na RAM sem alocar linha (**no-write-allocate**). A conclusão da escrita espera o `ready` da RAM. Não há bit dirty funcional.
- A linha que contém `0xFFD`–`0xFFF` é inteiramente **não cacheável**. Com linha de 4 palavras, isso inclui `0xFFC`; acesso a `0xFFC` vai à RAM, e escritas nos três endereços de E/S vão ao dispositivo. Essa regra impede que uma busca de linha traga registradores de E/S para a cache.
- O projeto assume que programas **não modificam o próprio código**. Não há coerência automática entre I-cache e D-cache; `RESET` ou nova carga invalida ambas.

## Árbitro e stalls

As caches compartilham a porta da RAM. O árbitro recebe pedidos completos e, quando a porta está livre, concede prioridade à D-cache se ambas pedirem no mesmo ciclo; caso contrário concede à solicitante presente. Uma linha em preenchimento mantém a concessão até suas `line_words` leituras terminarem, sem intercalar palavras da outra cache. O pedido não atendido permanece pendente. A CPU fica bloqueada enquanto aguarda a palavra necessária ou uma escrita ainda não concluída.

Um hit não usa a porta da RAM; um miss usa `line_words` transações de leitura. As estatísticas contam acessos, hits e misses por cache, além de ciclos de stall por RAW, desvio, I-cache, D-cache e espera de memória. A GUI calcula CPI a partir de ciclos e macroinstruções concluídas. O [protocolo](protocol.md) define o formato de `STATS` e `CYCLE_STATE`.
