#!/bin/sh
# Native filesystem metadata, canonical paths and private temporary children.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/std-file-tree-linux.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/file_tree_test.zi" <<'ZI'
#import "file_tree_linux"
#import "text_tools"
libc :: #system_library "libc";
User :: () -> u32 #foreign libc "geteuid";
Mask :: (mode: u32) -> u32 #foreign libc "umask";
ReportForeignUser :: (enabled: s32) #foreign libc "fixture_foreign_user";

#program_export
main :: () -> s32 {
    uid := User()
    parent := ResolvePath("parent")
    if parent.count == 0 || parent[0] != #char "/" { return 1 }
    info := PathStatus("parent")
    if info.kind != .Directory || info.owner_uid != uid || info.mode != 448 { return 2 }
    file := PathStatus("parent/note")
    if file.kind != .File || file.owner_uid != uid || file.size != 5 { return 3 }
    link := PathStatus("parent/link", false)
    if link.kind != .Link || link.owner_uid != uid || PathStatus("parent/link").kind != .Directory { return 4 }
    if ResolvePath("alias/link/../note") != JoinPath(parent, "note") ||
        ResolvePath("parent/target/./../target") != JoinPath(parent, "target") { return 5 }
    if ResolvePath("parent/absent") != "" || ResolvePath("parent/broken") != "" ||
        ResolvePath("parent/loop") != "" || ResolvePath("parent\0bad") != "" || ResolvePath("") != "" { return 6 }
    if PathStatus("parent/absent").kind != .Missing { return 7 }
    first := CreateTemporaryDirectory("alias/target/..", "test space-")
    second := CreateTemporaryDirectory("parent", "test space-")
    third := CreateTemporaryDirectory("parent")
    if first == "" || second == "" || third == "" || first == second { return 8 }
    if PathParent(first) != parent || !TextHasPrefix(PathName(first), "test space-") ||
        PathName(first).count != 17 || !TextHasPrefix(PathName(third), "ziran-") { return 9 }
    created := PathStatus(first, false)
    if created.kind != .Directory || created.owner_uid != uid || created.mode != 448 || ResolvePath(first) != first { return 10 }
    if !CreateDirectories(JoinPath(first, "nested")) || !RemoveTree(first) || !RemoveTree(second) || !RemoveTree(third) { return 11 }
    if CreateTemporaryDirectory("parent/absent") != "" || CreateTemporaryDirectory("parent/note") != "" ||
        CreateTemporaryDirectory("parent", "../escape") != "" || CreateTemporaryDirectory("parent", "a/b") != "" ||
        CreateTemporaryDirectory("parent", "bad\0name") != "" || CreateTemporaryDirectory("parent\0bad") != "" { return 12 }
    // Change only the UID reported to the library: the kernel would still
    // permit this creation, proving rejection comes from the owner check.
    ReportForeignUser(1)
    rejected := CreateTemporaryDirectory("parent")
    ReportForeignUser(0)
    if rejected != "" { return 13 }
    too_long: Vec(u8)
    for i: 0..4095 { VecPush(too_long, #char "a") }
    long_name := BuilderFinish(too_long)
    if CreateTemporaryDirectory("parent", long_name) != "" || ResolvePath(long_name) != "" { return 14 }
    for i: 0..63 {
        temporary := CreateTemporaryDirectory("parent", "repeat-")
        if temporary == "" || PathStatus(temporary).mode != 448 || !RemoveTree(temporary) { return 15 }
    }
    old_mask := Mask(511)
    restricted := CreateTemporaryDirectory("parent", "restricted-")
    unused Mask(old_mask)
    if restricted == "" || PathStatus(restricted).mode != 0 { return 16 }
    if !SetPathMode(restricted, 448) || !RemoveTree(restricted) { return 17 }
    return 0
}
ZI
cat > "$work/uid.c" <<'C'
#include <sys/types.h>
#include <unistd.h>
static int foreign_user;
void fixture_foreign_user(int enabled) { foreign_user = enabled; }
uid_t __real_geteuid(void);
uid_t __wrap_geteuid(void) {
    uid_t uid = __real_geteuid();
    return foreign_user ? uid ^ 1u : uid;
}
C
"$ziran" ir --target=c --root "$work" --module-path "$root/std" \
    -o "$work/ir" "$work/file_tree_test.zi"
for form in source saved; do
    input=$work/file_tree_test.zi; modules=$root/std; source_root=$work
    if test "$form" = saved; then
        input=$work/ir/file_tree_test.zir; modules=$work/ir; source_root=$work/ir
    fi
    output=$work/c-$form
    "$ziran" build --target=c --entry file_tree_test:main --root "$source_root" \
        --module-path "$modules" -o "$output" "$input"
    "${CC:-cc}" -std=c11 -Wall -Wextra -I"$root/include" -I"$output" \
        "$output"/*.c "$work/uid.c" -Wl,--wrap=geteuid -lm -o "$work/test-$form"
    run=$work/run-$form
    mkdir -p "$run/parent/target"
    chmod 700 "$run/parent"
    printf hello > "$run/parent/note"
    ln -s target "$run/parent/link"
    ln -s absent "$run/parent/broken"
    ln -s loop "$run/parent/loop"
    ln -s parent "$run/alias"
    (cd "$run" && umask 022 && "$work/test-$form")
    test "$(find "$run/parent" -mindepth 1 -maxdepth 1 | wc -l)" -eq 5
done
echo 'file_tree_linux: C source and saved IR metadata, canonical paths and private temporary directories passed'
