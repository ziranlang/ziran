#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${1:-"$repo/build/bin/ziran"}
work=${ENUM_ZERO_ARRAY_BUILD:-"$repo/build/enum-zero-arrays"}
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
cat > "$work/zero_arrays.zi" <<'ZI'
Mode :: enum { Idle :: 0; Running :: 7; }
Narrow :: enum u8 { First :: 4; Last :: 200; }
Wide :: enum u64 { First :: 4294967296; }
Row :: struct { mode: Mode; narrow: Narrow; number: s32; value: string; }
modes: [4]Mode;
narrow: [3][2]Narrow;
wide: [2]Wide;
rows: [2]Row;
names: [2]string;
given: [2]s32 = .{7, -3};
#program_export
Main :: () -> s32 {
    local: [3]Mode;
    local_rows: [2]Row;
    for i: 0..3 { if cast(s32)modes[i] != 0 { return 1 } }
    for i: 0..2 {
        if cast(s32)local[i] != 0 { return 2 }
        for j: 0..1 { if cast(u8)narrow[i][j] != cast(u8)0 { return 3 } }
    }
    for i: 0..1 {
        if cast(u64)wide[i] != cast(u64)0 || cast(s32)rows[i].mode != 0 ||
            cast(u8)rows[i].narrow != cast(u8)0 || rows[i].number != 0 || rows[i].value != "" ||
            cast(s32)local_rows[i].mode != 0 || local_rows[i].value != "" ||
            names[i] != "" { return 4 }
    }
    if given[0] != 7 || given[1] != -3 { return 5 }
    modes[1] = cast(Mode)Mode.Running; local[0] = modes[1]; rows[0].mode = local[0]
    if rows[0].mode != cast(Mode)Mode.Running || cast(s32)modes[2] != 0 { return 6 }
    return 0
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/zero_arrays.zi"
for form in source saved; do
    input=$work/zero_arrays.zi; module_root=$work
    if test "$form" = saved; then input=$work/ir/zero_arrays.zir; module_root=$work/ir; fi
    "$ziran" bundle --root "$module_root" --module-path "$module_root" \
        --entry zero_arrays:Main -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 0
    for target in ${ENUM_ZERO_ARRAY_TARGETS:-c cpp go rust py}; do
        output=$work/$form-$target
        executable=--exe; package=
        if test "$target" = cpp; then executable=; fi
        if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --root "$module_root" --module-path "$module_root" \
            --target="$target" --entry zero_arrays:Main $executable $package \
            -o "$output" "$input"
        if test "$target" = cpp; then
            printf '#include "zero_arrays.hpp"\nint main() { return Main(); }\n' > "$output/main.cpp"
            "${CXX:-c++}" -O2 -std=c++17 -I"$repo/include" -I"$output" "$output"/*.cpp \
                -lm -o "$output/zero_arrays"
        fi
        case "$target" in
            go) GO111MODULE=off go run "$output"/*.go;;
            py) python3 "$output";;
            rust) CARGO_TARGET_DIR="$work/rust-$form" cargo run --offline --quiet \
                --manifest-path "$output/Cargo.toml";;
            *) "$output/zero_arrays";;
        esac
    done
done
printf 'Enum and record zero arrays passed source and saved targets\n'
