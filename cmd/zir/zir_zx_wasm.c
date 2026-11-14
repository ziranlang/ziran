/*
 * zir_zx_wasm.c - revision-28 bundles executed as WebAssembly through the
 * installed provider. Mirrors the instruction engine's instance contract
 * so zir_host.c picks between engines at one seam. The module comes from
 * cmd/zir-wasm/zx_wasm_lower.c; its host calls and its globals cross as
 * the engine's host images, which the bridge copies between the
 * instance's memory and ExecHosts, the embedding API's translation to
 * VmHostValue trees.
 */
#include "zir_zx_wasm.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "../zir-wasm/zx_wasm_lower.h"
#include "zir_zx_wasm_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ZxWasm {
    /* The installed provider, its context this instance. */
    ZxWasmPlatform platform;
    unsigned long handle;
    long long budget;
    /* The step limit the budget started from, for diagnostics. */
    long long limit;
    /* Opened with a step limit: initialization and runs share it. */
    int shared_budget;
    int failed;
    const ExecBundle *exec;
    ExecHosts *hosts;
    /* The lowered module's capabilities and globals; its bytes are gone
     * once it is instantiated. */
    ZxWasmImage image;
    /* Each capability's bundle host index. */
    size_t *host_of;
    /* The image a state value of ZxWasmStateAt borrows. */
    unsigned char *state_image;
};

static ZxWasmPlatform active_platform;
static int platform_installed;

/* The Emscripten build's provider installs itself here; native builds
 * define the empty version below. The strong reference from this always
 * linked object pulls the provider into the web link. */
void ZxWasmInstallBundledPlatform(void);
#ifndef __EMSCRIPTEN__
void
ZxWasmInstallBundledPlatform(void)
{
}
#endif

__attribute__((constructor))
static void
zx_wasm_install_at_load(void)
{
    ZxWasmInstallBundledPlatform();
}

void
ZxWasmSetPlatform(const ZxWasmPlatform *platform)
{
    if(platform == NULL) {
        platform_installed = 0;
        memset(&active_platform, 0, sizeof(active_platform));
        return;
    }
    active_platform = *platform;
    platform_installed = 1;
}

const ZxWasmPlatform *
ZxWasmPlatformActive(void)
{
    return platform_installed ? &active_platform : NULL;
}

/* ---- Instance memory ---------------------------------------------------- */

static int
zx_wasm_read(void *context, size_t address, void *destination, size_t size)
{
    ZxWasm *wasm = context;
    return size <= SIZE_MAX - address &&
           wasm->platform.read(wasm->platform.context, wasm->handle, address, destination, size) == 0;
}

static int
zx_wasm_write(void *context, size_t address, const void *source, size_t size)
{
    ZxWasm *wasm = context;
    return size <= SIZE_MAX - address &&
           wasm->platform.write(wasm->platform.context, wasm->handle, address, source, size) == 0;
}

static size_t
zx_wasm_reserve(void *context, size_t size)
{
    ZxWasm *wasm = context;
    if(size > UINT32_MAX)
        return 0;
    return wasm->platform.allocate_state(wasm->platform.context, wasm->handle, size);
}

static ZxWasmMemory
zx_wasm_memory(ZxWasm *wasm)
{
    return (ZxWasmMemory){wasm, zx_wasm_read, zx_wasm_write, zx_wasm_reserve};
}

/* ---- Host calls --------------------------------------------------------- */

int
ZxWasmHostPrint(void *context, uint32_t service, int64_t argument)
{
    ZxWasm *wasm = context;
    if(wasm == NULL)
        return 0;
    if(service == ZX_WASM_PRINT_STRING) {
        size_t address = (uint32_t)argument;
        size_t length = (size_t)((uint64_t)argument >> 32);
        unsigned char bytes[4096];
        while(length != 0) {
            size_t count = length < sizeof(bytes) ? length : sizeof(bytes);
            if(!zx_wasm_read(wasm, address, bytes, count) ||
               fwrite(bytes, 1, count, stdout) != count) return 0;
            address += count;
            length -= count;
        }
        return 1;
    }
    char text[64];
    switch(service) {
    case ZX_WASM_PRINT_SIGNED: snprintf(text, sizeof(text), "%lld", (long long)argument); break;
    case ZX_WASM_PRINT_UNSIGNED: snprintf(text, sizeof(text), "%llu", (unsigned long long)argument); break;
    case ZX_WASM_PRINT_BOOL: snprintf(text, sizeof(text), "%s", argument ? "true" : "false"); break;
    case ZX_WASM_PRINT_FLOAT32: {
        uint32_t bits = (uint32_t)argument;
        float value;
        memcpy(&value, &bits, sizeof(value));
        FormatPrintFloat(value, 1, text, sizeof(text));
        break;
    }
    case ZX_WASM_PRINT_FLOAT64: {
        double value;
        memcpy(&value, &argument, sizeof(value));
        FormatPrintFloat(value, 0, text, sizeof(text));
        break;
    }
    default: return 0;
    }
    return fputs(text, stdout) >= 0;
}

