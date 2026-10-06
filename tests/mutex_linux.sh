#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran" check --root tests/spec --module-path std tests/spec/mutex_linux_test.zi
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/mutex_linux_test.zi
for form in source saved; do
    input=tests/spec/mutex_linux_test.zi
    if test "$form" = saved; then input="$work/ir/mutex_linux_test.zir"; fi
    "$ziran" build --target=c --exe --entry mutex_linux_test:Main --root tests/spec \
        --module-path std -o "$work/$form-c" "$input"
    "$work/$form-c/mutex_linux_test"
    "$ziran" build --target=cpp --no-main --root tests/spec --module-path std \
        -o "$work/$form-cpp" "$input"
    printf '#include "mutex_linux_test.hpp"\nint main() { return Main(); }\n' > "$work/$form-cpp/main.cpp"
    c++ -std=c++17 -Iinclude -I"$work/$form-cpp" "$work/$form-cpp"/*.cpp -pthread -o "$work/$form-cpp/test"
    "$work/$form-cpp/test"
done
echo 'Mutex lifecycle and eight concurrent writers pass C/C++ from source and saved IR'
