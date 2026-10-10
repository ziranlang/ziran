#include "zir_emit.h"
#include "zir_vm_internal.h"
#include <stdatomic.h>
#include <time.h>

typedef struct VmFunctionProfile {
    char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
    uint64_t calls, self_ticks, inclusive_ticks, self_allocations;
} VmFunctionProfile;

struct VmProfile {
    FILE *output;
    VmFunctionProfile **functions;
    size_t slots, count;
    uint64_t ticks, allocations, last_report, instance;
    uint64_t cache_hits[VM_CACHE_COUNT], cache_misses[VM_CACHE_COUNT];
    uint64_t cache_collisions[VM_CACHE_COUNT];
};

void
profile_cache_access(Vm *vm, int cache, int event)
{
    if(vm->profile == NULL || cache < 0 || cache >= VM_CACHE_COUNT)
        return;
    if(event == VM_CACHE_HIT)
        vm->profile->cache_hits[cache]++;
    else {
        vm->profile->cache_misses[cache]++;
        if(event == VM_CACHE_COLLISION)
            vm->profile->cache_collisions[cache]++;
    }
}

static atomic_uint profile_instance;
static atomic_flag profile_writer = ATOMIC_FLAG_INIT;

static uint64_t
profile_clock(void)
{
    clock_t ticks = clock();
    return ticks < 0 ? 0 : (uint64_t)ticks;
}

static size_t
profile_slot(VmFunctionProfile **functions, size_t slots,
             const char *module, const char *function)
{
    size_t slot = (vm_field_hash(module) ^ vm_field_hash(function)) & (slots - 1);
    while(functions[slot] != NULL &&
          (strcmp(functions[slot]->module, module) ||
           strcmp(functions[slot]->function, function)))
        slot = (slot + 1) & (slots - 1);
    return slot;
}

static VmFunctionProfile *
profile_function(VmProfile *profile, const ZirModule *module,
                 const ZirFunction *function)
{
    if((profile->count + 1) * 2 >= profile->slots) {
        size_t slots = profile->slots ? profile->slots * 2 : 256;
        VmFunctionProfile **entries = calloc(slots, sizeof(*entries));
        if(entries == NULL)
            return NULL;
        for(size_t i = 0; i < profile->slots; i++) {
            VmFunctionProfile *entry = profile->functions[i];
            if(entry != NULL)
                entries[profile_slot(entries, slots, entry->module, entry->function)] = entry;
        }
        free(profile->functions);
        profile->functions = entries;
        profile->slots = slots;
    }
    size_t slot = profile_slot(profile->functions, profile->slots,
                               module->name, function->name);
    if(profile->functions[slot] == NULL) {
        VmFunctionProfile *entry = calloc(1, sizeof(*entry));
        if(entry == NULL)
            return NULL;
        /* Foreign descriptors can be temporary. Copy just their bounded
         * names, without changing the compiler's shared intern tables. */
        memcpy(entry->module, module->name, sizeof(entry->module));
        memcpy(entry->function, function->name, sizeof(entry->function));
        profile->functions[slot] = entry;
        profile->count++;
    }
    return profile->functions[slot];
}

static void
profile_text(FILE *output, const char *text)
{
    fputc('"', output);
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if(*p == '"' || *p == '\\')
            fputc('\\', output);
        if(*p < 32)
            fprintf(output, "\\u%04x", *p);
        else
            fputc(*p, output);
    }
    fputc('"', output);
}

static void
profile_report(Vm *vm, int complete)
{
    VmProfile *profile = vm->profile;
    if(profile == NULL)
        return;
    // Instances can report from different threads to the same append file.
    // Flush one complete JSON record before another instance writes.
    while(atomic_flag_test_and_set_explicit(&profile_writer, memory_order_acquire)) {}
    fprintf(profile->output,
            "{\"schema_version\":1,\"instance\":%llu,\"clock\":\"process_cpu\","
            "\"complete\":%s,\"failed\":%s,\"live_value_bytes\":%llu,",
            (unsigned long long)profile->instance, complete ? "true" : "false",
            vm->failed ? "true" : "false",
            (unsigned long long)(vm->record_bytes + vm->array_bytes + vm->string_bytes));
    fprintf(profile->output,
            "\"metadata_caches\":{\"call\":{\"hits\":%llu,\"misses\":%llu,\"collisions\":%llu},"
            "\"global\":{\"hits\":%llu,\"misses\":%llu,\"collisions\":%llu}},\"functions\":[",
            (unsigned long long)profile->cache_hits[VM_CACHE_CALL],
            (unsigned long long)profile->cache_misses[VM_CACHE_CALL],
            (unsigned long long)profile->cache_collisions[VM_CACHE_CALL],
            (unsigned long long)profile->cache_hits[VM_CACHE_GLOBAL],
            (unsigned long long)profile->cache_misses[VM_CACHE_GLOBAL],
            (unsigned long long)profile->cache_collisions[VM_CACHE_GLOBAL]);
    int first = 1;
    for(size_t i = 0; i < profile->slots; i++) {
        VmFunctionProfile *entry = profile->functions[i];
        if(entry == NULL)
            continue;
        if(!first) fputc(',', profile->output);
        first = 0;
        fputs("{\"module\":", profile->output);
        profile_text(profile->output, entry->module);
        fputs(",\"function\":", profile->output);
        profile_text(profile->output, entry->function);
        fprintf(profile->output,
                ",\"calls\":%llu,\"self_cpu_ms\":%.6f,\"inclusive_cpu_ms\":%.6f,"
                "\"self_allocations\":%llu}",
                (unsigned long long)entry->calls,
                entry->self_ticks * (1000.0 / CLOCKS_PER_SEC),
                entry->inclusive_ticks * (1000.0 / CLOCKS_PER_SEC),
                (unsigned long long)entry->self_allocations);
    }
    fputs("]}\n", profile->output);
    fflush(profile->output);
    atomic_flag_clear_explicit(&profile_writer, memory_order_release);
    profile->last_report = profile_clock();
}

static void
profile_open(Vm *vm)
{
    const char *path = getenv("ZIRAN_VM_PROFILE");
    if(path == NULL || *path == '\0')
        return;
    VmProfile *profile = calloc(1, sizeof(*profile));
    if(profile == NULL)
        return;
    profile->output = fopen(path, "ab");
    if(profile->output == NULL) {
        free(profile);
        Diagnostic(Span("", 0, 0), "zib.profile", "cannot open runtime profile");
        return;
    }
    profile->instance = atomic_fetch_add(&profile_instance, 1) + 1;
    profile->last_report = profile_clock();
    vm->profile = profile;
}

