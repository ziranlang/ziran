/* Native execution of verified ZX bundles with TinyCC.
 *
 * The bundle's verified program is lowered to C (zir_zx_native_lower.c),
 * compiled in memory by TinyCC and run by zir_zx_native_run.c. Host
 * capabilities reach the instance's bindings through trampolines
 * (zir_zx_host.c) that pass the same VmHostValue shapes the portable VM does.
 *
 * Every failure is a hard error with a diagnostic. A program that does not
 * compile does not run. Compiled code sees only the C headers in
 * zx_native_include and links only against the symbols registered here. It
 * runs with the process's memory like any native build of the program:
 * unlike the portable VM, it does not check pointer lifetimes at run time. */
#include "zir_zx_native_internal.h"
#include "zir_diagnostic.h"
#include "zir_sha256.h"

#include "libtcc.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

static pthread_mutex_t compile_lock = PTHREAD_MUTEX_INITIALIZER;

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
        {"stdout", (const void *)&ZxNativeStdout},
        {"stderr", (const void *)&ZxNativeStderr},
        {"fprintf", (const void *)ZxNativePrint},
        {"abort", (const void *)ZxNativeAbort},
        {"exit", (const void *)ZxNativeExit},
        {"ZxNativeParallel", (const void *)ZxNativeParallel},
        {"getenv", (const void *)ZxNativeEnvironment},
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

/* C sources the lowering wrote, found again for TinyCC. */
static TCCState *source_target;
static const ZxNativeBuild *source_build;
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
    const ZxNativeBuild *build = source_build;
    const char *stem = path + source_root + 1;
    size_t stem_length = length - source_root - 3;
    for(int i = 0; i < build->rename_count; i += 3)
        if(strlen(build->renames[i]) == stem_length &&
           strncmp(build->renames[i], stem, stem_length) == 0)
            tcc_define_symbol(source_target, build->renames[i + 1], build->renames[i + 2]);
    if(tcc_add_file(source_target, path) < 0)
        source_failed = 1;
    for(int i = 0; i < build->rename_count; i += 3)
        if(strlen(build->renames[i]) == stem_length &&
           strncmp(build->renames[i], stem, stem_length) == 0)
            tcc_undefine_symbol(source_target, build->renames[i + 1]);
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
add_sources(TCCState *state, const ZxNativeBuild *build, const char *directory)
{
    char include[ZIR_PATH_MAX];
    snprintf(include, sizeof(include), "%s/zx_include", directory);
    tcc_add_sysinclude_path(state, include);
    tcc_add_include_path(state, directory);
    if(tcc_set_options(state, "-include zx_guard.h") < 0 ||
       (build->include_headers && tcc_set_options(state, "-include zx_headers.h") < 0))
        return 0;
    source_target = state;
    source_build = build;
    source_root = strlen(directory);
    source_failed = 0;
    nftw(directory, add_source, 16, FTW_PHYS);
    return !source_failed;
}

static void
release_compiler(void *code)
{
    tcc_delete(code);
}

/* Link compiled code (C sources or a cached object) into this instance. */
static int
link_program(ZxNative *native, const ZxNativeBuild *build, const char *directory,
             const char *object)
{
    TCCState *state = new_compiler(native, TCC_OUTPUT_MEMORY);
    if(state == NULL)
        return 0;
    native->code = state;
    native->release = release_compiler;
    if(!add_symbols(state))
        return 0;
    if(object != NULL ? tcc_add_file(state, object) < 0 : !add_sources(state, build, directory))
        return 0;
    if(tcc_relocate(state) < 0)
        return 0;
    native->start = (void (*)(void))tcc_get_symbol(state, "zx_native_start");
    native->run = (int64_t (*)(void))tcc_get_symbol(state, "zx_native_run");
    native->budget = (long long *)tcc_get_symbol(state, "zx_native_budget");
    native->floor = (char **)tcc_get_symbol(state, "zx_native_floor");
    native->stack_mask = (unsigned long *)tcc_get_symbol(state, "zx_native_stack_mask");
    native->stack_margin = (unsigned long *)tcc_get_symbol(state, "zx_native_stack_margin");
    native->budget_slot = (unsigned long *)tcc_get_symbol(state, "zx_native_budget_slot");
    if(native->start == NULL || native->run == NULL || native->budget == NULL ||
       native->floor == NULL || native->stack_mask == NULL || native->stack_margin == NULL ||
       native->budget_slot == NULL) {
        snprintf(native->error, sizeof(native->error), "compiled bundle has no entry points");
        return 0;
    }
    return 1;
}

