#include "zir_host_native.h"

#include "zir_host_exec.h"
#include "zir_native_memory.h"

#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZIRAN_NO_ENGINE) || defined(__EMSCRIPTEN__)

NativeBundleCode *
NativeBundleCodeOpen(const ExecBundle *bundle, const char **problem)
{
    (void)bundle;
    if(problem != NULL) *problem = "assembly.gap.target-unimplemented";
    return NULL;
}
void NativeBundleCodeClose(NativeBundleCode *code) { (void)code; }
NativeInstance *NativeInstanceOpen(const NativeBundleCode *code, long long max_steps)
{ (void)code; (void)max_steps; return NULL; }
int NativeInstanceRun(NativeInstance *instance, long long *result, int *has_result)
{ (void)instance; (void)result; (void)has_result; return 0; }
void NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps)
{ (void)instance; (void)max_steps; }
void NativeInstanceClose(NativeInstance *instance) { (void)instance; }

#else

#include "compiler_machine_lower.h"
#include "compiler_machine_pipeline.h"
#include "compiler_target.h"
#include "compiler_x86_64.h"

enum { NATIVE_TRAP_STUB_SIZE = 12 };

struct NativeBundleCode {
    MachineNativeOwner owner;
    int has_result;
};

struct NativeInstance {
    ZirNativeMapping arena;
    unsigned char *data;
    size_t data_size;
    int64_t fuel_offset;
    int64_t trap_offset;
    void *entry;
    int has_result;
    int terminal;
    jmp_buf trap;
};

static _Thread_local NativeInstance *active_instance;

static size_t
align_size(size_t value, size_t alignment)
{
    if(alignment == 0 || value > SIZE_MAX - (alignment - 1))
        return 0;
    return (value + alignment - 1) / alignment * alignment;
}

static int
data_layout(MachineProgram program, size_t *size, int64_t *fuel,
            int64_t *trap)
{
    MachineGlobal *globals = program.globals.data;
    MachineType *types = program.types.data;
    size_t at = 0;
    for(int64_t i = 0; i < program.globals.length; i++) {
        MachineGlobal global = globals[i];
        if(!global.writable || global.type < 0 || global.type >= program.types.length)
            return 0;
        MachineType type = types[global.type];
        if(type.size <= 0 || type.alignment <= 0 ||
           (type.alignment & (type.alignment - 1)) != 0)
            return 0;
        at = align_size(at, (size_t)type.alignment);
        if((uint64_t)type.size > SIZE_MAX - at)
            return 0;
        at += (size_t)type.size;
    }
    at = align_size(at, 8);
    if(at > (size_t)INT64_MAX - 8)
        return 0;
    *fuel = (int64_t)at;
    at += 8;
    at = align_size(at, 4);
    if(at > (size_t)INT64_MAX - 4)
        return 0;
    *trap = (int64_t)at;
    at += 4;
    at = align_size(at, 8);
    if(at == 0)
        return 0;
    *size = at;
    return 1;
}

static int
global_offset(MachineProgram program, int32_t symbol, size_t *offset)
{
    if(symbol < 0 || symbol >= program.globals.length)
        return 0;
    MachineGlobal *globals = program.globals.data;
    MachineType *types = program.types.data;
    size_t at = 0;
    for(int32_t i = 0; i <= symbol; i++) {
        if(globals[i].type < 0 || globals[i].type >= program.types.length)
            return 0;
        MachineType type = types[globals[i].type];
        if(type.size <= 0 || type.alignment <= 0 ||
           (type.alignment & (type.alignment - 1)) != 0)
            return 0;
        at = align_size(at, (size_t)type.alignment);
        if(i == symbol) {
            *offset = at;
            return 1;
        }
        if((uint64_t)type.size > SIZE_MAX - at)
            return 0;
        at += (size_t)type.size;
    }
    return 0;
}

