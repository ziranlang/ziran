#include "zir_rust_internal.h"
static void free_scratch_EmitExternDefinitionsBuffers(void *pointer);
static void free_scratch_EmitTypeDefinitionsBuffers(void *pointer);

static const char *const rust_keywords[] = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn",
    "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
    "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
    "self", "Self", "static", "struct", "super", "trait", "true", "type",
    "unsafe", "use", "where", "while", "abstract", "become", "box", "do",
    "final", "macro", "override", "priv", "try", "typeof", "unsized",
    "virtual", "yield", NULL
};

const char *rust_scalar_type(const char *type);
int rust_enum_type(RustEmitter *emitter, const char *type,
                          const ZirModule **owner, const ZirType **enumeration);
int rust_owned_vec_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **record, char *element,
                               size_t element_size);
int rust_option_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record,
                            char *element, size_t element_size);
int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record);
int rust_type(RustEmitter *emitter, const char *type, char *output,
                     size_t size);
int rust_procedure_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **procedure);
int rust_copyable_type(RustEmitter *emitter, const char *type);
void rust_zero_value(RustEmitter *emitter, const char *type,
                            char *output, size_t size);
void rust_identifier(const char *source, char *output, size_t size);
void rust_field_name(const ZirType *record, const char *source,
                            char *output, size_t size);

static int identifier_character(int character)
{
    return isalnum((unsigned char)character) || character == '_';
}

/* Direct type applications are stored beside each checked module that uses
 * them, but equivalent instances share one identity in the Rust program. */
