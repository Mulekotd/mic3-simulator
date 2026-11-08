#!/usr/bin/env python3

import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSEMBLER = ROOT / 'assembler/builds/local/assembler'
SIMULATOR = ROOT / 'simulator/builds/local/mic3_simulator'


def execute(command):
    """Executa um comando e mostra o erro da ferramenta em caso de falha."""

    result = subprocess.run(command, capture_output=True, text=True)

    if result.returncode:
        raise RuntimeError(f'{command}: {result.stderr}')

    return result


def main():
    # Os 32 termos esperados começam em 0 e 1.
    fib = [0, 1]

    for _ in range(30):
        fib.append(fib[-1] + fib[-2])

    expected = {
        'count_1000.asm': list(range(1001)),
        'fibonacci_32.asm': fib,
        'sum.asm': [16],
        'factorial_5.asm': [120],
        'vector_sum.asm': [15],
        'procedure_call.asm': [14],
        'all_instructions.asm': [3],
    }

    # Usa uma imagem temporária para não deixar binários em examples/.
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / 'program.bin'

        for name, values in expected.items():
            execute([ASSEMBLER, ROOT / 'examples' / name, '-o', binary])
            result = execute([SIMULATOR, binary, '--cycles', '1000000', '--trace'])

            output = [
                int(line.partition('=')[2])
                for line in result.stdout.splitlines()
                if line.startswith('PROGRAM_OUT=')
            ]

            if output != values or 'HALTED ' not in result.stdout:
                raise AssertionError(f'{name}: saída ou parada incorreta')

            if name == 'all_instructions.asm':
                # Cada retorno ao MPC 0 representa uma instrução ISA concluída.
                executed = set()
                pattern = r'micro=(\d+).*?IR=(\d+).*?MPC=0\b'

                for match in re.finditer(pattern, result.stderr):
                    if int(match[1]) == 0:
                        continue

                    word = int(match[2])
                    opcode = word >> 28
                    executed.add((opcode, (word >> 24) & 15 if opcode == 15 else None))

                if len(executed) != 23:
                    raise AssertionError(f'{len(executed)} de 23 instruções executadas')

            print(f'{name}: {len(output)} saída(s), último={output[-1]}')

    print('Todos exemplos passaram; 23 instruções exercitadas.')


if __name__ == '__main__':
    main()
