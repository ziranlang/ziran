#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/explain-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" --help > "$work/help.txt"
grep -Fq 'explain' "$work/help.txt"
"$ziran" guide > "$work/guide.txt"
grep -Fq 'ziran explain --list --json' "$work/guide.txt"
"$ziran" explain check.slice_lifetime > "$work/text.txt"
grep -Fq 'code: check.slice_lifetime' "$work/text.txt"
grep -Fq 'borrowed slice or text view' "$work/text.txt"
grep -Fq 'Keep the owner live and immutable' "$work/text.txt"
"$ziran" explain zir.noncanonical --json > "$work/item.json"
"$ziran" explain --list --json > "$work/list.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/explain_cases.zi" -- "$ziran" "$work"
cat > "$work/go-import.zi" <<'ZI'
libc :: #system_library "libc";
Read :: (value: []u8) -> s32 #foreign libc "read";
ZI
if "$ziran" build --target=go --diagnostics=json --root "$work" \
    -o "$work/go-import" "$work/go-import.zi" \
    > "$work/go-import.out" 2> "$work/go-import.jsonl"; then
    echo 'Go accepted an unsupported C ABI slice parameter' >&2
    exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- code-message "$work/go-import.jsonl" zir_go.import 'C ABI symbol'
cat > "$work/output.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI
if "$ziran" build --target=go --diagnostics=json --root "$work" \
    -o /proc/ziran-go-output "$work/output.zi" \
    > "$work/output.out" 2> "$work/output.jsonl"; then
    echo 'Go output failure did not fail the build' >&2
    exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- code-message "$work/output.jsonl" zir_go.global /proc/ziran-go-output

if "$ziran" build --target=c --diagnostics=json --root "$work" \
    -o /proc/ziran-c-output "$work/output.zi" \
    > "$work/c-output.out" 2> "$work/c-output.jsonl"; then
    echo "c output failure did not fail the build" >&2
    exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/json_assertions.zi" -- code-message "$work/c-output.jsonl" zir_c.global /proc/ziran-

if "$ziran" explain check.not_a_code > "$work/bad.out" 2> "$work/bad.err"; then
    echo 'explain accepted an unknown diagnostic code' >&2
    exit 1
fi
grep -Fq 'unknown diagnostic code: check.not_a_code' "$work/bad.err"
if "$ziran" explain --json > "$work/no-code.out" 2> "$work/no-code.err"; then
    echo 'explain accepted --json without a code or --list' >&2
    exit 1
fi
