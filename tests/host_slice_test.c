#include "ziran_host.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int
fill(void *context, const char *module, const char *function,
     const VmHostValue *args, int arg_count, VmHostValue *result)
{
    int *calls = context;
    if(strcmp(module, "returned") == 0)
        return 0;
    assert(strcmp(module, "host_buffer") == 0);
    assert(arg_count == 1);
    if(strcmp(function, "ReplaceWord") == 0) {
        assert(args[0].kind == VM_HOST_SLICE);
        assert(strcmp(args[0].type, "[]string") == 0);
        assert(args[0].length == 2);
        assert(args[0].elements[0].kind == VM_HOST_STRING);
        assert(args[0].elements[0].length == 3);
        assert(memcmp(args[0].elements[0].data, "old", 3) == 0);
        assert(args[0].elements[1].length == 6);
        assert(memcmp(args[0].elements[1].data, "second", 6) == 0);
        (*calls)++;
        args[0].elements[0].data = (const unsigned char *)"new";
        args[0].elements[0].length = 3;
        result->kind = VM_HOST_INTEGER;
        result->integer = 2;
        return 1;
    }
    assert(strcmp(function, "Fill") == 0);
    assert(args[0].kind == VM_HOST_SLICE);
    assert(strcmp(args[0].type, "[]u8") == 0);
    (*calls)++;
    result->kind = VM_HOST_INTEGER;
    result->integer = (int64_t)args[0].length;
    if(args[0].length == 0)
        return 1;
    assert(args[0].length == 2);
    assert(args[0].elements[0].kind == VM_HOST_UNSIGNED);
    assert(strcmp(args[0].elements[0].type, "u8") == 0);
    assert(args[0].elements[0].bits == 1);
    assert(args[0].elements[1].bits == 2);
    args[0].elements[0].bits = 40;
    if(*calls == 7)
        args[0].elements[1].kind = VM_HOST_INTEGER;
    return 1;
}

static int view[3] = {10, 20, 30};

static int
return_view(void *context, const char *module, const char *function,
            const VmHostValue *args, int arg_count, VmHostValue *result)
{
    static VmHostValue elements[3];
    int mode = *(const int *)context;
    (void)args; (void)arg_count;
    if(strcmp(module, "returned") != 0 || strcmp(function, "MakeView") != 0)
        return 0;
    for(int i = 0; i < 3; i++) {
        elements[i].kind = VM_HOST_INTEGER;
        elements[i].type = "s32";
        elements[i].integer = view[i];
    }
    result->kind = VM_HOST_SLICE;
    result->type = "[]s32";
    result->elements = elements;
    result->length = 3;
    if(mode == 1) {
        result->elements = NULL;
        result->length = 0;
    } else if(mode == 2) {
#if SIZE_MAX > UINT_MAX
        result->length = (size_t)UINT_MAX + 1;
#else
        result->length = SIZE_MAX;
#endif
    }
    return 1;
}

static int
record_slice(void *context, const char *module, const char *function,
             const VmHostValue *args, int arg_count, VmHostValue *result)
{
    int bad_return = *(const int *)context;
    assert(strcmp(module, "record_slices") == 0);
    if(strcmp(function, "Mutate") == 0) {
        assert(arg_count == 1 && args[0].kind == VM_HOST_SLICE);
        assert(strcmp(args[0].type, "[]Point") == 0);
        assert(args[0].length == 2 && args[0].elements != NULL);
        assert(args[0].elements[0].kind == VM_HOST_RECORD);
        assert(strcmp(args[0].elements[0].type, "Point") == 0);
        assert(args[0].elements[0].field_count == 2);
        VmHostField *fields = (VmHostField *)args[0].elements[0].fields;
        assert(strcmp(fields[0].name, "x") == 0);
        assert(fields[0].value.kind == VM_HOST_INTEGER);
        assert(fields[0].value.integer == 1);
        fields[0].value.integer = 40;
        result->kind = VM_HOST_INTEGER;
        result->type = "s32";
        result->integer = 2;
        return 1;
    }
    assert(strcmp(function, "Make") == 0 && arg_count == 0);
    static VmHostField fields[2][2];
    static VmHostValue elements[2];
    for(int i = 0; i < 2; i++) {
        for(int j = 0; j < 2; j++) {
            fields[i][j].name = j == 0 ? "x" : "y";
            fields[i][j].value.kind = VM_HOST_INTEGER;
            fields[i][j].value.type = "s32";
            fields[i][j].value.integer = i == 0 && j == 0 ? 1 :
                                         i == 1 && j == 1 ? 2 : 0;
        }
        elements[i].kind = VM_HOST_RECORD;
        elements[i].type = "Point";
        elements[i].fields = fields[i];
        elements[i].field_count = 2;
    }
    if(bad_return)
        fields[1][1].name = "wrong";
    result->kind = VM_HOST_SLICE;
    result->type = "[]Point";
    result->elements = elements;
    result->length = 2;
    return 1;
}

