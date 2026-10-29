#!/bin/sh
# Environment values and leading whole numbers in a native C build.
set -eu
ziran=${1:-build/bin/ziran}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ziran-environment-XXXXXX")
trap 'rm -rf "$work"' EXIT
"$ziran" build --target=c --no-main --root "$root/tests" --module-path "$root/std" -o "$work/c" "$root/tests/environment_native.zi"
# shellcheck disable=SC2046
"${CC:-cc}" -O1 -DZIRAN_BOUNDS_CHECK -I"$root/include" $(find "$work/c" -type d -printf ' -iquote %p') \
    $(find "$work/c" -name '*.c') -o "$work/native"
env -u ZIRAN_ENVIRONMENT_UNSET ZIRAN_ENVIRONMENT_TEST="760 tall" ZIRAN_ENVIRONMENT_WORD=tall "$work/native"
echo "environment: values, unset names and leading numbers"
