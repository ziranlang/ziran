#!/bin/sh
set -eu

ziran=$1
zi2asm=${2:-$(dirname -- "$ziran")/zi2asm}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/print-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0

cat > "$work/greet.zi" <<'EOF'
Twice :: (value: s32) -> s32 {
    print("[twice %]", value);
    return value * 2;
}

Large :: (binary: bool) {
    bytes: [9000]u8
    for index: 0..8999 { bytes[index] = cast(u8)(65 + index % 26) }
    if binary {
        bytes[4095] = 0
        bytes[4096] = 0
        bytes[8191] = 0
    }
    lowest: s64 = -9223372036854775807 - 1
    widest: u64 = 18446744073709551615
    print("large:<%>:%:%\n", TextView(bytes[:]), lowest, widest)
}

#program_export
Binary :: () { Large(true) }

#program_export
Greet :: () -> s32 {
    name := "Ziran";
    count: s32 = 3;
    small: u8 = 255;
    widest: u64 = 18446744073709551615;
    lowest: s64 = -9223372036854775807 - 1;
    tiny: s8 = -7;
    third: float32 = 0.1;
    zero: float64 = 0.0;
    print("Hello, World!\n");
    print("Hello, %! count=% small=% ok=% 100%%\n", name, count, small, true);
    print("wide: % % %\n", widest, lowest, tiny);
    print("floats: % % % % % %\n", 3.25, 0.1, 1.0e21, 0.000012, 5.0, 1.0 / 3.0);
    print("float32: % zero: % %\n", third, zero, -zero);
    print("escapes: \x25 \"quoted\"\ttab\n", 25);
    print("order: % %\n", Twice(1), Twice(2));
    print("%%%%%\n", "!");
    Large(false)
    return 0;
}
EOF

cat > "$work/expected" <<'EOF'
Hello, World!
Hello, Ziran! count=3 small=255 ok=true 100%
wide: 18446744073709551615 -9223372036854775808 -7
floats: 3.25 0.1 1000000000000000000000 0.000012 5 0.3333333333333333
float32: 0.1 zero: 0 -0
escapes: 25 "quoted"	tab
[twice 1][twice 2]order: 2 4
%%!
EOF
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" print

"$ziran" check --root "$work" "$work/greet.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/greet.zi"

for input in "$work/greet.zi" "$work/ir/greet.zir"; do
    case "$input" in
        *.zi) suffix=source ;;
        *) suffix=saved ;;
    esac
    "$ziran" bundle --root "$work" --entry greet:Greet \
        -o "$work/$suffix.zx" "$input"
    "$ziran" run "$work/$suffix.zx" > "$work/$suffix.zx.out"
    printf '0\n' | cat "$work/expected" - | cmp - "$work/$suffix.zx.out"
    "$ziran" run --vm "$work/$suffix.zx" > "$work/$suffix.vm.out"
    printf '0\n' | cat "$work/expected" - | cmp - "$work/$suffix.vm.out"
    # The VM writes explicit byte lengths, including embedded NULs.
    "$ziran" bundle --root "$work" --entry greet:Binary \
        -o "$work/$suffix-binary.zx" "$input"
    "$ziran" run --vm "$work/$suffix-binary.zx" > "$work/$suffix-binary.out"
    cmp "$work/expected-binary" "$work/$suffix-binary.out"
    native_root=$work
    if test "$suffix" = saved; then native_root=$work/ir; fi
    "$zi2asm" --root "$native_root" --entry greet:Binary --emit elf \
        -o "$work/$suffix-binary.elf" "$input"
    "$work/$suffix-binary.elf" > "$work/$suffix-binary.native.out"
    cmp "$work/expected-binary" "$work/$suffix-binary.native.out"

    for target in c go; do
        out="$work/$suffix-$target"
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$out" "$input"
            cat > "$out/main.go" <<'EOF'
