# ISA (macroinstruções)

Sobre a ISA: **máquina de acumulador com pilha** (registradores visíveis: `ac`, `pc`, `sp`), **23 instruções**.

**Codificação em 32 bits.** Para o microprograma fornecido continuar valendo sem mudanças, o opcode fica nos **4 bits mais altos** da palavra (o microprograma identifica a instrução deslocando `ir` e testando o sinal `N`):

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

**Sem HALT e sem E/S na ISA.** A ISA do PDF não tem instrução de parada nem de entrada/saída. Para "retornar a saída final do programa", proposta: **E/S mapeada em memória**, usando os últimos endereços do espaço de 4096 palavras. Escritas nesses endereços **não passam pela D-cache** e são tratadas pelo simulador:

| Endereço | Efeito de `STOD` nele                                  |
| -------- | ------------------------------------------------------ |
| `0xFFF`  | imprime o valor (inteiro com sinal) como `PROGRAM_OUT` |
| `0xFFE`  | imprime o byte menos significativo como caractere      |
| `0xFFD`  | **para a simulação** (`HALTED`)                        |

O montador oferece pseudo-instruções: `OUTI` = `STOD 0xFFF`, `OUTC` = `STOD 0xFFE`, `HALT` = `STOD 0xFFD`.
