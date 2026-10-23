#include "zir_scratch.h"
#include "zir_rust_internal.h"

/* Buffers validate_module keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ValidateModuleBuffers {
    RustEmitter emitter;
    const ZirParameters *parameters;
} ValidateModuleBuffers;

void validate_module(const ZirModule *module);

static void
validate_module_with_buffers(const ZirModule *module, ValidateModuleBuffers *buffers)
{
    memset(&buffers->emitter, 0, sizeof(buffers->emitter));
    buffers->emitter.module = module;
    for(int index = 0; index < module->global_count; index++) {
        const ZirGlobal *global = &module->globals[index];
        char type_name[ZIR_NAME_MAX];
        require_rust_type(&buffers->emitter, global->span, global->type, type_name,
                          sizeof(type_name));
    }
    for(int index = 0; index < module->type_count; index++) {
        const ZirType *record = &module->types[index];
        const ZirModule *owner = NULL;
        const ZirType *checked = NULL;
        char checked_type[ZIR_NAME_MAX];
        /* A record template has no layout; its specializations do. */
        if(record->is_record_template)
            continue;
        if(rust_owned_vec_type(&buffers->emitter, record->name, NULL, NULL,
                               checked_type, sizeof(checked_type)) ||
           rust_option_type(&buffers->emitter, record->name, NULL, NULL, checked_type,
                            sizeof(checked_type))) {
            if(!rust_type(&buffers->emitter, checked_type, checked_type,
                          sizeof(checked_type))) {
                Diagnostic(record->span, "zir_rust.type",
                           "unsupported template type: %s", record->name);
                exit(1);
            }
            continue;
        }
        if(rust_enum_type(&buffers->emitter, record->name, &owner, &checked)) {
            if(rust_scalar_type(checked->enum_backing) == NULL) {
                Diagnostic(record->span, "zir_rust.enum",
                           "invalid enum backing type: %s",
                           checked->enum_backing);
                exit(1);
            }
            continue;
        }
        if(FindType(module, record->name, &owner) != NULL &&
           (checked = FindType(module, record->name, &owner)) != NULL &&
           checked->is_procedure_type) {
            char checked_return[ZIR_NAME_MAX];
            if(!rust_type(&buffers->emitter, checked->procedure_return_type,
                          checked_return, sizeof(checked_return))) {
                Diagnostic(record->span, "zir_rust.type",
                           "unsupported procedure type: %s", record->name);
                exit(1);
            }
            continue;
        }
        /* A record template has no layout; its specializations do. */
        if(record->is_record_template)
            continue;
        if(!rust_record_type(&buffers->emitter, record->name, &owner, &checked)) {
            Diagnostic(record->span, "zir_rust.type",
                       "the Rust target cannot lower this record: %s",
                       record->name);
            exit(1);
        }
        size_t offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &offset, &field) == 1)
            if(!rust_type(&buffers->emitter, field.type, checked_type,
                          sizeof(checked_type))) {
                Diagnostic(record->span, "zir_rust.type",
                           "the Rust target cannot lower this record: %s",
                           record->name);
                exit(1);
            }
    }
    for(int index = 0; index < module->import_count; index++) {
        const ZirImport *import = &module->imports[index];
        char checked_type[ZIR_NAME_MAX];
        if(import->kind != ZIR_IMPORT_EXTERN)
            continue;
        if(import->extern_kind != ZIR_EXTERN_C && import->extern_kind != ZIR_EXTERN_HOST) {
            Diagnostic(import->span, "zir_rust.import",
                       "only the C foreign ABI is supported by the Rust target: %s",
                       import->name);
            exit(1);
        }
        if(*import->args) {
            int count = (buffers->parameters = ParametersOf(import->args))->count;
            if(count < 0) {
                Diagnostic(import->span, "zir_rust.import",
                           "too many foreign parameters: %s", import->name);
                exit(1);
            }
            for(int index = 0; index < count; index++) {
                const ZirParameter *parameter = &buffers->parameters->items[index];
                const char *type = parameter->value_type ? parameter->value_type : parameter->text;
                while(*type == ' ' || *type == '\t')
                    type++;
                if(import->is_varargs && !strncmp(type, "..", 2))
                    continue;
                if(parameter->value_type == NULL ||
                   !rust_type(&buffers->emitter, type, checked_type,
                              sizeof(checked_type))) {
                    Diagnostic(import->span, "zir_rust.import",
                               "unsupported foreign parameter type in %s",
                               import->name);
                    exit(1);
                }
            }
        }
        if(!rust_type(&buffers->emitter, import->return_type, checked_type,
                      sizeof(checked_type))) {
            Diagnostic(import->span, "zir_rust.import",
                       "unsupported foreign return type: %s", import->name);
            exit(1);
        }
    }
}

