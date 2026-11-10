#!/bin/sh
set -eu
python3 tests/package_relative_imports.py "${1:?pass ziran}"
