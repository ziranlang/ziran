/*
 * zir_host_native.c - native execution of verified ZX on linux-x86_64.
 *
 * The machine pipeline compiles the bundle's verified program once per
 * content, CPU and backend. Each instance copies that image into RW pages,
 * resolves its relocations against its own data, host veneers and trap stub,
 * then publishes code RX before anything runs; no page is ever writable and
 * executable at once. Fuel, the stack limit and traps live in the instance's
 * data, so instances are independent. A trap longjmps out of generated code
 * and leaves the instance terminal; nothing retries in another engine.
 */
#include "zir_host_native.h"

#include "zir_diagnostic.h"
#include "zir_host_exec.h"
#include "zir_native_memory.h"

#include <limits.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZIRAN_NO_ENGINE) || !defined(__x86_64__) || !defined(__linux__)

static const char native_unavailable[] =
    "assembly.gap.target-unimplemented: native ZX runs only on linux-x86_64; "
    "no alternate engine was selected";

const NativeBundleCode *
NativeBundleSlotCode(NativeBundleSlot *slot, const ExecBundle *bundle,
                     const unsigned char digest[32])
{
    (void)slot; (void)bundle; (void)digest;
    return NULL;
}
const char *
NativeBundleSlotProblem(NativeBundleSlot *slot, const ExecBundle *bundle,
                        const unsigned char digest[32])
{
    (void)slot; (void)bundle; (void)digest;
    return native_unavailable;
}
void NativeBundleSlotClose(NativeBundleSlot *slot) { (void)slot; }
NativeInstance *
NativeInstanceOpen(const NativeBundleCode *code, const ExecBundle *bundle,
                   NativeFindBinding find, void *context, long long max_steps)
{
    (void)code; (void)bundle; (void)find; (void)context; (void)max_steps;
    return NULL;
}
int NativeInstanceRun(NativeInstance *instance, long long *result, int *has_result)
{ (void)instance; (void)result; (void)has_result; return 0; }
void NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps)
{ (void)instance; (void)max_steps; }
void NativeInstanceMatchFieldsByName(NativeInstance *instance, int enabled)
{ (void)instance; (void)enabled; }
size_t NativeInstanceStateCount(const NativeInstance *instance) { (void)instance; return 0; }
int NativeInstanceStateAt(NativeInstance *instance, size_t index, const char **module,
                          const char **name, const char **type, struct HostValue *value)
{ (void)instance; (void)index; (void)module; (void)name; (void)type; (void)value; return 0; }
int NativeInstanceStateStore(NativeInstance *instance, const struct HostValue *const *values)
{ (void)instance; (void)values; return 0; }
size_t NativeInstanceLiveBytes(const NativeInstance *instance) { (void)instance; return 0; }
void NativeInstanceClose(NativeInstance *instance) { (void)instance; }

#else

#include "compiler_exec_debug.h"
#include "compiler_lowered_ir.h"
#include "compiler_machine_image.h"
#include "compiler_machine_pipeline.h"
#include "compiler_target.h"
#include "compiler_x86_64.h"
#include "zir_exec_console.h"
#include "zir_text.h"

#include <cpuid.h>
#include <pthread.h>

enum {
    NATIVE_HOST_STUB_SIZE = 256,
    NATIVE_TRAP_STUB_SIZE = 32,
    NATIVE_RESIZE_STUB_SIZE = 384,
    NATIVE_RESIZE_FRAME = 264,
    NATIVE_HEAP_MAGIC = 0x5a58,
    NATIVE_CALL_STACK = 200,
    NATIVE_CALL_STACK_ARGUMENTS = 208,
    /* Code compiled for a file nobody holds, kept for a reopen. */
    NATIVE_CACHE_IDLE = 4
};

#define NATIVE_TRAP_HOST ((uint32_t)MachineX64TrapReserved)
#define NATIVE_TRAP_BOUNDS ((uint32_t)MachineX64TrapReserved - 1u)
#define NATIVE_TRAP_STORAGE ((uint32_t)MachineX64TrapStorage)
/* Live bytes of an instance's resizable storage, the engine's value limit. */
#define NATIVE_HEAP_LIMIT ((size_t)268435456)

/* Stack the limit leaves for host calls and the C runtime, and the most
 * generated code may use when the thread's bounds are unknown. */
#define NATIVE_STACK_RESERVE ((uintptr_t)256 * 1024)
#define NATIVE_STACK_FALLBACK ((uintptr_t)1024 * 1024)
#define NATIVE_STACK_MAXIMUM ((uintptr_t)64 * 1024 * 1024)

typedef struct NativeActiveCall {
    struct NativeActiveCall *previous;
    jmp_buf trap;
} NativeActiveCall;

/* This is the target-neutral input to the common typed bridge. The x86-64
 * veneer only snapshots its volatile ABI registers and stack argument base. */
typedef struct NativeAbiFrame {
    uint64_t general[6];
    unsigned char floating[8][16];
    const unsigned char *stack;
    uint64_t floating_result;
    /* rax at the call: the caller's destination for a record or array result. */
    unsigned char *result_address;
} NativeAbiFrame;

/* write_host_stub stores to these offsets from its own stack frame. */
_Static_assert(offsetof(NativeAbiFrame, floating) == 48, "frame floats");
_Static_assert(offsetof(NativeAbiFrame, stack) == 176, "frame stack");
_Static_assert(offsetof(NativeAbiFrame, floating_result) == 184, "frame float result");
_Static_assert(offsetof(NativeAbiFrame, result_address) == 192, "frame result address");
_Static_assert(sizeof(NativeAbiFrame) <= NATIVE_CALL_STACK, "frame size");

/* Compiled code, shared by every bundle with the same content digest on the
 * same CPU. The backend is this build's, so it is part of every key. */
struct NativeBundleCode {
    MachineNativeOwner owner;
    unsigned char digest[32];
    uint64_t cpu;
    int has_result;
    size_t references;
    /* While unreferenced: when it was released, for eviction order. */
    unsigned long long idle;
    NativeBundleCode *next;
};

/* The header before each block Resize returns. */
typedef struct NativeBlock {
    uint64_t magic;
    uint64_t count;
    uint64_t element;
    struct NativeBlock *previous, *next;
    const struct NativeInstance *owner;
} NativeBlock;

_Static_assert(sizeof(NativeBlock) % 16 == 0, "block data alignment");

struct NativeInstance {
    const NativeBundleCode *code;
    const ExecBundle *bundle;
    ExecHosts *hosts;
    ZirNativeMapping arena;
    unsigned char *data;
    size_t data_size;
    int64_t fuel_offset;
    int64_t trap_offset;
    int64_t stack_limit_offset;
    void *entry;
    int has_result;
    int terminal;
    /* Steps as the engine counts them: a limit from instantiation is one
     * allowance for initializers and every run (cumulative); a limit set
     * later bounds each run on its own. 0 is unbounded. */
    long long limit;
    long long used;
    int cumulative;
    uint32_t trap_code;
    uintptr_t trap_pc;
    NativeActiveCall *active;
    NativeBlock *blocks;
    size_t heap_bytes;
    /* Bytes of the program's globals, which the engine counts as live. */
    size_t global_bytes;
    /* The pointer table and frame arena; the header is in data. */
    ZirNativeMapping pointers;
    unsigned char *pointer_header;
};

