#!/bin/sh
set -eu
. "$(dirname -- "$0")/lib/expect_rejection.sh"

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/branches.zi" <<'EOF'
MODE :: 2;
#if false {
Hidden :: () -> s32 {
    if true {
        return 1
    } else {
        return 2
    }
}
StillHidden :: s32;
}
#if MODE == 1 {
Value :: () -> s32 { return MissingValue() }
Unused :: struct {
    missing: MissingType
}
#if MISSING_COMPILE_CONSTANT {
AlsoUnused :: MissingType;
}
}
else #if MODE == 2 {
Value :: () -> s32 { return 39 }
} else #if MISSING_COMPILE_CONSTANT {
Value :: () -> s32 { return MissingValue() }
} else {
Value :: () -> s32 { return MissingValue() }
}
Choice :: struct {
    #if MODE == 2 {
        value: s32
    } else {
        missing: MissingType
    }
}
ChoiceEnum :: enum {
    #if MODE == 2 {
        Selected
    } else {
        Missing
    }
}
#program_export
Answer :: () -> s32 {
    using ChoiceEnum;
    result: s32 = Value()
    choice: Choice
    choice.value = 1
    result += choice.value
    if Selected != 0 { return 0 }
    #if MODE == 1 {
        result += MissingValue()
        #if MISSING_COMPILE_CONSTANT {
            result += MissingValue()
        }
    }
    else #if MODE == 2 {
        result += 2
        // A one-line runtime else inside a region is not the region's else.
        if result > 0 { result += 0 } else { result += 100 }
        #if false {
            if true {
                result += MissingValue()
            }
        } else {
            result += 0
        }
    } else #if MISSING_COMPILE_CONSTANT {
        result += MissingValue()
    } else {
        result += MissingValue()
    }
    #if OS == .LINUX || OS != .LINUX {
        result += 0
    }
    return result
}
EOF

"$ziran" check --root "$work" "$work/branches.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/branches.zi"
if grep -aFq 'Hidden' "$work/ir/branches.zir" ||
   grep -aFq 'MissingValue' "$work/ir/branches.zir" ||
   grep -aFq 'MissingType' "$work/ir/branches.zir"; then
    echo 'unselected #if branch was saved in IR' >&2
    exit 1
fi
"$ziran" bundle --root "$work" --entry branches:Answer \
    -o "$work/source.zx" "$work/branches.zi"
"$ziran" bundle --root "$work/ir" --entry branches:Answer \
    -o "$work/saved.zx" "$work/ir/branches.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42
"$ziran" build --target=c --root "$work" \
    -o "$work/c" "$work/branches.zi"
cat > "$work/c/main.c" <<'EOF'
#include "branches.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -Iinclude -I"$work/c" "$work/c/branches.c" \
    "$work/c/main.c" -o "$work/c/app"
"$work/c/app"

out="$work/go"
"$ziran" build --target=go --pkg main --root "$work" \
    -o "$out" "$work/branches.zi"
cat > "$out/main.go" <<'EOF'
package main
func main() { if Branches_Answer() != 42 { panic("wrong #if result") } }
EOF
GO111MODULE=off go run "$out/branches.go" "$out/main.go"

cat > "$work/old_branch.zi" <<'EOF'
#if 1 {
Value :: 1;
} #else_if 0 {
Value :: 2;
}
EOF
expect_rejection 'obsolete #else_if was accepted' "$work/old_branch.err" \
    "$ziran" check --root "$work" "$work/old_branch.zi"
grep -Fq "Jai compile-time branches use 'else #if'" \
    "$work/old_branch.err"

cat > "$work/old_else.zi" <<'EOF'
#if 1 {
Value :: 1;
} #else {
Value :: 2;
}
EOF
expect_rejection 'obsolete #else was accepted' "$work/old_else.err" \
    "$ziran" check --root "$work" "$work/old_else.zi"
grep -Fq "Jai compile-time branches use 'else #if'" \
    "$work/old_else.err"

cat > "$work/runtime_condition.zi" <<'EOF'
#if UNKNOWN {
Answer :: () -> s32 { return 42 }
}
EOF
expect_rejection 'unknown #if condition was accepted' "$work/runtime_condition.err" \
    "$ziran" check --root "$work" "$work/runtime_condition.zi"
grep -Fq '#if condition is not a compile-time constant' \
    "$work/runtime_condition.err"

cat > "$work/unterminated.zi" <<'EOF'
#if true {
Answer :: () -> s32 { return 42 }
EOF
expect_rejection 'unterminated #if was accepted' "$work/unterminated.err" \
    "$ziran" check --root "$work" "$work/unterminated.zi"
grep -Fq 'unterminated #if block' "$work/unterminated.err"

cat > "$work/forward.zi" <<'EOF'
#if LATER == 2 {
Selected :: 40;
} else {
Selected :: MissingValue;
}
RUN :: #run LATER;
LATER :: BASE + 1;
BASE :: 1;
#program_export
Answer :: () -> s64 { return Selected + RUN }
EOF
"$ziran" ir --root "$work" -o "$work/forward-ir" "$work/forward.zi"
if grep -aFq 'MissingValue' "$work/forward-ir/forward.zir"; then
    echo 'unselected forward branch was saved in IR' >&2
    exit 1
