/*
 * ziran_bootstrap_runtime.h - the whole C surface of the bootstrap runtime.
 *
 * Generated compiler modules under bootstrap/compiler_* hold every parser,
 * checker, lowering, emission, cache, package and diagnostic rule. This
 * directory holds only what generated C cannot express: the process
 * boundary around it. A non-static function defined in a bootstrap/runtime
 * .c file must be declared here, and scripts/bootstrap_fixed_point.py
 * validate_runtime_boundary enforces that.
 *
 * The permitted kinds of function are:
 *
 *   - process startup and the call into the generated zi2c root;
 *   - checked allocation, reallocation and release;
 *   - panic and recovery transfer (setjmp/longjmp bridges);
 *   - file, environment, process, time, thread and dynamic-library calls.
 *
 * Nothing here may make a compiler decision. When a callback starts to,
 * the decision moves into a generated Ziran module and the callback keeps
 * only the OS call.
 *
 * Types come from the generated module headers, which own every record the
 * boundary shares with Ziran. The hand-written zir_*.h compatibility views
 * declare the same functions for C-only consumers and are deliberately not
 * included here: a translation unit reads one view or the other.
 */
#ifndef ZIRAN_BOOTSTRAP_RUNTIME_H
#define ZIRAN_BOOTSTRAP_RUNTIME_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Shared records, each from the module that owns it. Only the four core
 * frontend type headers are pulled in here so standalone runtime tests and
 * parallel builds do not require persistence or backend headers; records
 * named only by pointer stay forward-declared and the files that
 * dereference them include their owner directly. */
#include "compiler_ir.h"             /* ZirProgram, ProgramSet, ZirSourceSpan, target facts */
#include "compiler_check_types.h"    /* DiagnosticDetails */
#include "compiler_context_types.h"  /* CompilerThread, S15OverlayState, scratch, Sha256 */
#include "compiler_parse_types.h"    /* ZirCompileImportResolver */

typedef struct BodyMemo BodyMemo;
typedef struct ZirPackageMap ZirPackageMap;
typedef struct ZxAssets ZxAssets;

/* Generated calls the runtime makes, with the C text spellings the callers
 * use. compiler_diagnostic.h, compiler_frontend_ir.h and compiler_profile.h
 * are not included: each carries its own view of a record compiler_ir.h
 * already defines, so a file that needs them names the calls here instead. */
void DiagnosticOutOfMemory(void);
void ProgramFree(ZirProgram *program);
void SymbolLookupsChanged(void);
uint64_t ProfileStart(void);
void ProfileEnd(const char *phase, uint64_t started);
void ProfileAllocation(size_t bytes);
void ProfileCount(const char *counter);
uint8_t *KeepName(uint8_t *text);
double ParseDecimal(uint8_t *text, uint8_t **end);
ZirSourceSpan Span(uint8_t *path, int32_t line, int32_t column);
void FormatPrintFloat(double value, int32_t single, uint8_t *out, size_t capacity);
int32_t FileStatusNoLinks(uint8_t *path, void *info);
void Sha256Begin(Sha256 *hash);
void Sha256Add(Sha256 *hash, void *data, size_t size);
void Sha256End(Sha256 *hash, uint8_t *digest);
int ZxWasmHostPrint(void *wasm, uint32_t service, int64_t argument);
int ZxWasmHostCall(void *wasm, uint32_t index, uint32_t record);

/* C-only views with no generated counterpart. The recoverable allocator's
 * record stays opaque here; the view that also remaps malloc and free to it
 * is zir_memory.h, included by the files that want that. */
typedef struct CompilerMemory CompilerMemory;
typedef struct TextStream TextStream;
#include "zir_native_memory.h"       /* ZirNativeMapping */
#include "zir_host_exec.h"           /* ExecBundle */
#include "zir_host_native.h"         /* NativeBundleSlot and instances */
#include "zir_host_native_arch.h"    /* NativeStub, NativeVeneerCalls */
#include "zir_zx_wasm_platform.h"    /* ZxWasmPlatform */

