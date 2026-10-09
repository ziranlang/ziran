#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/app.zi" <<'ZI'
Cell :: struct { value: s32; values: [4]s32; label: string }
Read :: (cell: Cell) -> s32 { return cell.value + cell.values[0] }
Write :: (cell: Cell) -> s32 {
    cell.value += 1
    cell.values[0] += 1
    return cell.value + cell.values[0]
}
WriteArray :: (values: []s32) { values[0] += 1 }
BorrowWrite :: (cell: Cell) -> s32 {
    WriteArray(cell.values[:])
    return cell.values[0]
}
Callback :: #type (cell: Cell) -> s32;
current: Callback = Read;
stored: Cell;
Handle :: struct { slot: s32; epoch: u64 }
epochs: [3]u64;
handle: Handle;
Record :: struct { numbers: [2]s32; marker: s32 }
shared: Record;
Fresh :: () -> Record {
    result: Record
    result.numbers[0] = 7
    return result
}
Shared :: () -> Record { return shared }
Relay :: (value: Record) -> Record { return value }
MutateShared :: () -> s32 { shared.numbers[0] = 90; return 0 }
Valid :: (value: Handle) -> bool { return epochs[value.slot] == value.epoch }
Index :: (value: Handle) -> s32 {
    if Valid(value) { return value.slot }
    return 0
}
Change :: (values: []u64) { values[1] += cast(u64)1 }
Recursive :: (value: s32) -> s32 {
    if value <= 0 { return 0 }
    return Recursive(value - 1) + 1
}
One :: () -> s32 { return 1 }
#program_export
Budget :: () -> s32 {
    return One() + One() + One() + One() + One() +
        One() + One() + One() + One() + One()
}

#program_export
main :: () -> s32 {
    fresh := Fresh()
    fresh.numbers[0] = 42
    if Fresh().numbers[0] != 7 { return 14 }
    shared = Fresh()
    if Shared().numbers[MutateShared()] != 7 { return 15 }
    shared.numbers[0] = 8
    if Relay(shared).numbers[MutateShared()] != 8 { return 16 }
    epochs[1] = cast(u64)7; handle.slot = 1; handle.epoch = cast(u64)7
    for pass: 0..9 {
        if !Valid(handle) || Index(handle) != 1 { return 8 }
        Change(epochs[:])
        if Valid(handle) || Index(handle) != 0 { return 9 }
        handle.epoch += cast(u64)1
        if !Valid(handle) || Index(handle) != 1 { return 10 }
        address := *epochs[1]
        address.* += cast(u64)1
        if Valid(handle) || Index(handle) != 0 { return 11 }
        handle.epoch += cast(u64)1
        replacement := epochs
        replacement[1] += cast(u64)1
        epochs = replacement
        if Valid(handle) || Index(handle) != 0 { return 12 }
        handle.epoch += cast(u64)1
        if Recursive(4) != 4 { return 13 }
    }
    stored.value = 10; stored.values[0] = 20; stored.label = "retained"
    for i: 0..99 {
        // Identical parameter spellings must keep each function's own
        // mutation analysis; mutable globals and procedure bindings stay live.
        if Read(stored) != 30 || Write(stored) != 32 { return 1 }
        if BorrowWrite(stored) != 21 || stored.value != 10 || stored.values[0] != 20 { return 2 }
        current = Write
        if current(stored) != 32 { return 3 }
        current = Read
        if current(stored) != 30 { return 4 }
        stored.value += 1
        if current(stored) != 31 { return 5 }
        stored.value -= 1
        {
            stored: Cell
            stored.value = cast(s32)i
            if Read(stored) != i { return 6 }
        }
        if stored.label != "retained" { return 7 }
    }
    return 0
}
ZI
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
library=${ZIRAN_LIB:-"$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)/libziran.a"}
cat > "$work/budget.c" <<'C'
#include "zir_bundle.h"
#include "zir_vm.h"
#include <assert.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    char module[ZIR_NAME_MAX], entry[ZIR_NAME_MAX];
    ZibLawTable laws = {0};
    FILE *file = fopen(argv[1], "rb"); assert(file);
    ZirProgram *program = BundleRead(file, argv[1], module, sizeof(module),
                                    entry, sizeof(entry), &laws, NULL);
    fclose(file); assert(program);
    for(int warm = 0; warm < 2; warm++) {
        VmInstance *instance = VmInstanceOpen(program, module, entry, NULL, NULL);
        assert(instance);
        long long answer; int present;
        if(warm) {
            assert(VmInstanceRun(instance, &answer, &present) && present && answer == 10);
            VmInstanceLimitSteps(instance, 20);
            assert(VmInstanceRun(instance, &answer, &present) && present && answer == 10);
        }
        VmInstanceLimitSteps(instance, 4);
        assert(!VmInstanceRun(instance, &answer, &present));
        VmInstanceClose(instance);
    }
    ZibLawTableFree(&laws); ProgramFree(program);
    return 0;
}
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$repo/cmd/zir" \
    "$work/budget.c" "$library" -lm -o "$work/budget"
for form in source saved; do
    module="$work/app.zi"; root=$work
    if test "$form" = saved; then module="$work/ir/app.zir"; root="$work/ir"; fi
    "$ziran" bundle --root "$root" --entry app:main -o "$work/$form.zib" "$module"
    test "$("$ziran" run "$work/$form.zib")" = 0
    "$ziran" build --root "$root" --target=c --exe --entry app:main \
        -o "$work/$form-c" "$module"
    "$work/$form-c/app"
    "$ziran" bundle --root "$root" --entry app:Budget -o "$work/$form-budget.zib" "$module"
    "$work/budget" "$work/$form-budget.zib"
done
