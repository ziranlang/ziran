#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -m py_compile "$repo"/bench/*.py
python3 "$repo/bench/record_calls.py" --help >/dev/null
python3 "$repo/bench/vm_startup.py" --help >/dev/null
python3 "$repo/bench/vm_execution.py" --help >/dev/null
python3 "$repo/bench/cpu_features.py" --help >/dev/null
