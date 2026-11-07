#!/bin/sh
# One std module however it is spelled: a project imports `hash_map` in one
# file and `std/hash_map` in another, and both share one HashMap type and one
# set of HashKey overloads. Checked as a project and as plain files, and run.
set -eu

ziran=${1:?pass the ziran command}
case $ziran in /*) ;; *) ziran=$PWD/$ziran ;; esac
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

project=$work/project
mkdir -p "$project/src"
cat > "$project/ziran.toml" <<'TOML'
[package]
name = "spelling"
module_roots = ["src"]
entry = "src/main.zi"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
TOML
printf '[overrides]\nziran = "%s"\n' "$repo" > "$project/ziran.local.toml"
cat > "$project/src/short.zi" <<'ZI'
#import "hash_map"

#scope_export
ShortKey :: () -> u64 { return HashKey("abc") }
ShortPut :: (map: *HashMap(string, s32), value: s32) { HashMapSet(map, "abc", value) }
ZI
cat > "$project/src/long.zi" <<'ZI'
#import "std/hash_map"

#scope_export
LongKey :: () -> u64 { return HashKey("abc") }
LongGet :: (map: *HashMap(string, s32)) -> s32 {
    value: s32
    if HashMapGet(map, "abc", *value) { return value }
    return -1
}
ZI
cat > "$project/src/main.zi" <<'ZI'
#import "short"
#import "long"
#import "hash_map"
#import "std/hash_map"

main :: () {
    map: HashMap(string, s32)
    ShortPut(*map, 42)
    same := ShortKey() == LongKey() && HashKey("abc") == LongKey()
    print("% %\n", LongGet(*map), same)
    HashMapFree(*map)
}
ZI
expected="42 true"

cd "$project"
"$ziran" lock
"$ziran" check --project
result=$("$ziran" run)
test "$result" = "$expected" || { echo "project run: $result, expected $expected" >&2; exit 1; }
# Both spellings load one hash_map module.
"$ziran" ir --project --entry main:main -o "$work/project-ir"
modules=$(ls "$work/project-ir" | grep -c 'hash_map' || true)
test "$modules" = 1 || { ls "$work/project-ir" >&2; echo "project: $modules hash_map modules" >&2; exit 1; }

# The same files outside a project, with the compiler's own std.
cd "$work"
"$ziran" check --root "$project/src" "$project/src/main.zi"
"$ziran" ir --root "$project/src" --entry main:main -o "$work/ir" "$project/src/main.zi"
modules=$(ls "$work/ir" | grep -c 'hash_map' || true)
test "$modules" = 1 || { ls "$work/ir" >&2; echo "plain: $modules hash_map modules" >&2; exit 1; }
result=$("$ziran" run --target=c "$project/src/main.zi")
test "$result" = "$expected" || { echo "plain run: $result, expected $expected" >&2; exit 1; }
echo "std import spelling identity: hash_map and std/hash_map share one module"
