#!/bin/sh
# A module that names Text or Vec(T) without importing std/vec gets it
# imported, so its signatures, headers and bundles see std/vec's types: C,
# saved IR, native and VM bundles and Go agree. A parameter or return type
# that names nothing visible is an error where it is written.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/src" "$work/bad"
cat > "$work/src/maker.zi" <<'ZI'
#import "std/vec"
Make :: (a: string) -> Text { return TextCopy(a) }
ZI
# No std/vec import: Text in signatures, a Vec(u8) local and BuilderFinish.
cat > "$work/src/relay.zi" <<'ZI'
#import "maker"
Len :: (s: string) -> s64 { return s.count }
Pass :: (t: Text) -> s64 { return Len(t) }
Again :: (a: string) -> Text { return Make(a) }
Join :: (a: string, b: string) -> Text {
    out: Vec(u8)
    BuilderAppend(out, a); BuilderAppend(out, b)
    return BuilderFinish(out)
}
ZI
cat > "$work/src/main.zi" <<'ZI'
#import "relay"
#import "maker"
#program_export
Main :: () -> s64 { return Pass(Make("x")) + Pass(Again("yz")) + Pass(Join("ab", "c")) * 10 }
ZI
args="--module-path $repo"
"$ziran" check --root "$work/src" $args "$work/src/main.zi"
"$ziran" ir --root "$work/src" $args -o "$work/ir" "$work/src/main.zi"
for kind in source saved; do
    root=$work/src; module=$work/src/main.zi
    if test "$kind" = saved; then root=$work/ir; module=$work/ir/main.zir; fi
    "$ziran" build --target=c --exe --entry main:Main --root "$root" $args \
        -o "$work/$kind-c" "$module"
    status=0; "$work/$kind-c/main" || status=$?
    test "$status" = 33
    "$ziran" bundle --entry main:Main --root "$root" $args -o "$work/$kind.zx" "$module"
    test "$("$(dirname -- "$ziran")/zi2zx" run "$work/$kind.zx")" = 33
    test "$("$(dirname -- "$ziran")/zi2zx" run --vm "$work/$kind.zx")" = 33
done
cmp "$work/source-c/relay.c" "$work/saved-c/relay.c"
if command -v go > /dev/null 2>&1; then
    "$ziran" build --target=go --pkg main --exe --entry main:Main --root "$work/src" $args \
        -o "$work/go" "$work/src/main.zi"
    (cd "$work/go" && GO111MODULE=off GOFLAGS= go build -o "$work/go-main" .)
    status=0; "$work/go-main" || status=$?
    test "$status" = 33
fi

cat > "$work/bad/main.zi" <<'ZI'
Pass :: (t: Txet) -> s64 { return 3 }
Back :: () -> Nope { return 1 }
#program_export
Main :: () -> s64 { return 0 }
ZI
if "$ziran" check --root "$work/bad" "$work/bad/main.zi" 2> "$work/bad.err"; then
    echo 'unknown signature types were accepted' >&2
    exit 1
fi
grep -q 'unknown parameter type: Txet' "$work/bad.err"
grep -q 'unknown return type: Nope' "$work/bad.err"
echo 'Text without a std/vec import: C, saved IR, native and VM bundles and Go agree; unknown signature types are rejected'