static int
write_object(ZxNative *native, const ZxNativeBuild *build, const char *directory, const char *path)
{
    char temporary[ZIR_PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.%ld.tmp", path, (long)getpid());
    TCCState *state = new_compiler(native, TCC_OUTPUT_OBJ);
    int ok = state != NULL && add_sources(state, build, directory) &&
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

/* Lower the program and compile it, into the cache when there is one. */
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
    ZxNativeBuild build = {0};
    int ok = ZxNativeLower(program, entry_owner, entry, native->capability_modules,
                           native->capability_functions, native->capability_count,
                           ZX_NATIVE_TINYCC, NULL, directory, &build);
    if(!ok)
        snprintf(native->error, sizeof(native->error), "%s", build.error);
    double lowered = milliseconds();
    if(ok && object != NULL && write_object(native, &build, directory, object)) {
        ok = link_program(native, &build, NULL, object);
    } else if(ok) {
        /* Without a writable cache the program still compiles, in memory. */
        native->error[0] = '\0';
        ok = link_program(native, &build, directory, NULL);
    }
    /* ZIRAN_NATIVE_TIMING=1 reports where compilation time goes. */
    if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_TIMING"))
        fprintf(stderr, "ziran: native %s: lowering %.1f ms%s, compiling %.1f ms\n",
                entry->name, lowered - start, build.pruned ? " (pruned)" : "",
                milliseconds() - lowered);
    /* ZIRAN_NATIVE_KEEP=1 leaves the generated C for inspection. */
    if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_KEEP"))
        fprintf(stderr, "ziran: native build kept in %s\n", directory);
    else
        ZxNativeRemoveTree(directory);
    ZxNativeBuildFree(&build);
    return ok;
}

int
ZxNativeCached(const unsigned char identity[32])
{
    char folder[ZIR_PATH_MAX], object[ZIR_PATH_MAX], key[65];
    if(identity == NULL || !cache_directory(folder, sizeof(folder)))
        return 0;
    cache_key(identity, key);
    snprintf(object, sizeof(object), "%s/%s.o", folder, key);
    return access(object, R_OK) == 0;
}

/* Without a program only cached code runs: *missing reports, without a
 * diagnostic, that there was none to link. */
static ZxNative *
native_open(const ZirProgram *program, const ZirModule *entry_owner,
            const ZirFunction *entry, const unsigned char identity[32],
            const char *const *capability_modules,
            const char *const *capability_functions, size_t capability_count,
            VmHostCall host, void *context, int *missing)
{
    ZxNative *native = ZxNativeNew(entry, capability_modules, capability_functions,
                                   capability_count, host, context);
    if(native == NULL)
        return NULL;
    pthread_mutex_lock(&compile_lock);
    char folder[ZIR_PATH_MAX], object[ZIR_PATH_MAX], key[65];
    int cached = identity != NULL && cache_directory(folder, sizeof(folder));
    int ok = 0;
    if(cached) {
        cache_key(identity, key);
        snprintf(object, sizeof(object), "%s/%s.o", folder, key);
        if(access(object, R_OK) == 0) {
            double start = milliseconds();
            ok = link_program(native, NULL, NULL, object);
            if(ok) {
                utime(object, NULL);
                if(ZxNativeEnvironmentSet("ZIRAN_NATIVE_TIMING"))
                    fprintf(stderr, "ziran: native %s: cached, linking %.1f ms\n",
                            entry->name, milliseconds() - start);
            } else {
                /* An unusable object is rebuilt, never run. */
                remove(object);
                if(native->code != NULL)
                    tcc_delete(native->code);
                native->code = NULL;
                native->error[0] = '\0';
            }
        }
    }
    if(!ok && program == NULL) {
        pthread_mutex_unlock(&compile_lock);
        *missing = 1;
        ZxNativeClose(native);
        return NULL;
    }
    if(!ok) {
        ok = compile(native, program, entry_owner, entry, cached ? object : NULL);
        if(cached)
            cache_prune(folder);
    }
    pthread_mutex_unlock(&compile_lock);
    if(ok)
        ok = ZxNativeStart(native);
    if(!ok) {
        Diagnostic(entry->span, "zx.native", "cannot run natively: %s",
                   native->error[0] ? native->error : "compilation failed");
        ZxNativeClose(native);
        return NULL;
    }
    return native;
}

ZxNative *
ZxNativeOpen(const ZirProgram *program, const ZirModule *entry_owner,
             const ZirFunction *entry, const unsigned char identity[32],
             const char *const *capability_modules,
             const char *const *capability_functions, size_t capability_count,
             VmHostCall host, void *context)
{
    int missing = 0;
    if(program == NULL || entry_owner == NULL || entry == NULL || host == NULL)
        return NULL;
    return native_open(program, entry_owner, entry, identity, capability_modules,
                       capability_functions, capability_count, host, context, &missing);
}

ZxNative *
ZxNativeOpenCached(const ZirFunction *entry, const unsigned char identity[32],
                   const char *const *capability_modules,
                   const char *const *capability_functions, size_t capability_count,
                   VmHostCall host, void *context, int *missing)
{
    *missing = 0;
    if(entry == NULL || host == NULL || identity == NULL)
        return NULL;
    return native_open(NULL, NULL, entry, identity, capability_modules,
                       capability_functions, capability_count, host, context, missing);
}
