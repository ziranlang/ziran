#!/bin/sh
set -eu

ziran=$1
command -v cargo >/dev/null 2>&1 || exit 0
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/empty_text.zi" <<'ZI'
#import "std/vec"

Check :: () -> s32 {
    empty: []u8
    text := TextView(empty)
    if text != "" || text == "x" || "x" == text { return 1 }
    if text[0:0] != "" { return 2 }
    view := empty[0:0]
    if TextView(view) != "" { return 3 }
    vector: Vec(u8)
    if TextView(VecSlice(vector, 0, 0)) != "" { return 4 }
    if BuilderFinish(vector) != "" { return 5 }
    print("A%B\n", text)
    bytes: [3]u8 = .[#char "a", #char "b", #char "c"]
    full := TextView(bytes[:])
    if full[1:2] != "b" || full[3:3] != "" { return 6 }
    return 42
}

main :: () { print("%\n", Check()) }
ZI

printf 'AB\n42\n' > "$work/expected"
"$ziran" ir --root "$work" -o "$work/ir" "$work/empty_text.zi"
for input in "$work/empty_text.zi" "$work/ir/empty_text.zir"; do
    form=source
    case "$input" in *.zir) form=saved ;; esac
    "$ziran" build --target=rust --exe --entry empty_text:main \
        --root "$work" -o "$work/$form" "$input"
    CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet \
        --manifest-path "$work/$form/Cargo.toml"
    "$work/rust-target/debug/ziran_generated" > "$work/$form.out"
    cmp "$work/expected" "$work/$form.out"
done
