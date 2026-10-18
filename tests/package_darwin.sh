#!/bin/sh
# The package launcher and formatter for Darwin: libSystem hashing, errno,
# stat and executable lookup, Mach-O symbol labels and valid C for both ABIs.
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=$(dirname -- "$ziran")/zi2c
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
printf 'Version :: "%s";\n' "$(cat "$repo/VERSION")" > "$work/version.zi"
for arch in __aarch64__ __x86_64__; do
    out="$work/$arch"
    "$compiler" --no-main --define __APPLE__ --define "$arch" --root "$repo/cmd" \
        --module-path "$repo/std" --module-path "$work" -o "$out" "$repo/cmd/package_entry.zi"
    grep -q '__asm__("_CC_SHA256")' "$out"/*
    grep -q '__asm__("_arc4random_buf")' "$out"/*
    grep -q '__asm__("__NSGetExecutablePath")' "$out"/*
    grep -q '__asm__("_open")' "$out"/*
    grep -q '__asm__("___error")' "$out"/*
    if grep -q 'RAND_bytes\|"SHA256"\|__errno_location\|__asm__("_readdir")' "$out"/*; then
        echo "Darwin launcher still uses a Linux or OpenSSL symbol" >&2
        exit 1
    fi
    ${CC:-cc} -std=c11 -D_GNU_SOURCE -fsyntax-only -I"$repo/include" -I"$out" "$out"/*.c
    format="$work/$arch-format"
    "$compiler" --entry format:main --define __APPLE__ --define "$arch" --root "$repo/cmd" \
        --module-path "$repo/std" -o "$format" "$repo/cmd/format.zi"
    grep -q '__asm__("_fstat")' "$format"/*
    if grep -q 'statx' "$format"/*; then
        echo "Darwin formatter still uses statx" >&2
        exit 1
    fi
    ${CC:-cc} -std=c11 -D_GNU_SOURCE -fsyntax-only -I"$repo/include" -I"$format" "$format"/*.c
done
printf 'Darwin package launcher and formatter generation passed for both ABIs.\n'
