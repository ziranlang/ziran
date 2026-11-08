#!/bin/sh
# Native no-replace copies preserve occupied entries and symbolic links.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/file-copy-new.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
input=$root/tests/spec/file_copy_new_test.zi
"$ziran" ir --target=c --root "$root/tests/spec" -o "$work/ir" "$input"
cat > "$work/read-failure.c" <<'C'
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
ssize_t __real_pread(int, void *, size_t, off_t);
ssize_t __wrap_pread(int fd, void *bytes, size_t count, off_t offset) {
    struct stat source, opened;
    if (stat("fail-read", &source) == 0 && fstat(fd, &opened) == 0 &&
        source.st_dev == opened.st_dev && source.st_ino == opened.st_ino) {
        if (offset >= 3) { errno = EIO; return -1; }
        if (count > 3) count = 3;
    }
    return __real_pread(fd, bytes, count, offset);
}
C
for form in source saved; do
    source=$input; module_root=$root/tests/spec
    if test "$form" = saved; then source=$work/ir/file_copy_new_test.zir; module_root=$work/ir; fi
    output=$work/$form
    "$ziran" build --target=c --root "$module_root" --entry file_copy_new_test:main -o "$output" "$source"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$root/include" "$output"/*.c \
        "$work/read-failure.c" -Wl,--wrap=pread -o "$output/test"
    run=$work/run-$form
    mkdir -p "$run/source/nested" "$run/occupied-dir" "$run/unsupported" "$run/merge"
    printf 'source bytes' > "$run/source/data"
    chmod 640 "$run/source/data"
    printf 'nested bytes' > "$run/source/nested/data"
    printf 'keep bytes' > "$run/occupied"
    printf 'keep child' > "$run/occupied-dir/child"
    ln -s occupied "$run/occupied-link"
    ln -s missing "$run/occupied-dangling"
    ln -s source "$run/alias"
    ln -s missing "$run/source/dangling"
    ln -s nested "$run/source/directory-link"
    ln -s data "$run/source/file-link"
    printf 'partial bytes' > "$run/unsupported/a-file"
    chmod 600 "$run/unsupported/a-file"
    printf 'private bytes' > "$run/fail-read"
    chmod 600 "$run/fail-read"
    mkfifo "$run/unsupported/z-fifo"
    printf old > "$run/replace"
    printf retained > "$run/merge/retained"
    before=$(stat -c '%i:%a:%s:%y:%z' "$run/occupied" "$run/occupied-dir" "$run/occupied-dir/child" "$run/occupied-link" "$run/occupied-dangling" "$run/source/data")
    (cd "$run" && umask 022 && "$output/test")
    test "$(stat -c %a "$run/partial")" = 700
    test "$(stat -c %a "$run/partial/a-file")" = 600
    test "$(stat -c %a "$run/partial-file")" = 600
    test "$before" = "$(stat -c '%i:%a:%s:%y:%z' "$run/occupied" "$run/occupied-dir" "$run/occupied-dir/child" "$run/occupied-link" "$run/occupied-dangling" "$run/source/data")"
    test "$(cat "$run/occupied")" = 'keep bytes'
    test "$(cat "$run/occupied-dir/child")" = 'keep child'
    test "$(readlink "$run/occupied-link")" = occupied
    test "$(readlink "$run/occupied-dangling")" = missing
    for pair in 'dangling missing' 'directory-link nested' 'file-link data'; do
        set -- $pair
        test "$(readlink "$run/tree-copy/$1")" = "$2"
    done
    test "$(readlink "$run/link-copy")" = missing
    test "$(readlink "$run/directory-link-copy")" = nested
    test "$(readlink "$run/file-link-copy")" = data
    test "$(readlink "$run/root-link-copy")" = nested
    test "$(readlink "$run/root-dangling-copy")" = missing
    test "$(cat "$run/replace")" = 'source bytes'
    test "$(cat "$run/merge/retained")" = retained
    test "$(cat "$run/merge/data")" = 'source bytes'
    printf 'Filesystem copies: %s native C exclusive copies, preserved links/occupied entries, descendant rejection and partial failure passed\n' "$form"
done
