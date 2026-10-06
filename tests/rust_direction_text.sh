#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
command -v cargo >/dev/null 2>&1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/direction_text.zi" <<'ZI'
Directions :: "\u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069";
main :: () {
    print("literal \u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069\n")
    print("argument %\n", Directions)
    print("braces {%} %%\n", "\u2066")
}
ZI
python3 - "$work/expected" <<'PY'
import sys
from pathlib import Path
directions='\u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069'
Path(sys.argv[1]).write_bytes(('literal '+directions+'\nargument '+directions+'\nbraces {\u2066} %\n').encode())
PY
"$ziran" ir --root "$work" -o "$work/ir" "$work/direction_text.zi"
for form in source saved; do
    if test "$form" = source; then input="$work/direction_text.zi"; else input="$work/ir/direction_text.zir"; fi
    "$ziran" build --target=rust --exe --entry direction_text:main --root "$work" -o "$work/$form" "$input"
    CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --offline --manifest-path "$work/$form/Cargo.toml"
    "$work/rust-target/debug/ziran_generated" > "$work/$form.out"
    cmp "$work/expected" "$work/$form.out"
done
cmp "$work/source/src/main.rs" "$work/saved/src/main.rs"
echo 'Rust directional text passed source and saved IR with exact UTF-8 output'