#ifdef __cplusplus
extern "C" {
#endif

/* The host's own OS and CPU, as the ZIR_OS_* and ZIR_CPU_* values
 * compiler_target_bridge.zi names. The hand-written zir.h view spells the
 * same ints as ZirTargetOS and ZirTargetCPU; this contract keeps them plain
 * int so the runtime needs no record the generator does not own. */
enum { ZIR_OS_NONE, ZIR_OS_LINUX, ZIR_OS_WINDOWS, ZIR_OS_MACOS,
       ZIR_OS_ANDROID, ZIR_OS_WEB };
enum { ZIR_CPU_NONE, ZIR_CPU_X64, ZIR_CPU_ARM64, ZIR_CPU_WASM32 };

/* ---- allocation and recovery transfer (zir_memory.c) -------------------- */

/* Recoverable-operation tracking around the generated allocator contract. */
CompilerThread *CompilerThreadStateAddress(void);
S15OverlayState *S15OverlayStateAddress(void);
CompilerMemory *CompilerMemoryBegin(void);
void CompilerMemoryEnd(CompilerMemory *memory, int success);
CompilerMemory *CompilerMemoryPause(void);
void CompilerMemoryResume(CompilerMemory *memory);
int CompilerMemoryRecovering(void);
void *CompilerOwn(void *pointer);
void CompilerMemoryCleanup(void *pointer, void (*destroy)(void *));
void *CompilerMalloc(size_t size);
void *CompilerCalloc(size_t count, size_t size);
void *CompilerRealloc(void *pointer, size_t size);
char *CompilerStrdup(const char *text);
void CompilerFree(void *pointer);
FILE *CompilerFopen(const char *path, const char *mode);
int CompilerFclose(FILE *file);

/* ---- process, environment, time and stream leaves (zir_services.c) ------ */

int ZiranInspectRewind(FILE *file);
const char *ZiranInspectIOError(void);
int ZiranInspectOutputError(void);
int64_t CompilerFileSize(FILE *file);
ZirTargetFacts *CompilerTargetFactsAddress(void);
char *CompilerTargetOptionsAddress(void);
/* The host's own OS and CPU, as the ZIR_OS_* and ZIR_CPU_* values
 * compiler_target_bridge.zi defines; the translation target is chosen
 * elsewhere. The C-only zir.h view names them ZirTargetOS and ZirTargetCPU,
 * the same underlying int. */
int CompilerHostOS(void);
int CompilerHostCPU(void);
BodyMemo *CompilerBodyMemoAddress(void);
CompilerScratchCache *CompilerBodyScratchAddress(void);
const char *S15DecimalPoint(void);
const char *KeepNameFormat(const char *format, ...) __attribute__((format(printf, 1, 2)));
void **ZirConstantTablesSlot(void);
FILE *ZirTextStreamOpen(TextStream *stream, char **text, size_t *size);
int ZirTextStreamClose(TextStream *stream);
FILE *ZirEmitScratchOpen(unsigned char **text, size_t *size);
void ZirEmitScratchClose(FILE *file, size_t *size);
int ZirSignbit(double value);
int ZirParseInt64(const char *text, int64_t *out);
void ZirSimdInitialize(void (*initialize)(void));

/* The Go SIMD typed-body and lane tables are generated data
 * ($(SIMD_DIR)/zir_simd_go_bodies.h, from scripts/simd/header.zi); these read
 * it. compiler_go_simd.zi owns the choice of body and every spelling. */
typedef struct GoSimdText {
    const char *name, *text;
} GoSimdText;
size_t ZirGoSimdBodyCount(void);
const GoSimdText *ZirGoSimdBodyAt(size_t i);
size_t ZirGoSimdLaneCount(void);
const GoSimdText *ZirGoSimdLaneAt(size_t i);

/* ---- atomics and the per-thread engine slot (zir_atomics.c) ------------- */

int S15FlagTake(void *flag);
void S15FlagClear(void *flag);
int S15IntExchange(void *slot, int value);
void S15IntStore(void *slot, int value);
int S15IntLoad(void *slot);
int S15IntAdd(void *slot, int value);
int S15IntCompare(void *slot, int *expected, int value);
size_t S15SizeAdd(void *slot, size_t value);
size_t S15SizeLoad(void *slot);
void *S15ThreadEngine(void);
void S15SetThreadEngine(void *engine);

/* ---- diagnostic sink (zir_diagnostic_host.c) ---------------------------- */

/* The formatting and policy live in compiler_diagnostic.zi; these reach
 * stdio, the Android log and the C varargs ABI, then hand the finished
 * message to the generated S15DiagnosticMessage. */
void *CompilerStandardError(void);
void *CompilerStandardOutput(void);
void S15PlatformDiagnosticLog(int warning, const char *path, int line, int column,
                              const char *message, const char *code);
void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args);
void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
void DiagnosticDetailed(ZirSourceSpan span, const char *code,
                        const DiagnosticDetails *details, const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target,
                      const char *capability, const char *format, ...)
    __attribute__((format(printf, 5, 6)));
