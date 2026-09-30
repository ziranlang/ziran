#include "zir_parse_internal.h"

/* `[] s32` and `* Node` spell the types []s32 and *Node; drop the space
 * so every reader of the signature text sees one spelling. */
static void
tighten_type_spacing(char *text)
{
    char *out = text;
    int quote = 0;
    for(const char *p = text; *p; p++) {
        *out++ = *p;
        if(quote) {
            if(*p == '\\' && p[1]) *out++ = *++p;
            else if(*p == '"') quote = 0;
            continue;
        }
        if(*p == '"') {
            quote = 1;
            continue;
        }
        if(*p == ']' || *p == '*') {
            const char *next = p + 1;
            while(*next == ' ' || *next == '\t') next++;
            if(next != p + 1 && (isalpha((unsigned char)*next) || *next == '_' ||
                                 *next == '[' || *next == '$'))
                p = next - 1;
        }
    }
    *out = '\0';
}

void
parse_function_header(char *name, size_t name_size, char *args,
                      size_t args_size, char *ret, size_t ret_size,
                      const char *line)
{
    const char *p;
    const char *q;
    size_t n = 0;

    name[0] = '\0';
    args[0] = '\0';
    snprintf(ret, ret_size, "void");
    p = strstr(line, "::");
    if(p != NULL) {
        q = line;
        while(q < p && (*q == ' ' || *q == '\t'))
            q++;
        while(q < p && (isalnum((unsigned char)*q) || *q == '_') &&
              n + 1 < name_size)
            name[n++] = *q++;
        name[n] = '\0';
    }
    p = strchr(line, '(');
    q = closing_parenthesis(p);
    if(p != NULL && q != NULL && q > p) {
        n = (size_t)(q - p - 1);
        if(n >= args_size)
            n = args_size - 1;
        memcpy(args, p + 1, n);
        args[n] = '\0';
        while(n > 0 && isspace((unsigned char)args[n - 1]))
            args[--n] = '\0';
        /* Return type: after the closing ')', an optional '-> T' before any
         * trailing directive such as #foreign. */
        q++;
        while(*q == ' ' || *q == '\t')
            q++;
        if(q[0] == '-' && q[1] == '>') {
            q += 2;
            while(*q == ' ' || *q == '\t')
                q++;
            n = 0;
            while(*q != '\0' && *q != '#' && *q != '{' && n + 1 < ret_size)
                ret[n++] = *q++;
            while(n > 0 && isspace((unsigned char)ret[n - 1]))
                n--;
            ret[n] = '\0';
        }
    }
    tighten_type_spacing(args);
    tighten_type_spacing(ret);
}

int
function_must_use(const char *line, const char *return_type,
                  ZirSourceSpan span)
{
    const char *parameters = strchr(line, '(');
    const char *closing = closing_parenthesis(parameters);
    const char *end;
    int count = 0;

    if(closing == NULL)
        return 0;
    end = strchr(closing + 1, '{');
    if(end == NULL)
        end = strchr(closing + 1, ';');
    if(end == NULL)
        end = line + strlen(line);
    for(const char *cursor = closing + 1; cursor < end; cursor++) {
        if(*cursor == '"') {
            for(cursor++; cursor < end && *cursor != '"'; cursor++)
                if(*cursor == '\\' && cursor + 1 < end)
                    cursor++;
            if(cursor == end)
                break;
        } else if((size_t)(end - cursor) >= 5 &&
                  strncmp(cursor, "#must", 5) == 0 &&
                  (cursor + 5 == end ||
                   (!isalnum((unsigned char)cursor[5]) &&
                    cursor[5] != '_'))) {
            const char *after = skip_ws(cursor + 5);
            if(after < end && *after != '#')
                die_at(span, "#must does not take arguments");
            count++;
            cursor += 4;
        }
    }
    if(count > 1)
        die_at(span, "duplicate #must procedure modifier");
    if(count && !strcmp(return_type, "void"))
        die_at(span, "#must requires a return value");
    return count;
}

/* Return 1 for a standalone directive and 2 for an inline declaration. */
int
strip_program_export(char *line, char *symbol, size_t symbol_size,
                     ZirSourceSpan span)
{
    const char *cursor;
    const char *end;

    symbol[0] = '\0';
    if(!starts_word(line, "#program_export"))
        return 0;
    cursor = skip_ws(line + strlen("#program_export"));
    if(*cursor == '"') {
        end = strchr(cursor + 1, '"');
        if(end == NULL || (size_t)(end - cursor - 1) >= symbol_size)
            die_at(span, "#program_export requires a valid quoted symbol");
        memcpy(symbol, cursor + 1, (size_t)(end - cursor - 1));
        symbol[end - cursor - 1] = '\0';
        if(!is_c_ident(symbol))
            die_at(span, "#program_export symbol must be an identifier");
        cursor = skip_ws(end + 1);
    }
    if(*cursor == '\0')
        return 1;
    memmove(line, cursor, strlen(cursor) + 1);
    return 2;
}

void
separate_parameter_defaults(char *args, size_t capacity,
                            char *defaults, size_t defaults_capacity,
                            ZirSourceSpan span)
{
    char (*parts)[ZIR_TEXT_MAX] = calloc(65, sizeof(*parts));
    char cleaned[ZIR_TEXT_MAX] = "";
    size_t used = 0;
    int has_default = 0;
    if(parts == NULL)
        die("out of memory reading procedure parameters");
    int count = *skip_ws(args) ?
        split_top_level(args, parts[0], 65, sizeof(parts[0])) : 0;
    if(count > 64)
        die_at(span, "too many procedure parameters");
    for(int i = 0; i < count; i++) {
        char *assignment = top_level_assignment(parts[i]);
        if(assignment != NULL) {
            if(!*skip_ws(assignment + 1))
                die_at(span, "default parameter needs a value");
            *assignment = '\0';
            trim_in_place(parts[i]);
            has_default = 1;
        }
        int written = snprintf(cleaned + used, sizeof(cleaned) - used,
                               "%s%s", i ? ", " : "", parts[i]);
        if(written < 0 || (size_t)written >= sizeof(cleaned) - used)
            die_at(span, "procedure parameters exceed size limit");
        used += (size_t)written;
    }
    if(has_default) {
        copy_text(defaults, defaults_capacity, args);
        copy_text(args, capacity, cleaned);
    }
    free(parts);
}

uint64_t
strip_using_parameters(char *args, size_t capacity, ZirSourceSpan span)
{
    char (*parts)[ZIR_TEXT_MAX] = calloc(65, sizeof(*parts));
    char cleaned[ZIR_TEXT_MAX] = "";
    size_t used = 0;
    uint64_t flags = 0;
    if(parts == NULL)
        die("out of memory reading using parameters");
    int count = *skip_ws(args) ?
        split_top_level(args, parts[0], 65, sizeof(parts[0])) : 0;
    if(count > 64)
        die_at(span, "too many procedure parameters");
    for(int i = 0; i < count; i++) {
        char *part = trim(parts[i]);
        if(starts_word(part, "using")) {
            if(!isspace((unsigned char)part[5]))
                die_at(span, "using parameter modifiers are not supported");
            part = trim(part + 5);
            char *colon = strchr(part, ':');
            if(colon == NULL || colon == part ||
               (size_t)(colon - part) >= ZIR_NAME_MAX)
                die_at(span, "using parameter needs name: Type");
            char name[ZIR_NAME_MAX];
            memcpy(name, part, (size_t)(colon - part));
            name[colon - part] = '\0';
            trim_in_place(name);
            if(!is_identifier_text(name))
                die_at(span, "using parameter needs a plain name");
            flags |= UINT64_C(1) << i;
        }
        int written = snprintf(cleaned + used, sizeof(cleaned) - used,
                               "%s%s", i ? ", " : "", part);
        if(written < 0 || (size_t)written >= sizeof(cleaned) - used)
            die_at(span, "procedure parameters exceed size limit");
        used += (size_t)written;
    }
    copy_text(args, capacity, cleaned);
    free(parts);
    return flags;
}

