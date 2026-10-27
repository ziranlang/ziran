#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(dirname "$ziran")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
# Checking a call's arguments can widen a nested call's own arguments, which
# appends casts and moves the function's expressions. The outer call must
# still record its result type, or the saved IR differs from the checked
# program and every later build from it is refused.
cat > "$work/widen.zi" <<'EOF'
Board :: struct { tiles: [4]u8; }
Index :: (x: s64, y: s64) -> s32 { return cast(s32)(y * 2 + x) }
Open :: (board: *Board, at: s32) -> bool { tile := board.tiles[at]; return tile < 20 || tile >= 128 }
Solid :: (board: *Board, x: s32, y: s32) -> bool { return !Open(board, Index(x, y)) }
#program_export
Answer :: () -> s64 { board: Board; board.tiles[3] = 30; if Solid(*board, 1, 1) { return 12 }; return 0 }
EOF
"$ziran" ir --root "$work" --module-path "$repo/std" -o "$work/ir" "$work/widen.zi"
"$ziran" bundle --root "$work" --module-path "$repo/std" --entry widen:Answer -o "$work/source.zx" "$work/widen.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry widen:Answer -o "$work/saved.zx" "$work/ir/widen.zir"
cmp "$work/source.zx" "$work/saved.zx"
"$bin/zi2c" --root "$work/ir" -o "$work/c" "$work/ir/widen.zir"
test "$("$ziran" run "$work/saved.zx")" = 12