COMPILER_SCRATCH_VOID(, validate_module,
    (const ZirModule *module),
    ValidateModuleBuffers, POOLED, free,
    validate_module_with_buffers(module, buffers))
/* Buffers emit_slot_wrappers keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSlotWrappersBuffers {
    const ZirParameters *parameters;
    char wrapper[ZIR_RUST_NAME_MAX * 2];
    char symbol[ZIR_RUST_NAME_MAX * 2];
} EmitSlotWrappersBuffers;

static void emit_slot_wrappers(RustEmitter *emitter, const ZirModule *module,
                               const ZirFunction *function);

static void
emit_slot_wrappers_with_buffers(RustEmitter *emitter, const ZirModule *module,
                               const ZirFunction *function, EmitSlotWrappersBuffers *buffers)
{
    emitter->module = module;
    emitter->function = function;
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *value = &function->exprs[index];
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = FindType(module, value->type, &slot_owner);
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        int count;
        if(!value->is_function_value || slot == NULL ||
           !slot->is_procedure_type || slot->is_c_call)
            continue;
        count = (buffers->parameters = ParametersOf(slot->body))->count;
        if(count < 0) {
            Diagnostic(value->span, "zir_rust.slot",
                       "too many procedure parameters: %s", value->type);
            exit(1);
        }
        if(!strcmp(value->name, "#this")) {
            owner = module;
            callee = function;
        } else if(ResolveFunctionAt(module, value->name, SpanPath(value->span),
                                    &owner, &callee) != 1 ||
                  owner == NULL || callee == NULL) {
            Diagnostic(value->span, "zir_rust.slot",
                       "unsupported procedure value: %s", value->name);
            exit(1);
        }
        slot_wrapper_symbol(emitter, index, buffers->wrapper, sizeof(buffers->wrapper));
        function_symbol(emitter, owner, callee, buffers->symbol, sizeof(buffers->symbol));
        fprintf(emitter->output,
                "unsafe extern \"C\" fn %s(_context: *mut core::ffi::c_void",
                buffers->wrapper);
        emitter->module = slot_owner;
        for(int argument = 0; argument < count; argument++) {
            const ZirParameter *parameter = &buffers->parameters->items[argument];
            char parameter_type[ZIR_NAME_MAX];
            const const char *type;
            if(parameter->value_type == NULL) {
                Diagnostic(value->span, "zir_rust.slot",
                           "invalid procedure parameter: %s",
                           buffers->parameters->items[argument].text);
                exit(1);
            }
            type = parameter->value_type;
            require_rust_type(emitter, value->span, type, parameter_type,
                              sizeof(parameter_type));
            fprintf(emitter->output, ", ziran_slot_arg_%d: %s", argument,
                    parameter_type);
        }
        if(strcmp(slot->procedure_return_type, "void") != 0) {
            char return_type[ZIR_NAME_MAX];
            require_rust_type(emitter, value->span,
                              slot->procedure_return_type, return_type,
                              sizeof(return_type));
            fprintf(emitter->output, ") -> %s {\n", return_type);
        } else {
            fprintf(emitter->output, ") {\n");
        }
        emitter->module = module;
        fprintf(emitter->output, "    unsafe { ");
        if(strcmp(slot->procedure_return_type, "void") != 0)
            fputc('(', emitter->output);
        fputs(buffers->symbol, emitter->output);
        for(int argument = 0; argument < count; argument++)
            fprintf(emitter->output, "%sziran_slot_arg_%d",
                    argument ? ", " : "(", argument);
        if(count == 0)
            fputs("(", emitter->output);
        fputs(")", emitter->output);
        if(strcmp(slot->procedure_return_type, "void") != 0)
            fputc(')', emitter->output);
        fputs(" }\n}\n\n", emitter->output);
    }
}

COMPILER_SCRATCH_VOID(static, emit_slot_wrappers,
    (RustEmitter *emitter, const ZirModule *module,
                               const ZirFunction *function),
    EmitSlotWrappersBuffers, POOLED, free,
    emit_slot_wrappers_with_buffers(emitter, module, function, buffers))

/* Whether any program takes this function as a procedure value. */
static int rust_function_is_value(RustEmitter *emitter, const ZirModule *module,
                                  const ZirFunction *function)
{
    for(int p = 0; p < emitter->program_count; p++)
        for(int m = 0; m < emitter->programs[p]->module_count; m++) {
            const ZirModule *scope = &emitter->programs[p]->modules[m];
            for(int f = 0; f < scope->function_count; f++) {
                const ZirFunction *user = &scope->functions[f];
                for(int e = 0; e < user->expr_count; e++) {
                    const ZirExpr *expression = &user->exprs[e];
                    const ZirModule *owner = NULL;
                    const ZirFunction *target = NULL;
                    if(!expression->is_function_value ||
                       strcmp(expression->name, function->name) != 0)
                        continue;
                    if(ResolveFunctionAt(scope, expression->name, SpanPath(user->span),
                                         &owner, &target) == 1 &&
                       owner == module && target == function)
                        return 1;
                }
            }
        }
    return 0;
}
/* Buffers lower_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerFunctionBuffers {
    const ZirParameters *parameters;
    char symbol[ZIR_RUST_NAME_MAX * 2];
} LowerFunctionBuffers;

void lower_function(RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function);

static void
lower_function_with_buffers(RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function, LowerFunctionBuffers *buffers)
{
    int count;
    /* Templates have no body of their own, only checked specializations,
     * and foreign declarations are emitted with the imports. */
    if(function->is_extern || function->is_template)
        return;
    emitter->module = module;
    emitter->function = function;
    emitter->local_count = 0;
    function_symbol(emitter, module, function, buffers->symbol, sizeof(buffers->symbol));
    /* Only what C can see or call keeps the C ABI: exported functions and
     * functions taken as procedure values. Others are ordinary Rust fns. */
    if(function->export_symbol[0] ||
       (function->exported && strcmp(function->name, "main") != 0))
        fputs("#[no_mangle]\npub extern \"C\" fn ", emitter->output);
    else if(rust_function_is_value(emitter, module, function))
        fputs("extern \"C\" fn ", emitter->output);
    else
        fputs("fn ", emitter->output);
    fputs(buffers->symbol, emitter->output);
    fputc('(', emitter->output);
    count = (buffers->parameters = ParametersOf(FunctionArgs(function)))->count;
    if(count < 0) {
        Diagnostic(function->span, "zir_rust.parameter",
                   "too many parameters in the Rust target: %s",
                   function->name);
        exit(1);
    }
    for(int index = 0; index < count; index++) {
        const ZirParameter *parameter = &buffers->parameters->items[index];
        char source_name[ZIR_NAME_MAX];
        char rust_name[ZIR_NAME_MAX];
        const char *type;
        if(parameter->value_type == NULL) {
            Diagnostic(function->span, "zir_rust.parameter",
                       "invalid parameter declaration: %s", buffers->parameters->items[index].text);
            exit(1);
        }
        copy_text(source_name, sizeof(source_name), parameter->binding_name);
        rust_identifier(source_name, rust_name, sizeof(rust_name));
        type = parameter->value_type;
        char parameter_type[ZIR_NAME_MAX];
        require_rust_type(emitter, function->span, type, parameter_type,
                          sizeof(parameter_type));
        fprintf(emitter->output, "%s%s%s: %s", index ? ", " : "",
                rust_name_mutable(function, source_name, type, -1) ? "mut " : "",
                rust_name, parameter_type);
        register_local(emitter, source_name);
    }
    fputc(')', emitter->output);
    {
        char return_type[ZIR_NAME_MAX];
        require_rust_type(emitter, function->span, function->return_type,
                          return_type, sizeof(return_type));
        if(*return_type)
            fprintf(emitter->output, " -> %s", return_type);
    }
    fputs(" {\n", emitter->output);
    {
        /* The body is written inside unsafe first; the block stays only
         * when the body needs it, otherwise the body moves out a level. */
        FILE *function_output = emitter->output;
        char *body_text = NULL;
        size_t body_size = 0;
        TextStream stream;
        FILE *body = TextStreamOpen(&stream, &body_text, &body_size);
        if(body == NULL) {
            Diagnostic(function->span, "zir_rust.output", "cannot buffer Rust output");
            exit(1);
        }
        emitter->output = body;
        emitter->indent = 2;
        emit_slot_wrappers(emitter, module, function);
        emit_rust_assembly(emitter, -1);
        emit_sequence(emitter, 0, function->stmt_count);
        if(TextStreamClose(&stream) != 0) {
            Diagnostic(function->span, "zir_rust.output", "cannot finish buffered Rust output");
            exit(1);
        }
        emitter->output = function_output;
        if(rust_function_needs_unsafe(module, function) ||
           rust_text_needs_unsafe(body_text, body_size)) {
            fputs("    unsafe {\n", function_output);
            fwrite(body_text, 1, body_size, function_output);
            fputs("    }\n", function_output);
        } else {
            for(const char *line = body_text; line < body_text + body_size;) {
                const char *next = memchr(line, '\n', (size_t)(body_text + body_size - line));
                size_t length = next ? (size_t)(next - line) + 1 :
                                (size_t)(body_text + body_size - line);
                if(length > 4 && !strncmp(line, "    ", 4)) {
                    line += 4;
                    length -= 4;
                }
                fwrite(line, 1, length, function_output);
                line += length;
            }
        }
        free(body_text);
    }
    emitter->indent = 0;
    fputs("}\n\n", emitter->output);
}

