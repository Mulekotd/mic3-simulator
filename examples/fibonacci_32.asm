; Imprime F1 a F32, com F1 = 0 e F2 = 1.
start:
    LOCO 32
    STOD remaining

loop:
    LODD curr
    OUTI

    LODD curr
    ADDD next
    STOD sum
    LODD next
    STOD curr
    LODD sum
    STOD next

    LODD remaining
    SUBD one
    STOD remaining
    JNZE loop
    HALT

curr:       .word 0
next:       .word 1
sum:        .word 0
remaining:  .word 0
one:        .word 1
