#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/choice.zi" <<'EOF'
Point :: struct { x: s32; y: s32; }
ENABLED :: true;
CALCULATED :: #run 2 + 2;
VALUE :: #ifx CALCULATED == 4 then 40 else MissingValue();
OFFSET :: #ifx false then MissingValue(); else 2;
NESTED :: #ifx ENABLED then #ifx false then MissingValue() else 0 else MissingValue();
NESTED_CONDITION :: #ifx (#ifx true then true else false) then 0 else MissingValue();
SKIP :: #ifx true then 0 else (#ifx Unknown then 1 else 2);
PLATFORM :: #ifx OS == .LINUX || OS != .LINUX then 0 else MissingValue();
CHOSEN_POINT :: #ifx true then Point.{.x = 40, .y = 2} else MissingValue();
CHOSEN_ARRAY :: #ifx true then s32.[40, 2] else MissingValue();
chosen_global: Point = #ifx true then Point.{.x = 40, .y = 2} else MissingValue();
counter: s32;
TakePoint :: (point: Point) -> s32 { return point.x + point.y }
TakeArray :: (values: [2]s32) -> s32 { return values[0] + values[1] }

#program_export
Answer :: () -> s32 {
    counter += 1
    chosen_local: Point = #ifx true then Point.{.x = 40, .y = 2} else MissingValue()
    if CHOSEN_POINT.x != 40 || CHOSEN_POINT.y != 2 ||
       CHOSEN_ARRAY[0] != 40 || CHOSEN_ARRAY[1] != 2 ||
       chosen_global.x != 40 || chosen_global.y != 2 ||
       chosen_local.x != 40 || chosen_local.y != 2 { return 0 }
    if TakePoint(#ifx true then Point.{.x = 40, .y = 2} else MissingValue()) != 42 ||
       TakePoint(point = #ifx true then Point.{.x = 40, .y = 2} else MissingValue()) != 42 ||
       TakeArray(#ifx true then s32.[40, 2] else MissingValue()) != 42 { return 0 }
    selected: s32 = #ifx ENABLED then VALUE + OFFSET + NESTED + NESTED_CONDITION + SKIP + PLATFORM else MissingValue()
    inferred := #ifx true then 42; else MissingValue();
    if inferred != 42 { return 0 }
    grouped: s32 = 2 * #ifx true then 3 + 4 else MissingValue()
    if grouped != 14 { return 0 }
    return #ifx false then MissingValue(); else selected;
}
EOF

"$ziran" check --root "$work" "$work/choice.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/choice.zi"
if grep -aFq '#ifx' "$work/ir/choice.zir"; then
    echo 'unlowered #ifx was saved in IR' >&2
    exit 1
fi

for input in "$work/choice.zi" "$work/ir/choice.zir"; do
    case "$input" in
        *.zi) suffix=source ;;
        *) suffix=saved ;;
    esac
    "$ziran" bundle --root "$work" --entry choice:Answer \
        -o "$work/$suffix.zx" "$input"
    test "$("$ziran" run "$work/$suffix.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"

for target in c go; do
    out="$work/$target"
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --root "$work" \
            -o "$out" "$work/choice.zi"
        cat > "$out/main.go" <<'EOF'
package main
func main() { if Choice_Answer() != 42 { panic("wrong #ifx result") } }
EOF
        GO111MODULE=off go run "$out/choice.go" "$out/main.go"
    else
        "$ziran" build "--target=$target" --root "$work" \
            -o "$out" "$work/choice.zi"
        cat > "$out/main.c" <<'EOF'
#include "choice.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
        ${CC:-cc} -Iinclude -I"$out" "$out/choice.c" \
            "$out/main.c" -o "$out/app"
        "$out/app"
    fi
done

cat > "$work/unknown_condition.zi" <<'EOF'
Answer :: (value: bool) -> s32 {
    return #ifx value then 1 else 2
}
EOF
if "$ziran" check --root "$work" "$work/unknown_condition.zi" \
    2> "$work/unknown_condition.err"; then
    echo 'runtime condition was accepted by #ifx' >&2
    exit 1
fi
grep -Fq '#ifx condition is not a compile-time constant' \
    "$work/unknown_condition.err"

cat > "$work/unknown_platform.zi" <<'EOF'
Answer :: () -> s32 { return #ifx OS == .SOLARIS then 1 else 2 }
EOF
if "$ziran" check --root "$work" "$work/unknown_platform.zi" \
    2> "$work/unknown_platform.err"; then
    echo 'unknown OS member was accepted by #ifx' >&2
    exit 1
fi
if ! grep -Fq '#ifx condition is not a compile-time constant' \
    "$work/unknown_platform.err"; then
    cat "$work/unknown_platform.err" >&2
    exit 1
fi

cat > "$work/inline.zi" <<'EOF'
#program_export
Answer :: () -> s32 { return #ifx OS == .LINUX || OS != .LINUX then 42 else MissingValue() }
EOF
"$ziran" bundle --root "$work" --entry inline:Answer \
    -o "$work/inline.zx" "$work/inline.zi"
test "$("$ziran" run "$work/inline.zx")" = 42

cat > "$work/literal_text.zi" <<'EOF'
TEXT :: "#ifx true then 1 else 2";
Answer :: () -> s32 { return 42 }
EOF
"$ziran" check --root "$work" "$work/literal_text.zi"

cat > "$work/global_ifx.zi" <<'EOF'
counter: s32 = #ifx true then 42 else MissingValue();
#program_export
Answer :: () -> s32 { return counter }
EOF
"$ziran" check --root "$work" "$work/global_ifx.zi"
"$ziran" ir --root "$work" -o "$work/global-ir" "$work/global_ifx.zi"
if grep -aFq '#ifx' "$work/global-ir/global_ifx.zir"; then
    echo 'unlowered global #ifx was saved in IR' >&2
    exit 1
fi
for input in "$work/global_ifx.zi" "$work/global-ir/global_ifx.zir"; do
    for target in c go; do
        case "$input" in
            *.zi) suffix=source ;;
            *) suffix=saved ;;
        esac
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$work/global-$target-$suffix" "$input"
        else
            "$ziran" build "--target=$target" --root "$work" \
                -o "$work/global-$target-$suffix" "$input"
        fi
    done
done
