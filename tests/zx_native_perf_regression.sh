#!/bin/sh
# check-cost: 4
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-$repo/build}/zx-native-perf.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

cat > "$work/steps.zi" <<'ZI'
Leaf :: () -> s64 { return 7 }
Work :: () -> s64 {
    total: s64 = 0
    for value: 0..31 { total += Leaf() + value }
    return total
}
#program_export
main :: () -> s64 { return Work() + 112 }
#program_export
Wide :: () -> s64 {
    total: s64 = 0
    for value: 0..599 { total += Leaf() + value }
    return total + 112
}
ZI

cat > "$work/parallel.zi" <<'ZI'
Value :: (index: s64) -> s64 {
    value := index * 3
    for count: 0..15 { value += count }
    return value
}
#program_export
main :: () -> s32 {
    #parallel for 0..127 {
        value := Value(it)
        if value < 0 { value = 0 }
    }
    return 42
}
ZI

"$ziran" bundle --root "$work" --entry steps:main -o "$work/steps.zx" "$work/steps.zi"
"$ziran" bundle --root "$work" --entry steps:Wide -o "$work/wide.zx" "$work/steps.zi"
"$ziran" bundle --root "$work" --entry parallel:main -o "$work/parallel.zx" "$work/parallel.zi"

cat > "$work/host.c" <<'C'
#include "ziran_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
runs_within(const char *path, int expected, int steps, long long answer)
{
    Bundle *bundle = BundleOpen(path);
    if(bundle == NULL)
        return 0;
    BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
    if(instance == NULL)
        return 0;
    int has_answer = 0;
    long long result = 0;
    BundleInstanceLimitSteps(instance, steps);
    int ok = BundleInstanceRun(instance, &result, &has_answer) == expected;
    if(ok && expected)
        ok = has_answer && result == answer;
    if(instance != NULL)
        BundleInstanceClose(instance);
    BundleClose(bundle);
    return ok;
}

int
main(int argc, char **argv)
{
    if(argc != 5)
        return 2;
    int expected = strcmp(argv[2], "success") == 0;
    if(!expected && strcmp(argv[2], "failure") != 0)
        return 2;
    return runs_within(argv[1], expected, atoi(argv[3]),
                      strtoll(argv[4], NULL, 10)) ? 0 : 1;
}
C

library=${ZIRAN_LIB:-$repo/build/libziran.a}
case $(uname -s) in
Linux) libraries='-lm -pthread -ldl' ;;
Darwin) libraries='-lm' ;;
*) libraries='-lm' ;;
esac
"${CC:-cc}" -std=c11 -I"$repo/include" "$work/host.c" "$library" $libraries -o "$work/host"

show_log() {
    tail -n 120 "$1" >&2
}

run_case() {
    cache=$1
    threads=$2
    log=$3
    shift 3
    if ! ZIRAN_NATIVE_CACHE="$cache" ZIRAN_NATIVE_DIR="$work" \
        ZIRAN_PAR_THREADS="$threads" "$@" 2>"$log"; then
        show_log "$log"
        return 1
    fi
}

require_error() {
    text=$1
    log=$2
    grep -q "$text" "$log" || {
        show_log "$log"
        return 1
    }
}

ZIRAN_NATIVE_TIMING=1 run_case "$work/steps-cache" 1 "$work/cold-native.err" \
    "$work/host" "$work/steps.zx" success 66 832
if ! grep -Eq '^ziran: native main( invocation)?:' "$work/cold-native.err"; then
    echo 'portable build: native step lowering unavailable'
    exit 0
fi

run_case "$work/steps-cache" 1 "$work/cached-native.err" \
    "$work/host" "$work/steps.zx" success 66 832
run_case "$work/steps-failure-cache" 1 "$work/cold-failure.err" \
    "$work/host" "$work/steps.zx" failure 65 0
require_error 'stopped after 65 steps' "$work/cold-failure.err"
run_case "$work/steps-failure-cache" 1 "$work/cached-failure.err" \
    "$work/host" "$work/steps.zx" failure 65 0
require_error 'stopped after 65 steps' "$work/cached-failure.err"

run_case "$work/wide-cache" 1 "$work/wide-cold.err" \
    "$work/host" "$work/wide.zx" success 1201 184012
run_case "$work/wide-cache" 1 "$work/wide-cached.err" \
    "$work/host" "$work/wide.zx" success 1201 184012
run_case "$work/wide-cache" 1 "$work/wide-unlimited.err" \
    "$work/host" "$work/wide.zx" success 0 184012
run_case "$work/wide-failure-cache" 1 "$work/wide-failure.err" \
    "$work/host" "$work/wide.zx" failure 1200 0
require_error 'stopped after 1200 steps' "$work/wide-failure.err"

run_case "$work/parallel-cache" 1 "$work/parallel-one.err" \
    "$work/host" "$work/parallel.zx" success 2177 42
run_case "$work/parallel-cache" 1 "$work/parallel-one-failure.err" \
    "$work/host" "$work/parallel.zx" failure 2176 0
require_error 'stopped after 2176 steps' "$work/parallel-one-failure.err"
run_case "$work/parallel-cache" 3 "$work/parallel-three.err" \
    "$work/host" "$work/parallel.zx" success 2177 42
run_case "$work/parallel-cache" 3 "$work/parallel-exhausted.err" \
    "$work/host" "$work/parallel.zx" failure 1 0
require_error 'stopped after 1 steps' "$work/parallel-exhausted.err"
echo 'ZX native gas boundaries and parallel budgets passed'
