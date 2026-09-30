#!/bin/sh
# Python native imports: #system_library "py:module" calls Python functions,
# methods on the first argument, and dotted attributes on the Python target.
# string crosses as str, []u8 as bytes, integers wrap to their width,
# procedures become callables, #py_field reads and writes attributes, and
# #py_results catches exceptions as text or as the exception object.
# Source and saved IR agree; every other target rejects py: natives.
set -eu

ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

command -v python3 >/dev/null 2>&1 || {
    echo 'python3 is required to test the Python target' >&2
    exit 1
}

cat > "$work/native.zi" <<'ZI'
#import "py_types"
builtins :: #system_library "py:builtins";
json :: #system_library "py:json";
math :: #system_library "py:math";
sys :: #system_library "py:sys";
types :: #system_library "py:types";

Loads :: (text: string) -> Object #foreign json "loads";
Dumps :: (value: Object) -> string #foreign json "dumps";
Sqrt :: (value: float64) -> float64 #foreign math "sqrt";
Power :: (base: s64, exponent: s64) -> s32 #foreign builtins "pow";
Upper :: (text: string) -> string #foreign builtins "(str).upper";
Length :: (value: Object) -> s64 #foreign builtins "len";
Item :: (value: Object, key: string) -> Object #foreign builtins "(dict).__getitem__";
Text :: (value: Object) -> string #foreign builtins "str";
Is :: (value: Object, kind: Object) -> bool #foreign builtins "isinstance";
DictType :: () -> Object #py_field #foreign builtins "dict";
Hex :: (data: []u8) -> string #foreign builtins "(bytes).hex";
FromHex :: (text: string) -> []u8 #foreign builtins "bytes.fromhex";
Major :: () -> s32 #py_field #foreign sys "version_info.major";
Namespace :: () -> Object #foreign types "SimpleNamespace";
SetName :: (object: Object, value: string) #py_field #foreign types "(SimpleNamespace).name";
Name :: (object: Object) -> string #py_field #foreign types "(SimpleNamespace).name";
Mapper :: #type (text: string) -> string;
Map :: (function: Mapper, items: Object) -> Object #foreign builtins "map";
Joined :: (separator: string, items: Object) -> string #foreign builtins "(str).join";

Loaded :: struct { value: Object; error: string }
TryLoads :: (text: string) -> Loaded #py_results #foreign json "loads";
Raised :: struct { error: Object }
TryRoot :: (value: float64) -> Raised #py_results #foreign math "sqrt";
ErrorArguments :: (error: Object) -> Object #py_field #foreign builtins "(BaseException).args";

Shout :: (text: string) -> string { return Upper(text) }

#program_export
main :: () -> s32 {
    data := Loads("{\"name\": \"café\", \"list\": [1, 2, 3]}")
    if Text(Item(data, "name")) != "café" { return 1 }
    if Length(Item(data, "list")) != 3 || !Is(data, DictType()) { return 2 }
    if Sqrt(16.0) != 4.0 || Upper("abc") != "ABC" { return 3 }
    if Power(2, 40) != 0 || Power(2, 31) != -2147483648 { return 4 }
    bad := TryLoads("{nope")
    if bad.value != null || bad.error[0:17] != "JSONDecodeError: " { return 5 }
    good := TryLoads("[1]")
    if good.error != "" || Length(good.value) != 1 { return 6 }
    raised := TryRoot(-1.0)
    if raised.error == null || Length(ErrorArguments(raised.error)) != 1 { return 7 }
    if TryRoot(4.0).error != null { return 8 }
    if Joined("-", Map(Shout, Loads("[\"a\", \"b\"]"))) != "A-B" { return 9 }
    item := Namespace()
    SetName(item, "zi")
    if Name(item) != "zi" { return 10 }
    bytes: [3]u8 = .[1, 171, 255]
    if Hex(bytes[:]) != "01abff" { return 11 }
    back := FromHex("01abff")
    if back.count != 3 || back[1] != cast(u8)171 { return 12 }
    if Dumps(Loads("[true, null]")) != "[true, null]" || Major() != 3 { return 13 }
    return 0
}
ZI

"$ziran" check --root "$work" --module-path std "$work/native.zi"
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/native.zi
        root=$work
    else
        file=$work/ir/native.zir
        root=$work/ir
    fi
    "$ziran" build --target=py --entry native:main --root "$root" \
        --module-path std --exe -o "$work/$input" "$file"
    python3 "$work/$input"
done
cmp "$work/source/__main__.py" "$work/saved/__main__.py"

# Python natives exist only on the Python target.
for target in c cpp go rust; do
    if "$ziran" build --target=$target --entry native:main --root "$work" \
        --module-path std -o "$work/out-$target" "$work/native.zi" \
        2> "$work/$target.err"; then
        echo "the $target target accepted py: natives" >&2
        exit 1
    fi
    grep -Fq 'require the Python target' "$work/$target.err"
done

reject() {
    name=$1
    message=$2
    cat > "$work/$name.zi"
    if "$ziran" check --root "$work" --module-path std "$work/$name.zi" \
        2> "$work/$name.err"; then
        echo "$name: accepted" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/$name.err"
}
reject field_on_go '#py_field requires a py: foreign target' <<'ZI'
strings :: #system_library "go:strings";
Upper :: (text: string) -> string #py_field #foreign strings "ToUpper";
ZI
reject results_without_record '#py_results requires a record' <<'ZI'
json :: #system_library "py:json";
Loads :: (text: string) -> string #py_results #foreign json "loads";
ZI
reject results_error_last '#py_results requires a record' <<'ZI'
json :: #system_library "py:json";
Swapped :: struct { error: string; value: s64 }
Loads :: (text: string) -> Swapped #py_results #foreign json "loads";
ZI
reject variadic 'not ..any' <<'ZI'
builtins :: #system_library "py:builtins";
Print :: (values: ..any) #foreign builtins "print";
ZI
reject record_parameter 'Python foreign parameters must be' <<'ZI'
builtins :: #system_library "py:builtins";
Point :: struct { x: s32 }
Show :: (point: Point) -> string #foreign builtins "str";
ZI
reject method_receiver 'receiver as the first parameter' <<'ZI'
builtins :: #system_library "py:builtins";
Upper :: () -> string #foreign builtins "(str).upper";
ZI
reject pointer_receiver 'Python symbol must be' <<'ZI'
builtins :: #system_library "py:builtins";
Upper :: (text: string) -> string #foreign builtins "(*str).upper";
ZI
reject field_setter_result '#py_field reads an attribute' <<'ZI'
types :: #system_library "py:types";
#import "py_types"
SetName :: (object: Object, value: string) -> string #py_field #foreign types "(SimpleNamespace).name";
ZI