COMPILER_SCRATCH_VOID(, lower_function,
    (RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function),
    LowerFunctionBuffers, POOLED, free,
    lower_function_with_buffers(emitter, module, function, buffers))

/* A parsed initializer names its record but not every node's type; the
 * declared type supplies each field's and element's type. */
/* A type named in another module, spelled so the reading module finds it:
 * through the import alias when needed, like Lib.Point. */
static void rust_spelled_from(const ZirModule *reader, const ZirModule *owner,
                              const char *type, char *output, size_t size)
{
    snprintf(output, size, "%s", type);
    if(reader == NULL || owner == NULL || reader == owner ||
       FindType(reader, type, NULL) != NULL)
        return;
    for(int index = 0; index < reader->import_count; index++)
        if(reader->imports[index].resolved_module == owner &&
           reader->imports[index].name[0]) {
            snprintf(output, size, "%s.%s", reader->imports[index].name, type);
            return;
        }
}

static void rust_fill_literal_types(RustEmitter *emitter, ZirFunction *literal,
                                    int index, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    int position = 0;
    if(index < 0 || index >= literal->expr_count || depth > 32)
        return;
    ZirExpr *node = &literal->exprs[index];
    if(node->type[0] == '\0' || !strcmp(node->type, "integer") ||
       !strcmp(node->type, "real") || node->kind == ZIR_EXPR_COMPOUND)
        node->type = KeepNameFormat("%s", type);
    /* A negative number, -0.5 in a [4]float32 literal, is a sign applied
     * to a literal; the literal takes the element's type too. */
    if(node->kind == ZIR_EXPR_UNARY && !strcmp(node->op, "-") &&
       !strcmp(node->type, type)) {
        rust_fill_literal_types(emitter, literal, node->right, type, depth + 1);
        return;
    }
    if(node->kind != ZIR_EXPR_COMPOUND)
        return;
    int array = ArrayElementType(type, element, sizeof(element), NULL);
    if(!array)
        rust_record_type(emitter, type, &owner, &record);
    for(int child = node->first_child; child >= 0;
        child = literal->exprs[child].next_sibling, position++) {
        ZirExpr *entry = &literal->exprs[child];
        char field_type[ZIR_NAME_MAX] = "";
        if(array)
            snprintf(field_type, sizeof(field_type), "%s", element);
        else if(record != NULL) {
            size_t offset = 0;
            int ordinal = 0;
            ZirTypeField field;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(entry->name[0] ? !strcmp(field.name, entry->name) : ordinal == position) {
                    snprintf(field_type, sizeof(field_type), "%s", field.type);
                    if(!entry->name[0])
                        entry->name = KeepNameFormat("%s", field.name);
                    break;
                }
                ordinal++;
            }
        }
        if(!field_type[0])
            continue;
        if(!array) {
            char spelled[ZIR_NAME_MAX];
            rust_spelled_from(emitter->module, owner, field_type, spelled, sizeof(spelled));
            snprintf(field_type, sizeof(field_type), "%s", spelled);
        }
        if(entry->kind == ZIR_EXPR_FIELD_INIT) {
            entry->type = KeepNameFormat("%s", field_type);
            rust_fill_literal_types(emitter, literal, entry->right, field_type, depth + 1);
        } else
            rust_fill_literal_types(emitter, literal, child, field_type, depth + 1);
    }
}
/* Buffers rust_global_initializer keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RustGlobalInitializerBuffers {
    ZirFunction probe;
} RustGlobalInitializerBuffers;

static void rust_global_initializer(RustEmitter *emitter,
                                     const ZirGlobal *global, char *output,
                                     size_t size);

static void
rust_global_initializer_with_buffers(RustEmitter *emitter,
                                     const ZirGlobal *global, char *output,
                                     size_t size, RustGlobalInitializerBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirType *enumeration = NULL;
    if(!strcmp(global->type, "string") &&
       rust_string_literal(global->init, output, size))
        return;
    if(rust_enum_type(emitter, global->type, &owner, &enumeration) &&
       global->init[0] != '\0') {
        /* Type.Member or .Member names an enum value; others are numbers. */
        const char *member = strrchr(global->init, '.');
        int64_t value;
        if(member != NULL && EnumMemberValue(enumeration, member + 1, &value)) {
            snprintf(output, size, "%lld", (long long)value);
            return;
        }
    }
    if(global->init[0] != '\0' && strchr(global->init, '(') == NULL &&
       (rust_scalar_type(global->type) != NULL ||
        (rust_enum_type(emitter, global->type, &owner, &enumeration) &&
         strchr(global->init, '.') == NULL))) {
        snprintf(output, size, "%s", global->init);
        return;
    }
    if(global->init[0] != '\0') {
        /* A constant record or array literal lowers like any checked
         * expression; Rust accepts it as a static initializer. */
        memset(&buffers->probe, 0, sizeof(buffers->probe));
        int root = ParseExprTyped(&buffers->probe, emitter->module, global->init,
                                  global->span, global->type);
        /* Only literals of constants; a literal naming a procedure or
         * global is set up by module startup. */
        int constant = root >= 0;
        for(int index = 0; index < buffers->probe.expr_count && constant; index++)
            constant = buffers->probe.exprs[index].kind != ZIR_EXPR_IDENT &&
                       buffers->probe.exprs[index].kind != ZIR_EXPR_CALL;
        if(constant && buffers->probe.exprs[root].kind == ZIR_EXPR_COMPOUND) {
            const ZirFunction *saved = emitter->function;
            rust_fill_literal_types(emitter, &buffers->probe, root, global->type, 0);
            emitter->function = &buffers->probe;
            emit_typed_expression(emitter, root, global->type, output, size);
            emitter->function = saved;
            free(buffers->probe.exprs);
            return;
        }
        free(buffers->probe.exprs);
    }
    rust_zero_value(emitter, global->type, output, size);
}

