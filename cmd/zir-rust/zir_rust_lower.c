/*
 * zir_rust_lower.c - checked ZIR to Rust backend.
 */
#include "zir_rust_lower.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"
#include "zir_check.h"
#include "zir_parse.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ZIR_RUST_TEXT_MAX 65536
#define ZIR_RUST_NAME_MAX 256
#define ZIR_RUST_LOCAL_MAX 512

typedef struct RustLocal {
    char source[ZIR_NAME_MAX];
    char rust[ZIR_NAME_MAX];
} RustLocal;

typedef struct RustEmitter {
    FILE *output;
    const ZirProgram *const *programs;
    int program_count;
    const ZirModule *module;
    const ZirFunction *function;
    RustLocal locals[ZIR_RUST_LOCAL_MAX];
    int local_count;
    int indent;
} RustEmitter;

typedef struct RustModuleVisit {
    const ZirModule *module;
    int state;
} RustModuleVisit;

typedef struct RustModuleVisits {
    RustModuleVisit *items;
    size_t count;
    size_t capacity;
} RustModuleVisits;

static const char *const rust_keywords[] = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn",
    "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
    "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
    "self", "Self", "static", "struct", "super", "trait", "true", "type",
    "unsafe", "use", "where", "while", "abstract", "become", "box", "do",
    "final", "macro", "override", "priv", "try", "typeof", "unsized",
    "virtual", "yield", NULL
};

static const char *rust_scalar_type(const char *type);
static int rust_enum_type(RustEmitter *emitter, const char *type,
                          const ZirModule **owner, const ZirType **enumeration);
static int rust_owned_vec_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **record, char *element,
                               size_t element_size);
static int rust_option_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record,
                            char *element, size_t element_size);
static int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record);
static int rust_type(RustEmitter *emitter, const char *type, char *output,
                     size_t size);
static int rust_procedure_type(RustEmitter *emitter, const char *type,
                               const ZirModule **owner,
                               const ZirType **procedure);
static int rust_copyable_type(RustEmitter *emitter, const char *type);
static void rust_zero_value(RustEmitter *emitter, const char *type,
                            char *output, size_t size);
static int split_arguments(const char *text,
                           char parts[][ZIR_RUST_TEXT_MAX], int maximum);
static void rust_identifier(const char *source, char *output, size_t size);
static void rust_field_name(const ZirType *record, const char *source,
                            char *output, size_t size);

static int identifier_character(int character)
{
    return isalnum((unsigned char)character) || character == '_';
}

static void emit_type_definitions(RustEmitter *emitter, FILE *output)
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
                    char parts[32][ZIR_RUST_TEXT_MAX];
                    char return_type[ZIR_NAME_MAX];
                    int count = *procedure->body ?
                        split_arguments(procedure->body, parts, 32) : 0;
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
                        char *colon = strchr(parts[index], ':');
                        char parameter_type[ZIR_NAME_MAX];
                        char *type;
                        if(colon == NULL) {
                            Diagnostic(procedure->span, "zir_rust.type",
                                       "invalid procedure parameter: %s",
                                       parts[index]);
                            exit(1);
                        }
                        type = colon + 1;
                        while(*type == ' ' || *type == '\t')
                            type++;
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