static int
default_is_scope_independent(const char *expression, const char *path)
{
    ZirLexer lexer;
    ZirToken previous = {0};
    LexerInit(&lexer, expression, path);
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) return 1;
        if(token.kind == ZIR_TOKEN_UNKNOWN || token.truncated) return 0;
        if(token.kind == ZIR_TOKEN_DIRECTIVE &&
           strcmp(token.text, "#char")) return 0;
        if(token.kind == ZIR_TOKEN_IDENT &&
           strcmp(token.text, "true") && strcmp(token.text, "false") &&
           strcmp(token.text, "null")) {
            ZirLexer ahead = lexer;
            ZirToken next = LexerNext(&ahead);
            if((strcmp(previous.text, "{") &&
                strcmp(previous.text, ",")) ||
               strcmp(next.text, "=")) return 0;
        }
        previous = token;
    }
}
/* Buffers lower_procedure_name_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerProcedureNameExpressionBuffers {
    char lowered[ZIR_TEXT_MAX];
    ZirToken token;
    ZirToken open;
    ZirToken close;
} LowerProcedureNameExpressionBuffers;

int lower_procedure_name_expression(char *part, size_t capacity,
                                const ZirFunction *function);

static int
lower_procedure_name_expression_with_buffers(char *part, size_t capacity,
                                const ZirFunction *function, LowerProcedureNameExpressionBuffers *buffers)
{
    ZirLexer lexer;
    size_t copied = 0, used = 0;
    int changed = 0;
    /* Compile-time evaluation lowers every statement it runs; nearly none
     * mention the directive, so skip lexing those. */
    if(strstr(part, "#procedure_name") == NULL)
        return 0;
    LexerInit(&lexer, part, SpanPath(function->span));
    for(;;) {
        buffers->token = LexerNext(&lexer);
        if(buffers->token.kind == ZIR_TOKEN_EOF) break;
        size_t start = lexer.pos - strlen(buffers->token.text);
        if(buffers->token.kind == ZIR_TOKEN_OPERATOR &&
           strcmp(buffers->token.text, "/") == 0 && part[start + 1] == '/')
            break;
        if(buffers->token.kind != ZIR_TOKEN_DIRECTIVE ||
           strcmp(buffers->token.text, "#procedure_name") != 0)
            continue;
        buffers->open = LexerNext(&lexer);
        buffers->close = LexerNext(&lexer);
        if(strcmp(buffers->open.text, "(") || strcmp(buffers->close.text, ")"))
            die_at(function->span,
                   "#procedure_name() requires empty parentheses");
        char literal[ZIR_NAME_MAX + 3];
        int written = snprintf(literal, sizeof(literal), "\"%s\"",
                               function->name);
        if(written < 0 || (size_t)written >= sizeof(literal) ||
           start < copied || used + start - copied + (size_t)written >=
           sizeof(buffers->lowered))
            die_at(function->span, "procedure name expression is too long");
        memcpy(buffers->lowered + used, part + copied, start - copied);
        used += start - copied;
        memcpy(buffers->lowered + used, literal, (size_t)written);
        used += (size_t)written;
        copied = lexer.pos;
        changed = 1;
    }
    if(!changed) return 0;
    size_t rest = strlen(part + copied);
    if(used + rest >= sizeof(buffers->lowered) || used + rest >= capacity)
        die_at(function->span, "procedure name expression is too long");
    memcpy(buffers->lowered + used, part + copied, rest + 1);
    copy_text(part, capacity, buffers->lowered);
    return 1;
}

int
lower_procedure_name_expression(char *part, size_t capacity,
                                const ZirFunction *function)
{
    static _Thread_local LowerProcedureNameExpressionBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerProcedureNameExpressionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_procedure_name_expression_with_buffers(part, capacity, function, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers lower_template_record_default keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerTemplateRecordDefaultBuffers {
    char body[ZIR_TEXT_MAX];
    char normalized[ZIR_TEXT_MAX];
    char return_text[ZIR_TEXT_MAX];
    char rewritten[ZIR_TEXT_MAX];
} LowerTemplateRecordDefaultBuffers;

static int lower_template_record_default(ZirModule *module,
                              const ZirFunction *function, int parameter,
                              char *part, size_t capacity);

static int
lower_template_record_default_with_buffers(ZirModule *module,
                              const ZirFunction *function, int parameter,
                              char *part, size_t capacity, LowerTemplateRecordDefaultBuffers *buffers)
{
    char *assignment = top_level_assignment(part);
    if(!function->is_template || assignment == NULL) return 0;
    const char *value = skip_ws(assignment + 1);
    const char *dot = value;
    size_t name_length = strlen(function->template_param);
    if(strncmp(value, function->template_param, name_length) == 0)
        dot = skip_ws(value + name_length);
    if(*dot != '.') return 0;
    const char *opening = skip_ws(dot + 1);
    size_t value_length = strlen(value);
    while(value_length > 0 && isspace((unsigned char)value[value_length - 1]))
        value_length--;
    if(*opening != '{' || value_length == 0 ||
       value[value_length - 1] != '}') return 0;
    const char *closing = value + value_length - 1;
    if(closing < opening) return 0;
    size_t body_length = (size_t)(closing - opening - 1);
    if(body_length >= sizeof(buffers->body))
        die_at(function->span, "default record initializer is too long");
    memcpy(buffers->body, opening + 1, body_length);
    buffers->body[body_length] = '\0';
    char (*fields)[ZIR_TEXT_MAX] = calloc(65, sizeof(*fields));
    if(fields == NULL)
        die("out of memory lowering default record initializer");
    int field_count = *skip_ws(buffers->body) ?
        split_top_level(buffers->body, fields[0], 65, sizeof(fields[0])) : 0;
    if(field_count > 64)
        die_at(function->span, "too many default record initializer fields");
    int written = snprintf(buffers->normalized, sizeof(buffers->normalized), "%.*s.{",
                           (int)(assignment + 1 - part), part);
    if(written < 0 || (size_t)written >= sizeof(buffers->normalized))
        die_at(function->span, "default parameter expression is too long");
    size_t used = (size_t)written;
    for(int field = 0; field < field_count; field++) {
        char *field_assignment = top_level_assignment(fields[field]);
        const char *field_value = skip_ws(field_assignment != NULL ?
                                          field_assignment + 1 : fields[field]);
        if(!default_is_scope_independent(field_value, SpanPath(function->span))) {
            char result_type[ZIR_NAME_MAX];
            if(!InferExpressionType(module, field_value, function->span,
                                    result_type, sizeof(result_type)) ||
               !strcmp(result_type, "void") ||
               !strcmp(result_type, "null"))
                die_at(function->span,
                       "cannot resolve polymorphic default field in declaration scope: %s",
                       function->name);
            if(!strcmp(result_type, "integer"))
                copy_text(result_type, sizeof(result_type), "s64");
            else if(!strcmp(result_type, "real"))
                copy_text(result_type, sizeof(result_type), "float64");
            char base_name[ZIR_NAME_MAX], helper_name[ZIR_NAME_MAX];
            FunctionDefaultHelperName(function, parameter, base_name,
                                      sizeof(base_name));
            written = snprintf(helper_name, sizeof(helper_name),
                               "%s_field_%d", base_name, field);
            if(written < 0 || (size_t)written >= sizeof(helper_name))
                die_at(function->span, "default field helper name is too long");
            for(int f = 0; f < module->function_count; f++)
                if(!strcmp(module->functions[f].name, helper_name))
                    die_at(function->span,
                           "default field helper name conflicts with a procedure");
            written = snprintf(buffers->return_text, sizeof(buffers->return_text),
                               "return %s", field_value);
            if(written < 0 || (size_t)written >= sizeof(buffers->return_text))
                die_at(function->span, "default field expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name, "",
                                                    result_type, 0,
                                                    function->span);
            if(helper == NULL)
                die("out of memory creating default field helper");
            helper->is_public = function->is_public;
            helper->is_file_private = function->is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, buffers->return_text,
                               function->span) == NULL)
                die("out of memory creating default field helper body");
            written = field_assignment != NULL ?
                snprintf(buffers->rewritten, sizeof(buffers->rewritten), "%.*s %s()",
                         (int)(field_assignment + 1 - fields[field]),
                         fields[field], helper_name) :
                snprintf(buffers->rewritten, sizeof(buffers->rewritten), "%s()", helper_name);
            if(written < 0 || (size_t)written >= sizeof(buffers->rewritten))
                die_at(function->span, "default field initializer is too long");
            copy_text(fields[field], sizeof(fields[field]), buffers->rewritten);
        }
        written = snprintf(buffers->normalized + used, sizeof(buffers->normalized) - used,
                           "%s%s", field ? ", " : "", fields[field]);
        if(written < 0 || (size_t)written >= sizeof(buffers->normalized) - used)
            die_at(function->span, "default record initializer is too long");
        used += (size_t)written;
    }
    if(used + 2 > sizeof(buffers->normalized))
        die_at(function->span, "default record initializer is too long");
    buffers->normalized[used++] = '}';
    buffers->normalized[used] = '\0';
    copy_text(part, capacity, buffers->normalized);
    free(fields);
    return 1;
}

