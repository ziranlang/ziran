#!/bin/sh
# Checks std/json_document: reading, queries, edits, and text laid out as
# Python's json.dumps lays it out.
set -eu

ziran=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/std_json_document.zi" <<'ZI'
#import "json_document"

Same :: (document: *JsonDocument, node: s32, style: JsonStyle, expected: string) -> bool {
    return JsonWrite(document, node, style) == expected
}

#program_export
main :: () -> s32 {
    text := "{\"b\": [1, -2, 3.5e1, true, false, null, [], {}], \"a\": {\"x\": \"caf\\u00e9 \\ud83d\\ude00\\n\", \"y\": \"\\\"q\\\"\"}, \"d\": 1, \"d\": 2}"
    document: JsonDocument
    if !JsonRead(*document, text) { return 1 }
    root := document.root
    if JsonKindOf(*document, root) != .Object || JsonCount(*document, root) != 4 { return 2 }
    b := JsonMember(*document, root, "b")
    if JsonCount(*document, b) != 8 || JsonKindOf(*document, JsonElement(*document, b, 6)) != .Array { return 3 }
    number, ok := JsonInteger(*document, JsonElement(*document, b, 1))
    if !ok || number != -2 { return 4 }
    fraction, whole := JsonInteger(*document, JsonElement(*document, b, 2))
    if whole || JsonNumberText(*document, JsonElement(*document, b, 2)) != "3.5e1" { return 5 }
    truth, is_bool := JsonBool(*document, JsonElement(*document, b, 3))
    if !truth || !is_bool { return 6 }
    // A repeated name reads as its last member, as a Python dict does.
    last, found := JsonInteger(*document, JsonMember(*document, root, "d"))
    if !found || last != 2 || JsonMember(*document, root, "none") != -1 { return 7 }
    x := JsonMember(*document, JsonMember(*document, root, "a"), "x")
    if JsonString(*document, x) != "caf\xc3\xa9 \xf0\x9f\x98\x80\n" || !JsonIs(*document, x, JsonString(*document, x)) { return 8 }
    if JsonKey(*document, x) != "x" || JsonString(*document, -1) != "" || JsonCount(*document, -1) != 0 { return 9 }

    // json.dumps(value) and its other layouts.
    if !Same(*document, JsonMember(*document, root, "a"), JsonPythonStyle(), "{\"x\": \"caf\\u00e9 \\ud83d\\ude00\\n\", \"y\": \"\\\"q\\\"\"}") { return 10 }
    if !Same(*document, JsonMember(*document, root, "a"), JsonPythonStyle(-1, false), "{\"x\": \"caf\xc3\xa9 \xf0\x9f\x98\x80\\n\", \"y\": \"\\\"q\\\"\"}") { return 11 }
    if !Same(*document, b, JsonCompactStyle(), "[1,-2,3.5e1,true,false,null,[],{}]") { return 12 }
    if !Same(*document, b, JsonPythonStyle(1), "[\n 1,\n -2,\n 3.5e1,\n true,\n false,\n null,\n [],\n {}\n]") { return 13 }
    nested := "{\"k\": [{\"z\": 1, \"a\": [2]}]}"
    small: JsonDocument
    if !JsonRead(*small, nested) { return 14 }
    if !Same(*small, small.root, JsonPythonStyle(2, true, true), "{\n  \"k\": [\n    {\n      \"a\": [\n        2\n      ],\n      \"z\": 1\n    }\n  ]\n}") { return 15 }
    if !Same(*small, small.root, JsonPythonStyle(0), "{\n\"k\": [\n{\n\"z\": 1,\n\"a\": [\n2\n]\n}\n]\n}") { return 16 }

    // Edits: a replaced member keeps its place and drops its repeats; new
    // members go last; copies come across documents.
    if !JsonSet(*document, root, "d", JsonNewString(*document, "\x01\x7f")) { return 17 }
    if !JsonSet(*document, root, "e", JsonNewInteger(*document, -9223372036854775807 - 1)) { return 18 }
    if !JsonRemove(*document, root, "b") || JsonRemove(*document, root, "b") { return 19 }
    if !JsonAppend(*small, JsonMember(*small, small.root, "k"), JsonCopy(*small, *document, JsonMember(*document, root, "a"))) { return 20 }
    JsonClear(*document, JsonMember(*document, root, "a"))
    if !Same(*document, root, JsonPythonStyle(), "{\"a\": {}, \"d\": \"\\u0001\\u007f\", \"e\": -9223372036854775808}") { return 21 }
    if !Same(*document, root, JsonPythonStyle(-1, false), "{\"a\": {}, \"d\": \"\\u0001\x7f\", \"e\": -9223372036854775808}") { return 22 }
    if JsonCount(*small, JsonMember(*small, small.root, "k")) != 2 { return 23 }
    built: JsonDocument
    object := JsonNewObject(*built)
    list := JsonNewArray(*built)
    JsonAppend(*built, list, JsonNewBool(*built, false))
    JsonAppend(*built, list, JsonNewNull(*built))
    JsonAppend(*built, list, JsonNewNumber(*built, "0.25"))
    JsonSet(*built, object, "list", list)
    if !Same(*built, object, JsonCompactStyle(), "{\"list\":[false,null,0.25]}") { return 24 }
    if JsonQuote("tab\there") != "\"tab\\there\"" { return 25 }

    // Only one complete value is accepted.
    bad: JsonDocument
    if JsonRead(*bad, "[1,]") || JsonRead(*bad, "{} {}") || JsonRead(*bad, "01") ||
        JsonRead(*bad, "\"open") || JsonRead(*bad, "{\"a\" 1}") || JsonRead(*bad, "") { return 26 }
    if JsonRead(*bad, "[1, 2") || bad.error.count == 0 || bad.root != -1 { return 27 }
    if !JsonRead(*bad, " \n[ ]\t") || JsonCount(*bad, bad.root) != 0 { return 28 }
    return 0
}
ZI

"$ziran" build --target=c --entry std_json_document:main \
    --root "$work" --module-path "$root/std" \
    -o "$work/c" "$work/std_json_document.zi"
"${CC:-cc}" -std=c99 -I"$root/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/program"
(cd "$work" && ./program)
