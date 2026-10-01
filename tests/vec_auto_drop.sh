#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/owned.zi" <<'ZI'
#import "vec"

Make :: () -> Vec(s32) {
    result: Vec(s32)
    VecPush(result, 7)
    return result
}

Take :: (items: Vec(s32)) -> s32 {
    return items[0]
}

Relay :: (items: Vec(s32)) -> Vec(s32) {
    return items
}

Early :: (stop: bool) -> s32 {
    values: Vec(s32)
    VecPush(values, 3)
    if stop { return values[0] }
    return values[0]
}

Branched :: (first: bool) -> s32 {
    values: Vec(s32)
    VecPush(values, 2)
    if first { return values[0] } else { return 3 }
}

Shadowed :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 4)
    {
        values: Vec(s32)
        VecPush(values, 5)
    }
    return values[0]
}

ReusedSlot :: (first: bool) -> s32 {
    if first {
        values: Vec(s32)
        VecPush(values, 6)
        return values[0]
    }
    scalar: s32 = 9
    if scalar > 0 { return scalar }
    return 0
}

#program_export
Check :: () -> s32 {
    result: s32 = 0
    {
        values: Vec(s32)
        VecPush(values, 1)
        result += values[0]
    }
    result += Early(true)
    result += Early(false)
    result += Branched(true) + Branched(false)
    result += Shadowed()
    result += ReusedSlot(true) + ReusedSlot(false)
    Make()
    moved: Vec(s32) = Make()
    result += Take(moved)
    nested: Vec(s32) = Make()
    result += Take(Relay(nested))
    for_index: s32 = 0
    while for_index < 3 {
        values: Vec(s32)
        VecPush(values, for_index)
        for_index += 1
        if for_index < 3 { continue }
        result += values[0]
        break
    }
    return result
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/owned.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/owned.zi
        root=$work
    else
        module=$work/ir/owned.zir
        root=$work/ir
    fi
    output=$work/$input-c
    "$ziran" build --target=c --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "owned.h"
#include <stddef.h>
#include <stdlib.h>

void *__real_realloc(void *, size_t);
void __real_free(void *);
static int outstanding;

void *__wrap_realloc(void *p, size_t size) {
    void *result = __real_realloc(p, size);
    if (p == NULL && result != NULL) outstanding++;
    return result;
}

void __wrap_free(void *p) {
    if (p != NULL) outstanding--;
    __real_free(p);
}

int main(void) {
    if (Check() != 47) return 1;
    return outstanding == 0 ? 0 : 2;
}
C
    "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
        "$output"/*.c -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/test-$input"
    "$work/test-$input"
    cpp_output=$work/$input-cpp
    "$ziran" build --target=cpp --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$cpp_output" "$module"
    cat > "$cpp_output/main.cpp" <<'CPP'
#include "owned.hpp"
#include <cstddef>

extern "C" void *__real_realloc(void *, std::size_t);
extern "C" void __real_free(void *);
static int outstanding;

extern "C" void *__wrap_realloc(void *p, std::size_t size) {
    void *result = __real_realloc(p, size);
    if (p == nullptr && result != nullptr) outstanding++;
    return result;
}

extern "C" void __wrap_free(void *p) {
    if (p != nullptr) outstanding--;
    __real_free(p);
}

int main() {
    if (Check() != 47) return 1;
    return outstanding == 0 ? 0 : 2;
}
CPP
    "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$cpp_output" \
        "$cpp_output"/*.cpp -Wl,--wrap=realloc -Wl,--wrap=free \
        -o "$work/test-$input-cpp"
    "$work/test-$input-cpp"
    go_output=$work/$input-go
    "$ziran" build --target=go --pkg main --entry owned:Check --root "$root" \
        --module-path "$repo/std" -o "$go_output" "$module"
    cat > "$go_output/main.go" <<'GO'
package main
func main() { if Owned_Check() != 47 { panic("Vec result") } }
GO
    GO111MODULE=off go run "$go_output"/*.go
done
