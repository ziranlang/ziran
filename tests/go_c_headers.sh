#!/bin/sh
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/fixture.h" <<'C'
#ifndef FIXTURE_H
#define FIXTURE_H
#include <stddef.h>
#include <stdint.h>
typedef struct fixture_handle {
    uint8_t padding[19];
    size_t size;
    uint8_t *bytes;
} fixture_handle;
#define FIXTURE_SUCCESS 42
#define FIXTURE_NAME "header-owned layout"
fixture_handle *fixture_new(const char *name);
void fixture_free(fixture_handle *handle);
int32_t fixture_use(fixture_handle *handle, uint8_t *bytes, size_t *length);
int32_t fixture_frees(void);
#endif
C
cat > "$work/fixture.c" <<'C'
#include "fixture.h"
#include <stdlib.h>
#include <string.h>
static int32_t frees;
fixture_handle *fixture_new(const char *name) {
    if (strcmp(name, "available")) return NULL;
    fixture_handle *handle = calloc(1, sizeof(*handle));
    if (handle) {
        handle->size = 43;
        handle->bytes = malloc(3);
        if (!handle->bytes) { free(handle); return NULL; }
    }
    return handle;
}
void fixture_free(fixture_handle *handle) {
    if (!handle) abort();
    frees++;
    free(handle->bytes);
    free(handle);
}
int32_t fixture_use(fixture_handle *handle, uint8_t *bytes, size_t *length) {
    if (handle->size != 42 || !bytes || *length != 3) return -1;
    bytes[1] = 255;
    *length = 2;
    return FIXTURE_SUCCESS;
}
int32_t fixture_frees(void) { return frees; }
C
${CC:-cc} -c "$work/fixture.c" -o "$work/fixture.o"
${AR:-ar} rcs "$work/libfixture.a" "$work/fixture.o"
cat > "$work/types.zi" <<'ZI'
c :: #system_library "go:C/fixture.h";
Handle :: #type #foreign c "fixture_handle";
Character :: #type #foreign c "char";
ZI
cat > "$work/other_types.zi" <<'ZI'
c :: #system_library "go:C/fixture.h";
Handle :: #type #foreign c "fixture_handle";
ZI
cat > "$work/native.zi" <<'ZI'
types :: #import "types";
c :: #system_library "go:C/fixture.h";
New :: (name: *types.Character) -> *types.Handle #foreign c "fixture_new";
FreeAtReturn :: (handle: *types.Handle) #go_defer #foreign c "fixture_free";
Use :: (handle: *types.Handle, bytes: *u8, length: *usize) -> s32 #foreign c "fixture_use";
Size :: (handle: *types.Handle) -> usize #go_field #foreign c "(*fixture_handle).size";
SetSize :: (handle: *types.Handle, size: usize) #go_field #foreign c "(*fixture_handle).size";
Bytes :: (handle: *types.Handle) -> *u8 #go_field #foreign c "(*fixture_handle).bytes";
SetBytes :: (handle: *types.Handle, bytes: *u8) #go_field #foreign c "(*fixture_handle).bytes";
Success :: () -> s32 #go_field #foreign c "FIXTURE_SUCCESS";
Name :: () -> string #go_field #foreign c "FIXTURE_NAME";
Frees :: () -> s32 #foreign c "fixture_frees";
ZI
cat > "$work/policy.zi" <<'ZI'
#load "native.zi";
others :: #import "other_types";
builtin :: #system_library "go:builtin";
Enabled :: () -> bool #foreign builtin "cgo_enabled";
Panic :: (message: string) #foreign builtin "panic";
Snapshot :: struct { pointer: *types.Handle; other: *others.Handle; status: s32 }
Run :: (name: []u8, bytes: []u8) -> s32 {
    if !Enabled() { return -2 }
    handle := New(cast(*types.Character)*name[0])
    if handle == null { return -3 }
    FreeAtReturn(handle)
    if Size(handle) != cast(usize)43 { return -4 }
    SetSize(handle, cast(usize)42)
    buffer := Bytes(handle)
    SetBytes(handle, null)
    if Bytes(handle) != null { return -5 }
    SetBytes(handle, buffer)
    if Bytes(handle) != buffer { return -5 }
    length: usize = cast(usize)bytes.count
    if Use(handle, *bytes[0], *length) != Success() { return -6 }
    return cast(s32)length
}
RunPanic :: (name: []u8) {
    handle := New(cast(*types.Character)*name[0])
    FreeAtReturn(handle)
    Panic("native cleanup")
}
#program_export
Answer :: () -> s32 { return 42 }
NativeEntry :: () -> s32 { return Success() }
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/policy.zi"
# Checked metadata must survive removal of the source signature spelling.
python3 - "$work/ir/policy.zir" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
signature = b'Use :: (handle: *types.Handle, bytes: *u8, length: *usize) -> s32 #foreign c "fixture_use";'
assert data.count(signature) == 1
path.write_bytes(data.replace(signature, b'?' * len(signature)))
PY
for form in source saved; do
    input=$work/policy.zi
    if test "$form" = saved; then input=$work/ir/policy.zir; fi
    out=$work/$form-go
    LDLIBS="-L$work -lfixture" "$ziran" build --target=go --no-main --pkg main --root "$work" -o "$out" "$input"
    cat > "$out/main.go" <<'GO'