static void
profile_close(Vm *vm)
{
    if(vm->profile == NULL)
        return;
    profile_report(vm, 1);
    VmProfile *profile = vm->profile;
    for(size_t i = 0; i < profile->slots; i++)
        free(profile->functions[i]);
    free(profile->functions);
    fclose(profile->output);
    free(profile);
    vm->profile = NULL;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <pthread.h>
#define VM_KNOWS_STACK 1
#endif

/* C stack kept free below the deepest call: one call's statements and
 * expressions nest at most VM_MAX_DEPTH levels each before the next
 * call checks again. */
enum { VM_STACK_MARGIN = 1024 * 1024 };

static void collect_unreachable(Vm *vm);

static Flow
execute_sequence(Frame *frame, int begin, int end, int depth, Value *result);

static int return_storage_owned(Value value, int depth);

/* A call or aggregate literal can hand fresh, unaliased storage directly to
 * its destination. Reads of existing bindings still preserve value copying. */
static Value
coerce_storage_expression(Frame *frame, int expression, Value value,
                          const char *type, uint64_t allocation_entry)
{
    if(expression >= 0 && value.kind == VALUE_RECORD && value.record != NULL &&
       value.record->allocation > allocation_entry &&
       (frame->function->exprs[expression].kind == ZIR_EXPR_CALL ||
        frame->function->exprs[expression].kind == ZIR_EXPR_COMPOUND) &&
       !vm_type_contains_vec(frame->vm, frame->module, type, 0) &&
       return_storage_owned(value, 0))
        return coerce_expression(frame->vm, frame->module, value, type);
    return coerce(frame->vm, frame->module, value, type);
}


static Flow
execute_sequence_body(Frame *frame, int begin, int end, int depth,
                      Value *result)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    if(depth >= VM_MAX_DEPTH)
        return FLOW_ERROR;
    for(int s = begin; s < end && !vm->failed; s++) {
        /* Suspended expressions retain their partial values explicitly,
         * so nested calls can reclaim garbage at statement boundaries too.
         * Batch collection instead of walking globals after every helper. */
        size_t threshold = vm->collection_threshold ?
            vm->collection_threshold : 4 * 1024 * 1024;
        if(vm->allocated_since_collection >= threshold)
            collect_unreachable(vm);
        const ZirStmt *statement = &function->stmts[s];
        int close;
        int saved_locals;
        Flow flow;
        if(vm->max_steps > 0 && ++vm->steps > vm->max_steps)
            return FLOW_ERROR;
        switch(statement->kind) {
        case ZIR_STMT_DECL: {
            uint64_t allocation_entry = vm->allocation;
            Value value = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) :
                default_value(vm, frame->module, statement->type, 0);
            if(vm->failed || frame->local_count >= frame->local_capacity)
                return FLOW_ERROR;
            Local *local = &frame->locals[frame->local_count++];
            local->name = statement->name;
            local->type = statement->type;
            /* A default was constructed exclusively for this local. There
             * is no existing binding to isolate with another deep copy. */
            local->value = statement->expr_root < 0 ?
                coerce_expression(vm, frame->module, value, statement->type) :
                coerce_storage_expression(frame, statement->expr_root,
                    value, statement->type, allocation_entry);
            break;
        }
        case ZIR_STMT_ASSIGN: {
            Record *union_record = NULL;
            const char *union_type = NULL;
            Value held[2] = {assignment_slot_root(frame, statement->lhs_root, 0), {0}};
            if(frame->union_write_record != NULL) {
                union_record = frame->union_write_record;
                union_type = frame->union_write_type;
                frame->union_write_record = NULL;
                frame->union_write_type = NULL;
            }
            if(!place_valid(held[0]))
                return FLOW_ERROR;
            /* A compound assignment reads its target before the right side
             * runs, as native targets do: x += Bump() adds to the old x. */
            int compound = strcmp(statement->assignment_op, "=") != 0;
            Value current = {0};
            if(compound)
                current = union_record != NULL ?
                    union_member_read(vm, union_record, union_type) : place_read(vm, held[0]);
            held[1] = current;
            VmRoots roots = {vm->evaluation_roots, held, 2};
            vm->evaluation_roots = &roots;
            uint64_t allocation_entry = vm->allocation;
            Value right = eval(frame, statement->expr_root, 0);
            if(vm->failed)
                return FLOW_ERROR;
            if(compound) {
                const char *operation = assignment_binary_operator(
                    statement->assignment_op);
                if(operation == NULL)
                    return FLOW_ERROR;
                right = binary_value(vm, operation, current, right,
                    function->exprs[statement->lhs_root].type,
                    function->exprs[statement->expr_root].type);
            }
            Value replacement = union_record != NULL ? right :
                coerce_storage_expression(frame, statement->expr_root, right,
                    function->exprs[statement->lhs_root].type, allocation_entry);
            if(vm->failed)
                return FLOW_ERROR;
            if(union_record != NULL) {
                if(!union_member_write(vm, union_record, union_type,
                                       replacement))
                    return FLOW_ERROR;
                vm->evaluation_roots = roots.previous;
                break;
            }
            Value previous = place_read(vm, held[0]);
            if(previous.kind == VALUE_ARRAY && previous.array != NULL &&
               replacement.kind == VALUE_ARRAY && replacement.array != NULL &&
               previous.array->length == replacement.array->length &&
               (previous.array->address_taken || array_has_active_slice(vm, previous.array))) {
                /* A pointer or live slice borrows the array's storage. Native array
                 * assignment updates that storage, so keep its identity and
                 * move the replacement elements into the existing slots. */
                for(int element = 0; element < previous.array->length; element++) {
                    Value old = array_get(previous.array, (size_t)element);
                    array_set(previous.array, (size_t)element,
                              array_get(replacement.array, (size_t)element));
                    array_set(replacement.array, (size_t)element, old);
                }
                retire_value(vm, replacement, 0);
                release_retired(vm);
                vm->evaluation_roots = roots.previous;
                break;
            }
            place_write(vm, held[0], replacement);
            if(previous.kind == VALUE_RECORD || previous.kind == VALUE_ARRAY) {
                retire_value(vm, previous, 0);
                release_retired(vm);
            }
            vm->evaluation_roots = roots.previous;
            break;
        }
        case ZIR_STMT_RETURN:
            *result = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) : int_value(0);
            if(statement->expr_root >= 0 && result->kind == VALUE_RECORD &&
               function->exprs[statement->expr_root].kind == ZIR_EXPR_IDENT) {
                const char *name = function->exprs[statement->expr_root].name;
                for(int i = frame->local_count - 1; i >= 0; i--)
                    if(!strcmp(frame->locals[i].name, name)) {
                        frame->returned_local = frame->locals[i].value.kind == VALUE_RECORD &&
                            frame->locals[i].value.record == result->record;
                        break;
                    }
            }
            return vm->failed ? FLOW_ERROR : FLOW_RETURN;
        case ZIR_STMT_UNREACHABLE:
            Diagnostic(statement->span, "vm.unreachable", "unreachable code executed");
            vm->failed = 1;
            return FLOW_ERROR;
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED: {
            Value unused = eval(frame, statement->expr_root, 0);
            if(!vm->failed && statement->expr_root >= 0 &&
               function->exprs[statement->expr_root].kind == ZIR_EXPR_CALL &&
               vm_vec_element_type(vm, frame->module,
                   function->exprs[statement->expr_root].type, NULL, 0)) {
                retire_value(vm, unused, 0);
                release_retired(vm);
            }
            break;
        }
        case ZIR_STMT_IF: {
            int branch = s;
            int executed = 0;
            do {
                const ZirStmt *head = &function->stmts[branch];
                close = statement_close(function, branch, end);
                if(close < 0)
                    return FLOW_ERROR;
                if(!executed && (head->expr_root < 0 ||
                                 truthy(eval(frame, head->expr_root, 0)))) {
                    if(vm->failed)
                        return FLOW_ERROR;
                    saved_locals = frame->local_count;
                    flow = execute_sequence(frame, branch + 1, close,
                                            depth + 1, result);
                    if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                        drop_owned_locals(frame, saved_locals);
                    if(flow != FLOW_NEXT)
                        return flow;
                    executed = 1;
                }
                if(vm->failed)
                    return FLOW_ERROR;
                branch = close + 1;
            } while(branch < end &&
                    is_else_branch(&function->stmts[branch]));
            s = branch - 1;
            break;
        }
        case ZIR_STMT_WHILE:
            close = statement_close(function, s, end);
            if(close < 0)
                return FLOW_ERROR;
            saved_locals = frame->local_count;
            while(1) {
                if(vm->max_steps > 0 && ++vm->steps > vm->max_steps)
                    return FLOW_ERROR;
                Value condition = eval(frame, statement->expr_root, 0);
                if(vm->failed)
                    return FLOW_ERROR;
                if(!truthy(condition))
                    break;
                flow = execute_sequence(frame, s + 1, close, depth + 1,
                                        result);
                if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                    drop_owned_locals(frame, saved_locals);
                if(flow == FLOW_RETURN || flow == FLOW_ERROR)
                    return flow;
                if((flow == FLOW_BREAK || flow == FLOW_CONTINUE) &&
                   frame->control_target != 0 &&
                   frame->control_target != statement->loop_id)
                    return flow;
                if(flow == FLOW_BREAK) {
                    frame->control_target = 0;
                    break;
                }
                if(flow == FLOW_CONTINUE)
                    frame->control_target = 0;
            }
            s = close;
            break;
        case ZIR_STMT_BLOCK_OPEN:
            close = statement_close(function, s, end);
            if(close < 0)
                return FLOW_ERROR;
            saved_locals = frame->local_count;
            flow = execute_sequence(frame, s + 1, close, depth + 1, result);
            if(flow != FLOW_RETURN && flow != FLOW_ERROR)
                drop_owned_locals(frame, saved_locals);
            if(flow != FLOW_NEXT)
                return flow;
            s = close;
            break;
        case ZIR_STMT_BREAK:
            frame->control_target = statement->target_id;
            return FLOW_BREAK;
        case ZIR_STMT_CONTINUE:
            frame->control_target = statement->target_id;
            return FLOW_CONTINUE;
        default:
            return FLOW_ERROR;
        }
    }
    return vm->failed ? FLOW_ERROR : FLOW_NEXT;
}

