#!/bin/sh
# zi2c lowers every module of a many-module program, in forked workers when
# there are enough modules, yet rewrites only the C whose text changed: after
# a one-module edit every other module's .c and .h stay untouched, so a build
# tool recompiles only what the edit reaches.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(dirname "${1:-$repo/build/bin/ziran}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# A chain of open imports whose public records name the previous module's
# record, so each header includes the one before it, plus an include cycle.
cat > "$work/m0.zi" <<'ZI'
Layer0 :: struct { value: s32; }
Step0 :: (value: s32) -> s32 { return value + 1 }
ZI
i=1
while test "$i" -lt 40; do
    cat > "$work/m$i.zi" <<ZI
#import "m$((i - 1))"
Layer$i :: struct { inner: Layer$((i - 1)); extra: s32; }
Step$i :: (value: s32) -> s32 { return Step$((i - 1))(value) + 1 }
Total$i :: (layer: Layer$i) -> s32 { return Offset(layer.extra) }
#scope_file
Offset :: (value: s32) -> s32 { return value + $i }
ZI
    i=$((i + 1))
done
cat > "$work/left.zi" <<'ZI'
Right :: #import "right";
Wide :: (value: s32) -> s32 { return Right.Pad(value) + 1 }
Double :: (value: s32) -> s32 { return value * 2 }
ZI
cat > "$work/right.zi" <<'ZI'
Left :: #import "left";
Pad :: (value: s32) -> s32 { return value + 4 }
Weigh :: (value: s32) -> s32 { return Left.Double(value) + Pad(0) }
ZI

lower() {
    "$bin/zi2c" --no-main --root "$work" -o "$work/c" "$work"/*.zi
}
# Every output except those named keeps the old timestamp.
untouched_except() {
    for file in "$work/c"/*; do
        case " $* " in *" ${file##*/} "*) continue ;; esac
        test "$(date -r "$file" +%Y)" = 2001 || {
            echo "unchanged output was rewritten: $file" >&2
            exit 1
        }
    done
    for name in "$@"; do
        test "$(date -r "$work/c/$name" +%Y)" != 2001 || {
            echo "edited module's output was not rewritten: $name" >&2
            exit 1
        }
    done
    if ls -A "$work/c" | grep -q '\.tmp$'; then
        echo "temporary output left behind" >&2
        exit 1
    fi
}

lower
grep -q '^#include "m38.h"' "$work/c/m39.h"
grep -q '^#define m39_Step39(...) m39_Step39_ziran_inline(__VA_ARGS__)' "$work/c/m39.h"
for name in left_Wide right_Weigh; do
    if grep -q "${name}_ziran_inline" "$work/c/left.h" "$work/c/right.h"; then
        echo "${name} calls into its include cycle but was copied into a header" >&2
        exit 1
    fi
done
touch -d '2001-01-01 00:00:00' "$work/c"/*
lower
untouched_except

# A file-scope procedure's body reaches only its own module's C file.
sed 's/value + 17 }/value + 1017 }/' "$work/m17.zi" > "$work/m17.next"
mv "$work/m17.next" "$work/m17.zi"
lower
untouched_except m17.c
grep -q '1017' "$work/c/m17.c"

# An inlined body also changes the module's header, and nothing else.
touch -d '2001-01-01 00:00:00' "$work/c"/*
sed 's/(value) + 1 }/(value) + 2 }/' "$work/m5.zi" > "$work/m5.next"
mv "$work/m5.next" "$work/m5.zi"
lower
untouched_except m5.c m5.h

for file in "$work/c"/*.c; do
    "${CC:-cc}" -std=c99 -Wall -Werror -I"$repo/include" -I"$work/c" -c "$file" -o "$work/module.o"
done
