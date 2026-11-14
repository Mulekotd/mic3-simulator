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

`docker compose up --build` monta todos os exemplos em `shared/`, com os nomes `nome.bin` e `nome.lst` correspondentes a cada `nome.asm`. Veja o [passo a passo no README](../README.md#docker-compose) para escolher um binário no simulador. Os binários seguem a [ISA](../docs/isa.md). O simulador executa o [microprograma da MIC-1](../tools/microcode/mic1.masm); as caches ainda serão implementadas.
