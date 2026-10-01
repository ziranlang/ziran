#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

cat > "$work/reply.zi" <<'ZI'
Issue :: struct {
    #go_anonymous
    code: isize #go_tag "json:\"code\""
    message: string #go_tag "json:\"message\""
}
Reply :: struct {
    #go_anonymous
    value: isize #go_tag "json:\"value\""
    issue: *Issue #go_tag "json:\"issue\""
}
Box :: struct($T: Type) {
    #go_anonymous
    value: T #go_tag "json:\"value\""
}
Number :: Box(s32)
Named :: struct { value: isize }
#program_export
Answer :: () -> s32 {
    value: Number
    value.value = 42
    return value.value
}
ZI
cat > "$work/app.zi" <<'ZI'
#import "reply"
Copy :: (value: Reply) -> Reply { return value }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    input=$root/app.zi
    record=$root/reply.zi
    if test "$form" = saved; then
        root=$work/ir
        input=$root/app.zir
        record=$root/reply.zir
    fi
    "$ziran" build --target=go --pkg main --root "$root" -o "$work/$form" "$input"
    cat > "$work/$form/main.go" <<'GO'
package main
import (
    "encoding/json"
    "reflect"
)
type nativeIssue = struct {
    Code int `json:"code"`
    Message string `json:"message"`
}
type nativeReply = struct {
    Value int `json:"value"`
    Issue *nativeIssue `json:"issue"`
}
func main() {
    if reflect.TypeOf(Reply{}).Name() != "" || reflect.TypeOf(Named{}).Name() != "Named" {
        panic("native record identity")
    }
    if reflect.TypeOf(Reply{}) != reflect.TypeOf(nativeReply{}) {
        panic("anonymous field storage and tags")
    }
    if Reply_Answer() != 42 || App_Copy(Reply{Value: 42}).Value != 42 {
        panic("generic and imported records")
    }
    for _, source := range []string{
        `[]`, `{"value":"wrong"}`, `{"issue":"wrong"}`,
        `{"issue":{"code":"wrong"}}`, `{"issue":{"message":42}}`,
        `{"value":7,"issue":{"code":3,"message":"日本語"}}`, `null`,
    } {
        var actual Reply
        var expected nativeReply
        actualError := json.Unmarshal([]byte(source), &actual)
        expectedError := json.Unmarshal([]byte(source), &expected)
        if !reflect.DeepEqual(actualError, expectedError) || !reflect.DeepEqual(actual, expected) {
            panic("native JSON values or error details changed: " + source)
        }
    }
}
GO
    gofmt -w "$work/$form"/*.go
    GO111MODULE=off go run "$work/$form"/*.go
    "$ziran" bundle --root "$root" --entry reply:Answer -o "$work/$form.zib" "$record"
    test "$("$ziran" run "$work/$form.zib")" = 42
done
cmp "$work/source/reply.go" "$work/saved/reply.go"

for declaration in \
    'Bad :: struct { #go_anonymous; #go_anonymous; value: s32 }' \
    'Bad :: struct { #go_anonymous 1; value: s32 }' \
    'Bad :: struct { value: s32 #go_anonymous }' \
    'Bad :: union { #go_anonymous; value: s32 }' \
    'Bad :: struct { #abi_incomplete; #go_anonymous; value: s32 }'; do
    printf '%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid anonymous record accepted: $declaration" >&2
        exit 1
    fi
done
