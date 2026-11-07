#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_parse_internal.h"

static void
expand_compile_expr_depth(char *dst, size_t dst_size, const ZirConsts *consts,
                          const char *src, const char *lookup_path, int depth,
                          const ZirConst **active)
{
    size_t n = 0;
    int in_string = 0;
    int escaped = 0;
    int i;

    if(dst_size == 0)
        return;
    if(depth > 16)
        die_at(Span(lookup_path, 1, 1),
               "compile-time constant expansion exceeds 16 levels");
    for(const char *p = src; p != NULL && *p != '\0' && n + 1 < dst_size;) {
        if(in_string) {
            dst[n++] = *p;
            if(escaped)
                escaped = 0;
            else if(*p == '\\')
                escaped = 1;
            else if(*p == '"')
                in_string = 0;
            p++;
            continue;
        }
        if(*p == '"') {
            in_string = 1;
            dst[n++] = *p++;
            continue;
        }
        /* Expanded text spells the probe defined(NAME), which expands to
         * itself again. */
        size_t directive = *p == '#';
        if(strncmp(p + directive, "defined", 7) == 0 &&
           (directive || p == src || !is_ident_char((unsigned char)p[-1]))) {
            const char *word_end = p + directive + 7;

            if(*word_end == '\0' || *word_end == '(' ||
               isspace((unsigned char)*word_end)) {
                if(n + 7 >= dst_size)
                    break;
                memcpy(dst + n, "defined", 7);
                n += 7;
                p = word_end;
                /* Copy the probe name verbatim: constants must not expand
                 * inside defined(...). */
                {
                    const char *gap = p;
                    while(*gap == ' ' || *gap == '\t')
                        gap++;
                    if(*gap == '(') {
                        int probe_depth = 0;
                        while(*gap != '\0' && n + 1 < dst_size) {
                            if(*gap == '(')
                                probe_depth++;
                            else if(*gap == ')') {
                                probe_depth--;
                                if(probe_depth == 0) {
                                    dst[n++] = *gap++;
                                    break;
                                }
                            }
                            dst[n++] = *gap++;
                        }
                        p = gap;
                    }
                }
                continue;
            }
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            char ident[ZIR_NAME_MAX];
            size_t il = 0;
            int found = 0;

            size_t previous = n;
            while(previous > 0 &&
                  isspace((unsigned char)dst[previous - 1])) previous--;
            int member = previous > 0 && dst[previous - 1] == '.';

            while(isalnum((unsigned char)*p) || *p == '_') {
                if(il + 1 < sizeof(ident))
                    ident[il++] = *p;
                p++;
            }
            ident[il] = '\0';
            for(i = 0; !member && i < consts->count; i++) {
                if(strcmp(consts->items[i].name, ident) == 0 &&
                   (!consts->items[i].is_file_private ||
                    strcmp(consts->items[i].path, lookup_path) == 0)) {
                    char expanded[ZIR_TEXT_MAX];
                    int written;

                    for(int ancestor = 0; ancestor < depth; ancestor++)
                        if(active[ancestor] == &consts->items[i])
                            die_at(Span(consts->items[i].path,
                                        consts->items[i].source_line, 1),
                                   "cyclic compile-time constant: %s",
                                   ident);
                    active[depth] = &consts->items[i];

                    expand_compile_expr_depth(expanded, sizeof(expanded),
                                              consts, consts->items[i].expr,
                                              consts->items[i].path,
                                              depth + 1, active);
                    if(ZirLawEvaluation &&
                       (ScalarWidth(consts->items[i].type) ||
                        !strcmp(consts->items[i].type, "bool")))
                        written = snprintf(dst + n, dst_size - n,
                            "(cast(%s) (%s))", consts->items[i].type, expanded);
                    else
                        written = snprintf(dst + n, dst_size - n, "(%s)", expanded);
                    if(written < 0)
                        written = 0;
                    if((size_t)written >= dst_size - n)
                        n = dst_size - 1;
                    else
                        n += (size_t)written;
                    found = 1;
                    break;
                }
            }
            if(!found) {
                if(n + il >= dst_size)
                    break;
                memcpy(dst + n, ident, il);
                n += il;
            }
            continue;
        }
        dst[n++] = *p++;
    }
    dst[n] = '\0';
}

