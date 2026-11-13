#!/bin/sh
set -eu

if [ "$#" -gt 0 ]; then
    exec /usr/local/bin/assembler "$@"
fi

set -- /examples/*.asm

if [ ! -f "$1" ]; then
    echo 'Nenhum arquivo .asm encontrado em /examples' >&2
    exit 1
fi

for source do
    /usr/local/bin/assembler "$source"
done
