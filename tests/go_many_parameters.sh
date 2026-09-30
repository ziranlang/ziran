#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Exercise the complete checked parameter list, including the former 16-argument
# foreign-call and 32-argument procedure-signature boundaries.
python3 - "$work" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
parts = [
    '#import "go_types"',
    'io :: #import "io_go";',
    'sql :: #import "sql_go";',
    'fmt :: #system_library "go:fmt";',
    'database :: #system_library "go:database/sql";',
    'PrintResult :: struct { count: isize; error: Error; }',
]
for count in [16, 17, 32, 33, 64]:
    parameters = ', '.join(f'p{i}: s64' for i in range(count))
    values = ', '.join(f'cast(s64){i + 1}' for i in range(count))
    total = ' + '.join(f'p{i}' for i in range(count))
    parts.extend([
        f'Sprint{count} :: ({parameters}) -> string #foreign fmt "Sprint";',
        f'Sum{count} :: ({parameters}) -> s64 {{ return {total} }}',
        f'Text{count} :: () -> string {{ return Sprint{count}({values}) }}',
        f'Total{count} :: () -> s64 {{ return Sum{count}({values}) }}',
    ])
parameters = ', '.join(f'p{i}: s64' for i in range(63))
values = ', '.join(f'cast(s64){i + 1}' for i in range(63))
parts.extend([
    f'Print :: (writer: io.Writer, {parameters}) -> PrintResult #go_results #foreign fmt "Fprint";',
    f'PrintAll :: (writer: io.Writer) -> PrintResult {{ return Print(writer, {values}) }}',
    f'Cleanup :: (writer: io.Writer, {parameters}) #go_defer #foreign fmt "Fprint";',
    f'Deferred :: (writer: io.Writer) {{ Cleanup(writer, {values}) }}',
])
pointers = ', '.join(f'p{i}: *s64' for i in range(63))
addresses = ', '.join(f'*values[{i}]' for i in range(63))
parts.extend([
    f'Scan :: (row: *sql.Row, {pointers}) -> Error #foreign database "(*Row).Scan";',
    'Read :: (row: *sql.Row) -> s64 {',
    '    values: [63]s64',
    f'    if Scan(row, {addresses}) != null {{ return -1 }}',
    '    result: s64 = 0',
    '    for value: values { result += value }',
    '    return result',
    '}',
])
(root / 'many.zi').write_text('\n'.join(parts) + '\n')
PY

"$ziran" ir --root "$work" --module-path std -o "$work/ir" "$work/many.zi"
for input in source saved; do
    root=$work
    file=$root/many.zi
    if test "$input" = saved; then root=$work/ir; file=$root/many.zir; fi
    out=$work/$input-go
    "$ziran" build --target=go --no-main --pkg main --root "$root" --module-path std -o "$out" "$file"
    cat > "$out/main.go" <<'GO'
package main

import (
    "bytes"
    "database/sql"
    "database/sql/driver"
    "fmt"
    "io"
    "strconv"
    "strings"
)

type testDriver struct{}
type testConnection struct{}
type testRows struct{ read bool }

func (testDriver) Open(string) (driver.Conn, error) { return testConnection{}, nil }
func (testConnection) Prepare(string) (driver.Stmt, error) { panic("unused") }
func (testConnection) Close() error { return nil }
func (testConnection) Begin() (driver.Tx, error) { panic("unused") }
func (testConnection) Query(string, []driver.Value) (driver.Rows, error) { return &testRows{}, nil }
func (*testRows) Columns() []string { return make([]string, 63) }
func (*testRows) Close() error { return nil }
func (rows *testRows) Next(values []driver.Value) error {
    if rows.read { return io.EOF }
    rows.read = true
    for index := range values { values[index] = int64(index + 1) }
    return nil
}

func expected(count int) string {
    values := make([]string, count)
    for index := range values { values[index] = strconv.Itoa(index + 1) }
    return strings.Join(values, " ")
}

func main() {
    for _, test := range []struct {
        count int
        text func() string
        total func() int64
    }{
        {16, Many_Text16, Many_Total16}, {17, Many_Text17, Many_Total17},
        {32, Many_Text32, Many_Total32}, {33, Many_Text33, Many_Total33},
        {64, Many_Text64, Many_Total64},
    } {
        if got := test.text(); got != expected(test.count) { panic(fmt.Sprintf("%d native arguments: %q", test.count, got)) }
        if got := test.total(); got != int64(test.count * (test.count + 1) / 2) { panic("procedure parameters") }
    }
    var buffer bytes.Buffer
    printed := Many_PrintAll(&buffer)
    if printed.Error != nil || printed.Count != buffer.Len() || buffer.String() != expected(63) {
        panic("multiple-result adapter parameters")
    }
    buffer.Reset()
    Many_Deferred(&buffer)
    if buffer.String() != expected(63) { panic("deferred native arguments") }
    sql.Register("fixture", testDriver{})
    database, error := sql.Open("fixture", "")
    if error != nil { panic(error) }
    defer database.Close()
    if Many_Read(database.QueryRow("fixture")) != 63 * 64 / 2 { panic("native scan arguments") }
}
GO
    GO111MODULE=off go run -race "$out"/*.go
done
cmp "$work/source-go/many.go" "$work/saved-go/many.go"
echo 'Go procedure, foreign, result, deferred and scan parameter lists: passed'
