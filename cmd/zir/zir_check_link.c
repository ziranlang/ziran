#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_check_internal.h"
#include "compiler_link.h"

int
check_foreign_slice_returns(ZirProgram **programs, int count)
{
    /* A foreign slice can contain an imported generic application. Check it
     * after linking and specialization have given the element a concrete
     * type; the source spelling Map(string, Any) is not a record name. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                ZirImport *import = &module->imports[i];
                if(import->kind == ZIR_IMPORT_EXTERN &&
                   SliceElementType(import->return_type, NULL, 0)) {
                    char element[ZIR_NAME_MAX];
                    if(!SliceElementType(import->return_type, element,
                                         sizeof(element)) ||
                       !*element || strchr(element, '[') ||
                       !strcmp(element, "char") ||
                       !strcmp(element, "const char") ||
                       (!*ScalarType(element) &&
                        FindType(module, element, NULL) == NULL)) {
                        Diagnostic(import->span, "check.slice_signature",
                                      "host slice returns need a supported element type");
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

/* Resolve Jai type-constructor calls before the ordinary checker sees type
 * names. Concrete applications get a stable private name so the existing IR
 * and target backends can refer to the same instantiated record. */
int
canonical_type_arguments(const char *source, char *output, size_t capacity)
{
    size_t used = 0;
    int space = 0;
    char quote = '\0';
    for(const unsigned char *p = (const unsigned char *)source; *p; p++) {
        if(quote == '\0' && isspace(*p)) {
            space = 1;
            continue;
        }
        if(space && used &&
           (isalnum((unsigned char)output[used - 1]) || output[used - 1] == '_') &&
           (isalnum(*p) || *p == '_')) {
            if(used + 1 >= capacity) return 0;
            output[used++] = ' ';
        }
        space = 0;
        if(used + 1 >= capacity) return 0;
        output[used++] = (char)*p;
        if(quote && *p == '\\' && p[1]) {
            if(used + 1 >= capacity) return 0;
            output[used++] = (char)*++p;
        } else if(quote && *p == (unsigned char)quote) {
            quote = '\0';
        } else if(!quote && (*p == '"' || *p == '\'')) {
            quote = (char)*p;
        }
    }
    if(used >= capacity) return 0;
    output[used] = '\0';
    return 1;
}
/* Buffers rewrite_type_applications keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
/* A type application such as Vec(Pixel) takes the narrowest scope of its
 * template and its arguments. An argument private to the module keeps the
 * application out of the public C header, since other modules cannot spell
 * it, but every file of the module, such as a #load-ed one, reaches the same
 * instance; only an argument private to its file keeps the instance to that
 * file. */
void
TypeApplicationScope(const ZirModule *module, const char *arguments,
                     int template_public, int template_file_private,
                     ZirType *instance)
{
    int is_public = template_public;
    int is_file_private = template_file_private;
    const char *cursor = arguments;
    while(cursor != NULL && *cursor) {
        if(isalpha((unsigned char)*cursor) || *cursor == '_') {
            char name[ZIR_NAME_MAX];
            size_t length = 0;
            while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
                  length + 1 < sizeof(name))
                name[length++] = *cursor++;
            name[length] = '\0';
            while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
            const ZirModule *owner = NULL;
            const ZirType *type = FindType(module, name, &owner);
            if(type != NULL && owner == module) {
                if(!type->is_public) is_public = 0;
                if(type->is_file_private) is_file_private = 1;
            }
            continue;
        }
        cursor++;
    }
    instance->is_public = is_public;
    instance->is_file_private = is_file_private;
}

typedef struct RewriteTypeApplicationsBuffers {
    char arguments[ZIR_TEXT_MAX];
    char expanded[ZIR_TEXT_MAX];
    char canonical[ZIR_TEXT_MAX];
} RewriteTypeApplicationsBuffers;

int rewrite_type_applications(ZirModule *module, const char *source,
                          char *output, size_t capacity,
                          ZirSourceSpan span, int recursion);

