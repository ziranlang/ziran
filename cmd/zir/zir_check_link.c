#include "zir_check_internal.h"

int
LinkImports(ZirProgram **programs, int count)
{
    /* Open imports have no source alias, but checked types need a stable
     * qualifier when the consumer declares a type with the same name. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            int source_count = module->import_count;
            for(int i = 0; i < source_count; i++) {
                if(module->imports[i].kind != ZIR_IMPORT_OPEN)
                    continue;
                int existing = 0;
                for(int j = 0; j < module->import_count; j++)
                    if(module->imports[j].kind == ZIR_IMPORT_MODULE &&
                       strcmp(module->imports[j].signature,
                              "internal-open") == 0 &&
                       strcmp(module->imports[j].target,
                              module->imports[i].target) == 0) {
                        existing = 1;
                        break;
                    }
                if(existing) continue;
                char target[ZIR_PATH_MAX];
                copy_text(target, sizeof(target), module->imports[i].target);
                ZirSourceSpan span = module->imports[i].span;
                char alias[ZIR_NAME_MAX];
                for(int suffix = 0;; suffix++) {
                    int length = snprintf(alias, sizeof(alias),
                                          "__zi_open_%d", suffix);
                    if(length < 0 || (size_t)length >= sizeof(alias)) {
                        Diagnostic(span, "check.import",
                                   "too many internal open imports");
                        return 0;
                    }
                    int occupied = 0;
                    for(int j = 0; j < module->import_count; j++)
                        if(module->imports[j].kind == ZIR_IMPORT_MODULE &&
                           strcmp(module->imports[j].name, alias) == 0) {
                            occupied = 1;
                            break;
                        }
                    if(!occupied) break;
                }
                if(ModuleAddImport(module, ZIR_IMPORT_MODULE, alias, target,
                                   "internal-open", 0, span) == NULL) {
                    Diagnostic(span, "check.import",
                               "cannot allocate internal open import");
                    return 0;
                }
            }
        }
    }
    /* Every source import names a Ziran module in the current build. */
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                ZirImport *import = &module->imports[i];
                import->resolved_module = NULL;
                if(import->kind == ZIR_IMPORT_MODULE)
                    for(int previous = 0; previous < i; previous++)
                        if(module->imports[previous].kind == ZIR_IMPORT_MODULE &&
                           strcmp(module->imports[previous].name,
                                  import->name) == 0) {
                            Diagnostic(import->span, "check.import",
                                       "duplicate import alias: %s", import->name);
                            return 0;
                        }
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
                if(import->kind != ZIR_IMPORT_OPEN &&
                   import->kind != ZIR_IMPORT_MODULE)
                    continue;
                if(import->target[0] == '\0' ||
                   (!isalpha((unsigned char)import->target[0]) &&
                    import->target[0] != '_')) {
                    Diagnostic(import->span, "check.import",
                               "invalid Jai module name: %s", import->target);
                    return 0;
                }
                for(const unsigned char *cursor =
                        (const unsigned char *)import->target;
                    *cursor; cursor++)
                    if(!isalnum(*cursor) && *cursor != '_') {
                        Diagnostic(import->span, "check.import",
                                   "invalid Jai module name: %s", import->target);
                        return 0;
                    }
                for(int q = 0; q < count; q++) {
                    for(int n = 0; n < programs[q]->module_count; n++) {
                        const ZirModule *candidate = &programs[q]->modules[n];
                        char stem[ZIR_PATH_MAX];
                        size_t length;
                        copy_text(stem, sizeof(stem), candidate->source_path);
                        length = strlen(stem);
                        if(length > 3 && strcmp(stem + length - 3, ".zi") == 0)
                            stem[length - 3] = '\0';
                        if(strcmp(import->target, candidate->name) != 0 &&
                           strcmp(import->target, stem) != 0)
                            continue;
                        if(import->resolved_module && import->resolved_module != candidate) {
                            Diagnostic(import->span, "check.import", "ambiguous Ziran import: %s",
                                          import->target);
                            return 0;
                        }
                        import->resolved_module = candidate;
                    }
                }
                if(import->resolved_module == NULL) {
                    Diagnostic(import->span, "check.import",
                               "unresolved Jai module: %s", import->target);
                    return 0;
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

int
rewrite_type_applications(ZirModule *module, const char *source,
                          char *output, size_t capacity,
                          ZirSourceSpan span, int recursion)
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
            while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
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
                char arguments[ZIR_TEXT_MAX];
                char expanded[ZIR_TEXT_MAX];
                memcpy(arguments, opening + 1,
                       (size_t)(closing - opening - 1));
                arguments[closing - opening - 1] = '\0';
                if(!rewrite_type_applications(module, arguments, expanded,
                        sizeof(expanded), span, recursion + 1))
                    return 0;
                char canonical[ZIR_TEXT_MAX];
                if(!canonical_type_arguments(expanded, canonical,
                        sizeof(canonical))) {
                    Diagnostic(span, "check.type_application",
                               "type application arguments are too long: %s", base);
                    return 0;
                }
                uint64_t hash = UINT64_C(14695981039346656037);
                for(const unsigned char *p = (const unsigned char *)base; *p; p++)
                    hash = (hash ^ *p) * UINT64_C(1099511628211);
                hash = (hash ^ '(') * UINT64_C(1099511628211);
                for(const unsigned char *p = (const unsigned char *)canonical; *p; p++)
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
                     (strcmp(instance->template_name, base) != 0 ||
                      strcmp(instance->template_args, canonical) != 0)))) {
                    Diagnostic(span, "check.type_application",
                               "type application name collision: %s", base);
                    return 0;
                }
                if(instance == NULL) {
                    int generic_public = generic->is_public;
                    int generic_file_private = generic->is_file_private;
                    instance = ModuleAddType(module, name, span);
                    if(instance == NULL) return 0;
                    instance->is_public = generic_public;
                    instance->is_file_private = generic_file_private;
                    instance->is_type_instance = 1;
                    instance->is_synthetic_application = 1;
                    copy_text(instance->template_name,
                              sizeof(instance->template_name), base);
                    copy_text(instance->template_args,
                              sizeof(instance->template_args), canonical);
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

static int
rewrite_function_type_applications(ZirModule *module, ZirFunction *fn)
{
    char expanded[ZIR_TEXT_MAX * 2];
    if(!rewrite_type_applications(module, fn->args, expanded,
            sizeof(expanded), fn->span, 0)) return 0;
    if(strlen(expanded) >= sizeof(fn->args)) return 0;
    copy_text(fn->args, sizeof(fn->args), expanded);
    if(!rewrite_type_applications(module, fn->return_type, expanded,
            sizeof(expanded), fn->span, 0)) return 0;
    if(strlen(expanded) >= sizeof(fn->return_type)) return 0;
    copy_text(fn->return_type, sizeof(fn->return_type), expanded);
    for(int s = 0; s < fn->stmt_count; s++) {
        ZirStmt *statement = &fn->stmts[s];
        if(!rewrite_type_applications(module, statement->text, expanded,
                sizeof(expanded), statement->span, 0)) return 0;
        if(strlen(expanded) >= sizeof(statement->text)) return 0;
        copy_text(statement->text, sizeof(statement->text), expanded);
    }
    StructureFunction(fn, module);
    return 1;
}

int
normalize_type_applications(ZirModule *module)
{
    char expanded[ZIR_TEXT_MAX * 2];
    int original_types = module->type_count;
    for(int t = 0; t < original_types; t++) {
        ZirType *type = &module->types[t];
        if(type->is_type_instance) {
            char arguments[sizeof(type->template_args)];
            copy_text(arguments, sizeof(arguments), type->template_args);
            if(!rewrite_type_applications(module, arguments,
                    expanded, sizeof(expanded), type->span, 0)) return 0;
            char canonical[sizeof(type->template_args)];
            if(!canonical_type_arguments(expanded, canonical,
                    sizeof(canonical)))
                return 0;
            copy_text(module->types[t].template_args,
                      sizeof(module->types[t].template_args), canonical);
        } else if(!type->is_record_template &&
                  !type->is_enum && type->body[0]) {
            char body[sizeof(type->body)];
            copy_text(body, sizeof(body), type->body);
            if(!rewrite_type_applications(module, body, expanded,
                    sizeof(expanded), type->span, 0)) return 0;
            copy_text(module->types[t].body,
                      sizeof(module->types[t].body), expanded);
        }
    }
    for(int g = 0; g < module->global_count; g++) {
        ZirGlobal *global = &module->globals[g];
        if(!rewrite_type_applications(module, global->type, expanded,
                sizeof(expanded), global->span, 0)) return 0;
        if(strlen(expanded) >= sizeof(global->type)) return 0;
        copy_text(global->type, sizeof(global->type), expanded);
    }
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *fn = &module->functions[f];
        if(fn->from_ir || fn->is_template) continue;
        if(!rewrite_function_type_applications(module, fn)) return 0;
    }
    return 1;
}