COMPILER_SCRATCH_VOID(static, rust_global_initializer,
    (RustEmitter *emitter,
                                     const ZirGlobal *global, char *output,
                                     size_t size),
    RustGlobalInitializerBuffers, POOLED, free,
    rust_global_initializer_with_buffers(emitter, global, output, size, buffers))
/* Buffers emit_global_definitions keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitGlobalDefinitionsBuffers {
    char symbol[ZIR_RUST_NAME_MAX * 2];
    char zero[ZIR_RUST_TEXT_MAX];
} EmitGlobalDefinitionsBuffers;

void emit_global_definitions(RustEmitter *emitter, FILE *output);

static void
emit_global_definitions_with_buffers(RustEmitter *emitter, FILE *output, EmitGlobalDefinitionsBuffers *buffers)
{
    for(int program_index = 0; program_index < emitter->program_count;
        program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
            module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int global_index = 0; global_index < module->global_count;
                global_index++) {
                const ZirGlobal *global = &module->globals[global_index];
                char type_name[ZIR_NAME_MAX];
                global_symbol(emitter, module, global, buffers->symbol,
                              sizeof(buffers->symbol));
                require_rust_type(emitter, global->span, global->type,
                                  type_name, sizeof(type_name));
                rust_global_initializer(emitter, global, buffers->zero, sizeof(buffers->zero));
                fprintf(output, "static mut %s: %s = %s;\n\n", buffers->symbol,
                        type_name, buffers->zero);
            }
        }
    }
}

COMPILER_SCRATCH_VOID(, emit_global_definitions,
    (RustEmitter *emitter, FILE *output),
    EmitGlobalDefinitionsBuffers, POOLED, free,
    emit_global_definitions_with_buffers(emitter, output, buffers))

static RustModuleVisit *module_visit(RustModuleVisits *visits,
                                     const ZirModule *module)
{
    for(size_t index = 0; index < visits->count; index++)
        if(visits->items[index].module == module)
            return &visits->items[index];
    if(visits->count == visits->capacity) {
        size_t capacity = visits->capacity > 0 ? visits->capacity * 2 : 16;
        RustModuleVisit *items = realloc(visits->items,
                                         capacity * sizeof(*items));
        if(items == NULL) {
            Diagnostic(module->span, "zir_rust.startup",
                       "out of memory ordering module startup");
            exit(1);
        }
        visits->items = items;
        visits->capacity = capacity;
    }
    visits->items[visits->count].module = module;
    visits->items[visits->count].state = 0;
    return &visits->items[visits->count++];
}

static void append_startup_module(RustModuleVisits *ordered,
                                  const ZirModule *module)
{
    for(size_t index = 0; index < ordered->count; index++)
        if(ordered->items[index].module == module)
            return;
    module_visit(ordered, module);
}

static void visit_startup_module(RustModuleVisits *visits,
                                 RustModuleVisits *ordered,
                                 const ZirModule *module)
{
    RustModuleVisit *visit = module_visit(visits, module);
    if(visit->state == 1) {
        Diagnostic(module->span, "zir_rust.startup",
                   "cyclic module startup is unsupported");
        exit(1);
    }
    if(visit->state == 2)
        return;
    visit->state = 1;
    /* As in C and Go, only dependencies with something to set up take part:
     * an import cycle through modules without globals, such as widgets that
     * import each other, orders nothing. */
    for(int index = 0; index < module->import_count; index++)
        if(module->imports[index].resolved_module != NULL &&
           ModuleNeedsStartup(module->imports[index].resolved_module))
            visit_startup_module(visits, ordered,
                                 module->imports[index].resolved_module);
    visit->state = 2;
    for(int index = 0; index < module->function_count; index++)
        if(module->functions[index].is_global_initializer) {
            append_startup_module(ordered, module);
            break;
        }
}

