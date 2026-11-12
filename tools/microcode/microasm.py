#!/usr/bin/env python3

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

REGISTERS = {
    'zero': 0, 'pc': 1, 'ac': 2, 'sp': 3, 'ir': 4, 'tir': 5,
    'one': 6, 'mone': 7, 'amask': 8, 'smask': 9,
    **{chr(ord('a') + index): 10 + index for index in range(6)},
}

PHASES = {'busca': 'Busca', 'identifica': 'Identifica', 'executa': 'Executa'}


@dataclass(frozen=True)
class Instruction:
    address: int
    word: int
    phase: str
    line: int
    source: str


class MicroasmError(ValueError):
    """Erro de sintaxe ou codificação com localização no arquivo de entrada."""


def fail(line, message):
    raise MicroasmError(f'linha {line}: {message}')


def register(name, line):
    if name not in REGISTERS:
        fail(line, f'registrador desconhecido: {name}')

    return REGISTERS[name]


def parse_expression(source, mar_source, line):
    """Resolve a expressão da ULA e a seleção comum do barramento B/MAR."""

    source = re.sub(r'\s+', '', source.lower())

    shift = 0
    for function, code in (('lshift', 2), ('rshift', 1)):
        prefix = function + '('

        if source.startswith(prefix) and source.endswith(')'):
            source = source[len(prefix):-1]
            shift = code
            break

    if source.startswith('inv(') and source.endswith(')'):
        operation, operands = 3, [source[4:-1]]
    elif source.startswith('band(') and source.endswith(')'):
        operation, operands = 1, source[5:-1].split(',')
    elif '+' in source:
        operation, operands = 0, source.split('+')
    else:
        operation, operands = 2, [source]

    if len(operands) != (2 if operation in (0, 1) else 1):
        fail(line, f'expressão inválida: {source}')

    operands = [{'1': 'one', '(-1)': 'mone'}.get(item, item) for item in operands]

    for operand in operands:
        if operand != 'mbr':
            register(operand, line)

    if mar_source is not None:
        register(mar_source, line)

        b = mar_source

        if len(operands) == 2 and b not in operands:
            fail(line, 'MAR e ULA exigem fontes B diferentes')

        a = operands[1] if len(operands) == 2 and operands[0] == b else operands[0]
    else:
        a = operands[0]
        b = operands[1] if len(operands) == 2 else 'zero'

    if b == 'mbr':
        fail(line, 'MBR não pode alimentar o barramento B')

    if len(operands) == 2 and a == 'mbr' and b == 'mbr':
        fail(line, 'MBR não pode alimentar as duas entradas da ULA')

    return int(a == 'mbr'), operation, shift, REGISTERS.get(a, 0), register(b, line)


def encode(body, line):
    """Valida cláusulas e empacota AMUX, COND, ULA, SH, destinos e ADDR."""

    clauses = [part.strip().lower() for part in body.split(';') if part.strip()]

    if not clauses:
        fail(line, 'microinstrução vazia')

    assignments = {}
    read = write = ifetch = 0
    condition = address = 0
    branch_seen = False

    for clause in clauses:
        assignment = re.fullmatch(r'([a-z][a-z0-9]*)\s*:=\s*(.+)', clause)
        branch = re.fullmatch(r'goto\s+(\d+)', clause)
        conditional = re.fullmatch(r'if\s+(n|z)\s+then\s+goto\s+(\d+)', clause)

        if assignment:
            destination, source = assignment.groups()

            if destination in assignments:
                fail(line, f'destino repetido: {destination}')

            if destination not in REGISTERS and destination not in ('mar', 'mbr', 'alu'):
                fail(line, f'destino desconhecido: {destination}')

            assignments[destination] = source
        elif clause in ('rd', 'wr', 'ifetch'):
            if clause == 'rd':
                if read:
                    fail(line, 'RD repetido')

                read = 1
            elif clause == 'wr':
                if write:
                    fail(line, 'WR repetido')

                write = 1
            else:
                if ifetch:
                    fail(line, 'ifetch repetido')

                ifetch = 1
        elif branch or conditional:
            if branch_seen:
                fail(line, 'mais de um desvio na microinstrução')

            branch_seen = True
            condition = 3 if branch else (1 if conditional[1] == 'n' else 2)
            address = int(branch[1] if branch else conditional[2])

            if address > 255:
                fail(line, 'endereço de desvio fora da Memória de Controle')
        else:
            fail(line, f'cláusula desconhecida: {clause}')

    if read and write:
        fail(line, 'RD e WR não podem ocorrer juntos')

    if ifetch and not read:
        fail(line, 'ifetch exige RD')

    mar_source = assignments.pop('mar', None)
    mbr_source = assignments.pop('mbr', None)

    if len(assignments) > 1:
        fail(line, 'mais de um destino no barramento C')

    destination, source = next(iter(assignments.items()), ('', ''))

    if mbr_source and source and re.sub(r'\s+', '', source) != re.sub(r'\s+', '', mbr_source):
        fail(line, 'MBR e barramento C exigem a mesma saída da ULA')

    alu_source = source or mbr_source or 'zero'
    amux, alu, shift, a, b = parse_expression(alu_source, mar_source, line)
    c = register(destination, line) if destination and destination != 'alu' else 0
    enable = int(bool(destination) and destination != 'alu')

    return (
        (amux << 35) | (condition << 33) | (alu << 31) | (shift << 29)
        | (int(mbr_source is not None) << 28) | (int(mar_source is not None) << 27)
        | (read << 26) | (write << 25) | (enable << 24) | (c << 19)
        | (b << 14) | (a << 9) | (address << 1) | ifetch
    )


