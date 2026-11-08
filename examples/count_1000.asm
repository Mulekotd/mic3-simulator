; Imprime os inteiros de 0 a 1000, inclusive.
start:
    LOCO 0
    STOD counter

loop:
    LODD counter
    OUTI
    LODD counter
    SUBD limit
    JZER end
    LODD counter
    ADDD one
    STOD counter
    JUMP loop

end:
    HALT

counter: .word 0
limit:   .word 1000
one:     .word 1