static int
rewrite_type_applications_with_buffers(ZirModule *module, const char *source,
                          char *output, size_t capacity,
                          ZirSourceSpan span, int recursion, RewriteTypeApplicationsBuffers *buffers)
{
    size_t used = 0;
    select_lookup_file(module, span);
    if(recursion > 16) {
        Diagnostic(span, "check.type_application", "type application is nested too deeply");
        return 0;
    }
    for(const char *cursor = source; *cursor; ) {
        if(*cursor == '"' || *cursor == '\'') {
            char quote = *cursor;
            if(used + 1 >= capacity) return 0;
            output[used++] = *cursor++;
            while(*cursor) {
                char next = *cursor++;
                if(used + 1 >= capacity) return 0;
                output[used++] = next;
                if(next == '\\' && *cursor) {
                    if(used + 1 >= capacity) return 0;
                    output[used++] = *cursor++;
                } else if(next == quote) {
                    break;
                }
            }
            continue;
        }
        if(isalpha((unsigned char)*cursor) || *cursor == '_') {
            const char *start = cursor;
            /* module.Record(...) names a record reached through a module. */
            while(isalnum((unsigned char)*cursor) || *cursor == '_' ||
                  (*cursor == '.' && (isalpha((unsigned char)cursor[1]) || cursor[1] == '_')))
                cursor++;
            size_t length = (size_t)(cursor - start);
            const char *opening = skip_ws(cursor);
            char base[ZIR_NAME_MAX];
            const ZirType *generic = NULL;
            if(length < sizeof(base) && *opening == '(') {
                memcpy(base, start, length);
                base[length] = '\0';
                generic = FindType(module, base, NULL);
            }
            if(generic != NULL && generic->is_record_template) {
                const char *closing = opening + 1;
                int depth = 1;
                while(*closing && depth) {
                    if(*closing == '(') depth++;
                    else if(*closing == ')') depth--;
                    if(depth) closing++;
                }
                if(depth || closing == opening + 1 ||
                   (size_t)(closing - opening - 1) >= ZIR_TEXT_MAX) {
                    Diagnostic(span, "check.type_application", "invalid type application: %s", base);
                    return 0;
                }
                memcpy(buffers->arguments, opening + 1,
                       (size_t)(closing - opening - 1));
                buffers->arguments[closing - opening - 1] = '\0';
                if(!rewrite_type_applications(module, buffers->arguments, buffers->expanded,
                        sizeof(buffers->expanded), span, recursion + 1))
                    return 0;
                if(!canonical_type_arguments(buffers->expanded, buffers->canonical,
                        sizeof(buffers->canonical))) {
                    Diagnostic(span, "check.type_application",
                               "type application arguments are too long: %s", base);
                    return 0;
                }
                /* Name the instance by the record, not by how this module
                 * spells it (Vec or vec.Vec), so every copy gets one name. */
                uint64_t hash = UINT64_C(14695981039346656037);
                for(const unsigned char *p = (const unsigned char *)generic->name; *p; p++)
                    hash = (hash ^ *p) * UINT64_C(1099511628211);
                hash = (hash ^ '(') * UINT64_C(1099511628211);
                for(const unsigned char *p = (const unsigned char *)buffers->canonical; *p; p++)
                    hash = (hash ^ *p) * UINT64_C(1099511628211);
                char name[ZIR_NAME_MAX];
                snprintf(name, sizeof(name), "__type_%016llx",
                         (unsigned long long)hash);
                ZirType *instance = NULL;
                for(int t = 0; t < module->type_count; t++)
                    if(strcmp(module->types[t].name, name) == 0) {
                        instance = &module->types[t];
                        break;
                    }
                if(instance != NULL &&
                   (!instance->is_synthetic_application ||
                    (instance->is_type_instance &&
                     (FindType(module, instance->template_name, NULL) != generic ||
                      strcmp(instance->template_args, buffers->canonical) != 0)))) {
                    Diagnostic(span, "check.type_application",
                               "type application name collision: %s", base);
                    return 0;
                }
                if(instance == NULL) {
                    int generic_public = generic->is_public;
                    int generic_file_private = generic->is_file_private;
                    instance = ModuleAddType(module, name, span);
                    if(instance == NULL) return 0;
                    TypeApplicationScope(module, buffers->canonical, generic_public,
                                         generic_file_private, instance);
                    instance->is_type_instance = 1;
                    instance->is_synthetic_application = 1;
                    copy_text(instance->template_name,
                              sizeof(instance->template_name), base);
                    copy_text(instance->template_args,
                              sizeof(instance->template_args), buffers->canonical);
                }
                size_t name_length = strlen(name);
                if(used + name_length >= capacity) return 0;
                memcpy(output + used, name, name_length);
                used += name_length;
                cursor = closing + 1;
                continue;
            }
            if(used + length >= capacity) return 0;
            memcpy(output + used, start, length);
            used += length;
            continue;
        }
        if(used + 1 >= capacity) return 0;
        output[used++] = *cursor++;
    }
    output[used] = '\0';
    return 1;
}

