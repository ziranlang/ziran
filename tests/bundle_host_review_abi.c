#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Both records must be visible together so a field-width or padding mismatch
 * cannot be hidden by compiling each side in a separate translation unit. */
#define HostBinding RuntimeHostBinding
#define BundleStateValue RuntimeBundleStateValue
#define BundleCapabilityAbi RuntimeBundleCapabilityAbi
#define BundleStateMigrate RuntimeBundleStateMigrate
#define BundleComponentVerify RuntimeBundleComponentVerify
#include "ziran_host.h"
#undef HostBinding
#undef BundleStateValue
#undef BundleCapabilityAbi
#undef BundleStateMigrate
#undef BundleComponentVerify
#include "bundle_host.h"

#define SAME_FIELD(generated, runtime, left, right) \
    _Static_assert(offsetof(generated, left) == offsetof(runtime, right), \
                   "bundle host field offset: " #left); \
    _Static_assert(sizeof(((generated *)0)->left) == \
                   sizeof(((runtime *)0)->right), \
                   "bundle host field width: " #left)

_Static_assert(sizeof(HostValue) == sizeof(VmHostValue), "host value size");
_Static_assert(_Alignof(HostValue) == _Alignof(VmHostValue), "host value alignment");
SAME_FIELD(HostValue, VmHostValue, kind, kind);
SAME_FIELD(HostValue, VmHostValue, type_name, type);
SAME_FIELD(HostValue, VmHostValue, integer, integer);
SAME_FIELD(HostValue, VmHostValue, bits, bits);
SAME_FIELD(HostValue, VmHostValue, real, real);
SAME_FIELD(HostValue, VmHostValue, data, data);
SAME_FIELD(HostValue, VmHostValue, length, length);
SAME_FIELD(HostValue, VmHostValue, fields, fields);
SAME_FIELD(HostValue, VmHostValue, field_count, field_count);
SAME_FIELD(HostValue, VmHostValue, elements, elements);
SAME_FIELD(HostValue, VmHostValue, pointer, pointer);
_Static_assert(sizeof(HostField) == sizeof(VmHostField), "host field size");
SAME_FIELD(HostField, VmHostField, name, name);
SAME_FIELD(HostField, VmHostField, value, value);
_Static_assert(sizeof(HostBinding) == sizeof(RuntimeHostBinding), "binding size");
SAME_FIELD(HostBinding, RuntimeHostBinding, module, module);
SAME_FIELD(HostBinding, RuntimeHostBinding, function, function);
SAME_FIELD(HostBinding, RuntimeHostBinding, call, call);
SAME_FIELD(HostBinding, RuntimeHostBinding, context, context);
_Static_assert(sizeof(BundleStateValue) == sizeof(RuntimeBundleStateValue),
               "state value size");
_Static_assert(_Alignof(BundleStateValue) == _Alignof(RuntimeBundleStateValue),
               "state value alignment");
SAME_FIELD(BundleStateValue, RuntimeBundleStateValue, module, module);
SAME_FIELD(BundleStateValue, RuntimeBundleStateValue, name, name);
SAME_FIELD(BundleStateValue, RuntimeBundleStateValue, type, type);
SAME_FIELD(BundleStateValue, RuntimeBundleStateValue, value, value);
_Static_assert(sizeof(BundleCapabilityAbi) == sizeof(RuntimeBundleCapabilityAbi),
               "capability ABI size");
_Static_assert(_Alignof(BundleCapabilityAbi) ==
               _Alignof(RuntimeBundleCapabilityAbi), "capability ABI alignment");
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi, module, module);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi, function, function);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi, signature, signature);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi, arguments, arguments);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi, return_type, return_type);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi,
           variable_arguments, variable_arguments);
SAME_FIELD(BundleCapabilityAbi, RuntimeBundleCapabilityAbi,
           reads_arguments, reads_arguments);
_Static_assert(sizeof(BundleStateMigrate) == sizeof(RuntimeBundleStateMigrate),
               "state migration callback size");
_Static_assert(sizeof(BundleComponentVerify) ==
               sizeof(RuntimeBundleComponentVerify), "component verify callback size");

/* These implementations use the real runtime header's signatures. Linking
 * the generated wrappers against them exercises raw parameter/return widths
 * as well as checked narrowing of the public u64 inputs. */
