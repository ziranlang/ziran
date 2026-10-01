#include "zir_parse_internal.h"

#include "compiler_source.h"
#include "compiler_declaration.h"
#include "compiler_statement.h"

/* Ziran owns declaration text rules. This boundary retains the C parser's
 * output buffers and diagnostics until the declaration IR is migrated. */
static String
declaration_text(const char *source)
{
    return StringView(source != NULL ? source : "", source != NULL ? strlen(source) : 0);
}

static void
copy_declaration_part(char *output, size_t capacity, String part,
                      int normalize_type)
{
    compiler_source_CopyProcedureText(part, (Slice){output, (int64_t)capacity}, normalize_type);
}

void
parse_function_header(char *name, size_t name_size, char *args,
                      size_t args_size, char *ret, size_t ret_size,
                      const char *line)
{
    ProcedureHeader header = compiler_source_ProcedureHeaderParts(declaration_text(line));
    copy_declaration_part(name, name_size, header.name, 0);
    copy_declaration_part(args, args_size, header.parameters, 1);
    copy_declaration_part(ret, ret_size, header.result, 1);
}

int
function_must_use(const char *line, const char *return_type,
                  ZirSourceSpan span)
{
    MustUseModifier modifier = compiler_source_ProcedureMustUse(
        declaration_text(line), declaration_text(return_type));
    if(modifier == MustUseModifier_Arguments)
        die_at(span, "#must does not take arguments");
    if(modifier == MustUseModifier_Duplicate)
        die_at(span, "duplicate #must procedure modifier");
    if(modifier == MustUseModifier_NoResult)
        die_at(span, "#must requires a return value");
    return modifier == MustUseModifier_Required;
}

static void
check_declaration_error(DeclarationError error, ZirSourceSpan span)
{
    if(error != DeclarationError_None) {
        String message = compiler_declaration_ErrorText(error);
        die_at(span, "%.*s", (int)message.length, message.data);
    }
}

/* Grammar lives in compiler_declaration.zi; this boundary retains storage
 * and source diagnostics for the existing declaration IR. */
int
parse_using_modifiers(const char **cursor, char *filter, size_t filter_size,
                      const char *path, int line_no)
{
    UsingFilter result = compiler_declaration_UsingModifierClause(
        declaration_text(*cursor),
        (Slice){filter, filter_size > 0 ? (int64_t)filter_size - 1 : 0}, ZIR_NAME_MAX);
    check_declaration_error(result.error, Span(path, line_no, 1));
    if(!result.present) return 1;
    if((uint64_t)result.count >= filter_size)
        die_at(Span(path, line_no, 1), "using modifier list is too long");
    filter[result.count] = 0;
    *cursor += result.next;
    return 1;
}

/* Return 1 for a standalone directive and 2 for an inline declaration. */
int
strip_program_export(char *line, char *symbol, size_t symbol_size,
                     ZirSourceSpan span)
{
    ExportDirective directive = compiler_declaration_ProgramExport(
        declaration_text(line), (int64_t)symbol_size);
    check_declaration_error(directive.error, span);
    copy_declaration_part(symbol, symbol_size, directive.symbol, 0);
    if(directive.kind == 2)
        memmove(line, directive.body.data, (size_t)directive.body.length + 1);
    return directive.kind;
}

static ParameterRewrite
rewrite_parameters(const char *args, char *cleaned, int strip_defaults,
                   int strip_using, ZirSourceSpan span)
{
    ParameterRewrite result = compiler_declaration_RewriteParameters(
        declaration_text(args), (Slice){cleaned, ZIR_TEXT_MAX - 1},
        strip_defaults, strip_using, ZIR_NAME_MAX);
    check_declaration_error(result.error, span);
    if(result.count >= ZIR_TEXT_MAX)
        die_at(span, "procedure parameters exceed size limit");
    cleaned[result.count] = '\0';
    return result;
}

