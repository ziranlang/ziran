#!/bin/sh
# The portable runner's fixed limits name themselves when a program hits
# them: parameters per function, parameters and locals held at once, and
# call depth. (Statements are unbounded; see portable_long_runs.sh.)
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

expect_failure() {
    name=$1 message=$2
    mkdir -p "$work/$name"
    cat > "$work/$name/app.zi"
    # Bundling verifies static limits; running reports call depth.
    if "$ziran" bundle --root "$work/$name" --entry app:main \
           -o "$work/$name/app.zib" "$work/$name/app.zi" > "$work/$name.out" 2>&1 &&
       "$ziran" run "$work/$name/app.zib" >> "$work/$name.out" 2>&1; then
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
locals_ = '\n'.join('    v%d: s32 = %d' % (i, i) for i in range(65))
open(sys.argv[2], 'w').write(
    'main :: () {\n%s\n    print("%%\\n", v0 + v64)\n}\n' % locals_)
PY

expect_failure params 'portable functions take at most 16 parameters: Sum' < "$work/params.zi"
expect_failure locals 'portable functions hold at most 64 parameters and locals at once: main' < "$work/locals.zi"
expect_failure depth 'portable execution failed: calls nested deeper than 128' <<'ZI'
Depth :: (n: s32) -> s32 {
    if n == 0 { return 0 }
    return 1 + Depth(n - 1)
}
main :: () {
    print("%\n", Depth(1000))
}
ZI

# Just inside the limits still runs.
mkdir -p "$work/inside"
cat > "$work/inside/app.zi" <<'ZI'
Depth :: (n: s32) -> s32 {
    if n == 0 { return 0 }
    return 1 + Depth(n - 1)
}
main :: () {
    print("%\n", Depth(100))
}
ZI
"$ziran" bundle --root "$work/inside" --entry app:main -o "$work/inside/app.zib" "$work/inside/app.zi"
test "$("$ziran" run "$work/inside/app.zib")" = 100
