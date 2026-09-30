#include "zir_check_internal.h"

static int declared_application_valid(const ZirModule *module, const char *text, int depth);

static int
record_declaration_error(const ZirType *record, const char *message,
                         const char *field)
{
    Diagnostic(record->span, "check.record", "%s: %s%s%s",
            message, record->name, field && *field ? "." : "",
            field ? field : "");
    return 0;
}

const char *
storage_type_error(const ZirModule *module, const char *source,
                   const RecordPath *path, int indirect, ValidatedRecords *checked,
                   char *detail, size_t detail_capacity)
{
    char type[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record;
    int capacity;

    copy_text(type, sizeof(type), source);
    trim_in_place(type);
    normalize_array(module, type, sizeof(type));
    if(!strcmp(type, "void"))
        return indirect ? NULL : "stored values cannot have void type";
    {
        char slice_element[ZIR_NAME_MAX];
        if(SliceElementType(type, slice_element, sizeof(slice_element))) {
            if(SliceElementType(slice_element, NULL, 0) ||
               !strcmp(slice_element, "char") ||
               !strcmp(slice_element, "const char"))
                return "nested slice descriptors are not storable";
            return storage_type_error(module, slice_element, path, 1,
                                      checked, detail, detail_capacity);
        }
    }
    if(*ScalarType(type) != '\0' || TargetType(type, ZIR_C) != NULL)
        return NULL;
    if(type[0] == '*') {
        const char *pointee = skip_ws(type + 1);
        if(!*pointee)
            return "pointer requires an element type";
        return storage_type_error(module, pointee, path, 1, checked,
                                  detail, detail_capacity);
    }
    if(type[0] == '[') {
        if(type[1] == ']')
            return "slices do not yet have portable storage semantics";
        if(!ArrayElementType(type, element, sizeof(element), &capacity))
            return "malformed fixed array type";
        if(capacity < 0) {
            int status = array_capacity(module, type, &capacity);
            if(status < 0)
                return "array capacity is not a valid bounded integer constant";
            if(status == 0)
                return "array capacity requires a known integer constant";
        }
        return storage_type_error(module, element, path, indirect, checked,
                                  detail, detail_capacity);
    }
    size_t length = strlen(type);
    if(length > 0 && type[length - 1] == '*') {
        type[length - 1] = '\0';
        trim_in_place(type);
        if(!strncmp(type, "const ", 6))
            memmove(type, type + 6, strlen(type + 6) + 1);
        return storage_type_error(module, type, path, 1, checked,
                                  detail, detail_capacity);
    }
    record = FindType(module, type, &owner);
    if(record == NULL) {
        if(indirect && declared_application_valid(module, type, 0))
            return NULL;
        if(detail != NULL && detail_capacity > 0)
            copy_text(detail, detail_capacity, type);
        return "unknown stored type";
    }
    if(record->is_record_template)
        return "generic types require a concrete specialization";
    if(record->is_procedure_type)
        return NULL;
    if(record->is_enum || record->is_map || indirect)
        return NULL;
    for(const RecordPath *ancestor = path; ancestor != NULL; ancestor = ancestor->parent) {
        if(ancestor->record == record)
            return "record has a recursive value layout";
    }
    for(size_t i = 0; i < checked->count; i++) {
        if(checked->items[i] == record)
            return NULL;
    }
    if(path != NULL && path->depth >= 128)
        return "record nesting exceeds the portable limit";
    RecordPath current = {record, path, path == NULL ? 1 : path->depth + 1};
    ZirTypeField field;
    size_t offset = 0;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        const char *error = storage_type_error(owner, field.type, &current, 0,
                                               checked, detail, detail_capacity);
        if(error != NULL)
            return error;
    }
    if(status < 0)
        return "malformed record field";
    const ZirType **items = realloc(checked->items, (checked->count + 1) * sizeof(*items));
    if(items == NULL)
        return "cannot allocate record type validation state";
    checked->items = items;
    checked->items[checked->count++] = record;
    return NULL;
}