package main
func main() {
    if (Snapshot{Status: 42}).Status != 42 { panic("ordinary record disappeared") }
    name := []byte("available\x00")
    bytes := []byte{1, 2, 3}
    if !Policy_Enabled() {
        if Policy_Run(nil, nil) != -2 { panic("unavailable boundary") }
        defer func() {
            if recover() != "C ABI requires CGO_ENABLED=1" { panic("unavailable C ABI must panic") }
        }()
        Policy_Success()
        return
    }
    if Policy_Name() != "header-owned layout" { panic("C macro string") }
    if Policy_Run(name, bytes) != 2 || bytes[1] != 255 || Policy_Frees() != 1 {
        panic("C layout, pointer mutation, size_t or deferred free")
    }
    if Policy_Run([]byte("missing\x00"), bytes) != -3 || Policy_Frees() != 1 { panic("allocation failure") }
    if Policy_Run(name, []byte{1}) != -6 || Policy_Frees() != 2 { panic("error cleanup") }
    func() {
        defer func() { if recover() != "native cleanup" { panic("panic identity") } }()
        Policy_RunPanic(name)
    }()
    if Policy_Frees() != 3 { panic("panic cleanup") }
}
GO
    (cd "$out" && CGO_ENABLED=1 GOEXPERIMENT=cgocheck2 CGO_CFLAGS="-I$work" GO111MODULE=off GOCACHE=/tmp/ziran-cgo-header-cache go run -race .)
    (cd "$out" && CGO_ENABLED=0 GO111MODULE=off GOCACHE=/tmp/ziran-cgo-header-cache go run .)
    for target in c cpp; do
        if "$ziran" build --target="$target" --root "$work" --entry policy:NativeEntry -o "$work/rejected" "$input" > "$work/rejected.log" 2>&1; then
            echo "reachable cgo header binding accepted by $target" >&2
            exit 1
        fi
    done
    if "$ziran" bundle --root "$work" --entry policy:NativeEntry -o "$work/rejected.zib" "$input" > "$work/rejected.log" 2>&1; then
        echo 'reachable cgo header binding accepted by portable bundle' >&2
        exit 1
    fi
    "$ziran" build --target=c --exe --root "$work" --entry policy:Answer -o "$work/$form-c" "$input"
    result=0
    "$work/$form-c/policy" || result=$?
    test "$result" = 42
    "$ziran" bundle --root "$work" --entry policy:Answer -o "$work/$form.zib" "$input"
    test "$("$ziran" run "$work/$form.zib")" = 42
done
for file in types.go types.cgo.go types.nocgo.go other_types.go other_types.cgo.go other_types.nocgo.go policy.go policy.cgo.go policy.nocgo.go zi2go.cgo.go zi2go.nocgo.go; do
    cmp "$work/source-go/$file" "$work/saved-go/$file"
done
for mode in bad_header empty_header absolute_header record_value pointer_record slice_value packing variadic method; do
    cat > "$work/bad.zi" <<'ZI'
c :: #system_library "go:C/fixture.h";
Handle :: #type #foreign c "fixture_handle";
ZI
    case "$mode" in
    bad_header) sed -i 's@C/fixture.h@C/../fixture.h@' "$work/bad.zi" ;;
    empty_header) sed -i 's@C/fixture.h@C/@' "$work/bad.zi" ;;
    absolute_header) sed -i 's@C/fixture.h@C//fixture.h@' "$work/bad.zi" ;;
    record_value) echo 'Use :: (handle: Handle) -> s32 #foreign c "fixture_use";' ;;
    pointer_record) echo 'Result :: struct { value: s32 }; Use :: (value: *Result) -> s32 #foreign c "fixture_use";' ;;
    slice_value) echo 'Use :: (bytes: []u8) -> s32 #foreign c "fixture_use";' ;;
    packing) echo 'Result :: struct { value: s32 }; Use :: () -> Result #go_results #foreign c "fixture_use";' ;;
    variadic) echo 'Use :: (bytes: []u8) -> s32 #go_variadic #foreign c "fixture_use";' ;;
    method) echo 'Use :: (handle: *Handle) -> s32 #foreign c "(*fixture_handle).fixture_use";' ;;
    esac >> "$work/bad.zi"
    sed -i 's/; Use/;\nUse/;s/; Enabled/;\nEnabled/' "$work/bad.zi"
    if "$ziran" ir --root "$work" -o "$work/bad-ir" "$work/bad.zi" > "$work/bad.log" 2>&1; then
        echo "invalid cgo header interface accepted: $mode" >&2
        exit 1
    fi
    if ! grep -Eq 'cgo header|C-pointer ABI' "$work/bad.log"; then
        cat "$work/bad.log" >&2
        echo "invalid header interface failed outside ABI validation: $mode" >&2
        exit 1
    fi
done
for declaration in 'Enabled :: (value: bool) -> bool' 'Enabled :: () -> s32' 'Enabled :: () -> bool #go_field'; do
    printf 'builtin :: #system_library "go:builtin";\n%s #foreign builtin "cgo_enabled";\n' "$declaration" > "$work/bad.zi"
    if "$ziran" ir --root "$work" -o "$work/bad-ir" "$work/bad.zi" > "$work/bad.log" 2>&1; then
        echo 'invalid native availability signature accepted' >&2
        exit 1
    fi
    grep -Eq 'Go builtin requires a valid|#go_field requires' "$work/bad.log"
done
echo 'Go C header bindings: passed'
