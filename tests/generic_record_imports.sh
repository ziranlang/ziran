#!/bin/sh
# A generic record from another module may hold types its user never
# imports: Bag(K) holds Vec(K) though only the library imports std/vec, and
# the user's Bag(string) reads it as vec.Vec(string). Every module's copy
# of one application gets the same name, so saved bundles re-check. A
# record applied through a named import, Pairs.Pair(s32, string), works
# too. Source and saved IR agree on every target.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/bags.zi" <<'ZI'
#import "std/vec"
Bag :: struct($K: Type) { items: Vec(K); count: s64; }
BagAdd :: (bag: *Bag($K), item: K) {
    VecPush(bag.items, item)
    bag.count += 1
}
ZI

cat > "$work/pairs.zi" <<'ZI'
Pair :: struct($A: Type, $B: Type) { first: A; second: B; }
ZI

cat > "$work/app.zi" <<'ZI'
#import "bags"
Pairs :: #import "pairs"

#program_export
Answer :: () -> s32 {
    bag: Bag(string)
    BagAdd(*bag, "a")
    BagAdd(*bag, "b")
    numbers: Bag(s32)
    BagAdd(*numbers, 7)
    pair: Pairs.Pair(s32, string)
    pair.first = 30
    pair.second = "x"
    if pair.second.count != 1 { return 1 }
    return cast(s32) (bag.count + numbers.count) + pair.first + 9
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
