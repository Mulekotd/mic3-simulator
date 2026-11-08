; Exercita as 23 instruções da ISA e imprime 3.
start:
    LOCO 5
    STOD scratch
    LODD scratch
    ADDD scratch
    SUBD scratch
    JPOS positive
    LOCO 99

positive:
    JZER wrong
    JNEG wrong
    JNZE nonzero
    LOCO 99

nonzero:
    JUMP call_section

wrong:
    LOCO 99
    OUTI
    HALT

call_section:
    CALL routine
    LOCO 0
    JZER zero_done
    JUMP wrong

zero_done:
    LODD minus_one
    JNEG negative_done
    JUMP wrong

negative_done:
    JPOS wrong
    LOCO 7
    PUSH
    LOCO 0
    POP
    LOCO scratch
    PSHI
    POP
    LOCO 9
    PUSH
    LOCO destination
    POPI
    LODD destination
    LOCO 3
    STOL 0
    LODL 0
    ADDL 0
    SUBL 0
    DESP 1
    INSP 1
    SWAP
    SWAP
    OUTI
    HALT

routine:
    LOCO 1
    RETN

scratch:     .word 0
destination: .word 0
minus_one:   .word -1
