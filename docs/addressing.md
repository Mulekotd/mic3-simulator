# Modos de endereçamento da MIC-1

Esta página descreve como as **macroinstruções da ISA** obtêm valores e formam endereços. A MIC-1 do projeto usa palavras de 32 bits; os endereços de memória são índices de palavras na RAM de `0x000` a `0xFFF`. O campo `x` tem 12 bits e o campo `y`, usado por `INSP` e `DESP`, tem 8 bits. Ambos são sem sinal. Consulte a [tabela completa de instruções](isa.md).

| Modo                 | Operando ou endereço efetivo                                              | Instruções típicas                                                                |
| -------------------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------- |
| Imediato             | O valor está na própria instrução: `x` ou `y`                             | `LOCO x` carrega `x` em `AC`; `INSP y` e `DESP y` ajustam `SP` por `y`            |
| Direto ou absoluto   | `x` é o endereço de memória ou o destino do desvio                        | `LODD x`, `STOD x`, `ADDD x`, `SUBD x`; `JUMP x`, desvios condicionais e `CALL x` |
| Relativo a `SP`      | O endereço efetivo é `SP + x`                                             | `LODL x`, `STOL x`, `ADDL x`, `SUBL x`                                            |
| Indireto por `AC`    | O valor de `AC` é usado como endereço de memória                          | `PSHI` lê `M[AC]` e empilha o valor; `POPI` escreve o topo da pilha em `M[AC]`    |
| Implícito pela pilha | A instrução usa `SP` para acessar o topo, sem campo de endereço explícito | `PUSH`, `POP`, `RETN`; `CALL` também empilha o retorno                            |

A **busca de instrução** usa implicitamente `PC`: a palavra em `M[PC]` é buscada, e `PC` passa a apontar para a próxima instrução. Em `CALL`, esse valor atualizado é empilhado como endereço de retorno. `RETN` recupera o destino da pilha. `JUMP`, `CALL` e os desvios condicionais usam `x` como **endereço absoluto**; a ISA não possui desvio relativo a `PC`.

## Formação do endereço na microarquitetura

O microprograma calcula os endereços com o banco de registradores, a ULA e o shifter. Quando precisa acessar a RAM, coloca o endereço em `MAR`; a leitura entrega a palavra em `MBR`. Por exemplo, `LODL x` soma `SP` e `x` antes de ler a memória. Os seletores A, B e C da microinstrução escolhem registradores internos: eles não são novos modos de endereçamento da ISA. A [microarquitetura](microarch.md) descreve esses campos.

Os acessos de dados fora da RAM válida são rejeitados. Os endereços `0xFFD`–`0xFFF` têm funções de E/S mapeada; isso é uma **região de endereços**, não um modo adicional. A pilha começa com `SP = 0xFFC` e cresce em direção a endereços menores.
