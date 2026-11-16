#include "ziran_host.h"

#include <assert.h>
#include <string.h>

static int malformed;
static int calls;
static HostField point_fields[2];
static HostField packet_fields[3];

static int
sum_slice(void *context, const char *module, const char *function,
          const HostValue *args, int arg_count, HostValue *result)
{
    int64_t total = 0;
    (void)context;
    assert(strcmp(module, "record_host") == 0);
    assert(strcmp(function, "SumSliceHost") == 0);
    assert(arg_count == 1);
    assert(args[0].kind == HOST_SLICE);
    assert(strcmp(args[0].type, "[]shapes.Point") == 0);
    assert(args[0].length == 2);
    for(size_t i = 0; i < args[0].length; i++) {
        const HostValue *point = &args[0].elements[i];
        assert(point->kind == HOST_RECORD);
        assert(strcmp(point->type, "shapes.Point") == 0);
        assert(point->field_count == 2);
        assert(strcmp(point->fields[0].name, "x") == 0);
        total += point->fields[0].value.integer +
                 point->fields[1].value.integer;
    }
    result->kind = HOST_INTEGER;
    result->type = "s32";
    result->integer = total;
    return 1;
}

static int
host_call(void *context, const char *module, const char *function,
          const HostValue *args, int arg_count, HostValue *result)
{
    (void)context;
    if(strcmp(function, "SumSliceHost") == 0) {
        calls++;
        return sum_slice(context, module, function, args, arg_count, result);
    }
    if(strcmp(function, "ObserveHost") == 0) {
        assert(strcmp(module, "record_host") == 0);
        assert(arg_count == 2 && args[0].kind == HOST_RECORD);
        assert(strcmp(args[0].type, "shapes.State") == 0 && args[0].field_count == 1);
        const HostValue *values = &args[0].fields[0].value;
        assert(values->kind == HOST_ARRAY && values->length == 2);
        assert(values->elements[0].integer == 10 && args[1].integer == 1);
        calls++;
        result->kind = HOST_INTEGER;
        result->type = "s32";
        result->integer = values->elements[0].integer * 10 + args[1].integer;
        return 1;
    }
    assert(strcmp(module, "record_host") == 0);
    assert(strcmp(function, "TransformHost") == 0);
    assert(arg_count == 1);
    assert(args[0].kind == HOST_RECORD);
    assert(strcmp(args[0].type, "shapes.Packet") == 0);
    assert(args[0].field_count == 3);
    assert(strcmp(args[0].fields[0].name, "point") == 0);
    assert(args[0].fields[0].value.kind == HOST_RECORD);
    assert(strcmp(args[0].fields[0].value.type, "shapes.Point") == 0);
    assert(args[0].fields[0].value.field_count == 2);
    assert(args[0].fields[0].value.fields[0].value.integer == 40);
    assert(args[0].fields[0].value.fields[1].value.integer == 1);
    assert(strcmp(args[0].fields[1].name, "label") == 0);
    assert(args[0].fields[1].value.kind == HOST_STRING);
    assert(args[0].fields[1].value.length == 2);
    assert(memcmp(args[0].fields[1].value.data, "in", 2) == 0);
    assert(strcmp(args[0].fields[2].name, "tone") == 0);
    assert(strcmp(args[0].fields[2].value.type, "shapes.Tone") == 0);
    assert(args[0].fields[2].value.integer == 0);
    calls++;

    point_fields[0] = (HostField){"x", {.kind = HOST_INTEGER,
        .type = "s32", .integer = 41}};
    point_fields[1] = (HostField){"y", {.kind = HOST_INTEGER,
        .type = "s32", .integer = 1}};
    if(malformed == 2)
        point_fields[0].value.type = "u32";
    packet_fields[0] = (HostField){malformed == 1 ? "wrong" : "point",
        {.kind = HOST_RECORD, .type = "shapes.Point", .fields = point_fields,
         .field_count = 2}};
    packet_fields[1] = (HostField){"label",
        {.kind = HOST_STRING, .type = "string",
         .data = (const unsigned char *)"ok", .length = 2}};
    packet_fields[2] = (HostField){"tone",
        {.kind = HOST_INTEGER, .type = "shapes.Tone", .integer = 1}};
    if(malformed == 3)
        packet_fields[2].value.kind = HOST_REAL;
    result->kind = HOST_RECORD;
    result->fields = packet_fields;
    result->field_count = 3;
    return 1;
}

/* The same bindings on the strict native backend: a record result with a
 * nested record and a string, a slice argument and a record holding an
 * array cross the native host ABI exactly as on the engine. */
static int
run_native(const Bundle *bundle, const HostBinding *binding, long long *result)
{
    BundleInstanceOptions native = {BUNDLE_BACKEND_NATIVE, 0};
    BundleInstance *instance = BundleInstantiateWithOptions(bundle, binding, 3, &native);
    assert(instance != NULL);
    assert(BundleInstanceBackend(instance) == BUNDLE_EXECUTION_NATIVE_X86_64);
    int has_result = 0;
    int ok = BundleInstanceRun(instance, result, &has_result);
    assert(!ok || has_result);
    BundleInstanceClose(instance);
    return ok;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 3);
    assert(strcmp(BundleCapabilityModule(bundle, 0), "record_host") == 0);
    assert(strcmp(BundleCapabilityFunction(bundle, 0), "TransformHost") == 0);
    HostBinding binding[3] = {
        {"record_host", "TransformHost", host_call, NULL},
        {"record_host", "SumSliceHost", host_call, NULL},
        {"record_host", "ObserveHost", host_call, NULL}
    };
    long long result = 0;
    int has_result = 0;
    assert(BundleRun(bundle, binding, 3, &result, &has_result));
    assert(has_result && result == 42 && calls == 3);
    malformed = 1;
    assert(!BundleRun(bundle, binding, 3, &result, &has_result));
    malformed = 2;
    assert(!BundleRun(bundle, binding, 3, &result, &has_result));
    malformed = 3;
    assert(!BundleRun(bundle, binding, 3, &result, &has_result));
    malformed = 0;
    assert(BundleRun(bundle, binding, 3, &result, &has_result));
    assert(has_result && result == 42 && calls == 9);
#if defined(__x86_64__) && defined(__linux__)
    assert(strcmp(BundleNativeProblem(bundle), "") == 0);
    result = 0;
    assert(run_native(bundle, binding, &result) && result == 42 && calls == 12);
    for(malformed = 1; malformed <= 3; malformed++)
        assert(!run_native(bundle, binding, &result));
    malformed = 0;
    assert(run_native(bundle, binding, &result) && result == 42);
#endif
    BundleClose(bundle);
    return 0;
}
