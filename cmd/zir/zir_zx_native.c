/* Native execution of verified ZX bundles.
 *
 * The bundle's verified program is lowered to C by the same backend native
 * builds use, compiled in memory by TinyCC and called directly. Host
 * capabilities reach the instance's bindings through trampolines
 * (zir_zx_host.c) that pass the same VmHostValue shapes the portable VM does.
 *
 * Every failure is a hard error with a diagnostic. A program that does not
 * compile does not run; a runtime failure (bounds, division, a host breaking
 * the value contract) stops the run and leaves the instance unusable, as in
 * the VM. Compiled code sees only the C headers in zx_native_include and
 * links only against the symbols registered here. It runs with the process's
 * memory like any native build of the program: unlike the portable VM, it
 * does not check pointer lifetimes at run time. */
#include "zir_zx_native.h"
#include "zir_zx_host.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_native.h"
#include "zir_sha256.h"
#include "../zir-c/zir_c_lower.h"

#include "libtcc.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

/* Bump storage. Scratch blocks hold one host call's argument values and are
 * rewound before the next call; kept blocks live until the instance closes. */
typedef struct Block {
    struct Block *next;
    size_t used;
    size_t size;
    max_align_t data[];
} Block;

/* Host strings copied into the instance, once per distinct text. */
typedef struct HostString {
    struct HostString *next;
    uint64_t hash;
    size_t length;
    char data[];
} HostString;

struct ZxNative {
    TCCState *tcc;
    VmHostCall host;
    void *context;
    const char *const *capability_modules;
    const char *const *capability_functions;
    size_t capability_count;
    const ZirFunction *entry;
    int has_result;
    int include_headers;
    /* Module sources whose capability calls are renamed: stem, symbol,
     * replacement, as consecutive strings. */
    char **renames;
    int rename_count;
    void (*start)(void);
    int64_t (*run)(void);
    long long *budget;
    char **floor;
    int match_fields_by_name;
    int max_steps;
    int failed;
    jmp_buf *escape;
    Block *scratch;
    Block *scratch_at;
    Block *kept;
    HostString **strings;
    size_t string_buckets;
    size_t string_count;
    char error[2048];
};

static _Thread_local ZxNative *current;
static pthread_mutex_t compile_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *bundle_stdout;
static FILE *bundle_stderr;

static void
append_error(ZxNative *native, const char *format, va_list args)
{
    size_t used = strlen(native->error);
    if(used + 1 < sizeof(native->error))
        vsnprintf(native->error + used, sizeof(native->error) - used, format, args);
}

/* Stop the running bundle. Only called while ZxNativeRun or initialization
 * has an escape point. */
static void
native_fail(const char *format, ...)
{
    ZxNative *native = current;
    if(native == NULL || native->escape == NULL)
        abort();
    if(format != NULL) {
        va_list args;
        va_start(args, format);
        native->error[0] = '\0';
        append_error(native, format, args);
        va_end(args);
    }
    native->failed = 1;
    longjmp(*native->escape, 1);
}

/* Run guards C lowering writes when the engine asks: a step budget spent at
 * each function entry and loop iteration, a stack floor checked at entry,
 * and unreachable statements that report where they are. Every compiled
 * file includes this first; the trampoline file defines the two variables,
 * which each run sets for its instance. */
static const char guard_header[] =
    "#ifndef ZX_GUARD_H\n"
    "#define ZX_GUARD_H\n"
    "extern long long zx_native_budget;\n"
    "extern char *zx_native_floor;\n"
    "void ZxNativeOverrun(void);\n"
    "void ZxNativeStackOverrun(void);\n"
    "void ZxNativeUnreachable(const char *place);\n"
    "#define ZIR_STEP() do { if(--zx_native_budget < 0) ZxNativeOverrun(); } while(0)\n"
    "#define ZIR_ENTER() do { char zx_here; if(&zx_here < zx_native_floor) ZxNativeStackOverrun(); ZIR_STEP(); } while(0)\n"
    "#define ZIR_UNREACHABLE(place) ZxNativeUnreachable(place)\n"
    "#endif\n";

/* Room left below the floor for host callbacks and the C library. */
enum { STACK_MARGIN = 256 * 1024 };

/* ---- Services the generated code calls ---- */

static void *
block_take(Block **chain, Block **at, size_t size)
{
    size = (size + sizeof(max_align_t) - 1) / sizeof(max_align_t) * sizeof(max_align_t);
    Block *block = at != NULL ? *at : *chain;
    while(block != NULL && block->size - block->used < size)
        block = block->next;
    if(block == NULL) {
        size_t capacity = size > 65536 ? size : 65536;
        block = malloc(sizeof(Block) + capacity);
        if(block == NULL)
            native_fail("out of memory");
        block->used = 0;
        block->size = capacity;
        block->next = *chain;
        *chain = block;
    }
    if(at != NULL)
        *at = block;
    void *memory = (unsigned char *)block->data + block->used;
    block->used += size;
    memset(memory, 0, size);
    return memory;
}

static void
ZxNativeScratchReset(void)
{
    for(Block *block = current->scratch; block != NULL; block = block->next)
        block->used = 0;
    current->scratch_at = current->scratch;
}

static void *
ZxNativeScratch(size_t size)
{
    return block_take(&current->scratch, &current->scratch_at, size);
}

static void *
ZxNativeKeep(size_t size)
{
    return block_take(&current->kept, NULL, size);
}

static void
ZxNativeHost(int capability, VmHostValue *args, int count, VmHostValue *result)
{
    ZxNative *native = current;
    if(capability < 0 || (size_t)capability >= native->capability_count)
        native_fail("unknown host capability %d", capability);
    const char *module = native->capability_modules[capability];
    const char *function = native->capability_functions[capability];
    if(!native->host(native->context, module, function, args, count, result))
        native_fail("host capability %s:%s failed", module, function);
}

