#!/bin/sh
# A host may switch LC_NUMERIC to a decimal-comma locale after startup; GTK
# adopts the user's locale. Portable float literals, global initializers and
# float text must keep using '.' there, from source and saved IR alike.
set -eu

ziran=${1:?pass the ziran command}
vm_test=${2:?pass the VM locale test executable}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/numbers.zi" <<'ZI'
factor: float64 = 0.25;

Label :: (value: float64) -> string {
    line: Vec(u8)
    BuilderPrint(*line, "%", value)
    return BuilderFinish(line)
}

#program_export
Check :: () -> s32 {
    half: float64 = 0.5
    if half * 4.0 != 2.0 { return 1 }
    small: float32 = 1.25
    if small + 0.75 != 2.0 { return 2 }
    if factor * 8.0 != 2.0 { return 3 }
    if Label(1.5) != "1.5" { return 4 }
    if Label(0.0625) != "0.0625" { return 5 }
    return 0
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/std" \
    -o "$work/ir" "$work/numbers.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/numbers.zi
        root=$work
    else
        module=$work/ir/numbers.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$repo/std" \
        --entry numbers:Check -o "$work/$input.zib" "$module"
    "$vm_test" "$work/$input.zib"
done
