#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/tags.zi" <<'ZI'
Record :: struct {
    id: string #go_tag "json:\"user_id\""
    label: string #go_tag "json:\"label,omitempty\" yaml:\"label,omitempty\""
    count: s64 #go_tag "json:\"count,string\""
    hidden: string #go_tag "json:\"-\""
    extra: string #go_tag "note:\"};#abi_incomplete,\\\"\\\\\""
}
Inline :: struct { value: s32 #go_tag "json:\"semi;colon\""; other: s32 }
Box :: struct($T: Type) { value: T #go_tag "json:\"box,omitempty\""; }
Number :: Box(s32)

#program_export
Answer :: () -> s32 {
    number: Number
    number.value = 42
    return number.value
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/tags.zi"
for input in source saved; do
    root=$work
    file=$work/tags.zi
    if test "$input" = saved; then
        root=$work/ir
        file=$work/ir/tags.zir
    fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "encoding/json"
    "reflect"
)

func main() {
    record := Record{ID: "account", Hidden: "secret"}
    data, err := json.Marshal(record)
    if err != nil || string(data) != `{"user_id":"account","count":"0","Extra":""}` {
        panic(string(data))
    }
    var decoded Record
    if err := json.Unmarshal([]byte(`{"user_id":"new","label":"name","count":"42"}`), &decoded); err != nil {
        panic(err)
    }
    if decoded.ID != "new" || decoded.Label != "name" || decoded.Count != 42 || decoded.Hidden != "" {
        panic("field tag decoding")
    }
    field, _ := reflect.TypeOf(record).FieldByName("Label")
    if field.Tag.Get("yaml") != "label,omitempty" {
        panic("multiple tag keys")
    }
    field, _ = reflect.TypeOf(record).FieldByName("Extra")
    if field.Tag.Get("note") != "};#abi_incomplete,\"\\" {
        panic(string(field.Tag))
    }
    data, err = json.Marshal(Inline{Value: 2, Other: 3})
    if err != nil || string(data) != `{"semi;colon":2,"Other":3}` {
        panic(string(data))
    }
    data, err = json.Marshal(Number{Value: 42})
    if err != nil || string(data) != `{"box":42}` || Tags_Answer() != 42 {
        panic(string(data))
    }
}
GO
    GO111MODULE=off go run "$out"/*.go
    "$ziran" bundle --root "$root" --entry tags:Answer -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp; do
        native=$work/$input-$target
        "$ziran" build "--target=$target" --no-main --root "$root" \
            -o "$native" "$file"
        if test "$target" = c; then
            printf '#include "tags.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$native/main.c"
            ${CC:-cc} -Iinclude -I"$native" "$native"/*.c -o "$native/test"
        else
            printf '#include "tags.hpp"\nint main() { return Answer() == 42 ? 0 : 1; }\n' > "$native/main.cpp"
            ${CXX:-c++} -Iinclude -I"$native" "$native"/*.cpp -o "$native/test"
        fi
        "$native/test"
    done
done
cmp "$work/source-go/tags.go" "$work/saved-go/tags.go"

for field in 'value: s32 #go_tag' 'value: s32 #go_tag 42' \
    'value: s32 #go_tag "first" #go_tag "second"' \
    'value: s32 #unknown "json"' 'value: s32 #go_tag "\q"' \
    'value: s32 #go_tag "\0"'; do
    printf 'Bad :: struct { %s }\n' "$field" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" \
        > "$work/bad.out" 2> "$work/bad.err"; then
        echo "invalid Go tag was accepted: $field" >&2
        exit 1
    fi
done
printf 'Bad :: union { value: s32 #go_tag "json" }\n' > "$work/bad.zi"
if "$ziran" check --root "$work" "$work/bad.zi" \
    > "$work/bad.out" 2> "$work/bad.err"; then
    echo 'union field tag was accepted' >&2
    exit 1
fi
echo 'Go field tags, JSON behavior, and saved IR: passed'
