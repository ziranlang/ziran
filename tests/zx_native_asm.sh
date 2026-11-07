#!/bin/sh
# Procedures' #asm variants in bundles: `zi2zx run FILE.zi` builds the bundle
# from local source in memory and may run them natively, while a bundle
# file, wherever it came from, and the portable VM always take the portable
# body. The variant here wraps where its body saturates, so the result shows
# which one ran.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
zx="$(dirname -- "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export ZIRAN_NATIVE_CACHE="$work/cache"

cat > "$work/app.zi" <<'ZI'
Add :: (dst: []s16, src: []s16) #asm("x86_64+sse2", #string END
    movdqu (%[dst]), %%xmm0
    movdqu (%[src]), %%xmm1
    paddw %%xmm1, %%xmm0
    movdqu %%xmm0, (%[dst])
END, "", "r;r", "xmm0..xmm1") {
    for i: 0..7 {
        sum := cast(s32)dst[i] + src[i]
        if sum > 32767 { sum = 32767 }
        dst[i] = cast(s16)sum
    }
}

main :: () -> s64 {
    a: [8]s16
    b: [8]s16
    a[0] = 32767
    b[0] = 1
    Add(a[:], b[:])
    return a[0]
}
ZI

"$zx" bundle --root "$work" --entry app:main -o "$work/app.zx" "$work/app.zi"
grep -aq 'paddw' "$work/app.zx"
# The bundle keeps the variant, but no bundle file runs it, not even after
# local source compiled the same program with it.
test "$("$zx" run --vm "$work/app.zx")" = 32767
test "$("$zx" run "$work/app.zx")" = 32767
local=32767
case $(uname -m) in x86_64) local=-32768 ;; esac
test "$("$zx" run "$work/app.zi")" = "$local"
test "$("$zx" run "$work/app.zx")" = 32767
test "$("$zx" run --vm "$work/app.zi")" = 32767
test "$("$ziran" run "$work/app.zi")" = "$local"
# The same bytes from elsewhere are no different.
cp "$work/app.zx" "$work/downloaded.zx"
test "$("$zx" run "$work/downloaded.zx")" = 32767
echo "Bundles ran their portable bodies; local source ran its #asm variant"
