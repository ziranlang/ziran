#!/bin/sh
# bench/bench.zi registers every benchmark driver in bench/ and gives each the
# shared benchmark environment (recording off, no display session).
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:?pass ziran launcher}
cd "$repo"
exec "$ziran" run --target=c --root bench --module-path . bench/bench.zi -- check
