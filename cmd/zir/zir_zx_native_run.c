/* Running natively compiled bundles.
 *
 * Compiled code, whether TinyCC compiled it in this process
 * (zir_zx_native.c) or it was compiled ahead of time and linked into the
 * host (ZxNativeRegister), runs here: its module initializers, its entry,
 * the services it calls for host capabilities and memory, its parallel
 * regions, and its failures. A runtime failure (bounds, division, a host
 * breaking the value contract) stops the run and leaves the instance
 * unusable, as in the VM. */
#include "zir_zx_native_internal.h"
#include "zir_diagnostic.h"

#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/stack.h>
#else
#include <sys/mman.h>
#endif

/* Bump storage. Scratch blocks hold one host call's argument values and are
 * rewound before the next call; kept blocks live until the instance closes. */
struct ZxNativeBlock {
    struct ZxNativeBlock *next;
    size_t used;
    size_t size;
    max_align_t data[];
};

/* Host strings copied into the instance, once per distinct text. */
struct ZxNativeKeptText {
    struct ZxNativeKeptText *next;
    uint64_t hash;
    size_t length;
    char data[];
};

static _Thread_local ZxNative *current;
/* A parallel region's worker fails into its own escape point and message;
 * the region then reports the first failure on the calling thread. */
static _Thread_local jmp_buf *worker_escape;
static _Thread_local char worker_error[ZX_NATIVE_ERROR_SIZE];
/* The streams compiled code names stdout and stderr. */
FILE *ZxNativeStdout;
FILE *ZxNativeStderr;

/* Room left below the floor for host callbacks and the C library. */
enum { STACK_MARGIN = 256 * 1024 };

int
ZxNativeEnvironmentSet(const char *name)
{
    const char *value = getenv(name);
    return value != NULL && strcmp(value, "1") == 0;
}

/* Where this thread's failure message goes. */
static char *
error_text(ZxNative *native)
{
    return worker_escape != NULL ? worker_error : native->error;
}

static void
append_error(char *error, const char *format, va_list args)
{
    size_t used = strlen(error);
    if(used + 1 < ZX_NATIVE_ERROR_SIZE)
        vsnprintf(error + used, ZX_NATIVE_ERROR_SIZE - used, format, args);
}

/* Stop the running bundle. Only called while ZxNativeRun, initialization or
 * a parallel region's worker has an escape point. */
static void
native_fail(const char *format, ...)
{
    ZxNative *native = current;
    jmp_buf *escape = worker_escape != NULL ? worker_escape :
                      native != NULL ? native->escape : NULL;
    if(native == NULL || escape == NULL)
        abort();
    if(format != NULL) {
        va_list args;
        va_start(args, format);
        char *error = error_text(native);
        error[0] = '\0';
        append_error(error, format, args);
        va_end(args);
    }
    if(worker_escape == NULL)
        native->failed = 1;
    longjmp(*escape, 1);
}

/* ---- Services the generated code calls ---- */

