/*
 * zir_host_native.c - native execution of verified ZX on linux-x86_64,
 * linux-aarch64 and linux-armv7, Android included.
 *
 * The machine pipeline compiles the bundle's verified program once per
 * content, CPU and backend. Each instance copies that image into RW pages,
 * resolves its relocations against its own data, host veneers and trap stub,
 * then flushes the instruction cache and publishes code RX before anything
 * runs; no page is ever writable and executable at once. This file is
 * target-neutral; zir_host_native_arch.h names the machine-code part each
 * architecture supplies. Fuel, the stack limit and traps live in the instance's
 * data, so instances are independent. A trap longjmps out of generated code
 * and leaves the instance terminal; nothing retries in another engine.
 */
#include "zir_host_native.h"
#include "zir_sha256.h"

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

#if defined(ZIRAN_NO_ENGINE) || !(defined(__x86_64__) || defined(__aarch64__) || defined(__arm__)) || \
    !defined(__linux__)

static const char native_unavailable[] =
    "assembly.gap.target-unimplemented: native ZX runs only on linux-x86_64, linux-aarch64 and "
    "linux-armv7; "
    "no alternate engine was selected";

const NativeBundleCode *
NativeBundleSlotCode(NativeBundleSlot *slot, const ExecBundle *bundle,
                     const unsigned char digest[32], int unbounded)
{
    (void)slot; (void)bundle; (void)digest; (void)unbounded;
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
#include "compiler_native_cache.h"
#include "compiler_target.h"
#include "compiler_aarch64.h"
#include "compiler_x86_64.h"
#include "compiler_x86_runtime.h"
#include "zir_exec_console.h"
#include "zir_profile.h"
#include "zir_host_native_arch.h"
#include "zir_text.h"

#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum {
    NATIVE_HOST_STUB_SIZE = 256,
    NATIVE_TRAP_STUB_SIZE = 64,
    NATIVE_RESIZE_STUB_SIZE = 384,
    /* Small blocks: 16-byte classes up to MachineX64HeapSmall bytes, carved
     * from chunks of this size. */
    NATIVE_HEAP_CLASSES = MachineX64HeapSmall / 16,
    NATIVE_HEAP_CHUNK = 1024 * 1024,
    /* Code compiled for a file nobody holds, kept for a reopen. */
    NATIVE_CACHE_IDLE = 4,
    /* Entries the disk cache keeps; the oldest go first. */
    NATIVE_DISK_ENTRIES = 64
};

/* Bytes the disk cache keeps, and the age after which a writer's temporary
 * file is taken as abandoned. */
#define NATIVE_DISK_BYTES ((off_t)512 * 1024 * 1024)
#define NATIVE_DISK_ABANDONED ((time_t)3600)

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

/* Compiled code, shared by every bundle with the same content digest on the
 * same CPU. The backend is this build's, so it is part of every key. */
struct NativeBundleCode {
    /* Compiled in this process (owner), or read from the disk cache
     * (cached). The image, globals and result flag come from either. */
    int compiled;
    MachineNativeOwner owner;
    NativeCacheOwner cached;
    MachineImage image;
    NativeCacheGlobal *globals;
    int64_t global_count;
    unsigned char digest[32];
    uint64_t cpu;
    int has_result;
    /* Built without fuel checks, for instances that never limit steps. */
    int unbounded;
    size_t references;
    /* While unreferenced: when it was released, for eviction order. */
    unsigned long long idle;
    NativeBundleCode *next;
};

/* The header before each block Resize returns: its element count and its
 * element size xor the heap's cookie, zero once the block is freed. */
typedef struct NativeHeader {
    uint64_t count;
    uint64_t tag;
} NativeHeader;

/* A block above MachineX64HeapSmall bytes is its own allocation, listed so
 * the instance frees it at close; smaller ones come from heap chunks. */
typedef struct NativeBlock {
    struct NativeBlock *previous, *next;
    NativeHeader header;
} NativeBlock;

typedef struct NativeChunk {
    struct NativeChunk *next;
    uint64_t unused;
} NativeChunk;

/* What the x86-64 Resize fast path (compiler_x86_runtime.zi) reads and
 * writes: the cookie, live bytes and their limit, the chunk's free bytes
 * and the free list of each 16-byte class. */
typedef struct NativeHeap {
    uint64_t cookie;
    uint64_t live;
    uint64_t limit;
    unsigned char *top;
    unsigned char *end;
    unsigned char *lists[NATIVE_HEAP_CLASSES];
} NativeHeap;

/* Block data keeps calloc's alignment. */
_Static_assert(sizeof(NativeBlock) % _Alignof(max_align_t) == 0, "block data alignment");
_Static_assert(sizeof(NativeChunk) % 16 == 0 && sizeof(NativeHeader) == 16, "chunk data alignment");
#if defined(__x86_64__)
_Static_assert(offsetof(NativeHeap, cookie) == MachineX64HeapCookie &&
               offsetof(NativeHeap, live) == MachineX64HeapLive &&
               offsetof(NativeHeap, limit) == MachineX64HeapLimit &&
               offsetof(NativeHeap, top) == MachineX64HeapTop &&
               offsetof(NativeHeap, end) == MachineX64HeapEnd &&
               offsetof(NativeHeap, lists) == MachineX64HeapLists, "native heap layout");
#endif

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
    /* Code without fuel checks: no step limit can apply. */
    int unbounded;
    /* For each function: its host capability's index, or -1; and the
     * platform service it names, resolved once instead of per call. */
    int32_t *capabilities;
    unsigned char *services;
    size_t function_count;
    /* Built with --checks: pointers are tokens of the slot table. */
    int checked;
    /* The read-only constants beside the code. */
    unsigned char *constants;
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
    NativeChunk *chunks;
    NativeHeap heap;
    /* Bytes of the program's globals, which the engine counts as live. */
    size_t global_bytes;
    /* Pointer slots persist; temporary frame storage lives only during an
     * outermost invocation, including any reentrant host callbacks. */
    ZirNativeMapping pointers;
    ZirNativeMapping frames;
    unsigned char *pointer_header;
    /* Standard output the program writes, handed to stdio once per run.
     * Last, so its buffer keeps the fields every call reads together. */
    ZxConsole console;
};

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

static _Noreturn void NativeTrap(NativeInstance *instance, uint32_t code, uintptr_t at);
static uint64_t NativeHostInvoke(NativeInstance *instance, int32_t function,
                                 NativeAbiFrame *frame);
static uint64_t NativeResize(uint64_t data, uint64_t old_count, uint64_t new_count,
                             uint64_t element, NativeInstance *instance);
static int native_call_entry(NativeInstance *instance, void *address, long long *result);

/* The target this process compiles for and keys the disk cache by. */
static TargetDescription
native_target(void)
{
#if defined(__x86_64__)
    return compiler_target_TargetLinuxX64();
#else
#if defined(__aarch64__)
    TargetArch arch = TargetArch_AArch64;
#else
    TargetArch arch = TargetArch_ArmV7;
#endif
#if defined(__ANDROID__)
    return compiler_target_TargetDescribe(arch, TargetOS_Android).target;
#else
    return compiler_target_TargetDescribe(arch, TargetOS_Linux).target;
#endif
#endif
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
    case MachineProblem_Version: return "version";
    case MachineProblem_Source: return "source";
    case MachineProblem_Origin: return "origin";
    case MachineProblem_Function: return "function";
    case MachineProblem_Block: return "block";
    case MachineProblem_Virtual: return "virtual";
    case MachineProblem_Instruction: return "instruction";
    case MachineProblem_Operand: return "operand";
    case MachineProblem_Flags: return "flags";
    case MachineProblem_Stack: return "stack";
    case MachineProblem_Edge: return "edge";
    case MachineProblem_Copy: return "copy";
    case MachineProblem_NativeWidth: return "native-width";
    case MachineProblem_Stage: return "stage";
    case MachineProblem_Fuel: return "fuel";
    case MachineProblem_Undefined: return "undefined";
    case MachineProblem_Interference: return "interference";
    case MachineProblem_Clobber: return "clobber";
    case MachineProblem_Tie: return "tie";
    case MachineProblem_Pressure: return "pressure";
    case MachineProblem_Cycle: return "cycle";
    case MachineProblem_Spill: return "spill";
    default: return "rejected";
    }
}

