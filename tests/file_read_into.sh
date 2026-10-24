#!/bin/sh
# ReadFileInto fills a caller-owned vector that is freed with it, so reading a
# file again and again keeps native memory flat.
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

head -c 1048576 /dev/zero | tr '\0' 'z' > "$work/big.txt"
printf 'hello\n' > "$work/small.txt"
cat > "$work/main.zi" <<'ZI'
#import "file"
using Vectors :: #import "vec"

Count :: (path: string) -> s64 {
    bytes: Vec(u8)
    if !ReadFileInto(path, *bytes) { return -1 }
    return bytes.count
}

#program_export
Small :: () -> s64 {
    bytes: Vec(u8)
    if !ReadFileInto("SMALL", *bytes) || bytes.count != 6 { return -1 }
    if TextView(VecSlice(bytes, 0, bytes.count)) != "hello\n" { return -2 }
    if ReadFileInto("SMALL.missing", *bytes) { return -3 }
    return bytes.count
}

#program_export
Repeat :: () -> s64 {
    total: s64 = 0
    for round: cast(s64)0..199 { total += Count("BIG") }
    return total
}
ZI
sed -i "s|SMALL|$work/small.txt|; s|\"SMALL.missing\"|\"$work/none.txt\"|; s|BIG|$work/big.txt|" "$work/main.zi"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/main.zi"
cat > "$work/c/entry.c" <<'C'
#include <stdio.h>
#include <sys/resource.h>
#include "main.h"
int main(void) {
    if (Small() != 6) return 1;
    if (Repeat() != 200LL * 1048576) return 2;
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    /* 200 reads of 1 MiB: kept text would need 200 MiB; owned vectors stay near one file. */
    if (usage.ru_maxrss > 64 * 1024) { fprintf(stderr, "peak %ld KiB\n", usage.ru_maxrss); return 3; }
    return 0;
}
C
"${CC:-cc}" -O1 -Iinclude -I"$work/c" "$work/c"/*.c -o "$work/c/app"
"$work/c/app"
echo "ReadFileInto: owned bytes, flat native memory over repeated reads"
