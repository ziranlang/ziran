#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -m py_compile "$repo"/bench/*.py
python3 "$repo/bench/record_calls.py" --help >/dev/null
