/* Native library and thread-local details used by compiler modules. */
#include "zir.h"
#include "compiler_portable_body.h"
#include "zir_memory.h"
#include "zir_stream.h"

#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* Native stdio details used by the saved-IR inspector. */
int ZiranInspectRewind(FILE *file) { return fseek(file, 0, SEEK_SET); }
const char *ZiranInspectIOError(void) { return strerror(errno); }
int ZiranInspectOutputError(void) { return ferror(stdout); }

/* Measure an open input and position it at the start for reading. */
int64_t
CompilerFileSize(FILE *file)
{
    if(fseek(file, 0, SEEK_END) != 0)
        return -1;
    long bytes = ftell(file);
    if(bytes < 0 || fseek(file, 0, SEEK_SET) != 0)
        return -1;
    return bytes;
}

/* Each thread selects its translation target independently of the host. */
static _Thread_local ZirTargetFacts target_facts;
static _Thread_local char target_options[128];

ZirTargetFacts *CompilerTargetFactsAddress(void) { return &target_facts; }
char *CompilerTargetOptionsAddress(void) { return target_options; }

ZirTargetOS
CompilerHostOS(void)
{
#if defined(__EMSCRIPTEN__) || defined(__wasi__)
    return ZIR_OS_WEB;
#elif defined(__ANDROID__)
    return ZIR_OS_ANDROID;
#elif defined(_WIN32)
    return ZIR_OS_WINDOWS;
#elif defined(__APPLE__)
    return ZIR_OS_MACOS;
#elif defined(__linux__)
    return ZIR_OS_LINUX;
#else
    return ZIR_OS_NONE;
#endif
}

ZirTargetCPU
CompilerHostCPU(void)
{
#if defined(__wasm32__)
    return ZIR_CPU_WASM32;
#elif defined(__x86_64__) || defined(_M_X64)
    return ZIR_CPU_X64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return ZIR_CPU_ARM64;
#else
    return ZIR_CPU_NONE;
#endif
}

BodyMemo *CompilerBodyMemoAddress(void)
{
    static _Thread_local BodyMemo memo;
    return &memo;
}

CompilerScratchCache *CompilerBodyScratchAddress(void)
{
    static _Thread_local CompilerScratchCache cache;
    return &cache;
}

const char *S15DecimalPoint(void) { return localeconv()->decimal_point; }

const char *KeepNameFormat(const char *format, ...)
{
    char name[ZIR_NAME_MAX];
    va_list args;
    va_start(args, format);
    if(vsnprintf(name, sizeof(name), format, args) < 0) name[0] = '\0';
    va_end(args);
    return KeepName(name);
}

void **
ZirConstantTablesSlot(void)
{
    static _Thread_local void *slot;
    return &slot;
}

FILE *
ZirTextStreamOpen(TextStream *stream, char **text, size_t *size)
{
    return TextStreamOpen(stream, text, size);
}

int
ZirTextStreamClose(TextStream *stream)
{
    return TextStreamClose(stream);
}

FILE *
ZirEmitScratchOpen(unsigned char **text, size_t *size)
{
#if defined(ZIR_MEMORY_STREAMS)
    return ZirWriteMemory(text, size);
#else
    *text = NULL;
    *size = 0;
    return tmpfile();
#endif
}

void
ZirEmitScratchClose(FILE *file, size_t *size)
{
#if !defined(ZIR_MEMORY_STREAMS)
    long written = ftell(file);
    if(written < 0) {
        fclose(file);
        *size = 1;
        return;
    }
    *size = (size_t)written;
#endif
    if(fclose(file) != 0)
        *size = 1;
}

int
ZirSignbit(double value)
{
    return signbit(value) != 0;
}

int
ZirParseInt64(const char *text, int64_t *out)
{
    char *end = NULL;
    errno = 0;
    long long value = strtoll(text, &end, 0);
    if(errno != 0 || end == text || *end != '\0')
        return 0;
    *out = (int64_t)value;
    return 1;
}

/* Publish the immutable SIMD metadata once, even with parallel checkers. */
void
ZirSimdInitialize(void (*initialize)(void))
{
    enum { UNINITIALIZED, INITIALIZING, READY };
    static atomic_int state = ATOMIC_VAR_INIT(UNINITIALIZED);
    if(atomic_load_explicit(&state, memory_order_acquire) == READY)
        return;
    int expected = UNINITIALIZED;
    if(atomic_compare_exchange_strong_explicit(&state, &expected, INITIALIZING,
                                               memory_order_acq_rel, memory_order_acquire)) {
        initialize();
        atomic_store_explicit(&state, READY, memory_order_release);
    } else {
        while(atomic_load_explicit(&state, memory_order_acquire) != READY) {}
    }
}

typedef struct GoSimdText {
    const char *name, *text;
} GoSimdText;
#include "zir_simd_go_bodies.h"

size_t ZirGoSimdBodyCount(void) { return sizeof(go_simd_bodies) / sizeof(go_simd_bodies[0]); }
const GoSimdText *ZirGoSimdBodyAt(size_t i) { return &go_simd_bodies[i]; }
size_t ZirGoSimdLaneCount(void) { return sizeof(go_simd_lane_functions) / sizeof(go_simd_lane_functions[0]); }
const GoSimdText *ZirGoSimdLaneAt(size_t i) { return &go_simd_lane_functions[i]; }
