/*
 * zir_rust_lower.c - checked ZIR to Rust backend.
 */
#include "zir_rust_lower.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"

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

static const char *const rust_keywords[] = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn",
    "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in",
    "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
    "self", "Self", "static", "struct", "super", "trait", "true", "type",
    "unsafe", "use", "where", "while", NULL
};

static int identifier_character(int character)
{
    return isalnum((unsigned char)character) || character == '_';
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

static void require_rust_type(ZirSourceSpan span, const char *type)
{
    if(rust_scalar_type(type) != NULL)
        return;
    Diagnostic(span, "zir_rust.type",
               "the initial Rust target supports scalar non-text types only: %s",
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

static void function_symbol(RustEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output,
                            size_t size)
{
    NativeGoFunctionName(emitter->programs, emitter->program_count, module,
                         function, output, size);
}

static void unsupported_expression(RustEmitter *emitter,
                                   const ZirExpr *expression)
{
    Diagnostic(expression->span, "zir_rust.expression",
               "unsupported expression in the initial Rust target: %s",
               expression->text[0] ? expression->text :
                   ExprKindName(expression->kind));
    exit(1);
}

static void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

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
    suffix = "i64";
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
    case ZIR_EXPR_IDENT:
        snprintf(output, size, "%s", local_name(emitter, expression->name));
        break;
    case ZIR_EXPR_INT:
        emit_integer_literal(expression, output, size);
        break;
    case ZIR_EXPR_FLOAT:
        snprintf(output, size, "%s%s", expression->text,
                 !strcmp(expression->type, "float32") ? "f32" : "f64");
        break;
    case ZIR_EXPR_CALL:
        emit_call(emitter, expression, output, size);
        break;
    case ZIR_EXPR_BINARY:
        emit_expression(emitter, expression->left, left, sizeof(left));
        emit_expression(emitter, expression->right, right, sizeof(right));
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
                                     const ZirExpr *destination,
                                     const char *value)
{
    char operation[4];
    const ZirExpr *left = &emitter->function->exprs[statement->lhs_root];
    const char *name = local_name(emitter, destination->name);
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

static void emit_sequence(RustEmitter *emitter, int begin, int end)
{
    char value[ZIR_RUST_TEXT_MAX];
    for(int index = begin; index < end; index++) {
        const ZirStmt *statement = &emitter->function->stmts[index];
        switch(statement->kind) {
        case ZIR_STMT_DECL:
            require_rust_type(statement->span, statement->type);
            if(statement->expr_root >= 0) {
                emit_typed_expression(emitter, statement->expr_root,
                                      statement->type, value, sizeof(value));
                write_line(emitter, "let mut %s: %s = %s;",
                           local_name(emitter, statement->name),
                           rust_scalar_type(statement->type), value);
            } else if(!strcmp(statement->type, "bool")) {
                write_line(emitter, "let mut %s: bool = false;",
                           local_name(emitter, statement->name));
            } else if(float_type(statement->type)) {
                write_line(emitter, "let mut %s: %s = 0.0%s;",
                           local_name(emitter, statement->name),
                           rust_scalar_type(statement->type),
                           !strcmp(statement->type, "float32") ? "f32" : "f64");
            } else {
                write_line(emitter, "let mut %s: %s = 0;",
                           local_name(emitter, statement->name),
                           rust_scalar_type(statement->type));
            }
            register_local(emitter, statement->name);
            break;
        case ZIR_STMT_ASSIGN: {
            const ZirExpr *target = &emitter->function->exprs[statement->lhs_root];
            if(target->kind != ZIR_EXPR_IDENT)
                goto unsupported_statement;
            emit_typed_expression(emitter, statement->expr_root,
                                  emitter->function->exprs[statement->lhs_root].type,
                                  value, sizeof(value));
            if(strcmp(statement->assignment_op, "=") == 0)
                write_line(emitter, "%s = %s;",
                           local_name(emitter, target->name), value);
            else
                emit_compound_assignment(emitter, statement, target, value);
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
    if(module->global_count || module->define_count || module->type_count) {
        Diagnostic(module->span, "zir_rust.module",
                   "the initial Rust target supports scalar procedures only");
        exit(1);
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
       function->export_symbol[0] || function->is_global_initializer) {
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
        require_rust_type(function->span, type);
        fprintf(emitter->output, "%smut %s: %s", index ? ", " : "",
                rust_name, rust_scalar_type(type));
        register_local(emitter, source_name);
    }
    fputc(')', emitter->output);
    require_rust_type(function->span, function->return_type);
    if(*rust_scalar_type(function->return_type))
        fprintf(emitter->output, " -> %s",
                rust_scalar_type(function->return_type));
    fputs(" {\n", emitter->output);
    emitter->indent = 1;
    emit_sequence(emitter, 0, function->stmt_count);
    emitter->indent = 0;
    fputs("}\n\n", emitter->output);
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
    fputs("#![allow(non_snake_case)]\n#![allow(unused)]\n\n", output);
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
    if(executable) {
        function_symbol(&emitter, entry_owner, entry, symbol, sizeof(symbol));
        fputs("fn main() {\n", output);
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
