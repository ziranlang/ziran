#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/strings.zi" <<'ZI'
Escaped :: "<footer class=\"status\" aria-label=\"Users: %d; Storage: %d/%d GB\">";
Backslashes :: "path\\\"; trailing\\";
Raw :: #string END
"quoted"; text (name) {value}
END;
Value :: () -> string {
    return Escaped
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/strings.zi"
for input in source saved; do
    root=$work
    module=$root/strings.zi
    if test "$input" = saved; then
        root=$work/ir
        module=$root/strings.zir
    fi
    output=$work/$input
    "$ziran" build --target=go --no-main --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/entry.go" <<'GO'
package main

func main() {
    want := "<footer class=\"status\" aria-label=\"Users: %d; Storage: %d/%d GB\">"
    if Escaped != want || Strings_Value() != want {
        panic("native constants and checked string values disagree")
    }
    if Backslashes != "path\\\"; trailing\\" {
        panic("escaped backslash and quote changed native constant text")
    }
    if Raw != "\"quoted\"; text (name) {value}\n" {
        panic("raw constant text passed through expression translation")
    }
}
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source/strings.go" "$work/saved/strings.go"
echo 'Go constant strings retain escaped quotes, backslashes and punctuation through saved IR: passed'
