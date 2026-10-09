#!/bin/sh
set -eu

work=${ZIRAN_THREAD_TEST_BUILD:-"$(pwd)/build/thread-linux"}
mkdir -p "$work"
exec 9>"$work/check.lock"
flock -n 9 || { echo 'Thread checks are already running' >&2; exit 1; }

"$1" check --root tests/spec --module-path std \
    tests/spec/thread_linux_test.zi
"$1" ir --root tests/spec --module-path std -o "$work/ir" \
    tests/spec/thread_linux_test.zi
for form in source saved; do
for target in c cpp; do
output="$work/$form-$target"
entry=tests/spec/thread_linux_test.zi
root=tests/spec
modules=std
if test "$form" = saved; then
    entry="$work/ir/thread_linux_test.zir"
    root="$work/ir"
    modules="$work/ir"
fi
"$1" build --target="$target" --no-main \
    --root "$root" --module-path "$modules" -o "$output" "$entry"
if test "$target" = c; then
"${CC:-cc}" -std=c11 -Iinclude -I"$output" tests/thread_completion_test.c \
    "$output"/*.c -pthread -Wl,--wrap=pthread_join -Wl,--wrap=pthread_mutex_lock -o "$work/test"
else
"${CXX:-c++}" -std=c++17 -Iinclude -I"$output" tests/thread_completion_test.c \
    "$output"/*.cpp -pthread -Wl,--wrap=pthread_join -Wl,--wrap=pthread_mutex_lock -o "$work/test"
fi
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS "$work/test"
done
done
