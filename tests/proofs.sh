#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" fmt --check "$repo/tests/proof/PROOF.zi" "$repo/tests/proof/LAWS.zi" "$repo/tests/proof/scalar.zi"

"$ziran" check --root "$repo/tests/proof" "$repo/tests/proof/PROOF.zi" > "$work/source.json"
python3 - "$work/source.json" <<'PY'
import json, sys
laws = [json.loads(line) for line in open(sys.argv[1])]
assert len(laws) == 8
assert all(l['status'] == 'proved' and l['evidence']['method'] == 'kernel' for l in laws)
assert all(l['evidence']['cases_checked'] == 0 for l in laws)
PY
"$ziran" ir --root "$repo/tests/proof" -o "$work/ir" "$repo/tests/proof/PROOF.zi"
"$ziran" check --root "$work/ir" "$work/ir/PROOF.zir" > "$work/saved.json"
cmp "$work/source.json" "$work/saved.json"
"$ziran" bundle --root "$repo/tests/proof" --entry scalar:Answer -o "$work/source.zib" "$repo/tests/proof/PROOF.zi"
"$ziran" bundle --root "$work/ir" --entry scalar:Answer -o "$work/saved.zib" "$work/ir/PROOF.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42

cat > "$work/good.zi" <<'ZI'
Adjust :: (x: u8) -> u8 {
    result: u8 = x
    if x == cast(u8)255 {
        result += cast(u8)1
    } else {
        result = result + cast(u8)0
    }
    return result
}
#law Zero theorem x: s32 => x + 0 == x;
#proof Zero {
    // Modular arithmetic includes overflow.
    ring; // close the identity { }
} // certificate { }
#law Reuse theorem x: s32 => x + 0 == x;
#proof Reuse { use Zero(x); }
#law Rewrite theorem x: s32 => (x + 0) * x == x * x;
#proof Rewrite { rewrite Zero(x); refl; }
#law Boolean theorem p: bool => p || !p;
#proof Boolean { cases; refl; }
#law BooleanEquality theorem p: bool, q: bool => (p && q) == (q && p);
#proof BooleanEquality { cases; refl; }
#law DoubleNegation theorem p: bool => !(!p) == p;
#proof DoubleNegation { cases; refl; }
#law LocalBranch theorem x: u8 => Adjust(x) == x || x == cast(u8)255;
#proof LocalBranch { unfold Adjust; cases; ring; }
Code :: enum u8 { Ok, Failed }
#law AllEnumBits theorem x: Code => x == x;
#proof AllEnumBits { refl; }
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" check --root "$work" "$work/good.zi" > "$work/good.json"
"$ziran" ir --root "$work" -o "$work/gir" "$work/good.zi"
${CC:-cc} ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/include" \
    "$repo/tests/proof_tamper.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/tamper"
for kind in goal step cycle index evidence; do
    "$work/tamper" "$work/gir/good.zir" "$work/bad.zir" "$kind"
    if "$ziran" check --root "$work" "$work/bad.zir" > "$work/bad.json" 2> "$work/bad.err"; then
        echo "tampered $kind certificate was accepted" >&2; exit 1
    fi
done
python3 - "$work/source.zib" "$work/forged.zib" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert b'kernel' in data
Path(sys.argv[2]).write_bytes(data.replace(b'kernel', b'forged', 1))
PY
if "$ziran" run "$work/forged.zib" > /dev/null 2> "$work/forged.err"; then
    echo 'forged bundle law evidence was accepted' >&2; exit 1
fi
rg -q 'law table differs' "$work/forged.err"

