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
    --entry main:Answer -o "$work/app.zib" "$work/app/main.zi"
test "$("$ziran" run "$work/app.zib")" = 42

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
