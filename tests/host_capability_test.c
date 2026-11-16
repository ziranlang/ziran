#include "ziran_host.h"
#include "zir_host_exec.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int storage = 7;

static unsigned char *
read_file(const char *path, size_t *size)
{
    FILE *input = fopen(path, "rb");
    assert(input != NULL && fseek(input, 0, SEEK_END) == 0);
    long length = ftell(input);
    assert(length > 0 && fseek(input, 0, SEEK_SET) == 0);
    unsigned char *bytes = malloc((size_t)length);
    assert(bytes != NULL &&
           fread(bytes, 1, (size_t)length, input) == (size_t)length);
    fclose(input);
    *size = (size_t)length;
    return bytes;
}

/* The sum of a tree's values through every level of children. */
static long long
tree_sum(const HostValue *node)
{
    assert(node->kind == HOST_RECORD && strcmp(node->type, "tree.Node") == 0 && node->field_count == 2);
    const HostValue *children = &node->fields[1].value;
    assert(strcmp(node->fields[1].name, "children") == 0 && children->kind == HOST_SLICE);
    long long sum = node->fields[0].value.integer;
    for(size_t i = 0; i < children->length; i++) sum += tree_sum(&children->elements[i]);
    return sum;
}

static HostValue
tree_node(HostField fields[2], long long value, HostValue *children, size_t count)
{
    fields[0] = (HostField){"value", {.kind = HOST_INTEGER, .type = "s32", .integer = value,
                                        .bits = (uint64_t)value}};
    fields[1] = (HostField){"children", {.kind = HOST_SLICE, .type = "[]Node", .length = count,
                                           .elements = children}};
    return (HostValue){.kind = HOST_RECORD, .type = "Node", .fields = fields, .field_count = 2};
}

/* Gives back a tree with the given tree's sum at its root, over leaves 100
 * and 200, the second with a leaf 7 of its own. */
static int
grow_host(void *context, const char *module, const char *function,
          const HostValue *args, int arg_count, HostValue *result)
{
    static HostField root[2], first[2], second[2], inner[2];
    static HostValue leaves[2], below[1];
    (void)module; (void)function;
    (*(int *)context)++;
    if(arg_count != 1) return 0;
    below[0] = tree_node(inner, 7, NULL, 0);
    leaves[0] = tree_node(first, 100, NULL, 0);
    leaves[1] = tree_node(second, 200, below, 1);
    *result = tree_node(root, tree_sum(&args[0]), leaves, 2);
    return 1;
}

/* A 64-byte label for each index, in storage the next call reuses. */
static int
label_host(void *context, const char *module, const char *function,
           const HostValue *args, int arg_count, HostValue *result)
{
    static unsigned char text[64];
    (void)module; (void)function;
    if(arg_count != 1 || args[0].kind != HOST_INTEGER) return 0;
    (*(int *)context)++;
    memset(text, '.', sizeof(text));
    int written = snprintf((char *)text, sizeof(text), "label-%lld", (long long)args[0].integer);
    text[written] = '.';
    result->kind = HOST_STRING;
    result->data = text;
    result->length = sizeof(text);
    return 1;
}

/* The one capability of a program that reads through the handle it gets. */
static int
make_handle(void *context, const char *module, const char *function,
            const HostValue *args, int arg_count, HostValue *result)
{
    (void)module; (void)function; (void)args;
    (*(int *)context)++;
    if(arg_count != 0) return 0;
    result->kind = HOST_POINTER;
    result->pointer = &storage;
    return 1;
}

