#!/bin/sh
# A call returning an array writes its result straight into the array a
# declaration creates or a plain assignment names, without a temporary and a
# copy in C. The callee writes its result only when it returns, so reading
# the destination during the call, through an argument or a global, still
# sees the old value. A return passes the caller's destination on the same
# way, and returns any other array with one copy from its own storage.
# Results agree on C, Go and the portable VM from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx-vm"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
shared: [4]s64;
Holder :: struct { values: [4]s64; grid: [2][4]s64; tag: s64; }
Make :: (n: s64) -> [4]s64 {
    out: [4]s64
    for i: 0..3 { out[i] = n + i }
    return out
}
// Reads its argument after building part of the result.
Reverse :: (values: [4]s64) -> [4]s64 {
    out: [4]s64
    for i: 0..3 { out[i] = values[3 - i] }
    return out
}
// Reads the global it is assigned to while it runs.
FromShared :: (n: s64) -> [4]s64 {
    out: [4]s64
    for i: 0..3 { out[i] = shared[i] + n }
    shared[0] = 100
    return out
}
Early :: (n: s64) -> [4]s64 {
    if n < 0 { return Make(0) }
    return Make(n)
}
First :: (values: [4]s64) -> s64 { return values[0] }
// Returns: a call passes the caller's destination on; a parameter, a
// global, a field or a row is copied once.
Wrap :: (n: s64) -> [4]s64 { return Make(n + 1) }
ViaShared :: (n: s64) -> [4]s64 { return FromShared(n) }
Pass :: (values: [4]s64) -> [4]s64 { return values }
Global :: () -> [4]s64 { return shared }
Field :: (holder: Holder) -> [4]s64 { return holder.values }
Row :: (holder: Holder, at: s64) -> [4]s64 { return holder.grid[at] }
#program_export
Answer :: () -> s64 {
    fresh := Make(1)
    typed: [4]s64 = Make(2)
    if fresh[3] != 4 || typed[0] != 2 { return 1 }
    typed = Reverse(typed)
    if typed[0] != 5 || typed[3] != 2 { return 2 }
    holder: Holder
    holder.values = Make(10)
    holder.grid[1] = Make(20)
    if holder.values[2] != 12 || holder.grid[1][3] != 23 || holder.grid[0][0] != 0 { return 3 }
    shared = Make(1)
    shared = FromShared(5)
    if shared[0] != 6 || shared[3] != 9 { return 4 }
    nested := Make(First(Make(7)))
    if nested[1] != 8 { return 5 }
    negative := Early(-1)
    if negative[3] != 3 || Early(4)[0] != 4 { return 6 }
    // The new binding's name in the argument reads the outer value.
    outer := Make(30)
    {
        outer := Reverse(outer)
        if outer[0] != 33 { return 7 }
    }
    if outer[0] != 30 { return 8 }
    wrapped := Wrap(1)
    if wrapped[0] != 2 || wrapped[3] != 5 { return 9 }
    shared = Make(1)
    shared = ViaShared(5)
    if shared[0] != 6 || shared[3] != 9 { return 10 }
    typed = Pass(typed)
    if typed[0] != 5 || typed[3] != 2 { return 11 }
    copied := Global()
    copied[1] = 0
    if copied[0] != 6 || shared[1] != 7 { return 12 }
    if Field(holder)[2] != 12 || Row(holder, 1)[3] != 23 || Row(holder, 0)[3] != 0 { return 13 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    sed -n '/^Answer(/,/^}/p' "$output/app.c" > "$work/answer.c"
    grep -q '^    app_Make(fresh, 1LL);' "$work/answer.c"
    # The by-value argument is copied; the result lands in typed directly.
    grep -q '^    app_Reverse(typed, value_[0-9]*);' "$work/answer.c"
    grep -q '^    app_Make(holder.values, 10LL);' "$work/answer.c"
    grep -q 'app_FromShared(shared, 5LL);' "$work/answer.c"
    # No result temporary is copied into a destination afterwards.
    if grep -q 'memmove(\(fresh\|typed\|nested\|negative\|shared\|holder\.values\),' "$work/answer.c"; then
        echo 'an array call result was still copied into its destination' >&2
        exit 1
    fi
    # Returns write the caller's destination without a result temporary.
    sed -n '/^app_Early(/,/^}/p; /^app_Wrap(/,/^}/p; /^app_ViaShared(/,/^}/p;
            /^app_Make(/,/^}/p; /^app_Pass(/,/^}/p; /^app_Global(/,/^}/p;
            /^app_Field(/,/^}/p; /^app_Row(/,/^}/p' "$output/app.c" > "$work/returns.c"
    if grep -q 'value_[0-9]' "$work/returns.c"; then
        echo 'an array return still went through a temporary' >&2
        exit 1
    fi
    grep -q '^        app_Make(array_result_0_0, 0LL);' "$work/returns.c"
    grep -q '^    app_Make(array_result_0_0, n);' "$work/returns.c"
    grep -q '^    app_FromShared(array_result_0_0, n);' "$work/returns.c"
    grep -q 'memmove(array_result_0_0, out, sizeof(int64_t\[4\]));' "$work/returns.c"
    grep -q 'memmove(array_result_0_0, values, sizeof(int64_t\[4\]));' "$work/returns.c"
    grep -q 'memmove(array_result_0_0, shared, sizeof(int64_t\[4\]));' "$work/returns.c"
    # Pass also copies its by-value parameter in.
    test "$(grep -c 'memmove' "$work/returns.c")" = 6

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("array result in place") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