void
expand_compile_expr(char *dst, size_t dst_size, const ZirConsts *consts,
                    const char *src, const char *lookup_path)
{
    const ZirConst *active[17] = {0};
    expand_compile_expr_depth(dst, dst_size, consts, src, lookup_path, 0,
                              active);
}

int
eval_integer_type(const char *type, int64_t value)
{
    if(!strcmp(type, "s8"))
        return value >= INT8_MIN && value <= INT8_MAX;
    if(!strcmp(type, "s16"))
        return value >= INT16_MIN && value <= INT16_MAX;
    if(!strcmp(type, "s32"))
        return value >= INT32_MIN && value <= INT32_MAX;
    if(!strcmp(type, "u8")) return value >= 0 && value <= UINT8_MAX;
    if(!strcmp(type, "u16")) return value >= 0 && value <= UINT16_MAX;
    if(!strcmp(type, "u32")) return value >= 0 && value <= UINT32_MAX;
    if(!strcmp(type, "s64")) return 1;
    if(!strcmp(type, "bool")) return value == 0 || value == 1;
    return 0;
}

int
eval_close(const ZirFunction *fn, int opening, int stop)
{
    int depth = 1;
    for(int i = opening + 1; i < stop; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
           kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return -1;
}

static const ZirDefine *
find_imported_define(const ZirModule *module, const char *lookup_path,
                     const char *name, size_t length,
                     const ZirModule **owner_out)
{
    const char *dot = memchr(name, '.', length);
    const ZirImport *import = NULL;
    const ZirDefine *definition = NULL;
    if(module == NULL || dot == NULL ||
       memchr(dot + 1, '.', length - (size_t)(dot + 1 - name)) != NULL)
        return NULL;
    size_t alias_length = (size_t)(dot - name);
    size_t member_length = length - alias_length - 1;
    if(alias_length == 0 || alias_length >= ZIR_NAME_MAX ||
       member_length == 0 || member_length >= ZIR_NAME_MAX)
        return NULL;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *candidate = &module->imports[i];
        if(candidate->kind != ZIR_IMPORT_MODULE ||
           (candidate->is_file_private &&
            strcmp(SpanPath(candidate->span), lookup_path) != 0) ||
           strlen(candidate->name) != alias_length ||
           strncmp(candidate->name, name, alias_length) != 0)
            continue;
        if(import != NULL && import != candidate)
            return NULL;
        import = candidate;
    }
    if(import == NULL || import->resolved_module == NULL)
        return NULL;
    const ZirModule *owner = import->resolved_module;
    for(int i = 0; i < owner->define_count; i++) {
        const ZirDefine *candidate = &owner->defines[i];
        if(!candidate->is_public || candidate->is_file_private ||
           strlen(candidate->name) != member_length ||
           strncmp(candidate->name, dot + 1, member_length) != 0)
            continue;
        if(definition != NULL)
            return NULL;
        definition = candidate;
    }
    if(definition != NULL) *owner_out = owner;
    return definition;
}

