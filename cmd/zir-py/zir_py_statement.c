#include "zir_py_internal.h"

static int block_end(const ZirFunction *function, int begin, int end)
{
    int depth = 1;
    for(int index = begin + 1; index < end; index++) {
        ZirStmtKind kind = function->stmts[index].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE || kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return index;
    }
    return end;
}

static void emit_sequence(PyEmitter *emitter, int begin, int end);

/* A block body, with pass when it writes nothing. */
static void emit_body(PyEmitter *emitter, int begin, int end)
{
    long before = emitter->lines;
    emitter->indent++;
    emitter->depth++;
    emit_sequence(emitter, begin, end);
    leave_scope(emitter);
    if(emitter->lines == before)
        write_line(emitter, "pass");
    emitter->indent--;
}

/* An if condition as an operand of and. */
static char *emit_condition_operand(PyEmitter *emitter, int index)
{
    const ZirExpr *condition = expression_at(emitter, index);
    char *text = emit_expression(emitter, index);
    if(condition != NULL && condition->kind == ZIR_EXPR_BINARY && !strcmp(condition->op, "||"))
        return text;
    char *bare = py_bare(text);
    free(text);
    return bare;
}

static int has_else(PyEmitter *emitter, int close, int end)
{
    return close + 1 < end && emitter->function->stmts[close + 1].kind == ZIR_STMT_IF &&
           emitter->function->stmts[close + 1].is_else;
}

static int emit_if(PyEmitter *emitter, int index, int end, int is_elif)
{
    const ZirStmt *statement = &emitter->function->stmts[index];
    int close = block_end(emitter->function, index, end);
    int body = index + 1;
    char *condition = emit_condition_operand(emitter, statement->expr_root);
    /* if a { if b { ... } } with no else on either is if a and b: Python
     * allows only 100 nested blocks, and the flat form reads better. */
    while(!has_else(emitter, close, end) && body < close &&
          emitter->function->stmts[body].kind == ZIR_STMT_IF &&
          !emitter->function->stmts[body].is_else &&
          block_end(emitter->function, body, close) == close - 1) {
        char *inner = emit_condition_operand(emitter, emitter->function->stmts[body].expr_root);
        char *joined = py_format("%s and %s", condition, inner);
        free(condition);
        free(inner);
        condition = joined;
        body++;
        close--;
    }
    if(body == index + 1) {
        char *bare = py_bare(condition);
        free(condition);
        condition = bare;
    }
    write_line(emitter, "%s %s:", is_elif ? "elif" : "if", condition);
    free(condition);
    emit_body(emitter, body, close);
    /* The closes of merged inner ifs come before the outer one. */
    close = block_end(emitter->function, index, end);
    if(close + 1 < end && emitter->function->stmts[close + 1].kind == ZIR_STMT_IF &&
       emitter->function->stmts[close + 1].is_else) {
        int next = close + 1;
        if(emitter->function->stmts[next].expr_root >= 0)
            close = emit_if(emitter, next, end, 1);
        else {
            close = block_end(emitter->function, next, end);
            write_line(emitter, "else:");
            emit_body(emitter, next + 1, close);
        }
    }
    return close;
}

static int loop_is_native(PyEmitter *emitter, int loop_id)
{
    for(int index = 0; index < emitter->native_loop_count; index++)
        if(emitter->native_loops[index] == loop_id)
            return 1;
    return 0;
}

static int innermost_loop(PyEmitter *emitter)
{
    return emitter->loop_count > 0 ? emitter->loops[emitter->loop_count - 1] : 0;
}

/* The far jumps a loop body makes: to loops outside it. */
static void loop_exits(PyEmitter *emitter, int open, int close, int loop_id,
                       int *breaks_parent, int *continues_parent, int *farther, int parent)
{
    const ZirFunction *function = emitter->function;
    *breaks_parent = *continues_parent = *farther = 0;
    for(int index = open + 1; index < close; index++) {
        const ZirStmt *statement = &function->stmts[index];
        int inside = 0;
        if((statement->kind != ZIR_STMT_BREAK && statement->kind != ZIR_STMT_CONTINUE) ||
           !statement->target_id || statement->target_id == loop_id)
            continue;
        for(int scan = open + 1; scan < close; scan++)
            if(function->stmts[scan].kind == ZIR_STMT_WHILE &&
               function->stmts[scan].loop_id == statement->target_id)
                inside = 1;
        if(inside)
            continue;
        if(statement->target_id == parent) {
            if(statement->kind == ZIR_STMT_BREAK)
                *breaks_parent = 1;
            else
                *continues_parent = 1;
        } else
            *farther = 1;
    }
}

