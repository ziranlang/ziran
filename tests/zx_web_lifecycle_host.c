/* End-to-end lifecycle host for a browser WebAssembly embedding. */
#define _POSIX_C_SOURCE 200809L
#include "ziran_host.h"
#include "zir_zx_wasm_platform.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed;

static void report(const char *name, int ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failed = failed || !ok;
}

static int adjust(void *context, const char *module, const char *function,
                  const VmHostValue *arguments, int count, VmHostValue *result)
{
    (void)context;
    if(strcmp(module, "component") != 0 || strcmp(function, "Adjust") != 0 ||
       count != 1 || arguments[0].kind != VM_HOST_INTEGER)
        return 0;
    result->kind = VM_HOST_INTEGER;
    result->type = "s64";
    result->integer = arguments[0].integer + 1;
    result->bits = (uint64_t)result->integer;
    return 1;
}

static int state_find(const BundleState *state, const char *name, size_t *index)
{
    for(size_t i = 0; i < BundleStateCount(state); i++) {
        BundleStateValue value;
        if(BundleStateValueAt(state, i, &value) && strcmp(value.name, name) == 0)
            return (*index = i, 1);
    }
    return 0;
}

static VmHostValue *record_field(VmHostValue *record, const char *name)
{
    VmHostField *fields;
    if(record == NULL || record->kind != VM_HOST_RECORD)
        return NULL;
    fields = (VmHostField *)record->fields;
    for(size_t i = 0; i < record->field_count; i++)
        if(fields[i].name != NULL && strcmp(fields[i].name, name) == 0)
            return &fields[i].value;
    return NULL;
}

static VmHostValue *state_field(BundleStateValue *value, const char *name)
{
    return value == NULL ? NULL : record_field(&value->value, name);
}

static int copy_owned_string(VmHostValue *target, const VmHostValue *source)
{
    unsigned char *copied = NULL;
    if(target == NULL || source == NULL || target->kind != VM_HOST_STRING ||
       source->kind != VM_HOST_STRING)
        return 0;
    if(source->length != 0) {
        copied = malloc(source->length + 1);
        if(copied == NULL)
            return 0;
        memcpy(copied, source->data, source->length);
        copied[source->length] = 0;
    }
    free((void *)target->data);
    target->data = copied;
    target->length = source->length;
    return 1;
}

static int migrate(void *context, const BundleState *old_state,
                   BundleState *new_state)
{
    (void)context;
    size_t old_index = 0, new_index = 0;
    BundleStateValue old_value;
    BundleStateValue *new_value;
    VmHostValue *old_count, *new_count, *old_first, *new_first;
    VmHostValue *old_history, *new_history, *new_open;
    VmHostValue *old_title, *new_title, *old_weight, *new_weight;
    int ok = state_find(old_state, "state", &old_index) &&
             state_find(new_state, "state", &new_index) &&
             BundleStateValueAt(old_state, old_index, &old_value) &&
             (new_value = BundleStateValueRef(new_state, new_index)) != NULL &&
             (old_count = state_field(&old_value, "count")) != NULL &&
             (new_count = state_field(new_value, "count")) != NULL &&
             (old_first = state_field(&old_value, "first")) != NULL &&
             (new_first = state_field(new_value, "first")) != NULL &&
             (old_history = state_field(&old_value, "history")) != NULL &&
             (new_history = state_field(new_value, "history")) != NULL &&
             (new_open = state_field(new_value, "open")) != NULL &&
             old_count->kind == VM_HOST_INTEGER &&
             new_count->kind == VM_HOST_INTEGER;
    if(!ok)
        return 0;
    new_count->integer = old_count->integer + 100;
    new_count->bits = (uint64_t)new_count->integer;
    old_title = record_field(old_first, "title");
    new_title = record_field(new_first, "title");
    old_weight = record_field(old_first, "weight");
    new_weight = record_field(new_first, "weight");
    ok = old_title != NULL && new_title != NULL && old_weight != NULL &&
         new_weight != NULL && copy_owned_string(new_title, old_title) &&
         old_weight->kind == VM_HOST_INTEGER &&
         new_weight->kind == VM_HOST_INTEGER &&
         (new_weight->integer = old_weight->integer,
          new_weight->bits = old_weight->bits, 1) &&
         old_history->kind == VM_HOST_ARRAY &&
         new_history->kind == VM_HOST_ARRAY &&
         new_history->length == old_history->length;
    for(size_t i = 0; ok && i < old_history->length; i++)
        ok = old_history->elements[i].kind == VM_HOST_INTEGER &&
             new_history->elements[i].kind == VM_HOST_INTEGER &&
             (new_history->elements[i].integer = old_history->elements[i].integer,
              new_history->elements[i].bits = old_history->elements[i].bits, 1);
    new_open->kind = VM_HOST_INTEGER;
    new_open->integer = 1;
    new_open->bits = 1;
    return ok;
}

