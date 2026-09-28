#!/bin/sh
set -eu

# The examples page and the standard library reference are generated from
# site/examples and std/; fail when either no longer matches the toolchain.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo/scripts/site_pages.py" --ziran "$ziran" --check