static int
lower_template_record_default(ZirModule *module,
                              const ZirFunction *function, int parameter,
                              char *part, size_t capacity)
{
    static _Thread_local LowerTemplateRecordDefaultBuffers *spares[16];
    static _Thread_local int spare_count;
    LowerTemplateRecordDefaultBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = lower_template_record_default_with_buffers(module, function, parameter, part, capacity, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers add_default_helpers keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct AddDefaultHelpersBuffers {
    char value[ZIR_TEXT_MAX];
    ZirFunction signature;
    char args[ZIR_TEXT_MAX];
    char full[ZIR_TEXT_MAX];
    char body[ZIR_TEXT_MAX];
} AddDefaultHelpersBuffers;

void add_default_helpers(ZirProgram *program, ZirModule *module,
                    const char *source_path, const char *root,
                    ZirCompileImportResolver resolver, void *resolver_context);

static void
add_default_helpers_with_buffers(ZirProgram *program, ZirModule *module,
                    const char *source_path, const char *root,
                    ZirCompileImportResolver resolver, void *resolver_context, AddDefaultHelpersBuffers *buffers)
{
    int declarations = module->function_count;
    int imports_resolved = 0;
    for(int fi = 0; fi < declarations; fi++) {
        const ZirFunction *function = &module->functions[fi];
        if(!FunctionDefaultArgs(function)[0] || function->default_helpers_created)
            continue;
        char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
        char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
        if(parameters == NULL || defaults == NULL)
            die("out of memory creating default argument helpers");
        int count = split_top_level(FunctionArgs(function), parameters[0], 64,
                                    sizeof(parameters[0]));
        if(split_top_level(FunctionDefaultArgs(function), defaults[0], 64,
                           sizeof(defaults[0])) != count)
            die_at(function->span, "invalid default parameter signature");
        int inferred = 0;
        for(int i = 0; i < count; i++)
            if(lower_procedure_name_expression(defaults[i],
                                               sizeof(defaults[i]), function))
                inferred = 1;
        unsigned char contextual[64] = {0};
        for(int i = 0; i < count; i++) {
            char *assignment = top_level_assignment(defaults[i]);
            if(assignment == NULL || assignment == defaults[i] ||
               assignment[-1] != ':') continue;
            char *colon = strchr(parameters[i], ':');
            if(colon == NULL || *skip_ws(colon + 1) != '\0')
                die_at(function->span, "inferred default parameter needs a name");
            *colon = '\0';
            char name[ZIR_NAME_MAX], type[ZIR_NAME_MAX];
            copy_text(name, sizeof(name), trim(parameters[i]));
            copy_text(buffers->value, sizeof(buffers->value), trim(assignment + 1));
            for(int imported = 0; imported < module->import_count; imported++) {
                ZirImport *import = &module->imports[imported];
                if(import->resolved_module != NULL ||
                   (import->kind != ZIR_IMPORT_OPEN &&
                    import->kind != ZIR_IMPORT_MODULE)) continue;
                for(int m = 0; m < program->module_count; m++)
                    if(&program->modules[m] != module &&
                       !strcmp(import->target, program->modules[m].name)) {
                        import->resolved_module = &program->modules[m];
                        TypeLookupsChanged();
                        break;
                    }
            }
            if(resolver != NULL && !imports_resolved) {
                if(!resolver(resolver_context, program, module, source_path,
                             root, NULL))
                    die_at(function->span,
                           "cannot resolve imports for default parameter: %s",
                           name);
                imports_resolved = 1;
            }
            int type_known;
            if(strcmp(buffers->value, "#caller_location") == 0) {
                copy_text(type, sizeof(type), "Source_Code_Location");
                type_known = 1;
            } else
                type_known = InferExpressionType(module, buffers->value,
                                                 function->span, type,
                                                 sizeof(type));
            if(!is_identifier_text(name) || !type_known ||
               !strcmp(type, "void") || !strcmp(type, "null"))
                die_at(function->span,
                       "cannot infer default parameter type: %s", name);
            int written = snprintf(parameters[i], sizeof(parameters[i]),
                                   "%s: %s", name, type);
            if(written < 0 || (size_t)written >= sizeof(parameters[i]))
                die_at(function->span, "procedure parameters exceed size limit");
            written = snprintf(defaults[i], sizeof(defaults[i]),
                               "%s = %s", parameters[i], buffers->value);
            if(written < 0 || (size_t)written >= sizeof(defaults[i]))
                die_at(function->span, "default parameters exceed size limit");
            inferred = 1;
        }
        buffers->signature = module->functions[fi];
        for(int i = 0; i < count; i++)
            if((contextual[i] = lower_template_record_default(
                    module, &buffers->signature, i, defaults[i],
                    sizeof(defaults[i]))))
                inferred = 1;
        if(inferred) {
            buffers->args[0] = '\0'; buffers->full[0] = '\0';
            size_t args_used = 0, full_used = 0;
            for(int i = 0; i < count; i++) {
                int written = snprintf(buffers->args + args_used,
                                       sizeof(buffers->args) - args_used, "%s%s",
                                       i ? ", " : "", parameters[i]);
                if(written < 0 || (size_t)written >= sizeof(buffers->args) - args_used)
                    die_at(function->span,
                           "procedure parameters exceed size limit");
                args_used += (size_t)written;
                written = snprintf(buffers->full + full_used,
                                   sizeof(buffers->full) - full_used, "%s%s",
                                   i ? ", " : "", defaults[i]);
                if(written < 0 || (size_t)written >= sizeof(buffers->full) - full_used)
                    die_at(function->span,
                           "default parameters exceed size limit");
                full_used += (size_t)written;
            }
            module->functions[fi].args_text = KeepParameters(buffers->args);
            module->functions[fi].default_args_text = KeepParameters(buffers->full);
        }
        for(int i = 0; i < count; i++) {
            function = &module->functions[fi];
            char *assignment = top_level_assignment(defaults[i]);
            if(assignment == NULL) continue;
            const char *value = trim(assignment + 1);
            if(contextual[i]) continue;
            if(strcmp(value, "#caller_location") == 0) {
                char *colon = strchr(parameters[i], ':');
                if(colon == NULL ||
                   strcmp(trim(colon + 1), "Source_Code_Location") != 0)
                    die_at(function->span,
                           "#caller_location requires a Source_Code_Location parameter");
                continue;
            }
            if(DefaultIsLiteral(value) ||
               (function->is_template &&
                default_is_scope_independent(value, SpanPath(function->span))))
                continue;
            char *colon = strchr(parameters[i], ':');
            if(colon == NULL)
                die_at(function->span, "default parameter needs a type");
            char helper_name[ZIR_NAME_MAX];
            char result_type[ZIR_NAME_MAX];
            ZirSourceSpan span = function->span;
            int is_public = function->is_public;
            int is_file_private = function->is_file_private;
            copy_text(result_type, sizeof(result_type), trim(colon + 1));
            if(function->is_template &&
               (!InferExpressionType(module, value, span, result_type,
                                     sizeof(result_type)) ||
                !strcmp(result_type, "void") ||
                !strcmp(result_type, "null")))
                die_at(span,
                       "cannot resolve polymorphic default in declaration scope: %s",
                       function->name);
            FunctionDefaultHelperName(function, i, helper_name,
                                      sizeof(helper_name));
            for(int f = 0; f < module->function_count; f++)
                if(!strcmp(module->functions[f].name, helper_name))
                    die_at(span, "default helper name conflicts with a procedure");
            int written = snprintf(buffers->body, sizeof(buffers->body), "return %s", value);
            if(written < 0 || (size_t)written >= sizeof(buffers->body))
                die_at(span, "default parameter expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name,
                                                    "", result_type, 0, span);
            if(helper == NULL)
                die("out of memory creating default argument helper");
            helper->is_public = is_public;
            helper->is_file_private = is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, buffers->body, span) == NULL)
                die("out of memory creating default argument body");
        }
        module->functions[fi].default_helpers_created = 1;
        free(parameters);
        free(defaults);
    }
}

