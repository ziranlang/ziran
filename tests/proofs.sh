#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/proofs-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" fmt --check "$repo/tests/proof/PROOF.zi" "$repo/tests/proof/LAWS.zi" "$repo/tests/proof/scalar.zi"

"$ziran" check --root "$repo/tests/proof" "$repo/tests/proof/PROOF.zi" > "$work/source.json"
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/law_assertions.zi" -- "$ziran" "$work" proofs
"$ziran" ir --root "$repo/tests/proof" -o "$work/ir" "$repo/tests/proof/PROOF.zi"
"$ziran" check --root "$work/ir" "$work/ir/PROOF.zir" > "$work/saved.json"
cmp "$work/source.json" "$work/saved.json"
"$ziran" bundle --root "$repo/tests/proof" --entry scalar:Answer -o "$work/source.zx" "$repo/tests/proof/PROOF.zi"
"$ziran" bundle --root "$work/ir" --entry scalar:Answer -o "$work/saved.zx" "$work/ir/PROOF.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42

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
${CC:-cc} ${VM_CFLAGS:-} -D_GNU_SOURCE -std=c11 -I"$repo/cmd/zir" -I"$repo/bootstrap/compiler_ir" -I"$repo/include" \
    "$repo/tests/proof_tamper.c" "${ZIRAN_LIB:-$repo/build/libziran.a}" -lm -o "$work/tamper"
for kind in goal step cycle index evidence; do
    "$work/tamper" "$work/gir/good.zir" "$work/bad.zir" "$kind"
    if "$ziran" check --root "$work" "$work/bad.zir" > "$work/bad.json" 2> "$work/bad.err"; then
        echo "tampered $kind certificate was accepted" >&2; exit 1
    fi
done
# Laws are checked from source; a bundle carries no evidence to forge, and an
# edited byte fails its identity check.
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/saved_ir_mutations.zi" -- --replace-first "$work/source.zx" "$work/forged.zx" Answer Bnswer
if "$ziran" run "$work/forged.zx" > /dev/null 2> "$work/forged.err"; then
    echo 'an edited bundle was accepted' >&2; exit 1
fi
rg -q 'executable ZX rejected' "$work/forged.err"

"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" proof-negatives
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
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" proof-budget
"$ziran" check --root "$work" "$work/budget.zi" > "$work/budget.json"
rg -q '"status":"unknown".*deterministic budget' "$work/budget.json"
"$ziran" check --root "$work" "$work/budget.zi" > "$work/budget-again.json"
cmp "$work/budget.json" "$work/budget-again.json"
