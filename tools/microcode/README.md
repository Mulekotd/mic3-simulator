# Microasm

O arquivo [`mic1.masm`](mic1.masm) contém o microprograma da MIC-1. Cada linha de código começa com um endereço decimal de `0` a `255`, seguido de `:` e de cláusulas separadas por `;`. São aceitos `rd`, `wr`, `ifetch`, atribuições (`destino:=expressão`), `goto N` e `if n|z then goto N`. Expressões usam registradores, `mbr`, `+`, `band(...)`, `inv(...)`, `lshift(...)` e `rshift(...)`. As linhas `; @phase Busca`, `; @phase Identifica` e `; @phase Executa` definem a fase exibida na listagem; linhas de comentário comuns também começam com `;`.

Na raiz do projeto:

```sh
python3 tools/microcode/microasm.py tools/microcode/mic1.masm -o ctrl.bin -l ctrl.mlst
python3 tools/microcode/build_rom.py --check
```

`ctrl.bin` contém até 256 palavras de controle de **5 bytes little endian**; os quatro bits altos do quinto byte são zero e os endereços omitidos recebem zero. `ctrl.mlst` relaciona endereço, palavra de 36 bits, fase, linha e fonte. `ifetch` define o bit IF e exige `rd`.
