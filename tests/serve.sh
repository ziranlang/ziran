#!/bin/sh
# `ziran serve` runs ordinary commands in one foreground process: a failing
# command does not end the session, the greeting and prompt go to standard
# error, `Exit` stops before later input, the end of input leaves too, and a
# nested serve is refused.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/serve-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

fail() { echo "serve: $*" >&2; exit 1; }

app="$work/app"
mkdir -p "$app"
cat > "$app/good.zi" <<'ZI'
main :: () -> s32 { return 0 }
ZI
cat > "$app/bad.zi" <<'ZI'
main :: () -> s32 { return "text" }
ZI

"$ziran" version >"$work/version.txt" || fail "cannot read the version"
if "$ziran" check --root "$app" "$app/bad.zi" >"$work/direct.out" 2>"$work/direct.err"; then
    fail "the invalid check passed directly"
fi

cat > "$work/session.txt" <<EOF
version
check --root $app $app/bad.zi
serve
Exit
version
EOF
if ! "$ziran" serve <"$work/session.txt" >"$work/out" 2>"$work/err"; then
    fail "the session failed"
fi
cmp "$work/out" "$work/version.txt" || fail "Exit did not stop before the later command"
grep -Fq "bad.zi" "$work/err" || fail "the failing command reported nothing"
grep -Fq "already in a serve session" "$work/err" || fail "a nested serve was not refused"
grep -Fq "ziran> " "$work/err" || fail "the prompt is missing"

# Standard input's end leaves as well, and a command's standard output is
# only its own: prompts never appear there.
printf 'version\n' >"$work/eof.txt"
if ! "$ziran" serve <"$work/eof.txt" >"$work/eof.out" 2>"$work/eof.err"; then
    fail "the session did not end at the end of input"
fi
cmp "$work/eof.out" "$work/version.txt" || fail "the prompt reached standard output"

# The command takes no arguments and answers --help without starting a session.
if "$ziran" serve extra </dev/null >"$work/extra.out" 2>"$work/extra.err"; then
    fail "serve accepted an argument"
fi
"$ziran" serve --help >"$work/help.out" 2>"$work/help.err" || fail "serve --help failed"
grep -Fq "usage: ziran serve" "$work/help.out" "$work/help.err" || fail "serve has no usage line"
