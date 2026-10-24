#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/comments.zi" <<'ZI'
// A color's quoted "name" and path / must not affect parser state.
#program_export
Answer :: () -> s32 {
    value: s32 = 40
    // This comment's trailing operator + is text, not a continuation +
    value += 2
    return value
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/comments.zi"
"$ziran" bundle --root "$work" --entry comments:Answer \
    -o "$work/source.zx" "$work/comments.zi"
"$ziran" bundle --root "$work/ir" --entry comments:Answer \
    -o "$work/saved.zx" "$work/ir/comments.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/comments.zi"
printf '#include "comments.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c/comments.c" "$work/main.c" -o "$work/app"
"$work/app"

cat > "$work/hash_file.zi" <<'ZI'
# A legacy file comment
Answer :: () -> s32 { return 42 }
ZI
cat > "$work/hash_type.zi" <<'ZI'
Record :: struct {
    # A legacy type comment
    value: s32
}
ZI
cat > "$work/hash_body.zi" <<'ZI'
Answer :: () -> s32 {
    # A legacy function comment
    return 42
}
ZI
cat > "$work/hash_bare.zi" <<'ZI'
#
Answer :: () -> s32 { return 42 }
ZI

for scope in file type body bare; do
    source="$work/hash_$scope.zi"
    for command in check ir; do
        if "$ziran" "$command" --root "$work" \
            -o "$work/hash-ir-$scope" "$source" \
            > "$work/$scope-$command.out" 2>&1; then
            echo "$command accepted a legacy hash comment in $scope scope" >&2
            exit 1
        fi
        rg -q 'unknown .*directive' "$work/$scope-$command.out"
    done
    for target in c go; do
        if "$ziran" build "--target=$target" --root "$work" \
            -o "$work/hash-$scope-$target" "$source" \
            > "$work/$scope-$target.out" 2>&1; then
            echo "$target build accepted a legacy hash comment in $scope scope" >&2
            exit 1
        fi
        rg -q 'unknown .*directive' "$work/$scope-$target.out"
    done
done
