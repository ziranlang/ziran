#!/bin/sh
# Exclusive public creation must never modify an occupied entry or link target.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/file-new.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
input="$root/tests/spec/file_new_test.zi"
"$ziran" ir --target=c --root "$root/tests/spec" -o "$work/ir" "$input"
for form in source saved; do
    source=$input; module_root=$root/tests/spec
    if test "$form" = saved; then source=$work/ir/file_new_test.zir; module_root=$work/ir; fi
    output=$work/$form
    "$ziran" build --target=c --root "$module_root" --entry file_new_test:main -o "$output" "$source"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$root/include" "$output"/*.c -o "$output/test"
    run=$work/run-$form
    mkdir "$run"
    printf 'keep these bytes' > "$run/occupied"
    ln -s occupied "$run/link"
    ln -s missing "$run/dangling"
    # Verify entry identity, mode, size and modification/change times too.
    before=$(stat -c '%i:%a:%s:%y:%z' "$run/occupied" "$run/link" "$run/dangling")
    (cd "$run" && umask 022 && "$output/test")
    test "$(cat "$run/created")" = 'new bytes'
    test "$(stat -c %a "$run/created")" = 644
    test "$(cat "$run/occupied")" = 'keep these bytes'
    test "$(readlink "$run/link")" = occupied
    test "$(readlink "$run/dangling")" = missing
    test ! -e "$run/missing"
    test "$before" = "$(stat -c '%i:%a:%s:%y:%z' "$run/occupied" "$run/link" "$run/dangling")"
    printf 'OpenNew: %s native C creation, occupied file/link preservation and invalid paths passed\n' "$form"
done
