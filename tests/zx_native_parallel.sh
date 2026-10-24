#!/bin/sh
# Natively run bundles execute `#parallel for` regions on worker threads:
# a pool sized by ZIRAN_PAR_THREADS runs many regions in turn, and a failure
# inside a worker (an index out of bounds, a recursion that fills the
# worker's stack) stops the run with its diagnostic instead of the process.
# The reported failure is the one a serial run meets first.
# Builds without the native engine run the regions serially and skip the
# worker checks.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/rounds.zi" <<'ZI'
Collatz :: (n: s64) -> s64 {
    steps: s64 = 0
    value := n
    while value != 1 {
        if value % 2 == 0 { value = value / 2 } else { value = value * 3 + 1 }
        steps += 1
    }
    return steps
}
#program_export
main :: () -> s32 {
    round := 0
    while round < 40 {
        #parallel for 1..5000 {
            steps := Collatz(it)
            if steps < 0 { steps = 0 }
        }
        round += 1
    }
    return 42
}
ZI
cat > "$work/bounds.zi" <<'ZI'
#program_export
main :: () -> s32 {
    #parallel for 0..100 {
        values: [8]s64
        value := values[it]
        if value > 0 { value = 0 }
    }
    return 42
}
ZI
cat > "$work/deep.zi" <<'ZI'
Depth :: (n: s64) -> s64 {
    if n == 0 { return 0 }
    return Depth(n - 1) + 1
}
#program_export
main :: () -> s32 {
    #parallel for 0..3 {
        depth := Depth(100000000)
        if depth < 0 { depth = 0 }
    }
    return 42
}
ZI
for name in rounds bounds deep; do
    "$ziran" bundle --root "$work" --entry "$name:main" -o "$work/$name.zx" "$work/$name.zi"
done

ZIRAN_NATIVE_CACHE=0 ZIRAN_NATIVE_TIMING=1 ZIRAN_PAR_THREADS=3 \
    "$ziran" run "$work/rounds.zx" > "$work/out" 2> "$work/err"
test "$(cat "$work/out")" = 42
if ! grep -q '^ziran: native' "$work/err"; then
    echo "portable VM build: parallel regions run serially"
    exit 0
fi
grep -q 'native main: 3 parallel workers' "$work/err"
ZIRAN_NATIVE_CACHE=0 ZIRAN_NATIVE_TIMING=1 ZIRAN_PAR_THREADS=1 \
    "$ziran" run "$work/rounds.zx" > "$work/out" 2> "$work/err"
test "$(cat "$work/out")" = 42
! grep -q 'parallel workers' "$work/err"

fails() {
    if ZIRAN_NATIVE_CACHE=0 ZIRAN_PAR_THREADS=4 "$ziran" run "$work/$1.zx" \
        > "$work/$1.out" 2> "$work/$1.err"; then
        echo "$1 succeeded" >&2
        exit 1
    fi
    grep -q "portable execution failed: .*$2" "$work/$1.err" || {
        cat "$work/$1.err" >&2
        exit 1
    }
}
fails bounds 'index 8 out of bounds for values'
fails deep 'filled the stack'
echo "native parallel regions ran on worker threads and reported worker failures"
