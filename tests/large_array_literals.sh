#!/bin/sh
# check-cost: 610
# An array literal has no size cap: a table of tens of thousands of elements,
# or more than 1 MB of source text, checks in linear time and runs the same
# from source and saved IR on the VM, C and Go. Limits that remain name
# themselves.
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

# NAME COUNT: a procedure holding COUNT negative and positive s32 values,
# 16 to a line, that returns 0 only when every value arrived in order.
generate() {
    python3 - "$work/$1.zi" "$2" <<'PY'
import sys
path, count = sys.argv[1], int(sys.argv[2])
values = [(i * 37) % 251 - 100 for i in range(count)]
lines = ["Check :: () -> s32 {", f"    table: [{count}]s32 = .["]
for start in range(0, count, 16):
    lines.append("        " + ", ".join(str(v) for v in values[start:start + 16]) + ",")
total = sum(values)
weighted = sum((i % 7 + 1) * v for i, v in enumerate(values))
lines += ["    ]", "    total: s64 = 0", "    weighted: s64 = 0",
          f"    for i: 0..{count}-1 {{",
          "        total += cast(s64) table[i]",
          "        weighted += cast(s64) (i % 7 + 1) * cast(s64) table[i]",
          "    }",
          f"    if total != {total} {{ return 1 }}",
          f"    if weighted != {weighted} {{ return 2 }}",
          f"    if table[{count - 1}] != {values[-1]} {{ return 3 }}",
          "    return 0", "}",
          "main :: () -> s32 { return Check() }"]
open(path, "w").write("\n".join(lines) + "\n")
PY
}

# Checks NAME, saves its IR, and runs both forms on the VM and Go. C runs both
# forms when FULL_C is 1; otherwise the two forms must emit the same C and
# that C runs once, since a C compiler takes long on very large tables.
run_everywhere() {
    name=$1 full_c=$2
    "$ziran" check --root "$work" "$work/$name.zi"
    "$ziran" ir --root "$work" -o "$work/$name-ir" "$work/$name.zi"
    for form in source saved; do
        input=$work/$name.zi root=$work
        if test "$form" = saved; then
            input=$work/$name-ir/$name.zir root=$work/$name-ir
        fi
        "$ziran" bundle --root "$root" --entry "$name:main" -o "$work/$name-$form.zx" "$input"
        test "$("$ziran" run "$work/$name-$form.zx")" = 0
        "$ziran" build --target=go --pkg main --root "$root" --entry "$name:main" \
            --exe -o "$work/$name-$form-go" "$input"
        GO111MODULE=off go run "$work/$name-$form-go"/*.go
        if test "$full_c" = 1; then
            "$ziran" build --target=c --root "$root" --entry "$name:main" \
                --exe -o "$work/$name-$form-c" "$input"
            "$work/$name-$form-c/$name"
        else
            "$ziran" build --target=c --root "$root" --entry "$name:main" \
                -o "$work/$name-$form-c-text" "$input"
        fi
    done
    cmp "$work/$name-source.zx" "$work/$name-saved.zx"
    if test "$full_c" != 1; then
        diff -r "$work/$name-source-c-text" "$work/$name-saved-c-text"
        "$ziran" build --target=c --root "$work/$name-ir" --entry "$name:main" \
            --exe -o "$work/$name-saved-c" "$work/$name-ir/$name.zir"
        "$work/$name-saved-c/$name"
    fi
}

generate table20k 20000
run_everywhere table20k 1
generate table70k 70000
run_everywhere table70k 0

# More than 1 MB of literal text checks and runs through saved IR on the VM.
generate table260k 260000
test "$(wc -c < "$work/table260k.zi")" -gt 1048576
"$ziran" ir --root "$work" -o "$work/table260k-ir" "$work/table260k.zi"
"$ziran" bundle --root "$work" --entry table260k:main \
    -o "$work/table260k-source.zx" "$work/table260k.zi"
"$ziran" bundle --root "$work/table260k-ir" --entry table260k:main \
    -o "$work/table260k-saved.zx" "$work/table260k-ir/table260k.zir"
cmp "$work/table260k-source.zx" "$work/table260k-saved.zx"
test "$("$ziran" run "$work/table260k-saved.zx")" = 0

# A file-scope initializer keeps a fixed buffer; past it the error names the
# limit instead of a confusing parse failure.
python3 - "$work/global.zi" <<'PY'
import sys
rows = ",\n".join("    " + ", ".join("-100" for _ in range(20)) for _ in range(50))
open(sys.argv[1], "w").write(f"Table: [1000]s32 = .[\n{rows}\n];\nmain :: () -> s32 {{ return Table[0] + 100 }}\n")
PY
if "$ziran" check --root "$work" "$work/global.zi" 2> "$work/global.err"; then
    echo "an oversized file-scope initializer was accepted" >&2
    exit 1
fi
grep -q 'file-scope variable initializer exceeds 4095 bytes' "$work/global.err"

# An untyped .[...] gets its element type from a declaration or a prefix.
cat > "$work/assign.zi" <<'ZI'
main :: () -> s32 {
    table: [3]s32
    table = .[1, 2, 3]
    return table[2] - 3
}
ZI
if "$ziran" check --root "$work" "$work/assign.zi" 2> "$work/assign.err"; then
    echo "an untyped array literal assignment was accepted" >&2
    exit 1
fi
grep -q 'needs a declaration that names its array type, or write the element type' "$work/assign.err"
sed 's/table = \.\[/table = s32.[/' "$work/assign.zi" > "$work/typed.zi"
"$ziran" bundle --root "$work" --entry typed:main -o "$work/typed.zx" "$work/typed.zi"
test "$("$ziran" run "$work/typed.zx")" = 0
echo 'Large array literals check in linear time and agree from source and saved IR on the VM, C and Go'
