# Programas de exemplo

| Arquivo                | Resultado esperado                   |
| ---------------------- | ------------------------------------ |
| `count_1000.asm`       | Imprime 0 a 1000, inclusive          |
| `fibonacci_32.asm`     | Imprime F1 a F32, de 0 até 1346269   |
| `sum.asm`              | Imprime 16                           |
| `factorial_5.asm`      | Imprime 120                          |
| `vector_sum.asm`       | Soma cinco palavras e imprime 15     |
| `procedure_call.asm`   | Usa `CALL`/`RETN` e imprime 14       |
| `all_instructions.asm` | Executa as 23 instruções e imprime 3 |

Para montar um exemplo, execute o [montador](../README.md#montador). Os binários gerados seguem a [ISA](../docs/isa.md). O simulador executa o [microprograma transcrito](../tools/microcode/mic1.masm), sem pipeline nem cache. `python3 tools/validate_examples.py` monta e confere a saída de todos os exemplos.
