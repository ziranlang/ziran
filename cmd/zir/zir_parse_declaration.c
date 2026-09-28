#include "zir_parse_internal.h"

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

int
lower_procedure_name_expression(char *part, size_t capacity,
                                const ZirFunction *function)
{
    ZirLexer lexer;
    char lowered[ZIR_TEXT_MAX];
    size_t copied = 0, used = 0;
    int changed = 0;
    LexerInit(&lexer, part, SpanPath(function->span));
    for(;;) {
        ZirToken token = LexerNext(&lexer);
        if(token.kind == ZIR_TOKEN_EOF) break;
        size_t start = lexer.pos - strlen(token.text);
        if(token.kind == ZIR_TOKEN_OPERATOR &&
           strcmp(token.text, "/") == 0 && part[start + 1] == '/')
            break;
        if(token.kind != ZIR_TOKEN_DIRECTIVE ||
           strcmp(token.text, "#procedure_name") != 0)
            continue;
        ZirToken open = LexerNext(&lexer);
        ZirToken close = LexerNext(&lexer);
        if(strcmp(open.text, "(") || strcmp(close.text, ")"))
            die_at(function->span,
                   "#procedure_name() requires empty parentheses");
        char literal[ZIR_NAME_MAX + 3];
        int written = snprintf(literal, sizeof(literal), "\"%s\"",
                               function->name);
        if(written < 0 || (size_t)written >= sizeof(literal) ||
           start < copied || used + start - copied + (size_t)written >=
           sizeof(lowered))
            die_at(function->span, "procedure name expression is too long");
        memcpy(lowered + used, part + copied, start - copied);
        used += start - copied;
        memcpy(lowered + used, literal, (size_t)written);
        used += (size_t)written;
        copied = lexer.pos;
        changed = 1;
    }
    if(!changed) return 0;
    size_t rest = strlen(part + copied);
    if(used + rest >= sizeof(lowered) || used + rest >= capacity)
        die_at(function->span, "procedure name expression is too long");
    memcpy(lowered + used, part + copied, rest + 1);
    copy_text(part, capacity, lowered);
    return 1;
}

static int
lower_template_record_default(ZirModule *module,
                              const ZirFunction *function, int parameter,
                              char *part, size_t capacity)
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
    char body[ZIR_TEXT_MAX];
    size_t body_length = (size_t)(closing - opening - 1);
    if(body_length >= sizeof(body))
        die_at(function->span, "default record initializer is too long");
    memcpy(body, opening + 1, body_length);
    body[body_length] = '\0';
    char (*fields)[ZIR_TEXT_MAX] = calloc(65, sizeof(*fields));
    if(fields == NULL)
        die("out of memory lowering default record initializer");
    int field_count = *skip_ws(body) ?
        split_top_level(body, fields[0], 65, sizeof(fields[0])) : 0;
    if(field_count > 64)
        die_at(function->span, "too many default record initializer fields");
    char normalized[ZIR_TEXT_MAX];
    int written = snprintf(normalized, sizeof(normalized), "%.*s.{",
                           (int)(assignment + 1 - part), part);
    if(written < 0 || (size_t)written >= sizeof(normalized))
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
            char return_text[ZIR_TEXT_MAX];
            written = snprintf(return_text, sizeof(return_text),
                               "return %s", field_value);
            if(written < 0 || (size_t)written >= sizeof(return_text))
                die_at(function->span, "default field expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name, "",
                                                    result_type, 0,
                                                    function->span);
            if(helper == NULL)
                die("out of memory creating default field helper");
            helper->is_public = function->is_public;
            helper->is_file_private = function->is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, return_text,
                               function->span) == NULL)
                die("out of memory creating default field helper body");
            char rewritten[ZIR_TEXT_MAX];
            written = field_assignment != NULL ?
                snprintf(rewritten, sizeof(rewritten), "%.*s %s()",
                         (int)(field_assignment + 1 - fields[field]),
                         fields[field], helper_name) :
                snprintf(rewritten, sizeof(rewritten), "%s()", helper_name);
            if(written < 0 || (size_t)written >= sizeof(rewritten))
                die_at(function->span, "default field initializer is too long");
            copy_text(fields[field], sizeof(fields[field]), rewritten);
        }
        written = snprintf(normalized + used, sizeof(normalized) - used,
                           "%s%s", field ? ", " : "", fields[field]);
        if(written < 0 || (size_t)written >= sizeof(normalized) - used)
            die_at(function->span, "default record initializer is too long");
        used += (size_t)written;
    }
    if(used + 2 > sizeof(normalized))
        die_at(function->span, "default record initializer is too long");
    normalized[used++] = '}';
    normalized[used] = '\0';
    copy_text(part, capacity, normalized);
    free(fields);
    return 1;
}

