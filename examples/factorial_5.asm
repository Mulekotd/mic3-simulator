; Calcula 5! por adições repetidas e imprime 120.
start:
    LOCO 5
    STOD n
    LOCO 1
    STOD result

factor_loop:
    LODD n
    JZER done
    LOCO 0
    STOD product
    LODD n
    STOD count

multiply_loop:
    LODD product
    ADDD result
    STOD product
    LODD count
    SUBD one
    STOD count
    JNZE multiply_loop

    LODD product
    STOD result
    LODD n
    SUBD one
    STOD n
    JUMP factor_loop

done:
    LODD result
    OUTI
    HALT

n:       .word 0
result:  .word 0
product: .word 0
count:   .word 0
one:     .word 1
