#include "zir_check_internal.h"

const char *
expression_type(Checker *c, int index)
{
    ZirExpr *e;
    const char *type = "", *left = "", *right = "";
    char member_type[ZIR_NAME_MAX] = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->is_this && e->kind != ZIR_EXPR_IDENT && e->kind != ZIR_EXPR_CALL) {
        error(c, e->span, "invalid #this expression", e->name);
        return "";
    }
    if(e->kind == ZIR_EXPR_CALL && e->type[0]) {
        const char *unqualified = strrchr(e->name, '.');
        unqualified = unqualified == NULL ? e->name : unqualified + 1;
        for(int request = 0; request < c->specialization_count; request++)
            if(!strcmp(unqualified, c->specializations[request].name))
                return e->type;
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *middle = &c->fn->exprs[e->left];
        if(middle->left >= 0 &&
           c->fn->exprs[middle->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[middle->left].name;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, middle->name);
            if(!*lookup_lexical(c, alias) && length >= 0 &&
               (size_t)length < sizeof(qualified)) {
                const ZirType *enumeration = FindType(c->module, qualified,
                                                      NULL);
                if(enumeration != NULL && enumeration->is_enum) {
                    if(lower_enum_reference(c, e, enumeration, e->name)) {
                        copy_text(e->type, sizeof(e->type), qualified);
                        return e->type;
                    }
                    return "";
                }
            }
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const char *alias = c->fn->exprs[e->left].name;
        for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *import = &c->module->imports[i];
            if(import->kind != ZIR_IMPORT_MODULE ||
               !in_lookup_file(c->module, import->is_file_private,
                               import->span) ||
               strcmp(import->name, alias) != 0)
                continue;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, e->name);
            if(length < 0 || (size_t)length >= sizeof(qualified)) {
                error(c, e->span, "qualified name is too long", alias);
                return "";
            }
            e->kind = ZIR_EXPR_IDENT;
            e->left = -1;
            copy_text(e->name, sizeof(e->name), qualified);
            break;
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const ZirType *enumeration = FindType(c->module,
            c->fn->exprs[e->left].name, NULL);
        if(enumeration != NULL && enumeration->is_enum) {
            if(lower_enum_reference(c, e, enumeration, e->name))
                return e->type;
            return "";
        }
    }
    if(e->kind == ZIR_EXPR_IDENT && e->name[0] == '.') {
        const ZirType *enumeration = FindType(c->module, c->expected_type, NULL);
        if(enumeration == NULL || !enumeration->is_enum) {
            error(c, e->span, "inferred enum member needs an enum type", e->name);
            return "";
        }
        if(lower_enum_reference(c, e, enumeration, e->name + 1))
            return e->type;
        return "";
    }
    if(e->kind == ZIR_EXPR_CONDITIONAL && !strcmp(e->op, "#ifx")) {
        error(c, e->span, "unlowered #ifx expression", e->text);
        return "";
    }
    if(e->kind == ZIR_EXPR_SIZE_OF) {
        size_t size, alignment;
        char operand[ZIR_TEXT_MAX], resolved[ZIR_NAME_MAX];
        const char *sized_type = e->name;
        int from_value = TypeOfOperand(e->name, operand, sizeof(operand));
        if(!from_value && !JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        if(from_value) {
            ZirFunction probe = {0};
            ZirFunction *saved_fn = c->fn;
            ZirStmt *saved_stmt = c->current_stmt;
            int root = ParseExpr(&probe, c->module, operand, e->span);
            c->fn = &probe;
            c->current_stmt = NULL;
            const char *inferred = expression_type(c, root);
            copy_text(resolved, sizeof(resolved),
                      !strcmp(inferred, "integer") ? "s64" :
                      !strcmp(inferred, "real") ? "float64" : inferred);
            c->fn = saved_fn;
            c->current_stmt = saved_stmt;
            free(probe.exprs);
            sized_type = resolved;
        }
        if(!TypeLayout(c->module, sized_type, &size, &alignment)) {
            error(c, e->span, "size_of requires a known sized type", e->name);
            return "";
        }
        char replacement[ZIR_TEXT_MAX];
        snprintf(replacement, sizeof(replacement), "size_of(%s)", sized_type);
        if(!rewrite_checked_text(c, e, replacement)) {
            error(c, e->span, "cannot lower size_of expression", e->text);
            return "";
        }
        if(!c->fn->from_ir)
            copy_text(e->text, sizeof(e->text), replacement);
        if(sized_type != e->name)
            copy_text(e->name, sizeof(e->name), sized_type);
        e->left = e->right = e->third = -1;
        copy_text(e->type, sizeof(e->type), "integer");
        return e->type;
    }
    if(e->is_function_value)
        return e->type;
    if(e->kind == ZIR_EXPR_CALL && e->name[0] == '\0' && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *member = &c->fn->exprs[e->left];
        if(member->left >= 0 &&
           c->fn->exprs[member->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[member->left].name;
            for(int i = 0; i < c->module->import_count; i++) {
                const ZirImport *import = &c->module->imports[i];
                if(import->kind != ZIR_IMPORT_MODULE ||
                   !in_lookup_file(c->module, import->is_file_private,
                                   import->span) ||
                   strcmp(import->name, alias) != 0)
                    continue;
                int length = snprintf(e->name, sizeof(e->name),
                                      "%s.%s", alias, member->name);
                if(length < 0 || (size_t)length >= sizeof(e->name))
                    error(c, e->span, "qualified call name is too long", alias);
                else
                    e->left = -1;
                break;
            }
        }
    }
    if(e->left >= 0 && !(e->kind == ZIR_EXPR_CALL && e->name[0]))
        left = expression_type(c, e->left);
    if(e->right >= 0) {
        char saved_expected[ZIR_NAME_MAX];
        copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
        if(e->kind == ZIR_EXPR_BINARY) {
            const ZirType *enumeration = FindType(c->module, left, NULL);
            if(enumeration != NULL && enumeration->is_enum)
                copy_text(c->expected_type, sizeof(c->expected_type), left);
        }
        right = expression_type(c, e->right);
        copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
    }
    switch(e->kind) {
    case ZIR_EXPR_COMPOUND: {
        if(!e->name[0]) {
            if(!c->expected_type[0]) {
                error(c, e->span,
                      "inferred record literal needs a record type", e->text);
                break;
            }
            copy_text(e->name, sizeof(e->name), c->expected_type);
        }
        if(SliceElementType(e->name, NULL, 0)) {
            error(c, e->span, "slice literals require a backing range", e->name);
            break;
        }
        if(e->name[0] == '[') {
            char element[ZIR_NAME_MAX];
            int capacity = 0;
            const char *problem = local_storage_error(c->module, e->name);
            if(problem != NULL) {
                error(c, e->span, problem, e->name);
                break;
            }
            normalize_array(c->module, e->name, sizeof(e->name));
            ArrayElementType(e->name, element, sizeof(element), &capacity);
            if(capacity < 0)
                error(c, e->span, "array literals require a resolved capacity", e->name);
            int count = 0;
            for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
                ZirExpr *entry = &c->fn->exprs[child];
                if(!strcmp(entry->op, "="))
                    error(c, entry->span, "array literals require positional elements", entry->name);
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                copy_text(c->expected_type, sizeof(c->expected_type), element);
                const char *value_type = expression_type(c, entry->right);
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(!compatible_checked(c, element, value_type))
                    error(c, entry->span, "array initializer element type mismatch", element);
                copy_text(entry->type, sizeof(entry->type), element);
                count++;
            }
            if(capacity >= 0 && count > capacity)
                error(c, e->span, "too many array initializer elements", e->name);
            type = e->name;
            break;
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, e->name, &record_owner);
        int ordinal = 0;
        int mode = -1;
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           record->is_record_template) {
            error(c, e->span, "initializer requires a declared record type", e->name);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            ZirExpr *entry = &c->fn->exprs[child];
            int named = !strcmp(entry->op, "=");
            if(mode >= 0 && mode != named)
                error(c, entry->span, "cannot mix named and positional record fields", e->name);
            mode = named;
            size_t offset = 0;
            ZirTypeField field;
            int position = 0;
            int found = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) : position == ordinal) {
                    found = 1;
                    break;
                }
                position++;
            }
            if(found) {
                normalize_array(record_owner, field.type, sizeof(field.type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             e->name, field.type,
                                             sizeof(field.type)))
                    error(c, entry->span,
                          "imported record field type is shadowed",
                          field.name);
                ZirExpr *initializer = &c->fn->exprs[entry->right];
                if(initializer->kind == ZIR_EXPR_COMPOUND)
                    normalize_array(record_owner, initializer->name, sizeof(initializer->name));
            }
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(found)
                copy_text(c->expected_type, sizeof(c->expected_type), field.type);
            const char *value_type = expression_type(c, entry->right);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(!found) {
                error(c, entry->span, named ? "unknown initializer field" : "too many positional record fields", entry->name);
                ordinal++;
                continue;
            }
            for(int previous = e->first_child; previous != child; previous = c->fn->exprs[previous].next_sibling) {
                if(!strcmp(c->fn->exprs[previous].name, field.name))
                    error(c, entry->span, "duplicate initializer field", field.name);
            }
            copy_text(entry->name, sizeof(entry->name), field.name);
            copy_text(entry->type, sizeof(entry->type), field.type);
            if(!compatible_checked(c, field.type, value_type))
                error(c, entry->span, "initializer field type mismatch", field.name);
            if(contains_vec(c->module, field.type, 0) &&
               !owned_initializer_shape(c, entry->right))
                error(c, entry->span,
                      "owned record field moves a binding or takes a call result",
                      field.name);
            ordinal++;
        }
        type = e->name;
        break;
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER: {
        char record_name[ZIR_NAME_MAX] = "";
        if(e->kind == ZIR_EXPR_MEMBER && left[0] == '*') {
            e->kind = ZIR_EXPR_POINTER_MEMBER;
            copy_text(e->op, sizeof(e->op), "->");
        }
        if(e->kind == ZIR_EXPR_POINTER_MEMBER) {
            const char *base = skip_ws(left);
            if(!strncmp(base, "const ", 6))
                base = skip_ws(base + 6);
            if(*base == '*') {
                base = skip_ws(base + 1);
                if(!*base || strlen(base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                copy_text(record_name, sizeof(record_name), base);
            } else {
                const char *star = strchr(base, '*');
                if(star == NULL || *skip_ws(star + 1) ||
                   star == base || (size_t)(star - base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                size_t length = (size_t)(star - base);
                while(length > 0 && isspace((unsigned char)base[length - 1]))
                    length--;
                memcpy(record_name, base, length);
                record_name[length] = '\0';
            }
        } else {
            copy_text(record_name, sizeof(record_name), left);
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, record_name, &record_owner);
        if(e->kind == ZIR_EXPR_MEMBER && !strcmp(e->name, "length") &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            error(c, e->span, "length is not Jai syntax; use count", e->name);
            break;
        }
        if(e->kind == ZIR_EXPR_MEMBER && record == NULL &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            if(!strcmp(e->name, "count")) {
                type = "s64";
                break;
            }
            char element[ZIR_NAME_MAX];
            if(!strcmp(e->name, "data") &&
               ArrayElementType(left, element, sizeof(element), NULL)) {
                snprintf(member_type, sizeof(member_type), "*%s", element);
                type = member_type;
                break;
            }
        }
        if(VecElementType(c->module, record_name, NULL, 0) &&
           strcmp(e->name, "count") && strcmp(e->name, "capacity")) {
            error(c, e->span, "Vec storage is private", e->name);
            break;
        }
        if(record != NULL && !record->is_enum) {
            if(strchr(e->name, '.') != NULL) {
                if(!RecordFieldPathType(record_owner, record, e->name,
                                        member_type, sizeof(member_type)))
                    error(c, e->span, "invalid promoted record field path",
                          e->name);
            } else {
                char path[ZIR_NAME_MAX];
                int found = ResolveRecordField(record_owner, record, e->name,
                                               path, sizeof(path), member_type,
                                               sizeof(member_type));
                if(found < 0)
                    error(c, e->span, "ambiguous using record field", e->name);
                else if(found > 0)
                    copy_text(e->name, sizeof(e->name), path);
            }
            if(*member_type) {
                normalize_array(record_owner, member_type, sizeof(member_type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             record_name, member_type,
                                             sizeof(member_type)))
                    error(c, e->span,
                          "imported record field type is shadowed",
                          e->name);
            }
        }
        if(!*member_type)
            error(c, e->span, "unknown record field", e->name);
        type = member_type;
        check_moved_path_use(c, index);
        break;
    }
    case ZIR_EXPR_SLICE: {
        char element[ZIR_NAME_MAX];
        int array = ArrayElementType(left, element, sizeof(element), NULL);
        int string = !strcmp(left, "string");
        if(!string && !array && !SliceElementType(left, element, sizeof(element))) {
            error(c, e->span, "slice source requires a string, array, or slice", e->text);
            break;
        }
        if(array && !assignable(c, e->left))
            error(c, e->span, "slice source requires persistent array storage", e->text);
        if(!string && ((!strcmp(element, "char") || !strcmp(element, "const char")) || element[0] == '['))
            error(c, e->span, "unsupported slice element type", element);
        if(e->right >= 0 && !integer_type(right))
            error(c, e->span, "slice lower bound requires an integer", e->text);
        if(e->third >= 0 && !integer_type(expression_type(c, e->third)))
            error(c, e->span, "slice upper bound requires an integer", e->text);
        if(string) type = "string";
        else {
            snprintf(member_type, sizeof(member_type), "[]%s", element);
            type = member_type;
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];

        /* Native pointer indexing follows the same element type as a
         * borrowed array. Portable bundles reject reachable raw pointers. */
        if(!strcmp(left, "string")) {
            if(!integer_type(right))
                error(c, e->span, "string index requires an integer operand", e->text);
            copy_text(element, sizeof(element), "u8");
        } else if(*left == '*' && *skip_ws(left + 1) &&
                  strcmp(skip_ws(left + 1), "void")) {
            copy_text(element, sizeof(element), skip_ws(left + 1));
            if(!integer_type(right))
                error(c, e->span, "pointer index requires an integer operand", e->text);
        } else if(!ArrayElementType(left, element, sizeof(element), NULL) &&
                  !SliceElementType(left, element, sizeof(element)) &&
                  !VecElementType(c->module, left, element, sizeof(element))) {
            error(c, e->span, "index requires an array, slice, pointer, or string", e->text);
            copy_text(element, sizeof(element), "s32");
        } else if(!integer_type(right)) {
            error(c, e->span, "array index requires an integer operand", e->text);
        }
        copy_text(member_type, sizeof(member_type), element);
        type = member_type;
        break;
    }
    case ZIR_EXPR_INT: {
        const ZirType *enumeration = FindType(c->module, e->type, NULL);
        type = enumeration != NULL && enumeration->is_enum ? e->type : "integer";
        break;
    }
    case ZIR_EXPR_FLOAT: type = "real"; break;
    case ZIR_EXPR_COMPILE_TIME: type = "bool"; break;
    case ZIR_EXPR_STRING: type = "string"; break;
    case ZIR_EXPR_IDENT:
        if(e->is_this) {
            error(c, e->span, "#this needs a procedure type or call", e->name);
            break;
        }
        if(!strcmp(e->name, "true") || !strcmp(e->name, "false")) type = "bool";
        else if(!strcmp(e->name, "null")) type = "null";
        else {
            const ZirType *enumeration = NULL;
            char source_member[ZIR_NAME_MAX];
            int enum_member = !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) == NULL ?
                resolve_using_enum(c, e->name, &enumeration, source_member,
                                   sizeof(source_member)) : 0;
            if(enum_member < 0) {
                error(c, e->span, "ambiguous using enum member", e->name);
                break;
            }
            if(enum_member == 1) {
                if(!lower_enum_reference(c, e, enumeration, source_member))
                    break;
                type = "integer";
                break;
            }
            type = lookup(c, e->name);
            e->is_global_value = !*lookup_lexical(c, e->name) &&
                                  global_binding(c, e->name) != NULL;
            if(e->is_global_value) {
                const ZirModule *owner = NULL;
                const ZirGlobal *global = NULL;
                if(ResolveGlobal(c->module, e->name,
                                 &owner, &global) == 1) {
                    int qualified = qualified_global_type(c->module,
                        e->name, owner, global->type,
                        e->type, sizeof(e->type));
                    if(qualified < 0)
                        error(c, e->span, "qualified global type is too long",
                              e->name);
                    else if(qualified > 0)
                        type = e->type;
                }
            }
            for(int i = c->count - 1; i >= 0; i--)
                if(!c->bindings[i].is_using_namespace &&
                   !strcmp(c->bindings[i].name, e->name)) {
                    c->bindings[i].touched = 1;
                    if(c->bindings[i].moved && !c->assign_destination)
                        error(c, e->span, "Vec binding is used after moving",
                              e->name);
                    break;
                }
            check_moved_path_use(c, index);
        }
        if(!*type) {
            CompoundConstant compound = {0};
            if(bound_compound_constant(c->module, e->name, 0,
                                       &compound) == 1) {
                if(!inline_compound_constant(c, index, &compound)) {
                    error(c, e->span,
                          "cannot bind aggregate constant", e->name);
                    break;
                }
                return expression_type(c, index);
            }
            /* A bare procedure name is a value when the surrounding context
             * expects a matching slot (return, assignment, argument). */
            if(c->expected_type[0] && !e->is_this &&
               !*lookup_lexical(c, e->name)) {
                const ZirType *expected_slot =
                    FindType(c->module, c->expected_type, NULL);
                const ZirModule *decl_owner = NULL;
                const ZirFunction *declaration = NULL;
                if(expected_slot != NULL && expected_slot->is_procedure_type &&
                   ResolveFunction(c->module, e->name, &decl_owner,
                                   &declaration) == 1) {
                    contextual_slot(c, index, c->expected_type);
                    if(c->failed)
                        break;
                    if(e->is_function_value) {
                        type = e->type;
                        break;
                    }
                }
            }
            char literal[ZIR_TEXT_MAX];
            int string_status = bound_string_constant(c->module, e->name, 0,
                                                       literal, sizeof(literal));
            int real_status = string_status == 0 ?
                bound_real_constant(c->module, e->name, 0,
                                    literal, sizeof(literal)) : 0;
            int64_t value = 0;
            int status = string_status == 0 && real_status == 0 ?
                bound_constant(c->module, e->name, 0, &value) :
                string_status != 0 ? string_status : real_status;
            if(string_status == 1) {
                copy_text(e->text, sizeof(e->text), literal);
                e->kind = ZIR_EXPR_STRING;
                type = "string";
            } else if(real_status == 1) {
                copy_text(e->text, sizeof(e->text), literal);
                e->kind = ZIR_EXPR_FLOAT;
                type = "real";
            } else if(status == 1) {
                snprintf(e->text, sizeof(e->text), "%lld", (long long)value);
                e->kind = ZIR_EXPR_INT;
                type = "integer";
            } else if(status < 0) {
                error(c, e->span, "invalid or ambiguous constant", e->name);
            } else {
                error(c, e->span, "unresolved name", e->name);
            }
        }
        break;
    case ZIR_EXPR_CALL: {
        char display_name[ZIR_NAME_MAX];
        copy_text(display_name, sizeof(display_name), e->name);
        if(!strcmp(e->name, "TextView")) {
            int first = e->first_child;
            if(first < 0 || c->fn->exprs[first].next_sibling >= 0 ||
               c->fn->exprs[first].argument_name[0] ||
               strcmp(expression_type(c, first), "[]u8"))
                error(c, e->span, "TextView requires one []u8 argument", e->name);
            type = "string";
            break;
        }
        if(!strcmp(e->name, "print")) {
            PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
            int first = e->first_child, placeholders = 0, arguments = 0;
            int count;
            for(int child = first; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                const char *arg_type = expression_type(c, child);
                const char *scalar = ScalarType(arg_type);
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "print has no named parameters",
                          c->fn->exprs[child].argument_name);
                if(child == first)
                    continue;
                arguments++;
                if(!strcmp(arg_type, "integer"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "s64");
                else if(!strcmp(arg_type, "real"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "float64");
                else if(scalar == NULL ||
                        (!integer_type(arg_type) &&
                         strcmp(scalar, "bool") &&
                         strcmp(scalar, "float32") &&
                         strcmp(scalar, "float64") &&
                         strcmp(scalar, "string")))
                    error(c, c->fn->exprs[child].span,
                          "print argument must be an integer, float, bool, or string",
                          arg_type);
            }
            if(pieces == NULL) {
                c->failed = 1;
                break;
            }
            if(first < 0 || c->fn->exprs[first].kind != ZIR_EXPR_STRING)
                error(c, e->span, "print requires a string literal format",
                      e->name);
            else if((count = PrintFormatPieces(c->fn->exprs[first].text,
                                               pieces, PRINT_PIECES_MAX)) < 0)
                error(c, c->fn->exprs[first].span,
                      "print format is invalid or too long", e->name);
            else {
                for(int i = 0; i < count; i++)
                    placeholders += pieces[i].is_argument;
                if(placeholders != arguments) {
                    char detail[64];
                    snprintf(detail, sizeof(detail), "%d %% for %d argument%s",
                             placeholders, arguments, arguments == 1 ? "" : "s");
                    error(c, e->span,
                          "print format placeholders do not match arguments",
                          detail);
                }
            }
            free(pieces);
            type = "void";
            break;
        }
        if(!strcmp(e->name, "VecPush") || !strcmp(e->name, "VecClear") ||
           !strcmp(e->name, "VecFree") || !strcmp(e->name, "VecSwap") ||
           !strcmp(e->name, "VecPop") || !strcmp(e->name, "VecGet") ||
           !strcmp(e->name, "VecClone") || !strcmp(e->name, "VecSlice") ||
           !strcmp(e->name, "BuilderAppend") ||
           !strcmp(e->name, "BuilderFinish")) {
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling)
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "Vec operation has no named parameters",
                          c->fn->exprs[child].argument_name);
            int first = e->first_child;
            int second = first >= 0 ? c->fn->exprs[first].next_sibling : -1;
            int push = !strcmp(e->name, "VecPush");
            int swap = !strcmp(e->name, "VecSwap");
            int pop = !strcmp(e->name, "VecPop");
            int get = !strcmp(e->name, "VecGet");
            int text_append = !strcmp(e->name, "BuilderAppend");
            int text_finish = !strcmp(e->name, "BuilderFinish");
            int byte_builder = text_append || text_finish;
            char element[ZIR_NAME_MAX];
            char option_type[ZIR_NAME_MAX];
            Binding *first_binding = lexical_vec_binding(c, first);
            int first_touched = first_binding != NULL &&
                                first_binding->touched;
            const char *vector_type = expression_type(c, first);
            int clone = !strcmp(e->name, "VecClone");
            int view = !strcmp(e->name, "VecSlice");
            if(first < 0 || !VecElementType(c->module, vector_type,
                                           element, sizeof(element)) ||
               !(get || assignable(c, first)))
                error(c, e->span, "Vec operation requires mutable Vec storage",
                      e->name);
            if(byte_builder && strcmp(element, "u8"))
                error(c, e->span,
                      "string builder requires a Vec(u8) place", element);
            if(first_binding != NULL && first_binding->borrow_count > 0 &&
               !get && !view)
                error(c, e->span,
                      "cannot mutate a Vec with a live borrowed view",
                      e->name);
            if(push) {
                if(contains_vec(c->module, element, 0))
                    error(c, e->span, "nested Vec elements are not supported", element);
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecPush requires a value", e->name);
                else {
                    const char *item_type = expression_type(c, second);
                    if(!compatible_checked(c, element, item_type))
                        error(c, e->span, "VecPush element type mismatch", element);
                    else if(!strcmp(item_type, "integer") ||
                            !strcmp(item_type, "real")) {
                        const char *scalar = ScalarType(element);
                        if(*scalar)
                            copy_text(c->fn->exprs[second].type,
                                      ZIR_NAME_MAX, scalar);
                    }
                }
            } else if(swap) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0 ||
                   !assignable(c, second) ||
                   strcmp(vector_type, expression_type(c, second)))
                    error(c, e->span, "VecSwap requires two matching Vec places", e->name);
            } else if(pop || text_finish) {
                if(second >= 0)
                    error(c, e->span, "Vec operation takes one argument", e->name);
            } else if(get) {
                const char *index_type;
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecGet requires an index", e->name);
                else {
                    index_type = expression_type(c, second);
                    if(!integer_type(index_type))
                        error(c, e->span, "VecGet requires an integer index",
                              index_type);
                }
            } else if(text_append) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "BuilderAppend requires text", e->name);
                else if(strcmp(expression_type(c, second), "string"))
                    error(c, e->span, "BuilderAppend requires string text",
                          expression_type(c, second));
            } else if(clone) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                if(second < 0 || third >= 0)
                    error(c, e->span, "VecClone requires a source Vec", e->name);
                else {
                    const char *source_type = expression_type(c, second);
                    if(!VecElementType(c->module, source_type, NULL, 0) ||
                       strcmp(vector_type, source_type))
                        error(c, e->span,
                              "VecClone requires a matching Vec source",
                              e->name);
                    else {
                        Binding *destination = lexical_vec_binding(c, first);
                        if(c->fn->exprs[first].kind != ZIR_EXPR_IDENT)
                            error(c, e->span,
                                  "VecClone requires a simple binding destination",
                                  e->name);
                        else if(destination == NULL)
                            error(c, e->span,
                                  "global Vec storage cannot clone; use a local",
                                  e->name);
                        else if(destination->borrow_count > 0)
                            error(c, e->span,
                                  "cannot mutate a Vec with a live borrowed view",
                                  e->name);
                        else if(first_touched && !destination->moved)
                            error(c, e->span,
                                  "clone destination must be fresh or moved-from",
                                  destination->name);
                    }
                }
            } else if(view) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                Binding *source = lexical_vec_binding(c, first);
                if(source == NULL)
                    error(c, e->span,
                          "VecSlice requires a local Vec binding", e->name);
                if(second < 0 || third < 0 ||
                   c->fn->exprs[third].next_sibling >= 0)
                    error(c, e->span, "VecSlice requires low and high bounds",
                          e->name);
                else {
                    if(!integer_type(expression_type(c, second)) ||
                       !integer_type(expression_type(c, third)))
                        error(c, e->span,
                              "VecSlice requires integer bounds", e->name);
                }
            } else if(second >= 0)
                error(c, e->span, "Vec operation takes one argument", e->name);
            if(pop) {
                if(vec_option_result_type(c, element, e->span, option_type,
                                           sizeof(option_type)))
                    type = option_type;
                else
                    type = "";
            } else if(get) {
                if(vec_option_result_type(c, element, e->span, option_type,
                                           sizeof(option_type)))
                    type = option_type;
                else
                    type = "";
            } else if(view) {
                if(snprintf(c->vec_slice_type, sizeof(c->vec_slice_type),
                            "[]%s", element) < (int)sizeof(c->vec_slice_type))
                    type = c->vec_slice_type;
                else
                    type = "";
            } else if(push || text_append || clone)
                type = "bool";
            else if(text_finish)
                type = "string";
            else
                type = "void";
            break;
        }
        const char *binding = e->is_this ? "" : lookup(c, e->name);
        e->is_global_value = !e->is_this && !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) != NULL;
        const ZirType *slot = FindType(c->module, binding, NULL);
        if(slot != NULL && !slot->is_procedure_type)
            slot = NULL;
        copy_text(e->slot_type, sizeof(e->slot_type), slot ? binding : "");
        const ZirFunction *callee = *binding ? NULL : function(c, e->name, e->span);
        const ZirModule *callee_owner = NULL;
        if(callee != NULL) {
            const ZirFunction *resolved = NULL;
            if(ResolveFunctionAt(c->module, e->name, SpanPath(e->span),
                                 &callee_owner, &resolved) != 1 ||
               resolved != callee) callee_owner = NULL;
        }
        char specialized_args[ZIR_TEXT_MAX] = "";
        char specialized_return[ZIR_NAME_MAX] = "";
        if(callee != NULL && callee->is_template) {
            const ZirModule *owner = NULL;
            const ZirFunction *resolved = NULL;
            if(ResolveFunction(c->module, e->name, &owner, &resolved) != 1 ||
               resolved != callee || owner == NULL) {
                error(c, e->span, "cannot resolve polymorphic procedure", e->name);
                break;
            }
            char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
            if(parameters == NULL) { c->failed = 1; break; }
            int parameter_count = *skip_ws(callee->args) ?
                split_top_level(callee->args, parameters[0], 64,
                                sizeof(parameters[0])) : 0;
            if(!bind_call_arguments(c, e, parameters, parameter_count,
                                    display_name, callee->default_args)) {
                free(parameters);
                break;
            }
            char concrete[ZIR_NAME_MAX] = "";
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                int argument = c->fn->exprs[child].argument_index;
                const char *colon = strchr(parameters[argument], ':');
                if(colon == NULL) continue;
                const char *parameter_type = skip_ws(colon + 1);
                if(*parameter_type != '$' ||
                   strcmp(parameter_type + 1, callee->template_param))
                    continue;
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                if(concrete[0])
                    copy_text(c->expected_type, sizeof(c->expected_type), concrete);
                const char *actual_type = expression_type(c, child);
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(!strcmp(actual_type, "integer") || !strcmp(actual_type, "real")) {
                    if(concrete[0]) actual_type = concrete;
                    else if(!strcmp(callee->return_type, callee->template_param) &&
                            ScalarType(saved_expected)[0])
                        actual_type = saved_expected;
                    else {
                        error(c, c->fn->exprs[child].span,
                              "polymorphic literal needs a concrete type", e->name);
                        continue;
                    }
                }
                if(!*actual_type || !strcmp(actual_type, "null")) {
                    error(c, c->fn->exprs[child].span,
                          "cannot infer polymorphic type", e->name);
                    continue;
                }
                if(!concrete[0])
                    copy_text(concrete, sizeof(concrete), actual_type);
                else if(strcmp(concrete, actual_type))
                    signature_error(c, c->fn->exprs[child].span,
                                    "polymorphic type mismatch", e->name);
            }
            free(parameters);
            if(!concrete[0]) {
                error(c, e->span, "cannot infer polymorphic type", e->name);
                break;
            }
            if(!replace_template_type(specialized_args, sizeof(specialized_args),
                                      callee->args, callee->template_param, concrete) ||
               !replace_template_type(specialized_return,
                                      sizeof(specialized_return),
                                      callee->return_type,
                                      callee->template_param, concrete)) {
                error(c, e->span, "specialized signature is too long", e->name);
                break;
            }
            char name[ZIR_NAME_MAX];
            const ZirModule *instance_owner = owner;
            if(owner != c->module &&
               FindType(owner, concrete, NULL) == NULL &&
               FindType(c->module, concrete, NULL) != NULL)
                instance_owner = c->module;
            if(!queue_specialization(c, owner, instance_owner,
                                     callee, concrete,
                                     name, sizeof(name), e->span)) break;
            const char *dot = instance_owner == owner ?
                strchr(e->name, '.') : NULL;
            if(dot) {
                char qualified[ZIR_NAME_MAX];
                int written = snprintf(qualified, sizeof(qualified), "%.*s.%s",
                                       (int)(dot - e->name), e->name, name);
                if(written < 0 || (size_t)written >= sizeof(qualified)) {
                    error(c, e->span, "specialized name is too long", e->name);
                    break;
                }
                copy_text(e->name, sizeof(e->name), qualified);
            } else copy_text(e->name, sizeof(e->name), name);
        }
        if(callee != NULL && callee->is_extern && callee->extern_kind == ZIR_EXTERN_HOST)
            c->fn->uses_host = 1;
        const char *args = slot ? slot->body :
                           specialized_args[0] ? specialized_args :
                           callee ? callee->args : NULL;
        const char *return_type = slot ? slot->procedure_return_type :
                                  specialized_return[0] ? specialized_return :
                                  callee ? callee->return_type : "";
        char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        int varargs = 0;
        if(!parts) { c->errors++; c->failed=1; break; }
        if(*binding && slot == NULL)
            error(c, e->span, "binding is not a callable function", e->name);
        if(!callee && slot == NULL) for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *imp = &c->module->imports[i];
            if(imp->kind == ZIR_IMPORT_EXTERN &&
               in_lookup_file(c->module, imp->is_file_private, imp->span) &&
               !strcmp(imp->name, e->name)) {
                if(imp->extern_kind == ZIR_EXTERN_HOST)
                    c->fn->uses_host = 1;
                args = imp->args; return_type = imp->return_type;
                varargs = imp->is_varargs;
                break;
            }
        }
        /* Calling a procedure-typed record field (value.is_active(app)) or a
         * field behind a record pointer (practice.draw(app)): resolve the
         * field's slot type and type the call from its signature. Anonymous
         * calls name their callee in e->left. */
        if(!callee && slot == NULL) {
            if(e->name[0] == '\0' && e->left >= 0) {
                const ZirType *indirect = FindType(c->module, left, NULL);
                if(indirect != NULL && indirect->is_procedure_type) {
                    copy_text(e->slot_type, sizeof(e->slot_type), left);
                    slot = indirect;
                    args = indirect->body;
                    return_type = indirect->procedure_return_type;
                }
            } else if(strchr(e->name, '.') != NULL) {
                char base[ZIR_NAME_MAX];
                copy_text(base, sizeof(base), e->name);
                char *field = strrchr(base, '.');
                *field++ = '\0';
                const char *base_type = lookup(c, base);
                const char *record_name = base_type;
                if(*record_name == '*')
                    record_name = skip_ws(record_name + 1);
                const ZirModule *field_owner = NULL;
                const ZirType *record = FindType(c->module, record_name, &field_owner);
                if(record != NULL && !record->is_enum) {
                    char member_type[ZIR_NAME_MAX] = "";
                    char field_path[ZIR_NAME_MAX];
                    int found = strchr(field, '.') != NULL ?
                        RecordFieldPathType(field_owner, record, field,
                                            member_type, sizeof(member_type)) :
                        ResolveRecordField(field_owner, record, field,
                                           field_path, sizeof(field_path),
                                           member_type, sizeof(member_type));
                    const ZirType *field_slot = *member_type ?
                        FindType(c->module, member_type, NULL) : NULL;
                    if(found > 0 && field_slot != NULL &&
                       field_slot->is_procedure_type) {
                        copy_text(e->slot_type, sizeof(e->slot_type), member_type);
                        slot = field_slot;
                        args = field_slot->body;
                        return_type = field_slot->procedure_return_type;
                    }
                }
            }
        }
        expected = args && *skip_ws(args) ? split_top_level(args, parts[0], 64, sizeof(parts[0])) : 0;
        if(callee_owner != NULL && callee_owner != c->module)
            for(int parameter = 0; parameter < expected; parameter++) {
                char *colon = strchr(parts[parameter], ':');
                if(colon == NULL) continue;
                char mapped[ZIR_NAME_MAX];
                copy_text(mapped, sizeof(mapped), skip_ws(colon + 1));
                trim_in_place(mapped);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, mapped,
                                             sizeof(mapped))) {
                    error(c, e->span,
                          "imported procedure parameter type is shadowed",
                          e->name);
                    continue;
                }
                int written = snprintf(colon + 1,
                    sizeof(parts[parameter]) - (size_t)(colon + 1 - parts[parameter]),
                    " %s", mapped);
                if(written < 0 || (size_t)written >=
                   sizeof(parts[parameter]) -
                       (size_t)(colon + 1 - parts[parameter]))
                    error(c, e->span, "imported parameter type is too long",
                          e->name);
            }
        int fixed = varargs && expected > 0 ? expected - 1 : expected;
        if(varargs && expected > 0) {
            /* bind_call_arguments assigns slots from the fixed parameters;
             * variadic extras take the next sequential slots. */
            if(!bind_varargs_call(c, e, parts, fixed, display_name)) {
                free(parts);
                break;
            }
        } else if(args && !bind_call_arguments(c, e, parts, expected, display_name,
                                       callee ? callee->default_args : NULL)) {
            free(parts);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            int parameter = c->fn->exprs[child].argument_index;
            const char *expected_type = parameter >= 0 && parameter < expected &&
                parameter < fixed ?
                strchr(parts[parameter], ':') : NULL;
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(expected_type != NULL)
                contextual_slot(c, child, skip_ws(expected_type + 1));
            if(expected_type != NULL)
                copy_text(c->expected_type, sizeof(c->expected_type),
                          skip_ws(expected_type + 1));
            const char *arg_type = expression_type(c, child);
            if(contains_vec(c->module, arg_type, 0) &&
               c->fn->exprs[child].kind != ZIR_EXPR_IDENT &&
               c->fn->exprs[child].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, child))
                error(c, c->fn->exprs[child].span,
                      "Vec arguments move a binding or pass a call result",
                      display_name);
            if(contains_vec(c->module, arg_type, 0) &&
               global_vec_source(c, child))
                error(c, c->fn->exprs[child].span,
                      "global Vec storage cannot move; use a local",
                      c->fn->exprs[child].name);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(args && parameter >= 0 && parameter < fixed) {
                char *colon = strchr(parts[parameter], ':');
                if(colon && !compatible_checked(c, skip_ws(colon + 1), arg_type)) {
                    const char *converted = try_conversion(c, child,
                                                           skip_ws(colon + 1),
                                                           c->fn->exprs[child].span);
                    e = &c->fn->exprs[index];
                    if(converted != NULL)
                        arg_type = converted;
                    else
                        signature_error(c, c->fn->exprs[child].span,
                                        "argument type mismatch", display_name);
                }
                if(colon && (!strcmp(arg_type, "integer") || !strcmp(arg_type, "real"))) {
                    const char *context = ScalarType(skip_ws(colon + 1));
                    if(*context) copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, context);
                }
            }
            actual++;
        }
        if(args) {
            char saved_checked_type[ZIR_NAME_MAX];
            copy_text(saved_checked_type, sizeof(saved_checked_type), e->type);
            type = return_type;
            if(specialized_return[0]) {
                copy_text(e->type, sizeof(e->type), specialized_return);
                type = e->type;
            }
            if(callee_owner != NULL && callee_owner != c->module) {
                if(type != e->type)
                    copy_text(e->type, sizeof(e->type), type);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, e->type,
                                             sizeof(e->type)))
                    error(c, e->span,
                          "imported procedure result type is shadowed",
                          e->name);
                if(saved_checked_type[0] &&
                   same_declared_type(c->module, e->type,
                                      saved_checked_type, 0))
                    copy_text(e->type, sizeof(e->type), saved_checked_type);
                type = e->type;
            }
            if(actual < fixed && !c->inference_only)
                signature_error(c, e->span, "argument count mismatch", display_name);
        } else
            error(c, e->span, "unresolved function", e->name);
        free(parts);
        break;
    }
    case ZIR_EXPR_BINARY: {
        if(left[0] == '[' || right[0] == '[')
            error(c, e->span, "array values do not support binary operations", e->op);
        const ZirType *left_slot = FindType(c->module, left, NULL);
        const ZirType *right_slot = FindType(c->module, right, NULL);
        /* Procedure slots compare against null (and identical slot types)
         * the same way pointers do; every other operation stays rejected. */
        int slot_null_compare =
            (!strcmp(e->op, "==") || !strcmp(e->op, "!=")) &&
            ((left_slot != NULL && left_slot->is_procedure_type &&
              (!strcmp(right, "null") ||
               (right_slot != NULL && right_slot->is_procedure_type &&
                !strcmp(left, right)))) ||
             (right_slot != NULL && right_slot->is_procedure_type &&
              !strcmp(left, "null")));
        if(((left_slot && left_slot->is_procedure_type) ||
            (right_slot && right_slot->is_procedure_type)) && !slot_null_compare)
            error(c, e->span, "slot values do not support binary operations", e->op);
        if((text_type(left) || text_type(right)) &&
           strcmp(e->op, "==") && strcmp(e->op, "!="))
            error(c, e->span, "string operation is not supported", e->op);
        if((!strcmp(e->op, "&") || !strcmp(e->op, "|") || !strcmp(e->op, "^") ||
            !strcmp(e->op, "<<") || !strcmp(e->op, ">>") || !strcmp(e->op, "%")) &&
           (left[0] == 'f' || right[0] == 'f' || !strcmp(left, "real") || !strcmp(right, "real")))
            error(c, e->span, "integer operands required", e->op);
        if((!strcmp(e->op, "&&") || !strcmp(e->op, "||")) &&
           (strcmp(left, "bool") || strcmp(right, "bool")))
            error(c, e->span, "logical operands require bool", e->op);
        const ZirType *left_flags = flags_type(c, left);
        const ZirType *right_flags = flags_type(c, right);
        if(left_flags != NULL || right_flags != NULL) {
            const char *flag_name = left_flags != NULL ? left : right;
            const char *other = left_flags != NULL ? right : left;
            if((left_flags != NULL && right_flags != NULL && strcmp(left, right)) ||
               (strcmp(other, flag_name) && !integer_type(other)))
                error(c, e->span, "flag operands require the same enum or an integer", e->op);
            if(!strcmp(e->op, "==") || !strcmp(e->op, "!="))
                type = "bool";
            else if(!strcmp(e->op, "&") || !strcmp(e->op, "|") ||
                    !strcmp(e->op, "^") || !strcmp(e->op, "+") ||
                    !strcmp(e->op, "-"))
                type = flag_name;
            else
                error(c, e->span, "unsupported enum_flags operation", e->op);
            break;
        }
        if(!compatible_checked(c, left, right) &&
           !compatible_checked(c, right, left) && !slot_null_compare)
            error(c, e->span, "operand types differ; use an explicit cast", e->op);
        if(!strcmp(e->op, "==") || !strcmp(e->op, "!=") || !strcmp(e->op, "<") ||
           !strcmp(e->op, "<=") || !strcmp(e->op, ">") || !strcmp(e->op, ">=") ||
           !strcmp(e->op, "&&") || !strcmp(e->op, "||")) type = "bool";
        else if(numeric(left) && numeric(right))
            type = (!strcmp(left, "integer") || !strcmp(left, "real")) ? right : left;
        else if(*left && *right) error(c, e->span, "numeric operands required", e->op);
        break;
    }
    case ZIR_EXPR_UNARY:
        if(strcmp(e->op, "+") && strcmp(e->op, "-") &&
           strcmp(e->op, "!") && strcmp(e->op, "~") &&
           strcmp(e->op, "&") && strcmp(e->op, "*"))
            error(c, e->span, "unsupported unary operator", e->op);
        if(!strcmp(e->op, "!") && strcmp(right, "bool"))
            error(c, e->span, "logical operand requires bool", e->op);
        if(!strcmp(e->op, "~") && !integer_type(right))
            error(c, e->span, "bitwise operand requires an integer", e->op);
        if(!strcmp(e->op, "&")) {
            if(!assignable(c, e->right))
                error(c, e->span, "address-of requires an assignable expression", e->text);
            if(!*right || !strcmp(right, "null") || strlen(right) + 1 >= sizeof(e->type))
                error(c, e->span, "address-of requires a known type", e->text);
            snprintf(e->type, sizeof(e->type), "*%s", right);
            type = e->type;
        } else if(!strcmp(e->op, "*")) {
            if(right[0] != '*' || !*skip_ws(right + 1))
                error(c, e->span, "dereference requires a pointer", right);
            type = skip_ws(right + 1);
        } else if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else
            error(c, e->span, "unresolved unary operation", e->op);
        break;
    case ZIR_EXPR_CAST: {
        if(!JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        if(right[0] == '[' || e->name[0] == '[')
            error(c, e->span, "array casts are not supported", e->name);
        const ZirType *destination = FindType(c->module, e->name, NULL);
        const ZirType *source = FindType(c->module, right, NULL);
        if(destination != NULL && destination->is_enum &&
           !numeric(right) && strcmp(right, "bool") &&
           (source == NULL || !source->is_enum))
            error(c, e->span, "enum casts require a numeric, bool, or enum value", e->name);
        if((text_type(right) || text_type(e->name)) && strcmp(right, e->name))
            error(c, e->span, "string casts require an explicit conversion API", e->name);
        type = e->name;
        break;
    }
    case ZIR_EXPR_CONDITIONAL: {
        const char *third = expression_type(c, e->third);
        if(strcmp(left, "bool")) error(c, e->span, "conditional requires bool", left);
        if(!compatible_checked(c, right, third) &&
           !compatible_checked(c, third, right))
            error(c, e->span, "conditional arms have different types", "");
        type = !strcmp(right, "integer") || !strcmp(right, "null") ?
               third : right;
        break;
    }
    default: error(c, e->span, "expression is not supported by language checking", e->text); break;
    }
    if(*ScalarType(type)) type = ScalarType(type);
    if(type != e->type)
        copy_text(e->type, sizeof(e->type), type);
    normalize_array(c->module, e->type, sizeof(e->type));
    return e->type;
}

int
InferExpressionType(const ZirModule *module, const char *expression,
                    ZirSourceSpan span, char *type, size_t capacity)
{
    ZirModule lookup = *module;
    ZirFunction probe = {0};
    Checker checker = {0};
    int root, valid;
    const char *inferred;

    copy_text(lookup.lookup_path, sizeof(lookup.lookup_path), SpanPath(span));
    probe.span = span;
    root = ParseExprNoDefaults(&probe, &lookup, expression, span);
    checker.module = &lookup;
    checker.fn = &probe;
    checker.inference_only = 1;
    inferred = root >= 0 ? expression_type(&checker, root) : "";
    if(strcmp(inferred, "integer") == 0) inferred = "s64";
    else if(strcmp(inferred, "real") == 0) inferred = "float64";
    valid = checker.errors == 0 && *inferred != '\0' &&
            strlen(inferred) < capacity;
    if(valid) copy_text(type, capacity, inferred);
    free(checker.bindings);
    free(checker.specializations);
    free(probe.exprs);
    return valid;
}