typedef struct StubWriter {
    unsigned char *bytes;
    size_t at, capacity;
    int failed;
} StubWriter;

static pthread_mutex_t native_lock = PTHREAD_MUTEX_INITIALIZER;
static NativeBundleCode *native_cache;
static unsigned long long native_clock;

static size_t
align_size(size_t value, size_t alignment)
{
    if(alignment == 0 || value > SIZE_MAX - (alignment - 1))
        return 0;
    return (value + alignment - 1) / alignment * alignment;
}

static void
stub_byte(StubWriter *writer, unsigned value)
{
    if(writer->at >= writer->capacity) {
        writer->failed = 1;
        return;
    }
    writer->bytes[writer->at++] = (unsigned char)value;
}

static void
stub_u32(StubWriter *writer, uint32_t value)
{
    for(int at = 0; at < 4; at++) stub_byte(writer, value >> (8 * at));
}

static void
stub_u64(StubWriter *writer, uint64_t value)
{
    for(int at = 0; at < 8; at++) stub_byte(writer, value >> (8 * at));
}

static void
stub_store_general(StubWriter *writer, int reg, uint32_t offset)
{
    stub_byte(writer, reg >= 8 ? 0x4c : 0x48);
    stub_byte(writer, 0x89);
    stub_byte(writer, 0x84 | ((reg & 7) << 3));
    stub_byte(writer, 0x24);
    stub_u32(writer, offset);
}

static void
stub_store_float(StubWriter *writer, int reg, uint32_t offset)
{
    stub_byte(writer, 0xf3);
    stub_byte(writer, 0x0f);
    stub_byte(writer, 0x7f);
    stub_byte(writer, 0x84 | (reg << 3));
    stub_byte(writer, 0x24);
    stub_u32(writer, offset);
}

static _Noreturn void NativeTrap(NativeInstance *instance, uint32_t code, uintptr_t at);
static uint64_t NativeHostInvoke(NativeInstance *instance, int32_t function,
                                 NativeAbiFrame *frame);
static uint64_t NativeResize(uint64_t data, uint64_t old_count, uint64_t new_count,
                             uint64_t element, NativeInstance *instance);
static int native_call_entry(NativeInstance *instance, void *address, long long *result);

/* movdqu [rsp + offset], xmm / movdqu xmm, [rsp + offset]. */
static void
stub_xmm(StubWriter *writer, unsigned opcode, int reg, uint32_t offset)
{
    stub_byte(writer, 0xf3);
    if(reg >= 8) stub_byte(writer, 0x44);
    stub_byte(writer, 0x0f);
    stub_byte(writer, opcode);
    stub_byte(writer, 0x84 | ((reg & 7) << 3));
    stub_byte(writer, 0x24);
    stub_u32(writer, offset);
}

/* Resize keeps every float register: the machine form clobbers only general
 * caller-saved registers, while the C allocator may use any. */
static int
write_resize_stub(unsigned char *bytes, size_t capacity, NativeInstance *instance)
{
    StubWriter out = {bytes, 0, capacity, 0};
    stub_byte(&out, 0x48); stub_byte(&out, 0x81); stub_byte(&out, 0xec);
    stub_u32(&out, NATIVE_RESIZE_FRAME);
    for(int reg = 0; reg < 16; reg++) stub_xmm(&out, 0x7f, reg, (uint32_t)(reg * 16));
    /* NativeResize(rdi, rsi, rdx, rcx, instance). */
    stub_byte(&out, 0x49); stub_byte(&out, 0xb8);
    stub_u64(&out, (uint64_t)(uintptr_t)instance);
    stub_byte(&out, 0x48); stub_byte(&out, 0xb8);
    stub_u64(&out, (uint64_t)(uintptr_t)(void *)&NativeResize);
    stub_byte(&out, 0xff); stub_byte(&out, 0xd0);
    for(int reg = 0; reg < 16; reg++) stub_xmm(&out, 0x6f, reg, (uint32_t)(reg * 16));
    stub_byte(&out, 0x48); stub_byte(&out, 0x81); stub_byte(&out, 0xc4);
    stub_u32(&out, NATIVE_RESIZE_FRAME);
    stub_byte(&out, 0xc3);
    return !out.failed;
}

/* One veneer per relocation keeps target code ignorant of C callback
 * addresses. All typed conversion and copy-back remains in NativeHostInvoke. */
static int
write_host_stub(unsigned char *bytes, size_t capacity, NativeInstance *instance,
                int32_t function, size_t *written)
{
    StubWriter out = {bytes, 0, capacity, 0};
    stub_byte(&out, 0x48); stub_byte(&out, 0x81); stub_byte(&out, 0xec);
    stub_u32(&out, NATIVE_CALL_STACK);
    stub_store_general(&out, 0, 192);
    static const int general[] = {7, 6, 2, 1, 8, 9};
    for(int at = 0; at < 6; at++)
        stub_store_general(&out, general[at], (uint32_t)(at * 8));
    for(int at = 0; at < 8; at++)
        stub_store_float(&out, at, (uint32_t)(48 + at * 16));
    /* rax = caller's first stack argument; save it in the frame. */
    stub_byte(&out, 0x48); stub_byte(&out, 0x8d); stub_byte(&out, 0x84);
    stub_byte(&out, 0x24); stub_u32(&out, NATIVE_CALL_STACK_ARGUMENTS);
    stub_store_general(&out, 0, 176);
    /* NativeHostInvoke(instance, function, &frame). */
    stub_byte(&out, 0x48); stub_byte(&out, 0xbf);
    stub_u64(&out, (uint64_t)(uintptr_t)instance);
    stub_byte(&out, 0xbe); stub_u32(&out, (uint32_t)function);
    stub_byte(&out, 0x48); stub_byte(&out, 0x8d);
    stub_byte(&out, 0x14); stub_byte(&out, 0x24);
    stub_byte(&out, 0x48); stub_byte(&out, 0xb8);
    stub_u64(&out, (uint64_t)(uintptr_t)(void *)&NativeHostInvoke);
    stub_byte(&out, 0xff); stub_byte(&out, 0xd0);
    /* Float results are written to the frame as their canonical bits. */
    stub_byte(&out, 0xf3); stub_byte(&out, 0x0f); stub_byte(&out, 0x7e);
    stub_byte(&out, 0x84); stub_byte(&out, 0x24); stub_u32(&out, 184);
    stub_byte(&out, 0x48); stub_byte(&out, 0x81); stub_byte(&out, 0xc4);
    stub_u32(&out, NATIVE_CALL_STACK);
    stub_byte(&out, 0xc3);
    *written = out.at;
    return !out.failed;
}

