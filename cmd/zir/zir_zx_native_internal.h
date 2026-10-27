#ifndef ZIR_ZX_NATIVE_INTERNAL_H
#define ZIR_ZX_NATIVE_INTERNAL_H

/* Shared by the native engine's parts: zir_zx_native_run.c runs compiled
 * code and serves it, zir_zx_native_lower.c writes the C, and
 * zir_zx_native.c compiles that C with TinyCC. */
#include "zir_zx_native.h"

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

typedef struct ZxNativeBlock ZxNativeBlock;
typedef struct ZxNativeKeptText ZxNativeKeptText;
typedef struct ZxNativePool ZxNativePool;

enum { ZX_NATIVE_ERROR_SIZE = 2048 };

struct ZxNative {
    /* The code's owner, released at close: TinyCC's state, or the linked
     * program the instance holds. */
    void *code;
    void (*release)(void *code);
    VmHostCall host;
    void *context;
    const char *const *capability_modules;
    const char *const *capability_functions;
    size_t capability_count;
    /* Copied: a bundle checked after this instance opened may move its
     * functions. */
    char entry_name[ZIR_NAME_MAX];
    ZirSourceSpan entry_span;
    int has_result;
    void (*start)(void);
    int64_t (*run)(void);
    long long *budget;
    char **floor;
    unsigned long *stack_mask;
    unsigned long *stack_margin;
    unsigned long *budget_slot;
    ZxNativePool *pool;
    int match_fields_by_name;
    int max_steps;
    int failed;
    jmp_buf *escape;
    ZxNativeBlock *scratch;
    ZxNativeBlock *scratch_at;
    ZxNativeBlock *kept;
    ZxNativeKeptText **strings;
    size_t string_buckets;
    size_t string_count;
    char error[ZX_NATIVE_ERROR_SIZE];
};

/* Services compiled code calls by these names (ZxNativeServicesHeader and
 * the guard header declare them for it), and the streams it names stdout
 * and stderr. TinyCC links the C library names the engine redirects
 * (abort, exit, fprintf, getenv) to them too; linked code is renamed to
 * them by its guard header. */
extern FILE *ZxNativeStdout;
extern FILE *ZxNativeStderr;
void ZxNativeHost(int capability, VmHostValue *args, int count, VmHostValue *result);
void ZxNativeScratchReset(void);
void *ZxNativeScratch(size_t size);
void *ZxNativeKeep(size_t size);
void ZxNativeString(const VmHostValue *value, const char **data, size_t *length);
void ZxNativeExpect(const VmHostValue *value, int kind, const char *type);
void ZxNativeExpectLength(const VmHostValue *value, size_t length);
void ZxNativeExpectFields(const VmHostValue *value, size_t count);
void ZxNativeExpectSlice(const VmHostValue *value, size_t element_size);
void ZxNativeDistinct(const void *a, size_t a_bytes, const void *b, size_t b_bytes);
int ZxNativeMatchByName(void);
const VmHostValue *ZxNativeField(const VmHostValue *value, const char *name);
const VmHostValue *ZxNativeFieldAt(const VmHostValue *value, size_t index, const char *name);
int ZxNativePrint(FILE *stream, const char *format, ...);
void ZxNativeAbort(void);
void ZxNativeExit(int status);
char *ZxNativeEnvironment(const char *name);
void ZxNativeOverrun(void);
void ZxNativeStackOverrun(void);
void ZxNativeUnreachable(const char *place);
void ZxNativeParallel(void (*worker)(int64_t low, int64_t high, void *context),
                      int64_t start, int64_t end, void *context);

/* A new instance of `entry` with nothing compiled yet. */
ZxNative *ZxNativeNew(const ZirFunction *entry, const char *const *capability_modules,
                      const char *const *capability_functions, size_t capability_count,
                      VmHostCall host, void *context);
/* Run the module initializers; 0 after a failure, with its message. */
int ZxNativeStart(ZxNative *native);
int ZxNativeEnvironmentSet(const char *name);

/* How the lowered program is compiled. TinyCC compiles it in this process;
 * linked code is compiled ahead of time into the host executable. */
typedef enum ZxNativeTarget {
    ZX_NATIVE_TINYCC,
    ZX_NATIVE_LINKED
} ZxNativeTarget;

/* What compiling the C that ZxNativeLower wrote needs to know. */
typedef struct ZxNativeBuild {
    /* Sources first include zx_headers.h, all generated headers in order. */
    int include_headers;
    /* Sources whose capability calls are renamed: source stem, symbol,
     * replacement, as consecutive strings. */
    char **renames;
    int rename_count;
    /* Whether the entry's linked program was lowered rather than the
     * bundle's own. */
    int pruned;
    char error[ZX_NATIVE_ERROR_SIZE];
} ZxNativeBuild;

/* Lower a verified program to C in `directory` (which must exist and be
 * empty): one source per module, the host trampolines and entry points in
 * zx_native_host.c, and the headers compiled code sees in zx_include.
 * Linked code also gets zx_native_program.c, which registers it under
 * `identity`. */
int ZxNativeLower(const ZirProgram *program, const ZirModule *entry_owner,
                  const ZirFunction *entry, const char *const *capability_modules,
                  const char *const *capability_functions, size_t capability_count,
                  ZxNativeTarget target, const unsigned char identity[32],
                  const char *directory, ZxNativeBuild *build);
void ZxNativeBuildFree(ZxNativeBuild *build);
void ZxNativeRemoveTree(const char *path);

#endif