COMPILER_SCRATCH_RETURN(, int, rewrite_type_applications,
    (ZirModule *module, const char *source,
                          char *output, size_t capacity,
                          ZirSourceSpan span, int recursion),
    RewriteTypeApplicationsBuffers, RECOVERABLE, free,
    rewrite_type_applications_with_buffers(module, source, output, capacity, span, recursion, buffers))
/* Buffers rewrite_function_type_applications keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RewriteFunctionTypeApplicationsBuffers {
    char expanded[ZIR_TEXT_MAX * 2];
} RewriteFunctionTypeApplicationsBuffers;

static int rewrite_function_type_applications(ZirModule *module, ZirFunction *fn);

static int
rewrite_function_type_applications_with_buffers(ZirModule *module, ZirFunction *fn, RewriteFunctionTypeApplicationsBuffers *buffers)
{
    if(!rewrite_type_applications(module, FunctionArgs(fn), buffers->expanded,
            sizeof(buffers->expanded), fn->span, 0)) return 0;
    if(strlen(buffers->expanded) >= ZIR_TEXT_MAX) return 0;
    fn->args_text = KeepParameters(buffers->expanded);
    /* Defaults retain a second parameter signature. Keep its types and any
     * record constructors concrete too, so checked IR can validate it. */
    if(FunctionDefaultArgs(fn)[0]) {
        if(!rewrite_type_applications(module, FunctionDefaultArgs(fn), buffers->expanded,
                sizeof(buffers->expanded), fn->span, 0)) return 0;
        if(strlen(buffers->expanded) >= ZIR_TEXT_MAX) return 0;
        fn->default_args_text = KeepParameters(buffers->expanded);
    }
    if(!rewrite_type_applications(module, fn->return_type, buffers->expanded,
            sizeof(buffers->expanded), fn->span, 0)) return 0;
    if(strlen(buffers->expanded) >= sizeof(fn->return_type)) return 0;
    copy_text(fn->return_type, sizeof(fn->return_type), buffers->expanded);
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *statement = &fn->stmts[s];
        if(strchr(statement->text, '(') == NULL)
            continue; /* no type application to rewrite */
        /* A long statement, such as a table literal, gets a buffer of its
         * own; each application's generated name is shorter than it. */
        size_t length = strlen(statement->text);
        size_t capacity = sizeof(buffers->expanded);
        char *expanded = buffers->expanded;
        if(length * 2 + 256 > capacity) {
            capacity = length * 2 + 256;
            expanded = malloc(capacity);
            if(expanded == NULL) return 0;
        }
        int ok = rewrite_type_applications(module, statement->text, expanded,
                                           capacity, statement->span, 0);
        if(ok) statement->text = KeepText(expanded);
        if(expanded != buffers->expanded) free(expanded);
        if(!ok) return 0;
    }
    StructureFunction(fn, module);
    return 1;
}

COMPILER_SCRATCH_RETURN(static, int, rewrite_function_type_applications,
    (ZirModule *module, ZirFunction *fn),
    RewriteFunctionTypeApplicationsBuffers, RECOVERABLE, free,
    rewrite_function_type_applications_with_buffers(module, fn, buffers))