static int
write_trap_stub(unsigned char *bytes, size_t capacity, NativeInstance *instance,
                size_t *written)
{
    StubWriter out = {bytes, 0, capacity, 0};
    /* NativeTrap(instance, code, return address): the generated trap put the
     * code in rdi and called here from inside the trapping instruction. */
    stub_byte(&out, 0x48); stub_byte(&out, 0x8b);
    stub_byte(&out, 0x14); stub_byte(&out, 0x24);
    stub_byte(&out, 0x48); stub_byte(&out, 0x89); stub_byte(&out, 0xfe);
    stub_byte(&out, 0x48); stub_byte(&out, 0xbf);
    stub_u64(&out, (uint64_t)(uintptr_t)instance);
    stub_byte(&out, 0x48); stub_byte(&out, 0xb8);
    stub_u64(&out, (uint64_t)(uintptr_t)(void *)&NativeTrap);
    stub_byte(&out, 0xff); stub_byte(&out, 0xe0);
    *written = out.at;
    return !out.failed;
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

static int
capability_index(const ExecBundle *bundle, int32_t function, size_t *index)
{
    for(size_t at = 0; at < ExecBundleCapabilityCount(bundle); at++)
        if(ExecBundleCapabilityFunctionId(bundle, at) == function) {
            *index = at;
            return 1;
        }
    return 0;
}

/* ---- Compiling and the code cache --------------------------------------- */

static uint64_t
native_cpu(void)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    uint64_t key = 0;
    if(__get_cpuid(1, &a, &b, &c, &d)) key = ((uint64_t)c << 32) | d;
    if(__get_cpuid_count(7, 0, &a, &b, &c, &d)) key ^= ((uint64_t)b << 7) ^ ((uint64_t)c << 39);
    return key;
}

static const char *
machine_problem_name(MachineProblem problem)
{
    switch(problem) {
    case MachineProblem_Owner: return "owner";
    case MachineProblem_Target: return "target";
    case MachineProblem_UnsupportedTarget: return "unsupported-target";
    case MachineProblem_VerifyInput: return "verify-input";
    case MachineProblem_Limit: return "limit";
    case MachineProblem_Allocation: return "allocation";
    case MachineProblem_Unsupported: return "unsupported";
    case MachineProblem_Relocation: return "relocation";
    case MachineProblem_Frame: return "frame";
    case MachineProblem_Call: return "call";
    case MachineProblem_Constraint: return "constraint";
    default: return "rejected";
    }
}

static void
native_free_code(NativeBundleCode *code)
{
    compiler_machine_pipeline_MachineNativeFree(&code->owner);
    free(code);
}

/* Compiles BUNDLE, with its debug attachment's origins when it has one: the
 * code is the same either way, and traps can then name source lines. */
static NativeBundleCode *
native_compile(const ExecBundle *bundle, char *problem, size_t capacity)
{
    const LirProgram *lir = ExecBundleProgram(bundle);
    if(lir == NULL) {
        snprintf(problem, capacity, "assembly.native.input: the bundle has no verified program");
        return NULL;
    }
    NativeBundleCode *code = calloc(1, sizeof(*code));
    if(code == NULL) {
        snprintf(problem, capacity, "assembly.native.allocation: cannot allocate native code");
        return NULL;
    }
    ZxDebugOwner debug = {0};
    LirProgram restored = {0};
    const LirProgram *source = lir;
    if(ExecBundleDebugProgram(bundle, &debug)) {
        restored = compiler_exec_debug_ZxDebugProgram(&debug);
        source = &restored;
    }
    MachineDiagnostic diagnostic = compiler_machine_pipeline_MachineCodeBuild(
        *source, compiler_target_TargetLinuxX64(),
        compiler_machine_ir_MachineDefaultLimits(), &code->owner);
    compiler_exec_debug_ZxDebugFree(&debug);
    if(diagnostic.problem != MachineProblem_None) {
        const LirFunction *functions = lir->functions.data;
        String name = {"", 0};
        if(diagnostic.function >= 0 && diagnostic.function < lir->functions.length)
            name = functions[diagnostic.function].name;
        snprintf(problem, capacity,
            "assembly.gap.machine-%s: native code generation rejected %.*s "
            "(function=%d block=%d instruction=%d); no alternate engine was selected",
            machine_problem_name(diagnostic.problem),
            (int)name.length, name.data, diagnostic.function, diagnostic.block,
            diagnostic.instruction);
        native_free_code(code);
        return NULL;
    }
    MachineImage image = compiler_machine_pipeline_MachineCodeImageView(&code->owner);
    MachineRelocation *relocations = image.relocations.data;
    for(int64_t at = 0; at < image.relocations.length; at++) {
        MachineRelocation relocation = relocations[at];
        size_t index;
        if(relocation.kind == MachineRelocationKind_HostCapability &&
           !capability_index(bundle, relocation.symbol, &index)) {
            snprintf(problem, capacity,
                "assembly.native.host-symbol: host call %d is not a capability", relocation.symbol);
            native_free_code(code);
            return NULL;
        }
        if(relocation.kind == MachineRelocationKind_PlatformService &&
           ZxPlatformService(lir, relocation.symbol) == ZX_SERVICE_NONE) {
            snprintf(problem, capacity,
                "assembly.gap.platform-service: function %d is not a native platform service",
                relocation.symbol);
            native_free_code(code);
            return NULL;
        }
    }
    MachineFunction *functions = code->owner.machine.program.functions.data;
    MachineType *types = code->owner.machine.program.types.data;
    int32_t entry = code->owner.machine.program.entry;
    if(image.stack_limit_offset < 0 || image.pointer_offset <= 0 || entry < 0 ||
       entry >= code->owner.machine.program.functions.length ||
       functions[entry].result_type < 0 ||
       functions[entry].result_type >= code->owner.machine.program.types.length) {
        snprintf(problem, capacity, "assembly.native.image: the native image has no checked entry");
        native_free_code(code);
        return NULL;
    }
    code->has_result = types[functions[entry].result_type].kind != MachineTypeKind_Void;
    return code;
}

/* Drops idle entries beyond NATIVE_CACHE_IDLE, oldest first. Holds the lock. */
static void
native_evict(void)
{
    for(;;) {
        size_t idle = 0;
        NativeBundleCode **oldest = NULL;
        for(NativeBundleCode **at = &native_cache; *at != NULL; at = &(*at)->next) {
            if((*at)->references != 0) continue;
            idle++;
            if(oldest == NULL || (*at)->idle < (*oldest)->idle) oldest = at;
        }
        if(idle <= NATIVE_CACHE_IDLE || oldest == NULL) return;
        NativeBundleCode *evicted = *oldest;
        *oldest = evicted->next;
        native_free_code(evicted);
    }
}

static void
native_slot_prepare(NativeBundleSlot *slot, const ExecBundle *bundle,
                    const unsigned char digest[32])
{
    pthread_mutex_lock(&native_lock);
    if(!slot->ready) {
        uint64_t cpu = native_cpu();
        for(NativeBundleCode *code = native_cache; code != NULL; code = code->next)
            if(code->cpu == cpu && !memcmp(code->digest, digest, 32)) {
                code->references++;
                slot->code = code;
                break;
            }
        if(slot->code == NULL) {
            NativeBundleCode *code = native_compile(bundle, slot->problem, sizeof(slot->problem));
            if(code != NULL) {
                memcpy(code->digest, digest, 32);
                code->cpu = cpu;
                code->references = 1;
                code->next = native_cache;
                native_cache = code;
                slot->code = code;
            }
        }
        if(slot->code != NULL) slot->problem[0] = 0;
        slot->ready = 1;
    }
    pthread_mutex_unlock(&native_lock);
}

const NativeBundleCode *
NativeBundleSlotCode(NativeBundleSlot *slot, const ExecBundle *bundle,
                     const unsigned char digest[32])
{
    if(slot == NULL || bundle == NULL || digest == NULL) return NULL;
    native_slot_prepare(slot, bundle, digest);
    return slot->code;
}