static int
synthetic_type_emitted(const RustEmitter *emitter, int program_index,
                       int module_index, const ZirType *type)
{
    if(!type->is_synthetic_application) return 0;
    for(int p = 0; p <= program_index; p++) {
        const ZirProgram *program = emitter->programs[p];
        int end = p == program_index ? module_index : program->module_count;
        for(int m = 0; m < end; m++) {
            const ZirModule *previous = &program->modules[m];
            for(int t = 0; t < previous->type_count; t++)
                if(same_type_application(emitter->module, type,
                                         previous, &previous->types[t]))
                    return 1;
        }
    }
    return 0;
}
/* Buffers emit_type_definitions keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitTypeDefinitionsBuffers {
    const ZirParameters *parameters;
} EmitTypeDefinitionsBuffers;

void emit_type_definitions(RustEmitter *emitter, FILE *output);

static void
emit_type_definitions_with_buffers(RustEmitter *emitter, FILE *output, EmitTypeDefinitionsBuffers *buffers)
{
    for(int program_index = 0; program_index < emitter->program_count;
        program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
            module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int type_index = 0; type_index < module->type_count;
                type_index++) {
                const ZirType *record = &module->types[type_index];
                if(synthetic_type_emitted(emitter, program_index,
                                          module_index, record))
                    continue;
                char type_name[ZIR_NAME_MAX];
                const ZirModule *enum_owner = NULL;
                const ZirType *enumeration = NULL;
                const ZirModule *procedure_owner = NULL;
                const ZirType *procedure = NULL;
                if(rust_owned_vec_type(emitter, record->name, NULL, NULL,
                                       NULL, 0) ||
                   rust_option_type(emitter, record->name, NULL, NULL,
                                    NULL, 0))
                    continue;
                if(FindType(module, record->name, &procedure_owner) != NULL &&
                   (procedure = FindType(module, record->name,
                                         &procedure_owner)) != NULL &&
                   procedure->is_procedure_type) {
                    char return_type[ZIR_NAME_MAX];
                    int count = (buffers->parameters = ParametersOf(procedure->body))->count;
                    if(count < 0) {
                        Diagnostic(procedure->span, "zir_rust.type",
                                   "unsupported procedure type: %s",
                                   procedure->name);
                        exit(1);
                    }
                    NativeTypeName(procedure_owner, procedure, type_name,
                                   sizeof(type_name));
                    if(!procedure->is_c_call) {
                        fprintf(output,
                                "#[repr(C)]\n#[derive(Clone, Copy)]\npub struct %s {\n"
                                "    pub context: *mut core::ffi::c_void,\n"
                                "    pub call: Option<unsafe extern \"C\" fn(*mut core::ffi::c_void",
                                type_name);
                    } else {
                        fprintf(output,
                                "pub type %s = Option<unsafe extern \"C\" fn(",
                                type_name);
                    }
                    for(int index = 0; index < count; index++) {
                        const ZirParameter *parameter = &buffers->parameters->items[index];
                        char parameter_type[ZIR_NAME_MAX];
                        const char *type;
                        if(parameter->value_type == NULL) {
                            Diagnostic(procedure->span, "zir_rust.type",
                                       "invalid procedure parameter: %s",
                                       buffers->parameters->items[index].text);
                            exit(1);
                        }
                        type = parameter->value_type;
                        if(!rust_type(emitter, type, parameter_type,
                                      sizeof(parameter_type))) {
                            Diagnostic(procedure->span, "zir_rust.type",
                                       "unsupported procedure parameter type: %s",
                                       type);
                            exit(1);
                        }
                        fprintf(output, "%s%s",
                                index || !procedure->is_c_call ? ", " : "",
                                parameter_type);
                    }
                    if(!rust_type(emitter, procedure->procedure_return_type,
                                  return_type, sizeof(return_type))) {
                        Diagnostic(procedure->span, "zir_rust.type",
                                   "unsupported procedure return type: %s",
                                   procedure->procedure_return_type);
                        exit(1);
                    }
                    if(!procedure->is_c_call) {
                        if(strcmp(procedure->procedure_return_type, "void") != 0)
                            fprintf(output, ") -> %s>,\n}\n\n", return_type);
                        else
                            fprintf(output, ")>,\n}\n\n");
                    } else if(strcmp(procedure->procedure_return_type, "void") != 0)
                        fprintf(output, ") -> %s>;\n\n", return_type);
                    else
                        fprintf(output, ")>;\n\n");
                    continue;
                }
                if(rust_enum_type(emitter, record->name, &enum_owner,
                                  &enumeration)) {
                    const char *backing = rust_scalar_type(
                        enumeration->enum_backing);
                    if(backing == NULL) {
                        Diagnostic(enumeration->span, "zir_rust.enum",
                                   "invalid enum backing type: %s",
                                   enumeration->enum_backing);
                        exit(1);
                    }
                    NativeTypeName(enum_owner, enumeration, type_name,
                                   sizeof(type_name));
                    fprintf(output, "pub type %s = %s;\n\n", type_name,
                            backing);
                    continue;
                }
                if(!rust_record_type(emitter, record->name, NULL, NULL))
                    continue;
                int copyable = 1;
                size_t offset = 0;
                ZirTypeField field;
                NativeTypeName(module, record, type_name, sizeof(type_name));
                while(TypeNextField(record, &offset, &field) == 1)
                    if(!rust_copyable_type(emitter, field.type))
                        copyable = 0;
                offset = 0;
                if(record->is_union)
                    fprintf(output,
                            "#[repr(C)]\n#[derive(Clone, Copy)]\npub union %s {\n",
                            type_name);
                else if(copyable)
                    fprintf(output,
                            "#[repr(C)]\n#[derive(Clone, Copy)]\npub struct %s {\n",
                            type_name);
                else
                    fprintf(output, "#[repr(C)]\npub struct %s {\n",
                            type_name);
                while(TypeNextField(record, &offset, &field) == 1) {
                    char field_name[ZIR_NAME_MAX];
                    char field_type[ZIR_NAME_MAX];
                    rust_field_name(record, field.name, field_name,
                                    sizeof(field_name));
                    if(!rust_type(emitter, field.type, field_type,
                                  sizeof(field_type))) {
                        Diagnostic(record->span, "zir_rust.type",
                                   "unsupported record field type: %s",
                                   field.type);
                        exit(1);
                    }
                    fprintf(output, "    pub %s: %s,\n", field_name,
                            field_type);
                }
                fputs("}\n\n", output);
            }
        }
    }
}

void
emit_type_definitions(RustEmitter *emitter, FILE *output)
{
    static const char scratch_key;
    static _Thread_local uint64_t scratch_identity;
    static _Thread_local CompilerScratch *scratch_hint;
    if(scratch_identity != CompilerContextIdentity()) {
        scratch_hint = CompilerContextScratch(&scratch_key, free_scratch_EmitTypeDefinitionsBuffers);
        scratch_identity = CompilerContextIdentity();
    }
    EmitTypeDefinitionsBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_type_definitions_with_buffers(emitter, output, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
}

void rust_extern_symbol(RustEmitter *emitter, const ZirModule *module,
                               const ZirImport *import, char *output,
                               size_t size)
{
    char file_stem[1024];
    char module_guard[1024];
    char safe_module[ZIR_RUST_NAME_MAX];
    char safe_name[ZIR_RUST_NAME_MAX];
    NativeGoModuleIdentity(emitter->programs, emitter->program_count, module,
                           file_stem, sizeof(file_stem),
                           module_guard, sizeof(module_guard));
    rust_identifier(module_guard, safe_module, sizeof(safe_module));
    rust_identifier(import->name, safe_name, sizeof(safe_name));
    snprintf(output, size, "ziran_foreign_%s_%s", safe_module, safe_name);
    rust_identifier(output, output, size);
}
/* Buffers emit_extern_definitions keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitExternDefinitionsBuffers {
    const ZirParameters *parameters;
    char symbol[ZIR_RUST_NAME_MAX * 2];
} EmitExternDefinitionsBuffers;

void emit_extern_definitions(RustEmitter *emitter, FILE *output);

static void
emit_extern_definitions_with_buffers(RustEmitter *emitter, FILE *output, EmitExternDefinitionsBuffers *buffers)
{
    int opened = 0;
    for(int program_index = 0; program_index < emitter->program_count;
        program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
             module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int import_index = 0; import_index < module->import_count;
                import_index++) {
                const ZirImport *import = &module->imports[import_index];
                char return_type[ZIR_NAME_MAX];
                int count;
                /* Host functions are C functions the embedding program
                 * provides under their Ziran names, as on the C targets. */
                if(import->kind != ZIR_IMPORT_EXTERN ||
                   (import->extern_kind != ZIR_EXTERN_C &&
                    import->extern_kind != ZIR_EXTERN_HOST))
                    continue;
                count = (buffers->parameters = ParametersOf(import->args))->count;
                /* A trailing ..any parameter is C's ... */
                if(import->is_varargs && count > 0 && strstr(buffers->parameters->items[count - 1].text, "..") != NULL)
                    count--;
                if(count < 0) {
                    Diagnostic(import->span, "zir_rust.import",
                               "unsupported foreign ABI for Rust: %s",
                               import->name);
                    exit(1);
                }
                if(!opened) {
                    fputs("extern \"C\" {\n", output);
                    opened = 1;
                }
                rust_extern_symbol(emitter, module, import, buffers->symbol,
                                   sizeof(buffers->symbol));
                if(import->extern_kind == ZIR_EXTERN_HOST)
                    fprintf(output, "    #[link_name = \"%s\"]\n", import->name);
                else
                    fprintf(output, "    #[link_name = \"%s\"]\n",
                            import->extern_symbol[0] ? import->extern_symbol : import->name);
                fprintf(output, "    fn %s(", buffers->symbol);
                for(int index = 0; index < count; index++) {
                    const ZirParameter *parameter = &buffers->parameters->items[index];
                    char parameter_type[ZIR_NAME_MAX];
                    const char *type;
                    if(parameter->value_type == NULL) {
                        Diagnostic(import->span, "zir_rust.import",
                                   "invalid foreign parameter: %s",
                                   buffers->parameters->items[index].text);
                        exit(1);
                    }
                    type = parameter->value_type;
                    if(!rust_type(emitter, type, parameter_type,
                                  sizeof(parameter_type))) {
                        Diagnostic(import->span, "zir_rust.import",
                                   "unsupported foreign parameter type: %s",
                                   type);
                        exit(1);
                    }
                    fprintf(output, "%s_%d: %s", index ? ", " : "", index,
                            parameter_type);
                }
                if(!rust_type(emitter, import->return_type, return_type,
                              sizeof(return_type))) {
                    Diagnostic(import->span, "zir_rust.import",
                               "unsupported foreign return type: %s",
                               import->return_type);
                    exit(1);
                }
                if(import->is_varargs)
                    fprintf(output, "%s...", count ? ", " : "");
                if(import->return_type[0] &&
                   strcmp(import->return_type, "void") != 0)
                    fprintf(output, ") -> %s;\n", return_type);
                else
                    fprintf(output, ");\n");
            }
        }
    }
    if(opened)
        fputs("}\n\n", output);
}

