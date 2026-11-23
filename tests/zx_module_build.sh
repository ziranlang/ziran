#!/bin/sh
# check-cost: 30
set -eu
# A project builds a ZX library with `ziran bundle --library`: the manifest's
# exported modules are the inputs, --export names the roots, and the module
# options reach zi2zx. A consumer loads the image from a module root.
ziran=${1:?pass the ziran command}
case $ziran in
    /*) ;;
    */*) ziran=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)/$(basename -- "$ziran") ;;
    *) ziran=$(command -v "$ziran") ;;
esac
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
work=$build/tests/zx-module-build
project=$work/project
modules=$work/modules
rm -rf "$work"
mkdir -p "$modules/demo" "$project/src"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export XDG_CACHE_HOME="$work/cache"

# The local override keeps the fixture on this checkout, so the pinned
# toolchain is the ziran under test.
revision=$(git -C "$repo" rev-parse HEAD)
cat >"$project/ziran.toml" <<EOF
[package]
name = "geometry"
module_roots = ["src"]

[toolchain]
git = "https://git.waozi.xyz/ziranlang/ziran.git"
ref = "$revision"

[exports]
counter = "src/counter.zi"
math = "src/math.zi"
EOF
cat >"$project/ziran.lock" <<EOF
{"format":1,"packages":[],"root":{"name":"geometry","dependencies":{}},
 "toolchain":{"url":"https://git.waozi.xyz/ziranlang/ziran.git","ref":"$revision","commit":"$revision"}}
EOF
printf '[overrides]\nziran = "%s"\n' "$repo" >"$project/ziran.local.toml"

cat >"$project/src/counter.zi" <<'ZI'
Add :: (amount: s32) -> s32 { return amount + 40 }
ZI
cat >"$project/src/math.zi" <<'ZI'
host_api :: #system_library "host_api";
SubExternal :: (handle: u64, left: s32, right: s32) -> s32 #foreign host_api;
Difference :: (handle: u64, left: s32, right: s32) -> s32 { return SubExternal(handle, left, right) }
ZI
cat >"$work/calc.zi" <<'ZI'
Sub :: (left: s32, right: s32) -> s32 { return left - right }
ZI

# The standalone form and the module root a consumer searches.
"$ziran" bundle --library demo/calc --export calc:Sub \
    -o "$modules/demo/calc.zx" "$work/calc.zi"
test -s "$modules/demo/calc.zx"

# In the project the manifest's exports are the inputs and no file or entry
# is written on the command line.
(cd "$project" && "$ziran" bundle --project --library demo/counter \
    --export counter:Add -o "$modules/demo/counter.zx")
test -s "$modules/demo/counter.zx"

# --require-module and --import pass through to zi2zx unchanged.
(cd "$project" && "$ziran" bundle --project --library demo/math \
    --export math:Difference --require-module Calc.v1=demo/calc \
    --import math:SubExternal=Calc.v1:Sub -o "$modules/demo/math.zx")
linkage=$("$ziran" inspect --emit linkage "$modules/demo/math.zx")
for record in "module	demo/math" "export	Difference" \
              "require	Calc.v1	demo/calc" "import	math	SubExternal	Calc.v1	Sub"; do
    printf '%s\n' "$linkage" | grep -Fqx "$record" || {
        echo "zx-module-build: library linkage lacks: $record"
        printf '%s\n' "$linkage"
        exit 1
    }
done

# A consumer built outside the project loads the library from a module root.
cat >"$work/app.zi" <<'ZI'
using Modules :: #import "std/zx_module"
host_api :: #system_library "host_api";
AddExternal :: (handle: u64, amount: s32) -> s32 #foreign host_api;
main :: () -> s32 {
    handle := Modules.Load("Counter.v1")
    if handle == 0 { return 1 }
    result := AddExternal(handle, 2)
    if !Modules.Release(handle) { return 2 }
    return result
}
ZI
"$ziran" bundle --root "$work" --require-module Counter.v1=demo/counter \
    --import app:AddExternal=Counter.v1:Add --entry app:main \
    -o "$work/app.zx" "$work/app.zi"
result=$(cd "$work" && "$ziran" run --module-root "$modules" app.zx)
test "$result" = 42 || {
    echo "zx-module-build: consumer printed '$result'"
    exit 1
}

# A value-ABI portable std module (std/utf8) can also be built as a shared ZX
# library and loaded by a consumer from a module root.
mkdir -p "$modules/std"
"$ziran" bundle --library std/utf8 --export utf8:Valid --export utf8:Count \
    -o "$modules/std/utf8.zx" "$repo/std/utf8.zi"
test -s "$modules/std/utf8.zx"
cat >"$work/std_app.zi" <<'ZI'
using Modules :: #import "std/zx_module"
host_api :: #system_library "host_api";
Utf8Valid :: (handle: u64, text: string) -> bool #foreign host_api;
Utf8Count :: (handle: u64, text: string) -> s32 #foreign host_api;
main :: () -> s32 {
    handle := Modules.Load("StdUtf8.v1")
    if handle == 0 { return 1 }
    if !Utf8Valid(handle, "taiji") { return 2 }
    count := Utf8Count(handle, "taiji")
    if !Modules.Release(handle) { return 3 }
    return count
}
ZI
"$ziran" bundle --root "$work" --require-module StdUtf8.v1=std/utf8 \
    --import std_app:Utf8Valid=StdUtf8.v1:Valid \
    --import std_app:Utf8Count=StdUtf8.v1:Count \
    --entry std_app:main -o "$work/std_app.zx" "$work/std_app.zi"
std_result=$(cd "$work" && "$ziran" run --module-root "$modules" std_app.zx)
test "$std_result" = 5 || {
    echo "zx-module-build: std consumer printed '$std_result'"
    exit 1
}

# A library build that names an entry is refused once, by zi2zx, naming both
# flags; the front end adds no input or second error of its own.
refuse() {
    directory=$1
    shift
    if (cd "$directory" && "$ziran" "$@") >"$work/refusal.out" 2>"$work/refusal.err"; then
        echo 'zx-module-build: a library build with an entry succeeded'
        exit 1
    fi
    test ! -s "$work/refusal.out"
    test "$(grep -c -- '--library requires' "$work/refusal.err")" = 1 || {
        echo 'zx-module-build: the refusal is not one diagnostic'
        cat "$work/refusal.err"
        exit 1
    }
    grep -- '--library requires' "$work/refusal.err" | grep -q -- '--entry' || {
        echo 'zx-module-build: the refusal does not name --entry'
        cat "$work/refusal.err"
        exit 1
    }
}
refuse "$project" bundle --project --library demo/counter \
    --export counter:Add --entry counter:Add -o "$work/refused.zx"
refuse "$work" bundle --library demo/calc --export calc:Sub \
    --entry calc:Sub -o "$work/refused.zx" "$work/calc.zi"
test ! -e "$work/refused.zx"

echo 'Project ZX libraries build from manifest exports and load from a module root'
