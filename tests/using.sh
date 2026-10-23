#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Pair :: struct { left: s64; right: s64; }
Position :: struct { x: s64; y: s64; }
Status :: struct { code: s64; }
Entity :: struct { position: Position; status: Status; }
Container :: struct { entity: Entity; }
Sum :: (using pair: Pair) -> s64 {
    return left + right
}
Explicit :: (pair: Pair) -> s64 {
    using pair;
    left += 1
    return left + right
}
Nested :: (entity: Entity) -> s64 {
    using entity.position;
    return x + y
}
Deep :: (container: Container) -> s64 {
    using container.entity.position;
    return x + y
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib"
#program_export
Answer :: () -> s32 {
    using pair: Pair = Pair.{.left = 20, .right = 22}
    if Sum(pair) != 42 { return 0 }
    if Explicit(Pair.{.left = 19, .right = 22}) != 42 { return 0 }
    left += 1
    if left != 21 { return 0 }
    {
        left: s64 = 9
        if left != 9 { return 0 }
    }
    if left != 21 { return 0 }
    entity := Entity.{.position = Position.{.x = 19, .y = 22},
                      .status = Status.{.code = 1}}
    using entity.position;
    using entity.position;
    using entity.status;
    x += code
    if x + y != 42 { return 0 }
    {
        using entity.position;
        if x != 20 { return 0 }
    }
    if x + y != 42 { return 0 }
    if Nested(entity) != 42 { return 0 }
    container := Container.{.entity = entity}
    if Deep(container) != 42 { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zx" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zx" "$work/ir/app.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in "$work/app.zi" "$work/ir/app.zir"; do
    case "$input" in
        *.zi) suffix=source; root=$work ;;
        *) suffix=saved; root=$work/ir ;;
    esac
    for target in c cpp go; do
        out="$work/$target-$suffix"
        native_answer_42 "$target" "$root" "$out" "$input" \
            app App_Answer 'using'
    done
done

cat > "$work/ambiguous.zi" <<'ZI'
Box :: struct { value: s64; }
Bad :: (using one: Box, using two: Box) -> s64 {
    return value
}
ZI
expect_rejection 'ambiguous using field was accepted' "$work/ambiguous.err" \
    "$ziran" check --root "$work" "$work/ambiguous.zi"
grep -Fq 'ambiguous using field' "$work/ambiguous.err"

cat > "$work/scalar.zi" <<'ZI'
Bad :: (using value: s64) -> s64 { return value }
ZI
expect_rejection 'scalar using binding was accepted' "$work/scalar.err" \
    "$ziran" check --root "$work" "$work/scalar.zi"
grep -Fq 'using requires a concrete record binding' "$work/scalar.err"

cat > "$work/scope.zi" <<'ZI'
Box :: struct { value: s64; }
Bad :: () -> s64 {
    {
        using box: Box = Box.{.value = 1}
    }
    return value
}
ZI
expect_rejection 'out-of-scope using field was accepted' "$work/scope.err" \
    "$ziran" check --root "$work" "$work/scope.zi"
grep -Fq 'unresolved name: value' "$work/scope.err"

cat > "$work/nested_scope.zi" <<'ZI'
Position :: struct { x: s64; }
Entity :: struct { position: Position; }
Bad :: () -> s64 {
    entity := Entity.{.position = Position.{.x = 1}}
    {
        using entity.position;
        if x != 1 { return 0 }
    }
    return x
}
ZI
expect_rejection 'out-of-scope nested using field was accepted' "$work/nested_scope.err" \
    "$ziran" check --root "$work" "$work/nested_scope.zi"
grep -Fq 'unresolved name: x' "$work/nested_scope.err"

cat > "$work/unknown_path.zi" <<'ZI'
Entity :: struct { value: s64; }
Bad :: (entity: Entity) -> s64 {
    using entity.missing;
    return 0
}
ZI
expect_rejection 'unknown using field was accepted' "$work/unknown_path.err" \
    "$ziran" check --root "$work" "$work/unknown_path.zi"
grep -Fq 'unknown using field: missing' "$work/unknown_path.err"

cat > "$work/invalid_path.zi" <<'ZI'
Entity :: struct { value: s64; }
Bad :: (entity: Entity) -> s64 {
    using entity..value;
    return 0
}
ZI
expect_rejection 'invalid using field path was accepted' "$work/invalid_path.err" \
    "$ziran" check --root "$work" "$work/invalid_path.zi"
grep -Fq 'using needs a record binding or field path' "$work/invalid_path.err"

cat > "$work/ambiguous_path.zi" <<'ZI'
Position :: struct { x: s64; }
Entity :: struct { left: Position; right: Position; }
Bad :: (entity: Entity) -> s64 {
    using entity.left;
    using entity.right;
    return x
}
ZI
expect_rejection 'ambiguous nested using field was accepted' "$work/ambiguous_path.err" \
    "$ziran" check --root "$work" "$work/ambiguous_path.zi"
grep -Fq 'ambiguous using field: x' "$work/ambiguous_path.err"

cat > "$work/global_path.zi" <<'ZI'
Position :: struct { x: s64; }
Entity :: struct { position: Position; }
Global: Entity;
Read :: () -> s64 {
    using Global.position;
    return x
}
ZI
"$ziran" check --root "$work" "$work/global_path.zi"

cat > "$work/scalar_path.zi" <<'ZI'
Entity :: struct { value: s64; }
Bad :: (entity: Entity) -> s64 {
    using entity.value;
    return 0
}
ZI
expect_rejection 'scalar using field path was accepted' "$work/scalar_path.err" \
    "$ziran" check --root "$work" "$work/scalar_path.zi"
grep -Fq 'using requires a concrete record binding' "$work/scalar_path.err"
