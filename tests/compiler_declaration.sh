#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source="$repo/tests/spec/compiler_declaration_test.zi"
module=compiler_declaration_test

"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/cmd" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:Answer" \
    -o "$work/answer.zib" "$source"
test "$("$ziran" run "$work/answer.zib")" = 42
"$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:Fingerprint" \
    -o "$work/fingerprint.zib" "$source"
expected="42 $("$ziran" run "$work/fingerprint.zib")"

for input in "$source" "$work/ir/$module.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    output="$work/$form"
    for entry in Answer Fingerprint; do
        "$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:$entry" \
            -o "$work/$entry.zib" "$input"
        case "$entry" in
            Answer) test "$("$ziran" run "$work/$entry.zib")" = 42 ;;
            Fingerprint) test "42 $("$ziran" run "$work/$entry.zib")" = "$expected" ;;
        esac
    done
    "$ziran" build --target=c --exe --entry "$module:Run" --root "$repo/tests/spec" \
        --module-path "$repo/cmd" -o "$output/c" "$input"
    test "$("$output/c/$module")" = "$expected"
    "$ziran" build --target=cpp --root "$repo/tests/spec" --module-path "$repo/cmd" \
        -o "$output/cpp" "$input"
    printf '#include "compiler_declaration_test.hpp"\nint main() { compiler_declaration_test_Run(); }\n' > "$output/cpp/run.cpp"
    ${CXX:-c++} -std=c++17 -I"$repo/include" -I"$output/cpp" \
        "$output/cpp/"*.cpp -o "$output/cpp/app"
    test "$("$output/cpp/app")" = "$expected"
    "$ziran" build --target=go --exe --entry "$module:Run" --pkg main --root "$repo/tests/spec" \
        --module-path "$repo/cmd" -o "$output/go" "$input"
    test "$(GO111MODULE=off go run "$output/go/"*.go)" = "$expected"
    "$ziran" build --target=py --exe --entry "$module:Run" --root "$repo/tests/spec" \
        --module-path "$repo/cmd" -o "$output/py" "$input"
    test "$(python3 "$output/py")" = "$expected"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry "$module:Run" --root "$repo/tests/spec" \
            --module-path "$repo/cmd" -o "$output/rust" "$input"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet \
            --manifest-path "$output/rust/Cargo.toml"
        test "$("$work/rust-target/debug/ziran_generated")" = "$expected"
    fi
done

# Both source and saved IR must regenerate the exact fresh-build seed.
"$ziran" ir --root "$repo/cmd" -o "$work/source-ir" "$repo/cmd/compiler_declaration.zi"
for input in "$repo/cmd/compiler_declaration.zi" "$work/source-ir/compiler_declaration.zir"; do
    "$bin/zi2c" --no-main --root "$repo/cmd" -o "$work/source-c" "$input"
    cmp "$repo/bootstrap/compiler_declaration/compiler_declaration.c" "$work/source-c/compiler_declaration.c"
    cmp "$repo/bootstrap/compiler_declaration/compiler_declaration.h" "$work/source-c/compiler_declaration.h"
done

${CC:-cc} ${VM_CFLAGS:-} -std=c11 -D_GNU_SOURCE -I"$repo/include" -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-declaration" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-text" \
    -I"$(dirname "${ZIRAN_LIB:-$repo/build/libziran.a}")/compiler-source" \
    "$repo/tests/compiler_declaration_test.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" \
    -o "$work/source-boundary"
"$work/source-boundary"
