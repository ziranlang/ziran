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
    (void)path;
    return compiler_declaration_DefaultScopeIndependent(
        declaration_text(expression), ZIR_TEXT_MAX);
}

int
lower_procedure_name_expression(char *part, size_t capacity,
                                const ZirFunction *function)
{
    /* Nearly all statements lack the directive; avoid allocating for them. */
    if(strstr(part, "#procedure_name") == NULL) return 0;
    char *lowered = AllocateOrExit(capacity);
    TextRewrite result = compiler_declaration_RewriteProcedureName(
        declaration_text(part), declaration_text(function->name),
        (Slice){lowered, capacity > 0 ? (int64_t)capacity - 1 : 0});
    check_declaration_error(result.error, function->span);
    if(result.changed) {
        if((uint64_t)result.count >= capacity || result.count >= ZIR_TEXT_MAX)
            die_at(function->span, "procedure name expression is too long");
        lowered[result.count] = 0;
        copy_text(part, capacity, lowered);
    }
    free(lowered);
    return result.changed;
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
    size_t capacity = strlen(header) + 1;
    char *cleaned = AllocateOrExit(capacity);
    SymmetricRewrite result = compiler_declaration_RewriteSymmetric(
        declaration_text(header), (Slice){cleaned, (int64_t)capacity - 1},
        (Slice){wrapper, size > 0 ? (int64_t)size - 1 : 0}, ZIR_NAME_MAX);
    check_declaration_error(result.error, span);
    if((uint64_t)result.header_count >= capacity ||
       (result.needed && (uint64_t)result.count >= size))
        die_at(span, "#symmetric operator header exceeds source limit");
    cleaned[result.header_count] = 0;
    memcpy(header, cleaned, (size_t)result.header_count + 1);
    if(result.needed) wrapper[result.count] = 0;
    free(cleaned);
    return result.needed;
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
    if(count <= 0) return;
    NameReplacement *replacements = AllocateOrExit((size_t)count * sizeof(*replacements));
    for(int i = 0; i < count; i++) {
        replacements[i].source = declaration_text(names[i][0]);
        replacements[i].target = declaration_text(names[i][1]);
    }
    char *out = AllocateOrExit(capacity);
    TextRewrite result = compiler_declaration_RewriteLocalNames(
        declaration_text(line), (Slice){replacements, count},
        (Slice){out, capacity > 0 ? (int64_t)capacity - 1 : 0});
    check_declaration_error(result.error, span);
    if((uint64_t)result.count >= capacity)
        die_at(span, "source line exceeds size limit");
    out[result.count] = 0;
    memcpy(line, out, (size_t)result.count + 1);
    free(out);
    free(replacements);
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
    ResultDeclaration results = compiler_declaration_ParseResults(
        declaration_text(ret), declaration_text(template_parameters));
    check_declaration_error(results.error, span);
    if(!results.count) return 0;
    char record[ZIR_NAME_MAX], parameters[ZIR_NAME_MAX];
    int64_t record_size = compiler_declaration_ResultsRecord(
        results, (Slice){record, sizeof(record) - 1});
    if(record_size >= sizeof(record))
        die_at(span, "the result types of %s are too long to combine", name);
    record[record_size] = 0;
    int64_t parameter_size = compiler_declaration_ResultsParameters(
        results, (Slice){parameters, sizeof(parameters) - 1});
    if(parameter_size >= sizeof(parameters))
        die_at(span, "multiple result type parameters exceed the size limit");
    parameters[parameter_size] = 0;
    char *body = AllocateOrExit(ZIR_TEXT_MAX * 2);
    int64_t used = compiler_declaration_ResultsBody(
        results, (Slice){body, ZIR_TEXT_MAX * 2 - 1});
    if(used >= ZIR_TEXT_MAX * 2)
        die_at(span, "multiple results exceed the size limit");
    body[used] = 0;
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
    free(body);
    return results.count;
}

/* `return a, b` in a procedure with several results returns its results
 * record. Returns 0 when TEXT is not such a return. */
int
lower_multiple_return(char *text, size_t size, const char *record, int count,
                      ZirSourceSpan span)
{
    char *lowered = AllocateOrExit(ZIR_TEXT_MAX);
    ResultReturn result = compiler_declaration_RewriteResultReturn(
        declaration_text(text), declaration_text(record), count,
        (Slice){lowered, ZIR_TEXT_MAX - 1});
    if(result.error == DeclarationError_ReturnCount)
        die_at(span, "return gives %d values but the procedure has %d results",
               result.values, count);
    check_declaration_error(result.error, span);
    if(result.present) {
        if(result.count >= ZIR_TEXT_MAX || (uint64_t)result.count >= size)
            die_at(span, "return statement exceeds the size limit");
        lowered[result.count] = 0;
        copy_text(text, size, lowered);
    }
    free(lowered);
    return result.present;
}

/* `a, b := F()` and `a, b = F()` bind the results of a procedure with
 * several results in order; `_` skips one. Fills TARGETS, OPERATOR, and
 * VALUE and returns the target count, or 0 when TEXT has another form. */
int
split_multiple_binding(const char *text, char targets[][ZIR_NAME_MAX], int max,
                       char *operator, char *value, size_t value_size)
{
    ResultBinding binding = compiler_declaration_MultipleBinding(
        declaration_text(text), max, ZIR_NAME_MAX);
    if(!binding.present) return 0;
    if((uint64_t)binding.value.length >= value_size)
        return 0;
    for(int i = 0; i < binding.count; i++)
        copy_declaration_part(targets[i], ZIR_NAME_MAX, binding.targets[i], 0);
    copy_text(operator, 3, binding.inferred ? ":=" : "=");
    copy_declaration_part(value, value_size, binding.value, 0);
    return binding.count;
}

/* BuilderPrint(*builder, "x=%\n", x) appends formatted text the way print
 * writes it: each literal piece and each argument becomes an Append call from
 * std/format. Writes those statements to LINES and returns their count, or 0
 * when TEXT is not a BuilderPrint statement. */
int
expand_builder_print(const char *text, char (*lines)[ZIR_TEXT_MAX], int max,
                     ZirSourceSpan span)
{
    StatementOutput output[64];
    int slots = max < 64 ? max : 64;
    for(int i = 0; i < slots; i++)
        output[i].bytes = (Slice){lines[i], ZIR_TEXT_MAX - 1};
    PrintRewrite result = compiler_declaration_BuilderPrintPieces(
        declaration_text(text), (Slice){output, slots > 0 ? slots : 0}, max);
    check_declaration_error(result.error, span);
    for(int i = 0; i < result.statements; i++) {
        if(result.lengths[i] >= ZIR_TEXT_MAX)
            die_at(span, "BuilderPrint statement exceeds the size limit");
        lines[i][result.lengths[i]] = 0;
    }
    return result.statements;
}
