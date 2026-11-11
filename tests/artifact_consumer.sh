#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$1" run --target=c --root "$root/tests" "$root/tests/artifact_consumer_cases.zi" -- "$1"
