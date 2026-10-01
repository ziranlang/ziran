#!/bin/sh
set -eu
ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/strings.zi" <<'ZI'
#import "std/go_types"
Reference :: struct { value: *string }
Equal :: (value: *string, expected: string) -> bool { return <<value == expected }
Box :: (value: *string) -> Any { return value }
Pass :: (value: *string) -> *string { return value }
EqualRecord :: (value: Reference, expected: string) -> bool { return <<value.value == expected }
ZI
"$ziran" ir --root "$work" --module-path "std=$repo/std" -o "$work/ir" "$work/strings.zi"
for input in "$work/strings.zi" "$work/ir/strings.zir"; do
    "$ziran" build --target=go --pkg main --root "$work" --module-path "std=$repo/std" -o "$work/go" "$input"
    cat > "$work/go/main.go" <<'GO'
package main
import "encoding/json"
func main() {
    for _, original := range []string{"", "日本語", "a\x00b", string([]byte{0xff,0x80})} {
        value := original
        if !Strings_Equal(&value, original) || Strings_Pass(&value) != &value || Strings_Box(&value) != any(&value) {
            panic("string pointer identity")
        }
        reference := Reference{Value: &value}
        if err := json.Unmarshal([]byte(`"decoded"`),Strings_Box(&value)); err != nil || value != "decoded" {
            panic("native pointer destination")
        }
        if !Strings_EqualRecord(reference, "decoded") { panic("shared string mutation") }
    }
    if Strings_Pass(nil) != nil { panic("nil string pointer") }
}
GO
    gofmt -w "$work/go"/*.go
    GO111MODULE=off go run "$work/go"/*.go
done
