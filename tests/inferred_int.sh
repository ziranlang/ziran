#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/inferred_int.zi" <<'EOF'
Wide :: () -> int {
    value := 4294967296
    value += 1
    return value
}
#program_export
Answer :: () -> s32 {
    value := Wide()
    if value != 4294967297 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/inferred_int.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/inferred_int.zi"
"$ziran" bundle --root "$work" --entry inferred_int:Answer \
    -o "$work/source.zx" "$work/inferred_int.zi"
"$ziran" bundle --root "$work/ir" --entry inferred_int:Answer \
    -o "$work/saved.zx" "$work/ir/inferred_int.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in "$work/inferred_int.zi" "$work/ir/inferred_int.zir"; do
    case "$input" in
        *.zi) suffix=source ;;
        *) suffix=saved ;;
    esac
    for target in c go; do
        out="$work/$target-$suffix"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$out" "$input"
            cat > "$out/main.go" <<'EOF'
package main
func main() { if InferredInt_Answer() != 42 { panic("inferred int width") } }
EOF
            GO111MODULE=off go run "$out/inferred_int.go" "$out/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" \
                -o "$out" "$input"
            cat > "$out/main.c" <<'EOF'
#include "inferred_int.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
            ${CC:-cc} -Iinclude -I"$out" "$out/inferred_int.c" \
                "$out/main.c" -o "$out/app"
            "$out/app"
        fi
    done
done