void
emit_extern_definitions(RustEmitter *emitter, FILE *output)
{
    static const char scratch_key;
    static _Thread_local uint64_t scratch_identity;
    static _Thread_local CompilerScratch *scratch_hint;
    if(scratch_identity != CompilerContextIdentity()) {
        scratch_hint = CompilerContextScratch(&scratch_key, free_scratch_EmitExternDefinitionsBuffers);
        scratch_identity = CompilerContextIdentity();
    }
    EmitExternDefinitionsBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_extern_definitions_with_buffers(emitter, output, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
}

void rust_identifier(const char *source, char *output, size_t size)
{
    size_t used = 0;
    if(source == NULL || *source == '\0') {
        snprintf(output, size, "ziran_empty");
        return;
    }
    if(!(isalpha((unsigned char)*source) || *source == '_') && used + 7 < size) {
        memcpy(output, "ziran_", 6);
        used = 6;
    }
    for(const unsigned char *p = (const unsigned char *)source;
        *p && used + 1 < size; p++)
        output[used++] = identifier_character(*p) ? *p : '_';
    output[used] = '\0';
    for(size_t index = 0; rust_keywords[index]; index++) {
        if(strcmp(output, rust_keywords[index]) == 0) {
            char saved[ZIR_RUST_NAME_MAX];
            snprintf(saved, sizeof(saved), "%s", output);
            snprintf(output, size, "ziran_keyword_%s", saved);
            break;
        }
    }
}

void write_line(RustEmitter *emitter, const char *format, ...)
{
    va_list arguments;
    for(int index = 0; index < emitter->indent; index++)
        fputs("    ", emitter->output);
    va_start(arguments, format);
    vfprintf(emitter->output, format, arguments);
    va_end(arguments);
    fputc('\n', emitter->output);
}

static void make_directory(const char *path)
{
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

void make_directories(const char *path)
{
    char temporary[1024];
    snprintf(temporary, sizeof(temporary), "%s", path);
    for(size_t index = 1; index < strlen(temporary); index++) {
        if(temporary[index] != '/'
#if defined(_WIN32)
           && temporary[index] != '\\'
#endif
        )
            continue;
        char separator = temporary[index];
        temporary[index] = '\0';
        make_directory(temporary);
        temporary[index] = separator;
    }
    make_directory(path);
}

const char *rust_scalar_type(const char *type)
{
    if(!strcmp(type, "bool")) return "bool";
    if(!strcmp(type, "s8")) return "i8";
    if(!strcmp(type, "s16")) return "i16";
    if(!strcmp(type, "s32")) return "i32";
    if(!strcmp(type, "s64") || !strcmp(type, "integer")) return "i64";
    if(!strcmp(type, "u8")) return "u8";
    if(!strcmp(type, "u16")) return "u16";
    if(!strcmp(type, "u32")) return "u32";
    if(!strcmp(type, "u64")) return "u64";
    if(!strcmp(type, "usize")) return "usize";
    if(!strcmp(type, "isize")) return "isize";
    if(!strcmp(type, "float32")) return "f32";
    if(!strcmp(type, "float64") || !strcmp(type, "real")) return "f64";
    if(!strcmp(type, "void")) return "";
    return NULL;
}

/* Rust rejects raw text-direction controls in literals. Keep the original
 * UTF-8 value while spelling those codepoints visibly in generated source. */
unsigned rust_direction_codepoint(const unsigned char *bytes, size_t length)
{
    if(length < 3 || bytes[0] != 0xe2)
        return 0;
    if(bytes[1] == 0x80 && bytes[2] >= 0xaa && bytes[2] <= 0xae)
        return 0x202a + bytes[2] - 0xaa;
    if(bytes[1] == 0x81 && bytes[2] >= 0xa6 && bytes[2] <= 0xa9)
        return 0x2066 + bytes[2] - 0xa6;
    return 0;
}

int rust_string_literal(const char *source, char *output, size_t size)
{
    static const char escapes[] = { 'n', 'r', 't', '0' };
    static const unsigned char values[] = { '\n', '\r', '\t', 0 };
    unsigned char decoded[ZIR_TEXT_MAX];
    size_t length = 0;
    size_t used = 0;

    int written;

    if(!DecodeStringLiteral(source, decoded, sizeof(decoded), &length))
        return 0;
    written = snprintf(output + used, size - used, "ZiranText::new(\"");
    if(written < 0 || used + (size_t)written >= size)
        return 0;
    used += (size_t)written;
    for(size_t index = 0; index < length; index++) {
        unsigned direction = rust_direction_codepoint(decoded + index, length - index);
        if(direction != 0) {
            written = snprintf(output + used, size - used, "\\u{%x}", direction);
            if(written < 0 || used + (size_t)written >= size)
                return 0;
            used += (size_t)written;
            index += 2;
            continue;
        }
        unsigned char byte = decoded[index];
        int escape = -1;
        for(size_t candidate = 0; candidate < sizeof(values); candidate++)
            if(byte == values[candidate])
                escape = (int)candidate;
        if(byte == '"' || byte == '\\' || escape >= 0) {
            if(used + 3 >= size)
                return 0;
            output[used++] = '\\';
            output[used++] = byte == '"' ? '"' :
                byte == '\\' ? '\\' : escapes[escape];
        } else if(byte < 0x20 || byte == 0x7f) {
            written = snprintf(output + used, size - used, "\\u{%02x}", byte);
            if(written < 0 || used + (size_t)written >= size)
                return 0;
            used += (size_t)written;
        } else {
            if(used + 1 >= size)
                return 0;
            output[used++] = (char)byte;
        }
    }
    written = snprintf(output + used, size - used, "\")");
    if(written < 0 || used + (size_t)written >= size)
        return 0;
    return 1;
}

int integer_type(const char *type)
{
    return !strcmp(type, "s8") || !strcmp(type, "s16") ||
           !strcmp(type, "s32") || !strcmp(type, "s64") ||
           !strcmp(type, "integer") || !strcmp(type, "u8") ||
           !strcmp(type, "u16") || !strcmp(type, "u32") ||
           !strcmp(type, "u64") || !strcmp(type, "usize") ||
           !strcmp(type, "isize");
}

/* Where two differently typed integer operands meet: the wider type, as
 * Ziran widens implicitly; equal widths of opposite sign meet at s64. An
 * untyped constant, such as a folded Width * Height, counts as s64. */
const char *rust_common_integer(const char *left, const char *right)
{
    if(!integer_type(left) || !integer_type(right) || !strcmp(left, right))
        return NULL;
    if(!strcmp(left, "integer"))
        left = !strcmp(right, "u64") || !strcmp(right, "usize") ? right : "s64";
    if(!strcmp(right, "integer"))
        right = !strcmp(left, "u64") || !strcmp(left, "usize") ? left : "s64";
    if(!strcmp(left, right))
        return left;
    unsigned left_width = ScalarWidth(left), right_width = ScalarWidth(right);
    if(left_width == 0) left_width = 64;
    if(right_width == 0) right_width = 64;
    if(left_width > right_width) return left;
    if(right_width > left_width) return right;
    return left_width < 64 ? "s64" : NULL;
}

int float_type(const char *type)
{
    return !strcmp(type, "float32") || !strcmp(type, "float64") ||
           !strcmp(type, "real");
}

int rust_enum_type(RustEmitter *emitter, const char *type,
                          const ZirModule **owner, const ZirType **enumeration)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    if(declared == NULL || !declared->is_enum) {
        if(owner != NULL) *owner = NULL;
        if(enumeration != NULL) *enumeration = NULL;
        return 0;
    }
    if(owner != NULL) *owner = type_owner;
    if(enumeration != NULL) *enumeration = declared;
    return 1;
}

int rust_owned_vec_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **record, char *element,
                               size_t element_size)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    ZirTypeField field;
    size_t offset = 0;
    if(declared == NULL ||
       TypeNextField(declared, &offset, &field) != 1 ||
       strcmp(field.name, "data") != 0 || field.type[0] != '*' ||
       field.type[1] == '\0') {
        if(owner != NULL) *owner = NULL;
        if(record != NULL) *record = NULL;
        return 0;
    }
    if(element != NULL)
        snprintf(element, element_size, "%s", field.type + 1);
    if(owner != NULL) *owner = type_owner;
    if(record != NULL) *record = declared;
    return 1;
}