int emit_startup(RustEmitter *emitter, FILE *output)
{
    RustModuleVisits visits = {0};
    RustModuleVisits ordered = {0};
    for(int program_index = 0; program_index < emitter->program_count;
        program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
             module_index++)
            if(ModuleNeedsStartup(&program->modules[module_index]))
                visit_startup_module(&visits, &ordered,
                                     &program->modules[module_index]);
    }
    free(visits.items);
    if(ordered.count == 0) {
        free(ordered.items);
        return 0;
    }
    fputs("static mut ZIRAN_STARTUP_STATE: u8 = 0;\n\n"
          "pub fn ziran_startup() {\n    unsafe {\n"
          "        if ZIRAN_STARTUP_STATE == 2 { return; }\n"
          "        if ZIRAN_STARTUP_STATE == 1 { panic!(\"cyclic module startup\"); }\n"
          "        ZIRAN_STARTUP_STATE = 1;\n", output);
    for(size_t index = 0; index < ordered.count; index++) {
        const ZirModule *module = ordered.items[index].module;
        for(int function_index = 0; function_index < module->function_count;
             function_index++) {
            const ZirFunction *function = &module->functions[function_index];
            char symbol[ZIR_RUST_NAME_MAX * 2];
            if(!function->is_global_initializer)
                continue;
            function_symbol(emitter, module, function, symbol,
                            sizeof(symbol));
            fprintf(output, "        %s();\n", symbol);
        }
    }
    fputs("        ZIRAN_STARTUP_STATE = 2;\n    }\n}\n\n", output);
    free(ordered.items);
    return 1;
}
