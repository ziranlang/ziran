#!/bin/sh
set -eu
ulimit -c 0

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/default-bounds-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/bounds.zi" <<'ZI'
Box :: struct { values: [2]s32; }
box: Box;
#program_export
Read :: (index: s64) -> s32 {
    values := s32.[11, 42]
    return values[index]
}
#program_export
Write :: (index: s64) -> s32 {
    box.values[index] = 17
    return box.values[0]
}
#program_export
Text :: (index: s64) -> s32 {
    value := "AZ"
    return cast(s32)value[index]
}
#program_export
Unsigned :: (index: u64) -> s32 {
    values := s32.[11, 42]
    return values[index]
}
ZI

cat > "$work/main.c" <<'C'
#include "bounds.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if(argc != 3)
        return 2;
    int64_t index = strtoll(argv[2], NULL, 10);
    int32_t result;
    if(strcmp(argv[1], "read") == 0)
        result = Read(index);
    else if(strcmp(argv[1], "write") == 0)
        result = Write(index);
    else if(strcmp(argv[1], "text") == 0)
        result = Text(index);
    else
        result = Unsigned(strtoull(argv[2], NULL, 10));
    printf("%d\n", (int)result);
    return 0;
}
C
"$ziran" ir --root "$work" -o "$work/ir" "$work/bounds.zi"
for input in source saved; do
    root=$work
    module=$work/bounds.zi
    if test "$input" = saved; then
        root=$work/ir
        module=$root/bounds.zir
    fi
    output=$work/$input-c
    "$ziran" build --target=c --root "$root" \
        -o "$output" "$module"
    compiler=${CC:-cc}
    standard=c99
    suffix=c
    cp "$work/main.c" "$output/main.$suffix"
    # Match release optimization and disabled assertions. There is no
    # opt-in bounds flag: the language must guard these accesses itself.
    "$compiler" -std="$standard" -O2 -DNDEBUG -I"$output" \
        "$output"/*.$suffix -o "$output/program"
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/native_inline_cases.zi" -- "$ziran" "$work" bounds "$output/program"
done
echo 'default release bounds: fixed arrays, record writes, text and wide indices checked'