void
separate_parameter_defaults(char *args, size_t capacity,
                            char *defaults, size_t defaults_capacity,
                            ZirSourceSpan span)
{
    char cleaned[ZIR_TEXT_MAX];
    ParameterRewrite result = rewrite_parameters(args, cleaned, 1, 0, span);
    if(result.has_defaults) {
        copy_text(defaults, defaults_capacity, args);
        copy_text(args, capacity, cleaned);
    }
}

uint64_t
strip_using_parameters(char *args, size_t capacity, ZirSourceSpan span)
{
    char cleaned[ZIR_TEXT_MAX];
    ParameterRewrite result = rewrite_parameters(args, cleaned, 0, 1, span);
    copy_text(args, capacity, cleaned);
    return result.using_parameters;
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
    ModuleImport declaration = compiler_declaration_ImportDeclaration(
        declaration_text(line), ZIR_NAME_MAX, SOURCE_PATH_MAX);
    if(!declaration.present)
        return 0;
    ZirSourceSpan span = Span(path, line_no, 1);
    if(declaration.error == DeclarationError_ImportMode) {
        String message = compiler_declaration_ErrorText(declaration.error);
        die_at(span, "%.*s: %.*s", (int)message.length, message.data,
               (int)declaration.detail.length, declaration.detail.data);
    }
    check_declaration_error(declaration.error, span);
    char name[ZIR_NAME_MAX], target[SOURCE_PATH_MAX], signature[ZIR_TEXT_MAX] = "";
    copy_declaration_part(name, sizeof(name), declaration.name, 0);
    copy_declaration_part(target, sizeof(target), declaration.target, 0);
    if(declaration.mode != ImportMode_Module)
        snprintf(signature, sizeof(signature), "%s:%.*s",
                 declaration.mode == ImportMode_Directory ? "dir" : "file",
                 (int)declaration.path.length, declaration.path.data);
    ZirImportKind kind = declaration.named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN;
    ZirImport *imported = ModuleAddImport(module, kind, name, target,
                                         signature, scope_public, span);
    if(imported != NULL) {
        imported->is_public = scope_public;
        imported->is_using = declaration.using_import;
    }
    return 1;
}

int
parse_foreign_library_line(const char *path, int line_no, const char *line,
                           char names[][ZIR_NAME_MAX],
                           char targets[][ZIR_PATH_MAX], int *count)
{
    LibraryDeclaration declaration = compiler_declaration_SystemLibrary(
        declaration_text(line), ZIR_NAME_MAX, ZIR_PATH_MAX);
    if(!declaration.present)
        return 0;
    ZirSourceSpan span = Span(path, line_no, 1);
    check_declaration_error(declaration.error, span);
    char name[ZIR_NAME_MAX];
    copy_declaration_part(name, sizeof(name), declaration.name, 0);
    for(int i = 0; i < *count; i++)
        if(strcmp(names[i], name) == 0)
            die_at(span, "duplicate #system_library name: %s", name);
    if(*count >= 32)
        die_at(span, "too many #system_library declarations");
    copy_text(names[*count], ZIR_NAME_MAX, name);
    copy_declaration_part(targets[*count], ZIR_PATH_MAX, declaration.target, 0);
    (*count)++;
    return 1;
}
/* Foreign grammar and target spelling are maintained in Ziran. This
 * boundary resolves library visibility and stores the existing C IR. */
