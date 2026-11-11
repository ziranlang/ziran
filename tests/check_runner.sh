#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$(dirname -- "${1:-$repo/build/bin/ziran}")/check-runner" --self-test