const char *
NativeBundleSlotProblem(NativeBundleSlot *slot, const ExecBundle *bundle,
                        const unsigned char digest[32])
{
    if(slot == NULL || bundle == NULL || digest == NULL) return "assembly.native.input: no bundle";
    native_slot_prepare(slot, bundle, digest);
    return slot->problem;
}

void
NativeBundleSlotClose(NativeBundleSlot *slot)
{
    if(slot == NULL || slot->code == NULL) return;
    pthread_mutex_lock(&native_lock);
    if(--slot->code->references == 0) {
        slot->code->idle = ++native_clock;
        native_evict();
    }
    pthread_mutex_unlock(&native_lock);
    slot->code = NULL;
    slot->ready = 0;
}

/* ---- Traps and their diagnostics --------------------------------------- */

static const char *
native_trap_text(uint32_t code)
{
    if(code == NATIVE_TRAP_BOUNDS) return "an index or address is out of bounds or no longer valid";
    if(code == NATIVE_TRAP_HOST) return "a host call failed";
    if(code == MachineX64TrapUnreachable) return "unreachable code executed";
    if(code == MachineX64TrapDivideByZero) return "integer division by zero";
    if(code == MachineX64TrapShift) return "shift count out of range";
    if(code == MachineX64TrapFuel) return "the step limit ran out";
    if(code == MachineX64TrapConversion) return "conversion out of range";
    if(code == MachineX64TrapStack) return "nested calls filled the stack";
    if(code == MachineX64TrapStorage) return "the program ran out of value storage";
    return "the program trapped";
}

/* The portable engine's problem name for the same failure. */
static const char *
native_trap_problem(uint32_t code)
{
    if(code == NATIVE_TRAP_BOUNDS) return "pointer";
    if(code == NATIVE_TRAP_HOST) return "host";
    if(code == MachineX64TrapUnreachable) return "unreachable";
    if(code == MachineX64TrapDivideByZero) return "divide-by-zero";
    if(code == MachineX64TrapShift) return "invalid-shift";
    if(code == MachineX64TrapFuel) return "fuel-limit";
    if(code == MachineX64TrapConversion) return "invalid-conversion";
    if(code == MachineX64TrapStack) return "frame-limit";
    if(code == MachineX64TrapStorage) return "value-bytes";
    return "trap";
}

/* Names the function, block and source origin of a trap's return address. */
static void
native_trap_site(const NativeInstance *instance, char *text, size_t capacity)
{
    const MachineNativeOwner *owner = &instance->code->owner;
    MachineX64Image image = owner->encoded.image;
    MachineProgram program = owner->machine.program;
    uintptr_t base = (uintptr_t)instance->arena.address;
    snprintf(text, capacity, "function=? block=?");
    if(instance->trap_pc <= base || instance->trap_pc - base > (uintptr_t)image.code.length)
        return;
    int64_t offset = (int64_t)(instance->trap_pc - base) - 1;
    const MachineX64Function *extents = image.functions.data;
    const MachineX64Range *ranges = image.instructions.data;
    const MachineFunction *functions = program.functions.data;
    const MachineBlock *blocks = program.blocks.data;
    const MachineInstruction *instructions = program.instructions.data;
    const MachineOrigin *origins = program.origins.data;
    const MachineSource *sources = program.sources.data;
    const char *names = (const char *)program.text.data;
    for(int64_t id = 0; id < image.functions.length && id < program.functions.length; id++) {
        if(offset < extents[id].begin || offset >= extents[id].end) continue;
        MachineFunction function = functions[id];
        int32_t block = -1, origin = function.origin;
        for(int32_t at = function.block_begin; at < function.block_begin + function.block_count; at++)
            for(int32_t instruction = blocks[at].instruction_begin;
                instruction < blocks[at].instruction_begin + blocks[at].instruction_count; instruction++)
                if(offset >= ranges[instruction].begin && offset < ranges[instruction].end) {
                    block = blocks[at].lir_block;
                    if(instructions[instruction].origin >= 0) origin = instructions[instruction].origin;
                }
        int written = snprintf(text, capacity, "function=%.*s block=%d",
                               (int)function.name_length, names + function.name_offset, block);
        if(origin >= 0 && origin < program.origins.length && written > 0 && (size_t)written < capacity) {
            MachineOrigin where = origins[origin];
            if(where.file >= 0 && where.file < program.sources.length)
                snprintf(text + written, capacity - (size_t)written, " at %.*s:%d:%d",
                         (int)sources[where.file].text_length, names + sources[where.file].text_offset,
                         where.line, where.column);
        }
        return;
    }
}

static void
native_report(const NativeInstance *instance, int initializing)
{
    fflush(stdout);
    if(instance->trap_code == MachineX64TrapFuel && instance->limit > 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.runtime", "native execution failed: stopped after %lld steps",
            instance->limit);
        return;
    }
    char site[512];
    native_trap_site(instance, site, sizeof(site));
    Diagnostic((ZirSourceSpan){0}, "zx.runtime",
        "native execution failed in %s: %s (problem=%s trap=0x%08x %s)",
        initializing ? "global initialization" : ExecBundleEntryFunction(instance->bundle),
        native_trap_text(instance->trap_code), native_trap_problem(instance->trap_code),
        (unsigned)instance->trap_code, site);
}

static _Noreturn void
NativeTrap(NativeInstance *instance, uint32_t code, uintptr_t at)
{
    if(instance == NULL || instance->active == NULL)
        abort();
    if(instance->trap_offset >= 0 && instance->data_size >= sizeof(code) &&
       (uint64_t)instance->trap_offset <= instance->data_size - sizeof(code))
        memcpy(instance->data + instance->trap_offset, &code, sizeof(code));
    instance->trap_code = code;
    instance->trap_pc = at;
    instance->terminal = 1;
    longjmp(instance->active->trap, 1);
}

/* ---- Host calls ---------------------------------------------------------- */

static int64_t
native_type_size(const LirProgram *program, int32_t type, int depth)
{
    if(depth > 256 || type < 0 || type >= program->types.length)
        return -1;
    const LirType *shape = &((const LirType *)program->types.data)[type];
    if(shape->kind == LirTypeKind_Void) return 0;
    if(shape->kind == LirTypeKind_Bool) return 1;
    if(shape->kind == LirTypeKind_Integer || shape->kind == LirTypeKind_Float)
        return shape->bits > 0 && shape->bits <= 64 ? (shape->bits + 7) / 8 : -1;
    if(shape->kind == LirTypeKind_Pointer || shape->kind == LirTypeKind_Function)
        return 8;
    if(shape->kind == LirTypeKind_Array) {
        int64_t element = native_type_size(program, shape->element, depth + 1);
        if(element < 0 || shape->count < 0 ||
           shape->count > INT64_MAX / (element > 0 ? element : 1))
            return -1;
        return element * shape->count;
    }
    if(shape->kind == LirTypeKind_Record) {
        int64_t size = 0;
        const int32_t *fields = shape->fields.data;
        for(int64_t at = 0; at < shape->fields.length; at++) {
            int64_t field = native_type_size(program, fields[at], depth + 1);
            if(field < 0 || size > INT64_MAX - field) return -1;
            size += field;
        }
        return size;
    }
    return -1;
}

/* Native registers hold signed integers sign-extended and the rest
 * zero-extended; the host bridge takes and gives canonical bits, the value's
 * own width with nothing above it. */
