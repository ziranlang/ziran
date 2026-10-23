#include "zir_rust_internal.h"

/* Buffers emit_destination keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitDestinationBuffers {
    char global_name[ZIR_RUST_NAME_MAX * 2];
    char base[ZIR_RUST_TEXT_MAX];
    char pointer[ZIR_RUST_TEXT_MAX];
    char index[ZIR_RUST_TEXT_MAX];
} EmitDestinationBuffers;

void emit_destination(RustEmitter *emitter, int index, char *output,
                             size_t size);

static void
emit_destination_with_buffers(RustEmitter *emitter, int index, char *output,
                             size_t size, EmitDestinationBuffers *buffers)
{
    const ZirExpr *expression;
    if(index < 0 || index >= emitter->function->expr_count) {
        snprintf(output, size, "()");
        return;
    }
    expression = &emitter->function->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        if(global_reference(emitter, expression->name, buffers->global_name,
                            sizeof(buffers->global_name)))
            snprintf(output, size, "%s", buffers->global_name);
        else
            snprintf(output, size, "%s", local_name(emitter, expression->name));
        return;
    }
    if(expression->kind == ZIR_EXPR_MEMBER) {
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
                emit_destination(emitter, expression->left, buffers->base,
                                 sizeof(buffers->base));
                snprintf(output, size, "(%s.%s as i64)", buffers->base,
                         owned_vec ? "count" : "len");
                return;
            }
        }
        if(base_type[0] == '*' &&
           rust_record_type(emitter, base_type + 1, &owner, &record)) {
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
            rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
            snprintf(output, size, "(*%s).%s", buffers->base, field);
            return;
        }
        if(!rust_record_type(emitter, base_type, &owner, &record)) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "%s.%s", buffers->base, field);
        return;
    }
    if(expression->kind == ZIR_EXPR_POINTER_MEMBER) {
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        const char *base_type = emitter->function->exprs[expression->left].type;
        if(base_type[0] != '*' ||
           !rust_record_type(emitter, base_type + 1, &owner, &record)) {
            unsupported_expression(emitter, expression);
            return;
        }
        emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "(*%s).%s", buffers->base, field);
        return;
    }
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "*")) {
        emit_expression(emitter, expression->right, buffers->pointer, sizeof(buffers->pointer));
        snprintf(output, size, "(*%s)", buffers->pointer);
        return;
    }
    if(expression->kind == ZIR_EXPR_INDEX) {
        char element[ZIR_NAME_MAX];
        emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
        emit_expression(emitter, expression->right, buffers->index, sizeof(buffers->index));
        if(rust_owned_vec_type(
               emitter, emitter->function->exprs[expression->left].type,
               NULL, NULL, element, sizeof(element)))
            snprintf(output, size, "(*%s.element(%s as i64))", buffers->base, buffers->index);
        else if(emitter->function->exprs[expression->left].type[0] == '*')
            snprintf(output, size, "(*%s.offset(%s as isize))", buffers->base, buffers->index);
        else if(SliceElementType(
                    emitter->function->exprs[expression->left].type, element,
                    sizeof(element)))
            snprintf(output, size, "(*%s.element(%s as i64))", buffers->base, buffers->index);
        else
            snprintf(output, size, "%s[%s as usize]", buffers->base, buffers->index);
        return;
    }
    if(expression->kind == ZIR_EXPR_MEMBER &&
       SliceElementType(emitter->function->exprs[expression->left].type,
                        NULL, 0) &&
       !strcmp(expression->name, "count")) {
        emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
        snprintf(output, size, "(%s.len as i64)", buffers->base);
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

void
emit_destination(RustEmitter *emitter, int index, char *output,
                             size_t size)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    EmitDestinationBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_destination_with_buffers(emitter, index, output, size, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
}

/* A body needs an unsafe block only to reach static mut globals or go
 * through raw pointers; foreign calls and checked indexing carry their own
 * unsafe blocks. */
static int rust_union_field(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_union;
}