const char *
local_storage_error(const ZirModule *module, const char *type)
{
    ValidatedRecords checked = {0};
    char element[ZIR_NAME_MAX];
    int slice = SliceElementType(type, element, sizeof(element));
    const char *problem;
    if(slice && (element[0] == '[' || !strcmp(element, "char") || !strcmp(element, "const char")))
        problem = "unsupported slice element type";
    else
        problem = storage_type_error(module, slice ? element : type, NULL, 0,
                                     &checked, NULL, 0);
    free(checked.items);
    return problem;
}
/* Buffers declared_application_valid keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct DeclaredApplicationValidBuffers {
    char arguments[ZIR_TEXT_MAX];
    char actual[16][ZIR_NAME_MAX];
    char parameters[16][ZIR_NAME_MAX];
} DeclaredApplicationValidBuffers;

static int declared_application_valid(const ZirModule *module, const char *text, int depth);

static int
declared_application_valid_with_buffers(const ZirModule *module, const char *text, int depth, DeclaredApplicationValidBuffers *buffers)
{
    char name[ZIR_NAME_MAX];
    size_t length = 0;
    const char *cursor = skip_ws(text);
    if(depth > 16) return 0;
    while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
          length + 1 < sizeof(name))
        name[length++] = *cursor++;
    name[length] = '\0';
    cursor = skip_ws(cursor);
    if(length == 0 || *cursor++ != '(') return 0;
    const char *start = cursor;
    int nesting = 1;
    while(*cursor && nesting) {
        if(*cursor == '(') nesting++;
        else if(*cursor == ')') nesting--;
        if(nesting) cursor++;
    }
    if(nesting || *skip_ws(cursor + 1) != '\0' ||
       (size_t)(cursor - start) >= sizeof(buffers->arguments)) return 0;
    memcpy(buffers->arguments, start, (size_t)(cursor - start));
    buffers->arguments[cursor - start] = '\0';
    const ZirType *generic = FindType(module, name, NULL);
    if(generic == NULL || !generic->is_record_template)
        return 0;
    int expected = split_top_level(generic->template_params, buffers->parameters[0],
                                   16, sizeof(buffers->parameters[0]));
    int count = split_top_level(buffers->arguments, buffers->actual[0],
                                16, sizeof(buffers->actual[0]));
    if(count != expected || count < 1 || count >= 16) return 0;
    for(int i = 0; i < count; i++)
        if(!declared_application_valid(module, buffers->actual[i], depth + 1) &&
           local_storage_error(module, buffers->actual[i]) != NULL)
            return 0;
    return 1;
}

static int
declared_application_valid(const ZirModule *module, const char *text, int depth)
{
    static _Thread_local DeclaredApplicationValidBuffers *spares[16];
    static _Thread_local int spare_count;
    DeclaredApplicationValidBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = declared_application_valid_with_buffers(module, text, depth, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers check_type_declarations keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CheckTypeDeclarationsBuffers {
    char parameters[16][ZIR_NAME_MAX];
    char concrete[16][ZIR_NAME_MAX];
    char parameters_2[64][ZIR_TEXT_MAX];
} CheckTypeDeclarationsBuffers;

int check_type_declarations(ZirModule *module);

static int
check_go_binding(const ZirModule *module, const ZirImport *binding,
                  char parameters[][ZIR_TEXT_MAX])
{
    if(binding->kind != ZIR_IMPORT_EXTERN || binding->extern_kind != ZIR_EXTERN_GO)
        return 1;
    char package[ZIR_PATH_MAX], receiver[ZIR_NAME_MAX], symbol[ZIR_NAME_MAX];
    if(!GoForeignCallParts(binding->target, package, sizeof(package), receiver,
                           sizeof(receiver), symbol, sizeof(symbol))) {
        Diagnostic(binding->span, "check.foreign", "invalid Go foreign target");
        return 0;
    }
    int count = *skip_ws(binding->args) ?
        split_top_level(binding->args, parameters[0], 64, ZIR_TEXT_MAX) : 0;
    if(binding->go_field && (!receiver[0] || count != 1 || binding->is_varargs ||
                            binding->go_results || !strcmp(binding->return_type, "void"))) {
        Diagnostic(binding->span, "check.foreign", "#go_field requires one receiver and a field value result");
        return 0;
    }
    if(binding->go_results) {
        const ZirType *record = FindType(module, binding->return_type, NULL);
        size_t offset = 0;
        ZirTypeField field;
        int fields = 0, status = -1;
        if(record && !record->is_extern && !record->is_enum && !record->is_union &&
           !record->is_owned_vec && !record->is_map && !record->is_record_template &&
           !record->is_procedure_type && !contains_vec(module, record->name, 0)) {
            while((status = TypeNextField(record, &offset, &field)) == 1) {
                if(field.is_using || !strcmp(field.name, "_")) { status = -1; break; }
                fields++;
            }
        }
        if(!fields || status != 0 || binding->is_varargs || !strcmp(package, "builtin")) {
            Diagnostic(binding->span, "check.foreign", "#go_results requires a concrete result record without owned storage");
            return 0;
        }
    }
    if(receiver[0]) {
        const char *colon = count > 0 ? strchr(parameters[0], ':') : NULL;
        const char *type = colon ? skip_ws(colon + 1) : "";
        if((type[0] == '*') != (receiver[0] == '*')) {
            Diagnostic(binding->span, "check.foreign", "Go method requires its declared receiver as the first parameter");
            return 0;
        }
        const ZirType *declared = FindType(module, type + (type[0] == '*'), NULL);
        char target[ZIR_PATH_MAX];
        snprintf(target, sizeof(target), "go:%s.%s", package, receiver + (receiver[0] == '*'));
        if(!declared || strcmp(declared->foreign_target, target)) {
            Diagnostic(binding->span, "check.foreign", "Go method receiver does not match its foreign type");
            return 0;
        }
    }
    if(!strcmp(package, "builtin")) {
        const char *result = skip_ws(binding->return_type);
        int allocation = !strcmp(symbol, "new") && count == 0 && result[0] == '*' &&
            strcmp(skip_ws(result + 1), "void") && local_storage_error(module, result + 1) == NULL;
        if(!strcmp(symbol, "make")) {
            allocation = SliceElementType(result, NULL, 0) ? count == 1 || count == 2 :
                MapTypeParts(module, result, NULL, 0, NULL, 0) && (count == 0 || count == 1);
            for(int i = 0; i < count && allocation; i++) {
                const char *colon = strchr(parameters[i], ':');
                allocation = colon && integer_type(skip_ws(colon + 1));
            }
            allocation = allocation && local_storage_error(module, result) == NULL;
        }
        if(!strcmp(symbol, "string")) {
            const char *colon = count == 1 ? strchr(parameters[0], ':') : NULL;
            allocation = colon && !strcmp(skip_ws(colon + 1), "[]u8") && !strcmp(result, "string");
        }
        if(!strcmp(symbol, "len")) {
            const char *colon = count == 1 ? strchr(parameters[0], ':') : NULL;
            const char *value_type = colon ? skip_ws(colon + 1) : "";
            const ZirType *foreign = FindType(module, value_type, NULL);
            allocation = colon && !strcmp(result, "isize") &&
                (!strcmp(value_type, "string") || SliceElementType(value_type, NULL, 0) ||
                 ArrayElementType(value_type, NULL, 0, NULL) ||
                 MapTypeParts(module, value_type, NULL, 0, NULL, 0) ||
                 (foreign && foreign->is_extern && foreign->foreign_target[0] &&
                  strncmp(foreign->foreign_target, "go:builtin.", 11)));
        }
        if(!strcmp(symbol, "append")) {
            const char *slice_colon = count == 2 ? strchr(parameters[0], ':') : NULL;
            const char *value_colon = count == 2 ? strchr(parameters[1], ':') : NULL;
            char element[ZIR_NAME_MAX];
            allocation = slice_colon && value_colon &&
                !strcmp(skip_ws(slice_colon + 1), result) &&
                SliceElementType(result, element, sizeof(element)) &&
                !strcmp(skip_ws(value_colon + 1), element) &&
                !contains_vec(module, element, 0);
        }
        if(!allocation) {
            Diagnostic(binding->span, "check.foreign", "Go builtin requires a valid new, make, string, len or append signature");
            return 0;
        }
    }
    return 1;
}

static int
check_type_declarations_with_buffers(ZirModule *module, CheckTypeDeclarationsBuffers *buffers)
{
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *record = &module->types[i];
        select_lookup_file(module, record->span);
        size_t offset = 0;
        int status;
        ZirTypeField field;
        if(record->name[0] == '#')
            return record_declaration_error(record,
                "retired compiler-generated type", NULL);
        if(BuiltinType(record->name) != NULL)
            return record_declaration_error(record,
                "built-in type cannot be redeclared", NULL);
        for(int previous = 0; previous < i; previous++) {
            const ZirType *other = &module->types[previous];
            if(strcmp(record->name, other->name) == 0 &&
               !(strcmp(SpanPath(record->span), SpanPath(other->span)) != 0 &&
                 (record->is_file_private || other->is_file_private)))
                return record_declaration_error(record, "duplicate type declaration", NULL);
        }
        if(record->is_record_template) {
            int parameter_count = split_top_level(record->template_params,
                buffers->parameters[0], 16, sizeof(buffers->parameters[0]));
            if(parameter_count < 1 || parameter_count >= 16)
                return record_declaration_error(record,
                    "invalid generic type parameters", NULL);
            for(int parameter = 0; parameter < parameter_count; parameter++) {
                const unsigned char *name =
                    (const unsigned char *)buffers->parameters[parameter];
                if(!isalpha(*name) && *name != '_')
                    return record_declaration_error(record,
                        "invalid generic type parameter", buffers->parameters[parameter]);
                for(name++; *name; name++)
                    if(!isalnum(*name) && *name != '_')
                        return record_declaration_error(record,
                            "invalid generic type parameter", buffers->parameters[parameter]);
                for(int earlier = 0; earlier < parameter; earlier++)
                    if(!strcmp(buffers->parameters[earlier], buffers->parameters[parameter]))
                        return record_declaration_error(record,
                            "duplicate generic type parameter", buffers->parameters[parameter]);
                copy_text(buffers->concrete[parameter], sizeof(buffers->concrete[parameter]),
                          "s32");
            }
            int members = 0;
            size_t member_offset = 0;
            {
                ZirTypeField item, previous;
                while((status = TypeNextField(record, &member_offset,
                                              &item)) == 1) {
                    size_t prior_offset = 0;
                    while(prior_offset < member_offset) {
                        if(TypeNextField(record, &prior_offset,
                                         &previous) != 1)
                            return record_declaration_error(record,
                                "malformed generic record field", NULL);
                        if(prior_offset == member_offset) break;
                        if(!strcmp(previous.name, item.name))
                            return record_declaration_error(record,
                                "duplicate generic record field", item.name);
                    }
                    char resolved[ZIR_NAME_MAX];
                    if(!SubstituteGenericType(item.type, resolved,
                            sizeof(resolved), buffers->parameters, buffers->concrete,
                            parameter_count))
                        return record_declaration_error(record,
                            "generic record field type exceeds size limit",
                            item.name);
                    const char *problem = local_storage_error(module, resolved);
                    if(problem != NULL &&
                       !declared_application_valid(module, resolved, 0))
                        return record_declaration_error(record, problem,
                                                        item.name);
                    members++;
                }
            }
            if(status < 0 || members == 0)
                return record_declaration_error(record,
                    "invalid generic type declaration", NULL);
            continue;
        }
        if(record->is_enum)
            continue;
        if(record->is_map) {
            char key[ZIR_NAME_MAX], value[ZIR_NAME_MAX];
            if(!MapTypeParts(module, record->name, key, sizeof(key), value, sizeof(value)) ||
               !MapKeyComparable(module, key, 0))
                return record_declaration_error(record, "map key must be comparable", NULL);
            const char *key_problem = local_storage_error(module, key);
            if(key_problem != NULL)
                return record_declaration_error(record, key_problem, "key");
            const char *problem = local_storage_error(module, value);
            if(problem != NULL || contains_vec(module, value, 0))
                return record_declaration_error(record,
                    problem ? problem : "map values cannot own Vec storage", NULL);
            continue;
        }
        if(record->is_procedure_type) {
            const ZirType *result = FindType(module,
                record->procedure_return_type, NULL);
            const char *result_base = record->procedure_return_type;
            while(*result_base == '*')
                result_base++;
            const ZirType *result_base_type = result != NULL ? result :
                FindType(module, result_base, NULL);
            if(!record->procedure_return_type[0] ||
               (record->is_c_call ?
                (strcmp(record->procedure_return_type, "void") &&
                 local_storage_error(module, record->procedure_return_type) != NULL) :
                (TargetType(record->procedure_return_type, ZIR_C) == NULL &&
                 TargetType(result_base, ZIR_C) == NULL &&
                 result_base_type == NULL)) ||
               (result_base_type && result_base_type->is_procedure_type))
                return record_declaration_error(record,
                    "invalid procedure type result", record->procedure_return_type);
            int count = *skip_ws(record->body) ?
                split_top_level(record->body, buffers->parameters_2[0], 64, sizeof(buffers->parameters_2[0])) : 0;
            for(int parameter = 0; parameter < count; parameter++) {
                char *colon = strchr(buffers->parameters_2[parameter], ':');
                if(colon == NULL)
                    return record_declaration_error(record, "slot parameters require name: type", NULL);
                *colon++ = '\0';
                trim_in_place(buffers->parameters_2[parameter]);
                trim_in_place(colon);
                /* Pointer slot parameters name a pointee type: strip the
                 * leading '*' chain before resolving the base type. */
                const char *base = colon;
                while(*base == '*')
                    base++;
                const ZirType *type = FindType(module, colon, NULL);
                const ZirType *base_type = type != NULL ? type :
                    FindType(module, base, NULL);
                if(!*buffers->parameters_2[parameter] || !strcmp(colon, "void") ||
                   (record->is_c_call ?
                    local_storage_error(module, colon) != NULL :
                    (TargetType(colon, ZIR_C) == NULL &&
                     TargetType(base, ZIR_C) == NULL && base_type == NULL)) ||
                   (base_type && base_type->is_procedure_type))
                    return record_declaration_error(record, "invalid slot parameter", buffers->parameters_2[parameter]);
                for(int previous = 0; previous < parameter; previous++)
                    if(!strcmp(buffers->parameters_2[previous], buffers->parameters_2[parameter]))
                        return record_declaration_error(record, "duplicate slot parameter", buffers->parameters_2[parameter]);
            }
            continue;
        }
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            size_t previous_offset = 0;
            ZirTypeField previous;
            if(strcmp(field.type, "void") == 0)
                return record_declaration_error(record, "record field cannot have void type", field.name);
            if(field.is_using) {
                const char *nested_name = skip_ws(field.type);
                if(*nested_name == '*') nested_name = skip_ws(nested_name + 1);
                const ZirType *nested = FindType(module, nested_name, NULL);
                if(nested == NULL || nested->is_enum ||
                   nested->is_procedure_type || nested->is_record_template || nested->is_map)
                    return record_declaration_error(record,
                        "using field requires a concrete record type", field.name);
            }
            while(TypeNextField(record, &previous_offset, &previous) == 1 &&
                  previous_offset < offset) {
                if(strcmp(previous.name, field.name) == 0)
                    return record_declaration_error(record, "duplicate record field", field.name);
            }
        }
        if(status < 0)
            return record_declaration_error(record, "malformed record field", NULL);
        ValidatedRecords checked = {0};
        char detail[ZIR_NAME_MAX] = "";
        const char *error = storage_type_error(module, record->name, NULL, 0,
                                               &checked, detail, sizeof(detail));
        free(checked.items);
        if(error != NULL)
            return record_declaration_error(record, error,
                                            detail[0] ? detail : NULL);
    }
    for(int i = 0; i < module->import_count; i++) {
        select_lookup_file(module, module->imports[i].span);
        if(!check_go_binding(module, &module->imports[i], buffers->parameters_2))
            return 0;
    }
    return 1;
}