/* Buffers normalize_type_applications keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NormalizeTypeApplicationsBuffers {
    char expanded[ZIR_TEXT_MAX * 2];
    char arguments[sizeof(((ZirType *)0)->template_args)];
    char canonical[sizeof(((ZirType *)0)->template_args)];
    char body[sizeof(((ZirType *)0)->body)];
} NormalizeTypeApplicationsBuffers;

int normalize_type_applications(ZirModule *module);

static int
normalize_type_applications_with_buffers(ZirModule *module, NormalizeTypeApplicationsBuffers *buffers)
{
    int original_types = module->type_count;
    for(int t = 0; t < original_types; t++) {
        ZirType *type = &module->types[t];
        if(type->is_type_instance) {
            copy_text(buffers->arguments, sizeof(buffers->arguments), type->template_args);
            if(!rewrite_type_applications(module, buffers->arguments,
                    buffers->expanded, sizeof(buffers->expanded), type->span, 0)) return 0;
            if(!canonical_type_arguments(buffers->expanded, buffers->canonical,
                    sizeof(buffers->canonical)))
                return 0;
            copy_text(module->types[t].template_args,
                      sizeof(module->types[t].template_args), buffers->canonical);
        } else if(!type->is_record_template &&
                  !type->is_enum && type->body[0]) {
            copy_text(buffers->body, sizeof(buffers->body), type->body);
            if(!rewrite_type_applications(module, buffers->body, buffers->expanded,
                    sizeof(buffers->expanded), type->span, 0)) return 0;
            copy_text(module->types[t].body,
                      sizeof(module->types[t].body), buffers->expanded);
        }
    }
    for(int g = 0; g < module->global_count; g++) {
        ZirGlobal *global = &module->globals[g];
        if(!rewrite_type_applications(module, global->type, buffers->expanded,
                sizeof(buffers->expanded), global->span, 0)) return 0;
        if(strlen(buffers->expanded) >= sizeof(global->type)) return 0;
        copy_text(global->type, sizeof(global->type), buffers->expanded);
    }
    for(int i = 0; i < module->import_count; i++) {
        ZirImport *binding = &module->imports[i];
        if(binding->kind != ZIR_IMPORT_EXTERN) continue;
        if(!rewrite_type_applications(module, binding->args, buffers->expanded,
                sizeof(buffers->expanded), binding->span, 0)) return 0;
        if(strlen(buffers->expanded) >= sizeof(binding->args)) return 0;
        copy_text(binding->args, sizeof(binding->args), buffers->expanded);
        if(!rewrite_type_applications(module, binding->return_type, buffers->expanded,
                sizeof(buffers->expanded), binding->span, 0)) return 0;
        if(strlen(buffers->expanded) >= sizeof(binding->return_type)) return 0;
        copy_text(binding->return_type, sizeof(binding->return_type), buffers->expanded);
    }
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *fn = &module->functions[f];
        if(fn->from_ir || fn->is_template) continue;
        if(!rewrite_function_type_applications(module, fn)) return 0;
    }
    return 1;
}

COMPILER_SCRATCH_RETURN(, int, normalize_type_applications,
    (ZirModule *module),
    NormalizeTypeApplicationsBuffers, RECOVERABLE, free,
    normalize_type_applications_with_buffers(module, buffers))
/* Buffers order_local_types keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct OrderLocalTypesBuffers {
    ZirType needed;
} OrderLocalTypesBuffers;

int order_local_types(ZirModule *module);

static int
order_local_types_with_buffers(ZirModule *module, OrderLocalTypesBuffers *buffers)
{
    int count = module->type_count;
    if(count == 0) return 1;
    for(int pass = 0; pass < count * count; pass++) {
        int moved = 0;
        for(int t = 0; t < count && !moved; t++) {
            const ZirType *owner = &module->types[t];
            if(owner->is_enum || owner->is_record_template || owner->is_procedure_type ||
               owner->is_map)
                continue;
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(owner, &offset, &field) == 1) {
                for(int dependency = t + 1; dependency < count; dependency++) {
                    if(strcmp(module->types[dependency].name, field.type) != 0)
                        continue;
                    buffers->needed = module->types[dependency];
                    memmove(&module->types[t + 1], &module->types[t],
                            (size_t)(dependency - t) * sizeof(buffers->needed));
                    module->types[t] = buffers->needed;
                    moved = 1;
                    break;
                }
                if(moved) break;
            }
        }
        if(!moved) return 1;
    }
    Diagnostic(module->span, "check.type_order",
               "record values contain a cyclic type dependency");
    return 0;
}

/* Native interfaces define records by value. Put a field's local record
 * before its owner, including records created by nested type application. */
COMPILER_SCRATCH_RETURN(, int, order_local_types,
    (ZirModule *module),
    OrderLocalTypesBuffers, RECOVERABLE, free,
    order_local_types_with_buffers(module, buffers))

/* Type spellings are validated after parsing and after generic expansion so
 * source and saved IR cannot disagree about which declarations are Jai. */