static Flow
execute_sequence(Frame *frame, int begin, int end, int depth, Value *result)
{
    VmRoots *previous = frame->vm->evaluation_roots;
    Flow flow = execute_sequence_body(frame, begin, end, depth, result);
    frame->vm->evaluation_roots = previous;
    return flow;
}

static void
release_host_argument(VmHostValue *value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value->kind == VM_HOST_SLICE || value->kind == VM_HOST_ARRAY) {
        for(size_t i = 0; i < value->length && value->elements != NULL; i++)
            release_host_argument(&value->elements[i], depth + 1);
        free(value->elements);
        value->elements = NULL;
        return;
    }
    if(value->kind != VM_HOST_RECORD || value->fields == NULL)
        return;
    VmHostField *fields = (VmHostField *)value->fields;
    for(size_t i = 0; i < value->field_count; i++)
        release_host_argument(&fields[i].value, depth + 1);
    free(fields);
    value->fields = NULL;
}

static int
host_argument(const ZirModule *module, const char *type, Value value,
              VmHostValue *out, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return 0;
    /* A pointer into VM storage has no host address to pass. */
    if(value.kind == VALUE_POINTER)
        return 0;
    out->type = type;
    char element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        if(capacity < 0 || value.kind != VALUE_ARRAY || value.array == NULL ||
           value.array->length != capacity ||
           !array_element_matches(module, element, value.array))
            return 0;
        out->kind = VM_HOST_ARRAY;
        out->length = (size_t)capacity;
        if(capacity == 0)
            return 1;
        out->elements = calloc((size_t)capacity, sizeof(*out->elements));
        if(out->elements == NULL)
            return 0;
        for(int i = 0; i < capacity; i++)
            if(!host_argument(module, value.array->element_type,
                              array_get(value.array, (size_t)i),
                              &out->elements[i], depth + 1))
                return 0;
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        if(value.kind != VALUE_SLICE ||
           (value.length > 0 && value.array == NULL) ||
           (value.array != NULL &&
            (!array_element_matches(module, element, value.array) ||
             value.offset > (size_t)value.array->length ||
             value.length > (size_t)value.array->length - value.offset)))
            return 0;
        out->kind = VM_HOST_SLICE;
        out->length = value.length;
        out->elements = calloc(value.length ? value.length : 1,
                               sizeof(*out->elements));
        if(out->elements == NULL)
            return 0;
        for(size_t i = 0; i < value.length; i++)
            if(!host_argument(module, value.array->element_type,
                              array_get(value.array, value.offset + i),
                              &out->elements[i], depth + 1))
                return 0;
        return 1;
    }
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record != NULL && !record->is_enum) {
        if(value.kind != VALUE_RECORD || value.record == NULL ||
           !same_record_type(owner, record, value.record) ||
           value.record->field_count < 0 ||
           value.record->field_count > VM_MAX_FIELDS)
            return 0;
        int count = value.record->field_count;
        VmHostField *fields = calloc(count ? (size_t)count : 1,
                                     sizeof(*fields));
        if(fields == NULL)
            return 0;
        out->kind = VM_HOST_RECORD;
        out->fields = fields;
        out->field_count = (size_t)count;
        for(int i = 0; i < count; i++) {
            fields[i].name = value.record->fields[i].field.name;
            if(!host_argument(owner,
                              value.record->fields[i].field.type,
                              value.record->fields[i].value,
                              &fields[i].value, depth + 1))
                return 0;
        }
        return 1;
    }
    if(record != NULL && record->is_enum && value.kind != VALUE_ENUM)
        return 0;
    /* The host ABI has separate fields. Copy only the active VM payload;
     * inactive union words may contain a different kind's bits or address. */
    if(value.kind == VALUE_INT || value.kind == VALUE_ENUM) {
        out->integer = value.integer;
        out->bits = value.bits;
    } else if(value.kind == VALUE_REAL) {
        out->real = value.real;
    } else if(value.kind == VALUE_STRING) {
        out->data = value.data;
        out->length = value.length;
    }
    out->kind = value.kind == VALUE_REAL ? VM_HOST_REAL :
        value.kind == VALUE_STRING ? VM_HOST_STRING :
        type[0] == '*' ? VM_HOST_POINTER :
        type[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    if(out->kind == VM_HOST_POINTER) {
        if(value.kind != VALUE_INT)
            return 0;
        out->pointer = (void *)(uintptr_t)integer_bits(value);
        return 1;
    }
    return value.kind == VALUE_INT || value.kind == VALUE_ENUM ||
           value.kind == VALUE_REAL || value.kind == VALUE_STRING;
}

static Value
host_return(Vm *vm, const ZirModule *module, const char *type,
            const VmHostValue *input, int depth)
{
    Value result = int_value(0);
    if(depth >= VM_MAX_DEPTH || input->type == NULL ||
       strcmp(input->type, type) != 0) {
        vm->failed = 1;
        return result;
    }
    char array_element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, array_element, sizeof(array_element),
                        &capacity)) {
        if(capacity < 0 || input->kind != VM_HOST_ARRAY ||
           input->length != (size_t)capacity ||
           (capacity > 0 && input->elements == NULL) ||
           input->field_count != 0 || input->fields != NULL) {
            vm->failed = 1;
            return result;
        }
        Array *array = allocate_array_try(vm, module, array_element,
                                          capacity, 1);
        if(array == NULL)
            return result;
        for(int i = 0; i < capacity && !vm->failed; i++)
            array_set(array, (size_t)i, host_return(vm, module, array_element,
                                              &input->elements[i], depth + 1));
        return (Value){.kind = VALUE_ARRAY, .array = array};
    }
    const ZirModule *owner = NULL;
    const ZirType *declared = FindType(module, type, &owner);
    if(declared != NULL && !declared->is_enum) {
        if(input->kind != VM_HOST_RECORD ||
           (input->field_count > 0 && input->fields == NULL) ||
           input->field_count > VM_MAX_FIELDS) {
            vm->failed = 1;
            return result;
        }
        const VmLayout *layout = record_layout(vm, declared);
        if(layout == NULL || layout->count != (int)input->field_count) {
            vm->failed = 1;
            return result;
        }
        int count = layout->count;
        const VmField *fields = layout->fields;
        Record *record = allocate_record(vm, owner, declared, count);
        if(record == NULL)
            return result;
        for(int i = 0; i < count && !vm->failed; i++) {
            record->fields[i].field = fields[i];
            if(input->fields[i].name == NULL ||
               strcmp(input->fields[i].name,
                      record->fields[i].field.name) != 0) {
                vm->failed = 1;
                break;
            }
            record->fields[i].value = host_return(vm, owner,
                record->fields[i].field.type,
                &input->fields[i].value, depth + 1);
        }
        return (Value){.kind = VALUE_RECORD, .record = record};
    }
    ValueKind expected = value_kind(type);
    char slice_element[ZIR_NAME_MAX];
    if(SliceElementType(type, slice_element, sizeof(slice_element))) {
        /* A host-returned slice is copied into VM-owned storage; the caller
         * owns the elements from here on. */
        Array *owned;
        if(input->kind != VM_HOST_SLICE ||
           (input->length > 0 && input->elements == NULL) ||
           !array_length_fits(slice_element, input->length) ||
           input->field_count != 0 || input->fields != NULL) {
            vm->failed = 1;
            return result;
        }
        owned = allocate_array_try(vm, module, slice_element,
                                   (int)input->length, 1);
        if(owned == NULL)
            return result;
        for(size_t i = 0; i < input->length && !vm->failed; i++)
            array_set(owned, i, host_return(vm, module, slice_element,
                                             &input->elements[i], depth + 1));
        if(vm->failed)
            return result;
        return (Value){.kind = VALUE_SLICE, .array = owned,
                       .offset = 0, .length = input->length};
    }
    VmHostValueKind kind = expected == VALUE_VOID ? VM_HOST_VOID :
        expected == VALUE_REAL ? VM_HOST_REAL :
        expected == VALUE_STRING ? VM_HOST_STRING :
        type[0] == '*' ? VM_HOST_POINTER :
        type[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    if((declared == NULL && expected == VALUE_INVALID) ||
       input->kind != kind || input->field_count != 0 ||
       input->fields != NULL ||
       (kind == VM_HOST_STRING && input->data == NULL &&
        input->length != 0)) {
        vm->failed = 1;
        return result;
    }
    if(kind == VM_HOST_REAL)
        result = real_value(input->real);
    else if(kind == VM_HOST_STRING) {
        result = string_value(input->data, input->length);
        /* A host may return a range of one of its borrowed text arguments.
         * Preserve that VM owner rather than treating the alias as host-owned. */
        uintptr_t data = (uintptr_t)input->data;
        for(StringLiteral *item = vm->strings; item != NULL; item = item->next) {
            uintptr_t begin = (uintptr_t)item->data;
            if(data >= begin && data - begin <= item->length &&
               input->length <= item->length - (data - begin)) {
                result.string_owner = item;
                break;
            }
        }
    }
    else if(kind == VM_HOST_POINTER)
        result = uint_value((uintptr_t)input->pointer);
    else if(kind == VM_HOST_UNSIGNED)
        result = uint_value(input->bits);
    else if(kind == VM_HOST_INTEGER)
        result = int_value(input->integer);
    else
        result.kind = VALUE_VOID;
    return coerce(vm, module, result, type);
}