void Warning(ZirSourceSpan span, const char *code, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/* ---- process calls behind compiler_cli (zir_command_host.c) ------------- */

int ZiranCommandPriorityAvailable(void);
int ZiranCommandPriorityGet(int *error);
void ZiranCommandPrioritySet(int value);
void ZiranCommandPriorityIo(int value);
int ZiranCommandPipe(int *descriptors, int *error);
int ZiranCommandFork(int *error);
long ZiranCommandReadRetry(int descriptor, void *output, size_t count, int *error);
int ZiranCommandWriteAll(int descriptor, const void *bytes, size_t count);
long ZiranCommandWaitRetry(int process, int *status, int *error);
const char *ZiranCommandErrorText(int error);
int ZiranCommandLastError(void);
void ZiranCommandClearError(void);

/* ---- editor open flags and stat fields (zir_edit_host.c) ---------------- */

int ZiranEditHostOpenSource(const char *path);
int ZiranEditHostOpenDirectory(const char *path);
int ZiranEditHostOpenSubdirectory(int parent, const char *name);
int ZiranEditHostOpenLeaf(int parent, const char *name);
int ZiranEditHostCreatePrivate(int parent, const char *name);
int ZiranEditHostFdInfo(int descriptor, int64_t *size, uint32_t *mode, int *regular,
                        uint64_t *device, uint64_t *inode);
int ZiranEditHostAtInfo(int directory, const char *name, uint64_t *device,
                        uint64_t *inode, uint32_t *mode, int *regular,
                        uint64_t *links, int64_t *size);
int ZiranEditHostPathIsDirectory(const char *path);
#if defined(_WIN32)
/* A Windows host has no POSIX atomic replace. compiler_edit.zi still names
 * these libc calls, so the boundary keeps failing definitions for them and
 * the editor reports the path as unavailable. */
int fchmod(int descriptor, int mode);
int fsync(int descriptor);
long long pread(int descriptor, void *output, size_t count, long long offset);
char *realpath(const char *path, char *resolved);
int renameat(int from_directory, const char *from, int to_directory, const char *to);
int unlinkat(int directory, const char *name, int flags);
#endif

/* ---- file, directory and path calls (zir_files_host.c) ------------------ */

int S15HostOS(void);
size_t S15PathMax(void);
ptrdiff_t S15ReadLink(const char *path, char *output, size_t size);
char *S15RealPath(const char *path, char *output);
int S15FileStat(const char *path, void *info, int follow);
int S15StatIsLink(const void *info);
int S15DarwinExecutablePath(char *output, uint32_t *size);
void *S15WindowsPathOpen(const char *path);
uint32_t S15WindowsFinalPath(void *file, char *output, uint32_t capacity);
void S15WindowsPathClose(void *file);
uint32_t S15WindowsExecutablePath(char *output, uint32_t capacity);
uint32_t S15WindowsAttributes(const char *path);
char *S15WindowsFullPath(const char *path);
void *CompilerDirectoryOpen(const char *path);
const char *CompilerDirectoryNext(void *directory);
int CompilerDirectoryCloseStatus(void *directory);
void CompilerDirectoryClose(void *directory);
int CompilerPathEntryKind(const char *path);
int CompilerPathEntryKindNoLinks(const char *path);
int CompilerPathIsRegular(const char *path);
int ZiranApiReadExampleFile(const char *path, int optional, unsigned char *text,
                            size_t capacity, size_t *bytes);
int64_t CompilerFileTell(FILE *file);
int CompilerFileSeek(FILE *file, int64_t offset, int origin);
int S15FileIdentity(const char *path, uint64_t *device, uint64_t *inode, int *regular);

/* ---- SHA-256 hardware rounds (zir_sha256_host.c) ------------------------ */

/* The portable rounds are compiler_sha256; these are the CPU instructions
 * it calls when the host has them. */
int S15ShaHardwareAvailable(void);
void S15ShaHardwareCompress(uint32_t *state, const unsigned char *block, size_t blocks);

/* ---- W^X mappings (zir_native_memory.c) --------------------------------- */

size_t ZirNativePageSize(void);
int ZirNativeReserve(size_t requested, ZirNativeMapping *mapping);
int ZirNativeProtectRX(const ZirNativeMapping *mapping);
int ZirNativeProtectR(const ZirNativeMapping *mapping);
int ZirNativeFlushInstructions(const ZirNativeMapping *mapping);
void ZirNativeRelease(ZirNativeMapping *mapping);

/* ---- saved-IR and package leaves (zir_persistence_host.c) --------------- */

const char *CompilerBuiltinDirectory(int kind);
int CompilerMemoryStreams(void);
FILE *CompilerOpenMemoryOutput(unsigned char **data, size_t *size);
int ProgramWrite(const ZirProgram *program, FILE *out);
ZirProgram *ProgramRead(FILE *in, const char *path);
int PathIsIR(const char *path);
ZirProgram *ProgramLoad(const char *path, const char *root);
int CheckCanonicalPrograms(ZirProgram **programs, int count,
                           const char *const *input_paths);
const char *ToolchainStandardDirectory(void);
const char *ToolchainIncludeDirectory(void);
int ProgramsLoad(ProgramSet *set, const char *root, const char *const *module_paths,
                 int module_path_count, const char *const *inputs, int input_count);
int ProgramsLoadWithDefines(ProgramSet *set, const char *root,
                            const char *const *module_paths, int module_path_count,
                            const char *const *defines, int define_count,
                            const char *const *inputs, int input_count);
ZirPackageMap *PackageMapLoad(const char *path);
const char *PackageOwner(const ZirPackageMap *map, const char *path);
const char *PackageModuleName(const ZirPackageMap *map, const char *path);
const char *PackageAmbiguity(const ZirPackageMap *map, const char *owner, const char *visible);
int PackageDependencyModule(const ZirPackageMap *map, const char *owner,
                            const char *qualified, char *identity, size_t identity_size);
int PackageResolve(const ZirPackageMap *map, const char *owner, const char *visible,
                   char *path, size_t path_size, char *identity, size_t identity_size);
int PackageResolveSaved(const ZirPackageMap *map, const char *owner, const char *identity,
                        char *path, size_t path_size);
int ZxAssetsAdd(ZxAssets *assets, const char *name, const void *data, size_t size);
int ZxAssetsCollect(ZxAssets *assets, const char *spec);

/* ---- native code loading (zir_host_native*.c) --------------------------- */

BundleExecutionBackend BundleNativePlatformBackend(void);
BundleExecutionBackend BundleAutoPlatformBackend(void);
const NativeBundleCode *NativeBundleSlotCode(NativeBundleSlot *slot, const ExecBundle *bundle,
                                             const unsigned char digest[32], int unbounded);
const char *NativeBundleSlotProblem(NativeBundleSlot *slot, const ExecBundle *bundle,
                                    const unsigned char digest[32]);
void NativeBundleSlotClose(NativeBundleSlot *slot);
NativeInstance *NativeInstanceOpen(const NativeBundleCode *code, const ExecBundle *bundle,
                                   NativeFindBinding find, void *context, long long max_steps);
int NativeInstanceRun(NativeInstance *instance, long long *result, int *has_result);
void NativeInstanceLimitSteps(NativeInstance *instance, long long max_steps);
void NativeInstanceMatchFieldsByName(NativeInstance *instance, int enabled);
size_t NativeInstanceStateCount(const NativeInstance *instance);
int NativeInstanceStateAt(NativeInstance *instance, size_t index, const char **module,
                          const char **name, const char **type, struct HostValue *value);
int NativeInstanceStateStore(NativeInstance *instance, const struct HostValue *const *values);
size_t NativeInstanceLiveBytes(const NativeInstance *instance);
void NativeInstanceClose(NativeInstance *instance);
int ZirNativeHostVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t function);
int ZirNativeTrapVeneer(NativeStub *out, const NativeVeneerCalls *calls);
int ZirNativeResizeVeneer(NativeStub *out, const NativeVeneerCalls *calls);
#if defined(__arm__)
int ZirNativeHelperVeneer(NativeStub *out, const NativeVeneerCalls *calls, int32_t helper);
#endif
int ZirNativePatch(unsigned char *field, uintptr_t target, int32_t width, int64_t addend);
uint64_t ZirNativeCpuKey(void);

