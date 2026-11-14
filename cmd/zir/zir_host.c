/*
 * zir_host.c - the embedding API (include/ziran_host.h) over revision-28
 * executable ZX: bundles, instances, module state and hot-swapped
 * components. A bundle runs on the portable instruction engine
 * (cmd/zir/zir_host_exec.c), or on the host's installed wasm provider.
 */
#include "ziran_host.h"
#include "zir.h"
#include "zir_diagnostic.h"
#include "zir_host_exec.h"
#include "zir_sha256.h"
#include "zir_zx_wasm.h"
#include "zir_zx_wasm_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Bundle {
    ExecBundle *exec;
    size_t references;
    /* SHA-256 of the file's bytes. */
    unsigned char digest[32];
};

static Bundle *
open_bundle(const unsigned char *bytes, size_t size, const char *name)
{
    Bundle *bundle = calloc(1, sizeof(*bundle));
    if(bundle == NULL)
        return NULL;
    bundle->references = 1;
    bundle->exec = ExecBundleOpen(bytes, size, name);
    if(bundle->exec == NULL) {
        free(bundle);
        return NULL;
    }
    Sha256 hash;
    Sha256Begin(&hash);
    Sha256Add(&hash, bytes, size);
    Sha256End(&hash, bundle->digest);
    return bundle;
}

Bundle *
BundleOpen(const char *path)
{
    if(path == NULL)
        return NULL;
    FILE *file = fopen(path, "rb");
    unsigned char *bytes = NULL;
    long size = -1;
    if(file != NULL && fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) >= 0 &&
       fseek(file, 0, SEEK_SET) == 0 && (bytes = malloc(size ? (size_t)size : 1)) != NULL &&
       fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        bytes = NULL;
    }
    if(file != NULL)
        fclose(file);
    if(bytes == NULL) {
        Diagnostic(Span(path, 1, 1), "zx.input", "cannot open bundle");
        return NULL;
    }
    Bundle *bundle = open_bundle(bytes, (size_t)size, path);
    free(bytes);
    return bundle;
}

Bundle *
BundleOpenBytes(const unsigned char *data, size_t size)
{
    if(data == NULL || size == 0)
        return NULL;
    return open_bundle(data, size, "<embedded bundle>");
}

void
BundleClose(Bundle *bundle)
{
    if(bundle == NULL || --bundle->references != 0)
        return;
    ExecBundleClose(bundle->exec);
    free(bundle);
}

size_t
BundleCapabilityCount(const Bundle *bundle)
{
    return bundle != NULL ? ExecBundleCapabilityCount(bundle->exec) : 0;
}

const char *
BundleCapabilityModule(const Bundle *bundle, size_t index)
{
    return bundle != NULL ? ExecBundleCapabilityModule(bundle->exec, index) : NULL;
}

const char *
BundleCapabilityFunction(const Bundle *bundle, size_t index)
{
    return bundle != NULL ? ExecBundleCapabilityFunction(bundle->exec, index) : NULL;
}

int
BundleCapabilityAbiAt(const Bundle *bundle, size_t index, BundleCapabilityAbi *abi)
{
    return bundle != NULL && abi != NULL && ExecBundleCapabilityAbi(bundle->exec, index, abi);
}

static int
same_optional_text(const char *left, const char *right)
{
    return strcmp(left != NULL ? left : "", right != NULL ? right : "") == 0;
}

/* Whether CANDIDATE declares capability INDEX of BUNDLE the same way. */
static int
same_capability_abi(const Bundle *bundle, size_t index, const Bundle *candidate)
{
    BundleCapabilityAbi left, right;
    if(!BundleCapabilityAbiAt(bundle, index, &left))
        return 0;
    for(size_t i = 0; i < BundleCapabilityCount(candidate); i++) {
        if(!BundleCapabilityAbiAt(candidate, i, &right))
            return 0;
        if(strcmp(left.module, right.module) == 0 &&
           strcmp(left.function, right.function) == 0 &&
           same_optional_text(left.signature, right.signature) &&
           same_optional_text(left.arguments, right.arguments) &&
           same_optional_text(left.return_type, right.return_type) &&
           left.variable_arguments == right.variable_arguments &&
           left.reads_arguments == right.reads_arguments)
            return 1;
    }
    return 0;
}