static int
host_copy_back(Vm *vm, const ZirModule *module, const char *type,
               Value target, const VmHostValue *input)
{
    char element[ZIR_NAME_MAX];
    if(!SliceElementType(type, element, sizeof(element)))
        return 1;
    if(input->kind != VM_HOST_SLICE || input->type == NULL ||
       strcmp(input->type, type) != 0 || target.kind != VALUE_SLICE ||
       target.length != input->length ||
       (target.length > 0 &&
        (target.array == NULL || input->elements == NULL))) {
        vm->failed = 1;
        return 0;
    }
    if(target.length == 0)
        return 1;
    Value *updates = calloc(target.length, sizeof(*updates));
    if(updates == NULL) {
        vm->failed = 1;
        return 0;
    }
    for(size_t i = 0; i < target.length && !vm->failed; i++)
        updates[i] = host_return(vm, module, element,
                                 &input->elements[i], 1);
    if(!vm->failed) {
        for(size_t i = 0; i < target.length; i++) {
            Value previous = array_get(target.array, target.offset + i);
            array_set(target.array, target.offset + i, updates[i]);
            retire_value(vm, previous, 0);
        }
        release_retired(vm);
    }
    free(updates);
    return !vm->failed;
}

static const char *
assignment_root_name(const ZirFunction *function, int index)
{
    while(index >= 0 && index < function->expr_count) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
            index = expression->right;
            continue;
        }
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_POINTER_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

/* A record or array parameter can share its caller's value only when the
 * callee cannot write through that parameter. Other storage paths still copy
 * values, so passing a parameter onward to a mutating function stays safe. */
static int
parameter_read_only(const ZirFunction *function, const char *name)
{
    /* A pointer taken into the parameter could write through it later. */
    for(int i = 0; i < function->expr_count; i++) {
        const ZirExpr *expression = &function->exprs[i];
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&")) {
            const char *root = assignment_root_name(function, expression->right);
            if(root == NULL || strcmp(root, name) == 0)
                return 0;
        }
        /* A mutable slice of a by-value array can be passed to another
         * function or procedure. The array must belong to this call first. */
        if(expression->kind == ZIR_EXPR_SLICE && expression->left >= 0 &&
           ArrayElementType(function->exprs[expression->left].type, NULL, 0, NULL)) {
            const char *root = assignment_root_name(function, expression->left);
            if(root == NULL || strcmp(root, name) == 0)
                return 0;
        }
    }
    for(int i = 0; i < function->stmt_count; i++) {
        const ZirStmt *statement = &function->stmts[i];
        if(statement->kind == ZIR_STMT_ASSIGN) {
            const char *root = assignment_root_name(function,
                                                    statement->lhs_root);
            if(root == NULL || strcmp(root, name) == 0)
                return 0;
        }
    }
    return 1;
}

/* Heap pointer chains are not bounded by the call-depth limit. Traverse
 * borrowed slots iteratively so a deep live graph cannot be partly marked. */
typedef struct PinRange {
    const unsigned char *values;
    size_t count, stride;
    const int *indices;
} PinRange;

typedef struct PinWork {
    PinRange *ranges;
    size_t count, capacity;
    PinRange local[32];
} PinWork;

/* If the small traversal stack cannot grow, retaining storage for this
 * generation is safe. A later collection can reclaim it without losing roots. */
static void
pin_all(Vm *vm)
{
    for(Record *record = vm->records; record != NULL; record = record->next)
        record->pinned = vm->pin_generation;
    for(Array *array = vm->arrays; array != NULL; array = array->next)
        array->pinned = vm->pin_generation;
    for(StringLiteral *text = vm->strings; text != NULL; text = text->next)
        text->pinned = vm->pin_generation;
}

static int
pin_range(PinWork *work, const Value *values, size_t count, size_t stride,
          const int *indices)
{
    if(count == 0)
        return 1;
    if(work->count == work->capacity) {
        if(work->capacity > SIZE_MAX / 2 / sizeof(*work->ranges))
            return 0;
        size_t capacity = work->capacity * 2;
        PinRange *ranges;
        if(work->ranges == work->local) {
            ranges = malloc(capacity * sizeof(*ranges));
            if(ranges != NULL)
                memcpy(ranges, work->ranges, work->count * sizeof(*ranges));
        } else {
            ranges = realloc(work->ranges, capacity * sizeof(*ranges));
        }
        if(ranges == NULL)
            return 0;
        work->ranges = ranges;
        work->capacity = capacity;
    }
    work->ranges[work->count++] = (PinRange){
        (const unsigned char *)values, count, stride, indices
    };
    return 1;
}

static int
pin_slot(Vm *vm, const Value *value, int depth, PinWork *work);

static int
pin_slots(Vm *vm, const Value *values, size_t count, size_t stride,
          const int *indices, int depth, PinWork *work)
{
    /* Finish shallow graphs directly, retaining the fast path for ordinary
     * aggregates. Defer deeper ranges instead of stopping their traversal. */
    if(depth >= 32)
        return pin_range(work, values, count, stride, indices);
    const unsigned char *bytes = (const unsigned char *)values;
    for(size_t i = 0; i < count; i++) {
        size_t index = indices != NULL ? (size_t)indices[i] : i;
        const Value *slot = (const Value *)(bytes + index * stride);
        if((slot->kind == VALUE_STRING || slot->kind >= VALUE_RECORD) &&
           !pin_slot(vm, slot, depth + 1, work))
            return 0;
    }
    return 1;
}

static int
pin_slot(Vm *vm, const Value *value, int depth, PinWork *work)
{
    if(value->kind == VALUE_STRING && value->string_owner != NULL) {
        value->string_owner->pinned = vm->pin_generation;
        return 1;
    }
    Record *record = NULL;
    Array *array = NULL;
    if(value->kind == VALUE_RECORD || value->kind == VALUE_POINTER)
        record = value->record;
    if(value->kind == VALUE_ARRAY || value->kind == VALUE_SLICE ||
       value->kind == VALUE_POINTER)
        array = value->array;
    if(record != NULL && record->pinned != vm->pin_generation) {
        record->pinned = vm->pin_generation;
        const int *references = record->field_slots != NULL ?
            record->field_slots + record->field_slot_count : NULL;
        size_t count = references != NULL ? (size_t)references[0] :
                                           (size_t)record->field_count;
        if(record->field_count > 0 && count > 0 &&
           !pin_slots(vm, &record->fields[0].value,
                      count, sizeof(RecordField),
                      references != NULL ? references + 1 : NULL, depth, work))
            return 0;
    }
    if(array != NULL && array->pinned != vm->pin_generation) {
        array->pinned = vm->pin_generation;
        if(array->holds_references) {
            if(array->storage == ARRAY_BOXED) {
                if(!pin_slots(vm, (const Value *)array->data,
                              (size_t)array->length, sizeof(Value), NULL, depth, work))
                    return 0;
            } else {
                for(int i = 0; i < array->length; i++) {
                    Value item = array_get(array, (size_t)i);
                    if(!pin_slot(vm, &item, depth + 1, work))
                        return 0;
                }
            }
        }
    }
    return 1;
}