void
add_default_helpers(ZirProgram *program, ZirModule *module,
                    const char *source_path, const char *root,
                    ZirCompileImportResolver resolver, void *resolver_context)
{
    static _Thread_local AddDefaultHelpersBuffers *spares[16];
    static _Thread_local int spare_count;
    AddDefaultHelpersBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    add_default_helpers_with_buffers(program, module, source_path, root, resolver, resolver_context, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

int
parse_import_line(ZirModule *module, const char *path, int line_no,
                  const char *line, int scope_public)
{
    const char *directive;
    char target[SOURCE_PATH_MAX];
    char name[ZIR_NAME_MAX];
    char signature[ZIR_TEXT_MAX];
    ZirImportKind kind;
    int quoted;

    directive = strstr(line, "#import");
    if(directive == NULL)
        return 0;
    /* `using Alias :: #import "Module";` re-exports the module's public
     * names into unqualified scope beside the alias. */
    int using_import = starts_word(line, "using");
    const char *body = line;
    if(using_import) {
        body = skip_ws(line + 5);
        if(!isalpha((unsigned char)*body) && *body != '_')
            return 0;
    }
    target[0] = '\0';
    name[0] = '\0';
    signature[0] = '\0';
    quoted = parse_quoted(directive, target, sizeof(target));
    if(!quoted)
        die_at(Span(path, line_no, 1),
               "Jai #import requires a quoted module or file name");
    /* Import targets are emitted verbatim inside #include "..." lines;
     * quotes and control bytes would let a crafted path escape the literal. */
    for(const char *p = target; *p != '\0'; p++)
        if(*p == '"' || *p == '>' || (unsigned char)*p < 0x20)
            die_at(Span(path, line_no, 1), "#import target contains a character that cannot "
                "appear in an include path");
    const char *mode = skip_ws(directive + strlen("#import"));
    if(*mode == ',') {
        mode = skip_ws(mode + 1);
        if(strncmp(mode, "dir", 3) == 0 &&
           isspace((unsigned char)mode[3])) {
            const char *path_arg = skip_ws(mode + 3);
            const char *end = *path_arg == '"' ? strchr(path_arg + 1, '"') : NULL;
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, dir requires a quoted path and ';'");
            int named = parse_symbol_before_colons(line, name, sizeof(name));
            if(target[0] == '/' || target[0] == '\0' ||
               strchr(target, '\\') != NULL ||
               target[strlen(target) - 1] == '/')
                die_at(Span(path, line_no, 1),
                       "#import, dir requires a relative directory path");
            const char *basename = strrchr(target, '/');
            basename = basename == NULL ? target : basename + 1;
            size_t length = strlen(basename);
            if(length == 0 || length >= sizeof(name) ||
               (!isalpha((unsigned char)basename[0]) && basename[0] != '_'))
                die_at(Span(path, line_no, 1),
                       "#import, dir path needs an identifier directory name");
            for(size_t i = 1; i < length; i++)
                if(!source_identifier_byte((unsigned char)basename[i]))
                    die_at(Span(path, line_no, 1),
                           "#import, dir path needs an identifier directory name");
            if(!named) {
                memcpy(name, basename, length);
                name[length] = '\0';
            }
            snprintf(signature, sizeof(signature), "dir:%s", target);
            memmove(target, basename, length);
            target[length] = '\0';
            kind = named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN;
        } else {
            if(strncmp(mode, "file", 4) != 0 ||
               !isspace((unsigned char)mode[4]))
                die_at(Span(path, line_no, 1),
                       "unsupported #import mode: %s", mode);
            const char *file_arg = skip_ws(mode + 4);
            const char *end = *file_arg == '"' ? strchr(file_arg + 1, '"') : NULL;
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, file requires a quoted path and ';'");
            int named = parse_symbol_before_colons(line, name, sizeof(name));
            if(target[0] == '/' || target[0] == '\0' ||
               strlen(target) < 4 ||
               strcmp(target + strlen(target) - 3, ".zi") != 0)
                die_at(Span(path, line_no, 1),
                       "#import, file requires a relative .zi path");
            const char *basename = strrchr(target, '/');
            basename = basename == NULL ? target : basename + 1;
            size_t stem_length = strlen(basename) - 3;
            if(stem_length == 0 || stem_length >= sizeof(name) ||
               (!isalpha((unsigned char)basename[0]) && basename[0] != '_'))
                die_at(Span(path, line_no, 1),
                       "#import, file path needs an identifier filename");
            for(size_t i = 1; i < stem_length; i++)
                if(!source_identifier_byte((unsigned char)basename[i]))
                    die_at(Span(path, line_no, 1),
                           "#import, file path needs an identifier filename");
            if(!named) {
                memcpy(name, basename, stem_length);
                name[stem_length] = '\0';
            }
            snprintf(signature, sizeof(signature), "file:%s", target);
            if(named) {
                memmove(target, basename, stem_length);
                target[stem_length] = '\0';
            } else
                copy_text(target, sizeof(target), name);
            kind = named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN;
        }
    }
    /* A package-qualified import names the dependency before the module,
     * such as #import "kryon/Widgets" or #import "std/text". */
    const char *module_part = signature[0] == '\0' && strchr(target, '/') != NULL ?
                              strchr(target, '/') + 1 : target;
    if(signature[0] == '\0') {
        if(parse_symbol_before_colons(body, name, sizeof(name)))
            kind = ZIR_IMPORT_MODULE;
        else {
            copy_text(name, sizeof(name), module_part);
            kind = ZIR_IMPORT_OPEN;
        }
    }
    if(kind == ZIR_IMPORT_MODULE &&
       !isalpha((unsigned char)name[0]) && name[0] != '_')
        die_at(Span(path, line_no, 1),
               "import alias must start with a letter or underscore");
    for(const char *part = target; part != NULL;
        part = part == module_part ? NULL : module_part) {
        if(!isalpha((unsigned char)part[0]) && part[0] != '_')
            die_at(Span(path, line_no, 1),
                   "Jai #import requires a module identifier or PACKAGE/Module; "
                   "use #import, file for paths");
        for(const unsigned char *p = (const unsigned char *)part;
            *p && *p != '/'; p++)
            if(!source_identifier_byte(*p))
                die_at(Span(path, line_no, 1),
                       "Jai #import requires a module identifier or PACKAGE/Module; "
                       "use #import, file for paths");
        if(part == module_part && strchr(part, '/') != NULL)
            die_at(Span(path, line_no, 1),
                   "a package import has one '/', as in #import \"PACKAGE/Module\"");
    }
    /* A private-scope include belongs in the implementation, not the
     * generated header. File imports retain their source path in signature. */
    {
        ZirImport *imported = ModuleAddImport(module, kind, name, target,
                                             signature, scope_public,
                                             Span(path, line_no, 1));
        if(imported != NULL)
            imported->is_public = scope_public;
        if(imported != NULL && using_import) {
            if(kind != ZIR_IMPORT_MODULE)
                die_at(Span(path, line_no, 1),
                       "using #import requires a named module alias");
            imported->is_using = 1;
        }
    }
    return 1;
}

int
parse_foreign_library_line(const char *path, int line_no, const char *line,
                           char names[][ZIR_NAME_MAX],
                           char targets[][ZIR_PATH_MAX], int *count)
{
    char name[ZIR_NAME_MAX];
    char target[ZIR_PATH_MAX];
    const char *colons = strstr(line, "::");
    const char *declaration;
    const char *quote;
    const char *end;

    if(colons == NULL ||
       !parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    declaration = skip_ws(colons + 2);
    if(!starts_word(declaration, "#system_library"))
        return 0;
    quote = skip_ws(declaration + strlen("#system_library"));
    if(*quote != '"' || !parse_quoted(quote, target, sizeof(target)) ||
       target[0] == '\0')
        die_at(Span(path, line_no, 1),
               "#system_library requires a quoted library name");
    end = strchr(quote + 1, '"');
    if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0)
        die_at(Span(path, line_no, 1),
               "#system_library declaration must end with ';'");
    for(const unsigned char *byte = (const unsigned char *)target;
        *byte != '\0'; byte++)
        if(*byte < 0x20 || *byte == '"' || *byte == '\\')
            die_at(Span(path, line_no, 1),
                   "#system_library name contains an invalid character");
    for(int i = 0; i < *count; i++)
        if(strcmp(names[i], name) == 0)
            die_at(Span(path, line_no, 1),
                   "duplicate #system_library name: %s", name);
    if(*count >= 32)
        die_at(Span(path, line_no, 1), "too many #system_library declarations");
    copy_text(names[*count], ZIR_NAME_MAX, name);
    copy_text(targets[*count], ZIR_PATH_MAX, target);
    (*count)++;
    return 1;
}
/* Buffers parse_foreign_line keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ParseForeignLineBuffers {
    char target[ZIR_PATH_MAX];
    char parameters[8][ZIR_TEXT_MAX];
} ParseForeignLineBuffers;

static int
go_method_symbol(const char *source, char *receiver, size_t receiver_size,
                 char *method, size_t method_size)
{
    const char *dot = strchr(source, '.');
    if(!dot || dot == source) return 0;
    size_t length = (size_t)(dot - source);
    const char *start = source;
    if(source[0] == '(') {
        if(length < 3 || source[length - 1] != ')') return 0;
        start++;
        length -= 2;
    }
    if(length >= receiver_size || strlen(dot + 1) >= method_size) return 0;
    memcpy(receiver, start, length);
    receiver[length] = '\0';
    copy_text(method, method_size, dot + 1);
    return is_c_ident(receiver + (receiver[0] == '*')) && is_c_ident(method);
}

int parse_foreign_line(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count);

static int
parse_foreign_line_with_buffers(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count, ParseForeignLineBuffers *buffers)
{
    char name[ZIR_NAME_MAX];
    char symbol[ZIR_NAME_MAX];
    char library[ZIR_NAME_MAX];
    char foreign_name[ZIR_NAME_MAX];
    ZirImport *imp;
    ZirExternKind extern_kind;
    if(!parse_symbol_before_colons(line, name, sizeof(name)))
        return 0;
    const char *declaration = skip_ws(strstr(line, "::") + 2);
    const char *dir = strstr(line, "#foreign");
    if(dir == NULL)
        return 0;
    int foreign_type = starts_word(declaration, "#type");
    const char *results_attribute = strstr(declaration, "#go_results");
    int go_results = contains_source_directive(declaration, "#go_results") &&
        results_attribute && results_attribute < dir;
    if(go_results && (foreign_type || strstr(results_attribute + strlen("#go_results"), "#go_results")))
        die_at(Span(path, line_no, 1), "#go_results requires one foreign procedure result record");
    const char *field_attribute = strstr(declaration, "#go_field");
    int go_field = contains_source_directive(declaration, "#go_field") &&
        field_attribute && field_attribute < dir;
    if(go_field && (foreign_type || go_results ||
                   strstr(field_attribute + strlen("#go_field"), "#go_field")))
        die_at(Span(path, line_no, 1), "#go_field requires one foreign field getter without #go_results");
    const char *defer_attribute = strstr(declaration, "#go_defer");
    int go_defer = contains_source_directive(declaration, "#go_defer") &&
        defer_attribute && defer_attribute < dir;
    if(go_defer && (foreign_type || go_results || go_field ||
                   strstr(defer_attribute + strlen("#go_defer"), "#go_defer")))
        die_at(Span(path, line_no, 1), "#go_defer requires one foreign procedure without result or field attributes");
    if(foreign_type && skip_ws(declaration + strlen("#type")) != dir)
        die_at(Span(path, line_no, 1),
               "foreign type requires Name :: #type #foreign library;");
    if(!foreign_type && *declaration != '(')
        return 0;
    if(strchr(line, '{') != NULL)
        die_at(Span(path, line_no, 1),
               "#foreign procedure declarations cannot have a body");
    const char *cursor = skip_ws(dir + strlen("#foreign"));
    size_t used = 0;
    while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
          used + 1 < sizeof(library))
        library[used++] = *cursor++;
    library[used] = '\0';
    if(!is_identifier_text(library))
        die_at(Span(path, line_no, 1),
               "#foreign requires a named #system_library");
    cursor = skip_ws(cursor);
    copy_text(foreign_name, sizeof(foreign_name), name);
    if(*cursor == '"') {
        size_t length = 0;
        cursor++;
        while(*cursor != '\0' && *cursor != '"' &&
              length + 1 < sizeof(foreign_name))
            foreign_name[length++] = *cursor++;
        foreign_name[length] = '\0';
        if(*cursor != '"')
            die_at(Span(path, line_no, 1),
                   "#foreign alternate symbol must be an identifier");
        cursor = skip_ws(cursor + 1);
    }
    if(strcmp(cursor, ";") != 0)
        die_at(Span(path, line_no, 1),
               "#foreign declaration must end with ';'");
    const char *library_target = NULL;
    for(int i = 0; i < count; i++)
        if(strcmp(names[i], library) == 0 &&
           (!file_private[i] || strcmp(paths[i], path) == 0)) {
            library_target = targets[i];
            break;
        }
    if(library_target == NULL)
        die_at(Span(path, line_no, 1),
               "#foreign library is not declared: %s", library);
    char receiver[ZIR_NAME_MAX] = "", method[ZIR_NAME_MAX] = "";
    int method_binding = !foreign_type && !strncmp(library_target, "go:", 3) &&
        go_method_symbol(foreign_name, receiver, sizeof(receiver), method, sizeof(method));
    if(!is_c_ident(foreign_name) && !method_binding)
        die_at(Span(path, line_no, 1),
               "#foreign alternate symbol must be an identifier or a Go method expression");
    buffers->target[0] = '\0';
    symbol[0] = '\0';
    if(strcmp(library_target, "host_api") == 0) {
        if(strcmp(foreign_name, name) != 0)
            die_at(Span(path, line_no, 1),
                   "host capability cannot rename a #foreign symbol");
    } else if(method_binding) {
        if(snprintf(buffers->target, sizeof(buffers->target), "%s.(%s).%s", library_target,
                    receiver, method) >= (int)sizeof(buffers->target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    } else if(strncmp(library_target, "go:", 3) == 0 ||
              strchr(library_target, '/') != NULL) {
        if(snprintf(buffers->target, sizeof(buffers->target), "%s.%s", library_target,
                    foreign_name) >= (int)sizeof(buffers->target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    } else {
        if(snprintf(buffers->target, sizeof(buffers->target), "c.%s", foreign_name) >=
           (int)sizeof(buffers->target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    }
    extern_kind = classify_extern_target(buffers->target, symbol, sizeof(symbol),
                                         path, line_no);
    if(go_results && (extern_kind != ZIR_EXTERN_GO || strncmp(buffers->target, "go:", 3)))
        die_at(Span(path, line_no, 1), "#go_results requires an explicit Go foreign target");
    if(go_field && (extern_kind != ZIR_EXTERN_GO || strncmp(buffers->target, "go:", 3)))
        die_at(Span(path, line_no, 1), "#go_field requires an explicit Go foreign target");
    if(go_defer && (extern_kind != ZIR_EXTERN_GO || strncmp(buffers->target, "go:", 3)))
        die_at(Span(path, line_no, 1), "#go_defer requires an explicit Go foreign target");
    if(foreign_type) {
        if(!GoForeignTargetValid(buffers->target))
            die_at(Span(path, line_no, 1),
                   "foreign types require an explicit go: #system_library");
        ZirType *type = ModuleAddType(module, name, Span(path, line_no, 1));
        if(type == NULL)
            die("out of memory declaring foreign type");
        type->is_extern = 1;
        type->is_public = scope_public;
        copy_text(type->foreign_target, sizeof(type->foreign_target), buffers->target);
        return 1;
    }
    imp = ModuleAddImport(module, ZIR_IMPORT_EXTERN,
                             name, buffers->target[0] ? buffers->target : name, line, 1,
                             Span(path, line_no, 1));
    if(imp != NULL) {
        char parsed_name[ZIR_NAME_MAX];
        imp->is_public = scope_public;
        parse_function_header(parsed_name, sizeof(parsed_name), imp->args,
                              sizeof(imp->args), imp->return_type,
                              sizeof(imp->return_type), line);
        imp->must_use = function_must_use(line, imp->return_type,
                                           imp->span);
        imp->extern_kind = extern_kind;
        imp->go_results = go_results;
        imp->go_field = go_field;
        imp->go_defer = go_defer;
        snprintf(imp->extern_symbol, sizeof(imp->extern_symbol), "%s",
                 symbol);
        /* A trailing `..any` parameter marks a variadic C ABI: calls may
         * pass any number of extra arguments after the fixed ones. */
        {
            int parameter_count = *skip_ws(imp->args) ?
                split_top_level(imp->args, buffers->parameters[0], 8,
                                sizeof(buffers->parameters[0])) : 0;
            if(parameter_count > 0) {
                const char *last = skip_ws(buffers->parameters[parameter_count - 1]);
                const char *colon = strchr(last, ':');
                const char *tail = colon != NULL ? skip_ws(colon + 1) : last;
                if(!strcmp(tail, "..any") || !strcmp(last, "..any"))
                    imp->is_varargs = 1;
            }
        }
    }
    return 1;
}