int
BundleAbiCompatible(const Bundle *left, const Bundle *right)
{
    if(left == NULL || right == NULL ||
       !same_optional_text(BundleEntryModuleName(left), BundleEntryModuleName(right)) ||
       !same_optional_text(BundleEntryFunctionName(left), BundleEntryFunctionName(right)) ||
       !same_optional_text(ExecBundleEntryArguments(left->exec), ExecBundleEntryArguments(right->exec)) ||
       !same_optional_text(ExecBundleEntryReturnType(left->exec), ExecBundleEntryReturnType(right->exec)))
        return 0;
    for(size_t i = 0; i < BundleCapabilityCount(left); i++)
        if(!same_capability_abi(left, i, right))
            return 0;
    for(size_t i = 0; i < BundleCapabilityCount(right); i++)
        if(!same_capability_abi(right, i, left))
            return 0;
    return 1;
}

const char *
BundleEntryModuleName(const Bundle *bundle)
{
    return bundle != NULL ? ExecBundleEntryModule(bundle->exec) : NULL;
}

const char *
BundleEntryFunctionName(const Bundle *bundle)
{
    return bundle != NULL ? ExecBundleEntryFunction(bundle->exec) : NULL;
}

const char *
BundleEntryReturnType(const Bundle *bundle)
{
    const char *type = bundle != NULL ? ExecBundleEntryReturnType(bundle->exec) : NULL;
    return type != NULL ? type : "";
}

void
BundleCodeIdentity(const Bundle *bundle, unsigned char identity[32])
{
    if(identity == NULL)
        return;
    if(bundle == NULL)
        memset(identity, 0, 32);
    else
        memcpy(identity, bundle->digest, 32);
}

size_t
BundleAssetCount(const Bundle *bundle)
{
    return bundle != NULL ? ExecBundleAssetCount(bundle->exec) : 0;
}

const char *
BundleAssetName(const Bundle *bundle, size_t index)
{
    return bundle != NULL ? ExecBundleAssetName(bundle->exec, index) : NULL;
}

const unsigned char *
BundleAssetData(const Bundle *bundle, size_t index)
{
    return bundle != NULL ? ExecBundleAssetData(bundle->exec, index) : NULL;
}

size_t
BundleAssetSize(const Bundle *bundle, size_t index)
{
    return bundle != NULL ? ExecBundleAssetSize(bundle->exec, index) : 0;
}

typedef struct BindingSet {
    const HostBinding *bindings;
    size_t count;
} BindingSet;

struct BundleInstance {
    BindingSet set;
    HostBinding *owned_bindings;
    /* One of these runs the bundle. */
    ExecInstance *exec;
    ZxWasm *wasm;
};

static int
compare_bindings(const void *left, const void *right)
{
    const HostBinding *a = left, *b = right;
    int module = strcmp(a->module, b->module);
    return module != 0 ? module : strcmp(a->function, b->function);
}

static const HostBinding *
find_binding(void *context, const char *module, const char *function)
{
    const BindingSet *set = context;
    HostBinding key = {.module = module, .function = function};
    size_t low = 0, high = set->count;
    while(low < high) {
        size_t middle = low + (high - low) / 2;
        int order = compare_bindings(&key, &set->bindings[middle]);
        if(order == 0)
            return &set->bindings[middle];
        if(order < 0) high = middle;
        else low = middle + 1;
    }
    return NULL;
}

static int
validate_bindings(const Bundle *bundle, BindingSet *set)
{
    for(size_t i = 1; i < set->count; i++) {
        if(compare_bindings(&set->bindings[i - 1], &set->bindings[i]) == 0) {
            Diagnostic(Span("<host>", 1, 1), "zx.capability",
                       "duplicate host capability binding: %s:%s",
                       set->bindings[i].module, set->bindings[i].function);
            return 0;
        }
    }
    for(size_t i = 0; i < BundleCapabilityCount(bundle); i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        if(find_binding(set, module, function) == NULL) {
            Diagnostic(Span("<host>", 1, 1), "zx.capability",
                       "missing host capability: %s:%s", module, function);
            return 0;
        }
    }
    return 1;
}