/* ---- the i386 block compiler (zir_exec_i386.c) -------------------------- */

/* The packed engine's hot-block compiler. Its plan, frame and instance
 * records come from the engine's generated headers, which the boundary
 * deliberately does not pull in, so the entry points name them loosely. */
void *ExecI386Open(const void *instance);
int ExecI386RunEntry(void *instance, void *ran, void *cache);
void ExecI386Close(void *cache);

/* ---- Wasm provider plumbing (zir_zx_wasm_install.c) --------------------- */

void ZxWasmInstallBundledPlatform(void);

/* ---- profiling leaves (zir_profile_host.c) ------------------------------ */

uint64_t ProfileNow(void);
int64_t ProfileMaxRssKib(void);
int64_t ProfilePid(void);
void ProfileRegister(void);
int64_t ProfileWrite(void *file, const unsigned char *record, size_t size);

/* ---- parser recovery transfer (zir_parse.c) ----------------------------- */

/* The parser, the recovery loop and every recovery decision live in
 * compiler_parse_source.zi. What remains here is the transfer: a nested
 * parse operation's setjmp/longjmp target, the source text and line a
 * failing operation reported, and the varargs entry points the generated
 * parser calls when it cannot continue. */
ZirProgram *ParserWithRecovery(const char *path, const char *root, const char *source,
                               ZirCompileImportResolver resolver, void *context,
                               const char *const *defines, int define_count,
                               const ZirImport *site);
void ParseRememberSource(const char *text);
int *ZirLawEvaluationAddress(void);
double ParseDecimalRange(const char *text, char **end, int *range_error);
void die(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void die_at(ZirSourceSpan span, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void parse_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* ZIRAN_BOOTSTRAP_RUNTIME_H */