int rust_function_needs_unsafe(const ZirModule *module, const ZirFunction *function)
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
int rust_text_needs_unsafe(const char *text, size_t size)
{
    static const char *const operations[] = {
        ".offset(", ".read()", ".write(", ".element(", "from_raw_parts", "core::ptr::",
        "std::ptr::", "std::alloc::", "core::mem::zeroed", NULL};
    int depth = 0, inline_depth = -1;
    for(size_t index = 0; index < size; index++) {
        /* Assembly templates and source strings can contain braces and
         * apparent operations. They are data, not Rust lexical scopes. */
        if(text[index] == '"') {
            while(++index < size) {
                if(text[index] == '\\' && index + 1 < size) index++;
                else if(text[index] == '"') break;
            }
            continue;
        }
        if(index + 1 < size && text[index] == '/' && text[index + 1] == '/') {
            while(index + 1 < size && text[index + 1] != '\n') index++;
            continue;
        }
        if(index + 1 < size && text[index] == '/' && text[index + 1] == '*') {
            int comments = 1;
            index += 2;
            while(index + 1 < size && comments) {
                if(text[index] == '/' && text[index + 1] == '*') { comments++; index += 2; }
                else if(text[index] == '*' && text[index + 1] == '/') { comments--; index += 2; }
                else index++;
            }
            if(index > 0) index--;
            continue;
        }
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
const char *rust_bare(const char *text, char *output, size_t size)
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
int rust_plain_receiver(const char *text)
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
/* Buffers emit_typed_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitTypedExpressionBuffers {
    char value[ZIR_RUST_TEXT_MAX];
} EmitTypedExpressionBuffers;

void emit_typed_expression(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size);

static void
emit_typed_expression_with_buffers(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size, EmitTypedExpressionBuffers *buffers)
{
    const ZirExpr *expression = index >= 0 &&
        index < emitter->function->expr_count ?
            &emitter->function->exprs[index] : NULL;
    if(expression != NULL && !strcmp(expression->type, "integer") && float_type(type)) {
        if(expression->kind == ZIR_EXPR_INT) {
            /* Rust has no hexadecimal float literals. Spell the checked
             * integer magnitude in decimal before giving it a float suffix. */
            snprintf(output, size, "%llu%s", strtoull(expression->text, NULL, 0),
                     rust_scalar_type(type));
            return;
        }
        if(expression->kind == ZIR_EXPR_UNARY &&
           (!strcmp(expression->op, "-") || !strcmp(expression->op, "+"))) {
            /* Apply the sign after coercion so a large negative literal does
             * not first have to fit Rust's default i32. */
            emit_typed_expression(emitter, expression->right, type,
                                  buffers->value, sizeof(buffers->value));
            snprintf(output, size, "(%s%s)",
                     !strcmp(expression->op, "+") ? "" : expression->op, buffers->value);
            return;
        }
    }
    emit_expression(emitter, index, buffers->value, sizeof(buffers->value));
    /* An untyped literal takes the type as a suffix: 2026i32. */
    if(expression != NULL && expression->kind == ZIR_EXPR_INT &&
       !strcmp(expression->type, "integer") && rust_scalar_type(type) != NULL &&
       isdigit((unsigned char)buffers->value[0]) && strspn(buffers->value, "0123456789abcdefABCDEFxX") == strlen(buffers->value))
        snprintf(output, size, "%s%s", buffers->value, rust_scalar_type(type));
    else if(expression != NULL && strcmp(expression->type, type) != 0 &&
       rust_scalar_type(type) != NULL &&
       rust_scalar_type(expression->type) != NULL)
        snprintf(output, size, "((%s) as %s)", buffers->value,
                 rust_scalar_type(type));
    else
        snprintf(output, size, "%s", buffers->value);
}

