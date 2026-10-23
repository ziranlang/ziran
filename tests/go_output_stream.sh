#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
case $(uname -s) in Linux) ;; *) exit 0 ;; esac
cat > "$work/no_tmpfile.c" <<'C'
#include <stdio.h>
#include <stdlib.h>
FILE *tmpfile(void) { abort(); }
C
"${CC:-cc}" -shared -fPIC "$work/no_tmpfile.c" -o "$work/no_tmpfile.so"
cat > "$work/numeric.zi" <<'ZI'
Main :: () {
    value: s64 = 84
    print("%\n", value / 2)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/numeric.zi"
for form in source saved; do
    input="$work/numeric.zi"
    root="$work"
    if test "$form" = saved; then input="$work/ir/numeric.zir"; root="$work/ir"; fi
    LD_PRELOAD="$work/no_tmpfile.so" "$ziran" build --target=go --pkg main \
        --exe --entry numeric:Main --root "$root" -o "$work/$form" "$input"
    test "$(GO111MODULE=off go run "$work/$form"/*.go)" = 42
done
mkdir "$work/full"
ln -s /dev/full "$work/full/numeric.go"
if LD_PRELOAD="$work/no_tmpfile.so" "$ziran" build --target=go --no-main \
    --root "$work" -o "$work/full" "$work/numeric.zi" > "$work/out" 2> "$work/error"; then
    echo 'Go output accepted a failed write' >&2
    exit 1
fi
grep -Fq 'cannot finish Go output' "$work/error"
echo 'Go output executes numeric helpers without temporary files and reports write failures'
