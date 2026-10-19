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

# A directory module's short imports resolve in its own package even when
# nothing else loaded them first.
cat > "$work/app/directory.zi" <<'ZI'
#import "lib/Greeting"
#program_export
Answer :: () -> s32 { return Greet() + Extra() }
ZI
"$ziran" bundle --root "$work/app" --module-path "lib=$work/lib/src" \
    --entry directory:Answer -o "$work/directory.zib" "$work/app/directory.zi"
test "$("$ziran" run "$work/directory.zib")" = 42

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
    --entry forest:Answer -o "$work/forest.zib" "$work/app/forest.zi"
test "$("$ziran" run "$work/forest.zib")" = 42
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
