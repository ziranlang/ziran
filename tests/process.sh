#!/bin/sh
set -eu

ziran=$1
host_test=$2
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran" check --root "$root/tests/spec" --module-path "$root/std" \
    "$root/tests/spec/process_test.zi"
"$ziran" ir --root "$root/tests/spec" --module-path "$root/std" \
    -o "$work/ir" "$root/tests/spec/process_test.zi"
"$ziran" bundle --root "$root/tests/spec" --module-path "$root/std" \
    --entry process_test:SelfTest -o "$work/source.zib" \
    "$root/tests/spec/process_test.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry process_test:SelfTest -o "$work/saved.zib" \
    "$work/ir/process_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
"$host_test" "$work/source.zib"
"$host_test" "$work/saved.zib"

for target in c cpp go; do
    output="$work/$target"
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main \
            --root "$root/tests/spec" --module-path "$root/std" \
            -o "$output" "$root/tests/spec/process_test.zi"
        cat > "$output/main.go" <<'GO'
package main
type mock struct { calls int }
func (m *mock) StartHost(request ProcessRequest, args []string) ProcessStart {
    if request.Executable != "mock" || !request.IsolateDesktop ||
        len(args) != 1 || args[0] != "arg" { panic("bad start") }
    m.calls++
    return ProcessStart{Handle: 7}
}
func (m *mock) NextLineHost(handle int32) ProcessLine {
    if handle != 7 { panic("bad handle") }
    m.calls++
    return ProcessLine{Text: "ready"}
}
func (m *mock) WaitHost(handle int32) ProcessExit {
    if handle != 7 { panic("bad handle") }
    m.calls++
    return ProcessExit{}
}
func main() {
    host := &mock{}
    SetProcessHost(host)
    if ProcessTest_SelfTest() != 42 || host.calls != 3 { panic("process failed") }
}
GO
        cp "$output/process_test_ziran.go" "$output/process_check.go"
        GO111MODULE=off go run "$output/process.go" \
            "$output/process_check.go" "$output/main.go"
    elif test "$target" = c; then
        "$ziran" build --target=c --root "$root/tests/spec" \
            --module-path "$root/std" -o "$output" \
            "$root/tests/spec/process_test.zi"
        cat > "$output/main.c" <<'C'
#include "process_test.h"
#include <assert.h>
static int calls;
ProcessStart StartHost(ProcessRequest request, Slice args) {
    assert(request.isolate_desktop && args.length == 1);
    assert(StringEqual(((String *)args.data)[0], StringView("arg", 3)));
    calls++;
    ProcessStart result = {0}; result.handle = 7; return result;
}
ProcessLine NextLineHost(int32_t handle) {
    assert(handle == 7); calls++;
    ProcessLine result = {0}; result.text = StringView("ready", 5); return result;
}
ProcessExit WaitHost(int32_t handle) {
    assert(handle == 7); calls++;
    ProcessExit result = {0}; return result;
}
int main(void) { return SelfTest() == 42 && calls == 3 ? 0 : 1; }
C
        ${CC:-cc} -I"$root/include" -I"$output" \
            "$output"/*.c -o "$output/app"
        "$output/app"
    else
        "$ziran" build --target=cpp --root "$root/tests/spec" \
            --module-path "$root/std" -o "$output" \
            "$root/tests/spec/process_test.zi"
        cat > "$output/main.cpp" <<'CPP'
#include "process_test.hpp"
#include <cassert>
static int calls;
extern "C" ProcessStart StartHost(ProcessRequest request, Slice args) {
    assert(request.isolate_desktop && args.length == 1);
    assert(StringEqual(static_cast<String *>(args.data)[0], StringView("arg", 3)));
    calls++;
    ProcessStart result = {}; result.handle = 7; return result;
}
extern "C" ProcessLine NextLineHost(int32_t handle) {
    assert(handle == 7); calls++;
    ProcessLine result = {}; result.text = StringView("ready", 5); return result;
}
extern "C" ProcessExit WaitHost(int32_t handle) {
    assert(handle == 7); calls++;
    ProcessExit result = {}; return result;
}
int main() { return SelfTest() == 42 && calls == 3 ? 0 : 1; }
CPP
        ${CXX:-c++} -I"$root/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
        "$output/app"
    fi
done