static void
pin_value_at(Vm *vm, const Value *value)
{
    if(value->kind == VALUE_STRING) {
        if(value->string_owner != NULL)
            value->string_owner->pinned = vm->pin_generation;
        return;
    }
    if(value->kind != VALUE_RECORD && value->kind != VALUE_ARRAY &&
       value->kind != VALUE_SLICE && value->kind != VALUE_POINTER)
        return;
    if(value->kind == VALUE_RECORD &&
       (value->record == NULL || value->record->pinned == vm->pin_generation))
        return;
    if((value->kind == VALUE_ARRAY || value->kind == VALUE_SLICE) &&
       (value->array == NULL || value->array->pinned == vm->pin_generation))
        return;
    if(value->kind == VALUE_POINTER &&
       (value->record == NULL || value->record->pinned == vm->pin_generation) &&
       (value->array == NULL || value->array->pinned == vm->pin_generation))
        return;
    PinWork work;
    work.ranges = work.local;
    work.count = 0;
    work.capacity = sizeof(work.local) / sizeof(work.local[0]);
    int complete = pin_slot(vm, value, 0, &work);
    while(complete && work.count > 0) {
        PinRange range = work.ranges[--work.count];
        complete = pin_slots(vm, (const Value *)range.values,
                             range.count, range.stride, range.indices, 0, &work);
    }
    if(!complete)
        pin_all(vm);
    if(work.ranges != work.local)
        free(work.ranges);
}

void
pin_value(Vm *vm, Value value, int depth)
{
    (void)depth;
    pin_value_at(vm, &value);
}

void
pin_evaluation_roots(Vm *vm)
{
    for(VmRoots *roots = vm->evaluation_roots; roots != NULL;
        roots = roots->previous)
        for(int i = 0; i < roots->count; i++)
            pin_value_at(vm, &roots->values[i]);
}

static void
pin_globals(Vm *vm)
{
    for(int i = 0; i < vm->global_count; i++)
        pin_value_at(vm, &vm->globals[i].value);
}

/* A callee may write through a slice borrowed from a caller. Its newly
 * allocated record elements then belong to that caller's array even though
 * their allocation sequence falls inside the completed call. */
static void
pin_active_frames(Vm *vm)
{
    for(Frame *frame = vm->active_frame; frame != NULL;
        frame = frame->caller)
        for(int i = 0; i < frame->local_count; i++)
            pin_value_at(vm, &frame->locals[i].value);
}

static void
release_call_strings(Vm *vm, uint64_t entry, uint64_t before_result)
{
    StringLiteral **slot = &vm->strings;
    while(*slot != NULL) {
        StringLiteral *current = *slot;
        if(current->expression == NULL && current->allocation > entry &&
           current->allocation <= before_result &&
           current->pinned != vm->pin_generation) {
            *slot = current->next;
            vm->string_bytes -= current->bytes;
            free(current);
        } else slot = &current->next;
    }
}

static void
collect_unreachable(Vm *vm)
{
    vm->pin_generation++;
    pin_globals(vm);
    pin_active_frames(vm);
    pin_evaluation_roots(vm);
    Record **record = &vm->records;
    while(*record != NULL) {
        Record *current = *record;
        if(current->pinned != vm->pin_generation) {
            release_record(vm, current);
        } else {
            record = &current->next;
        }
    }
    Array **array = &vm->arrays;
    while(*array != NULL) {
        Array *current = *array;
        if(current->pinned != vm->pin_generation) {
            release_array(vm, current);
        } else {
            array = &current->next;
        }
    }
    release_call_strings(vm, 0, UINT64_MAX);
    vm->allocated_since_collection = 0;
    /* A retained program may keep a large, mostly unchanged object graph.
     * Give it allocation room proportional to that live graph instead of
     * repeatedly marking it after every small batch of temporary values.
     * Keep the batch bounded independently of the VM's storage limits. */
    size_t threshold = (vm->record_bytes + vm->array_bytes + vm->string_bytes) / 2;
    if(threshold < 4 * 1024 * 1024) threshold = 4 * 1024 * 1024;
    if(threshold > 32 * 1024 * 1024) threshold = 32 * 1024 * 1024;
    vm->collection_threshold = threshold;
}

/* Result coercion creates its own deep copy after before_result. Reclaim
 * earlier allocations from the completed call by allocation sequence, since
 * replacing a global can remove the record that was at the call's entry. */
static void
release_call_records(Vm *vm, uint64_t entry, uint64_t before_result)
{
    Record **record = &vm->records;
    while(*record != NULL) {
        Record *current = *record;
        if(current->allocation > entry &&
           current->allocation <= before_result &&
           current->pinned != vm->pin_generation) {
            release_record(vm, current);
        } else {
            if(current->allocation <= entry)
                break;
            record = &current->next;
        }
    }
}

static void
release_call_arrays(Vm *vm, uint64_t entry, uint64_t before_result)
{
    Array **array = &vm->arrays;
    while(*array != NULL) {
        Array *current = *array;
        if(current->allocation > entry &&
           current->allocation <= before_result &&
           current->pinned != vm->pin_generation) {
            release_array(vm, current);
        } else {
            if(current->allocation <= entry)
                break;
            array = &current->next;
        }
    }
}
/* Buffers run_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
static size_t
signature_slot(const Vm *vm, const ZirModule *module,
               const ZirFunction *function, const char *extern_args)
{
    size_t slot = (((uintptr_t)module >> 4) ^
                   ((uintptr_t)(function ? (const void *)function : extern_args) >> 3)) *
                  11400714819323198485ull;
    slot &= vm->signature_slots - 1;
    while(vm->signatures[slot] != NULL &&
          (vm->signatures[slot]->module != module ||
           vm->signatures[slot]->function != function ||
           vm->signatures[slot]->extern_args != extern_args))
        slot = (slot + 1) & (vm->signature_slots - 1);
    return slot;
}

const VmSignature *
vm_signature(Vm *vm, const ZirModule *module, const ZirFunction *function)
{
    const ZirFunction *key = function->is_extern ? NULL : function;
    const char *extern_args = function->is_extern ? FunctionArgs(function) : NULL;
    if(vm->signature_count * 2 >= vm->signature_slots) {
        size_t old_slots = vm->signature_slots;
        const VmSignature **old = vm->signatures;
        vm->signature_slots = old_slots ? old_slots * 2 : 256;
        vm->signatures = calloc(vm->signature_slots, sizeof(*vm->signatures));
        if(vm->signatures == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        for(size_t i = 0; i < old_slots; i++)
            if(old[i] != NULL)
                vm->signatures[signature_slot(vm, old[i]->module,
                    old[i]->function, old[i]->extern_args)] = old[i];
        free(old);
    }
    size_t slot = signature_slot(vm, module, key, extern_args);
    if(vm->signatures[slot] == NULL) {
        Parameter *parsed = AllocateOrExit(VM_MAX_PARAMS * sizeof(*parsed));
        int count = parse_parameters(module, function, parsed);
        VmSignature *signature = AllocateOrExit(sizeof(*signature) +
            (size_t)(count > 0 ? count : 0) * sizeof(signature->parameters[0]));
        signature->module = module;
        signature->function = key;
        signature->extern_args = extern_args;
        signature->count = count;
        signature->local_bound = function_local_bound(function, count);
        signature->read_only = 0;
        for(int i = 0; !function->is_extern && i < count; i++)
            if(!vm_type_contains_vec(vm, module, parsed[i].type, 0) &&
               parameter_read_only(function, parsed[i].name))
                signature->read_only |= UINT64_C(1) << i;
        for(int i = 0; i < count; i++) {
            signature->parameters[i].name = KeepName(parsed[i].name);
            signature->parameters[i].type = KeepName(parsed[i].type);
        }
        free(parsed);
        vm->signatures[slot] = signature;
        vm->signature_count++;
    }
    return vm->signatures[slot];
}

void
free_signatures(Vm *vm)
{
    for(size_t i = 0; i < vm->signature_slots; i++)
        free((void *)vm->signatures[i]);
    free(vm->signatures);
    vm->signatures = NULL;
    vm->signature_count = vm->signature_slots = 0;
}

struct RunFunctionBuffers {
    Frame frame;
    /* The frame's locals; kept with the buffers for the next call. */
    Local *locals;
    int local_capacity;
};

