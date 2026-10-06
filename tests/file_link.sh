#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
input="$root/tests/spec/file_link_test.zi"
"$ziran" ir --root "$root/tests/spec" -o "$work/ir" "$input"
for form in source saved; do
    source="$input"; module_root="$root/tests/spec"
    if test "$form" = saved; then source="$work/ir/file_link_test.zir"; module_root="$work/ir"; fi
    for target in c cpp; do
        output="$work/$form-$target"
        "$ziran" build --target="$target" --root "$module_root" --entry file_link_test:main -o "$output" "$source"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$root/include" "$output"/*.c -o "$output/test"
        else
            "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$root/include" "$output"/*.cpp -o "$output/test"
        fi
        run="$work/run-$form-$target"; mkdir "$run" "$run/directory"
        : > "$run/ordinary"
        ln -s ordinary "$run/link"
        ln -s missing "$run/dangling"
        ln -s directory "$run/link-directory"
        ln -s ordinary "$run/lien café 日本語"
        (cd "$run" && "$output/test")
    done
done
"$ziran" build --target=c --define ANDROID_BUILD --define __arm__ --root "$root/tests/spec" -o "$work/android" "$input"
"$ziran" build --target=c --define PLATFORM_WEB --root "$root/tests/spec" -o "$work/web" "$input"
"$ziran" build --target=c --define _WIN32 --root "$root/tests/spec" -o "$work/windows" "$input"
if rg '__asm__\("readlink"\)' "$work/windows"; then exit 1; fi
"$ziran" build --target=plan9-c --root "$root/tests/spec" -o "$work/plan9" "$input"
test -f "$work/plan9/file_plan9.c"
test ! -f "$work/plan9/file_linux.c"
printf 'Fresh link queries passed native C/C++ source and saved IR; other-platform generation checked.\n'
