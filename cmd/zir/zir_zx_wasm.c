/*
 * zir_zx_wasm.c - bundles executed as WebAssembly through the installed
 * provider. Mirrors the native engine's five-function contract so
 * zir_host.c picks between tiers at one seam. A revision-28 module's host
 * calls cross as the instruction engine's host images: the bridge copies
 * them between the instance's memory and ExecHosts, which translates them
 * to VmHostValue trees by the embedding API's rules.
 */
#include "zir_zx_wasm.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "compiler_bundle.h"
#include "../zir-wasm/zir_wasm_lower.h"
#include "../zir-wasm/zx_wasm_lower.h"
#include "zir_zx_wasm_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef struct ZxWasmHostBridge {
    ZxWasm *wasm;
    VmHostCall call;
    void *context;
    const char **modules;
    const char **functions;
    char *names;
    size_t count;
} ZxWasmHostBridge;

typedef struct ZxWasmStateSlot {
    char *module;
    char *name;
    char *type;
    size_t address;
    size_t size;
} ZxWasmStateSlot;

struct ZxWasm {
    ZxWasmPlatform platform;
    const ZirProgram *program;
    ZxWasmHostBridge *bridge;
    ZxWasmStateSlot *states;
    size_t state_count;
    unsigned long handle;
    long long budget;
    int failed;
    /* A revision-28 bundle: its host calls, the lowered module's
     * capabilities, and each one's bundle host index. */
    ExecHosts *hosts;
    const ExecBundle *exec;
    ZxWasmImage image;
    size_t *host_of;
    /* Opened with a step limit: initialization and runs share it. */
    int shared_budget;
};

static void host_bridge_release(ZxWasmHostBridge *bridge);

static ZxWasmPlatform active_platform;
static int platform_installed;
enum { ZX_WASM_RUNTIME_DATA_BASE = 1024 };

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

static ZxWasmHostBridge *
host_bridge_create(VmHostCall call, void *context,
                   const struct Capability *capabilities, size_t count)
{
    if((count != 0 && capabilities == NULL) ||
       count > SIZE_MAX / sizeof(*capabilities))
        return NULL;
    ZxWasmHostBridge *bridge = calloc(1, sizeof(*bridge));
    if(bridge == NULL)
        return NULL;
    bridge->call = call;
    bridge->context = context;
    bridge->count = count;
    if(count == 0)
        return bridge;
    size_t bytes = 0;
    for(size_t i = 0; i < count; i++) {
        const struct Capability *capability = &capabilities[i];
        if(capability->module.length >= SIZE_MAX - bytes ||
           capability->function.length >= SIZE_MAX - bytes ||
           bytes > SIZE_MAX - capability->module.length -
                   capability->function.length - 2 ||
           count > SIZE_MAX / (2 * sizeof(char *)))
            goto fail;
        bytes += capability->module.length + capability->function.length + 2;
        if(bytes >= SIZE_MAX)
            goto fail;
    }
    bridge->modules = calloc(count, sizeof(*bridge->modules));
    bridge->functions = calloc(count, sizeof(*bridge->functions));
    bridge->names = malloc(bytes);
    if(bridge->modules == NULL || bridge->functions == NULL ||
       bridge->names == NULL)
        goto fail;
    char *name = bridge->names;
    for(size_t i = 0; i < count; i++) {
        const struct Capability *capability = &capabilities[i];
        bridge->modules[i] = name;
        memcpy(name, capability->module.data, capability->module.length);
        name[capability->module.length] = '\0';
        name += capability->module.length + 1;
        bridge->functions[i] = name;
        memcpy(name, capability->function.data, capability->function.length);
        name[capability->function.length] = '\0';
        name += capability->function.length + 1;
    }
    return bridge;
fail:
    host_bridge_release(bridge);
    return NULL;
}

static void
host_bridge_release(ZxWasmHostBridge *bridge)
{
    if(bridge == NULL)
        return;
    free(bridge->modules);
    free(bridge->functions);
    free(bridge->names);
    free(bridge);
}

typedef struct WasmReader {
    const unsigned char *bytes;
    size_t size;
    size_t at;
} WasmReader;

static int
wasm_read(WasmReader *reader, void *destination, size_t size)
{
    if(size > reader->size - reader->at)
        return 0;
    memcpy(destination, reader->bytes + reader->at, size);
    reader->at += size;
    return 1;
}

static int
wasm_read_uleb(WasmReader *reader, size_t *value)
{
    uint64_t result = 0;
    for(unsigned int shift = 0; shift < 64; shift += 7) {
        unsigned char byte;
        if(!wasm_read(reader, &byte, 1))
            return 0;
        result |= (uint64_t)(byte & 0x7f) << shift;
        if((byte & 0x80) == 0) {
            if(result > SIZE_MAX)
                return 0;
            *value = (size_t)result;
            return 1;
        }
    }
    return 0;
}

