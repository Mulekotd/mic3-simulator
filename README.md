# Simulador MIC-3

Simulador de microarquitetura em C++17 com montador de assembly e microasm em Python. Executa as 23 instruções da [ISA](docs/isa.md) em uma MIC-1 de 32 bits, sem pipeline ou cache nesta etapa.

No código C++, funções, variáveis e atributos usam `camelCase`; classes e tipos usam `PascalCase`. Atributos privados não recebem `_` no final. Nomes definidos por C, POSIX, Raylib e pela biblioteca padrão seguem suas próprias APIs.

## Uso local

Requisitos: CMake 3.12+, compilador C++17 e Python 3. A geração dos IDs do protocolo usa PyYAML. Para compilar a GUI localmente, CMake 3.24+ baixa Raylib 6.2 automaticamente.

```sh
cmake -S assembler -B assembler/builds/local
cmake --build assembler/builds/local
cmake -S simulator -B simulator/builds/local
cmake --build simulator/builds/local

./assembler/builds/local/assembler examples/sum.asm -o /tmp/sum.bin -l /tmp/sum.lst
./simulator/builds/local/mic3_simulator /tmp/sum.bin --cycles 100000
```

O resultado esperado é `PROGRAM_OUT=16` seguido de `HALTED`. Use `--trace` para ver os ciclos no `stderr`. O arquivo `.bin` contém palavras de 32 bits little endian, sem cabeçalho, até 4096 palavras.

## Protocolo binário e GUI

O simulador aceita `--protocol` para receber quadros pelo `stdin` e responder pelo `stdout`. O cliente de diagnóstico carrega um programa e confirma `PROGRAM_OUT` e `HALTED`:

```sh
python3 tools/generate_protocol_cpp.py --check
python3 tools/tx_rx_client.py simulator/builds/local/mic3_simulator /tmp/sum.bin
```

A base da GUI usa C++17 e Raylib. O CMake baixa Raylib durante a configuração:

```sh
cmake -S gui -B gui/builds/local
cmake --build gui/builds/local
./gui/builds/local/mic3_gui ./simulator/builds/local/mic3_simulator /tmp/sum.bin
```

Na janela, `L` carrega o `.bin`, espaço executa um ciclo, `I` uma instrução, `R` executa, `P` pausa e `X` reinicia. A execução local usa pipes; no Compose, a GUI se conecta ao socket Unix do serviço persistente. A leitura dos quadros ocorre em thread separada. Esta é a estrutura inicial da GUI; os painéis completos pertencem à Fase 7.

## Microprograma

O [microasm](tools/microcode/README.md) monta `tools/microcode/mic1.masm` em `ctrl.bin` e `.mlst`. O simulador atual usa a ROM C++ gerada do mesmo arquivo:

```sh
python3 tools/microcode/build_rom.py --check
python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o /tmp/ctrl.bin -l /tmp/ctrl.mlst
```

## Executar com Docker Compose

Execute os comandos a seguir na raiz do repositório. O Compose monta `./shared` em `/data` nos containers.

### 1. Subir a stack

```sh
docker compose up -d --build
docker compose ps -a
docker compose logs assembler
docker compose logs simulator
```

Essa inicialização monta todos os arquivos `.asm` de `examples/`. Cada `nome.asm` produz `shared/nome.bin` e `shared/nome.lst`. O container `assembler` termina com código 0; o `simulator` permanece `Up`, aguardando comandos TX/RX no socket local `/run/mic3/simulator.sock`. `HALT` encerra a execução do programa, sem parar o serviço. A comunicação entre os containers usa um volume Docker, sem porta TCP.

### 2. Montar os programas

Para montar novamente **todos** os exemplos após alterar arquivos em `examples/`:

```sh
docker compose run --rm --no-deps assembler
```

Para montar **um** arquivo, por exemplo `examples/factorial_5.asm`:

```sh
docker compose run --rm --no-deps assembler /examples/factorial_5.asm
```

O resultado fica em `shared/factorial_5.bin` e `shared/factorial_5.lst`. Para um programa próprio, coloque `meu_programa.asm` em `examples/` e use `/examples/meu_programa.asm` no comando. As opções `-o` e `-l` do montador permitem escolher caminhos de saída específicos.

### 3. Carregar e executar um programa no simulador

O serviço persistente aguarda a GUI enviar o conteúdo do `.bin` pelo protocolo. Para usar um binário próprio já montado, copie-o para `shared/`; depois passe seu nome ao iniciar a GUI no passo 4 e pressione **L** na janela. A carga copia as palavras para a RAM a partir do endereço zero; **R** inicia ou retoma a execução.

Para executar e inspecionar um `.bin` diretamente no terminal, use a CLI **dentro do container do simulador**:

```sh
docker compose exec -T simulator mic3_simulator /data/factorial_5.bin --cycles 1000000 --trace
```

Essa CLI cria uma execução independente da sessão aberta na GUI. `--trace` mostra cada microciclo no terminal; sem essa opção, a saída numérica aparece como `PROGRAM_OUT=...` e o fim como `HALTED`. Para acompanhar apenas os primeiros ciclos, troque o limite por `--cycles 20`.

### 4. Abrir a interface de usuário

No WSLg, execute:

```sh
GUI_UID="$(id -u)" GUI_GID="$(id -g)" docker compose --profile gui up --build gui
```

A GUI usa `shared/sum.bin` por padrão. Para selecionar outro binário já montado:

```sh
SIMULATOR_PROGRAM=factorial_5.bin GUI_UID="$(id -u)" GUI_GID="$(id -g)" \
  docker compose --profile gui up --build gui
```

A janela aparece no desktop do Windows, possivelmente atrás do VS Code; use **Alt+Tab** se necessário. Pressione **L** para carregar, **R** para executar ou retomar, **P** para pausar, **I** para avançar uma instrução, **B** para voltar uma instrução, **Espaço** para avançar um microciclo e **X** para reiniciar. No Linux com X11 e autenticação, defina `GUI_XAUTHORITY` com o caminho de um arquivo existente. No WSLg, deixe essa variável indefinida. O build da imagem GUI baixa Raylib pelo `gui/CMakeLists.txt`.

Para parar a stack, execute `docker compose --profile gui down`.

## Verificação e documentação

Os programas montados podem ser verificados com a CLI no container `simulator`, conforme o passo 3.

- [Exemplos](examples/README.md)
- [Microarquitetura](docs/microarch.md)
- [Fases de desenvolvimento](docs/fases-desenvolvimento.md)
- [Protocolo binário](docs/protocol.md)
