#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

"$ziran" check --root "$repo/tests/spec" --module-path "$repo/std" \
    "$repo/tests/spec/date_parse_test.zi"
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" \
    -o "$work/ir" "$repo/tests/spec/date_parse_test.zi"
"$ziran" check --root "$work/ir" "$work/ir/date_parse_test.zir"
for input in source saved; do
    root=$repo/tests/spec
    file=$root/date_parse_test.zi
    if test "$input" = saved; then root=$work/ir; file=$root/date_parse_test.zir; fi
    for target in c cpp go; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --no-main --entry date_parse_test:Check \
            --root "$root" --module-path "$repo/std" -o "$out" "$file"
        case $target in
        c)
            printf '#include "date_parse_test.h"\nint main(void) { return Check(); }\n' > "$out/entry.c"
            cc -std=c11 -I"$out" "$out"/*.c -lm -o "$work/program"
            "$work/program"
            ;;
        cpp)
            printf '#include "date_parse_test.hpp"\nint main() { return Check(); }\n' > "$out/entry.cpp"
            c++ -std=c++17 -I"$out" "$out"/*.cpp -lm -o "$work/program"
            "$work/program"
            ;;
        go)
            printf 'package ziran\nimport "testing"\nfunc TestDateParse(t *testing.T) { if DateParseTest_Check() != 0 { t.Fatal("date parse") } }\n' > "$out/main_test.go"
            GO111MODULE=off go test "$out"/*.go
            ;;
        esac
    done
    "$ziran" bundle --root "$root" --module-path "$repo/std" \
        --entry date_parse_test:Check -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 0
done