static void *
block_take(ZxNativeBlock **chain, ZxNativeBlock **at, size_t size)
{
    size = (size + sizeof(max_align_t) - 1) / sizeof(max_align_t) * sizeof(max_align_t);
    ZxNativeBlock *block = at != NULL ? *at : *chain;
    while(block != NULL && block->size - block->used < size)
        block = block->next;
    if(block == NULL) {
        size_t capacity = size > 65536 ? size : 65536;
        block = malloc(sizeof(ZxNativeBlock) + capacity);
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

void
ZxNativeScratchReset(void)
{
    for(ZxNativeBlock *block = current->scratch; block != NULL; block = block->next)
        block->used = 0;
    current->scratch_at = current->scratch;
}

void *
ZxNativeScratch(size_t size)
{
    return block_take(&current->scratch, &current->scratch_at, size);
}

void *
ZxNativeKeep(size_t size)
{
    return block_take(&current->kept, NULL, size);
}

void
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

void
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

void
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
void
ZxNativeExpectSlice(const VmHostValue *value, size_t element_size)
{
    if(value->length > INT32_MAX ||
       value->length > (size_t)1024 * 1024 * 1024 / (element_size ? element_size : 1))
        native_fail("host returned %zu elements of %s, more than a bundle can hold",
                    value->length, value->type);
    if(value->length > 0 && value->elements == NULL)
        native_fail("host returned %s without its elements", value->type);
}

void
ZxNativeExpectFields(const VmHostValue *value, size_t count)
{
    if(value->field_count != count)
        native_fail("host returned %zu fields of %s, which declares %zu",
                    value->field_count, value->type, count);
}

void
ZxNativeDistinct(const void *a, size_t a_bytes, const void *b, size_t b_bytes)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if(a_bytes > 0 && b_bytes > 0 && x < y + b_bytes && y < x + a_bytes)
        native_fail("overlapping mutable host slice arguments require separate storage");
}

int
ZxNativeMatchByName(void)
{
    return current->match_fields_by_name;
}

const VmHostValue *
ZxNativeField(const VmHostValue *value, const char *name)
{
    for(size_t i = 0; i < value->field_count; i++)
        if(value->fields[i].name != NULL && strcmp(value->fields[i].name, name) == 0)
            return &value->fields[i].value;
    return NULL;
}

const VmHostValue *
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
void
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
        ZxNativeKeptText **grown = calloc(buckets, sizeof(*grown));
        if(grown == NULL)
            native_fail("out of memory");
        for(size_t i = 0; i < native->string_buckets; i++)
            for(ZxNativeKeptText *item = native->strings[i], *next; item != NULL; item = next) {
                next = item->next;
                item->next = grown[item->hash % buckets];
                grown[item->hash % buckets] = item;
            }
        free(native->strings);
        native->strings = grown;
        native->string_buckets = buckets;
    }
    ZxNativeKeptText **bucket = &native->strings[hash % native->string_buckets];
    for(ZxNativeKeptText *item = *bucket; item != NULL; item = item->next)
        if(item->hash == hash && item->length == value->length &&
           memcmp(item->data, value->data, value->length) == 0) {
            *data = item->data;
            *length = item->length;
            return;
        }
    ZxNativeKeptText *item = malloc(sizeof(*item) + value->length + 1);
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
int
ZxNativePrint(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int written;
    if(stream == ZxNativeStderr && current != NULL) {
        va_list copy;
        va_copy(copy, args);
        append_error(error_text(current), format, copy);
        va_end(copy);
        /* A worker's message is printed only if its region reports it. */
        written = worker_escape != NULL ? (int)strlen(format) : vfprintf(stream, format, args);
    } else {
        written = vfprintf(stream, format, args);
    }
    va_end(args);
    return written;
}

void
ZxNativeAbort(void)
{
    char *error = current != NULL ? error_text(current) : NULL;
    if(error != NULL && error[0] != '\0') {
        size_t length = strlen(error);
        while(length > 0 && error[length - 1] == '\n')
            error[--length] = '\0';
        native_fail(NULL);
    }
    native_fail("aborted");
}

void
ZxNativeExit(int status)
{
    native_fail("exit(%d) called inside a bundle", status);
}

/* Bundles see no process environment. */
char *
ZxNativeEnvironment(const char *name)
{
    (void)name;
    return NULL;
}

void
ZxNativeOverrun(void)
{
    native_fail("stopped after %d steps", current->max_steps);
}

void
ZxNativeStackOverrun(void)
{
    native_fail("nested calls filled the stack");
}

void
ZxNativeUnreachable(const char *place)
{
    native_fail("%s: unreachable code executed", place);
}

/* ---- Parallel regions ----
 *
 * `#parallel for` regions run on a pool of worker threads each instance
 * starts at its first region, one per processor (ZIRAN_PAR_THREADS in the
 * host's environment chooses 1-16). The checker admits only bodies whose
 * iterations share no mutable storage and call no host code, so any
 * interleaving gives the serial result. The calling thread waits while the
 * workers take chunks of the range in order. After a failure no new chunk
 * starts; the ones running finish, and the failure in the lowest chunk, the
 * one a serial run meets first, is reported on the calling thread. Each
 * worker counts its steps on its own stack page, starting from what the run
 * has left; the region then charges the run for every worker's steps. The
 * browser runs regions in place. */

typedef void (*ParallelWorker)(int64_t low, int64_t high, void *context);

#if defined(__EMSCRIPTEN__)
static void
pool_close(ZxNativePool *pool)
{
    (void)pool;
}

void
ZxNativeParallel(ParallelWorker worker, int64_t start, int64_t end, void *context)
{
    if(end >= start)
        worker(start, end, context);
}
#else
enum { POOL_MAX = 16, CHUNKS_PER_WORKER = 4 };
/* Worker stacks are aligned to their size; see the guard header. The top
 * page holds the worker's step budget, below it the thread's stack. */
#define WORKER_STACK ((size_t)8 << 20)

