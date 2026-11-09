#!/bin/sh
set -eu
ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$ziran" in /*) ;; *) ziran="$root/$ziran" ;; esac
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/toml-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
input="$root/tests/spec/toml_document_test.zi"
"$ziran" ir --root "$root/tests/spec" --module-path "$root/std" -o "$work/ir" "$input"
for form in source saved; do
    if test "$form" = saved; then input="$work/ir/toml_document_test.zir"; fi
    "$ziran" bundle --root "$root/tests/spec" --module-path "$root/std" --module-path "$work/ir" \
        --entry toml_document_test:SelfTest -o "$work/$form.zx" "$input"
    test "$("$ziran" run "$work/$form.zx")" = 42
    "$ziran" build --target=c --exe --root "$root/tests/spec" --module-path "$root/std" --module-path "$work/ir" \
        --entry toml_document_test:NativeMain -o "$work/$form-c" "$input"
    "$work/$form-c/toml_document_test"
    "$ziran" build --target=go --pkg main --root "$root/tests/spec" --module-path "$root/std" --module-path "$work/ir" \
        --entry toml_document_test:NativeMain -o "$work/$form-go" "$input"
    cat > "$work/$form-go/main.go" <<'GO'
package main
func main() { if TomlDocumentTest_NativeMain() != 0 { panic("TOML reading differs") } }
GO
    GO111MODULE=off go run "$work/$form-go"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'TOML reading passed source, saved IR, portable VM and C/Go execution'
# Exercise the actual package reader, including mixed inline and full tables.
cat > "$work/valid.toml" <<'TOML'
[package]
'name' = "native-manifest"
links = ["gio-2.0", "gdk_pixbuf-2.0"]
module_roots = [
  'src',
  "generated",
]
[toolchain]
git = 'https://github.com/ziranlang/ziran.git'
[dependencies]
kryon = {git = "https://github.com/kryonlabs/kryon.git", ref = "feature#text", module_aliases = {'Entry' = "Renamed"}}
[dependencies.'helper']
git = "https://codeberg.org/waozi/helper.git"
source = true
[exports]
'Entry' = "src/\u0045ntry.zi"
TOML
cat > "$work/invalid.toml" <<'TOML'
[package]
name = "native-manifest"
[unrelated]
invalid = [1 2]
TOML
"$ziran" build --target=c --exe --root "$root/tests/spec" --module-path "$root/cmd" --module-path "$root/std" \
    --entry package_manifest_test:main -o "$work/package-c" "$root/tests/spec/package_manifest_test.zi"
"$work/package-c/package_manifest_test" "$work/valid.toml" "$work/invalid.toml"
echo 'Package manifest reader passed quoted keys, escapes, tables, inline dependencies and invalid-input rejection'
