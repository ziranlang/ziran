#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
printf '10 11 12 13\n11 12 13 14\n1 2\n12 34\n5 1104\n' > "$work/expected"
"$ziran" ir --root tests/spec -o "$work/ir" tests/spec/compact_branches.zi
for mode in source saved; do
    root=tests/spec
    entry=tests/spec/compact_branches.zi
    if [ "$mode" = saved ]; then root=$work/ir; entry=$work/ir/compact_branches.zir; fi
    "$ziran" bundle --root "$root" --entry compact_branches:main -o "$work/$mode.zib" "$entry"
    "$ziran" run "$work/$mode.zib" > "$work/$mode-vm.out"
    cmp "$work/expected" "$work/$mode-vm.out"
    for target in c cpp go; do
        out=$work/$mode-$target
        if [ "$target" = go ]; then
            "$ziran" build --target=go --pkg main --exe --entry compact_branches:main --root "$root" -o "$out" "$entry"
            (cd "$out" && GO111MODULE=off go run .) > "$out.out"
        elif [ "$target" = c ]; then
            "$ziran" build --target=c --root "$root" -o "$out" "$entry"
            printf '#include "compact_branches.h"\nint main(void) { compact_branches_main(); return 0; }\n' > "$out/run.c"
            "${CC:-cc}" -std=c99 -I"$out" "$out/compact_branches.c" "$out/run.c" -o "$out/program"
            "$out/program" > "$out.out"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$entry"
            printf '#include "compact_branches.hpp"\nint main() { compact_branches_main(); return 0; }\n' > "$out/run.cpp"
            "${CXX:-c++}" -std=c++17 -I"$out" "$out/compact_branches.cpp" "$out/run.cpp" -o "$out/program"
            "$out/program" > "$out.out"
        fi
        cmp "$work/expected" "$out.out"
    done
done
