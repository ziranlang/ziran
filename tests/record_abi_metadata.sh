#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "$repo/build/test/record_abi_metadata.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir "$work/src"
cat > "$work/src/record_abi.zi" <<'EOF'
ForeignRecord :: struct {
    #abi_incomplete
    value: s32
}

#program_export
ReadForeignRecord :: () -> s32 {
    record: ForeignRecord
    record.value = 42
    return record.value
}
EOF

"$ziran" check --root "$work/src" "$work/src/record_abi.zi"
"$ziran" ir --root "$work/src" -o "$work/ir" \
    "$work/src/record_abi.zi"
test -s "$work/ir/record_abi.zir"

"$ziran" build --target=c --root "$work/src" -o "$work/c" \
    "$work/src/record_abi.zi"
"$ziran" build --target=cpp --root "$work/src" -o "$work/cpp" \
    "$work/src/record_abi.zi"
"$ziran" build --target=c --root "$work/ir" -o "$work/c-from-ir" \
    "$work/ir/record_abi.zir"
cmp "$work/c/record_abi.h" "$work/c-from-ir/record_abi.h"
cmp "$work/c/record_abi.c" "$work/c-from-ir/record_abi.c"

rg -q '^typedef struct ForeignRecord ForeignRecord;$' \
    "$work/c/record_abi.h"
rg -q '^#pragma ziran abi_incomplete ForeignRecord$' \
    "$work/c/record_abi.h"
awk '
    /^typedef struct ForeignRecord ForeignRecord;$/ { forward = 1 }
    /^#pragma ziran abi_incomplete ForeignRecord$/ {
        marker = 1
        if(!forward) exit 1
    }
    /^struct ForeignRecord \{$/ { if(!marker) exit 1; found = 1 }
    END { exit found ? 0 : 1 }
' "$work/c/record_abi.h"

cat > "$work/c/main.c" <<'EOF'
#include "record_abi.h"
int main(void) { return ReadForeignRecord() == 42 ? 0 : 1; }
EOF
cat > "$work/cpp/main.cpp" <<'EOF'
#include "record_abi.hpp"
int main() { return ReadForeignRecord() == 42 ? 0 : 1; }
EOF
"${CC:-cc}" -std=c11 -I"$work/c" "$work/c"/*.c "$work/c/main.c" \
    -o "$work/c/runner"
"$work/c/runner"
"${CXX:-c++}" -std=c++11 -I"$work/cpp" "$work/cpp"/*.cpp \
    "$work/cpp/main.cpp" -o "$work/cpp/runner"
"$work/cpp/runner"

"$ziran" build --target=plan9-c --root "$work/src" \
    -o "$work/plan9" "$work/src/record_abi.zi"
rg -q '^#pragma incomplete ForeignRecord$' "$work/plan9/record_abi.h"
if rg -q '#pragma ziran abi_incomplete' "$work/plan9"/*; then
    echo 'plan9-c retained generic ABI metadata' >&2
    exit 1
fi
awk '
    /^typedef struct ForeignRecord ForeignRecord;$/ { forward = 1 }
    /^#pragma incomplete ForeignRecord$/ {
        marker = 1
        if(!forward) exit 1
    }
    /^struct ForeignRecord \{$/ { if(!marker) exit 1; found = 1 }
    END { exit found ? 0 : 1 }
' "$work/plan9/record_abi.h"