/* Binding names and the array are copied, sorted for lookup. */
static BundleInstance *
instantiate_bundle(const Bundle *bundle, const HostBinding *bindings,
                   size_t binding_count, int max_steps)
{
    if(bundle == NULL || (binding_count != 0 && bindings == NULL) ||
       binding_count > SIZE_MAX / sizeof(HostBinding))
        return NULL;
    size_t bytes = binding_count * sizeof(HostBinding);
    for(size_t i = 0; i < binding_count; i++) {
        if(bindings[i].module == NULL || bindings[i].function == NULL || bindings[i].call == NULL)
            return NULL;
        size_t length = strlen(bindings[i].module);
        if(length >= SIZE_MAX - bytes) return NULL;
        bytes += length + 1;
        length = strlen(bindings[i].function);
        if(length >= SIZE_MAX - bytes) return NULL;
        bytes += length + 1;
    }
    BundleInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL)
        return NULL;
    instance->set.count = binding_count;
    if(binding_count > 0) {
        instance->owned_bindings = malloc(bytes);
        if(instance->owned_bindings == NULL) {
            BundleInstanceClose(instance);
            return NULL;
        }
        char *name = (char *)(instance->owned_bindings + binding_count);
        for(size_t i = 0; i < binding_count; i++) {
            instance->owned_bindings[i] = bindings[i];
            size_t length = strlen(bindings[i].module) + 1;
            memcpy(name, bindings[i].module, length);
            instance->owned_bindings[i].module = name;
            name += length;
            length = strlen(bindings[i].function) + 1;
            memcpy(name, bindings[i].function, length);
            instance->owned_bindings[i].function = name;
            name += length;
        }
        if(binding_count > 1)
            qsort(instance->owned_bindings, binding_count, sizeof(HostBinding), compare_bindings);
    }
    instance->set.bindings = instance->owned_bindings;
    if(!validate_bindings(bundle, &instance->set)) {
        BundleInstanceClose(instance);
        return NULL;
    }
    /* A host that installs the wasm provider runs bundles on it; its
     * rejection is an error, never a reason to start the engine. */
    if(ZxWasmPlatformActive() != NULL)
        instance->wasm = ZxWasmOpenExec(bundle->exec, find_binding, &instance->set, max_steps);
    else
        instance->exec = ExecInstanceOpen(bundle->exec, find_binding, &instance->set, max_steps);
    if(instance->exec == NULL && instance->wasm == NULL) {
        BundleInstanceClose(instance);
        return NULL;
    }
    return instance;
}

BundleInstance *
BundleInstantiate(const Bundle *bundle, const HostBinding *bindings, size_t binding_count)
{
    return instantiate_bundle(bundle, bindings, binding_count, 0);
}

BundleInstance *
BundleInstantiateLimited(const Bundle *bundle, const HostBinding *bindings,
                         size_t binding_count, int max_steps)
{
    return max_steps > 0 ? instantiate_bundle(bundle, bindings, binding_count, max_steps) : NULL;
}

int
BundleInstanceRun(BundleInstance *instance, long long *result, int *has_result)
{
    if(instance == NULL)
        return 0;
    if(instance->wasm != NULL)
        return ZxWasmRun(instance->wasm, result, has_result);
    return ExecInstanceRun(instance->exec, result, has_result);
}

void
BundleInstanceLimitSteps(BundleInstance *instance, int max_steps)
{
    if(instance == NULL)
        return;
    if(instance->wasm != NULL)
        ZxWasmLimitSteps(instance->wasm, max_steps);
    else
        ExecInstanceLimitSteps(instance->exec, max_steps);
}

void
BundleInstanceMatchFieldsByName(BundleInstance *instance, int enabled)
{
    if(instance == NULL)
        return;
    if(instance->wasm != NULL)
        ZxWasmMatchFieldsByName(instance->wasm, enabled);
    else
        ExecInstanceMatchFieldsByName(instance->exec, enabled);
}

void
BundleInstanceClose(BundleInstance *instance)
{
    if(instance == NULL)
        return;
    ExecInstanceClose(instance->exec);
    ZxWasmClose(instance->wasm);
    free(instance->owned_bindings);
    free(instance);
}

int
BundleRun(const Bundle *bundle, const HostBinding *bindings,
          size_t binding_count, long long *result, int *has_result)
{
    if(result == NULL || has_result == NULL)
        return 0;
    BundleInstance *instance = BundleInstantiate(bundle, bindings, binding_count);
    if(instance == NULL)
        return 0;
    int ok = BundleInstanceRun(instance, result, has_result);
    BundleInstanceClose(instance);
    return ok;
}