int
JaiTypeSpelling(ZirSourceSpan span, const char *type)
{
    const char *start = skip_ws(type);
    int brackets = 0, parens = 0;
    char operand[ZIR_TEXT_MAX];
    /* type_of holds an expression, which the checker types in place. */
    if(TypeOfOperand(start, operand, sizeof(operand)))
        return 1;
    for(const char *p = start; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
            continue;
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *word = p;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            if(!brackets && (size_t)(p - word) == 5 &&
               !strncmp(word, "const", 5)) {
                Diagnostic(span, "check.jai_syntax",
                           "const qualifier is not Jai syntax: %s", type);
                return 0;
            }
            if(!brackets && (size_t)(p - word) == 4 &&
               !strncmp(word, "char", 4)) {
                Diagnostic(span, "check.jai_syntax",
                           "non-Jai primitive type spelling: char");
                return 0;
            }
            continue;
        }
        if(*p == '[') brackets++;
        else if(*p == ']' && brackets) brackets--;
        else if(*p == '(') parens++;
        else if(*p == ')' && parens) parens--;
        else if(*p == '=' && !brackets && !parens) break;
        else if(*p == '*' && !brackets) {
            const char *previous = p;
            while(previous > start && isspace((unsigned char)previous[-1]))
                previous--;
            if(previous > start &&
               (isalnum((unsigned char)previous[-1]) ||
                previous[-1] == '_' || previous[-1] == ')')) {
                Diagnostic(span, "check.jai_syntax",
                           "C-style pointer type is not Jai syntax; use *Type: %s",
                           type);
                return 0;
            }
        }
        p++;
    }
    return 1;
}
/* Buffers jai_parameter_types keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct JaiParameterTypesBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} JaiParameterTypesBuffers;

static int jai_parameter_types(ZirSourceSpan span, const char *args);

static int
jai_parameter_types_with_buffers(ZirSourceSpan span, const char *args, JaiParameterTypesBuffers *buffers)
{
    int count = *skip_ws(args) ?
        split_top_level(args, buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(buffers->parameters[i], ':');
        if(colon != NULL && !JaiTypeSpelling(span, colon + 1))
            return 0;
    }
    return 1;
}

COMPILER_SCRATCH_RETURN(static, int, jai_parameter_types,
    (ZirSourceSpan span, const char *args),
    JaiParameterTypesBuffers, RECOVERABLE, free,
    jai_parameter_types_with_buffers(span, args, buffers))

int
jai_module_types(const ZirModule *module)
{
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *definition = &module->defines[i];
        const char *value = skip_ws(definition->value);
        size_t length = strlen(value);
        while(length > 0 && isspace((unsigned char)value[length - 1]))
            length--;
        if(length > 0 && value[length - 1] == '*' &&
           !JaiTypeSpelling(definition->span, value)) return 0;
    }
    for(int i = 0; i < module->global_count; i++)
        if(!JaiTypeSpelling(module->globals[i].span,
                              module->globals[i].type)) return 0;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *imp = &module->imports[i];
        if(imp->kind == ZIR_IMPORT_EXTERN &&
           (!jai_parameter_types(imp->span, imp->args) ||
            !JaiTypeSpelling(imp->span, imp->return_type))) return 0;
    }
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *record = &module->types[i];
        if(record->is_procedure_type) {
            if(!jai_parameter_types(record->span, record->body) ||
               !JaiTypeSpelling(record->span, record->procedure_return_type)) return 0;
            continue;
        }
        if(record->is_enum) {
            if(!EnumMemberValue(record, NULL, NULL)) {
                Diagnostic(record->span, "check.enum",
                           "invalid enum value or value outside backing type: %s",
                           record->name);
                return 0;
            }
            continue;
        }
        size_t offset = 0;
        ZirTypeField field;
        int status;
        while((status = TypeNextField(record, &offset, &field)) == 1)
            if(!JaiTypeSpelling(record->span, field.type)) return 0;
        if(status < 0) continue; /* existing record diagnostics report this */
    }
    for(int i = 0; i < module->function_count; i++) {
        const ZirFunction *fn = &module->functions[i];
        if(!jai_parameter_types(fn->span, FunctionArgs(fn)) ||
           !JaiTypeSpelling(fn->span, fn->return_type)) return 0;
        for(int s = 0; s < fn->stmt_count; s++)
            if(fn->stmts[s].kind == ZIR_STMT_DECL &&
               fn->stmts[s].type[0] &&
               !JaiTypeSpelling(fn->stmts[s].span,
                                   fn->stmts[s].type)) return 0;
    }
    return 1;
}

/* Instance types added after the program pass filled the others, as when a
 * specialized signature names Table(string, s32): give each its fields, then
 * put every record before the records that hold it by value. */