int
check_type_declarations(ZirModule *module)
{
    static _Thread_local CheckTypeDeclarationsBuffers *spares[16];
    static _Thread_local int spare_count;
    CheckTypeDeclarationsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = check_type_declarations_with_buffers(module, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers normalize_record_arrays keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NormalizeRecordArraysBuffers {
    char normalized[sizeof(((ZirTypeField *)0)->type)];
    char body[sizeof(((ZirType *)0)->body)];
} NormalizeRecordArraysBuffers;

int normalize_record_arrays(ZirModule *module);

static int
normalize_record_arrays_with_buffers(ZirModule *module, NormalizeRecordArraysBuffers *buffers)
{
    for(int i = 0; i < module->type_count; i++) {
        ZirType *record = &module->types[i];
        select_lookup_file(module, record->span);
        if(record->is_procedure_type || record->is_enum)
            continue;
        size_t offset = 0;
        ZirTypeField field;
        int status, changed = 0;
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            copy_text(buffers->normalized, sizeof(buffers->normalized), field.type);
            normalize_array(module, buffers->normalized, sizeof(buffers->normalized));
            if(strcmp(buffers->normalized, field.type) != 0)
                changed = 1;
        }
        if(status < 0)
            return 0;
        if(!changed)
            continue;
        buffers->body[0] = '\0';
        size_t used = 0;
        offset = 0;
        while((status = TypeNextField(record, &offset, &field)) == 1) {
            normalize_array(module, field.type, sizeof(field.type));
            int length = snprintf(buffers->body + used, sizeof(buffers->body) - used,
                                  "%s%s: %s\n", field.is_using ? "using " : "",
                                  field.name, field.type);
            if(length < 0 || (size_t)length >= sizeof(buffers->body) - used) {
                Diagnostic(record->span, "check.record", "normalized record is too large: %s",
                           record->name);
                return 0;
            }
            used += (size_t)length;
        }
        if(status < 0)
            return 0;
        copy_text(record->body, sizeof(record->body), buffers->body);
    }
    return 1;
}

/* Checked record layouts carry concrete array sizes into saved IR and ZIB.
 * A bundle intentionally omits source definitions, so a field must not keep
 * depending on a compile-time name after this point. */
int
normalize_record_arrays(ZirModule *module)
{
    static _Thread_local NormalizeRecordArraysBuffers *spares[16];
    static _Thread_local int spare_count;
    NormalizeRecordArraysBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = normalize_record_arrays_with_buffers(module, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
return_block_end(const ZirFunction *fn, int begin, int end)
{
    int depth = 1;
    for(int i = begin + 1; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return end;
}

static int
branch_returns(const ZirFunction *fn, int begin, int end, int *last)
{
    *last = return_block_end(fn, begin, end);
    int returns = sequence_returns(fn, begin + 1, *last);
    if(fn->stmts[begin].expr_root < 0)
        return returns;
    int next = *last + 1;
    if(next < end && fn->stmts[next].kind == ZIR_STMT_IF &&
       fn->stmts[next].is_else) {
        int alternative = branch_returns(fn, next, end, last);
        return returns && alternative;
    }
    return 0;
}

/* True when every explicit arm of the if chain at `begin` returns. The
 * fall-through state of such a chain is the state before it. */
int
all_arms_return(const ZirFunction *fn, int begin, int *last)
{
    int arms = 0;
    int index = begin;
    while(index < fn->stmt_count && fn->stmts[index].kind == ZIR_STMT_IF &&
          (arms == 0 || fn->stmts[index].is_else)) {
        int close = return_block_end(fn, index, fn->stmt_count);
        if(close <= index + 1 || !sequence_returns(fn, index + 2, close))
            return 0;
        *last = close;
        index = close + 1;
        arms++;
    }
    return arms > 0;
}

/* True when the `while true` loop at `begin`, closed at `close`, never
 * finishes normally: no break leaves it, and no named break or continue
 * reaches a loop outside it. Control then only leaves by returning. */
static int
while_diverges(const ZirFunction *fn, int begin, int close)
{
    const ZirStmt *header = &fn->stmts[begin];
    if(header->expr_root < 0 || header->expr_root >= fn->expr_count)
        return 0;
    const ZirExpr *condition = &fn->exprs[header->expr_root];
    if(condition->kind != ZIR_EXPR_IDENT || strcmp(condition->name, "true"))
        return 0;
    int *loops = calloc((size_t)(close - begin + 1), sizeof(*loops));
    int *opened = calloc((size_t)(close - begin + 1), sizeof(*opened));
    if(loops == NULL || opened == NULL) {
        free(loops);
        free(opened);
        return 0;
    }
    int depth = 0, nested = 0, diverges = 1;
    for(int i = begin + 1; i < close && diverges; i++) {
        const ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE && depth > 0) {
            depth--;
            if(opened[depth]) nested--;
            continue;
        }
        if(statement->kind == ZIR_STMT_BREAK ||
           statement->kind == ZIR_STMT_CONTINUE) {
            int inner = 0;
            for(int loop = 0; loop < nested; loop++)
                inner |= statement->target_id != 0 &&
                         loops[loop] == statement->target_id;
            if(statement->target_id == 0)
                inner = nested > 0;
            else if(header->loop_id != 0 && statement->target_id == header->loop_id)
                inner = statement->kind == ZIR_STMT_CONTINUE;
            if(!inner && (statement->kind == ZIR_STMT_BREAK ||
                          statement->target_id != 0))
                diverges = 0;
        }
        if(statement->kind == ZIR_STMT_IF || statement->kind == ZIR_STMT_WHILE ||
           statement->kind == ZIR_STMT_BLOCK_OPEN) {
            opened[depth] = statement->kind == ZIR_STMT_WHILE;
            if(opened[depth]) loops[nested++] = statement->loop_id;
            depth++;
        }
    }
    free(loops);
    free(opened);
    return diverges;
}

int
sequence_returns(const ZirFunction *fn, int begin, int end)
{
    for(int i = begin; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_RETURN || kind == ZIR_STMT_UNREACHABLE)
            return 1;
        if(kind == ZIR_STMT_BREAK || kind == ZIR_STMT_CONTINUE)
            return 0;
        if(kind == ZIR_STMT_IF) {
            int last;
            if(branch_returns(fn, i, end, &last))
                return 1;
            i = last;
        } else if(kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN) {
            int last = return_block_end(fn, i, end);
            if(kind == ZIR_STMT_BLOCK_OPEN && sequence_returns(fn, i + 1, last))
                return 1;
            if(kind == ZIR_STMT_WHILE && while_diverges(fn, i, last))
                return 1;
            i = last;
        }
    }
    return 0;
}

int
FunctionReturnsOnEveryPath(const ZirFunction *fn)
{
    return sequence_returns(fn, 0, fn->stmt_count);
}

int
starts_word(const char *source, const char *word)
{
    size_t length = strlen(word);
    return strncmp(source, word, length) == 0 &&
           (source[length] == '\0' || isspace((unsigned char)source[length]));
}

static int
if_case_opens_block(ZirStmtKind kind)
{
    return kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_FOR || kind == ZIR_STMT_IF_CASE ||
           kind == ZIR_STMT_BLOCK_OPEN;
}

static void
if_case_generated(ZirStmt *statement, ZirStmtKind kind,
                const char *text, ZirSourceSpan span)
{
    memset(statement, 0, sizeof(*statement));
    statement->kind = kind;
    statement->text = KeepText(text);
    statement->expr_root = statement->lhs_root = -1;
    statement->span = span;
}

void
if_case_error(Checker *c, ZirSourceSpan span, const char *message,
            const char *detail)
{
    c->errors++;
    c->failed = 1;
    Diagnostic(span, "check.if_case", "%s%s%s", message,
               detail && *detail ? ": " : "", detail ? detail : "");
}

static int
if_case_through(Checker *c, const ZirFunction *fn, const int *cases,
                   int case_count, int close, unsigned char *through)
{
    int has_through = 0;
    for(int arm = 0; arm < case_count; arm++) {
        int arm_end = arm + 1 < case_count ? cases[arm + 1] : close;
        int nested = 0;
        for(int i = cases[arm] + 1; i < arm_end; i++) {
            const ZirStmt *statement = &fn->stmts[i];
            if(statement->kind == ZIR_STMT_IF_CASE) {
                int inner_depth = 1;
                while(++i < arm_end && inner_depth > 0) {
                    if(if_case_opens_block(fn->stmts[i].kind)) inner_depth++;
                    if(fn->stmts[i].kind == ZIR_STMT_BLOCK_CLOSE)
                        inner_depth--;
                }
                if(inner_depth != 0) {
                    if_case_error(c, statement->span,
                                "unterminated nested if-case", "");
                    return -1;
                }
                continue;
            }
            if(strcmp(statement->text, "#through") == 0 ||
               strcmp(statement->text, "#through;") == 0) {
                if(nested != 0 || i != arm_end - 1 ||
                   arm == case_count - 1) {
                    if_case_error(c, statement->span,
                        "#through must end a case before another case", "");
                    return -1;
                }
                through[arm] = has_through = 1;
            }
            if(if_case_opens_block(statement->kind)) nested++;
            if(statement->kind == ZIR_STMT_BLOCK_CLOSE) nested--;
        }
    }
    return has_through;
}

static int
if_cases_return(const ZirFunction *fn, const int *cases,
                   int case_count, int close, const unsigned char *through)
{
    int next_returns = 0, all_return = 1;
    for(int arm = case_count - 1; arm >= 0; arm--) {
        int end = arm + 1 < case_count ? cases[arm + 1] : close;
        if(through[arm]) end--;
        int escapes = 0;
        for(int i = cases[arm] + 1; i < end; i++)
            escapes |= fn->stmts[i].kind == ZIR_STMT_BREAK ||
                       fn->stmts[i].kind == ZIR_STMT_CONTINUE ||
                       fn->stmts[i].kind == ZIR_STMT_IF_CASE;
        int returns = !escapes &&
            sequence_returns(fn, cases[arm] + 1, end);
        if(!returns && !escapes && through[arm])
            returns = next_returns;
        all_return &= returns;
        next_returns = returns;
    }
    return all_return;
}

static int
scalar_case_label(const ZirStmt *statement, char *label, size_t capacity)
{
    const char *cursor = skip_ws(statement->text);
    if(strncmp(cursor, "case", 4) != 0 ||
       (cursor[4] != ';' && !isspace((unsigned char)cursor[4])))
        return 0;
    cursor = skip_ws(cursor + 4);
    size_t length = strlen(cursor);
    while(length && isspace((unsigned char)cursor[length - 1])) length--;
    if(length == 0 || cursor[length - 1] != ';' || length >= capacity)
        return 0;
    memcpy(label, cursor, length - 1);
    label[length - 1] = '\0';
    trim_in_place(label);
    return 1;
}

int
scalar_case_type(const char *type)
{
    return !strcmp(type, "bool") || !strcmp(type, "string") ||
           numeric(type);
}

static int
enum_case_members(const ZirType *enumeration,
                  char (*names)[ZIR_NAME_MAX], int capacity)
{
    const char *cursor = enumeration->body;
    int count = 0;
    while(*cursor) {
        while(isspace((unsigned char)*cursor) ||
              *cursor == ',' || *cursor == ';') cursor++;
        if(!*cursor) break;
        const char *start = cursor;
        if(!isalpha((unsigned char)*cursor) && *cursor != '_') return -1;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        size_t length = (size_t)(cursor - start);
        if(count >= capacity || length >= ZIR_NAME_MAX) return -1;
        memcpy(names[count], start, length);
        names[count++][length] = '\0';
        while(*cursor && *cursor != ',' && *cursor != ';' &&
              *cursor != '\n') cursor++;
    }
    return count;
}

static int
enum_case_member(const ZirType *enumeration, const char *label,
                 char *member, size_t capacity)
{
    const char *start = label;
    if(*start == '.') start++;
    else {
        size_t length = strlen(enumeration->name);
        if(strncmp(start, enumeration->name, length) != 0 ||
           start[length] != '.') return 0;
        start += length + 1;
    }
    if(!isalpha((unsigned char)*start) && *start != '_') return 0;
    const char *end = start + 1;
    while(isalnum((unsigned char)*end) || *end == '_') end++;
    size_t length = (size_t)(end - start);
    if(*end || length >= capacity) return 0;
    memcpy(member, start, length);
    member[length] = '\0';
    return EnumMemberValue(enumeration, member, NULL);
}
/* Buffers lower_if_case keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerIfCaseBuffers {
    char label[ZIR_TEXT_MAX];
    char line[ZIR_TEXT_MAX];
    char source[ZIR_TEXT_MAX];
    char comparison[ZIR_TEXT_MAX];
} LowerIfCaseBuffers;

int lower_if_case(Checker *c, int index, const char *checked_type);

static int
lower_if_case_with_buffers(Checker *c, int index, const char *checked_type, LowerIfCaseBuffers *buffers)
{
    ZirFunction *fn = c->fn;
    ZirStmt *head = &fn->stmts[index];
    const ZirType *enumeration = FindType(c->module, checked_type, NULL);
    int complete = enumeration != NULL &&
        starts_word(skip_ws(head->text + 2), "#complete");
    int *cases = calloc((size_t)fn->stmt_count + 1, sizeof(*cases));
    unsigned char *through = calloc((size_t)fn->stmt_count + 1,
                                    sizeof(*through));
    int member_capacity = enumeration != NULL ?
        (int)strlen(enumeration->body) + 1 : 0;
    char (*members)[ZIR_NAME_MAX] = enumeration != NULL ?
        calloc((size_t)member_capacity, sizeof(*members)) : NULL;
    unsigned char *seen = enumeration != NULL ?
        calloc((size_t)member_capacity, sizeof(*seen)) : NULL;
    int member_count = enumeration != NULL ?
        (members != NULL && seen != NULL ?
         enum_case_members(enumeration, members, member_capacity) : -1) : 0;
    ZirStmt *output = NULL;
    int count = 0, depth = 1, close = -1, default_arm = -1, ok = 0;
    char temporary[ZIR_NAME_MAX];
    char active[ZIR_NAME_MAX], matched_name[ZIR_NAME_MAX];
    int length;
    if(cases == NULL || through == NULL ||
       (enumeration != NULL && member_count <= 0)) {
        if_case_error(c, head->span, "out of memory lowering if-case", "");
        goto done;
    }
    for(int i = index + 1; i < fn->stmt_count; i++) {
        ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(--depth == 0) { close = i; break; }
        } else if(depth == 1 && statement->kind == ZIR_STMT_CASE) {
            if(!scalar_case_label(statement, buffers->label, sizeof(buffers->label))) {
                if_case_error(c, statement->span,
                            "scalar case requires 'case expression;' or 'case;'",
                            statement->text);
                goto done;
            }
            if(enumeration != NULL) {
                char member[ZIR_NAME_MAX];
                if(!enum_case_member(enumeration, buffers->label, member,
                                     sizeof(member))) {
                    if_case_error(c, statement->span,
                        "enum case requires 'case .Member;' or 'case Enum.Member;'",
                        statement->text);
                    goto done;
                }
                int selected = -1;
                for(int m = 0; m < member_count; m++)
                    if(!strcmp(member, members[m])) { selected = m; break; }
                if(selected < 0 || seen[selected]) {
                    if_case_error(c, statement->span,
                        selected < 0 ? "case is not a member of the selected enum" :
                                       "duplicate if-case member", member);
                    goto done;
                }
                seen[selected] = 1;
            }
            if(default_arm >= 0) {
                if_case_error(c, statement->span, "default case must be last", "");
                goto done;
            }
            if(buffers->label[0] == '\0') default_arm = count;
            cases[count++] = i;
        } else if(depth == 1 && count == 0) {
            if_case_error(c, statement->span,
                        "if-case body must begin with a case", "");
            goto done;
        }
        if(if_case_opens_block(statement->kind)) depth++;
    }
    if(close < 0 || count == 0) {
        if_case_error(c, head->span, "unterminated or empty if-case", "");
        goto done;
    }
    for(int m = 0; complete && m < member_count; m++)
        if(!seen[m]) {
            if_case_error(c, head->span,
                          "non-exhaustive if-case; missing case", members[m]);
            goto done;
        }
    int has_through = if_case_through(c, fn, cases, count,
                                         close, through);
    if(has_through < 0) goto done;
    int all_return = has_through && (default_arm >= 0 || complete) &&
        if_cases_return(fn, cases, count, close, through);
    copy_text(buffers->source, sizeof(buffers->source), head->text);
    char *equals = strstr(buffers->source, "==");
    if(equals == NULL) {
        if_case_error(c, head->span, "if-case requires a block", "");
        goto done;
    }
    *equals = '\0';
    const char *value = skip_ws(buffers->source + 2);
    if(complete) value = skip_ws(value + strlen("#complete"));
    trim_in_place((char *)value);
    if(!*value) {
        if_case_error(c, head->span, "if-case requires a value", "");
        goto done;
    }
    int serial = index, collision;
    do {
        snprintf(temporary, sizeof(temporary), "case_value_%d", serial++);
        collision = strstr(fn->args, temporary) != NULL;
        for(int i = 0; i < fn->stmt_count; i++)
            collision |= strstr(fn->stmts[i].text, temporary) != NULL ||
                         strcmp(fn->stmts[i].name, temporary) == 0;
    } while(collision);
    length = snprintf(active, sizeof(active), "%s_active", temporary);
    if(length < 0 || (size_t)length >= sizeof(active)) goto too_long;
    length = snprintf(matched_name, sizeof(matched_name),
                      "%s_matched", temporary);
    if(length < 0 || (size_t)length >= sizeof(matched_name)) goto too_long;
    int capacity = fn->stmt_count + count * 4 + 8;
    output = calloc((size_t)capacity, sizeof(*output));
    if(output == NULL) {
        if_case_error(c, head->span, "out of memory lowering if-case", "");
        goto done;
    }
    int next = 0;
    for(int i = 0; i < index; i++) output[next++] = fn->stmts[i];
    const char *storage = !strcmp(checked_type, "integer") ? "s64" :
                          !strcmp(checked_type, "real") ? "float64" :
                          checked_type;
    length = snprintf(buffers->line, sizeof(buffers->line), "%s: %s = %s",
                      temporary, storage, value);
    if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
    if_case_generated(&output[next++], ZIR_STMT_DECL, buffers->line, head->span);
    if(has_through) {
        length = snprintf(buffers->line, sizeof(buffers->line), "%s: bool = false", active);
        if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_DECL, buffers->line, head->span);
        if(default_arm >= 0 || complete) {
            length = snprintf(buffers->line, sizeof(buffers->line), "%s: bool = false",
                              matched_name);
            if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_DECL, buffers->line, head->span);
        }
    }
    for(int arm = 0; arm < count; arm++) {
        ZirSourceSpan span = fn->stmts[cases[arm]].span;
        if(!scalar_case_label(&fn->stmts[cases[arm]], buffers->label,
                              sizeof(buffers->label))) goto done;
        if(enumeration != NULL) {
            char member[ZIR_NAME_MAX];
            if(!enum_case_member(enumeration, buffers->label, member,
                                 sizeof(member))) goto done;
            /* Enum references lower to integer literals during a later
             * checker restart. Keep the generated comparison enum typed. */
            length = snprintf(buffers->label, sizeof(buffers->label), "cast(%s) %s.%s",
                              enumeration->name, enumeration->name, member);
            if(length < 0 || (size_t)length >= sizeof(buffers->label)) goto too_long;
        }
        length = enumeration == NULL ?
            snprintf(buffers->comparison, sizeof(buffers->comparison), "(%s)", buffers->label) :
            snprintf(buffers->comparison, sizeof(buffers->comparison), "%s", buffers->label);
        if(length < 0 || (size_t)length >= sizeof(buffers->comparison)) goto too_long;
        if(buffers->label[0] == '\0')
            length = has_through ?
                snprintf(buffers->line, sizeof(buffers->line), "if %s || !%s {",
                         active, matched_name) :
                snprintf(buffers->line, sizeof(buffers->line), "else {");
        else
            length = has_through ?
                snprintf(buffers->line, sizeof(buffers->line), "if %s || %s == %s {",
                         active, temporary, buffers->comparison) :
                snprintf(buffers->line, sizeof(buffers->line), "%s %s == %s {",
                         arm ? "else if" : "if", temporary, buffers->comparison);
        if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_IF, buffers->line, span);
        if(has_through && (default_arm >= 0 || complete)) {
            length = snprintf(buffers->line, sizeof(buffers->line), "%s = true", matched_name);
            if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_ASSIGN, buffers->line, span);
        }
        int arm_end = arm + 1 < count ? cases[arm + 1] : close;
        for(int i = cases[arm] + 1; i < arm_end; i++)
            if(!(through[arm] && i == arm_end - 1))
                output[next++] = fn->stmts[i];
        if(has_through) {
            length = snprintf(buffers->line, sizeof(buffers->line), "%s = %s", active,
                              through[arm] ? "true" : "false");
            if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
            if_case_generated(&output[next++], ZIR_STMT_ASSIGN, buffers->line, span);
        }
        if_case_generated(&output[next++], ZIR_STMT_BLOCK_CLOSE, "}", span);
    }
    if(complete) {
        length = has_through ?
            snprintf(buffers->line, sizeof(buffers->line), "if !%s {", matched_name) :
            snprintf(buffers->line, sizeof(buffers->line), "else {");
        if(length < 0 || (size_t)length >= sizeof(buffers->line)) goto too_long;
        if_case_generated(&output[next++], ZIR_STMT_IF, buffers->line, head->span);
        if_case_generated(&output[next++], ZIR_STMT_UNREACHABLE,
                          "unreachable", head->span);
        if_case_generated(&output[next++], ZIR_STMT_BLOCK_CLOSE, "}", head->span);
    }
    if(all_return)
        if_case_generated(&output[next++], ZIR_STMT_UNREACHABLE,
                        "unreachable", head->span);
    for(int i = close + 1; i < fn->stmt_count; i++)
        output[next++] = fn->stmts[i];
    if(next > capacity) {
        if_case_error(c, head->span, "internal if-case lowering overflow", "");
        goto done;
    }
    free(fn->stmts);
    fn->stmts = output;
    fn->stmt_count = fn->stmt_cap = next;
    output = NULL;
    ok = 1;
    goto done;