static uint64_t
native_canonical(const LirType *type, uint64_t bits)
{
    if(type->kind == LirTypeKind_Bool) return bits & 1;
    if(type->kind != LirTypeKind_Integer || type->bits <= 0 || type->bits >= 64) return bits;
    return bits & ((UINT64_C(1) << type->bits) - 1);
}

static uint64_t
native_register(const LirType *type, uint64_t bits)
{
    if(type->kind != LirTypeKind_Integer || type->bits <= 0 || type->bits >= 64) return bits;
    bits &= (UINT64_C(1) << type->bits) - 1;
    if(type->is_signed && (bits >> (type->bits - 1)) != 0) bits |= ~((UINT64_C(1) << type->bits) - 1);
    return bits;
}

static size_t
round_eight(size_t value)
{
    return value <= SIZE_MAX - 7 ? (value + 7) & ~(size_t)7 : 0;
}

static int
native_arguments(NativeInstance *instance, int32_t function,
                 NativeAbiFrame *frame, ExecHostNativeValue **values,
                 size_t *count, unsigned char **aggregate_result)
{
    const LirProgram *program = ExecBundleProgram(instance->bundle);
    if(function < 0 || function >= program->functions.length)
        return 0;
    const LirFunction *declared =
        &((const LirFunction *)program->functions.data)[function];
    const LirType *types = program->types.data;
    const int32_t *parameters = declared->parameters.data;
    size_t parameters_count = (size_t)declared->parameters.length;
    ExecHostNativeValue *arguments =
        calloc(parameters_count ? parameters_count : 1, sizeof(*arguments));
    if(arguments == NULL) return 0;
    int general = 0, floating = 0;
    size_t stack = 0;
    const LirType *result = &types[declared->result];
    *aggregate_result = NULL;
    if(result->kind == LirTypeKind_Array ||
       result->kind == LirTypeKind_Record) {
        *aggregate_result = frame->result_address;
        if(*aggregate_result == NULL) {
            free(arguments);
            return 0;
        }
    }
    for(size_t at = 0; at < parameters_count; at++) {
        int32_t type = parameters[at];
        const LirType *shape = &types[type];
        int64_t width = native_type_size(program, type, 0);
        if(width < 0) goto failed;
        arguments[at].type = type;
        if(shape->kind == LirTypeKind_Float && floating < 8) {
            memcpy(&arguments[at].bits, frame->floating[floating++],
                   (size_t)width);
        } else if(shape->kind != LirTypeKind_Array &&
                  shape->kind != LirTypeKind_Record && general < 6) {
            arguments[at].bits = native_canonical(shape, frame->general[general++]);
        } else if(shape->kind == LirTypeKind_Array ||
                  shape->kind == LirTypeKind_Record) {
            size_t aligned = round_eight((size_t)width);
            if(aligned == 0 && width != 0) goto failed;
            arguments[at].bytes = (unsigned char *)frame->stack + stack;
            arguments[at].size = (size_t)width;
            if(stack > SIZE_MAX - aligned) goto failed;
            stack += aligned;
        } else {
            size_t aligned = round_eight((size_t)width);
            if(aligned == 0 && width != 0) goto failed;
            memcpy(&arguments[at].bits, frame->stack + stack,
                   (size_t)width);
            arguments[at].bits = native_canonical(shape, arguments[at].bits);
            if(stack > SIZE_MAX - aligned) goto failed;
            stack += aligned;
        }
    }
    *values = arguments;
    *count = parameters_count;
    return 1;
failed:
    free(arguments);
    return 0;
}

static int
native_platform_call(NativeInstance *instance, int32_t function,
                     ExecHostNativeValue *arguments, size_t count,
                     ExecHostNativeValue *result)
{
    int service = ZxPlatformService(ExecBundleProgram(instance->bundle), function);
    result->bits = 0;
    if(service == ZX_SERVICE_PRINT_BYTES) {
        if(count != 3 ||
           (arguments[1].bits == 0 && arguments[2].bits != 0))
            return 0;
        FILE *stream = arguments[0].bits == 2 ? stderr :
            arguments[0].bits == 1 ? stdout : NULL;
        if(stream == NULL) return 0;
        const unsigned char *bytes =
            (const unsigned char *)(uintptr_t)arguments[1].bits;
        size_t left = (size_t)arguments[2].bits;
        while(left != 0) {
            size_t written = fwrite(bytes, 1, left, stream);
            if(written == 0) return 0;
            bytes += written;
            left -= written;
        }
        return 1;
    }
    if(count != 1) return 0;
    char text[64];
    if(service == ZX_SERVICE_PRINT_SIGNED)
        snprintf(text, sizeof(text), "%lld", (long long)arguments[0].bits);
    else if(service == ZX_SERVICE_PRINT_UNSIGNED)
        snprintf(text, sizeof(text), "%llu",
                 (unsigned long long)arguments[0].bits);
    else if(service == ZX_SERVICE_PRINT_BOOL)
        snprintf(text, sizeof(text), "%s",
                 arguments[0].bits ? "true" : "false");
    else if(service == ZX_SERVICE_PRINT_FLOAT32) {
        uint32_t bits = (uint32_t)arguments[0].bits;
        float value;
        memcpy(&value, &bits, sizeof(value));
        FormatPrintFloat(value, 1, text, sizeof(text));
    } else if(service == ZX_SERVICE_PRINT_FLOAT64) {
        double value;
        memcpy(&value, &arguments[0].bits, sizeof(value));
        FormatPrintFloat(value, 0, text, sizeof(text));
    } else return 0;
    return fputs(text, stdout) >= 0;
}

static uint64_t
NativeHostInvoke(NativeInstance *instance, int32_t function,
                 NativeAbiFrame *frame)
{
    if(instance == NULL || frame == NULL || instance->active == NULL)
        abort();
    ExecHostNativeValue *arguments = NULL, returned = {0};
    size_t count = 0;
    unsigned char *aggregate_result = NULL;
    if(!native_arguments(instance, function, frame, &arguments, &count,
                         &aggregate_result))
        NativeTrap(instance, NATIVE_TRAP_HOST, 0);
    size_t host = 0;
    int ok;
    if(capability_index(instance->bundle, function, &host))
        ok = ExecHostsCallNative(instance->hosts, host, arguments, count,
                                 &returned);
    else
        ok = native_platform_call(instance, function, arguments, count,
                                  &returned);
    free(arguments);
    if(!ok)
        NativeTrap(instance, NATIVE_TRAP_HOST, 0);
    const LirProgram *program = ExecBundleProgram(instance->bundle);
    const LirFunction *declared =
        &((const LirFunction *)program->functions.data)[function];
    const LirType *result =
        &((const LirType *)program->types.data)[declared->result];
    if(result->kind == LirTypeKind_Array ||
       result->kind == LirTypeKind_Record) {
        /* An empty record or array result has no bytes to copy. */
        int64_t expected = native_type_size(program, declared->result, 0);
        if(expected < 0 || returned.size != (size_t)expected ||
           (expected > 0 && returned.bytes == NULL))
            NativeTrap(instance, NATIVE_TRAP_HOST, 0);
        if(expected > 0) memcpy(aggregate_result, returned.bytes, returned.size);
    } else if(result->kind == LirTypeKind_Float)
        frame->floating_result = returned.bits;
    return native_register(result, returned.bits);
}

/* ---- Resizable storage --------------------------------------------------- */