void
emit_typed_expression(RustEmitter *emitter, int index,
                                  const char *type, char *output, size_t size)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    EmitTypedExpressionBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_typed_expression_with_buffers(emitter, index, type, output, size, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
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

void emit_expression(RustEmitter *emitter, int index, char *output,
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
/* Text inside a print! format: Rust string escapes, and braces doubled. */
static void rust_format_text(char *format, size_t *used, const unsigned char *bytes,
                             size_t length)
{
    for(size_t i = 0; i < length && *used + 12 < ZIR_RUST_TEXT_MAX; i++) {
        unsigned direction = rust_direction_codepoint(bytes + i, length - i);
        if(direction != 0) {
            *used += (size_t)snprintf(format + *used, ZIR_RUST_TEXT_MAX - *used,
                                    "\\u{%x}", direction);
            i += 2;
            continue;
        }
        unsigned char byte = bytes[i];
        const char *escape = byte == '\n' ? "\\n" : byte == '\t' ? "\\t" :
            byte == '\r' ? "\\r" : byte == '"' ? "\\\"" : byte == '\\' ? "\\\\" :
            byte == '{' ? "{{" : byte == '}' ? "}}" : NULL;
        if(escape != NULL)
            *used += (size_t)snprintf(format + *used, ZIR_RUST_TEXT_MAX - *used, "%s", escape);
        else if(byte < 0x20 || byte == 0x7f)
            *used += (size_t)snprintf(format + *used, ZIR_RUST_TEXT_MAX - *used, "\\u{%x}", byte);
        else
            format[(*used)++] = (char)byte;
    }
}
/* Buffers emit_print_statement keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitPrintStatementBuffers {
    unsigned char bytes[ZIR_TEXT_MAX];
    char value[ZIR_RUST_TEXT_MAX];
    char raw[ZIR_RUST_TEXT_MAX];
    char copied[ZIR_RUST_TEXT_MAX];
} EmitPrintStatementBuffers;

int emit_print_statement(RustEmitter *emitter, const ZirExpr *expression);

static int
emit_print_statement_with_buffers(RustEmitter *emitter, const ZirExpr *expression, EmitPrintStatementBuffers *buffers)
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
            size_t length;
            if(!DecodeStringLiteral(pieces[piece].literal, buffers->bytes, sizeof(buffers->bytes), &length))
                break;
            rust_format_text(format, &format_used, buffers->bytes, length);
            continue;
        }
        if(argument_node < 0)
            break;
        const char *type = function->exprs[argument_node].type;
        /* A literal string argument is simply part of the text. */
        if(function->exprs[argument_node].kind == ZIR_EXPR_STRING) {
            size_t length;
            if(DecodeStringLiteral(function->exprs[argument_node].text, buffers->bytes,
                                   sizeof(buffers->bytes), &length)) {
                rust_format_text(format, &format_used, buffers->bytes, length);
                argument_node = function->exprs[argument_node].next_sibling;
                continue;
            }
        }
        {
            int later_calls = 0;
            emit_expression(emitter, argument_node, buffers->raw, sizeof(buffers->raw));
            rust_bare(buffers->raw, buffers->value, sizeof(buffers->value));
            for(int next = function->exprs[argument_node].next_sibling; next >= 0;
                next = function->exprs[next].next_sibling)
                later_calls |= rust_expression_calls(function, next);
            /* print! borrows its arguments and reads them after every one
             * has run, so a value a later call could change is copied out
             * first. A static is always copied: Rust warns on borrowing one. */
            if(function->exprs[argument_node].kind != ZIR_EXPR_CALL &&
               ((later_calls && function->exprs[argument_node].kind != ZIR_EXPR_INT &&
                 function->exprs[argument_node].kind != ZIR_EXPR_FLOAT) ||
                !strncmp(buffers->value, "ziran_global_", 13))) {
                snprintf(buffers->copied, sizeof(buffers->copied),
                         !strcmp(type, "string") ? "%s.clone()" : "{ %s }", buffers->value);
                snprintf(buffers->value, sizeof(buffers->value), "%s", buffers->copied);
            }
        }
        format_used += (size_t)snprintf(format + format_used,
                                        ZIR_RUST_TEXT_MAX - format_used, "{}");
        if(!strcmp(type, "string"))
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used,
                rust_plain_receiver(buffers->value) ? ", %s.as_str()" : ", (%s).as_str()", buffers->value);
        else if(!strcmp(type, "float32") || !strcmp(type, "float64"))
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used, ", ZiranFloat(%s)", buffers->value);
        else
            arguments_used += (size_t)snprintf(arguments + arguments_used,
                ZIR_RUST_TEXT_MAX - arguments_used, ", %s", buffers->value);
        argument_node = function->exprs[argument_node].next_sibling;
    }
    format[format_used] = '\0';
    /* A line ending in a newline is println!, as Rust code writes it. */
    if(format_used >= 2 && !strcmp(format + format_used - 2, "\\n") &&
       (format_used < 3 || format[format_used - 3] != '\\')) {
        format[format_used - 2] = '\0';
        write_line(emitter, "println!(\"%s\"%s);", format, arguments);
    } else
        write_line(emitter, "print!(\"%s\"%s);", format, arguments);
    free(pieces);
    free(format);
    free(arguments);
    return 1;
}