NativeBundleCode *
NativeBundleCodeOpen(const ExecBundle *bundle, const char **problem)
{
    if(problem != NULL) *problem = "assembly.gap.machine-slice";
    if(bundle == NULL || ExecBundleCapabilityCount(bundle) != 0 ||
       ExecBundleStateCount(bundle) != 0)
        return NULL;
    const LirProgram *lir = ExecBundleProgram(bundle);
    if(lir == NULL)
        return NULL;
    NativeBundleCode *code = calloc(1, sizeof(*code));
    if(code == NULL) {
        if(problem != NULL) *problem = "assembly.native.allocation";
        return NULL;
    }
    MachineDiagnostic diagnostic = compiler_machine_pipeline_MachineNativeBuild(
        *lir, compiler_target_TargetLinuxX64(),
        compiler_machine_ir_MachineDefaultLimits(), &code->owner);
    if(diagnostic.problem != MachineProblem_None) {
        compiler_machine_pipeline_MachineNativeFree(&code->owner);
        free(code);
        return NULL;
    }
    MachineProgram program = compiler_machine_lower_MachineProgramView(&code->owner.machine);
    MachineFunction *functions = program.functions.data;
    MachineType *types = program.types.data;
    if(program.entry < 0 || program.entry >= program.functions.length ||
       functions[program.entry].result_type < 0 ||
       functions[program.entry].result_type >= program.types.length) {
        compiler_machine_pipeline_MachineNativeFree(&code->owner);
        free(code);
        return NULL;
    }
    code->has_result = types[functions[program.entry].result_type].kind != MachineTypeKind_Void;
    if(problem != NULL) *problem = "";
    return code;
}

void
NativeBundleCodeClose(NativeBundleCode *code)
{
    if(code == NULL) return;
    compiler_machine_pipeline_MachineNativeFree(&code->owner);
    free(code);
}

static _Noreturn void
native_trap(uint32_t code)
{
    NativeInstance *instance = active_instance;
    if(instance == NULL)
        abort();
    if(instance->trap_offset >= 0 && instance->data_size >= sizeof(code) &&
       (uint64_t)instance->trap_offset <= instance->data_size - sizeof(code))
        memcpy(instance->data + instance->trap_offset, &code, sizeof(code));
    instance->terminal = 1;
    longjmp(instance->trap, 1);
}

static int
write_relative32(unsigned char *field, uintptr_t target, int64_t addend)
{
    uintptr_t source = (uintptr_t)field;
    int64_t displacement;
    if(target > (uintptr_t)INT64_MAX || source > (uintptr_t)INT64_MAX ||
       addend < INT32_MIN || addend > INT32_MAX)
        return 0;
    if(target >= source) {
        uint64_t difference = target - source;
        if(difference > (uint64_t)INT64_MAX) return 0;
        displacement = (int64_t)difference;
    } else {
        uint64_t difference = source - target;
        if(difference > (uint64_t)INT64_MAX) return 0;
        displacement = -(int64_t)difference;
    }
    if((addend > 0 && displacement > INT64_MAX - addend) ||
       (addend < 0 && displacement < INT64_MIN - addend))
        return 0;
    displacement += addend;
    if(displacement < INT32_MIN || displacement > INT32_MAX)
        return 0;
    int32_t narrowed = (int32_t)displacement;
    memcpy(field, &narrowed, sizeof(narrowed));
    return 1;
}