/* Resize(DATA, OLD_COUNT, NEW_COUNT, ELEMENT): a block of NEW_COUNT elements
 * holding the first OLD_COUNT, or none for zero. DATA is null with no
 * elements, or a block this instance returned with exactly OLD_COUNT of them;
 * anything else traps. New elements are zero. */
static uint64_t
NativeResize(uint64_t data, uint64_t old_count, uint64_t new_count, uint64_t element,
             NativeInstance *instance)
{
    if(instance == NULL || instance->active == NULL) abort();
    NativeBlock *old = NULL;
    if(data == 0) {
        if(old_count != 0) NativeTrap(instance, NATIVE_TRAP_BOUNDS, 0);
    } else {
        old = (NativeBlock *)(uintptr_t)(data - sizeof(NativeBlock));
        if(data < sizeof(NativeBlock) || old->magic != NATIVE_HEAP_MAGIC ||
           old->owner != instance || old->count != old_count || old->element != element)
            NativeTrap(instance, NATIVE_TRAP_BOUNDS, 0);
    }
    size_t old_bytes = (size_t)(old_count * element);
    if(new_count == 0) {
        if(old != NULL) {
            if(old->previous) old->previous->next = old->next;
            else instance->blocks = old->next;
            if(old->next) old->next->previous = old->previous;
            old->magic = 0;
            instance->heap_bytes -= old_bytes;
            free(old);
        }
        return 0;
    }
    /* A request that does not fit gives null and keeps the old block, as on
     * the portable engine. */
    if(element != 0 && new_count > (NATIVE_HEAP_LIMIT - sizeof(NativeBlock)) / element)
        return 0;
    size_t new_bytes = (size_t)(new_count * element);
    if(instance->heap_bytes - old_bytes > NATIVE_HEAP_LIMIT - new_bytes)
        return 0;
    NativeBlock *block = calloc(1, sizeof(NativeBlock) + new_bytes);
    if(block == NULL) return 0;
    block->magic = NATIVE_HEAP_MAGIC;
    block->owner = instance;
    block->count = new_count;
    block->element = element;
    if(old != NULL) {
        memcpy(block + 1, old + 1, old_bytes < new_bytes ? old_bytes : new_bytes);
        if(old->previous) old->previous->next = old->next;
        else instance->blocks = old->next;
        if(old->next) old->next->previous = old->previous;
        old->magic = 0;
        free(old);
    }
    block->next = instance->blocks;
    if(block->next) block->next->previous = block;
    instance->blocks = block;
    instance->heap_bytes = instance->heap_bytes - old_bytes + new_bytes;
    return (uint64_t)(uintptr_t)(block + 1);
}

/* ---- Checked pointers ---------------------------------------------------- */

typedef struct NativeSlot {
    uint64_t base;
    uint64_t size;
    uint16_t version;
    uint16_t flags;
    uint32_t next;
    unsigned char reserved[8];
} NativeSlot;

_Static_assert(sizeof(NativeSlot) == MachinePointerEntryBytes, "pointer entry size");
_Static_assert(offsetof(NativeSlot, size) == MachinePointerEntrySize, "pointer entry size field");
_Static_assert(offsetof(NativeSlot, version) == MachinePointerEntryVersion, "pointer entry version");
_Static_assert(offsetof(NativeSlot, flags) == MachinePointerEntryFlags, "pointer entry flags");
_Static_assert(offsetof(NativeSlot, next) == MachinePointerEntryNext, "pointer entry next");

static uint64_t
native_header(const NativeInstance *instance, int word)
{
    uint64_t value;
    memcpy(&value, instance->pointer_header + word, sizeof(value));
    return value;
}

static void
native_set_header(NativeInstance *instance, int word, uint64_t value)
{
    memcpy(instance->pointer_header + word, &value, sizeof(value));
}

static NativeSlot *
native_slots(const NativeInstance *instance)
{
    return (NativeSlot *)instance->pointers.address;
}

/* The same checks as generated code: live slot, matching version, writable
 * for writes, and BYTES within the slot. */
static void *
native_resolve(void *context, uint64_t token, size_t bytes, int writable)
{
    NativeInstance *instance = context;
    uint64_t slot = token >> MachinePointerSlotShift;
    uint64_t version = (token >> MachinePointerVersionShift) & MachinePointerVersionMask;
    uint64_t offset = token & 0xffffffffu;
    if(slot >= MachinePointerCapacity) return NULL;
    NativeSlot *entry = &native_slots(instance)[slot];
    if(entry->version != version || !(entry->flags & MachinePointerLive) ||
       (writable && !(entry->flags & MachinePointerWritable)) ||
       bytes > entry->size || offset > entry->size - bytes)
        return NULL;
    return (void *)(uintptr_t)(entry->base + offset);
}

/* A live read-only slot for BYTES a host returned; 0 when none is left. */
static uint64_t
native_create(void *context, const void *address, size_t bytes)
{
    NativeInstance *instance = context;
    NativeSlot *slots = native_slots(instance);
    uint64_t slot = native_header(instance, MachinePointerHeaderFree);
    if(slot != 0) native_set_header(instance, MachinePointerHeaderFree, slots[slot].next);
    else {
        slot = native_header(instance, MachinePointerHeaderUnused);
        if(slot >= MachinePointerCapacity) return 0;
        native_set_header(instance, MachinePointerHeaderUnused, slot + 1);
    }
    slots[slot].base = (uint64_t)(uintptr_t)address;
    slots[slot].size = bytes;
    slots[slot].flags = MachinePointerLive;
    slots[slot].next = 0;
    return (slot << MachinePointerSlotShift) | ((uint64_t)slots[slot].version << MachinePointerVersionShift);
}

/* The header, slot 0 (live and empty at address 0) and a slot per global. */
static int
native_pointers_open(NativeInstance *instance, MachineImage image, const unsigned char *constants)
{
    size_t table = (size_t)(MachinePointerCapacity + 1) * MachinePointerEntryBytes;
    if(!ZirNativeReserve(table + MachinePointerArenaBytes, &instance->pointers)) return 0;
    instance->pointer_header = instance->data + image.pointer_offset;
    unsigned char *base = instance->pointers.address;
    MachineProgram program = instance->code->owner.machine.program;
    MachineGlobal *globals = program.globals.data;
    MachineType *types = program.types.data;
    int64_t *global_offsets = image.global_offsets.data;
    int64_t *constant_offsets = image.constant_offsets.data;
    if(program.globals.length + 1 >= MachinePointerCapacity) return 0;
    native_set_header(instance, MachinePointerHeaderFree, 0);
    native_set_header(instance, MachinePointerHeaderUnused, (uint64_t)program.globals.length + 1);
    native_set_header(instance, MachinePointerHeaderCapacity, MachinePointerCapacity);
    native_set_header(instance, MachinePointerHeaderArenaTop, (uint64_t)(uintptr_t)(base + table));
    native_set_header(instance, MachinePointerHeaderArenaLimit,
                      (uint64_t)(uintptr_t)(base + table + MachinePointerArenaBytes));
    native_set_header(instance, MachinePointerHeaderTable, (uint64_t)(uintptr_t)base);
    NativeSlot *slots = native_slots(instance);
    slots[0].flags = MachinePointerLive;
    for(int64_t at = 0; at < program.globals.length; at++) {
        NativeSlot *entry = &slots[at + 1];
        if(globals[at].type < 0 || globals[at].type >= program.types.length) return 0;
        if(globals[at].writable) {
            if(at >= image.global_offsets.length || global_offsets[at] < 0) return 0;
            entry->base = (uint64_t)(uintptr_t)(instance->data + global_offsets[at]);
            entry->flags = MachinePointerLive | MachinePointerWritable;
        } else {
            if(at >= image.constant_offsets.length || constant_offsets[at] < 0) return 0;
            entry->base = (uint64_t)(uintptr_t)(constants + constant_offsets[at]);
            entry->flags = MachinePointerLive;
        }
        entry->size = (uint64_t)types[globals[at].type].size;
        instance->global_bytes += (size_t)entry->size;
    }
    ExecHostsNativePointers(instance->hosts, native_resolve, native_create, instance);
    return 1;
}

