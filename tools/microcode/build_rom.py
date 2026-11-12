#!/usr/bin/env python3

import argparse
from pathlib import Path

from tools.microcode.microasm import MicroasmError, assemble

SOURCE = Path(__file__).with_name('mic1.masm')
HEADER = Path(__file__).resolve().parents[2] / 'simulator/src/generated/Mic1Rom.hpp'


def render_header(words):
    """Produz a representação C++ usada pela memória de controle atual."""

    values = ',\n'.join(f'    0x{word:09X}ULL' for word in words)

    return (
        '#pragma once\n\n#include <array>\n#include <cstdint>\n\n'
        f'inline constexpr std::array<std::uint64_t, {len(words)}> mic1Rom = {{\n'
        f'{values}\n}};\n'
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='confere a ROM sem alterá-la')
    args = parser.parse_args()

    try:
        instructions = assemble(SOURCE.read_text(encoding='utf-8'))
    except (OSError, UnicodeError, MicroasmError) as error:
        parser.exit(1, f'{SOURCE}: {error}\n')

    if sorted(instructions) != list(range(79)):
        parser.exit(1, 'o microprograma MIC-1 deve conter os endereços 0 a 78\n')

    content = render_header([instructions[address].word for address in range(79)])

    if args.check:
        if not HEADER.exists() or HEADER.read_text(encoding='utf-8') != content:
            parser.exit(1, f'ROM desatualizada: {HEADER}\n')

        print(f'ROM conferida: {HEADER}')
    else:
        HEADER.parent.mkdir(parents=True, exist_ok=True)
        HEADER.write_text(content, encoding='utf-8')

        print(f'ROM gerada: {HEADER}')


if __name__ == '__main__':
    main()
