#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

command -v cargo >/dev/null 2>&1 || {
    echo 'cargo is required to test Rust print output' >&2
    exit 1
}

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

# Plan 9 C builds with plan9port when it is installed, else a host libc shim.
plan9=${PLAN9:-}
if test -z "$plan9"; then
    for candidate in "$HOME/Projects/plan9port" /usr/local/plan9 /usr/lib/plan9; do
        if test -x "$candidate/bin/9c"; then plan9=$candidate; break; fi
    done
fi

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

    rust_out="$work/$suffix-rust"
    "$ziran" build --target=rust --exe --entry greet:Greet \
        --root "$work" -o "$rust_out" "$input"
    cargo build --quiet --manifest-path "$rust_out/Cargo.toml"
    "$rust_out/target/debug/ziran_generated" > "$rust_out.out"
    cmp "$work/expected" "$rust_out.out"

    py_out="$work/$suffix-py"
    "$ziran" build --target=py --exe --entry greet:Greet \
        --root "$work" -o "$py_out" "$input"
    python3 "$py_out" > "$py_out.out"
    cmp "$work/expected" "$py_out.out"

    # Plan 9 C: plan9port when installed, else the host through a libc shim.
    plan9_out="$work/$suffix-plan9"
    "$ziran" build --target=plan9-c --root "$work" -o "$plan9_out" "$input"
    if grep -n '#include <std' "$plan9_out"/*.c "$plan9_out"/*.h; then
        echo 'plan9-c print output retained hosted C headers' >&2
        exit 1
    fi
    if test -n "$plan9" && test -x "$plan9/bin/9c"; then
        # plan9port compiles against the real Plan 9 u.h and libc.h.
        cat > "$plan9_out/driver.c" <<'EOF'
#include <u.h>
#include <libc.h>
#include "greet.h"
void main(int argc, char **argv) { USED(argc); USED(argv); Greet(); exits(nil); }
EOF
        (cd "$plan9_out" &&
            PLAN9=$plan9 "$plan9/bin/9c" -I. greet.c driver.c 2> 9c.log &&
            PLAN9=$plan9 "$plan9/bin/9l" -o program greet.o driver.o) ||
            { cat "$plan9_out/9c.log" >&2; exit 1; }
        "$plan9_out/program" > "$plan9_out.out"
    else
        mkdir -p "$work/plan9-include"
        cat > "$work/plan9-include/u.h" <<'EOF'
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef long long vlong;
typedef unsigned long long uvlong;
typedef unsigned long usize;
EOF
        cat > "$work/plan9-include/libc.h" <<'EOF'
extern void *realloc(void *, unsigned long);
extern void free(void *);
extern void *memmove(void *, const void *, unsigned long);
extern void *memcpy(void *, const void *, unsigned long);
extern void *memset(void *, int, unsigned long);
extern int memcmp(const void *, const void *, unsigned long);
extern int fprint(int, const char *, ...);
extern int snprint(char *, int, const char *, ...);
extern long write(int, const void *, long);
extern double strtod(const char *, char **);
extern int atoi(const char *);
extern int isNaN(double);
extern int isInf(double, int);
extern void exits(const char *);
extern void abort(void);
EOF
        cat > "$work/plan9-shim.c" <<'EOF'
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int fprint(int fd, const char *format, ...) {
    char buffer[512];
    va_list arguments;
    int length;
    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return (int)write(fd, buffer, (size_t)length);
}
int snprint(char *buffer, int size, const char *format, ...) {
    va_list arguments;
    int length;
    va_start(arguments, format);
    length = vsnprintf(buffer, (size_t)size, format, arguments);
    va_end(arguments);
    return length;
}
int isNaN(double value) { return isnan(value); }
int isInf(double value, int sign) {
    return sign == 0 ? isinf(value) != 0 :
        sign > 0 ? value == INFINITY : value == -INFINITY;
}
void exits(const char *status) { exit(status == NULL || *status == 0 ? 0 : 1); }
EOF
        cat > "$plan9_out/driver.c" <<'EOF'
#include <u.h>
#include <libc.h>
#include "greet.h"
int main(void) { return Greet(); }
EOF
        "${CC:-cc}" -std=c11 -I"$work/plan9-include" -I"$plan9_out" \
            "$plan9_out/greet.c" "$plan9_out/driver.c" "$work/plan9-shim.c" \
            -lm -o "$plan9_out/program"
        "$plan9_out/program" > "$plan9_out.out"
    fi
    cmp "$work/expected" "$plan9_out.out"
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

# A whole Plan 9 program: the generated main wrapper must survive the print
# helpers' guard, and its status becomes the exit status.
if test -n "$plan9" && test -x "$plan9/bin/9c"; then
    mkdir -p "$work/plan9-program"
    cat > "$work/plan9-program/hello.zi" <<'EOF'
#program_export
main :: () -> s32 {
    print("Hello, %!\n", "Plan 9");
    return 0;
}
EOF
    "$ziran" build --target=plan9-c --root "$work/plan9-program" \
        -o "$work/plan9-program/out" "$work/plan9-program/hello.zi"
    (cd "$work/plan9-program/out" &&
        PLAN9=$plan9 "$plan9/bin/9c" -I. hello.c 2> 9c.log &&
        PLAN9=$plan9 "$plan9/bin/9l" -o hello hello.o) ||
        { cat "$work/plan9-program/out/9c.log" >&2; exit 1; }
    test "$("$work/plan9-program/out/hello")" = "Hello, Plan 9!"
fi

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