package main
func main() { Greet_Greet() }
EOF
            GO111MODULE=off go run "$out"/*.go > "$out.out"
        else
            "$ziran" build "--target=$target" --root "$work" -o "$out" "$input"
            cat > "$out/main.c" <<'EOF'
#include "greet.h"
int main(void) { return Greet(); }
EOF
            "${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$out" \
                "$out/greet.c" "$out/main.c" -o "$out/program"
            "$out/program" > "$out.out"
        fi
        cmp "$work/expected" "$out.out"
    done

done

# The direct Linux x86-64 native path owns formatting and write_all. Keep this
# fixture float-free: float printing has a stable named gap until its exact
# shortest-round-trip formatter is part of the native image.
cat > "$work/native_print.zi" <<'EOF'
TwiceNative :: (value: s32) -> s32 {
    print("[twice %]", value)
    return value * 2
}

LiteralAndReturn :: () -> u64 {
    print("literal-return: ")
    value: s8 = -1
    return cast(u64)value
}

#program_export
NativePrint :: () -> s32 {
    s8_min: s8 = -127 - 1
    s8_max: s8 = 127
    s16_min: s16 = -32767 - 1
    s16_max: s16 = 32767
    s32_min: s32 = -2147483647 - 1
    s32_max: s32 = 2147483647
    s64_min: s64 = -9223372036854775807 - 1
    s64_max: s64 = 9223372036854775807
    u8_min: u8 = 0
    u8_max: u8 = 255
    u16_min: u16 = 0
    u16_max: u16 = 65535
    u32_min: u32 = 0
    u32_max: u32 = 4294967295
    u64_min: u64 = 0
    u64_max: u64 = 18446744073709551615
    name := "Ziran"
    bytes: [3]u8 = .[65, 66, 67]
    tail := bytes[1:3]
    print("s8: % %\n", s8_min, s8_max)
    print("s16: % %\n", s16_min, s16_max)
    print("s32: % %\n", s32_min, s32_max)
    print("s64: % %\n", s64_min, s64_max)
    print("u8: % %\n", u8_min, u8_max)
    print("u16: % %\n", u16_min, u16_max)
    print("u32: % %\n", u32_min, u32_max)
    print("u64: % %\n", u64_min, u64_max)
    print("bool: % %\n", true, false)
    print("string=% percent=%%\n", name)
    print("order: % %\n", TwiceNative(1), TwiceNative(2))
    print("view:<%>\n", TextView(bytes[:]))
    print("slice:<%> nested:<%>\n", TextView(tail), TextView(tail[1:2]))
    print("%\n", LiteralAndReturn())
    return 0
}
EOF
cat > "$work/native.expected" <<'EOF'
s8: -128 127
s16: -32768 32767
s32: -2147483648 2147483647
s64: -9223372036854775808 9223372036854775807
u8: 0 255
u16: 0 65535
u32: 0 4294967295
u64: 0 18446744073709551615
bool: true false
string=Ziran percent=%
[twice 1][twice 2]order: 2 4
EOF
printf 'view:<ABC>\nslice:<BC> nested:<C>\nliteral-return: 18446744073709551615\n' >> "$work/native.expected"
"$ziran" ir --root "$work" -o "$work/native-ir" "$work/native_print.zi"
"$zi2asm" --root "$work/native-ir" --entry native_print:NativePrint --emit zx \
    -o "$work/native.zx" "$work/native-ir/native_print.zir"
"$zi2asm" --input zx --opt --emit elf -o "$work/native.elf" "$work/native.zx"
"$work/native.elf" > "$work/native.out"
cmp "$work/native.expected" "$work/native.out"
"$ziran" run --target=c --root "$repo/tests" \
    "$repo/tests/native_write_all_cases.zi" -- "$work/native.elf" > "$work/native-injected.out"
cmp "$work/native.expected" "$work/native-injected.out"
set +e
"$work/native.elf" 1>&-
native_closed_status=$?
set -e
test "$native_closed_status" -eq 125

"$ziran" bundle --root "$work" --entry native_print:NativePrint \
    -o "$work/native.zx" "$work/native_print.zi"
"$ziran" run --vm "$work/native.zx" > "$work/native.vm.out"
printf '0\n' | cat "$work/native.expected" - | cmp - "$work/native.vm.out"
"$ziran" build --target=c --root "$work" -o "$work/native-c" "$work/native_print.zi"
cat > "$work/native-c/main.c" <<'EOF'
#include "native_print.h"
int main(void) { return NativePrint(); }
EOF
"${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$work/native-c" \
    "$work/native-c/native_print.c" "$work/native-c/main.c" -o "$work/native-c/program"
"$work/native-c/program" > "$work/native.c.out"
cmp "$work/native.expected" "$work/native.c.out"

cat > "$work/native_float.zi" <<'EOF'
#program_export
NativeFloat :: () { print("%\n", 0.1) }
EOF
"$ziran" ir --root "$work" -o "$work/native-float-ir" "$work/native_float.zi"
"$zi2asm" --root "$work/native-float-ir" --entry native_float:NativeFloat --emit zx \
    -o "$work/native-float.zx" "$work/native-float-ir/native_float.zir"
printf 'preserve-output\n' > "$work/native-float.elf"
if ZIRAN_DIAGNOSTICS=json "$zi2asm" --input zx --opt --emit elf \
    -o "$work/native-float.elf" "$work/native-float.zx" > "$work/native-float.log" 2>&1; then
    echo "native ELF unexpectedly accepted float print formatting" >&2
    exit 1
fi
grep -Fq 'assembly.gap.native-float-format' "$work/native-float.log"
printf 'preserve-output\n' | cmp - "$work/native-float.elf"

# The classic program: no return value, so `ziran run` prints only the text.
cat > "$work/hello.zi" <<'EOF'
main :: () {
    print("Hello, World!\n");
}
EOF
"$ziran" bundle --root "$work" --entry hello:main -o "$work/hello.zx" \
    "$work/hello.zi"
test "$("$ziran" run "$work/hello.zx")" = "Hello, World!"

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "print accepted $name" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err"
}

reject too_few 'print format placeholders do not match arguments: 2 % for 1 argument' <<'EOF'
main :: () { print("% %\n", 1); }
EOF
reject too_many 'print format placeholders do not match arguments: 0 % for 1 argument' <<'EOF'
main :: () { print("none\n", 1); }
EOF
reject trailing_percent 'print format placeholders do not match arguments: 1 % for 0 arguments' <<'EOF'
main :: () { print("broken %\n"); }
EOF
reject dynamic_format 'print requires a string literal format' <<'EOF'
main :: () { format := "%\n"; print(format, 1); }
EOF
reject missing_format 'print requires a string literal format' <<'EOF'
main :: () { print(); }
EOF
reject named 'print has no named parameters' <<'EOF'
main :: () { print("%\n", value = 1); }
EOF
reject pointer 'print argument must be an integer, float, bool, string, enum, record, or array' <<'EOF'
Point :: struct { x: s32; }
main :: () { point: Point; print("%\n", *point); }
EOF
reject vec_holder 'print a value holding a Vec through a variable' <<'EOF'
#import "std/vec"
Holder :: struct { items: Vec(s32); }
Make :: () -> Holder { holder: Holder; return holder; }
main :: () { print("%\n", Make()); }
EOF
reject compile_time '#run expression is not a constant' <<'EOF'
Noisy :: () -> s32 { print("side effect\n"); return 1; }
VALUE :: #run Noisy();
main :: () -> s32 { return VALUE; }
EOF
reject parallel '#parallel cannot print' <<'EOF'
#program_export
Bad :: () -> s64 {
    #parallel for 0..4 {
        print("%\n", it)
    }
    return 0
}
EOF
reject parallel_callee '#parallel calls external code: Shout' <<'EOF'
Shout :: () -> s64 { print("!\n"); return 1 }
#program_export
Bad :: () -> s64 {
    #parallel for 0..4 {
        Shout()
    }
    return 0
}
EOF
