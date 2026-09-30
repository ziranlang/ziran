#!/bin/sh
# The portable runner has no statement budget: a program that runs millions
# of statements finishes on .zib with the same output as on C, as it would
# on every native target. Only the web playground bounds its runs.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/long.zi" <<'ZI'
Step :: (value: s64) -> s64 {
    if (value & 1) == 1 { return 3 * value + 1 }
    return value / 2
}

main :: () {
    total: s64 = 0
    for seed: 1..30000 {
        value := seed
        while value != 1 {
            value = Step(value)
            total += 1
        }
    }
    print("%\n", total)
}
ZI

"$ziran" bundle --root "$work" --entry long:main -o "$work/long.zib" "$work/long.zi"
"$ziran" run "$work/long.zib" > "$work/vm.out"
"$ziran" build --target=c --root "$work" -o "$work/c" "$work/long.zi"
printf '#include "long.h"\nint main(void) { long_main(); return 0; }\n' > "$work/c/run.c"
"${CC:-cc}" -std=c99 -I"$work/c" "$work/c/long.c" "$work/c/run.c" -o "$work/c/program"
"$work/c/program" > "$work/c.out"
cmp "$work/c.out" "$work/vm.out"
test "$(cat "$work/vm.out")" -gt 1000000
