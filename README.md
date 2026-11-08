# Simulador MIC-3

Simulador de microarquitetura em C++17 com montador de assembly e microasm em Python. Executa as 23 instruções da [ISA](docs/isa.md) em uma MIC-1 de 32 bits, sem pipeline ou cache nesta etapa.

## Uso local

Requisitos: CMake 3.12+, compilador C++17 e Python 3.

```sh
cmake -S assembler -B assembler/builds/local
cmake --build assembler/builds/local
cmake -S simulator -B simulator/builds/local
cmake --build simulator/builds/local

./assembler/builds/local/assembler examples/sum.asm -o /tmp/sum.bin -l /tmp/sum.lst
./simulator/builds/local/mic3_simulator /tmp/sum.bin --cycles 100000
```

O resultado esperado é `PROGRAM_OUT=16` seguido de `HALTED`. Use `--trace` para ver os ciclos no `stderr`. O arquivo `.bin` contém palavras de 32 bits little endian, sem cabeçalho, até 4096 palavras.

## Microprograma

O [microasm](tools/microcode/README.md) monta `tools/microcode/mic1.masm` em `ctrl.bin` e `.mlst`. O simulador atual usa a ROM C++ gerada do mesmo arquivo:

```sh
python3 tools/microcode/build_rom.py --check
python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o /tmp/ctrl.bin -l /tmp/ctrl.mlst
```

## Docker Compose

```sh
mkdir -p shared
cp examples/sum.asm shared/programa.asm
ASSEMBLER_UID="$(id -u)" ASSEMBLER_GID="$(id -g)" docker compose up --build --abort-on-container-failure
```

O serviço `assembler` gera `shared/programa.bin`; o `simulator` o executa.

## Verificação e documentação

```sh
ctest --test-dir simulator/builds/local --output-on-failure
python3 tools/validate_examples.py
```

- [Exemplos](examples/README.md)
- [Microarquitetura](docs/microarch.md)
- [Fases de desenvolvimento](docs/fases-desenvolvimento.md)
- [Protocolo planejado](docs/protocol.md)
