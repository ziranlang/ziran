/*
 * zir_rust_lower.c - checked ZIR to Rust backend.
 */
#include "zir_rust_lower.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ZIR_RUST_TEXT_MAX 8192
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
    "unsafe", "use", "where", "while", NULL
};

static const char *rust_scalar_type(const char *type);
static int rust_enum_type(RustEmitter *emitter, const char *type,
                          const ZirModule **owner, const ZirType **enumeration);
static int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record);
static int rust_type(RustEmitter *emitter, const char *type, char *output,
                     size_t size);
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
                NativeTypeName(module, record, type_name, sizeof(type_name));
                fprintf(output,
                        "#[repr(C)]\n#[derive(Clone, Copy)]\npub struct %s {\n",
                        type_name);
                size_t offset = 0;
                ZirTypeField field;
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
        fputc(' ', emitter->output);
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
           !strcmp(type, "u64") || !strcmp(type, "usize");
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

static int rust_record_type(RustEmitter *emitter, const char *type,
                            const ZirModule **owner, const ZirType **record)
{
    const ZirModule *type_owner = NULL;
    const ZirType *declared = emitter != NULL ?
        FindType(emitter->module, type, &type_owner) : NULL;
    if(declared == NULL || declared->is_enum || declared->is_union ||
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
    if(rust_scalar_type(type) != NULL) {
        snprintf(output, size, "%s", rust_scalar_type(type));
        return 1;
    }
    if(rust_enum_type(emitter, type, &owner, &record)) {
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

static void require_rust_type(RustEmitter *emitter, ZirSourceSpan span,
                              const char *type, char *output, size_t size)
{
    if(rust_type(emitter, type, output, size))
        return;
    Diagnostic(span, "zir_rust.type",
               "the initial Rust target supports scalar and borrowed text types only: %s",
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
    NativeGoFunctionName(emitter->programs, emitter->program_count, module,
                         function, output, size);
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
               "unsupported expression in the initial Rust target: %s",
               expression->text[0] ? expression->text :
                   ExprKindName(expression->kind));
    exit(1);
}

static void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

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
        if((SliceElementType(base_type, NULL, 0) ||
            !strcmp(base_type, "string")) &&
           !strcmp(expression->name, "count")) {
            emit_destination(emitter, expression->left, base, sizeof(base));
            snprintf(output, size, "(%s.len as i64)", base);
            return;
        }
        if(!rust_record_type(emitter, base_type, &owner, &record)) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_destination(emitter, expression->left, base, sizeof(base));
        rust_field_name(record, expression->name, field, sizeof(field));
        snprintf(output, size, "%s.%s", base, field);
        return;
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        char base[ZIR_RUST_TEXT_MAX];
        char index[ZIR_RUST_TEXT_MAX];
        char element[ZIR_NAME_MAX];
        emit_destination(emitter, expression->left, base, sizeof(base));
        emit_expression(emitter, expression->right, index, sizeof(index));
        if(SliceElementType(emitter->function->exprs[expression->left].type,
                            element, sizeof(element)))
            snprintf(output, size, "*%s.data.offset(%s as isize)", base,
                     index);
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
    if(expression->kind == ZIR_EXPR_SLICE) {
        emit_expression(emitter, index, output, size);
        return;
    }
    unsupported_expression(emitter, expression);
}

static void emit_typed_expression(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size)
{
    char value[ZIR_RUST_TEXT_MAX];
    const ZirExpr *expression = index >= 0 &&
        index < emitter->function->expr_count ?
            &emitter->function->exprs[index] : NULL;
    emit_expression(emitter, index, value, sizeof(value));
    if(expression != NULL && strcmp(expression->type, type) != 0 &&
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
    else if(!strcmp(expression->type, "u8")) suffix = "u8";
    else if(!strcmp(expression->type, "u16")) suffix = "u16";
    else if(!strcmp(expression->type, "u32")) suffix = "u32";
    else if(!strcmp(expression->type, "u64") ||
            !strcmp(expression->type, "usize")) suffix = "u64";
    snprintf(output, size, "%s%s", expression->text, suffix);
}

static void emit_call(RustEmitter *emitter, const ZirExpr *expression,
                      char *output, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    char symbol[ZIR_RUST_NAME_MAX * 2];
    char arguments[ZIR_RUST_TEXT_MAX] = "";
    char child[ZIR_RUST_TEXT_MAX];
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
    if(expression->slot_type[0] || expression->is_function_value) {
        unsupported_expression(emitter, expression);
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
    for(int child_index = expression->first_child; child_index >= 0;
        child_index = emitter->function->exprs[child_index].next_sibling) {
        emit_expression(emitter, child_index, child, sizeof(child));
        if(*arguments)
            strncat(arguments, ", ", sizeof(arguments) - strlen(arguments) - 1);
        strncat(arguments, child, sizeof(arguments) - strlen(arguments) - 1);
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
        if(global_reference(emitter, expression->name, global_name,
                            sizeof(global_name)))
            snprintf(output, size, "%s", global_name);
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
        snprintf(output, size, "%s%s", expression->text,
                 !strcmp(expression->type, "float32") ? "f32" : "f64");
        break;
    case ZIR_EXPR_CALL:
        emit_call(emitter, expression, output, size);
        break;
    case ZIR_EXPR_MEMBER:
        emit_destination(emitter, index, output, size);
        break;
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];
        char base[ZIR_RUST_TEXT_MAX];
        char index[ZIR_RUST_TEXT_MAX];
        emit_destination(emitter, expression->left, base, sizeof(base));
        emit_expression(emitter, expression->right, index, sizeof(index));
        if(!strcmp(emitter->function->exprs[expression->left].type,
                   "string")) {
            snprintf(output, size,
                     "unsafe { assert!((%s as isize) >= 0 && (%s as usize) < %s.len); *%s.data.offset(%s as isize) }",
                     index, index, base, base, index);
            break;
        }
        if(SliceElementType(
               emitter->function->exprs[expression->left].type, element,
               sizeof(element))) {
            snprintf(output, size,
                     "unsafe { assert!((%s as isize) >= 0 && (%s as usize) < %s.len); *%s.data.offset(%s as isize) }",
                     index, index, base, base, index);
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
        if(!ArrayElementType(
               emitter->function->exprs[expression->left].type, element,
               sizeof(element), &capacity)) {
            unsupported_expression(emitter, expression);
            break;
        }
        {
            const char *open = strrchr(expression->text, '[');
            const char *close = strrchr(expression->text, ']');
            const char *colon = open != NULL && close > open ?
                strchr(open, ':') : NULL;
            char low[ZIR_RUST_TEXT_MAX] = "0";
            char high[ZIR_RUST_TEXT_MAX];
            if(open == NULL || close == NULL || colon == NULL ||
               colon < open + 1 || colon > close - 1) {
                unsupported_expression(emitter, expression);
                break;
            }
            if(colon - open - 1 > 0)
                snprintf(low, sizeof(low), "%.*s",
                         (int)(colon - open - 1), open + 1);
            if(close - colon - 1 == 0)
                snprintf(high, sizeof(high), "%d", capacity);
            else
                snprintf(high, sizeof(high), "%.*s",
                         (int)(close - colon - 1), colon + 1);
            snprintf(output, size,
                     "ZiranSlice { data: %s.as_mut_ptr().offset(%s as isize), len: ((%s as isize) - (%s as isize)) as usize }",
                     base, low, high, low);
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
                if(initializer->kind != ZIR_EXPR_FIELD_INIT) {
                    unsupported_expression(emitter, initializer);
                    return;
                }
                emit_typed_expression(emitter, initializer->right, element,
                                      value, sizeof(value));
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
        snprintf(output, size, "%s { ", type_name);
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
        size_t used = strlen(output);
        snprintf(output + used, size - used, " }");
        break;
    }
    case ZIR_EXPR_BINARY:
        emit_expression(emitter, expression->left, left, sizeof(left));
        emit_expression(emitter, expression->right, right, sizeof(right));
        if((expression->left >= 0 &&
            !strcmp(emitter->function->exprs[expression->left].type,
                    "string")) ||
           (expression->right >= 0 &&
            !strcmp(emitter->function->exprs[expression->right].type,
                    "string"))) {
            if(strcmp(expression->op, "==") != 0 &&
               strcmp(expression->op, "!=") != 0)
                unsupported_expression(emitter, expression);
            snprintf(output, size, "%sZiranText::eq(%s, %s)",
                     !strcmp(expression->op, "==") ? "" : "!", left, right);
            break;
        }
        if(integer_type(expression->type) &&
           wrapping_operation(expression->op))
            snprintf(output, size, "(%s).%s(%s)", left,
                     wrapping_method(expression->op), right);
        else if(!strcmp(expression->op, "<<") ||
                !strcmp(expression->op, ">>") ||
                !strcmp(expression->op, "~"))
            unsupported_expression(emitter, expression);
        else
            snprintf(output, size, "(%s %s %s)", left, expression->op, right);
        break;
    case ZIR_EXPR_UNARY:
        emit_expression(emitter, expression->right, right, sizeof(right));
        if(!strcmp(expression->op, "!") || !strcmp(expression->op, "-") ||
           !strcmp(expression->op, "+"))
            snprintf(output, size, "(%s%s)", expression->op, right);
        else
            unsupported_expression(emitter, expression);
        break;
    case ZIR_EXPR_CAST:
        emit_expression(emitter, expression->right, right, sizeof(right));
        snprintf(output, size, "(%s as %s)", right,
                 rust_scalar_type(expression->type));
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
    write_line(emitter, "if %s {", condition);
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
    if(integer_type(left->type) && wrapping_operation(operation)) {
        write_line(emitter, "%s = (%s).%s(%s);", name, name,
                   wrapping_method(operation), value);
        return;
    }
    if(!strcmp(operation, "<<") || !strcmp(operation, ">>") ||
       !strcmp(operation, "~")) {
        Diagnostic(statement->span, "zir_rust.assignment",
                   "unsupported assignment operation in the initial Rust target: %s",
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
    if(SliceElementType(type, element, sizeof(element))) {
        char element_type[ZIR_NAME_MAX];
        rust_type(emitter, element, element_type, sizeof(element_type));
        snprintf(output, size,
                 "ZiranSlice<%s> { data: core::ptr::null_mut(), len: 0 }",
                 element_type);
        return;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        char zero[ZIR_NAME_MAX];
        rust_zero_value(emitter, element, zero, sizeof(zero));
        snprintf(output, size, "[%s; %d]", zero, capacity);
        return;
    }
    if(!rust_record_type(emitter, type, &owner, &record) ||
       !rust_type(emitter, type, type_name, sizeof(type_name))) {
        snprintf(output, size, "0");
        return;
    }
    snprintf(output, size, "%s { ", type_name);
    size_t used = 0;
    size_t offset = 0;
    ZirTypeField field;
    int first = 1;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        char value[ZIR_NAME_MAX];
        rust_field_name(record, field.name, field_name, sizeof(field_name));
        rust_zero_value(emitter, field.type, value, sizeof(value));
        used = strlen(output);
        snprintf(output + used, size - used, "%s%s: %s", first ? "" : ", ",
                 field_name, value);
        first = 0;
    }
    used = strlen(output);
    snprintf(output + used, size - used, " }");
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
            if(statement->expr_root >= 0) {
                emit_typed_expression(emitter, statement->expr_root,
                                      statement->type, value, sizeof(value));
                write_line(emitter, "let mut %s: %s = %s;",
                           local_name(emitter, statement->name),
                           type_name, value);
            } else if(!strcmp(statement->type, "bool")) {
                write_line(emitter, "let mut %s: bool = false;",
                           local_name(emitter, statement->name));
            } else if(float_type(statement->type)) {
                write_line(emitter, "let mut %s: %s = 0.0%s;",
                           local_name(emitter, statement->name),
                           rust_scalar_type(statement->type),
                           !strcmp(statement->type, "float32") ? "f32" : "f64");
            } else {
                char zero[ZIR_RUST_TEXT_MAX];
                rust_zero_value(emitter, statement->type, zero, sizeof(zero));
                write_line(emitter, "let mut %s: %s = %s;",
                           local_name(emitter, statement->name), type_name,
                           zero);
            }
            register_local(emitter, statement->name);
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
            write_line(emitter, "while %s {", value);
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
            write_line(emitter, "break;");
            break;
        case ZIR_STMT_CONTINUE:
            write_line(emitter, "continue;");
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
                   "unsupported statement in the initial Rust target: %s",
                   statement->text[0] ? statement->text :
                       StmtKindName(statement->kind));
        exit(1);
    }
}

static void validate_module(const ZirModule *module)
{
    RustEmitter emitter = {0};
    emitter.module = module;
    if(module->define_count) {
        Diagnostic(module->span, "zir_rust.module",
                   "the initial Rust target does not support named constants yet");
        exit(1);
    }
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
        if(rust_enum_type(&emitter, record->name, &owner, &checked)) {
            if(rust_scalar_type(checked->enum_backing) == NULL) {
                Diagnostic(record->span, "zir_rust.enum",
                           "invalid enum backing type: %s",
                           checked->enum_backing);
                exit(1);
            }
            continue;
        }
        if(!rust_record_type(&emitter, record->name, &owner, &checked)) {
            Diagnostic(record->span, "zir_rust.type",
                       "the initial Rust target supports plain records with scalar fields only: %s",
                       record->name);
            exit(1);
        }
        size_t offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &offset, &field) == 1)
            if(!rust_type(&emitter, field.type, checked_type,
                          sizeof(checked_type))) {
                Diagnostic(record->span, "zir_rust.type",
                           "the initial Rust target supports plain records with scalar fields only: %s",
                           record->name);
                exit(1);
            }
    }
    for(int index = 0; index < module->import_count; index++) {
        if(module->imports[index].kind == ZIR_IMPORT_EXTERN) {
            Diagnostic(module->imports[index].span, "zir_rust.import",
                       "foreign imports are unsupported by the initial Rust target");
            exit(1);
        }
    }
}

static void lower_function(RustEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function)
{
    char parts[32][ZIR_RUST_TEXT_MAX];
    char symbol[ZIR_RUST_NAME_MAX * 2];
    int count;
    if(function->is_extern || function->is_template ||
       function->export_symbol[0]) {
        Diagnostic(function->span, "zir_rust.function",
                   "unsupported procedure form in the initial Rust target: %s",
                   function->name);
        exit(1);
    }
    emitter->module = module;
    emitter->function = function;
    emitter->local_count = 0;
    function_symbol(emitter, module, function, symbol, sizeof(symbol));
    fputs("#[inline(never)]\npub fn ", emitter->output);
    fputs(symbol, emitter->output);
    fputc('(', emitter->output);
    count = *function->args ? split_arguments(function->args, parts, 32) : 0;
    if(count < 0) {
        Diagnostic(function->span, "zir_rust.parameter",
                   "too many parameters in the initial Rust target: %s",
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
    emitter->indent = 1;
    write_line(emitter, "unsafe {");
    emitter->indent++;
    emit_sequence(emitter, 0, function->stmt_count);
    emitter->indent--;
    write_line(emitter, "}");
    emitter->indent = 0;
    fputs("}\n\n", emitter->output);
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
    if((rust_scalar_type(global->type) != NULL ||
        rust_enum_type(emitter, global->type, &owner, &enumeration)) &&
       global->init[0] != '\0' && strchr(global->init, '(') == NULL) {
        snprintf(output, size, "%s", global->init);
        return;
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
    emitter.output = output;
    emitter.programs = programs;
    emitter.program_count = program_count;
    fputs("#![allow(non_snake_case)]\n#![allow(non_camel_case_types)]\n#![allow(non_upper_case_globals)]\n#![allow(unused)]\n\n", output);
    fputs("#[repr(C)]\n#[derive(Clone, Copy)]\npub struct ZiranSlice<T> {\n"
          "    pub data: *mut T,\n    pub len: usize,\n}\n\n", output);
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
          "    pub fn slice(value: Self, low: isize, high: isize) -> Self {\n"
          "        assert!(low >= 0 && low <= high && high as usize <= value.len);\n"
          "        Self { data: unsafe { value.data.offset(low) }, len: (high - low) as usize }\n"
          "    }\n"
          "}\n\n", output);
    emit_type_definitions(&emitter, output);
    emit_global_definitions(&emitter, output);
    for(int program_index = 0; program_index < program_count; program_index++) {
        const ZirProgram *program = programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
            module_index++) {
            const ZirModule *module = &program->modules[module_index];
            fprintf(output,
                    "// Code generated by zi2rust from %s. DO NOT EDIT.\n",
                    module->source_path);
            for(int function_index = 0; function_index < module->function_count;
                function_index++)
                lower_function(&emitter, module,
                               &module->functions[function_index]);
        }
    }
    int has_startup = emit_startup(&emitter, output);
    if(executable) {
        function_symbol(&emitter, entry_owner, entry, symbol, sizeof(symbol));
        fputs("fn main() {\n", output);
        if(has_startup)
            fputs("    ziran_startup();\n", output);
        if(strcmp(entry->return_type, "void") == 0)
            fprintf(output, "    %s();\n", symbol);
        else if(strcmp(entry->return_type, "bool") == 0)
            fprintf(output,
                    "    if %s() { std::process::exit(1); }\n", symbol);
        else
            fprintf(output, "    std::process::exit(%s() as i32);\n", symbol);
        fputs("}\n", output);
    }
    if(fclose(output) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot finish Rust output");
        return 1;
    }
    return 0;
}