static void
native_free_code(NativeBundleCode *code)
{
    if(code->compiled) compiler_machine_pipeline_MachineNativeFree(&code->owner);
    else compiler_native_cache_NativeCacheFree(&code->cached);
    free(code->globals);
    free(code);
}

/* Every host and platform relocation names something this bundle has. */
static int
native_check_image(const ExecBundle *bundle, const LirProgram *lir, MachineImage image,
                   char *problem, size_t capacity)
{
    MachineRelocation *relocations = image.relocations.data;
    for(int64_t at = 0; at < image.relocations.length; at++) {
        MachineRelocation relocation = relocations[at];
        size_t index;
        if(relocation.kind == MachineRelocationKind_HostCapability &&
           !capability_index(bundle, relocation.symbol, &index)) {
            snprintf(problem, capacity,
                "assembly.native.host-symbol: host call %d is not a capability", relocation.symbol);
            return 0;
        }
        if(relocation.kind == MachineRelocationKind_PlatformService &&
           ZxPlatformService(lir, relocation.symbol) == ZX_SERVICE_NONE) {
            snprintf(problem, capacity,
                "assembly.gap.platform-service: function %d is not a native platform service",
                relocation.symbol);
            return 0;
        }
    }
    if(image.stack_limit_offset < 0 || image.pointer_offset <= 0 || image.entry < 0 ||
       image.entry >= image.function_offsets.length) {
        snprintf(problem, capacity, "assembly.native.image: the native image has no checked entry");
        return 0;
    }
    return 1;
}

/* Compiles BUNDLE, with its debug attachment's origins when it has one: the
 * code is the same either way, and traps can then name source lines. */
static NativeBundleCode *
native_compile(const ExecBundle *bundle, int unbounded, char *problem, size_t capacity)
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
    code->compiled = 1;
    ZxDebugOwner debug = {0};
    LirProgram restored = {0};
    const LirProgram *source = lir;
    if(ExecBundleDebugProgram(bundle, &debug)) {
        restored = compiler_exec_debug_ZxDebugProgram(&debug);
        source = &restored;
    }
    code->unbounded = unbounded;
    MachineDiagnostic diagnostic = unbounded ?
        compiler_machine_pipeline_MachineCodeBuildUnbounded(*source, native_target(),
            compiler_machine_ir_MachineDefaultLimits(), &code->owner) :
        compiler_machine_pipeline_MachineCodeBuild(*source, native_target(),
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
    if(!native_check_image(bundle, lir, image, problem, capacity)) {
        native_free_code(code);
        return NULL;
    }
    MachineProgram program = code->owner.machine.program;
    MachineFunction *functions = program.functions.data;
    MachineType *types = program.types.data;
    MachineGlobal *globals = program.globals.data;
    int32_t entry = program.entry;
    if(entry < 0 || entry >= program.functions.length ||
       functions[entry].result_type < 0 || functions[entry].result_type >= program.types.length ||
       program.globals.length < 0 || program.globals.length + 1 >= MachinePointerCapacity) {
        snprintf(problem, capacity, "assembly.native.image: the native image has no checked entry");
        native_free_code(code);
        return NULL;
    }
    code->has_result = types[functions[entry].result_type].kind != MachineTypeKind_Void;
    code->global_count = program.globals.length;
    code->globals = calloc((size_t)(code->global_count > 0 ? code->global_count : 1), sizeof(*code->globals));
    if(code->globals == NULL) {
        snprintf(problem, capacity, "assembly.native.allocation: cannot allocate native code");
        native_free_code(code);
        return NULL;
    }
    for(int64_t at = 0; at < code->global_count; at++) {
        if(globals[at].type < 0 || globals[at].type >= program.types.length) {
            snprintf(problem, capacity, "assembly.native.image: global %lld has no type", (long long)at);
            native_free_code(code);
            return NULL;
        }
        code->globals[at].size = types[globals[at].type].size;
        code->globals[at].writable = globals[at].writable;
    }
    code->image = image;
    return code;
}

/* ---- The disk cache ------------------------------------------------------ */

/* The GNU build ID of the object that holds this code: any change to the
 * compiler that made a cached image changes it. */
typedef struct NativeBuildSearch {
    uintptr_t address;
    unsigned char id[64];
    size_t length;
} NativeBuildSearch;

/* An object linked without a build ID (a reproducible APK's library) is
 * identified by its loaded code: a 64-bit FNV-1a hash of every executable
 * segment's bytes, a few milliseconds even for a 12 MB library. */
static void
native_code_identity(struct dl_phdr_info *info, NativeBuildSearch *search)
{
    uint64_t hash = 14695981039346656037ull;
    uint64_t bytes = 0;
    for(int at = 0; at < info->dlpi_phnum; at++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[at];
        // Execute-only memory cannot be read, so only readable code is hashed.
        if(header->p_type != PT_LOAD || (header->p_flags & PF_X) == 0 || (header->p_flags & PF_R) == 0 ||
           header->p_filesz == 0) continue;
        const unsigned char *code = (const unsigned char *)(info->dlpi_addr + header->p_vaddr);
        size_t length = (size_t)header->p_filesz;
        size_t words = length / 4;
        for(size_t word = 0; word < words; word++) {
            uint32_t value;
            memcpy(&value, code + word * 4, 4);
            hash = (hash ^ value) * 1099511628211ull;
        }
        for(size_t tail = words * 4; tail < length; tail++) hash = (hash ^ code[tail]) * 1099511628211ull;
        hash = (hash ^ (uint64_t)length) * 1099511628211ull;
        bytes += length;
    }
    if(bytes == 0) return;
    static const char tag[] = "code";
    memcpy(search->id, tag, 4);
    for(int at = 0; at < 8; at++) search->id[4 + at] = (unsigned char)(hash >> (8 * at));
    search->length = 12;
}

static int
native_build_note(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    NativeBuildSearch *search = data;
    int contains = 0;
    for(int at = 0; at < info->dlpi_phnum; at++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[at];
        uintptr_t begin = (uintptr_t)info->dlpi_addr + header->p_vaddr;
        if(header->p_type == PT_LOAD && search->address >= begin &&
           search->address - begin < header->p_memsz) contains = 1;
    }
    if(!contains) return 0;
    for(int at = 0; at < info->dlpi_phnum; at++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[at];
        if(header->p_type != PT_NOTE) continue;
        const unsigned char *note = (const unsigned char *)(info->dlpi_addr + header->p_vaddr);
        size_t left = header->p_memsz;
        while(left >= sizeof(ElfW(Nhdr))) {
            const ElfW(Nhdr) *entry = (const ElfW(Nhdr) *)note;
            size_t name = (entry->n_namesz + 3u) & ~(size_t)3;
            size_t description = (entry->n_descsz + 3u) & ~(size_t)3;
            if(name > left - sizeof(*entry) || description > left - sizeof(*entry) - name) break;
            const unsigned char *text = note + sizeof(*entry);
            if(entry->n_type == NT_GNU_BUILD_ID && entry->n_namesz == 4 && !memcmp(text, "GNU", 4) &&
               entry->n_descsz > 0 && entry->n_descsz <= sizeof(search->id)) {
                memcpy(search->id, text + name, entry->n_descsz);
                search->length = entry->n_descsz;
                return 1;
            }
            note += sizeof(*entry) + name + description;
            left -= sizeof(*entry) + name + description;
        }
    }
    native_code_identity(info, search);
    return 1;
}

#if defined(__ANDROID__)
/* An Android app process has no HOME; its own cache directory is
 * /data/data/PACKAGE/cache, and its command line is PACKAGE, or
 * PACKAGE:PROCESS for a secondary process. A command such as zi2zx run from
 * a shell is no package and gets 0. */