static uint64_t
zx_wasm_u64(const unsigned char *bytes)
{
    uint64_t value = 0;
    for(int at = 0; at < 8; at++) value |= (uint64_t)bytes[at] << (8 * at);
    return value;
}

static uint32_t
zx_wasm_u32(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static void
zx_wasm_put(unsigned char *bytes, uint64_t value, int width)
{
    for(int at = 0; at < width; at++) bytes[at] = (unsigned char)(value >> (8 * at));
}

/* The elements a []T argument names, from its record entry and image. */
static int
zx_wasm_slice(const ZxWasmValue *value, const unsigned char *entry,
              const ExecHostArgument *argument, uint64_t *start, uint64_t *end)
{
    if(value->kind != ZX_WASM_IMAGE || value->element_size == 0 || argument->size < 16)
        return 0;
    uint64_t count = zx_wasm_u64(argument->bytes + 8);
    if(count == 0 || count > UINT32_MAX / value->element_size)
        return 0;
    *start = zx_wasm_u32(entry + 4);
    *end = *start + count * value->element_size;
    return 1;
}

int
ZxWasmHostCall(void *context, uint32_t index, uint32_t record)
{
    ZxWasm *wasm = context;
    if(wasm == NULL || wasm->hosts == NULL || index >= wasm->image.capability_count)
        return 0;
    const ZxWasmCapability *declared = &wasm->image.capabilities[index];
    size_t host = wasm->host_of[index];
    size_t count = declared->parameter_count;
    if(count > SIZE_MAX / 16 - 1)
        return 0;
    unsigned char *entries = malloc(16 * (count + 1));
    ExecHostArgument *arguments = calloc(count + 1, sizeof(*arguments));
    unsigned char **images = calloc(count + 1, sizeof(*images));
    int ok = 0;
    if(entries == NULL || arguments == NULL || images == NULL ||
       !zx_wasm_read(wasm, record, entries, 16 * (count + 1)))
        goto done;
    for(size_t at = 0; at < count; at++) {
        const unsigned char *entry = entries + 16 * at;
        arguments[at].type = declared->parameters[at].type;
        if(declared->parameters[at].kind == ZX_WASM_BITS) {
            arguments[at].bits = zx_wasm_u64(entry);
            continue;
        }
        uint32_t length = zx_wasm_u32(entry + 8);
        images[at] = malloc(length ? length : 1);
        if(images[at] == NULL || !zx_wasm_read(wasm, zx_wasm_u32(entry), images[at], length))
            goto done;
        arguments[at].bytes = images[at];
        arguments[at].size = length;
    }
    /* Two []T arguments of one call may not share elements. */
    for(size_t left = 0; left < count; left++) {
        uint64_t left_start, left_end;
        if(!ExecBundleHostMutable(wasm->exec, host, left) ||
           !zx_wasm_slice(&declared->parameters[left], entries + 16 * left, &arguments[left],
                          &left_start, &left_end))
            continue;
        for(size_t right = left + 1; right < count; right++) {
            uint64_t right_start, right_end;
            if(ExecBundleHostMutable(wasm->exec, host, right) &&
               zx_wasm_slice(&declared->parameters[right], entries + 16 * right, &arguments[right],
                             &right_start, &right_end) &&
               left_start < right_end && right_start < left_end) {
                Diagnostic((ZirSourceSpan){0}, "zx.host_alias",
                           "host capability %s:%s received two mutable slices that share elements",
                           declared->module, declared->function);
                goto done;
            }
        }
    }
    ExecHostArgument result = {.type = declared->result.type};
    if(!ExecHostsCall(wasm->hosts, host, arguments, count, &result))
        goto done;
    /* Changed slices go back into their images; the module copies them
     * into the caller's elements. */
    for(size_t at = 0; at < count; at++) {
        if(!arguments[at].changed)
            continue;
        unsigned char *entry = entries + 16 * at;
        unsigned char flag[4];
        zx_wasm_put(flag, 1, 4);
        if(declared->parameters[at].kind != ZX_WASM_IMAGE || declared->parameters[at].element_size == 0 ||
           arguments[at].size != zx_wasm_u32(entry + 8) ||
           !zx_wasm_write(wasm, zx_wasm_u32(entry), arguments[at].bytes, arguments[at].size) ||
           !zx_wasm_write(wasm, record + 16 * at + 12, flag, sizeof(flag)))
            goto done;
    }
    unsigned char entry[16] = {0};
    if(declared->result.kind == ZX_WASM_BITS) {
        zx_wasm_put(entry, result.bits, 8);
    } else if(declared->result.kind == ZX_WASM_IMAGE && result.size > 0) {
        size_t address = zx_wasm_reserve(wasm, result.size);
        if(address == 0 || address > UINT32_MAX || !zx_wasm_write(wasm, address, result.bytes, result.size))
            goto done;
        zx_wasm_put(entry, address, 4);
        zx_wasm_put(entry + 8, result.size, 4);
    }
    ok = declared->result.kind == ZX_WASM_VOID || zx_wasm_write(wasm, record + 16 * count, entry, sizeof(entry));
done:
    for(size_t at = 0; images != NULL && at < count; at++)
        free(images[at]);
    free(images);
    free(arguments);
    free(entries);
    return ok;
}

/* ---- Instances ---------------------------------------------------------- */

/* Spends a shared budget by what the provider says is left; gives whether
 * the budget ran out. */
static int
zx_wasm_charge(ZxWasm *wasm)
{
    long long left = 1;
    if(wasm->platform.budget_left == NULL ||
       wasm->platform.budget_left(wasm->platform.context, wasm->handle, &left) != 0)
        return 0;
    if(wasm->shared_budget)
        wasm->budget = left > 0 ? left : 0;
    return left <= 0;
}

/* A failed call: a spent step limit, as the engine words it, or a trap. */
static void
zx_wasm_failed(ZxWasm *wasm, int spent, const char *what)
{
    if(spent && wasm->limit > 0)
        Diagnostic((ZirSourceSpan){0}, "zx.runtime", "wasm execution failed: stopped after %lld steps",
                   wasm->limit);
    else
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "%s trapped or failed", what);
}