/* Native interfaces define records by value. Put a field's local record
 * before its owner, including records created by nested type application. */
int
order_local_types(ZirModule *module)
{
    int count = module->type_count;
    if(count == 0) return 1;
    for(int pass = 0; pass < count * count; pass++) {
        int moved = 0;
        for(int t = 0; t < count && !moved; t++) {
            const ZirType *owner = &module->types[t];
            if(owner->is_enum || owner->is_record_template || owner->is_procedure_type)
                continue;
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(owner, &offset, &field) == 1) {
                for(int dependency = t + 1; dependency < count; dependency++) {
                    if(strcmp(module->types[dependency].name, field.type) != 0)
                        continue;
                    ZirType needed = module->types[dependency];
                    memmove(&module->types[t + 1], &module->types[t],
                            (size_t)(dependency - t) * sizeof(needed));
                    module->types[t] = needed;
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

/* Type spellings are validated after parsing and after generic expansion so
 * source and saved IR cannot disagree about which declarations are Jai. */
int
JaiTypeSpelling(ZirSourceSpan span, const char *type)
{
    const char *start = skip_ws(type);
    int brackets = 0, parens = 0;
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

static int
jai_parameter_types(ZirSourceSpan span, const char *args)
{
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(args) ?
        split_top_level(args, parameters[0], 64, sizeof(parameters[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon != NULL && !JaiTypeSpelling(span, colon + 1))
            return 0;
    }
    return 1;
}

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
        if(!jai_parameter_types(fn->span, fn->args) ||
           !JaiTypeSpelling(fn->span, fn->return_type)) return 0;
        for(int s = 0; s < fn->stmt_count; s++)
            if(fn->stmts[s].kind == ZIR_STMT_DECL &&
               fn->stmts[s].type[0] &&
               !JaiTypeSpelling(fn->stmts[s].span,
                                   fn->stmts[s].type)) return 0;
    }
    return 1;
}

static int
name_conflicts(const ZirModule *module, const PrivateFunctionName *renames,
               int count, const char *name)
{
    for(int i = 0; i < module->function_count; i++)
        if(strcmp(module->functions[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->global_count; i++)
        if(strcmp(module->globals[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->define_count; i++)
        if(strcmp(module->defines[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->type_count; i++)
        if(strcmp(module->types[i].name, name) == 0)
            return 1;
    for(int i = 0; i < module->import_count; i++)
        if(strcmp(module->imports[i].name, name) == 0)
            return 1;
    for(int i = 0; i < count; i++)
        if(strcmp(renames[i].internal, name) == 0)
            return 1;
    return 0;
}

/* File-private declarations may share a source name in different loaded
 * files. Give colliding procedures distinct checked IR names after source
 * resolution, so native backends and the portable linker use one identity. */
int
name_private_functions(ZirModule *module)
{
    PrivateFunctionName *renames = calloc((size_t)module->function_count + 1,
                                          sizeof(*renames));
    int count = 0;
    if(renames == NULL)
        return 0;
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *function = &module->functions[f];
        if(!function->is_file_private)
            continue;
        int collision = 0;
        for(int other = 0; other < module->function_count; other++) {
            const ZirFunction *candidate = &module->functions[other];
            if(other == f || strcmp(candidate->name, function->name) != 0)
                continue;
            if(strcmp(candidate->span.path, function->span.path) == 0) {
                Diagnostic(function->span, "check.file_scope",
                           "duplicate procedure in one file: %s",
                           function->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision)
            continue;
        if(function->exported) {
            Diagnostic(function->span, "check.file_scope",
                       "colliding #program_export procedure: %s",
                       function->name);
            free(renames);
            return 0;
        }
        renames[count].function = function;
        for(int serial = f; ; serial++) {
            snprintf(renames[count].internal,
                     sizeof(renames[count].internal),
                     "zir_file_function_%d", serial);
            if(!name_conflicts(module, renames, count,
                               renames[count].internal))
                break;
        }
        count++;
    }
    if(count == 0) {
        free(renames);
        return 1;
    }
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *caller = &module->functions[f];
        for(int x = 0; x < caller->expr_count; x++) {
            ZirExpr *expression = &caller->exprs[x];
            const ZirModule *owner = NULL;
            const ZirFunction *target = NULL;
            if(!((expression->kind == ZIR_EXPR_CALL &&
                  !expression->slot_type[0]) ||
                 (expression->kind == ZIR_EXPR_IDENT &&
                  expression->is_function_value)) ||
               !expression->name[0] ||
               ResolveFunctionAt(module, expression->name,
                   caller->span.path, &owner, &target) != 1 ||
               owner != module)
                continue;
            for(int i = 0; i < count; i++)
                if(target == renames[i].function) {
                    copy_text(expression->name, sizeof(expression->name),
                              renames[i].internal);
                    break;
                }
        }
        caller->from_ir = 1;
    }
    for(int i = 0; i < count; i++)
        copy_text(renames[i].function->name,
                  sizeof(renames[i].function->name),
                  renames[i].internal);
    free(renames);
    return 1;
}

static int
rewrite_private_reference(char *source, size_t capacity,
                          ZirSourceSpan span, const char *original,
                          const char *internal)
{
    ZirLexer lexer;
    ZirToken previous = {0};
    ZirToken current, next;
    size_t current_end, next_end, copied = 0, written = 0;
    char *output = calloc(capacity, 1);
    if(output == NULL) return 0;
    LexerInit(&lexer, source, span.path);
    current = LexerNext(&lexer);
    current_end = lexer.pos;
    next = LexerNext(&lexer);
    next_end = lexer.pos;
    while(current.kind != ZIR_TOKEN_EOF) {
        if(current.kind == ZIR_TOKEN_IDENT &&
           !strcmp(current.text, original) &&
           strcmp(previous.text, ".") != 0 &&
           strcmp(next.text, ":") != 0) {
            size_t start = current_end - strlen(current.text);
            size_t prefix = start - copied;
            size_t replacement = strlen(internal);
            if(written + prefix + replacement >= capacity) goto too_long;
            memcpy(output + written, source + copied, prefix);
            written += prefix;
            memcpy(output + written, internal, replacement);
            written += replacement;
            copied = current_end;
        }
        previous = current;
        current = next;
        current_end = next_end;
        next = LexerNext(&lexer);
        next_end = lexer.pos;
    }
    size_t suffix = strlen(source + copied);
    if(written + suffix >= capacity) goto too_long;
    memcpy(output + written, source + copied, suffix + 1);
    memcpy(source, output, written + suffix + 1);
    free(output);
    return 1;
too_long:
    Diagnostic(span, "check.file_scope",
               "file-private reference exceeds text limit: %s", original);
    free(output);
    return 0;
}

int
name_private_defines(ZirModule *module)
{
    PrivateDefineName *renames = calloc((size_t)module->define_count + 1,
                                        sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int d = 0; d < module->define_count; d++) {
        ZirDefine *definition = &module->defines[d];
        if(!definition->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->define_count; other++) {
            const ZirDefine *candidate = &module->defines[other];
            if(other == d || strcmp(candidate->name, definition->name))
                continue;
            if(!strcmp(candidate->span.path, definition->span.path)) {
                Diagnostic(definition->span, "check.file_scope",
                           "duplicate constant in one file: %s",
                           definition->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateDefineName *rename = &renames[count];
        rename->definition = definition;
        copy_text(rename->original, sizeof(rename->original),
                  definition->name);
        for(int serial = d; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_constant_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateDefineName *rename = &renames[r];
        const char *path = rename->definition->span.path;
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->type,
                                          sizeof(global->type), global->span,
                                          rename->original, rename->internal) ||
               !rewrite_private_reference(global->init,
                                          sizeof(global->init), global->span,
                                          rename->original, rename->internal))
                goto failed;
        }
        for(int d = 0; d < module->define_count; d++) {
            ZirDefine *definition = &module->defines[d];
            if(strcmp(definition->span.path, path)) continue;
            if(!rewrite_private_reference(definition->value,
                                          sizeof(definition->value),
                                          definition->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int t = 0; t < module->type_count; t++) {
            ZirType *type = &module->types[t];
            if(strcmp(type->span.path, path)) continue;
            if(!rewrite_private_reference(type->body, sizeof(type->body),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->template_args,
                                          sizeof(type->template_args),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].definition->name,
                  sizeof(renames[r].definition->name),
                  renames[r].internal);
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
}

int
name_private_globals(ZirModule *module)
{
    PrivateGlobalName *renames = calloc((size_t)module->global_count + 1,
                                        sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int g = 0; g < module->global_count; g++) {
        ZirGlobal *global = &module->globals[g];
        if(!global->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->global_count; other++) {
            const ZirGlobal *candidate = &module->globals[other];
            if(other == g || strcmp(candidate->name, global->name))
                continue;
            if(!strcmp(candidate->span.path, global->span.path)) {
                Diagnostic(global->span, "check.file_scope",
                           "duplicate global in one file: %s", global->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateGlobalName *rename = &renames[count];
        rename->global = global;
        copy_text(rename->original, sizeof(rename->original), global->name);
        for(int serial = g; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_global_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateGlobalName *rename = &renames[r];
        const char *path = rename->global->span.path;
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->init, sizeof(global->init),
                                          global->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *caller = &module->functions[f];
            if(strcmp(caller->span.path, path)) continue;
            for(int x = 0; x < caller->expr_count; x++) {
                ZirExpr *expression = &caller->exprs[x];
                if(expression->is_global_value &&
                   !strcmp(expression->name, rename->original))
                    copy_text(expression->name, sizeof(expression->name),
                              rename->internal);
            }
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].global->name, sizeof(renames[r].global->name),
                  renames[r].internal);
    if(count > 0)
        for(int f = 0; f < module->function_count; f++)
            module->functions[f].from_ir = 1;
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
}

static int
template_parameter_shadows(const ZirType *type, const char *name)
{
    ZirLexer lexer;
    ZirToken current, next;
    LexerInit(&lexer, type->template_params, type->span.path);
    current = LexerNext(&lexer);
    next = LexerNext(&lexer);
    while(current.kind != ZIR_TOKEN_EOF) {
        if(current.kind == ZIR_TOKEN_IDENT &&
           !strcmp(current.text, name) && !strcmp(next.text, ":"))
            return 1;
        current = next;
        next = LexerNext(&lexer);
    }
    return 0;
}

int
name_private_types(ZirModule *module)
{
    PrivateTypeName *renames = calloc((size_t)module->type_count + 1,
                                      sizeof(*renames));
    int count = 0;
    if(renames == NULL) return 0;
    for(int t = 0; t < module->type_count; t++) {
        ZirType *type = &module->types[t];
        if(!type->is_file_private) continue;
        int collision = 0;
        for(int other = 0; other < module->type_count; other++) {
            const ZirType *candidate = &module->types[other];
            if(other == t || strcmp(candidate->name, type->name)) continue;
            if(!strcmp(candidate->span.path, type->span.path)) {
                Diagnostic(type->span, "check.file_scope",
                           "duplicate type in one file: %s", type->name);
                free(renames);
                return 0;
            }
            collision = 1;
        }
        if(!collision) continue;
        PrivateTypeName *rename = &renames[count];
        rename->type = type;
        copy_text(rename->original, sizeof(rename->original), type->name);
        for(int serial = t; ; serial++) {
            int reserved = 0;
            snprintf(rename->internal, sizeof(rename->internal),
                     "zir_file_type_%d", serial);
            for(int i = 0; i < count; i++)
                reserved |= !strcmp(renames[i].internal, rename->internal);
            if(!reserved && !name_conflicts(module, NULL, 0,
                                            rename->internal)) break;
        }
        count++;
    }
    for(int r = 0; r < count; r++) {
        const PrivateTypeName *rename = &renames[r];
        const char *path = rename->type->span.path;
        for(int t = 0; t < module->type_count; t++) {
            ZirType *type = &module->types[t];
            if(strcmp(type->span.path, path)) continue;
            if(!type->is_enum &&
               !template_parameter_shadows(type, rename->original) &&
               !rewrite_private_reference(type->body, sizeof(type->body),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
            if(!rewrite_private_reference(type->template_name,
                                          sizeof(type->template_name),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->template_args,
                                          sizeof(type->template_args),
                                          type->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(type->procedure_return_type,
                                          sizeof(type->procedure_return_type),
                                          type->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int g = 0; g < module->global_count; g++) {
            ZirGlobal *global = &module->globals[g];
            if(strcmp(global->span.path, path)) continue;
            if(!rewrite_private_reference(global->type, sizeof(global->type),
                                          global->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(global->init, sizeof(global->init),
                                          global->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int d = 0; d < module->define_count; d++) {
            ZirDefine *definition = &module->defines[d];
            if(strcmp(definition->span.path, path)) continue;
            if(!rewrite_private_reference(definition->value,
                                          sizeof(definition->value),
                                          definition->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int i = 0; i < module->import_count; i++) {
            ZirImport *import = &module->imports[i];
            if(strcmp(import->span.path, path)) continue;
            if(!rewrite_private_reference(import->signature,
                                          sizeof(import->signature),
                                          import->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(import->args, sizeof(import->args),
                                          import->span, rename->original,
                                          rename->internal) ||
               !rewrite_private_reference(import->return_type,
                                          sizeof(import->return_type),
                                          import->span, rename->original,
                                          rename->internal)) goto failed;
        }
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *function = &module->functions[f];
            if(strcmp(function->span.path, path)) continue;
            if(!rewrite_private_reference(function->args,
                                          sizeof(function->args), function->span,
                                          rename->original, rename->internal) ||
               !rewrite_private_reference(function->return_type,
                                          sizeof(function->return_type),
                                          function->span, rename->original,
                                          rename->internal)) goto failed;
            for(int s = 0; s < function->stmt_count; s++) {
                ZirStmt *statement = &function->stmts[s];
                if(!rewrite_private_reference(statement->type,
                                              sizeof(statement->type),
                                              statement->span, rename->original,
                                              rename->internal)) goto failed;
            }
            for(int x = 0; x < function->expr_count; x++) {
                ZirExpr *expression = &function->exprs[x];
                if(!rewrite_private_reference(expression->type,
                                              sizeof(expression->type),
                                              expression->span,
                                              rename->original,
                                              rename->internal) ||
                   !rewrite_private_reference(expression->slot_type,
                                              sizeof(expression->slot_type),
                                              expression->span,
                                              rename->original,
                                              rename->internal)) goto failed;
                if(expression->kind == ZIR_EXPR_CAST ||
                   expression->kind == ZIR_EXPR_COMPOUND ||
                   expression->kind == ZIR_EXPR_SIZE_OF ||
                   (expression->kind == ZIR_EXPR_CALL &&
                    rename->type->is_record_template))
                    if(!rewrite_private_reference(expression->name,
                                                  sizeof(expression->name),
                                                  expression->span,
                                                  rename->original,
                                                  rename->internal)) goto failed;
            }
        }
    }
    for(int r = 0; r < count; r++)
        copy_text(renames[r].type->name, sizeof(renames[r].type->name),
                  renames[r].internal);
    if(count > 0)
        for(int f = 0; f < module->function_count; f++)
            module->functions[f].from_ir = 1;
    free(renames);
    return 1;
failed:
    free(renames);
    return 0;
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

int
instantiate_specializations(Checker *checker)
{
    for(int request_index = 0;
        request_index < checker->specialization_count; request_index++) {
        SpecializationRequest *request =
            &checker->specializations[request_index];
        ZirModule *template_owner = request->template_owner;
        ZirModule *owner = request->instance_owner;
        ZirFunction original = template_owner->functions[request->template_index];
        ZirFunction *instance = ModuleAddFunction(owner, request->name,
            original.args, original.return_type, 0, original.span);
        if(instance == NULL) return 0;
        *instance = original;
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
            memcpy(instance->stmts, original.stmts,
                   (size_t)instance->stmt_count * sizeof(*instance->stmts));
        }
        if(instance->expr_count > 0) {
            instance->exprs = malloc((size_t)instance->expr_count *
                                     sizeof(*instance->exprs));
            if(instance->exprs == NULL) return 0;
            memcpy(instance->exprs, original.exprs,
                   (size_t)instance->expr_count * sizeof(*instance->exprs));
        }
        const char *parameter = original.template_param;
        const char *concrete = request->type;
        /* A second check of saved IR must see the same array signature. */
        if(!substitute_field(instance->args, sizeof(instance->args),
                             parameter, concrete) ||
           !substitute_field(instance->default_args,
                             sizeof(instance->default_args),
                             parameter, concrete) ||
           !substitute_field(instance->return_type,
                             sizeof(instance->return_type),
                             parameter, concrete) ||
           !normalize_function_arrays(owner, instance)) return 0;
        for(int s = 0; s < instance->stmt_count; s++)
            if(!substitute_field(instance->stmts[s].type,
                                 sizeof(instance->stmts[s].type),
                                 parameter, concrete)) return 0;
        for(int x = 0; x < instance->expr_count; x++) {
            ZirExpr *expression = &instance->exprs[x];
            if(!substitute_field(expression->type,
                                 sizeof(expression->type),
                                 parameter, concrete) ||
               !substitute_field(expression->slot_type,
                                 sizeof(expression->slot_type),
                                 parameter, concrete)) return 0;
            if(expression->kind == ZIR_EXPR_CAST ||
               expression->kind == ZIR_EXPR_COMPOUND ||
               expression->kind == ZIR_EXPR_SIZE_OF ||
               expression->kind == ZIR_EXPR_CALL)
                if(!substitute_field(expression->name,
                                     sizeof(expression->name),
                                     parameter, concrete)) return 0;
            if(owner != template_owner &&
               expression->kind == ZIR_EXPR_CALL &&
               strchr(expression->name, '.') == NULL) {
                const ZirModule *callee_owner = NULL;
                const ZirFunction *callee = NULL;
                if(ResolveFunctionAt(template_owner, expression->name,
                                     original.span.path,
                                     &callee_owner, &callee) == 1 &&
                   callee_owner == template_owner && callee != NULL) {
                    for(int import_index = 0;
                        import_index < owner->import_count; import_index++) {
                        const ZirImport *import = &owner->imports[import_index];
                        if(import->kind != ZIR_IMPORT_MODULE ||
                           import->resolved_module != template_owner ||
                           !in_lookup_file(owner, import->is_file_private,
                                           import->span)) continue;
                        char qualified[ZIR_NAME_MAX];
                        int written = snprintf(qualified, sizeof(qualified),
                            "%s.%s", import->name, expression->name);
                        if(written < 0 ||
                           (size_t)written >= sizeof(qualified)) return 0;
                        copy_text(expression->name,
                                  sizeof(expression->name), qualified);
                        break;
                    }
                }
            }
        }
    }
    checker->specialization_count = 0;
    return 1;
}