python3 - "$work" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
cases = {
 'false': '#law Bad theorem x: s32 => x + 1 == x;\n#proof Bad { ring; }',
 'wrap_order': '#law Bad theorem x: u8 => x + 1 > x;\n#proof Bad { cases; order; }',
 'wrong_type': '#law Bad theorem x: s32 => x == cast(u32)0;\n#proof Bad { ring; }',
 'missing': '#law Bad theorem x: s32 => x == x;',
 'incomplete': '#law Bad theorem x: s32 => x == x;\n#proof Bad { cases; }',
 'circular': '#law A theorem x: s32 => x == x;\n#law B theorem x: s32 => x == x;\n#proof A { use B(x); }\n#proof B { use A(x); }',
 'waived_cycle': '#law A theorem x: s32 => x == x;\n#law B theorem x: s32 => x == x;\n#proof A { use B(x); }\n#proof B { use A(x); }\n#law_waive A "invalid";\n#law_waive B "invalid";',
 'unproved_lemma': '#law A theorem x: s32 => x == x;\n#law B theorem x: s32 => x == x;\n#law_waive B "pending";\n#proof A { use B(x); }',
 'bad_rewrite': '#law A theorem x: s32 => x + 1 == x;\n#law B theorem x: s32 => x + 1 == x;\n#proof A { ring; }\n#proof B { rewrite A(x); refl; }',
 'duplicate': '#law A theorem x: s32 => x == x;\n#proof A { refl; }\n#proof A { refl; }',
 'foreign': 'Host :: #system_library "host";\nRead :: () -> s32 #foreign Host;\n#law Bad theorem x: s32 => Read() == x;\n#proof Bad { unfold Read; ring; }',
 'loop': 'Loop :: (x: s32) -> s32 { value: s32 = x; while value > 0 { value -= 1 }; return value }\n#law Bad theorem x: s32 => Loop(x) == 0;\n#proof Bad { unfold Loop; ring; }',
 'recursion': 'Rec :: (x: s32) -> s32 { return Rec(x) }\n#law Bad theorem x: s32 => Rec(x) == x;\n#proof Bad { unfold Rec; ring; }',
 'waive_invalid': '#law Bad theorem x: s32 => x + 1 == x;\n#proof Bad { ring; }\n#law_waive Bad "cannot authorize an invalid proof";',
}
for name, source in cases.items():
    (root / f'bad_{name}.zi').write_text(source + '\n#program_export\nAnswer :: () -> s32 { return 0 }\n')
PY
for source in "$work"/bad_*.zi; do
    if "$ziran" check --root "$work" "$source" > "$work/rejected.json" 2> "$work/rejected.err"; then
        echo "$source passed a proof gate" >&2; exit 1
    fi
    test -s "$work/rejected.err"
done

# Imported waivers use qualified law identity. Equal short names in different
# modules do not collide, and a waiver never supplies a lemma certificate.
cat > "$work/pending.zi" <<'ZI'
#law Same theorem x: s32 => x == x;
ZI
cat > "$work/qualified.zi" <<'ZI'
Pending :: #import "pending";
#law Same theorem x: s32 => x == x;
#proof Same { refl; }
#law_waive Pending.Same "proof pending";
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" check --root "$work" "$work/qualified.zi" > "$work/qualified.json"
rg -q '"waived":true' "$work/qualified.json"

cat > "$work/claims.zi" <<'ZI'
#law Zero theorem x: s32 => x + 0 == x;
#proof Zero { ring; }
ZI
cat > "$work/reuse.zi" <<'ZI'
Claims :: #import "claims";
#law Imported theorem x: s32 => x + 0 == x;
#proof Imported { use Claims.Zero(x); }
#program_export
Answer :: () -> s32 { return 42 }
ZI
"$ziran" check --root "$work" "$work/reuse.zi" > "$work/imported.json"
python3 - "$work" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
args = ', '.join(f'x{i}: s32' for i in range(16))
factors = [f'(x{i} + x{i+1})' for i in range(0,16,2)]
lhs = ' * '.join(factors)
rhs = ' * '.join(reversed(factors))
(root / 'budget.zi').write_text(f'#law Budget theorem {args} => {lhs} == {rhs};\n#proof Budget {{ ring; }}\n#law_waive Budget "normalization budget";\n#program_export\nAnswer :: () -> s32 {{ return 42 }}\n')
PY
"$ziran" check --root "$work" "$work/budget.zi" > "$work/budget.json"
rg -q '"status":"unknown".*deterministic budget' "$work/budget.json"
"$ziran" check --root "$work" "$work/budget.zi" > "$work/budget-again.json"
cmp "$work/budget.json" "$work/budget-again.json"