static void rust_extern_symbol(RustEmitter *emitter, const ZirModule *module,
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

static void emit_extern_definitions(RustEmitter *emitter, FILE *output)
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
                char parts[32][ZIR_RUST_TEXT_MAX];
                char return_type[ZIR_NAME_MAX];
                char symbol[ZIR_RUST_NAME_MAX * 2];
                int count;
                /* Host functions are C functions the embedding program
                 * provides under their Ziran names, as on the C targets. */
                if(import->kind != ZIR_IMPORT_EXTERN ||
                   (import->extern_kind != ZIR_EXTERN_C &&
                    import->extern_kind != ZIR_EXTERN_HOST))
                    continue;
                count = *import->args ?
                    split_arguments(import->args, parts, 32) : 0;
                /* A trailing ..any parameter is C's ... */
                if(import->is_varargs && count > 0 && strstr(parts[count - 1], "..") != NULL)
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
                rust_extern_symbol(emitter, module, import, symbol,
                                   sizeof(symbol));
                if(import->extern_kind == ZIR_EXTERN_HOST)
                    fprintf(output, "    #[link_name = \"%s\"]\n", import->name);
                else if(import->extern_symbol[0] &&
                   strcmp(import->extern_symbol, import->name) != 0)
                    fprintf(output, "    #[link_name = \"%s\"]\n",
                            import->extern_symbol);
                fprintf(output, "    fn %s(", symbol);
                for(int index = 0; index < count; index++) {
                    char *colon = strchr(parts[index], ':');
                    char parameter_type[ZIR_NAME_MAX];
                    char *type;
                    if(colon == NULL) {
                        Diagnostic(import->span, "zir_rust.import",
                                   "invalid foreign parameter: %s",
                                   parts[index]);
                        exit(1);
                    }
                    type = colon + 1;
                    while(*type == ' ' || *type == '\t')
                        type++;
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

static void rust_identifier(const char *source, char *output, size_t size)
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

static void write_line(RustEmitter *emitter, const char *format, ...)
{
    va_list arguments;
    for(int index = 0; index < emitter->indent; index++)
        fputs("    ", emitter->output);
    va_start(arguments, format);
    vfprintf(emitter->output, format, arguments);
    va_end(arguments);
    fputc('\n', emitter->output);
}

static void make_directories(const char *path)
{
    char temporary[1024];
    snprintf(temporary, sizeof(temporary), "%s", path);
    for(size_t index = 1; index < strlen(temporary); index++) {
        if(temporary[index] != '/')
            continue;
        temporary[index] = '\0';
        mkdir(temporary, 0755);
        temporary[index] = '/';
    }
    mkdir(path, 0755);
}

static const char *rust_scalar_type(const char *type)
{
    if(!strcmp(type, "bool")) return "bool";
    if(!strcmp(type, "s8")) return "i8";
    if(!strcmp(type, "s16")) return "i16";
    if(!strcmp(type, "s32")) return "i32";
    if(!strcmp(type, "s64") || !strcmp(type, "integer")) return "i64";
    if(!strcmp(type, "u8")) return "u8";
    if(!strcmp(type, "u16")) return "u16";
    if(!strcmp(type, "u32")) return "u32";
    if(!strcmp(type, "u64") || !strcmp(type, "usize")) return "u64";
    if(!strcmp(type, "isize")) return "i64";
    if(!strcmp(type, "float32")) return "f32";
    if(!strcmp(type, "float64") || !strcmp(type, "real")) return "f64";
    if(!strcmp(type, "void")) return "";
    return NULL;
}

static int rust_string_literal(const char *source, char *output, size_t size)
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

static int integer_type(const char *type)
{
    return !strcmp(type, "s8") || !strcmp(type, "s16") ||
           !strcmp(type, "s32") || !strcmp(type, "s64") ||
           !strcmp(type, "integer") || !strcmp(type, "u8") ||
           !strcmp(type, "u16") || !strcmp(type, "u32") ||
           !strcmp(type, "u64") || !strcmp(type, "usize") ||
           !strcmp(type, "isize");
}

static int float_type(const char *type)
{
    return !strcmp(type, "float32") || !strcmp(type, "float64") ||
           !strcmp(type, "real");
}

static int rust_enum_type(RustEmitter *emitter, const char *type,
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

static int rust_owned_vec_type(RustEmitter *emitter, const char *type,
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

static int rust_option_type(RustEmitter *emitter, const char *type,
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

static int rust_record_type(RustEmitter *emitter, const char *type,
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

static int rust_procedure_type(RustEmitter *emitter, const char *type,
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

static int rust_type(RustEmitter *emitter, const char *type, char *output,
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

static int rust_copyable_type(RustEmitter *emitter, const char *type)
{
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(type[0] == '*' && type[1] != '\0')
        return 1;
    if(rust_scalar_type(type) != NULL || !strcmp(type, "string") ||
       rust_enum_type(emitter, type, &owner, &record))
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

static void require_rust_type(RustEmitter *emitter, ZirSourceSpan span,
                              const char *type, char *output, size_t size)
{
    if(rust_type(emitter, type, output, size))
        return;
    Diagnostic(span, "zir_rust.type",
               "the Rust target cannot lower this type: %s",
               type);
    exit(1);
}

static void register_local(RustEmitter *emitter, const char *source)
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

static const char *local_name(RustEmitter *emitter, const char *source)
{
    for(int index = emitter->local_count - 1; index >= 0; index--)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return emitter->locals[index].rust;
    return source;
}

static void rust_field_name(const ZirType *record, const char *source,
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

static void function_symbol(RustEmitter *emitter, const ZirModule *module,
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

static void slot_wrapper_symbol(RustEmitter *emitter, int expression,
                                 char *output, size_t size)
{
    char symbol[ZIR_RUST_NAME_MAX * 2];
    function_symbol(emitter, emitter->module, emitter->function, symbol,
                    sizeof(symbol));
    snprintf(output, size, "ziran_slot_%s_%d", symbol, expression);
    rust_identifier(output, output, size);
}

static void global_symbol(RustEmitter *emitter, const ZirModule *module,
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

static int global_reference(RustEmitter *emitter, const char *name,
                            char *output, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(has_local(emitter, name) ||
       ResolveGlobalAt(emitter->module, name,
                       emitter->function->span.path, &owner,
                       &global) != 1 || owner == NULL)
        return 0;
    global_symbol(emitter, owner, global, output, size);
    return 1;
}

static void unsupported_expression(RustEmitter *emitter,
                                   const ZirExpr *expression)
{
    (void)emitter;
    Diagnostic(expression->span, "zir_rust.expression",
               "unsupported expression in the Rust target: %s",
               expression->text[0] ? expression->text :
                   ExprKindName(expression->kind));
    exit(1);
}

static void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

/* A field reached through using fields is a dotted path (middle.point.x);
 * each step is named in its own record. */
static void rust_member_path(RustEmitter *emitter, const ZirModule *owner,
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

static void emit_destination(RustEmitter *emitter, int index, char *output,
                             size_t size)
{
    const ZirExpr *expression;
    if(index < 0 || index >= emitter->function->expr_count) {
        snprintf(output, size, "()");
        return;
    }
    expression = &emitter->function->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        char global_name[ZIR_RUST_NAME_MAX * 2];
        if(global_reference(emitter, expression->name, global_name,
                            sizeof(global_name)))
            snprintf(output, size, "%s", global_name);
        else
            snprintf(output, size, "%s", local_name(emitter, expression->name));
        return;
    }
    if(expression->kind == ZIR_EXPR_MEMBER) {
        char base[ZIR_RUST_TEXT_MAX];
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        char base_type[ZIR_NAME_MAX];
        snprintf(base_type, sizeof(base_type), "%s",
                 emitter->function->exprs[expression->left].type);
        {
            int owned_vec = rust_owned_vec_type(emitter, base_type, NULL,
                                                NULL, NULL, 0);
            if((SliceElementType(base_type, NULL, 0) ||
                !strcmp(base_type, "string") || owned_vec) &&
               !strcmp(expression->name, "count")) {
                emit_destination(emitter, expression->left, base,
                                 sizeof(base));
                snprintf(output, size, "(%s.%s as i64)", base,
                         owned_vec ? "count" : "len");
                return;
            }
        }
        if(base_type[0] == '*' &&
           rust_record_type(emitter, base_type + 1, &owner, &record)) {
            emit_expression(emitter, expression->left, base, sizeof(base));
            rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
            snprintf(output, size, "(*%s).%s", base, field);
            return;
        }
        if(!rust_record_type(emitter, base_type, &owner, &record)) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_destination(emitter, expression->left, base, sizeof(base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "%s.%s", base, field);
        return;
    }
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        char base[ZIR_RUST_TEXT_MAX];
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        const char *base_type = emitter->function->exprs[expression->left].type;
        if(base_type[0] != '*' ||
           !rust_record_type(emitter, base_type + 1, &owner, &record)) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_expression(emitter, expression->left, base, sizeof(base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "(*%s).%s", base, field);
        return;
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
        char pointer[ZIR_RUST_TEXT_MAX];
        emit_expression(emitter, expression->right, pointer, sizeof(pointer));
        snprintf(output, size, "(*%s)", pointer);
        return;
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        char base[ZIR_RUST_TEXT_MAX];
        char index[ZIR_RUST_TEXT_MAX];
        char element[ZIR_NAME_MAX];
        emit_destination(emitter, expression->left, base, sizeof(base));
        emit_expression(emitter, expression->right, index, sizeof(index));
        if(rust_owned_vec_type(
               emitter, emitter->function->exprs[expression->left].type,
               NULL, NULL, element, sizeof(element)))
            snprintf(output, size, "(*%s.element(%s as i64))", base, index);
        else if(emitter->function->exprs[expression->left].type[0] == '*')
            snprintf(output, size, "(*%s.offset(%s as isize))", base, index);
        else if(SliceElementType(
                    emitter->function->exprs[expression->left].type, element,
                    sizeof(element)))
            snprintf(output, size, "(*%s.element(%s as i64))", base, index);
        else
            snprintf(output, size, "%s[%s as usize]", base, index);
        return;
    }
    if(expression->kind == ZIR_EXPR_MEMBER &&
       SliceElementType(emitter->function->exprs[expression->left].type,
                        NULL, 0) &&
       !strcmp(expression->name, "count")) {
        char base[ZIR_RUST_TEXT_MAX];
        emit_destination(emitter, expression->left, base, sizeof(base));
        snprintf(output, size, "(%s.len as i64)", base);
        return;
    }
    /* A slice, call result, or literal is read in place, not borrowed. */
    if(expression->kind == ZIR_EXPR_SLICE || expression->kind == ZIR_EXPR_CALL ||
       expression->kind == ZIR_EXPR_COMPOUND || expression->kind == ZIR_EXPR_STRING ||
       expression->kind == ZIR_EXPR_CONDITIONAL) {
        emit_expression(emitter, index, output, size);
        return;
    }
    unsupported_expression(emitter, expression);
}

/* A body needs an unsafe block only to reach static mut globals or go
 * through raw pointers; foreign calls and checked indexing carry their own
 * unsafe blocks. */
static int rust_union_field(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_union;
}

static int rust_function_needs_unsafe(const ZirModule *module, const ZirFunction *function)
{
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->is_global_value || expression->kind == ZIR_EXPR_POINTER_MEMBER)
            return 1;
        if(expression->kind == ZIR_EXPR_MEMBER && expression->left >= 0 &&
           rust_union_field(module, function->exprs[expression->left].type))
            return 1;
        if(expression->kind == ZIR_EXPR_UNARY &&
           (!strcmp(expression->op, "*") || !strcmp(expression->op, "&")))
            return 1;
        if((expression->kind == ZIR_EXPR_INDEX || expression->kind == ZIR_EXPR_SLICE ||
            expression->kind == ZIR_EXPR_MEMBER) &&
           expression->left >= 0 && function->exprs[expression->left].type[0] == '*')
            return 1;
        if(expression->type[0] == '*')
            return 1;
    }
    for(int index = 0; index < function->stmt_count; index++)
        if(function->stmts[index].type[0] == '*')
            return 1;
    return 0;
}

/* Raw-pointer operations written outside an inline unsafe block. */
static int rust_text_needs_unsafe(const char *text, size_t size)
{
    static const char *const operations[] = {
        ".offset(", ".read()", ".write(", ".element(", "from_raw_parts", "core::ptr::",
        "std::ptr::", "std::alloc::", "core::mem::zeroed", NULL};
    int depth = 0, inline_depth = -1;
    for(size_t index = 0; index < size; index++) {
        if(inline_depth < 0 && index + 8 <= size && !strncmp(text + index, "unsafe {", 8)) {
            inline_depth = depth;
            depth++;
            index += 7;
            continue;
        }
        if(text[index] == '{') depth++;
        else if(text[index] == '}' && --depth == inline_depth) inline_depth = -1;
        if(inline_depth >= 0)
            continue;
        for(int op = 0; operations[op] != NULL; op++)
            if(!strncmp(text + index, operations[op], strlen(operations[op])))
                return 1;
    }
    return 0;
}

/* One enclosing pair of parentheses removed from a standalone expression. */
static const char *rust_bare(const char *text, char *output, size_t size)
{
    size_t length = strlen(text);
    int depth = 0, whole = length >= 2 && text[0] == '(' && text[length - 1] == ')';
    for(size_t index = 0; whole && index < length; index++) {
        if(text[index] == '(') depth++;
        else if(text[index] == ')' && --depth == 0 && index + 1 < length) whole = 0;
    }
    if(whole)
        snprintf(output, size, "%.*s", (int)(length - 2), text + 1);
    else
        snprintf(output, size, "%s", text);
    return output;
}

/* A method receiver needs parentheses unless it is a name or an unsigned
 * suffixed literal. */
static int rust_plain_receiver(const char *text)
{
    if(!isalnum((unsigned char)text[0]) && text[0] != '_')
        return 0;
    for(const char *c = text; *c; c++)
        if(!isalnum((unsigned char)*c) && *c != '_' && *c != '.')
            return 0;
    return 1;
}

/* A suffixed Rust integer literal of this exact type, like 40i64. */
static int rust_literal_bits(const char *text, const char *type, uint64_t *bits)
{
    const char *suffix = rust_scalar_type(type);
    char *end;
    if(suffix == NULL || !isdigit((unsigned char)text[0]))
        return 0;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 0);
    if(errno != 0 || strcmp(end, suffix) != 0)
        return 0;
    *bits = (uint64_t)value;
    return 1;
}

static void rust_literal(const char *type, uint64_t bits, char *output, size_t size)
{
    const char *suffix = rust_scalar_type(type);
    int bits_width = suffix[1] == '8' ? 8 : suffix[1] == '1' ? 16 :
                     suffix[1] == '3' ? 32 : 64;
    uint64_t mask = bits_width == 64 ? UINT64_MAX : (UINT64_C(1) << bits_width) - 1;
    bits &= mask;
    if(suffix[0] == 'i' && (bits >> (bits_width - 1))) {
        int64_t value = bits_width == 64 ? (int64_t)bits : (int64_t)(bits | ~mask);
        if(value == INT64_MIN)
            snprintf(output, size, "i64::MIN");
        else
            snprintf(output, size, "(%lld%s)", (long long)value, suffix);
    } else
        snprintf(output, size, "%llu%s", (unsigned long long)bits, suffix);
}

static void emit_typed_expression(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size)
{
    char value[ZIR_RUST_TEXT_MAX];
    const ZirExpr *expression = index >= 0 &&
        index < emitter->function->expr_count ?
            &emitter->function->exprs[index] : NULL;
    emit_expression(emitter, index, value, sizeof(value));
    /* An untyped literal takes the type as a suffix: 2026i32. */
    if(expression != NULL && expression->kind == ZIR_EXPR_INT &&
       !strcmp(expression->type, "integer") && rust_scalar_type(type) != NULL &&
       isdigit((unsigned char)value[0]) && strspn(value, "0123456789abcdefABCDEFxX") == strlen(value))
        snprintf(output, size, "%s%s", value, rust_scalar_type(type));
    else if(expression != NULL && strcmp(expression->type, type) != 0 &&
       rust_scalar_type(type) != NULL &&
       rust_scalar_type(expression->type) != NULL)
        snprintf(output, size, "((%s) as %s)", value,
                 rust_scalar_type(type));
    else
        snprintf(output, size, "%s", value);
}

static void emit_integer_literal(const ZirExpr *expression, char *output,
                                 size_t size)
{
    const char *suffix;
    if(!strcmp(expression->type, "integer") || expression->type[0] == '\0') {
        snprintf(output, size, "%s", expression->text);
        return;
    }
    suffix = "";
    if(!strcmp(expression->type, "s8")) suffix = "i8";
    else if(!strcmp(expression->type, "s16")) suffix = "i16";
    else if(!strcmp(expression->type, "s32")) suffix = "i32";
    else if(!strcmp(expression->type, "s64")) suffix = "i64";
    else if(!strcmp(expression->type, "u8")) suffix = "u8";
    else if(!strcmp(expression->type, "u16")) suffix = "u16";
    else if(!strcmp(expression->type, "u32")) suffix = "u32";
    else if(!strcmp(expression->type, "u64") ||
            !strcmp(expression->type, "usize")) suffix = "u64";
    snprintf(output, size, "%s%s", expression->text, suffix);
}

static const ZirImport *rust_foreign_import(RustEmitter *emitter,
                                            const char *name)
{
    for(int index = 0; index < emitter->module->import_count; index++) {
        const ZirImport *import = &emitter->module->imports[index];
        if(import->kind == ZIR_IMPORT_EXTERN &&
           (import->extern_kind == ZIR_EXTERN_C || import->extern_kind == ZIR_EXTERN_HOST) &&
           strcmp(import->name, name) == 0)
            return import;
    }
    return NULL;
}

static void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

static int rust_expression_calls(const ZirFunction *function, int index)
{
    if(index < 0 || index >= function->expr_count)
        return 0;
    const ZirExpr *expression = &function->exprs[index];
    if(expression->kind == ZIR_EXPR_CALL)
        return 1;
    for(int child = expression->first_child; child >= 0;
        child = function->exprs[child].next_sibling)
        if(rust_expression_calls(function, child))
            return 1;
    return rust_expression_calls(function, expression->left) ||
           rust_expression_calls(function, expression->right) ||
           rust_expression_calls(function, expression->third);
}

/* A print statement writes one call per piece. Arguments are bound first
 * only when one contains a call, so every argument evaluates before any
 * output, as on the other targets. */
static int emit_print_statement(RustEmitter *emitter, const ZirExpr *expression)
{
    /* One print! per statement: text with {} for each value. print!
     * evaluates every argument before it writes, as the other targets do. */
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    char *format = calloc(1, ZIR_RUST_TEXT_MAX);
    char *arguments = calloc(1, ZIR_RUST_TEXT_MAX);
    const ZirFunction *function = emitter->function;
    int first = expression->first_child, count, argument_node;
    size_t format_used = 0, arguments_used = 0;
    if(pieces == NULL || format == NULL || arguments == NULL || first < 0 ||
       (count = PrintFormatPieces(function->exprs[first].text, pieces,
                                  PRINT_PIECES_MAX)) < 0) {
        free(pieces);
        free(format);
        free(arguments);
        return 0;
    }
    argument_node = function->exprs[first].next_sibling;
    for(int piece = 0; piece < count; piece++) {
        if(!pieces[piece].is_argument) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(!DecodeStringLiteral(pieces[piece].literal, bytes, sizeof(bytes), &length))
                break;
            for(size_t i = 0; i < length && format_used + 12 < ZIR_RUST_TEXT_MAX; i++) {
                unsigned char byte = bytes[i];
                const char *escape = byte == '\n' ? "\\n" : byte == '\t' ? "\\t" :
                    byte == '\r' ? "\\r" : byte == '"' ? "\\\"" : byte == '\\' ? "\\\\" :
                    byte == '{' ? "{{" : byte == '}' ? "}}" : NULL;
                if(escape != NULL)
                    format_used += (size_t)snprintf(format + format_used,
                                                    ZIR_RUST_TEXT_MAX - format_used, "%s", escape);
                else if(byte < 0x20 || byte == 0x7f)
                    format_used += (size_t)snprintf(format + format_used,
                                                    ZIR_RUST_TEXT_MAX - format_used, "\\u{%x}", byte);
                else
                    format[format_used++] = (char)byte;
            }
            continue;
        }
        if(argument_node < 0)
            break;
        char value[ZIR_RUST_TEXT_MAX];
        const char *type = function->exprs[argument_node].type;
        emit_expression(emitter, argument_node, value, sizeof(value));
        format_used += (size_t)snprintf(format + format_used,
                                        ZIR_RUST_TEXT_MAX - format_used, "{}");
        if(!strcmp(type, "string"))
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used,
                rust_plain_receiver(value) ? ", %s.as_str()" : ", (%s).as_str()", value);
        else if(!strcmp(type, "float32") || !strcmp(type, "float64"))
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used, ", ZiranFloat(%s)", value);
        else
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used, ", %s", value);
        argument_node = function->exprs[argument_node].next_sibling;
    }
    format[format_used] = '\0';
    write_line(emitter, "print!(\"%s\"%s);", format, arguments);
    free(pieces);
    free(format);
    free(arguments);
    return 1;
}

static void emit_call(RustEmitter *emitter, const ZirExpr *expression,
                      char *output, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    char symbol[ZIR_RUST_NAME_MAX * 2];
    char arguments[ZIR_RUST_TEXT_MAX] = "";
    char child[ZIR_RUST_TEXT_MAX];
    char element[ZIR_NAME_MAX];
    const ZirImport *foreign = rust_foreign_import(emitter, expression->name);
    if(!strcmp(expression->name, "VecPush") ||
       !strcmp(expression->name, "VecPop") ||
       !strcmp(expression->name, "VecGet") ||
       !strcmp(expression->name, "VecFree") ||
       !strcmp(expression->name, "VecClone") ||
       !strcmp(expression->name, "VecSlice") ||
       !strcmp(expression->name, "VecClear") ||
       !strcmp(expression->name, "VecSwap") ||
       !strcmp(expression->name, "BuilderAppend") ||
       !strcmp(expression->name, "BuilderFinish")) {
        int first = expression->first_child;
        int second = first >= 0 ?
            emitter->function->exprs[first].next_sibling : -1;
        int third = second >= 0 ?
            emitter->function->exprs[second].next_sibling : -1;
        if(first < 0 ||
           !rust_owned_vec_type(
               emitter, emitter->function->exprs[first].type, NULL, NULL,
               element, sizeof(element))) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_expression(emitter, first, child, sizeof(child));
        if(!strcmp(expression->name, "VecFree")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecFree(&mut %s)", child);
        } else if(!strcmp(expression->name, "VecPush")) {
            char value[ZIR_RUST_TEXT_MAX];
            if(second < 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, value, sizeof(value));
            snprintf(output, size, "ZiranVecPush(&mut %s, %s)", child, value);
        } else if(!strcmp(expression->name, "VecPop")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecPop(&mut %s)", child);
        } else if(!strcmp(expression->name, "VecGet")) {
            char index[ZIR_RUST_TEXT_MAX];
            if(second < 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, index, sizeof(index));
            snprintf(output, size, "ZiranVecGet(&mut %s, %s as usize)",
                     child, index);
        } else if(!strcmp(expression->name, "VecClone")) {
            char source[ZIR_RUST_TEXT_MAX];
            if(third >= 0 || second < 0 ||
               strcmp(emitter->function->exprs[first].type,
                      emitter->function->exprs[second].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, source, sizeof(source));
            snprintf(output, size, "ZiranVecClone(&mut %s, &%s)",
                     child, source);
            return;
        } else if(!strcmp(expression->name, "VecSlice")) {
            char low[ZIR_RUST_TEXT_MAX];
            char high[ZIR_RUST_TEXT_MAX];
            int fourth = third >= 0 ?
                emitter->function->exprs[third].next_sibling : -1;
            if(third < 0 || fourth >= 0 ||
               !integer_type(emitter->function->exprs[second].type) ||
               !integer_type(emitter->function->exprs[third].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, low, sizeof(low));
            emit_expression(emitter, third, high, sizeof(high));
            snprintf(output, size,
                     "ZiranVecSlice(&%s, %s as isize, %s as isize)",
                    child, low, high);
            return;
        } else if(!strcmp(expression->name, "VecClear")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecClear(&mut %s)", child);
        } else if(!strcmp(expression->name, "VecSwap")) {
            char other[ZIR_RUST_TEXT_MAX];
            if(second < 0 || third >= 0 ||
               strcmp(emitter->function->exprs[first].type,
                      emitter->function->exprs[second].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, other, sizeof(other));
            snprintf(output, size, "core::mem::swap(&mut %s, &mut %s)",
                     child, other);
        } else if(!strcmp(expression->name, "BuilderAppend")) {
            char text[ZIR_RUST_TEXT_MAX];
            if(second < 0 || third >= 0 || strcmp(element, "u8") ||
               strcmp(emitter->function->exprs[second].type, "string")) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, text, sizeof(text));
            snprintf(output, size, "ZiranBuilderAppend(&mut %s, %s)",
                     child, text);
        } else {
            if(second >= 0 || strcmp(element, "u8")) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranBuilderFinish(&mut %s)", child);
        }
        return;
    }
    if(!strcmp(expression->name, "TextView")) {
        if(expression->first_child < 0 ||
           emitter->function->exprs[expression->first_child].next_sibling >= 0 ||
           strcmp(expression->type, "string") != 0) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_expression(emitter, expression->first_child, child,
                        sizeof(child));
        snprintf(output, size,
                 "ZiranText { data: %s.data as *const u8, len: %s.len }",
                 child, child);
        return;
    }
    if(expression->slot_type[0]) {
        char callable[ZIR_RUST_TEXT_MAX];
        char global_name[ZIR_RUST_NAME_MAX * 2];
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = FindType(emitter->module,
                                       expression->slot_type, &slot_owner);
        if(expression->left >= 0)
            emit_expression(emitter, expression->left, callable,
                            sizeof(callable));
        else if(global_reference(emitter, expression->name, global_name,
                                 sizeof(global_name)))
            snprintf(callable, sizeof(callable), "%s", global_name);
        else
            snprintf(callable, sizeof(callable), "%s",
                     local_name(emitter, expression->name));
        {
            /* Named arguments go to their checked parameter positions. */
            int ordered[32], count = 0;
            for(int child_index = expression->first_child;
                child_index >= 0 && count < 32;
                child_index = emitter->function->exprs[child_index].next_sibling)
                ordered[count++] = child_index;
            for(int position = 0; position < count; position++) {
                int chosen = ordered[position];
                for(int candidate = 0; candidate < count; candidate++)
                    if(emitter->function->exprs[ordered[candidate]].argument_index == position)
                        chosen = ordered[candidate];
                emit_expression(emitter, chosen, child, sizeof(child));
                if(*arguments)
                    strncat(arguments, ", ",
                            sizeof(arguments) - strlen(arguments) - 1);
                strncat(arguments, child,
                        sizeof(arguments) - strlen(arguments) - 1);
            }
        }
        if(slot != NULL && slot->is_procedure_type && !slot->is_c_call)
            snprintf(output, size, "unsafe { %s.call.unwrap()(%s.context%s%s) }",
                     callable, callable, *arguments ? ", " : "", arguments);
        else
            snprintf(output, size, "unsafe { %s.unwrap()(%s) }", callable, arguments);
        return;
    }
    if(expression->is_function_value) {
        unsupported_expression(emitter, expression);
        return;
    }
    /* print is a statement, lowered by emit_print_statement. */
    if(!strcmp(expression->name, "print")) {
        unsupported_expression(emitter, expression);
        return;
    }
    if(foreign != NULL) {
        rust_extern_symbol(emitter, emitter->module, foreign, symbol,
                           sizeof(symbol));
        for(int child_index = expression->first_child; child_index >= 0;
            child_index = emitter->function->exprs[child_index].next_sibling) {
            emit_expression(emitter, child_index, child, sizeof(child));
            if(*arguments)
                strncat(arguments, ", ",
                        sizeof(arguments) - strlen(arguments) - 1);
            strncat(arguments, child,
                    sizeof(arguments) - strlen(arguments) - 1);
        }
        snprintf(output, size, "unsafe { %s(%s) }", symbol, arguments);
        return;
    }
    if(ResolveFunctionAt(emitter->module, expression->name,
                         emitter->function->span.path, &owner, &callee) != 1 ||
       owner == NULL) {
        unsupported_expression(emitter, expression);
        return;
    }
    if(callee->is_extern || callee->is_template) {
        unsupported_expression(emitter, expression);
        return;
    }
    function_symbol(emitter, owner, callee, symbol, sizeof(symbol));
    {
        /* Named arguments go to their checked parameter positions. */
        int ordered[32], count = 0;
        for(int child_index = expression->first_child;
            child_index >= 0 && count < 32;
            child_index = emitter->function->exprs[child_index].next_sibling)
            ordered[count++] = child_index;
        int moved = 0, calls = 0;
        for(int position = 0; position < count; position++) {
            int chosen = -1;
            for(int candidate = 0; candidate < count; candidate++)
                if(emitter->function->exprs[ordered[candidate]].argument_index == position)
                    chosen = ordered[candidate];
            moved |= chosen >= 0 && chosen != ordered[position];
        }
        for(int position = 0; position < count; position++)
            calls |= rust_expression_calls(emitter->function, ordered[position]);
        if(moved && calls) {
            /* Arguments evaluate in source order before the call, as on
             * the other targets, then pass in parameter order. */
            size_t used = (size_t)snprintf(output, size, "{ ");
            for(int position = 0; position < count; position++) {
                emit_expression(emitter, ordered[position], child, sizeof(child));
                used += (size_t)snprintf(output + used, size > used ? size - used : 0,
                                         "let argument_%d = %s; ", position, child);
            }
            used += (size_t)snprintf(output + used, size > used ? size - used : 0, "%s(", symbol);
            for(int position = 0; position < count; position++) {
                int source = position;
                for(int candidate = 0; candidate < count; candidate++)
                    if(emitter->function->exprs[ordered[candidate]].argument_index == position)
                        source = candidate;
                used += (size_t)snprintf(output + used, size > used ? size - used : 0,
                                         "%sargument_%d", position ? ", " : "", source);
            }
            snprintf(output + used, size > used ? size - used : 0, ") }");
            return;
        }
        for(int position = 0; position < count; position++) {
            int chosen = -1;
            for(int candidate = 0; candidate < count; candidate++)
                if(emitter->function->exprs[ordered[candidate]].argument_index == position)
                    chosen = ordered[candidate];
            if(chosen < 0)
                chosen = ordered[position];
            emit_expression(emitter, chosen, child, sizeof(child));
            if(*arguments)
                strncat(arguments, ", ", sizeof(arguments) - strlen(arguments) - 1);
            strncat(arguments, child, sizeof(arguments) - strlen(arguments) - 1);
        }
    }
    snprintf(output, size, "%s(%s)", symbol, arguments);
}

static int wrapping_operation(const char *operation)
{
    return !strcmp(operation, "+") || !strcmp(operation, "-") ||
           !strcmp(operation, "*") || !strcmp(operation, "/") ||
           !strcmp(operation, "%");
}

static const char *wrapping_method(const char *operation)
{
    if(!strcmp(operation, "+")) return "wrapping_add";
    if(!strcmp(operation, "-")) return "wrapping_sub";
    if(!strcmp(operation, "*")) return "wrapping_mul";
    if(!strcmp(operation, "/")) return "wrapping_div";
    if(!strcmp(operation, "%")) return "wrapping_rem";
    return NULL;
}

static void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size)
{
    const ZirExpr *expression;
    char left[ZIR_RUST_TEXT_MAX];
    char right[ZIR_RUST_TEXT_MAX];
    char third[ZIR_RUST_TEXT_MAX];
    if(index < 0 || index >= emitter->function->expr_count) {
        snprintf(output, size, "()");
        return;
    }
    expression = &emitter->function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_IDENT: {
        char global_name[ZIR_RUST_NAME_MAX * 2];
        const ZirModule *function_owner = NULL;
        const ZirFunction *function = NULL;
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = NULL;
        if(!strcmp(expression->type, "null") || !strcmp(expression->name, "null"))
            snprintf(output, size, "core::ptr::null_mut()");
        else if(global_reference(emitter, expression->name, global_name,
                            sizeof(global_name)))
            snprintf(output, size, "%s", global_name);
        else if(expression->is_function_value &&
                rust_procedure_type(emitter, expression->type,
                                    &slot_owner, &slot) &&
                !slot->is_c_call) {
            char wrapper[ZIR_RUST_NAME_MAX * 2];
            char type_name[ZIR_NAME_MAX];
            slot_wrapper_symbol(emitter, index, wrapper, sizeof(wrapper));
            NativeTypeName(slot_owner, slot, type_name, sizeof(type_name));
            snprintf(output, size,
                     "%s { context: core::ptr::null_mut(), call: Some(%s) }",
                     type_name, wrapper);
        }
        else if(expression->is_function_value &&
                ResolveFunctionAt(emitter->module, expression->name,
                                  emitter->function->span.path,
                                  &function_owner, &function) == 1 &&
                function_owner != NULL && function != NULL) {
            char function_name[ZIR_RUST_NAME_MAX * 2];
            function_symbol(emitter, function_owner, function,
                            function_name, sizeof(function_name));
            snprintf(output, size, "Some(%s)", function_name);
        }
        else
            snprintf(output, size, "%s", local_name(emitter, expression->name));
        break;
    }
    case ZIR_EXPR_INT:
        emit_integer_literal(expression, output, size);
        break;
    case ZIR_EXPR_STRING:
        if(!rust_string_literal(expression->text, output, size))
            unsupported_expression(emitter, expression);
        break;
    case ZIR_EXPR_FLOAT:
        /* An untyped float takes its type from context, like 1.5 in Rust. */
        if(!strcmp(expression->type, "real"))
            snprintf(output, size, "%s%s", expression->text,
                     strpbrk(expression->text, ".eE") ? "" : ".0");
        else
            snprintf(output, size, "%s%s", expression->text,
                     !strcmp(expression->type, "float32") ? "f32" : "f64");
        break;
    case ZIR_EXPR_CALL:
        emit_call(emitter, expression, output, size);
        break;
    case ZIR_EXPR_MEMBER: {
        char base[ZIR_RUST_TEXT_MAX];
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        char base_type[ZIR_NAME_MAX];
        snprintf(base_type, sizeof(base_type), "%s",
                 emitter->function->exprs[expression->left].type);
        if((SliceElementType(base_type, NULL, 0) ||
            !strcmp(base_type, "string")) &&
           !strcmp(expression->name, "count")) {
            emit_expression(emitter, expression->left, base, sizeof(base));
            snprintf(output, size, "(%s.len as i64)", base);
            break;
        }
        if(rust_owned_vec_type(emitter, base_type, NULL, NULL, NULL, 0) &&
           (!strcmp(expression->name, "count") ||
            !strcmp(expression->name, "capacity"))) {
            emit_expression(emitter, expression->left, base, sizeof(base));
            snprintf(output, size, "(%s.%s as i64)", base,
                     expression->name);
            break;
        }
        {
            int capacity = 0;
            if(ArrayElementType(base_type, NULL, 0, &capacity)) {
                if(!strcmp(expression->name, "count")) {
                    snprintf(output, size, "%di64", capacity);
                    break;
                }
                if(!strcmp(expression->name, "data")) {
                    /* An empty array has no storage: its data is null. */
                    if(capacity == 0) {
                        char element_type[ZIR_NAME_MAX];
                        char element[ZIR_NAME_MAX];
                        ArrayElementType(base_type, element, sizeof(element), NULL);
                        if(rust_type(emitter, element, element_type, sizeof(element_type)))
                            snprintf(output, size, "core::ptr::null_mut::<%s>()", element_type);
                        else
                            snprintf(output, size, "core::ptr::null_mut()");
                        break;
                    }
                    emit_destination(emitter, expression->left, base, sizeof(base));
                    snprintf(output, size, "%s.as_mut_ptr()", base);
                    break;
                }
            }
            if(SliceElementType(base_type, NULL, 0) && !strcmp(expression->name, "data")) {
                emit_expression(emitter, expression->left, base, sizeof(base));
                snprintf(output, size, "%s.data", base);
                break;
            }
        }
        /* A field through a pointer reads the pointee: (*node).value. */
        if(base_type[0] == '*' && rust_record_type(emitter, base_type + 1, &owner, &record)) {
            emit_expression(emitter, expression->left, base, sizeof(base));
            rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
            snprintf(output, size, "(*%s).%s", base, field);
            break;
        }
        if(!rust_record_type(emitter, base_type, &owner, &record)) {
            unsupported_expression(emitter, expression);
            break;
        }
        emit_expression(emitter, expression->left, base, sizeof(base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "%s.%s", base, field);
        break;
    }
    case ZIR_EXPR_POINTER_MEMBER: {
        char base[ZIR_RUST_TEXT_MAX];
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        const char *base_type = emitter->function->exprs[expression->left].type;
        if(base_type[0] != '*' ||
           !rust_record_type(emitter, base_type + 1, &owner, &record)) {
            unsupported_expression(emitter, expression);
            break;
        }
        emit_expression(emitter, expression->left, base, sizeof(base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "(*%s).%s", base, field);
        break;
    }
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];
        char base[ZIR_RUST_TEXT_MAX];
        char index[ZIR_RUST_TEXT_MAX];
        ZirExprKind base_kind = emitter->function->exprs[expression->left].kind;
        /* Only places are borrowed; a literal or call result is indexed as a value. */
        if(base_kind == ZIR_EXPR_COMPOUND || base_kind == ZIR_EXPR_CALL ||
           base_kind == ZIR_EXPR_CONDITIONAL)
            emit_expression(emitter, expression->left, base, sizeof(base));
        else
            emit_destination(emitter, expression->left, base, sizeof(base));
        emit_expression(emitter, expression->right, index, sizeof(index));
        if(!strcmp(emitter->function->exprs[expression->left].type,
                   "string")) {
            snprintf(output, size, "%s.at(%s as i64)", base, index);
            break;
        }
        if(emitter->function->exprs[expression->left].type[0] == '*') {
            snprintf(output, size, "(*%s.offset(%s as isize))", base, index);
            break;
        }
        if(rust_owned_vec_type(
               emitter, emitter->function->exprs[expression->left].type,
               NULL, NULL, element, sizeof(element))) {
            snprintf(output, size, "%s.at(%s as i64)", base, index);
            break;
        }
        if(SliceElementType(
               emitter->function->exprs[expression->left].type, element,
               sizeof(element))) {
            snprintf(output, size, "%s.get(%s as i64)", base, index);
        } else {
            snprintf(output, size, "%s[%s as usize]", base, index);
        }
        break;
    }
    case ZIR_EXPR_SLICE: {
        char base[ZIR_RUST_TEXT_MAX];
        char element[ZIR_NAME_MAX];
        int capacity = 0;
        emit_destination(emitter, expression->left, base, sizeof(base));
        if(!strcmp(emitter->function->exprs[expression->left].type,
                   "string")) {
            char low[ZIR_RUST_TEXT_MAX];
            char high[ZIR_RUST_TEXT_MAX];
            emit_expression(emitter, expression->right, low, sizeof(low));
            emit_expression(emitter, expression->third, high, sizeof(high));
            snprintf(output, size,
                     "ZiranText::slice(%s, %s as isize, %s as isize)",
                     base, low, high);
            break;
        }
        {
            /* Bounds come from the checked graph; a missing bound is the
             * start or the end. Out-of-range views fail like other targets. */
            const char *base_type = emitter->function->exprs[expression->left].type;
            int array = ArrayElementType(base_type, element, sizeof(element), &capacity);
            int slice = !array && SliceElementType(base_type, element, sizeof(element));
            char low[ZIR_RUST_TEXT_MAX] = "0i64";
            char high[ZIR_RUST_TEXT_MAX];
            if(!array && !slice) {
                unsupported_expression(emitter, expression);
                break;
            }
            if(expression->right >= 0)
                emit_typed_expression(emitter, expression->right, "s64", low, sizeof(low));
            if(expression->third >= 0)
                emit_typed_expression(emitter, expression->third, "s64", high, sizeof(high));
            else if(array)
                snprintf(high, sizeof(high), "%di64", capacity);
            else
                high[0] = '\0';
            if(array)
                snprintf(output, size,
                         "ZiranSlice::view(ZiranSlice { data: %s.as_mut_ptr(), len: %d }, %s, %s)",
                         base, capacity, low, high);
            else if(high[0])
                snprintf(output, size, "ZiranSlice::view(%s, %s, %s)", base, low, high);
            else
                snprintf(output, size, "ZiranSlice::view_from(%s, %s)", base, low);
        }
        break;
    }
    case ZIR_EXPR_COMPOUND: {
        char type_name[ZIR_NAME_MAX];
        const ZirModule *record_owner = NULL;
        const ZirType *record = NULL;
        int first = 1;
        char element[ZIR_NAME_MAX];
        int capacity = 0;
        if(ArrayElementType(expression->type, element, sizeof(element),
                            &capacity)) {
            snprintf(output, size, "[");
            for(int child_index = expression->first_child; child_index >= 0;
                child_index =
                    emitter->function->exprs[child_index].next_sibling) {
                const ZirExpr *initializer =
                    &emitter->function->exprs[child_index];
                char value[ZIR_RUST_TEXT_MAX];
                emit_typed_expression(emitter,
                                      initializer->kind == ZIR_EXPR_FIELD_INIT ?
                                          initializer->right : child_index,
                                      element, value, sizeof(value));
                size_t used = strlen(output);
                snprintf(output + used, size - used, "%s%s", first ? "" : ", ",
                         value);
                first = 0;
            }
            size_t used = strlen(output);
            snprintf(output + used, size - used, "]");
            break;
        }
        if(!rust_type(emitter, expression->type, type_name,
                      sizeof(type_name))) {
            unsupported_expression(emitter, expression);
            break;
        }
        {
            /* A generic type in expression position needs ::<T>. */
            const char *generic = strchr(type_name, '<');
            if(generic != NULL)
                snprintf(output, size, "%.*s::%s { ", (int)(generic - type_name),
                         type_name, generic);
            else
                snprintf(output, size, "%s { ", type_name);
        }
        rust_record_type(emitter, expression->type, &record_owner, &record);
        for(int child_index = expression->first_child; child_index >= 0;
            child_index = emitter->function->exprs[child_index].next_sibling) {
            const ZirExpr *initializer = &emitter->function->exprs[child_index];
            char value[ZIR_RUST_TEXT_MAX];
            char field_name[ZIR_NAME_MAX];
            if(initializer->kind != ZIR_EXPR_FIELD_INIT) {
                unsupported_expression(emitter, initializer);
                return;
            }
            emit_expression(emitter, initializer->right, value, sizeof(value));
            size_t used = strlen(output);
            rust_field_name(record, initializer->name, field_name,
                            sizeof(field_name));
            snprintf(output + used, size - used, "%s%s: %s",
                     first ? "" : ", ", field_name, value);
            first = 0;
        }
        /* Fields a literal leaves out start at their zero value. */
        if(record != NULL && !record->is_union) {
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(record, &offset, &field) == 1) {
                int given = 0;
                for(int child_index = expression->first_child; child_index >= 0;
                    child_index = emitter->function->exprs[child_index].next_sibling)
                    if(!strcmp(emitter->function->exprs[child_index].name, field.name))
                        given = 1;
                if(given)
                    continue;
                char value[ZIR_RUST_TEXT_MAX];
                char field_name[ZIR_NAME_MAX];
                const ZirModule *saved_module = emitter->module;
                if(record_owner != NULL)
                    emitter->module = record_owner;
                rust_zero_value(emitter, field.type, value, sizeof(value));
                emitter->module = saved_module;
                rust_field_name(record, field.name, field_name, sizeof(field_name));
                size_t used = strlen(output);
                snprintf(output + used, size - used, "%s%s: %s",
                         first ? "" : ", ", field_name, value);
                first = 0;
            }
        }
        size_t used = strlen(output);
        snprintf(output + used, size - used, "%s}", first ? "" : " ");
        break;
    }
    case ZIR_EXPR_BINARY:
        left[0] = right[0] = '\0';
        if(!strcmp(expression->op, "==") || !strcmp(expression->op, "!=")) {
            const ZirExpr *left_expr = expression->left >= 0 ?
                &emitter->function->exprs[expression->left] : NULL;
            const ZirExpr *right_expr = expression->right >= 0 ?
                &emitter->function->exprs[expression->right] : NULL;
            int left_null = left_expr != NULL &&
                !strcmp(left_expr->type, "null");
            int right_null = right_expr != NULL &&
                !strcmp(right_expr->type, "null");
            const char *slot_type = left_null && right_expr != NULL ?
                right_expr->type : right_null && left_expr != NULL ?
                left_expr->type : NULL;
            const ZirModule *slot_owner = NULL;
            const ZirType *slot = slot_type != NULL ?
                FindType(emitter->module, slot_type, &slot_owner) : NULL;
            int equal = !strcmp(expression->op, "==");
            if(slot == NULL && left_expr != NULL && right_expr != NULL)
                slot = FindType(emitter->module, left_expr->type, &slot_owner);
            if(slot != NULL && slot->is_procedure_type) {
                int value_index = left_null ? expression->right :
                    expression->left;
                emit_expression(emitter, value_index, left, sizeof(left));
                if(left_null || right_null) {
                    if(slot->is_c_call)
                        snprintf(output, size, "(%s.is_%s())", left,
                                 equal ? "none" : "some");
                    else
                        snprintf(output, size, "(%s.call.is_%s())", left,
                                 equal ? "none" : "some");
                } else {
                    emit_expression(emitter, expression->right, right,
                                    sizeof(right));
                    if(slot->is_c_call)
                        snprintf(output, size,
                                 "(match (%s, %s) { (left, right) => left %s right })",
                                 left, right, equal ? "==" : "!=");
                    else
                        snprintf(output, size,
                                 "(match (%s, %s) { (left, right) => left.call %s right.call %s left.context %s right.context })",
                                 left, right, equal ? "==" : "!=",
                                 equal ? "&&" : "||",
                                 equal ? "==" : "!=");
                }
                break;
            }
        }
        if((expression->left >= 0 &&
            !strcmp(emitter->function->exprs[expression->left].type,
                    "string")) ||
           (expression->right >= 0 &&
            !strcmp(emitter->function->exprs[expression->right].type,
                    "string"))) {
           if(strcmp(expression->op, "==") != 0 &&
              strcmp(expression->op, "!=") != 0)
               unsupported_expression(emitter, expression);
            emit_expression(emitter, expression->left, left, sizeof(left));
            emit_expression(emitter, expression->right, right, sizeof(right));
            snprintf(output, size, "%sZiranText::eq(%s, %s)",
                     !strcmp(expression->op, "==") ? "" : "!", left, right);
            break;
        }
        /* An enum's arithmetic wraps at its backing integer type. */
        const char *arithmetic_type = expression->type;
        {
            const ZirModule *enum_owner = NULL;
            const ZirType *enumeration = NULL;
            if(rust_enum_type(emitter, expression->type, &enum_owner, &enumeration))
                arithmetic_type = enumeration->enum_backing;
        }
        int wrapping = integer_type(arithmetic_type) &&
                       wrapping_operation(expression->op);
        if(!wrapping) {
            /* Untyped literal operands take the operation's integer type;
             * two compared with each other are s64, as on other targets. */
            int untyped = expression->left >= 0 && expression->right >= 0 &&
                !strcmp(emitter->function->exprs[expression->left].type, "integer") &&
                !strcmp(emitter->function->exprs[expression->right].type, "integer");
            const char *operand_type = integer_type(expression->type) &&
                strcmp(expression->type, "integer") != 0 ? expression->type : "s64";
            if(untyped) {
                emit_typed_expression(emitter, expression->left, operand_type, left, sizeof(left));
                emit_typed_expression(emitter, expression->right, operand_type, right, sizeof(right));
            } else {
                emit_expression(emitter, expression->left, left, sizeof(left));
                emit_expression(emitter, expression->right, right, sizeof(right));
            }
        }
        if(wrapping) {
            emit_typed_expression(emitter, expression->left, expression->type,
                                  left, sizeof(left));
            emit_typed_expression(emitter, expression->right, expression->type,
                                  right, sizeof(right));
            uint64_t left_bits, right_bits, folded;
            if(rust_literal_bits(left, expression->type, &left_bits) &&
               rust_literal_bits(right, expression->type, &right_bits) &&
               FoldIntegerOperation(expression->op, expression->type,
                                    left_bits, right_bits, &folded))
                rust_literal(expression->type, folded, output, size);
            else if(!strcmp(expression->type, "integer"))
                snprintf(output, size,
                         "((%s as i64).%s(%s as i64))", left,
                         wrapping_method(expression->op), right);
            else
                snprintf(output, size, rust_plain_receiver(left) ? "%s.%s(%s)" : "(%s).%s(%s)",
                         left, wrapping_method(expression->op), right);
        }
        else if(!strcmp(expression->op, "<<") || !strcmp(expression->op, ">>")) {
            /* A count below zero or at the width fails, as on other targets;
             * >> on a signed type is arithmetic. */
            char count[ZIR_RUST_TEXT_MAX];
            emit_typed_expression(emitter, expression->left, expression->type,
                                  left, sizeof(left));
            emit_expression(emitter, expression->right, count, sizeof(count));
            snprintf(output, size,
                     "%s%s%s.%s(u32::try_from(%s).unwrap_or(u32::MAX)).expect(\"shift count out of range\")",
                     rust_plain_receiver(left) ? "" : "(", left,
                     rust_plain_receiver(left) ? "" : ")",
                     !strcmp(expression->op, "<<") ? "checked_shl" : "checked_shr", count);
        }
        else
            snprintf(output, size, "(%s %s %s)", left, expression->op, right);
        break;
    case ZIR_EXPR_UNARY:
        if(!strcmp(expression->op, "&")) {
            char pointee[ZIR_NAME_MAX];
            emit_destination(emitter, expression->right, right, sizeof(right));
            if(!rust_type(emitter, emitter->function->exprs[expression->right].type,
                          pointee, sizeof(pointee))) {
                unsupported_expression(emitter, expression);
                break;
            }
            snprintf(output, size, "(&mut %s as *mut %s)", right, pointee);
            break;
        }
        emit_expression(emitter, expression->right, right, sizeof(right));
        if(!strcmp(expression->op, "!") || !strcmp(expression->op, "-") ||
           !strcmp(expression->op, "+"))
            snprintf(output, size, "(%s%s)", expression->op, right);
        else if(!strcmp(expression->op, "~"))
            snprintf(output, size, "(!%s)", right);
        else if(!strcmp(expression->op, "*"))
            snprintf(output, size, "(*%s)", right);
        else
            unsupported_expression(emitter, expression);
        break;
    case ZIR_EXPR_CAST: {
        char target[ZIR_NAME_MAX];
        if(!strcmp(expression->type, "null")) {
            snprintf(output, size, "core::ptr::null_mut()");
            break;
        }
        emit_expression(emitter, expression->right, right, sizeof(right));
        {
            /* A literal converts at compile time to its wrapped value;
             * Rust rejects 258 as u8. */
            const ZirExpr *operand = &emitter->function->exprs[expression->right];
            size_t length = strspn(right, "0123456789");
            if(operand->kind == ZIR_EXPR_INT && integer_type(expression->type) &&
               strcmp(expression->type, "integer") != 0 && length > 0 &&
               strspn(right + length, "iu0123456789") == strlen(right + length)) {
                errno = 0;
                unsigned long long value = strtoull(right, NULL, 10);
                if(errno == 0) {
                    rust_literal(expression->type, (uint64_t)value, output, size);
                    break;
                }
            }
        }
        if(rust_scalar_type(expression->type) != NULL)
            snprintf(output, size, "(%s as %s)", right,
                     rust_scalar_type(expression->type));
        else if(rust_type(emitter, expression->type, target, sizeof(target)))
            snprintf(output, size, "(%s as %s)", right, target);
        else
            unsupported_expression(emitter, expression);
        break;
    }
    case ZIR_EXPR_SIZE_OF: {
        size_t type_size, alignment;
        if(!TypeLayout(emitter->module, expression->name, &type_size, &alignment)) {
            unsupported_expression(emitter, expression);
            break;
        }
        snprintf(output, size, "%zui64", type_size);
        break;
    }
    case ZIR_EXPR_COMPILE_TIME:
        snprintf(output, size, "false");
        break;
    case ZIR_EXPR_CONDITIONAL:
        emit_expression(emitter, expression->left, left, sizeof(left));
        emit_expression(emitter, expression->right, right, sizeof(right));
        emit_expression(emitter, expression->third, third, sizeof(third));
        snprintf(output, size, "(if %s { %s } else { %s })", left, right,
                 third);
        break;
    default:
        unsupported_expression(emitter, expression);
    }
}

static int split_arguments(const char *text, char parts[][ZIR_RUST_TEXT_MAX],
                           int maximum)
{
    int count = 0;
    int depth = 0;
    const char *start = text;
    for(const char *cursor = text; ; cursor++) {
        if(*cursor != '\0' && (*cursor == '(' || *cursor == '['))
            depth++;
        else if(*cursor != '\0' && (*cursor == ')' || *cursor == ']'))
            depth--;
        else if((*cursor == ',' && depth == 0) || *cursor == '\0') {
            size_t length = (size_t)(cursor - start);
            if(count >= maximum)
                return -1;
            if(length >= ZIR_RUST_TEXT_MAX)
                length = ZIR_RUST_TEXT_MAX - 1;
            memcpy(parts[count], start, length);
            parts[count][length] = '\0';
            count++;
            if(*cursor == '\0')
                return count;
            start = cursor + 1;
        }
    }
}

static int block_end(const ZirFunction *function, int begin, int end)
{
    int depth = 1;
    for(int index = begin + 1; index < end; index++) {
        ZirStmtKind kind = function->stmts[index].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return index;
    }
    return end;
}

static void emit_sequence(RustEmitter *emitter, int begin, int end);

static int emit_if(RustEmitter *emitter, int index, int end)
{
    const ZirStmt *statement = &emitter->function->stmts[index];
    char condition[ZIR_RUST_TEXT_MAX];
    int close = block_end(emitter->function, index, end);
    emit_expression(emitter, statement->expr_root, condition,
                    sizeof(condition));
    char plain_condition[ZIR_RUST_TEXT_MAX];
    write_line(emitter, "if %s {", rust_bare(condition, plain_condition, sizeof(plain_condition)));
    emitter->indent++;
    emit_sequence(emitter, index + 1, close);
    emitter->indent--;
    if(close + 1 < end &&
       emitter->function->stmts[close + 1].kind == ZIR_STMT_IF &&
       emitter->function->stmts[close + 1].is_else) {
        int next = close + 1;
        write_line(emitter, "} else {");
        emitter->indent++;
        if(emitter->function->stmts[next].expr_root >= 0) {
            close = emit_if(emitter, next, end);
        } else {
            close = block_end(emitter->function, next, end);
            emit_sequence(emitter, next + 1, close);
        }
        emitter->indent--;
    }
    write_line(emitter, "}");
    return close;
}

static void emit_compound_assignment(RustEmitter *emitter,
                                     const ZirStmt *statement,
                                     const char *destination_text,
                                     const char *value)
{
    char operation[4];
    const ZirExpr *left = &emitter->function->exprs[statement->lhs_root];
    const char *name = destination_text;
    snprintf(operation, sizeof(operation), "%s", statement->assignment_op);
    operation[strlen(operation) - 1] = '\0';
    const char *arithmetic_type = left->type;
    {
        const ZirModule *enum_owner = NULL;
        const ZirType *enumeration = NULL;
        if(rust_enum_type(emitter, left->type, &enum_owner, &enumeration))
            arithmetic_type = enumeration->enum_backing;
    }
    if(integer_type(arithmetic_type) && wrapping_operation(operation)) {
        write_line(emitter, rust_plain_receiver(name) ? "%s = %s.%s(%s);" : "%s = (%s).%s(%s);", name, name,
                   wrapping_method(operation), value);
        return;
    }
    if(!strcmp(operation, "<<") || !strcmp(operation, ">>")) {
        write_line(emitter,
                   "%s = %s.%s(u32::try_from(%s).unwrap_or(u32::MAX)).expect(\"shift count out of range\");",
                   name, name, !strcmp(operation, "<<") ? "checked_shl" : "checked_shr", value);
        return;
    }
    if(!strcmp(operation, "~")) {
        Diagnostic(statement->span, "zir_rust.assignment",
                   "unsupported assignment operation in the Rust target: %s",
                   statement->assignment_op);
        exit(1);
    }
    write_line(emitter, "%s = %s %s %s;", name, name, operation, value);
}

static void rust_zero_value(RustEmitter *emitter, const char *type,
                            char *output, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    char type_name[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(!strcmp(type, "string")) {
        snprintf(output, size, "ZiranText::new(\"\")");
        return;
    }
    if(rust_owned_vec_type(emitter, type, NULL, NULL, element,
                           sizeof(element))) {
        snprintf(output, size, "ZiranVec::new()");
        return;
    }
    if(rust_procedure_type(emitter, type, &owner, &record)) {
        char type_name[ZIR_NAME_MAX];
        NativeTypeName(owner, record, type_name, sizeof(type_name));
        if(!record->is_c_call) {
            snprintf(output, size,
                     "%s { context: core::ptr::null_mut(), call: None }",
                     type_name);
            return;
        }
        snprintf(output, size, "None");
        return;
    }
    if(rust_option_type(emitter, type, &owner, &record, element,
                        sizeof(element))) {
        char value[ZIR_RUST_TEXT_MAX];
        rust_zero_value(emitter, element, value, sizeof(value));
        snprintf(output, size,
                 "ZiranOption { has_value: false, value: %s }", value);
        return;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        char element_type[ZIR_NAME_MAX];
        rust_type(emitter, element, element_type, sizeof(element_type));
        snprintf(output, size,
                 "ZiranSlice::<%s> { data: core::ptr::null_mut(), len: 0 }",
                 element_type);
        return;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        char zero[ZIR_RUST_TEXT_MAX];
        rust_zero_value(emitter, element, zero, sizeof(zero));
        /* [x; N] needs Copy elements; others are listed. */
        if(rust_copyable_type(emitter, element) || capacity == 0)
            snprintf(output, size, "[%s; %d]", zero, capacity);
        else {
            size_t used = (size_t)snprintf(output, size, "[");
            for(int index = 0; index < capacity && used < size; index++)
                used += (size_t)snprintf(output + used, size - used, "%s%s",
                                         index ? ", " : "", zero);
            if(used < size)
                snprintf(output + used, size - used, "]");
        }
        return;
    }
    if(type[0] == '*') {
        snprintf(output, size, "core::ptr::null_mut()");
        return;
    }
    if(!strcmp(type, "bool")) {
        snprintf(output, size, "false");
        return;
    }
    if(float_type(type)) {
        snprintf(output, size, "0.0");
        return;
    }
    if(!rust_record_type(emitter, type, &owner, &record) ||
       !rust_type(emitter, type, type_name, sizeof(type_name))) {
        snprintf(output, size, "0");
        return;
    }
    /* A union's zero value is all of its bytes zero, whichever field is
     * read, as on the other targets. */
    if(record->is_union) {
        snprintf(output, size, "unsafe { core::mem::zeroed::<%s>() }", type_name);
        return;
    }
    snprintf(output, size, "%s { ", type_name);
    size_t used = 0;
    size_t offset = 0;
    ZirTypeField field;
    int first = 1;
    while(TypeNextField(record, &offset, &field) == 1 && (first || !record->is_union)) {
        char field_name[ZIR_NAME_MAX];
        char value[ZIR_RUST_TEXT_MAX];
        /* Field types are spelled in the record's own module. A union
         * sets only its first field. */
        const ZirModule *saved_module = emitter->module;
        if(owner != NULL)
            emitter->module = owner;
        rust_field_name(record, field.name, field_name, sizeof(field_name));
        rust_zero_value(emitter, field.type, value, sizeof(value));
        emitter->module = saved_module;
        used = strlen(output);
        snprintf(output + used, size - used, "%s%s: %s", first ? "" : ", ",
                 field_name, value);
        first = 0;
    }
    used = strlen(output);
    snprintf(output + used, size - used, " }");
}

/* The binding a place expression writes through, or NULL. */
static const char *rust_place_base(const ZirFunction *function, int index)
{
    while(index >= 0 && index < function->expr_count) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        if(expression->kind != ZIR_EXPR_MEMBER && expression->kind != ZIR_EXPR_INDEX &&
           expression->kind != ZIR_EXPR_SLICE)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

/* A scalar or text local needs `mut` only when something may change it:
 * an assignment to it, a borrow of it, or a Vec or builder operation on
 * it. Other types, and names another declaration reuses, keep `mut`. */
static int rust_binding_mutable(const ZirFunction *function, int declaration)
{
    const ZirStmt *declared = &function->stmts[declaration];
    const char *name = declared->name;
    if(strcmp(declared->type, "string") != 0 &&
       (rust_scalar_type(declared->type) == NULL || !strcmp(declared->type, "void")))
        return 1;
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        const char *base;
        if(index != declaration && statement->kind == ZIR_STMT_DECL &&
           !strcmp(statement->name, name))
            return 1;
        if(statement->kind == ZIR_STMT_ASSIGN &&
           (base = rust_place_base(function, statement->lhs_root)) != NULL &&
           !strcmp(base, name))
            return 1;
    }
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *expression = &function->exprs[index];
        const char *base = NULL;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&"))
            base = rust_place_base(function, expression->right);
        else if(expression->kind == ZIR_EXPR_CALL &&
                (!strncmp(expression->name, "Vec", 3) ||
                 !strncmp(expression->name, "Builder", 7)))
            base = rust_place_base(function, expression->first_child);
        if(base != NULL && !strcmp(base, name))
            return 1;
    }
    return 0;
}

static void emit_sequence(RustEmitter *emitter, int begin, int end)
{
    char value[ZIR_RUST_TEXT_MAX];
    char type_name[ZIR_NAME_MAX];
    char destination[ZIR_RUST_TEXT_MAX];
    for(int index = begin; index < end; index++) {
        const ZirStmt *statement = &emitter->function->stmts[index];
        switch(statement->kind) {
        case ZIR_STMT_DECL:
            require_rust_type(emitter, statement->span, statement->type,
                              type_name, sizeof(type_name));
            /* The value is lowered before the name is bound, so an
             * initializer can read a shadowed outer binding. */
            if(statement->expr_root >= 0)
                emit_typed_expression(emitter, statement->expr_root,
                                      statement->type, value, sizeof(value));
            else
                rust_zero_value(emitter, statement->type, value, sizeof(value));
            register_local(emitter, statement->name);
            write_line(emitter, "let %s%s: %s = %s;",
                       rust_binding_mutable(emitter->function, index) ? "mut " : "",
                       local_name(emitter, statement->name), type_name, value);
            break;
        case ZIR_STMT_ASSIGN: {
            const ZirExpr *target = &emitter->function->exprs[statement->lhs_root];
            emit_destination(emitter, statement->lhs_root, destination,
                             sizeof(destination));
            emit_typed_expression(emitter, statement->expr_root,
                                  emitter->function->exprs[statement->lhs_root].type,
                                  value, sizeof(value));
            if(strcmp(statement->assignment_op, "=") == 0)
                write_line(emitter, "%s = %s;", destination, value);
            else
                emit_compound_assignment(emitter, statement, destination,
                                         value);
            break;
        }
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED:
            if(statement->expr_root < 0)
                break;
            if(emitter->function->exprs[statement->expr_root].kind == ZIR_EXPR_CALL &&
               !strcmp(emitter->function->exprs[statement->expr_root].name, "print") &&
               emit_print_statement(emitter,
                                    &emitter->function->exprs[statement->expr_root]))
                break;
            emit_expression(emitter, statement->expr_root, value,
                            sizeof(value));
            write_line(emitter, "let _ = %s;", value);
            break;
        case ZIR_STMT_IF:
            index = emit_if(emitter, index, end);
            break;
        case ZIR_STMT_WHILE: {
            int close = block_end(emitter->function, index, end);
            emit_expression(emitter, statement->expr_root, value,
                            sizeof(value));
            if(statement->is_parallel || statement->is_gpu)
                write_line(emitter,
                           "// ziran: #parallel region downgraded to serial");
            char plain_value[ZIR_RUST_TEXT_MAX];
            /* A loop a named break or continue targets carries a label. */
            int labeled = 0;
            for(int target = 0; target < emitter->function->stmt_count; target++)
                if(statement->loop_id &&
                   emitter->function->stmts[target].target_id == statement->loop_id)
                    labeled = 1;
            if(labeled)
                write_line(emitter, "'zir_loop_%d: while %s {", statement->loop_id,
                           rust_bare(value, plain_value, sizeof(plain_value)));
            else
                write_line(emitter, "while %s {", rust_bare(value, plain_value, sizeof(plain_value)));
            emitter->indent++;
            emit_sequence(emitter, index + 1, close);
            emitter->indent--;
            write_line(emitter, "}");
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close = block_end(emitter->function, index, end);
            write_line(emitter, "{");
            emitter->indent++;
            emit_sequence(emitter, index + 1, close);
            emitter->indent--;
            write_line(emitter, "}");
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_CLOSE:
            break;
        case ZIR_STMT_RETURN:
            if(statement->expr_root >= 0) {
                emit_typed_expression(emitter, statement->expr_root,
                                      emitter->function->return_type, value,
                                      sizeof(value));
                write_line(emitter, "return %s;", value);
            } else
                write_line(emitter, "return;");
            break;
        case ZIR_STMT_BREAK:
        case ZIR_STMT_CONTINUE:
            if(statement->target_id)
                write_line(emitter, "%s 'zir_loop_%d;",
                           statement->kind == ZIR_STMT_BREAK ? "break" : "continue",
                           statement->target_id);
            else
                write_line(emitter, "%s;",
                           statement->kind == ZIR_STMT_BREAK ? "break" : "continue");
            break;
        case ZIR_STMT_UNREACHABLE:
            write_line(emitter, "unreachable!();");
            break;
        default:
            goto unsupported_statement;
        }
        continue;
unsupported_statement:
        Diagnostic(statement->span, "zir_rust.statement",
                   "unsupported statement in the Rust target: %s",
                   statement->text[0] ? statement->text :
                       StmtKindName(statement->kind));
        exit(1);
    }
}

static void validate_module(const ZirModule *module)
{
    RustEmitter emitter = {0};
    emitter.module = module;
    for(int index = 0; index < module->global_count; index++) {
        const ZirGlobal *global = &module->globals[index];
        char type_name[ZIR_NAME_MAX];
        require_rust_type(&emitter, global->span, global->type, type_name,
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
        if(rust_owned_vec_type(&emitter, record->name, NULL, NULL,
                               checked_type, sizeof(checked_type)) ||
           rust_option_type(&emitter, record->name, NULL, NULL, checked_type,
                            sizeof(checked_type))) {
            if(!rust_type(&emitter, checked_type, checked_type,
                          sizeof(checked_type))) {
                Diagnostic(record->span, "zir_rust.type",
                           "unsupported template type: %s", record->name);
                exit(1);
            }
            continue;
        }
        if(rust_enum_type(&emitter, record->name, &owner, &checked)) {
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
            if(!rust_type(&emitter, checked->procedure_return_type,
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
        if(!rust_record_type(&emitter, record->name, &owner, &checked)) {
            Diagnostic(record->span, "zir_rust.type",
                       "the Rust target cannot lower this record: %s",
                       record->name);
            exit(1);
        }
        size_t offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &offset, &field) == 1)
            if(!rust_type(&emitter, field.type, checked_type,
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
            char parts[32][ZIR_RUST_TEXT_MAX];
            int count = split_arguments(import->args, parts, 32);
            if(count < 0) {
                Diagnostic(import->span, "zir_rust.import",
                           "too many foreign parameters: %s", import->name);
                exit(1);
            }
            for(int parameter = 0; parameter < count; parameter++) {
                char *colon = strchr(parts[parameter], ':');
                char *type = colon != NULL ? colon + 1 : parts[parameter];
                while(*type == ' ' || *type == '\t')
                    type++;
                if(import->is_varargs && !strncmp(type, "..", 2))
                    continue;
                if(colon == NULL ||
                   !rust_type(&emitter, type, checked_type,
                              sizeof(checked_type))) {
                    Diagnostic(import->span, "zir_rust.import",
                               "unsupported foreign parameter type in %s",
                               import->name);
                    exit(1);
                }
            }
        }
        if(!rust_type(&emitter, import->return_type, checked_type,
                      sizeof(checked_type))) {
            Diagnostic(import->span, "zir_rust.import",
                       "unsupported foreign return type: %s", import->name);
            exit(1);
        }
    }
}

static void emit_slot_wrappers(RustEmitter *emitter, const ZirModule *module,
                               const ZirFunction *function)
{
    emitter->module = module;
    emitter->function = function;
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *value = &function->exprs[index];
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = FindType(module, value->type, &slot_owner);
        char parts[32][ZIR_RUST_TEXT_MAX];
        char wrapper[ZIR_RUST_NAME_MAX * 2];
        char symbol[ZIR_RUST_NAME_MAX * 2];
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        int count;
        if(!value->is_function_value || slot == NULL ||
           !slot->is_procedure_type || slot->is_c_call)
            continue;
        count = *slot->body ?
            split_arguments(slot->body, parts, 32) : 0;
        if(count < 0) {
            Diagnostic(value->span, "zir_rust.slot",
                       "too many procedure parameters: %s", value->type);
            exit(1);
        }
        if(!strcmp(value->name, "#this")) {
            owner = module;
            callee = function;
        } else if(ResolveFunctionAt(module, value->name, value->span.path,
                                    &owner, &callee) != 1 ||
                  owner == NULL || callee == NULL) {
            Diagnostic(value->span, "zir_rust.slot",
                       "unsupported procedure value: %s", value->name);
            exit(1);
        }
        slot_wrapper_symbol(emitter, index, wrapper, sizeof(wrapper));
        function_symbol(emitter, owner, callee, symbol, sizeof(symbol));
        fprintf(emitter->output,
                "unsafe extern \"C\" fn %s(_context: *mut core::ffi::c_void",
                wrapper);
        emitter->module = slot_owner;
        for(int argument = 0; argument < count; argument++) {
            char *colon = strchr(parts[argument], ':');
            char parameter_type[ZIR_NAME_MAX];
            const char *type;
            if(colon == NULL) {
                Diagnostic(value->span, "zir_rust.slot",
                           "invalid procedure parameter: %s",
                           parts[argument]);
                exit(1);
            }
            type = colon + 1;
            while(*type == ' ' || *type == '\t')
                type++;
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
        fputs(symbol, emitter->output);
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
                    if(ResolveFunctionAt(scope, expression->name, user->span.path,
                                         &owner, &target) == 1 &&
                       owner == module && target == function)
                        return 1;
                }
            }
        }
    return 0;
}

static void lower_function(RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function)
{
    char parts[32][ZIR_RUST_TEXT_MAX];
    char symbol[ZIR_RUST_NAME_MAX * 2];
    int count;
    /* Templates have no body of their own, only checked specializations,
     * and foreign declarations are emitted with the imports. */
    if(function->is_extern || function->is_template)
        return;
    emitter->module = module;
    emitter->function = function;
    emitter->local_count = 0;
    function_symbol(emitter, module, function, symbol, sizeof(symbol));
    /* Only what C can see or call keeps the C ABI: exported functions and
     * functions taken as procedure values. Others are ordinary Rust fns. */
    if(function->export_symbol[0] ||
       (function->exported && strcmp(function->name, "main") != 0))
        fputs("#[no_mangle]\npub extern \"C\" fn ", emitter->output);
    else if(rust_function_is_value(emitter, module, function))
        fputs("extern \"C\" fn ", emitter->output);
    else
        fputs("fn ", emitter->output);
    fputs(symbol, emitter->output);
    fputc('(', emitter->output);
    count = *function->args ? split_arguments(function->args, parts, 32) : 0;
    if(count < 0) {
        Diagnostic(function->span, "zir_rust.parameter",
                   "too many parameters in the Rust target: %s",
                   function->name);
        exit(1);
    }
    for(int index = 0; index < count; index++) {
        char *colon = strchr(parts[index], ':');
        char source_name[ZIR_NAME_MAX];
        char rust_name[ZIR_NAME_MAX];
        char *type;
        size_t length;
        if(colon == NULL) {
            Diagnostic(function->span, "zir_rust.parameter",
                       "invalid parameter declaration: %s", parts[index]);
            exit(1);
        }
        const char *name_start = parts[index];
        while(*name_start == ' ' || *name_start == '\t')
            name_start++;
        length = (size_t)(colon - name_start);
        while(length > 0 && isspace((unsigned char)name_start[length - 1]))
            length--;
        memcpy(source_name, name_start, length);
        source_name[length] = '\0';
        rust_identifier(source_name, rust_name, sizeof(rust_name));
        type = colon + 1;
        while(*type == ' ' || *type == '\t')
            type++;
        char parameter_type[ZIR_NAME_MAX];
        require_rust_type(emitter, function->span, type, parameter_type,
                          sizeof(parameter_type));
        fprintf(emitter->output, "%smut %s: %s", index ? ", " : "",
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
        FILE *body = open_memstream(&body_text, &body_size);
        if(body == NULL) {
            Diagnostic(function->span, "zir_rust.output", "cannot buffer Rust output");
            exit(1);
        }
        emitter->output = body;
        emitter->indent = 2;
        emit_slot_wrappers(emitter, module, function);
        emit_sequence(emitter, 0, function->stmt_count);
        fclose(body);
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
        snprintf(node->type, sizeof(node->type), "%s", type);
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
                        snprintf(entry->name, sizeof(entry->name), "%s", field.name);
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
            snprintf(entry->type, sizeof(entry->type), "%s", field_type);
            rust_fill_literal_types(emitter, literal, entry->right, field_type, depth + 1);
        } else
            rust_fill_literal_types(emitter, literal, child, field_type, depth + 1);
    }
}

static void rust_global_initializer(RustEmitter *emitter,
                                     const ZirGlobal *global, char *output,
                                     size_t size)
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
        ZirFunction probe = {0};
        int root = ParseExprTyped(&probe, emitter->module, global->init,
                                  global->span, global->type);
        /* Only literals of constants; a literal naming a procedure or
         * global is set up by module startup. */
        int constant = root >= 0;
        for(int index = 0; index < probe.expr_count && constant; index++)
            constant = probe.exprs[index].kind != ZIR_EXPR_IDENT &&
                       probe.exprs[index].kind != ZIR_EXPR_CALL;
        if(constant && probe.exprs[root].kind == ZIR_EXPR_COMPOUND) {
            const ZirFunction *saved = emitter->function;
            rust_fill_literal_types(emitter, &probe, root, global->type, 0);
            emitter->function = &probe;
            emit_typed_expression(emitter, root, global->type, output, size);
            emitter->function = saved;
            free(probe.exprs);
            return;
        }
        free(probe.exprs);
    }
    rust_zero_value(emitter, global->type, output, size);
}

static void emit_global_definitions(RustEmitter *emitter, FILE *output)
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
                char symbol[ZIR_RUST_NAME_MAX * 2];
                char type_name[ZIR_NAME_MAX];
                char zero[ZIR_RUST_TEXT_MAX];
                global_symbol(emitter, module, global, symbol,
                              sizeof(symbol));
                require_rust_type(emitter, global->span, global->type,
                                  type_name, sizeof(type_name));
                rust_global_initializer(emitter, global, zero, sizeof(zero));
                fprintf(output, "static mut %s: %s = %s;\n\n", symbol,
                        type_name, zero);
            }
        }
    }
}

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
    for(int index = 0; index < module->import_count; index++)
        if(module->imports[index].resolved_module != NULL)
            visit_startup_module(visits, ordered,
                                 module->imports[index].resolved_module);
    visit->state = 2;
    for(int index = 0; index < module->function_count; index++)
        if(module->functions[index].is_global_initializer) {
            append_startup_module(ordered, module);
            break;
        }
}

