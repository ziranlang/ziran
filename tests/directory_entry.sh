#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/directory-entry.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
input=$repo/tests/spec/directory_entry_test.zi
cat > "$work/fixture.c" <<'C'
#define _LARGEFILE64_SOURCE
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
static int fail_next, unknown_next;
void directory_test_fail_next(void) { fail_next = 1; }
void directory_test_unknown_next(void) { unknown_next = 1; }
void directory_test_dirty_errno(void) { errno = EIO; }
#ifdef __EMSCRIPTEN__
struct dirent *__real_readdir(DIR *);
struct dirent *__wrap_readdir(DIR *directory) {
#else
struct dirent64 *__real_readdir64(DIR *);
struct dirent64 *__wrap_readdir64(DIR *directory) {
#endif
    if (fail_next) { fail_next = 0; errno = EIO; return 0; }
#ifdef __EMSCRIPTEN__
    struct dirent *entry = __real_readdir(directory);
#else
    struct dirent64 *entry = __real_readdir64(directory);
#endif
    if (entry && unknown_next) { unknown_next = 0; entry->d_type = DT_UNKNOWN; }
    return entry;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -c "$work/fixture.c" -o "$work/fixture.o"
"$ziran" ir --root "$repo/tests/spec" -o "$work/ir" "$input"
for form in source saved; do
    source=$input; module_root=$repo/tests/spec
    if test "$form" = saved; then source=$work/ir/directory_entry_test.zir; module_root=$work/ir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --root "$module_root" --entry directory_entry_test:main -o "$output" "$source"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$repo/include" "$output"/*.c "$work/fixture.o" -Wl,--wrap=readdir64 -o "$output/test"
        else
            "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$repo/include" "$output"/*.cpp "$work/fixture.o" -Wl,--wrap=readdir64 -o "$output/test"
        fi
        run=$work/run-$form-$target; mkdir -p "$run/entries/directory"
        : > "$run/entries/ordinary"; : > "$run/entries/café 日本語.json"
        ln -s ordinary "$run/entries/link"; ln -s missing "$run/entries/dangling"
        mkfifo "$run/entries/fifo"
        (cd "$run" && "$output/test")
        printf '%s %s directory entry checks passed\n' "$form" "$target"
    done
done
"$ziran" build --target=c --define _WIN32 --root "$repo/tests/spec" -o "$work/windows" "$input"
if rg '__asm__\("__errno(_location)?"\)' "$work/windows"; then exit 1; fi
"$ziran" build --target=c --define PLATFORM_WEB --root "$repo/tests/spec" -o "$work/web" "$input"
emcc=${EMCC:-emcc}
if command -v "$emcc" >/dev/null 2>&1; then
    "$ziran" ir --define PLATFORM_WEB --root "$repo/tests/spec" -o "$work/web-ir" "$input"
    cat > "$work/web-fixture.js" <<'JS'
Module.preRun = [function () {
    FS.mkdir('/entries');
    FS.mkdir('/entries/directory');
    FS.writeFile('/entries/ordinary', '');
    FS.writeFile('/entries/café 日本語.json', '');
    FS.symlink('ordinary', '/entries/link');
    FS.symlink('missing', '/entries/dangling');
}];
JS
    for form in source saved; do
        source=$input; module_root=$repo/tests/spec
        if test "$form" = saved; then source=$work/web-ir/directory_entry_test.zir; module_root=$work/web-ir; fi
        for target in c cpp; do
            output=$work/web-$form-$target
            "$ziran" build --target="$target" --define PLATFORM_WEB --root "$module_root" \
                --entry directory_entry_test:main -o "$output" "$source"
            extension=c
            if test "$target" = cpp; then extension=cpp; fi
            "$emcc" -std=c11 -Wall -Wextra -Werror -c "$work/fixture.c" -o "$work/web-fixture.o"
            "$emcc" -I"$repo/include" "$output"/*."$extension" "$work/web-fixture.o" \
                -Wl,--wrap=readdir --pre-js "$work/web-fixture.js" -sENVIRONMENT=node \
                -sEXIT_RUNTIME=1 -o "$output/test.js"
            node "$output/test.js"
            printf '%s %s web directory entry checks passed\n' "$form" "$target"
        done
    done
else
    printf 'Emscripten unavailable; web link and runtime checks skipped.\n'
fi
android_clang=
for candidate in "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/clang; do
    if test -x "$candidate"; then android_clang=$candidate; fi
done
if test -n "$android_clang"; then
    for architecture in x86_64 aarch64 armv7a i686; do
        case "$architecture" in
            x86_64) target=x86_64-linux-android21; define=__x86_64__ ;;
            aarch64) target=aarch64-linux-android21; define=__aarch64__ ;;
            armv7a) target=armv7a-linux-androideabi21; define=__arm__ ;;
            i686) target=i686-linux-android21; define=__i386__ ;;
        esac
        output=$work/android-$architecture
        "$ziran" build --target=c --define ANDROID_BUILD --define "$define" --root "$repo/tests/spec" -o "$output" "$input"
        "$android_clang" --target="$target" -std=c11 -fPIC -shared -Wl,--no-undefined \
            -I"$repo/include" "$output"/*.c "$work/fixture.c" -Wl,--wrap=readdir64 -o "$output/test.so"
    done
    printf 'Android API 21 directory entries linked for all four NDK ABIs.\n'
fi
printf 'Windows and web directory entry generation checked.\n'