/* What a call needs only while it calls the host. Taken from a pool and
 * returned before a body runs, so nested calls reuse one instead of each
 * holding kilobytes for their whole run. */
struct CallSetup {
    VmHostValue host_args[VM_MAX_PARAMS];
};

static CallSetup *
take_setup(Vm *vm)
{
    return vm->setup_buffer_count > 0 ? vm->setup_buffers[--vm->setup_buffer_count] :
        AllocateOrExit(sizeof(CallSetup));
}

static void
give_setup(Vm *vm, CallSetup **setup)
{
    if(*setup == NULL)
        return;
    if(vm->setup_buffer_count < 16)
        vm->setup_buffers[vm->setup_buffer_count++] = *setup;
    else
        free(*setup);
    *setup = NULL;
}

/* A fresh local record can become the return value without a second deep
 * copy. Borrowed fields or array elements can escape through pointers, so
 * those records still materialize a separate value at the return boundary. */
static int
return_storage_owned(Value value, int depth)
{
    if(depth >= VM_MAX_DEPTH) return 0;
    if(value.kind == VALUE_RECORD && value.record != NULL) {
        if(value.record->address_taken) return 0;
        for(int i = 0; i < value.record->field_count; i++) {
            Value field = value.record->fields[i].value;
            if((field.kind == VALUE_RECORD || field.kind == VALUE_ARRAY) &&
               !return_storage_owned(field, depth + 1)) return 0;
        }
    } else if(value.kind == VALUE_ARRAY && value.array != NULL) {
        if(value.array->address_taken || value.array->borrowed) return 0;
        if(value.array->holds_references)
            for(int i = 0; i < value.array->length; i++)
                if(!return_storage_owned(array_get(value.array, (size_t)i), depth + 1))
                    return 0;
    }
    return 1;
}

Value run_function(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count);

static Value
run_function_with_buffers(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count, RunFunctionBuffers *buffers,
             CallSetup **setup)
{
    memset(&buffers->frame, 0, sizeof(buffers->frame));
    const VmSignature *signature = vm_signature(vm, module, function);
    const VmParameter *parameters = signature->parameters;
    int count = signature->count;
    Value result = int_value(0);
    uint64_t allocation_entry = vm->allocation;
    int too_deep = vm->stack_floor != NULL ?
        (const char *)__builtin_frame_address(0) < vm->stack_floor :
        vm->depth >= VM_MAX_DEPTH;
    if(!vm->failed && too_deep)
        vm->depth_exceeded = vm->depth > 0 ? vm->depth : 1;
    if(vm->failed || too_deep || count != arg_count) {
        vm->failed = 1;
        return result;
    }
    if(function->is_extern) {
        const ZirImport *import = host_import(module, function->name);
        if(import != NULL && strncmp(import->target, "ziran:", 6) == 0) {
            const ZirModule *provider_module = NULL;
            const ZirFunction *provider = bound_provider(vm->program, import,
                &provider_module);
            if(provider == NULL) {
                vm->failed = 1;
                return result;
            }
            return run_function(vm, provider_module, provider, args, arg_count);
        }
        memset((*setup)->host_args, 0, sizeof((*setup)->host_args));
        VmHostValue host_result = {0};
        if(vm->host == NULL) {
            Diagnostic(function->span, "zib.capability",
                       "missing host capability: %s:%s",
                       module->name, function->name);
            vm->failed = 1;
            return result;
        }
        for(int i = 0; i < count && !vm->failed; i++) {
            if(args[i].kind != VALUE_SLICE || args[i].length == 0)
                continue;
            for(int j = i + 1; j < count; j++) {
                if(args[j].kind != VALUE_SLICE || args[j].length == 0 ||
                   args[i].array != args[j].array)
                    continue;
                if(args[i].offset < args[j].offset + args[j].length &&
                   args[j].offset < args[i].offset + args[i].length) {
                    Diagnostic(function->span, "zib.host_alias",
                               "overlapping mutable host slice arguments require separate storage");
                    vm->failed = 1;
                    break;
                }
            }
        }
        for(int i = 0; i < count; i++) {
            if(vm->failed)
                break;
            Value argument = coerce_expression(vm, module, args[i],
                                               parameters[i].type);
            if(vm->failed || !host_argument(module,
                                            parameters[i].type, argument,
                                            &(*setup)->host_args[i], 0)) {
                vm->failed = 1;
                break;
            }
        }
        host_result.type = function->return_type;
        if(!vm->failed && !vm->host(vm->host_context, module->name,
                                    function->name, (*setup)->host_args, count,
                                    &host_result))
            vm->failed = 1;
        for(int i = 0; i < count && !vm->failed; i++)
            host_copy_back(vm, module, parameters[i].type,
                           args[i], &(*setup)->host_args[i]);
        for(int i = 0; i < count; i++)
            release_host_argument(&(*setup)->host_args[i], 0);
        if(vm->failed)
            return result;
        return host_return(vm, module, function->return_type,
                           &host_result, 0);
    }
    int bound = signature->local_bound;
    if(buffers->local_capacity < bound) {
        Local *locals = realloc(buffers->locals, (size_t)bound * sizeof(*locals));
        if(locals == NULL) {
            vm->failed = 1;
            return result;
        }
        buffers->locals = locals;
        buffers->local_capacity = bound;
    }
    buffers->frame.locals = buffers->locals;
    buffers->frame.local_capacity = buffers->local_capacity;
    vm->depth++;
    buffers->frame.vm = vm;
    buffers->frame.module = module;
    buffers->frame.function = function;
    buffers->frame.caller = vm->active_frame;
    buffers->frame.serial = ++vm->call_serial;
    vm->active_frame = &buffers->frame;
    for(int i = 0; i < count; i++) {
        buffers->frame.locals[i].name = parameters[i].name;
        buffers->frame.locals[i].type = parameters[i].type;
        /* Scalars, strings and borrowed views never need the aggregate
         * read-only analysis. Coercion copies those values identically. */
        buffers->frame.locals[i].value =
                                (args[i].kind == VALUE_RECORD || args[i].kind == VALUE_ARRAY) &&
                                (signature->read_only & (UINT64_C(1) << i)) ?
            coerce_expression(vm, module, args[i], parameters[i].type) :
            coerce(vm, module, args[i], parameters[i].type);
    }
    buffers->frame.local_count = count;
    give_setup(vm, setup);
    Flow flow = execute_sequence(&buffers->frame, 0, function->stmt_count, 0, &result);
    if(flow == FLOW_ERROR || flow == FLOW_BREAK || flow == FLOW_CONTINUE ||
       (flow != FLOW_RETURN && strcmp(function->return_type, "void") != 0))
        vm->failed = 1;
    uint64_t allocation_before_result = vm->allocation;
    Value returned = buffers->frame.returned_local && result.record != NULL &&
        result.record->allocation > allocation_entry &&
        !vm_type_contains_vec(vm, module, function->return_type, 0) &&
        return_storage_owned(result, 0) ?
        coerce_expression(vm, module, result, function->return_type) :
        coerce(vm, module, result, function->return_type);
    /* Nested return paths leave their locals in this frame. Materialize the
     * return value before releasing those bindings. */
    drop_owned_locals(&buffers->frame, 0);
    vm->active_frame = buffers->frame.caller;
    vm->depth--;
    /* Named Ziran procedure values carry no borrowed frame context. The
     * returned value has already been copied, so slot-using calls can drop
     * temporaries by the same reachability rule as direct calls. */
    /* Nested calls are collected at statement boundaries. A call
     * with unusually large live storage still reclaims its own temporaries
     * before the VM's allocation limits are reached. */
    /* A call that allocated nothing left nothing to reclaim; skipping it
     * avoids marking every live value after each small helper call. */
    if(!vm->failed && allocation_before_result > allocation_entry &&
       (vm->depth == 0 ||
        vm->record_bytes > VM_MAX_RECORD_BYTES / 2 ||
        vm->array_bytes > VM_MAX_ARRAY_BYTES / 2)) {
        vm->pin_generation++;
        pin_globals(vm);
        pin_active_frames(vm);
        pin_evaluation_roots(vm);
        pin_value(vm, returned, 0);
        release_call_records(vm, allocation_entry, allocation_before_result);
        release_call_arrays(vm, allocation_entry, allocation_before_result);
        /* Nested strings are reclaimed at the next statement boundary;
         * an outermost return also collects its call's temporary strings. */
        if(vm->depth == 0)
            release_call_strings(vm, allocation_entry, allocation_before_result);
    }
    return returned;
}