/* ---- Running ------------------------------------------------------------- */

/* The lowest stack address generated code may reach on this thread, leaving
 * NATIVE_STACK_RESERVE for host calls and the C runtime. */
static uintptr_t
native_stack_limit(void)
{
    uintptr_t here = (uintptr_t)__builtin_frame_address(0);
    uintptr_t floor = here > NATIVE_STACK_MAXIMUM ? here - NATIVE_STACK_MAXIMUM : 0;
    pthread_attr_t attributes;
    void *low = NULL;
    size_t size = 0;
    if(pthread_getattr_np(pthread_self(), &attributes) == 0) {
        if(pthread_attr_getstack(&attributes, &low, &size) != 0) low = NULL;
        pthread_attr_destroy(&attributes);
    }
    uintptr_t bottom = (uintptr_t)low;
    if(low != NULL && size > NATIVE_STACK_RESERVE && here > bottom + NATIVE_STACK_RESERVE &&
       here - bottom <= size) {
        uintptr_t limit = bottom + NATIVE_STACK_RESERVE;
        return limit > floor ? limit : floor;
    }
    return here > NATIVE_STACK_FALLBACK ? here - NATIVE_STACK_FALLBACK : 0;
}

static int64_t
native_fuel(const NativeInstance *instance)
{
    int64_t fuel;
    memcpy(&fuel, instance->data + instance->fuel_offset, sizeof(fuel));
    return fuel;
}

/* 1 after a return, 0 for an instance a trap already ended or whose steps
 * ran out before it started, -1 for a trap now, which the caller reports. */
static int
native_call(NativeInstance *instance, void *address, long long *result)
{
    if(instance == NULL || address == NULL || instance->terminal)
        return 0;
    int64_t budget = INT64_MAX;
    if(instance->limit > 0)
        budget = instance->cumulative ? instance->limit - instance->used : instance->limit;
    if(budget <= 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.runtime", "native execution failed: stopped after %lld steps",
            instance->limit);
        instance->terminal = 1;
        return 0;
    }
    if(instance->active == NULL)
        memcpy(instance->data + instance->fuel_offset, &budget, sizeof(budget));
    int called = native_call_entry(instance, address, result);
    if(instance->cumulative && instance->active == NULL) {
        int64_t left = native_fuel(instance);
        instance->used += budget - (left > 0 ? left : 0);
    }
    return called;
}

static int
native_call_entry(NativeInstance *instance, void *address, long long *result)
{
    /* The outermost call on this thread sets the limit; a host callback
     * that re-enters keeps it. */
    if(instance->active == NULL) {
        uint64_t limit = native_stack_limit();
        memcpy(instance->data + instance->stack_limit_offset, &limit, sizeof(limit));
    }
    NativeActiveCall call = {.previous = instance->active};
    instance->active = &call;
    if(setjmp(call.trap) != 0) {
        instance->active = call.previous;
        return -1;
    }
    long long (*entry)(void);
    memcpy(&entry, &address, sizeof(entry));
    long long value = entry();
    instance->active = call.previous;
    if(result != NULL) *result = value;
    return 1;
}

