; CALL empilha o retorno; RETN volta e imprime 14.
start:
    LOCO 7
    CALL double
    OUTI
    HALT

double:
    STOD saved
    ADDD saved
    RETN

saved: .word 0
