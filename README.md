# Simulador MIC-1 com cache

Simulador de uma MIC-1 de 32 bits em C++17, com montador de assembly e microasm em Python. Executa as 23 instruções da [ISA](docs/isa.md). A implementação de caches de instruções e dados faz parte do projeto, mas ainda não está pronta.

## Arquitetura atual

O código da interface e do núcleo está em `simulator/src/`: `MicroMachine` executa os microciclos, `CLI.cpp` carrega e executa programas, e `GUI.hpp` abre a janela Raylib. Tudo pertence ao executável `mic1-simulator`; não há processo de GUI separado nem canal TX/RX. A GUI atual mostra apenas uma janela preta. Carga, controles e visualização do estado ainda serão integrados à janela.

## Uso local

Requisitos: CMake 3.12+, compilador C++17 e Python 3. Para compilar a janela, são necessárias as dependências de desenvolvimento do Raylib. O CMake procura Raylib instalado e, se não o encontrar, baixa a versão 6.2 durante a configuração. Em um ambiente sem dependências gráficas ou acesso à rede, configure o simulador com `-DMIC1_ENABLE_GUI=OFF` para compilar somente a CLI; nesse build, `--gui` informa que a interface está indisponível.

```sh
cmake -S assembler -B assembler/builds/local
cmake --build assembler/builds/local

cmake -S simulator -B simulator/builds/local
cmake --build simulator/builds/local

./assembler/builds/local/assembler examples/sum.asm -o /tmp/sum.bin -l /tmp/sum.lst
./simulator/builds/local/mic1-simulator /tmp/sum.bin
./simulator/builds/local/mic1-simulator --gui
```

O exemplo imprime `PROGRAM_OUT=16` e `HALTED`. A CLI aceita `mic1-simulator programa.bin [--cycles N] [--trace]`: `--trace` envia os ciclos ao `stderr`, e `--cycles N` limita a execução a N ciclos. Sem limite, executa até `HALT`. O arquivo `.bin` contém até 4096 palavras de 32 bits em little endian, sem cabeçalho. A janela `--gui` ainda não abre o programa `.bin`.

## Microprograma

O [microasm](tools/microcode/README.md) monta `tools/microcode/mic1.masm` em `ctrl.bin` e `.mlst`.

```sh
python3 tools/microcode/build_rom.py --check
python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o /tmp/ctrl.bin -l /tmp/ctrl.mlst
```

## Docker Compose

Na raiz do repositório, `docker compose up --build` monta todos os programas de `examples/` em `shared/` e executa `sum.bin` no container do simulador. O container termina quando o programa chega a `HALT`; ele não é um serviço persistente.

```sh
docker compose up --build
```

Para montar novamente todos os exemplos ou apenas um arquivo:

```sh
docker compose run --rm --no-deps assembler
docker compose run --rm --no-deps assembler /examples/factorial_5.asm
```

Para executar outro binário no container, use `SIMULATOR_PROGRAM=factorial_5.bin docker compose up --build` ou:

```sh
docker compose run --rm --no-deps simulator /data/factorial_5.bin --trace
```

A GUI usa a mesma imagem e o mesmo executável. No Linux com X11 ou XWayland:

```sh
GUI_UID="$(id -u)" GUI_GID="$(id -g)" \
  GUI_XAUTHORITY="${XAUTHORITY:-/dev/null}" \
  docker compose --profile gui run --build --rm gui
```

O Compose encaminha `DISPLAY` e o socket X11. No WSLg, o mesmo comando pode ser usado sem `GUI_XAUTHORITY`. No Windows com Docker Desktop e um servidor X configurado no display `0`, execute `docker compose --profile gui-windows run --build --rm gui-windows`. A janela ainda não carrega nem executa programas; use a CLI para isso.

## Testes e documentação

Testes permanecem disponíveis para verificar implementações quando necessário, mas não são uma etapa obrigatória do build:

```sh
cmake -S simulator -B simulator/builds/tests -DMIC1_ENABLE_GUI=OFF -DBUILD_TESTING=ON
cmake --build simulator/builds/tests
ctest --test-dir simulator/builds/tests --output-on-failure
```

- [Exemplos](examples/README.md)
- [ISA](docs/isa.md)
- [Microarquitetura](docs/microarch.md)
- [Memória](docs/memory.md)
- [Plano de desenvolvimento](docs/development.md)
