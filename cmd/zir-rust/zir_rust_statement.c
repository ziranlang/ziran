#include "zir_scratch.h"
#include "zir_rust_internal.h"

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

void emit_sequence(RustEmitter *emitter, int begin, int end);
/* Buffers emit_if keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitIfBuffers {
    char condition[ZIR_RUST_TEXT_MAX];
    char plain_condition[ZIR_RUST_TEXT_MAX];
} EmitIfBuffers;

static int emit_if(RustEmitter *emitter, int index, int end);

static int
emit_if_with_buffers(RustEmitter *emitter, int index, int end, EmitIfBuffers *buffers)
{
    const ZirStmt *statement = &emitter->function->stmts[index];
    int close = block_end(emitter->function, index, end);
    emit_expression(emitter, statement->expr_root, buffers->condition,
                    sizeof(buffers->condition));
    write_line(emitter, "if %s {", rust_bare(buffers->condition, buffers->plain_condition, sizeof(buffers->plain_condition)));
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

COMPILER_SCRATCH_RETURN(static, int, emit_if,
    (RustEmitter *emitter, int index, int end),
    EmitIfBuffers, POOLED, free,
    emit_if_with_buffers(emitter, index, end, buffers))

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
/* Buffers rust_zero_value keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RustZeroValueBuffers {
    char value[ZIR_RUST_TEXT_MAX];
    char zero[ZIR_RUST_TEXT_MAX];
} RustZeroValueBuffers;

void rust_zero_value(RustEmitter *emitter, const char *type,
                            char *output, size_t size);

static void
rust_zero_value_with_buffers(RustEmitter *emitter, const char *type,
                            char *output, size_t size, RustZeroValueBuffers *buffers)
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
        rust_zero_value(emitter, element, buffers->value, sizeof(buffers->value));
        snprintf(output, size,
                 "ZiranOption { has_value: false, value: %s }", buffers->value);
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
        rust_zero_value(emitter, element, buffers->zero, sizeof(buffers->zero));
        /* [x; N] needs Copy elements; others are listed. */
        if(rust_copyable_type(emitter, element) || capacity == 0)
            snprintf(output, size, "[%s; %d]", buffers->zero, capacity);
        else {
            size_t used = (size_t)snprintf(output, size, "[");
            for(int index = 0; index < capacity && used < size; index++)
                used += (size_t)snprintf(output + used, size - used, "%s%s",
                                         index ? ", " : "", buffers->zero);
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
        /* Field types are spelled in the record's own module. A union
         * sets only its first field. */
        const ZirModule *saved_module = emitter->module;
        if(owner != NULL)
            emitter->module = owner;
        rust_field_name(record, field.name, field_name, sizeof(field_name));
        rust_zero_value(emitter, field.type, buffers->value, sizeof(buffers->value));
        emitter->module = saved_module;
        used = strlen(output);
        snprintf(output + used, size - used, "%s%s: %s", first ? "" : ", ",
                 field_name, buffers->value);
        first = 0;
    }
    used = strlen(output);
    snprintf(output + used, size - used, " }");
}

COMPILER_SCRATCH_VOID(, rust_zero_value,
    (RustEmitter *emitter, const char *type,
                            char *output, size_t size),
    RustZeroValueBuffers, POOLED, free,
    rust_zero_value_with_buffers(emitter, type, output, size, buffers))

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
/* Whether a binding, declared by statement declaration or, at -1, a
 * parameter, is ever changed and so needs mut. */
