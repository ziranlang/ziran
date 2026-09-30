#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/host_types.zi" <<'ZI'
#import "date_time"
Reading :: () -> LocalDateTime { return LocalNow() }
ZI
"$ziran" check --root "$work" --module-path "$repo/std" "$work/host_types.zi"
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/host-ir" "$work/host_types.zi"
"$ziran" check --root "$work/host-ir" "$work/host-ir/host_types.zir"
"$ziran" ir --root "$repo/tests/spec" --module-path "$repo/std" -o "$work/ir" "$repo/tests/spec/calendar_test.zi"
for input in source saved; do
    root=$repo/tests/spec
    file=$root/calendar_test.zi
    if test "$input" = saved; then root=$work/ir; file=$root/calendar_test.zir; fi
    for target in c cpp go; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --no-main --entry calendar_test:Check --root "$root" --module-path "$repo/std" -o "$out" "$file"
        case $target in
        c)
            printf '#include "calendar_test.h"\nint main(void) { return Check(); }\n' > "$out/entry.c"
            cc -std=c11 -I"$out" "$out"/*.c -lm -o "$work/program"
            "$work/program"
            ;;
        cpp)
            printf '#include "calendar_test.hpp"\nint main() { return Check(); }\n' > "$out/entry.cpp"
            c++ -std=c++17 -I"$out" "$out"/*.cpp -lm -o "$work/program"
            "$work/program"
            ;;
        go)
            printf 'package ziran\nimport "testing"\nfunc TestCalendar(t *testing.T) { if CalendarTest_Check() != 0 { t.Fatal("calendar") } }\n' > "$out/main_test.go"
            GO111MODULE=off go test "$out"/*.go
            ;;
        esac
    done
    "$ziran" bundle --root "$root" --module-path "$repo/std" --entry calendar_test:Check -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 0
done