too_long:
    if_case_error(c, head->span, "if-case expression exceeds statement limit", "");
done:
    free(output); free(cases); free(through); free(members); free(seen);
    return ok;
}

/* A Jai if-case evaluates its selector once. With #through, an active flag
 * carries execution into the next arm without retesting its label. */
int
lower_if_case(Checker *c, int index, const char *checked_type)
{
    static _Thread_local LowerIfCaseBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerIfCaseBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_if_case_with_buffers(c, index, checked_type, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

int
validate_loop_targets(Checker *c, const ZirFunction *fn)
{
    int depth = 0, ok = 1;
    int *scopes = calloc((size_t)fn->stmt_count + 1, sizeof(*scopes));
    unsigned char *seen = calloc((size_t)fn->stmt_count + 1,
                                 sizeof(*seen));
    if(scopes == NULL || seen == NULL) {
        free(scopes); free(seen);
        error(c, fn->span, "out of memory checking loop targets", "");
        c->failed = 1;
        return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) {
            if(depth == 0) {
                error(c, st->span, "unexpected block close", "");
                ok = 0;
                break;
            }
            depth--;
        }
        if(st->loop_id < 0 || st->target_id < 0 ||
           (st->loop_id && st->kind != ZIR_STMT_WHILE) ||
           st->for_form < 0 || st->for_form > 2 ||
           (st->for_form && !st->loop_id) ||
           st->for_step < 0 || (st->for_step && st->kind != ZIR_STMT_ASSIGN) ||
           (st->target_id && st->kind != ZIR_STMT_BREAK &&
            st->kind != ZIR_STMT_CONTINUE)) {
            error(c, st->span, "invalid loop control metadata", "");
            ok = 0;
            break;
        }
        if(st->kind == ZIR_STMT_BREAK || st->kind == ZIR_STMT_CONTINUE) {
            int found = 0;
            for(int scope = depth - 1; scope >= 0; scope--)
                if(st->target_id ? scopes[scope] == st->target_id :
                   scopes[scope] != 0) {
                    found = 1;
                    break;
                }
            if(!found) {
                error(c, st->span, "loop control requires an enclosing target", st->text);
                ok = 0;
                break;
            }
        }
        if(st->kind == ZIR_STMT_WHILE) {
            if(st->loop_id > fn->stmt_count ||
               (st->loop_id && seen[st->loop_id])) {
                error(c, st->span, "duplicate or invalid loop target", st->text);
                ok = 0;
                break;
            }
            if(st->loop_id) seen[st->loop_id] = 1;
            scopes[depth++] = st->loop_id ? st->loop_id : -1;
        } else if(st->kind == ZIR_STMT_IF ||
                  st->kind == ZIR_STMT_BLOCK_OPEN) {
            scopes[depth++] = 0;
        }
    }
    if(ok && depth != 0) {
        error(c, fn->span, "unclosed block in function", fn->name);
        ok = 0;
    }
    free(scopes); free(seen);
    if(!ok) c->failed = 1;
    return ok;
}

static int
call_must_use(Checker *c, const ZirExpr *call)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    const char *name = strrchr(call->name, '.');

    if(call->slot_type[0])
        return 0;
    if(ResolveFunctionAt(c->module, call->name, SpanPath(call->span),
                         &owner, &callee) == 1 && callee != NULL)
        return callee->must_use;
    name = name != NULL ? name + 1 : call->name;
    for(int i = 0; i < c->specialization_count; i++) {
        const SpecializationRequest *request = &c->specializations[i];
        if(strcmp(request->name, name) == 0)
            return request->template_owner->functions[
                request->template_index].must_use;
    }
    for(int i = 0; i < c->module->import_count; i++) {
        const ZirImport *import = &c->module->imports[i];
        if(import->kind == ZIR_IMPORT_EXTERN &&
           in_lookup_file(c->module, import->is_file_private,
                          import->span) &&
           strcmp(import->name, call->name) == 0)
            return import->must_use;
    }
    return 0;
}