static void
ZxNativeExpect(const VmHostValue *value, int kind, const char *type)
{
    if(value->type == NULL || strcmp(value->type, type) != 0)
        native_fail("host returned %s where %s was declared",
                    value->type != NULL ? value->type : "an untyped value", type);
    if((int)value->kind != kind)
        native_fail("host returned the wrong kind of value for %s", type);
    if(kind == VM_HOST_RECORD) {
        if(value->field_count > 0 && value->fields == NULL)
            native_fail("host returned record %s without fields", type);
    } else if(value->field_count != 0 || value->fields != NULL) {
        native_fail("host returned fields for %s", type);
    }
    if(kind == VM_HOST_STRING && value->data == NULL && value->length != 0)
        native_fail("host returned %s without its bytes", type);
}

static void
ZxNativeExpectLength(const VmHostValue *value, size_t length)
{
    if(value->length != length)
        native_fail("host returned %zu elements of %s where %zu were expected",
                    value->length, value->type, length);
    if(length > 0 && value->elements == NULL)
        native_fail("host returned %s without its elements", value->type);
}

/* A returned slice becomes storage the bundle owns; refuse the sizes the VM
 * refuses rather than allocating whatever length a host reports. */
static void
ZxNativeExpectSlice(const VmHostValue *value, size_t element_size)
{
    if(value->length > INT32_MAX ||
       value->length > (size_t)1024 * 1024 * 1024 / (element_size ? element_size : 1))
        native_fail("host returned %zu elements of %s, more than a bundle can hold",
                    value->length, value->type);
    if(value->length > 0 && value->elements == NULL)
        native_fail("host returned %s without its elements", value->type);
}

static void
ZxNativeExpectFields(const VmHostValue *value, size_t count)
{
    if(value->field_count != count)
        native_fail("host returned %zu fields of %s, which declares %zu",
                    value->field_count, value->type, count);
}

static void
ZxNativeDistinct(const void *a, size_t a_bytes, const void *b, size_t b_bytes)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if(a_bytes > 0 && b_bytes > 0 && x < y + b_bytes && y < x + a_bytes)
        native_fail("overlapping mutable host slice arguments require separate storage");
}

static int
ZxNativeMatchByName(void)
{
    return current->match_fields_by_name;
}

static const VmHostValue *
ZxNativeField(const VmHostValue *value, const char *name)
{
    for(size_t i = 0; i < value->field_count; i++)
        if(value->fields[i].name != NULL && strcmp(value->fields[i].name, name) == 0)
            return &value->fields[i].value;
    return NULL;
}

static const VmHostValue *
ZxNativeFieldAt(const VmHostValue *value, size_t index, const char *name)
{
    if(index >= value->field_count || value->fields[index].name == NULL ||
       strcmp(value->fields[index].name, name) != 0)
        native_fail("host record %s has no field %s at position %zu",
                    value->type, name, index + 1);
    return &value->fields[index].value;
}

/* The host's bytes may be reused by its next call, so copy them into the
 * instance. Text the host returns again shares the first copy, which keeps
 * per-frame labels from growing memory. */
static void
ZxNativeString(const VmHostValue *value, const char **data, size_t *length)
{
    ZxNative *native = current;
    if(value->length == 0) {
        *data = "";
        *length = 0;
        return;
    }
    uint64_t hash = UINT64_C(14695981039346656037);
    for(size_t i = 0; i < value->length; i++)
        hash = (hash ^ value->data[i]) * UINT64_C(1099511628211);
    if(native->string_count >= native->string_buckets) {
        size_t buckets = native->string_buckets ? native->string_buckets * 2 : 256;
        HostString **grown = calloc(buckets, sizeof(*grown));
        if(grown == NULL)
            native_fail("out of memory");
        for(size_t i = 0; i < native->string_buckets; i++)
            for(HostString *item = native->strings[i], *next; item != NULL; item = next) {
                next = item->next;
                item->next = grown[item->hash % buckets];
                grown[item->hash % buckets] = item;
            }
        free(native->strings);
        native->strings = grown;
        native->string_buckets = buckets;
    }
    HostString **bucket = &native->strings[hash % native->string_buckets];
    for(HostString *item = *bucket; item != NULL; item = item->next)
        if(item->hash == hash && item->length == value->length &&
           memcmp(item->data, value->data, value->length) == 0) {
            *data = item->data;
            *length = item->length;
            return;
        }
    HostString *item = malloc(sizeof(*item) + value->length + 1);
    if(item == NULL)
        native_fail("out of memory");
    item->hash = hash;
    item->length = value->length;
    memcpy(item->data, value->data, value->length);
    item->data[value->length] = '\0';
    item->next = *bucket;
    *bucket = item;
    native->string_count++;
    *data = item->data;
    *length = item->length;
}

/* Generated code reports a failure on stderr, then aborts. Keep the message
 * for the diagnostic and stop the run instead of the process. */
static int
bundle_fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int written;
    if(stream == bundle_stderr && current != NULL) {
        va_list copy;
        va_copy(copy, args);
        append_error(current, format, copy);
        va_end(copy);
        written = vfprintf(stream, format, args);
    } else {
        written = vfprintf(stream, format, args);
    }
    va_end(args);
    return written;
}

static void
bundle_abort(void)
{
    if(current != NULL && current->error[0] != '\0') {
        size_t length = strlen(current->error);
        while(length > 0 && current->error[length - 1] == '\n')
            current->error[--length] = '\0';
        native_fail(NULL);
    }
    native_fail("aborted");
}

static void
bundle_exit(int status)
{
    native_fail("exit(%d) called inside a bundle", status);
}

static void
ZxNativeOverrun(void)
{
    native_fail("stopped after %d steps", current->max_steps);
}

static void
ZxNativeStackOverrun(void)
{
    native_fail("nested calls filled the stack");
}

static void
ZxNativeUnreachable(const char *place)
{
    native_fail("%s: unreachable code executed", place);
}

/* Parallel regions run serially on the calling thread: declining a thread
 * makes them run each chunk in place, as the VM runs them. */
static int
bundle_thread_create(void *thread, const void *attributes, void *(*start)(void *),
                     void *argument)
{
    (void)thread;
    (void)attributes;
    (void)start;
    (void)argument;
    return EAGAIN;
}

