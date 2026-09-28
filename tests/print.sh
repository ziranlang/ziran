#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/greet.zi" <<'EOF'
Twice :: (value: s32) -> s32 {
    print("[twice %]", value);
    return value * 2;
}

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

"$ziran" check --root "$work" "$work/greet.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/greet.zi"

for input in "$work/greet.zi" "$work/ir/greet.zir"; do
    case "$input" in
        *.zi) suffix=source ;;
        *) suffix=saved ;;
    esac
    "$ziran" bundle --root "$work" --entry greet:Greet \
        -o "$work/$suffix.zib" "$input"
    "$ziran" run "$work/$suffix.zib" > "$work/$suffix.zib.out"
    printf '0\n' | cat "$work/expected" - | cmp - "$work/$suffix.zib.out"

    for target in c cpp go; do
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
            if test "$target" = c; then
                cat > "$out/main.c" <<'EOF'
#include "greet.h"
int main(void) { return Greet(); }
EOF
                "${CC:-cc}" -std=c99 -pedantic-errors -Iinclude -I"$out" \
                    "$out/greet.c" "$out/main.c" -o "$out/program"
            else
                cat > "$out/main.cpp" <<'EOF'
#include "greet.hpp"
int main() { return Greet(); }
EOF
                "${CXX:-c++}" -std=c++17 -Iinclude -I"$out" \
                    "$out/greet.cpp" "$out/main.cpp" -o "$out/program"
            fi
            "$out/program" > "$out.out"
        fi
        cmp "$work/expected" "$out.out"
    done
done

# The classic program: no return value, so `ziran run` prints only the text.
cat > "$work/hello.zi" <<'EOF'
main :: () {
    print("Hello, World!\n");
}
EOF
"$ziran" bundle --root "$work" --entry hello:main -o "$work/hello.zib" \
    "$work/hello.zi"
test "$("$ziran" run "$work/hello.zib")" = "Hello, World!"

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
reject dynamic_format 'print requires a string literal format' <<'EOF'
main :: () { format := "%\n"; print(format, 1); }
EOF
reject missing_format 'print requires a string literal format' <<'EOF'
main :: () { print(); }
EOF
reject named 'print has no named parameters' <<'EOF'
main :: () { print("%\n", value = 1); }
EOF
reject record 'print argument must be an integer, float, bool, or string' <<'EOF'
Point :: struct { x: s32; }
main :: () { point: Point; print("%\n", point); }
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