static void emit_loop_body(PyEmitter *emitter, int loop_id, int begin, int end)
{
    emitter->loops[emitter->loop_count++] = loop_id;
    emit_body(emitter, begin, end);
    emitter->loop_count--;
}

/* After an inner loop, a jump it made to an outer loop goes on. */
static void emit_loop_exits(PyEmitter *emitter, int open, int close, int loop_id)
{
    int breaks_parent, continues_parent, farther;
    int parent = innermost_loop(emitter);
    if(emitter->loop_count == 0)
        return;
    loop_exits(emitter, open, close, loop_id, &breaks_parent, &continues_parent,
               &farther, parent);
    if(breaks_parent) {
        write_line(emitter, "if ziran_jump == -%d:", parent);
        emitter->indent++;
        write_line(emitter, "ziran_jump = 0");
        write_line(emitter, "break");
        emitter->indent--;
    }
    if(continues_parent) {
        write_line(emitter, "if ziran_jump == %d:", parent);
        emitter->indent++;
        write_line(emitter, "ziran_jump = 0");
        write_line(emitter, "continue");
        emitter->indent--;
    }
    if(farther) {
        write_line(emitter, "if ziran_jump:");
        emitter->indent++;
        write_line(emitter, "break");
        emitter->indent--;
    }
}

/* A lowered counting loop, { i: s64 = 0; while i < n { ...; i += 1 } },
 * is for i in range(0, n) in Python. The range steps, so the marked step
 * statements go. */
static int emit_native_for(PyEmitter *emitter, int open, int close)
{
    const ZirFunction *function = emitter->function;
    const ZirStmt *counter, *loop;
    const ZirExpr *condition;
    int loop_close, reverse = 0;
    char *start, *bound, *range;
    PyLocal *local;
    if(open + 2 >= close || emitter->native_loop_count >= 128)
        return 0;
    counter = &function->stmts[open + 1];
    loop = &function->stmts[open + 2];
    if(counter->kind != ZIR_STMT_DECL || counter->expr_root < 0 ||
       loop->kind != ZIR_STMT_WHILE || !loop->for_form || loop->is_parallel ||
       loop->expr_root < 0 || names_has(&emitter->boxed, counter->name))
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
    emitter->depth++;
    start = emit_value(emitter, counter->expr_root, counter->type);
    bound = emit_value(emitter, condition->right, counter->type);
    {
        char *bare_start = py_bare(start), *bare_bound = py_bare(bound);
        free(start);
        free(bound);
        start = bare_start;
        bound = bare_bound;
    }
    local = declare_local(emitter, counter->name);
    if(reverse) {
        long long value;
        char *stop = text_integer(bound, &value) ? py_format("%lld", value - 1) :
                                                   py_format("%s - 1", bound);
        range = py_format("range(%s, %s, -1)", start, stop);
        free(stop);
    } else if(!strcmp(condition->op, "<=")) {
        long long value;
        char *stop = text_integer(bound, &value) ? py_format("%lld", value + 1) :
                                                   py_format("%s + 1", bound);
        if(!strcmp(start, "0"))
            range = py_format("range(%s)", stop);
        else
            range = py_format("range(%s, %s)", start, stop);
        free(stop);
    } else if(!strcmp(start, "0"))
        range = py_format("range(%s)", bound);
    else
        range = py_format("range(%s, %s)", start, bound);
    write_line(emitter, "for %s in %s:", local->python, range);
    if(!reverse && (is_nonnegative_literal(start) ||
                    names_has(&emitter->counters, start)))
        names_add(&emitter->counters, local->python);
    free(start);
    free(bound);
    free(range);
    emitter->native_loops[emitter->native_loop_count++] = loop->loop_id;
    emit_loop_body(emitter, loop->loop_id, open + 3, loop_close);
    emitter->native_loop_count--;
    leave_scope(emitter);
    emit_loop_exits(emitter, open + 2, loop_close, loop->loop_id);
    return 1;
}

