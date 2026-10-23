#!/bin/sh
set -eu

ziran=$1
tool_dir=$(dirname "$ziran")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/zip_test.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/zip_test.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry zip_test:SelfTest -o "$work/source.zx" tests/spec/zip_test.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry zip_test:SelfTest -o "$work/saved.zx" "$work/ir/zip_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0

"$ziran" check --root tests/spec --module-path std tests/spec/zip_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/native" tests/spec/zip_linux_test.zi
"${CC:-cc}" -std=c11 -Iinclude -I"$work/native" \
    "$work/native/zip.c" "$work/native/zip_linux.c" \
    "$work/native/file_linux.c" \
    "$work/native/c_string.c" \
    "$work/native/mapped_file_linux.c" \
    "$work/native/byte_text_linux.c" \
    "$work/native/zip_linux_test.c" -x c - -lz -o "$work/zip-test" <<'EOF'
#include <stdint.h>
int SelfTest(void);
int StreamingTest(uint8_t *, uint8_t *);
int main(int argc, char **argv) {
    if (argc != 3 || SelfTest() != 0) return 1;
    return StreamingTest((uint8_t *)argv[1], (uint8_t *)argv[2]);
}
EOF
python3 - "$work/large.zip" <<'PY'
from zipfile import ZIP_DEFLATED, ZipFile
import sys

with ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("long", b"OggS" + b"A" * 150000,
                     compress_type=ZIP_DEFLATED)
PY
(cd "$work" && env -u DISPLAY -u WAYLAND_DISPLAY "$work/zip-test" \
    "$work/large.zip" "$work/large-output.bin")
