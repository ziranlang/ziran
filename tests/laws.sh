#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/laws.zi" <<'ZI'
host_api :: #system_library "host_api";
Count :: 4
Point :: struct { x: s32; y: s32; }
Pure :: (a: s32, b: s32) -> s32 { return a + b }
Extern :: () -> s32 #foreign host_api;

#law LayoutComplete type Point;
#law BoundResolved bounds [Count]s32;
#law EffectPure effect Pure;
#law AbiPortable abi Pure;
#law CustomTrue custom Pure(20, 22) == 42;
#law EffectClassPure effect Pure == pure;
#law BoundExact bounds [Count]s32 == 4;
#law BoundAtLeast bounds [Count]s32 >= 4;
#law SizeExact size Point == 8;
#law SizeAtLeast size Point >= 8;

#program_export
Answer :: () -> s32 { return Pure(20, 22) }
ZI

"$ziran" check --root "$work" "$work/laws.zi" > "$work/laws.json"
test "$(wc -l < "$work/laws.json")" = 10
rg -q '"law":"LayoutComplete","kind":"type".*"status":"proved"' "$work/laws.json"
rg -q '"law":"BoundResolved","kind":"bounds".*"status":"proved"' "$work/laws.json"
rg -q '"law":"EffectPure","kind":"effect".*"status":"proved"' "$work/laws.json"
rg -q '"law":"AbiPortable","kind":"abi".*"status":"proved"' "$work/laws.json"
rg -q '"law":"CustomTrue","kind":"custom".*"status":"proved"' "$work/laws.json"
rg -q '"law":"EffectClassPure".*"status":"proved".*Pure is pure' "$work/laws.json"
rg -q '"law":"BoundExact".*"status":"proved".*bound 4 == 4' "$work/laws.json"
rg -q '"law":"SizeExact".*"status":"proved".*8 bytes == 8' "$work/laws.json"

"$ziran" ir --root "$work" -o "$work/ir" "$work/laws.zi"
"$ziran" check --root "$work/ir" "$work/ir/laws.zir" > "$work/saved.json"
cmp "$work/laws.json" "$work/saved.json"

"$ziran" bundle --root "$work" --entry laws:Answer \
    -o "$work/laws-source.zib" "$work/laws.zi"
"$ziran" bundle --root "$work/ir" --entry laws:Answer \
    -o "$work/laws-saved.zib" "$work/ir/laws.zir"
cmp "$work/laws-source.zib" "$work/laws-saved.zib"
test "$("$ziran" run "$work/laws-source.zib")" = 42

cat > "$work/disproved.zi" <<'ZI'
Pair :: struct { a: s64; b: s64; }
Pure :: (a: s32, b: s32) -> s32 { return a + b }
#law CustomFalse custom Pure(1, 1) == 42;
#law EffectWrong effect Pure == mutating;
#law BoundWrong bounds [2]s32 >= 8;
#law SizeWrong size Pair <= 8;
#program_export
Answer :: () -> s32 { return 1 }
ZI
if "$ziran" check --root "$work" "$work/disproved.zi" \
    > "$work/disproved.json" 2> "$work/disproved.err"; then
    echo 'a disproved law passed the gate' >&2
    exit 1
fi
rg -q '"law":"CustomFalse".*"status":"disproved"' "$work/disproved.json"
rg -q '"law":"EffectWrong".*"status":"disproved".*not mutating' "$work/disproved.json"
rg -q '"law":"BoundWrong".*"status":"disproved".*bound 2 >= 8' "$work/disproved.json"
rg -q '"law":"SizeWrong".*"status":"disproved".*16 bytes <= 8' "$work/disproved.json"
rg -q 'law CustomFalse is disproved' "$work/disproved.err"
rg -q 'law EffectWrong is disproved' "$work/disproved.err"
rg -q 'law BoundWrong is disproved' "$work/disproved.err"
rg -q 'law SizeWrong is disproved' "$work/disproved.err"

cat > "$work/unknown.zi" <<'ZI'
host_api :: #system_library "host_api";
Extern :: () -> s32 #foreign host_api;
#law EffectForeign effect Extern;
#program_export
Answer :: () -> s32 { return 2 }
ZI
if "$ziran" check --root "$work" "$work/unknown.zi" \
    > "$work/unknown.json" 2> "$work/unknown.err"; then
    echo 'an unwaived unknown law passed the gate' >&2
    exit 1
fi
rg -q '"status":"unknown"' "$work/unknown.json"

