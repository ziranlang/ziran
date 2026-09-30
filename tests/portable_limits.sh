#!/bin/sh
# The portable runner holds as many locals as a function declares, lets
# calls nest as deep as the stack allows, and says so when a runaway
# recursion fills it. Its one fixed limit, 16 parameters per function,
# names itself. (Statements are unbounded; see portable_long_runs.sh.)
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

run_program() {
    name=$1
    mkdir -p "$work/$name"
    cat > "$work/$name/app.zi"
    "$ziran" bundle --root "$work/$name" --entry app:main \
        -o "$work/$name/app.zib" "$work/$name/app.zi" > "$work/$name.out" 2>&1 &&
    "$ziran" run "$work/$name/app.zib" >> "$work/$name.out" 2>&1
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

python3 - "$work/params.zi" "$work/locals.zi" <<'PY'
import sys
params = ', '.join('p%d: s32' % i for i in range(17))
args = ', '.join(str(i) for i in range(17))
open(sys.argv[1], 'w').write(
    'Sum :: (%s) -> s32 {\n    return p0 + p16\n}\n'
    'main :: () {\n    print("%%\\n", Sum(%s))\n}\n' % (params, args))
locals_ = '\n'.join('    v%d: s32 = %d' % (i, i) for i in range(200))
open(sys.argv[2], 'w').write(
    'Pick :: (a: s32) -> s32 {\n%s\n    return a + v0 + v199\n}\n'
    'main :: () {\n    print("%%\\n", Pick(1))\n}\n' % locals_)
PY

expect_output locals 200 < "$work/locals.zi"
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
expect_failure params 'portable functions take at most 16 parameters: Sum' < "$work/params.zi"