static unsigned char token;
static size_t expected_size;
static size_t expected_index;
static size_t expected_count;
static int calls;
static const char module_name[] = "sample";
static const char function_name[] = "Read";
static const char asset_name[] = "assets/data";
static const char signature_text[] = "(value: s64) -> s64";
static const char arguments_text[] = "value: s64";
static const char return_text[] = "s64";
static const char state_module_name[] = "component";
static const char state_name[] = "state";
static const char state_type[] = "State";
static const unsigned char state_bytes[] = {1, 2, 3, 4};
static int64_t captured_integer;
static uint64_t captured_unsigned;
static double captured_real;
static int closed_state;
static int closed_component;
static int closed_component_handle;

Bundle *BundleOpen(const char *path)
{
    assert(path == (const char *)&token);
    calls++;
    return (Bundle *)&token;
}

Bundle *BundleOpenBytes(const unsigned char *data, size_t size)
{
    assert(data == &token && size == expected_size);
    calls++;
    return (Bundle *)&token;
}

Bundle *BundleOpenWithLibraries(const char *path, Bundle *const *libraries, size_t count)
{
    assert(path == (const char *)&token && libraries == NULL && count == expected_count);
    calls++;
    return (Bundle *)&token;
}

Bundle *BundleOpenBytesWithLibraries(const unsigned char *data, size_t size,
                                    Bundle *const *libraries, size_t count)
{
    assert(data == &token && size == expected_size && libraries == NULL && count == expected_count);
    calls++;
    return (Bundle *)&token;
}

void BundleClose(Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
}

size_t BundleCapabilityCount(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    return SIZE_MAX;
}

const char *BundleCapabilityModule(const Bundle *bundle, size_t index)
{
    assert(bundle == (Bundle *)&token && index == expected_index);
    calls++;
    return module_name;
}

const char *BundleCapabilityFunction(const Bundle *bundle, size_t index)
{
    assert(bundle == (Bundle *)&token && index == expected_index);
    calls++;
    return function_name;
}

int BundleCapabilityAbiAt(const Bundle *bundle, size_t index,
                          RuntimeBundleCapabilityAbi *abi)
{
    assert(bundle == (Bundle *)&token && index == expected_index && abi != NULL);
    calls++;
    *abi = (RuntimeBundleCapabilityAbi){
        .module = module_name,
        .function = function_name,
        .signature = signature_text,
        .arguments = arguments_text,
        .return_type = return_text,
        .variable_arguments = 0,
        .reads_arguments = 1
    };
    return 1;
}

int BundleAbiCompatible(const Bundle *left, const Bundle *right)
{
    assert(left == (Bundle *)&token && right == (Bundle *)&token);
    calls++;
    return 1;
}

const char *BundleEntryModuleName(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    calls++;
    return module_name;
}

const char *BundleEntryFunctionName(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    calls++;
    return function_name;
}

void BundleCodeIdentity(const Bundle *bundle, unsigned char identity[32])
{
    assert(bundle == (Bundle *)&token && identity != NULL);
    calls++;
    for(size_t i = 0; i < 32; i++)
        identity[i] = (unsigned char)i;
}

size_t BundleAssetCount(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    return SIZE_MAX;
}

size_t BundleOwnAssetCount(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    return SIZE_MAX - 1;
}

const char *BundleAssetName(const Bundle *bundle, size_t index)
{
    assert(bundle == (Bundle *)&token && index == expected_index);
    calls++;
    return asset_name;
}

const unsigned char *BundleAssetData(const Bundle *bundle, size_t index)
{
    assert(bundle == (Bundle *)&token && index == expected_index);
    calls++;
    return &token;
}

size_t BundleAssetSize(const Bundle *bundle, size_t index)
{
    assert(bundle == (Bundle *)&token && index == expected_index);
    calls++;
    return SIZE_MAX;
}

BundleInstance *BundleInstantiate(const Bundle *bundle,
                                 const RuntimeHostBinding *bindings,
                                 size_t count)
{
    assert(bundle == (Bundle *)&token);
    assert(bindings == NULL && count == expected_count);
    calls++;
    return (BundleInstance *)&token;
}

BundleInstance *BundleInstantiateLimited(const Bundle *bundle,
                                        const RuntimeHostBinding *bindings,
                                        size_t count, int max_steps)
{
    assert(max_steps == 19);
    return BundleInstantiate(bundle, bindings, count);
}

BundleInstance *BundleInstantiatePortable(const Bundle *bundle,
                                          const RuntimeHostBinding *bindings,
                                          size_t count)
{
    return BundleInstantiate(bundle, bindings, count);
}

int BundleInstanceRun(BundleInstance *instance, long long *result, int *has_result)
{
    assert(instance == (BundleInstance *)&token);
    *result = 42;
    *has_result = 1;
    return 1;
}

