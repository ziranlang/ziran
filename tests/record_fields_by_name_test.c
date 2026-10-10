#include "ziran_host.h"

#include <assert.h>
#include <string.h>

/* The host's Frame lists its fields in another order, adds one the bundle
 * does not declare and lacks "scale", as a host built against a newer or
 * older layout would. */
static int mistyped;
static VmHostField size_fields[1];
static VmHostField frame_fields[4];

static int
host_call(void *context, const char *module, const char *function,
          const VmHostValue *args, int arg_count, VmHostValue *result)
{
    (void)context;
    (void)args;
    assert(strcmp(module, "fields_host") == 0);
    assert(strcmp(function, "ReadFrameHost") == 0);
    assert(arg_count == 0);
    size_fields[0] = (VmHostField){"height", {.kind = VM_HOST_INTEGER,
        .type = "s32", .integer = 5}};
    frame_fields[0] = (VmHostField){"label", {.kind = VM_HOST_STRING,
        .type = "string", .data = (const unsigned char *)"ok", .length = 2}};
    frame_fields[1] = (VmHostField){"added", {.kind = VM_HOST_INTEGER,
        .type = "s32", .integer = 9}};
    frame_fields[2] = (VmHostField){"width", {.kind = VM_HOST_INTEGER,
        .type = "s32", .integer = 7}};
    if(mistyped)
        frame_fields[2].value = (VmHostValue){.kind = VM_HOST_STRING,
            .type = "string", .data = (const unsigned char *)"7", .length = 1};
    frame_fields[3] = (VmHostField){"size", {.kind = VM_HOST_RECORD,
        .type = "Size", .fields = size_fields, .field_count = 1}};
    result->kind = VM_HOST_RECORD;
    result->type = "Frame";
    result->fields = frame_fields;
    result->field_count = 4;
    return 1;
}

static int
run(Bundle *bundle, int by_name, long long *result)
{
    HostBinding binding[1] = {{"fields_host", "ReadFrameHost", host_call, NULL}};
    BundleInstance *instance = BundleInstantiate(bundle, binding, 1);
    assert(instance != NULL);
    BundleInstanceMatchFieldsByName(instance, by_name);
    int has_result = 0;
    int ok = BundleInstanceRun(instance, result, &has_result);
    BundleInstanceClose(instance);
    return ok && has_result;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    long long result = 0;
    /* By default a record must match its declared layout exactly. */
    assert(!run(bundle, 0, &result));
    /* By name, extra fields are ignored and missing ones are zero. */
    assert(run(bundle, 1, &result) && result == 42);
    /* A field of the wrong kind still fails, by name or not. */
    mistyped = 1;
    assert(!run(bundle, 1, &result));
    BundleClose(bundle);
    return 0;
}