int
discarded_must_call(Checker *c, int index)
{
    if(index < 0)
        return -1;
    const ZirExpr *expression = &c->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_CALL)
        return call_must_use(c, expression) ? index : -1;
    int found = discarded_must_call(c, expression->left);
    if(found >= 0)
        return found;
    found = discarded_must_call(c, expression->right);
    if(found >= 0)
        return found;
    found = discarded_must_call(c, expression->third);
    if(found >= 0)
        return found;
    for(int child = expression->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        found = discarded_must_call(c, child);
        if(found >= 0)
            return found;
    }
    return -1;
}
/* Buffers conversion_matches keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ConversionMatchesBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} ConversionMatchesBuffers;

static int conversion_matches(Checker *c, const ZirFunction *conversion,
                   const char *from, const char *to);

static int
conversion_matches_with_buffers(Checker *c, const ZirFunction *conversion,
                   const char *from, const char *to, ConversionMatchesBuffers *buffers)
{
    int count;
    char *colon;
    if(conversion->is_template || conversion->is_extern ||
       strcmp(conversion->return_type, to) != 0)
        return 0;
    count = *skip_ws(conversion->args) ?
        split_top_level(conversion->args, buffers->parameters[0], 64,
                        sizeof(buffers->parameters[0])) : 0;
    if(count != 1)
        return 0;
    colon = strchr(buffers->parameters[0], ':');
    if(colon == NULL)
        return 0;
    return compatible(skip_ws(colon + 1), from);
}

/* A conversion applies when its single parameter accepts `from` and its
 * result is exactly `to`. */
