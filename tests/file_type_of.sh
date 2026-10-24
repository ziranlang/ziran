#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/file_type_of.zi" <<'ZI'
Pair :: struct { left: s32; right: s64; }
pair: Pair;
calls: s32;
Measure :: () -> s32 { calls += 1; return 40; }
GLOBAL_SIZE :: size_of(type_of(calls));
FIELD_SIZE :: size_of(type_of(pair.right));
CALL_SIZE :: size_of(type_of(Measure()));
RUN_SIZE :: #run size_of(type_of(Measure())) + 1;
SELECTED_IFX :: #ifx size_of(type_of(calls)) == 4 then 1 else 0;
#assert GLOBAL_SIZE == 4
#assert FIELD_SIZE == 8
#assert size_of(type_of(pair.right)) == 8
#if CALL_SIZE == 4 {
SELECTED :: 1;
} else {
SELECTED :: 0;
}
#program_export
Answer :: () -> s32 {
    if calls != 0 { return 0 }
    return cast(s32)(GLOBAL_SIZE + FIELD_SIZE + CALL_SIZE +
                     RUN_SIZE + SELECTED + SELECTED_IFX + 19)
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/file_type_of.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/file_type_of.zi
        root=$work
    else
        module=$work/ir/file_type_of.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry file_type_of:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c go; do
        output=$work/$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "file_type_of.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output"/*.c "$work/main.c" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/file_type_of_test.go" <<'GO'
package ziran
import "testing"
func TestFileTypeOf(t *testing.T) {
    if FileTypeOf_Answer() != 42 { t.Fatal("file-scope type_of") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"

cat > "$work/global_query.zi" <<'ZI'
pair: s64;
byte_size: int = size_of(type_of(pair));
#program_export
Answer :: () -> int { return byte_size }
ZI
"$ziran" ir --root "$work" -o "$work/global-ir" "$work/global_query.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/global_query.zi
        root=$work
    else
        module=$work/global-ir/global_query.zir
        root=$work/global-ir
    fi
    for target in c go; do
        output=$work/global-$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "global_query.h"\nint main(void) { return Answer() == 8 ? 0 : 1; }\n' > "$work/global-main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output"/*.c "$work/global-main.c" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/global_query_test.go" <<'GO'
package ziran
import "testing"
func TestGlobalQuery(t *testing.T) {
    if GlobalQuery_Answer() != 8 { t.Fatal("global size query") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done

cat > "$work/unknown.zi" <<'ZI'
BAD :: size_of(type_of(MissingValue));
ZI
expect_rejection 'unknown file-scope type_of operand was accepted' "$work/unknown.err" \
    "$ziran" check --root "$work" "$work/unknown.zi"
rg -q 'unknown|checked expression' "$work/unknown.err"

cat > "$work/type_lib.zi" <<'ZI'
Value :: struct { number: s64; }
shared: Value;
Read :: () -> s32 { return 3 }
ZI
cat > "$work/type_loaded.zi" <<'ZI'
#scope_export
Loaded :: () -> s64 { return 7 }
LoadedValue :: (value: s64) -> s64 { return value + 1 }
LoadedPrivateValue :: () -> s64 { return LoadedPrivateConstant }
LoadedSteps :: () -> s64 {
    local := LoadedPrivateConstant
    local += 1
    return local
}
LoadedGlobal: s32;
#scope_file
LoadedPrivateConstant :: 9;
Secret :: () -> s64 { return 9 }
SecretGlobal: s64;
ZI
cat > "$work/deferred_type_of.zi" <<'ZI'
#import "type_lib"
Lib :: #import "type_lib"
FORWARD :: size_of(type_of(Later()));
IMPORTED :: size_of(type_of(Read()));
QUALIFIED :: size_of(type_of(Lib.Read()));
FIELD :: size_of(type_of(Lib.shared.number));
RUN :: #run size_of(type_of(Later()));
RUN_IMPORTED :: #run size_of(type_of(Lib.Read()));
SELECTED :: #ifx size_of(type_of(Later())) == 8 then 1 else 0;
#if size_of(type_of(Lib.Read())) == 4 {
SELECTED_IF :: 1;
} else {
SELECTED_IF :: 0;
}
#if size_of(type_of(Later())) == 8 {
SELECTED_FORWARD_IF :: 1;
} else {
SELECTED_FORWARD_IF :: 0;
}
#if size_of(type_of(Loaded())) == 8 {
SELECTED_LOADED_IF :: 1;
} else {
SELECTED_LOADED_IF :: 0;
}
#if size_of(type_of(Multiline(1))) == 8 {
SELECTED_MULTILINE_IF :: 1;
} else {
SELECTED_MULTILINE_IF :: 0;
}
#if size_of(type_of(LaterGlobal)) == 8 {
SELECTED_GLOBAL_IF :: 1;
} else {
SELECTED_GLOBAL_IF :: 0;
}
#if size_of(type_of(LoadedGlobal)) == 4 {
SELECTED_LOADED_GLOBAL_IF :: 1;
} else {
SELECTED_LOADED_GLOBAL_IF :: 0;
}
#if size_of(type_of(LaterRecord.value)) == 4 {
SELECTED_FIELD_IF :: 1;
} else {
SELECTED_FIELD_IF :: 0;
}
#if LaterValue(2) == 42 {
SELECTED_CALL_IF :: 1;
} else {
SELECTED_CALL_IF :: 0;
}
#if LoadedValue(7) == 8 {
SELECTED_LOADED_CALL_IF :: 1;
} else {
SELECTED_LOADED_CALL_IF :: 0;
}
#if Multiline(2) == 2 {
SELECTED_MULTILINE_CALL_IF :: 1;
} else {
SELECTED_MULTILINE_CALL_IF :: 0;
}
#if LaterDefault() == 42 {
SELECTED_DEFAULT_CALL_IF :: 1;
} else {
SELECTED_DEFAULT_CALL_IF :: 0;
}
#if LaterDefault(value = 43) == 43 {
SELECTED_NAMED_CALL_IF :: 1;
} else {
SELECTED_NAMED_CALL_IF :: 0;
}
#if LaterConstantValue() == 6 {
SELECTED_CONSTANT_CALL_IF :: 1;
} else {
SELECTED_CONSTANT_CALL_IF :: 0;
}
#if LoadedPrivateValue() == 9 {
SELECTED_PRIVATE_CONSTANT_CALL_IF :: 1;
} else {
SELECTED_PRIVATE_CONSTANT_CALL_IF :: 0;
}
#if LaterSteps(2) == 8 {
SELECTED_STEPS_CALL_IF :: 1;
} else {
SELECTED_STEPS_CALL_IF :: 0;
}
#if LoadedSteps() == 10 {
SELECTED_LOADED_STEPS_CALL_IF :: 1;
} else {
SELECTED_LOADED_STEPS_CALL_IF :: 0;
}
#if LaterBranch(3) == 6 {
SELECTED_BRANCH_CALL_IF :: 1;
} else {
SELECTED_BRANCH_CALL_IF :: 0;
}
#if LaterLoop(6) == 7 {
SELECTED_LOOP_CALL_IF :: 1;
} else {
SELECTED_LOOP_CALL_IF :: 0;
}
#if #defined(LATER_CONSTANT) {
SELECTED_DEFINED_CONSTANT_IF :: 1;
} else {
SELECTED_DEFINED_CONSTANT_IF :: 0;
}
#if #defined(LoadedPrivateConstant) {
SELECTED_HIDDEN_CONSTANT_IF :: 0;
} else {
SELECTED_HIDDEN_CONSTANT_IF :: 1;
}
RUN_STEPS :: #run LaterSteps(2);
RUN_LOOP :: #run LaterLoop(6);
#if RUN_LOOP == 7 {
SELECTED_RUN_LOOP_IF :: 1;
} else {
SELECTED_RUN_LOOP_IF :: 0;
}
global_size: s64 = size_of(type_of(Later()));
#program_export
Answer :: () -> s64 {
    return FORWARD + IMPORTED + QUALIFIED + FIELD + RUN +
           RUN_IMPORTED + SELECTED + SELECTED_IF +
           SELECTED_FORWARD_IF + SELECTED_LOADED_IF +
           SELECTED_MULTILINE_IF + SELECTED_GLOBAL_IF +
           SELECTED_LOADED_GLOBAL_IF + SELECTED_FIELD_IF +
           SELECTED_CALL_IF + SELECTED_LOADED_CALL_IF +
           SELECTED_MULTILINE_CALL_IF + SELECTED_DEFAULT_CALL_IF +
           SELECTED_NAMED_CALL_IF + SELECTED_CONSTANT_CALL_IF +
           SELECTED_PRIVATE_CONSTANT_CALL_IF +
           SELECTED_STEPS_CALL_IF + SELECTED_LOADED_STEPS_CALL_IF +
           SELECTED_BRANCH_CALL_IF + SELECTED_LOOP_CALL_IF +
           SELECTED_RUN_LOOP_IF +
           SELECTED_DEFINED_CONSTANT_IF + SELECTED_HIDDEN_CONSTANT_IF +
           RUN_STEPS + RUN_LOOP + global_size + 5
}
Later :: () -> s64 { return 42 }
LaterDefault :: (value: s64 = 42) -> s64 { return value }
LaterConstantValue :: () -> s64 { return LATER_CONSTANT + 1 }
LATER_CONSTANT :: 5;
LaterValue :: (value: s64) -> s64 {
    return value + 40
}
LaterSteps :: (value: s64) -> s64 {
    first: s64 = LaterIncrement(value)
    second := first * 2; second += 2
    return second
}
LaterIncrement :: (value: s64) -> s64 { return value + 1 }
LaterBranch :: (value: s64) -> s64 {
    if value > 3 {
        return 99
    } else if value > 2 {
        return value * 2
    } else {
        return value + 2
    }
}
LaterLoop :: (limit: s64) -> s64 {
    total := 0
    index := 0
    while index < limit {
        index += 1
        if index == 3 { continue }
        if index == 5 { break }
        total += index
    }
    return total
}
LaterGlobal: s64;
LaterRecord: Payload;
Payload :: struct { value: s32; }
Multiline :: (
    value: s64
) -> s64 {
    return value
}
#load "type_loaded.zi";
ZI

"$ziran" ir --root "$work" -o "$work/deferred-ir" \
    "$work/deferred_type_of.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/deferred_type_of.zi
        root=$work
    else
        module=$work/deferred-ir/deferred_type_of.zir
        root=$work/deferred-ir
    fi
    "$ziran" bundle --root "$root" --entry deferred_type_of:Answer \
        -o "$work/deferred-$input.zx" "$module"
    test "$("$ziran" run "$work/deferred-$input.zx")" = 86
    for target in c go; do
        output=$work/deferred-$target-$input
        "$ziran" build "--target=$target" --root "$root" \
            -o "$output" "$module"
        case "$target" in
            c)
                printf '#include "deferred_type_of.h"\nint main(void) { return Answer() == 86 ? 0 : 1; }\n' > "$work/deferred-main.c"
                "${CC:-cc}" -std=c11 -I"$repo/include" -I"$output" \
                    "$output"/*.c "$work/deferred-main.c" -o "$output/app"
                "$output/app"
                ;;
            go)
                cat > "$output/deferred_type_of_test.go" <<'GO'
package ziran
import "testing"
func TestDeferredTypeOf(t *testing.T) {
    if DeferredTypeOf_Answer() != 86 { t.Fatal("deferred type_of") }
}
GO
                GO111MODULE=off go test "$output"/*.go
                ;;
        esac
    done
done
cmp "$work/deferred-source.zx" "$work/deferred-saved.zx"

cat > "$work/inactive_forward.zi" <<'ZI'
#if false {
Hidden :: () -> s64 { return 7 }
HiddenGlobal: s64;
}
#if size_of(type_of(Hidden())) == 8 {
SELECTED :: 1;
}
ZI
expect_rejection 'inactive procedure leaked into a forward #if query' "$work/inactive_forward.err" \
    "$ziran" check --root "$work" "$work/inactive_forward.zi"
rg -q 'unresolved function|checked expression' "$work/inactive_forward.err"

cat > "$work/inactive_global.zi" <<'ZI'
#if false {
HiddenGlobal: s64;
}
#if size_of(type_of(HiddenGlobal)) == 8 {
SELECTED :: 1;
}
ZI
expect_rejection 'inactive global leaked into a forward #if query' "$work/inactive_global.err" \
    "$ziran" check --root "$work" "$work/inactive_global.zi"
rg -q 'unknown|checked expression' "$work/inactive_global.err"

cat > "$work/private_loaded.zi" <<'ZI'
#if size_of(type_of(Secret())) == 8 {
SELECTED :: 1;
}
#load "type_loaded.zi";
ZI
expect_rejection 'file-private loaded procedure leaked into a #if query' "$work/private_loaded.err" \
    "$ziran" check --root "$work" "$work/private_loaded.zi"
rg -q 'unresolved function|checked expression' "$work/private_loaded.err"

cat > "$work/private_loaded_global.zi" <<'ZI'
#if size_of(type_of(SecretGlobal)) == 8 {
SELECTED :: 1;
}
#load "type_loaded.zi";
ZI
expect_rejection 'file-private loaded global leaked into a #if query' "$work/private_loaded_global.err" \
    "$ziran" check --root "$work" "$work/private_loaded_global.zi"
rg -q 'unknown|checked expression' "$work/private_loaded_global.err"

cat > "$work/forward_global_value.zi" <<'ZI'
#if LaterGlobal == 1 {
SELECTED :: 1;
}
LaterGlobal: s32 = 1;
ZI
expect_rejection 'mutable global value became a compile-time constant' "$work/forward_global_value.err" \
    "$ziran" check --root "$work" "$work/forward_global_value.zi"
rg -q 'not a compile-time constant' "$work/forward_global_value.err"

cat > "$work/forward_effectful_call.zi" <<'ZI'
#if Effectful() == 1 {
SELECTED :: 1;
}
count: s32;
Effectful :: () -> s32 {
    count += 1
    return count
}
ZI
expect_rejection 'effectful forward procedure ran during branch selection' "$work/forward_effectful_call.err" \
    "$ziran" check --root "$work" "$work/forward_effectful_call.zi"
rg -q 'not a compile-time constant' "$work/forward_effectful_call.err"

cat > "$work/private_loaded_call.zi" <<'ZI'
#if Secret() == 9 {
SELECTED :: 1;
}
#load "type_loaded.zi";
ZI
expect_rejection 'file-private loaded procedure ran from another file' "$work/private_loaded_call.err" \
    "$ziran" check --root "$work" "$work/private_loaded_call.zi"
rg -q 'not a compile-time constant' "$work/private_loaded_call.err"

cat > "$work/private_loaded_run.zi" <<'ZI'
VALUE :: #run Secret();
#if VALUE == 9 {
SELECTED :: 1;
}
#load "type_loaded.zi";
ZI
expect_rejection 'file-private loaded procedure ran through a forward #run' "$work/private_loaded_run.err" \
    "$ziran" check --root "$work" "$work/private_loaded_run.zi"
rg -q 'not a compile-time constant|unresolved|private' \
    "$work/private_loaded_run.err"

cat > "$work/forward_recursive_call.zi" <<'ZI'
#if Recursive() == 1 {
SELECTED :: 1;
}
Recursive :: () -> s32 { return Recursive() }
ZI
expect_rejection 'recursive forward procedure exceeded the compile-time limit' "$work/forward_recursive_call.err" \
    "$ziran" check --root "$work" "$work/forward_recursive_call.zi"
rg -q 'not a compile-time constant' "$work/forward_recursive_call.err"

cat > "$work/forward_unbounded_loop.zi" <<'ZI'
#if Unbounded() == 1 {
SELECTED :: 1;
}
Unbounded :: () -> s64 {
    while true { }
    return 1
}
ZI
expect_rejection 'unbounded forward procedure exceeded the compile-time limit' "$work/forward_unbounded_loop.err" \
    "$ziran" check --root "$work" "$work/forward_unbounded_loop.zi"
rg -q 'not a compile-time constant' "$work/forward_unbounded_loop.err"
