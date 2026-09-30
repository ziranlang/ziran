#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY

# Laws decide floats only where the answer is exact on every target: an
# integer cast to a float within its significand, and comparisons of such
# values. Float arithmetic, which rounds, and wider casts stay unknown.
ziran=${1:?pass ziran}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/floats.zi" <<'ZI'
Seconds :: (attempt: s32) -> s32 {
    if attempt <= 1 { return 5 }
    return 60
}
Delay :: (attempt: s32) -> float64 {
    return cast(float64) Seconds(attempt)
}
Small :: (attempt: s32) -> float32 {
    return cast(float32) Seconds(attempt)
}
#law DelayMatches forall attempt: -2..6 => Delay(attempt) == cast(float64) Seconds(attempt);
#law DelayOrdered forall attempt: -2..6 => Delay(attempt) >= cast(float64) 5;
#law SmallMatches forall attempt: -2..6 => Small(attempt) == cast(float32) Seconds(attempt);
#law MixedCompare forall attempt: -2..6 => Delay(attempt) <= 60;
#law SignificandEdge custom cast(float64) cast(s64) 9007199254740992 == cast(float64) cast(s64) 9007199254740992;
#program_export
Answer :: () -> s32 { return 0 }
ZI

cat > "$work/unknown.zi" <<'ZI'
Delay :: (attempt: s32) -> float64 {
    return cast(float64) attempt
}
#law WideCast custom cast(float64) cast(s64) 9007199254740993 == cast(float64) cast(s64) 9007199254740993;
#law Arithmetic forall attempt: 0..3 => Delay(attempt) + Delay(attempt) == Delay(attempt) * cast(float64) 2;
#program_export
Answer :: () -> s32 { return 0 }
ZI

cat > "$work/false.zi" <<'ZI'
Delay :: (attempt: s32) -> float64 {
    return cast(float64) attempt
}
#law AlwaysFive forall attempt: 4..6 => Delay(attempt) == cast(float64) 5;
#program_export
Answer :: () -> s32 { return 0 }
ZI

"$ziran" check --root "$work" "$work/floats.zi" > "$work/floats.json"
if "$ziran" check --root "$work" "$work/unknown.zi" > "$work/unknown.json" 2> /dev/null; then
    echo 'float laws outside the exact fragment were accepted' >&2; exit 1
fi
if "$ziran" check --root "$work" "$work/false.zi" > "$work/false.json" 2> /dev/null; then
    echo 'a false float law was accepted' >&2; exit 1
fi
python3 - "$work" <<'PY'
import json, sys
from pathlib import Path
work = Path(sys.argv[1])
def statuses(name):
    return {entry['law']: entry['status']
            for entry in map(json.loads, (work / name).read_text().splitlines())}
proved = statuses('floats.json')
assert proved == {'DelayMatches': 'proved', 'DelayOrdered': 'proved',
                  'SmallMatches': 'proved', 'MixedCompare': 'proved',
                  'SignificandEdge': 'proved'}, proved
unknown = statuses('unknown.json')
assert unknown == {'WideCast': 'unknown', 'Arithmetic': 'unknown'}, unknown
false = statuses('false.json')
assert false == {'AlwaysFive': 'disproved'}, false
PY