static int
wasm_copy_name(WasmReader *reader, char **destination)
{
    size_t size;
    if(!wasm_read_uleb(reader, &size) || size > 1 << 20)
        return 0;
    char *copied = malloc(size + 1);
    if(copied == NULL || !wasm_read(reader, copied, size)) {
        free(copied);
        return 0;
    }
    copied[size] = '\0';
    *destination = copied;
    return 1;
}

static void
zx_wasm_state_slots_release(ZxWasmStateSlot *states, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        free(states[i].module);
        free(states[i].name);
        free(states[i].type);
    }
    free(states);
}

static int
zx_wasm_parse_states(const unsigned char *image, size_t size,
                     ZxWasmStateSlot **states, size_t *count)
{
    if(size < 8 || states == NULL || count == NULL)
        return 0;
    WasmReader module = {image, size, 8};
    while(module.at < module.size) {
        unsigned char id;
        size_t section_size;
        if(!wasm_read(&module, &id, 1) ||
           !wasm_read_uleb(&module, &section_size) ||
           section_size > module.size - module.at)
            return 0;
        size_t end = module.at + section_size;
        if(id == 0) {
            WasmReader section = {image, end, module.at};
            size_t name_size;
            if(!wasm_read_uleb(&section, &name_size))
                return 0;
            if(name_size == 7 && section.at + 7 <= section.size &&
               memcmp(image + section.at, "zxstate", 7) == 0) {
                section.at += 7;
                size_t state_count;
                if(!wasm_read_uleb(&section, &state_count) ||
                   state_count > 100000)
                    return 0;
                ZxWasmStateSlot *parsed = calloc(state_count ? state_count : 1,
                                                 sizeof(*parsed));
                if(parsed == NULL)
                    return 0;
                for(size_t i = 0; i < state_count; i++) {
                    size_t offset, slot_size;
                    if(!wasm_copy_name(&section, &parsed[i].module) ||
                       !wasm_copy_name(&section, &parsed[i].name) ||
                       !wasm_copy_name(&section, &parsed[i].type) ||
                       !wasm_read_uleb(&section, &offset) ||
                       !wasm_read_uleb(&section, &slot_size) ||
                       offset > SIZE_MAX - ZX_WASM_RUNTIME_DATA_BASE ||
                       slot_size > SIZE_MAX - offset) {
                        zx_wasm_state_slots_release(parsed, i + 1);
                        return 0;
                    }
                    parsed[i].address = ZX_WASM_RUNTIME_DATA_BASE + offset;
                    parsed[i].size = slot_size;
                }
                if(section.at != section.size) {
                    zx_wasm_state_slots_release(parsed, state_count);
                    return 0;
                }
                *states = parsed;
                *count = state_count;
                return 1;
            }
        }
        module.at = end;
    }
    return 0;
}

