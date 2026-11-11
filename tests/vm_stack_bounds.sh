#!/bin/sh
set -eu
case $(uname -s) in Linux) ;; *) exit 0 ;; esac
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(dirname "$ziran")/../libziran.a"}
build=$(CDPATH= cd -- "$(dirname -- "$library")" && pwd)
work=$build/tests/vm-stack-bounds
mkdir -p "$work"
cat > "$work/counter.zi" <<'ZI'
counter: s64;
Answer :: () -> s64 { counter += 1; return counter }
ZI
"$ziran" bundle --root "$work" --entry counter:Answer -o "$work/counter.zx" "$work/counter.zi"
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" \
    "$repo/tests/vm_stack_bounds_test.c" "$library" -lm -pthread -ldl \
    -Wl,--wrap=pthread_getattr_np -o "$work/test"
"$work/test" "$work/counter.zx"
echo 'VM stack bounds follow serial worker handoffs, reuse native bounds and refresh after stack-limit changes'
