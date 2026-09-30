#!/bin/sh
# --bind-host MODULE binds every host capability to the exported function of
# the same name in MODULE, as a C program links a host's symbols by name, so
# one Ziran host module serves the portable runner, Go, Rust, and Python.
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/api.zi" <<'ZI'
host_api :: #system_library "host_api";
Measure :: (value: s32) -> s32 #foreign host_api;
Scale :: (value: s32) -> s32 #foreign host_api;

#program_export
Area :: (side: s32) -> s32 { return Scale(Measure(side)) }
ZI
cat > "$work/host.zi" <<'ZI'
#program_export
Measure :: (value: s32) -> s32 { return value * value }

#program_export
Scale :: (value: s32) -> s32 { return value * 2 }

#program_export
Twice :: (value: s32) -> s32 { return value * 2 + 1 }
ZI
cat > "$work/app.zi" <<'ZI'
#import "api"
#import "host"

#program_export
main :: () -> s32 {
    print("%\n", Area(3))
    return 0
}
ZI
sources="$work/app.zi $work/api.zi $work/host.zi"

# C links the capabilities to host's symbols by name.
"$ziran" build --target=c --exe --root "$work" --entry app:main -o "$work/c" $sources
test "$("$work/c/app")" = 18

"$ziran" bundle --root "$work" --entry app:main --bind-host host \
    -o "$work/app.zib" $sources
test "$("$ziran" run "$work/app.zib")" = "18
0"

"$ziran" build --target=go --pkg main --exe --root "$work" --entry app:main \
    --bind-host host -o "$work/go" $sources
(cd "$work/go" && GO111MODULE=off go build -o app .)
test "$("$work/go/app")" = 18

"$ziran" build --target=rust --exe --root "$work" --entry app:main \
    --bind-host host -o "$work/rust" $sources
CARGO_TARGET_DIR=$work/rust-target cargo build --quiet \
    --manifest-path "$work/rust/Cargo.toml"
test "$("$work/rust-target/debug/ziran_generated")" = 18

"$ziran" build --target=py --exe --root "$work" --entry app:main \
    --bind-host host -o "$work/py" $sources
test "$(python3 "$work/py")" = 18

# An explicit --bind wins over the module's same-named function.
"$ziran" bundle --root "$work" --entry app:main --bind api:Scale=host:Twice \
    --bind-host host -o "$work/explicit.zib" $sources
test "$("$ziran" run "$work/explicit.zib")" = "19
0"

if "$ziran" bundle --root "$work" --entry app:main --bind-host nowhere \
    -o "$work/missing.zib" $sources > "$work/missing.out" 2> "$work/missing.err"; then
    echo 'a missing host module was accepted' >&2
    exit 1
fi
grep -q 'host provider module nowhere is not in the program' "$work/missing.err"