int rust_option_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record,
                            char *element, size_t element_size)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    ZirTypeField fields[2];
    size_t offset = 0;
    if(declared == NULL ||
       TypeNextField(declared, &offset, &fields[0]) != 1 ||
       TypeNextField(declared, &offset, &fields[1]) != 1 ||
       strcmp(fields[0].name, "has_value") != 0 ||
       strcmp(fields[0].type, "bool") != 0 ||
       strcmp(fields[1].name, "value") != 0 ||
       fields[1].type[0] == '\0') {
        if(owner != NULL) *owner = NULL;
        if(record != NULL) *record = NULL;
        return 0;
    }
    if(element != NULL)
        snprintf(element, element_size, "%s", fields[1].type);
    if(owner != NULL) *owner = type_owner;
    if(record != NULL) *record = declared;
    return 1;
}

int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    if(declared == NULL || declared->is_enum ||
       declared->is_extern || declared->is_procedure_type ||
       declared->is_record_template || declared->is_type_instance ||
       declared->is_owned_vec) {
        if(owner != NULL) *owner = NULL;
        if(record != NULL) *record = NULL;
        return 0;
    }
    if(owner != NULL) *owner = type_owner;
    if(record != NULL) *record = declared;
    return 1;
}

int rust_procedure_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **procedure)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    if(declared == NULL || !declared->is_procedure_type) {
        if(owner != NULL) *owner = NULL;
        if(procedure != NULL) *procedure = NULL;
        return 0;
    }
    if(owner != NULL) *owner = type_owner;
    if(procedure != NULL) *procedure = declared;
    return 1;
}

