#!/bin/sh
set -eu

# for loops come out as the target's own loops: a counting for in C and Go,
# range in Go when the index goes unused. continue, named continue,
# reverse ranges, and strings behave the same on every target.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/loops.zi" <<'EOF'
Sum :: (values: []s32) -> s32 {
    total: s32 = 0;
    for values { total += it; }
    return total;
}
main :: () {
    for step: 0..3 {
        if step == 1 { continue; }
        print("step %\n", step);
    }
    for < down: 1..3 { print("down %\n", down); }
    numbers := s32.[4, 5, 6];
    print("sum %\n", Sum(numbers[0:3]));
    for outer: numbers {
        for inner: 0..2 {
            if inner == 1 { continue outer; }
            print("pair % %\n", outer, inner);
        }
    }
    for number, position: numbers {
        print("% at %\n", number, position);
    }
    count := 0;
    for "héllo" { count += 1; }
    print("bytes %\n", count);
}
EOF
cat > "$work/expected" <<'EOF'
step 0
step 2
step 3
down 3
down 2
down 1
sum 15
pair 4 0
pair 5 0
pair 6 0
4 at 0
5 at 1
6 at 2
bytes 6
EOF

"$ziran" bundle --root "$work" --entry loops:main -o "$work/loops.zx" "$work/loops.zi"
"$ziran" run "$work/loops.zx" > "$work/vm.out"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/loops.zi"
printf '#include "loops.h"\nint main(void) { loops_main(); return 0; }\n' > "$work/c/run.c"
"${CC:-cc}" -std=c99 -I"$work/c" "$work/c/loops.c" "$work/c/run.c" -o "$work/c/program"
"$work/c/program" > "$work/c.out"
grep -Fq 'for (int64_t step = 0LL; step <= 3LL; step++) {' "$work/c/loops.c"

"$ziran" build --target=go --pkg main --exe --entry loops:main --root "$work" \
    -o "$work/go" "$work/loops.zi"
(cd "$work/go" && GO111MODULE=off go run .) > "$work/go.out"
grep -Fq 'for step := int64(0); step <= 3; step++ {' "$work/go/loops.go"
grep -Fq 'for down := int64(3); down >= 1; down-- {' "$work/go/loops.go"
grep -Fq 'for _, it := range values {' "$work/go/loops.go"

for target in vm c go; do
    if ! cmp -s "$work/expected" "$work/$target.out"; then
        echo "$target loops differ" >&2
        diff "$work/expected" "$work/$target.out" >&2 || true
        exit 1
    fi
done