/* A place the assignment writes, with its index read once. */
static char *emit_assignment_place(PyEmitter *emitter, int index, int compound)
{
    const ZirExpr *expression = expression_at(emitter, index);
    if(compound && expression != NULL && expression->kind == ZIR_EXPR_INDEX &&
       expression_calls(emitter->function, expression->right)) {
        /* x[f()] += v evaluates f once. */
        char temporary[64];
        PyType base;
        char *object = emit_place(emitter, expression->left);
        char *value = emit_expression(emitter, expression->right);
        char *bare = py_bare(value);
        snprintf(temporary, sizeof(temporary), "ziran_index_%d", ++emitter->temporaries);
        write_line(emitter, "%s = %s", temporary, bare);
        free(value);
        free(bare);
        py_classify(emitter->module, base_type_of(emitter, expression), &base);
        char *result = base.kind == PY_ARRAY || base.kind == PY_STRING ?
            py_format("%s[_index(%s)]", object, temporary) :
            py_format("%s[%s]", object, temporary);
        free(object);
        return result;
    }
    return emit_place(emitter, index);
}

static int local_in_place(PyEmitter *emitter, const char *name)
{
    return names_has(&emitter->in_place, name);
}

static void emit_assignment(PyEmitter *emitter, const ZirStmt *statement)
{
    const ZirExpr *target = expression_at(emitter, statement->lhs_root);
    PyType info;
    char *place, *value;
    if(target == NULL) {
        Diagnostic(statement->span, "zir_py.statement", "assignment has no destination");
        exit(1);
    }
    py_classify(emitter->module, target->type, &info);
    if(strcmp(statement->assignment_op, "=") == 0) {
        place = emit_assignment_place(emitter, statement->lhs_root, 0);
        value = emit_value(emitter, statement->expr_root, target->type);
        if(py_aggregate(info.kind) &&
           (target->kind != ZIR_EXPR_IDENT || find_local(emitter, target->name) == NULL ||
            local_in_place(emitter, target->name))) {
            /* Overwrite the place itself: views and pointers see it. */
            char *source = emit_expression(emitter, statement->expr_root);
            py_assign_in_place(emitter, emitter->module, target->type, place, source);
            free(source);
        } else {
            char *bare = py_bare(value);
            write_line(emitter, "%s = %s", place, bare);
            free(bare);
        }
        free(place);
        free(value);
        return;
    }
    {
        char operation[4];
        snprintf(operation, sizeof(operation), "%s", statement->assignment_op);
        operation[strlen(operation) - 1] = '\0';
        place = emit_assignment_place(emitter, statement->lhs_root, 1);
        value = emit_value(emitter, statement->expr_root,
                           !strcmp(operation, "<<") || !strcmp(operation, ">>") ?
                           expression_at(emitter, statement->expr_root)->type : target->type);
        char *bare = py_bare(value);
        char *combined;
        if(info.kind == PY_INT) {
            if(!strcmp(operation, "+") || !strcmp(operation, "-") || !strcmp(operation, "*")) {
                char *sum = py_format("%s %s %s", place, operation, value);
                combined = py_wrap(sum, info.bits, info.is_signed);
                free(sum);
            } else if((!strcmp(operation, "/") || !strcmp(operation, "%")) && !info.is_signed) {
                write_line(emitter, "%s %s= %s", place, !strcmp(operation, "/") ? "//" : "%",
                           bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else if(!strcmp(operation, "/")) {
                char *quotient = py_format("_div(%s, %s)", place, bare);
                combined = info.is_signed ? py_wrap(quotient, info.bits, 1) :
                                            py_copy_string(quotient);
                free(quotient);
            } else if(!strcmp(operation, "%"))
                combined = py_format("_rem(%s, %s)", place, bare);
            else if(!strcmp(operation, "<<")) {
                char *shifted = py_format("_shl(%s, %s, %d)", place, bare, info.bits);
                combined = py_wrap(shifted, info.bits, info.is_signed);
                free(shifted);
            } else if(!strcmp(operation, ">>"))
                combined = py_format("_shr(%s, %s, %d)", place, bare, info.bits);
            else if(!strcmp(operation, "&") || !strcmp(operation, "|") ||
                    !strcmp(operation, "^")) {
                write_line(emitter, "%s %s= %s", place, operation, bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else {
                Diagnostic(statement->span, "zir_py.assignment",
                        "unsupported assignment operation in the Python target: %s",
                        statement->assignment_op);
                exit(1);
                combined = NULL;
            }
        } else if(info.kind == PY_FLOAT) {
            if(!strcmp(operation, "/"))
                combined = py_format("_fdiv(%s, %s)", place, bare);
            else if(!strcmp(operation, "%"))
                combined = py_format("_frem(%s, %s)", place, bare);
            else if(info.bits == 64) {
                write_line(emitter, "%s %s= %s", place, operation, bare);
                free(place);
                free(value);
                free(bare);
                return;
            } else
                combined = py_format("%s %s %s", place, operation, value);
            if(info.bits == 32) {
                char *rounded = py_format("_f32(%s)", combined);
                free(combined);
                combined = rounded;
            }
        } else if(info.kind == PY_BOOL) {
            write_line(emitter, "%s = %s %s %s", place, place,
                       !strcmp(operation, "^") ? "!=" : operation, value);
            free(place);
            free(value);
            free(bare);
            return;
        } else {
            Diagnostic(statement->span, "zir_py.assignment",
                    "unsupported assignment operation in the Python target: %s",
                    statement->assignment_op);
            exit(1);
            combined = NULL;
        }
        char *bare_combined = py_bare(combined);
        write_line(emitter, "%s = %s", place, bare_combined);
        free(bare_combined);
        free(combined);
        free(bare);
    }
    free(place);
    free(value);
}

static void emit_print(PyEmitter *emitter, const ZirExpr *expression)
{
    PrintPiece *pieces = py_allocate(PRINT_PIECES_MAX * sizeof(*pieces));
    unsigned char *bytes = py_allocate(ZIR_TEXT_MAX);
    const ZirFunction *function = emitter->function;
    int first = expression->first_child, count, argument = -1, arguments = 0;
    char *format = py_copy_string("");
    char *values = py_copy_string("");
    if(first < 0 || (count = PrintFormatPieces(function->exprs[first].text, pieces,
                                               PRINT_PIECES_MAX)) < 0)
        unsupported_expression(expression);
    argument = function->exprs[first].next_sibling;
    for(int piece = 0; piece < count; piece++) {
        size_t length;
        const unsigned char *text = bytes;
        char *literal, *joined;
        if(!pieces[piece].is_argument) {
            if(!DecodeStringLiteral(pieces[piece].literal, bytes, ZIR_TEXT_MAX, &length))
                unsupported_expression(expression);
        } else {
            const ZirExpr *value;
            PyType info;
            char *text_value, *bare, *spelled;
            const char *placeholder = "%s";
            if(argument < 0)
                break;
            value = &function->exprs[argument];
            if(value->kind == ZIR_EXPR_STRING &&
               DecodeStringLiteral(value->text, bytes, ZIR_TEXT_MAX, &length)) {
                argument = value->next_sibling;
                goto literal_text;
            }
            py_classify(emitter->module, value->type, &info);
            text_value = emit_expression(emitter, argument);
            bare = py_bare(text_value);
            if(info.kind == PY_INT) {
                placeholder = "%d";
                spelled = py_copy_string(bare);
            } else if(info.kind == PY_BOOL)
                spelled = py_format("b\"true\" if %s else b\"false\"", bare);
            else if(info.kind == PY_FLOAT)
                spelled = info.bits == 32 ? py_format("_float_text(%s, True)", bare) :
                                            py_format("_float_text(%s)", bare);
            else
                spelled = py_copy_string(bare);
            joined = py_format("%s%s", format, placeholder);
            free(format);
            format = joined;
            joined = py_format("%s%s%s", values, arguments ? ", " : "", spelled);
            free(values);
            values = joined;
            arguments++;
            free(text_value);
            free(bare);
            free(spelled);
            argument = value->next_sibling;
            continue;
        }
literal_text:
        {
            /* Percent signs in the text are doubled for % formatting. */
            unsigned char *escaped = py_allocate(length * 2 + 1);
            size_t used = 0;
            for(size_t index = 0; index < length; index++) {
                escaped[used++] = text[index];
                if(text[index] == '%')
                    escaped[used++] = '%';
            }
            literal = py_string_literal(escaped, used);
            free(escaped);
            /* Drop b" and the closing quote to join the text. */
            literal[strlen(literal) - 1] = '\0';
            joined = py_format("%s%s", format, literal + 2);
            free(format);
            format = joined;
            free(literal);
        }
    }
    if(arguments == 0) {
        /* Without values the text needs no % formatting. */
        char *plain = py_allocate(strlen(format) + 1);
        size_t used = 0;
        for(const char *c = format; *c; c++) {
            if(c[0] == '%' && c[1] == '%')
                c++;
            plain[used++] = *c;
        }
        plain[used] = '\0';
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\")", plain);
        free(plain);
    } else if(arguments == 1)
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\" %% %s)", format,
                   strchr(values, ' ') != NULL && strncmp(values, "_float_text", 11) ?
                       py_format("(%s)", values) : values);
    else
        write_line(emitter, "sys.stdout.buffer.write(b\"%s\" %% (%s))", format, values);
    free(format);
    free(values);
    free(pieces);
    free(bytes);
}

static void emit_sequence(PyEmitter *emitter, int begin, int end)
{
    for(int index = begin; index < end; index++) {
        const ZirStmt *statement = &emitter->function->stmts[index];
        if(statement->kind == ZIR_STMT_ASSIGN && statement->for_step &&
           loop_is_native(emitter, statement->for_step))
            continue;
        /* Comments carry source locations without changing generated code
         * or the emitter's statement/empty-block accounting. */
        char *source = py_string_literal((const unsigned char *)SpanPath(statement->span),
                                        strlen(SpanPath(statement->span)));
        for(int indent = 0; indent < emitter->indent; indent++) fputs("    ", emitter->output);
        fprintf(emitter->output, "# ziran-source (%s, %d)\n", source, statement->span.line);
        free(source);
        switch(statement->kind) {
        case ZIR_STMT_DECL: {
            PyType info;
            char *value;
            PyLocal *local;
            py_require_type(emitter->module, statement->span, statement->type, &info);
            /* The value is lowered before the name is bound, so an
             * initializer can read a shadowed outer binding. */
            if(statement->expr_root >= 0)
                value = emit_value(emitter, statement->expr_root, statement->type);
            else
                value = py_zero(emitter, emitter->module, statement->type);
            local = declare_local(emitter, statement->name);
            {
                char *bare = py_bare(value);
                if(local->boxed && !py_aggregate(info.kind))
                    write_line(emitter, "%s = [%s]", local->python, bare);
                else {
                    local->boxed = 0;
                    write_line(emitter, "%s = %s", local->python, bare);
                }
                free(bare);
            }
            free(value);
            break;
        }
        case ZIR_STMT_ASSIGN:
            emit_assignment(emitter, statement);
            break;
        case ZIR_STMT_EXPR:
        case ZIR_STMT_UNUSED: {
            const ZirExpr *expression = expression_at(emitter, statement->expr_root);
            char *value, *bare;
            if(expression == NULL)
                break;
            if(expression->kind == ZIR_EXPR_CALL && !strcmp(expression->name, "print")) {
                emit_print(emitter, expression);
                break;
            }
            value = emit_expression(emitter, statement->expr_root);
            bare = py_bare(value);
            write_line(emitter, "%s", bare);
            free(value);
            free(bare);
            break;
        }
        case ZIR_STMT_IF:
            index = emit_if(emitter, index, end, 0);
            break;
        case ZIR_STMT_WHILE: {
            int close = block_end(emitter->function, index, end);
            char *condition = emit_expression(emitter, statement->expr_root);
            char *bare = py_bare(condition);
            if(statement->is_parallel || statement->is_gpu)
                write_line(emitter, "# ziran: #parallel region runs serially");
            write_line(emitter, "while %s:", bare);
            free(condition);
            free(bare);
            emit_loop_body(emitter, statement->loop_id, index + 1, close);
            emit_loop_exits(emitter, index, close, statement->loop_id);
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close = block_end(emitter->function, index, end);
            if(emit_native_for(emitter, index, close)) {
                index = close;
                break;
            }
            emitter->depth++;
            emit_sequence(emitter, index + 1, close);
            leave_scope(emitter);
            index = close;
            break;
        }
        case ZIR_STMT_BLOCK_CLOSE:
            break;
        case ZIR_STMT_RETURN:
            if(statement->expr_root >= 0) {
                const ZirExpr *expression = expression_at(emitter, statement->expr_root);
                char *value;
                PyLocal *local = expression->kind == ZIR_EXPR_IDENT ?
                    find_local(emitter, expression->name) : NULL;
                /* A local dies here, so it needs no copy. */
                if(local != NULL && !local->boxed)
                    value = emit_expression(emitter, statement->expr_root);
                else
                    value = emit_value(emitter, statement->expr_root,
                                       emitter->function->return_type);
                if(local != NULL && !local->boxed && expression->type[0] &&
                   strcmp(expression->type, emitter->function->return_type) != 0) {
                    char *converted = py_convert(emitter, value, expression->type,
                                                 emitter->function->return_type);
                    free(value);
                    value = converted;
                }
                char *bare = py_bare(value);
                write_line(emitter, "return %s", bare);
                free(value);
                free(bare);
            } else
                write_line(emitter, "return");
            break;
        case ZIR_STMT_BREAK:
        case ZIR_STMT_CONTINUE: {
            int is_break = statement->kind == ZIR_STMT_BREAK;
            if(statement->target_id && statement->target_id != innermost_loop(emitter)) {
                write_line(emitter, "ziran_jump = %s%d", is_break ? "-" : "",
                           statement->target_id);
                write_line(emitter, "break");
            } else
                write_line(emitter, is_break ? "break" : "continue");
            break;
        }
        case ZIR_STMT_UNREACHABLE:
            write_line(emitter, "raise AssertionError(\"unreachable\")");
            break;
        default:
            Diagnostic(statement->span, "zir_py.statement",
                    "unsupported statement in the Python target: %s",
                    statement->text && statement->text[0] ? statement->text :
                        StmtKindName(statement->kind));
            exit(1);
        }
    }
}

/* ----- functions ----- */

const char *place_base(const ZirFunction *function, int index, int *through_member)
{
    *through_member = 0;
    while(index >= 0 && index < function->expr_count) {
        const ZirExpr *expression = &function->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression->name;
        if(expression->kind != ZIR_EXPR_MEMBER && expression->kind != ZIR_EXPR_INDEX &&
           expression->kind != ZIR_EXPR_SLICE)
            return NULL;
        *through_member = 1;
        index = expression->left;
    }
    return NULL;
}

/* Which locals live in a box, and which aggregates are overwritten in
 * place because a pointer or view may reach them. */
static void scan_function(PyEmitter *emitter, const ZirFunction *function)
{
    emitter->boxed.count = 0;
    emitter->in_place.count = 0;
    for(int index = 0; index < function->expr_count; index++) {
        const ZirExpr *expression = &function->exprs[index];
        const char *base;
        int through_member;
        if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&")) {
            base = place_base(function, expression->right, &through_member);
            if(base != NULL) {
                if(!through_member)
                    names_add(&emitter->boxed, base);
                names_add(&emitter->in_place, base);
            }
        } else if(expression->kind == ZIR_EXPR_SLICE ||
                  (expression->kind == ZIR_EXPR_MEMBER && !strcmp(expression->name, "data"))) {
            base = place_base(function, expression->left, &through_member);
            if(base != NULL)
                names_add(&emitter->in_place, base);
        }
    }
}

static void emit_global_declarations(PyEmitter *emitter, const ZirFunction *function)
{
    PyNames rebound = {0};
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        const ZirExpr *target;
        char name[ZIR_NAME_MAX * 2];
        PyType info;
        int declared = 0;
        if(statement->kind != ZIR_STMT_ASSIGN || statement->lhs_root < 0)
            continue;
        target = &function->exprs[statement->lhs_root];
        if(target->kind != ZIR_EXPR_IDENT)
            continue;
        for(int scan = 0; scan < function->stmt_count && !declared; scan++)
            declared = function->stmts[scan].kind == ZIR_STMT_DECL &&
                       !strcmp(function->stmts[scan].name, target->name);
        if(declared || find_local(emitter, target->name) != NULL)
            continue;
        if(!global_reference(emitter, target->name, name, sizeof(name), NULL))
            continue;
        if(py_classify(emitter->module, target->type, &info) && py_aggregate(info.kind))
            continue;
        names_add(&rebound, name);
    }
    if(rebound.count > 0) {
        char *line = py_copy_string("global ");
        for(int index = 0; index < rebound.count; index++) {
            char *joined = py_format("%s%s%s", line, index ? ", " : "", rebound.items[index]);
            free(line);
            line = joined;
        }
        write_line(emitter, "%s", line);
        free(line);
    }
    free(rebound.items);
}

