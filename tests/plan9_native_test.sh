#!/bin/sh
# Execute compiler regression fixtures with real Plan 9 8c/8l.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
module=${1:-plan9_record_array_test}
case "$module" in
    plan9_record_array_test|plan9_wide_compare_test) ;;
    *) echo "Unknown native Plan 9 compiler fixture: $module" >&2; exit 2 ;;
esac
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY ENV BASH_ENV
taiji=$(CDPATH= cd -- "${TAIJI_DIR:?Set TAIJI_DIR to the canonical Taiji checkout}" && pwd)
ziran=${ZIRAN:-"$root/build/bin/ziran"}
mkdir -p build/test "$taiji/usr/glenda/tmp"
exec 9>build/test/plan9-native.lock
if ! flock -n 9; then echo 'A native Plan 9 compiler gate is already running' >&2; exit 1; fi
work=$(mktemp -d "$root/build/test/plan9-native.XXXXXX")
stage=$(mktemp -d "$taiji/usr/glenda/tmp/ziran-native.XXXXXX")
guest_stage=/usr/glenda/tmp/${stage##*/}
cleanup() {
    if test -f "$work/output.log"; then cp "$work/output.log" "$root/build/test/$module-native.log"; fi
    rm -rf "$work" "$stage"
}
trap cleanup EXIT HUP INT TERM
source=$root/tests/spec/$module.zi
entry=$module:main
"$ziran" ir --root "$root/tests/spec" --entry "$entry" -o "$work/ir" "$source"
"$ziran" build --target=plan9-c --root "$root/tests/spec" --entry "$entry" -o "$stage/source" "$source"
"$ziran" build --target=plan9-c --root "$work/ir" --entry "$entry" -o "$stage/saved" "$work/ir/$module.zir"
cat > "$work/qemu" <<'QEMU'
#!/bin/sh
exec "$ZIRAN_PLAN9_QEMU" -accel tcg,tb-size=32 "$@"
QEMU
chmod 700 "$work/qemu"
qemu_binary=${QEMU:-qemu-system-x86_64}
command="
failed=0
for(form in source saved) {
    if(~ \$failed 0) {
        cd $guest_stage/\$form
        for(source in *.c) {
            if(! 8c -FTVw \$source) failed=1
        }
        if(~ \$failed 0) {
            if(8l -o run *.8) {
                if(./run) echo ziran-native-ok \$form
                if not { echo ziran-native-failed \$status; failed=1 }
            }
            if not failed=1
        }
    }
}
if(~ \$failed 0) echo ziran-native-all-ok
if not echo ziran-native-failed
fshalt
"
limit=${ZIRAN_PLAN9_TIMEOUT:-120}
setsid --wait env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
    Q9_BOOT_TIMEOUT="$limit" Q9_TMPDIR="$work" Q9_MEM=256M Q9_SMP=1 \
    Q9_CHECKPOINT=0 Q9_BUILD_DESKTOP=0 ZIRAN_PLAN9_QEMU="$qemu_binary" QEMU="$work/qemu" \
    "$taiji/q9" --raw tty-run "$command" >"$work/output.log" 2>&1 &
vm_pid=$!
stop_vm() {
    kill -TERM -"$vm_pid" 2>/dev/null || kill -TERM "$vm_pid" 2>/dev/null || true
    wait "$vm_pid" 2>/dev/null || true
}
trap 'stop_vm; cleanup' EXIT HUP INT TERM
start=$(date +%s)
while test "$(( $(date +%s) - start ))" -lt "$limit"; do
    if rg -q '^ziran-native-all-ok' "$work/output.log"; then
        stop_vm
        echo "$module source and saved IR passed native Plan 9 8c/8l execution"
        exit 0
    fi
    if rg -q '^ziran-native-failed|Operation not permitted|cannot init 9P|can.t init 9P' "$work/output.log"; then
        tail -35 "$work/output.log" >&2; exit 1
    fi
    if ! kill -0 "$vm_pid" 2>/dev/null; then tail -35 "$work/output.log" >&2; exit 1; fi
    sleep 2
done
echo 'Native Plan 9 compiler fixture timed out' >&2
tail -35 "$work/output.log" >&2
exit 1