struct ZxNativePool {
    ZxNative *native;
    pthread_mutex_t lock;
    pthread_cond_t work;
    pthread_cond_t done;
    pthread_t threads[POOL_MAX];
    void *stacks[POOL_MAX];
    int count;
    int closing;
    unsigned job;
    ParallelWorker worker;
    void *context;
    int64_t start;
    int64_t end;
    int64_t chunk;
    int64_t chunks;
    int64_t next;
    int busy;
    int failed;
    int64_t failed_chunk;
    char error[ZX_NATIVE_ERROR_SIZE];
};

static int
pool_size(void)
{
    const char *requested = getenv("ZIRAN_PAR_THREADS");
    long count = requested != NULL && requested[0] != '\0' ?
                 strtol(requested, NULL, 10) : sysconf(_SC_NPROCESSORS_ONLN);
    if(count < 1)
        count = 1;
    return count > POOL_MAX ? POOL_MAX : (int)count;
}

/* A worker stack aligned to its size, with its lowest page unmapped so a
 * frame too large for the margin still stops at it. */
static size_t
page_size(void)
{
    return (size_t)sysconf(_SC_PAGESIZE);
}

static long long *
worker_budget(void *stack)
{
    return (long long *)((unsigned char *)stack + WORKER_STACK - page_size());
}

static void *
worker_stack(void)
{
    size_t size = WORKER_STACK;
    unsigned char *mapped = mmap(NULL, size * 2, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(mapped == MAP_FAILED)
        return NULL;
    uintptr_t base = ((uintptr_t)mapped + size - 1) & ~(uintptr_t)(size - 1);
    if(base > (uintptr_t)mapped)
        munmap(mapped, base - (uintptr_t)mapped);
    uintptr_t end = (uintptr_t)mapped + size * 2;
    if(end > base + size)
        munmap((void *)(base + size), end - (base + size));
    mprotect((void *)base, page_size(), PROT_NONE);
    return (void *)base;
}

/* Run chunks until the job has none left. Called with the lock held. */
static void
pool_take(ZxNativePool *pool)
{
    while(!pool->failed && pool->next < pool->chunks) {
        int64_t index = pool->next++;
        int64_t low = pool->start + index * pool->chunk;
        int64_t high = low + pool->chunk - 1;
        if(high > pool->end || high < low)
            high = pool->end;
        pthread_mutex_unlock(&pool->lock);
        jmp_buf escape;
        worker_escape = &escape;
        worker_error[0] = '\0';
        int failed = setjmp(escape) != 0;
        if(!failed)
            pool->worker(low, high, pool->context);
        worker_escape = NULL;
        pthread_mutex_lock(&pool->lock);
        if(failed && (!pool->failed || index < pool->failed_chunk)) {
            pool->failed = 1;
            pool->failed_chunk = index;
            memcpy(pool->error, worker_error, sizeof(pool->error));
        }
    }
}

static void *
pool_main(void *opaque)
{
    ZxNativePool *pool = opaque;
    current = pool->native;
    /* Jobs count from 1, so a worker that starts late still runs the first. */
    unsigned seen = 0;
    pthread_mutex_lock(&pool->lock);
    for(;;) {
        while(!pool->closing && pool->job == seen)
            pthread_cond_wait(&pool->work, &pool->lock);
        if(pool->closing)
            break;
        seen = pool->job;
        pool_take(pool);
        if(--pool->busy == 0)
            pthread_cond_signal(&pool->done);
    }
    pthread_mutex_unlock(&pool->lock);
    return NULL;
}

static void
pool_close(ZxNativePool *pool)
{
    if(pool == NULL)
        return;
    pthread_mutex_lock(&pool->lock);
    pool->closing = 1;
    pthread_cond_broadcast(&pool->work);
    pthread_mutex_unlock(&pool->lock);
    for(int i = 0; i < pool->count; i++)
        pthread_join(pool->threads[i], NULL);
    for(int i = 0; i < POOL_MAX; i++)
        if(pool->stacks[i] != NULL)
            munmap(pool->stacks[i], WORKER_STACK);
    pthread_cond_destroy(&pool->work);
    pthread_cond_destroy(&pool->done);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}

static ZxNativePool *
pool_open(ZxNative *native)
{
    ZxNativePool *pool = calloc(1, sizeof(*pool));
    if(pool == NULL)
        return NULL;
    pool->native = native;
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->work, NULL);
    pthread_cond_init(&pool->done, NULL);
    int wanted = pool_size();
    for(int i = 0; i < wanted; i++) {
        pthread_attr_t attributes;
        pool->stacks[i] = worker_stack();
        if(pool->stacks[i] == NULL || pthread_attr_init(&attributes) != 0)
            break;
        int started = pthread_attr_setstack(&attributes, pool->stacks[i],
                                            WORKER_STACK - page_size()) == 0 &&
                      pthread_create(&pool->threads[pool->count], &attributes, pool_main, pool) == 0;
        pthread_attr_destroy(&attributes);
        if(!started)
            break;
        pool->count++;
    }
    if(pool->count == 0) {
        pool_close(pool);
        return NULL;
    }
    if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_TIMING"))
        fprintf(stderr, "ziran: native %s: %d parallel workers\n",
                native->entry_name, pool->count);
    return pool;
}

