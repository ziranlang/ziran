#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/native.zi" <<'ZI'
#import "go_types"
clock :: #import "time_go";
ctx :: #import "context_go";
sql :: #import "sql_go";
go_sql :: #system_library "go:database/sql";
RowsResult :: struct {
    value: *sql.Rows
    error: Error
}
Query :: (database: *sql.Database, context: ctx.Context, query: string) -> RowsResult #go_results #foreign go_sql "(*DB).QueryContext";
Read :: (database: *sql.Database) -> Error {
    queried := Query(database, ctx.Background(), "failure")
    if queried.error != null {
        return queried.error
    }
    rows := queried.value
    while sql.Next(rows) {
    }
    error := sql.RowsError(rows)
    sql.CloseRows(rows)
    return error
}
Canonical :: (text: string) -> string {
    parsed := clock.Parse("2006-01-02T15:04:05.999999999Z07:00", text)
    if parsed.error != null {
        return "invalid"
    }
    return clock.Format(clock.UTC(parsed.value), "2006-01-02T15:04:05.000000000Z07:00")
}
#program_export
Answer :: () -> s32 {
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/native.zi"
for input in source saved; do
    root=$work
    file=$root/native.zi
    if test "$input" = saved; then root=$work/ir; file=$root/native.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "database/sql"
    "database/sql/driver"
    "errors"
)

var sentinel = errors.New("native row failure")
var closed bool

type testDriver struct{}
type testConnection struct{}
type testTransaction struct{}
type testRows struct{}
type testResult struct{ fail bool }

func (testDriver) Open(string) (driver.Conn, error) { return testConnection{}, nil }
func (testConnection) Prepare(string) (driver.Stmt, error) { return nil, sentinel }
func (testConnection) Close() error { return nil }
func (testConnection) Begin() (driver.Tx, error) { return testTransaction{}, nil }
func (testConnection) Query(string, []driver.Value) (driver.Rows, error) { return testRows{}, nil }
func (testTransaction) Commit() error { return sentinel }
func (testTransaction) Rollback() error { return nil }
func (testRows) Columns() []string { return []string{"value"} }
func (testRows) Close() error { closed = true; return nil }
func (testRows) Next([]driver.Value) error { return sentinel }
func (result testResult) LastInsertId() (int64, error) { return 0, nil }
func (result testResult) RowsAffected() (int64, error) {
    if result.fail { return 0, sentinel }
    return 9223372036854775807, nil
}

func main() {
    sql.Register("fixture", testDriver{})
    database, error := sql.Open("fixture", "")
    if error != nil { panic(error) }
    defer database.Close()
    if Native_Read(database) != sentinel || !closed { panic("row error identity or cleanup") }
    context := ContextGo_Background()
    if context.Err() != nil || context.Done() != nil { panic("background context") }
    transaction, error := database.BeginTx(context, nil)
    if error != nil { panic(error) }
    if SqlGo_Commit(transaction) != sentinel { panic("commit error identity") }
    transaction, error = database.BeginTx(context, nil)
    if error != nil { panic(error) }
    if SqlGo_Rollback(transaction) != nil { panic("rollback") }
    if SqlGo_Rollback(transaction) != sql.ErrTxDone { panic("rollback error identity") }
    nullable := sql.NullString{String: "native", Valid: true}
    if !SqlGo_NullStringValid(nullable) || SqlGo_NullStringValue(nullable) != "native" {
        panic("nullable string fields")
    }
    if SqlGo_NullStringValid(sql.NullString{}) { panic("nullable zero value") }
    count := SqlGo_RowsAffected(testResult{})
    if count.Value != 9223372036854775807 || count.Error != nil {
        panic("native affected row width or result order")
    }
    count = SqlGo_RowsAffected(testResult{fail: true})
    if count.Value != 0 || count.Error != sentinel {
        panic("native affected row error identity")
    }
    if Native_Canonical("2026-01-02T12:00:00.5+02:00") != "2026-01-02T10:00:00.500000000Z" {
        panic("native UTC conversion")
    }
    if Native_Canonical("invalid") != "invalid" { panic("native parse error") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
    "$ziran" bundle --entry native:Answer --root "$root" --module-path std -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
done
cmp "$work/source-go/native.go" "$work/saved-go/native.go"
echo 'Go SQL error identity, cleanup, contexts, timestamp parsing and saved IR: passed'
