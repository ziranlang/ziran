#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/foreign_keywords.zi" <<'ZI'
host :: #system_library "host";
Sum :: (switch: s32, class: s32, new: s32, ziran_keyword_new_0: s32) -> s32 #foreign host "sum_keywords";
#program_export
Answer :: () -> s32 { return Sum(1, 2, 3, 36) }
ZI
"$ziran" check --root "$work" "$work/foreign_keywords.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/foreign_keywords.zi"
for form in source saved; do
    input="$work/foreign_keywords.zi"; if test "$form" = saved; then input="$work/ir/foreign_keywords.zir"; fi
    for target in c cpp; do
        output="$work/$form-$target"
        "$ziran" build --target="$target" --root "$work" -o "$output" "$input"
        if test "$target" = c; then
            cat > "$output/host.c" <<'C'
#include "foreign_keywords.h"
int32_t sum_keywords(int32_t a, int32_t b, int32_t c, int32_t d) { return a+b+c+d; }
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            cc -std=c11 -pedantic-errors -Iinclude -I"$output" "$output"/*.c -o "$output/test"
        else
            cat > "$output/host.cpp" <<'CPP'
#include "foreign_keywords.hpp"
extern "C" int32_t sum_keywords(int32_t a, int32_t b, int32_t c, int32_t d) { return a+b+c+d; }
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            c++ -std=c++17 -pedantic-errors -Iinclude -I"$output" "$output"/*.cpp -o "$output/test"
        fi
        "$output/test"
    done
done
echo 'Foreign keyword parameters and escaped-name collisions pass C/C++ from source and saved IR'