def assemble(source):
    """Lê endereços explícitos; as posições omitidas viram palavras zero."""

    instructions = {}
    phase = 'Executa'

    for number, raw in enumerate(source.splitlines(), 1):
        stripped = raw.strip()

        if not stripped:
            continue

        if stripped.startswith(';'):
            marker = re.fullmatch(r';\s*@phase\s+(\w+)\s*', stripped, re.IGNORECASE)

            if marker:
                phase = PHASES.get(marker[1].lower())

                if phase is None:
                    fail(number, f'fase desconhecida: {marker[1]}')
            elif re.match(r';\s*@phase\b', stripped, re.IGNORECASE):
                fail(number, 'diretiva @phase inválida')

            continue

        match = re.fullmatch(r'(\d+)\s*:\s*(.*)', stripped)

        if not match:
            fail(number, 'esperado endereço numérico seguido de dois-pontos')

        address = int(match[1])

        if address > 255:
            fail(number, 'endereço fora da Memória de Controle (0..255)')

        if address in instructions:
            fail(number, f'endereço repetido: {address}')

        instructions[address] = Instruction(address, encode(match[2], number), phase, number, raw)

    if not instructions:
        raise MicroasmError('microprograma vazio')

    return instructions


def binary_image(instructions):
    """Grava cinco bytes little endian por palavra, inclusive lacunas internas."""

    return b''.join(
        instructions[address].word.to_bytes(5, 'little') if address in instructions else bytes(5)
        for address in range(max(instructions) + 1)
    )


def listing(instructions):
    """Relaciona endereço, palavra, fase, linha original e fonte simbólica."""

    lines = ['; endereço  palavra    fase        linha  fonte']

    for address in sorted(instructions):
        item = instructions[address]
        lines.append(
            f'{address:03X}       {item.word:09X}  {item.phase:<11} '
            f'{item.line:>4}   {item.source.strip()}'
        )

    return '\n'.join(lines) + '\n'


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='microprograma .masm')
    parser.add_argument('-o', '--output', type=Path, required=True, help='Memória de Controle .bin')
    parser.add_argument('-l', '--listing', type=Path, required=True, help='listagem .mlst')
    args = parser.parse_args(argv)

    paths = [path.resolve() for path in (args.source, args.output, args.listing)]

    if len(set(paths)) != 3:
        parser.error('entrada, binário e listagem devem usar arquivos diferentes')

    try:
        instructions = assemble(args.source.read_text(encoding='utf-8'))
        image = binary_image(instructions)
        report = listing(instructions)

        args.output.write_bytes(image)
        args.listing.write_text(report, encoding='utf-8')
    except (OSError, UnicodeError, MicroasmError) as error:
        parser.exit(1, f'{args.source}: {error}\n')

    print(f'microinstructions={len(instructions)} output={args.output} listing={args.listing}')

    return 0


if __name__ == '__main__':
    sys.exit(main())