Value
run_function(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count)
{
    VmFunctionProfile *profile = vm->profile ?
        profile_function(vm->profile, module, function) : NULL;
    uint64_t started = 0, child_ticks = 0, allocation_entry = 0, child_allocations = 0;
    if(profile != NULL) {
        started = profile_clock();
        child_ticks = vm->profile->ticks;
        allocation_entry = vm->allocation;
        child_allocations = vm->profile->allocations;
    }
    RunFunctionBuffers *buffers;
    if(vm->run_buffer_count > 0)
        buffers = vm->run_buffers[--vm->run_buffer_count];
    else {
        buffers = AllocateOrExit(sizeof(*buffers));
        buffers->locals = NULL;
        buffers->local_capacity = 0;
    }
    CallSetup *setup = take_setup(vm);
    Value returned = run_function_with_buffers(vm, module, function, args, arg_count,
                                               buffers, &setup);
    give_setup(vm, &setup);
    if(vm->run_buffer_count < 16)
        vm->run_buffers[vm->run_buffer_count++] = buffers;
    else {
        free(buffers->locals);
        free(buffers);
    }
    if(profile != NULL) {
        uint64_t finished = profile_clock();
        uint64_t elapsed = finished >= started ? finished - started : 0;
        uint64_t nested = vm->profile->ticks - child_ticks;
        uint64_t self = elapsed >= nested ? elapsed - nested : 0;
        uint64_t allocations = vm->allocation - allocation_entry;
        uint64_t allocated_in_children = vm->profile->allocations - child_allocations;
        uint64_t owned = allocations >= allocated_in_children ? allocations - allocated_in_children : 0;
        profile->calls++;
        profile->inclusive_ticks += elapsed;
        profile->self_ticks += self;
        profile->self_allocations += owned;
        vm->profile->ticks += self;
        vm->profile->allocations += owned;
    }
    return returned;
}

void
free_records(Vm *vm)
{
    while(vm->records != NULL) {
        release_record(vm, vm->records);
    }
}

void
free_arrays(Vm *vm)
{
    while(vm->arrays != NULL) {
        release_array(vm, vm->arrays);
    }
}

void
free_strings(Vm *vm)
{
    free(vm->literal_sites);
    vm->literal_sites = NULL;
    while(vm->strings != NULL) {
        StringLiteral *next = vm->strings->next;
        vm->string_bytes -= vm->strings->bytes;
        free(vm->strings);
        vm->strings = next;
    }
}

/* Fold one parsed initializer expression into a global's storage slot.
 * Types come from the declared global, never the probe expression (parsed
 * standalone, so it carries no inferred types). Scalars, string literals,
 * records, and arrays recurse through the declared storage shape. */
static int
fold_global_element_in_scope(Vm *vm, const ZirModule *module,
                             const ZirModule *scope, const ZirFunction *probe,
                             int index, Value *target, const char *type,
                             ZirSourceSpan span)
{
    const ZirExpr *expr = &probe->exprs[index];
    if((target->kind == VALUE_ARRAY || target->kind == VALUE_RECORD) &&
       expr->kind != ZIR_EXPR_COMPOUND)
        return 0;
    if(expr->kind == ZIR_EXPR_COMPOUND && target->kind == VALUE_ARRAY) {
        char element[ZIR_NAME_MAX];
        int position = 0;
        if(!ArrayElementType(type, element, sizeof(element), NULL))
            return 0;
        for(int child = expr->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            if(entry->right < 0 || position >= target->array->length)
                return 0;
            Value item = array_get(target->array, (size_t)position);
            if(!fold_global_element_in_scope(vm, module, scope, probe, entry->right,
                                            &item, element, span))
                return 0;
            array_set(target->array, (size_t)position, item);
            position++;
        }
        return 1;
    }
    if(expr->kind == ZIR_EXPR_COMPOUND && target->kind == VALUE_RECORD) {
        const ZirType *record = target->record->type;
        int matched = record != NULL;
        for(int child = expr->first_child; child >= 0 && matched;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            char field_type[ZIR_NAME_MAX] = "";
            Value *field = entry->name[0] ?
                record_field(target->record, entry->name) : NULL;
            size_t offset = 0;
            ZirTypeField decl;
            if(field == NULL || entry->right < 0)
                return 0;
            while(TypeNextField(record, &offset, &decl) == 1)
                if(!strcmp(decl.name, entry->name)) {
                    copy_text(field_type, sizeof(field_type), decl.type);
                    break;
                }
            if(field_type[0] == '\0') return 0;
            matched = fold_global_element_in_scope(vm, module, target->record->owner,
                                          probe, entry->right, field, field_type,
                                          span);
        }
        return matched;
    }
    if(target->kind == VALUE_SLOT) {
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        if((expr->kind != ZIR_EXPR_IDENT && expr->kind != ZIR_EXPR_MEMBER) ||
           !portable_function_value(scope, type, module, expr->text, &owner, &function))
            return 0;
        target->slot_module = owner;
        target->slot_function = function;
        return 1;
    }
    if(target->kind == VALUE_STRING) {
        *target = global_literal_string(vm, expr->text);
        return !vm->failed;
    }
    {
        long folded = 0;
        if(EvaluateCompileExpression(module, expr->text, span, 0, &folded)) {
            *target = target->kind == VALUE_REAL ?
                real_value((double)folded) : int_value(folded);
            return 1;
        }
        char *end = NULL;
        double number = strtod(expr->text, &end);
        if(end != expr->text && *skip_ws(end) == '\0' &&
           isfinite(number) && target->kind == VALUE_REAL) {
            *target = real_value(number);
            return 1;
        }
    }
    return 0;
}

int
fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe,
                    int index, Value *target, const char *type,
                    ZirSourceSpan span)
{
    return fold_global_element_in_scope(vm, module, module, probe, index,
                                        target, type, span);
}

