#define _POSIX_C_SOURCE 200809L
#include "ziran_host.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int adjust(void *context, const char *module, const char *function,
                  const VmHostValue *arguments, int count, VmHostValue *result)
{
    (void)context; (void)module; (void)function;
    if(count != 1 || arguments[0].kind != VM_HOST_INTEGER)
        return 0;
    result->kind = VM_HOST_INTEGER;
    result->type = "s64";
    result->integer = arguments[0].integer + 1;
    return 1;
}

static double milliseconds(void)
{
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC, &now))
        abort();
    return now.tv_sec * 1000.0 + now.tv_nsec / 1e6;
}

static int state_find(const BundleState *state, const char *name, size_t *index)
{
    for(size_t i = 0; i < BundleStateCount(state); i++) {
        BundleStateValue value;
        if(BundleStateValueAt(state, i, &value) &&
           strcmp(value.name, name) == 0)
            return (*index = i, 1);
    }
    return 0;
}

static VmHostValue *record_field(VmHostValue *record, const char *name)
{
    if(record == NULL || record->kind != VM_HOST_RECORD)
        return NULL;
    VmHostField *fields = (VmHostField *)record->fields;
    for(size_t i = 0; i < record->field_count; i++)
        if(fields[i].name != NULL && strcmp(fields[i].name, name) == 0)
            return &fields[i].value;
    return NULL;
}

static VmHostValue *state_field(BundleStateValue *value, const char *name)
{
    return value == NULL ? NULL : record_field(&value->value, name);
}

static int copy_owned_string(VmHostValue *target, const VmHostValue *source)
{
    if(target == NULL || source == NULL || target->kind != VM_HOST_STRING ||
       source->kind != VM_HOST_STRING)
        return 0;
    unsigned char *copied = NULL;
    if(source->length != 0) {
        copied = malloc(source->length + 1);
        if(copied == NULL)
            return 0;
        memcpy(copied, source->data, source->length);
        copied[source->length] = 0;
    }
    free((void *)target->data);
    target->data = copied;
    target->length = source->length;
    return 1;
}

static int migrate(void *context, const BundleState *old_state,
                   BundleState *new_state)
{
    (void)context;
    size_t old_index = 0, new_index = 0;
    BundleStateValue old_value;
    BundleStateValue *new_value;
    VmHostValue *old_count, *new_count, *old_first, *new_first;
    VmHostValue *old_history, *new_history, *new_open;
    int ok = state_find(old_state, "state", &old_index) &&
             state_find(new_state, "state", &new_index) &&
             BundleStateValueAt(old_state, old_index, &old_value) &&
             (new_value = BundleStateValueRef(new_state, new_index)) != NULL &&
             (old_count = state_field((BundleStateValue *)&old_value, "count")) != NULL &&
             (new_count = state_field(new_value, "count")) != NULL &&
             (old_first = state_field((BundleStateValue *)&old_value, "first")) != NULL &&
             (new_first = state_field(new_value, "first")) != NULL &&
             (old_history = state_field((BundleStateValue *)&old_value, "history")) != NULL &&
             (new_history = state_field(new_value, "history")) != NULL &&
             (new_open = state_field(new_value, "open")) != NULL;
    if(!ok)
        return 0;
    new_count->integer = old_count->integer + 100;
    new_count->bits = (uint64_t)new_count->integer;
    VmHostValue *old_title = record_field(old_first, "title");
    VmHostValue *new_title = record_field(new_first, "title");
    VmHostValue *old_weight = record_field(old_first, "weight");
    VmHostValue *new_weight = record_field(new_first, "weight");
    ok = old_title != NULL && new_title != NULL &&
         old_weight != NULL && new_weight != NULL &&
         copy_owned_string(new_title, old_title) &&
         old_weight->kind == VM_HOST_INTEGER &&
         new_weight->kind == VM_HOST_INTEGER &&
         (new_weight->integer = old_weight->integer,
          new_weight->bits = old_weight->bits, 1) &&
         old_history->kind == VM_HOST_ARRAY &&
         new_history->kind == VM_HOST_ARRAY &&
         new_history->length == old_history->length;
    for(size_t i = 0; ok && i < old_history->length; i++)
        ok = old_history->elements[i].kind == VM_HOST_INTEGER &&
             new_history->elements[i].kind == VM_HOST_INTEGER &&
             (new_history->elements[i].integer =
                  old_history->elements[i].integer,
              new_history->elements[i].bits =
                  old_history->elements[i].bits, 1);
    new_open->kind = VM_HOST_INTEGER;
    new_open->integer = 1;
    new_open->bits = 1;
    return ok;
}

static int reject_migration(void *context, const BundleState *old_state,
                            BundleState *new_state)
{
    (void)context; (void)old_state; (void)new_state;
    return 0;
}

static int reject_verification(void *context, BundleInstance *instance)
{
    (void)context; (void)instance;
    return 0;
}

static int accept_schema(void *context, const BundleState *old_state,
                         BundleState *new_state)
{
    (void)context; (void)old_state; (void)new_state;
    return 1;
}

static int run_handle(BundleComponentHandle *handle, long long expected)
{
    long long result = -1;
    int present = 0;
    return BundleComponentRun(handle, &result, &present) && present &&
           result == expected;
}

