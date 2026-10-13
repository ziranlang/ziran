#!/bin/sh
# A C function table keeps its whole layout even when another module reads
# one of its fields before the module that declares the C callbacks is seen.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(dirname "${1:-$repo/build/bin/ziran}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/table.zi" <<'ZI'
FirstFn :: #type (env: *Env, value: s32) -> s32 #c_call;
SecondFn :: #type (env: *Env) -> s32 #c_call;
Table :: struct {
    reserved: [3]*void
    First: FirstFn
    reserved_4: [2]*void
    Second: SecondFn
}
Env :: struct { functions: *Table; }
// Like a JavaVM: the table is reached only through a C callback.
GetEnvFn :: #type (output: **Env) -> s32 #c_call;
Vm :: struct { GetEnv: GetEnvFn; }
ZI
# Modules are checked in name order: "access" reads env.functions first.
cat > "$work/access.zi" <<'ZI'
#import "table"
native :: #system_library "native";
CurrentVm :: () -> *Vm #foreign native;
#program_export
Answer :: () -> s32 {
    vm := CurrentVm()
    env: *Env
    if vm.GetEnv(*env) != 0 {
        return 0
    }
    return env.functions.First(env, 41)
}
ZI
"$bin/zi2c" --no-main --entry access:Answer --root "$work" -o "$work/c" "$work/access.zi"
cat > "$work/c/host.c" <<'C'
#include "access.h"
_Static_assert(sizeof(Table) == 7 * sizeof(void *), "C function tables keep every slot");
static int32_t first(Env *env, int32_t value) { (void)env; return value + 1; }
static Table table = {{0, 0, 0}, first, {0, 0}, 0};
static Env env = {&table};
static int32_t get_env(Env **output) { *output = &env; return 0; }
static Vm vm = {get_env};
Vm *CurrentVm(void) { return &vm; }
int main(void) { return Answer() == 42 ? 0 : 1; }
C
"${CC:-cc}" -std=c11 -I"$repo/include" -I"$work/c" "$work"/c/*.c -o "$work/app"
"$work/app"
echo "C function tables keep every slot when another module reads them first"
