# Simulador MIC-1 com cache

Simulador de uma MIC-1 de 32 bits em C++17, com montador de assembly e microasm em Python. Executa as 23 instruções da [ISA](docs/isa.md). As caches de instruções e dados ainda serão implementadas.

## Arquitetura e uso local

O núcleo (`MicroMachine`), a sessão de execução e a GUI Raylib ficam em `simulator/src/` e no mesmo executável `mic1-simulator`. A sessão mantém até 1024 microciclos de histórico, incluindo RAM e estado da CPU, para permitir voltar um passo. A CLI de execução direta continua disponível para uso sem interface.

Requisitos: CMake 3.12+, compilador C++17 e Python 3. O CMake procura Raylib instalado e, se não o encontrar, baixa a versão 6.2 durante a configuração. Para compilar apenas a CLI, use `-DMIC1_ENABLE_GUI=OFF`.

```sh
cmake -S assembler -B assembler/builds/local
cmake --build assembler/builds/local

cmake -S simulator -B simulator/builds/local
cmake --build simulator/builds/local

./assembler/builds/local/assembler examples/sum.asm -o /tmp/sum.bin -l /tmp/sum.lst

./simulator/builds/local/mic1-simulator --gui /tmp/sum.bin
```

A GUI inicia a execução de `sum.bin` e permanece aberta após `HALT`. Os controles são: **R** executar/retomar, **P** pausar, **Espaço** avançar um microciclo, **B** voltar um microciclo, **X** reiniciar o programa e **L** digitar o caminho de outro `.bin` (Enter carrega, Esc cancela). Ao voltar, as saídas produzidas pelo microciclo desfeito também são retiradas da tela. Fechar a janela encerra o processo.

A execução direta pelo terminal usa `mic1-simulator program.bin [--cycles N] [--trace]`. Sem `--cycles`, a CLI executa até `HALT` e termina. O `.bin` contém até 4096 palavras de 32 bits em little-endian, sem cabeçalho.

## Docker Compose

No Linux com X11 ou XWayland, o serviço `simulator` abre a GUI no próprio container. Ele monta os exemplos, inicia `sum.bin` e **permanece em execução após `HALT`**, aguardando ações na janela. Configure `GUI_XAUTHORITY` quando o servidor X exigir autenticação.

```sh
GUI_UID="$(id -u)" GUI_GID="$(id -g)" \
  GUI_XAUTHORITY="${XAUTHORITY:-/dev/null}" \
  docker compose up --build
```

Para montar outro exemplo e executá-lo na sessão gráfica, monte o arquivo e pressione **L** na janela. Digite `/data/factorial_5.bin`, pressione **Enter** e depois **R**. A carga pausa a sessão e limpa o histórico anterior.

```sh
docker compose run --rm --no-deps assembler /examples/factorial_5.asm
```

A CLI pode executar um `.bin` de forma independente, sem alterar a sessão da GUI:

```sh
docker compose exec -T simulator mic1-simulator /data/factorial_5.bin
```

Para encerrar o serviço, feche a janela ou execute `docker compose down`. No Windows com Docker Desktop e servidor X no display `0`, use `docker compose --profile gui-windows up --build gui-windows`; os controles são os mesmos na janela.

## Microprograma e documentação

O [microasm](tools/microcode/README.md) monta `tools/microcode/mic1.masm` em `ctrl.bin` e `.mlst`.

```sh
python3 tools/microcode/build_rom.py --check
python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o /tmp/ctrl.bin -l /tmp/ctrl.mlst
```

- [Exemplos](examples/README.md)
- [ISA](docs/isa.md)
- [Modos de endereçamento](docs/addressing.md)
- [Microarquitetura](docs/microarch.md)
- [Memória](docs/memory.md)
- [Plano de desenvolvimento](docs/development.md)
