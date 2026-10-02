#!/bin/sh
set -eu
ulimit -c 0

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
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
#ifdef __cplusplus
#include "bounds.hpp"
#else
#include "bounds.h"
#endif
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
    for target in c cpp; do
        output=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" \
            -o "$output" "$module"
        if test "$target" = c; then
            compiler=${CC:-cc}
            standard=c99
            suffix=c
        else
            compiler=${CXX:-c++}
            standard=c++17
            suffix=cpp
        fi
        cp "$work/main.c" "$output/main.$suffix"
        # Match release optimization and disabled assertions. There is no
        # opt-in bounds flag: the language must guard these accesses itself.
        "$compiler" -std="$standard" -O2 -DNDEBUG -I"$output" \
            "$output"/*.$suffix -o "$output/program"
        python3 - "$output/program" <<'PY'
import signal
import subprocess
import sys

program = sys.argv[1]
for function, index, expected in [("read", "1", "42"),
                                  ("write", "0", "17"),
                                  ("text", "1", "90"),
                                  ("unsigned", "1", "42")]:
    result = subprocess.run([program, function, index], capture_output=True, text=True)
    assert result.returncode == 0 and result.stdout.strip() == expected, result

for function in ["read", "write", "text", "unsigned"]:
    for index in ["-1", "2", "4294967296", "9223372036854775807"]:
        result = subprocess.run([program, function, index], capture_output=True, text=True)
        assert result.returncode == -signal.SIGABRT, (function, index, result)
        assert "out of bounds" in result.stderr, (function, index, result.stderr)
PY
    done
done
echo 'default release bounds: fixed arrays, record writes, text and wide indices checked'
