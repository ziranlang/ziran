#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
cat > "$work/floats.zi" <<'ZI'
ZERO :: 0;
Cell :: struct { value: float64; }
Check32 :: (value: float32) -> bool {
    return +value<0 && 0>value && value<=-2 && -2>=value && value==-2 && -2==value &&
        value!=ZERO && ZERO!=value && value>=-3 && -3<=value && value>-3 && -3<value &&
        value+1==-1 && 1+value==-1 && value-1==-3 && 1-value==3 &&
        value*2==-4 && 2*value==-4 && value/2==-1 && 2/value==-1
}
Check64 :: (value: float64) -> bool {
    cell: Cell; cell.value=value
    return +cell.value<0 && 0>cell.value && value<=-2 && -2>=value && value==-2 && -2==value &&
        value!=ZERO && ZERO!=value && value>=-3 && -3<=value && value>-3 && -3<value &&
        value+1==-1 && 1+value==-1 && value-1==-3 && 1-value==3 &&
        value*2==-4 && 2*value==-4 && value/2==-1 && 2/value==-1
}
#program_export
Hex :: (value: float64) -> bool { return value==0xff && 0xff==value && value>+0xfe }
Wide :: (value: float64) -> bool { return value==-2147483649 && value>-0x80000002 && -0x80000000>value }
#program_export
Answer :: () -> s32 { return ifx Check32(-2.0) && Check64(-2.0) && Hex(255.0) && Wide(-2147483649.0) then 42 else 1 }
#program_export
Main :: () -> s32 { return ifx Answer()==42 then 0 else 1 }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/floats.zi"
for form in source saved; do
    root=$work; input=$work/floats.zi
    if test "$form" = saved; then root=$work/ir; input=$root/floats.zir; fi
    "$ziran" bundle --root "$root" --entry floats:Answer -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
    output=$work/$form-rust
    "$ziran" build --target=rust --exe --root "$root" --entry floats:Main -o "$output" "$input"
    CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --offline --manifest-path "$output/Cargo.toml"
    "$work/rust-target/debug/ziran_generated"
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Float32/float64 comparisons and arithmetic with integer literals passed source/saved Rust and VM'