int
emit_print_statement(RustEmitter *emitter, const ZirExpr *expression)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    EmitPrintStatementBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    int returned = emit_print_statement_with_buffers(emitter, expression, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
    return returned;
}
/* Buffers emit_call keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitCallBuffers {
    char symbol[ZIR_RUST_NAME_MAX * 2];
    char arguments[ZIR_RUST_TEXT_MAX];
    char child[ZIR_RUST_TEXT_MAX];
    char value[ZIR_RUST_TEXT_MAX];
    char index[ZIR_RUST_TEXT_MAX];
    char source[ZIR_RUST_TEXT_MAX];
    char low[ZIR_RUST_TEXT_MAX];
    char high[ZIR_RUST_TEXT_MAX];
    char other[ZIR_RUST_TEXT_MAX];
    char text[ZIR_RUST_TEXT_MAX];
    char callable[ZIR_RUST_TEXT_MAX];
    char global_name[ZIR_RUST_NAME_MAX * 2];
} EmitCallBuffers;

static void emit_call(RustEmitter *emitter, const ZirExpr *expression,
                      char *output, size_t size);

static void
emit_call_with_buffers(RustEmitter *emitter, const ZirExpr *expression,
                      char *output, size_t size, EmitCallBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    buffers->arguments[0] = '\0';
    char element[ZIR_NAME_MAX];
    const ZirImport *foreign = rust_foreign_import(emitter, expression->name);
    if(!strcmp(expression->name, "zi_new") && expression->type[0] == '*') {
        if(!rust_type(emitter, expression->type + 1, element, sizeof(element))) {
            unsupported_expression(emitter, expression);
            return;
        }
        snprintf(output, size, "ZiranNew::<%s>()", element);
        return;
    }
    if(!strcmp(expression->name, "zi_free") && expression->first_child >= 0) {
        emit_expression(emitter, expression->first_child, buffers->child, sizeof(buffers->child));
        snprintf(output, size, "unsafe { ZiranFree(%s) }", buffers->child);
        return;
    }
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
        emit_expression(emitter, first, buffers->child, sizeof(buffers->child));
        if(!strcmp(expression->name, "VecFree")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecFree(&mut %s)", buffers->child);
        } else if(!strcmp(expression->name, "VecPush")) {
            if(second < 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->value, sizeof(buffers->value));
            snprintf(output, size, "ZiranVecPush(&mut %s, %s)", buffers->child, buffers->value);
        } else if(!strcmp(expression->name, "VecPop")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecPop(&mut %s)", buffers->child);
        } else if(!strcmp(expression->name, "VecGet")) {
            if(second < 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->index, sizeof(buffers->index));
            snprintf(output, size, "ZiranVecGet(&mut %s, %s as usize)",
                     buffers->child, buffers->index);
        } else if(!strcmp(expression->name, "VecClone")) {
            if(third >= 0 || second < 0 ||
               strcmp(emitter->function->exprs[first].type,
                      emitter->function->exprs[second].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->source, sizeof(buffers->source));
            snprintf(output, size, "ZiranVecClone(&mut %s, &%s)",
                     buffers->child, buffers->source);
            return;
        } else if(!strcmp(expression->name, "VecSlice")) {
            int fourth = third >= 0 ?
                emitter->function->exprs[third].next_sibling : -1;
            if(third < 0 || fourth >= 0 ||
               !integer_type(emitter->function->exprs[second].type) ||
               !integer_type(emitter->function->exprs[third].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->low, sizeof(buffers->low));
            emit_expression(emitter, third, buffers->high, sizeof(buffers->high));
            snprintf(output, size,
                     "ZiranVecSlice(&%s, %s as isize, %s as isize)",
                    buffers->child, buffers->low, buffers->high);
            return;
        } else if(!strcmp(expression->name, "VecClear")) {
            if(second >= 0) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranVecClear(&mut %s)", buffers->child);
        } else if(!strcmp(expression->name, "VecSwap")) {
            if(second < 0 || third >= 0 ||
               strcmp(emitter->function->exprs[first].type,
                      emitter->function->exprs[second].type)) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->other, sizeof(buffers->other));
            snprintf(output, size, "core::mem::swap(&mut %s, &mut %s)",
                     buffers->child, buffers->other);
        } else if(!strcmp(expression->name, "BuilderAppend")) {
            if(second < 0 || third >= 0 || strcmp(element, "u8") ||
               strcmp(emitter->function->exprs[second].type, "string")) {
                unsupported_expression(emitter, expression);
                return;
            }
            emit_expression(emitter, second, buffers->text, sizeof(buffers->text));
            snprintf(output, size, "ZiranBuilderAppend(&mut %s, %s)",
                     buffers->child, buffers->text);
        } else {
            if(second >= 0 || strcmp(element, "u8")) {
                unsupported_expression(emitter, expression);
                return;
            }
            snprintf(output, size, "ZiranBuilderFinish(&mut %s)", buffers->child);
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
        emit_expression(emitter, expression->first_child, buffers->child,
                        sizeof(buffers->child));
        snprintf(output, size,
                 "ZiranText { data: %s.data as *const u8, len: %s.len }",
                 buffers->child, buffers->child);
        return;
    }
    if(expression->slot_type[0]) {
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = FindType(emitter->module,
                                       expression->slot_type, &slot_owner);
        if(expression->left >= 0)
            emit_expression(emitter, expression->left, buffers->callable,
                            sizeof(buffers->callable));
        else if(global_reference(emitter, expression->name, buffers->global_name,
                                 sizeof(buffers->global_name)))
            snprintf(buffers->callable, sizeof(buffers->callable), "%s", buffers->global_name);
        else
            snprintf(buffers->callable, sizeof(buffers->callable), "%s",
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
                emit_expression(emitter, chosen, buffers->child, sizeof(buffers->child));
                if(*buffers->arguments)
                    strncat(buffers->arguments, ", ",
                            sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
                strncat(buffers->arguments, buffers->child,
                        sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
            }
        }
        if(slot != NULL && slot->is_procedure_type && !slot->is_c_call)
            snprintf(output, size, "unsafe { %s.call.unwrap()(%s.context%s%s) }",
                     buffers->callable, buffers->callable, *buffers->arguments ? ", " : "", buffers->arguments);
        else
            snprintf(output, size, "unsafe { %s.unwrap()(%s) }", buffers->callable, buffers->arguments);
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
        rust_extern_symbol(emitter, emitter->module, foreign, buffers->symbol,
                           sizeof(buffers->symbol));
        for(int child_index = expression->first_child; child_index >= 0;
            child_index = emitter->function->exprs[child_index].next_sibling) {
            emit_expression(emitter, child_index, buffers->child, sizeof(buffers->child));
            if(*buffers->arguments)
                strncat(buffers->arguments, ", ",
                        sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
            strncat(buffers->arguments, buffers->child,
                    sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
        }
        snprintf(output, size, "unsafe { %s(%s) }", buffers->symbol, buffers->arguments);
        return;
    }
    if(ResolveFunctionAt(emitter->module, expression->name,
                         SpanPath(emitter->function->span), &owner, &callee) != 1 ||
       owner == NULL) {
        unsupported_expression(emitter, expression);
        return;
    }
    if(callee->is_extern || callee->is_template) {
        unsupported_expression(emitter, expression);
        return;
    }
    function_symbol(emitter, owner, callee, buffers->symbol, sizeof(buffers->symbol));
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
                emit_expression(emitter, ordered[position], buffers->child, sizeof(buffers->child));
                used += (size_t)snprintf(output + used, size > used ? size - used : 0,
                                         "let argument_%d = %s; ", position, buffers->child);
            }
            used += (size_t)snprintf(output + used, size > used ? size - used : 0, "%s(", buffers->symbol);
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
            emit_expression(emitter, chosen, buffers->child, sizeof(buffers->child));
            if(*buffers->arguments)
                strncat(buffers->arguments, ", ", sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
            strncat(buffers->arguments, buffers->child, sizeof(buffers->arguments) - strlen(buffers->arguments) - 1);
        }
    }
    snprintf(output, size, "%s(%s)", buffers->symbol, buffers->arguments);
}

static void
emit_call(RustEmitter *emitter, const ZirExpr *expression,
                      char *output, size_t size)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    EmitCallBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_call_with_buffers(emitter, expression, output, size, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
}

int wrapping_operation(const char *operation)
{
    return !strcmp(operation, "+") || !strcmp(operation, "-") ||
           !strcmp(operation, "*") || !strcmp(operation, "/") ||
           !strcmp(operation, "%");
}

const char *wrapping_method(const char *operation)
{
    if(!strcmp(operation, "+")) return "wrapping_add";
    if(!strcmp(operation, "-")) return "wrapping_sub";
    if(!strcmp(operation, "*")) return "wrapping_mul";
    if(!strcmp(operation, "/")) return "wrapping_div";
    if(!strcmp(operation, "%")) return "wrapping_rem";
    return NULL;
}
/* Buffers emit_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitExpressionBuffers {
    char left[ZIR_RUST_TEXT_MAX];
    char right[ZIR_RUST_TEXT_MAX];
    char third[ZIR_RUST_TEXT_MAX];
    char global_name[ZIR_RUST_NAME_MAX * 2];
    char wrapper[ZIR_RUST_NAME_MAX * 2];
    char function_name[ZIR_RUST_NAME_MAX * 2];
    char base[ZIR_RUST_TEXT_MAX];
    char index[ZIR_RUST_TEXT_MAX];
    char low[ZIR_RUST_TEXT_MAX];
    char high[ZIR_RUST_TEXT_MAX];
    char value[ZIR_RUST_TEXT_MAX];
    char count[ZIR_RUST_TEXT_MAX];
} EmitExpressionBuffers;

void emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size);

static void
emit_expression_with_buffers(RustEmitter *emitter, int index, char *output,
                            size_t size, EmitExpressionBuffers *buffers)
{
    const ZirExpr *expression;
    if(index < 0 || index >= emitter->function->expr_count) {
        snprintf(output, size, "()");
        return;
    }
    expression = &emitter->function->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_IDENT: {
        const ZirModule *function_owner = NULL;
        const ZirFunction *function = NULL;
        const ZirModule *slot_owner = NULL;
        const ZirType *slot = NULL;
        if(!strcmp(expression->type, "null") || !strcmp(expression->name, "null"))
            snprintf(output, size, "core::ptr::null_mut()");
        else if(global_reference(emitter, expression->name, buffers->global_name,
                            sizeof(buffers->global_name)))
            snprintf(output, size, "%s", buffers->global_name);
        else if(expression->is_function_value &&
                rust_procedure_type(emitter, expression->type,
                                    &slot_owner, &slot) &&
                !slot->is_c_call) {
            char type_name[ZIR_NAME_MAX];
            slot_wrapper_symbol(emitter, index, buffers->wrapper, sizeof(buffers->wrapper));
            NativeTypeName(slot_owner, slot, type_name, sizeof(type_name));
            snprintf(output, size,
                     "%s { context: core::ptr::null_mut(), call: Some(%s) }",
                     type_name, buffers->wrapper);
        }
        else if(expression->is_function_value &&
                ResolveFunctionAt(emitter->module, expression->name,
                                  SpanPath(emitter->function->span),
                                  &function_owner, &function) == 1 &&
                function_owner != NULL && function != NULL) {
            function_symbol(emitter, function_owner, function,
                            buffers->function_name, sizeof(buffers->function_name));
            snprintf(output, size, "Some(%s)", buffers->function_name);
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
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        char base_type[ZIR_NAME_MAX];
        snprintf(base_type, sizeof(base_type), "%s",
                 emitter->function->exprs[expression->left].type);
        if((SliceElementType(base_type, NULL, 0) ||
            !strcmp(base_type, "string")) &&
           !strcmp(expression->name, "count")) {
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
            snprintf(output, size, "(%s.len as i64)", buffers->base);
            break;
        }
        if(rust_owned_vec_type(emitter, base_type, NULL, NULL, NULL, 0) &&
           (!strcmp(expression->name, "count") ||
            !strcmp(expression->name, "capacity"))) {
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
            snprintf(output, size, "(%s.%s as i64)", buffers->base,
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
                    emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
                    snprintf(output, size, "%s.as_mut_ptr()", buffers->base);
                    break;
                }
            }
            if(SliceElementType(base_type, NULL, 0) && !strcmp(expression->name, "data")) {
                emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
                snprintf(output, size, "%s.data", buffers->base);
                break;
            }
        }
        /* A field through a pointer reads the pointee: (*node).value. */
        if(base_type[0] == '*' && rust_record_type(emitter, base_type + 1, &owner, &record)) {
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
            rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
            snprintf(output, size, "(*%s).%s", buffers->base, field);
            break;
        }
        if(!rust_record_type(emitter, base_type, &owner, &record)) {
            unsupported_expression(emitter, expression);
            break;
        }
        emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "%s.%s", buffers->base, field);
        break;
    }
    case ZIR_EXPR_POINTER_MEMBER: {
        char field[ZIR_NAME_MAX];
        const ZirModule *owner = NULL;
        const ZirType *record = NULL;
        const char *base_type = emitter->function->exprs[expression->left].type;
        if(base_type[0] == '*' &&
           rust_owned_vec_type(emitter, base_type + 1, NULL, NULL, NULL, 0) &&
           (!strcmp(expression->name, "count") ||
            !strcmp(expression->name, "capacity"))) {
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
            snprintf(output, size, "((*%s).%s as i64)", buffers->base,
                     expression->name);
            break;
        }
        if(base_type[0] != '*' ||
           !rust_record_type(emitter, base_type + 1, &owner, &record)) {
            unsupported_expression(emitter, expression);
            break;
        }
        emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
        rust_member_path(emitter, owner, record, expression->name, field, sizeof(field));
        snprintf(output, size, "(*%s).%s", buffers->base, field);
        break;
    }
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];
        ZirExprKind base_kind = emitter->function->exprs[expression->left].kind;
        /* Only places are borrowed; a literal or call result is indexed as a value. */
        if(base_kind == ZIR_EXPR_COMPOUND || base_kind == ZIR_EXPR_CALL ||
           base_kind == ZIR_EXPR_CONDITIONAL)
            emit_expression(emitter, expression->left, buffers->base, sizeof(buffers->base));
        else
            emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
        emit_expression(emitter, expression->right, buffers->index, sizeof(buffers->index));
        if(!strcmp(emitter->function->exprs[expression->left].type,
                   "string")) {
            snprintf(output, size, "%s.at(%s as i64)", buffers->base, buffers->index);
            break;
        }
        if(emitter->function->exprs[expression->left].type[0] == '*') {
            snprintf(output, size, "(*%s.offset(%s as isize))", buffers->base, buffers->index);
            break;
        }
        if(rust_owned_vec_type(
               emitter, emitter->function->exprs[expression->left].type,
               NULL, NULL, element, sizeof(element))) {
            snprintf(output, size, "%s.at(%s as i64)", buffers->base, buffers->index);
            break;
        }
        if(SliceElementType(
               emitter->function->exprs[expression->left].type, element,
               sizeof(element))) {
            snprintf(output, size, "%s.get(%s as i64)", buffers->base, buffers->index);
        } else {
            snprintf(output, size, "%s[%s as usize]", buffers->base, buffers->index);
        }
        break;
    }
    case ZIR_EXPR_SLICE: {
        char element[ZIR_NAME_MAX];
        int capacity = 0;
        emit_destination(emitter, expression->left, buffers->base, sizeof(buffers->base));
        if(!strcmp(emitter->function->exprs[expression->left].type,
                   "string")) {
            strcpy(buffers->low, "0");
            /* text[:] is the whole text; a missing bound is its start or end. */
            if(expression->right < 0 && expression->third < 0) {
                snprintf(output, size, "%s", buffers->base);
                break;
            }
            if(expression->right >= 0)
                emit_expression(emitter, expression->right, buffers->low, sizeof(buffers->low));
            if(expression->third >= 0)
                emit_expression(emitter, expression->third, buffers->high, sizeof(buffers->high));
            else
                snprintf(buffers->high, sizeof(buffers->high), "%s.len", buffers->base);
            snprintf(output, size,
                     "ZiranText::slice(%s, %s as isize, %s as isize)",
                     buffers->base, buffers->low, buffers->high);
            break;
        }
        {
            /* Bounds come from the checked graph; a missing bound is the
             * start or the end. Out-of-range views fail like other targets. */
            const char *base_type = emitter->function->exprs[expression->left].type;
            int array = ArrayElementType(base_type, element, sizeof(element), &capacity);
            int slice = !array && SliceElementType(base_type, element, sizeof(element));
            strcpy(buffers->low, "0i64");
            if(!array && !slice) {
                unsupported_expression(emitter, expression);
                break;
            }
            if(expression->right >= 0)
                emit_typed_expression(emitter, expression->right, "s64", buffers->low, sizeof(buffers->low));
            if(expression->third >= 0)
                emit_typed_expression(emitter, expression->third, "s64", buffers->high, sizeof(buffers->high));
            else if(array)
                snprintf(buffers->high, sizeof(buffers->high), "%di64", capacity);
            else
                buffers->high[0] = '\0';
            if(array)
                snprintf(output, size,
                         "ZiranSlice::view(ZiranSlice { data: %s.as_mut_ptr(), len: %d }, %s, %s)",
                         buffers->base, capacity, buffers->low, buffers->high);
            else if(buffers->high[0])
                snprintf(output, size, "ZiranSlice::view(%s, %s, %s)", buffers->base, buffers->low, buffers->high);
            else
                snprintf(output, size, "ZiranSlice::view_from(%s, %s)", buffers->base, buffers->low);
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
                emit_typed_expression(emitter,
                                      initializer->kind == ZIR_EXPR_FIELD_INIT ?
                                          initializer->right : child_index,
                                      element, buffers->value, sizeof(buffers->value));
                size_t used = strlen(output);
                snprintf(output + used, size - used, "%s%s", first ? "" : ", ",
                         buffers->value);
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
            char field_name[ZIR_NAME_MAX];
            if(initializer->kind != ZIR_EXPR_FIELD_INIT) {
                unsupported_expression(emitter, initializer);
                return;
            }
            emit_expression(emitter, initializer->right, buffers->value, sizeof(buffers->value));
            size_t used = strlen(output);
            rust_field_name(record, initializer->name, field_name,
                            sizeof(field_name));
            snprintf(output + used, size - used, "%s%s: %s",
                     first ? "" : ", ", field_name, buffers->value);
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
                char field_name[ZIR_NAME_MAX];
                const ZirModule *saved_module = emitter->module;
                if(record_owner != NULL)
                    emitter->module = record_owner;
                rust_zero_value(emitter, field.type, buffers->value, sizeof(buffers->value));
                emitter->module = saved_module;
                rust_field_name(record, field.name, field_name, sizeof(field_name));
                size_t used = strlen(output);
                snprintf(output + used, size - used, "%s%s: %s",
                         first ? "" : ", ", field_name, buffers->value);
                first = 0;
            }
        }
        size_t used = strlen(output);
        snprintf(output + used, size - used, "%s}", first ? "" : " ");
        break;
    }
    case ZIR_EXPR_BINARY:
        buffers->left[0] = buffers->right[0] = '\0';
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
                emit_expression(emitter, value_index, buffers->left, sizeof(buffers->left));
                if(left_null || right_null) {
                    if(slot->is_c_call)
                        snprintf(output, size, "(%s.is_%s())", buffers->left,
                                 equal ? "none" : "some");
                    else
                        snprintf(output, size, "(%s.call.is_%s())", buffers->left,
                                 equal ? "none" : "some");
                } else {
                    emit_expression(emitter, expression->right, buffers->right,
                                    sizeof(buffers->right));
                    if(slot->is_c_call)
                        snprintf(output, size,
                                 "(match (%s, %s) { (left, right) => left %s right })",
                                 buffers->left, buffers->right, equal ? "==" : "!=");
                    else
                        snprintf(output, size,
                                 "(match (%s, %s) { (left, right) => left.call %s right.call %s left.context %s right.context })",
                                 buffers->left, buffers->right, equal ? "==" : "!=",
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
            emit_expression(emitter, expression->left, buffers->left, sizeof(buffers->left));
            emit_expression(emitter, expression->right, buffers->right, sizeof(buffers->right));
            snprintf(output, size, "%sZiranText::eq(%s, %s)",
                     !strcmp(expression->op, "==") ? "" : "!", buffers->left, buffers->right);
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
            /* s32 < s64, as in `index < Width * Height`: Rust compares
             * only equal types, so both operands take the common one. */
            const char *common = expression->left >= 0 && expression->right >= 0 ?
                rust_common_integer(emitter->function->exprs[expression->left].type,
                                    emitter->function->exprs[expression->right].type) : NULL;
            if(expression->left >= 0 && expression->right >= 0) {
                const char *left = emitter->function->exprs[expression->left].type;
                const char *right = emitter->function->exprs[expression->right].type;
                if(float_type(left) && !strcmp(right, "integer")) common = left;
                else if(!strcmp(left, "integer") && float_type(right)) common = right;
            }
            if(common != NULL && integer_type(expression->type) &&
               strcmp(expression->type, "integer") != 0)
                common = expression->type;
            if(untyped) {
                emit_typed_expression(emitter, expression->left, operand_type, buffers->left, sizeof(buffers->left));
                emit_typed_expression(emitter, expression->right, operand_type, buffers->right, sizeof(buffers->right));
            } else if(common != NULL) {
                emit_typed_expression(emitter, expression->left, common, buffers->left, sizeof(buffers->left));
                emit_typed_expression(emitter, expression->right, common, buffers->right, sizeof(buffers->right));
            } else {
                emit_expression(emitter, expression->left, buffers->left, sizeof(buffers->left));
                emit_expression(emitter, expression->right, buffers->right, sizeof(buffers->right));
            }
        }
        if(wrapping) {
            emit_typed_expression(emitter, expression->left, expression->type,
                                  buffers->left, sizeof(buffers->left));
            emit_typed_expression(emitter, expression->right, expression->type,
                                  buffers->right, sizeof(buffers->right));
            uint64_t left_bits, right_bits, folded;
            if(rust_literal_bits(buffers->left, expression->type, &left_bits) &&
               rust_literal_bits(buffers->right, expression->type, &right_bits) &&
               FoldIntegerOperation(expression->op, expression->type,
                                    left_bits, right_bits, &folded))
                rust_literal(expression->type, folded, output, size);
            else if(!strcmp(expression->type, "integer"))
                snprintf(output, size,
                         "((%s as i64).%s(%s as i64))", buffers->left,
                         wrapping_method(expression->op), buffers->right);
            else
                snprintf(output, size, rust_plain_receiver(buffers->left) ? "%s.%s(%s)" : "(%s).%s(%s)",
                         buffers->left, wrapping_method(expression->op), buffers->right);
        }
        else if(!strcmp(expression->op, "<<") || !strcmp(expression->op, ">>")) {
            /* A count below zero or at the width fails, as on other targets;
             * >> on a signed type is arithmetic. */
            emit_typed_expression(emitter, expression->left, expression->type,
                                  buffers->left, sizeof(buffers->left));
            emit_expression(emitter, expression->right, buffers->count, sizeof(buffers->count));
            snprintf(output, size,
                     "%s%s%s.%s(u32::try_from(%s).unwrap_or(u32::MAX)).expect(\"shift count out of range\")",
                     rust_plain_receiver(buffers->left) ? "" : "(", buffers->left,
                     rust_plain_receiver(buffers->left) ? "" : ")",
                     !strcmp(expression->op, "<<") ? "checked_shl" : "checked_shr", buffers->count);
        }
        else
            snprintf(output, size, "(%s %s %s)", buffers->left, expression->op, buffers->right);
        break;
    case ZIR_EXPR_UNARY:
        if(!strcmp(expression->op, "&")) {
            char pointee[ZIR_NAME_MAX];
            emit_destination(emitter, expression->right, buffers->right, sizeof(buffers->right));
            if(!rust_type(emitter, emitter->function->exprs[expression->right].type,
                          pointee, sizeof(pointee))) {
                unsupported_expression(emitter, expression);
                break;
            }
            snprintf(output, size, "(&mut %s as *mut %s)", buffers->right, pointee);
            break;
        }
        emit_expression(emitter, expression->right, buffers->right, sizeof(buffers->right));
        if(!strcmp(expression->op, "!") || !strcmp(expression->op, "-") ||
           !strcmp(expression->op, "+"))
            snprintf(output, size, "(%s%s)",
                     !strcmp(expression->op, "+") ? "" : expression->op, buffers->right);
        else if(!strcmp(expression->op, "~"))
            snprintf(output, size, "(!%s)", buffers->right);
        else if(!strcmp(expression->op, "*"))
            snprintf(output, size, "(*%s)", buffers->right);
        else
            unsupported_expression(emitter, expression);
        break;
    case ZIR_EXPR_CAST: {
        char target[ZIR_NAME_MAX];
        if(!strcmp(expression->type, "null")) {
            snprintf(output, size, "core::ptr::null_mut()");
            break;
        }
        emit_expression(emitter, expression->right, buffers->right, sizeof(buffers->right));
        {
            /* A literal converts at compile time to its wrapped value;
             * Rust rejects 258 as u8. */
            const ZirExpr *operand = &emitter->function->exprs[expression->right];
            size_t length = strspn(buffers->right, "0123456789");
            if(operand->kind == ZIR_EXPR_INT && integer_type(expression->type) &&
               strcmp(expression->type, "integer") != 0 && length > 0 &&
               strspn(buffers->right + length, "iu0123456789") == strlen(buffers->right + length)) {
                errno = 0;
                unsigned long long value = strtoull(buffers->right, NULL, 10);
                if(errno == 0) {
                    rust_literal(expression->type, (uint64_t)value, output, size);
                    break;
                }
            }
        }
        if(rust_scalar_type(expression->type) != NULL)
            snprintf(output, size, "(%s as %s)", buffers->right,
                     rust_scalar_type(expression->type));
        else if(rust_type(emitter, expression->type, target, sizeof(target)))
            snprintf(output, size, "(%s as %s)", buffers->right, target);
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
        emit_expression(emitter, expression->left, buffers->left, sizeof(buffers->left));
        emit_expression(emitter, expression->right, buffers->right, sizeof(buffers->right));
        emit_expression(emitter, expression->third, buffers->third, sizeof(buffers->third));
        snprintf(output, size, "(if %s { %s } else { %s })", buffers->left, buffers->right,
                 buffers->third);
        break;
    default:
        unsupported_expression(emitter, expression);
    }
}

void
emit_expression(RustEmitter *emitter, int index, char *output,
                            size_t size)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    EmitExpressionBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    emit_expression_with_buffers(emitter, index, output, size, buffers);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
}
