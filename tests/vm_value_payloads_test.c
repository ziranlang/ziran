#include "ziran_host.h"
#include <assert.h>
#include <string.h>

static int handle;
static int calls, records;
static HostField returned[2][6];
static const char *names[] = {"number", "wide", "real", "text", "tone", "handle"};
static const char *types[] = {"s64", "u64", "float64", "string", "Tone", "*void"};

/* The public host ABI keeps independent fields. A compact VM payload must
 * not expose the bits of a float/string as an unrelated number or pointer. */
static void
check_unused(const HostValue *value)
{
    if(value->kind != HOST_INTEGER && value->kind != HOST_UNSIGNED &&
       value->kind != HOST_POINTER)
        assert(value->integer == 0 && value->bits == 0);
    if(value->kind != HOST_REAL) assert(value->real == 0.0);
    if(value->kind != HOST_STRING) assert(value->data == NULL);
    if(value->kind != HOST_STRING && value->kind != HOST_SLICE)
        assert(value->length == 0);
    if(value->kind != HOST_RECORD)
        assert(value->fields == NULL && value->field_count == 0);
    if(value->kind != HOST_SLICE) assert(value->elements == NULL);
    if(value->kind != HOST_POINTER) assert(value->pointer == NULL);
}

static void
check_scalars(const HostValue *values, int row)
{
    for(int i = 0; i < 6; i++) {
        assert(strcmp(values[i].type, types[i]) == 0);
        check_unused(&values[i]);
    }
    assert(values[0].kind == HOST_INTEGER);
    assert(values[0].integer == (row ? 23 : -17));
    assert(values[0].bits == (uint64_t)values[0].integer);
    assert(values[1].kind == HOST_UNSIGNED && values[1].integer == 0);
    assert(values[1].bits == (row ? UINT64_C(9223372036854775808) : UINT64_MAX));
    assert(values[2].kind == HOST_REAL && values[2].real == (row ? -2.5 : 1.25));
    assert(values[3].kind == HOST_STRING && values[3].length == (row ? 4 : 7));
    assert(memcmp(values[3].data, row ? "tail" : "payload", values[3].length) == 0);
    assert(values[4].kind == HOST_INTEGER && values[4].integer == (row ? 0 : 2));
    assert(values[5].kind == HOST_POINTER && values[5].pointer == &handle);
}

static int
host_call(void *context, const char *module, const char *function,
          const HostValue *args, int count, HostValue *result)
{
    (void)context;
    assert(strcmp(module, "vm_value_payloads_test") == 0);
    calls++;
    if(strcmp(function, "Handle") == 0) {
        assert(count == 0);
        result->kind = HOST_POINTER;
        result->pointer = &handle;
    } else if(strcmp(function, "RoundTrip") == 0) {
        assert(count == 6 && records < 2);
        check_scalars(args, records);
        HostField *fields = returned[records++];
        for(int i = 0; i < 6; i++)
            fields[i] = (HostField){names[i], args[i]};
        result->kind = HOST_RECORD;
        result->fields = fields;
        result->field_count = 6;
    } else {
        assert(strcmp(function, "Inspect") == 0 && count == 1);
        assert(args[0].kind == HOST_SLICE && args[0].length == 2);
        check_unused(&args[0]);
        for(int row = 0; row < 2; row++) {
            HostValue *record = &args[0].elements[row];
            assert(record->kind == HOST_RECORD && record->field_count == 6);
            check_unused(record);
            HostValue scalars[6];
            for(int i = 0; i < 6; i++) {
                assert(strcmp(record->fields[i].name, names[i]) == 0);
                scalars[i] = record->fields[i].value;
            }
            check_scalars(scalars, row);
        }
        HostField *fields = (HostField *)args[0].elements[0].fields;
        fields[1].value.bits = 9;
        fields[2].value.real = 3.5;
        fields[3].value.data = (const unsigned char *)"changed";
        fields[3].value.length = 7;
        result->kind = HOST_INTEGER;
        result->integer = 42;
    }
    return 1;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    HostBinding bindings[] = {
        {"vm_value_payloads_test", "Handle", host_call, NULL},
        {"vm_value_payloads_test", "RoundTrip", host_call, NULL},
        {"vm_value_payloads_test", "Inspect", host_call, NULL}
    };
    for(int repeat = 0; repeat < 2; repeat++) {
        calls = records = 0;
        long long result = 0;
        int present = 0;
        assert(BundleRun(bundle, bindings, 3, &result, &present));
        assert(present && result == 42 && calls == 4 && records == 2);
    }
    BundleClose(bundle);
    return 0;
}