int
fill_late_type_instances(ZirModule *module, ZirSourceSpan span)
{
    int filled = 0;
    char *body = NULL, *expanded = NULL;
    for(int t = 0; t < module->type_count; t++) {
        if(!module->types[t].is_type_instance) continue;
        const ZirModule *generic_owner = NULL;
        const ZirType *generic = FindType(module, module->types[t].template_name,
                                          &generic_owner);
        if(generic == NULL || !generic->is_record_template ||
           !InstantiateGenericRecordAt(&module->types[t], generic, generic_owner, module)) {
            Diagnostic(span, "check.specialize",
                       "invalid generic type specialization: %s",
                       module->types[t].name);
            goto failed;
        }
        filled = 1;
        if(!module->types[t].body[0]) continue;
        if(body == NULL) {
            body = malloc(sizeof(module->types[t].body));
            expanded = malloc(sizeof(module->types[t].body));
            if(body == NULL || expanded == NULL) goto failed;
        }
        copy_text(body, sizeof(module->types[t].body), module->types[t].body);
        /* Nested applications, such as Vec(string), append more instances
         * that this loop reaches in turn. */
        if(!rewrite_type_applications(module, body, expanded,
                                      sizeof(module->types[t].body),
                                      module->types[t].span, 0))
            goto failed;
        copy_text(module->types[t].body, sizeof(module->types[t].body), expanded);
    }
    free(body);
    free(expanded);
    return !filled || order_local_types(module);
failed:
    free(body);
    free(expanded);
    return 0;
}

/* A substituted instance may spell a generic record application, such as
 * *Table(string, s32); name it by the module's instance type instead. */
static int
canonical_field(ZirModule *module, char *field, size_t capacity, ZirSourceSpan span)
{
    if(strchr(field, '(') == NULL)
        return 1;
    char *expanded = malloc(ZIR_TEXT_MAX);
    if(expanded == NULL) return 0;
    int ok = rewrite_type_applications(module, field, expanded, ZIR_TEXT_MAX, span, 0) &&
             strlen(expanded) < capacity && fill_late_type_instances(module, span);
    if(ok) copy_text(field, capacity, expanded);
    free(expanded);
    return ok;
}

static int
substitute_field(char *field, size_t capacity, const char *parameter,
                 const char *concrete)
{
    char *copy = malloc(capacity);
    if(copy == NULL) return 0;
    memcpy(copy, field, capacity);
    int ok = replace_template_type(field, capacity, copy,
                                   parameter, concrete);
    free(copy);
    return ok;
}

/* Kept parameter text edited as the in-place helpers edit a buffer: on a
 * heap copy, since parameters fill ZIR_TEXT_MAX bytes, and kept after. */
static int
substitute_parameters(const char **field, const char *parameter,
                      const char *concrete)
{
    char *text = AllocateOrExit(ZIR_TEXT_MAX);
    copy_text(text, ZIR_TEXT_MAX, *field);
    int ok = substitute_field(text, ZIR_TEXT_MAX, parameter, concrete);
    *field = KeepParameters(text);
    free(text);
    return ok;
}

static int
canonical_parameters(ZirModule *module, const char **field, ZirSourceSpan span)
{
    char *text = AllocateOrExit(ZIR_TEXT_MAX);
    copy_text(text, ZIR_TEXT_MAX, *field);
    int ok = canonical_field(module, text, ZIR_TEXT_MAX, span);
    *field = KeepParameters(text);
    free(text);
    return ok;
}
/* Buffers instantiate_specializations keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct InstantiateSpecializationsBuffers {
    ZirFunction original;
} InstantiateSpecializationsBuffers;

int instantiate_specializations(Checker *checker);

/* A template instantiated beside a caller still calls its defining module's
 * private helpers. Retain those helpers under generated identities, without
 * making their source names importable. The duplicate lives in its defining
 * module, so ordinary helper bodies keep their original lexical scope. */