static int
bundle_thread_join(unsigned long thread, void **result)
{
    (void)thread;
    (void)result;
    return EINVAL;
}

/* Bundles see no process environment. */
static char *
bundle_getenv(const char *name)
{
    (void)name;
    return NULL;
}

/* Arithmetic TinyCC calls instead of emitting inline code. */
#if defined(__x86_64__)
static unsigned long long fixunsdfdi(double x) { return (unsigned long long)x; }
static unsigned long long fixunssfdi(float x) { return (unsigned long long)x; }
static double floatundidf(unsigned long long x) { return (double)x; }
static float floatundisf(unsigned long long x) { return (float)x; }
#elif defined(__arm__)
/* The compiler runtime's ARM EABI helpers return pairs in registers, so they
 * are passed through by address rather than wrapped in C. */
#define ARM_HELPERS(X) X(__aeabi_idiv) X(__aeabi_uidiv) X(__aeabi_idivmod) \
    X(__aeabi_uidivmod) X(__aeabi_ldivmod) X(__aeabi_uldivmod) X(__aeabi_l2f) \
    X(__aeabi_l2d) X(__aeabi_ul2f) X(__aeabi_ul2d) X(__aeabi_f2lz) X(__aeabi_d2lz) \
    X(__aeabi_f2ulz) X(__aeabi_d2ulz) X(__aeabi_lasr) X(__aeabi_llsr) X(__aeabi_llsl) \
    X(__aeabi_memcpy) X(__aeabi_memmove) X(__aeabi_memmove4) X(__aeabi_memmove8) \
    X(__aeabi_memset)
#define ARM_DECLARE(name) extern void name(void);
ARM_HELPERS(ARM_DECLARE)
#endif

typedef struct Symbol {
    const char *name;
    const void *address;
} Symbol;

