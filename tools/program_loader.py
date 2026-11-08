#!/usr/bin/env python3

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('program', type=Path)
    parser.add_argument('--cycles', type=int, default=1_000_000)
    parser.add_argument('--simulator', type=Path, default=ROOT / 'simulator/builds/local/mic3_simulator')

    args = parser.parse_args()

    if args.cycles < 0:
        parser.error('--cycles deve ser não negativo')

    # Valida o arquivo antes de iniciar o processo do simulador.
    try:
        size = args.program.stat().st_size
    except OSError as error:
        parser.error(f'não foi possível ler o programa: {error}')

    if not args.program.is_file() or size % 4 or size > 4096 * 4:
        parser.error('o programa deve ser um .bin de até 4096 palavras completas')

    # O simulador atual recebe o caminho da imagem binária pela linha de comando.
    try:
        command = [
            str(args.simulator.resolve()), str(args.program.resolve()),
            '--cycles', str(args.cycles),
        ]

        return subprocess.run(command, check=False).returncode
    except OSError as error:
        print(f'não foi possível iniciar o simulador: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