int
parse_foreign_line(ZirModule *module, const char *path, int line_no,
                   const char *line, int scope_public,
                   char names[][ZIR_NAME_MAX],
                   char targets[][ZIR_PATH_MAX],
                   char paths[][SOURCE_PATH_MAX],
                   const int *file_private, int count)
{
    ZirSourceSpan span = Span(path, line_no, 1);
    ForeignDeclaration declaration = compiler_declaration_ParseForeign(
        declaration_text(line), ZIR_NAME_MAX);
    check_declaration_error(declaration.error, span);
    if(!declaration.present) return 0;
    const char *library_target = NULL;
    for(int i = 0; i < count; i++)
        if(StringEqual(declaration.library, declaration_text(names[i])) &&
           (!file_private[i] || strcmp(paths[i], path) == 0)) {
            library_target = targets[i];
            break;
        }
    if(library_target == NULL)
        die_at(span, "#foreign library is not declared: %.*s",
               (int)declaration.library.length, declaration.library.data);
    char target[ZIR_PATH_MAX], name[ZIR_NAME_MAX], symbol[ZIR_NAME_MAX];
    ForeignTarget resolved = compiler_declaration_ResolveForeignTarget(
        declaration, declaration_text(library_target),
        (Slice){target, sizeof(target) - 1}, ZIR_NAME_MAX);
    check_declaration_error(resolved.error, span);
    if((uint64_t)resolved.count >= sizeof(target))
        die_at(span, "#foreign target is too long");
    target[resolved.count] = 0;
    copy_declaration_part(name, sizeof(name), declaration.name, 0);
    ZirExternKind extern_kind = classify_extern_target(target, symbol,
        sizeof(symbol), path, line_no);
    if(declaration.is_type) {
        if(!GoForeignTargetValid(target) && !PyForeignTargetValid(target))
            die_at(span, "foreign types require an explicit go: or py: #system_library");
        ZirType *type = ModuleAddType(module, name, span);
        if(type == NULL) die("out of memory declaring foreign type");
        type->is_extern = 1;
        type->is_public = scope_public;
        copy_text(type->foreign_target, sizeof(type->foreign_target), target);
        return 1;
    }
    ZirImport *imp = ModuleAddImport(module, ZIR_IMPORT_EXTERN, name,
        target[0] ? target : name, line, 1, span);
    if(imp != NULL) {
        char parsed_name[ZIR_NAME_MAX];
        imp->is_public = scope_public;
        parse_function_header(parsed_name, sizeof(parsed_name), imp->args,
                              sizeof(imp->args), imp->return_type,
                              sizeof(imp->return_type), line);
        imp->must_use = function_must_use(line, imp->return_type, imp->span);
        imp->extern_kind = extern_kind;
        imp->go_results = declaration.go_results;
        imp->go_field = declaration.go_field;
        imp->go_defer = declaration.go_defer;
        imp->go_variadic = declaration.go_variadic;
        imp->py_results = declaration.py_results;
        imp->py_field = declaration.py_field;
        copy_text(imp->extern_symbol, sizeof(imp->extern_symbol), symbol);
        imp->is_varargs = compiler_declaration_ForeignVarargs(declaration_text(imp->args));
    }
    return 1;
}

int
brace_outside_literals(const char *text)
{
    return compiler_source_BraceOutsideLiterals(declaration_text(text));
}

int
looks_like_function_header(const char *line)
{
    return compiler_source_LooksLikeProcedureHeader(declaration_text(line));
}

