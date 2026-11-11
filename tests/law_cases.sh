#!/bin/sh
# A forall law's condition is prepared once and each procedure call it makes
# is kept for the law's later cases. Kept calls must give the same results,
# counterexamples and evaluator limits (depth and fuel) as evaluating every
# case afresh; these results were recorded before calls were kept.
set -eu
unset DISPLAY WAYLAND_DISPLAY
ziran=${1:?pass ziran}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/law-cases-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/cases.zi" <<'ZI'
Mode :: enum { Off, Slow, Fast }
Twice :: (x: s64) -> s64 { return x + x }
Steps :: (n: s64) -> s64 {
    if n <= 0 { return 0 }
    return 1 + Steps(n - 1)
}
Speed :: (mode: Mode) -> s64 {
    if mode == .Fast { return 2 }
    if mode == .Slow { return 1 }
    return 0
}
Total :: (values: [3]s64) -> s64 { return values[0] + values[1] + values[2] }
Spread :: (n: s64) -> s64 {
    if n <= 0 { return 1 }
    return Spread(n - 1) + Spread(n - 1) + Spread(n - 1) + Spread(n - 1)
}
Scaled :: (x: s64, by: s64 = 3) -> s64 { return Twice(x) * by }
Limit :: 7
Near :: Twice(Limit)

#law TwiceRepeats forall x: 0..20,
    y: 0..20 =>
    Twice(x % 3) + Twice(y % 3) == 2 * (x % 3 + y % 3);
#law TwiceBreaks forall x: 0..20 => Twice(x % 4) < 6;
#law ScaledDefault forall x: 0..9 => Scaled(x % 2) == Scaled(x % 2, 3) && Scaled(x % 2, by = 1) == Twice(x % 2);
#law NearConstant forall x: 0..9 => Near + Twice(x) == Twice(Limit + x);
#law SpeedOrder forall mode: Mode => Speed(mode) == cast(s64) mode;
#law SpeedBreaks forall mode: Mode => Speed(mode) < 2;
#law StepsCount forall n: 0..6 => Steps(n) == n && Steps(n) == Steps(n);
#law StepsKeptShallow custom Steps(1) == 1 && Steps(7) == 7;
#law StepsReachKept forall n: 5..7 => Steps(1) == 1 && Steps(n) == n;
#law StepsDeep forall n: 0..12 => Steps(Steps(n)) == n;
#law TotalRepeats forall a: 0..3, b: 0..3 => Total(s64.[a, b, a]) == Total(s64.[b, a, a]);
#law TotalBreaks forall a: 0..3, b: 0..3 => Total(s64.[a, b, a]) < 7;
#law SpreadFits forall n: 0..3 => Spread(n) == Spread(n) && Spread(n) > 0;
#law SpreadSpends forall n: 0..6 => Spread(n) > 0;
#program_export
Answer :: () -> s32 { return 0 }
ZI
if "$ziran" check --root "$work" "$work/cases.zi" > "$work/laws.json" 2> "$work/laws.err"; then
    echo 'disproved and unknown laws were accepted' >&2; exit 1
fi
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/law_assertions.zi" -- "$ziran" "$work" cases
cat > "$work/expected" <<'EXPECTED'
TwiceRepeats | proved | 441 | held for all 441 cases
TwiceBreaks | disproved | 4 | counterexample x=3
ScaledDefault | proved | 10 | held for all 10 cases
NearConstant | proved | 10 | held for all 10 cases
SpeedOrder | proved | 3 | held for all 3 cases
SpeedBreaks | disproved | 3 | counterexample mode=2
StepsCount | proved | 7 | held for all 7 cases
StepsKeptShallow | unknown | 0 | payload is outside the compile-time evaluator
StepsReachKept | unknown | 2 | condition is outside the compile-time evaluator at n=7
StepsDeep | unknown | 7 | condition is outside the compile-time evaluator at n=7
TotalRepeats | proved | 16 | held for all 16 cases
TotalBreaks | disproved | 12 | counterexample a=2, b=3
SpreadFits | proved | 4 | held for all 4 cases
SpreadSpends | unknown | 5 | condition is outside the compile-time evaluator at n=5
EXPECTED
diff -u "$work/expected" "$work/results"
echo 'Kept law calls give the same results as fresh evaluation'