void
ZxNativeParallel(ParallelWorker worker, int64_t start, int64_t end, void *context)
{
    ZxNative *native = current;
    if(end < start)
        return;
    /* A region reached from inside another region's worker runs in place. */
    if(worker_escape != NULL) {
        worker(start, end, context);
        return;
    }
    if(native->pool == NULL && pool_size() > 1 && end > start)
        native->pool = pool_open(native);
    ZxNativePool *pool = native->pool;
    if(pool == NULL || end == start) {
        worker(start, end, context);
        return;
    }
    uint64_t span = (uint64_t)end - (uint64_t)start + 1;
    uint64_t chunks = (uint64_t)pool->count * CHUNKS_PER_WORKER;
    if(chunks > span)
        chunks = span;
    char *floor = *native->floor;
    pthread_mutex_lock(&pool->lock);
    pool->worker = worker;
    pool->context = context;
    pool->start = start;
    pool->end = end;
    pool->chunk = (int64_t)((span + chunks - 1) / chunks);
    pool->chunks = (int64_t)((span + (uint64_t)pool->chunk - 1) / (uint64_t)pool->chunk);
    pool->next = 0;
    pool->failed = 0;
    pool->busy = pool->count;
    long long budget = *native->budget;
    for(int i = 0; i < pool->count; i++)
        *worker_budget(pool->stacks[i]) = budget;
    *native->floor = NULL;
    *native->stack_mask = (unsigned long)(WORKER_STACK - 1);
    *native->stack_margin = STACK_MARGIN;
    *native->budget_slot = (unsigned long)(WORKER_STACK - page_size());
    pool->job++;
    pthread_cond_broadcast(&pool->work);
    while(pool->busy > 0)
        pthread_cond_wait(&pool->done, &pool->lock);
    *native->floor = floor;
    *native->stack_mask = 0;
    *native->stack_margin = 0;
    for(int i = 0; i < pool->count; i++)
        *native->budget -= budget - *worker_budget(pool->stacks[i]);
    int failed = pool->failed;
    pthread_mutex_unlock(&pool->lock);
    if(failed)
        native_fail("%s", pool->error);
}
#endif

/* ---- Instances ---- */

/* The lowest address calls may reach on this thread, or NULL when the
 * thread's stack is unknown. */