int rust_type(RustEmitter *emitter, const char *type, char *output,
                     size_t size)
{
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    char element[ZIR_NAME_MAX];
    char element_type[ZIR_NAME_MAX];
    int capacity = 0;
    if(!strcmp(type, "string")) {
        snprintf(output, size, "ZiranText");
        return 1;
    }
    if(!strcmp(type, "*void")) {
        snprintf(output, size, "*mut core::ffi::c_void");
        return 1;
    }
    if(type[0] == '*' && type[1] != '\0') {
        char element_type[ZIR_NAME_MAX];
        if(!rust_type(emitter, type + 1, element_type,
                      sizeof(element_type)))
            return 0;
        snprintf(output, size, "*mut %s", element_type);
        return 1;
    }
    if(rust_scalar_type(type) != NULL) {
        snprintf(output, size, "%s", rust_scalar_type(type));
        return 1;
    }
    if(rust_owned_vec_type(emitter, type, &owner, &record, element,
                           sizeof(element))) {
        char element_type[ZIR_NAME_MAX];
        if(!rust_type(emitter, element, element_type, sizeof(element_type)))
            return 0;
        snprintf(output, size, "ZiranVec<%s>", element_type);
        return 1;
    }
    if(rust_option_type(emitter, type, &owner, &record, element,
                        sizeof(element))) {
        char element_type[ZIR_NAME_MAX];
        if(!rust_type(emitter, element, element_type, sizeof(element_type)))
            return 0;
        snprintf(output, size, "ZiranOption<%s>", element_type);
        return 1;
    }
    if(rust_enum_type(emitter, type, &owner, &record)) {
        NativeTypeName(owner, record, output, size);
        return 1;
    }
    if(rust_procedure_type(emitter, type, &owner, &record)) {
        NativeTypeName(owner, record, output, size);
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element)) &&
       rust_type(emitter, element, element_type, sizeof(element_type))) {
        snprintf(output, size, "ZiranSlice<%s>", element_type);
        return 1;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity) &&
       rust_type(emitter, element, element_type, sizeof(element_type))) {
        snprintf(output, size, "[%s; %d]", element_type, capacity);
        return 1;
    }
    if(rust_record_type(emitter, type, &owner, &record)) {
        NativeTypeName(owner, record, output, size);
        return 1;
    }
    output[0] = '\0';
    return 0;
}