static int
initialize_module_startup(Vm *vm, const ZirProgram *program, int index,
                          unsigned char *state)
{
    const ZirModule *module = &program->modules[index];
    if(state[index] == 2) return 1;
    if(state[index] == 1) {
        Diagnostic(module->span, "zib.global",
                   "cyclic module startup dependency: %s", module->name);
        vm->failed = 1;
        return 0;
    }
    state[index] = 1;
    /* As in native output, only dependencies with something to set up take
     * part: an import cycle through modules without startup, such as widget
     * modules that import each other, orders nothing. */
    for(int i = 0; i < module->import_count; i++) {
        const ZirModule *dependency = module->imports[i].resolved_module;
        if(dependency == NULL || !ModuleNeedsStartup(dependency)) continue;
        for(int m = 0; m < program->module_count; m++)
            if(dependency == &program->modules[m] &&
               !initialize_module_startup(vm, program, m, state))
                return 0;
    }
    for(int f = 0; f < module->function_count; f++)
        if(module->functions[f].is_global_initializer) {
            run_function(vm, module, &module->functions[f], NULL, 0);
            if(vm->failed) return 0;
        }
    state[index] = 2;
    return 1;
}
/* Buffers initialize_globals keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct InitializeGlobalsBuffers {
    ZirFunction probe;
} InitializeGlobalsBuffers;

static int initialize_globals(Vm *vm, const ZirProgram *program);

static int
initialize_globals_with_buffers(Vm *vm, const ZirProgram *program, InitializeGlobalsBuffers *buffers)
{
    int count = 0;
    for(int m = 0; m < program->module_count; m++) {
        if(program->modules[m].global_count < 0 ||
           program->modules[m].global_count > VM_MAX_GLOBALS - count)
            return 0;
        count += program->modules[m].global_count;
    }
    if(count == 0)
        return 1;
    vm->globals = calloc((size_t)count, sizeof(*vm->globals));
    if(vm->globals == NULL)
        return 0;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int g = 0; g < module->global_count; g++) {
            GlobalSlot *slot = &vm->globals[vm->global_count++];
            slot->module = module;
            slot->declaration = &module->globals[g];
            slot->type = KeepName(slot->declaration->type);
            slot->value = default_value(vm, module,
                                        slot->declaration->type, 0);
            if(!vm->failed && slot->declaration->init[0]) {
                const char *init = skip_ws(slot->declaration->init);
                if(slot->value.kind == VALUE_ARRAY ||
                   slot->value.kind == VALUE_RECORD ||
                   slot->value.kind == VALUE_SLOT) {
                    memset(&buffers->probe, 0, sizeof(buffers->probe));
                    int root = ParseExprTyped(&buffers->probe, module,
                                               slot->declaration->init,
                                               slot->declaration->span,
                                               slot->declaration->type);
                    int matched = root >= 0 &&
                        fold_global_element(vm, module, &buffers->probe, root,
                                            &slot->value,
                                            slot->declaration->type,
                                            slot->declaration->span);
                    free(buffers->probe.exprs);
                    if(!matched)
                        vm->failed = 1;
                } else if(!strcmp(slot->declaration->type, "string")) {
                    slot->value = global_literal_string(vm, init);
                } else {
                    long folded = 0;
                    if(EvaluateCompileExpression(module, init,
                                                slot->declaration->span, 0,
                                                &folded)) {
                        Value coerced = coerce(vm, module,
                                               int_value(folded),
                                               slot->declaration->type);
                        if(!vm->failed)
                            slot->value = coerced;
                    } else if(!strcmp(slot->declaration->type,
                                      "float32") ||
                              !strcmp(slot->declaration->type,
                                      "float64")) {
                        char *end = NULL;
                        double number = strtod(init, &end);
                        if(end != init && *skip_ws(end) == '\0')
                            slot->value = real_value(number);
                        else
                            vm->failed = 1;
                    } else if(!strcmp(slot->declaration->type, "bool")) {
                        slot->value = int_value(
                            strcmp(init, "true") == 0);
                    } else
                        vm->failed = 1;
                }
                if(vm->failed)
                    return 0;
            }
            if(vm->failed)
                return 0;
        }
    }
    unsigned char *state = calloc((size_t)program->module_count, 1);
    if(state == NULL) return 0;
    int initialized = 1;
    for(int m = 0; m < program->module_count && initialized; m++)
        if(ModuleNeedsStartup(&program->modules[m]))
            initialized = initialize_module_startup(vm, program, m, state);
    free(state);
    return initialized;
}

static int
initialize_globals(Vm *vm, const ZirProgram *program)
{
    InitializeGlobalsBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    int returned = initialize_globals_with_buffers(vm, program, buffers);
    free(buffers);
    return returned;
}

/* Cached call workspaces are instance-owned rather than thread-owned, so
 * serial thread handoffs and close on a different worker release them too. */
static void
free_runtime_buffers(Vm *vm)
{
    while(vm->run_buffer_count > 0) {
        int slot = --vm->run_buffer_count;
        free(vm->run_buffers[slot]->locals);
        free(vm->run_buffers[slot]);
        vm->run_buffers[slot] = NULL;
    }
    while(vm->setup_buffer_count > 0) {
        int slot = --vm->setup_buffer_count;
        free(vm->setup_buffers[slot]);
        vm->setup_buffers[slot] = NULL;
    }
}

VmInstance *
VmInstanceOpen(const ZirProgram *program, const char *entry_module,
               const char *entry_function, VmHostCall host, void *context)
{
    if(!VmVerify(program, entry_module, entry_function))
        return NULL;
    VmInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL)
        return NULL;
    instance->vm.program = program;
    instance->vm.host = host;
    instance->vm.host_context = context;
    profile_open(&instance->vm);
    instance->entry = find_entry(program, entry_module, entry_function,
                                 &instance->module);
    if(instance->entry == NULL ||
       !initialize_globals(&instance->vm, program)) {
        VmInstanceClose(instance);
        return NULL;
    }
    return instance;
}

int
VmInstanceRun(VmInstance *instance, long long *result, int *has_result)
{
    if(instance == NULL || result == NULL || has_result == NULL ||
       instance->vm.failed)
        return 0;
    Vm *vm = &instance->vm;
    vm->steps = 0;
    vm->stack_floor = NULL;
#ifdef VM_KNOWS_STACK
    pthread_attr_t attributes;
    if(pthread_getattr_np(pthread_self(), &attributes) == 0) {
        void *low = NULL;
        size_t size = 0;
        const char *here = __builtin_frame_address(0);
        if(pthread_attr_getstack(&attributes, &low, &size) == 0 &&
           here > (const char *)low + 2 * VM_STACK_MARGIN)
            vm->stack_floor = (const char *)low + VM_STACK_MARGIN;
        pthread_attr_destroy(&attributes);
    }
#endif
    Value value = run_function(vm, instance->module, instance->entry,
                               NULL, 0);
    if(vm->profile != NULL &&
       profile_clock() - vm->profile->last_report >= (uint64_t)CLOCKS_PER_SEC)
        profile_report(vm, 0);
    *result = value.integer;
    *has_result = strcmp(instance->entry->return_type, "void") != 0;
    if(vm->failed) {
        if(vm->max_steps > 0 && vm->steps > vm->max_steps)
            Diagnostic(instance->entry->span, "zib.runtime",
                       "portable execution failed: stopped after %d statements",
                       vm->max_steps);
        else if(vm->depth_exceeded && vm->stack_floor != NULL)
            Diagnostic(instance->entry->span, "zib.runtime",
                       "portable execution failed: %d nested calls filled the stack",
                       vm->depth_exceeded);
        else if(vm->depth_exceeded)
            Diagnostic(instance->entry->span, "zib.runtime",
                       "portable execution failed: calls nested deeper than %d",
                       VM_MAX_DEPTH);
        else
            Diagnostic(instance->entry->span, "zib.runtime",
                       "portable execution failed");
        return 0;
    }
    return 1;
}

void
VmInstanceLimitSteps(VmInstance *instance, int max_steps)
{
    if(instance != NULL)
        instance->vm.max_steps = max_steps > 0 ? max_steps : 0;
}

size_t
VmInstanceLiveValueBytes(const VmInstance *instance)
{
    return instance == NULL ? 0 :
        instance->vm.record_bytes + instance->vm.array_bytes + instance->vm.string_bytes;
}

void
VmInstanceClose(VmInstance *instance)
{
    if(instance == NULL)
        return;
    profile_close(&instance->vm);
    free_records(&instance->vm);
    free_arrays(&instance->vm);
    free_strings(&instance->vm);
    free_layouts(&instance->vm);
    free_signatures(&instance->vm);
    free(instance->vm.vec_types);
    free(instance->vm.call_sites);
    free(instance->vm.global_sites);
    free(instance->vm.type_sites);
    free(instance->vm.constant_sites);
    free(instance->vm.globals);
    free_runtime_buffers(&instance->vm);
    free_evaluation_buffers(&instance->vm);
    free(instance);
}

int
VmRunWithHost(const ZirProgram *program, const char *entry_module,
              const char *entry_function, VmHostCall host, void *context,
              long long *result, int *has_result)
{
    return VmRunBounded(program, entry_module, entry_function, host, context,
                        0, result, has_result);
}

int
VmRunBounded(const ZirProgram *program, const char *entry_module,
             const char *entry_function, VmHostCall host, void *context,
             int max_steps, long long *result, int *has_result)
{
    VmInstance *instance = VmInstanceOpen(program, entry_module,
        entry_function, host, context);
    if(instance == NULL)
        return 0;
    VmInstanceLimitSteps(instance, max_steps);
    int ok = VmInstanceRun(instance, result, has_result);
    VmInstanceClose(instance);
    return ok;
}

int
VmRun(const ZirProgram *program, const char *entry_module,
      const char *entry_function, long long *result, int *has_result)
{
    return VmRunWithHost(program, entry_module, entry_function,
                         NULL, NULL, result, has_result);
}