static int function_uses_jump(const ZirFunction *function)
{
    for(int index = 0; index < function->stmt_count; index++) {
        const ZirStmt *statement = &function->stmts[index];
        if((statement->kind == ZIR_STMT_BREAK || statement->kind == ZIR_STMT_CONTINUE) &&
           statement->target_id) {
            /* A jump to the loop right around it needs no flag. */
            int depth = 0;
            for(int scan = index - 1; scan >= 0; scan--) {
                ZirStmtKind kind = function->stmts[scan].kind;
                if(kind == ZIR_STMT_BLOCK_CLOSE)
                    depth++;
                else if(kind == ZIR_STMT_IF || kind == ZIR_STMT_BLOCK_OPEN ||
                        kind == ZIR_STMT_WHILE) {
                    if(depth > 0)
                        depth--;
                    else if(kind == ZIR_STMT_WHILE) {
                        if(function->stmts[scan].loop_id != statement->target_id)
                            return 1;
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

void lower_function(PyEmitter *emitter, const ZirModule *module,
                           const ZirFunction *function)
{
    char symbol[ZIR_NAME_MAX * 2];
    char (*names)[ZIR_NAME_MAX];
    char (*types)[ZIR_NAME_MAX];
    char *parameters;
    int count;
    long before;
    if(function->is_extern || function->is_template)
        return;
    names = py_allocate(64 * ZIR_NAME_MAX);
    types = py_allocate(64 * ZIR_NAME_MAX);
    parameters = py_copy_string("");
    emitter->module = module;
    emitter->function = function;
    emitter->local_count = 0;
    emitter->depth = 0;
    emitter->loop_count = 0;
    emitter->native_loop_count = 0;
    emitter->temporaries = 0;
    emitter->counters.count = 0;
    scan_function(emitter, function);
    function_symbol(emitter, module, function, symbol, sizeof(symbol));
    count = function_parameter_names(function, names, types, 64);
    for(int index = 0; index < count; index++) {
        PyType info;
        PyLocal *local;
        char *joined;
        py_require_type(module, function->span, types[index], &info);
        local = declare_local(emitter, names[index]);
        joined = py_format("%s%s%s", parameters, index ? ", " : "", local->python);
        free(parameters);
        parameters = joined;
    }
    fprintf(emitter->output, "def %s(%s):\n", symbol, parameters);
    free(parameters);
    emitter->indent = 1;
    before = emitter->lines;
    emitter->depth = 1;
    emit_global_declarations(emitter, function);
    for(int index = 0; index < emitter->local_count; index++) {
        PyLocal *local = &emitter->locals[index];
        PyType info;
        py_classify(module, types[index], &info);
        if(local->boxed && !py_aggregate(info.kind))
            write_line(emitter, "%s = [%s]", local->python, local->python);
        else
            local->boxed = 0;
    }
    if(function_uses_jump(function))
        write_line(emitter, "ziran_jump = 0");
    emit_sequence(emitter, 0, function->stmt_count);
    if(emitter->lines == before)
        write_line(emitter, "pass");
    emitter->indent = 0;
    fputs("\n\n", emitter->output);
    free(names);
    free(types);
}

/* ----- types ----- */