int rust_copyable_type(RustEmitter *emitter, const char *type)
{
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(type[0] == '*' && type[1] != '\0')
        return 1;
    if(rust_scalar_type(type) != NULL || !strcmp(type, "string") ||
       rust_enum_type(emitter, type, &owner, &record) ||
       rust_procedure_type(emitter, type, &owner, &record))
        return 1;
    if(rust_owned_vec_type(emitter, type, NULL, NULL, NULL, 0) ||
       rust_option_type(emitter, type, NULL, NULL, NULL, 0))
        return 0;
    /* A slice is a pointer and a length, Copy for any element. */
    if(SliceElementType(type, element, sizeof(element)))
        return 1;
    if(ArrayElementType(type, element, sizeof(element), &capacity))
        return rust_copyable_type(emitter, element);
    if(rust_record_type(emitter, type, &owner, &record)) {
        size_t offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &offset, &field) == 1)
            if(!rust_copyable_type(emitter, field.type))
                return 0;
        return 1;
    }
    return 0;
}

void require_rust_type(RustEmitter *emitter, ZirSourceSpan span,
                              const char *type, char *output, size_t size)
{
    if(rust_type(emitter, type, output, size))
        return;
    DiagnosticTarget(span, "zir_rust.type", "rust", "types.rust",
               "the Rust target cannot lower this type: %s",
               type);
    exit(1);
}