ZxWasm *
ZxWasmOpenExec(const ExecBundle *bundle, ExecFindBinding find, void *context, long long max_steps)
{
    if(!platform_installed || active_platform.read == NULL ||
       active_platform.write == NULL || active_platform.allocate_state == NULL)
        return NULL;
    ZxWasm *wasm = calloc(1, sizeof(*wasm));
    if(wasm == NULL) {
        DiagnosticOutOfMemory();
        return NULL;
    }
    wasm->exec = bundle;
    if(!ZxWasmLowerProgram(ExecBundleProgram(bundle), &wasm->image))
        goto failed;
    wasm->hosts = ExecHostsOpen(bundle, find, context);
    wasm->host_of = calloc(wasm->image.capability_count + 1, sizeof(*wasm->host_of));
    if(wasm->hosts == NULL || wasm->host_of == NULL)
        goto failed;
    for(size_t index = 0; index < wasm->image.capability_count; index++) {
        const ZxWasmCapability *capability = &wasm->image.capabilities[index];
        size_t host = 0, hosts = ExecBundleCapabilityCount(bundle);
        while(host < hosts && (strcmp(ExecBundleCapabilityModule(bundle, host), capability->module) != 0 ||
                               strcmp(ExecBundleCapabilityFunction(bundle, host), capability->function) != 0))
            host++;
        if(host == hosts) {
            Diagnostic((ZirSourceSpan){0}, "zx.wasm", "the bundle declares no host capability %s:%s",
                       capability->module, capability->function);
            goto failed;
        }
        wasm->host_of[index] = host;
    }
    wasm->platform = active_platform;
    wasm->platform.context = wasm;
    wasm->shared_budget = max_steps > 0;
    wasm->limit = max_steps > 0 ? max_steps : 0;
    wasm->budget = max_steps > 0 ? max_steps : 0x7fffffffffffffffLL;
    wasm->handle = active_platform.instantiate(wasm->platform.context, wasm->image.bytes, wasm->image.size);
    free(wasm->image.bytes);
    wasm->image.bytes = NULL;
    if(wasm->handle == 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "the wasm provider rejected the lowered module");
        goto failed;
    }
    /* Global initializers run when the instance opens, as on the engine. */
    if(wasm->platform.initialize != NULL) {
        int initialized = wasm->platform.set_budget(wasm->platform.context, wasm->handle, wasm->budget) == 0 &&
                          wasm->platform.initialize(wasm->platform.context, wasm->handle) == 0;
        int spent = zx_wasm_charge(wasm);
        if(!initialized) {
            zx_wasm_failed(wasm, spent, "module initialization");
            goto failed;
        }
    }
    return wasm;