static int
retain_template_dependency(ZirModule *module, const ZirFunction *function,
                           char *name, size_t capacity)
{
    int written = snprintf(name, capacity, "__zi_dependency_%016llx",
        (unsigned long long)specialization_hash(module, module, function, ""));
    if(written < 0 || (size_t)written >= capacity) return 0;
    for(int f = 0; f < module->function_count; f++) {
        const ZirFunction *other = &module->functions[f];
        if(strcmp(other->name, name)) continue;
        return other->is_public && !other->is_file_private &&
               !strcmp(SpanPath(other->span), SpanPath(function->span)) &&
               other->span.line == function->span.line &&
               other->span.column == function->span.column &&
               !strcmp(FunctionArgs(other), FunctionArgs(function)) &&
               !strcmp(other->return_type, function->return_type);
    }
    ZirFunction *copy = AllocateOrExit(sizeof(*copy));
    *copy = *function;
    ZirFunction *retained = ModuleAddFunction(module, name,
        FunctionArgs(copy), copy->return_type, 0, copy->span);
    if(retained == NULL) { free(copy); return 0; }
    *retained = *copy;
    copy_text(retained->name, sizeof(retained->name), name);
    retained->is_public = 1;
    retained->is_file_private = 0;
    retained->exported = 0;
    /* Its graph and lexical owner are unchanged. Ordinary helpers have
     * already been checked; template helpers are checked when instantiated. */
    retained->from_ir = 1;
    retained->stmts = NULL;
    retained->exprs = NULL;
    retained->stmt_cap = retained->stmt_count;
    retained->expr_cap = retained->expr_count;
    if(copy->stmt_count > 0) {
        retained->stmts = AllocateOrExit((size_t)copy->stmt_count * sizeof(*copy->stmts));
        memcpy(retained->stmts, copy->stmts, (size_t)copy->stmt_count * sizeof(*copy->stmts));
    }
    if(copy->expr_count > 0) {
        retained->exprs = AllocateOrExit((size_t)copy->expr_count * sizeof(*copy->exprs));
        memcpy(retained->exprs, copy->exprs, (size_t)copy->expr_count * sizeof(*copy->exprs));
    }
    free(copy);
    return 1;
}