static int reject_migration(void *context, const BundleState *old_state,
                            BundleState *new_state)
{
    (void)context; (void)old_state; (void)new_state;
    return 0;
}

static int reject_verification(void *context, BundleInstance *instance)
{
    (void)context; (void)instance;
    return 0;
}

static int run_instance(BundleInstance *instance, long long expected)
{
    long long result = -1;
    int present = 0;
    return BundleInstanceRun(instance, &result, &present) && present &&
           result == expected;
}

static int run_handle(BundleComponentHandle *handle, long long expected)
{
    long long result = -1;
    int present = 0;
    return BundleComponentRun(handle, &result, &present) && present &&
           result == expected;
}

static int migrate_instance(BundleInstance *old, const Bundle *replacement,
                            const HostBinding *bindings, int portable)
{
    BundleInstance *candidate = portable ?
        BundleInstantiatePortable(replacement, bindings, 1) :
        BundleInstantiate(replacement, bindings, 1);
    BundleState *old_state = NULL, *new_state = NULL;
    int ok = candidate != NULL &&
             BundleInstanceExportState(old, &old_state) &&
             BundleInstanceExportState(candidate, &new_state) &&
             migrate(NULL, old_state, new_state) &&
             BundleInstanceImportState(candidate, new_state) &&
             run_instance(candidate, 2109);
    BundleInstanceClose(candidate);
    /* Snapshot names, types and nested strings outlive the source instance. */
    size_t index = 0;
    BundleStateValue value;
    VmHostValue *first, *title;
    ok = ok && BundleStateFind(new_state, "component", "state", &index) &&
         BundleStateValueAt(new_state, index, &value) &&
         strcmp(value.type, "State") == 0 &&
         (first = state_field(&value, "first")) != NULL &&
         (title = record_field(first, "title")) != NULL &&
         strcmp(title->type, "string") == 0 && title->length == 3 &&
         title->data != NULL && memcmp(title->data, "old", 3) == 0;
    BundleStateClose(old_state);
    BundleStateClose(new_state);
    return ok;
}

static int exported_title_is_old(BundleInstance *instance)
{
    BundleState *state = NULL;
    BundleStateValue value;
    size_t index = 0;
    VmHostValue *first, *title;
    int ok = BundleInstanceExportState(instance, &state) &&
             state_find(state, "state", &index) &&
             BundleStateValueAt(state, index, &value) &&
             (first = state_field(&value, "first")) != NULL &&
             (title = record_field(first, "title")) != NULL &&
             title->kind == VM_HOST_STRING && title->length == 3 &&
             title->data != NULL && memcmp(title->data, "old", 3) == 0;
    BundleStateClose(state);
    return ok;
}

EM_JS(unsigned long, zx_wasm_js_instantiate, (int ptr, int size), {
    const bytes = new Uint8Array(HEAPU8.buffer, ptr, size).slice();
    try {
        const budget = new WebAssembly.Global({value: 'i64', mutable: true}, 0n);
        const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {
            zx: {
                overrun: () => { throw new Error('zx budget spent'); },
                budget: budget,
            },
        });
        if(globalThis.zxWasmInstances === undefined)
            globalThis.zxWasmInstances = new Map();
        const id = globalThis.zxWasmInstances.size + 1;
        globalThis.zxWasmInstances.set(id, {instance: instance, budget: budget});
        return id;
    } catch(error) {
        console.error('instantiate failed: ' + error);
        return 0;
    }
});

EM_JS(int, zx_wasm_js_set_budget, (unsigned long handle, long long budget), {
    if(globalThis.zxWasmInstances === undefined)
        return 1;
    const entry = globalThis.zxWasmInstances.get(handle);
    if(entry === undefined)
        return 1;
    entry.budget.value = BigInt.asIntN(64, BigInt(budget));
    return 0;
});