static int
named_records(void *context, const char *module, const char *function,
              const VmHostValue *args, int arg_count, VmHostValue *result)
{
    int *calls = context;
    assert(strcmp(module, "bridge") == 0);
    assert(strcmp(function, "SliceHost") == 0 ||
           strcmp(function, "ArrayHost") == 0);
    assert(arg_count == 1 && args[0].length == 2 &&
           args[0].elements != NULL);
    assert(args[0].kind == (strcmp(function, "SliceHost") == 0 ?
                            VM_HOST_SLICE : VM_HOST_ARRAY));
    assert(strcmp(args[0].type, strcmp(function, "SliceHost") == 0 ?
                                      "[]Types.Point" : "[2]Types.Point") == 0);
    for(size_t i = 0; i < args[0].length; i++) {
        assert(args[0].elements[i].kind == VM_HOST_RECORD);
        assert(strcmp(args[0].elements[i].type, "Types.Point") == 0);
    }
    VmHostField *first = (VmHostField *)args[0].elements[0].fields;
    VmHostField *second = (VmHostField *)args[0].elements[1].fields;
    assert(first[0].value.integer == 40 && second[1].value.integer == 2);
    (*calls)++;
    result->kind = VM_HOST_INTEGER;
    result->type = "s32";
    result->integer = 42;
    return 1;
}

int
main(int argc, char **argv)
{
    long long result = 0;
    int has_result = 0, calls = 0;
    assert(argc == 2 ||
           (argc == 3 && (strcmp(argv[2], "overlap") == 0 ||
                          strcmp(argv[2], "return_view") == 0 ||
                          strcmp(argv[2], "return_empty") == 0 ||
                          strcmp(argv[2], "return_huge") == 0 ||
                          strcmp(argv[2], "records") == 0 ||
                          strcmp(argv[2], "records_bad") == 0 ||
                          strcmp(argv[2], "named_records") == 0)));
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    if(argc == 3 && strcmp(argv[2], "named_records") == 0) {
        assert(BundleCapabilityCount(bundle) == 2);
        HostBinding bindings[2] = {
            {"bridge", "SliceHost", named_records, &calls},
            {"bridge", "ArrayHost", named_records, &calls}
        };
        assert(BundleRun(bundle, bindings, 2, &result, &has_result));
        assert(has_result && result == 42 && calls == 2);
        BundleClose(bundle);
        return 0;
    }
    if(argc == 3 && strncmp(argv[2], "records", 7) == 0) {
        int bad_return = strcmp(argv[2], "records_bad") == 0;
        assert(BundleCapabilityCount(bundle) == 2);
        HostBinding bindings[2] = {
            {"record_slices", "Mutate", record_slice, &bad_return},
            {"record_slices", "Make", record_slice, &bad_return}
        };
        int ran = BundleRun(bundle, bindings, 2, &result, &has_result);
        if(bad_return) assert(!ran);
        else assert(ran && has_result && result == 42);
        BundleClose(bundle);
        return 0;
    }
    if(argc == 3 && strncmp(argv[2], "return_", 7) == 0) {
        int mode = strcmp(argv[2], "return_empty") == 0 ? 1 :
                   strcmp(argv[2], "return_huge") == 0 ? 2 : 0;
        assert(BundleCapabilityCount(bundle) == 1);
        HostBinding bindings[1] = {{"returned", "MakeView",
                                    return_view, &mode}};
        int ran = BundleRun(bundle, bindings, 1, &result, &has_result);
        if(mode == 2)
            assert(!ran);
        else
            assert(ran && has_result && result == (mode == 1 ? -1 : 60));
        BundleClose(bundle);
        return 0;
    }
    if(argc == 3) {
        assert(BundleCapabilityCount(bundle) == 1);
        HostBinding overlap = {"overlap", "Touch", fill, &calls};
        assert(!BundleRun(bundle, &overlap, 1, &result, &has_result));
        assert(calls == 0);
        BundleClose(bundle);
        return 0;
    }
    assert(BundleCapabilityCount(bundle) == 2);
    HostBinding bindings[2] = {
        {"host_buffer", "Fill", fill, &calls},
        {"host_buffer", "ReplaceWord", fill, &calls}
    };
    if(!BundleRun(bundle, bindings, 2, &result, &has_result)) {
        fprintf(stderr, "slice host run failed after %d calls\n", calls);
        return 1;
    }
    assert(has_result && result == 42 && calls == 4);
    assert(!BundleRun(bundle, bindings, 2, &result, &has_result));
    assert(calls == 7);
    BundleClose(bundle);
    return 0;
}
