#include "ziran_host.h"

#include <assert.h>
#include <string.h>

static HostField fields[3];

static HostValue
string_value(const char *text)
{
    HostValue value = {0};
    value.kind = HOST_STRING;
    value.type = "string";
    value.data = (const unsigned char *)text;
    value.length = strlen(text);
    return value;
}

static HostValue
integer_value(const char *type, long long number)
{
    HostValue value = {0};
    value.kind = HOST_INTEGER;
    value.type = type;
    value.integer = number;
    return value;
}

static int
host_call(void *context, const char *module, const char *function,
          const HostValue *args, int count, HostValue *result)
{
    int *calls = context;
    assert(strcmp(module, "process") == 0);
    if(strcmp(function, "StartHost") == 0) {
        assert(count == 2 && args[0].kind == HOST_RECORD);
        assert(args[1].kind == HOST_SLICE && args[1].length == 1);
        assert(args[0].fields[5].value.integer == 1);
        assert(args[1].elements[0].length == 3);
        assert(memcmp(args[1].elements[0].data, "arg", 3) == 0);
        fields[0] = (HostField){"handle", integer_value("s32", 7)};
        fields[1] = (HostField){"error", string_value("")};
        result->kind = HOST_RECORD;
        result->fields = fields;
        result->field_count = 2;
    } else if(strcmp(function, "NextLineHost") == 0) {
        assert(count == 1 && args[0].integer == 7);
        fields[0] = (HostField){"text", string_value("ready")};
        fields[1] = (HostField){"eof", integer_value("bool", 0)};
        fields[2] = (HostField){"error", string_value("")};
        result->kind = HOST_RECORD;
        result->fields = fields;
        result->field_count = 3;
    } else if(strcmp(function, "WaitHost") == 0) {
        assert(count == 1 && args[0].integer == 7);
        fields[0] = (HostField){"code", integer_value("s32", 0)};
        fields[1] = (HostField){"stderr", string_value("")};
        fields[2] = (HostField){"error", string_value("")};
        result->kind = HOST_RECORD;
        result->fields = fields;
        result->field_count = 3;
    } else {
        return 0;
    }
    (*calls)++;
    return 1;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    HostBinding bindings[] = {
        {"process", "StartHost", host_call, NULL},
        {"process", "NextLineHost", host_call, NULL},
        {"process", "WaitHost", host_call, NULL},
    };
    int calls = 0;
    for(int i = 0; i < 3; i++)
        bindings[i].context = &calls;
    long long value = 0;
    int has_result = 0;
    assert(BundleRun(bundle, bindings, 3, &value, &has_result));
    assert(has_result && value == 42 && calls == 3);
    BundleClose(bundle);
    return 0;
}
