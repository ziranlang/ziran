#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/compile-commands-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

cat > "$work/src/main.zi" <<'ZI'
#program_export
Answer :: () -> s32 { return 42 }
ZI

"$ziran" build --target=c --root "$work/src" -o "$work/c" "$work/src/main.zi"
# The compiler command may carry flags; they follow the resolved program.
(cd "$work" && "$ziran" compile-commands --compiler "cc -O2" c)

set --
if test "$(uname -s)" = Darwin; then
    set -- --define __APPLE__
    if test "$(uname -m)" = x86_64; then
        set -- "$@" --define __x86_64__
    else
        set -- "$@" --define __aarch64__
    fi
fi
"$ziran" build "$@" --target=c --exe --entry compile_commands_cases:main \
    --root "$repo/tests" -o "$work/assertions" "$repo/tests/compile_commands_cases.zi"
"$work/assertions/compile_commands_cases" "$work"
sh "$work/compile-commands.sh"

if "$ziran" compile-commands --include "$work/missing" "$work/c" 2>/dev/null; then
    echo "a missing include directory was accepted" >&2
    exit 1
fi
