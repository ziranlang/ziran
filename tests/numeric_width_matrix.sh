#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
python3 "$repo/tests/numeric_width_matrix.py" "$work/width_matrix.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/width_matrix.zi"
for form in source saved; do
    root=$work
    input=$work/width_matrix.zi
    if [ "$form" = saved ]; then root=$work/ir; input=$work/ir/width_matrix.zir; fi
    "$ziran" bundle --root "$root" --entry width_matrix:Answer -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
    for target in c cpp go rust py; do
        output=$work/$form-$target
        if [ "$target" = cpp ]; then
            "$ziran" build --target=cpp --entry width_matrix:Run --root "$root" -o "$output" "$input"
            printf '#include "width_matrix.hpp"\nint main() { Run(); }\n' > "$output/main.cpp"
            ${CXX:-c++} ${VM_CFLAGS:-} -std=c++17 -I"$repo/include" -I"$output" \
                "$output/"*.cpp -o "$output/app"
            result=$("$output/app")
        else
            set --
            if [ "$target" = go ]; then set -- --pkg main; fi
            "$ziran" build "--target=$target" --exe --entry width_matrix:Run \
                "$@" --root "$root" -o "$output" "$input"
            case "$target" in
                c) result=$("$output/width_matrix") ;;
                go) result=$(GO111MODULE=off go run "$output/"*.go) ;;
                py) result=$(python3 "$output") ;;
                rust)
                    CARGO_TARGET_DIR=$work/rust-target cargo build --quiet --manifest-path "$output/Cargo.toml"
                    result=$("$work/rust-target/debug/ziran_generated") ;;
            esac
        fi
        if [ "$result" != 42 ]; then
            echo "$form/$target failed conversion $result" >&2
            exit 1
        fi
    done
done