void BundleInstanceClose(BundleInstance *instance)
{
    assert(instance == (BundleInstance *)&token);
}

void BundleInstanceLimitSteps(BundleInstance *instance, int max_steps)
{
    assert(instance == (BundleInstance *)&token && max_steps == 19);
}

static int matched_by_name;

void BundleInstanceMatchFieldsByName(BundleInstance *instance, int enabled)
{
    assert(instance == (BundleInstance *)&token && (enabled == 0 || enabled == 1));
    matched_by_name = enabled;
}

int BundleInstanceExportState(BundleInstance *instance, BundleState **state)
{
    assert(instance == (BundleInstance *)&token && state != NULL);
    calls++;
    *state = (BundleState *)&token;
    return 1;
}

int BundleInstanceImportState(BundleInstance *instance, const BundleState *state)
{
    assert(instance == (BundleInstance *)&token && state == (BundleState *)&token);
    calls++;
    return 1;
}

size_t BundleStateCount(const BundleState *state)
{
    assert(state == (BundleState *)&token);
    return SIZE_MAX;
}

size_t BundleStateValueBytes(const BundleState *state)
{
    assert(state == (BundleState *)&token);
    calls++;
    return SIZE_MAX - 1;
}

int BundleStateValueAt(const BundleState *state, size_t index,
                       RuntimeBundleStateValue *value)
{
    assert(state == (BundleState *)&token && index == expected_index &&
           value != NULL);
    calls++;
    *value = (RuntimeBundleStateValue){
        .module = state_module_name,
        .name = state_name,
        .type = state_type,
        .value = {.kind = VM_HOST_INTEGER, .type = "s64", .integer = 9}
    };
    return 1;
}

RuntimeBundleStateValue *
BundleStateValueRef(BundleState *state, size_t index)
{
    static RuntimeBundleStateValue value;
    assert(state == (BundleState *)&token && index == expected_index);
    calls++;
    value = (RuntimeBundleStateValue){
        .module = state_module_name,
        .name = state_name,
        .type = state_type,
        .value = {.kind = VM_HOST_STRING, .type = "string",
                  .data = (unsigned char *)state_bytes, .length = 4}
    };
    return &value;
}

int BundleStateFind(const BundleState *state, const char *module,
                    const char *name, size_t *index)
{
    assert(state == (BundleState *)&token && module == module_name &&
           name == state_name && index != NULL);
    calls++;
    *index = expected_index;
    return 1;
}

int BundleStateSetInteger(BundleState *state, size_t index, int64_t value)
{
    assert(state == (BundleState *)&token && index == expected_index);
    captured_integer = value;
    return 1;
}

int BundleStateSetUnsigned(BundleState *state, size_t index, uint64_t value)
{
    assert(state == (BundleState *)&token && index == expected_index);
    captured_unsigned = value;
    return 1;
}

int BundleStateSetReal(BundleState *state, size_t index, double value)
{
    assert(state == (BundleState *)&token && index == expected_index);
    captured_real = value;
    return 1;
}

int BundleStateSetString(BundleState *state, size_t index,
                         const unsigned char *data, size_t length)
{
    assert(state == (BundleState *)&token && index == expected_index &&
           data == state_bytes && length == sizeof(state_bytes));
    calls++;
    return 1;
}

void BundleStateClose(BundleState *state)
{
    assert(state == (BundleState *)&token);
    closed_state = 1;
}

BundleComponent *BundleComponentOpen(const Bundle *bundle,
                                     const RuntimeHostBinding *bindings,
                                     size_t count, int max_steps)
{
    assert(bundle == (Bundle *)&token && bindings == NULL &&
           count == expected_count && max_steps == 19);
    calls++;
    return (BundleComponent *)&token;
}

BundleComponentHandle *BundleComponentAcquire(BundleComponent *component)
{
    assert(component == (BundleComponent *)&token);
    calls++;
    return (BundleComponentHandle *)&token;
}

int BundleComponentHandleValid(const BundleComponentHandle *handle)
{
    assert(handle == (BundleComponentHandle *)&token);
    return 1;
}

int BundleComponentRun(BundleComponentHandle *handle, long long *result,
                       int *has_result)
{
    assert(handle == (BundleComponentHandle *)&token);
    *result = 43;
    *has_result = 1;
    return 1;
}

static int review_migrate(void *context, void *old_state, void *new_state)
{
    assert(context == &token && old_state == &token && new_state == &token);
    return 1;
}

static int review_verify(void *context, void *instance)
{
    assert(context == &token && instance == &token);
    return 1;
}

