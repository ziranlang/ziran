#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" --help > "$work/help.txt"
grep -Fq 'explain' "$work/help.txt"
"$ziran" guide > "$work/guide.txt"
grep -Fq 'ziran explain --list --json' "$work/guide.txt"
"$ziran" explain check.slice_lifetime > "$work/text.txt"
grep -Fq 'code: check.slice_lifetime' "$work/text.txt"
grep -Fq 'borrowed slice or text view' "$work/text.txt"
grep -Fq 'Keep the owner live and immutable' "$work/text.txt"
"$ziran" explain zir.noncanonical --json > "$work/item.json"
"$ziran" explain --list --json > "$work/list.json"
python3 - "$repo" "$work/item.json" "$work/list.json" <<'PY'
import json
from pathlib import Path
import re
import sys

repo = Path(sys.argv[1])
item = json.loads(Path(sys.argv[2]).read_text())
listing = json.loads(Path(sys.argv[3]).read_text())
assert item == {
    "schema_version": 1,
    "stability": "stable-code",
    "code": "zir.noncanonical",
    "summary": "A saved-IR input or serialization is invalid or noncanonical.",
    "next_step": "Regenerate .zir from current source instead of editing it, and verify that source and saved-IR checks agree.",
}
assert listing["schema_version"] == 1
assert listing["stability"] == "stable-code"
assert len(listing["codes"]) >= 1
assert listing["codes"] == sorted(listing["codes"])

emitted = set()
for path in (repo / "cmd").rglob("*.c"):
    text = path.read_text()
    for match in re.finditer(r"Diagnostic(?:V)?\s*\(", text):
        end = match.end()
        tail = text[end:text.find(");", end)]
        literal = re.search(r'"([A-Za-z0-9_.-]+)"', tail)
        if literal:
            emitted.add(literal.group(1))
assert set(listing["codes"]) == emitted, (
    sorted(emitted - set(listing["codes"])),
    sorted(set(listing["codes"]) - emitted),
)
PY
cat > "$work/go-import.zi" <<'ZI'
libc :: #system_library "libc";
Read :: (value: s32) -> s32 #foreign libc "read";
ZI
if "$ziran" build --target=go --diagnostics=json --root "$work" \
    -o "$work/go-import" "$work/go-import.zi" \
    > "$work/go-import.out" 2> "$work/go-import.jsonl"; then
    echo 'Go accepted a C ABI import' >&2
    exit 1
fi
python3 - "$work/go-import.jsonl" <<'PY'
import json
from pathlib import Path
import sys

items = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert any(item["code"] == "zir_go.import" and
           "C ABI symbol" in item["message"] for item in items), items
PY
if "$ziran" explain check.not_a_code > "$work/bad.out" 2> "$work/bad.err"; then
    echo 'explain accepted an unknown diagnostic code' >&2
    exit 1
fi
grep -Fq 'unknown diagnostic code: check.not_a_code' "$work/bad.err"
if "$ziran" explain --json > "$work/no-code.out" 2> "$work/no-code.err"; then
    echo 'explain accepted --json without a code or --list' >&2
    exit 1
fi
