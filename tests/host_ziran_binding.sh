#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
host_api :: #system_library "host_api";
Effect :: (value: s32) -> s32 #foreign host_api;
#program_export
main :: () -> s32 { return Effect(41) }
ZI
cat > "$work/provider.zi" <<'ZI'
#program_export
Produce :: (value: s32) -> s32 { return value + 1 }
ZI
cat > "$work/bad_provider.zi" <<'ZI'
#program_export
Produce :: (value: bool) -> s32 { return 1 }
ZI

"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/app.zi" "$work/provider.zi"
"$ziran" bundle --root "$work" --entry app:main \
    --bind app:Effect=provider:Produce -o "$work/source.zib" \
    "$work/app.zi" "$work/provider.zi"
"$ziran" bundle --root "$work/ir" --entry app:main \
    --bind app:Effect=provider:Produce -o "$work/saved.zib" \
    "$work/ir/app.zir" "$work/ir/provider.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        root=$work
        app=$work/app.zi
        provider=$work/provider.zi
    else
        root=$work/ir
        app=$work/ir/app.zir
        provider=$work/ir/provider.zir
    fi
    "$ziran" build --target=go --pkg main --exe --root "$root" \
        --entry app:main --bind app:Effect=provider:Produce \
        -o "$output" "$app" "$provider"
    GO111MODULE=off go build -o "$output/app" "$output"/*.go
    status=0
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/app" || status=$?
    test "$status" = 42
done

# A native C build calls the bound export; another module's capability of
# the same name still resolves to the export with that name.
mkdir -p "$work/native"
cat > "$work/native/app.zi" <<'ZI'
host_api :: #system_library "host_api";
Effect :: (value: s32) -> s32 #foreign host_api;
Main :: () -> s32 { return Effect(41) }
ZI
cat > "$work/native/other.zi" <<'ZI'
host_api :: #system_library "host_api";
Effect :: (value: s32) -> s32 #foreign host_api;
Other :: () -> s32 { return Effect(1) }
ZI
# A capability also named by a function of a module it imports calls that
# function on every target, and its bound export is still kept and checked.
cat > "$work/native/shadow.zi" <<'ZI'
Effect :: (value: s32) -> s32 { return value * 20 }
ZI
cat > "$work/native/third.zi" <<'ZI'
#import "shadow"
host_api :: #system_library "host_api";
Effect :: (value: s32) -> s32 #foreign host_api;
Third :: () -> s32 { return Effect(3) }
ZI
cat > "$work/native/both.zi" <<'ZI'
#import "app"
#import "other"
#import "third"
#import "provider"
#program_export
Effect :: (value: s32) -> s32 { return value * 10 }
#program_export
both :: () -> s32 { return Main() + Other() + Third() }
ZI
cp "$work/provider.zi" "$work/native/"
cat >> "$work/native/provider.zi" <<'ZI'
#program_export
ProduceThird :: (value: s32) -> s32 { return value + 2 }
ZI
for input in source saved; do
    output=$work/c-$input
    if test "$input" = source; then
        "$ziran" build --target=c --exe --root "$work/native" --entry both:both \
            --bind app:Effect=provider:Produce --bind third:Effect=provider:ProduceThird \
            -o "$output" "$work/native/both.zi"
    else
        # The binding is kept in IR saved for the entry.
        "$ziran" ir --root "$work/native" --entry both:both --bind app:Effect=provider:Produce \
            --bind third:Effect=provider:ProduceThird -o "$work/native-ir" "$work/native/both.zi"
        # shellcheck disable=SC2046
        "$ziran" build --target=c --exe --root "$work/native-ir" --entry both:both \
            -o "$output" $(ls "$work"/native-ir/*.zir)
    fi
    status=0
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/both" || status=$?
    # app's Effect(41) is Produce: 42; other's Effect(1) is the export: 10;
    # third's Effect(3) is shadow's function: 60.
    test "$status" = 112
done

# A bundle keeps the bound export too; the other capability goes to the
# host module's export of that name.
"$ziran" bundle --root "$work/native" --entry both:both \
    --bind app:Effect=provider:Produce --bind third:Effect=provider:ProduceThird \
    --bind-host both -o "$work/native.zib" "$work/native/both.zi"
test "$("$ziran" run "$work/native.zib")" = 112

"$ziran" bundle --root "$work" --entry app:main \
    -o "$work/unbound.zib" "$work/app.zi"
if "$ziran" run "$work/unbound.zib" > "$work/unbound.out" 2> "$work/unbound.err"; then
    echo 'unbound capability unexpectedly ran' >&2
    exit 1
fi
rg -q 'missing host capability: app:Effect' "$work/unbound.err"

if "$ziran" bundle --root "$work" --entry app:main \
    --bind app:Effect=bad_provider:Produce -o "$work/bad.zib" \
    "$work/app.zi" "$work/bad_provider.zi" > "$work/bad.out" 2> "$work/bad.err"; then
    echo 'mismatched provider unexpectedly linked' >&2
    exit 1
fi
rg -q 'mismatched signature' "$work/bad.err"
