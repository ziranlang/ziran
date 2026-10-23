#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root tests/spec --module-path std tests/spec/std_string.zi
"$ziran" bundle --root tests/spec --module-path std \
    --entry std_string:main -o "$work/source.zx" tests/spec/std_string.zi
"$ziran" ir --root tests/spec --module-path std \
    -o "$work/ir" tests/spec/std_string.zi
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry std_string:main -o "$work/saved.zx" "$work/ir/std_string.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

"$ziran" build --target=c --root tests/spec --module-path std \
    -o "$work/c" tests/spec/std_string.zi
${CC:-cc} -std=c11 -Iinclude -I"$work/c" "$work/c"/*.c -o "$work/c/app"
"$work/c/app"

"$ziran" build --target=cpp --root tests/spec --module-path std \
    -o "$work/cpp" tests/spec/std_string.zi
${CXX:-c++} -std=c++17 -Iinclude -I"$work/cpp" "$work/cpp"/*.cpp -o "$work/cpp/app"
"$work/cpp/app"

"$ziran" build --target=go --pkg main --root tests/spec \
    --module-path std -o "$work/go" tests/spec/std_string.zi
cat > "$work/go/main.go" <<'GO'
package main
func main() { if StdString_Main() != 0 { panic("wrong string range") } }
GO
GO111MODULE=off go run "$work/go"/*.go

"$ziran" bundle --root tests/spec --entry std_string_bad:main \
    -o "$work/bad.zx" tests/spec/std_string_bad.zi
if "$ziran" run "$work/bad.zx" 2> "$work/bad.err"; then
    echo 'out-of-range string slice unexpectedly ran' >&2
    exit 1
fi
grep -Fq 'portable execution failed' "$work/bad.err"
