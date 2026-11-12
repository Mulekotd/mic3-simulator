#!/usr/bin/env python3

import argparse
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "protocol/protocol.yaml"
TARGET = ROOT / "simulator/src/io/generated/ProtocolIds.hpp"


def camel(name):
    if name in ("icache_miss", "dcache_miss"):
        return name[0].upper() + "CacheMiss"

    return "".join(part.capitalize() for part in name.lower().split("_"))


def generate():
    spec = yaml.safe_load(SOURCE.read_text())

    lines = ["// Gerado por tools/generate_protocol_cpp.py; não editar.", "#pragma once", "",
             "#include <cstddef>", "#include <cstdint>", "", "namespace tx {", "",
             f"constexpr std::size_t kMaxPayload = {spec['max_payload_bytes']};",
             f"constexpr uint32_t kMaxProgramWords = {spec['commands']['LOAD_BEGIN']['fields'][1]['max']};",
             f"constexpr uint32_t kMaxWordsPerFrame = {spec['commands']['LOAD_DATA']['length']['max_words']};",
             f"constexpr uint32_t kMaxBreakpointPc = {spec['commands']['SET_BP']['fields'][0]['max']};",
             f"constexpr uint32_t kRamEnd = {spec['commands']['READ_MEM']['fields'][0]['max'] + 1};",
             f"constexpr std::size_t kRegCount = {spec['messages']['CYCLE_STATE']['fields'][9]['count']};", ""]

    for source, name in [("status", "Status"), ("report_mode", "ReportMode"),
                         ("output_kind", "OutputKind"), ("stop_reason", "StopReason"),
                         ("cache_result", "CacheResult"), ("stall_reason", "StallReason")]:

        lines.append(f"enum class {name} : uint8_t {{")

        for key, value in spec["enums"][source].items():
            lines.append(f"    {camel(key)} = {value},")

        lines.extend(["};", ""])
    for source, name in [("commands", "CommandType"), ("messages", "MessageType")]:
        lines.append(f"enum class {name} : uint8_t {{")

        for key, value in spec[source].items():
            lines.append(f"    {camel(key)} = 0x{value['id']:02X},")

        lines.extend(["};", ""])

    lines.extend(["constexpr uint8_t raw(CommandType t) { return static_cast<uint8_t>(t); }",
                  "constexpr uint8_t raw(MessageType t) { return static_cast<uint8_t>(t); }",
                  "constexpr uint8_t raw(Status s) { return static_cast<uint8_t>(s); }",
                  "", "}  // namespace tx", ""])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    generated = generate()

    if args.check:
        if not TARGET.exists() or TARGET.read_text() != generated:
            raise SystemExit("ProtocolIds.hpp desatualizado")
    else:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(generated)


if __name__ == "__main__":
    main()