int
compile_value_literal(CompileValue *value)
{
    if(value->kind == COMPILE_INTEGER) {
        /* Every evaluated integer gets its text; format it directly, since
         * snprintf dominated law evaluation. */
        int negative = strcmp(value->type, "u64") && value->integer < 0;
        uint64_t magnitude = negative ? UINT64_C(0) - (uint64_t)value->integer :
                                        (uint64_t)value->integer;
        char digits[24];
        int count = 0;
        do {
            digits[count++] = (char)('0' + magnitude % 10);
            magnitude /= 10;
        } while(magnitude != 0);
        char *out = value->literal;
        if(negative) *out++ = '-';
        while(count > 0) *out++ = digits[--count];
        *out = '\0';
        return 1;
    }
    if(value->kind == COMPILE_REAL) {
        int written = snprintf(value->literal, sizeof(value->literal),
                               "%.17g", value->real);
        if(written < 0 || (size_t)written >= sizeof(value->literal)) return 0;
        DecimalPointDot(value->literal);
        if(strpbrk(value->literal, ".eE") == NULL) {
            if((size_t)written + 2 >= sizeof(value->literal)) return 0;
            strcat(value->literal, ".0");
        }
    }
    return value->kind != COMPILE_INVALID;
}

int
compile_truth(const CompileValue *value, int *truth)
{
    if(value->kind == COMPILE_INTEGER) *truth = value->integer != 0;
    else if(value->kind == COMPILE_REAL) *truth = value->real != 0.0;
    else return 0;
    return 1;
}

int
compile_type_value(const char *type, CompileValue *value)
{
    if(!strcmp(type, "u64")) {
        if(value->kind != COMPILE_INTEGER ||
           (value->integer < 0 && strcmp(value->type, "u64"))) return 0;
        copy_text(value->type, sizeof(value->type), type);
        return compile_value_literal(value);
    }
    if(eval_integer_type(type, 0)) {
        if(value->kind != COMPILE_INTEGER ||
           !eval_integer_type(type, value->integer)) return 0;
        copy_text(value->type, sizeof(value->type), type);
        return compile_value_literal(value);
    }
    if(!strcmp(type, "float32") || !strcmp(type, "float64")) {
        if(value->kind == COMPILE_INTEGER) {
            value->real = (double)value->integer;
            value->kind = COMPILE_REAL;
        }
        if(value->kind != COMPILE_REAL) return 0;
        if(!strcmp(type, "float32"))
            value->real = (float)value->real;
        copy_text(value->type, sizeof(value->type), type);
        return isfinite(value->real) && compile_value_literal(value);
    }
    if(!strcmp(type, "string")) return value->kind == COMPILE_STRING;
    if(!strcmp(type, "bool")) {
        if(value->kind != COMPILE_INTEGER) return 0;
        copy_text(value->type, sizeof(value->type), "bool");
        return 1;
    }
    /* Enum and other named integer types are not checked here, as before. */
    if(value->kind == COMPILE_INTEGER) return 1;
    if(value->kind == COMPILE_COMPOUND &&
       !strcmp(value->type, type)) return 1;
    return 0;
}

int
wrap_compile_integer(const char *type, int64_t *value)
{
    if(!ScalarWidth(type)) return 0;
    *value = ScalarStored((uint64_t)*value, type);
    return 1;
}
/* Laws decide a comparison between an integer and a real only when the
 * integer converts to a double exactly, so the answer matches every target. */
int
compile_law_exact_double(const CompileValue *value)
{
    if(value->kind == COMPILE_REAL) return 1;
    if(value->kind != COMPILE_INTEGER) return 0;
    if(!strcmp(value->type, "u64"))
        return (uint64_t)value->integer <= (UINT64_C(1) << 53);
    return value->integer >= -(INT64_C(1) << 53) &&
           value->integer <= (INT64_C(1) << 53);
}