static int
conversion_matches(Checker *c, const ZirFunction *conversion,
                   const char *from, const char *to)
{
    static _Thread_local ConversionMatchesBuffers *spares[16];
    static _Thread_local int spare_count;
    ConversionMatchesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = conversion_matches_with_buffers(c, conversion, from, to, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* Rewrite an incompatible expression into a call on a visible `#as`
 * conversion from its own type to `to`. The original expression moves into
 * the call's first argument; parent references keep their index. Returns
 * the conversion's result type, or NULL when no unique conversion applies. */
const char *
try_conversion(Checker *c, int index, const char *to, ZirSourceSpan span)
{
    const char *from = c->fn->exprs[index].type;
    const ZirFunction *conversion = NULL;
    const ZirModule *owner = NULL;
    if(!*from || !strcmp(from, to) || c->inference_only)
        return NULL;
    for(int pass = 0; pass < 2; pass++) {
        const ZirModule *scope = pass == 0 ? c->module : NULL;
        if(pass != 0) {
            for(int i = 0; i < c->module->import_count; i++) {
                const ZirImport *import = &c->module->imports[i];
                if((import->kind != ZIR_IMPORT_OPEN &&
                    !(import->kind == ZIR_IMPORT_MODULE &&
                      import->is_using)) ||
                   import->resolved_module == NULL)
                    continue;
                for(int f = 0; f < import->resolved_module->function_count;
                    f++) {
                    const ZirFunction *candidate =
                        &import->resolved_module->functions[f];
                    if(candidate->is_conversion && candidate->is_public &&
                       conversion_matches(c, candidate, from, to)) {
                        if(conversion != NULL && conversion != candidate) {
                            error(c, span, "ambiguous #as conversion", to);
                            return NULL;
                        }
                        conversion = candidate;
                        owner = import->resolved_module;
                    }
                }
            }
            continue;
        }
        for(int f = 0; f < scope->function_count; f++) {
            const ZirFunction *candidate = &scope->functions[f];
            if(!candidate->is_conversion ||
               !conversion_matches(c, candidate, from, to))
                continue;
            if(conversion != NULL && conversion != candidate) {
                error(c, span, "ambiguous #as conversion", to);
                return NULL;
            }
            conversion = candidate;
            owner = scope;
        }
    }
    if(conversion == NULL)
        return NULL;
    c->conversions_applied = 1;
    (void)owner;
    {
        int chain_next = c->fn->exprs[index].next_sibling;
        int saved_argument = c->fn->exprs[index].argument_index;
        ZirExpr *copy_slot = FunctionAddExpr(c->fn, c->fn->exprs[index].kind,
                                             c->fn->exprs[index].name,
                                             c->fn->exprs[index].span);
        ZirExpr *call;
        ZirExpr saved;
        int copy_index;
        if(copy_slot == NULL) {
            c->failed = 1;
            return NULL;
        }
        saved = c->fn->exprs[index];
        *copy_slot = saved;
        copy_slot->next_sibling = -1;
        copy_slot->argument_index = 0;
        copy_index = (int)(copy_slot - c->fn->exprs);
        call = &c->fn->exprs[index];
        memset(call, 0, sizeof(*call));
        call->kind = ZIR_EXPR_CALL;
        copy_text(call->name, sizeof(call->name), conversion->name);
        call->argument_index = saved_argument;
        call->first_child = copy_index;
        call->left = -1;
        call->right = -1;
        call->third = -1;
        call->next_sibling = chain_next;
        call->span = saved.span;
        copy_text(call->type, sizeof(call->type), conversion->return_type);
        return call->type;
    }
}

/* Rewritten #as calls append their argument after the call node, but the IR
 * layout requires every child to precede its parent and sibling chains to
 * ascend. Re-emit each statement tree in postorder with remapped links. */
static int
rebuild_postorder(ZirFunction *fn, int index, ZirExpr *out, int *count,
                  int *remap)
{
    int self;
    ZirExpr *node;
    if(index < 0 || index >= fn->expr_count)
        return 1;
    if(remap[index] >= 0)
        return 1;
    node = &fn->exprs[index];
    {
        int left = node->left, right = node->right, third = node->third;
        int child = node->first_child;
        if(!rebuild_postorder(fn, left, out, count, remap) ||
           !rebuild_postorder(fn, right, out, count, remap) ||
           !rebuild_postorder(fn, third, out, count, remap))
            return 0;
        while(child >= 0) {
            int next = fn->exprs[child].next_sibling;
            if(!rebuild_postorder(fn, child, out, count, remap))
                return 0;
            child = next;
        }
    }
    self = *count;
    out[self] = fn->exprs[index];
    remap[index] = self;
    node = &out[self];
    node->left = node->left >= 0 ? remap[node->left] : -1;
    node->right = node->right >= 0 ? remap[node->right] : -1;
    node->third = node->third >= 0 ? remap[node->third] : -1;
    node->first_child = node->first_child >= 0 ? remap[node->first_child] : -1;
    node->next_sibling = -1;
    (*count)++;
    return 1;
}

int
rebuild_conversion_layout(ZirFunction *fn)
{
    ZirExpr *out = calloc((size_t)fn->expr_count, sizeof(*out));
    int *remap = calloc((size_t)fn->expr_count, sizeof(*remap));
    int count = 0;
    if(out == NULL || remap == NULL) {
        free(out);
        free(remap);
        return 0;
    }
    for(int i = 0; i < fn->expr_count; i++)
        remap[i] = -1;
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *st = &fn->stmts[s];
        if(!rebuild_postorder(fn, st->expr_root, out, &count, remap) ||
           !rebuild_postorder(fn, st->lhs_root, out, &count, remap)) {
            free(out);
            free(remap);
            return 0;
        }
    }
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *st = &fn->stmts[s];
        if(st->expr_root >= 0)
            st->expr_root = remap[st->expr_root];
        if(st->lhs_root >= 0)
            st->lhs_root = remap[st->lhs_root];
    }
    memcpy(fn->exprs, out, (size_t)count * sizeof(*out));
    fn->expr_count = count;
    free(out);
    free(remap);
    return 1;
}

