#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Both records must be visible together so a field-width or padding mismatch
 * cannot be hidden by compiling each side in a separate translation unit. */
#define HostBinding RuntimeHostBinding
#include "ziran_host.h"
#undef HostBinding
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

size_t BundleAssetCount(const Bundle *bundle)
{
    assert(bundle == (Bundle *)&token);
    return SIZE_MAX;
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
    assert(BundleCapabilityModuleName(bundle, expected_index) ==
           (uint8_t *)module_name);
    assert(BundleCapabilityFunctionName(bundle, expected_index) ==
           (uint8_t *)function_name);
    assert(BundleAssetPath(bundle, expected_index) == (uint8_t *)asset_name);
    assert(BundleAssetBytes(bundle, expected_index) == &token);
    assert(BundleAssetLength(bundle, expected_index) == (uint64_t)SIZE_MAX);
    void *instance = InstantiateBundle(bundle, NULL, expected_count);
    assert(instance == &token);
    int64_t result = 0;
    int32_t has_result = 0;
    assert(RunBundleInstance(instance, &result, &has_result) == 1);
    assert(result == 42 && has_result == 1);
    LimitBundleSteps(instance, 19);
    CloseBundleInstance(instance);
    CloseBundle(bundle);

    if (sizeof(size_t) < sizeof(uint64_t)) {
        uint64_t too_large = (uint64_t)SIZE_MAX + 1;
        int previous_calls = calls;
        assert(OpenBundleBytes(&token, too_large) == NULL);
        assert(OpenBundleWithLibraries(&token, NULL, too_large) == NULL);
        assert(OpenBundleBytesWithLibraries(&token, too_large, NULL, expected_count) == NULL);
        assert(OpenBundleBytesWithLibraries(&token, expected_size, NULL, too_large) == NULL);
        assert(BundleCapabilityModuleName(bundle, too_large) == NULL);
        assert(BundleCapabilityFunctionName(bundle, too_large) == NULL);
        assert(BundleAssetPath(bundle, too_large) == NULL);
        assert(BundleAssetBytes(bundle, too_large) == NULL);
        assert(BundleAssetLength(bundle, too_large) == 0);
        assert(InstantiateBundle(bundle, NULL, too_large) == NULL);
        assert(calls == previous_calls);
    }
    printf("Bundle host ABI layout, calls and narrowing passed (%zu-bit size_t)\n",
           sizeof(size_t) * 8);
    return 0;
}