static int
host_call(void *context, const char *module, const char *function,
          const HostValue *args, int arg_count, HostValue *result)
{
    int *calls = context;
    if(strcmp(module, "host_api") != 0 &&
       strcmp(module, "handle_host") != 0 &&
       strcmp(module, "native_host") != 0)
        return 0;
    (*calls)++;
    if(strcmp(function, "AddTenHost") == 0 && arg_count == 1 &&
       args[0].kind == HOST_INTEGER &&
       strcmp(args[0].type, "s32") == 0) {
        result->kind = HOST_INTEGER;
        result->integer = args[0].integer + 10;
        return 1;
    }
    if(strcmp(function, "ByteCountHost") == 0 && arg_count == 1 &&
       args[0].kind == HOST_STRING &&
       args[0].length == 2 &&
       memcmp(args[0].data, "hi", 2) == 0) {
        result->kind = HOST_INTEGER;
        result->integer = (int64_t)args[0].length;
        return 1;
    }
    if(strcmp(function, "DoubleHost") == 0 && arg_count == 1 &&
       args[0].kind == HOST_REAL &&
       strcmp(args[0].type, "float32") == 0) {
        result->kind = HOST_REAL;
        result->real = args[0].real * 2.0;
        return 1;
    }
    if(strcmp(function, "EchoHost") == 0 && arg_count == 1 &&
       args[0].kind == HOST_UNSIGNED &&
       strcmp(args[0].type, "u32") == 0) {
        result->kind = HOST_UNSIGNED;
        result->bits = args[0].bits;
        return 1;
    }
    if(strcmp(function, "MakeHandleHost") == 0 && arg_count == 0) {
        result->kind = HOST_POINTER;
        result->type = "*s32";
        result->pointer = &storage;
        return 1;
    }
    if(strcmp(function, "ReadHandleHost") == 0 && arg_count == 1 &&
       args[0].kind == HOST_POINTER &&
       strcmp(args[0].type, "*s32") == 0) {
        result->kind = HOST_INTEGER;
        result->integer = args[0].pointer == NULL ? -1 : *(int *)args[0].pointer;
        return 1;
    }
    if(strcmp(function, "BorrowBoxHost") == 0 && arg_count == 0) {
        static HostField fields[2];
        static HostValue value, count;
        value.kind = HOST_POINTER;
        value.type = "*s32";
        value.pointer = &storage;
        count.kind = HOST_INTEGER;
        count.type = "s32";
        count.integer = 1;
        fields[0].name = "value";
        fields[0].value = value;
        fields[1].name = "count";
        fields[1].value = count;
        result->kind = HOST_RECORD;
        result->type = "Box";
        result->fields = fields;
        result->field_count = 2;
        return 1;
    }
    return 0;
}

static const HostBinding *
find_binding(void *context, const char *module, const char *function)
{
    const HostBinding *binding = context;
    return strcmp(binding->module, module) == 0 &&
           strcmp(binding->function, function) == 0 ? binding : NULL;
}

static int
mutate_native_slice(void *context, const char *module, const char *function,
                    const HostValue *args, int arg_count,
                    HostValue *result)
{
    int *calls = context;
    (void)module;
    assert(strcmp(function, "Mutate") == 0);
    assert(arg_count == 1 && args[0].kind == HOST_SLICE &&
           strcmp(args[0].type, "[]s32") == 0 && args[0].length == 2);
    assert(args[0].elements[0].integer == 1 &&
           args[0].elements[1].integer == 2);
    args[0].elements[0].integer = 40;
    args[0].elements[0].bits = 40;
    result->kind = HOST_INTEGER;
    result->type = "s32";
    result->integer = 2;
    (*calls)++;
    return 1;
}

typedef struct ReenterContext {
    BundleInstance *instance;
    int depth;
    int calls;
} ReenterContext;