NativeInstance *
NativeInstanceOpen(const NativeBundleCode *code, const ExecBundle *bundle,
                   NativeFindBinding find, void *context, long long max_steps)
{
    if(code == NULL || bundle == NULL) return NULL;
    MachineImage image = compiler_machine_pipeline_MachineCodeImageView(
        (MachineNativeOwner *)&code->owner);
    if(image.code.bytes.data == NULL || image.code.bytes.length <= 0 ||
       image.entry < 0 || image.entry >= image.function_offsets.length ||
       image.stack_limit_offset < 0)
        return NULL;
    NativeInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL) return NULL;
    instance->code = code;
    instance->bundle = bundle;
    instance->hosts = ExecHostsOpen(bundle, (ExecFindBinding)find, context);
    if(instance->hosts == NULL) goto failed;
    size_t page = ZirNativePageSize();
    size_t code_size = (size_t)image.code.bytes.length;
    size_t stubs = (size_t)image.relocations.length;
    if(page == 0 || stubs > (SIZE_MAX - code_size) / NATIVE_HOST_STUB_SIZE)
        goto failed;
    code_size += stubs * NATIVE_HOST_STUB_SIZE;
    if(code_size > SIZE_MAX - NATIVE_TRAP_STUB_SIZE - NATIVE_RESIZE_STUB_SIZE) goto failed;
    size_t trap_at = code_size;
    code_size += NATIVE_TRAP_STUB_SIZE;
    size_t resize_at = code_size;
    code_size += NATIVE_RESIZE_STUB_SIZE;
    size_t constants_at = align_size(code_size,
        image.constants.alignment > 0 ? (size_t)image.constants.alignment : 1);
    if(constants_at == 0 ||
       (uint64_t)image.constants.bytes.length > SIZE_MAX - constants_at)
        goto failed;
    code_size = constants_at + (size_t)image.constants.bytes.length;
    size_t code_pages = align_size(code_size, page);
    if(code_pages == 0 ||
       (uint64_t)image.data.bytes.length > SIZE_MAX - code_pages)
        goto failed;
    if(!ZirNativeReserve(code_pages + (size_t)image.data.bytes.length,
                         &instance->arena))
        goto failed;
    unsigned char *base = instance->arena.address;
    instance->data = base + code_pages;
    instance->data_size = instance->arena.size - code_pages;
    instance->fuel_offset = image.fuel_offset;
    instance->trap_offset = image.fuel_offset + 8;
    instance->stack_limit_offset = image.stack_limit_offset;
    instance->has_result = code->has_result;
    instance->limit = max_steps > 0 ? max_steps : 0;
    instance->cumulative = max_steps > 0;
    if(instance->data_size < 8 ||
       (uint64_t)image.stack_limit_offset > instance->data_size - 8) goto failed;
    memcpy(base, image.code.bytes.data, (size_t)image.code.bytes.length);
    memcpy(base + constants_at, image.constants.bytes.data,
           (size_t)image.constants.bytes.length);
    memcpy(instance->data, image.data.bytes.data,
           (size_t)image.data.bytes.length);
    if(image.pointer_offset <= 0 ||
       (uint64_t)image.pointer_offset > instance->data_size - MachinePointerHeaderBytes ||
       !native_pointers_open(instance, image, base + constants_at))
        goto failed;
    size_t trap_written;
    if(!write_trap_stub(base + trap_at, NATIVE_TRAP_STUB_SIZE, instance,
                        &trap_written) ||
       !write_resize_stub(base + resize_at, NATIVE_RESIZE_STUB_SIZE, instance))
        goto failed;
    MachineRelocation *relocations = image.relocations.data;
    size_t stub_at = (size_t)image.code.bytes.length;
    for(int64_t at = 0; at < image.relocations.length; at++) {
        MachineRelocation relocation = relocations[at];
        if(relocation.section != MachineSectionKind_Code ||
           relocation.width != 4 || relocation.offset < 0 ||
           relocation.offset > image.code.bytes.length - 4)
            goto failed;
        uintptr_t target;
        if(relocation.kind == MachineRelocationKind_GlobalAddress) {
            int32_t symbol = relocation.symbol;
            int64_t *offsets = image.global_offsets.data;
            if(symbol < 0 || symbol >= image.global_offsets.length ||
               offsets[symbol] < 0 ||
               (uint64_t)offsets[symbol] >= instance->data_size)
                goto failed;
            target = (uintptr_t)(instance->data + offsets[symbol]);
        } else if(relocation.kind == MachineRelocationKind_ConstantAddress) {
            int32_t symbol = relocation.symbol;
            int64_t *offsets = image.constant_offsets.data;
            if(symbol < 0 || symbol >= image.constant_offsets.length ||
               offsets[symbol] < 0 ||
               offsets[symbol] >= image.constants.bytes.length)
                goto failed;
            target = (uintptr_t)(base + constants_at + offsets[symbol]);
        } else if(relocation.kind == MachineRelocationKind_FuelCounter &&
                  relocation.symbol == 0) {
            if(image.fuel_offset < 0 ||
               image.fuel_offset > (int64_t)instance->data_size - 8)
                goto failed;
            target = (uintptr_t)(instance->data + image.fuel_offset);
        } else if(relocation.kind == MachineRelocationKind_StackLimit &&
                  relocation.symbol == 0) {
            target = (uintptr_t)(instance->data + image.stack_limit_offset);
        } else if(relocation.kind == MachineRelocationKind_PointerRuntime &&
                  relocation.symbol == 0) {
            target = (uintptr_t)(instance->data + image.pointer_offset);
        } else if(relocation.kind == MachineRelocationKind_RuntimeTrap) {
            target = (uintptr_t)(base + trap_at);
        } else if(relocation.kind == MachineRelocationKind_RuntimeResize &&
                  relocation.symbol == 0) {
            target = (uintptr_t)(base + resize_at);
        } else if(relocation.kind == MachineRelocationKind_HostCapability ||
                  relocation.kind == MachineRelocationKind_PlatformService) {
            size_t written;
            if(!write_host_stub(base + stub_at, NATIVE_HOST_STUB_SIZE,
                                instance, relocation.symbol, &written))
                goto failed;
            target = (uintptr_t)(base + stub_at);
            stub_at += NATIVE_HOST_STUB_SIZE;
        } else {
            goto failed;
        }
        if(!write_relative32(base + relocation.offset, target,
                             relocation.addend))
            goto failed;
    }
    int64_t *functions = image.function_offsets.data;
    if(functions[image.entry] < 0 ||
       functions[image.entry] >= image.code.bytes.length)
        goto failed;
    instance->entry = base + functions[image.entry];
    ZirNativeMapping code_mapping = {.address = base, .size = code_pages};
    if(!ZirNativeFlushInstructions(&code_mapping) ||
       !ZirNativeProtectRX(&code_mapping))
        goto failed;
    int32_t *initializers = image.initializers.data;
    for(int64_t at = 0; at < image.initializers.length; at++) {
        int32_t function = initializers[at];
        if(function < 0 || function >= image.function_offsets.length ||
           functions[function] < 0 ||
           functions[function] >= image.code.bytes.length)
            goto failed;
        int called = native_call(instance, base + functions[function], NULL);
        if(called != 1) {
            if(called < 0) native_report(instance, 1);
            NativeInstanceClose(instance);
            return NULL;
        }
    }
    return instance;
failed:
    Diagnostic((ZirSourceSpan){0}, "zx.native",
        "cannot load the native image of %s; no alternate engine was selected",
        ExecBundleEntryFunction(bundle));
    NativeInstanceClose(instance);
    return NULL;
}

int
NativeInstanceRun(NativeInstance *instance, long long *result, int *has_result)
{
    if(instance == NULL || result == NULL || has_result == NULL)
        return 0;
    long long value = 0;
    int called = native_call(instance, instance->entry, &value);
    if(called != 1) {
        if(called < 0) native_report(instance, 0);
        return 0;
    }
    fflush(stdout);
    fflush(stderr);
    *result = value;
    *has_result = instance->has_result;
    return 1;
}

void
NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps)
{
    if(instance == NULL || instance->terminal) return;
    instance->limit = max_steps > 0 ? max_steps : 0;
}

void
NativeInstanceMatchFieldsByName(NativeInstance *instance, int enabled)
{
    if(instance != NULL)
        ExecHostsMatchFieldsByName(instance->hosts, enabled);
}

size_t
NativeInstanceStateCount(const NativeInstance *instance)
{
    return instance != NULL ? ExecBundleStateCount(instance->bundle) : 0;
}

/* The native bytes of state INDEX: its global's slot names them. */
static unsigned char *
native_state_bytes(NativeInstance *instance, size_t index, const char **module, const char **name,
                   const char **type)
{
    int32_t global = -1;
    if(!ExecBundleStateAt(instance->bundle, index, module, name, type, &global) ||
       global < 0 || (uint64_t)global + 1 >= MachinePointerCapacity)
        return NULL;
    return (unsigned char *)(uintptr_t)native_slots(instance)[global + 1].base;
}

int
NativeInstanceStateAt(NativeInstance *instance, size_t index, const char **module,
                      const char **name, const char **type, struct HostValue *value)
{
    if(instance == NULL || instance->terminal) return 0;
    unsigned char *bytes = native_state_bytes(instance, index, module, name, type);
    if(bytes == NULL) return 0;
    if(value == NULL) return 1;
    return ExecHostsNativeStateValue(instance->hosts, index, bytes, value);
}

int
NativeInstanceStateStore(NativeInstance *instance, const struct HostValue *const *values)
{
    if(instance == NULL || values == NULL || instance->terminal) return 0;
    size_t count = ExecBundleStateCount(instance->bundle);
    for(size_t index = 0; index < count; index++)
        if(values[index] != NULL && !ExecHostsNativeStateCheck(instance->hosts, index, values[index]))
            return 0;
    for(size_t index = 0; index < count; index++) {
        if(values[index] == NULL) continue;
        const char *module, *name, *type;
        unsigned char *bytes = native_state_bytes(instance, index, &module, &name, &type);
        if(bytes == NULL || !ExecHostsNativeStateStore(instance->hosts, index, values[index], bytes))
            return 0;
    }
    return 1;
}

size_t
NativeInstanceLiveBytes(const NativeInstance *instance)
{
    return instance != NULL ? instance->global_bytes + instance->heap_bytes : 0;
}

void
NativeInstanceClose(NativeInstance *instance)
{
    if(instance == NULL) return;
    for(NativeBlock *block = instance->blocks; block != NULL;) {
        NativeBlock *next = block->next;
        block->magic = 0;
        free(block);
        block = next;
    }
    ExecHostsClose(instance->hosts);
    ZirNativeRelease(&instance->pointers);
    ZirNativeRelease(&instance->arena);
    free(instance);
}

#endif
