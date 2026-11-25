#!/bin/sh
# check-cost: 2
# Verified ZX runs natively in memory: through the embedding API with every
# backend policy, and through `zi2zx run --backend native` on linux-x86_64.
# A bundle native code cannot run fails before it starts with a named gap, and
# a native trap is reported once; nothing falls back to the engine.
set -eu
ziran=${1:?pass the ziran command}
native_load=${2:?pass the native load test}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
zi2zx=$(dirname -- "$ziran")/zi2zx
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/native-load-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0

for entry in Answer Quiet Spin Overflow Bump Long Trapper Retain Grow Fields; do
    "$zi2zx" --root "$repo/tests/spec" -I "$repo/std" --entry "native_bundle:$entry" \
        -o "$work/$entry.zx" "$repo/tests/spec/native_bundle.zi"
done
# The pointer-trap bundles are built with --checks: without it pointers are
# raw, as in C, and nothing traps.
for entry in Dangling AfterFree FreeTwice PastEnd; do
    "$zi2zx" --checks --root "$repo/tests/spec" -I "$repo/std" --entry "native_bundle:$entry" \
        -o "$work/$entry.zx" "$repo/tests/spec/native_bundle.zi"
done
"$zi2zx" --debug --root "$repo/tests/spec" --entry native_bundle:Trapper \
    -o "$work/TrapperDebug.zx" "$repo/tests/spec/native_bundle.zi"
"$native_load" "$work"

case $(uname -s)-$(uname -m) in
    Linux-x86_64) ;;
    *) exit 0 ;;
esac

expect_status() {
    expected=$1
    shift
    set +e
    "$@" > "$work/out" 2> "$work/err"
    status=$?
    set -e
    if test "$status" -ne "$expected"; then
        echo "$* exited $status, expected $expected" >&2
        cat "$work/err" >&2
        exit 1
    fi
}

expect_status 0 "$zi2zx" run --backend native "$work/Long.zx"
printf '20\n' | cmp - "$work/out"

# A run with ZIRAN_JIT_DUMP set publishes the loaded code as a JITDUMP file
# before it runs, so `perf record --jit` can attribute samples to the
# program's functions; a run without it writes nothing.
mkdir "$work/jit"
set +e
ZIRAN_JIT_DUMP="$work/jit" "$zi2zx" run --backend native "$work/Answer.zx" > "$work/out" 2> "$work/err"
jit_status=$?
set -e
test "$jit_status" -eq 0
python3 - "$work/jit" <<'PY'
import glob, struct, sys
files = sorted(glob.glob(sys.argv[1] + "/jit-*.dump"))
assert len(files) == 1, files
data = open(files[0], "rb").read()
magic, version, total, machine, pad, pid, timestamp, flags = struct.unpack_from("<IIIIIIQQ", data, 0)
assert magic == 0x4A695444 and version == 1, (hex(magic), version)
assert total == len(data), (total, len(data))
assert machine == 62, machine
assert pid > 0 and timestamp > 0
at = 40
names = []
while at < len(data):
    rid, size, stamp, code_size, rpad, address, index = struct.unpack_from("<IIQIIQQ", data, at)
    assert size >= 48 and at + size <= len(data), (at, size)
    name = data[at + 40:at + size].split(b"\0", 1)[0]
    assert code_size > 0 and address > 0 and name, (rid, code_size, address, name)
    names.append(name)
    at += size
assert at == len(data)
assert any(name == b"Answer" for name in names), names[:10]
PY

expect_status 1 "$zi2zx" run --backend native "$work/Overflow.zx"
grep -Fq 'native execution failed in Overflow: nested calls filled the stack' "$work/err"

# A trap after output: the output once, the native diagnostic, no engine run.
expect_status 1 "$zi2zx" run --backend native "$work/Trapper.zx"
printf 'before\n' | cmp - "$work/out"
grep -Fq 'native execution failed in Trapper: integer division by zero' "$work/err"
if grep -Fq 'portable execution' "$work/err"; then
    echo "a native trap was retried on the portable engine" >&2
    exit 1
fi
# With the debug attachment the diagnostic names the source line.
expect_status 1 "$zi2zx" run --backend native "$work/TrapperDebug.zx"
grep -Fq 'function=Divide' "$work/err"
grep -Fq 'at native_bundle.zi:' "$work/err"

# The platform picks the default, never the bundle: native code on
# linux-x86_64.
for bundle in Trapper Dangling; do
    expect_status 1 "$zi2zx" run "$work/$bundle.zx"
    grep -Fq "native execution failed in $bundle" "$work/err"
done
expect_status 1 "$zi2zx" run --backend engine "$work/Trapper.zx"
printf 'before\n' | cmp - "$work/out"
grep -Fq 'portable execution failed in Trapper: integer division by zero' "$work/err"

# Native pointers are checked: the read through a pointer into a returned
# frame traps after the output before it, exactly as on the engine.
expect_status 1 "$zi2zx" run --backend native "$work/Dangling.zx"
printf 'started\n' | cmp - "$work/out"
grep -Fq 'native execution failed in Dangling: an index or address is out of bounds or no longer valid' "$work/err"
expect_status 0 "$zi2zx" run --backend native "$work/Grow.zx"
printf '42\n' | cmp - "$work/out"
for bundle in AfterFree FreeTwice PastEnd; do
    expect_status 1 "$zi2zx" run --backend native "$work/$bundle.zx"
    grep -Fq "native execution failed in $bundle: an index or address is out of bounds or no longer valid" "$work/err"
done
expect_status 1 "$zi2zx" run --backend vm "$work/Answer.zx"