int BundleComponentReplace(BundleComponent *component,
                           const Bundle *replacement,
                           RuntimeBundleStateMigrate migrate,
                           RuntimeBundleComponentVerify verify, void *context,
                           BundleComponentHandle **new_handle)
{
    assert(component == (BundleComponent *)&token &&
           replacement == (Bundle *)&token &&
           migrate == (RuntimeBundleStateMigrate)review_migrate &&
           verify == (RuntimeBundleComponentVerify)review_verify &&
           context == &token && new_handle != NULL);
    calls++;
    assert(migrate(context, (BundleState *)&token, (BundleState *)&token));
    assert(verify(context, (BundleInstance *)&token));
    *new_handle = (BundleComponentHandle *)&token;
    return 1;
}

size_t BundleComponentLiveValueBytes(const BundleComponent *component)
{
    assert(component == (BundleComponent *)&token);
    calls++;
    return SIZE_MAX - 2;
}

void BundleComponentClose(BundleComponent *component)
{
    assert(component == (BundleComponent *)&token);
    closed_component = 1;
}

void BundleComponentHandleClose(BundleComponentHandle *handle)
{
    assert(handle == (BundleComponentHandle *)&token);
    closed_component_handle = 1;
}

int main(void)
{
    expected_size = 37;
    expected_index = 17;
    expected_count = 3;
    void *bundle = OpenBundle(&token);
    assert(bundle == &token);
    assert(OpenBundleBytes(&token, expected_size) == &token);
    assert(OpenBundleWithLibraries(&token, NULL, expected_count) == &token);
    assert(OpenBundleBytesWithLibraries(&token, expected_size, NULL, expected_count) == &token);
    assert(BundleCapabilities(bundle) == (uint64_t)SIZE_MAX);
    assert(BundleAssets(bundle) == (uint64_t)SIZE_MAX);
    assert(BundleOwnAssets(bundle) == (uint64_t)SIZE_MAX - 1);
    assert(BundleCapabilityModuleName(bundle, expected_index) ==
           (uint8_t *)module_name);
    assert(BundleCapabilityFunctionName(bundle, expected_index) ==
           (uint8_t *)function_name);
    assert(BundleAssetPath(bundle, expected_index) == (uint8_t *)asset_name);
    assert(BundleAssetBytes(bundle, expected_index) == &token);
    assert(BundleAssetLength(bundle, expected_index) == (uint64_t)SIZE_MAX);
    void *instance = InstantiateBundle(bundle, NULL, expected_count);
    assert(instance == &token);
    assert(InstantiateBundleLimited(bundle, NULL, expected_count, 19) == &token);
    assert(InstantiateBundleLimited(bundle, NULL, expected_count, 0) == NULL);
    assert(InstantiateBundleLimited(bundle, NULL, expected_count, -1) == NULL);
    assert(InstantiateBundlePortable(bundle, NULL, expected_count) == &token);
    int64_t result = 0;
    int32_t has_result = 0;
    assert(RunBundleInstance(instance, &result, &has_result) == 1);
    assert(result == 42 && has_result == 1);
    LimitBundleSteps(instance, 19);
    MatchBundleFieldsByName(instance, true);
    assert(matched_by_name == 1);
    MatchBundleFieldsByName(instance, false);
    assert(matched_by_name == 0);
    BundleCapabilityAbi capability_abi;
    assert(ReadBundleCapabilityAbi(bundle, expected_index, &capability_abi) == 1);
    assert(capability_abi.module == (uint8_t *)module_name &&
           capability_abi.function == (uint8_t *)function_name &&
           capability_abi.signature == (uint8_t *)signature_text &&
           capability_abi.arguments == (uint8_t *)arguments_text &&
           capability_abi.return_type == (uint8_t *)return_text &&
           capability_abi.variable_arguments == 0 &&
           capability_abi.reads_arguments == 1);
    assert(BundlesAbiCompatible(bundle, bundle));
    assert(BundleEntryModule(bundle) == (uint8_t *)module_name);
    assert(BundleEntryFunction(bundle) == (uint8_t *)function_name);
    uint8_t identity[32];
    BundleCodeDigest(bundle, identity);
    for(size_t i = 0; i < sizeof(identity); i++)
        assert(identity[i] == (uint8_t)i);
    void *state = NULL;
    assert(ExportBundleState(instance, &state));
    assert(state == &token);
    assert(ImportBundleState(instance, state));
    assert(BundleStateValueCount(state) == (uint64_t)SIZE_MAX);
    assert(BundleStateBytes(state) == (uint64_t)SIZE_MAX - 1);
    BundleStateValue state_value;
    assert(ReadBundleStateValue(state, expected_index, &state_value));
    assert(state_value.module == (uint8_t *)state_module_name &&
           state_value.name == (uint8_t *)state_name &&
           state_value.type == (uint8_t *)state_type &&
           state_value.value.kind == VM_HOST_INTEGER &&
           state_value.value.integer == 9);
    BundleStateValue *state_reference =
        BundleStateValueReference(state, expected_index);
    assert(state_reference != NULL && state_reference->module != NULL);
    uint64_t state_index = 0;
    assert(FindBundleStateValue(state, (uint8_t *)module_name,
                                (uint8_t *)state_name, &state_index));
    assert(state_index == (uint64_t)expected_index);
    assert(SetBundleStateInteger(state, expected_index, 9));
    assert(captured_integer == 9);
    assert(SetBundleStateUnsigned(state, expected_index, UINT64_MAX));
    assert(captured_unsigned == UINT64_MAX);
    assert(SetBundleStateReal(state, expected_index, 0.5));
    assert(captured_real == 0.5);
    assert(SetBundleStateString(state, expected_index,
                                (uint8_t *)state_bytes, sizeof(state_bytes)));
    void *component = OpenBundleComponent(bundle, NULL, expected_count, 19);
    assert(component == &token);
    assert(OpenBundleComponent(bundle, NULL, expected_count, 0) == NULL);
    assert(OpenBundleComponent(bundle, NULL, expected_count, -1) == NULL);
    void *component_handle = AcquireBundleComponent(component);
    assert(component_handle == &token);
    assert(BundleComponentHandleIsValid(component_handle));
    int64_t component_result = 0;
    int32_t component_has_result = 0;
    assert(RunBundleComponent(component_handle, &component_result,
                              &component_has_result) == 1);
    assert(component_result == 43 && component_has_result == 1);
    void *replacement_handle = NULL;
    assert(ReplaceBundleComponent(component, bundle, review_migrate,
                                  review_verify, &token, &replacement_handle));
    assert(replacement_handle == &token);
    assert(BundleComponentLiveBytes(component) == (uint64_t)SIZE_MAX - 2);
    CloseBundleInstance(instance);
    CloseBundleState(state);
    CloseBundleComponentHandle(component_handle);
    CloseBundleComponentHandle(replacement_handle);
    CloseBundleComponent(component);
    CloseBundle(bundle);
    assert(closed_state && closed_component && closed_component_handle);

    if (sizeof(size_t) < sizeof(uint64_t)) {
        uint64_t too_large = (uint64_t)SIZE_MAX + 1;
        int previous_calls = calls;
        assert(OpenBundleBytes(&token, too_large) == NULL);
        assert(OpenBundleWithLibraries(&token, NULL, too_large) == NULL);
        assert(OpenBundleBytesWithLibraries(&token, too_large, NULL, expected_count) == NULL);
        assert(OpenBundleBytesWithLibraries(&token, expected_size, NULL, too_large) == NULL);
        assert(BundleCapabilityModuleName(bundle, too_large) == NULL);
        assert(BundleCapabilityFunctionName(bundle, too_large) == NULL);
        assert(ReadBundleCapabilityAbi(bundle, too_large, &capability_abi) == 0);
        assert(ReadBundleStateValue(state, too_large, &state_value) == 0);
        assert(BundleStateValueReference(state, too_large) == NULL);
        assert(SetBundleStateInteger(state, too_large, 9) == 0);
        assert(SetBundleStateUnsigned(state, too_large, 9) == 0);
        assert(SetBundleStateReal(state, too_large, 0.5) == 0);
        assert(SetBundleStateString(state, too_large,
                                    (uint8_t *)state_bytes,
                                    sizeof(state_bytes)) == 0);
        assert(BundleAssetPath(bundle, too_large) == NULL);
        assert(BundleAssetBytes(bundle, too_large) == NULL);
        assert(BundleAssetLength(bundle, too_large) == 0);
        assert(InstantiateBundle(bundle, NULL, too_large) == NULL);
        assert(InstantiateBundleLimited(bundle, NULL, too_large, 19) == NULL);
        assert(InstantiateBundlePortable(bundle, NULL, too_large) == NULL);
        assert(OpenBundleComponent(bundle, NULL, too_large, 19) == NULL);
        assert(calls == previous_calls);
    }
    printf("Bundle host ABI layout, calls and narrowing passed (%zu-bit size_t)\n",
           sizeof(size_t) * 8);
    return 0;
}