struct BundleState {
    BundleStateValue *values;
    size_t count;
    size_t bytes;
};

enum {
    BUNDLE_STATE_MAX_BYTES = 16 * 1024 * 1024,
    STATE_DEPTH = 128
};

typedef struct StateBudget {
    size_t used;
    int failed;
} StateBudget;

static void
state_budget_add(StateBudget *budget, size_t bytes)
{
    if(budget->used > BUNDLE_STATE_MAX_BYTES ||
       bytes > (size_t)BUNDLE_STATE_MAX_BYTES - budget->used)
        budget->failed = 1;
    else
        budget->used += bytes;
}

static void
release_owned_state_value(VmHostValue *value, int depth)
{
    if(depth >= STATE_DEPTH)
        return;
    if(value->kind == VM_HOST_ARRAY) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_owned_state_value(&value->elements[i], depth + 1);
        free(value->elements);
        value->elements = NULL;
    } else if(value->kind == VM_HOST_RECORD && value->fields != NULL) {
        VmHostField *fields = (VmHostField *)value->fields;
        for(size_t i = 0; i < value->field_count; i++)
            release_owned_state_value(&fields[i].value, depth + 1);
        for(size_t i = 0; i < value->field_count; i++)
            free((void *)fields[i].name);
        free(fields);
        value->fields = NULL;
    } else if(value->kind == VM_HOST_STRING)
        free((void *)value->data);
    value->data = NULL;
    free((void *)value->type);
    value->type = NULL;
}

static int
state_type_portable(const char *type)
{
    return type != NULL && type[0] != '*' && !SliceElementType(type, NULL, 0);
}

static const char *
clone_state_name(const char *name, StateBudget *budget)
{
    if(name == NULL)
        return NULL;
    size_t length = strlen(name);
    if(length == SIZE_MAX)
        return NULL;
    if(budget != NULL) {
        state_budget_add(budget, length + 1);
        if(budget->failed)
            return NULL;
    }
    char *copied = malloc(length + 1);
    if(copied == NULL)
        return NULL;
    memcpy(copied, name, length + 1);
    return copied;
}

static int
state_value_portable(const VmHostValue *value, int depth)
{
    if(depth >= STATE_DEPTH || !state_type_portable(value->type) ||
       value->kind == VM_HOST_POINTER || value->kind == VM_HOST_SLICE ||
       (value->kind == VM_HOST_STRING && value->data == NULL &&
        value->length != 0))
        return 0;
    if(value->kind == VM_HOST_ARRAY) {
        if(value->length != 0 && value->elements == NULL)
            return 0;
        for(size_t i = 0; i < value->length; i++)
            if(!state_value_portable(&value->elements[i], depth + 1))
                return 0;
        return 1;
    }
    if(value->kind == VM_HOST_RECORD) {
        if(value->field_count != 0 && value->fields == NULL)
            return 0;
        for(size_t i = 0; i < value->field_count; i++)
            if(!state_value_portable(&value->fields[i].value, depth + 1))
                return 0;
    }
    return 1;
}