/* Buffers compile_values_equal keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CompileValuesEqualBuffers {
    unsigned char a[ZIR_TEXT_MAX];
    unsigned char b[ZIR_TEXT_MAX];
} CompileValuesEqualBuffers;

int compile_values_equal(const CompileValue *left, const CompileValue *right,
                     int *equal);

static int
compile_values_equal_with_buffers(const CompileValue *left, const CompileValue *right,
                     int *equal, CompileValuesEqualBuffers *buffers)
{
    if(left->kind == COMPILE_STRING && right->kind == COMPILE_STRING) {
        size_t an, bn;
        if(!DecodeStringLiteral(left->literal, buffers->a, sizeof(buffers->a), &an) ||
           !DecodeStringLiteral(right->literal, buffers->b, sizeof(buffers->b), &bn)) return 0;
        *equal = an == bn && memcmp(buffers->a, buffers->b, an) == 0;
        return 1;
    }
    if(left->kind == COMPILE_INTEGER && right->kind == COMPILE_INTEGER) {
        *equal = left->integer == right->integer;
        return 1;
    }
    /* Compound literals are built in canonical text, so equal values of one
     * type have equal text. */
    if(left->kind == COMPILE_COMPOUND && right->kind == COMPILE_COMPOUND &&
       !strcmp(left->type, right->type)) {
        *equal = !strcmp(left->literal, right->literal);
        return 1;
    }
    if((left->kind == COMPILE_REAL || left->kind == COMPILE_INTEGER) &&
       (right->kind == COMPILE_REAL || right->kind == COMPILE_INTEGER)) {
        if(ZirLawEvaluation && (!compile_law_exact_double(left) ||
                                !compile_law_exact_double(right))) return 0;
        double a = left->kind == COMPILE_REAL ? left->real :
                   (double)left->integer;
        double b = right->kind == COMPILE_REAL ? right->real :
                   (double)right->integer;
        *equal = a == b;
        return 1;
    }
    return 0;
}

COMPILER_SCRATCH_RETURN(, int, compile_values_equal,
    (const CompileValue *left, const CompileValue *right,
                     int *equal),
    CompileValuesEqualBuffers, RECOVERABLE, free,
    compile_values_equal_with_buffers(left, right, equal, buffers))

