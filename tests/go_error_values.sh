#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/native.zi" <<'ZI'
#import "go_types"
errors :: #import "errors_go";
sql :: #import "sql_go";
ctx :: #import "context_go";
go_math :: #system_library "go:math";
go_os :: #system_library "go:os";
builtin :: #system_library "go:builtin";
PiRaw :: () -> float64 #go_field #foreign go_math "Pi";
ArgumentsRaw :: () -> []string #go_field #foreign go_os "Args";
MessageRaw :: (error: Error) -> string #foreign builtin "error.Error";
ObserveRaw :: (error: Error) #foreign builtin "error.Error";
Pi :: () -> float64 {
    return PiRaw()
}
Arguments :: () -> []string {
    return ArgumentsRaw()
}
Message :: (error: Error) -> string {
    return errors.Message(error)
}
Shadow :: (Error: Error) -> string {
    return MessageRaw(Error)
}
Observe :: (error: Error) {
    ObserveRaw(error)
}
Unwrap :: (error: Error) -> Error {
    return errors.Unwrap(error)
}
IsNoRows :: (error: Error) -> bool {
    return errors.Is(error, sql.NoRows())
}
#program_export
Answer :: () -> s32 {
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
python3 - "$work/ir/native.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'PiRaw :: () -> float64 #go_field #foreign go_math "Pi";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
path = path.parent / 'errors_go.zir'
data = path.read_bytes()
signature = b'MessageRaw :: (error: Error) -> string #foreign builtin "error.Error";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for input in source saved; do
    root=$work
    file=$root/native.zi
    if test "$input" = saved; then root=$work/ir; file=$root/native.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "context"
    "database/sql"
    "errors"
    "fmt"
    "math"
    "os"
)

type formattedError struct{}
func (formattedError) Error() string { return "exact native message" }
func (formattedError) Format(state fmt.State, verb rune) { fmt.Fprint(state, "formatted message") }

func main() {
    if Native_Pi() != math.Pi { panic("package constant") }
    previous := os.Args
    defer func() { os.Args = previous }()
    os.Args = []string{"first"}
    if Native_Arguments()[0] != "first" { panic("package variable") }
    os.Args = []string{"updated"}
    if Native_Arguments()[0] != "updated" { panic("package getter captured a stale value") }
    if Native_Message(formattedError{}) != "exact native message" { panic("predeclared interface method") }
    if Native_Shadow(formattedError{}) != "exact native message" { panic("receiver type shadowed by parameter") }
    Native_Observe(formattedError{})
    sentinel := ErrorsGo_New("new native error")
    if sentinel.Error() != "new native error" { panic("error creation") }
    wrapped := fmt.Errorf("wrapped: %w", sentinel)
    if Native_Unwrap(wrapped) != sentinel || !ErrorsGo_Is(wrapped, sentinel) { panic("error identity") }
    if Native_Unwrap(sentinel) != nil || ErrorsGo_Is(sentinel, errors.New(sentinel.Error())) {
        panic("unrelated errors were conflated")
    }
    if !Native_IsNoRows(fmt.Errorf("query: %w", sql.ErrNoRows)) || Native_IsNoRows(sentinel) {
        panic("SQL sentinel identity")
    }
    if SqlGo_TransactionDone() != sql.ErrTxDone || SqlGo_NoRows() != sql.ErrNoRows ||
        ContextGo_Canceled() != context.Canceled || ContextGo_DeadlineExceeded() != context.DeadlineExceeded {
        panic("native package error values")
    }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    for target in c cpp; do
        "$ziran" build --target="$target" --entry native:Answer --root "$root" --module-path std -o "$work/$input-$target" "$file"
    done
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    if "$ziran" build --target=c --no-main --root "$root" --module-path std -o "$work/rejected" "$file" > "$work/rejected.out" 2>&1; then
        echo 'reachable foreign Go package values were accepted in C' >&2
        exit 1
    fi
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
for declaration in \
    'Bad :: (value: s32) -> float64 #go_field #foreign go_math "Pi";' \
    'Bad :: () #go_field #foreign go_math "Pi";' \
    'Bad :: () -> Vec(u8) #go_field #foreign go_math "Pi";' \
    'Bad :: () -> []Box #go_field #foreign go_math "Pi";' \
    'Bad :: () -> Nested #go_field #foreign go_math "Pi";' \
    'Bad :: () -> float64 #go_field #go_results #foreign go_math "Pi";' \
    'Bad :: () -> float64 #go_field #foreign libc "Pi";'; do
    printf '#import "vec"\ngo_math :: #system_library "go:math";\nlibc :: #system_library "libc";\nBox :: struct { values: Vec(u8) }\nNested :: struct { boxes: []Box }\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path std "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid Go package getter accepted: $declaration" >&2
        exit 1
    fi
done
echo 'Go package getters, predeclared interface methods, native errors and saved IR: passed'
