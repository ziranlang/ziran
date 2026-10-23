#!/bin/sh
set -eu

tool_dir=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std \
    tests/spec/mapped_file_linux_test.zi
"$tool_dir/zi2c" --no-main --root tests/spec --module-path std \
    -o "$work/c" tests/spec/mapped_file_linux_test.zi
"${CC:-cc}" -std=c11 -Iinclude -I"$work/c" \
    "$work/c/byte_text_linux.c" "$work/c/c_string.c" \
    "$work/c/file_linux.c" "$work/c/mapped_file_linux.c" \
    "$work/c/mapped_file_linux_test.c" -x c - -o "$work/test" <<'EOF'
#define _GNU_SOURCE
#include "mapped_file_linux_test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
extern long write(int, const void *, unsigned long);
extern int close(int);
extern int unlink(const char *);
int main(int argc, char **argv) {
    if(argc != 2) return 9;
    char *path = argv[1];
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if(fd < 0) return 10;
    if(write(fd, "hello", 5) != 5 || close(fd) != 0) return 11;
    int status = SelfTest((uint8_t *)path);
    if(unlink(path) != 0) return 12;
    return status;
}
EOF
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test" "$work/map"
