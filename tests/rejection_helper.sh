#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
. "$repo/tests/lib/expect_rejection.sh"

set -- 'parent argument'
expect_rejection 'expected failure was accepted' "$work/quoted path.err" \
    sh -c 'printf "%s" "$1" >&2; exit 7' -- 'two words'
test "$(cat "$work/quoted path.err")" = 'two words'
test "$#" = 1
test "$1" = 'parent argument'

# Errexit is suppressed within an if condition. An unexpected acceptance
# still has to exit the parent script before either marker runs.
cat > "$work/accepted.sh" <<'SH'
set -eu
. "$1"
test_case() {
    expect_rejection 'unexpected acceptance' "$1" \
        sh -c 'printf accepted >&2; exit 0'
    echo continued
}
if test_case "$2"; then echo accepted; fi
echo finished
SH
status=0
sh "$work/accepted.sh" "$repo/tests/lib/expect_rejection.sh" "$work/accepted.err" \
    > "$work/stdout" 2> "$work/stderr" || status=$?
test "$status" = 1
test "$(cat "$work/stderr")" = 'unexpected acceptance'
test "$(cat "$work/accepted.err")" = accepted
test ! -s "$work/stdout"
