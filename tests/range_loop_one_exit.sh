#!/bin/sh
# A range over a computed bound tests that bound once as it starts and then
# leaves only at its last value, so generated C writes it as a loop with one
# exit, `if (first <= last) for (;;)`, that GCC can vectorize. Continues,
# named continues from an inner loop, counting down, an empty range, a bound
# at the type's largest value and a body that changes its binder still count
# right; a while loop whose bound changes, or whose counter is reached through
# a pointer, keeps its test in every iteration.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/app.zi" <<'ZI'
Bump :: (value: *s64) { value.* += 1 }
Sum :: (first: s64, last: s64) -> s64 {
    total: s64 = 0
    for i: first..last { total += i }
    return total
}
Skips :: (count: s64) -> s64 {
    total: s64 = 0
    for i: 0..count {
        if i % 3 == 0 { continue }
        total += i
    }
    return total
}
Down :: (count: s64) -> s64 {
    total: s64 = 0
    for < i: 0..count { total = total * 2 + i }
    return total
}
Named :: (count: s64) -> s64 {
    total: s64 = 0
    for outer: 1..count {
        for inner: 1..count {
            if inner == 2 { continue outer }
            total += outer * 10
        }
    }
    return total
}
Changed :: (count: s64) -> s64 {
    total: s64 = 0
    for i: 0..count {
        total += i
        i += 100
        total += i
    }
    return total
}
Widest :: (first: s32) -> s32 {
    last: s32 = 2147483647
    steps: s32 = 0
    for i: first..last { steps += 1 }
    return steps
}
Shrinking :: (count: s64) -> s64 {
    i: s64 = 0
    n := count
    total: s64 = 0
    while i <= n {
        total += i
        n -= 1
        if i == n { break }
        i += 1
    }
    return total
}
Pointed :: (count: s64) -> s64 {
    i: s64 = 0
    n := count
    total: s64 = 0
    while i <= n {
        total += i
        Bump(*i)
        if i == n { break }
        i += 1
    }
    return total
}
#program_export
Answer :: () -> s64 {
    if Sum(1, 100) != 5050 || Sum(5, 4) != 0 || Sum(-3, -3) != -3 { return 1 }
    if Skips(10) != 37 || Skips(-1) != 0 { return 2 }
    if Down(3) != 34 || Down(-2) != 0 { return 3 }
    if Named(3) != 60 { return 4 }
    if Changed(2) != 306 { return 5 }
    if Widest(2147483645) != 3 { return 6 }
    if Shrinking(10) != 15 || Pointed(10) != 30 { return 7 }
    return 42
}
ZI

"$ziran" bundle --root "$work" --entry app:Answer -o "$work/app.zx" "$work/app.zi"
test "$("$ziran" run "$work/app.zx")" = 42
test "$("$(dirname "$ziran")/zi2zx" run --vm "$work/app.zx")" = 42

output=$work/c
"$ziran" build --target=c --root "$work" -o "$output" "$work/app.zi"
cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
for level in -O0 -O2; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror $level \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
done
for name in Sum Skips Down Named Changed Widest; do
    sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/$name.c"
    if ! grep -q 'for (;;)' "$work/$name.c" || grep -q 'while (' "$work/$name.c"; then
        echo "the range in $name tests its bound in every iteration" >&2
        exit 1
    fi
done
for name in Shrinking Pointed; do
    sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/$name.c"
    if grep -q 'for (;;)' "$work/$name.c"; then
        echo "the while loop in $name tests its bound only once" >&2
        exit 1
    fi
done