static int emit_startup(RustEmitter *emitter, FILE *output)
{
    RustModuleVisits visits = {0};
    RustModuleVisits ordered = {0};
    for(int program_index = 0; program_index < emitter->program_count;
        program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
             module_index++)
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

static FILE *open_output(ZirSourceSpan span, const char *directory,
                         const char *relative)
{
    char path[1024];
    char parent[1024];
    FILE *file;
    snprintf(path, sizeof(path), "%s/%s", directory, relative);
    snprintf(parent, sizeof(parent), "%s", path);
    char *separator = strrchr(parent, '/');
    if(separator != NULL)
        *separator = '\0';
    make_directories(parent);
    file = fopen(path, "wb");
    if(file == NULL) {
        Diagnostic(span, "zir_rust.output",
                   "cannot create Rust output: %s", path);
        exit(1);
    }
    return file;
}

static void emit_ziran_vec_runtime(FILE *output)
{
    fputs("#[repr(C)]\n", output);
    fputs("pub struct ZiranOption<T> {\n", output);
    fputs("    pub has_value: bool,\n", output);
    fputs("    pub value: T,\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("#[repr(C)]\n", output);
    fputs("pub struct ZiranVec<T> {\n", output);
    fputs("    pub data: *mut T,\n", output);
    fputs("    pub count: i64,\n", output);
    fputs("    pub capacity: i64,\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("unsafe impl<T> Sync for ZiranVec<T> {}\n", output);
    fputs("\n", output);
    fputs("impl<T> ZiranVec<T> {\n", output);
    fputs("    pub const fn new() -> Self {\n", output);
    fputs("        Self { data: core::ptr::null_mut(), count: 0, capacity: 0 }\n", output);
    fputs("    }\n", output);
    fputs("    pub fn element(&self, index: i64) -> *mut T {\n", output);
    fputs("        assert!(index >= 0 && index < self.count, \"Vec index out of bounds\");\n", output);
    fputs("        unsafe { self.data.offset(index as isize) }\n", output);
    fputs("    }\n", output);
    fputs("    pub fn at(&self, index: i64) -> T {\n", output);
    fputs("        unsafe { self.element(index).read() }\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("impl<T> Drop for ZiranVec<T> {\n", output);
    fputs("    fn drop(&mut self) {\n", output);
    fputs("        ZiranVecFree(self);\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecPush<T>(vector: &mut ZiranVec<T>, value: T) -> bool {\n", output);
    fputs("    if vector.count == vector.capacity {\n", output);
    fputs("        let capacity = if vector.capacity == 0 {\n", output);
    fputs("            4\n", output);
    fputs("        } else {\n", output);
    fputs("            match (vector.capacity as usize).checked_mul(2) {\n", output);
    fputs("                Some(capacity) if capacity <= i64::MAX as usize => capacity,\n", output);
    fputs("                _ => return false,\n", output);
    fputs("            }\n", output);
    fputs("        };\n", output);
    fputs("        let layout = match std::alloc::Layout::array::<T>(capacity) {\n", output);
    fputs("            Ok(layout) => layout,\n", output);
    fputs("            Err(_) => return false,\n", output);
    fputs("        };\n", output);
    fputs("        let old_size = vector.capacity as usize * core::mem::size_of::<T>();\n", output);
    fputs("        let allocation = if vector.data.is_null() {\n", output);
    fputs("            unsafe { std::alloc::alloc(layout) }\n", output);
    fputs("        } else {\n", output);
    fputs("            let pointer = vector.data as *mut u8;\n", output);
    fputs("            let old_layout = match std::alloc::Layout::from_size_align(\n", output);
    fputs("                old_size, layout.align()) {\n", output);
    fputs("                Ok(layout) => layout,\n", output);
    fputs("                Err(_) => return false,\n", output);
    fputs("            };\n", output);
    fputs("            unsafe { std::alloc::realloc(pointer, old_layout, layout.size()) }\n", output);
    fputs("        };\n", output);
    fputs("        if allocation.is_null() {\n", output);
    fputs("            return false;\n", output);
    fputs("        }\n", output);
    fputs("        vector.data = allocation as *mut T;\n", output);
    fputs("        vector.capacity = capacity as i64;\n", output);
    fputs("    }\n", output);
    fputs("    unsafe { vector.data.offset(vector.count as isize).write(value); }\n", output);
    fputs("    vector.count += 1;\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecClone<T>(destination: &mut ZiranVec<T>, source: &ZiranVec<T>) -> bool {\n", output);
    fputs("    ZiranVecFree(destination);\n", output);
    fputs("    if source.count == 0 {\n", output);
    fputs("        return true;\n", output);
    fputs("    }\n", output);
    fputs("    let count = source.count as usize;\n", output);
    fputs("    let layout = match std::alloc::Layout::array::<T>(count) {\n", output);
    fputs("        Ok(layout) => layout,\n", output);
    fputs("        Err(_) => return false,\n", output);
    fputs("    };\n", output);
    fputs("    let allocation = unsafe { std::alloc::alloc(layout) };\n", output);
    fputs("    if allocation.is_null() {\n", output);
    fputs("        return false;\n", output);
    fputs("    }\n", output);
    fputs("    unsafe { core::ptr::copy_nonoverlapping(source.data, allocation as *mut T, count); }\n", output);
    fputs("    destination.data = allocation as *mut T;\n", output);
    fputs("    destination.count = source.count;\n", output);
    fputs("    destination.capacity = source.count;\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecPop<T>(vector: &mut ZiranVec<T>) -> ZiranOption<T> {\n", output);
    fputs("    if vector.count == 0 {\n", output);
    fputs("        return ZiranOption {\n", output);
    fputs("            has_value: false,\n", output);
    fputs("            value: unsafe { core::mem::zeroed() },\n", output);
    fputs("        };\n", output);
    fputs("    }\n", output);
    fputs("    vector.count -= 1;\n", output);
    fputs("    let value = unsafe { vector.data.offset(vector.count as isize).read() };\n", output);
    fputs("    ZiranOption { has_value: true, value }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecGet<T>(vector: &mut ZiranVec<T>, index: usize) -> ZiranOption<T> {\n", output);
    fputs("    if index >= vector.count as usize {\n", output);
    fputs("        return ZiranOption {\n", output);
    fputs("            has_value: false,\n", output);
    fputs("            value: unsafe { core::mem::zeroed() },\n", output);
    fputs("        };\n", output);
    fputs("    }\n", output);
    fputs("    let value = unsafe { vector.data.offset(index as isize).read() };\n", output);
    fputs("    ZiranOption { has_value: true, value }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecSlice<T>(vector: &ZiranVec<T>, low: isize, high: isize) -> ZiranSlice<T> {\n", output);
    fputs("    assert!(low >= 0 && low <= high && high as usize <= vector.count as usize);\n", output);
    fputs("    ZiranSlice {\n", output);
    fputs("        data: unsafe { vector.data.offset(low) },\n", output);
    fputs("        len: (high - low) as usize,\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecClear<T>(vector: &mut ZiranVec<T>) {\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranBuilderAppend(vector: &mut ZiranVec<u8>, text: ZiranText) -> bool {\n", output);
    fputs("    for index in 0..text.len {\n", output);
    fputs("        if !ZiranVecPush(vector, unsafe { *text.data.offset(index as isize) }) {\n", output);
    fputs("            return false;\n", output);
    fputs("        }\n", output);
    fputs("    }\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranBuilderFinish(vector: &mut ZiranVec<u8>) -> ZiranText {\n", output);
    fputs("    let text = ZiranText { data: vector.data, len: vector.count as usize };\n", output);
    fputs("    vector.data = core::ptr::null_mut();\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("    vector.capacity = 0;\n", output);
    fputs("    text\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    /* print! spells floats as the shortest round-trip decimal, like every
     * target; only NaN is written differently, as nan. */
    fputs("pub struct ZiranFloat<T>(pub T);\n", output);
    fputs("\n", output);
    fputs("impl<T: core::fmt::Display + PartialEq> core::fmt::Display for ZiranFloat<T> {\n", output);
    fputs("    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {\n", output);
    fputs("        if self.0 != self.0 { f.write_str(\"nan\") } else { self.0.fmt(f) }\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecFree<T>(vector: &mut ZiranVec<T>) {\n", output);
    fputs("    if !vector.data.is_null() {\n", output);
    fputs("        let size = vector.capacity as usize * core::mem::size_of::<T>();\n", output);
    fputs("        let align = core::mem::align_of::<T>();\n", output);
    fputs("        match std::alloc::Layout::from_size_align(size, align) {\n", output);
    fputs("            Ok(layout) => unsafe {\n", output);
    fputs("                std::alloc::dealloc(vector.data as *mut u8, layout)\n", output);
    fputs("            },\n", output);
    fputs("            Err(_) => unreachable!(\"invalid Ziran vector layout\"),\n", output);
    fputs("        }\n", output);
    fputs("    }\n", output);
    fputs("    vector.data = core::ptr::null_mut();\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("    vector.capacity = 0;\n", output);
    fputs("}\n", output);
}

/* The Rust runtime is written in full, then only the items the program
 * refers to are kept: a struct or function stays when the program or a kept
 * item names it, and an impl stays with its type. */
typedef struct RustRuntimeItem {
    const char *start;
    size_t length;
    char name[ZIR_NAME_MAX];
    char impl_type[ZIR_NAME_MAX];
    int kept;
    /* A method of a split impl, or the impl's opening or closing line. */
    int method;
    int impl_line;
    int block; /* index of the impl opening line for methods and the close */
} RustRuntimeItem;

static int rust_names(const char *text, size_t length, const char *name)
{
    size_t size = strlen(name);
    if(size == 0)
        return 0;
    for(size_t index = 0; index + size <= length; index++) {
        if(memcmp(text + index, name, size) != 0)
            continue;
        if(index > 0 && is_ident_char((unsigned char)text[index - 1]))
            continue;
        if(index + size < length && is_ident_char((unsigned char)text[index + size]))
            continue;
        return 1;
    }
    return 0;
}

static void rust_item_name(RustRuntimeItem *item)
{
    const char *end = item->start + item->length;
    const char *markers[] = {"pub struct ", "pub fn ", "pub const fn ",
                             "    pub fn ", "    pub const fn "};
    for(const char *line = item->start; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        size_t size = next != NULL ? (size_t)(next - line) : (size_t)(end - line);
        for(int m = 0; m < 5; m++) {
            size_t marker = strlen(markers[m]);
            if(size > marker && memcmp(line, markers[m], marker) == 0) {
                size_t used = 0;
                for(const char *c = line + marker; c < line + size &&
                    is_ident_char((unsigned char)*c) && used + 1 < sizeof(item->name); c++)
                    item->name[used++] = *c;
                item->name[used] = '\0';
                return;
            }
        }
        if((size > 4 && memcmp(line, "impl", 4) == 0) ||
           (size > 11 && memcmp(line, "unsafe impl", 11) == 0)) {
            /* The implemented type follows " for " when present. */
            const char *type = NULL;
            for(const char *c = line; c + 5 <= line + size; c++)
                if(memcmp(c, " for ", 5) == 0) { type = c + 5; break; }
            if(type == NULL) {
                type = line + (line[0] == 'u' ? 11 : 4);
                if(*type == '<')
                    while(type < line + size && *type != '>') type++;
                while(type < line + size && !is_ident_char((unsigned char)*type)) type++;
            }
            size_t used = 0;
            while(type < line + size && is_ident_char((unsigned char)*type) &&
                  used + 1 < sizeof(item->impl_type))
                item->impl_type[used++] = *type++;
            item->impl_type[used] = '\0';
            return;
        }
        line = next != NULL ? next + 1 : end;
    }
}

static RustRuntimeItem *rust_add_item(RustRuntimeItem **items, int *count,
                                      int *capacity, const char *start,
                                      size_t length)
{
    if(*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 64;
        RustRuntimeItem *grown = realloc(*items, (size_t)*capacity * sizeof(**items));
        if(grown == NULL)
            return NULL;
        *items = grown;
    }
    RustRuntimeItem *item = &(*items)[(*count)++];
    memset(item, 0, sizeof(*item));
    item->start = start;
    item->length = length;
    item->block = -1;
    return item;
}

/* An inherent impl is split into its opening line, one piece per method,
 * and its closing line, so each method is kept only when used. */
static int rust_add_impl(RustRuntimeItem **items, int *count, int *capacity,
                         const char *start, size_t length)
{
    const char *end = start + length;
    const char *first_line_end = memchr(start, '\n', length);
    int methods = 0;
    for(const char *line = start; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        if(!strncmp(line, "    pub fn ", 11) || !strncmp(line, "    pub const fn ", 17) ||
           !strncmp(line, "    fn ", 7))
            methods++;
        line = next ? next + 1 : end;
    }
    RustRuntimeItem *whole = rust_add_item(items, count, capacity, start, length);
    if(whole == NULL)
        return 0;
    rust_item_name(whole);
    if(methods < 2 || !whole->impl_type[0] || first_line_end == NULL)
        return 1;
    /* Replace the whole impl with its pieces. */
    (*count)--;
    int opening = *count;
    RustRuntimeItem *head = rust_add_item(items, count, capacity, start,
                                          (size_t)(first_line_end + 1 - start));
    if(head == NULL)
        return 0;
    rust_item_name(head);
    head->impl_line = 1;
    const char *method_start = NULL;
    const char *close = end;
    while(close > start && close[-1] == '\n') close--;
    while(close > start && close[-1] != '\n') close--;
    for(const char *line = first_line_end + 1; line <= close;) {
        int starts = line == close || !strncmp(line, "    pub fn ", 11) ||
                     !strncmp(line, "    pub const fn ", 17) || !strncmp(line, "    fn ", 7);
        if(starts && method_start != NULL) {
            RustRuntimeItem *method = rust_add_item(items, count, capacity, method_start,
                                                    (size_t)(line - method_start));
            if(method == NULL)
                return 0;
            rust_item_name(method);
            method->method = 1;
            method->block = opening;
            snprintf(method->impl_type, sizeof(method->impl_type), "%s",
                     (*items)[opening].impl_type);
        }
        if(line == close)
            break;
        if(starts)
            method_start = line;
        const char *next = memchr(line, '\n', (size_t)(end - line));
        line = next ? next + 1 : end;
    }
    RustRuntimeItem *tail = rust_add_item(items, count, capacity, close,
                                          (size_t)(end - close));
    if(tail == NULL)
        return 0;
    tail->impl_line = 1;
    tail->block = opening;
    return 1;
}

/* A method is called as .name( or Type::name(; other items by name. */
static int rust_mentions(const char *text, size_t length, const char *name, int method)
{
    if(!method)
        return rust_names(text, length, name);
    size_t size = strlen(name);
    for(size_t index = 1; index + size < length; index++)
        if(memcmp(text + index, name, size) == 0 && text[index + size] == '(' &&
           (text[index - 1] == '.' || text[index - 1] == ':'))
            return 1;
    return 0;
}

static int rust_item_referenced(RustRuntimeItem *items, int count, int self,
                                const char *body, size_t body_size)
{
    int method = items[self].method;
    if(rust_mentions(body, body_size, items[self].name, method))
        return 1;
    for(int other = 0; other < count; other++)
        if(other != self && items[other].kept &&
           rust_mentions(items[other].start, items[other].length, items[self].name, method))
            return 1;
    return 0;
}

static int write_used_runtime(FILE *output, const char *runtime, size_t runtime_size,
                              const char *body, size_t body_size)
{
    RustRuntimeItem *items = NULL;
    int count = 0, capacity = 0, changed = 1;
    const char *end = runtime + runtime_size;
    const char *item_start = NULL;
    int open = 0;
    for(const char *line = runtime; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        const char *line_end = next != NULL ? next + 1 : end;
        size_t size = (size_t)(line_end - line);
        int blank = size <= 1;
        if(item_start == NULL && !blank) {
            item_start = line;
            open = 1;
        }
        if(item_start != NULL && open && line[0] == '}' )
            open = 0;
        if(item_start != NULL && open && size >= 3 && line[0] != ' ' &&
           (memcmp(line_end - 3, "{}\n", 3) == 0 || memcmp(line_end - 2, ";\n", 2) == 0))
            open = 0;
        if(item_start != NULL && !open) {
            if(!rust_add_impl(&items, &count, &capacity, item_start,
                              (size_t)(line_end - item_start))) {
                free(items);
                return 0;
            }
            item_start = NULL;
        }
        line = line_end;
    }
    for(int i = 0; i < count; i++)
        if(!items[i].method && !items[i].impl_line && items[i].name[0] &&
           rust_names(body, body_size, items[i].name))
            items[i].kept = 1;
    while(changed) {
        changed = 0;
        for(int i = 0; i < count; i++) {
            int keep = 0;
            if(items[i].kept || items[i].impl_line)
                continue;
            if(items[i].method) {
                /* A method stays when its type does and something calls it. */
                for(int j = 0; j < count && !keep; j++)
                    keep = items[j].kept && !items[j].method && !items[j].impl_line &&
                           !strcmp(items[j].name, items[i].impl_type);
                keep = keep && rust_item_referenced(items, count, i, body, body_size);
            } else if(items[i].impl_type[0]) {
                for(int j = 0; j < count && !keep; j++)
                    keep = items[j].kept && !items[j].method &&
                           !strcmp(items[i].impl_type, items[j].name);
            } else if(items[i].name[0])
                keep = rust_item_referenced(items, count, i, body, body_size);
            if(keep)
                items[i].kept = changed = 1;
        }
    }
    /* A split impl's opening and closing lines stay with any kept method. */
    for(int i = 0; i < count; i++)
        if(items[i].method && items[i].kept && items[i].block >= 0) {
            items[items[i].block].kept = 1;
            for(int j = items[i].block + 1; j < count; j++)
                if(items[j].impl_line && items[j].block == items[i].block) {
                    items[j].kept = 1;
                    break;
                }
        }
    for(int i = 0; i < count; i++)
        if(items[i].kept) {
            fwrite(items[i].start, 1, items[i].length, output);
            if(!items[i].method && !(items[i].impl_line && items[i].block < 0))
                fputc('\n', output);
        }
    free(items);
    return 1;
}

static int rust_text_has(const char *first, size_t first_size,
                         const char *second, size_t second_size, const char *needle)
{
    size_t length = strlen(needle);
    for(int part = 0; part < 2; part++) {
        const char *text = part ? second : first;
        size_t size = part ? second_size : first_size;
        for(size_t index = 0; index + length <= size; index++)
            if(!memcmp(text + index, needle, length))
                return 1;
    }
    return 0;
}

/* A definition keyword followed by a name containing an uppercase letter
 * or underscore, like fn Hello_Main or struct Source_Code_Location. */
static int rust_text_names(const char *first, size_t first_size,
                           const char *second, size_t second_size,
                           const char *keyword, int (*unusual)(int))
{
    size_t length = strlen(keyword);
    for(int part = 0; part < 2; part++) {
        const char *text = part ? second : first;
        size_t size = part ? second_size : first_size;
        for(size_t index = 0; index + length < size; index++) {
            if(memcmp(text + index, keyword, length) != 0 ||
               (index > 0 && is_ident_char((unsigned char)text[index - 1])))
                continue;
            for(size_t c = index + length; c < size && is_ident_char((unsigned char)text[c]); c++)
                if(unusual((unsigned char)text[c]))
                    return 1;
        }
    }
    return 0;
}

static int rust_upper(int c) { return isupper(c); }
static int rust_underscore(int c) { return c == '_'; }

/* Only the lints the generated names and code can trigger are allowed. */
static void write_rust_lint_allowances(FILE *output, const char *runtime, size_t runtime_size,
                                       const char *body, size_t body_size, int executable)
{
    int any = 0;
    if(rust_text_names(runtime, runtime_size, body, body_size, "fn ", rust_upper))
        any = fputs("#![allow(non_snake_case)]\n", output) >= 0;
    if(rust_text_names(runtime, runtime_size, body, body_size, "struct ", rust_underscore) ||
       rust_text_names(runtime, runtime_size, body, body_size, "union ", rust_underscore) ||
       rust_text_names(runtime, runtime_size, body, body_size, "type ", rust_underscore))
        any = fputs("#![allow(non_camel_case_types)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, "static mut "))
        any = fputs("#![allow(non_upper_case_globals)]\n", output) >= 0;
    if(!executable || rust_text_has("", 0, body, body_size, "let ") ||
       rust_text_has(runtime, runtime_size, body, body_size, "static mut "))
        any = fputs("#![allow(unused)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, "extern \"C\""))
        any = fputs("#![allow(improper_ctypes_definitions)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, ".wrapping_") ||
       rust_text_has(runtime, runtime_size, body, body_size, ".checked_") ||
       rust_text_has(runtime, runtime_size, body, body_size, " as usize]")) {
        fputs("#![allow(arithmetic_overflow)]\n", output);
        any = fputs("#![allow(unconditional_panic)]\n", output) >= 0;
    }
    if(any)
        fputc('\n', output);
}

int rust_lower(const ZirProgram *const *programs, int program_count,
               const char *output_directory, const char *entry_module,
               const char *entry_function, int executable)
{
    FILE *output;
    FILE *cargo;
    RustEmitter emitter = {0};
    const ZirModule *entry_owner = NULL;
    const ZirFunction *entry = NULL;
    char symbol[ZIR_RUST_NAME_MAX * 2];
    if(programs == NULL || program_count <= 0 || output_directory == NULL ||
       output_directory[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "zir_rust.input",
                   "no checked programs were supplied");
        return 1;
    }
    for(int program_index = 0; program_index < program_count; program_index++)
        for(int module_index = 0;
            module_index < programs[program_index]->module_count;
            module_index++)
            validate_module(&programs[program_index]->modules[module_index]);
    if(executable) {
        for(int program_index = 0; program_index < program_count; program_index++)
            for(int module_index = 0;
                module_index < programs[program_index]->module_count;
                module_index++) {
                const ZirModule *module =
                    &programs[program_index]->modules[module_index];
                if(strcmp(module->name, entry_module) != 0)
                    continue;
                for(int function_index = 0;
                    function_index < module->function_count; function_index++)
                    if(strcmp(module->functions[function_index].name,
                              entry_function) == 0) {
                        entry_owner = module;
                        entry = &module->functions[function_index];
                    }
            }
        if(entry == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zir_rust.entry",
                       "executable entry is missing: %s:%s", entry_module,
                       entry_function);
            return 1;
        }
        if(entry->args[0] || (strcmp(entry->return_type, "void") != 0 &&
           !integer_type(entry->return_type) &&
           strcmp(entry->return_type, "bool") != 0)) {
            Diagnostic(entry->span, "zir_rust.entry",
                       "executable entry must take no arguments and return void, bool, or an integer");
            return 1;
        }
    }
    output = open_output(programs[0]->modules[0].span, output_directory,
                         executable ? "src/main.rs" : "src/lib.rs");
    cargo = open_output(programs[0]->modules[0].span, output_directory,
                        "Cargo.toml");
    fprintf(cargo,
            "[package]\n"
            "name = \"ziran_generated\"\n"
            "version = \"0.1.0\"\n"
            "edition = \"2021\"\n"
            "publish = false\n\n"
            "[profile.dev]\npanic = \"abort\"\n\n"
            "[profile.release]\npanic = \"abort\"\n");
    fclose(cargo);
    char *runtime_text = NULL, *body_text = NULL;
    size_t runtime_size = 0, body_size = 0;
    FILE *runtime = open_memstream(&runtime_text, &runtime_size);
    FILE *body = open_memstream(&body_text, &body_size);
    if(runtime == NULL || body == NULL) {
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot buffer Rust output");
        return 1;
    }
    emitter.output = body;
    emitter.programs = programs;
    emitter.program_count = program_count;
    fputs("#[repr(C)]\npub struct ZiranSlice<T> {\n"
          "    pub data: *mut T,\n    pub len: usize,\n}\n\n"
          "impl<T> Clone for ZiranSlice<T> {\n"
          "    fn clone(&self) -> Self { *self }\n"
          "}\n\n"
          "impl<T> Copy for ZiranSlice<T> {}\n\n"
          "impl<T> ZiranSlice<T> {\n"
          "    pub fn view(source: Self, low: i64, high: i64) -> Self {\n"
          "        assert!(low >= 0 && low <= high && high as usize <= source.len, \"slice range out of bounds\");\n"
          "        Self { data: unsafe { source.data.offset(low as isize) }, len: (high - low) as usize }\n"
          "    }\n"
          "    pub fn view_from(source: Self, low: i64) -> Self {\n"
          "        Self::view(source, low, source.len as i64)\n"
          "    }\n"
          "    pub fn element(self, index: i64) -> *mut T {\n"
          "        assert!(index >= 0 && (index as usize) < self.len, \"slice index out of bounds\");\n"
          "        unsafe { self.data.offset(index as isize) }\n"
          "    }\n"
          "    pub fn get(self, index: i64) -> T {\n"
          "        unsafe { self.element(index).read() }\n"
          "    }\n"
          "}\n\n", runtime);
    emit_ziran_vec_runtime(runtime);
    fputs("#[repr(C)]\n#[derive(Clone, Copy)]\npub struct Source_Code_Location {\n"
          "    pub fully_pathed_filename: ZiranText,\n    pub line_number: i64,\n}\n\n", runtime);
    fputs("#[repr(C)]\n#[derive(Clone, Copy)]\npub struct ZiranText {\n"
          "    pub data: *const u8,\n    pub len: usize,\n"
          "}\n\n"
          "unsafe impl Sync for ZiranText {}\n\n"
          "impl ZiranText {\n"
          "    pub const fn new(value: &'static str) -> Self {\n"
          "        Self { data: value.as_ptr(), len: value.len() }\n"
          "    }\n"
          "    pub fn eq(left: Self, right: Self) -> bool {\n"
          "        unsafe {\n"
          "            left.len == right.len &&\n"
          "            core::slice::from_raw_parts(left.data, left.len) ==\n"
          "            core::slice::from_raw_parts(right.data, right.len)\n"
          "        }\n"
          "    }\n"
          "    pub fn as_str(self) -> std::borrow::Cow<'static, str> {\n"
          "        String::from_utf8_lossy(unsafe { core::slice::from_raw_parts(self.data, self.len) })\n"
          "    }\n"
          "    pub fn at(self, index: i64) -> u8 {\n"
          "        assert!(index >= 0 && (index as usize) < self.len, \"string index out of bounds\");\n"
          "        unsafe { *self.data.offset(index as isize) }\n"
          "    }\n"
          "    pub fn slice(value: Self, low: isize, high: isize) -> Self {\n"
          "        assert!(low >= 0 && low <= high && high as usize <= value.len);\n"
          "        Self { data: unsafe { value.data.offset(low) }, len: (high - low) as usize }\n"
          "    }\n"
          "}\n\n", runtime);
    emit_type_definitions(&emitter, body);
    emit_extern_definitions(&emitter, body);
    emit_global_definitions(&emitter, body);
    for(int program_index = 0; program_index < program_count; program_index++) {
        const ZirProgram *program = programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
            module_index++) {
            const ZirModule *module = &program->modules[module_index];
            if(program_count != 1 || programs[0]->module_count != 1)
                fprintf(body,
                        "// Code generated by zi2rust from %s. DO NOT EDIT.\n",
                        module->source_path);
            for(int function_index = 0; function_index < module->function_count;
                function_index++)
                lower_function(&emitter, module,
                               &module->functions[function_index]);
        }
    }
    int has_startup = emit_startup(&emitter, body);
    if(executable) {
        function_symbol(&emitter, entry_owner, entry, symbol, sizeof(symbol));
        fputs("fn main() {\n", body);
        if(has_startup)
            fputs("    ziran_startup();\n", body);
        if(strcmp(entry->return_type, "void") == 0) {
            fprintf(body, "    %s();\n", symbol);
        } else if(strcmp(entry->return_type, "bool") == 0) {
            fprintf(body, "    let failed = %s();\n", symbol);
            fprintf(body, "    if failed { std::process::exit(1); }\n");
        } else {
            fprintf(body, "    let code = %s() as i32;\n", symbol);
            fprintf(body, "    std::process::exit(code);\n");
        }
        fputs("}\n", body);
    }
    char *kept_text = NULL;
    size_t kept_size = 0;
    FILE *kept = open_memstream(&kept_text, &kept_size);
    if(kept == NULL || fclose(runtime) != 0 || fclose(body) != 0 ||
       !write_used_runtime(kept, runtime_text, runtime_size, body_text, body_size) ||
       fclose(kept) != 0) {
        free(runtime_text);
        free(body_text);
        free(kept_text);
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot buffer Rust output");
        return 1;
    }
    if(program_count == 1 && programs[0]->module_count == 1)
        fprintf(output, "// Code generated by zi2rust from %s. DO NOT EDIT.\n",
                programs[0]->modules[0].source_path);
    write_rust_lint_allowances(output, kept_text, kept_size, body_text, body_size,
                               executable);
    fwrite(kept_text, 1, kept_size, output);
    fwrite(body_text, 1, body_size, output);
    free(kept_text);
    free(runtime_text);
    free(body_text);
    if(fclose(output) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot finish Rust output");
        return 1;
    }
    return 0;
}
