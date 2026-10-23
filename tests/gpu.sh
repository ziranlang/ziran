#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/gpu.zi" <<'ZI'
Square :: (n: s64) -> s64 { return n * n }
limit: s64;

#program_export
Answer :: () -> s64 {
    #parallel_gpu for 0..10 {
        partial := Square(it)
        if partial > limit { partial = limit }
    }
    return 285
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/gpu.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/gpu.zi
        root=$work
    else
        module=$work/ir/gpu.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry gpu:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 285
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --entry gpu:Answer \
                --root "$root" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() { if Gpu_Answer() != 285 { panic("gpu region") } }
GO
            GO111MODULE=off go run "$output"/*.go
            rg -q "GPU device" "$output/gpu.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --entry gpu:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.c" <<'C'
#include "gpu.h"
int main(void) { return Answer() == 285 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -pedantic-errors -pthread \
                -I"$repo/include" -I"$output" "$output"/*.c \
                -o "$output/app"
            "$output/app"
            rg -q "no GPU device capability" "$output/gpu.c"
        else
            "$ziran" build --target=cpp --entry gpu:Answer --root "$root" \
                -o "$output" "$module"
            cat > "$output/main.cpp" <<'CPP'
#include "gpu.hpp"
int main() { return Answer() == 285 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -pedantic-errors -pthread \
                -I"$repo/include" -I"$output" "$output"/*.cpp \
                -o "$output/app"
            "$output/app"
            rg -q "no GPU device capability" "$output/gpu.cpp"
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"
ZIRAN_PAR_THREADS=1 "$work/c-source/app"
ZIRAN_PAR_THREADS=6 "$work/c-source/app"
ZIRAN_PAR_THREADS=4 "$work/cpp-source/app"

reject() {
    name=$1
    pattern=$2
    if "$ziran" check --root "$work" --module-path "$repo/std" \
        "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$3 was accepted" >&2
        exit 1
    fi
    grep -Fq "$pattern" "$work/$name.err"
}

cat > "$work/bad_pointer_local.zi" <<'ZI'
Buffer :: struct { data: *s64 }
#program_export
Bad :: () -> s64 {
    #parallel_gpu for 0..4 {
        buffer: Buffer
    }
    return 0
}
ZI
reject bad_pointer_local "storage cannot hold pointers" "pointer-bearing storage"

cat > "$work/bad_pointer_arg.zi" <<'ZI'
Read :: (p: *s64) -> s64 { return p.* }
#program_export
Bad :: () -> s64 {
    value := 3
    #parallel_gpu for 0..4 {
        taken := Read(*value)
    }
    return 0
}
ZI
reject bad_pointer_arg "argument holds a pointer" "a pointer argument"

cat > "$work/bad_allocator.zi" <<'ZI'
#import "vec"
#program_export
Bad :: () -> s64 {
    #parallel_gpu for 0..4 {
        fresh: Vec(s64)
        VecPush(fresh, it)
    }
    return 0
}
ZI
reject bad_allocator "storage cannot hold pointers" "allocator-backed storage"

cat > "$work/bad_external.zi" <<'ZI'
host_api :: #system_library "host_api";
Ping :: () -> s64 #foreign host_api;
#program_export
Bad :: () -> s64 {
    #parallel_gpu for 0..4 {
        Ping()
    }
    return 0
}
ZI
reject bad_external "foreign code" "an external-class call"
