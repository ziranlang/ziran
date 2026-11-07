#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
bin=$(dirname "$ziran")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

# `#c "SPELLING"` makes a generated C header match C code that shares the
# records: a field's C type, an array's element, an enum's constant prefix.
cat > "$work/spell.zi" <<'EOF'
Kind :: enum s32 #c "SPELL_KIND_" { Ident; Call :: 4; Index }
Flag :: enum u8 #specified #c "SPELL_" { First :: 1; Second :: 2 }

LABEL_MAX :: 6

Node :: struct {
    name: *u8 #c "const char *"
    op: [4]u8 #c "char"
    label: [LABEL_MAX]u8 #c "char"  // checking writes [6]; the spelling stays
    kind: Kind
    count: s64
}

Pair :: struct { left: Node; right: Node; }


Pick :: (pair: Pair) -> Kind {
    return ifx pair.left.kind == Kind.Call then pair.right.kind else Kind.Index
}

#program_export
Weight :: (node: *Node, flag: Flag) -> s32 {
    if node.kind == Kind.Call && flag == Flag.Second { return 40 + cast(s32) node.count }
    pair: Pair
    pair.left.kind = Kind.Call
    pair.right.kind = Kind.Ident
    return cast(s32) Pick(pair)
}

#program_export
Answer :: () -> s32 {
    node: Node
    node.kind = Kind.Call
    node.count = 2
    return Weight(*node, Flag.Second) + Weight(*node, Flag.First)
}
EOF
sed 's/ #c "[^"]*"//' "$work/spell.zi" > "$work/plain.zi"
mkdir "$work/plain-root"
cp "$work/plain.zi" "$work/plain-root/spell.zi"

"$bin/zi2c" --no-main --root "$work" -o "$work/c" "$work/spell.zi"
header=$work/c/spell.h
for line in 'const char * name;' 'char op[4];' 'char label[6];' 'SPELL_KIND_Ident = 0,' \
    'SPELL_KIND_Call = 4,' 'SPELL_KIND_Index = 5,' 'SPELL_First = 1,'; do
    grep -qF "$line" "$header" || { echo "missing in C header: $line" >&2; exit 1; }
done
if grep -q 'Kind_\|Flag_' "$header" "$work/c/spell.c"; then
    echo 'C kept a default enum spelling' >&2
    exit 1
fi
cat > "$work/c/main.c" <<'EOF'
#include "spell.h"
#include <string.h>
int main(void) {
    const char *text = "call";
    Node node = {.name = text, .kind = SPELL_KIND_Call, .count = 2};
    char *op = node.op;
    memcpy(op, "+=", 3);
    return Weight(&node, SPELL_Second) == 42 &&
        Weight(&node, SPELL_First) == SPELL_KIND_Ident &&
        SPELL_KIND_Index == 5 && strcmp(node.name, "call") == 0 ? 0 : 1;
}
EOF
${CC:-cc} -std=c11 -Werror=discarded-qualifiers -Werror=pointer-sign \
    -Werror=incompatible-pointer-types -Iinclude -I"$work/c" \
    "$work/c/spell.c" "$work/c/main.c" -o "$work/c/app"
"$work/c/app"

# A `using` union field spelled "union" is also an anonymous C member, so C
# reads tagged.label where Ziran's code reads tagged.names.label. Go unions
# take scalars only, so this part is C alone.
mkdir "$work/anon"
cat > "$work/anon/anon.zi" <<'EOF'
Names :: union {
    label: *u8 #c "const char *"
    note: *u8 #c "const char *"
}
Tagged :: struct {
    kind: s32
    using names: Names #c "union"
}

#program_export
Labelled :: (tagged: *Tagged) -> bool { return tagged.label != null }
EOF
"$bin/zi2c" --no-main --root "$work/anon" -o "$work/anon/c" "$work/anon/anon.zi"
for line in '    union {' '        Names names;' '            const char * label;'; do
    grep -qxF "$line" "$work/anon/c/anon.h" || { echo "missing in C header: $line" >&2; exit 1; }
done
cat > "$work/anon/c/main.c" <<'EOF'
#include "anon.h"
int main(void) {
    const char *text = "label";
    Tagged tagged = {.kind = 1};
    tagged.label = text;
    return Labelled(&tagged) && tagged.names.note == text ? 0 : 1;
}
EOF
${CC:-cc} -std=c11 -Werror=discarded-qualifiers -Iinclude -I"$work/anon/c" \
    "$work/anon/c/anon.c" "$work/anon/c/main.c" -o "$work/anon/app"
"$work/anon/app"

# Saved IR keeps the spellings.
"$ziran" ir --root "$work" -o "$work/ir" "$work/spell.zi"
"$bin/zi2c" --no-main --root "$work/ir" -o "$work/saved" "$work/ir/spell.zir"
cmp "$header" "$work/saved/spell.h"
cmp "$work/c/spell.c" "$work/saved/spell.c"

# Go ignores them; a .zx bundle carries them in its saved IR and runs alike.
"$bin/zi2go" --root "$work" -o "$work/go" "$work/spell.zi"
"$bin/zi2go" --root "$work/plain-root" -o "$work/go-plain" "$work/plain-root/spell.zi"
diff -r "$work/go" "$work/go-plain"
"$ziran" bundle --root "$work" --entry spell:Answer -o "$work/spell.zx" "$work/spell.zi"
"$ziran" bundle --root "$work/plain-root" --entry spell:Answer \
    -o "$work/plain.zx" "$work/plain-root/spell.zi"
"$ziran" bundle --root "$work/ir" --entry spell:Answer \
    -o "$work/saved.zx" "$work/ir/spell.zir"
cmp "$work/spell.zx" "$work/saved.zx"
test "$("$ziran" run "$work/spell.zx")" = 42
test "$("$ziran" run "$work/plain.zx")" = 42

# A spelling is one C type or identifier prefix, and an enum's goes last.
reject() {
    printf '%s\n' "$2" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" 2> "$work/bad.err"; then
        echo "accepted: $1" >&2
        exit 1
    fi
}
reject 'statement in a field spelling' 'Bad :: struct { name: *u8 #c "char *; int"; }'
reject 'empty field spelling' 'Bad :: struct { name: *u8 #c ""; }'
reject 'unquoted field spelling' 'Bad :: struct { name: *u8 #c char; }'
reject 'spaced enum prefix' 'Bad :: enum s32 #c "BAD X" { One }'
reject 'digit enum prefix' 'Bad :: enum s32 #c "1BAD" { One }'
reject 'enum prefix before #specified' 'Bad :: enum s32 #c "BAD_" #specified { One :: 1 }'
printf 'Names :: union { a: s32; }\nBad :: struct { names: Names #c "union"; }\n' > "$work/bad.zi"
if "$bin/zi2c" --no-main --root "$work" -o "$work/bad" "$work/bad.zi" 2> "$work/bad.err"; then
    echo 'accepted: anonymous member without using' >&2
    exit 1
fi
grep -q 'requires a using field' "$work/bad.err"
echo 'C spellings: header, C callers, saved IR, Go and .zx agreement passed'
