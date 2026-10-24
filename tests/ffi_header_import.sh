#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/ffi.zi" <<'ZI'
#import, file "sample.h";
libc :: #system_library "libc";
Read :: (value: s32) -> s32 #foreign libc "sample_read";
ZI

for command in check ir bundle; do
    case "$command" in
        check) set -- check --root "$work" "$work/ffi.zi" ;;
        ir) set -- ir --root "$work" -o "$work/ir" "$work/ffi.zi" ;;
        bundle) set -- bundle --root "$work" --entry ffi:Read \
            -o "$work/ffi.zx" "$work/ffi.zi" ;;
    esac
    if "$ziran" "$@" 2> "$work/$command.err"; then
        echo "$command accepted a C header import" >&2
        exit 1
    fi
    grep -Fq '#import, file requires a relative .zi path' "$work/$command.err"
done
for target in c go; do
    if "$ziran" build --target="$target" --root "$work" \
        -o "$work/$target" "$work/ffi.zi" 2> "$work/$target.err"; then
        echo "$target accepted a C header import" >&2
        exit 1
    fi
    grep -Fq '#import, file requires a relative .zi path' "$work/$target.err"
done

cat > "$work/legacy_type.zi" <<'ZI'
Sample :: struct Sample #type
ZI
if "$ziran" check --root "$work" "$work/legacy_type.zi" \
    2> "$work/legacy_type.err"; then
    echo 'C declarator #type unexpectedly passed checking' >&2
    exit 1
fi
grep -Fq 'C declarator #type is not Jai syntax' "$work/legacy_type.err"

cat > "$work/legacy_callback.zi" <<'ZI'
Callback :: void (*)(void *context) #type
ZI
if "$ziran" check --root "$work" "$work/legacy_callback.zi" \
    2> "$work/legacy_callback.err"; then
    echo 'C callback declarator unexpectedly passed checking' >&2
    exit 1
fi
grep -Fq 'C declarator #type is not Jai syntax' "$work/legacy_callback.err"
