#include "ziran_host.h"

#include <assert.h>
#include <string.h>

/* The host's Frame lists its fields in another order, adds one the bundle
 * does not declare and lacks "scale", as a host built against a newer or
 * older layout would. */
static int mistyped;
static HostField size_fields[1];
static HostField frame_fields[4];

static int
host_call(void *context, const char *module, const char *function,
          const HostValue *args, int arg_count, HostValue *result)
{
    (void)context;
    (void)args;
    assert(strcmp(module, "fields_host") == 0);
    assert(strcmp(function, "ReadFrameHost") == 0);
    assert(arg_count == 0);
    size_fields[0] = (HostField){"height", {.kind = HOST_INTEGER,
        .type = "s32", .integer = 5}};
    frame_fields[0] = (HostField){"label", {.kind = HOST_STRING,
        .type = "string", .data = (const unsigned char *)"ok", .length = 2}};
    frame_fields[1] = (HostField){"added", {.kind = HOST_INTEGER,
        .type = "s32", .integer = 9}};
    frame_fields[2] = (HostField){"width", {.kind = HOST_INTEGER,
        .type = "s32", .integer = 7}};
    if(mistyped)
        frame_fields[2].value = (HostValue){.kind = HOST_STRING,
            .type = "string", .data = (const unsigned char *)"7", .length = 1};
    frame_fields[3] = (HostField){"size", {.kind = HOST_RECORD,
        .type = "Size", .fields = size_fields, .field_count = 1}};
    result->kind = HOST_RECORD;
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
