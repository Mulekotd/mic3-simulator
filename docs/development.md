# Estado e próximos passos

## Estado atual

O projeto é um simulador de MIC-1 de 32 bits com caches de instruções e dados planejadas. O montador C++ produz imagens `.bin` e listagens `.lst`; o microasm Python gera a ROM de controle. O núcleo já executa as 23 instruções da ISA, com RAM de 4096 palavras, E/S mapeada e traço opcional por ciclo. As caches ainda não foram implementadas. A CLI carrega um `.bin` diretamente e o executa até `HALT`, salvo se receber `--cycles N`. [`simulator/src/GUI.hpp`](../simulator/src/GUI.hpp) contém a janela Raylib, iniciada pelo mesmo executável com `--gui`. A sessão mantém o estado da CPU e da RAM para permitir voltar um microciclo; a janela continua aberta após `HALT` e permite digitar o caminho de outro programa. A CLI carrega e executa arquivos em uma sessão independente. O diretório da GUI separada e o canal TX/RX foram removidos.

Os testes existentes são opcionais no build (`BUILD_TESTING=ON`) e podem ser usados para verificar novas implementações. Não há exigência de desenvolvimento guiado por testes.

## Próximas etapas

1. Ampliar a visualização de registradores, memória, pilha, microinstrução e listagens. A pilha será lida diretamente de `SP` e da RAM.
2. Aprimorar os controles e a apresentação da execução na GUI.
3. Expor a configuração da latência de memória e implementar I-cache, D-cache, arbitragem e estatísticas, preservando os resultados funcionais dos exemplos.
4. Integrar à GUI os indicadores de acesso, acerto e falha das caches.
5. Finalizar a experiência da GUI, a documentação e as demonstrações de programas representativos.

A [ISA](isa.md), a [microarquitetura](microarch.md), o [modelo de memória](memory.md) e o [README](../README.md) descrevem o comportamento e os comandos atuais.
