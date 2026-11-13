#!/usr/bin/env python3

import argparse
import struct
import subprocess
import sys
from pathlib import Path


def read_exact(stream, count):
    data = bytearray()

    while len(data) < count:
        chunk = stream.read(count - len(data))

        if not chunk:
            raise RuntimeError("EOF no meio de um quadro TX")

        data.extend(chunk)

    return bytes(data)


def frame(stream):
    kind, length = struct.unpack("<BH", read_exact(stream, 3))

    if length > 512:
        raise RuntimeError("quadro TX excede 512 bytes")

    return kind, read_exact(stream, length)


def send(stream, kind, payload=b""):
    stream.write(struct.pack("<BH", kind, len(payload)) + payload)
    stream.flush()


def ack(stream, command):
    kind, payload = frame(stream)

    if kind != 0x81 or len(payload) != 2 or payload[0] != command or payload[1] != 0:
        raise RuntimeError(f"ACK inesperado: {kind:#x} {payload.hex()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("simulator", help="caminho do executável mic3_simulator")
    parser.add_argument("program", type=Path, help="imagem .bin")
    parser.add_argument("--max-cycles", type=int, default=1000000)

    args = parser.parse_args()
    data = args.program.read_bytes()

    if len(data) % 4 or len(data) > 4096 * 4:
        raise RuntimeError(".bin deve conter até 4096 palavras inteiras")

    count = len(data) // 4

    with subprocess.Popen([args.simulator, "--protocol"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
        send(process.stdin, 0x01, struct.pack("<II", 0, count))
        ack(process.stdout, 0x01)

        for offset in range(0, count, 64):
            send(process.stdin, 0x02, struct.pack("<I", offset) + data[offset * 4:(offset + 64) * 4])
            ack(process.stdout, 0x02)

        send(process.stdin, 0x03)
        ack(process.stdout, 0x03)
        kind, payload = frame(process.stdout)

        if kind != 0x97 or len(payload) != 11 or struct.unpack("<IIHB", payload) != (0, 0xFFC, 0, 0):
            raise RuntimeError("estado inicial da pilha inesperado")

        send(process.stdin, 0x12, struct.pack("<IBH", args.max_cycles, 2, 1))
        ack(process.stdout, 0x12)

        halted = False
        stopped = False
        stats = False

        while not stats:
            kind, payload = frame(process.stdout)

            if kind == 0x91:
                output_kind, value = struct.unpack("<BI", payload)

                if output_kind == 0:
                    print(f"PROGRAM_OUT={struct.unpack('<i', payload[1:])[0]}")
                else:
                    print(f"PROGRAM_OUT_CHAR={chr(value & 255)!r}")
            elif kind == 0x93:
                halted = True
                print(f"HALTED cycles={struct.unpack('<II', payload)[0]}")
            elif kind == 0x95:
                stopped = True
                reason = payload[0]
                print(f"RUN_STOPPED reason={reason}")
            elif kind == 0x94:
                stats = True
            elif kind == 0x9F:
                raise RuntimeError(f"ERROR {payload.hex()}")

        process.stdin.close()

        if process.wait(timeout=5) != 0:
            raise RuntimeError(process.stderr.read().decode(errors="replace"))

        if not halted or not stopped:
            raise RuntimeError("execução sem HALTED ou RUN_STOPPED")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"cliente TX/RX: {error}", file=sys.stderr)
        sys.exit(1)
