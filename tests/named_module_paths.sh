#!/bin/sh
set -eu

# --module-path NAME=DIR serves NAME/Module imports without a project, so a
# package's sources build the same way from a plain module-path build.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/app" "$work/lib/src/Greeting"

cat > "$work/lib/src/greet.zi" <<'ZI'
Greet :: () -> s32 { return 40 }
ZI
cat > "$work/lib/src/Greeting/module.zi" <<'ZI'
using Base :: #import "greet";
Extra :: () -> s32 { return 2 }
ZI
cat > "$work/app/main.zi" <<'ZI'
#import "lib/greet"
#import "lib/Greeting"
#program_export
Answer :: () -> s32 { return Greet() + Extra() }
ZI

"$ziran" check --root "$work/app" --module-path "lib=$work/lib/src" "$work/app/main.zi"
"$ziran" bundle --root "$work/app" --module-path "lib=$work/lib/src" \
    --entry main:Answer -o "$work/app.zx" "$work/app/main.zi"
test "$("$ziran" run "$work/app.zx")" = 42

# A directory module's short imports resolve in its own package even when
# nothing else loaded them first.
cat > "$work/app/directory.zi" <<'ZI'
#import "lib/Greeting"
#program_export
Answer :: () -> s32 { return Greet() + Extra() }
ZI
"$ziran" bundle --root "$work/app" --module-path "lib=$work/lib/src" \
    --entry directory:Answer -o "$work/directory.zx" "$work/app/directory.zi"
test "$("$ziran" run "$work/directory.zx")" = 42

# A named package keeps its modules apart from the app's: both may have a
# module named tree, and the package's own short import finds its own.
mkdir -p "$work/lib/src/Branch"
cat > "$work/lib/src/tree.zi" <<'ZI'
Depth :: () -> s32 { return 2 }
ZI
cat > "$work/lib/src/Branch/module.zi" <<'ZI'
using LibTree :: #import "tree";
Leaf :: () -> s32 { return Depth() * 10 }
ZI
cat > "$work/app/tree.zi" <<'ZI'
Size :: () -> s32 { return 2 }
ZI
cat > "$work/app/forest.zi" <<'ZI'
#import "tree"
#import "lib/Branch"
#program_export
Answer :: () -> s32 { return Size() + Leaf() + 20 }
ZI
"$ziran" bundle --root "$work/app" --module-path "lib=$work/lib/src" \
    --entry forest:Answer -o "$work/forest.zx" "$work/app/forest.zi"
test "$("$ziran" run "$work/forest.zx")" = 42
"$ziran" build --target=c --no-main --root "$work/app" --module-path "lib=$work/lib/src" \
    -o "$work/forest-c" "$work/app/forest.zi"
${CC:-cc} -std=c11 -fsyntax-only -I"$(dirname -- "$(command -v "$ziran")")/../../include" \
    -I"$work/forest-c" "$work/forest-c"/*.c

# A named path serves only its qualified imports.
cat > "$work/app/short.zi" <<'ZI'
#import "greet"
ZI
if "$ziran" check --root "$work/app" --module-path "lib=$work/lib/src" \
    "$work/app/short.zi" 2>/dev/null; then
    echo "a named module path served a short import" >&2
    exit 1
fi

# Without the name the diagnostic says how to provide it.
message=$("$ziran" check --root "$work/app" "$work/app/main.zi" 2>&1 || true)
case $message in
    *"--module-path lib=DIR"*) ;;
    *) echo "unexpected diagnostic: $message" >&2; exit 1 ;;
esac

# std/NAME is the standard library's NAME without a project, even when a
# shared search path has its own image.zi or a named package imports it
# beside its own image module. The plain import keeps the library's.
standard=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/std
mkdir -p "$work/widgets" "$work/codec/src"
cat > "$work/widgets/image.zi" <<'ZI'
Image :: (size: s32) -> s32 { return size }
ZI
cat > "$work/codec/src/image.zi" <<'ZI'
using Pixels :: #import "std/image"
Decoded :: struct { box: Pixels.ImageColorBox }
Decode :: (count: s64) -> Decoded {
    result: Decoded
    result.box.count = count
    return result
}
ZI
cat > "$work/codec/src/api.zi" <<'ZI'
using Images :: #import "image"
Width :: (count: s64) -> s32 { return cast(s32)Images.Decode(count).box.count }
ZI
cat > "$work/app/pictures.zi" <<'ZI'
using Widgets :: #import "image"
using Codec :: #import "codec/api"
using Pixels :: #import "std/image"
#program_export
Answer :: () -> s32 {
    box: Pixels.ImageColorBox
    box.count = 2
    return Widgets.Image(30) + Codec.Width(10) + cast(s32)box.count
}
ZI
cat > "$work/app/pictures_first.zi" <<'ZI'
using Pixels :: #import "std/image"
using Widgets :: #import "image"
#program_export
Answer :: () -> s32 {
    box: Pixels.ImageColorBox
    box.start = 2
    return Widgets.Image(40) + cast(s32)box.start
}
ZI
for paths in "" "--module-path $standard"; do
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$work/app" --module-path "$work/widgets" $paths \
        --module-path "codec=$work/codec/src" --entry pictures:Answer \
        -o "$work/pictures.zx" "$work/app/pictures.zi"
    test "$("$ziran" run "$work/pictures.zx")" = 42
    # shellcheck disable=SC2086
    "$ziran" bundle --root "$work/app" --module-path "$work/widgets" $paths \
        --entry pictures_first:Answer -o "$work/pictures-first.zx" \
        "$work/app/pictures_first.zi"
    test "$("$ziran" run "$work/pictures-first.zx")" = 42
done
