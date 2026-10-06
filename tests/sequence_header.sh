#!/bin/sh
# Native-layout sequence headers retain their original place on managed Python.
set -eu
ziran=${1:?pass the ziran command}
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
project_dir=$(CDPATH= cd "$script_dir/.." && pwd)
work=$(mktemp -d "$project_dir/build/sequence-header.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source_file=$script_dir/spec/sequence_header_test.zi
"$ziran" ir --root "$script_dir/spec" --module-path "$project_dir/std" \
    -o "$work/ir" "$source_file"
# -l:FILE is the exact shared-library spelling accepted by native linkers.
# Use the actual installed math library, without requiring a development link.
exact_library=$(python3 -c 'import ctypes.util; print(ctypes.util.find_library("m") or "")')
test -n "$exact_library"
for form in source saved; do
    if test "$form" = source; then
        input=$source_file
        root=$script_dir/spec
    else
        input=$work/ir/sequence_header_test.zir
        root=$work/ir
    fi
    for target in py c cpp; do
        output=$work/$form-$target
        if test "$target" = py; then
            LDLIBS="-l:$exact_library" "$ziran" build --target=py --root "$root" \
                --module-path "$project_dir/std" --entry sequence_header_test:Main \
                --exe -o "$output" "$input"
            python3 "$output"
        else
            "$ziran" build --target="$target" --root "$root" --module-path "$project_dir/std" \
                --entry sequence_header_test:Main --no-main -o "$output" "$input"
            if test "$target" = c; then
                compiler=${CC:-cc}; extension=c; header=h; standard=c11
            else
                compiler=${CXX:-c++}; extension=cpp; header=hpp; standard=c++17
            fi
            printf '#include "sequence_header_test.%s"\nint main(void) { return (int)sequence_header_test_Main(); }\n' \
                "$header" > "$output/driver.$extension"
            "$compiler" -O2 -std="$standard" -I"$project_dir/include" -I"$output" \
                "$output"/*."$extension" -o "$output/probe" -lm
            "$output/probe"
        fi
        echo "$form $target sequence headers passed"
    done
done
cmp "$work/source-py/__main__.py" "$work/saved-py/__main__.py"