EM_JS(int, zx_wasm_js_call, (unsigned long handle, long long *result), {
    if(globalThis.zxWasmInstances === undefined)
        return 1;
    const entry = globalThis.zxWasmInstances.get(handle);
    if(entry === undefined)
        return 1;
    try {
        const value = entry.instance.exports.zx_entry();
        setValue(result, value, 'i64');
        return 0;
    } catch(error) {
        console.error('run trapped: ' + error);
        return 1;
    }
});

EM_JS(int, zx_wasm_js_release, (unsigned long handle), {
    if(globalThis.zxWasmInstances === undefined)
        return 1;
    return globalThis.zxWasmInstances.delete(handle) ? 0 : 1;
});

static unsigned long provider_instantiate(void *context,
                                           const unsigned char *bytes, size_t size)
{
    (void)context;
    return zx_wasm_js_instantiate((int)bytes, (int)size);
}

static int provider_set_budget(void *context, unsigned long handle,
                               long long budget)
{
    (void)context;
    return zx_wasm_js_set_budget(handle, budget);
}

static int provider_call(void *context, unsigned long handle, long long *result)
{
    (void)context;
    return zx_wasm_js_call(handle, result);
}

static void provider_release(void *context, unsigned long handle)
{
    (void)context;
    zx_wasm_js_release(handle);
}