static int
clone_state_value(const VmHostValue *input, VmHostValue *output,
                  int depth, StateBudget *budget)
{
    if(depth >= STATE_DEPTH || !state_type_portable(input->type) ||
       input->kind == VM_HOST_POINTER || input->kind == VM_HOST_SLICE ||
       (input->kind == VM_HOST_STRING && input->data == NULL &&
        input->length != 0))
        return 0;
    *output = *input;
    output->type = clone_state_name(input->type, budget);
    output->data = NULL;
    output->elements = NULL;
    output->fields = NULL;
    if(output->type == NULL)
        return 0;
    if(input->kind == VM_HOST_STRING) {
        if(input->length == 0) {
            output->data = NULL;
            return 1;
        }
        if(input->length == SIZE_MAX)
            goto fail;
        state_budget_add(budget, input->length + 1);
        if(budget->failed)
            goto fail;
        unsigned char *data = malloc(input->length + 1);
        if(data == NULL)
            goto fail;
        memcpy(data, input->data, input->length);
        data[input->length] = 0;
        output->data = data;
        return 1;
    }
    if(input->kind == VM_HOST_ARRAY) {
        if(input->length == 0) {
            output->elements = NULL;
            return 1;
        }
        if(input->elements == NULL ||
           input->length > SIZE_MAX / sizeof(*output->elements))
            goto fail;
        state_budget_add(budget, input->length * sizeof(*output->elements));
        if(budget->failed)
            goto fail;
        output->elements = calloc(input->length, sizeof(*output->elements));
        if(output->elements == NULL)
            goto fail;
        for(size_t i = 0; i < input->length; i++) {
            if(!clone_state_value(&input->elements[i], &output->elements[i],
                                  depth + 1, budget)) {
                goto fail;
            }
        }
        return 1;
    }
    if(input->kind != VM_HOST_RECORD)
        return 1;
    if(input->field_count == 0) {
        output->fields = NULL;
        return 1;
    }
    if(input->fields == NULL || input->field_count > SIZE_MAX / sizeof(VmHostField))
        goto fail;
    state_budget_add(budget, input->field_count * sizeof(VmHostField));
    if(budget->failed)
        goto fail;
    output->fields = calloc(input->field_count, sizeof(VmHostField));
    if(output->fields == NULL)
        goto fail;
    for(size_t i = 0; i < input->field_count; i++) {
        const char *name = clone_state_name(input->fields[i].name, budget);
        if(name == NULL) {
            goto fail;
        }
        ((VmHostField *)output->fields)[i].name = name;
        if(!clone_state_value(&input->fields[i].value,
                              (VmHostValue *)&output->fields[i].value,
                              depth + 1, budget)) {
            goto fail;
        }
    }
    return 1;
fail:
    release_owned_state_value(output, depth);
    return 0;
}

static void
state_close(BundleState *state)
{
    if(state != NULL) {
        for(size_t i = 0; i < state->count; i++) {
            release_owned_state_value(&state->values[i].value, 0);
            free((void *)state->values[i].module);
            free((void *)state->values[i].name);
            free((void *)state->values[i].type);
        }
        free(state->values);
        free(state);
    }
}

static int
clone_state_metadata(BundleStateValue *value, StateBudget *budget)
{
    const char *module = clone_state_name(value->module, budget);
    const char *name = clone_state_name(value->name, budget);
    const char *type = clone_state_name(value->type, budget);
    if(module == NULL || name == NULL || type == NULL) {
        free((void *)module);
        free((void *)name);
        free((void *)type);
        release_owned_state_value(&value->value, 0);
        return 0;
    }
    value->module = module;
    value->name = name;
    value->type = type;
    return 1;
}

/* Module globals the interface section names, on either engine. */
static size_t
state_count(BundleInstance *instance)
{
    return instance->wasm != NULL ? ZxWasmStateCount(instance->wasm) :
        ExecInstanceStateCount(instance->exec);
}

static int
state_at(BundleInstance *instance, size_t index, const char **module, const char **name,
         const char **type, VmHostValue *value)
{
    return instance->wasm != NULL ? ZxWasmStateAt(instance->wasm, index, module, name, type, value) :
        ExecInstanceStateAt(instance->exec, index, module, name, type, value);
}

static int
state_store(BundleInstance *instance, const VmHostValue *const *values)
{
    return instance->wasm != NULL ? ZxWasmStateStore(instance->wasm, values) :
        ExecInstanceStateStore(instance->exec, values);
}

int
BundleInstanceExportState(BundleInstance *instance, BundleState **state)
{
    if(state == NULL)
        return 0;
    *state = NULL;
    if(instance == NULL)
        return 0;
    size_t count = state_count(instance);
    BundleState *exported = calloc(1, sizeof(*exported));
    if(exported == NULL)
        return 0;
    exported->values = calloc(count ? count : 1, sizeof(*exported->values));
    if(exported->values == NULL) {
        free(exported);
        return 0;
    }
    StateBudget budget = {0};
    for(size_t i = 0; i < count; i++) {
        const char *module = "", *name = "", *type = "";
        VmHostValue borrowed = {0};
        if(!state_at(instance, i, &module, &name, &type, &borrowed) ||
           !state_type_portable(type) || !state_value_portable(&borrowed, 0)) {
            Diagnostic(Span("<host>", 1, 1), "zx.state",
                       "component state %s:%s is not owned portable state", module, name);
            state_close(exported);
            return 0;
        }
        BundleStateValue value = {.module = module, .name = name, .type = type};
        if(!clone_state_value(&borrowed, &value.value, 0, &budget)) {
            Diagnostic(Span("<host>", 1, 1), "zx.state",
                       "component state %s:%s exceeds portable state limits", module, name);
            state_close(exported);
            return 0;
        }
        if(!clone_state_metadata(&value, &budget)) {
            state_close(exported);
            return 0;
        }
        exported->values[exported->count++] = value;
    }
    exported->bytes = budget.used;
    *state = exported;
    return 1;
}

