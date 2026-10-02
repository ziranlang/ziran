#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/test"
work=$(mktemp -d "$repo/build/test/private-import.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
cat > "$work/value.zi" <<'ZI'
Cell :: struct { value: s32; }
Read :: (cell: Cell) -> s32 { return cell.value }
ZI
cat > "$work/main.zi" <<'ZI'
#scope_file
Value :: #import "value";
#scope_export
#program_export
Answer :: () -> s32 {
    cell: Value.Cell
    cell.value = 42
    opaque := cast(*void)*cell
    restored := cast(*Value.Cell)opaque
    return Value.Read(restored.*)
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/main.zi"
for input in source saved; do
    source_root=$work
    source=$work/main.zi
    if test "$input" = saved; then source_root=$work/ir; source=$work/ir/main.zir; fi
    for target in c cpp plan9-c; do
        out=$work/$input-$target
        "$ziran" build --target="$target" --root "$source_root" -o "$out" "$source"
        extension=c
        header=h
        if test "$target" = cpp; then extension=cpp; header=hpp; fi
        rg -Fq "#include \"value.$header\"" "$out/main.$extension"
        if rg -Fq "#include \"value.$header\"" "$out/main.$header"; then
            echo 'implementation-only import leaked into the public header' >&2
            exit 1
        fi
        if test "$target" = c; then
            printf '#include "main.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$out/driver.c"
            "${CC:-cc}" -std=c11 -Werror -I"$out" "$out"/*.c -o "$out/run"
            "$out/run"
        elif test "$target" = cpp; then
            printf '#include "main.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$out/driver.cpp"
            "${CXX:-c++}" -std=c++17 -Werror -I"$out" "$out"/*.cpp -o "$out/run"
            "$out/run"
        fi
    done
done

# A private source import still supplies types used in an exported native
# interface. Private records and procedure types need their dependencies
# before the module's own header, under its private guard.
cat > "$work/secret.zi" <<'ZI'
SecretCell :: struct { value: s32; }
ZI
cat > "$work/unused.zi" <<'ZI'
Value :: () -> s32 { return 1 }
ZI
cat > "$work/api_more.zi" <<'ZI'
#scope_export
Count :: (cell: Value.Cell) -> s32 { return cell.value }
ZI
cat > "$work/api.zi" <<'ZI'
#scope_file
Value :: #import "value";
Unused :: #import "unused";
#scope_export
Holder :: struct {
    cell: Value.Cell
    cells: [2]Value.Cell
    pointer: *Value.Cell
}
Handler :: #type (cell: Value.Cell) -> Value.Cell #c_call;
Make :: () -> Holder {
    result: Holder
    result.cell.value = 40
    result.cells[1].value = Unused.Value()
    return result
}
// Module-scope imports remain available to later loaded declarations.
#scope_module
ValueForLoad :: #import "value";
#load "api_more.zi";
ZI
# The loaded public signature must use its own module-scoped alias.
sed -i 's/Value.Cell/ValueForLoad.Cell/g' "$work/api_more.zi"
cat > "$work/internal.zi" <<'ZI'
#scope_module
Secret :: #import "secret";
PrivateHandler :: #type (cell: Secret.SecretCell) -> Secret.SecretCell #c_call;
Private :: struct { cell: Secret.SecretCell; handler: PrivateHandler; }
#scope_export
Read :: () -> s32 {
    state: Private
    state.cell.value = 1
    return state.cell.value
}
ZI
cat > "$work/interface.zi" <<'ZI'
API :: #import "api";
Internal :: #import "internal";
#program_export
Answer :: () -> s32 {
    holder := API.Make()
    return API.Count(holder.cell) + holder.cells[1].value + Internal.Read()
}
ZI
"$ziran" ir --root "$work" -o "$work/interface-ir" "$work/interface.zi"
for input in source saved; do
    source_root=$work
    source=$work/interface.zi
    if test "$input" = saved; then source_root=$work/interface-ir; source=$source_root/interface.zir; fi
    for target in c cpp plan9-c; do
        out=$work/interface-$input-$target
        "$ziran" build --target="$target" --root "$source_root" \
            --entry interface:Answer -o "$out" "$source"
        extension=c
        header=h
        if test "$target" = cpp; then extension=cpp; header=hpp; fi
        rg -Fq "#include \"value.$header\"" "$out/api.$header"
        if rg -Fq "#include \"unused.$header\"" "$out/api.$header"; then
            echo 'implementation-only import leaked beside interface types' >&2
            exit 1
        fi
        if test "$target" != plan9-c; then
            printf '#include "interface.%s"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' "$header" > "$out/driver.$extension"
            compiler=${CC:-cc}
            standard=c11
            if test "$target" = cpp; then compiler=${CXX:-c++}; standard=c++17; fi
            "$compiler" -std="$standard" -Werror -I"$out" "$out"/*."$extension" -o "$out/run"
            "$out/run"
            # Consumers of the public header cannot see private slot types.
            printf '#include "internal.%s"\nPrivateHandler leaked;\n' "$header" > "$out/leak.$extension"
            if "$compiler" -std="$standard" -Werror -I"$out" -fsyntax-only "$out/leak.$extension" >"$out/leak.log" 2>&1; then
                echo 'private procedure type leaked into the public header' >&2
                exit 1
            fi
        fi
    done
done
echo 'private imports and native interface types: C, C++ and Plan 9 C source/saved IR passed'