int
split_oneline_function(const char *line, char *head, size_t head_size,
                       char *body, size_t body_size)
{
    String source = declaration_text(line);
    ControlBlock block = compiler_source_SplitProcedureBody(source);
    if(!block.valid) return 0;
    copy_declaration_part(head, head_size, StringView(line, block.head_end), 0);
    copy_declaration_part(body, body_size,
        StringView(line + block.body_begin, block.body_end - block.body_begin), 0);
    if(body_size != 0) trim_in_place(body);
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

/* Ziran returns borrowed header/body ranges; the boundary only copies them
 * into the parser's mutable line and logical-line queue. */
void
split_jai_control_line(char *line, size_t capacity,
                       char queue[16][SOURCE_LINE_MAX * 2], int *count,
                       ZirSourceSpan span)
{
    ControlLine result = compiler_statement_SplitControlLine(declaration_text(line));
    if(result.error == ControlError_Condition)
        die_at(span, "if then requires a condition");
    if(!result.inline_body.length && !result.queued_body.length &&
       result.header.length == (int64_t)strlen(line))
        return;
    if(result.inline_body.length >= SOURCE_LINE_MAX * 2 ||
       result.queued_body.length >= SOURCE_LINE_MAX * 2)
        die_at(span, "if body exceeds source limit");
    char *header = AllocateOrExit(capacity);
    int written = snprintf(header, capacity, "%.*s%s%.*s",
        (int)result.header.length, result.header.data,
        result.inline_body.length ? " " : "",
        (int)result.inline_body.length,
        result.inline_body.data ? result.inline_body.data : "");
    if(written < 0 || (size_t)written >= capacity)
        die_at(span, "if header exceeds source limit");
    if(result.queued_body.length) {
        size_t body_size = (size_t)result.queued_body.length + 1;
        char *body = AllocateOrExit(body_size);
        copy_declaration_part(body, body_size, result.queued_body, 0);
        prepend_logical_line(queue, count, body, span);
        free(body);
    }
    copy_text(line, capacity, header);
    free(header);
}

int
line_is_compile_else(const char *line)
{
    return compiler_statement_CompileElse(declaration_text(line));
}

int
parse_cond_start(char *line, char **condition)
{
    CompileHeader header = compiler_statement_CompileCondition(declaration_text(line));
    if(!header.kind) return 0;
    *condition = (char *)header.condition.data;
    (*condition)[header.condition.length] = 0;
    return header.kind;
}

int
line_starts_compile_condition(const char *line)
{
    return compiler_statement_StartsCompileCondition(declaration_text(line));
}

/* A procedure with several results, `-> s32, s32` or
 * `-> (quotient: s32, remainder: s32)`, returns a generated record whose
 * fields value_0, value_1, ... hold the results in order. Replaces RET
 * with the record's name and returns the result count, or 0 for one result. */
int
declare_multiple_results(ZirModule *module, const char *name, char *ret,
                         size_t ret_size, int is_public, int is_file_private,
                         const char *template_parameters, ZirSourceSpan span)
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
    char parameters[ZIR_NAME_MAX] = "";
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
        /* Only parameters used by the results belong to the record template.
         * Keep their order of appearance so equal result shapes share it even
         * when procedures bind their type parameters in a different order. */
        for(const char *p = type; *p;) {
            if(!isalpha((unsigned char)*p) && *p != '_') { p++; continue; }
            const char *start = p++;
            while(isalnum((unsigned char)*p) || *p == '_') p++;
            size_t length = (size_t)(p - start);
            if(TemplateParameterIndex(template_parameters, start, length) < 0 ||
               TemplateParameterIndex(parameters, start, length) >= 0)
                continue;
            size_t offset = strlen(parameters);
            int added = snprintf(parameters + offset, sizeof(parameters) - offset,
                                 "%s%.*s", offset ? "," : "", (int)length, start);
            if(added < 0 || (size_t)added >= sizeof(parameters) - offset)
                die_at(span, "multiple result type parameters exceed the size limit");
        }
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
    ZirType *type = NULL;
    for(int i = 0; i < module->type_count; i++)
        if(!strcmp(module->types[i].name, record)) {
            type = &module->types[i];
            if(!type->is_results || strcmp(type->template_params, parameters))
                die_at(span, "several results need the type name %s, which is already declared", record);
            break;
        }
    if(type == NULL) {
        type = ModuleAddType(module, record, span);
        if(type == NULL)
            die("out of memory declaring multiple results");
        type->is_public = 1;
        type->is_file_private = 0;
        type->is_results = 1;
        type->is_record_template = parameters[0] != '\0';
        copy_text(type->template_params, sizeof(type->template_params), parameters);
        if(used >= sizeof(type->body))
            die_at(span, "multiple results exceed the size limit");
        copy_text(type->body, sizeof(type->body), body);
    }
    int result_length = parameters[0] ?
        snprintf(ret, ret_size, "%s(%s)", record, parameters) :
        snprintf(ret, ret_size, "%s", record);
    if(result_length < 0 || (size_t)result_length >= ret_size)
        die_at(span, "the result types of %s are too long to combine", name);
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
