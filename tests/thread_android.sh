#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
ndk=${ZIRAN_THREAD_ANDROID_NDK:?set the Android NDK root for focused cross-links}
work=${ZIRAN_THREAD_ANDROID_BUILD:-"$(pwd)/build/thread-android"}
mkdir -p "$work"
exec 9>"$work/check.lock"
flock -n 9 || { echo 'Android thread cross-links are already running' >&2; exit 1; }
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS LD_PRELOAD
export YUE_DESKTOP_RECOVERY=0
bin="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
for definition in ANDROID ANDROID_BUILD; do
    ir="$work/$definition/ir"
    "$ziran" ir --define "$definition" --root tests/spec --module-path std \
        -o "$ir" tests/spec/thread_linux_test.zi
    for form in source saved; do
        entry=tests/spec/thread_linux_test.zi
        root=tests/spec
        modules=std
        if test "$form" = saved; then
            entry="$ir/thread_linux_test.zir"
            root="$ir"
            modules="$ir"
        fi
        for target in c cpp; do
            output="$work/$definition/$form-$target"
            "$ziran" build --define "$definition" --target="$target" --no-main \
                --root "$root" --module-path "$modules" -o "$output" "$entry"
            for triple in aarch64-linux-android24 armv7a-linux-androideabi24; do
                if test "$target" = c; then
                    "$bin/clang" --target="$triple" -std=c11 -fPIC -shared -Iinclude -I"$output" \
                        tests/thread_completion_test.c "$output"/*.c -pthread \
                        -Wl,--wrap=pthread_join -Wl,--wrap=pthread_mutex_lock -Wl,--no-undefined \
                        -o "$output/$triple.so"
                else
                    "$bin/clang++" --target="$triple" -x c++ -std=c++17 -fPIC -shared -Iinclude -I"$output" \
                        tests/thread_completion_test.c "$output"/*.cpp -pthread \
                        -Wl,--wrap=pthread_join -Wl,--wrap=pthread_mutex_lock -Wl,--no-undefined \
                        -o "$output/$triple.so"
                fi
                echo "PASS: $definition $form $target $triple thread cross-link"
            done
        done
    done
done
