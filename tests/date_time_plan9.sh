#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$repo/tests/spec/date_time_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/source" "$repo/tests/spec/date_time_plan9_test.zi"
"$ziran" build --target=plan9-c --root "$work/ir" -o "$work/saved" "$work/ir/date_time_plan9_test.zir"
for input in source saved; do
    # libc declares native Tm* results and a long argument. Re-declaring the
    # foreign aliases with Ziran's opaque void* signature conflicts with it.
    if rg -n '^(void\*|int64_t) (localtime|gmtime|nsec)\(' "$work/$input/date_time_plan9.c"; then
        echo 'plan9-c redeclared native clock functions' >&2
        exit 1
    fi
done
for module in date_time_plan9 date_time_types calendar date_time_plan9_test; do
    cmp "$work/source/$module.c" "$work/saved/$module.c"
    cmp "$work/source/$module.h" "$work/saved/$module.h"
done
echo 'date-time-plan9: source and saved IR generation passed (native execution uses the Taiji guest gate)'
