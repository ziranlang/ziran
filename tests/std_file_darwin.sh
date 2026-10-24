#!/bin/sh
# Native filesystem behavior plus C generation of both Darwin ABIs.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
run=$work/run
mkdir "$run"
input="$repo/tests/std_file_darwin.zi"
set --
if test "$(uname -s)" = Darwin; then
    set -- --define __APPLE__
    if test "$(uname -m)" = x86_64; then
        set -- "$@" --define __x86_64__
    else
        set -- "$@" --define __aarch64__
    fi
fi
"$ziran" ir "$@" --root "$repo/tests" -o "$work/ir" "$input"
for form in source saved; do
    source=$input; module_root=$repo/tests
    if test "$form" = saved; then source=$work/ir/std_file_darwin.zir; module_root=$work/ir; fi
    "$ziran" build "$@" --target=c --exe --entry std_file_darwin:main \
        --root "$module_root" -o "$work/$form" "$source"
    mkdir "$run/$form"
    (cd "$run/$form" && "$work/$form/std_file_darwin")
done
for arch in __aarch64__ __x86_64__; do
    generated="$work/$arch-c"
    "$ziran" build --define __APPLE__ --define "$arch" --target=c \
        --root "$repo/tests" --no-main -o "$generated" "$input"
    "${CC:-cc}" -std=c11 -fsyntax-only -I"$repo/include" "$generated"/*.c
done
printf 'Native source/saved-IR file behavior and both Darwin C ABIs passed.\n'