failed:
    ZxWasmClose(wasm);
    return NULL;
}

int
ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result)
{
    if(wasm == NULL || wasm->failed)
        return 0;
    if(wasm->shared_budget && wasm->budget == 0) {
        zx_wasm_failed(wasm, 1, "the run");
        return 0;
    }
    long long value = 0;
    int called = wasm->platform.set_budget(wasm->platform.context, wasm->handle, wasm->budget) == 0 &&
                 wasm->platform.call(wasm->platform.context, wasm->handle, &value) == 0;
    int spent = zx_wasm_charge(wasm);
    if(!called) {
        zx_wasm_failed(wasm, spent, "the wasm instance");
        wasm->failed = 1;
        return 0;
    }
    if(result != NULL)
        *result = value;
    if(has_result != NULL)
        *has_result = wasm->image.returns;
    return 1;
}

void
ZxWasmLimitSteps(ZxWasm *wasm, long long max_steps)
{
    if(wasm == NULL)
        return;
    /* The lowered module charges its fuel against this budget; zero is
     * unbounded. A shared limit is replaced, otherwise each later run
     * gets it. */
    wasm->limit = max_steps > 0 ? max_steps : 0;
    wasm->budget = max_steps > 0 ? max_steps : 0x7fffffffffffffffLL;
}

void
ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled)
{
    if(wasm != NULL && wasm->hosts != NULL)
        ExecHostsMatchFieldsByName(wasm->hosts, enabled);
}

size_t
ZxWasmLiveBytes(const ZxWasm *wasm)
{
    size_t bytes = 0;
    for(size_t index = 0; wasm != NULL && index < wasm->image.global_count; index++)
        bytes += wasm->image.global_sizes[index];
    return bytes;
}

/* ---- Module state ------------------------------------------------------- */

size_t
ZxWasmStateCount(const ZxWasm *wasm)
{
    return wasm == NULL ? 0 : ExecBundleStateCount(wasm->exec);
}

int
ZxWasmStateAt(ZxWasm *wasm, size_t index, const char **module, const char **name, const char **type,
              VmHostValue *value)
{
    int32_t global;
    if(wasm == NULL || wasm->failed || !ExecBundleStateAt(wasm->exec, index, module, name, type, &global))
        return 0;
    if(value == NULL)
        return 1;
    free(wasm->state_image);
    wasm->state_image = NULL;
    ZxWasmMemory memory = zx_wasm_memory(wasm);
    size_t size = 0;
    if(!ZxWasmGlobalImage(ExecBundleProgram(wasm->exec), global, &wasm->image, &memory,
                          &wasm->state_image, &size))
        return 0;
    return ExecHostsStateValue(wasm->hosts, index, wasm->state_image, size, value);
}

int
ZxWasmStateStore(ZxWasm *wasm, const VmHostValue *const *values)
{
    if(wasm == NULL || wasm->failed || values == NULL)
        return 0;
    size_t count = ExecBundleStateCount(wasm->exec);
    unsigned char **images = calloc(count + 1, sizeof(*images));
    size_t *sizes = calloc(count + 1, sizeof(*sizes));
    int ok = images != NULL && sizes != NULL;
    /* Every value becomes an image before any global changes. */
    for(size_t index = 0; ok && index < count; index++) {
        const unsigned char *bytes;
        if(values[index] == NULL)
            continue;
        ok = ExecHostsStateImage(wasm->hosts, index, values[index], &bytes, &sizes[index]) &&
             (images[index] = malloc(sizes[index] ? sizes[index] : 1)) != NULL;
        if(ok && sizes[index]) memcpy(images[index], bytes, sizes[index]);
    }
    ZxWasmMemory memory = zx_wasm_memory(wasm);
    for(size_t index = 0; ok && index < count; index++) {
        const char *module, *name, *type;
        int32_t global;
        if(images[index] == NULL)
            continue;
        ok = ExecBundleStateAt(wasm->exec, index, &module, &name, &type, &global) &&
             ZxWasmGlobalStore(ExecBundleProgram(wasm->exec), global, &wasm->image, &memory,
                               images[index], sizes[index]);
    }
    for(size_t index = 0; images != NULL && index < count; index++)
        free(images[index]);
    free(images);
    free(sizes);
    return ok;
}

void
ZxWasmClose(ZxWasm *wasm)
{
    if(wasm == NULL)
        return;
    if(wasm->platform.release != NULL && wasm->handle != 0)
        wasm->platform.release(wasm->platform.context, wasm->handle);
    ExecHostsClose(wasm->hosts);
    ZxWasmImageFree(&wasm->image);
    free(wasm->host_of);
    free(wasm->state_image);
    free(wasm);
}