static int
reenter_native(void *opaque, const char *module, const char *function,
               const HostValue *args, int arg_count, HostValue *result)
{
    ReenterContext *context = opaque;
    assert(strcmp(module, "native_reentrant") == 0 &&
           strcmp(function, "ReenterHost") == 0);
    assert(arg_count == 1 && args[0].kind == HOST_INTEGER);
    context->calls++;
    long long value = args[0].integer + 10;
    if(context->depth == 0) {
        int has_result = 0;
        context->depth = 1;
        assert(BundleInstanceRun(context->instance, &value, &has_result));
        assert(has_result);
        context->depth = 0;
    }
    result->kind = HOST_INTEGER;
    result->type = "s32";
    result->integer = value;
    return 1;
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
    assert(argc == 2 || (argc == 3 && (strcmp(argv[2], "dereference") == 0 ||
                                       strcmp(argv[2], "strings") == 0 ||
                                       strcmp(argv[2], "native") == 0 ||
                                       strcmp(argv[2], "native_reentrant") == 0 ||
                                       strcmp(argv[2], "native_bridge") == 0)) ||
           (argc == 4 && strcmp(argv[2], "tree") == 0));
    if(argc == 3 && strcmp(argv[2], "native_bridge") == 0) {
        size_t size = 0;
        unsigned char *bytes = read_file(argv[1], &size);
        ExecBundle *exec = ExecBundleOpen(bytes, size, argv[1]);
        free(bytes);
        assert(exec != NULL && ExecBundleCapabilityCount(exec) == 1);
        HostBinding binding = {ExecBundleCapabilityModule(exec, 0),
                               ExecBundleCapabilityFunction(exec, 0),
                               mutate_native_slice, &calls};
        ExecHosts *hosts = ExecHostsOpen(exec, find_binding, &binding);
        assert(hosts != NULL);
        int32_t data[2] = {1, 2};
        struct { int32_t *data; int64_t count; } slice = {data, 2};
        ExecHostNativeValue argument = {
            .type = ExecBundleCapabilityParameterTypeId(exec, 0, 0),
            .bytes = (unsigned char *)&slice,
            .size = sizeof(slice)
        };
        ExecHostNativeValue returned = {0};
        assert(ExecHostsCallNative(hosts, 0, &argument, 1, &returned));
        assert(calls == 1 && data[0] == 40 && data[1] == 2);
        assert(returned.type == ExecBundleCapabilityResultTypeId(exec, 0) &&
               returned.bits == 2);
        ExecHostsClose(hosts);
        ExecBundleClose(exec);
        return 0;
    }
    if(argc == 3 && strcmp(argv[2], "native") == 0) {
        Bundle *native_bundle = BundleOpen(argv[1]);
        assert(native_bundle != NULL && BundleCapabilityCount(native_bundle) == 2);
        HostBinding native_bindings[2];
        for(size_t index = 0; index < 2; index++)
            native_bindings[index] = (HostBinding){
                BundleCapabilityModule(native_bundle, index),
                BundleCapabilityFunction(native_bundle, index),
                host_call, &calls
            };
        assert(strcmp(BundleNativeProblem(native_bundle), "") == 0);
        BundleInstanceOptions options = {BUNDLE_BACKEND_NATIVE, 1000000};
        BundleInstance *native = BundleInstantiateWithOptions(
            native_bundle, native_bindings, 2, &options);
        assert(native != NULL && BundleInstanceBackend(native) ==
                                 BUNDLE_EXECUTION_NATIVE_X86_64);
        assert(BundleInstanceRun(native, &result, &has_result));
        assert(has_result && result == 42 && calls == 3);
        BundleInstanceClose(native);
        BundleClose(native_bundle);
        return 0;
    }
    if(argc == 3 && strcmp(argv[2], "native_reentrant") == 0) {
        Bundle *native_bundle = BundleOpen(argv[1]);
        assert(native_bundle != NULL && BundleCapabilityCount(native_bundle) == 1);
        ReenterContext context = {0};
        HostBinding binding = {
            BundleCapabilityModule(native_bundle, 0),
            BundleCapabilityFunction(native_bundle, 0),
            reenter_native, &context
        };
        BundleInstanceOptions options = {BUNDLE_BACKEND_NATIVE, 1000000};
        context.instance = BundleInstantiateWithOptions(
            native_bundle, &binding, 1, &options);
        assert(context.instance != NULL);
        assert(BundleInstanceRun(context.instance, &result, &has_result));
        assert(has_result && result == 42 && context.calls == 2 &&
               context.depth == 0);
        BundleInstanceClose(context.instance);
        BundleClose(native_bundle);
        return 0;
    }
    if(argc == 3 && strcmp(argv[2], "strings") == 0) {
        /* 100,100 host strings of 64 bytes would hold 6.4 MB. The ones the
         * program drops give their storage back; the ones it keeps stay. */
        Bundle *labels = BundleOpen(argv[1]);
        assert(labels != NULL && BundleCapabilityCount(labels) == 1);
        HostBinding label = {BundleCapabilityModule(labels, 0), BundleCapabilityFunction(labels, 0),
                             label_host, &calls};
        BundleComponent *component = BundleComponentOpen(labels, &label, 1, 2147483647);
        BundleComponentHandle *handle = BundleComponentAcquire(component);
        assert(component != NULL && handle != NULL);
        size_t most = 0;
        for(int frame = 0; frame < 100; frame++) {
            assert(BundleComponentRun(handle, &result, &has_result) && has_result && result == 64000);
            size_t live = BundleComponentLiveValueBytes(component);
            if(live > most) most = live;
        }
        assert(calls == 100100 && most < 512 * 1024);
        BundleComponentHandleClose(handle);
        BundleComponentClose(component);
        BundleClose(labels);
        return 0;
    }
    if(argc == 4 && strcmp(argv[2], "tree") == 0) {
        /* A type that holds itself through a slice crosses a host call both
         * ways. A value that holds itself fails the run before the host is
         * called. */
        int grown = 0;
        for(int file = 0; file < 2; file++) {
            Bundle *trees = BundleOpen(argv[file == 0 ? 1 : 3]);
            assert(trees != NULL && BundleCapabilityCount(trees) == 1);
            HostBinding grow = {BundleCapabilityModule(trees, 0), BundleCapabilityFunction(trees, 0),
                                grow_host, &grown};
            int ran = BundleRun(trees, &grow, 1, &result, &has_result);
            if(file == 0) assert(ran && has_result && result == 7 && grown == 1);
            else assert(!ran && grown == 1);
            BundleClose(trees);
        }
        return 0;
    }
    if(argc == 3) {
        /* A host's pointer is an opaque handle: reading through it fails
         * when the program runs. */
        Bundle *opaque = BundleOpen(argv[1]);
        assert(opaque != NULL && BundleCapabilityCount(opaque) == 1);
        HostBinding make = {BundleCapabilityModule(opaque, 0), BundleCapabilityFunction(opaque, 0),
                            make_handle, &calls};
        assert(!BundleRun(opaque, &make, 1, &result, &has_result) && calls == 1);
        BundleClose(opaque);
        return 0;
    }
    FILE *input = fopen(argv[1], "rb");
    assert(input != NULL && fseek(input, 0, SEEK_END) == 0);
    long size = ftell(input);
    assert(size > 0 && fseek(input, 0, SEEK_SET) == 0);
    unsigned char *bytes = malloc((size_t)size);
    assert(bytes != NULL && fread(bytes, 1, (size_t)size, input) == (size_t)size);
    fclose(input);
    assert(BundleOpenBytes(NULL, (size_t)size) == NULL);
    assert(BundleOpenBytes(bytes, 0) == NULL);
    assert(BundleOpenBytes(bytes, (size_t)size - 1) == NULL);
    Bundle *bundle = BundleOpenBytes(bytes, (size_t)size);
    assert(bundle != NULL);
    BundleClose(bundle);
    unsigned char *trailing = malloc((size_t)size + 1);
    assert(trailing != NULL);
    memcpy(trailing, bytes, (size_t)size);
    trailing[size] = 0;
    assert(BundleOpenBytes(trailing, (size_t)size + 1) == NULL);
    free(trailing);
    bundle = BundleOpenBytes(bytes, (size_t)size);
    free(bytes); /* The runtime does not retain the caller's input allocation. */
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
