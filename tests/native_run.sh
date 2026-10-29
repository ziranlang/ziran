#!/bin/sh
# Native script runs: arguments, cache reuse and invalidation, and a project
# tool kept beside the project's module root.
set -eu
ziran=$(realpath "${1:?pass the ziran command}")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
export XDG_CACHE_HOME="$work/cache"
export PATH="$(dirname "$ziran"):$PATH"

cat > "$work/helper.zi" <<'ZI'
Value :: () -> s32 { return 41 }
ZI
cat > "$work/script.zi" <<'ZI'
#import "helper"
#import "std/byte_text_linux"
main :: (argc: s32, argv: **u8) -> s32 {
    print("%", Value())
    for index: 1..argc - 1 { print(":%", TextFromCString(argv[index])) }
    print("\n")
    return ifx argc > 1 && TextFromCString(argv[1]) == "fail" then 3 else 0
}
ZI
cd "$work"
test "$("$ziran" run --target=c script.zi "two words" --root x.zi)" = '41:two words:--root:x.zi'
ready=$(rg --files --hidden -g .ready "$XDG_CACHE_HOME/ziran/native")
test "$(printf '%s\n' "$ready" | wc -l)" -eq 1
output=${ready%/.ready}/script
before=$(stat -c %y "$output")
test "$("$ziran" run --target=c script.zi -- a b)" = '41:a:b'
test "$(stat -c %y "$output")" = "$before"
# The program's status is the run's status.
if "$ziran" run --target=c script.zi fail > /dev/null; then
    echo 'native run hid a failing status' >&2; exit 1
fi
# A dependency edit rebuilds the program.
sed -i 's/return 41/return 42/' helper.zi
test "$("$ziran" run --target=c script.zi)" = '42'

# A project tool beside the module root imports the project's modules.
mkdir -p project/src project/tools project/build/huge
cat > project/ziran.toml <<'TOML'
[package]
name = "native-tools"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
TOML
printf '[overrides]\nziran = "%s"\n' "$repo" > project/ziran.local.toml
cat > project/src/shared.zi <<'ZI'
Shared :: () -> s32 { return 7 }
ZI
cat > project/tools/check.zi <<'ZI'
#import "shared"
main :: () -> s32 {
    print("tool %\n", Shared())
    return 0
}
ZI
cd project
"$ziran" lock
test "$("$ziran" run --target=c tools/check.zi)" = 'tool 7'
echo "native run: arguments, status, cache reuse, rebuilds and project tools"