static char *
stack_floor(void)
{
#if defined(__EMSCRIPTEN__)
    uintptr_t low = emscripten_stack_get_end();
    return emscripten_stack_get_current() > low + 2 * STACK_MARGIN ?
           (char *)low + STACK_MARGIN : NULL;
#elif defined(__linux__)
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

ZxNative *
ZxNativeNew(const ZirFunction *entry, const char *const *capability_modules,
            const char *const *capability_functions, size_t capability_count,
            VmHostCall host, void *context)
{
    ZxNative *native = calloc(1, sizeof(*native));
    if(native == NULL)
        return NULL;
    native->host = host;
    native->context = context;
    native->capability_modules = capability_modules;
    native->capability_functions = capability_functions;
    native->capability_count = capability_count;
    snprintf(native->entry_name, sizeof(native->entry_name), "%s", entry->name);
    native->entry_span = entry->span;
    native->has_result = entry->return_type[0] != '\0' &&
                         strcmp(entry->return_type, "void") != 0;
    if(ZxNativeStdout == NULL) {
        ZxNativeStdout = stdout;
        ZxNativeStderr = stderr;
    }
    return native;
}

int
ZxNativeStart(ZxNative *native)
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
        Diagnostic(native->entry_span, "zx.runtime",
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
free_blocks(ZxNativeBlock *block)
{
    while(block != NULL) {
        ZxNativeBlock *next = block->next;
        free(block);
        block = next;
    }
}

void
ZxNativeClose(ZxNative *native)
{
    if(native == NULL)
        return;
    if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_TIMING")) {
        size_t kept = 0, text = 0;
        for(ZxNativeBlock *block = native->kept; block != NULL; block = block->next)
            kept += block->used;
        for(size_t i = 0; i < native->string_buckets; i++)
            for(ZxNativeKeptText *item = native->strings[i]; item != NULL; item = item->next)
                text += item->length;
        fprintf(stderr, "ziran: native %s closed: %zu host strings (%zu bytes), %zu bytes of host slices\n",
                native->entry_name, native->string_count, text, kept);
    }
    pool_close(native->pool);
    if(native->code != NULL && native->release != NULL)
        native->release(native->code);
    free_blocks(native->scratch);
    free_blocks(native->kept);
    for(size_t i = 0; i < native->string_buckets; i++)
        for(ZxNativeKeptText *item = native->strings[i], *next; item != NULL; item = next) {
            next = item->next;
            free(item);
        }
    free(native->strings);
    free(native);
}

/* ---- Code linked into the host ----
 *
 * `zi2zx native-c` writes a bundle's program as C for a host compiled
 * ahead of time, such as the browser's, with a constructor that registers
 * it here under the bundle's identity. Bundles whose identity matches run
 * as that code; others run on the portable VM. The program's globals sit
 * in one data section; each instance starts from a copy of its first
 * contents, as each TinyCC instance starts from fresh storage, so only one
 * instance at a time can run it. */

typedef struct LinkedProgram {
    unsigned char identity[32];
    void (*start)(void);
    int64_t (*run)(void);
    long long *budget;
    char **floor;
    unsigned long *stack_mask;
    unsigned long *stack_margin;
    unsigned long *budget_slot;
    char *data;
    size_t data_size;
    /* The globals as linked, kept when the first instance opens. */
    unsigned char *initial;
    int live;
} LinkedProgram;

enum { LINKED_MAX = 64 };
static LinkedProgram linked_programs[LINKED_MAX];
static int linked_count;

void
ZxNativeRegister(const unsigned char identity[32], void (*start)(void), int64_t (*run)(void),
                 long long *budget, char **floor, unsigned long *stack_mask,
                 unsigned long *stack_margin, unsigned long *budget_slot,
                 char *data, char *data_end)
{
    if(linked_count >= LINKED_MAX)
        return;
    LinkedProgram *program = &linked_programs[linked_count++];
    memcpy(program->identity, identity, sizeof(program->identity));
    program->start = start;
    program->run = run;
    program->budget = budget;
    program->floor = floor;
    program->stack_mask = stack_mask;
    program->stack_margin = stack_margin;
    program->budget_slot = budget_slot;
    program->data = data;
    program->data_size = data != NULL && data_end > data ? (size_t)(data_end - data) : 0;
}

static LinkedProgram *
linked_program(const unsigned char identity[32])
{
    for(int i = 0; identity != NULL && i < linked_count; i++)
        if(memcmp(linked_programs[i].identity, identity, sizeof(linked_programs[i].identity)) == 0)
            return &linked_programs[i];
    return NULL;
}

int
ZxNativeLinked(const unsigned char identity[32])
{
    return linked_program(identity) != NULL;
}

static void
release_linked(void *code)
{
    ((LinkedProgram *)code)->live = 0;
}

ZxNative *
ZxNativeOpenLinked(const ZirFunction *entry, const unsigned char identity[32],
                   const char *const *capability_modules,
                   const char *const *capability_functions, size_t capability_count,
                   VmHostCall host, void *context, int *busy)
{
    *busy = 0;
    LinkedProgram *program = linked_program(identity);
    if(program == NULL || entry == NULL || host == NULL)
        return NULL;
    if(program->live) {
        *busy = 1;
        return NULL;
    }
    if(program->data_size > 0 && program->initial == NULL) {
        program->initial = malloc(program->data_size);
        if(program->initial == NULL)
            return NULL;
        memcpy(program->initial, program->data, program->data_size);
    } else if(program->data_size > 0) {
        memcpy(program->data, program->initial, program->data_size);
    }
    ZxNative *native = ZxNativeNew(entry, capability_modules, capability_functions,
                                   capability_count, host, context);
    if(native == NULL)
        return NULL;
    program->live = 1;
    native->code = program;
    native->release = release_linked;
    native->start = program->start;
    native->run = program->run;
    native->budget = program->budget;
    native->floor = program->floor;
    native->stack_mask = program->stack_mask;
    native->stack_margin = program->stack_margin;
    native->budget_slot = program->budget_slot;
    if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_TIMING"))
        fprintf(stderr, "ziran: native %s: linked code\n", entry->name);
    if(!ZxNativeStart(native)) {
        Diagnostic(entry->span, "zx.native", "cannot run natively: %s",
                   native->error[0] ? native->error : "initialization failed");
        ZxNativeClose(native);
        return NULL;
    }
    return native;
}
