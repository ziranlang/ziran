# Source with ziran, repo and module set. The caller runs its C boundary
# regression in the same work directory after this shared backend matrix.
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
compiler_module=${module%_test}
source="$repo/tests/spec/$module.zi"

"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/cmd" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:Answer" \
    -o "$work/answer.zx" "$source"
test "$("$ziran" run "$work/answer.zx")" = 42
"$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:Fingerprint" \
    -o "$work/fingerprint.zx" "$source"
expected="42 $("$ziran" run "$work/fingerprint.zx")"

for input in "$source" "$work/ir/$module.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    output="$work/$form"
    for entry in Answer Fingerprint; do
        "$ziran" bundle --root "$repo/tests/spec" --module-path "$repo/cmd" --entry "$module:$entry" \
            -o "$work/$entry.zx" "$input"
        case "$entry" in
            Answer) test "$("$ziran" run "$work/$entry.zx")" = 42 ;;
            Fingerprint) test "42 $("$ziran" run "$work/$entry.zx")" = "$expected" ;;
        esac
    done
    "$ziran" build --target=c --exe --entry "$module:Run" --root "$repo/tests/spec" \
        --module-path "$repo/cmd" -o "$output/c" "$input"
    test "$("$output/c/$module")" = "$expected"
    "$ziran" build --target=cpp --root "$repo/tests/spec" --module-path "$repo/cmd" \
        -o "$output/cpp" "$input"
    printf '#include "%s.hpp"\nint main() { %s_Run(); }\n' "$module" "$module" > "$output/cpp/run.cpp"
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
"$ziran" ir --root "$repo/cmd" -o "$work/source-ir" "$repo/cmd/$compiler_module.zi"
for input in "$repo/cmd/$compiler_module.zi" "$work/source-ir/$compiler_module.zir"; do
    "$bin/zi2c" --no-main --root "$repo/cmd" -o "$work/source-c" "$input"
    cmp "$repo/bootstrap/$compiler_module/$compiler_module.c" "$work/source-c/$compiler_module.c"
    cmp "$repo/bootstrap/$compiler_module/$compiler_module.h" "$work/source-c/$compiler_module.h"
done
