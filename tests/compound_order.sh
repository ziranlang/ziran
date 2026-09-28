#!/bin/sh
set -eu

# A compound assignment reads its target before the right side runs, on
# every target: counter += Bump() adds Bump's result to the old counter.
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/order.zi" <<'EOF'
counter: s32 = 1;
Bump :: () -> s32 { counter = 10; return 5; }
#program_export
Order :: () -> s32 {
    counter += Bump();
    local: s32 = 2;
    local *= 3 + 4;
    return counter * 100 + local;
}
EOF
expected=614

"$ziran" bundle --root "$work" --entry order:Order -o "$work/order.zib" "$work/order.zi"
test "$("$ziran" run "$work/order.zib")" = "$expected"

"$ziran" build --target=c --root "$work" -o "$work/c" "$work/order.zi"
cat > "$work/c/main.c" <<'EOF'
#include <stdio.h>
#include "order.h"
int main(void) { printf("%d\n", (int)Order()); return 0; }
EOF
"${CC:-cc}" -std=c99 -Iinclude -I"$work/c" "$work/c/order.c" "$work/c/main.c" -o "$work/c/program"
test "$("$work/c/program")" = "$expected"

"$ziran" build --target=cpp --root "$work" -o "$work/cpp" "$work/order.zi"
cat > "$work/cpp/main.cpp" <<'EOF'
#include <cstdio>
#include "order.hpp"
int main() { std::printf("%d\n", (int)Order()); return 0; }
EOF
"${CXX:-c++}" -std=c++17 -Iinclude -I"$work/cpp" "$work/cpp/order.cpp" "$work/cpp/main.cpp" -o "$work/cpp/program"
test "$("$work/cpp/program")" = "$expected"

"$ziran" build --target=go --pkg main --root "$work" -o "$work/go" "$work/order.zi"
cat > "$work/go/main.go" <<'EOF'
package main
import "fmt"
func main() { fmt.Println(Order_Order()) }
EOF
test "$(GO111MODULE=off go run "$work"/go/*.go)" = "$expected"

"$ziran" build --target=rust --exe --entry order:Order --root "$work" -o "$work/rust" "$work/order.zi"
cargo build --quiet --manifest-path "$work/rust/Cargo.toml"
"$work/rust/target/debug/ziran_generated" && status=0 || status=$?
test "$status" = $((expected % 256))
