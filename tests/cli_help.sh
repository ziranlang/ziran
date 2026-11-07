#!/bin/sh
# `ziran COMMAND --help` and `-h` print that command's usage and exit 0 without
# acting: the scratch project's files, lock and the package cache stay
# byte-identical. A --help after `--` still belongs to the script.
set -eu

ziran=${1:?pass the ziran command}
case $ziran in /*) ;; *) ziran=$(pwd)/$ziran ;; esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir -p "$work/project/src" "$work/cache" "$work/home"
cat > "$work/project/ziran.toml" <<'TOML'
[package]
name = "helpcheck"
version = "0.1.0"
TOML
printf 'main :: () -> s32 { return 0 }\n' > "$work/project/src/main.zi"
printf 'lock sentinel\n' > "$work/project/ziran.lock"

digest() {
    (cd "$work" && find project cache home -type f -print0 | LC_ALL=C sort -z |
        xargs -0 sha256sum && find project cache home -print | LC_ALL=C sort) |
        sha256sum
}

before=$(digest)
cd "$work/project"
export HOME="$work/home" ZIRAN_CACHE="$work/cache" ZIRAN_PINNED_LAUNCHER=1
unset DISPLAY WAYLAND_DISPLAY

for command in version guide features capabilities explain api new init lock \
    update fetch add pkg tool install check ir build bundle run fmt inspect \
    compile-commands edit compact expand; do
    for flag in --help -h; do
        "$ziran" "$command" "$flag" > "$work/out" 2> "$work/err" ||
            { echo "ziran $command $flag exited $?" >&2; cat "$work/err" >&2; exit 1; }
        grep -q "^usage: ziran .*$command" "$work/out" ||
            { echo "ziran $command $flag printed no usage" >&2; cat "$work/out" >&2; exit 1; }
        [ ! -s "$work/err" ] ||
            { echo "ziran $command $flag wrote to stderr" >&2; exit 1; }
        [ "$(digest)" = "$before" ] ||
            { echo "ziran $command $flag changed the tree" >&2; exit 1; }
    done
done

# Flags after the command's own arguments are still the command's.
"$ziran" update somepackage --help > "$work/out"
grep -q '^usage: ziran update' "$work/out"
[ "$(digest)" = "$before" ]

# After `--`, or after a script's source file, --help belongs to the script.
printf 'main :: (argc: s32, argv: **u8) -> s32 { return argc }\n' > "$work/script.zi"
if "$ziran" run --target=c "$work/script.zi" -- --help > "$work/out" 2>&1; then
    echo "script did not receive --help" >&2; exit 1
fi
if grep -q '^usage: ziran run' "$work/out"; then
    echo "run printed usage for a script's --help" >&2; exit 1
fi