static int
add_symbols(TCCState *state)
{
    const Symbol symbols[] = {
        {"ZxNativeHost", (const void *)ZxNativeHost},
        {"ZxNativeScratchReset", (const void *)ZxNativeScratchReset},
        {"ZxNativeScratch", (const void *)ZxNativeScratch},
        {"ZxNativeKeep", (const void *)ZxNativeKeep},
        {"ZxNativeString", (const void *)ZxNativeString},
        {"ZxNativeExpect", (const void *)ZxNativeExpect},
        {"ZxNativeExpectLength", (const void *)ZxNativeExpectLength},
        {"ZxNativeExpectFields", (const void *)ZxNativeExpectFields},
        {"ZxNativeExpectSlice", (const void *)ZxNativeExpectSlice},
        {"ZxNativeDistinct", (const void *)ZxNativeDistinct},
        {"ZxNativeMatchByName", (const void *)ZxNativeMatchByName},
        {"ZxNativeField", (const void *)ZxNativeField},
        {"ZxNativeFieldAt", (const void *)ZxNativeFieldAt},
        {"ZxNativeOverrun", (const void *)ZxNativeOverrun},
        {"ZxNativeStackOverrun", (const void *)ZxNativeStackOverrun},
        {"ZxNativeUnreachable", (const void *)ZxNativeUnreachable},
        {"stdout", (const void *)&bundle_stdout},
        {"stderr", (const void *)&bundle_stderr},
        {"fprintf", (const void *)bundle_fprintf},
        {"abort", (const void *)bundle_abort},
        {"exit", (const void *)bundle_exit},
        {"pthread_create", (const void *)bundle_thread_create},
        {"pthread_join", (const void *)bundle_thread_join},
        {"getenv", (const void *)bundle_getenv},
        {"strtol", (const void *)strtol},
        {"printf", (const void *)printf},
        {"snprintf", (const void *)snprintf},
        {"fputs", (const void *)fputs},
        {"fputc", (const void *)fputc},
        {"putchar", (const void *)putchar},
        {"puts", (const void *)puts},
        {"fwrite", (const void *)fwrite},
        {"fflush", (const void *)fflush},
        {"malloc", (const void *)malloc},
        {"calloc", (const void *)calloc},
        {"realloc", (const void *)realloc},
        {"free", (const void *)free},
        {"atoi", (const void *)atoi},
        {"strtod", (const void *)strtod},
        {"strtof", (const void *)strtof},
        {"memcpy", (const void *)memcpy},
        {"memmove", (const void *)memmove},
        {"memset", (const void *)memset},
        {"memcmp", (const void *)memcmp},
        {"strlen", (const void *)strlen},
        {"strcmp", (const void *)strcmp},
        {"strncmp", (const void *)strncmp},
#if defined(__x86_64__)
        {"__fixunsdfdi", (const void *)fixunsdfdi},
        {"__fixunssfdi", (const void *)fixunssfdi},
        {"__floatundidf", (const void *)floatundidf},
        {"__floatundisf", (const void *)floatundisf},
#elif defined(__arm__)
#define ARM_SYMBOL(name) {#name, (const void *)name},
        ARM_HELPERS(ARM_SYMBOL)
#endif
    };
    for(size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
        if(tcc_add_symbol(state, symbols[i].name, symbols[i].address) < 0)
            return 0;
    return 1;
}

/* ---- Compilation ---- */

static int
write_text(const char *directory, const char *name, const char *text)
{
    char path[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "wb");
    if(file == NULL)
        return 0;
    int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static int
remove_entry(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)flag;
    (void)walk;
    remove(path);
    return 0;
}

static void
remove_tree(const char *path)
{
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

/* C sources the lowering wrote, found again for TinyCC. */
static TCCState *source_target;
static ZxNative *source_native;
static size_t source_root;
static int source_failed;

static int
add_source(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)walk;
    size_t length = strlen(path);
    if(flag != FTW_F || length < 3 || strcmp(path + length - 2, ".c") != 0)
        return 0;
    /* A module whose capability shares its C name with another module's
     * calls its own trampoline; defines apply to the files added after them. */
    ZxNative *native = source_native;
    const char *stem = path + source_root + 1;
    size_t stem_length = length - source_root - 3;
    for(int i = 0; i < native->rename_count; i += 3)
        if(strlen(native->renames[i]) == stem_length &&
           strncmp(native->renames[i], stem, stem_length) == 0)
            tcc_define_symbol(source_target, native->renames[i + 1], native->renames[i + 2]);
    if(tcc_add_file(source_target, path) < 0)
        source_failed = 1;
    for(int i = 0; i < native->rename_count; i += 3)
        if(strlen(native->renames[i]) == stem_length &&
           strncmp(native->renames[i], stem, stem_length) == 0)
            tcc_undefine_symbol(source_target, native->renames[i + 1]);
    return 0;
}

static void
tcc_message(void *opaque, const char *message)
{
    ZxNative *native = opaque;
    size_t used = strlen(native->error);
    if(used + 2 < sizeof(native->error))
        snprintf(native->error + used, sizeof(native->error) - used, "%s%s",
                 used ? "\n" : "", message);
}

/* The generated program's two fixed entry points: zx_native_start runs the
 * module initializers in import order and zx_native_run calls the bundle
 * entry, widening its result as the VM does. Cached objects carry them, so
 * a cache hit needs no lowering. */
static int
write_entry_points(FILE *out, const ZirProgram *program, const ZirCModuleSyms *symbols,
                   const ZirModule *owner, const ZirFunction *entry,
                   char *error, size_t size)
{
    const ZirCModuleSyms *owned = &symbols[owner - program->modules];
    const char *function = NULL;
    for(int i = 0; i < owned->fn_count && function == NULL; i++)
        if(strcmp(owned->fns[i].source, entry->name) == 0)
            function = owned->fns[i].c;
    if(function == NULL) {
        snprintf(error, size, "entry %s has no C function", entry->name);
        return 0;
    }
    const char *path = owner->source_path;
    size_t length = strlen(path);
    if(length > 3 && strcmp(path + length - 3, ".zi") == 0)
        length -= 3;
    fprintf(out, "\n#include \"%.*s.h\"\n", (int)length, path);
    char name[ZIR_NAME_MAX * 3];
    for(int i = 0; i < program->module_count; i++)
        if(ModuleNeedsStartup(&program->modules[i])) {
            NativeCModuleInitName(&program->modules[i], name, sizeof(name));
            fprintf(out, "void %s(void);\n", name);
        }
    fputs("\nvoid\nzx_native_start(void)\n{\n", out);
    for(int i = 0; i < program->module_count; i++)
        if(ModuleNeedsStartup(&program->modules[i])) {
            NativeCModuleInitName(&program->modules[i], name, sizeof(name));
            fprintf(out, "    %s();\n", name);
        }
    fputs("}\n\nlong long zx_native_budget = 9223372036854775807LL;\n"
          "char *zx_native_floor;\n\nint64_t\nzx_native_run(void)\n{\n", out);
    if(entry->return_type[0] == '\0' || strcmp(entry->return_type, "void") == 0)
        fprintf(out, "    %s();\n    return 0;\n}\n", function);
    else
        fprintf(out, "    return (int64_t)%s();\n}\n", function);
    return !ferror(out);
}

static int
compare_names(const void *left, const void *right)
{
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* Whether two modules define one C function name, as two libraries'
 * exported `main` do. Pruning to what the entry reaches resolves it. */
static int
duplicate_functions(const ZirCModuleSyms *symbols, int count)
{
    size_t total = 0;
    for(int i = 0; i < count; i++)
        total += (size_t)symbols[i].fn_count;
    const char **names = malloc((total ? total : 1) * sizeof(*names));
    if(names == NULL)
        return 1;
    size_t used = 0;
    for(int i = 0; i < count; i++)
        for(int f = 0; f < symbols[i].fn_count; f++)
            names[used++] = symbols[i].fns[f].c;
    qsort(names, used, sizeof(*names), compare_names);
    int duplicate = 0;
    for(size_t i = 1; i < used && !duplicate; i++)
        duplicate = strcmp(names[i - 1], names[i]) == 0;
    free(names);
    return duplicate;
}

/* Generated headers, read back for their #include "..." lines. */
typedef struct Header {
    char *path; /* relative to the build directory */
    int *includes;
    int include_count;
    int depth; /* 0 unvisited, -1 visiting */
} Header;

typedef struct HeaderSet {
    Header *items;
    int count;
    int capacity;
    size_t root_length;
    int failed;
} HeaderSet;

static HeaderSet *header_target;

static int
collect_header(const char *path, const struct stat *status, int flag, struct FTW *walk)
{
    (void)status;
    (void)walk;
    HeaderSet *set = header_target;
    size_t length = strlen(path);
    if(flag != FTW_F || length < 3 || strcmp(path + length - 2, ".h") != 0 ||
       length <= set->root_length || strstr(path + set->root_length, "/zx_include/") != NULL)
        return 0;
    if(set->count == set->capacity) {
        int capacity = set->capacity ? set->capacity * 2 : 64;
        Header *grown = realloc(set->items, (size_t)capacity * sizeof(*grown));
        if(grown == NULL) {
            set->failed = 1;
            return 1;
        }
        set->items = grown;
        set->capacity = capacity;
    }
    Header *header = &set->items[set->count];
    memset(header, 0, sizeof(*header));
    header->path = strdup(path + set->root_length + 1);
    if(header->path == NULL) {
        set->failed = 1;
        return 1;
    }
    set->count++;
    return 0;
}

static int
find_header(const HeaderSet *set, const char *path)
{
    for(int i = 0; i < set->count; i++)
        if(strcmp(set->items[i].path, path) == 0)
            return i;
    return -1;
}

/* Quoted includes resolve beside the including header, then at the root. */
static void
read_includes(HeaderSet *set, int index, const char *directory)
{
    Header *header = &set->items[index];
    char path[ZIR_PATH_MAX], line[ZIR_PATH_MAX], candidate[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, header->path);
    FILE *file = fopen(path, "rb");
    if(file == NULL)
        return;
    const char *slash = strrchr(header->path, '/');
    int folder = slash != NULL ? (int)(slash - header->path) : 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        if(strncmp(line, "#include \"", 10) != 0)
            continue;
        char *close = strchr(line + 10, '"');
        if(close == NULL)
            continue;
        *close = '\0';
        int found = -1;
        if(folder > 0) {
            snprintf(candidate, sizeof(candidate), "%.*s/%s", folder, header->path, line + 10);
            found = find_header(set, candidate);
        }
        if(found < 0)
            found = find_header(set, line + 10);
        if(found < 0)
            continue;
        int *grown = realloc(header->includes, (size_t)(header->include_count + 1) * sizeof(*grown));
        if(grown == NULL) {
            set->failed = 1;
            break;
        }
        header = &set->items[index];
        header->includes = grown;
        header->includes[header->include_count++] = found;
    }
    fclose(file);
}

/* Depth-first, writing each header after the headers it includes: the
 * order in which a C preprocessor completes them. */
static int
include_depth(HeaderSet *set, int index, int *order, int *ordered)
{
    Header *header = &set->items[index];
    if(header->depth != 0)
        return header->depth < 0 ? 0 : header->depth;
    header->depth = -1; /* include guards stop cycles */
    int deepest = 0;
    for(int i = 0; i < set->items[index].include_count; i++) {
        int depth = include_depth(set, set->items[index].includes[i], order, ordered);
        if(depth > deepest)
            deepest = depth;
    }
    order[(*ordered)++] = index;
    set->items[index].depth = deepest + 1;
    return deepest + 1;
}

/* Module headers include the headers of their imports, and TinyCC allows 32
 * nested includes, while import chains, and cycles of re-exports, can be
 * longer. Then every source first includes zx_headers.h: all generated
 * headers' text in completion order, without the includes between them.
 * Their guards are then set, so the sources' own includes add nothing. */
enum { INCLUDE_DEPTH_LIMIT = 24 };

static int
append_header(FILE *out, const HeaderSet *set, const Header *header, const char *directory)
{
    char path[ZIR_PATH_MAX], line[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", directory, header->path);
    FILE *file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        int generated = 0;
        if(strncmp(line, "#include \"", 10) == 0) {
            for(int i = 0; i < header->include_count && !generated; i++) {
                const char *name = set->items[header->includes[i]].path;
                const char *base = strrchr(name, '/');
                size_t length = strlen(base != NULL ? base + 1 : name);
                const char *quoted = line + 10;
                const char *close = strchr(quoted, '"');
                generated = close != NULL && (size_t)(close - quoted) >= length &&
                            memcmp(close - length, base != NULL ? base + 1 : name, length) == 0;
            }
        }
        if(!generated)
            fputs(line, out);
    }
    fputc('\n', out);
    int ok = !ferror(file);
    fclose(file);
    return ok;
}

static int
write_header_order(ZxNative *native, const char *directory)
{
    HeaderSet set = {.root_length = strlen(directory)};
    header_target = &set;
    nftw(directory, collect_header, 16, FTW_PHYS);
    for(int i = 0; i < set.count && !set.failed; i++)
        read_includes(&set, i, directory);
    int *order = calloc((size_t)set.count + 1, sizeof(*order));
    int ordered = 0, deepest = 0;
    for(int i = 0; order != NULL && !set.failed && i < set.count; i++) {
        int depth = include_depth(&set, i, order, &ordered);
        if(depth > deepest)
            deepest = depth;
    }
    int ok = order != NULL && !set.failed;
    native->include_headers = ok && deepest >= INCLUDE_DEPTH_LIMIT;
    if(native->include_headers) {
        char path[ZIR_PATH_MAX];
        snprintf(path, sizeof(path), "%s/zx_headers.h", directory);
        FILE *out = fopen(path, "wb");
        ok = out != NULL;
        for(int i = 0; ok && i < ordered; i++)
            ok = append_header(out, &set, &set.items[order[i]], directory);
        if(out != NULL && fclose(out) != 0)
            ok = 0;
    }
    for(int i = 0; i < set.count; i++) {
        free(set.items[i].path);
        free(set.items[i].includes);
    }
    free(set.items);
    free(order);
    return ok;
}

static int
record_renames(ZxNative *native, const ZirProgram *program, const ZxCapabilityName *names)
{
    for(size_t i = 0; i < native->capability_count; i++) {
        if(!names[i].renamed)
            continue;
        const ZirModule *module = NULL;
        for(int m = 0; m < program->module_count && module == NULL; m++)
            if(strcmp(program->modules[m].name, names[i].module) == 0)
                module = &program->modules[m];
        if(module == NULL)
            return 0;
        char **grown = realloc(native->renames, (size_t)(native->rename_count + 3) * sizeof(*grown));
        if(grown == NULL)
            return 0;
        native->renames = grown;
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "zx_capability_%zu", i);
        size_t length = strlen(module->source_path);
        if(length > 3 && strcmp(module->source_path + length - 3, ".zi") == 0)
            length -= 3;
        native->renames[native->rename_count] = strndup(module->source_path, length);
        native->renames[native->rename_count + 1] = strdup(names[i].symbol);
        native->renames[native->rename_count + 2] = strdup(replacement);
        native->rename_count += 3;
        for(int k = native->rename_count - 3; k < native->rename_count; k++)
            if(native->renames[k] == NULL)
                return 0;
    }
    return 1;
}

