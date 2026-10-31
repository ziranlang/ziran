#!/bin/sh
# Native builds default to -O3, --release adds link-time optimization, and
# CFLAGS replaces both.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS CFLAGS
cat > "$work/step.zi" <<'ZI'
Step :: (value: s32) -> s32 { return value + 2; }
ZI
cat > "$work/app.zi" <<'ZI'
Step :: #import "step";
Main :: () -> s32 { return Step.Step(40); }
ZI
real_cc=${CC:-cc}
cat > "$work/cc" <<SH
#!/bin/sh
printf '%s\n' "\$*" >> "\$CC_LOG"
exec $real_cc "\$@"
SH
chmod +x "$work/cc"
build() { # name, extra arguments
    name=$1; shift
    CC_LOG="$work/$name.log" CC="$work/cc" "$ziran" build --target=c --exe "$@" --entry app:Main \
        --root "$work" -o "$work/$name" "$work/app.zi" "$work/step.zi"
    status=0
    "$work/$name/app" || status=$?
    test "$status" = 42
}
compile_lines() { grep -- ' -c ' "$work/$1.log"; }

build plain
compile_lines plain | grep -q -- '-O3'
if grep -q -- '-flto' "$work/plain.log"; then echo "default build used LTO" >&2; exit 1; fi

build release --release
compile_lines release | grep -q -- '-O3 -flto=auto'
grep -v -- ' -c ' "$work/release.log" | grep -- '-flto=auto' >/dev/null

CFLAGS=-O1 build chosen --release
compile_lines chosen | grep -q -- '-O1'
if grep -q -- '-flto' "$work/chosen.log"; then echo "CFLAGS did not replace --release" >&2; exit 1; fi

if "$ziran" build --target=c --release --entry app:Main --root "$work" -o "$work/none" \
    "$work/app.zi" "$work/step.zi" 2>"$work/none.err"; then
    echo "--release without --exe was accepted" >&2; exit 1
fi
grep -q -- '--release needs --exe' "$work/none.err"