int
ZxWasmHostCallI64(void *context, size_t index, int64_t argument,
                  int64_t *result)
{
    ZxWasmHostBridge *bridge = context;
    if(bridge != NULL && result != NULL && (uint32_t)index == ZX_WASM_PRINT_STRING && bridge->wasm != NULL) {
        size_t address = (uint32_t)argument;
        size_t length = (size_t)((uint64_t)argument >> 32);
        unsigned char bytes[4096];
        while(length != 0) {
            size_t count = length < sizeof(bytes) ? length : sizeof(bytes);
            if(!ZxWasmReadState(bridge->wasm, address, bytes, count) ||
               fwrite(bytes, 1, count, stdout) != count) return 0;
            address += count;
            length -= count;
        }
        *result = 0;
        return 1;
    }
    if(bridge != NULL && result != NULL && (uint32_t)index >= ZX_WASM_PRINT_BOOL) {
        char text[64];
        switch((uint32_t)index) {
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
        *result = 0;
        return fputs(text, stdout) >= 0;
    }
    if(bridge == NULL || index >= bridge->count || result == NULL ||
       bridge->call == NULL)
        return 0;
    VmHostValue host_argument = {
        .kind = VM_HOST_INTEGER,
        .type = "s64",
        .integer = argument,
        .bits = (uint64_t)argument
    };
    VmHostValue host_result = {.kind = VM_HOST_INTEGER, .type = "s64"};
    if(!bridge->call(bridge->context, bridge->modules[index],
                     bridge->functions[index], &host_argument, 1,
                     &host_result) ||
       host_result.kind != VM_HOST_INTEGER)
        return 0;
    *result = host_result.integer;
    return 1;
}

ZxWasm *
ZxWasmOpen(const ZirProgram *program, const ZirModule *entry_owner,
           const ZirFunction *entry, const struct Capability *capabilities,
           size_t capability_count, VmHostCall host, void *host_context)
{
    if(!platform_installed || active_platform.read == NULL ||
       active_platform.write == NULL || active_platform.allocate_state == NULL)
        return NULL;
    if(capability_count != 0 && host == NULL) {
        Diagnostic(entry->span, "zx.wasm",
                   "bundle needs %zu host bindings but no instance host was supplied",
                   capability_count);
        return NULL;
    }
    size_t size = 0;
    size_t lowered_capabilities = 0;
    unsigned char *image = zir_wasm_lower(program, entry_owner->name,
                                          entry->name, &size,
                                          &lowered_capabilities);
    if(image == NULL)
        return NULL;
    if(lowered_capabilities != capability_count) {
        Diagnostic(entry->span, "zx.wasm",
                   "lowered module has %zu host capabilities but bundle declares %zu",
                   lowered_capabilities, capability_count);
        free(image);
        return NULL;
    }
    ZxWasm *wasm = calloc(1, sizeof(*wasm));
    if(wasm == NULL) {
        DiagnosticOutOfMemory();
        free(image);
        return NULL;
    }
    wasm->program = program;
    wasm->bridge = host_bridge_create(host, host_context, capabilities,
                                      capability_count);
    if(wasm->bridge == NULL) {
        DiagnosticOutOfMemory();
        free(image);
        free(wasm);
        return NULL;
    }
    if(!zx_wasm_parse_states(image, size, &wasm->states,
                             &wasm->state_count)) {
        Diagnostic(entry->span, "zx.wasm",
                   "lowered module has no portable state table");
        free(image);
        host_bridge_release(wasm->bridge);
        free(wasm);
        return NULL;
    }
    wasm->bridge->wasm = wasm;
    wasm->platform = active_platform;
    wasm->platform.context = wasm->bridge;
    wasm->budget = 0x7fffffffffffffffLL;
    wasm->failed = 0;
    wasm->handle = active_platform.instantiate(wasm->platform.context,
                                               image, size);
    free(image);
    if(wasm->handle == 0) {
        Diagnostic(entry->span, "zx.wasm",
                   "the wasm provider rejected the lowered module");
        host_bridge_release(wasm->bridge);
        zx_wasm_state_slots_release(wasm->states, wasm->state_count);
        free(wasm);
        return NULL;
    }
    return wasm;
}

/* Spends a shared budget by what the provider says is left. */
static void
zx_wasm_charge(ZxWasm *wasm)
{
    long long left = 0;
    if(wasm->shared_budget && wasm->platform.budget_left != NULL &&
       wasm->platform.budget_left(wasm->platform.context, wasm->handle, &left) == 0)
        wasm->budget = left > 0 ? left : 0;
}

int
ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result)
{
    if(wasm == NULL || wasm->failed)
        return 0;
    if(wasm->shared_budget && wasm->budget == 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "the instance has spent its step limit");
        return 0;
    }
    long long value = 0;
    int called = wasm->platform.set_budget(wasm->platform.context, wasm->handle,
                                           wasm->budget) == 0 &&
                 wasm->platform.call(wasm->platform.context, wasm->handle,
                                     &value) == 0;
    zx_wasm_charge(wasm);
    if(!called) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm",
                   "the wasm instance trapped or failed");
        wasm->failed = 1;
        return 0;
    }
    if(result != NULL)
        *result = value;
    if(has_result != NULL)
        *has_result = wasm->exec == NULL || wasm->image.returns;
    return 1;
}

size_t
ZxWasmStateCount(const ZxWasm *wasm)
{
    return wasm == NULL ? 0 : wasm->state_count;
}

int
ZxWasmStateAt(const ZxWasm *wasm, size_t index, const char **module,
              const char **name, const char **type, size_t *address,
              size_t *size)
{
    if(wasm == NULL || index >= wasm->state_count || module == NULL ||
       name == NULL || type == NULL || address == NULL || size == NULL)
        return 0;
    *module = wasm->states[index].module;
    *name = wasm->states[index].name;
    *type = wasm->states[index].type;
    *address = wasm->states[index].address;
    *size = wasm->states[index].size;
    return 1;
}

const ZirProgram *
ZxWasmProgram(const ZxWasm *wasm)
{
    return wasm == NULL ? NULL : wasm->program;
}

int
ZxWasmReadState(const ZxWasm *wasm, size_t address, void *destination,
                size_t size)
{
    if(wasm == NULL || destination == NULL || size > SIZE_MAX - address)
        return 0;
    return wasm->platform.read(wasm->platform.context, wasm->handle, address,
                               destination, size) == 0;
}

