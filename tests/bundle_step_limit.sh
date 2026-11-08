#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-$repo/build}/bundle-step-limit.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
runtime_libs=-lm
case $(uname -s) in Linux) runtime_libs="$runtime_libs -ldl -pthread" ;; esac
cat > "$work/loop.zi" <<'EOF'
Spin :: () -> s32 { while true {} return 0 }
value: s32 = Spin();
main :: () -> s32 { return value }
EOF
cat > "$work/finite.zi" <<'EOF'
api :: #system_library "host_api";
Tick :: () #foreign api;
Work :: () -> s32 {
    count: s32 = 0
    while count < 100 { Tick(); count += 1 }
    return 7
}
value: s32 = Work();
main :: () -> s32 { return value + Work() }
EOF
"$ziran" bundle --root "$work" --entry loop:main -o "$work/loop.zx" "$work/loop.zi"
"$ziran" bundle --root "$work" --entry finite:main -o "$work/finite.zx" "$work/finite.zi"
for library in "${ZIRAN_VM_LIB:-$repo/build/libziran-vm.a}" "${ZIRAN_LIB:-$repo/build/libziran.a}"; do
    "${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" \
        "$repo/tests/bundle_step_limit_test.c" "$library" $runtime_libs -o "$work/host"
    timeout 20 "$work/host" "$work/loop.zx" "$work/finite.zx"
done
