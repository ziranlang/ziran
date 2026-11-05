#!/bin/sh
# Native filesystem metadata, canonical paths and private temporary children.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
ziran=${1:?pass ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/std-file-tree-linux.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/file_tree_test.zi" <<'ZI'
#import "file_tree_linux"
#import "text_tools"
#import "c_string"
#import "byte_text_linux"
libc :: #system_library "libc";
User :: () -> u32 #foreign libc "geteuid";
Mask :: (mode: u32) -> u32 #foreign libc "umask";
ReportForeignUser :: (enabled: s32) #foreign libc "fixture_foreign_user";
Timestamp :: (path: *u8, seconds: s64, nanoseconds: s32, no_follow: s32) -> s32 #foreign libc "fixture_timestamp";
IdentityMatches :: (path: *u8, inode: u64, major: u32, minor: u32, changed: s64, fraction: u32) -> s32 #foreign libc "fixture_identity";
Rewrite :: (path: *u8, replace: s32) -> s32 #foreign libc "fixture_rewrite";
SnapshotMask :: (mode: s32) #foreign libc "fixture_snapshot_mask";
HeapInfo :: struct { arena: usize; blocks: usize; small: usize; mappings: usize; mapped: usize; unused_high: usize; free_fast: usize; used: usize; free: usize; releasable: usize }
Heap :: () -> HeapInfo #foreign libc "mallinfo2";
Release :: (memory: *void) #foreign libc "free";
Drop :: (text: string) {
    if text.count == 0 { return }
    descriptor := cast(*TextDescriptor)*text
    Release(cast(*void)descriptor.data)
}
TemporaryCycle :: () -> bool {
    child := CreateTemporaryDirectory("parent","lifetime-")
    if child == "" { return false }
    payload := JoinPath(child,"payload")
    handle := OpenExclusivePrivate(payload)
    if !handle.valid { unused RemoveTree(child); Drop(payload); Drop(child); return false }
    closed := CloseFile(handle)
    names: Vec(string)
    listed := ListDirectory(child,*names)
    valid := listed && names.count == 1 && names[0] == "payload"
    for at: 0..names.count-1 { Drop(names[at]) }
    removed := RemoveTree(child)
    Drop(payload); Drop(child)
    return closed && valid && removed
}
TemporaryLifetime :: () -> bool {
    for at: 0..31 { if !TemporaryCycle() { return false } }
    before := Heap()
    for at: 0..1999 { if !TemporaryCycle() { return false } }
    after := Heap()
    growth := ifx after.used > before.used then after.used-before.used else 0
    print("file_tree_linux: 2000 create/list/remove cycles retain % heap bytes\n",growth)
    return growth <= 8192
}

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
    name: [64]u8
    if !CopyCString("parent/note",name[:]) || Timestamp(*name[0],1700000000,123456789,0) != 0 { return 18 }
    stamped := PathStatus("parent/note")
    if stamped.modified != 1700000000 || stamped.modified_nanoseconds != 123456789 { return 19 }
    snapshot := FileSnapshot("parent/note")
    if !snapshot.identity_valid || !SameFileSnapshot(snapshot,FileSnapshot("parent/note")) ||
        IdentityMatches(*name[0],snapshot.inode,snapshot.device_major,snapshot.device_minor,
            snapshot.changed,snapshot.changed_nanoseconds) != 1 { return 23 }
    if FileSnapshot("parent/note-link").identity_valid || FileSnapshot("parent/absent").identity_valid ||
        FileSnapshot("parent").identity_valid { return 24 }
    if Rewrite(*name[0],0) != 0 { return 25 }
    changed := FileSnapshot("parent/note")
    if !changed.identity_valid || changed.inode != snapshot.inode || changed.size != snapshot.size ||
        changed.modified != snapshot.modified || changed.modified_nanoseconds != snapshot.modified_nanoseconds ||
        SameFileSnapshot(snapshot,changed) { return 26 }
    if Rewrite(*name[0],1) != 0 { return 27 }
    replaced := FileSnapshot("parent/note")
    if !replaced.identity_valid || replaced.inode == changed.inode || SameFileSnapshot(changed,replaced) { return 28 }
    SnapshotMask(1)
    partial := FileSnapshot("parent/note")
    SnapshotMask(0)
    if partial.identity_valid || SameFileSnapshot(partial,partial) || (partial.metadata_mask & 256) != 0 { return 29 }
    SnapshotMask(2)
    invalid := FileSnapshot("parent/note")
    SnapshotMask(0)
    if invalid.identity_valid || SameFileSnapshot(invalid,invalid) { return 30 }
    if !CopyCString("parent/link",name[:]) || Timestamp(*name[0],1700000100,987654321,0) != 0 ||
        Timestamp(*name[0],1700000200,222222222,1) != 0 { return 20 }
    followed := PathStatus("parent/link")
    unfollowed := PathStatus("parent/link",false)
    if followed.kind != .Directory || followed.modified != 1700000100 || followed.modified_nanoseconds != 987654321 ||
        unfollowed.kind != .Link || unfollowed.modified != 1700000200 || unfollowed.modified_nanoseconds != 222222222 { return 21 }
    if ResolvePath("alias/link/../note") != JoinPath(parent, "note") ||
        ResolvePath("parent/target/./../target") != JoinPath(parent, "target") { return 5 }
    if ResolvePath("parent/absent") != "" || ResolvePath("parent/broken") != "" ||
        ResolvePath("parent/loop") != "" || ResolvePath("parent\0bad") != "" || ResolvePath("") != "" { return 6 }
    if PathStatus("parent/absent").kind != .Missing { return 7 }
    absent := PathStatus("parent/absent")
    if absent.modified != 0 || absent.modified_nanoseconds != 0 { return 22 }
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
    if !TemporaryLifetime() { return 31 }
    return 0
}
ZI
cat > "$work/uid.c" <<'C'
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/sysmacros.h>
static int snapshot_mask;
void fixture_snapshot_mask(int mode) { snapshot_mask = mode; }
int __real_statx(int, const char *, int, unsigned, struct statx *);
int __wrap_statx(int dir, const char *path, int flags, unsigned mask, struct statx *value) {
    int result = __real_statx(dir,path,flags,mask,value);
    if (result == 0 && snapshot_mask == 1) value->stx_mask &= ~STATX_INO;
    if (result == 0 && snapshot_mask == 2) value->stx_ctime.tv_nsec = 1000000000;
    return result;
}
int fixture_identity(const char *path, uint64_t inode, uint32_t device_major, uint32_t device_minor,
                     int64_t changed, uint32_t fraction) {
    struct stat info;
    return fstatat(AT_FDCWD,path,&info,AT_SYMLINK_NOFOLLOW) == 0 &&
        inode == info.st_ino && device_major == major(info.st_dev) && device_minor == minor(info.st_dev) &&
        changed == info.st_ctim.tv_sec && fraction == (uint32_t)info.st_ctim.tv_nsec;
}
int fixture_rewrite(const char *path, int replace) {
    struct stat old;
    if (lstat(path,&old) != 0) return -1;
    struct timespec pause = {.tv_nsec=20000000};
    nanosleep(&pause,NULL);
    char temporary[4096];
    if (snprintf(temporary,sizeof temporary,"%s.replace",path) >= (int)sizeof temporary) return -1;
    const char *output = replace ? temporary : path;
    int fd = open(output,O_WRONLY|O_CLOEXEC|(replace ? O_CREAT|O_EXCL : 0),0600);
    if (fd < 0) return -1;
    struct timespec times[2] = {{.tv_nsec=UTIME_OMIT},old.st_mtim};
    int result = write(fd,"world",5) == 5 && futimens(fd,times) == 0 && fsync(fd) == 0 ? 0 : -1;
    if (close(fd) != 0) result = -1;
    if (result == 0 && replace && rename(temporary,path) != 0) result = -1;
    return result;
}
static int foreign_user;
void fixture_foreign_user(int enabled) { foreign_user = enabled; }
uid_t __real_geteuid(void);
uid_t __wrap_geteuid(void) {
    uid_t uid = __real_geteuid();
    return foreign_user ? uid ^ 1u : uid;
}
int fixture_timestamp(const char *path, int64_t seconds, int32_t nanoseconds, int32_t no_follow) {
    struct timespec times[2] = {{.tv_nsec=UTIME_OMIT},{.tv_sec=seconds,.tv_nsec=nanoseconds}};
    return utimensat(AT_FDCWD,path,times,no_follow ? AT_SYMLINK_NOFOLLOW : 0);
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
        "$output"/*.c "$work/uid.c" -Wl,--wrap=geteuid -Wl,--wrap=statx -lm -o "$work/test-$form"
    run=$work/run-$form
    mkdir -p "$run/parent/target"
    chmod 700 "$run/parent"
    printf hello > "$run/parent/note"
    ln -s target "$run/parent/link"
    ln -s note "$run/parent/note-link"
    ln -s absent "$run/parent/broken"
    ln -s loop "$run/parent/loop"
    ln -s parent "$run/alias"
    (cd "$run" && umask 022 && "$work/test-$form")
    test "$(find "$run/parent" -mindepth 1 -maxdepth 1 | wc -l)" -eq 6
done
echo 'file_tree_linux: C source and saved IR metadata, complete no-follow snapshots, canonical paths and private temporary directories passed'
