#!/bin/sh
set -eu

ziran=$1
tools=$(dirname "$ziran")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

# A widget library calls its platform through `#foreign host_api`; a separate
# host module provides the implementation as a program export. Linking from
# an entry must keep the provider and the import that reaches it.
cat > "$work/src/api.zi" <<'ZI'
host_api :: #system_library "host_api";
HostValue :: () -> s32 #foreign host_api;
#program_export
Value :: () -> s32 { return HostValue() }
ZI
cat > "$work/src/host.zi" <<'ZI'
#program_export
HostValue :: () -> s32 { return 42 }
#program_export
Unused :: () -> s32 { return 7 }
ZI
cat > "$work/src/main.zi" <<'ZI'
#import "api"
#import "host"
#program_export
main :: () -> s32 {
    if Value() == 42 { return 0 }
    return 1
}
ZI

"$tools/zi2zir" --root "$work/src" --entry main:main -o "$work/ir" \
    "$work/src/main.zi"
test -f "$work/ir/host.zir"
"$tools/zi2c" --entry main:main --root "$work/ir" -o "$work/c" \
    "$work/ir/main.zir"
if grep -q 'Unused' "$work/c/host.c"; then
    echo 'unreferenced program export survived entry linking' >&2
    exit 1
fi
"${CC:-cc}" -std=c99 -I"$repo/include" -I"$work/c" "$work/c"/*.c \
    -o "$work/app"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/app"
echo 'host export linking: passed'
