# ISA (macroinstruções)

Sobre a ISA: **máquina de acumulador com pilha** (registradores visíveis: `ac`, `pc`, `sp`), **23 instruções**.

**Codificação em 32 bits.** Para o microprograma fornecido continuar valendo sem mudanças, o opcode fica nos **4 bits mais altos** da palavra (o microprograma identifica a instrução deslocando `ir` e testando o sinal `N`). O campo `x` tem 12 bits e representa endereço, constante ou deslocamento relativo a `sp`, conforme a instrução; `y` é um campo separado de 8 bits para `INSP`/`DESP`:

```
Formato A (opcodes 0000 a 1110):  [31:28] opcode | [27:12] = 0 | [11:0] x   (endereço ou constante, 12 bits)
Formato B (opcode 1111):          [31:28] = 1111 | [27:24] sub-opcode | [23:8] = 0 | [7:0] y (só INSP/DESP)
```

| Opcode             | Mnemônico | Significado                          |
| ------------------ | --------- | ------------------------------------ |
| `0000`             | LODD      | `ac := m[x]`                         |
| `0001`             | STOD      | `m[x] := ac`                         |
| `0010`             | ADDD      | `ac := ac + m[x]`                    |
| `0011`             | SUBD      | `ac := ac - m[x]`                    |
| `0100`             | JPOS      | se `ac >= 0` então `pc := x`         |
| `0101`             | JZER      | se `ac = 0` então `pc := x`          |
| `0110`             | JUMP      | `pc := x`                            |
| `0111`             | LOCO      | `ac := x` (0 a 4095)                 |
| `1000`             | LODL      | `ac := m[sp + x]`                    |
| `1001`             | STOL      | `m[sp + x] := ac`                    |
| `1010`             | ADDL      | `ac := ac + m[sp + x]`               |
| `1011`             | SUBL      | `ac := ac - m[sp + x]`               |
| `1100`             | JNEG      | se `ac < 0` então `pc := x`          |
| `1101`             | JNZE      | se `ac != 0` então `pc := x`         |
| `1110`             | CALL      | `sp := sp - 1; m[sp] := pc; pc := x` |
| `1111`, sub `0000` | PSHI      | `sp := sp - 1; m[sp] := m[ac]`       |
| `1111`, sub `0010` | POPI      | `m[ac] := m[sp]; sp := sp + 1`       |
| `1111`, sub `0100` | PUSH      | `sp := sp - 1; m[sp] := ac`          |
| `1111`, sub `0110` | POP       | `ac := m[sp]; sp := sp + 1`          |
| `1111`, sub `1000` | RETN      | `pc := m[sp]; sp := sp + 1`          |
| `1111`, sub `1010` | SWAP      | troca `ac` e `sp`                    |
| `1111`, sub `1100` | INSP      | `sp := sp + y` (0 a 255)             |
| `1111`, sub `1110` | DESP      | `sp := sp - y` (0 a 255)             |

**Sem HALT e sem E/S na ISA.** A ISA do PDF não tem instrução de parada nem de entrada/saída. Para "retornar a saída final do programa", o projeto usa **E/S mapeada em memória**, nos últimos endereços do espaço de 4096 palavras. Escritas nesses endereços **não passam pela D-cache** e são tratadas pelo simulador:

| Endereço | Efeito de `STOD` nele                                  |
| -------- | ------------------------------------------------------ |
| `0xFFF`  | imprime o valor (inteiro com sinal) como `PROGRAM_OUT` |
| `0xFFE`  | imprime o byte menos significativo como caractere      |
| `0xFFD`  | **para a simulação** (`HALTED`)                        |

O montador oferece pseudo-instruções: `OUTI` = `STOD 0xFFF`, `OUTC` = `STOD 0xFFE`, `HALT` = `STOD 0xFFD`.

## Endereçamento e representação

- `pc`, `sp` e `x` contêm **endereços de palavras**, não de bytes. Uma palavra tem 32 bits; a RAM tem endereços `0x000` a `0xFFF`.
- As operações aritméticas usam complemento de dois de 32 bits, com descarte do carry além do bit 31. `JPOS` e `JNEG` interpretam o sinal do valor de `ac`; `JZER` e `JNZE` testam zero.
- `x` e `y` são inteiros **sem sinal**. `LOCO x` não codifica constante negativa. Um acesso `sp + x` fora de `0x000` a `0xFFF` é inválido; não há retorno circular do endereço.
- O `pc` aponta para a próxima instrução após a busca. Por isso `CALL` empilha o endereço de retorno já incrementado.
- A pilha começa em `sp = 0xFFC` e cresce para endereços menores. O primeiro `PUSH` ou `CALL` escreve em `0xFFB`. Os endereços `0xFFD` a `0xFFF` ficam reservados para E/S.
- O arquivo `.bin` contém as palavras consecutivas em **little-endian**, sem cabeçalho, a partir do endereço 0. Instruções e dados compartilham a mesma RAM.

## Dialeto assembly previsto

O montador aceita um rótulo opcional (`nome:`), seguido de uma instrução ou diretiva. Um comentário começa em `;` e termina no fim da linha. Nomes de rótulos usam letras, dígitos e `_`, começando por letra ou `_`; mnemônicos não diferenciam maiúsculas de minúsculas. O operando pode ser um número decimal, `0x` hexadecimal ou um rótulo. Referências a rótulos adiante são permitidas; rótulos diferenciam maiúsculas de minúsculas.

Cada instrução e cada `.word` ocupam uma palavra. `.word valor` grava o padrão de 32 bits de um inteiro entre −2³¹ e 2³²−1. `.org endereço` usa um número literal, avança o endereço de montagem e preenche posições puladas com zero; não pode retroceder nem ultrapassar `0xFFF`. `OUTI`, `OUTC` e `HALT` ocupam **uma palavra cada**, pela expansão indicada acima. O montador rejeita operandos fora da faixa (`x`: 0–4095; `y`: 0–255), rótulos duplicados e instruções sem operando obrigatório.