int
BundleInstanceImportState(BundleInstance *instance, const BundleState *state)
{
    if(instance == NULL || state == NULL)
        return 0;
    size_t count = state_count(instance);
    const VmHostValue **values = calloc(count ? count : 1, sizeof(*values));
    if(values == NULL)
        return 0;
    for(size_t i = 0; i < state->count; i++) {
        const BundleStateValue *value = &state->values[i];
        const char *module, *name, *type = NULL;
        size_t slot = count;
        for(size_t g = 0; g < count && slot == count; g++)
            if(state_at(instance, g, &module, &name, &type, NULL) &&
               strcmp(module, value->module) == 0 && strcmp(name, value->name) == 0)
                slot = g;
        if(slot == count || strcmp(type, value->type) != 0 || !state_type_portable(type) ||
           !state_value_portable(&value->value, 0)) {
            Diagnostic(Span("<host>", 1, 1), "zx.state",
                       slot == count ? "component state %s:%s is absent from the replacement" :
                       "component state %s:%s is incompatible with the replacement",
                       value->module, value->name);
            free(values);
            return 0;
        }
        values[slot] = &value->value;
    }
    int stored = state_store(instance, values);
    free(values);
    if(!stored)
        Diagnostic(Span("<host>", 1, 1), "zx.state", "component state is incompatible with the replacement");
    return stored;
}

size_t
BundleStateCount(const BundleState *state)
{
    return state != NULL ? state->count : 0;
}

size_t
BundleStateValueBytes(const BundleState *state)
{
    return state != NULL ? state->bytes : 0;
}

int
BundleStateValueAt(const BundleState *state, size_t index,
                   BundleStateValue *value)
{
    if(state == NULL || index >= state->count || value == NULL)
        return 0;
    *value = state->values[index];
    return 1;
}

BundleStateValue *
BundleStateValueRef(BundleState *state, size_t index)
{
    return state != NULL && index < state->count ? &state->values[index] : NULL;
}

int
BundleStateFind(const BundleState *state, const char *module,
                const char *name, size_t *index)
{
    if(state == NULL || module == NULL || name == NULL || index == NULL)
        return 0;
    for(size_t i = 0; i < state->count; i++)
        if(strcmp(state->values[i].module, module) == 0 &&
           strcmp(state->values[i].name, name) == 0) {
            *index = i;
            return 1;
        }
    return 0;
}

int
BundleStateSetInteger(BundleState *state, size_t index, int64_t value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_INTEGER)
        return 0;
    state->values[index].value.integer = value;
    state->values[index].value.bits = (uint64_t)value;
    return 1;
}

int
BundleStateSetUnsigned(BundleState *state, size_t index, uint64_t value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_UNSIGNED)
        return 0;
    state->values[index].value.bits = value;
    state->values[index].value.integer = (int64_t)value;
    return 1;
}

int
BundleStateSetReal(BundleState *state, size_t index, double value)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_REAL)
        return 0;
    state->values[index].value.real = value;
    return 1;
}

int
BundleStateSetString(BundleState *state, size_t index,
                     const unsigned char *data, size_t length)
{
    if(state == NULL || index >= state->count ||
       state->values[index].value.kind != VM_HOST_STRING ||
       (data == NULL && length != 0) || length == SIZE_MAX)
        return 0;
    unsigned char *copied = NULL;
    if(length != 0) {
        copied = malloc(length + 1);
        if(copied == NULL)
            return 0;
        memcpy(copied, data, length);
        copied[length] = 0;
    }
    free((void *)state->values[index].value.data);
    state->values[index].value.data = copied;
    state->values[index].value.length = length;
    return 1;
}

void
BundleStateClose(BundleState *state)
{
    state_close(state);
}

struct BundleComponent {
    Bundle *bundle;
    BundleInstance *instance;
    int max_steps;
    uint64_t generation;
    int closed;
    size_t references;
};

struct BundleComponentHandle {
    BundleComponent *component;
    uint64_t generation;
};

static void
component_release(BundleComponent *component)
{
    if(component != NULL && --component->references == 0)
        free(component);
}