int
compile_compound_value(const ZirFunction *probe, const ZirExpr *expression,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result)
{
    char element[ZIR_NAME_MAX] = "";
    int capacity = 0;
    int array = ArrayElementType(expression->name, element,
                                 sizeof(element), &capacity);
    const ZirType *record = array ? NULL :
        FindType(module, expression->name, NULL);
    if((array && capacity < 0) ||
       (!array && (record == NULL || record->is_enum ||
                   record->is_procedure_type || record->is_record_template)))
        return 0;
    int written = array ?
        snprintf(result->literal, sizeof(result->literal), "%s.[", element) :
        snprintf(result->literal, sizeof(result->literal), "%s.{",
                 expression->name);
    if(written < 0 || (size_t)written >= sizeof(result->literal)) return 0;
    size_t used = (size_t)written;
    int ordinal = 0, mode = -1;
    for(int child = expression->first_child; child >= 0;
        child = probe->exprs[child].next_sibling) {
        const ZirExpr *entry = &probe->exprs[child];
        CompileValue value;
        compile_value_clear(&value);
        char field_type[ZIR_NAME_MAX];
        char field_name[ZIR_NAME_MAX] = "";
        int named = !strcmp(entry->op, "=");
        if(array) {
            if(named || ordinal >= capacity) return 0;
            copy_text(field_type, sizeof(field_type), element);
        } else {
            ZirTypeField field;
            size_t offset = 0;
            int position = 0, found = 0;
            if(mode >= 0 && mode != named) return 0;
            mode = named;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) :
                           position == ordinal) {
                    copy_text(field_type, sizeof(field_type), field.type);
                    copy_text(field_name, sizeof(field_name), field.name);
                    found = 1;
                    break;
                }
                position++;
            }
            if(!found) return 0;
            for(int previous = expression->first_child; previous != child;
                previous = probe->exprs[previous].next_sibling)
                if(named && !strcmp(probe->exprs[previous].name,
                                    field_name)) return 0;
        }
        if(!evaluate_typed_node(probe, entry->right, module, path,
                                depth + 1, fuel, &value) ||
           !compile_type_value(field_type, &value)) return 0;
        if(array)
            written = snprintf(result->literal + used,
                               sizeof(result->literal) - used, "%s%s",
                               ordinal ? ", " : "", value.literal);
        else
            written = snprintf(result->literal + used,
                               sizeof(result->literal) - used,
                               "%s.%s = %s", ordinal ? ", " : "",
                               field_name, value.literal);
        if(written < 0 || (size_t)written >= sizeof(result->literal) - used)
            return 0;
        used += (size_t)written;
        ordinal++;
    }
    if(array && ordinal != capacity) return 0;
    written = snprintf(result->literal + used, sizeof(result->literal) - used,
                       "%c", array ? ']' : '}');
    if(written != 1 || (size_t)written >= sizeof(result->literal) - used)
        return 0;
    result->kind = COMPILE_COMPOUND;
    copy_text(result->type, sizeof(result->type), expression->name);
    result->type_owner = module;
    return 1;
}
/* Buffers compile_compound_member keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CompileCompoundMemberBuffers {
    ZirFunction probe;
} CompileCompoundMemberBuffers;

int compile_compound_member(const CompileValue *compound, const char *member,
                        const ZirModule *module, const char *path, int depth,
                        int *fuel, CompileValue *result);

static int
compile_compound_member_with_buffers(const CompileValue *compound, const char *member,
                        const ZirModule *module, const char *path, int depth,
                        int *fuel, CompileValue *result, CompileCompoundMemberBuffers *buffers)
{
    if(compound->type_owner != NULL)
        module = compound->type_owner;
    memset(&buffers->probe, 0, sizeof(buffers->probe));
    int root = ParseExpr(&buffers->probe, module, compound->literal,
                         Span(path, 1, 1));
    int found = 0;
    if(root >= 0 && buffers->probe.exprs[root].kind == ZIR_EXPR_COMPOUND) {
        const ZirType *record = FindType(module, compound->type, NULL);
        for(int child = buffers->probe.exprs[root].first_child; child >= 0;
            child = buffers->probe.exprs[child].next_sibling) {
            const ZirExpr *entry = &buffers->probe.exprs[child];
            if(!strcmp(entry->name, member)) {
                ZirTypeField field;
                size_t offset = 0;
                while(record != NULL &&
                      TypeNextField(record, &offset, &field) == 1)
                    if(!strcmp(field.name, member)) {
                        found = evaluate_typed_node(&buffers->probe, entry->right,
                                                    module, path, depth + 1,
                                                    fuel, result) &&
                                compile_type_value(field.type, result);
                        break;
                    }
                break;
            }
        }
    }
    free(buffers->probe.exprs);
    return found;
}

COMPILER_SCRATCH_RETURN(, int, compile_compound_member,
    (const CompileValue *compound, const char *member,
                        const ZirModule *module, const char *path, int depth,
                        int *fuel, CompileValue *result),
    CompileCompoundMemberBuffers, RECOVERABLE, free,
    compile_compound_member_with_buffers(compound, member, module, path, depth, fuel, result, buffers))
/* Buffers compile_compound_index keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CompileCompoundIndexBuffers {
    ZirFunction probe;
} CompileCompoundIndexBuffers;

int compile_compound_index(const CompileValue *compound, int64_t index,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result);

static int
compile_compound_index_with_buffers(const CompileValue *compound, int64_t index,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result, CompileCompoundIndexBuffers *buffers)
{
    if(compound->type_owner != NULL)
        module = compound->type_owner;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(!ArrayElementType(compound->type, element, sizeof(element),
                         &capacity) || index < 0 || index >= capacity)
        return 0;
    const ZirFunction *probe;
    int root = CachedParse(module, compound->literal, Span(path, 1, 1),
                           &buffers->probe, &probe);
    int found = 0, ordinal = 0;
    if(root >= 0 && probe->exprs[root].kind == ZIR_EXPR_COMPOUND)
        for(int child = probe->exprs[root].first_child; child >= 0;
            child = probe->exprs[child].next_sibling, ordinal++)
            if(ordinal == index) {
                found = evaluate_typed_node(probe, probe->exprs[child].right,
                                            module, path, depth + 1, fuel,
                                            result) &&
                        compile_type_value(element, result);
                break;
            }
    CachedParseDone(probe, &buffers->probe);
    return found;
}

COMPILER_SCRATCH_RETURN(, int, compile_compound_index,
    (const CompileValue *compound, int64_t index,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result),
    CompileCompoundIndexBuffers, RECOVERABLE, free,
    compile_compound_index_with_buffers(compound, index, module, path, depth, fuel, result, buffers))

static int
local_value_name_visible(const ZirModule *module, const char *path,
                         const char *name)
{
    for(int i = 0; i < module->define_count; i++)
        if(!strcmp(module->defines[i].name, name) &&
           (!module->defines[i].is_file_private ||
            !strcmp(SpanPath(module->defines[i].span), path))) return 1;
    for(int i = 0; i < module->global_count; i++)
        if(!strcmp(module->globals[i].name, name) &&
           (!module->globals[i].is_file_private ||
            !strcmp(SpanPath(module->globals[i].span), path))) return 1;
    for(int i = 0; i < module->type_count; i++)
        if(!strcmp(module->types[i].name, name) &&
           (!module->types[i].is_file_private ||
            !strcmp(SpanPath(module->types[i].span), path))) return 1;
    for(int i = 0; i < module->function_count; i++)
        if(!strcmp(module->functions[i].name, name) &&
           (!module->functions[i].is_file_private ||
            !strcmp(SpanPath(module->functions[i].span), path))) return 1;
    return 0;
}

static const ZirDefine *
find_open_imported_define(const ZirModule *module, const char *path,
                          const char *name, const ZirModule **owner_out)
{
    const ZirDefine *definition = NULL;
    if(local_value_name_visible(module, path, name)) return NULL;
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if((import->kind != ZIR_IMPORT_OPEN &&
            !(import->kind == ZIR_IMPORT_MODULE && import->is_using)) ||
           (import->is_file_private &&
            strcmp(SpanPath(import->span), path) != 0) ||
           import->resolved_module == NULL) continue;
        const ZirModule *owner = import->resolved_module;
        for(int j = 0; j < owner->define_count; j++) {
            const ZirDefine *candidate = &owner->defines[j];
            if(!candidate->is_public || candidate->is_file_private ||
               strcmp(candidate->name, name)) continue;
            if(definition != NULL && definition != candidate) return NULL;
            definition = candidate;
            *owner_out = owner;
        }
    }
    return definition;
}

int
evaluate_imported_typed_define(const ZirModule *module, const char *path,
                               const char *name, int depth, int *fuel,
                               CompileValue *result)
{
    const ZirModule *owner = NULL;
    const ZirDefine *definition = strchr(name, '.') != NULL ?
        find_imported_define(module, path, name, strlen(name), &owner) :
        find_open_imported_define(module, path, name, &owner);
    ZirConsts names = {0};
    int ok;
    if(definition == NULL || depth >= 32) return 0;
    /* Law evaluation reads the owner's other constants through the owner
     * module when the value names them. */
    if(ZirLawEvaluation)
        return evaluate_typed_expression(owner, &names, definition->value,
                                         definition->span, depth + 1, fuel,
                                         result);
    names.count = owner->define_count;
    names.items = calloc((size_t)names.count + 1, sizeof(*names.items));
    if(names.items == NULL)
        die("out of memory evaluating imported constant");
    for(int i = 0; i < names.count; i++) {
        const ZirDefine *source = &owner->defines[i];
        ZirConst *constant = &names.items[i];
        copy_text(constant->name, sizeof(constant->name), source->name);
        constant->expr = KeepText(source->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(source->span));
        constant->is_file_private = source->is_file_private;
        constant->source_line = source->span.line;
    }
    ok = evaluate_typed_expression(owner, &names, definition->value,
                                   definition->span, depth + 1,
                                   fuel, result);
    free(names.items);
    return ok;
}
