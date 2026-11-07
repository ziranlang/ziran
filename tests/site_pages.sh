#!/bin/sh
set -eu

# The examples page and the standard library reference are generated from
# site/examples and std/; fail when either no longer matches the toolchain.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo/scripts/site_pages.py" --ziran "$ziran" --check

# A module that #loads sibling files reports each item at its own file and
# line; the doc comments must come from the declaring file, not the module.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/src"
printf '// Loader module.\n\n#load "part.zi";\n' > "$work/src/loader.zi"
printf '// Part file.\n\n\n\n\n\n// Doubles a number.\nDouble :: (n: s32) -> s32 {\n    return n * 2;\n}\n' > "$work/src/part.zi"
python3 -I - "$repo/scripts" "$ziran" "$work/src" <<'E2'
import sys
sys.path.insert(0, sys.argv[1])
import site_pages
summary, entries, _ = site_pages.module_api(sys.argv[2], sys.argv[3] + '/loader.zi', sys.argv[3], [])
assert summary == 'Loader module.', summary
assert [(sig, doc) for _, sig, doc, _ in entries] == [('Double :: (n: s32) -> s32', 'Doubles a number.')], entries
E2