cat > "$work/waived.zi" <<'ZI'
host_api :: #system_library "host_api";
Extern :: () -> s32 #foreign host_api;
#law EffectForeign effect Extern;
#law_waive EffectForeign "host bridge pending review";
#program_export
Answer :: () -> s32 { return 7 }
ZI
"$ziran" check --root "$work" "$work/waived.zi" > "$work/waived.json"
rg -q '"kind":"waiver".*"status":"waived".*host bridge pending review' \
    "$work/waived.json"
rg -q '"status":"unknown"' "$work/waived.json"

"$ziran" ir --root "$work" -o "$work/wir" "$work/waived.zi"
"$ziran" bundle --root "$work" --entry waived:Answer \
    -o "$work/waived-source.zib" "$work/waived.zi"
"$ziran" bundle --root "$work/wir" --entry waived:Answer \
    -o "$work/waived-saved.zib" "$work/wir/waived.zir"
cmp "$work/waived-source.zib" "$work/waived-saved.zib"
test "$("$ziran" run "$work/waived-source.zib")" = 7

cat > "$work/forall.zi" <<'ZI'
Kind :: enum { A, B, C }
Step :: struct { attempt: s32; delay: s32; }
Next :: (k: Kind, n: s32) -> Step {
    if k == Kind.B {
        return Step.{n + 1, 15}
    }
    return Step.{n, 0}
}
Delay :: (k: Kind, n: s32) -> s32 {
    return Next(k, n).delay
}
#law Bounded forall k: Kind, n: 0..4 => Delay(k, n) <= 15;
#law OnlyBWaits forall k: Kind, n: 0..4 => Next(k, n) == Step.{n, 0} || k == Kind.B;
#law BWaits forall n: 0..4 => Next(Kind.B, n) == Step.{n + 1, 15};
#program_export
Answer :: () -> s32 { return 3 }
ZI
"$ziran" check --root "$work" "$work/forall.zi" > "$work/forall.json"
rg -q '"law":"Bounded","kind":"forall".*"status":"proved".*held for all 15 cases' "$work/forall.json"
rg -q '"law":"OnlyBWaits".*"status":"proved"' "$work/forall.json"
rg -q '"law":"BWaits".*"status":"proved".*held for all 5 cases' "$work/forall.json"
"$ziran" ir --root "$work" -o "$work/fir" "$work/forall.zi"
"$ziran" check --root "$work/fir" "$work/fir/forall.zir" > "$work/forall-saved.json"
cmp "$work/forall.json" "$work/forall-saved.json"

cat > "$work/forall_bad.zi" <<'ZI'
Kind :: enum { A, B }
Next :: (k: Kind, n: s32) -> s32 {
    if k == Kind.B {
        return n + 1
    }
    return n
}
Big :: 2000000
#law Wrong forall k: Kind, n: 0..3 => Next(k, n) == n;
#law TooBig forall n: 0..Big => Next(Kind.A, n) == n;
#law Outside forall n: 0..1 => Missing(n) == 0;
#program_export
Answer :: () -> s32 { return 1 }
ZI
if "$ziran" check --root "$work" "$work/forall_bad.zi" \
    > "$work/forall_bad.json" 2> "$work/forall_bad.err"; then
    echo 'a failing forall law passed the gate' >&2
    exit 1
fi
rg -q '"law":"Wrong".*"status":"disproved".*counterexample k=1, n=0' \
    "$work/forall_bad.json"
rg -q '"law":"TooBig".*"status":"unknown".*budget' "$work/forall_bad.json"
rg -q '"law":"Outside".*"status":"unknown"' "$work/forall_bad.json"

cat > "$work/custom_enum.zi" <<'ZI'
Code :: enum { Ok :: 0; Failed :: 6; }
#law WireOk custom cast(s32) Code.Ok == 0;
#law WireFailed custom cast(s32) Code.Failed == 6;
#law WireWrong custom cast(s32) Code.Failed == 7;
#program_export
Answer :: () -> s32 { return 1 }
ZI
if "$ziran" check --root "$work" "$work/custom_enum.zi" \
    > "$work/custom_enum.json" 2> /dev/null; then
    echo 'a wrong enum custom law passed the gate' >&2
    exit 1
fi
rg -q '"law":"WireOk".*"status":"proved"' "$work/custom_enum.json"
rg -q '"law":"WireFailed".*"status":"proved"' "$work/custom_enum.json"
rg -q '"law":"WireWrong".*"status":"disproved"' "$work/custom_enum.json"
