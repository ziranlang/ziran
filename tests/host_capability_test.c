#include "ziran_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int storage = 7;

static int
host_call(void *context, const char *module, const char *function,
          const VmHostValue *args, int arg_count, VmHostValue *result)
{
    int *calls = context;
    if(strcmp(module, "host_api") != 0 && strcmp(module, "handle_host") != 0)
        return 0;
    (*calls)++;
    if(strcmp(function, "AddTenHost") == 0 && arg_count == 1 &&
       args[0].kind == VM_HOST_INTEGER &&
       strcmp(args[0].type, "s32") == 0) {
        result->kind = VM_HOST_INTEGER;
        result->integer = args[0].integer + 10;
        return 1;
    }
    if(strcmp(function, "ByteCountHost") == 0 && arg_count == 1 &&
       args[0].kind == VM_HOST_STRING &&
       args[0].length == 2 &&
       memcmp(args[0].data, "hi", 2) == 0) {
        result->kind = VM_HOST_INTEGER;
        result->integer = (int64_t)args[0].length;
        return 1;
    }
    if(strcmp(function, "DoubleHost") == 0 && arg_count == 1 &&
       args[0].kind == VM_HOST_REAL &&
       strcmp(args[0].type, "float32") == 0) {
        result->kind = VM_HOST_REAL;
        result->real = args[0].real * 2.0;
        return 1;
    }
    if(strcmp(function, "EchoHost") == 0 && arg_count == 1 &&
       args[0].kind == VM_HOST_UNSIGNED &&
       strcmp(args[0].type, "u32") == 0) {
        result->kind = VM_HOST_UNSIGNED;
        result->bits = args[0].bits;
        return 1;
    }
    if(strcmp(function, "MakeHandleHost") == 0 && arg_count == 0) {
        result->kind = VM_HOST_POINTER;
        result->type = "*s32";
        result->pointer = &storage;
        return 1;
    }
    if(strcmp(function, "ReadHandleHost") == 0 && arg_count == 1 &&
       args[0].kind == VM_HOST_POINTER &&
       strcmp(args[0].type, "*s32") == 0) {
        result->kind = VM_HOST_INTEGER;
        result->integer = args[0].pointer == NULL ? -1 : *(int *)args[0].pointer;
        return 1;
    }
    if(strcmp(function, "BorrowBoxHost") == 0 && arg_count == 0) {
        static VmHostField fields[2];
        static VmHostValue value, count;
        value.kind = VM_HOST_POINTER;
        value.type = "*s32";
        value.pointer = &storage;
        count.kind = VM_HOST_INTEGER;
        count.type = "s32";
        count.integer = 1;
        fields[0].name = "value";
        fields[0].value = value;
        fields[1].name = "count";
        fields[1].value = count;
        result->kind = VM_HOST_RECORD;
        result->type = "Box";
        result->fields = fields;
        result->field_count = 2;
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    long long result = 0;
    int has_result = 0, calls = 0;
    static const struct { const char *module, *function; } names[] = {
        {"host_api", "AddTenHost"},
        {"host_api", "ByteCountHost"},
        {"host_api", "DoubleHost"},
        {"host_api", "EchoHost"},
        {"handle_host", "MakeHandleHost"},
        {"handle_host", "ReadHandleHost"},
        {"handle_host", "BorrowBoxHost"},
    };
    HostBinding bindings[7];
    int bound[7] = {0};
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 7);
    for(size_t i = 0; i < 7; i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *function = BundleCapabilityFunction(bundle, i);
        int match = -1;
        assert(module != NULL && function != NULL);
        for(size_t j = 0; j < 7; j++)
            if(!bound[j] && strcmp(module, names[j].module) == 0 &&
               strcmp(function, names[j].function) == 0) {
                match = (int)j;
                break;
            }
        assert(match >= 0);
        bound[match] = 1;
        bindings[match] = (HostBinding){module, function, host_call, &calls};
    }
    assert(BundleCapabilityModule(bundle, 7) == NULL);
    assert(BundleCapabilityFunction(bundle, 7) == NULL);
    assert(!BundleRun(bundle, bindings, 6, &result, &has_result));
    assert(calls == 0);
    assert(BundleRun(bundle, bindings, 7, &result, &has_result));
    assert(has_result && result == 42 && calls == 9);
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    BundleInstanceLimitSteps(instance, 1);
    assert(!BundleInstanceRun(instance, &result, &has_result));
    BundleInstanceClose(instance);
    calls = 0;
    instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    BundleInstanceLimitSteps(instance, 1000000);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 42 && calls == 9);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