int
parse_foreign_line(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count)
{
    static _Thread_local ParseForeignLineBuffers *spares[16];
    static _Thread_local int spare_count;
    ParseForeignLineBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = parse_foreign_line_with_buffers(module, path, line_no, line, scope_public, names, targets, paths, file_private, count, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
paren_params_are_typed(const char *body)
{
    const char *inner = NULL;
    const char *end = NULL;
    const char *p;
    size_t depth = 0;

    /* Find the top-level parameter list and its matching close. */
    for(p = body; *p != '\0'; p++) {
        if(*p == '(') {
            if(depth == 0)
                inner = p + 1;
            depth++;
        } else if(*p == ')') {
            depth--;
            if(depth == 0) {
                end = p;
                break;
            }
        }
    }
    if(inner == NULL || end == NULL)
        return 0;
    while(inner < end && (*inner == ' ' || *inner == '\t'))
        inner++;
    if(inner >= end)
        return 1; /* () — empty parameter list */
    /* Jai-style rule: a binding like 'X :: (expr)' stays a constant unless
     * the parenthesized text is a typed parameter list. Parameter lists
     * name their arguments ('name: Type'); bare expressions — numbers,
     * arithmetic, ternaries, literals — do not. */
    if(*inner == '"' || *inner == '\'' || isdigit((unsigned char)*inner))
        return 0;
    if(strchr(inner, '?') != NULL)
        return 0;
    return strchr(inner, ':') != NULL;
}

/* Does a brace appear outside string/char literals? String values may
 * carry JSON or SQL text with braces. */
int
brace_outside_literals(const char *text)
{
    int in_string = 0, in_char = 0, escaped = 0;
    for(const char *p = text; *p; p++) {
        if(escaped) { escaped = 0; continue; }
        if(*p == '\\') { escaped = 1; continue; }
        if(in_string) {
            if(*p == '"') in_string = 0;
            continue;
        }
        if(in_char) {
            if(*p == '\'') in_char = 0;
            continue;
        }
        if(*p == '"') { in_string = 1; continue; }
        if(*p == '\'') { in_char = 1; continue; }
        if(*p == '{') return 1;
    }
    return 0;
}

int
looks_like_function_header(const char *line)
{
    char tmp[SOURCE_LINE_MAX];
    char *p;
    const char *body;

    if(starts_word(line, "#import") || strncmp(line, "#import,", 8) == 0)
        return 0;
    p = strstr(line, "::");
    if(p == NULL)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", p + 2);
    body = trim(tmp);
    if(starts_word(body, "#as"))
        body = skip_ws(body + 3);
    if(starts_word(body, "#import") || strncmp(body, "#import,", 8) == 0 ||
       starts_word(body, "#defined") ||
       starts_word(body, "#define") || starts_word(body, "struct") ||
       starts_word(body, "enum") || starts_word(body, "union"))
        return 0;
    if(strstr(body, "#type") != NULL)
        return 0;
    if(body[0] != '(')
        return 0;
    /* A body or return type makes it a procedure regardless of params. */
    if(strchr(body, '{') != NULL || strstr(body, "->") != NULL)
        return 1;
    return paren_params_are_typed(body);
}

int
split_oneline_function(const char *line, char *head, size_t head_size,
                       char *body, size_t body_size)
{
    const char *open = strchr(line, '{');
    size_t length = strlen(line);
    int depth = 0, in_string = 0, in_char = 0;

    if(open == NULL || length == 0 || line[length - 1] != '}' ||
       !looks_like_function_header(line))
        return 0;
    for(const char *p = open; *p; p++) {
        if(in_string || in_char) {
            if(*p == '\\' && p[1] != '\0') {
                p++;
            } else if(*p == (in_string ? '"' : '\'')) {
                in_string = in_char = 0;
            }
        } else if(*p == '"') {
            in_string = 1;
        } else if(*p == '\'') {
            in_char = 1;
        } else if(*p == '{') {
            depth++;
        } else if(*p == '}') {
            depth--;
            if(depth == 0 && p != line + length - 1)
                return 0;
        }
        if(depth < 0)
            return 0;
    }
    if(depth != 0 ||
       snprintf(head, head_size, "%.*s", (int)(open - line + 1), line) < 0 ||
       snprintf(body, body_size, "%.*s", (int)(line + length - open - 2),
                open + 1) < 0)
        return 0;
    trim_in_place(body);
    return 1;
}

/* Jai's `operator * :: (v: V, k: s32) -> V #symmetric` also takes its two
 * arguments in the other order. Remove the directive from HEADER and write
 * the one-line wrapper that takes them swapped into WRAPPER. Returns 0 when
 * both parameters have one type, so no wrapper is needed. */
int
symmetric_operator_wrapper(char *header, char *wrapper, size_t size, ZirSourceSpan span)
{
    char *directive = strstr(header, "#symmetric");
    if(directive != NULL)
        memmove(directive, directive + 10, strlen(directive + 10) + 1);
    char name[ZIR_NAME_MAX], result[ZIR_NAME_MAX];
    char *args = AllocateOrExit(ZIR_TEXT_MAX);
    char (*parts)[ZIR_TEXT_MAX] = AllocateOrExit(3 * sizeof(*parts));
    char *open = strchr(header, '{');
    char saved = 0;
    if(open != NULL) { saved = *open; *open = '\0'; }
    parse_function_header(name, sizeof(name), args, ZIR_TEXT_MAX,
                          result, sizeof(result), header);
    if(open != NULL) *open = saved;
    if(strncmp(name, "operator_", 9) != 0 ||
       split_top_level(args, parts[0], 3, sizeof(parts[0])) != 2)
        die_at(span, "#symmetric requires an operator procedure with two parameters");
    char first[ZIR_NAME_MAX], second[ZIR_NAME_MAX];
    const char *types[2];
    char *names[2] = { first, second };
    for(int i = 0; i < 2; i++) {
        char *colon = strchr(parts[i], ':');
        if(colon == NULL)
            die_at(span, "#symmetric requires an operator procedure with two parameters");
        snprintf(names[i], ZIR_NAME_MAX, "%.*s", (int)(colon - parts[i]), parts[i]);
        trim_in_place(names[i]);
        types[i] = skip_ws(colon + 1);
    }
    int needed = strcmp(types[0], types[1]) != 0;
    if(needed) {
        int written = snprintf(wrapper, size,
            "%s :: (%s: %s, %s: %s)%s%s { return %s(%s, %s); }",
            name, second, types[1], first, types[0],
            strcmp(result, "void") ? " -> " : "", strcmp(result, "void") ? result : "",
            name, first, second);
        if(written < 0 || (size_t)written >= size)
            die_at(span, "#symmetric operator header exceeds source limit");
    }
    free(args);
    free(parts);
    return needed;
}

void
defer_line(DeferredLines *lines, const char *line, const char *rel, int line_no,
           int scope_public, int scope_file)
{
    DeferredLine *grown = realloc(lines->items,
                                  (size_t)(lines->count + 1) * sizeof(*grown));
    if(grown == NULL)
        die("out of memory");
    lines->items = grown;
    DeferredLine *added = &lines->items[lines->count++];
    added->line = strdup(line);
    if(added->line == NULL)
        die("out of memory");
    copy_text(added->rel, sizeof(added->rel), rel);
    added->line_no = line_no;
    added->scope_public = scope_public;
    added->scope_file = scope_file;
}

/* Rename each use of a local procedure in LINE (NAMES holds source and
 * hoisted names) outside strings, characters, and member accesses. */
void
rename_local_procedures(char *line, size_t capacity, char (*names)[2][ZIR_NAME_MAX],
                        int count, ZirSourceSpan span)
{
    char *out = AllocateOrExit(capacity);
    size_t used = 0;
    for(const char *p = line; *p;) {
        if(*p == '"' || *p == '\'') {
            char quote = *p;
            const char *start = p++;
            while(*p && *p != quote) {
                if(*p == '\\' && p[1]) p++;
                p++;
            }
            if(*p) p++;
            size_t length = (size_t)(p - start);
            if(used + length >= capacity) die_at(span, "source line exceeds size limit");
            memcpy(out + used, start, length);
            used += length;
            continue;
        }
        if(p[0] == '/' && p[1] == '/') {
            size_t length = strlen(p);
            if(used + length >= capacity) die_at(span, "source line exceeds size limit");
            memcpy(out + used, p, length);
            used += length;
            break;
        }
        if(isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            size_t length = (size_t)(p - start);
            const char *replacement = NULL;
            int member = start > line && start[-1] == '.';
            for(int i = count - 1; i >= 0 && replacement == NULL && !member; i--)
                if(strlen(names[i][0]) == length && !strncmp(names[i][0], start, length))
                    replacement = names[i][1];
            const char *text = replacement != NULL ? replacement : start;
            size_t text_length = replacement != NULL ? strlen(replacement) : length;
            if(used + text_length >= capacity) die_at(span, "source line exceeds size limit");
            memcpy(out + used, text, text_length);
            used += text_length;
            continue;
        }
        if(used + 1 >= capacity) die_at(span, "source line exceeds size limit");
        out[used++] = *p++;
    }
    out[used] = '\0';
    memcpy(line, out, used + 1);
    free(out);
}

/* Locate Jai's optional `then` on an if header. A `then` inside a string,
 * grouped expression, or an unparenthesized ifx condition is not the header
 * separator. The latter remains an expression parse error until that form
 * can be split without guessing where its conditional arms end. */
static char *
if_header_then(char *line)
{
    const char *condition = NULL;
    int parens = 0, brackets = 0, braces = 0, quote = 0;
    if(starts_word(line, "if"))
        condition = skip_ws(line + 2);
    else if(starts_word(line, "else") &&
            starts_word(skip_ws(line + 4), "if"))
        condition = skip_ws(skip_ws(line + 4) + 2);
    if(condition == NULL)
        return NULL;
    for(char *p = (char *)condition; *p; p++) {
        if(quote) {
            if(*p == '\\' && p[1]) p++;
            else if(*p == quote) quote = 0;
            continue;
        }
        if(*p == '"' || *p == '\'') { quote = *p; continue; }
        if(*p == '(') { parens++; continue; }
        if(*p == ')') { parens--; continue; }
        if(*p == '[') { brackets++; continue; }
        if(*p == ']') { brackets--; continue; }
        if(*p == '{') { braces++; continue; }
        if(*p == '}') { braces--; continue; }
        if(parens || brackets || braces) continue;
        if((p == condition || !source_identifier_byte((unsigned char)p[-1])) &&
           strncmp(p, "ifx", 3) == 0 &&
           !source_identifier_byte((unsigned char)p[3]))
            return NULL;
        if((p == condition || !source_identifier_byte((unsigned char)p[-1])) &&
           strncmp(p, "then", 4) == 0 &&
           !source_identifier_byte((unsigned char)p[4]))
            return p;
    }
    return NULL;
}
/* Buffers split_jai_control_line keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct SplitJaiControlLineBuffers {
    char body[SOURCE_LINE_MAX * 2];
} SplitJaiControlLineBuffers;

void split_jai_control_line(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span);

static void
split_jai_control_line_with_buffers(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span, SplitJaiControlLineBuffers *buffers)
{
    char *then = if_header_then(line);
    if(then != NULL) {
        const char *condition = starts_word(line, "if") ?
            skip_ws(line + 2) : skip_ws(skip_ws(line + 4) + 2);
        if(then == condition)
            die_at(span, "if then requires a condition");
        const char *after = skip_ws(then + 4);
        if(strlen(after) >= sizeof(buffers->body))
            die_at(span, "if body exceeds source limit");
        copy_text(buffers->body, sizeof(buffers->body), after);
        char *end = then;
        while(end > line && isspace((unsigned char)end[-1]))
            end--;
        *end = '\0';
        if(buffers->body[0] == '{') {
            size_t used = strlen(line);
            if(snprintf(line + used, capacity - used, " %s", buffers->body) >=
               (int)(capacity - used))
                die_at(span, "if header exceeds source limit");
        } else if(buffers->body[0] != '\0') {
            prepend_logical_line(queue, count, buffers->body, span);
        }
    }
    if(starts_word(line, "else")) {
        const char *body = skip_ws(line + 4);
        if(*body && *body != '{' &&
           !starts_word(body, "if") &&
           !starts_word(body, "#if")) {
            prepend_logical_line(queue, count, body, span);
            line[4] = '\0';
        }
    }
}

void
split_jai_control_line(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span)
{
    static _Thread_local SplitJaiControlLineBuffers *spares[16];
    static _Thread_local int spare_count;
    SplitJaiControlLineBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    split_jai_control_line_with_buffers(line, capacity, queue, count, span, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

int
line_is_compile_else(const char *line)
{
    return strcmp(line, "} else {") == 0 || strcmp(line, "else {") == 0;
}

/* '#if COND {' / '} else #if COND {': strips the
 * trailing '{' — region braces are consumed, never emitted. Returns 1 for
 * '#if', 2 for 'else #if', 0 otherwise; *condition points into line. */
int
parse_cond_start(char *line, char **condition)
{
    char *q = NULL;
    int kind = 0;
    size_t n;

    if(strncmp(line, "#if", 3) == 0 &&
       (line[3] == '\0' || isspace((unsigned char)line[3]))) {
        q = line + 3;
        kind = 1;
    } else if(strncmp(line, "else #if", 8) == 0 &&
              (line[8] == '\0' || isspace((unsigned char)line[8]))) {
        q = line + 8;
        kind = 2;
    } else if(strncmp(line, "} else #if", 10) == 0 &&
              (line[10] == '\0' || isspace((unsigned char)line[10]))) {
        q = line + 10;
        kind = 2;
    } else {
        return 0;
    }
    q = trim(q);
    n = strlen(q);
    if(n == 0 || q[n - 1] != '{')
        return 0;
    q[n - 1] = '\0';
    q = trim(q);
    if(q[0] == '\0')
        return 0;
    *condition = q;
    return kind;
}

int
line_starts_compile_condition(const char *line)
{
    line = skip_ws(line);
    return starts_word(line, "#if") ||
           starts_word(line, "else #if") ||
           starts_word(line, "} else #if");
}

/* A procedure with several results, `-> s32, s32` or
 * `-> (quotient: s32, remainder: s32)`, returns a generated record whose
 * fields value_0, value_1, ... hold the results in order. Replaces RET
 * with the record's name and returns the result count, or 0 for one result. */
int
declare_multiple_results(ZirModule *module, const char *name, char *ret,
                         size_t ret_size, int is_public, int is_file_private,
                         ZirSourceSpan span)
{
    char list[ZIR_TEXT_MAX];
    char (*parts)[ZIR_TEXT_MAX] = AllocateOrExit(16 * sizeof(*parts));
    char *body = AllocateOrExit(ZIR_TEXT_MAX * 2);
    copy_text(list, sizeof(list), ret);
    trim_in_place(list);
    size_t length = strlen(list);
    int named = list[0] == '(' && length > 1 && list[length - 1] == ')' &&
                closing_parenthesis(list) == list + length - 1 &&
                strchr(list, ':') != NULL;
    if(named) {
        memmove(list, list + 1, length - 2);
        list[length - 2] = '\0';
    }
    int count = split_top_level(list, parts[0], 16, sizeof(parts[0]));
    if(count < 2) {
        free(parts);
        free(body);
        return 0;
    }
    size_t used = 0;
    body[0] = '\0';
    for(int i = 0; i < count; i++) {
        const char *type = parts[i];
        const char *colon = strchr(parts[i], ':');
        if(named) {
            if(colon == NULL)
                die_at(span, "named results need name: Type for every result");
            type = skip_ws(colon + 1);
        }
        if(!*type || strchr(type, '$') != NULL)
            die_at(span, "a result of a procedure with several results needs a concrete type");
        int written = snprintf(body + used, ZIR_TEXT_MAX * 2 - used, "value_%d: %s\n", i, type);
        if(written < 0 || (size_t)written >= ZIR_TEXT_MAX * 2 - used)
            die_at(span, "multiple results exceed the size limit");
        used += (size_t)written;
    }
    /* Procedures with the same result types share one record, so one can
     * return another's results: s32, s32 gives Results__s32__s32. */
    char record[ZIR_NAME_MAX];
    size_t written_name = (size_t)snprintf(record, sizeof(record), "Results");
    for(int i = 0; i < count && written_name + 3 < sizeof(record); i++) {
        const char *type = named ? skip_ws(strchr(parts[i], ':') + 1) : parts[i];
        record[written_name++] = '_';
        record[written_name++] = '_';
        for(const char *p = type; *p && written_name + 1 < sizeof(record); p++)
            if(!isspace((unsigned char)*p))
                record[written_name++] = isalnum((unsigned char)*p) || *p == '_' ? *p :
                                         *p == '*' ? 'p' : *p == '[' ? 'a' : '_';
        record[written_name] = '\0';
    }
    if(written_name + 2 >= sizeof(record))
        die_at(span, "the result types of %s are too long to combine", name);
    (void)is_public;
    (void)is_file_private;
    for(int i = 0; i < module->type_count; i++)
        if(!strcmp(module->types[i].name, record)) {
            if(!module->types[i].is_results)
                die_at(span, "several results need the type name %s, which is already declared", record);
            copy_text(ret, ret_size, record);
            free(parts);
            free(body);
            return count;
        }
    ZirType *type = ModuleAddType(module, record, span);
    if(type == NULL)
        die("out of memory declaring multiple results");
    type->is_public = 1;
    type->is_file_private = 0;
    type->is_results = 1;
    if(used >= sizeof(type->body))
        die_at(span, "multiple results exceed the size limit");
    copy_text(type->body, sizeof(type->body), body);
    copy_text(ret, ret_size, record);
    free(parts);
    free(body);
    return count;
}

/* `return a, b` in a procedure with several results returns its results
 * record. Returns 0 when TEXT is not such a return. */
int
lower_multiple_return(char *text, size_t size, const char *record, int count,
                      ZirSourceSpan span)
{
    const char *value = skip_ws(text + strlen("return"));
    char list[ZIR_TEXT_MAX];
    char (*parts)[ZIR_TEXT_MAX] = AllocateOrExit(16 * sizeof(*parts));
    copy_text(list, sizeof(list), value);
    size_t length = strlen(list);
    while(length > 0 && (list[length - 1] == ';' || isspace((unsigned char)list[length - 1])))
        list[--length] = '\0';
    int values = split_top_level(list, parts[0], 16, sizeof(parts[0]));
    free(parts);
    if(values < 2)
        return 0;
    if(values != count)
        die_at(span, "return gives %d values but the procedure has %d results", values, count);
    char lowered[ZIR_TEXT_MAX];
    if(snprintf(lowered, sizeof(lowered), "return %s.{%s};", record, list) >= (int)sizeof(lowered))
        die_at(span, "return statement exceeds the size limit");
    copy_text(text, size, lowered);
    return 1;
}

/* `a, b := F()` and `a, b = F()` bind the results of a procedure with
 * several results in order; `_` skips one. Fills TARGETS, OPERATOR, and
 * VALUE and returns the target count, or 0 when TEXT has another form. */
int
split_multiple_binding(const char *text, char targets[][ZIR_NAME_MAX], int max,
                       char *operator, char *value, size_t value_size)
{
    const char *cursor = skip_ws(text);
    int count = 0;
    for(;;) {
        const char *start = cursor;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        size_t length = (size_t)(cursor - start);
        if(length == 0 || length >= ZIR_NAME_MAX || count >= max ||
           isdigit((unsigned char)*start))
            return 0;
        memcpy(targets[count], start, length);
        targets[count][length] = '\0';
        count++;
        cursor = skip_ws(cursor);
        if(*cursor != ',')
            break;
        cursor = skip_ws(cursor + 1);
    }
    if(count < 2)
        return 0;
    if(cursor[0] == ':' && cursor[1] == '=') {
        strcpy(operator, ":=");
        cursor += 2;
    } else if(cursor[0] == '=' && cursor[1] != '=') {
        strcpy(operator, "=");
        cursor += 1;
    } else
        return 0;
    copy_text(value, value_size, skip_ws(cursor));
    size_t length = strlen(value);
    while(length > 0 && (value[length - 1] == ';' || isspace((unsigned char)value[length - 1])))
        value[--length] = '\0';
    return *value ? count : 0;
}

/* BuilderPrint(*builder, "x=%\n", x) appends formatted text the way print
 * writes it: each literal piece and each argument becomes an Append call from
 * std/format. Writes those statements to LINES and returns their count, or 0
 * when TEXT is not a BuilderPrint statement. */
int
expand_builder_print(const char *text, char (*lines)[ZIR_TEXT_MAX], int max,
                     ZirSourceSpan span)
{
    const char *cursor = skip_ws(text);
    if(strncmp(cursor, "BuilderPrint", 12) != 0)
        return 0;
    const char *open = skip_ws(cursor + 12);
    if(*open != '(')
        return 0;
    const char *close = closing_parenthesis(open);
    if(close == NULL)
        return 0;
    const char *rest = skip_ws(close + 1);
    if(*rest == ';') rest = skip_ws(rest + 1);
    if(*rest != '\0')
        return 0;
    char inner[ZIR_TEXT_MAX];
    size_t inner_length = (size_t)(close - open - 1);
    if(inner_length >= sizeof(inner))
        die_at(span, "BuilderPrint statement exceeds the size limit");
    memcpy(inner, open + 1, inner_length);
    inner[inner_length] = '\0';
    char (*parts)[ZIR_TEXT_MAX] = AllocateOrExit(64 * sizeof(*parts));
    int count = split_top_level(inner, parts[0], 64, sizeof(parts[0]));
    size_t format_length = count >= 2 ? strlen(parts[1]) : 0;
    if(count < 2 || format_length < 2 || parts[1][0] != '"' ||
       parts[1][format_length - 1] != '"')
        die_at(span, "BuilderPrint needs a builder and a literal format");
    int written = 0, argument = 2;
    char piece[ZIR_TEXT_MAX];
    size_t used = 0;
    for(size_t i = 1; i + 1 < format_length; i++) {
        char c = parts[1][i];
        if(c == '\\' && i + 2 < format_length) {
            if(used + 2 >= sizeof(piece)) die_at(span, "BuilderPrint format exceeds the size limit");
            piece[used++] = c;
            piece[used++] = parts[1][++i];
            continue;
        }
        if(c == '%' && parts[1][i + 1] == '%' && i + 2 < format_length) {
            if(used + 1 >= sizeof(piece)) die_at(span, "BuilderPrint format exceeds the size limit");
            piece[used++] = '%';
            i++;
            continue;
        }
        if(c != '%') {
            if(used + 1 >= sizeof(piece)) die_at(span, "BuilderPrint format exceeds the size limit");
            piece[used++] = c;
            continue;
        }
        if(argument >= count)
            die_at(span, "BuilderPrint format has more %% than arguments");
        if(written + 2 > max)
            die_at(span, "BuilderPrint has too many pieces");
        if(used > 0) {
            piece[used] = '\0';
            snprintf(lines[written++], ZIR_TEXT_MAX, "Append(%s, \"%s\");", parts[0], piece);
            used = 0;
        }
        snprintf(lines[written++], ZIR_TEXT_MAX, "Append(%s, %s);", parts[0], parts[argument++]);
    }
    if(argument != count)
        die_at(span, "BuilderPrint has more arguments than %% in its format");
    if(used > 0) {
        if(written + 1 > max)
            die_at(span, "BuilderPrint has too many pieces");
        piece[used] = '\0';
        snprintf(lines[written++], ZIR_TEXT_MAX, "Append(%s, \"%s\");", parts[0], piece);
    }
    free(parts);
    return written;
}
