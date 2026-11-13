# Memória e E/S

O simulador atual usa uma RAM de 4096 palavras de 32 bits, com endereços expressos em **índices de palavras**. A CLI carrega uma imagem `.bin` a partir do endereço zero. Pipeline, caches e árbitro de memória ainda não estão implementados.

## Mapa atual

| Endereço | Uso |
| --- | --- |
| `0x000`–`0xFFC` | RAM para código, dados e pilha |
| `0xFFD` | Escrita encerra a execução (`HALTED`) |
| `0xFFE` | Escrita emite o byte baixo como caractere |
| `0xFFF` | Escrita emite um inteiro com sinal (`PROGRAM_OUT`) |

`SP` começa em `0xFFC`; o primeiro `PUSH` ou `CALL` escreve em `0xFFB`. Os três endereços de E/S não armazenam dados. Leituras desses endereços e acessos além da RAM são rejeitados. O carregador rejeita imagens maiores que 4096 palavras ou cujo tamanho não seja múltiplo de quatro bytes.

## Latência atual

`MicroMachine` inicia com latência de dois ciclos por transação de leitura ou escrita. O núcleo aceita alteração por `MicroMachine::setMemoryLatency(unsigned)`, mas a CLI e a GUI ainda não oferecem esse ajuste. O método atualmente não valida o argumento; o valor zero não deve ser usado. Enquanto uma transação está pendente, o microprograma sustenta o pedido até a conclusão. A Memória de Controle, de 256 palavras de 36 bits, é separada da RAM e não usa esse mecanismo.

## Extensões planejadas

A I-cache e a D-cache planejadas serão de mapeamento direto, bloqueantes, com linhas de `line_words` palavras. Para um endereço `a`, `block = floor(a / line_words)`, `index = block mod lines`, `tag = floor(block / lines)` e `offset = a mod line_words`. A marca `IF` da microinstrução distingue futuras buscas de instrução dos acessos a dados.

A I-cache atenderá apenas leituras de instruções. A D-cache atenderá leituras e escritas de dados, com escrita direta na RAM e sem alocação em miss de escrita. A linha que contém `0xFFD`–`0xFFF` ficará fora das caches; com linhas de quatro palavras, isso também inclui `0xFFC`. O projeto assume que programas não alteram o próprio código. Reset ou nova carga invalidará ambas as caches quando elas forem implementadas.

As caches compartilharão uma porta da RAM. O árbitro dará prioridade à D-cache quando ambas solicitarem acesso no mesmo ciclo. Estão planejados contadores de acessos, hits, misses e ciclos de espera. A interface poderá consultar esses contadores diretamente no núcleo, sem protocolo entre processos.