static int
native_android_cache(char *path, size_t capacity)
{
    char name[256];
    int file = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if(file < 0) return 0;
    ssize_t got = read(file, name, sizeof(name) - 1);
    close(file);
    if(got <= 0) return 0;
    name[got] = 0;
    char *process = strchr(name, ':');
    if(process != NULL) *process = 0;
    if(name[0] == 0 || strchr(name, '.') == NULL || strchr(name, '/') != NULL) return 0;
    char cache[320];
    struct stat info;
    int written = snprintf(cache, sizeof(cache), "/data/data/%s/cache", name);
    if(written <= 0 || (size_t)written >= sizeof(cache) || stat(cache, &info) != 0 || !S_ISDIR(info.st_mode))
        return 0;
    written = snprintf(path, capacity, "%s/ziran/native", cache);
    return written > 0 && (size_t)written < capacity;
}
#endif

/* The cache directory: ZIRAN_NATIVE_CACHE names it, or "off" disables the
 * cache; otherwise $XDG_CACHE_HOME/ziran/native, an Android app's own cache
 * directory, or ~/.cache/ziran/native. */
static int
native_disk_directory(char *path, size_t capacity)
{
    const char *chosen = getenv("ZIRAN_NATIVE_CACHE");
    int written;
    if(chosen != NULL && (!strcmp(chosen, "off") || !strcmp(chosen, "0"))) return 0;
    if(chosen != NULL && chosen[0] != 0) written = snprintf(path, capacity, "%s", chosen);
    else {
        const char *cache = getenv("XDG_CACHE_HOME");
        const char *home = getenv("HOME");
        if(cache != NULL && cache[0] == '/') written = snprintf(path, capacity, "%s/ziran/native", cache);
#if defined(__ANDROID__)
        else if(native_android_cache(path, capacity)) written = (int)strlen(path);
#endif
        else if(home != NULL && home[0] == '/') written = snprintf(path, capacity, "%s/.cache/ziran/native", home);
        else return 0;
    }
    if(written <= 0 || (size_t)written >= capacity || path[0] != '/') return 0;
    for(char *at = path + 1; ; at++) {
        if(*at != '/' && *at != 0) continue;
        char saved = *at;
        *at = 0;
        int made = mkdir(path, 0700) == 0 || errno == EEXIST;
        *at = saved;
        if(!made) return 0;
        if(saved == 0) break;
    }
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

/* The entry's path under the cache directory, from its key. */
static int
native_disk_path(const NativeCacheDigest *key, const char *suffix, char *path, size_t capacity)
{
    if(!native_disk_directory(path, capacity)) return 0;
    size_t used = strlen(path);
    if(used + 1 + 64 + strlen(suffix) + 1 > capacity) return 0;
    path[used++] = '/';
    for(int at = 0; at < 32; at++) used += (size_t)snprintf(path + used, capacity - used, "%02x", key->bytes[at]);
    snprintf(path + used, capacity - used, "%s", suffix);
    return 1;
}

static int
native_disk_key(const unsigned char digest[32], uint64_t cpu, NativeCacheDigest *key)
{
    static NativeBuildSearch build;
    static int searched;
    if(!searched) {
        build.address = (uintptr_t)&native_disk_key;
        dl_iterate_phdr(native_build_note, &build);
        searched = 1;
    }
    if(build.length == 0) return 0;
    *key = compiler_native_cache_NativeCacheKey((Slice){(void *)digest, 32},
        native_target(), cpu, (Slice){build.id, (int64_t)build.length});
    return 1;
}

/* A verified entry for KEY, or NULL. A file that does not decode under its
 * key and digest, or whose image does not fit this bundle, is removed. */
static const char *
native_cache_reject_counter(NativeCacheFailure failure)
{
    switch(failure) {
    case NativeCacheFailure_TooLarge: return "native_cache_disk_reject_too_large";
    case NativeCacheFailure_Truncated: return "native_cache_disk_reject_truncated";
    case NativeCacheFailure_WrongFormat: return "native_cache_disk_reject_wrong_format";
    case NativeCacheFailure_WrongKey: return "native_cache_disk_reject_wrong_key";
    case NativeCacheFailure_WrongLength: return "native_cache_disk_reject_wrong_length";
    case NativeCacheFailure_WrongDigest: return "native_cache_disk_reject_wrong_digest";
    case NativeCacheFailure_WrongStructure: return "native_cache_disk_reject_wrong_structure";
    default: return NULL;
    }
}

static NativeBundleCode *
native_disk_load(const ExecBundle *bundle, const NativeCacheDigest *key)
{
    char path[4096];
    if(!native_disk_path(key, ".zxn", path, sizeof(path))) return NULL;
    int file = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if(file < 0) return NULL;
    struct stat info;
    unsigned char *bytes = NULL;
    size_t length = 0;
    if(fstat(file, &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0 &&
       info.st_size <= (off_t)NativeCacheMaximum) {
        length = (size_t)info.st_size;
        bytes = malloc(length);
        size_t done = 0;
        while(bytes != NULL && done < length) {
            ssize_t got = read(file, bytes + done, length - done);
            if(got < 0 && errno == EINTR) continue;
            if(got <= 0) break;
            done += (size_t)got;
        }
        if(done != length) { free(bytes); bytes = NULL; }
    }
    close(file);
    NativeBundleCode *code = bytes != NULL ? calloc(1, sizeof(*code)) : NULL;
    char problem[512];
    NativeCacheFailure cache_failure = NativeCacheFailure_None;
    int ok = code != NULL &&
        (cache_failure = compiler_native_cache_NativeCacheDecodeChecked(
            (Slice){bytes, (int64_t)length}, *key, &code->cached)) == NativeCacheFailure_None;
    free(bytes);
    if(ok) {
        code->image = code->cached.image.image;
        code->has_result = code->cached.has_result;
        code->global_count = compiler_native_cache_NativeCacheGlobalCount(&code->cached);
        code->globals = calloc((size_t)(code->global_count > 0 ? code->global_count : 1), sizeof(*code->globals));
        TargetDescription expected = native_target();
        ok = code->image.target.arch == expected.arch && code->image.target.os == expected.os &&
            code->globals != NULL && code->global_count + 1 < MachinePointerCapacity &&
            native_check_image(bundle, ExecBundleProgram(bundle), code->image, problem, sizeof(problem));
        for(int64_t at = 0; ok && at < code->global_count; at++)
            code->globals[at] = compiler_native_cache_NativeCacheGlobalAt(&code->cached, at);
    }
    if(!ok) {
        if(code != NULL) native_free_code(code);
        if(bytes != NULL || length > 0) {
            const char *counter = native_cache_reject_counter(cache_failure);
            unlink(path);
            ProfileCount("native_cache_disk_reject");
            if(counter != NULL) ProfileCount(counter);
        }
        return NULL;
    }
    ProfileCount("native_cache_disk_hit");
    return code;
}

typedef struct NativeDiskEntry {
    char name[96];
    time_t modified;
    off_t size;
} NativeDiskEntry;

static int
native_disk_older(const void *left, const void *right)
{
    const NativeDiskEntry *a = left, *b = right;
    return a->modified < b->modified ? -1 : a->modified > b->modified;
}

/* Keeps at most NATIVE_DISK_ENTRIES entries and NATIVE_DISK_BYTES, oldest
 * first out, and removes temporary files writers abandoned. */
static void
native_disk_trim(const char *directory)
{
    DIR *listing = opendir(directory);
    if(listing == NULL) return;
    NativeDiskEntry *entries = NULL;
    size_t count = 0, capacity = 0;
    off_t total = 0;
    time_t now = time(NULL);
    struct dirent *item;
    while((item = readdir(listing)) != NULL) {
        size_t length = strlen(item->d_name);
        int entry = length == 68 && !strcmp(item->d_name + 64, ".zxn");
        int temporary = length > 69 && length < sizeof(entries->name) && !strncmp(item->d_name + 64, ".tmp.", 5);
        if(!entry && !temporary) continue;
        struct stat info;
        if(fstatat(dirfd(listing), item->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(info.st_mode))
            continue;
        if(temporary) {
            if(now - info.st_mtime > NATIVE_DISK_ABANDONED) unlinkat(dirfd(listing), item->d_name, 0);
            continue;
        }
        if(count == capacity) {
            size_t grown = capacity ? capacity * 2 : 64;
            NativeDiskEntry *more = realloc(entries, grown * sizeof(*entries));
            if(more == NULL) break;
            entries = more;
            capacity = grown;
        }
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", item->d_name);
        entries[count].modified = info.st_mtime;
        entries[count].size = info.st_size;
        total += info.st_size;
        count++;
    }
    if(count > 0) qsort(entries, count, sizeof(*entries), native_disk_older);
    for(size_t at = 0; at < count && (count - at > NATIVE_DISK_ENTRIES || total > NATIVE_DISK_BYTES); at++) {
        if(unlinkat(dirfd(listing), entries[at].name, 0) == 0) total -= entries[at].size;
    }
    closedir(listing);
    free(entries);
}

/* Writes CODE's image under KEY: a private temporary file in the cache
 * directory, synced, then renamed over the entry, so a reader sees a whole
 * entry or none. */
static void
native_disk_store(const NativeBundleCode *code, const NativeCacheDigest *key)
{
    char path[4096], temporary[4200];
    if(!native_disk_path(key, ".zxn", path, sizeof(path))) return;
    if(snprintf(temporary, sizeof(temporary), "%.*s.tmp.XXXXXX", (int)(strlen(path) - 4), path) >=
       (int)sizeof(temporary)) return;
    NativeCacheBytes encoded = {0};
    if(!compiler_native_cache_NativeCacheEncodeBytes(*key, code->image,
            (Slice){code->globals, code->global_count}, code->has_result, &encoded)) {
        compiler_native_cache_NativeCacheBytesFree(&encoded);
        return;
    }
    Slice bytes = {encoded.bytes.data, encoded.bytes.count};
    int file = mkstemp(temporary);
    int ok = file >= 0;
    size_t done = 0;
    while(ok && done < (size_t)bytes.length) {
        ssize_t wrote = write(file, (const unsigned char *)bytes.data + done, (size_t)bytes.length - done);
        if(wrote < 0 && errno == EINTR) continue;
        if(wrote <= 0) ok = 0;
        else done += (size_t)wrote;
    }
    if(ok && fsync(file) != 0) ok = 0;
    if(file >= 0 && close(file) != 0) ok = 0;
    if(ok && rename(temporary, path) != 0) ok = 0;
    if(!ok && file >= 0) unlink(temporary);
    compiler_native_cache_NativeCacheBytesFree(&encoded);
    if(!ok) return;
    ProfileCount("native_cache_disk_store");
    char directory[4096];
    if(native_disk_directory(directory, sizeof(directory))) native_disk_trim(directory);
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

/* Unbounded code is a separate compilation of the same bundle, so it caches
 * under its own content key. */
static void
native_variant_digest(const unsigned char digest[32], int unbounded, unsigned char out[32])
{
    if(!unbounded) {
        memcpy(out, digest, 32);
        return;
    }
    static const char tag[] = "ziran native code without fuel checks";
    Sha256 hash;
    Sha256Begin(&hash);
    Sha256Add(&hash, tag, sizeof(tag) - 1);
    Sha256Add(&hash, digest, 32);
    Sha256End(&hash, out);
}

static void
native_slot_prepare(NativeBundleSlot *slot, const ExecBundle *bundle,
                    const unsigned char bundle_digest[32], int unbounded)
{
    unsigned char digest[32];
    native_variant_digest(bundle_digest, unbounded, digest);
    pthread_mutex_lock(&native_lock);
    if(!slot->ready) {
        uint64_t lookup_started = ProfileStart();
        uint64_t cpu = ZirNativeCpuKey();
        for(NativeBundleCode *code = native_cache; code != NULL; code = code->next)
            if(code->cpu == cpu && !memcmp(code->digest, digest, 32)) {
                code->references++;
                slot->code = code;
                break;
            }
        if(slot->code != NULL) {
            ProfileEnd("zx.native.lookup", lookup_started);
        } else {
            NativeCacheDigest key;
            int keyed = native_disk_key(digest, cpu, &key);
            NativeBundleCode *code = keyed ? native_disk_load(bundle, &key) : NULL;
            ProfileEnd("zx.native.lookup", lookup_started);
            if(code == NULL) {
                uint64_t compile_started = ProfileStart();
                code = native_compile(bundle, unbounded, slot->problem, sizeof(slot->problem));
                ProfileEnd("zx.native.compile", compile_started);
                if(code != NULL && keyed) {
                    uint64_t store_started = ProfileStart();
                    native_disk_store(code, &key);
                    ProfileEnd("zx.native.store", store_started);
                }
            }
            if(code != NULL) {
                memcpy(code->digest, digest, 32);
                code->cpu = cpu;
                code->unbounded = unbounded;
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
                     const unsigned char digest[32], int unbounded)
{
    if(slot == NULL || bundle == NULL || digest == NULL) return NULL;
    native_slot_prepare(slot, bundle, digest, unbounded);
    return slot->code;
}

const char *
NativeBundleSlotProblem(NativeBundleSlot *slot, const ExecBundle *bundle,
                        const unsigned char digest[32])
{
    if(slot == NULL || bundle == NULL || digest == NULL) return "assembly.native.input: no bundle";
    native_slot_prepare(slot, bundle, digest, 0);
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
static void native_trap_site_in(const NativeInstance *instance, const MachineNativeOwner *owner,
                                char *text, size_t capacity);

/* Code read from the disk cache has no machine program; the trap compiles
 * one to name its site, which costs a compile on this slow path only. */
static void
native_trap_site(const NativeInstance *instance, char *text, size_t capacity)
{
    if(instance->code->compiled) {
        native_trap_site_in(instance, &instance->code->owner, text, capacity);
        return;
    }
    char problem[512];
    NativeBundleCode *named = native_compile(instance->bundle, instance->unbounded, problem, sizeof(problem));
    if(named == NULL) { snprintf(text, capacity, "function=? block=?"); return; }
    native_trap_site_in(instance, &named->owner, text, capacity);
    native_free_code(named);
}

static void
native_trap_site_in(const NativeInstance *instance, const MachineNativeOwner *owner,
                    char *text, size_t capacity)
{
#if defined(__x86_64__)
    MachineX64Image image = owner->encoded.image;
    const MachineX64Function *extents = image.functions.data;
    const MachineX64Range *ranges = image.instructions.data;
#elif defined(__aarch64__)
    MachineA64Image image = owner->encoded_a64.image;
    const MachineA64Function *extents = image.functions.data;
    const MachineA64Range *ranges = image.instructions.data;
#else
    MachineV7Image image = owner->encoded_v7.image;
    const MachineV7Function *extents = image.functions.data;
    const MachineV7Range *ranges = image.instructions.data;
#endif
    MachineProgram program = owner->machine.program;
    uintptr_t base = (uintptr_t)instance->arena.address;
    snprintf(text, capacity, "function=? block=?");
    if(instance->trap_pc <= base || instance->trap_pc - base > (uintptr_t)image.code.length)
        return;
    int64_t offset = (int64_t)(instance->trap_pc - base) - 1;
    const MachineFunction *functions = program.functions.data;
    const MachineBlock *blocks = program.blocks.data;
    const MachineInstruction *instructions = program.instructions.data;
    const MachineOrigin *origins = program.origins.data;
    const MachineSource *sources = program.sources.data;
    const char *names = (const char *)program.text.data;
    for(int64_t id = 0; id < image.functions.length && id < program.functions.length; id++) {
        if(offset < extents[id].begin || offset >= extents[id].end) continue;
        MachineFunction function = functions[id];
        int32_t block = -1, origin = function.origin, check = -1;
#if defined(__x86_64__)
        /* An x86-64 check's trap lies after its function's blocks; the
         * call there to the trap handler names the check. */
        const MachineX64Relocation *relocations = image.relocations.data;
        for(int64_t at = 0; at < image.relocations.length; at++)
            if(relocations[at].kind == MachineX64RelocationKind_TrapHandler &&
               relocations[at].offset == offset - 3)
                check = relocations[at].instruction;
#endif
        for(int32_t at = function.block_begin; at < function.block_begin + function.block_count; at++)
            for(int32_t instruction = blocks[at].instruction_begin;
                instruction < blocks[at].instruction_begin + blocks[at].instruction_count; instruction++)
                if(instruction == check ||
                   (check < 0 && offset >= ranges[instruction].begin && offset < ranges[instruction].end)) {
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

/* Up to LOCAL_COUNT arguments go in LOCAL; more are allocated, and the
 * caller frees *VALUES when it is not LOCAL. */
static int
native_arguments(NativeInstance *instance, int32_t function,
                 NativeAbiFrame *frame, ExecHostNativeValue *local, size_t local_count,
                 ExecHostNativeValue **values, size_t *count, unsigned char **aggregate_result)
{
    const LirProgram *program = ExecBundleProgram(instance->bundle);
    if(function < 0 || function >= program->functions.length)
        return 0;
    const LirFunction *declared =
        &((const LirFunction *)program->functions.data)[function];
    const LirType *types = program->types.data;
    const int32_t *parameters = declared->parameters.data;
    size_t parameters_count = (size_t)declared->parameters.length;
    ExecHostNativeValue *arguments = local;
    if(parameters_count > local_count)
        arguments = calloc(parameters_count, sizeof(*arguments));
    else
        memset(local, 0, parameters_count * sizeof(*local));
    if(arguments == NULL) return 0;
    int general = 0, floating = 0;
    size_t stack = 0;
    const LirType *result = &types[declared->result];
    *aggregate_result = NULL;
    if(result->kind == LirTypeKind_Array ||
       result->kind == LirTypeKind_Record) {
        *aggregate_result = frame->result_address;
        if(*aggregate_result == NULL) {
            if(arguments != local) free(arguments);
            return 0;
        }
    }
    for(size_t at = 0; at < parameters_count; at++) {
        int32_t type = parameters[at];
        const LirType *shape = &types[type];
        int64_t width = native_type_size(program, type, 0);
        if(width < 0) goto failed;
        arguments[at].type = type;
        if(shape->kind == LirTypeKind_Float && floating < NATIVE_ABI_FLOAT) {
            memcpy(&arguments[at].bits, frame->floating[floating++],
                   (size_t)width);
        } else if(shape->kind != LirTypeKind_Array &&
                  shape->kind != LirTypeKind_Record && general < NATIVE_ABI_GENERAL) {
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
    if(arguments != local) free(arguments);
    return 0;
}

/* Decimal digits of VALUE at the end of TEXT[20]; returns where they start. */
static char *
native_digits(char *end, uint64_t value)
{
    do {
        *--end = (char)('0' + value % 10);
        value /= 10;
    } while(value != 0);
    return end;
}

/* One console service on its raw argument values: the descriptor, data and
 * length for bytes, or one scalar whose bool is its low bit and whose float
 * occupies its low bytes. */
static int
native_service(NativeInstance *instance, int service, const uint64_t *values, size_t count)
{
    if(service == ZX_SERVICE_PRINT_BYTES) {
        if(count != 3 || (values[1] == 0 && values[2] != 0))
            return 0;
        const unsigned char *bytes = (const unsigned char *)(uintptr_t)values[1];
        size_t left = (size_t)values[2];
        if(values[0] == 1) return ZxConsoleWrite(&instance->console, bytes, left);
        if(values[0] != 2) return 0;
        /* What came before on standard output reaches stdio first. */
        if(!ZxConsoleFlush(&instance->console)) return 0;
        while(left != 0) {
            size_t written = fwrite(bytes, 1, left, stderr);
            if(written == 0) return 0;
            bytes += written;
            left -= written;
        }
        return 1;
    }
    if(count != 1) return 0;
    char text[64];
    if(service == ZX_SERVICE_PRINT_SIGNED || service == ZX_SERVICE_PRINT_UNSIGNED) {
        uint64_t magnitude = values[0];
        int negative = service == ZX_SERVICE_PRINT_SIGNED && (int64_t)magnitude < 0;
        if(negative) magnitude = ~magnitude + 1;
        char *start = native_digits(text + sizeof(text), magnitude);
        if(negative) *--start = '-';
        return ZxConsoleWrite(&instance->console, start, (size_t)(text + sizeof(text) - start));
    }
    if(service == ZX_SERVICE_PRINT_BOOL)
        return values[0] & 1 ? ZxConsoleWrite(&instance->console, "true", 4) :
                               ZxConsoleWrite(&instance->console, "false", 5);
    if(service == ZX_SERVICE_PRINT_FLOAT32) {
        uint32_t bits = (uint32_t)values[0];
        float value;
        memcpy(&value, &bits, sizeof(value));
        FormatPrintFloat(value, 1, text, sizeof(text));
    } else if(service == ZX_SERVICE_PRINT_FLOAT64) {
        double value;
        memcpy(&value, &values[0], sizeof(value));
        FormatPrintFloat(value, 0, text, sizeof(text));
    } else return 0;
    return ZxConsoleWrite(&instance->console, text, strlen(text));
}

static int
native_platform_call(NativeInstance *instance, int service,
                     ExecHostNativeValue *arguments, size_t count,
                     ExecHostNativeValue *result)
{
    result->bits = 0;
    uint64_t values[3];
    if(count > 3) return 0;
    for(size_t at = 0; at < count; at++) values[at] = arguments[at].bits;
    return native_service(instance, service, values, count);
}

/* A console service whose declared parameters are 64-bit words, a pointer,
 * a bool or a float, with no result, can take its arguments straight from
 * the saved registers: x86-64 and aarch64 pass each in one register. */
#define NATIVE_SERVICE_DIRECT 0x80

static int
native_service_direct(const LirProgram *lir, size_t function, int service)
{
#if defined(__x86_64__) || defined(__aarch64__)
    const LirFunction *declared = &((const LirFunction *)lir->functions.data)[function];
    const LirType *types = lir->types.data;
    const int32_t *parameters = declared->parameters.data;
    if(types[declared->result].kind != LirTypeKind_Void) return 0;
    if(service == ZX_SERVICE_PRINT_BYTES) {
        if(declared->parameters.length != 3) return 0;
        for(int at = 0; at < 3; at++) {
            const LirType *type = &types[parameters[at]];
            int word = type->kind == LirTypeKind_Integer && type->bits == 64;
            if(at == 1 ? type->kind != LirTypeKind_Pointer : !word) return 0;
        }
        return 1;
    }
    if(declared->parameters.length != 1) return 0;
    const LirType *type = &types[parameters[0]];
    switch(service) {
    case ZX_SERVICE_PRINT_SIGNED:
    case ZX_SERVICE_PRINT_UNSIGNED: return type->kind == LirTypeKind_Integer && type->bits == 64;
    case ZX_SERVICE_PRINT_BOOL: return type->kind == LirTypeKind_Bool;
    case ZX_SERVICE_PRINT_FLOAT32: return type->kind == LirTypeKind_Float && type->bits == 32;
    case ZX_SERVICE_PRINT_FLOAT64: return type->kind == LirTypeKind_Float && type->bits == 64;
    default: return 0;
    }
#else
    (void)lir; (void)function; (void)service;
    return 0;
#endif
}

static uint64_t
NativeHostInvoke(NativeInstance *instance, int32_t function,
                 NativeAbiFrame *frame)
{
    if(instance == NULL || frame == NULL || instance->active == NULL)
        abort();
    if(function >= 0 && (size_t)function < instance->function_count &&
       (instance->services[function] & NATIVE_SERVICE_DIRECT) != 0) {
        uint64_t values[3] = {frame->general[0], 0, 0};
        int service = instance->services[function] & ~NATIVE_SERVICE_DIRECT;
        size_t count = 1;
        if(service == ZX_SERVICE_PRINT_BYTES) {
            values[1] = frame->general[1];
            values[2] = frame->general[2];
            count = 3;
        } else if(service == ZX_SERVICE_PRINT_FLOAT32 || service == ZX_SERVICE_PRINT_FLOAT64) {
            memcpy(&values[0], frame->floating[0], sizeof(values[0]));
        }
        if(!native_service(instance, service, values, count))
            NativeTrap(instance, NATIVE_TRAP_HOST, 0);
        return 0;
    }
    ExecHostNativeValue local[8], *arguments = NULL, returned = {0};
    size_t count = 0;
    unsigned char *aggregate_result = NULL;
    if(!native_arguments(instance, function, frame, local, 8, &arguments, &count,
                         &aggregate_result))
        NativeTrap(instance, NATIVE_TRAP_HOST, 0);
    int ok;
    /* A host capability may print too: the program's output comes first. */
    if(instance->capabilities[function] >= 0)
        ok = (instance->console.used == 0 || ZxConsoleFlush(&instance->console)) &&
             ExecHostsCallNative(instance->hosts, (size_t)instance->capabilities[function],
                                 arguments, count, &returned);
    else
        ok = native_platform_call(instance, instance->services[function] & ~NATIVE_SERVICE_DIRECT,
                                  arguments, count, &returned);
    if(arguments != local) free(arguments);
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

/* A per-instance tag for its blocks' headers; the top bit keeps every tag
 * nonzero, unlike a freed block's. */
static uint64_t
native_cookie(const NativeInstance *instance)
{
    static uint64_t counter;
    uint64_t value = (uint64_t)(uintptr_t)instance ^ ((uint64_t)time(NULL) << 32) ^
                     __atomic_add_fetch(&counter, 0x9e3779b97f4a7c15ull, __ATOMIC_RELAXED);
    value ^= value >> 31;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 29;
    return value | ((uint64_t)1 << 63);
}

static NativeHeader *
native_header_of(uint64_t data)
{
    return (NativeHeader *)(uintptr_t)(data - sizeof(NativeHeader));
}

static int
native_small(size_t bytes)
{
    return bytes > 0 && bytes <= MachineX64HeapSmall;
}

/* A small block of BYTES: a recycled one, cleared, or fresh chunk bytes,
 * which are zero. The fast path takes the same way first. */
static unsigned char *
native_small_take(NativeInstance *instance, size_t bytes)
{
    NativeHeap *heap = &instance->heap;
    size_t unit = (bytes - 1) / 16;
    unsigned char *data = heap->lists[unit];
    if(data != NULL) {
        memcpy(&heap->lists[unit], data, sizeof(heap->lists[unit]));
        memset(data, 0, (unit + 1) * 16);
        return data;
    }
    size_t need = (unit + 2) * 16;
    if(heap->top == NULL || (size_t)(heap->end - heap->top) < need) {
        NativeChunk *chunk = calloc(1, NATIVE_HEAP_CHUNK);
        if(chunk == NULL) return NULL;
        chunk->next = instance->chunks;
        instance->chunks = chunk;
        heap->top = (unsigned char *)(chunk + 1);
        heap->end = (unsigned char *)chunk + NATIVE_HEAP_CHUNK;
    }
    data = heap->top + sizeof(NativeHeader);
    heap->top += need;
    return data;
}

static unsigned char *
native_large_take(NativeInstance *instance, size_t bytes)
{
    NativeBlock *block = calloc(1, sizeof(NativeBlock) + bytes);
    if(block == NULL) return NULL;
    block->next = instance->blocks;
    if(block->next) block->next->previous = block;
    instance->blocks = block;
    return (unsigned char *)(block + 1);
}

/* Frees the block at DATA holding BYTES; its tag no longer matches. */
static void
native_release(NativeInstance *instance, uint64_t data, size_t bytes)
{
    native_header_of(data)->tag = 0;
    if(native_small(bytes)) {
        NativeHeap *heap = &instance->heap;
        size_t unit = (bytes - 1) / 16;
        memcpy((void *)(uintptr_t)data, &heap->lists[unit], sizeof(heap->lists[unit]));
        heap->lists[unit] = (unsigned char *)(uintptr_t)data;
        return;
    }
    NativeBlock *block = (NativeBlock *)(uintptr_t)(data - sizeof(NativeBlock));
    if(block->previous) block->previous->next = block->next;
    else instance->blocks = block->next;
    if(block->next) block->next->previous = block->previous;
    free(block);
}

/* Resize(DATA, OLD_COUNT, NEW_COUNT, ELEMENT): a block of NEW_COUNT elements
 * holding the first OLD_COUNT, or none for zero. DATA is null with no
 * elements, or a block this instance returned with exactly OLD_COUNT of them;
 * anything else traps. New elements are zero. On x86-64 the code's fast path
 * handles small allocations and frees and comes here for the rest. */
static uint64_t
NativeResize(uint64_t data, uint64_t old_count, uint64_t new_count, uint64_t element,
             NativeInstance *instance)
{
    if(instance == NULL || instance->active == NULL) abort();
    NativeHeap *heap = &instance->heap;
    NativeHeader *old = NULL;
    if(data == 0) {
        if(old_count != 0) NativeTrap(instance, NATIVE_TRAP_BOUNDS, 0);
    } else {
        old = native_header_of(data);
        if(data < sizeof(NativeBlock) || old->count != old_count || old->tag != (element ^ heap->cookie))
            NativeTrap(instance, NATIVE_TRAP_BOUNDS, 0);
    }
    size_t old_bytes = (size_t)(old_count * element);
    if(new_count == 0) {
        if(old != NULL) {
            native_release(instance, data, old_bytes);
            heap->live -= old_bytes;
        }
        return 0;
    }
    /* A request that does not fit gives null and keeps the old block, as on
     * the portable engine. */
    if(element != 0 && new_count > (NATIVE_HEAP_LIMIT - sizeof(NativeBlock)) / element)
        return 0;
    size_t new_bytes = (size_t)(new_count * element);
    if(heap->live - old_bytes > NATIVE_HEAP_LIMIT - new_bytes)
        return 0;
    if(old != NULL && native_small(old_bytes) && native_small(new_bytes) &&
       (old_bytes - 1) / 16 == (new_bytes - 1) / 16) {
        /* The same class keeps its block; a shrink clears what it drops. */
        if(new_bytes < old_bytes)
            memset((unsigned char *)(uintptr_t)data + new_bytes, 0, old_bytes - new_bytes);
        old->count = new_count;
        heap->live = heap->live - old_bytes + new_bytes;
        return data;
    }
    if(old != NULL && !native_small(old_bytes) && !native_small(new_bytes)) {
        /* Keep large storage when the allocator can grow it in place. A
         * failed realloc leaves both the old block and its list links live. */
        NativeBlock *previous = (NativeBlock *)(uintptr_t)(data - sizeof(NativeBlock));
        NativeBlock *resized = realloc(previous, sizeof(NativeBlock) + new_bytes);
        if(resized == NULL) return 0;
        if(resized->previous) resized->previous->next = resized;
        else instance->blocks = resized;
        if(resized->next) resized->next->previous = resized;
        unsigned char *bytes = (unsigned char *)(resized + 1);
        if(new_bytes > old_bytes) memset(bytes + old_bytes, 0, new_bytes - old_bytes);
        resized->header.count = new_count;
        heap->live = heap->live - old_bytes + new_bytes;
        return (uint64_t)(uintptr_t)bytes;
    }
    unsigned char *block = native_small(new_bytes) ? native_small_take(instance, new_bytes)
                                                   : native_large_take(instance, new_bytes);
    if(block == NULL) return 0;
    NativeHeader *header = native_header_of((uint64_t)(uintptr_t)block);
    header->count = new_count;
    header->tag = element ^ heap->cookie;
    if(old != NULL) {
        memcpy(block, (void *)(uintptr_t)data, old_bytes < new_bytes ? old_bytes : new_bytes);
        native_release(instance, data, old_bytes);
    }
    heap->live = heap->live - old_bytes + new_bytes;
    return (uint64_t)(uintptr_t)block;
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

/* Global AT's bytes: a writable global in the instance's data, a read-only
 * one in the constants beside the code. NULL when the image has no place
 * for it. */
static unsigned char *
native_global_bytes(const NativeInstance *instance, MachineImage image, int64_t at)
{
    const int64_t *offsets = instance->code->globals[at].writable ?
        image.global_offsets.data : image.constant_offsets.data;
    int64_t length = instance->code->globals[at].writable ?
        image.global_offsets.length : image.constant_offsets.length;
    if(at >= length || offsets[at] < 0) return NULL;
    unsigned char *base = instance->code->globals[at].writable ? instance->data : instance->constants;
    return base + offsets[at];
}

/* The persistent pointer header; with --checks also slot 0 (live and empty at
 * address 0), a slot per global, and the host bridge's token resolver.
 * Without it a pointer is the address and hosts take it as it is. */
static int
native_pointers_open(NativeInstance *instance, MachineImage image, unsigned char *constants)
{
    instance->checked = ((const LirProgram *)ExecBundleProgram(instance->bundle))->checks;
    instance->constants = constants;
    size_t table = instance->checked ? (size_t)(MachinePointerCapacity + 1) * MachinePointerEntryBytes : 0;
    if(table != 0 && !ZirNativeReserve(table, &instance->pointers)) {
        int error = errno;
        Diagnostic((ZirSourceSpan){0}, "zx.native.pointers",
            "cannot reserve pointer table for %s (bytes=%llu errno=%d)",
            ExecBundleEntryFunction(instance->bundle),
            (unsigned long long)table, error);
        return 0;
    }
    instance->pointer_header = instance->data + image.pointer_offset;
    unsigned char *base = instance->pointers.address;
    native_set_header(instance, MachinePointerHeaderArenaTop, 0);
    native_set_header(instance, MachinePointerHeaderArenaLimit, 0);
    int64_t global_count = instance->code->global_count;
    if(global_count + 1 >= MachinePointerCapacity) return 0;
    for(int64_t at = 0; at < global_count; at++) {
        if(native_global_bytes(instance, image, at) == NULL) {
            Diagnostic((ZirSourceSpan){0}, "zx.native.pointers",
                "global %lld has no native section in %s (writable=%d globals=%lld constants=%lld)",
                (long long)at, ExecBundleEntryFunction(instance->bundle),
                instance->code->globals[at].writable,
                (long long)image.global_offsets.length,
                (long long)image.constant_offsets.length);
            return 0;
        }
        instance->global_bytes += (size_t)instance->code->globals[at].size;
    }
    if(!instance->checked) return 1;
    native_set_header(instance, MachinePointerHeaderFree, 0);
    native_set_header(instance, MachinePointerHeaderUnused, (uint64_t)global_count + 1);
    native_set_header(instance, MachinePointerHeaderCapacity, MachinePointerCapacity);
    native_set_header(instance, MachinePointerHeaderTable, (uint64_t)(uintptr_t)base);
    NativeSlot *slots = native_slots(instance);
    slots[0].flags = MachinePointerLive;
    for(int64_t at = 0; at < global_count; at++) {
        NativeSlot *entry = &slots[at + 1];
        entry->base = (uint64_t)(uintptr_t)native_global_bytes(instance, image, at);
        entry->flags = instance->code->globals[at].writable ?
            MachinePointerLive | MachinePointerWritable : MachinePointerLive;
        entry->size = (uint64_t)instance->code->globals[at].size;
    }
    ExecHostsNativePointers(instance->hosts, native_resolve, native_create, instance);
    return 1;
}

/* ---- Running ------------------------------------------------------------- */

/* pthread_getattr_np may scan /proc for the main thread, so each thread keeps
 * its one stack range, never an instance limit: the depth cap still follows
 * each entry's frame, and a switched or alternate stack misses the range.
 * Linux derives the main stack's bottom from RLIMIT_STACK. Checking that
 * limit on every entry costs a system call, most of a warm entry, so a
 * forked child forgets the range it inherited and looks it up again under
 * its own limit; a host that lowers the limit in place does so before the
 * thread's first entry. Failed queries and fallback limits are not kept. */
static _Thread_local struct {
    uintptr_t bottom;
    size_t size;
} native_stack_range;

static void
native_stack_forget(void)
{
    native_stack_range.size = 0;
}

static void
native_stack_watch_forks(void)
{
    (void)pthread_atfork(NULL, NULL, native_stack_forget);
}

/* The lowest stack address generated code may reach on this thread, leaving
 * NATIVE_STACK_RESERVE for host calls and the C runtime. */
static uintptr_t
native_stack_limit(void)
{
    uintptr_t here = (uintptr_t)__builtin_frame_address(0);
    uintptr_t floor = here > NATIVE_STACK_MAXIMUM ? here - NATIVE_STACK_MAXIMUM : 0;
    void *low = NULL;
    size_t size = 0;
    if(native_stack_range.size != 0 && here > native_stack_range.bottom &&
       here - native_stack_range.bottom <= native_stack_range.size) {
        low = (void *)native_stack_range.bottom;
        size = native_stack_range.size;
    } else {
        static pthread_once_t watching = PTHREAD_ONCE_INIT;
        (void)pthread_once(&watching, native_stack_watch_forks);
        pthread_attr_t attributes;
        native_stack_range.size = 0;
        if(pthread_getattr_np(pthread_self(), &attributes) == 0) {
            if(pthread_attr_getstack(&attributes, &low, &size) != 0) low = NULL;
            pthread_attr_destroy(&attributes);
        }
        if(low != NULL && size > NATIVE_STACK_RESERVE &&
           here > (uintptr_t)low && here - (uintptr_t)low <= size) {
            native_stack_range.bottom = (uintptr_t)low;
            native_stack_range.size = size;
        }
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
    int outermost = instance->active == NULL;
    if(outermost) {
        /* Frame addresses are valid only while their call is active. Keeping
         * this reservation in idle instances exhausts a 32-bit host's address
         * space after a few bundles, even though almost none is resident. */
        if(!ZirNativeReserve(MachinePointerArenaBytes, &instance->frames)) {
            int error = errno;
            Diagnostic((ZirSourceSpan){0}, "zx.native.pointers",
                "cannot reserve active frame arena for %s (bytes=%llu errno=%d)",
                ExecBundleEntryFunction(instance->bundle),
                (unsigned long long)MachinePointerArenaBytes, error);
            instance->terminal = 1;
            return 0;
        }
        unsigned char *base = instance->frames.address;
        native_set_header(instance, MachinePointerHeaderArenaTop, (uint64_t)(uintptr_t)base);
        native_set_header(instance, MachinePointerHeaderArenaLimit,
            (uint64_t)(uintptr_t)(base + MachinePointerArenaBytes));
        memcpy(instance->data + instance->fuel_offset, &budget, sizeof(budget));
    }
    int called = native_call_entry(instance, address, result);
    if(outermost) {
        native_set_header(instance, MachinePointerHeaderArenaTop, 0);
        native_set_header(instance, MachinePointerHeaderArenaLimit, 0);
        ZirNativeRelease(&instance->frames);
    }
    /* A run hands its output to stdio's buffering, before any report. */
    if(instance->console.used != 0) ZxConsoleFlush(&instance->console);
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
    if(code == NULL || bundle == NULL || (code->unbounded && max_steps > 0)) return NULL;
    uint64_t load_started = ProfileStart();
    MachineImage image = code->image;
    if(image.code.bytes.data == NULL || image.code.bytes.length <= 0 ||
       image.entry < 0 || image.entry >= image.function_offsets.length ||
       image.stack_limit_offset < 0)
        return NULL;
    NativeInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL) return NULL;
    instance->code = code;
    instance->heap.cookie = native_cookie(instance);
    instance->heap.limit = NATIVE_HEAP_LIMIT;
    instance->bundle = bundle;
    ZxConsoleOpen(&instance->console);
    const char *load_stage = "host bindings";
    int64_t relocation_at = -1;
    instance->hosts = ExecHostsOpen(bundle, (ExecFindBinding)find, context);
    if(instance->hosts == NULL) goto failed;
    load_stage = "host metadata";
    const LirProgram *lir = ExecBundleProgram(bundle);
    size_t function_count = lir != NULL ? (size_t)lir->functions.length : 0;
    instance->capabilities = calloc(function_count ? function_count : 1, sizeof(*instance->capabilities));
    instance->services = calloc(function_count ? function_count : 1, sizeof(*instance->services));
    if(instance->capabilities == NULL || instance->services == NULL) goto failed;
    for(size_t at = 0; at < function_count; at++) {
        size_t host = 0;
        instance->capabilities[at] = capability_index(bundle, (int32_t)at, &host) ? (int32_t)host : -1;
        int service = ZxPlatformService(lir, (int64_t)at);
        instance->services[at] = (unsigned char)service;
        if(service != ZX_SERVICE_NONE && instance->capabilities[at] < 0 &&
           native_service_direct(lir, at, service))
            instance->services[at] |= NATIVE_SERVICE_DIRECT;
    }
    instance->function_count = function_count;
    load_stage = "image layout";
    size_t page = ZirNativePageSize();
    size_t code_size = (size_t)image.code.bytes.length;
    size_t stubs = (size_t)image.relocations.length;
    if(page == 0 || stubs > (SIZE_MAX - code_size) / NATIVE_HOST_STUB_SIZE)
        goto failed;
    code_size += stubs * NATIVE_HOST_STUB_SIZE;
    if(code_size > SIZE_MAX - NATIVE_TRAP_STUB_SIZE - NATIVE_RESIZE_STUB_SIZE - MachineX64HeapStubBytes)
        goto failed;
    size_t trap_at = code_size;
    code_size += NATIVE_TRAP_STUB_SIZE;
    size_t resize_at = code_size;
    code_size += NATIVE_RESIZE_STUB_SIZE;
    /* x86-64 code resizes through its heap fast path, whose other requests
     * go to the resize veneer. */
    size_t heap_at = code_size;
#if defined(__x86_64__)
    code_size += MachineX64HeapStubBytes;
#endif
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
    load_stage = "image memory";
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
    instance->unbounded = code->unbounded;
    instance->limit = max_steps > 0 ? max_steps : 0;
    instance->cumulative = max_steps > 0;
    if(instance->data_size < 8 ||
       (uint64_t)image.stack_limit_offset > instance->data_size - 8) goto failed;
    /* An empty section has no bytes to copy and may have no buffer. */
    if(image.code.bytes.length > 0)
        memcpy(base, image.code.bytes.data, (size_t)image.code.bytes.length);
    if(image.constants.bytes.length > 0)
        memcpy(base + constants_at, image.constants.bytes.data,
               (size_t)image.constants.bytes.length);
    if(image.data.bytes.length > 0)
        memcpy(instance->data, image.data.bytes.data,
               (size_t)image.data.bytes.length);
    load_stage = "pointer metadata";
    if(image.pointer_offset <= 0 ||
       (uint64_t)image.pointer_offset > instance->data_size - MachinePointerHeaderBytes ||
       !native_pointers_open(instance, image, base + constants_at))
        goto failed;
    load_stage = "runtime veneers";
    NativeVeneerCalls calls = {
        .instance = instance,
        .host = (uintptr_t)(void *)&NativeHostInvoke,
        .trap = (uintptr_t)(void *)&NativeTrap,
        .resize = (uintptr_t)(void *)&NativeResize,
    };
    NativeStub trap_stub = {base + trap_at, 0, NATIVE_TRAP_STUB_SIZE, 0};
    NativeStub resize_stub = {base + resize_at, 0, NATIVE_RESIZE_STUB_SIZE, 0};
    if(!ZirNativeTrapVeneer(&trap_stub, &calls) ||
       !ZirNativeResizeVeneer(&resize_stub, &calls))
        goto failed;
#if defined(__x86_64__)
    Slice heap_stub = {base + heap_at, MachineX64HeapStubBytes};
    if(compiler_x86_runtime_MachineX64HeapEmit(heap_stub, (uint64_t)(uintptr_t)&instance->heap,
                                               (uint64_t)(uintptr_t)(base + resize_at)) < 0)
        goto failed;
#endif
    load_stage = "relocations";
    MachineRelocation *relocations = image.relocations.data;
    size_t stub_at = (size_t)image.code.bytes.length;
#if defined(__arm__)
    size_t helper_at[MachineHelperCount] = {0};
#endif
    for(int64_t at = 0; at < image.relocations.length; at++) {
        relocation_at = at;
        MachineRelocation relocation = relocations[at];
        if(relocation.section != MachineSectionKind_Code || relocation.offset < 0 ||
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
#if defined(__x86_64__)
            target = (uintptr_t)(base + heap_at);
#else
            target = (uintptr_t)(base + resize_at);
            (void)heap_at;
#endif
        } else if(relocation.kind == MachineRelocationKind_FunctionAddress) {
            int32_t symbol = relocation.symbol;
            int64_t *offsets = image.function_offsets.data;
            if(symbol < 0 || symbol >= image.function_offsets.length ||
               offsets[symbol] < 0 || offsets[symbol] >= image.code.bytes.length)
                goto failed;
            target = (uintptr_t)(base + offsets[symbol]);
#if defined(__arm__)
        } else if(relocation.kind == MachineRelocationKind_RuntimeHelper) {
            /* One veneer per helper, in the first stub slot it needs. */
            int32_t helper = relocation.symbol;
            if(helper < 0 || helper >= MachineHelperCount) goto failed;
            if(helper_at[helper] == 0) {
                NativeStub helper_stub = {base + stub_at, 0, NATIVE_HOST_STUB_SIZE, 0};
                if(!ZirNativeHelperVeneer(&helper_stub, &calls, helper)) goto failed;
                helper_at[helper] = stub_at;
                stub_at += NATIVE_HOST_STUB_SIZE;
            }
            target = (uintptr_t)(base + helper_at[helper]);
#endif
        } else if(relocation.kind == MachineRelocationKind_HostCapability ||
                  relocation.kind == MachineRelocationKind_PlatformService) {
            NativeStub host_stub = {base + stub_at, 0, NATIVE_HOST_STUB_SIZE, 0};
            if(!ZirNativeHostVeneer(&host_stub, &calls, relocation.symbol))
                goto failed;
            target = (uintptr_t)(base + stub_at);
            stub_at += NATIVE_HOST_STUB_SIZE;
        } else {
            goto failed;
        }
        if(!ZirNativePatch(base + relocation.offset, target, relocation.width,
                           relocation.addend)) {
            Diagnostic((ZirSourceSpan){0}, "zx.native.relocation",
                "cannot patch relocation %lld: kind=%d offset=%lld width=%d "
                "addend=%lld distance=%lld code_bytes=%lld",
                (long long)at, (int)relocation.kind, (long long)relocation.offset,
                (int)relocation.width, (long long)relocation.addend,
                (long long)((int64_t)target - (int64_t)(uintptr_t)(base + relocation.offset)),
                (long long)image.code.bytes.length);
            goto failed;
        }
    }
    load_stage = "entry point";
    int64_t *functions = image.function_offsets.data;
    if(functions[image.entry] < 0 ||
       functions[image.entry] >= image.code.bytes.length)
        goto failed;
    instance->entry = base + functions[image.entry];
    load_stage = "executable mapping";
    ZirNativeMapping code_mapping = {.address = base, .size = code_pages};
    if(!ZirNativeFlushInstructions(&code_mapping) ||
       !ZirNativeProtectRX(&code_mapping))
        goto failed;
    ProfileEnd("zx.native.load", load_started);
    uint64_t init_started = ProfileStart();
    load_stage = "initializers";
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
    ProfileEnd("zx.native.init", init_started);
    return instance;
failed:
    Diagnostic((ZirSourceSpan){0}, "zx.native",
        "cannot load the native image of %s (stage=%s relocation=%lld); "
        "no alternate engine was selected",
        ExecBundleEntryFunction(bundle), load_stage, (long long)relocation_at);
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
    /* A run that ends normally leaves its output to the stream's own
     * buffering, as the engine and a C program do: a flush per run would
     * cost a write per call. A trap's report flushes first, and closing the
     * instance flushes what it wrote. An entry without a result leaves
     * whatever its last call put in the result register; it reports zero,
     * as the engine does. */
    *result = instance->has_result ? value : 0;
    *has_result = instance->has_result;
    return 1;
}

void
NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps)
{
    if(instance == NULL || instance->terminal || instance->unbounded) return;
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
    if(!instance->checked) return native_global_bytes(instance, instance->code->image, global);
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
    return instance != NULL ? instance->global_bytes + (size_t)instance->heap.live : 0;
}

void
NativeInstanceClose(NativeInstance *instance)
{
    if(instance == NULL) return;
    /* What the instance wrote is out by the time it closes. */
    ZxConsoleFlush(&instance->console);
    fflush(stdout);
    for(NativeBlock *block = instance->blocks; block != NULL;) {
        NativeBlock *next = block->next;
        free(block);
        block = next;
    }
    for(NativeChunk *chunk = instance->chunks; chunk != NULL;) {
        NativeChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    ExecHostsClose(instance->hosts);
    free(instance->capabilities);
    free(instance->services);
    ZirNativeRelease(&instance->pointers);
    ZirNativeRelease(&instance->frames);
    ZirNativeRelease(&instance->arena);
    free(instance);
}

#endif
