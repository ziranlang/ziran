#!/bin/sh
set -eu

tools=$(dirname "$1")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/packages.zi" <<'ZI'
strings :: #system_library "go:strings";
bits :: #system_library "go:math/bits";
Trim :: (value: string) -> string #foreign strings "TrimSpace";
Upper :: (value: string) -> string #foreign strings "ToUpper";
Join :: (values: []string, separator: string) -> string #foreign strings "Join";
Reverse :: (value: u32) -> u32 #foreign bits "Reverse32";
Answer :: () -> string {
    if Reverse(cast(u32)1) != cast(u32)2147483648 {
        return ""
    }
    parts: [2]string = .[Upper(Trim("  ziran\t")), "Go"]
    return Join(parts[:], ":")
}
ZI
"$tools/zi2zir" --root "$work" -o "$work/ir" "$work/packages.zi"

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/packages.zi
    else
        root=$work/ir
        file=$work/ir/packages.zir
    fi
    out=$work/$input
    "$tools/zi2go" --no-main --pkg main --root "$root" -o "$out" "$file"
    if grep -q 'Host interface' "$out/packages.go"; then
        echo 'explicit Go package was emitted as a host capability' >&2
        exit 1
    fi
    cat > "$out/main.go" <<'GO'
package main
func main() {
    if Packages_Answer() != "ZIRAN:Go" {
        panic("Go package call mismatch")
    }
}
GO
    GO111MODULE=off go run "$out/packages.go" "$out/main.go"
done

for library in 'go:' 'go:bad path' 'go:bad:package'; do
    printf 'library :: #system_library "%s";\nValue :: () -> s32 #foreign library;\n' "$library" > "$work/invalid.zi"
    if "$tools/zi2zir" --check-only --root "$work" "$work/invalid.zi" > "$work/error" 2>&1; then
        echo "invalid Go package accepted: $library" >&2
        exit 1
    fi
    grep -q 'Go extern' "$work/error"
done
echo 'explicit Go package imports: passed'