/* Returns 1 when written, 0 on failure, and -1 when the program must first
 * be pruned to its entry. */
static int
lower_program(ZirProgram *program, const ZirModule *owner, const ZirFunction *entry,
              ZxNative *native, const char *directory, int pruned)
{
    char include[ZIR_PATH_MAX];
    ZirCModuleSyms *symbols = NULL;
    ZxCapabilityName *names = NULL;
    int ok = 0;
    CompilerContext *previous = CompilerContextSelect(program->context);
    if(!PrepareNativeNames(&program, 1))
        goto done;
    symbols = calloc((size_t)program->module_count + 1, sizeof(*symbols));
    names = calloc(native->capability_count + 1, sizeof(*names));
    if(symbols == NULL || names == NULL)
        goto done;
    for(int i = 0; i < program->module_count; i++) {
        ZirProgram view = {0};
        view.context = program->context;
        view.modules = &program->modules[i];
        view.module_count = 1;
        if(!c_build_syms(&view, &symbols[i]))
            goto done;
    }
    /* C lowering also writes a C main() for each module's program main. */
    int mains = 0;
    for(int m = 0; m < program->module_count; m++)
        for(int f = 0; f < program->modules[m].function_count; f++)
            mains += !program->modules[m].functions[f].is_template &&
                     NativeProgramMain(&program->modules[m], &program->modules[m].functions[f]);
    if(!pruned && (mains > 1 || duplicate_functions(symbols, program->module_count))) {
        ok = -1;
        goto done;
    }
    EmitRunGuards(1);
    int lowered = c_lower(program, ".", directory, symbols, program->module_count, 1);
    EmitRunGuards(0);
    if(!lowered || !write_header_order(native, directory))
        goto done;
    for(size_t i = 0; i < native->capability_count; i++) {
        names[i].module = native->capability_modules[i];
        names[i].function = native->capability_functions[i];
    }
    char path[ZIR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/zx_native_host.c", directory);
    FILE *out = fopen(path, "wb");
    if(out == NULL)
        goto done;
    ok = ZxWriteHostTrampolines(program, names, native->capability_count, out,
                                native->error, sizeof(native->error)) &&
         write_entry_points(out, program, symbols, owner, entry,
                            native->error, sizeof(native->error)) &&
         record_renames(native, program, names);
    if(fclose(out) != 0)
        ok = 0;
    snprintf(include, sizeof(include), "%s/zx_include", directory);
    if(ok && mkdir(include, 0700) != 0)
        ok = 0;
    for(size_t i = 0; ok && ZxNativeHeaders[i].name != NULL; i++)
        ok = write_text(include, ZxNativeHeaders[i].name, ZxNativeHeaders[i].text);
    if(ok)
        ok = write_text(include, "zx_native_services.h", ZxNativeServicesHeader) &&
             write_text(include, "zx_guard.h", guard_header);
done:
    CompilerContextSelect(previous);
    if(symbols != NULL)
        c_free_syms(symbols, program->module_count);
    free(symbols);
    free(names);
    return ok;
}

static TCCState *
new_compiler(ZxNative *native, int output)
{
    TCCState *state = tcc_new();
    if(state == NULL)
        return NULL;
    tcc_set_error_func(state, native, tcc_message);
    if(tcc_set_options(state, "-nostdinc -nostdlib -w") < 0 ||
       tcc_set_output_type(state, output) < 0) {
        tcc_delete(state);
        return NULL;
    }
    /* TinyCC predefines `arm` and `arm_elf` on ARM, which are ordinary
     * identifiers in Ziran programs. */
    tcc_undefine_symbol(state, "arm");
    tcc_undefine_symbol(state, "arm_elf");
    return state;
}

static int
add_sources(ZxNative *native, TCCState *state, const char *directory)
{
    char include[ZIR_PATH_MAX];
    snprintf(include, sizeof(include), "%s/zx_include", directory);
    tcc_add_sysinclude_path(state, include);
    tcc_add_include_path(state, directory);
    if(tcc_set_options(state, "-include zx_guard.h") < 0 ||
       (native->include_headers && tcc_set_options(state, "-include zx_headers.h") < 0))
        return 0;
    source_target = state;
    source_native = native;
    source_root = strlen(directory);
    source_failed = 0;
    nftw(directory, add_source, 16, FTW_PHYS);
    return !source_failed;
}

/* Link compiled code (C sources or a cached object) into this instance. */
static int
link_program(ZxNative *native, const char *directory, const char *object)
{
    native->tcc = new_compiler(native, TCC_OUTPUT_MEMORY);
    if(native->tcc == NULL || !add_symbols(native->tcc))
        return 0;
    if(object != NULL ? tcc_add_file(native->tcc, object) < 0 :
       !add_sources(native, native->tcc, directory))
        return 0;
    if(tcc_relocate(native->tcc) < 0)
        return 0;
    native->start = (void (*)(void))tcc_get_symbol(native->tcc, "zx_native_start");
    native->run = (int64_t (*)(void))tcc_get_symbol(native->tcc, "zx_native_run");
    native->budget = (long long *)tcc_get_symbol(native->tcc, "zx_native_budget");
    native->floor = (char **)tcc_get_symbol(native->tcc, "zx_native_floor");
    if(native->start == NULL || native->run == NULL || native->budget == NULL ||
       native->floor == NULL) {
        snprintf(native->error, sizeof(native->error), "compiled bundle has no entry points");
        return 0;
    }
    return 1;
}

static int
write_object(ZxNative *native, const char *directory, const char *path)
{
    char temporary[ZIR_PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.%ld.tmp", path, (long)getpid());
    TCCState *state = new_compiler(native, TCC_OUTPUT_OBJ);
    int ok = state != NULL && add_sources(native, state, directory) &&
             tcc_output_file(state, temporary) >= 0;
    if(state != NULL)
        tcc_delete(state);
    if(ok && (chmod(temporary, 0600) != 0 || rename(temporary, path) != 0))
        ok = 0;
    if(!ok)
        remove(temporary);
    return ok;
}

/* ---- Compiled code cache ---- */

/* Objects are keyed by the bundle's bytes (with its libraries), the CPU and
 * the identity of the compiler sources that produced them, which the build
 * computes; without that identity nothing is cached. */
enum { CACHE_ENTRIES = 32 };

static int
cache_directory(char *path, size_t size)
{
#ifndef ZX_NATIVE_IDENTITY
    (void)path;
    (void)size;
    return 0;
#else
    const char *base = getenv("ZIRAN_NATIVE_CACHE");
    if(base != NULL)
        return base[0] != '\0' && strcmp(base, "0") != 0 &&
               snprintf(path, size, "%s", base) > 0 && (mkdir(path, 0700) == 0 || errno == EEXIST);
    if((base = getenv("XDG_CACHE_HOME")) != NULL && base[0] != '\0')
        snprintf(path, size, "%s/ziran-native", base);
    else if((base = getenv("HOME")) != NULL && base[0] != '\0') {
        snprintf(path, size, "%s/.cache", base);
        mkdir(path, 0700);
        snprintf(path, size, "%s/.cache/ziran-native", base);
    } else if((base = getenv("TMPDIR")) != NULL && base[0] != '\0')
        snprintf(path, size, "%s/ziran-native-cache", base);
    else
        return 0;
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

static void
cache_key(const unsigned char identity[32], char hex[65])
{
#ifdef ZX_NATIVE_IDENTITY
    static const char compiler[] = "ziran-zx-native-1 " ZX_NATIVE_IDENTITY;
#else
    static const char compiler[] = "ziran-zx-native-1";
#endif
#if defined(__x86_64__)
    static const char machine[] = "x86_64";
#elif defined(__aarch64__)
    static const char machine[] = "aarch64";
#elif defined(__arm__) && defined(__ARM_PCS_VFP)
    static const char machine[] = "arm-hard-float";
#elif defined(__arm__)
    static const char machine[] = "arm-soft-float";
#else
    static const char machine[] = "unknown";
#endif
    Sha256 hash;
    unsigned char digest[32];
    Sha256Begin(&hash);
    Sha256Add(&hash, compiler, sizeof(compiler));
    Sha256Add(&hash, machine, sizeof(machine));
    Sha256Add(&hash, identity, 32);
    Sha256End(&hash, digest);
    for(int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
}

/* Keep the most recently used objects; a hit refreshes its time. */
static void
cache_prune(const char *directory)
{
    DIR *folder = opendir(directory);
    if(folder == NULL)
        return;
    char oldest[ZIR_PATH_MAX] = "";
    time_t oldest_time = 0;
    int count = 0;
    struct dirent *entry;
    while((entry = readdir(folder)) != NULL) {
        size_t length = strlen(entry->d_name);
        if(length != 66 || strcmp(entry->d_name + 64, ".o") != 0)
            continue;
        char path[ZIR_PATH_MAX];
        struct stat status;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if(stat(path, &status) != 0)
            continue;
        count++;
        if(oldest[0] == '\0' || status.st_mtime < oldest_time) {
            oldest_time = status.st_mtime;
            snprintf(oldest, sizeof(oldest), "%s", path);
        }
    }
    closedir(folder);
    if(count > CACHE_ENTRIES && oldest[0] != '\0')
        remove(oldest);
}

static int
make_directory(char *path, size_t size)
{
    const char *base = getenv("ZIRAN_NATIVE_DIR");
    if(base == NULL || base[0] == '\0')
        base = getenv("TMPDIR");
    if(base == NULL || base[0] == '\0')
        base = "/tmp";
    snprintf(path, size, "%s/ziran-zx-XXXXXX", base);
    return mkdtemp(path) != NULL;
}

static double
milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1e3 + now.tv_nsec / 1e6;
}

static int
environment_set(const char *name)
{
    const char *value = getenv(name);
    return value != NULL && strcmp(value, "1") == 0;
}

static void
free_program(const ZirProgram *context, ZirProgram *program)
{
    if(program == NULL)
        return;
    CompilerContext *previous = CompilerContextSelect(context->context);
    ProgramFree(program);
    CompilerContextSelect(previous);
}

/* Lower the program and compile it, into the cache when there is one. The
 * bundle's program is already pruned to its entry; lowering only records
 * native spellings in it, which the portable VM does not read. */
static int
compile(ZxNative *native, const ZirProgram *program, const ZirModule *entry_owner,
        const ZirFunction *entry, const char *object)
{
    char directory[ZIR_PATH_MAX];
    if(!make_directory(directory, sizeof(directory))) {
        snprintf(native->error, sizeof(native->error),
                 "cannot create a build directory under $ZIRAN_NATIVE_DIR or $TMPDIR");
        return 0;
    }
    double start = milliseconds();
    ZirProgram *linked = NULL;
    int ok = lower_program((ZirProgram *)program, entry_owner, entry, native, directory, 0);
    if(ok < 0) {
        /* Library modules may export names the entry never reaches. */
        remove_tree(directory);
        mkdir(directory, 0700);
        CompilerContext *previous = CompilerContextSelect(program->context);
        linked = NativeLink(program, entry_owner->name, entry->name);
        CompilerContextSelect(previous);
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        for(int m = 0; linked != NULL && m < linked->module_count && function == NULL; m++)
            for(int f = 0; strcmp(linked->modules[m].name, entry_owner->name) == 0 &&
                           f < linked->modules[m].function_count; f++)
                if(strcmp(linked->modules[m].functions[f].name, entry->name) == 0) {
                    owner = &linked->modules[m];
                    function = &linked->modules[m].functions[f];
                    break;
                }
        ok = function != NULL && lower_program(linked, owner, function, native, directory, 1) > 0;
        if(function == NULL)
            snprintf(native->error, sizeof(native->error), "cannot link entry %s", entry->name);
    }
    double lowered = milliseconds();
    if(ok && object != NULL && write_object(native, directory, object)) {
        ok = link_program(native, NULL, object);
    } else if(ok) {
        /* Without a writable cache the program still compiles, in memory. */
        native->error[0] = '\0';
        ok = link_program(native, directory, NULL);
    }
    /* ZIRAN_NATIVE_TIMING=1 reports where compilation time goes. */
    if(environment_set("ZIRAN_NATIVE_TIMING"))
        fprintf(stderr, "ziran: native %s: lowering %.1f ms%s, compiling %.1f ms\n",
                entry->name, lowered - start, linked != NULL ? " (pruned)" : "",
                milliseconds() - lowered);
    /* ZIRAN_NATIVE_KEEP=1 leaves the generated C for inspection. */
    if(environment_set("ZIRAN_NATIVE_KEEP"))
        fprintf(stderr, "ziran: native build kept in %s\n", directory);
    else
        remove_tree(directory);
    free_program(program, linked);
    return ok;
}

/* The lowest address calls may reach on this thread, or NULL when the
 * thread's stack is unknown. */
static char *
stack_floor(void)
{
#if defined(__linux__)
    pthread_attr_t attributes;
    char *floor = NULL;
    if(pthread_getattr_np(pthread_self(), &attributes) == 0) {
        void *low = NULL;
        size_t size = 0;
        /* The frame address is on the thread's stack even where a sanitizer
         * moves locals elsewhere. */
        char *here = __builtin_frame_address(0);
        if(pthread_attr_getstack(&attributes, &low, &size) == 0 &&
           here > (char *)low + 2 * STACK_MARGIN)
            floor = (char *)low + STACK_MARGIN;
        pthread_attr_destroy(&attributes);
    }
    return floor;
#else
    return NULL;
#endif
}

static void
arm_guards(ZxNative *native)
{
    *native->budget = native->max_steps > 0 ? native->max_steps : 9223372036854775807LL;
    *native->floor = stack_floor();
}

static int
start_modules(ZxNative *native)
{
    jmp_buf escape;
    ZxNative *previous = current;
    current = native;
    native->escape = &escape;
    if(setjmp(escape) != 0) {
        native->escape = NULL;
        current = previous;
        return 0;
    }
    arm_guards(native);
    native->start();
    native->escape = NULL;
    current = previous;
    return 1;
}

ZxNative *
ZxNativeOpen(const ZirProgram *program, const ZirModule *entry_owner,
             const ZirFunction *entry, const unsigned char identity[32],
             const char *const *capability_modules,
             const char *const *capability_functions, size_t capability_count,
             VmHostCall host, void *context)
{
    if(program == NULL || entry_owner == NULL || entry == NULL || host == NULL)
        return NULL;
    ZxNative *native = calloc(1, sizeof(*native));
    if(native == NULL)
        return NULL;
    native->host = host;
    native->context = context;
    native->capability_modules = capability_modules;
    native->capability_functions = capability_functions;
    native->capability_count = capability_count;
    native->entry = entry;
    native->has_result = entry->return_type[0] != '\0' &&
                         strcmp(entry->return_type, "void") != 0;
    pthread_mutex_lock(&compile_lock);
    if(bundle_stdout == NULL) {
        bundle_stdout = stdout;
        bundle_stderr = stderr;
    }
    char folder[ZIR_PATH_MAX], object[ZIR_PATH_MAX], key[65];
    int cached = identity != NULL && cache_directory(folder, sizeof(folder));
    int ok = 0;
    if(cached) {
        cache_key(identity, key);
        snprintf(object, sizeof(object), "%s/%s.o", folder, key);
        if(access(object, R_OK) == 0) {
            double start = milliseconds();
            ok = link_program(native, NULL, object);
            if(ok) {
                utime(object, NULL);
                if(environment_set("ZIRAN_NATIVE_TIMING"))
                    fprintf(stderr, "ziran: native %s: cached, linking %.1f ms\n",
                            entry->name, milliseconds() - start);
            } else {
                /* An unusable object is rebuilt, never run. */
                remove(object);
                if(native->tcc != NULL)
                    tcc_delete(native->tcc);
                native->tcc = NULL;
                native->error[0] = '\0';
            }
        }
    }
    if(!ok) {
        ok = compile(native, program, entry_owner, entry, cached ? object : NULL);
        if(cached)
            cache_prune(folder);
    }
    pthread_mutex_unlock(&compile_lock);
    if(ok)
        ok = start_modules(native);
    if(!ok) {
        Diagnostic(entry->span, "zx.native", "cannot run natively: %s",
                   native->error[0] ? native->error : "compilation failed");
        ZxNativeClose(native);
        return NULL;
    }
    return native;
}

int
ZxNativeRun(ZxNative *native, long long *result, int *has_result)
{
    if(native == NULL || result == NULL || has_result == NULL || native->failed)
        return 0;
    ZxNative *previous = current;
    jmp_buf *outer = native->escape;
    jmp_buf escape;
    current = native;
    native->escape = &escape;
    native->error[0] = '\0';
    if(setjmp(escape) != 0) {
        native->escape = outer;
        current = previous;
        Diagnostic(native->entry->span, "zx.runtime",
                   "portable execution failed: %s", native->error);
        return 0;
    }
    arm_guards(native);
    long long value = (long long)native->run();
    native->escape = outer;
    current = previous;
    *result = value;
    *has_result = native->has_result;
    return 1;
}

void
ZxNativeLimitSteps(ZxNative *native, int max_steps)
{
    if(native != NULL)
        native->max_steps = max_steps > 0 ? max_steps : 0;
}

void
ZxNativeMatchFieldsByName(ZxNative *native, int enabled)
{
    if(native != NULL)
        native->match_fields_by_name = enabled != 0;
}

static void
free_blocks(Block *block)
{
    while(block != NULL) {
        Block *next = block->next;
        free(block);
        block = next;
    }
}

void
ZxNativeClose(ZxNative *native)
{
    if(native == NULL)
        return;
    if(environment_set("ZIRAN_NATIVE_TIMING")) {
        size_t kept = 0, text = 0;
        for(Block *block = native->kept; block != NULL; block = block->next)
            kept += block->used;
        for(size_t i = 0; i < native->string_buckets; i++)
            for(HostString *item = native->strings[i]; item != NULL; item = item->next)
                text += item->length;
        fprintf(stderr, "ziran: native %s closed: %zu host strings (%zu bytes), %zu bytes of host slices\n",
                native->entry->name, native->string_count, text, kept);
    }
    if(native->tcc != NULL)
        tcc_delete(native->tcc);
    free_blocks(native->scratch);
    free_blocks(native->kept);
    for(size_t i = 0; i < native->string_buckets; i++)
        for(HostString *item = native->strings[i], *next; item != NULL; item = next) {
            next = item->next;
            free(item);
        }
    free(native->strings);
    for(int i = 0; i < native->rename_count; i++)
        free(native->renames[i]);
    free(native->renames);
    free(native);
}
