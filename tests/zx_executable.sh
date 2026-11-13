#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
zi2asm=${2:-$(dirname "$ziran")/zi2asm}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$zi2asm" in /*) ;; *) zi2asm="$repo/$zi2asm" ;; esac
output="$repo/build/zx-executable-test"
saved="$output/saved"
mkdir -p "$output" "$saved"
rm -f "$output/source.zx" "$output/saved.zx" "$output/pipe.zx" \
    "$output/again.zx" "$output/listing.txt" "$output/legacy.zx" \
    "$output/corrupt.zx" "$output/rejected.out" "$output/error.txt"

source="$repo/tests/spec/zx_executable_program.zi"
"$zi2asm" --root "$repo/tests/spec" --entry zx_executable_program:main \
    --emit zx -o "$output/source.zx" "$source"
"$ziran" ir --root "$repo/tests/spec" -o "$saved" "$source"
"$zi2asm" --root "$saved" --entry zx_executable_program:main --emit zx \
    -o "$output/saved.zx" "$saved/zx_executable_program.zir"
cmp "$output/source.zx" "$output/saved.zx"
"$zi2asm" --root "$repo/tests/spec" --entry zx_executable_program:main \
    --emit zx -o - "$source" > "$output/pipe.zx"
cmp "$output/source.zx" "$output/pipe.zx"

set +e
"$zi2asm" --input zx --run "$output/source.zx"
status=$?
set -e
test "$status" -eq 42
"$zi2asm" --input zx --emit zx -o "$output/again.zx" "$output/source.zx"
cmp "$output/source.zx" "$output/again.zx"
# The internal instruction codec is not a second executable file format.
printf 'preserved output\n' > "$output/rejected.out"
cp "$output/rejected.out" "$output/expected.out"
for removed in input emit; do
    set +e
    "$zi2asm" --input zx --emit zx --"$removed" exec -o "$output/rejected.out" \
        "$output/source.zx" > "$output/error.txt" 2>&1
    status=$?
    set -e
    test "$status" -eq 2
    cmp "$output/expected.out" "$output/rejected.out"
done
"$zi2asm" --input zx --emit lir -o "$output/listing.txt" "$output/source.zx"
grep -q 'function @0 module=m0 name="main"' "$output/listing.txt"
if grep -q 'zx_executable_program.zi' "$output/listing.txt"; then
    echo "stripped executable retained source paths" >&2
    exit 1
fi

cp "$output/source.zx" "$output/legacy.zx"
printf '\032\000\000\000' | dd of="$output/legacy.zx" bs=1 seek=4 conv=notrunc status=none
if "$zi2asm" --input zx --run "$output/legacy.zx" 2>"$output/error.txt"; then
    echo "legacy executable ZX unexpectedly ran" >&2
    exit 1
fi
grep -q 'legacy ZX version contains semantic IR' "$output/error.txt"

cp "$output/source.zx" "$output/corrupt.zx"
printf '\002' | dd of="$output/corrupt.zx" bs=1 seek=16 conv=notrunc status=none
printf 'preserve previous output\n' > "$output/rejected.out"
cp "$output/rejected.out" "$output/expected.out"
if "$zi2asm" --input zx --emit zx -o "$output/rejected.out" "$output/corrupt.zx" 2>"$output/error.txt"; then
    echo "corrupt executable ZX unexpectedly rewrote output" >&2
    exit 1
fi
cmp "$output/expected.out" "$output/rejected.out"
grep -q 'unsupported required executable ZX feature' "$output/error.txt"

echo "zx executable CLI: deterministic source/saved, stripped execution, pipe and rejection local tests passed"