void register_local(RustEmitter *emitter, const char *source)
{
    char mapped[ZIR_NAME_MAX];
    if(source == NULL || !*source ||
       emitter->local_count >= ZIR_RUST_LOCAL_MAX)
        return;
    for(int index = 0; index < emitter->local_count; index++)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return;
    rust_identifier(source, mapped, sizeof(mapped));
    snprintf(emitter->locals[emitter->local_count].source,
             sizeof(emitter->locals[emitter->local_count].source), "%s", source);
    snprintf(emitter->locals[emitter->local_count].rust,
             sizeof(emitter->locals[emitter->local_count].rust), "%s", mapped);
    emitter->local_count++;
}

const char *local_name(RustEmitter *emitter, const char *source)
{
    for(int index = emitter->local_count - 1; index >= 0; index--)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return emitter->locals[index].rust;
    return source;
}

void rust_field_name(const ZirType *record, const char *source,
                            char *output, size_t size)
{
    rust_identifier(source, output, size);
    if(strcmp(output, source) == 0 || record == NULL)
        return;
    for(size_t serial = 0; ; serial++) {
        char candidate[ZIR_NAME_MAX];
        ZirTypeField field;
        size_t offset = 0;
        int collision = 0;
        snprintf(candidate, sizeof(candidate), "ziran_keyword_%s_%zu",
                 source, serial);
        while(TypeNextField(record, &offset, &field) == 1)
            if(strcmp(field.name, source) != 0 &&
               strcmp(field.name, candidate) == 0)
                collision = 1;
        if(!collision) {
            snprintf(output, size, "%s", candidate);
            return;
        }
    }
}

static void rust_assembly_name(void *context, const char *source, char *output, size_t size)
{
    snprintf(output, size, "%s", local_name(context, source));
}

static void rust_assembly_type(void *context, const char *source, char *output, size_t size)
{
    RustEmitter *emitter = context;
    require_rust_type(emitter, emitter->function->span, source, output, size);
}

static void rust_assembly_field(void *context, const char *source, char *output, size_t size)
{
    RustEmitter *emitter = context;
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    rust_record_type(emitter, emitter->function->return_type, &owner, &record);
    rust_field_name(record, source, output, size);
}

int emit_rust_assembly(RustEmitter *emitter, int statement)
{
    RustAssemblyTarget target = {emitter, rust_assembly_name, rust_assembly_type, rust_assembly_field};
    return RustEmitAssembly(emitter->output, emitter->module, emitter->function,
                            statement, emitter->indent, &target);
}

