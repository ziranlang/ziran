#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/core.zi" <<'ZI'
Clock :: struct {
    count: s32
}
ZI
cat > "$work/api.zi" <<'ZI'
native :: #import "core";
Wrapper :: struct {
    value: native.Clock
}
Value :: () -> Wrapper {
    result: Wrapper
    result.value = native.Clock.{count = 42}
    return result
}
Count :: (value: native.Clock) -> s32 {
    return value.count
}
ZI
cat > "$work/main.zi" <<'ZI'
#import "api"
#program_export
Answer :: () -> s32 {
    wrapped := Value()
    return Count(wrapped.value)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in source saved; do
    root=$work
    file=$root/main.zi
    if test "$input" = saved; then root=$work/ir; file=$root/main.zir; fi
    for target in c cpp go; do
        out=$work/$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --no-main --pkg main --root "$root" --entry main:Answer -o "$out" "$file"
        else
            "$ziran" build --target="$target" --no-main --root "$root" --entry main:Answer -o "$out" "$file"
        fi
        case $target in
        c)
            printf '#include "main.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$out/entry.c"
            cc -std=c11 -Iinclude -I"$out" "$out"/*.c -lm -o "$work/program"
            ;;
        cpp)
            printf '#include "main.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$out/entry.cpp"
            c++ -std=c++17 -Iinclude -I"$out" "$out"/*.cpp -lm -o "$work/program"
            ;;
        go)
            printf 'package main\nfunc main() { if Main_Answer() != 42 { panic("imported field type") } }\n' > "$out/entry.go"
            GO111MODULE=off go build -o "$work/program" "$out"/*.go
            ;;
        esac
        "$work/program"
    done
    "$ziran" bundle --root "$root" --entry main:Answer -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
echo 'Imported record fields and procedure parameters retain qualified type identity: passed'
