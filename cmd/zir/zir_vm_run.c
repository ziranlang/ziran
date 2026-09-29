#include "zir_vm_internal.h"

static void collect_unreachable(Vm *vm);

static Flow
execute_sequence(Frame *frame, int begin, int end, int depth,
                 Value *result)
{
    Vm *vm = frame->vm;
    const ZirFunction *function = frame->function;
    if(depth >= VM_MAX_DEPTH)
        return FLOW_ERROR;
    for(int s = begin; s < end && !vm->failed; s++) {
        /* At a top-level statement boundary no expression temporaries are
         * live outside Frame.locals. Batch reclamation here instead of
         * walking growing globals after every helper call. */
        if(vm->depth == 1 &&
           vm->allocated_since_collection >= 4 * 1024 * 1024)
            collect_unreachable(vm);
        const ZirStmt *statement = &function->stmts[s];
        int close;
        int saved_locals;
        Flow flow;
        if(++vm->steps > VM_MAX_STEPS)
            return FLOW_ERROR;
        switch(statement->kind) {
        case ZIR_STMT_DECL: {
            Value value = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) :
                default_value(vm, frame->module, statement->type, 0);
            if(vm->failed || frame->local_count >= VM_MAX_LOCALS)
                return FLOW_ERROR;
            Local *local = &frame->locals[frame->local_count++];
            copy_text(local->name, sizeof(local->name), statement->name);
            copy_text(local->type, sizeof(local->type), statement->type);
            local->value = coerce(vm, frame->module, value, statement->type);
            break;
        }
        case ZIR_STMT_ASSIGN: {
            Record *union_record = NULL;
            const char *union_type = NULL;
            Value *slot = assignment_slot(frame, statement->lhs_root, 0);
            if(frame->union_write_record != NULL) {
                union_record = frame->union_write_record;
                union_type = frame->union_write_type;
                frame->union_write_record = NULL;
                frame->union_write_type = NULL;
            }
            if(slot == NULL)
                return FLOW_ERROR;
            /* A compound assignment reads its target before the right side
             * runs, as native targets do: x += Bump() adds to the old x. */
            int compound = strcmp(statement->assignment_op, "=") != 0;
            Value current = {0};
            if(compound)
                current = union_record != NULL ?
                    union_member_read(vm, union_record, union_type) : *slot;
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
                coerce(vm, frame->module, right,
                       function->exprs[statement->lhs_root].type);
            if(vm->failed)
                return FLOW_ERROR;
            if(union_record != NULL) {
                if(!union_member_write(vm, union_record, union_type,
                                       replacement))
                    return FLOW_ERROR;
                break;
            }
            Value previous = *slot;
            if(previous.kind == VALUE_ARRAY && previous.array != NULL &&
               replacement.kind == VALUE_ARRAY && replacement.array != NULL &&
               previous.array->length == replacement.array->length &&
               array_has_active_slice(vm, previous.array)) {
                /* A live slice borrows the array's storage. Native array
                 * assignment updates that storage, so keep its identity and
                 * move the replacement elements into the existing slots. */
                for(int element = 0; element < previous.array->length; element++) {
                    Value old = previous.array->elements[element];
                    previous.array->elements[element] =
                        replacement.array->elements[element];
                    replacement.array->elements[element] = old;
                }
                retire_value(vm, replacement, 0);
                release_retired(vm);
                break;
            }
            *slot = replacement;
            if(previous.kind == VALUE_RECORD || previous.kind == VALUE_ARRAY) {
                retire_value(vm, previous, 0);
                release_retired(vm);
            }
            break;
        }
        case ZIR_STMT_RETURN:
            *result = statement->expr_root >= 0 ?
                eval(frame, statement->expr_root, 0) : int_value(0);
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
               VecElementType(frame->module,
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
                if(++vm->steps > VM_MAX_STEPS)
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
    out->type = type;
    char element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        if(capacity < 0 || value.kind != VALUE_ARRAY || value.array == NULL ||
           value.array->length != capacity ||
           strcmp(value.array->element_type, element) != 0)
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
                              value.array->elements[i],
                              &out->elements[i], depth + 1))
                return 0;
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        if(value.kind != VALUE_SLICE ||
           (value.length > 0 && value.array == NULL) ||
           (value.array != NULL &&
            (strcmp(value.array->element_type, element) != 0 ||
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
                              value.array->elements[value.offset + i],
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
    out->integer = value.integer;
    out->bits = value.bits;
    out->real = value.real;
    out->data = value.data;
    out->length = value.length;
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
            array->elements[i] = host_return(vm, module, array_element,
                                              &input->elements[i], depth + 1);
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
        size_t offset = 0;
        ZirTypeField field;
        int count = 0, status;
        while((status = TypeNextField(declared, &offset, &field)) == 1)
            count++;
        if(status < 0 || count != (int)input->field_count) {
            vm->failed = 1;
            return result;
        }
        Record *record = allocate_record(vm, owner, declared, count);
        if(record == NULL)
            return result;
        offset = 0;
        for(int i = 0; i < count && !vm->failed; i++) {
            if(TypeNextField(declared, &offset,
                             &record->fields[i].field) != 1 ||
               input->fields[i].name == NULL ||
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
           input->length >
               (VM_MAX_ARRAY_BYTES - sizeof(Array)) / sizeof(Value) ||
           input->field_count != 0 || input->fields != NULL) {
            vm->failed = 1;
            return result;
        }
        owned = allocate_array_try(vm, module, slice_element,
                                   (int)input->length, 1);
        if(owned == NULL)
            return result;
        for(size_t i = 0; i < input->length && !vm->failed; i++)
            owned->elements[i] = host_return(vm, module, slice_element,
                                             &input->elements[i], depth + 1);
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
    else if(kind == VM_HOST_STRING)
        result = string_value(input->data, input->length);
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
            Value *slot = &target.array->elements[target.offset + i];
            Value previous = *slot;
            *slot = updates[i];
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
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

static int
function_uses_slots(const ZirFunction *function)
{
    for(int i = 0; i < function->expr_count; i++) {
        if(function->exprs[i].is_function_value ||
           function->exprs[i].slot_type[0])
            return 1;
    }
    return 0;
}

/* A record or array parameter can share its caller's value only when the
 * callee cannot write through that parameter. Other storage paths still copy
 * values, so passing a parameter onward to a mutating function stays safe. */
static int
parameter_read_only(const ZirFunction *function, const char *name)
{
    if(function_uses_slots(function))
        return 0;
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

/* Globals can receive values during a call. Keep every allocation reachable
 * from them when reclaiming completed call temporaries. */
void
pin_value(Vm *vm, Value value, int depth)
{
    if(depth >= VM_MAX_DEPTH)
        return;
    if(value.kind == VALUE_RECORD && value.record != NULL &&
       value.record->pinned != vm->pin_generation) {
        value.record->pinned = vm->pin_generation;
        for(int i = 0; i < value.record->field_count; i++)
            pin_value(vm, value.record->fields[i].value, depth + 1);
    } else if(value.kind == VALUE_ARRAY && value.array != NULL &&
              value.array->pinned != vm->pin_generation) {
        value.array->pinned = vm->pin_generation;
        for(int i = 0; i < value.array->length; i++)
            pin_value(vm, value.array->elements[i], depth + 1);
    } else if(value.kind == VALUE_SLICE && value.array != NULL) {
        pin_value(vm, (Value){.kind = VALUE_ARRAY, .array = value.array},
                  depth + 1);
    }
}

static void
pin_globals(Vm *vm)
{
    for(int i = 0; i < vm->global_count; i++)
        pin_value(vm, vm->globals[i].value, 0);
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
            pin_value(vm, frame->locals[i].value, 0);
}

static void
collect_unreachable(Vm *vm)
{
    uint64_t remaining_retired = 0;
    vm->pin_generation++;
    pin_globals(vm);
    pin_active_frames(vm);
    Record **record = &vm->records;
    while(*record != NULL) {
        Record *current = *record;
        if(current->pinned != vm->pin_generation) {
            *record = current->next;
            vm->record_bytes -= sizeof(Record) +
                (size_t)current->field_count * sizeof(RecordField);
            free(current);
        } else {
            if(current->retired &&
               (remaining_retired == 0 ||
                current->allocation < remaining_retired))
                remaining_retired = current->allocation;
            record = &current->next;
        }
    }
    Array **array = &vm->arrays;
    while(*array != NULL) {
        Array *current = *array;
        if(current->pinned != vm->pin_generation) {
            *array = current->next;
            vm->array_bytes -= sizeof(Array) +
                (size_t)current->length * sizeof(Value);
            free(current);
        } else {
            if(current->retired &&
               (remaining_retired == 0 ||
                current->allocation < remaining_retired))
                remaining_retired = current->allocation;
            array = &current->next;
        }
    }
    vm->retire_floor = remaining_retired;
    vm->allocated_since_collection = 0;
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
            *record = current->next;
            vm->record_bytes -= sizeof(Record) +
                (size_t)current->field_count * sizeof(RecordField);
            free(current);
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
            *array = current->next;
            vm->array_bytes -= sizeof(Array) +
                (size_t)current->length * sizeof(Value);
            free(current);
        } else {
            if(current->allocation <= entry)
                break;
            array = &current->next;
        }
    }
}
/* Buffers run_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RunFunctionBuffers {
    Frame frame;
    Parameter parameters[VM_MAX_PARAMS];
    VmHostValue host_args[VM_MAX_PARAMS];
} RunFunctionBuffers;

Value run_function(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count);

static Value
run_function_with_buffers(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count, RunFunctionBuffers *buffers)
{
    memset(&buffers->frame, 0, sizeof(buffers->frame));
    int count = parse_parameters(module, function, buffers->parameters);
    Value result = int_value(0);
    uint64_t allocation_entry = vm->allocation;
    if(vm->failed || vm->depth >= VM_MAX_DEPTH || count != arg_count) {
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
        memset(buffers->host_args, 0, sizeof(buffers->host_args));
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
                                               buffers->parameters[i].type);
            if(vm->failed || !host_argument(module,
                                            buffers->parameters[i].type, argument,
                                            &buffers->host_args[i], 0)) {
                vm->failed = 1;
                break;
            }
        }
        host_result.type = function->return_type;
        if(!vm->failed && !vm->host(vm->host_context, module->name,
                                    function->name, buffers->host_args, count,
                                    &host_result))
            vm->failed = 1;
        for(int i = 0; i < count && !vm->failed; i++)
            host_copy_back(vm, module, buffers->parameters[i].type,
                           args[i], &buffers->host_args[i]);
        for(int i = 0; i < count; i++)
            release_host_argument(&buffers->host_args[i], 0);
        if(vm->failed)
            return result;
        return host_return(vm, module, function->return_type,
                           &host_result, 0);
    }
    vm->depth++;
    buffers->frame.vm = vm;
    buffers->frame.module = module;
    buffers->frame.function = function;
    buffers->frame.caller = vm->active_frame;
    vm->active_frame = &buffers->frame;
    for(int i = 0; i < count; i++) {
        copy_text(buffers->frame.locals[i].name, sizeof(buffers->frame.locals[i].name),
                  buffers->parameters[i].name);
        copy_text(buffers->frame.locals[i].type, sizeof(buffers->frame.locals[i].type),
                  buffers->parameters[i].type);
        buffers->frame.locals[i].value = !VecElementType(module, buffers->parameters[i].type,
                                                NULL, 0) &&
                                parameter_read_only(function,
                                                    buffers->parameters[i].name) ?
            coerce_expression(vm, module, args[i], buffers->parameters[i].type) :
            coerce(vm, module, args[i], buffers->parameters[i].type);
    }
    buffers->frame.local_count = count;
    Flow flow = execute_sequence(&buffers->frame, 0, function->stmt_count, 0, &result);
    if(flow == FLOW_ERROR || flow == FLOW_BREAK || flow == FLOW_CONTINUE ||
       (flow != FLOW_RETURN && strcmp(function->return_type, "void") != 0))
        vm->failed = 1;
    uint64_t allocation_before_result = vm->allocation;
    Value returned = coerce(vm, module, result, function->return_type);
    /* Nested return paths leave their locals in this frame. Materialize the
     * return value before releasing those bindings. */
    drop_owned_locals(&buffers->frame, 0);
    vm->active_frame = buffers->frame.caller;
    vm->depth--;
    /* Named Ziran procedure values carry no borrowed frame context. The
     * returned value has already been copied, so slot-using calls can drop
     * temporaries by the same reachability rule as direct calls. */
    /* Nested calls are collected at a top-level statement boundary. A call
     * with unusually large live storage still reclaims its own temporaries
     * before the VM's allocation limits are reached. */
    if(!vm->failed && (vm->depth == 0 ||
       vm->record_bytes > VM_MAX_RECORD_BYTES / 2 ||
       vm->array_bytes > VM_MAX_ARRAY_BYTES / 2)) {
        vm->pin_generation++;
        pin_globals(vm);
        pin_active_frames(vm);
        pin_value(vm, returned, 0);
        release_call_records(vm, allocation_entry, allocation_before_result);
        release_call_arrays(vm, allocation_entry, allocation_before_result);
    }
    return returned;
}

Value
run_function(Vm *vm, const ZirModule *module, const ZirFunction *function,
             const Value *args, int arg_count)
{
    static _Thread_local RunFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    RunFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    Value returned = run_function_with_buffers(vm, module, function, args, arg_count, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

void
free_records(Vm *vm)
{
    while(vm->records != NULL) {
        Record *next = vm->records->next;
        free(vm->records);
        vm->records = next;
    }
}

void
free_arrays(Vm *vm)
{
    while(vm->arrays != NULL) {
        Array *next = vm->arrays->next;
        free(vm->arrays);
        vm->arrays = next;
    }
}

void
free_strings(Vm *vm)
{
    while(vm->strings != NULL) {
        StringLiteral *next = vm->strings->next;
        free(vm->strings);
        vm->strings = next;
    }
}

/* Fold one parsed initializer expression into a global's storage slot.
 * Types come from the declared global, never the probe expression (parsed
 * standalone, so it carries no inferred types). Scalars, string literals,
 * records, and arrays recurse through the declared storage shape. */
int
fold_global_element(Vm *vm, const ZirModule *module, const ZirFunction *probe,
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
            if(entry->right < 0 || position >= target->array->length ||
               !fold_global_element(vm, module, probe, entry->right,
                                    &target->array->elements[position],
                                    element, span))
                return 0;
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
            matched = fold_global_element(vm, module, probe,
                                          entry->right, field, field_type,
                                          span);
        }
        return matched;
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
    for(int i = 0; i < module->import_count; i++) {
        const ZirModule *dependency = module->imports[i].resolved_module;
        if(dependency == NULL) continue;
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
            slot->value = default_value(vm, module,
                                        slot->declaration->type, 0);
            if(!vm->failed && slot->declaration->init[0]) {
                const char *init = skip_ws(slot->declaration->init);
                if(slot->value.kind == VALUE_ARRAY ||
                   slot->value.kind == VALUE_RECORD) {
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
        initialized = initialize_module_startup(vm, program, m, state);
    free(state);
    return initialized;
}

static int
initialize_globals(Vm *vm, const ZirProgram *program)
{
    static _Thread_local InitializeGlobalsBuffers *spares[16];
    static _Thread_local int spare_count;
    InitializeGlobalsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = initialize_globals_with_buffers(vm, program, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
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
    Value value = run_function(vm, instance->module, instance->entry,
                               NULL, 0);
    *result = value.integer;
    *has_result = strcmp(instance->entry->return_type, "void") != 0;
    if(vm->failed) {
        Diagnostic(instance->entry->span, "zib.runtime",
                      "portable execution failed");
        return 0;
    }
    return 1;
}

size_t
VmInstanceLiveValueBytes(const VmInstance *instance)
{
    return instance == NULL ? 0 :
        instance->vm.record_bytes + instance->vm.array_bytes;
}

void
VmInstanceClose(VmInstance *instance)
{
    if(instance == NULL)
        return;
    free_records(&instance->vm);
    free_arrays(&instance->vm);
    free_strings(&instance->vm);
    free(instance->vm.globals);
    free(instance);
}

int
VmRunWithHost(const ZirProgram *program, const char *entry_module,
              const char *entry_function, VmHostCall host, void *context,
              long long *result, int *has_result)
{
    VmInstance *instance = VmInstanceOpen(program, entry_module,
        entry_function, host, context);
    if(instance == NULL)
        return 0;
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
