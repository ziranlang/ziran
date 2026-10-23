#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "update"
#import "text_buffer"

#program_export
Check :: () -> s32 {
    if CompareVersions("1.9.4", "1.10.0") != -1 ||
        CompareVersions("v1.10.0", "1.9.4") != 1 ||
        CompareVersions("1.9", "1.9.0") != 0 ||
        CompareVersions("1.9.4-rc1", "1.9.4") != 0 ||
        CompareVersions("", "0.0.1") != -1 ||
        CompareVersions("bad", "1.0.0") != 0 {
        return 1
    }
    json := "{\"version\":\"9.9.9\",\"notes_url\":\"https://example.test/release\",\"channels\":{\"appimage-amd64\":{\"url\":\"https://example.test/app.AppImage\",\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"size\":123456}}}"
    appcast := ParseAppcast(json, "appimage-amd64")
    if !appcast.valid || !appcast.artifact.available ||
        TextUntilNul(appcast.version[:]) != "9.9.9" ||
        TextUntilNul(appcast.notes_url[:]) != "https://example.test/release" ||
        TextUntilNul(appcast.artifact.url[:]) != "https://example.test/app.AppImage" ||
        appcast.artifact.size != 123456 {
        return 2
    }
    package := ParseAppcast(json, "deb-amd64")
    if !package.valid || package.artifact.available {
        return 3
    }
    bad_digest := ParseAppcast("{\"version\":\"2\",\"channels\":{\"appimage-amd64\":{\"url\":\"https://example.test/a\",\"sha256\":\"oops\"}}}", "appimage-amd64")
    if bad_digest.valid {
        return 4
    }
    malformed := ParseAppcast("{\"version\":\"2\"} trailing", "")
    if malformed.valid {
        return 5
    }
    bad_version := ParseAppcast("{\"version\":\"v1..2\"}", "")
    if bad_version.valid {
        return 6
    }
    trailing_version := ParseAppcast("{\"version\":\"1.2.3.4oops\"}", "")
    if trailing_version.valid {
        return 8
    }
    overflow_size := ParseAppcast("{\"version\":\"2\",\"channels\":{\"appimage-amd64\":{\"url\":\"https://example.test/a\",\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"size\":9223372036854775808}}}", "appimage-amd64")
    if overflow_size.valid {
        return 7
    }
    return 0
}
ZI

"$ziran" bundle --root "$work" --module-path "$repo/std" \
    --entry app:Check -o "$work/app.zx" "$work/app.zi"
test "$("$ziran" run "$work/app.zx")" = 0

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$repo/std" \
    --entry app:Check -o "$work/saved.zx" "$work/ir/app.zir"
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --no-main --root "$work" \
    --module-path "$repo/std" -o "$work/c" "$work/app.zi"
cat > "$work/c/main.c" <<'C'
#include "app.h"
int main(void) { return Check(); }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/c/app"
"$work/c/app"
