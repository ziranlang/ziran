#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/path_test.zi
for mode in source saved; do
    root=tests/spec
    input=tests/spec/path_test.zi
    if test "$mode" = saved; then root=$work/ir; input=$root/path_test.zir; fi
    "$ziran" bundle --root "$root" --module-path std --entry path_test:main -o "$work/$mode.zib" "$input"
    test "$("$ziran" run "$work/$mode.zib")" = 0
    for target in c cpp go; do
        out=$work/$mode-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry path_test:main --root "$root" --module-path std -o "$out" "$input"
        else
            "$ziran" build --target="$target" --root "$root" --module-path std -o "$out" "$input"
        fi
        case "$target" in
            c) cc -std=c11 -I"$out" "$out"/*.c -o "$out/run"; "$out/run" ;;
            cpp) c++ -std=c++17 -I"$out" "$out"/*.cpp -o "$out/run"; "$out/run" ;;
            go) (cd "$out" && GO111MODULE=off go run .) ;;
        esac
    done
done
echo 'path: source, saved IR, C, C++, Go, and VM passed'
