/*
 * zir_zx_wasm.c - bundles executed as WebAssembly through the installed
 * provider. Mirrors the native engine's five-function contract so
 * zir_host.c picks between tiers at one seam.
 */
#include "zir_zx_wasm.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "compiler_bundle.h"
#include "../zir-wasm/zir_wasm_lower.h"
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
    ZxWasm *wasm = malloc(sizeof(*wasm));
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

int
ZxWasmRun(ZxWasm *wasm, long long *result, int *has_result)
{
    if(wasm == NULL || wasm->failed)
        return 0;
    long long value = 0;
    if(wasm->platform.set_budget(wasm->platform.context, wasm->handle,
                                 wasm->budget) != 0 ||
       wasm->platform.call(wasm->platform.context, wasm->handle,
                           &value) != 0) {
        Diagnostic((ZirSourceSpan){0}, "zx.wasm",
                   "the wasm instance trapped or failed");
        wasm->failed = 1;
        return 0;
    }
    if(result != NULL)
        *result = value;
    if(has_result != NULL)
        *has_result = 1;
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
    /* The lowered module counts function entries and loop iterations
     * against this global, as the native tier does; zero is unbounded. */
    wasm->budget = max_steps > 0 ? max_steps : 0x7fffffffffffffffLL;
}

void
ZxWasmMatchFieldsByName(ZxWasm *wasm, int enabled)
{
    /* Record field matching needs the host-value bridge, which the scalar
     * tier does not cross; the flag is accepted and ignored. */
    (void)wasm;
    (void)enabled;
}

void
ZxWasmClose(ZxWasm *wasm)
{
    if(wasm == NULL)
        return;
    if(wasm->platform.release != NULL)
        wasm->platform.release(wasm->platform.context, wasm->handle);
    host_bridge_release(wasm->bridge);
    zx_wasm_state_slots_release(wasm->states, wasm->state_count);
    free(wasm);
}