NativeInstance *
NativeInstanceOpen(const NativeBundleCode *code, long long max_steps)
{
    if(code == NULL) return NULL;
    MachineProgram program = compiler_machine_lower_MachineProgramView(
        (MachineOwner *)&code->owner.machine);
    MachineX64Image image = compiler_x86_64_MachineX64View(
        (MachineX64Owner *)&code->owner.encoded);
    MachineX64Function *functions = image.functions.data;
    MachineX64Relocation *relocations = image.relocations.data;
    if(image.code.data == NULL || image.code.length <= 0 || image.entry < 0 ||
       image.entry >= image.functions.length)
        return NULL;
    size_t data_size;
    int64_t fuel, trap;
    if(!data_layout(program, &data_size, &fuel, &trap)) return NULL;
    size_t page = ZirNativePageSize();
    if(page == 0 || (uint64_t)image.code.length > SIZE_MAX - NATIVE_TRAP_STUB_SIZE)
        return NULL;
    size_t code_size = (size_t)image.code.length + NATIVE_TRAP_STUB_SIZE;
    size_t code_pages = align_size(code_size, page);
    if(code_pages == 0 || data_size > SIZE_MAX - code_pages)
        return NULL;
    NativeInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL || !ZirNativeReserve(code_pages + data_size, &instance->arena)) {
        free(instance);
        return NULL;
    }
    unsigned char *base = instance->arena.address;
    instance->data = base + code_pages;
    instance->data_size = instance->arena.size - code_pages;
    instance->fuel_offset = fuel;
    instance->trap_offset = trap;
    instance->has_result = code->has_result;
    memcpy(base, image.code.data, (size_t)image.code.length);

    unsigned char *stub = base + image.code.length;
    stub[0] = 0x48; stub[1] = 0xb8;
    uintptr_t trap_function = (uintptr_t)(void *)&native_trap;
    memcpy(stub + 2, &trap_function, sizeof(trap_function));
    stub[10] = 0xff; stub[11] = 0xe0;

    for(int64_t i = 0; i < image.relocations.length; i++) {
        MachineX64Relocation relocation = relocations[i];
        if(relocation.offset < 0 || relocation.offset > image.code.length - 4)
            goto failed;
        uintptr_t target;
        if(relocation.kind == MachineX64RelocationKind_GlobalAddress) {
            size_t offset;
            if(!global_offset(program, relocation.symbol, &offset) ||
               offset >= instance->data_size)
                goto failed;
            target = (uintptr_t)(instance->data + offset);
        } else if(relocation.kind == MachineX64RelocationKind_FuelCounter &&
                  relocation.symbol == 0) {
            target = (uintptr_t)(instance->data + fuel);
        } else if(relocation.kind == MachineX64RelocationKind_TrapHandler &&
                  relocation.symbol == 0) {
            target = (uintptr_t)stub;
        } else {
            goto failed;
        }
        if(!write_relative32(base + relocation.offset, target, relocation.addend))
            goto failed;
    }
    int64_t initial_fuel = max_steps > 0 ? max_steps : INT64_MAX;
    memcpy(instance->data + fuel, &initial_fuel, sizeof(initial_fuel));
    instance->entry = base + functions[image.entry].begin;
    ZirNativeMapping code_mapping = {.address = base, .size = code_pages};
    if(!ZirNativeFlushInstructions(&code_mapping) ||
       !ZirNativeProtectRX(&code_mapping))
        goto failed;
    return instance;
failed:
    NativeInstanceClose(instance);
    return NULL;
}

int
NativeInstanceRun(NativeInstance *instance, long long *result, int *has_result)
{
    if(instance == NULL || result == NULL || has_result == NULL ||
       instance->terminal || active_instance != NULL)
        return 0;
    active_instance = instance;
    if(setjmp(instance->trap) != 0) {
        active_instance = NULL;
        return 0;
    }
    long long (*entry)(void);
    memcpy(&entry, &instance->entry, sizeof(entry));
    long long value = entry();
    active_instance = NULL;
    *result = value;
    *has_result = instance->has_result;
    return 1;
}

void
NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps)
{
    if(instance == NULL || instance->terminal) return;
    int64_t fuel = max_steps > 0 ? max_steps : INT64_MAX;
    memcpy(instance->data + instance->fuel_offset, &fuel, sizeof(fuel));
}

void
NativeInstanceClose(NativeInstance *instance)
{
    if(instance == NULL) return;
    if(active_instance == instance) active_instance = NULL;
    ZirNativeRelease(&instance->arena);
    free(instance);
}

#endif