static int
vec_primitive_name(const char *name)
{
    return !strcmp(name, "VecPush") || !strcmp(name, "VecClear") ||
           !strcmp(name, "VecFree") || !strcmp(name, "VecSwap") ||
           !strcmp(name, "VecPop") || !strcmp(name, "VecGet") ||
           !strcmp(name, "VecClone") || !strcmp(name, "VecSlice") ||
           !strcmp(name, "BuilderAppend") || !strcmp(name, "BuilderFinish");
}

/* Mark bindings whose ownership transfers in this statement: VecFree and
 * BuilderFinish consume their place, and an owned Vec passed as an ordinary
 * call argument moves into the callee. Runs after the statement checked
 * cleanly so erroneous calls cannot poison later statements. */
void
mark_expr_moves(Checker *c, int index)
{
    const ZirExpr *e;
    int primitive;
    if(index < 0 || index >= c->fn->expr_count)
        return;
    e = &c->fn->exprs[index];
    if(e->kind == ZIR_EXPR_FIELD_INIT &&
       contains_vec(c->module, e->type, 0)) {
        int right = e->right;
        Binding *binding = lexical_owned_binding(c, right);
        if(binding != NULL) {
            if(binding->moved || binding->moved_path_count != 0) {
                error(c, c->fn->exprs[right].span,
                      "owned binding is used after moving", binding->name);
                return;
            }
            if(binding->borrow_count > 0) {
                error(c, e->span,
                      "cannot move a Vec with a live borrowed view",
                      binding->name);
                return;
            }
            binding->moved = 1;
            c->fn->exprs[right].is_move = 1;
            return;
        }
        if(mark_moved_member_path(c, right))
            return;
        if(c->fn->exprs[right].kind == ZIR_EXPR_COMPOUND ||
           c->fn->exprs[right].kind == ZIR_EXPR_CALL) {
            mark_expr_moves(c, right);
            return;
        }
        error(c, c->fn->exprs[right].span,
              "owned record field moves a binding or takes a call result",
              e->name);
        return;
    }
    if(e->kind == ZIR_EXPR_IDENT) {
        Binding *binding = lexical_vec_binding(c, index);
        if(binding != NULL && binding->moved)
            error(c, e->span, "Vec binding is used after moving", e->name);
        return;
    }
    primitive = e->kind == ZIR_EXPR_CALL && vec_primitive_name(e->name);
    Binding *storage = primitive && e->first_child >= 0 ?
        lexical_vec_binding(c, e->first_child) : NULL;
    int storage_was_moved = storage != NULL && storage->moved;
    for(int child = e->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        Binding *binding = e->kind == ZIR_EXPR_CALL ?
            lexical_owned_binding(c, child) : NULL;
        int consumes = !primitive || !strcmp(e->name, "VecFree") ||
                       !strcmp(e->name, "BuilderFinish");
        if(binding != NULL && consumes) {
            if(binding->moved || binding->moved_path_count != 0) {
                error(c, c->fn->exprs[child].span,
                      "owned binding is used after moving", binding->name);
                continue;
            }
            if(binding->borrow_count > 0)
                error(c, e->span,
                      "cannot move a Vec with a live borrowed view",
                      binding->name);
            binding->moved = 1;
            if(!primitive)
                c->fn->exprs[child].is_move = 1;
        } else if(consumes && mark_moved_member_path(c, child))
            continue;
        else
            mark_expr_moves(c, child);
    }
    if(primitive && storage != NULL && !storage_was_moved &&
       storage->moved && strcmp(e->name, "VecFree") &&
       strcmp(e->name, "BuilderFinish"))
        error(c, e->span, "Vec operation moves its storage in an argument",
              storage->name);
    mark_expr_moves(c, e->left);
    mark_expr_moves(c, e->right);
    mark_expr_moves(c, e->third);
}