void
add_default_helpers(ZirProgram *program, ZirModule *module,
                    const char *source_path, const char *root,
                    ZirCompileImportResolver resolver, void *resolver_context)
{
    int declarations = module->function_count;
    int imports_resolved = 0;
    for(int fi = 0; fi < declarations; fi++) {
        const ZirFunction *function = &module->functions[fi];
        if(!function->default_args[0] || function->default_helpers_created)
            continue;
        char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
        char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
        if(parameters == NULL || defaults == NULL)
            die("out of memory creating default argument helpers");
        int count = split_top_level(function->args, parameters[0], 64,
                                    sizeof(parameters[0]));
        if(split_top_level(function->default_args, defaults[0], 64,
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
            char name[ZIR_NAME_MAX], value[ZIR_TEXT_MAX], type[ZIR_NAME_MAX];
            copy_text(name, sizeof(name), trim(parameters[i]));
            copy_text(value, sizeof(value), trim(assignment + 1));
            for(int imported = 0; imported < module->import_count; imported++) {
                ZirImport *import = &module->imports[imported];
                if(import->resolved_module != NULL ||
                   (import->kind != ZIR_IMPORT_OPEN &&
                    import->kind != ZIR_IMPORT_MODULE)) continue;
                for(int m = 0; m < program->module_count; m++)
                    if(&program->modules[m] != module &&
                       !strcmp(import->target, program->modules[m].name)) {
                        import->resolved_module = &program->modules[m];
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
            if(strcmp(value, "#caller_location") == 0) {
                copy_text(type, sizeof(type), "Source_Code_Location");
                type_known = 1;
            } else
                type_known = InferExpressionType(module, value,
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
                               "%s = %s", parameters[i], value);
            if(written < 0 || (size_t)written >= sizeof(defaults[i]))
                die_at(function->span, "default parameters exceed size limit");
            inferred = 1;
        }
        ZirFunction signature = module->functions[fi];
        for(int i = 0; i < count; i++)
            if((contextual[i] = lower_template_record_default(
                    module, &signature, i, defaults[i],
                    sizeof(defaults[i]))))
                inferred = 1;
        if(inferred) {
            char args[ZIR_TEXT_MAX] = "", full[ZIR_TEXT_MAX] = "";
            size_t args_used = 0, full_used = 0;
            for(int i = 0; i < count; i++) {
                int written = snprintf(args + args_used,
                                       sizeof(args) - args_used, "%s%s",
                                       i ? ", " : "", parameters[i]);
                if(written < 0 || (size_t)written >= sizeof(args) - args_used)
                    die_at(function->span,
                           "procedure parameters exceed size limit");
                args_used += (size_t)written;
                written = snprintf(full + full_used,
                                   sizeof(full) - full_used, "%s%s",
                                   i ? ", " : "", defaults[i]);
                if(written < 0 || (size_t)written >= sizeof(full) - full_used)
                    die_at(function->span,
                           "default parameters exceed size limit");
                full_used += (size_t)written;
            }
            copy_text(module->functions[fi].args,
                      sizeof(module->functions[fi].args), args);
            copy_text(module->functions[fi].default_args,
                      sizeof(module->functions[fi].default_args), full);
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
            if(function->is_template &&
               default_is_scope_independent(value, SpanPath(function->span)))
                continue;
            char *colon = strchr(parameters[i], ':');
            if(colon == NULL)
                die_at(function->span, "default parameter needs a type");
            char helper_name[ZIR_NAME_MAX];
            char result_type[ZIR_NAME_MAX];
            char body[ZIR_TEXT_MAX];
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
            int written = snprintf(body, sizeof(body), "return %s", value);
            if(written < 0 || (size_t)written >= sizeof(body))
                die_at(span, "default parameter expression is too long");
            ZirFunction *helper = ModuleAddFunction(module, helper_name,
                                                    "", result_type, 0, span);
            if(helper == NULL)
                die("out of memory creating default argument helper");
            helper->is_public = is_public;
            helper->is_file_private = is_file_private;
            if(FunctionAddStmt(helper, ZIR_STMT_RETURN, body, span) == NULL)
                die("out of memory creating default argument body");
        }
        module->functions[fi].default_helpers_created = 1;
        free(parameters);
        free(defaults);
    }
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
    } else if(parse_symbol_before_colons(body, name, sizeof(name)))
        kind = ZIR_IMPORT_MODULE;
    else {
        copy_text(name, sizeof(name), target);
        kind = ZIR_IMPORT_OPEN;
    }
    if(kind == ZIR_IMPORT_MODULE &&
       !isalpha((unsigned char)name[0]) && name[0] != '_')
        die_at(Span(path, line_no, 1),
               "import alias must start with a letter or underscore");
    if(target[0] == '\0' ||
       (!isalpha((unsigned char)target[0]) && target[0] != '_'))
        die_at(Span(path, line_no, 1),
               "Jai #import requires a module identifier; use #import, file for paths");
    for(const unsigned char *p = (const unsigned char *)target; *p; p++)
        if(!source_identifier_byte(*p))
            die_at(Span(path, line_no, 1),
                   "Jai #import requires a module identifier; use #import, file for paths");
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

int
parse_foreign_line(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count)
{
    char name[ZIR_NAME_MAX];
    char target[ZIR_PATH_MAX];
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
    if(*declaration != '(')
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
        if(*cursor != '"' || !is_c_ident(foreign_name))
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
    target[0] = '\0';
    symbol[0] = '\0';
    if(strcmp(library_target, "host_api") == 0) {
        if(strcmp(foreign_name, name) != 0)
            die_at(Span(path, line_no, 1),
                   "host capability cannot rename a #foreign symbol");
    } else if(strchr(library_target, '/') != NULL) {
        if(snprintf(target, sizeof(target), "%s.%s", library_target,
                    foreign_name) >= (int)sizeof(target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    } else {
        if(snprintf(target, sizeof(target), "c.%s", foreign_name) >=
           (int)sizeof(target))
            die_at(Span(path, line_no, 1), "#foreign target is too long");
    }
    extern_kind = classify_extern_target(target, symbol, sizeof(symbol),
                                         path, line_no);
    imp = ModuleAddImport(module, ZIR_IMPORT_EXTERN,
                             name, target[0] ? target : name, line, 1,
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
        snprintf(imp->extern_symbol, sizeof(imp->extern_symbol), "%s",
                 symbol);
        /* A trailing `..any` parameter marks a variadic C ABI: calls may
         * pass any number of extra arguments after the fixed ones. */
        {
            char parameters[8][ZIR_TEXT_MAX];
            int parameter_count = *skip_ws(imp->args) ?
                split_top_level(imp->args, parameters[0], 8,
                                sizeof(parameters[0])) : 0;
            if(parameter_count > 0) {
                const char *last = skip_ws(parameters[parameter_count - 1]);
                const char *colon = strchr(last, ':');
                const char *tail = colon != NULL ? skip_ws(colon + 1) : last;
                if(!strcmp(tail, "..any") || !strcmp(last, "..any"))
                    imp->is_varargs = 1;
            }
        }
    }
    return 1;
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

void
split_jai_control_line(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span)
{
    char *then = if_header_then(line);
    if(then != NULL) {
        const char *condition = starts_word(line, "if") ?
            skip_ws(line + 2) : skip_ws(skip_ws(line + 4) + 2);
        if(then == condition)
            die_at(span, "if then requires a condition");
        const char *after = skip_ws(then + 4);
        char body[SOURCE_LINE_MAX * 2];
        if(strlen(after) >= sizeof(body))
            die_at(span, "if body exceeds source limit");
        copy_text(body, sizeof(body), after);
        char *end = then;
        while(end > line && isspace((unsigned char)end[-1]))
            end--;
        *end = '\0';
        if(body[0] == '{') {
            size_t used = strlen(line);
            if(snprintf(line + used, capacity - used, " %s", body) >=
               (int)(capacity - used))
                die_at(span, "if header exceeds source limit");
        } else if(body[0] != '\0') {
            prepend_logical_line(queue, count, body, span);
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