static int
instantiate_specializations_with_buffers(Checker *checker, InstantiateSpecializationsBuffers *buffers)
{
    for(int request_index = 0;
        request_index < checker->specialization_count; request_index++) {
        SpecializationRequest *request =
            &checker->specializations[request_index];
        ZirModule *template_owner = request->template_owner;
        ZirModule *owner = request->instance_owner;
        buffers->original = template_owner->functions[request->template_index];
        ZirFunction *instance = ModuleAddFunction(owner, request->name,
            FunctionArgs(&buffers->original), buffers->original.return_type, 0, buffers->original.span);
        if(instance == NULL) return 0;
        *instance = buffers->original;
        copy_text(instance->name, sizeof(instance->name), request->name);
        copy_text(instance->specialization_type,
                  sizeof(instance->specialization_type), request->type);
        instance->is_template = 0;
        instance->is_specialization = 1;
        instance->is_public = 1;
        instance->is_file_private = 0;
        instance->exported = 0;
        if(owner != template_owner)
            instance->span = request->call_span;
        instance->checked = 0;
        instance->from_ir = 1;
        instance->uses_host = 0;
        instance->stmts = NULL;
        instance->exprs = NULL;
        instance->stmt_cap = instance->stmt_count;
        instance->expr_cap = instance->expr_count;
        if(instance->stmt_count > 0) {
            instance->stmts = malloc((size_t)instance->stmt_count *
                                     sizeof(*instance->stmts));
            if(instance->stmts == NULL) return 0;
            memcpy(instance->stmts, buffers->original.stmts,
                   (size_t)instance->stmt_count * sizeof(*instance->stmts));
        }
        if(instance->expr_count > 0) {
            instance->exprs = malloc((size_t)instance->expr_count *
                                     sizeof(*instance->exprs));
            if(instance->exprs == NULL) return 0;
            memcpy(instance->exprs, buffers->original.exprs,
                   (size_t)instance->expr_count * sizeof(*instance->exprs));
        }
        const char *parameter = buffers->original.template_param;
        const char *concrete = request->type;
        /* A second check of saved IR must see the same array signature. */
        if(!template_parameters_at_use(owner, template_owner, &buffers->original, "",
                                      &instance->args_text) ||
           !template_type_at_use(owner, template_owner, &buffers->original, "",
                                 instance->return_type, sizeof(instance->return_type)) ||
           !substitute_parameters(&instance->args_text, parameter, concrete) ||
           !substitute_parameters(&instance->default_args_text,
                                  parameter, concrete) ||
           !substitute_field(instance->return_type,
                             sizeof(instance->return_type),
                             parameter, concrete) ||
           !normalize_function_arrays(owner, instance)) return 0;
        if(!canonical_parameters(owner, &instance->args_text, instance->span) ||
           !canonical_field(owner, instance->return_type, sizeof(instance->return_type),
                            instance->span)) return 0;
        for(int s = 0; s < instance->stmt_count; s++) {
            char type[ZIR_NAME_MAX];
            copy_text(type, sizeof(type), instance->stmts[s].type);
            if(!template_type_at_use(owner, template_owner, &buffers->original, "", type, sizeof(type)) ||
               !substitute_field(type, sizeof(type), parameter, concrete) ||
               !canonical_field(owner, type, sizeof(type),
                                instance->stmts[s].span)) return 0;
            instance->stmts[s].type = KeepName(type);
        }
        for(int x = 0; x < instance->expr_count; x++) {
            ZirExpr *expression = &instance->exprs[x];
            char type[ZIR_NAME_MAX], slot_type[ZIR_NAME_MAX];
            copy_text(type, sizeof(type), expression->type);
            copy_text(slot_type, sizeof(slot_type), expression->slot_type);
            if(!template_type_at_use(owner, template_owner, &buffers->original, "", type, sizeof(type)) ||
               !template_type_at_use(owner, template_owner, &buffers->original, "", slot_type, sizeof(slot_type)) ||
               !substitute_field(type, sizeof(type), parameter, concrete) ||
               !substitute_field(slot_type, sizeof(slot_type),
                                 parameter, concrete) ||
               !canonical_field(owner, type, sizeof(type),
                                expression->span)) return 0;
            expression->type = KeepName(type);
            expression->slot_type = KeepName(slot_type);
            if(expression->kind == ZIR_EXPR_CAST ||
               expression->kind == ZIR_EXPR_COMPOUND ||
               expression->kind == ZIR_EXPR_SIZE_OF ||
               expression->kind == ZIR_EXPR_CALL) {
                char name[ZIR_NAME_MAX];
                copy_text(name, sizeof(name), expression->name);
                if((expression->kind != ZIR_EXPR_CALL &&
                    !template_type_at_use(owner, template_owner, &buffers->original, "", name, sizeof(name))) ||
                   !substitute_field(name, sizeof(name), parameter, concrete) ||
                   (expression->kind != ZIR_EXPR_CALL &&
                    !canonical_field(owner, name, sizeof(name),
                                     expression->span))) return 0;
                expression->name = KeepName(name);
            }
            if(owner != template_owner &&
               expression->kind == ZIR_EXPR_CALL &&
               strchr(expression->name, '.') == NULL) {
                const ZirModule *callee_owner = NULL;
                const ZirFunction *callee = NULL;
                if(ResolveFunctionAt(template_owner, expression->name,
                                     SpanPath(buffers->original.span),
                                     &callee_owner, &callee) == 1 &&
                   callee_owner == template_owner && callee != NULL) {
                    for(int import_index = 0;
                        import_index < owner->import_count; import_index++) {
                        const ZirImport *import = &owner->imports[import_index];
                        if(import->kind != ZIR_IMPORT_MODULE ||
                           import->resolved_module != template_owner ||
                           (import->is_file_private &&
                            strcmp(SpanPath(import->span),
                                   SpanPath(instance->span)))) continue;
                        char dependency[ZIR_NAME_MAX];
                        const char *callee_name = expression->name;
                        if(!callee->is_public || callee->is_file_private) {
                            if(!retain_template_dependency(template_owner, callee,
                                                            dependency, sizeof(dependency))) return 0;
                            callee_name = dependency;
                        }
                        char qualified[ZIR_NAME_MAX];
                        int written = snprintf(qualified, sizeof(qualified),
                            "%s.%s", import->name, callee_name);
                        if(written < 0 ||
                           (size_t)written >= sizeof(qualified)) return 0;
                        expression->name = KeepName(qualified);
                        break;
                    }
                }
            }
        }
    }
    checker->specialization_count = 0;
    return 1;
}

COMPILER_SCRATCH_RETURN(, int, instantiate_specializations,
    (Checker *checker),
    InstantiateSpecializationsBuffers, RECOVERABLE, free,
    instantiate_specializations_with_buffers(checker, buffers))

int LinkImports(ZirProgram **programs, int count)
{
    CompilerContext *previous = CompilerContextSelect(count > 0 && programs && programs[0] ? programs[0]->context : CompilerContextCurrent());
    int result = compiler_link_LinkImports(programs, count);
    CompilerContextSelect(previous);
    return result;
}