void function_symbol(RustEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output,
                            size_t size)
{
    /* #program_export fixes the linker name, as on the C targets. */
    if(function->export_symbol[0]) {
        snprintf(output, size, "%s", function->export_symbol);
        return;
    }
    /* main stays module-qualified: the executable's own fn main calls it. */
    if(function->exported && strcmp(function->name, "main") != 0) {
        rust_identifier(function->name, output, size);
        return;
    }
    NativeGoFunctionName(emitter->programs, emitter->program_count, module,
                         function, output, size);
}

void slot_wrapper_symbol(RustEmitter *emitter, int expression,
                                 char *output, size_t size)
{
    char symbol[ZIR_RUST_NAME_MAX * 2];
    function_symbol(emitter, emitter->module, emitter->function, symbol,
                    sizeof(symbol));
    snprintf(output, size, "ziran_slot_%s_%d", symbol, expression);
    rust_identifier(output, output, size);
}

void global_symbol(RustEmitter *emitter, const ZirModule *module,
                           const ZirGlobal *global, char *output, size_t size)
{
    char file_stem[1024];
    char module_guard[1024];
    char safe_module[ZIR_RUST_NAME_MAX];
    char safe_name[ZIR_RUST_NAME_MAX];
    NativeGoModuleIdentity(emitter->programs, emitter->program_count, module,
                           file_stem, sizeof(file_stem), module_guard,
                           sizeof(module_guard));
    rust_identifier(module_guard, safe_module, sizeof(safe_module));
    rust_identifier(global->name, safe_name, sizeof(safe_name));
    snprintf(output, size, "ziran_global_%s_%s", safe_module, safe_name);
    rust_identifier(output, output, size);
}

static int has_local(RustEmitter *emitter, const char *source)
{
    for(int index = 0; index < emitter->local_count; index++)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return 1;
    return 0;
}

int global_reference(RustEmitter *emitter, const char *name,
                            char *output, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(has_local(emitter, name) ||
       ResolveGlobalAt(emitter->module, name,
                       SpanPath(emitter->function->span), &owner,
                       &global) != 1 || owner == NULL)
        return 0;
    global_symbol(emitter, owner, global, output, size);
    return 1;
}

void unsupported_expression(RustEmitter *emitter,
                                   const ZirExpr *expression)
{
    (void)emitter;
    Diagnostic(expression->span, "zir_rust.expression",
               "unsupported expression in the Rust target: %s",
               expression->text[0] ? expression->text :
                   ExprKindName(expression->kind));
    exit(1);
}

void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

/* A field reached through using fields is a dotted path (middle.point.x);
 * each step is named in its own record. */
void rust_member_path(RustEmitter *emitter, const ZirModule *owner,
                             const ZirType *record, const char *path,
                             char *output, size_t size)
{
    char part[ZIR_NAME_MAX];
    size_t used = 0;
    output[0] = '\0';
    while(*path && record != NULL) {
        const char *dot = strchr(path, '.');
        size_t length = dot ? (size_t)(dot - path) : strlen(path);
        char field_name[ZIR_NAME_MAX];
        snprintf(part, sizeof(part), "%.*s", (int)length, path);
        rust_field_name(record, part, field_name, sizeof(field_name));
        used += (size_t)snprintf(output + used, size > used ? size - used : 0, "%s%s",
                                 used ? "." : "", field_name);
        if(dot == NULL)
            break;
        {
            size_t offset = 0;
            ZirTypeField field;
            const ZirType *next = NULL;
            const ZirModule *next_owner = NULL;
            const ZirModule *saved = emitter->module;
            while(TypeNextField(record, &offset, &field) == 1)
                if(!strcmp(field.name, part)) {
                    if(owner != NULL)
                        emitter->module = owner;
                    rust_record_type(emitter, field.type, &next_owner, &next);
                    emitter->module = saved;
                    break;
                }
            record = next;
            owner = next_owner;
        }
        path = dot + 1;
    }
}

static void free_scratch_EmitExternDefinitionsBuffers(void *pointer)
{
    EmitExternDefinitionsBuffers *buffers = pointer;
    free(buffers);
}

static void free_scratch_EmitTypeDefinitionsBuffers(void *pointer)
{
    EmitTypeDefinitionsBuffers *buffers = pointer;
    free(buffers);
}