fi
for input in "$work/forward.zi" "$work/forward-ir/forward.zir"; do
    case "$input" in
        *.zi) root=$work; label=source ;;
        *.zir) root=$work/forward-ir; label=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry forward:Answer \
        -o "$work/forward-$label.zx" "$input"
    test "$("$ziran" run "$work/forward-$label.zx")" = 42
    for target in c go; do
        out="$work/forward-$label-$target"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        case "$target" in
            c)
                printf '#include "forward.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/forward-main.c"
                ${CC:-cc} -Iinclude -I"$out" "$out"/*.c \
                    "$work/forward-main.c" -o "$out/app"
                "$out/app"
                ;;
            go)
                cat > "$out/forward_test.go" <<'GO'
package ziran
import "testing"
func TestForward(t *testing.T) {
    if Forward_Answer() != 42 { t.Fatal("forward constant") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/forward-source.zx" "$work/forward-saved.zx"

cat > "$work/inactive_constant.zi" <<'EOF'
#if false {
Hidden :: 1;
}
#if Hidden == 1 {
Answer :: () -> s64 { return 42 }
}
EOF
expect_rejection 'constant in inactive branch became visible' "$work/inactive_constant.err" \
    "$ziran" check --root "$work" "$work/inactive_constant.zi"
grep -Fq '#if condition is not a compile-time constant' \
    "$work/inactive_constant.err"

cat > "$work/cyclic_constant.zi" <<'EOF'
#if FIRST == 1 {
Answer :: () -> s64 { return 42 }
}
FIRST :: SECOND;
SECOND :: FIRST;
EOF
expect_rejection 'cyclic forward constants were accepted' "$work/cyclic_constant.err" \
    "$ziran" check --root "$work" "$work/cyclic_constant.zi"
grep -Fq 'cyclic compile-time constant: FIRST' \
    "$work/cyclic_constant.err"

cat > "$work/forward_using.zi" <<'EOF'
Color :: enum { Red :: 1; Blue :: 2; }
#if Red == 1 {
Selected :: 1;
} else {
Selected :: MissingValue;
}
RUN :: #run Red + 40;
#scope_file
using,only("Red") Color;
#scope_export
#program_export
Answer :: () -> s64 { return Selected + RUN }
EOF
"$ziran" ir --root "$work" -o "$work/using-ir" \
    "$work/forward_using.zi"
if grep -aFq 'MissingValue' "$work/using-ir/forward_using.zir"; then
    echo 'unselected forward-using branch was saved in IR' >&2
    exit 1
fi
for input in "$work/forward_using.zi" "$work/using-ir/forward_using.zir"; do
    case "$input" in
        *.zi) root=$work; label=source ;;
        *.zir) root=$work/using-ir; label=saved ;;
    esac
    "$ziran" bundle --root "$root" --entry forward_using:Answer \
        -o "$work/using-$label.zx" "$input"
    test "$("$ziran" run "$work/using-$label.zx")" = 42
    for target in c go; do
        out="$work/using-$label-$target"
        "$ziran" build "--target=$target" --root "$root" -o "$out" "$input"
        case "$target" in
            c)
                printf '#include "forward_using.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/using-main.c"
                ${CC:-cc} -Iinclude -I"$out" "$out"/*.c \
                    "$work/using-main.c" -o "$out/app"
                "$out/app"
                ;;
            go)
                cat > "$out/forward_using_test.go" <<'GO'
package ziran
import "testing"
func TestForwardUsing(t *testing.T) {
    if ForwardUsing_Answer() != 42 { t.Fatal("forward using") }
}
GO
                GO111MODULE=off go test "$out"/*.go
                ;;
        esac
    done
done
cmp "$work/using-source.zx" "$work/using-saved.zx"

cat > "$work/inactive_using.zi" <<'EOF'
Color :: enum { Red :: 1; }
#if false {
using Color;
}
#if Red == 1 {
Answer :: () -> s64 { return 42 }
}
EOF
expect_rejection 'using in inactive branch became visible' "$work/inactive_using.err" \
    "$ziran" check --root "$work" "$work/inactive_using.zi"
grep -Fq '#if condition is not a compile-time constant' \
    "$work/inactive_using.err"

cat > "$work/filtered_using.zi" <<'EOF'
Color :: enum { Red :: 1; Blue :: 2; }
#if Blue == 2 {
Answer :: () -> s64 { return 42 }
}
using,only("Red") Color;
EOF
expect_rejection 'filtered future enum member became visible' "$work/filtered_using.err" \
    "$ziran" check --root "$work" "$work/filtered_using.zi"
grep -Fq '#if condition is not a compile-time constant' \
    "$work/filtered_using.err"

cat > "$work/inactive_tokens.zi" <<'EOF'
#if false {
RejectedType :: i64;
} else #if size_of(int) == 8 {
Selected :: 42;
} else {
Selected :: MissingValue;
}
#if true {
Kept :: 0;
} else #if size_of(i64) == 8 {
Kept :: MissingValue;
}
#program_export
Answer :: () -> s64 {
    #if false {
        bad: i64;
    }
    return Selected + Kept
}
EOF
"$ziran" ir --root "$work" -o "$work/inactive-ir" \
    "$work/inactive_tokens.zi"
if grep -aFq 'RejectedType' "$work/inactive-ir/inactive_tokens.zir" ||
   grep -aFq 'MissingValue' "$work/inactive-ir/inactive_tokens.zir"; then
    echo 'inactive tokens were saved in IR' >&2
    exit 1
fi
"$ziran" bundle --root "$work" --entry inactive_tokens:Answer \
    -o "$work/inactive_tokens.zx" "$work/inactive_tokens.zi"
test "$("$ziran" run "$work/inactive_tokens.zx")" = 42