int rust_name_mutable(const ZirFunction *function, const char *name,
                             const char *type, int declaration)
{
    if(strcmp(type, "string") != 0 &&
       (rust_scalar_type(type) == NULL || !strcmp(type, "void")))
        return 1;
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        if(StatementHasAssembly(statement)) {
            const char *names[30], *types[30];
            int outputs = 0;
            if(AssemblyBlockOperands(function, index, names, types, 30, &outputs) >= 0)
                for(int output = 0; output < outputs; output++)
                    if(!strcmp(names[output], name)) return 1;
        }
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

static int rust_binding_mutable(const ZirFunction *function, int declaration)
{
    const ZirStmt *declared = &function->stmts[declaration];
    return rust_name_mutable(function, declared->name, declared->type, declaration);
}

void emit_sequence(RustEmitter *emitter, int begin, int end);

static int rust_native_step(const RustEmitter *emitter, int loop)
{
    for(int i = 0; i < emitter->native_loop_count; i++)
        if(emitter->native_loops[i] == loop)
            return 1;
    return 0;
}
/* Buffers emit_native_for keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitNativeForBuffers {
    char start[ZIR_RUST_TEXT_MAX];
    char bound[ZIR_RUST_TEXT_MAX];
} EmitNativeForBuffers;

static int emit_native_for(RustEmitter *emitter, int open, int close);

static int
emit_native_for_with_buffers(RustEmitter *emitter, int open, int close, EmitNativeForBuffers *buffers)
{
    const ZirFunction *function = emitter->function;
    const ZirStmt *counter, *loop;
    const ZirExpr *condition;
    int loop_close, reverse = 0, labeled = 0;
    if(open + 2 >= close || emitter->native_loop_count >= 64)
        return 0;
    counter = &function->stmts[open + 1];
    loop = &function->stmts[open + 2];
    if(counter->kind != ZIR_STMT_DECL || counter->expr_root < 0 ||
       loop->kind != ZIR_STMT_WHILE || !loop->for_form || loop->is_parallel ||
       loop->expr_root < 0)
        return 0;
    loop_close = block_end(function, open + 2, close);
    if(loop_close + 1 != close)
        return 0;
    condition = &function->exprs[loop->expr_root];
    if(condition->kind != ZIR_EXPR_BINARY ||
       function->exprs[condition->left].kind != ZIR_EXPR_IDENT ||
       strcmp(function->exprs[condition->left].name, counter->name) ||
       (strcmp(condition->op, "<=") && strcmp(condition->op, "<") &&
        strcmp(condition->op, ">=")))
        return 0;
    for(int s = open + 3; s < loop_close; s++)
        if(function->stmts[s].for_step == loop->loop_id) {
            reverse = !strcmp(function->stmts[s].assignment_op, "-=");
            break;
        }
    if(reverse != !strcmp(condition->op, ">="))
        return 0;
    emit_typed_expression(emitter, counter->expr_root, counter->type, buffers->start, sizeof(buffers->start));
    emit_typed_expression(emitter, condition->right, counter->type, buffers->bound, sizeof(buffers->bound));
    for(int target = 0; target < function->stmt_count; target++)
        if(loop->loop_id && function->stmts[target].target_id == loop->loop_id)
            labeled = 1;
    register_local(emitter, counter->name);
    char label[32] = "";
    if(labeled)
        snprintf(label, sizeof(label), "'zir_loop_%d: ", loop->loop_id);
    if(reverse)
        write_line(emitter, "%sfor %s in (%s..=%s).rev() {", label,
                   local_name(emitter, counter->name), buffers->bound, buffers->start);
    else
        write_line(emitter, "%sfor %s in %s..%s%s {", label,
                   local_name(emitter, counter->name), buffers->start,
                   !strcmp(condition->op, "<=") ? "=" : "", buffers->bound);
    emitter->native_loops[emitter->native_loop_count++] = loop->loop_id;
    emitter->indent++;
    emit_sequence(emitter, open + 3, loop_close);
    emitter->indent--;
    emitter->native_loop_count--;
    write_line(emitter, "}");
    return 1;
}

/* A lowered counting loop, { step: s64 = 0; while step <= 2 { ...;
 * step += 1 } }, is for step in 0i64..=2i64 in Rust, or .rev() for one
 * counting down. The range steps, so the marked step statements go. */
COMPILER_SCRATCH_RETURN(static, int, emit_native_for,
    (RustEmitter *emitter, int open, int close),
    EmitNativeForBuffers, POOLED, free,
    emit_native_for_with_buffers(emitter, open, close, buffers))
/* Buffers emit_sequence keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSequenceBuffers {
    char value[ZIR_RUST_TEXT_MAX];
    char destination[ZIR_RUST_TEXT_MAX];
    char plain_value[ZIR_RUST_TEXT_MAX];
} EmitSequenceBuffers;

void emit_sequence(RustEmitter *emitter, int begin, int end);

static void
emit_sequence_with_buffers(RustEmitter *emitter, int begin, int end, EmitSequenceBuffers *buffers)
{
    char type_name[ZIR_NAME_MAX];
    for(int index = begin; index < end; index++) {
        const ZirStmt *statement = &emitter->function->stmts[index];
        if(statement->kind == ZIR_STMT_ASSIGN && statement->for_step &&
           rust_native_step(emitter, statement->for_step))
            continue;
        switch(statement->kind) {
        case ZIR_STMT_DECL:
            require_rust_type(emitter, statement->span, statement->type,
                              type_name, sizeof(type_name));
            /* The value is lowered before the name is bound, so an
             * initializer can read a shadowed outer binding. */
            if(statement->expr_root >= 0)
                emit_typed_expression(emitter, statement->expr_root,
                                      statement->type, buffers->value, sizeof(buffers->value));
            else
                rust_zero_value(emitter, statement->type, buffers->value, sizeof(buffers->value));
            register_local(emitter, statement->name);
            write_line(emitter, "let %s%s: %s = %s;",
                       rust_binding_mutable(emitter->function, index) ? "mut " : "",
                       local_name(emitter, statement->name), type_name, buffers->value);
            break;
        case ZIR_STMT_ASSIGN: {
            const ZirExpr *target = &emitter->function->exprs[statement->lhs_root];
            emit_destination(emitter, statement->lhs_root, buffers->destination,
                             sizeof(buffers->destination));
            emit_typed_expression(emitter, statement->expr_root,
                                  emitter->function->exprs[statement->lhs_root].type,
                                  buffers->value, sizeof(buffers->value));
            if(strcmp(statement->assignment_op, "=") == 0)
                write_line(emitter, "%s = %s;", buffers->destination, buffers->value);
            else
                emit_compound_assignment(emitter, statement, buffers->destination,
                                         buffers->value);
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
            emit_expression(emitter, statement->expr_root, buffers->value,
                            sizeof(buffers->value));
            /* A call stands as a statement; another value is dropped. */
            if(emitter->function->exprs[statement->expr_root].kind == ZIR_EXPR_CALL)
                write_line(emitter, "%s;", buffers->value);
            else
                write_line(emitter, "let _ = %s;", buffers->value);
            break;
        case ZIR_STMT_IF:
            index = emit_if(emitter, index, end);
            break;
        case ZIR_STMT_WHILE: {
            int close = block_end(emitter->function, index, end);
            emit_expression(emitter, statement->expr_root, buffers->value,
                            sizeof(buffers->value));
            if(statement->is_parallel || statement->is_gpu)
                write_line(emitter,
                           "// ziran: #parallel region downgraded to serial");
            /* A loop a named break or continue targets carries a label. */
            int labeled = 0;
            for(int target = 0; target < emitter->function->stmt_count; target++)
                if(statement->loop_id &&
                   emitter->function->stmts[target].target_id == statement->loop_id)
                    labeled = 1;
            if(labeled)
                write_line(emitter, "'zir_loop_%d: while %s {", statement->loop_id,
                           rust_bare(buffers->value, buffers->plain_value, sizeof(buffers->plain_value)));
            else
                write_line(emitter, "while %s {", rust_bare(buffers->value, buffers->plain_value, sizeof(buffers->plain_value)));
            emitter->indent++;
            emit_sequence(emitter, index + 1, close);
            emitter->indent--;
            write_line(emitter, "}");
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close = block_end(emitter->function, index, end);
            if(StatementHasAssembly(statement) && emit_rust_assembly(emitter, index)) {
                emitter->indent += 2;
                emit_sequence(emitter, index + 1, close);
                emitter->indent -= 2;
                write_line(emitter, "    }");
                write_line(emitter, "}");
                index = close;
                break;
            }
            if(emit_native_for(emitter, index, close)) {
                index = close;
                break;
            }
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
                                      emitter->function->return_type, buffers->value,
                                      sizeof(buffers->value));
                write_line(emitter, "return %s;", buffers->value);
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

COMPILER_SCRATCH_VOID(, emit_sequence,
    (RustEmitter *emitter, int begin, int end),
    EmitSequenceBuffers, POOLED, free,
    emit_sequence_with_buffers(emitter, begin, end, buffers))
