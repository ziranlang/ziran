#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" features --json > "$work/features.json"
python3 - "$work/features.json" "$repo" <<'PY'
import json
from pathlib import Path
import sys

actual = Path(sys.argv[1]).read_bytes()
repo = Path(sys.argv[2])
manifest_bytes = (repo / "docs/FEATURES.json").read_bytes()
assert actual == manifest_bytes, "ziran features --json disagrees with docs/FEATURES.json"
manifest = json.loads(actual)
assert manifest["schema_version"] == 1
assert manifest["stability"] == "stable-id"
features = manifest["features"]
assert features
ids = [feature["id"] for feature in features]
assert ids == sorted(ids)
assert len(ids) == len(set(ids))
targets = ["c", "cpp", "go", "rust", "zib", "plan9-c"]
required = {
    "id", "status", "syntax", "target_support", "limits",
    "accepted_example", "rejected_example", "rejection", "evidence"
}
for feature in features:
    assert set(feature) == required
    assert feature["status"] == "implemented"
    assert all(feature[key] for key in (
        "id", "syntax", "limits", "accepted_example", "rejected_example",
        "rejection", "evidence"
    ))
    assert [item["target"] for item in feature["target_support"]] == targets
    assert all(set(item) == {"target", "status", "limit"} for item in feature["target_support"])
    assert all(item["status"] in ("supported", "experimental", "unsupported") for item in feature["target_support"])
    for evidence in feature["evidence"]:
        if "/" in evidence:
            assert (repo / evidence).is_file(), evidence
numeric = json.loads((repo / "tests/numeric_conformance.json").read_text())
numeric_ids = {case["id"] for case in numeric["cases"]}
numeric_feature = next(feature for feature in features if feature["id"] == "numeric.integer-width-conformance")
assert {item for item in numeric_feature["evidence"] if "/" not in item} <= numeric_ids
PY

python3 - "$work/features.json" "$work" <<'PY'
import json
from pathlib import Path
import sys

manifest = json.loads(Path(sys.argv[1]).read_text())
work = Path(sys.argv[2])
for index, feature in enumerate(manifest["features"]):
    (work / f"{index}-accepted.zi").write_text(feature["accepted_example"])
    (work / f"{index}-rejected.zi").write_text(feature["rejected_example"])
PY

for source in "$work"/*-accepted.zi; do
    "$ziran" check --diagnostics=json --root "$work" \
        --module-path "$repo/std" "$source" > "$work/accepted.out" \
        2> "$work/accepted.err"
done
for source in "$work"/*-rejected.zi; do
    if "$ziran" check --diagnostics=json --root "$work" \
        --module-path "$repo/std" "$source" \
        > "$work/rejected.out" 2> "$work/rejected.err"; then
        echo "$source was accepted by ziran check" >&2
        exit 1
    fi
    test -s "$work/rejected.err"
done

first_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["features"][0]["id"])' "$work/features.json")
"$ziran" features --id "$first_id" --json > "$work/selected.json"
python3 - "$work/selected.json" "$first_id" <<'PY'
import json
from pathlib import Path
import sys
selected = json.loads(Path(sys.argv[1]).read_text())
assert selected["schema_version"] == 1
assert selected["stability"] == "stable-id"
assert selected["feature"]["id"] == sys.argv[2]
PY

"$ziran" capabilities --json > "$work/capabilities.json"
"$ziran" guide > "$work/guide.txt"
"$ziran" --help > "$work/help.txt"
python3 - "$work/features.json" "$work/capabilities.json" \
        "$work/guide.txt" "$work/help.txt" <<'PY'
import json
from pathlib import Path
import sys
manifest = json.loads(Path(sys.argv[1]).read_text())
capabilities = json.loads(Path(sys.argv[2]).read_text())
guide = Path(sys.argv[3]).read_text()
help_text = Path(sys.argv[4]).read_text()
assert capabilities["targets"] == [item["target"] for item in manifest["features"][0]["target_support"]]
assert "ziran features --id compile.assertions --json" in guide
assert "features" in help_text
PY

if "$ziran" features --id missing.feature > "$work/unknown.out" 2> "$work/unknown.err"; then
    echo 'features accepted an unknown ID' >&2
    exit 1
fi
if "$ziran" features --id > "$work/no-id.out" 2> "$work/no-id.err"; then
    echo 'features accepted --id without a value' >&2
    exit 1
fi
if "$ziran" features first extra > "$work/extra.out" 2> "$work/extra.err"; then
    echo 'features accepted extra positional arguments' >&2
    exit 1
fi
