#!/bin/sh
# New(T) allocates a zeroed T and returns *T; free(p) releases it and
# ignores null. A linked list built with New runs the same on C, C++, Go,
# and .zib from source and saved IR. The portable runner stops cleanly on
# a read after free and on a second free. A procedure or foreign procedure
# of the program's own named New or free still wins over the built-in.
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Node :: struct { value: s32; next: *Node; }

Push :: (head: *Node, value: s32) -> *Node {
    node := New(Node)
    node.value = value
    node.next = head
    return node
}

#program_export
Answer :: () -> s32 {
    head: *Node
    i: s32 = 1
    while i <= 5 { head = Push(head, i); i += 1; }
    total: s32 = 0
    cursor := head
    while cursor != null { total += cursor.value; cursor = cursor.next; }
    while head != null {
        next := head.next
        free(head)
        head = next
    }
    free(head)
    count := New(s64)
    if <<count != 0 { return 1 }
    <<count = 27
    result := cast(s32) <<count + total
    free(count)
    return result
}
ZI

cat > "$work/own.zi" <<'ZI'
New :: (value: s32) -> s32 { return value + 1; }
#program_export
Answer :: () -> s32 { return New(41); }
ZI

run_fails() {
    name=$1
    cat > "$work/$name.zi"
    "$ziran" bundle --root "$work" --entry "$name:main" -o "$work/$name.zib" "$work/$name.zi"
    if "$ziran" run "$work/$name.zib" > "$work/$name.out" 2>&1; then
        echo "$name: ran" >&2
        exit 1
    fi
    grep -Fq 'portable execution failed' "$work/$name.out"
}
run_fails after_free <<'ZI'
main :: () { p := New(s32); <<p = 1; free(p); print("%\n", <<p); }
ZI
run_fails twice <<'ZI'
main :: () { p := New(s32); free(p); free(p); }
ZI

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" "$work/$name.zi" 2> "$work/$name.err"; then
        echo "$name: accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err" || { cat "$work/$name.err" >&2; exit 1; }
}
reject not_type 'New takes one type: New(T)' <<'ZI'
main :: () { x: s32 = 1; p := New(x + 1); }
ZI
reject not_pointer 'free takes one pointer from New' <<'ZI'
main :: () { x: s32 = 1; free(x); }
ZI

"$ziran" bundle --root "$work" --entry own:Answer -o "$work/own.zib" "$work/own.zi"
test "$("$ziran" run "$work/own.zib")" = 42

# A foreign free, such as C's, is called as declared.
cat > "$work/foreign.zi" <<'ZI'
c :: #system_library "c";
malloc :: (size: usize) -> *void #foreign c;
free :: (pointer: *void) #foreign c;
#program_export
Answer :: () -> s32 { p := malloc(8); free(p); return 42; }
ZI
"$ziran" check --root "$work" "$work/foreign.zi"

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 42
    output="$work/c-$input"
    "$ziran" build --target=c --exe --entry app:Answer --root "$root" -o "$output" "$module"
    status=0
    "$output/app" || status=$?
    test "$status" = 42
    output="$work/cpp-$input"
    "$ziran" build --target=cpp --root "$root" -o "$output" "$module"
    cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
    "${CXX:-c++}" -I"$repo/include" -I"$output" "$output"/*.cpp -o "$output/app"
    "$output/app"
    output="$work/go-$input"
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("wrong result") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zib" "$work/saved.zib"