int main(int argc, char **argv)
{
    if(argc != 8)
        return 2;
    double begin = milliseconds();
    Bundle *old = BundleOpen(argv[1]);
    double old_load = milliseconds() - begin;
    begin = milliseconds();
    Bundle *replacement = BundleOpen(argv[2]);
    double new_load = milliseconds() - begin;
    Bundle *wrong_abi = BundleOpen(argv[3]);
    Bundle *init_failure = BundleOpen(argv[4]);
    Bundle *schema_failure = BundleOpen(argv[5]);
    Bundle *resource_failure = BundleOpen(argv[6]);
    Bundle *unrelated_bundle = BundleOpen(argv[7]);
    if(old == NULL || replacement == NULL || wrong_abi == NULL ||
       init_failure == NULL || schema_failure == NULL ||
       resource_failure == NULL || unrelated_bundle == NULL)
        return 3;

    unsigned char old_identity[32], new_identity[32];
    BundleCodeIdentity(old, old_identity);
    BundleCodeIdentity(replacement, new_identity);
    if(memcmp(old_identity, new_identity, sizeof(old_identity)) == 0)
        return 4;
    if(strcmp(BundleEntryModuleName(old), "component") != 0 ||
       strcmp(BundleEntryFunctionName(old), "Next") != 0 ||
       !BundleAbiCompatible(old, replacement) ||
       BundleAbiCompatible(old, wrong_abi))
        return 5;
    BundleCapabilityAbi capability;
    int found = 0;
    for(size_t i = 0; i < BundleCapabilityCount(old); i++) {
        if(BundleCapabilityAbiAt(old, i, &capability) &&
           strcmp(capability.module, "component") == 0 &&
           strcmp(capability.function, "Adjust") == 0) {
            found = capability.arguments[0] != '\0' &&
                    strcmp(capability.return_type, "s64") == 0;
            break;
        }
    }
    if(!found)
        return 6;

    HostBinding bindings[] = {
        {.module = "component", .function = "Adjust", .call = adjust}
    };
    begin = milliseconds();
    BundleComponent *component = BundleComponentOpen(
        old, bindings, 1, 10000);
    double instantiate = milliseconds() - begin;
    BundleComponent *unrelated = BundleComponentOpen(
        unrelated_bundle, NULL, 0, 10000);
    if(component == NULL || unrelated == NULL)
        return 7;
    BundleComponentHandle *handle = BundleComponentAcquire(component);
    BundleComponentHandle *unrelated_handle = BundleComponentAcquire(unrelated);
    if(handle == NULL || unrelated_handle == NULL ||
       !BundleComponentHandleValid(handle) || !run_handle(handle, 89) ||
       !run_handle(handle, 99) || !run_handle(unrelated_handle, 101))
        return 8;

    begin = milliseconds();
    BundleComponentHandle *replacement_handle = NULL;
    if(!BundleComponentReplace(component, replacement, migrate, NULL, NULL,
                               &replacement_handle))
        return 9;
    double swap = milliseconds() - begin;
    if(BundleComponentHandleValid(handle) ||
       BundleComponentRun(handle, &(long long){0}, &(int){0}) ||
       !BundleComponentHandleValid(replacement_handle) ||
       !run_handle(replacement_handle, 2109) ||
       !run_handle(unrelated_handle, 102))
        return 10;

    BundleComponentHandle *current = BundleComponentAcquire(component);
    if(BundleComponentReplace(component, replacement, reject_migration,
                              NULL, NULL, NULL) ||
       !BundleComponentHandleValid(current) || !run_handle(current, 2119))
        return 11;
    if(BundleComponentReplace(component, replacement, migrate,
                              reject_verification, NULL, NULL) ||
       !BundleComponentHandleValid(current) || !run_handle(current, 2129))
        return 12;
    if(BundleComponentReplace(component, schema_failure, accept_schema,
                              NULL, NULL, NULL) ||
       !BundleComponentHandleValid(current) || !run_handle(current, 2139))
        return 13;
    if(BundleComponentReplace(component, resource_failure, migrate,
                              NULL, NULL, NULL) ||
       !BundleComponentHandleValid(current) || !run_handle(current, 2149))
        return 14;

    BundleComponent *bounded = BundleComponentOpen(old, bindings, 1, 64);
    BundleComponentHandle *bounded_handle = BundleComponentAcquire(bounded);
    if(bounded == NULL || bounded_handle == NULL ||
       !run_handle(bounded_handle, 89))
        return 15;
    if(BundleComponentReplace(bounded, init_failure, migrate, NULL, NULL, NULL) ||
       !BundleComponentHandleValid(bounded_handle) ||
       !run_handle(bounded_handle, 99))
        return 16;

    size_t live_bytes = BundleComponentLiveValueBytes(component);
    BundleComponentHandleClose(handle);
    BundleComponentHandleClose(replacement_handle);
    BundleComponentHandleClose(current);
    BundleComponentHandleClose(bounded_handle);
    BundleComponentClose(component);
    BundleComponentHandleClose(unrelated_handle);
    BundleComponentClose(unrelated);
    BundleComponentClose(bounded);
    BundleClose(old); BundleClose(replacement); BundleClose(wrong_abi);
    BundleClose(init_failure); BundleClose(schema_failure);
    BundleClose(resource_failure); BundleClose(unrelated_bundle);
    printf("bundle-component load_old_ms=%.6f load_new_ms=%.6f "
           "instantiate_ms=%.6f swap_ms=%.6f live_value_bytes=%zu\n",
           old_load, new_load, instantiate, swap, live_bytes);
    return live_bytes > 0 ? 0 : 17;
}

