#!/bin/sh
# A unary operator inside a binary one keeps its grouping on every target:
# a ^ -b is a ^ (-b) in C, Go and the portable VM, though Go reads its
# integer negation 0 - b, and - -x stays two negations rather than the C
# token --. Each value is printed, and all targets must agree with the
# expected list.
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/app.zi" <<'ZI'
// Every binary precedence level, with a unary operand on either side.
Ints :: (a: s32, b: s32) {
    print("% % % % % %\n", a * -b, -a * b, a / -b, -a % b, a % -b, -a * -b)
    print("% % % % % %\n", a << 1 + -b * 0, -a << 2, ~a << 2, -a >> 1, ~a >> 1, a >> -(-1))
    print("% % % % % %\n", a + -b, a - -b, -a - -b, a - ~b, ~a + b, -(-b) - a)
    print("% % % % % %\n", a ^ -b, -a ^ b, a | -b, -a | b, a & -b, ~a & b)
    print("% % % %\n", a ^ ~b, ~(~a) | b, a & ~b, -a & ~b)
    print("% % % % % %\n", -a < b, a < -b, ~a == -a - 1, -a != b, -b >= a, ~b <= a)
    print("% % % %\n", a + -b * a, -a * -b + -a, a - -b * -a, -(a + b) * -b)
}

Wide :: (a: s64, b: u32) {
    print("% % % % %\n", a ^ -a, ~a & 255, a | -1 - a, b ^ ~b, ~b & cast(u32)a)
}

Reals :: (x: float64) {
    print("% % % % %\n", -x * 2.0, x - -x, -(-x), x * -x, -x < x)
    y: float32 = -2.5
    print("% % %\n", -(-y) + y, -y * -y, y - -(-y))
}

Logic :: (c: bool, d: bool) {
    print("% % % % %\n", !c && d, c || !d, !(c && d), !c == !d, !(!c) || d)
}

main :: () {
    Ints(5, 1)
    Ints(-7, 3)
    Wide(-1234567890123, 7)
    Reals(1.5)
    Logic(true, false)
}
ZI

cat > "$work/expected" <<'OUT'
-5 -5 -5 0 0 5
10 -20 -24 -3 -3 2
4 6 -4 7 -5 -4
-6 -6 -1 -5 5 0
-5 5 4 -6
true false true true false true
0 0 0 6
21 21 2 1 -1 -21
-14 28 24 3 3 -4
-10 -4 10 -3 9 10
4 4 -3 7 -7 2
5 -5 -8 4
false true true true true false
14 -14 14 -12
-2 202 -1 4294967295 2382691120
-3 3 1.5 -2.25 true
-5 6.25 0
false true true false true
OUT

"$ziran" build --target=c --exe --entry app:main --root "$work" -o "$work/c" "$work/app.zi"
"$work/c/app" > "$work/c.out"
"$ziran" build --target=go --pkg main --exe --entry app:main --root "$work" -o "$work/go" "$work/app.zi"
GO111MODULE=off go build -o "$work/go/app" "$work/go"/*.go
"$work/go/app" > "$work/go.out"
"$ziran" bundle --root "$work" --entry app:main -o "$work/app.zx" "$work/app.zi"
"$ziran" run "$work/app.zx" > "$work/zx.out"
"$(dirname "$ziran")/zi2zx" run --vm "$work/app.zx" > "$work/vm.out"
for target in c go zx vm; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "unary operators grouped wrongly on $target:" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
echo "unary_precedence: C, Go, native and the VM agree"
