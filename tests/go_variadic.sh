#!/bin/sh
# Native Go slice expansion preserves variadic values, result ordering,
# receiver identity, nil slices, deferred calls and saved-IR behavior.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

cat > "$work/app.zi" <<'ZI'
#import "std/go_types"
io :: #import "std/io_go";
sql :: #import "std/sql_go";
ctx :: #import "std/context_go";
fmt :: #system_library "go:fmt";
Result :: struct { count: isize; error: Error; }
Sprint :: (arguments: []Any) -> string #go_variadic #foreign fmt;
Sprintf :: (format: string, arguments: []Any) -> string #go_variadic #foreign fmt;
Fprintf :: (writer: io.Writer, format: string, arguments: []Any) -> Result #go_results #go_variadic #foreign fmt;
PrintAtReturn :: (writer: io.Writer, arguments: []Any) #go_defer #go_variadic #foreign fmt "Fprint";
Format :: (arguments: []Any) -> string { return Sprint(arguments); }
Formatted :: (arguments: []Any) -> string { return Sprintf("%d:%s", arguments); }
Output :: (writer: io.Writer, arguments: []Any) -> Result { return Fprintf(writer, "%d:%s", arguments); }
Deferred :: (writer: io.Writer, arguments: []Any) {
    PrintAtReturn(writer, arguments)
    arguments[0] = "later"
}
Read :: (database: *sql.Database, context: ctx.Context, query: string, arguments: []Any, destinations: []Any) -> Error {
    return sql.ScanRow(sql.QueryRow(database, context, query, arguments), destinations)
}
Rows :: (rows: *sql.Rows, destinations: []Any) -> Error { return sql.ScanRows(rows, destinations); }
ZI
"$ziran" ir --root "$work" --module-path "std=$repo/std" -o "$work/ir" "$work/app.zi"
for form in source saved; do
    root=$work
    input=$work/app.zi
    if test "$form" = saved; then root=$work/ir; input=$root/app.zir; fi
    "$ziran" build --target=go --pkg main --root "$root" --module-path "std=$repo/std" -o "$work/$form" "$input"
    cat > "$work/$form/main.go" <<'GO'
package main
import (
    "context"
    "database/sql"
    "database/sql/driver"
    "errors"
    "fmt"
    "io"
    "strings"
)
var failure = errors.New("write sentinel")
type brokenWriter struct{}
func (brokenWriter) Write(value []byte) (int,error) { return 2,failure }
type databaseDriver struct{}
type connection struct{}
type rows struct { next bool }
func (databaseDriver) Open(string) (driver.Conn,error) { return connection{},nil }
func (connection) Prepare(string) (driver.Stmt,error) { return nil,errors.New("unsupported prepare") }
func (connection) Close() error { return nil }
func (connection) Begin() (driver.Tx,error) { return nil,errors.New("unsupported transaction") }
func (connection) QueryContext(_ context.Context, query string, args []driver.NamedValue) (driver.Rows,error) {
    if query != "select" || len(args) != 2 || args[0].Value != int64(7) || args[1].Value != "argument" { panic("query arguments") }
    return &rows{},nil
}
func (*rows) Columns() []string { return []string{"number","text"} }
func (*rows) Close() error { return nil }
func (r *rows) Next(dest []driver.Value) error {
    if r.next { return io.EOF }; r.next=true; dest[0]=int64(42); dest[1]="result"; return nil
}
func main() {
    for _, args := range [][]any{nil,{}, {int64(7),"text",nil}} {
        if App_Format(args) != fmt.Sprint(args...) { panic("slice expansion") }
    }
    args:=[]any{int64(7),"text"}
    if App_Formatted(args) != "7:text" { panic("fixed argument") }
    result:=App_Output(brokenWriter{},args)
    if result.Count != 2 || result.Error != failure { panic("native result identity") }
    var output strings.Builder
    deferred:=[]any{"early"}
    App_Deferred(&output,deferred)
    if output.String() != "later" || deferred[0] != "later" { panic("deferred slice storage") }
    sql.Register("variadic-fixture",databaseDriver{})
    database,err:=sql.Open("variadic-fixture","")
    if err != nil { panic(err) }; defer database.Close()
    var number int64
    var text string
    if err:=App_Read(database,context.Background(),"select",[]any{int64(7),"argument"},[]any{&number,&text}); err!=nil || number!=42 || text!="result" { panic("dynamic scan") }
    nativeRows,err:=database.QueryContext(context.Background(),"select",int64(7),"argument")
    if err!=nil { panic(err) }; defer nativeRows.Close()
    if !nativeRows.Next() || App_Rows(nativeRows,[]any{&number,&text})!=nil { panic("row receiver") }
    if err:=App_Rows(nativeRows,[]any{&number}); err==nil { panic("scan arity") }
}
GO
    gofmt -w "$work/$form"/*.go
    GO111MODULE=off go run "$work/$form"/*.go
done

for declaration in \
 'Bad :: () #go_variadic #foreign fmt "Print";' \
 'Bad :: (value: s64) #go_variadic #foreign fmt "Print";' \
 'Bad :: (value: [2]Any) #go_variadic #foreign fmt "Print";' \
 'Bad :: (value: []Any) #go_variadic #go_variadic #foreign fmt "Print";' \
 'Bad :: (value: []Any) #go_variadic { }' \
 'Bad :: (value: []Any) #go_variadic #foreign libc "write";' \
 'Bad :: (value: []Any) #go_field #go_variadic #foreign fmt "Print";' \
 'Bad :: (value: []Any) #foreign fmt "Print" #go_variadic;' \
 'Bad :: (value: []Any) #go_variadic #foreign builtin "append";'; do
    printf '#import "std/go_types"\nfmt :: #system_library "go:fmt";\nlibc :: #system_library "c";\nbuiltin :: #system_library "go:builtin";\n%s\n' "$declaration" > "$work/bad.zi"
    if "$ziran" check --root "$work" --module-path "std=$repo/std" "$work/bad.zi" > "$work/bad.out" 2>&1; then
        echo "invalid variadic binding accepted: $declaration" >&2
        exit 1
    fi
    rg -q '#go_variadic' "$work/bad.out"
done