/* A fresh vector returned by a call has one owner. It can become a binding,
 * return value, or ordinary call argument, or be dropped as a whole statement.
 * Reading a member of an unbound temporary would otherwise lose its owner. */
void
check_vec_call_results(Checker *c, int index, int transferred, int discarded)
{
    if(index < 0 || index >= c->fn->expr_count)
        return;
    const ZirExpr *expr = &c->fn->exprs[index];
    if(expr->kind == ZIR_EXPR_CALL &&
       contains_vec(c->module, expr->type, 0) &&
       !transferred && !discarded)
        error(c, expr->span,
              "temporary owned result must be bound or passed to an owning call",
              expr->name);
    for(int child = expr->first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        int child_transfer =
            (expr->kind == ZIR_EXPR_CALL &&
             !vec_primitive_name(expr->name) &&
             contains_vec(c->module, c->fn->exprs[child].type, 0)) ||
            (expr->kind == ZIR_EXPR_COMPOUND &&
             c->fn->exprs[child].kind == ZIR_EXPR_FIELD_INIT &&
             contains_vec(c->module, c->fn->exprs[child].type, 0));
        check_vec_call_results(c, child, child_transfer, 0);
    }
    check_vec_call_results(c, expr->left, 0, 0);
    check_vec_call_results(c, expr->right, 0, 0);
    check_vec_call_results(c, expr->third, 0, 0);
}