int
ZxWasmWriteState(const ZxWasm *wasm, size_t address, const void *source,
                 size_t size)
{
    if(wasm == NULL || source == NULL || size > SIZE_MAX - address)
        return 0;
    return wasm->platform.write(wasm->platform.context, wasm->handle, address,
                                source, size) == 0;
}

size_t
ZxWasmAllocateState(const ZxWasm *wasm, size_t size)
{
    if(wasm == NULL || size > UINT32_MAX)
        return 0;
    return wasm->platform.allocate_state(wasm->platform.context,
                                         wasm->handle, size);
}

void
ZxWasmLimitSteps(ZxWasm *wasm, int max_steps)
{
    if(wasm == NULL)
        return;
    /* The lowered module charges its fuel against this budget; zero is
     * unbounded. A shared limit is replaced, otherwise each later run
     * gets it. */
    wasm->budget = max_steps > 0 ? max_steps : 0x7fffffffffffffffLL;
}

void
ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled)
{
    /* The semantic tier crosses scalars only, so the flag changes nothing
     * there; revision-28 records cross through ExecHosts. */
    if(wasm != NULL && wasm->hosts != NULL)
        ExecHostsMatchFieldsByName(wasm->hosts, enabled);
}

void
ZxWasmClose(ZxWasm *wasm)
{
    if(wasm == NULL)
        return;
    if(wasm->platform.release != NULL && wasm->handle != 0)
        wasm->platform.release(wasm->platform.context, wasm->handle);
    host_bridge_release(wasm->bridge);
    zx_wasm_state_slots_release(wasm->states, wasm->state_count);
    ExecHostsClose(wasm->hosts);
    ZxWasmImageFree(&wasm->image);
    free(wasm->host_of);
    free(wasm);
}

/* ---- Revision-28 bundles ----------------------------------------------- */

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
    ZxWasmHostBridge *bridge = context;
    ZxWasm *wasm = bridge != NULL ? bridge->wasm : NULL;
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
       !ZxWasmReadState(wasm, record, entries, 16 * (count + 1)))
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
        if(images[at] == NULL || !ZxWasmReadState(wasm, zx_wasm_u32(entry), images[at], length))
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
           !ZxWasmWriteState(wasm, zx_wasm_u32(entry), arguments[at].bytes, arguments[at].size) ||
           !ZxWasmWriteState(wasm, record + 16 * at + 12, flag, sizeof(flag)))
            goto done;
    }
    unsigned char entry[16] = {0};
    if(declared->result.kind == ZX_WASM_BITS) {
        zx_wasm_put(entry, result.bits, 8);
    } else if(declared->result.kind == ZX_WASM_IMAGE && result.size > 0) {
        if(result.size > UINT32_MAX)
            goto done;
        size_t address = ZxWasmAllocateState(wasm, result.size);
        if(address == 0 || address > UINT32_MAX ||
           !ZxWasmWriteState(wasm, address, result.bytes, result.size))
            goto done;
        zx_wasm_put(entry, address, 4);
        zx_wasm_put(entry + 8, result.size, 4);
    }
    ok = declared->result.kind == ZX_WASM_VOID ||
         ZxWasmWriteState(wasm, record + 16 * count, entry, sizeof(entry));
done:
    for(size_t at = 0; images != NULL && at < count; at++)
        free(images[at]);
    free(images);
    free(arguments);
    free(entries);
    return ok;
}

ZxWasm *
ZxWasmOpenExec(const ExecBundle *bundle, ExecFindBinding find, void *context, long long max_steps)
{
    if(!platform_installed || active_platform.read == NULL ||
       active_platform.write == NULL || active_platform.allocate_state == NULL)
        return NULL;
    ZxWasm *wasm = calloc(1, sizeof(*wasm));
    if(wasm == NULL || (wasm->bridge = calloc(1, sizeof(*wasm->bridge))) == NULL) {
        DiagnosticOutOfMemory();
        free(wasm);
        return NULL;
    }
    wasm->exec = bundle;
    wasm->bridge->wasm = wasm;
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
    if(!zx_wasm_parse_states(wasm->image.bytes, wasm->image.size, &wasm->states, &wasm->state_count)) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm", "lowered module has no portable state table");
        goto failed;
    }
    wasm->platform = active_platform;
    wasm->platform.context = wasm->bridge;
    wasm->shared_budget = max_steps > 0;
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
        zx_wasm_charge(wasm);
        if(!initialized) {
            Diagnostic((ZirSourceSpan){0}, "zx.wasm", "module initialization trapped or failed");
            goto failed;
        }
    }
    return wasm;
failed:
    ZxWasmClose(wasm);
    return NULL;
}