static size_t
component_live_value_bytes(const BundleComponent *component)
{
    if(component == NULL || component->closed || component->instance == NULL)
        return 0;
    if(component->instance->wasm != NULL)
        return ZxWasmLiveBytes(component->instance->wasm);
    return ExecInstanceLiveBytes(component->instance->exec);
}

BundleComponent *
BundleComponentOpen(const Bundle *bundle, const HostBinding *bindings,
                    size_t binding_count, int max_steps)
{
    if(bundle == NULL || max_steps <= 0)
        return NULL;
    Bundle *retained = (Bundle *)bundle;
    if(retained->references == SIZE_MAX)
        return NULL;
    retained->references++;
    BundleComponent *component = calloc(1, sizeof(*component));
    if(component == NULL) {
        BundleClose(retained);
        return NULL;
    }
    component->bundle = retained;
    component->references = 1;
    component->max_steps = max_steps;
    component->instance = instantiate_bundle(retained, bindings, binding_count, max_steps);
    if(component->instance == NULL) {
        BundleClose(retained);
        free(component);
        return NULL;
    }
    return component;
}

BundleComponentHandle *
BundleComponentAcquire(BundleComponent *component)
{
    if(component == NULL || component->closed ||
       component->references == SIZE_MAX)
        return NULL;
    component->references++;
    BundleComponentHandle *handle = malloc(sizeof(*handle));
    if(handle == NULL) {
        component_release(component);
        return NULL;
    }
    *handle = (BundleComponentHandle){component, component->generation};
    return handle;
}

int
BundleComponentHandleValid(const BundleComponentHandle *handle)
{
    return handle != NULL && handle->component != NULL &&
           !handle->component->closed &&
           handle->generation == handle->component->generation;
}

int
BundleComponentRun(BundleComponentHandle *handle, long long *result,
                   int *has_result)
{
    return BundleComponentHandleValid(handle) &&
           BundleInstanceRun(handle->component->instance, result, has_result);
}

int
BundleComponentReplace(BundleComponent *component, const Bundle *replacement,
                       BundleStateMigrate migrate, BundleComponentVerify verify,
                       void *context, BundleComponentHandle **new_handle)
{
    if(component == NULL || component->closed || replacement == NULL ||
       !BundleAbiCompatible(component->bundle, replacement))
        return 0;
    if(new_handle != NULL)
        *new_handle = NULL;
    Bundle *retained = (Bundle *)replacement;
    if(retained->references == SIZE_MAX)
        return 0;
    BundleComponentHandle *handle = NULL;
    if(new_handle != NULL) {
        handle = malloc(sizeof(*handle));
        if(handle == NULL)
            return 0;
    }
    retained->references++;
    BundleState *old_state = NULL, *new_state = NULL;
    BundleInstance *candidate = instantiate_bundle(
        retained, component->instance->set.bindings,
        component->instance->set.count, component->max_steps);
    int ok = candidate != NULL &&
             BundleInstanceExportState(component->instance, &old_state) &&
             BundleInstanceExportState(candidate, &new_state) &&
             (migrate == NULL || migrate(context, old_state, new_state)) &&
             BundleInstanceImportState(candidate, new_state) &&
             (verify == NULL || verify(context, candidate));
    if(ok) {
        BundleInstanceClose(component->instance);
        BundleClose(component->bundle);
        component->bundle = retained;
        component->instance = candidate;
        component->generation++;
        if(new_handle != NULL) {
            component->references++;
            *handle = (BundleComponentHandle){component, component->generation};
            *new_handle = handle;
            handle = NULL;
        }
    } else {
        BundleInstanceClose(candidate);
        BundleClose(retained);
    }
    BundleStateClose(old_state);
    BundleStateClose(new_state);
    free(handle);
    return ok;
}

size_t
BundleComponentLiveValueBytes(const BundleComponent *component)
{
    return component_live_value_bytes(component);
}

void
BundleComponentClose(BundleComponent *component)
{
    if(component == NULL || component->closed)
        return;
    component->closed = 1;
    BundleInstanceClose(component->instance);
    component->instance = NULL;
    BundleClose(component->bundle);
    component->bundle = NULL;
    component_release(component);
}

void
BundleComponentHandleClose(BundleComponentHandle *handle)
{
    if(handle != NULL) {
        component_release(handle->component);
        free(handle);
    }
}
