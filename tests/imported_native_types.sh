set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Names :: union {
    label: *u8 #c "const char *"
    note: *u8 #c "const char *"
}
Tagged :: struct {
    kind: s32
    plain: *u8 #c "const char *"
    using names: Names #c "union"
}
ZI
cat > "$work/app.zi" <<'ZI'
B :: #import "lib";

Length :: (text: *u8) -> s64 {
    count: s64 = 0
    while text[count] != #char "\0" { count += 1 }
    return count
}

#program_export
Read :: (items: *B.Tagged, at: s32) -> s64 {
    promoted := items[at].label
    direct := items[at].plain
    if promoted == null || direct == null { return 0 }
    return Length(items[at].note) + Length(items[at].plain)
}

Holder :: struct { items: [3]*B.Tagged; }

#program_export
Copy :: (holder: *Holder, other: *Holder) -> s32 {
    list: [3]*B.Tagged = holder.items
    other.items = list
    again := other.items
    if again[0] == null { return 0 }
    return 1
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    module=$work/app.zi
    root=$work
    if test "$input" = saved; then
        module=$work/ir/app.zir
        root=$work/ir
    fi
    output=$work/$input-c
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) {
    Tagged values[3] = {0};
    Holder original = {0}, copy = {0};
    if (Read(values, 0) != 0 || Copy(&original, &copy) != 0) return 1;
    values[0].label = "abc";
    values[0].plain = "hello";
    original.items[0] = values;
    if (Read(values, 0) != 8 || Copy(&original, &copy) != 1) return 2;
    return copy.items[0] == values ? 0 : 3;
}
C
    "${CC:-cc}" -std=c11 -Werror=discarded-qualifiers \
        -Werror=pointer-sign -Werror=incompatible-pointer-types \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
done

echo 'imported native types: C reads and arrays use the declaring module'
