#!/bin/sh
set -eu

ziran=$1
tool_dir=$(dirname "$ziran")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/zip-file-linux-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/zip_file_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/zip_file_linux_test.zi
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" \
    "$work/c/zip.c" "$work/c/zip_file_linux.c" \
    "$work/c/file_linux.c" "$work/c/c_string.c" \
    "$work/c/mapped_file_linux.c" "$work/c/byte_text_linux.c" \
    "$work/c/zip_file_linux_test.c" -x c - -o "$work/test" <<'EOF'
#include "zip_file_linux_test.h"
#include <stdint.h>
int main(int argc, char **argv) {
    return argc == 3 ? SelfTest((uint8_t *)argv[1], (uint8_t *)argv[2]) : 12;
}
EOF
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test" "$work/archive.tmp" "$work/archive.zip"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/zip_interop_cases.zi" -- "$ziran" "$work" read
