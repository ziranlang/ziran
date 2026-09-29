#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/shared.zi" <<'ZI'
#import "map_go"
Entry :: struct { label: string }
Entries :: Map(string, Entry)
Counts :: Map(string, isize)
NewCounts :: () -> Counts {
    result: Counts
    MapSet(result, "zero", cast(isize)0)
    MapSet(result, "answer", cast(isize)42)
    return result
}
ReadEntry :: (values: Entries) -> Entry {
    return MapGet(values, "entry")
}
NewEntries :: () -> Entries {
    result: Entries
    MapSet(result, "entry", Entry.{label = "foreign"})
    return result
}
ZI
cat > "$work/maps.zi" <<'ZI'
#import "map_go"
#import "go_types"
#import "option"
shared :: #import "shared";
Entry :: struct { other: s64 }
Counts :: Map(string, isize)
Counters :: struct { values: Counts }
Node :: struct { children: Map(string, Node); number: s64 }
Tables :: Map(string, []Map(string, Any))
Response :: struct {
    tables: Tables #go_tag "json:\"tables\""
    counts: Map(string, isize) #go_tag "json:\"counts\""
}
Counter :: (values: Counts, key: string) -> isize {
    found := MapLookup(values, key)
    if found.has_value {
        return found.value
    }
    return cast(isize)-1
}
ReadMissing :: () -> isize {
    values: Counts
    return MapGet(values, "missing")
}
Modify :: (values: Counts) {
    MapSet(values, "answer", cast(isize)43)
    MapDelete(values, "zero")
}
Increment :: (counters: *Counters) {
    MapSet(counters.values, "answer", MapGet(counters.values, "answer") + cast(isize)1)
}
GetFromOther :: (values: shared.Entries) -> shared.Entry {
    return MapGet(values, "entry")
}
LookupFromOther :: (values: shared.Entries) -> shared.Entry {
    found := MapLookup(values, "entry")
    return found.value
}
RecordKey :: () -> s64 {
    values: Map(Entry, s64)
    key: Entry
    key.other = 5
    MapSet(values, key, cast(s64)6)
    return MapGet(values, key)
}
Keys :: (values: Counts) -> []string {
    return MapKeys(values)
}
Lookup :: (values: Counts, key: string) -> bool {
    return MapContains(values, key)
}
Reset :: (values: Counts) {
    MapClear(values)
}
Zero :: () -> Counts {
    values: Counts
    return values
}
Null :: () -> Counts {
    return null
}
IsNull :: (values: Counts) -> bool {
    return values == null && null == values
}
Tree :: () -> Node {
    result: Node
    child: Node
    child.number = 7
    MapSet(result.children, "child", child)
    return result
}
Empty :: () -> Counts {
    values: Counts
    MapInit(values)
    return values
}
Count :: (values: Counts) -> s64 {
    return MapCount(values)
}
BuildResponse :: () -> Response {
    counts: Counts = shared.NewCounts()
    row: Map(string, Any)
    MapSet(row, "label", "雪")
    MapSet(row, "number", cast(s64)42)
    MapSet(row, "missing", null)
    rows: [1]Map(string, Any)
    rows[0] = row
    response: Response
    MapSet(response.tables, "data", rows[:])
    response.counts = counts
    return response
}
#program_export
Answer :: () -> s32 {
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/maps.zi"
for input in source saved; do
    root=$work
    file=$work/maps.zi
    if test "$input" = saved; then
        root=$work/ir
        file=$work/ir/maps.zir
    fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main
import (
    "encoding/json"
    "reflect"
    "sort"
)
func main() {
    response := Maps_BuildResponse()
    data, err := json.Marshal(response)
    if err != nil || string(data) != `{"tables":{"data":[{"label":"雪","missing":null,"number":42}]},"counts":{"answer":42,"zero":0}}` {
        panic(string(data))
    }
    if Maps_Counter(response.Counts, "zero") != 0 || Maps_Counter(response.Counts, "missing") != -1 || Maps_ReadMissing() != 0 {
        panic("presence/zero distinction changed")
    }
    keys := Maps_Keys(response.Counts)
    sort.Strings(keys)
    if !reflect.DeepEqual(keys, []string{"answer", "zero"}) || !Maps_Lookup(response.Counts, "zero") || Maps_Lookup(response.Counts, "missing") {
        panic("key snapshot or membership changed")
    }
    Maps_Modify(response.Counts)
    if response.Counts["answer"] != 43 || len(response.Counts) != 1 || Maps_Count(response.Counts) != 1 {
        panic("map copy lost shared storage")
    }
    counters := Counters{}
    Maps_Increment(&counters)
    Maps_Increment(&counters)
    if counters.Values["answer"] != 2 {
        panic("in-place map field initialization")
    }
    entries := Shared_NewEntries()
    if Maps_GetFromOther(entries).Label != "foreign" || Maps_LookupFromOther(entries).Label != "foreign" || Maps_RecordKey() != 6 {
        panic("record types lost their declaring module or comparability")
    }
    Maps_Reset(response.Counts)
    if len(response.Counts) != 0 || response.Counts == nil || Maps_Zero() != nil || Maps_Null() != nil || Maps_Empty() == nil || Maps_Count(nil) != 0 || !Maps_IsNull(nil) || Maps_IsNull(response.Counts) {
        panic("nil/empty/clear semantics changed")
    }
    if Maps_Tree().Children["child"].Number != 7 {
        panic("recursive map value layout changed")
    }
    field, _ := reflect.TypeOf(response).FieldByName("Tables")
    if field.Type != reflect.TypeOf(map[string][]map[string]any{}) {
        panic(field.Type.String())
    }
}
GO
    GO111MODULE=off go run "$out"/*.go
    for target in c cpp rust py; do
        if "$ziran" build "--target=$target" --root "$root" --module-path std -o "$work/rejected-$target" "$file" > "$work/rejected.out" 2>&1; then
            echo "maps accepted by $target" >&2
            exit 1
        fi
        grep -Eq 'maps require the Go target|foreign Go types require the Go target' "$work/rejected.out" || { cat "$work/rejected.out"; exit 1; }
    done
    if "$ziran" bundle --root "$root" --module-path std --entry maps:Count -o "$work/rejected.zib" "$file" > "$work/rejected.out" 2>&1; then
        echo 'map entered a portable ABI' >&2
        exit 1
    fi
    "$ziran" bundle --root "$root" --module-path std --entry maps:Answer -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/maps.go" "$work/saved-go/maps.go"
for key in '[]u8' 'Map(string, s64)' 'Any' 'void' '*Unknown'; do
    printf '#import "map_go";\n#import "go_types";\nBad :: Map(%s, s64)\n' "$key" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "uncomparable map key accepted: $key" >&2
        exit 1
    fi
done
for body in 'Bad :: Map(string, Vec(u8))' \
    'Box :: struct { value: Vec(u8) }; Bad :: Map(string, Box)' \
    'Bad :: () { values: Map(string, Any); owned: Vec(u8); MapSet(values, "x", owned) }'; do
    printf '#import "map_go";\n#import "go_types";\n#import "vec";\n%s\n' "$body" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "owned storage escaped into a map: $body" >&2
        exit 1
    fi
done
for body in 'MapSet(values, "x", "wrong")' 'MapGet(values, cast(s64)42)' \
    'MapGet(values)' 'MapSet(values, "x", cast(s64)1, cast(s64)2)' \
    'values.key = "private"' 'other := Map(string, s64).{}' \
    'size := size_of(Map(string, s64))' 'equal := values == values' \
    'less := values < values'; do
    printf '#import "map_go";\nBad :: () {\nvalues: Map(string, s64)\n%s\n}\n' "$body" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid map operation accepted: $body" >&2
        exit 1
    fi
done
cat > "$work/parallel_bad.zi" <<'ZI'
#import "map_go"
Bad :: () {
    values: Map(string, s64)
    #parallel
    for index: 0..4 {
        MapSet(values, "x", index)
    }
}
ZI
if "$ziran" check --root "$work" --module-path std "$work/parallel_bad.zi" > "$work/bad.out" 2>&1; then
    echo 'parallel map mutation accepted' >&2
    exit 1
fi
grep -q 'parallel' "$work/bad.out"
echo 'Go maps, shared storage, typed operations, nested JSON and saved IR: passed'
