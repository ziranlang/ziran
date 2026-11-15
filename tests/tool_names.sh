#!/bin/sh
# check-cost: 5
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$ziran" in /*) ;; *) ziran="$repo/$ziran" ;; esac
bin=$(dirname -- "$ziran")
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/tool-names-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

for tool in zi2zir zi2c zi2go zi2zx zx2wasm zx2native; do
    test -x "$bin/$tool"
done
test ! -e "$bin/zi2asm"
test ! -e "$bin/zi2wasm"

source=$repo/tests/spec/assembly_scalar.zi
"$bin/zi2zx" --root "$repo/tests/spec" --entry assembly_scalar:Answer \
    -o "$work/source.zx" "$source"
"$bin/zi2zir" --root "$repo/tests/spec" -o "$work/zir" "$source"
"$bin/zi2zx" --root "$work/zir" --entry assembly_scalar:Answer \
    -o "$work/saved.zx" "$work/zir/assembly_scalar.zir"
cmp "$work/source.zx" "$work/saved.zx"

"$bin/zi2zx" --emit lir -o "$work/source.lir" "$work/source.zx"
grep -q 'function @' "$work/source.lir"
"$bin/zx2wasm" -o "$work/source.wasm" "$work/source.zx"
test -s "$work/source.wasm"
"$bin/zx2native" --target x86_64 --platform linux --emit elf \
    -o "$work/source.elf" "$work/source.zx"
test -x "$work/source.elf"
set +e
"$work/source.elf"
status=$?
set -e
test "$status" -eq 42
"$bin/zx2native" -o "$work/source-machine.elf" "$work/source.zx"
test -x "$work/source-machine.elf"
set +e
"$work/source-machine.elf"
status=$?
set -e
test "$status" -eq 42

# The high-level build command composes the same format boundaries. Native is
# the default for an explicit source file; named portable targets stay exact.
(
    cd "$work"
    "$ziran" build --root "$repo/tests/spec" --entry assembly_scalar:Answer "$source"
    test -x assembly_scalar
    set +e
    ./assembly_scalar
    status=$?
    set -e
    test "$status" -eq 42
)
"$ziran" build --target=zx --root "$repo/tests/spec" --entry assembly_scalar:Answer \
    -o "$work/high-level.zx" "$source"
cmp "$work/source.zx" "$work/high-level.zx"
"$ziran" build --target=wasm --root "$repo/tests/spec" --entry assembly_scalar:Answer \
    -o "$work/high-level.wasm" "$source"
test -s "$work/high-level.wasm"

for command in \
    "zi2zx bundle --root $repo/tests/spec --entry assembly_scalar:Answer -o $work/rejected.zx $source" \
    "zi2zx --input zx --emit lir -o $work/rejected.lir $work/source.zx"
do
    if PATH="$bin:$PATH" sh -c "$command" >"$work/rejected.out" 2>"$work/rejected.err"; then
        echo "removed command unexpectedly succeeded: $command" >&2
        exit 1
    fi
done

echo 'format-named tool split, high-level build targets, deterministic ZX and removed command rejection local tests passed'