int main(int argc, char **argv)
{
    Bundle *old, *replacement, *wrong_abi, *init_failure, *unrelated_bundle;
    unsigned char old_identity[32], new_identity[32];
    BundleCapabilityAbi capability;
    BundleInstance *left, *right, *state_instance;
    BundleComponent *component, *same_code, *unrelated, *bounded;
    BundleComponentHandle *handle, *same_handle, *unrelated_handle;
    BundleComponentHandle *replacement_handle, *current, *bounded_handle;
    HostBinding bindings[] = {
        {.module = "component", .function = "Adjust", .call = adjust}
    };
    int wasm_mode, capability_found = 0;
    char done[64];

    if(argc != 7) {
        fprintf(stderr,
                "usage: zx_web_lifecycle_host wasm|linked old new wrong init unrelated\n");
        return 2;
    }
    wasm_mode = strcmp(argv[1], "wasm") == 0;
    if(!wasm_mode && strcmp(argv[1], "linked") != 0) {
        fprintf(stderr, "unknown lifecycle host mode %s\n", argv[1]);
        return 2;
    }
    old = BundleOpen(argv[2]);
    replacement = BundleOpen(argv[3]);
    wrong_abi = BundleOpen(argv[4]);
    init_failure = BundleOpen(argv[5]);
    unrelated_bundle = BundleOpen(argv[6]);
    if(old == NULL || replacement == NULL || wrong_abi == NULL ||
       init_failure == NULL || unrelated_bundle == NULL) {
        fprintf(stderr, "cannot open browser lifecycle fixtures\n");
        return 3;
    }

    BundleCodeIdentity(old, old_identity);
    BundleCodeIdentity(replacement, new_identity);
    for(size_t i = 0; i < BundleCapabilityCount(old); i++) {
        if(BundleCapabilityAbiAt(old, i, &capability) &&
           strcmp(capability.module, "component") == 0 &&
           strcmp(capability.function, "Adjust") == 0) {
            capability_found = capability.arguments[0] != '\0' &&
                               strcmp(capability.return_type, "s64") == 0;
            break;
        }
    }
    report("identity-entry-abi", memcmp(old_identity, new_identity, 32) != 0 &&
           strcmp(BundleEntryModuleName(old), "component") == 0 &&
           strcmp(BundleEntryFunctionName(old), "Next") == 0 &&
           BundleAbiCompatible(old, replacement) &&
           !BundleAbiCompatible(old, wrong_abi) && capability_found);

    left = BundleInstantiate(old, bindings, 1);
    right = BundleInstantiate(old, bindings, 1);
    report(wasm_mode ? "wasm-provider-independent" : "linked-independent",
           left != NULL && right != NULL && run_instance(left, 89) &&
           run_instance(left, 99) && run_instance(right, 89) &&
           run_instance(right, 99));
    if(wasm_mode)
        report("wasm-engine-active", EM_ASM_INT({
            return globalThis.zxWasmTier !== undefined &&
                   globalThis.zxWasmTier.instances.size === 2;
        }));
    /* Linked native code has opaque globals; its component lifecycle uses
     * portable instances. The Wasm provider exposes its typed state table. */
    state_instance = wasm_mode ? left :
        BundleInstantiatePortable(old, bindings, 1);
    int state_ready = wasm_mode || (state_instance != NULL &&
                      run_instance(state_instance, 89) &&
                      run_instance(state_instance, 99));
    report("instance-state-migration", state_ready &&
           exported_title_is_old(state_instance) &&
           migrate_instance(state_instance, replacement, bindings, !wasm_mode));
    if(!wasm_mode)
        BundleInstanceClose(state_instance);
    BundleInstanceClose(left);
    BundleInstanceClose(right);

    component = BundleComponentOpen(old, bindings, 1, 10000);
    same_code = BundleComponentOpen(old, bindings, 1, 10000);
    unrelated = BundleComponentOpen(unrelated_bundle, NULL, 0, 10000);
    handle = component == NULL ? NULL : BundleComponentAcquire(component);
    same_handle = same_code == NULL ? NULL : BundleComponentAcquire(same_code);
    unrelated_handle = unrelated == NULL ? NULL : BundleComponentAcquire(unrelated);
    report("component-independent-state",
           handle != NULL && same_handle != NULL && unrelated_handle != NULL &&
           run_handle(handle, 89) && run_handle(handle, 99) &&
           run_handle(same_handle, 89) && run_handle(same_handle, 99) &&
           run_handle(unrelated_handle, 101));

    report("typed-abi-replacement-rejected",
           !BundleComponentReplace(component, wrong_abi, migrate, NULL, NULL, NULL) &&
           BundleComponentHandleValid(handle) && run_handle(handle, 109));

    replacement_handle = NULL;
    report("replacement-state-continuity",
           BundleComponentReplace(component, replacement, migrate, NULL, NULL,
                                  &replacement_handle) &&
           replacement_handle != NULL &&
           BundleComponentHandleValid(replacement_handle) &&
           run_handle(replacement_handle, 2119) &&
           run_handle(unrelated_handle, 102));
    report("stale-reference-rejected",
           !BundleComponentHandleValid(handle) &&
           !BundleComponentRun(handle, &(long long){0}, &(int){0}));

    current = BundleComponentAcquire(component);
    report("migration-failure-rollback",
           !BundleComponentReplace(component, replacement, reject_migration,
                                   NULL, NULL, NULL) &&
           BundleComponentHandleValid(current) && run_handle(current, 2129));
    report("verification-failure-rollback",
           !BundleComponentReplace(component, replacement, migrate,
                                   reject_verification, NULL, NULL) &&
           BundleComponentHandleValid(current) && run_handle(current, 2139));

    bounded = BundleComponentOpen(old, bindings, 1, 64);
    bounded_handle = bounded == NULL ? NULL : BundleComponentAcquire(bounded);
    report("initialization-failure-rollback",
           bounded_handle != NULL && run_handle(bounded_handle, 89) &&
           !BundleComponentReplace(bounded, init_failure, migrate, NULL, NULL, NULL) &&
           BundleComponentHandleValid(bounded_handle) &&
           run_handle(bounded_handle, 99) && run_handle(unrelated_handle, 103));
    report("unrelated-component-live",
           BundleComponentHandleValid(unrelated_handle) &&
           run_handle(unrelated_handle, 104) &&
           BundleComponentLiveValueBytes(component) > 0);

    BundleComponentHandleClose(handle);
    BundleComponentHandleClose(same_handle);
    BundleComponentHandleClose(replacement_handle);
    BundleComponentHandleClose(current);
    BundleComponentHandleClose(bounded_handle);
    BundleComponentHandleClose(unrelated_handle);
    BundleComponentClose(component);
    BundleComponentClose(same_code);
    BundleComponentClose(bounded);
    BundleComponentClose(unrelated);
    BundleClose(old);
    BundleClose(replacement);
    BundleClose(wrong_abi);
    BundleClose(init_failure);
    BundleClose(unrelated_bundle);

    printf("%s browser-lifecycle %s\n", failed ? "FAIL" : "PASS",
           wasm_mode ? "wasm" : "linked");
    snprintf(done, sizeof(done), "globalThis.lifecycleDone = %d",
             failed ? 1 : 0);
    emscripten_run_script(done);
    return failed;
}
