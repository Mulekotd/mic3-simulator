# Simulador MIC-3

Projeto de um simulador de microarquitetura MIC-3 em C++17, com montador e interface gráfica planejados. As decisões de arquitetura e o roteiro de desenvolvimento estão em [docs/plano-desenvolvimento.md](docs/plano-desenvolvimento.md).

## Estado atual

O simulador contém ULA, multiplexadores, shifter, registradores, RAM de 4096 palavras de 32 bits, memória de controle e clock simulado. Ele aceita um arquivo binário e o carrega na RAM a partir do endereço de palavra 0.

**A execução da ISA ainda não está implementada.** Por enquanto, cada ciclo demonstra a borda de subida com a micro-operação fixa `PC ← PC + 1`; o conteúdo do arquivo carregado não determina essa operação. Pipeline, caches, microprograma, protocolo TX/RX, montador e GUI continuam no plano de desenvolvimento.

## Formato do programa

O argumento `programa.bin` deve conter palavras consecutivas de **32 bits little-endian**, sem cabeçalho. Seu tamanho deve ser múltiplo de 4 bytes e não pode exceder **4096 palavras (16 KiB)**. Os endereços são índices de palavras, não de bytes.

Para criar um arquivo de exemplo com uma palavra zero:

```bash
mkdir -p shared
printf '\x00\x00\x00\x00' > shared/programa.bin
```

## Compilar e executar localmente

Requisitos: compilador C++17 e CMake 3.12 ou superior.

```bash
cmake -S simulator -B simulator/builds/local -DCMAKE_BUILD_TYPE=Release
cmake --build simulator/builds/local
./simulator/builds/local/mic3_simulator shared/programa.bin --cycles 3
```

`--cycles N` é opcional; o padrão é **1**. A saída informa quantas palavras foram carregadas, os ciclos simulados, PC, MPC e a capacidade da RAM. O comando acima deve informar `loaded_words=1 cycles=3 PC=3 MPC=3 RAM_words=4096`.

## Executar com Docker

```bash
docker build -f dockerfiles/Simulator -t mic3-sim ./simulator
docker run --rm -v "$PWD/shared:/data:ro" mic3-sim /data/programa.bin --cycles 3
```

O volume torna `shared/programa.bin` acessível ao container em `/data/programa.bin`. Use um caminho absoluto dentro do container ao passar outro programa. A imagem executa o simulador como usuário sem privilégios.

Com Docker Compose, o serviço `simulator` usa o mesmo arquivo em `shared/`:

```bash
SIMULATOR_CYCLES=3 docker compose run --rm --build simulator
```

O padrão é `shared/programa.bin` e 1 ciclo. Para outro arquivo dentro de `shared/`, use `SIMULATOR_PROGRAM=outro.bin`. O serviço não expõe portas nem usa rede.

## Documentação

- [Visão geral](docs/visao-geral.md)
- [Arquitetura e protocolo planejado](docs/arquitetura-sistema.md)
- [ISA](docs/isa.md)
- [Parâmetros e microarquitetura](docs/decisoes-projeto.md)
- [Fases de desenvolvimento](docs/fases-desenvolvimento.md)
- [Planejamento da entrega](docs/planejamento-entrega.md)
