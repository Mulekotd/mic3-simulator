; Percorre o vetor de cinco palavras com PSHI/POP e imprime 15.
start:
    LOCO values
    STOD pointer
    LOCO 5
    STOD remaining
    LOCO 0
    STOD total

loop:
    LODD pointer
    PSHI
    POP
    ADDD total
    STOD total
    LODD pointer
    ADDD one
    STOD pointer
    LODD remaining
    SUBD one
    STOD remaining
    JNZE loop

    LODD total
    OUTI
    HALT

pointer:   .word 0
remaining: .word 0
total:     .word 0
one:       .word 1
values:    .word 1
           .word 2
           .word 3
           .word 4
           .word 5
