#!/bin/sh
# The portable runner holds as many locals as a function declares, takes
# the language's full 64 parameters (more than 16 once failed), lets calls
# nest as deep as the stack allows, and says so when a runaway recursion
# fills it. (Statements are unbounded; see portable_long_runs.sh.)
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/portable-limits-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

run_program() {
    name=$1
    mkdir -p "$work/$name"
    cat > "$work/$name/app.zi"
    "$ziran" bundle --root "$work/$name" --entry app:main \
        -o "$work/$name/app.zx" "$work/$name/app.zi" > "$work/$name.out" 2>&1 &&
    "$ziran" run "$work/$name/app.zx" >> "$work/$name.out" 2>&1
}

expect_output() {
    name=$1 expected=$2
    run_program "$name" || { cat "$work/$name.out" >&2; exit 1; }
    test "$(cat "$work/$name.out")" = "$expected" || { cat "$work/$name.out" >&2; exit 1; }
}

expect_failure() {
    name=$1 message=$2
    if run_program "$name"; then
        echo "$name: ran past the limit" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.out" || { cat "$work/$name.out" >&2; exit 1; }
}

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" portable-limits

expect_output locals 200 < "$work/locals.zi"
expect_output many 64 < "$work/many.zi"
expect_output deep 5000 <<'ZI'
Depth :: (n: s32) -> s32 {
    if n == 0 { return 0 }
    return 1 + Depth(n - 1)
}
main :: () {
    print("%\n", Depth(5000))
}
ZI
expect_failure runaway 'nested calls filled the stack' <<'ZI'
Forever :: (n: s64) -> s64 {
    return Forever(n + 1) + 1
}
main :: () {
    print("%\n", Forever(0))
}
ZI
