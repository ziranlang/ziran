#include "zir_parse_internal.h"

/* Text of the qualified enum member holding `value`, so a bound enum value
 * keeps its enum type after textual expansion. */
static int
enum_member_text(const ZirModule *module, const char *type, long value,
                 char *out, size_t size)
{
    const ZirType *enumeration = type[0] ? FindType(module, type, NULL) : NULL;
    const char *cursor;
    if(enumeration == NULL || !enumeration->is_enum) return 0;
    cursor = enumeration->body;
    while(*cursor) {
        char member[ZIR_NAME_MAX];
        size_t length = 0;
        int64_t member_value;
        while(*cursor == ',' || isspace((unsigned char)*cursor)) cursor++;
        while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
              length + 1 < sizeof(member))
            member[length++] = *cursor++;
        member[length] = '\0';
        while(*cursor && *cursor != ',' && *cursor != '\n') cursor++;
        if(length && EnumMemberValue(enumeration, member, &member_value) &&
           member_value == value) {
            snprintf(out, size, "%s.%s", enumeration->name, member);
            return 1;
        }
    }
    return 0;
}

static int evaluate_typed_function(const ZirModule *module, const char *name, ZirSourceSpan call_span, CompileValue *arguments, char argument_names[][ZIR_NAME_MAX], int argument_count, int depth, int *fuel, CompileValue *result);
/* Buffers evaluate_typed_node keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedNodeBuffers {
    unsigned char bytes[ZIR_TEXT_MAX];
    CompileValue args[16];
    char names[16][ZIR_NAME_MAX];
} EvaluateTypedNodeBuffers;

/* Enum members fold to typed integers only while a law is evaluated; other
 * callers rely on member expressions staying symbolic. */
static _Thread_local int law_enum_members;

int evaluate_typed_node(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result);

static int
evaluate_typed_node_with_buffers(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result, EvaluateTypedNodeBuffers *buffers)
{
    if(index < 0 || index >= probe->expr_count || depth > 64 ||
       --*fuel < 0) return 0;
    const ZirExpr *expression = &probe->exprs[index];
    CompileValue left = {0}, right = {0};
    char *end;
    long integer;
    double real;
    int truth;
    switch(expression->kind) {
    case ZIR_EXPR_INT:
        errno = 0;
        integer = strtol(expression->text, &end, 0);
        if(errno == ERANGE || end == expression->text || *end) return 0;
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        return compile_value_literal(result);
    case ZIR_EXPR_FLOAT:
        errno = 0;
        real = strtod(expression->text, &end);
        if(errno == ERANGE || end == expression->text || *end ||
           !isfinite(real)) return 0;
        result->kind = COMPILE_REAL;
        result->real = real;
        return compile_value_literal(result);
    case ZIR_EXPR_STRING:
        result->kind = COMPILE_STRING;
        copy_text(result->literal, sizeof(result->literal),
                  expression->text);
        return 1;
    case ZIR_EXPR_COMPOUND:
        return compile_compound_value(probe, expression, module, path,
                                      depth, fuel, result);
    case ZIR_EXPR_COMPILE_TIME:
        result->kind = COMPILE_INTEGER;
        result->integer = 1;
        return compile_value_literal(result);
    case ZIR_EXPR_IDENT:
        if(!strcmp(expression->name, "true") ||
           !strcmp(expression->name, "false")) {
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(expression->name, "true");
            return compile_value_literal(result);
        }
        return evaluate_imported_typed_define(module, path,
                    expression->name, depth + 1, fuel, result);
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(!TypeLayout(module, expression->name, &size, &alignment) ||
           size > LONG_MAX) return 0;
        result->kind = COMPILE_INTEGER;
        result->integer = (long)size;
        return compile_value_literal(result);
    }
    case ZIR_EXPR_UNARY: {
        const ZirExpr *operand = &probe->exprs[expression->right];
        if(!strcmp(expression->op, "-") && operand->kind == ZIR_EXPR_INT &&
           !strcmp(operand->text, "9223372036854775808")) {
            result->kind = COMPILE_INTEGER;
            result->integer = LONG_MIN;
            return compile_value_literal(result);
        }
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(!strcmp(expression->op, "!")) {
            if(!compile_truth(&right, &truth)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = !truth;
        } else if(!strcmp(expression->op, "+")) {
            *result = right;
        } else if(!strcmp(expression->op, "-")) {
            *result = right;
            if(right.kind == COMPILE_REAL) result->real = -right.real;
            else if(right.kind == COMPILE_INTEGER && right.integer != LONG_MIN)
                result->integer = -right.integer;
            else return 0;
        } else if(!strcmp(expression->op, "~") &&
                  right.kind == COMPILE_INTEGER) {
            result->kind = COMPILE_INTEGER;
            result->integer = ~right.integer;
        } else return 0;
        return compile_value_literal(result);
    }
    case ZIR_EXPR_BINARY: {
        const char *op = expression->op;
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left)) return 0;
        if(!strcmp(op, "&&") || !strcmp(op, "||")) {
            if(!compile_truth(&left, &truth)) return 0;
            if((!strcmp(op, "&&") && !truth) ||
               (!strcmp(op, "||") && truth)) {
                result->kind = COMPILE_INTEGER;
                result->integer = truth;
                return compile_value_literal(result);
            }
        }
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(!strcmp(op, "==") || !strcmp(op, "!=")) {
            int equal;
            if(!compile_values_equal(&left, &right, &equal)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(op, "==") ? equal : !equal;
            return compile_value_literal(result);
        }
        if(!strcmp(op, "&&") || !strcmp(op, "||")) {
            if(!compile_truth(&right, &truth)) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = truth;
            return compile_value_literal(result);
        }
        if((left.kind != COMPILE_INTEGER && left.kind != COMPILE_REAL) ||
           (right.kind != COMPILE_INTEGER && right.kind != COMPILE_REAL))
            return 0;
        double a = left.kind == COMPILE_REAL ? left.real :
                   (double)left.integer;
        double b = right.kind == COMPILE_REAL ? right.real :
                   (double)right.integer;
        if(!strcmp(op, "<") || !strcmp(op, "<=") ||
           !strcmp(op, ">") || !strcmp(op, ">=")) {
            result->kind = COMPILE_INTEGER;
            result->integer = !strcmp(op, "<") ? a < b :
                              !strcmp(op, "<=") ? a <= b :
                              !strcmp(op, ">") ? a > b : a >= b;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER) {
            long x = left.integer, y = right.integer, folded;
            if(!strcmp(op, "+")) {
                if(!checked_add_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "-")) {
                if(!checked_sub_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "*")) {
                if(!checked_mul_long(x, y, &folded)) return 0;
            } else if(!strcmp(op, "/") && y != 0)
                folded = x == LONG_MIN && y == -1 ? x : x / y;
            else if(!strcmp(op, "%") && y != 0)
                folded = x == LONG_MIN && y == -1 ? 0 : x % y;
            else if(!strcmp(op, "&")) folded = x & y;
            else if(!strcmp(op, "|")) folded = x | y;
            else if(!strcmp(op, "^")) folded = x ^ y;
            else if(!strcmp(op, "<<") && y >= 0 &&
                    y < (long)(sizeof(long) * CHAR_BIT) && x >= 0 &&
                    x <= (LONG_MAX >> y)) folded = x << y;
            else if(!strcmp(op, ">>") && y >= 0 &&
                    y < (long)(sizeof(long) * CHAR_BIT)) folded = x >> y;
            else return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = folded;
            if(wrap_compile_integer(left.type, &result->integer))
                copy_text(result->type, sizeof(result->type), left.type);
            return compile_value_literal(result);
        }
        result->kind = COMPILE_REAL;
        if(!strcmp(op, "+")) result->real = a + b;
        else if(!strcmp(op, "-")) result->real = a - b;
        else if(!strcmp(op, "*")) result->real = a * b;
        else if(!strcmp(op, "/") && b != 0.0) result->real = a / b;
        else return 0;
        return isfinite(result->real) && compile_value_literal(result);
    }
    case ZIR_EXPR_MEMBER:
        if(expression->left >= 0 &&
           probe->exprs[expression->left].kind == ZIR_EXPR_IDENT) {
            char qualified[ZIR_NAME_MAX];
            if(snprintf(qualified, sizeof(qualified), "%s.%s",
                        probe->exprs[expression->left].name,
                        expression->name) < (int)sizeof(qualified) &&
               evaluate_imported_typed_define(module, path, qualified,
                                              depth + 1, fuel, result))
                return 1;
{
            const ZirType *enumeration = FindType(module,
                probe->exprs[expression->left].name, NULL);
            int64_t member_value;
            if(law_enum_members && enumeration != NULL && enumeration->is_enum &&
               EnumMemberValue(enumeration, expression->name,
                               &member_value)) {
                result->kind = COMPILE_INTEGER;
                result->integer = (long)member_value;
                copy_text(result->type, sizeof(result->type),
                          enumeration->name);
                return compile_value_literal(result);
            }
}
        }
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left)) return 0;
        if(left.kind == COMPILE_STRING &&
           !strcmp(expression->name, "count")) {
            size_t length;
            if(!DecodeStringLiteral(left.literal, buffers->bytes, sizeof(buffers->bytes),
                                     &length) || length > LONG_MAX) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = (long)length;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_COMPOUND) {
            int capacity = 0;
            if(ArrayElementType(left.type, NULL, 0, &capacity) &&
               !strcmp(expression->name, "count")) {
                result->kind = COMPILE_INTEGER;
                result->integer = capacity;
                return compile_value_literal(result);
            }
            return compile_compound_member(&left, expression->name,
                                           module, path, depth, fuel, result);
        }
        return 0;
    case ZIR_EXPR_INDEX:
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left) ||
           !evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right) ||
           left.kind != COMPILE_COMPOUND ||
           right.kind != COMPILE_INTEGER) return 0;
        return compile_compound_index(&left, right.integer, module, path,
                                      depth, fuel, result);
    case ZIR_EXPR_CAST:
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, result)) return 0;
        if(result->kind == COMPILE_INTEGER &&
           wrap_compile_integer(expression->name, &result->integer))
            copy_text(result->type, sizeof(result->type), expression->name);
        if(eval_integer_type(expression->name, 0) &&
           result->kind == COMPILE_REAL) {
            if(result->real < (double)LONG_MIN ||
               result->real >= -(double)LONG_MIN) return 0;
            result->integer = (long)result->real;
            result->kind = COMPILE_INTEGER;
        }
        return compile_type_value(expression->name, result);
    case ZIR_EXPR_CONDITIONAL:
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left) ||
           !compile_truth(&left, &truth)) return 0;
        return evaluate_typed_node(probe, truth ? expression->right :
                                   expression->third, module, path,
                                   depth + 1, fuel, result);
    case ZIR_EXPR_CALL: {
        char name[ZIR_NAME_MAX];
        memset(buffers->args, 0, sizeof(buffers->args));
        memset(buffers->names, 0, sizeof(buffers->names));
        int count = 0;
        copy_text(name, sizeof(name), expression->name);
        if(!name[0] && expression->left >= 0) {
            const ZirExpr *callee = &probe->exprs[expression->left];
            if(callee->kind != ZIR_EXPR_MEMBER || callee->left < 0 ||
               probe->exprs[callee->left].kind != ZIR_EXPR_IDENT ||
               snprintf(name, sizeof(name), "%s.%s",
                        probe->exprs[callee->left].name, callee->name) >=
                   (int)sizeof(name)) return 0;
        }
        for(int child = expression->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            if(count == 16 ||
               !evaluate_typed_node(probe, child, module, path,
                                    depth + 1, fuel, &buffers->args[count])) return 0;
            copy_text(buffers->names[count], sizeof(buffers->names[count]),
                      probe->exprs[child].argument_name);
            count++;
        }
        ZirSourceSpan call_span = expression->left >= 0 ?
            probe->exprs[expression->left].span : expression->span;
        return evaluate_typed_function(module, name, call_span, buffers->args, buffers->names,
                                       count, depth + 1, fuel, result);
    }
    default:
        return 0;
    }
}

int
evaluate_typed_node(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result)
{
    static _Thread_local EvaluateTypedNodeBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateTypedNodeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = evaluate_typed_node_with_buffers(probe, index, module, path, depth, fuel, result, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers evaluate_typed_expression keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedExpressionBuffers {
    ZirFunction probe;
    char input[ZIR_TEXT_MAX];
    char expanded[ZIR_TEXT_MAX];
} EvaluateTypedExpressionBuffers;

int evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result);

static int
evaluate_typed_expression_with_buffers(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result, EvaluateTypedExpressionBuffers *buffers)
{
    ZirConsts empty = {0};
    memset(&buffers->probe, 0, sizeof(buffers->probe));
    long integer;
    int root, ok;
    const char *path = SpanPath(span);
    if(depth > 32 || source == NULL || --*fuel < 0) return 0;
    copy_text(buffers->input, sizeof(buffers->input), source);
    trim_in_place(buffers->input);
    size_t length = strlen(buffers->input);
    if(length && buffers->input[length - 1] == ';') {
        buffers->input[length - 1] = '\0';
        trim_in_place(buffers->input);
    }
    if(!buffers->input[0]) return 0;
    if(names == NULL) names = &empty;
    expand_compile_expr(buffers->expanded, sizeof(buffers->expanded), names, buffers->input, path);
    if(eval_const_condition_with_fuel(buffers->expanded, &integer, module, names,
                                     path, span.line, depth, fuel)) {
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        return compile_value_literal(result);
    }
    root = ParseExpr(&buffers->probe, module, buffers->expanded, span);
    ok = root >= 0;
    if(depth == 0)
        for(int i = 0; i < buffers->probe.expr_count; i++)
            if(buffers->probe.exprs[i].kind == ZIR_EXPR_COMPILE_TIME)
                ok = 0;
    if(ok)
        ok = evaluate_typed_node(&buffers->probe, root, module, path,
                                 depth + 1, fuel, result);
    free(buffers->probe.exprs);
    return ok;
}

int
evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result)
{
    static _Thread_local EvaluateTypedExpressionBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateTypedExpressionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = evaluate_typed_expression_with_buffers(module, names, source, span, depth, fuel, result, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
typed_body_expression(TypedBody *body, const char *source,
                      CompileValue *value)
{
    char lowered[ZIR_TEXT_MAX];
    copy_text(lowered, sizeof(lowered), source);
    lower_procedure_name_expression(lowered, sizeof(lowered), body->fn);
    return evaluate_typed_expression(body->module, &body->names, lowered,
                                     body->current_span, body->depth + 1,
                                     body->fuel, value);
}
/* Buffers typed_body_condition keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct TypedBodyConditionBuffers {
    char condition[ZIR_TEXT_MAX];
} TypedBodyConditionBuffers;

static int typed_body_condition(TypedBody *body, const char *header,
                     const char *word, int *truth);

static int
typed_body_condition_with_buffers(TypedBody *body, const char *header,
                     const char *word, int *truth, TypedBodyConditionBuffers *buffers)
{
    const char *source = skip_ws(header + strlen(word));
    copy_text(buffers->condition, sizeof(buffers->condition), source);
    trim_in_place(buffers->condition);
    size_t length = strlen(buffers->condition);
    if(!length || buffers->condition[length - 1] != '{') return 0;
    buffers->condition[length - 1] = '\0';
    trim_in_place(buffers->condition);
    length = strlen(buffers->condition);
    if(length >= 4 && !strcmp(buffers->condition + length - 4, "then") &&
       (length == 4 || isspace((unsigned char)buffers->condition[length - 5]))) {
        buffers->condition[length - 4] = '\0';
        trim_in_place(buffers->condition);
    }
    CompileValue value = {0};
    return typed_body_expression(body, buffers->condition, &value) &&
           compile_truth(&value, truth);
}

static int
typed_body_condition(TypedBody *body, const char *header,
                     const char *word, int *truth)
{
    static _Thread_local TypedBodyConditionBuffers *spares[16];
    static _Thread_local int spare_count;
    TypedBodyConditionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = typed_body_condition_with_buffers(body, header, word, truth, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static void
typed_leave_scope(TypedBody *body, int saved)
{
    int removed = body->local_count - saved;
    if(removed <= 0) return;
    memmove(body->names.items, body->names.items + removed,
            (size_t)(body->names.count - removed) *
                sizeof(*body->names.items));
    body->names.count -= removed;
    body->local_count = saved;
}

static int
typed_add_local(TypedBody *body, const char *name, const char *type,
                const CompileValue *value)
{
    if(!is_identifier_text(name) ||
       body->names.count >= body->capacity) return 0;
    memmove(body->names.items + 1, body->names.items,
            (size_t)body->names.count * sizeof(*body->names.items));
    ZirConst *local = &body->names.items[0];
    memset(local, 0, sizeof(*local));
    copy_text(local->name, sizeof(local->name), name);
    copy_text(local->type, sizeof(local->type), type);
    copy_text(local->path, sizeof(local->path), SpanPath(body->fn->span));
    if(value != NULL)
        copy_text(local->expr, sizeof(local->expr), value->literal);
    body->names.count++;
    body->local_count++;
    return 1;
}

static int
typed_local_index(TypedBody *body, const char *name)
{
    for(int i = 0; i < body->local_count; i++)
        if(!strcmp(body->names.items[i].name, name)) return i;
    return -1;
}
/* Zero literal for a local `name: [N]integer`, e.g. `s32.[0, 0, 0]`. */
static int
zero_array_literal(const char *type, char *out, size_t size)
{
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    size_t used;
    if(!ArrayElementType(type, element, sizeof(element), &capacity) ||
       capacity < 1 || capacity > 256 || !eval_integer_type(element, 0))
        return 0;
    used = (size_t)snprintf(out, size, "%s.[", element);
    for(int i = 0; i < capacity && used < size; i++)
        used += (size_t)snprintf(out + used, size - used, "%s0",
                                 i ? ", " : "");
    if(used + 2 > size) return 0;
    out[used] = ']';
    out[used + 1] = '\0';
    return 1;
}

/* Replace element `index` of a canonical array literal `T.[a, b, c]`. */
static int
store_array_element(const char *literal, long index, const char *replacement,
                    char *out, size_t size)
{
    const char *open = strstr(literal, ".[");
    const char *cursor;
    size_t used;
    long ordinal = 0;
    int depth = 0;
    if(open == NULL || index < 0) return 0;
    cursor = open + 2;
    used = (size_t)(cursor - literal);
    if(used >= size) return 0;
    memcpy(out, literal, used);
    for(;;) {
        const char *start = cursor;
        while(*cursor && !(depth == 0 && (*cursor == ',' || *cursor == ']'))) {
            if(*cursor == '[' || *cursor == '{' || *cursor == '(') depth++;
            if(*cursor == ']' || *cursor == '}' || *cursor == ')') depth--;
            cursor++;
        }
        if(*cursor == '\0') return 0;
        if(ordinal == index) {
            start = skip_ws(replacement);
            size_t length = strlen(start);
            if(used + length + 2 >= size) return 0;
            memcpy(out + used, start, length);
            used += length;
        } else {
            size_t length = (size_t)(cursor - start);
            if(ordinal > 0 && *start == ' ') { start++; length--; }
            if(used + length + 2 >= size) return 0;
            memcpy(out + used, start, length);
            used += length;
        }
        if(*cursor == ']') {
            if(ordinal < index) return 0;
            out[used++] = ']';
            out[used] = '\0';
            return 1;
        }
        out[used++] = ',';
        out[used++] = ' ';
        cursor++;
        ordinal++;
    }
}

/* Buffers typed_body_statements keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct TypedBodyStatementsBuffers {
    char text[ZIR_TEXT_MAX];
    char combined[ZIR_TEXT_MAX];
    char control[ZIR_TEXT_MAX];
} TypedBodyStatementsBuffers;

static int typed_body_statements(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result);

static int
typed_body_statements_with_buffers(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result, TypedBodyStatementsBuffers *buffers)
{
    int saved = body->local_count;
    for(int i = start; i < stop && !*flow; i++) {
        const ZirStmt *statement = &body->fn->stmts[i];
        const char *source = skip_ws(statement->text);
        body->current_span = statement->span;
        if(--*body->fuel < 0) goto failed;
        if(statement->kind == ZIR_STMT_IF) {
            int taken = 0;
            for(;;) {
                const ZirStmt *arm = &body->fn->stmts[i];
                const char *header = skip_ws(arm->text);
                body->current_span = arm->span;
                int close = eval_close(body->fn, i, stop);
                int condition = 1;
                if(close < 0) goto failed;
                if(starts_word(header, "else if")) {
                    if(!taken &&
                       !typed_body_condition(body, header, "else if",
                                             &condition)) goto failed;
                } else if(starts_word(header, "if")) {
                    if(!taken &&
                       !typed_body_condition(body, header, "if",
                                             &condition)) goto failed;
                } else if(!starts_word(header, "else")) goto failed;
                if(!taken && condition) {
                    if(!typed_body_statements(body, i + 1, close,
                                              flow, result)) goto failed;
                    taken = 1;
                }
                i = close;
                if(i + 1 >= stop ||
                   body->fn->stmts[i + 1].kind != ZIR_STMT_IF ||
                   !starts_word(skip_ws(body->fn->stmts[i + 1].text),
                                "else")) break;
                i++;
            }
            continue;
        }
        if(statement->kind == ZIR_STMT_WHILE) {
            int close = eval_close(body->fn, i, stop);
            if(close < 0) goto failed;
            for(;;) {
                int condition = 0;
                body->current_span = statement->span;
                if(--*body->fuel < 0 ||
                   !typed_body_condition(body, source, "while",
                                         &condition)) goto failed;
                if(!condition) break;
                if(!typed_body_statements(body, i + 1, close,
                                          flow, result)) goto failed;
                if(*flow == 2) { *flow = 0; break; }
                if(*flow == 3) *flow = 0;
                if(*flow == 1) break;
            }
            i = close;
            continue;
        }
        if(statement->kind == ZIR_STMT_BLOCK_OPEN) {
            int close = eval_close(body->fn, i, stop);
            if(close < 0 ||
               !typed_body_statements(body, i + 1, close,
                                      flow, result)) goto failed;
            i = close;
            continue;
        }
        if(statement->kind == ZIR_STMT_RETURN) {
            if(!typed_body_expression(body, skip_ws(source + 6),
                                      result)) goto failed;
            *flow = 1;
            break;
        }
        if(statement->kind == ZIR_STMT_DECL) {
            copy_text(buffers->text, sizeof(buffers->text), source);
            char *colon = strchr(buffers->text, ':');
            if(colon == NULL) goto failed;
            *colon++ = '\0';
            trim_in_place(buffers->text);
            char *equals = strchr(colon, '=');
            if(equals != NULL) *equals++ = '\0';
            trim_in_place(colon);
            size_t length = strlen(colon);
            if(length && colon[length - 1] == ';') {
                colon[length - 1] = '\0';
                trim_in_place(colon);
            }
            CompileValue value = {0};
            if(equals != NULL) {
                if(!typed_body_expression(body, equals, &value)) goto failed;
            }
            const char *type = colon[0] ? colon :
                value.kind == COMPILE_REAL ? "float64" :
                value.kind == COMPILE_STRING ? "string" :
                value.kind == COMPILE_COMPOUND ? value.type : "s64";
            if(equals != NULL && !compile_type_value(type, &value))
                goto failed;
            if(equals == NULL &&
               zero_array_literal(type, buffers->combined,
                                  sizeof(buffers->combined))) {
                value.kind = COMPILE_COMPOUND;
                copy_text(value.type, sizeof(value.type), type);
                copy_text(value.literal, sizeof(value.literal),
                          buffers->combined);
                equals = (char *)"";
            }
            if(!typed_add_local(body, buffers->text, type,
                                equals != NULL ? &value : NULL)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_ASSIGN) {
            copy_text(buffers->text, sizeof(buffers->text), source);
            char *equals = strchr(buffers->text, '=');
            if(equals == NULL) goto failed;
            char op = equals > buffers->text ? equals[-1] : '\0';
            if(op && strchr("+-*/%", op) != NULL) equals[-1] = '\0';
            else op = '\0';
            *equals++ = '\0';
            trim_in_place(buffers->text);
            {
                char *bracket = strchr(buffers->text, '[');
                if(bracket != NULL && !op) {
                    /* `name[index] = value` on an array local. */
                    char *close = strrchr(bracket, ']');
                    CompileValue at = {0}, element = {0};
                    char element_type[ZIR_NAME_MAX];
                    int capacity = 0, target;
                    if(close == NULL || close[1] != '\0') goto failed;
                    *bracket++ = '\0';
                    *close = '\0';
                    trim_in_place(buffers->text);
                    target = typed_local_index(body, buffers->text);
                    if(target < 0 ||
                       !ArrayElementType(body->names.items[target].type,
                                         element_type, sizeof(element_type),
                                         &capacity) ||
                       !typed_body_expression(body, bracket, &at) ||
                       at.kind != COMPILE_INTEGER || at.integer < 0 ||
                       at.integer >= capacity ||
                       !typed_body_expression(body, equals, &element) ||
                       !compile_type_value(element_type, &element) ||
                       !store_array_element(body->names.items[target].expr,
                                            at.integer, element.literal,
                                            buffers->combined,
                                            sizeof(buffers->combined)))
                        goto failed;
                    copy_text(body->names.items[target].expr,
                              sizeof(body->names.items[target].expr),
                              buffers->combined);
                    continue;
                }
            }
            int local = typed_local_index(body, buffers->text);
            if(local < 0) goto failed;
            CompileValue value = {0};
            if(op) {
                int written = snprintf(buffers->combined, sizeof(buffers->combined),
                    "(%s) %c (%s)", body->names.items[local].expr, op,
                    equals);
                if(written < 0 || (size_t)written >= sizeof(buffers->combined) ||
                   !typed_body_expression(body, buffers->combined, &value)) goto failed;
            } else if(!typed_body_expression(body, equals, &value)) goto failed;
            if(!compile_type_value(body->names.items[local].type,
                                   &value)) goto failed;
            copy_text(body->names.items[local].expr,
                      sizeof(body->names.items[local].expr), value.literal);
            continue;
        }
        if(statement->kind == ZIR_STMT_EXPR) {
            CompileValue ignored = {0};
            if(!typed_body_expression(body, source, &ignored)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_BREAK ||
           statement->kind == ZIR_STMT_CONTINUE) {
            copy_text(buffers->control, sizeof(buffers->control), source);
            trim_in_place(buffers->control);
            size_t length = strlen(buffers->control);
            if(length && buffers->control[length - 1] == ';')
                buffers->control[length - 1] = '\0';
            if(strcmp(buffers->control, statement->kind == ZIR_STMT_BREAK ?
                       "break" : "continue") != 0) goto failed;
            *flow = statement->kind == ZIR_STMT_BREAK ? 2 : 3;
            break;
        }
        goto failed;
    }
    typed_leave_scope(body, saved);
    return 1;
failed:
    typed_leave_scope(body, saved);
    return 0;
}

static int
typed_body_statements(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result)
{
    static _Thread_local TypedBodyStatementsBuffers *spares[16];
    static _Thread_local int spare_count;
    TypedBodyStatementsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = typed_body_statements_with_buffers(body, start, stop, flow, result, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers evaluate_typed_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedFunctionBuffers {
    char parameters[16][ZIR_TEXT_MAX];
    char defaults[16][ZIR_TEXT_MAX];
    CompileValue ordered[16];
} EvaluateTypedFunctionBuffers;

static int evaluate_typed_function(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result);

static int
evaluate_typed_function_with_buffers(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result, EvaluateTypedFunctionBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    memset(buffers->defaults, 0, sizeof(buffers->defaults));
    memset(buffers->ordered, 0, sizeof(buffers->ordered));
    unsigned used = 0;
    TypedBody body = {0};
    int expected, flow = 0, ok = 0;
    if(depth >= 32 || module == NULL || !name[0] ||
       ResolveFunctionAt(module, name, SpanPath(call_span), &owner, &fn) != 1 ||
       fn == NULL || fn->is_extern || fn->is_template ||
       fn->stmt_count == 0) return 0;
    expected = *skip_ws(fn->args) ?
        split_top_level(fn->args, buffers->parameters[0], 16,
                        sizeof(buffers->parameters[0])) : 0;
    if(expected < 0 || argument_count > expected) return 0;
    if(fn->default_args[0] &&
       split_top_level(fn->default_args, buffers->defaults[0], 16,
                       sizeof(buffers->defaults[0])) != expected) return 0;
    for(int argument = 0; argument < argument_count; argument++) {
        int position = -1;
        if(argument_names[argument][0]) {
            size_t length = strlen(argument_names[argument]);
            for(int i = 0; i < expected; i++) {
                const char *start = skip_ws(buffers->parameters[i]);
                const char *colon = strchr(start, ':');
                if(colon == NULL) continue;
                const char *end = colon;
                while(end > start && isspace((unsigned char)end[-1])) end--;
                if((size_t)(end - start) == length &&
                   !strncmp(start, argument_names[argument], length)) {
                    position = i;
                    break;
                }
            }
        } else {
            for(int i = 0; i < expected; i++)
                if(!(used & (1u << i))) { position = i; break; }
        }
        if(position < 0 || (used & (1u << position))) return 0;
        used |= 1u << position;
        buffers->ordered[position] = arguments[argument];
    }
    body.capacity = expected + owner->define_count + fn->stmt_count + 1;
    body.names.items = calloc((size_t)body.capacity,
                              sizeof(*body.names.items));
    if(body.names.items == NULL) die("out of memory evaluating #run procedure");
    body.module = owner;
    body.fn = fn;
    body.depth = depth;
    body.fuel = fuel;
    for(int i = 0; i < expected; i++) {
        char *part = trim(buffers->parameters[i]);
        char *colon = strchr(part, ':');
        if(colon == NULL) goto done;
        *colon++ = '\0';
        trim_in_place(part);
        char *type = trim(colon);
        char *default_value = top_level_assignment(buffers->defaults[i]);
        if(default_value != NULL) default_value = trim(default_value + 1);
        char *embedded_default = strchr(type, '=');
        if(embedded_default != NULL) {
            *embedded_default++ = '\0';
            trim_in_place(type);
            if(default_value == NULL) default_value = trim(embedded_default);
        }
        if(!type[0] && default_value != NULL &&
           !strcmp(default_value, "#caller_location"))
            type = "Source_Code_Location";
        if(!(used & (1u << i))) {
            if(default_value == NULL) goto done;
            if(!strcmp(default_value, "#caller_location")) {
                buffers->ordered[i].kind = COMPILE_COMPOUND;
                copy_text(buffers->ordered[i].type, sizeof(buffers->ordered[i].type),
                          "Source_Code_Location");
                buffers->ordered[i].type_owner = owner;
                if(!CallerLocationLiteral(module, call_span,
                                          buffers->ordered[i].literal,
                                          sizeof(buffers->ordered[i].literal))) goto done;
            } else if(!evaluate_typed_expression(owner, &body.names,
                       default_value, fn->span, depth + 1, fuel,
                       &buffers->ordered[i])) goto done;
        }
        if(!is_identifier_text(part) ||
           !compile_type_value(type, &buffers->ordered[i])) goto done;
        ZirConst *binding = &body.names.items[i];
        copy_text(binding->name, sizeof(binding->name), part);
        copy_text(binding->type, sizeof(binding->type), type);
        copy_text(binding->expr, sizeof(binding->expr),
                  buffers->ordered[i].literal);
        if(buffers->ordered[i].kind == COMPILE_INTEGER)
            enum_member_text(owner, type, buffers->ordered[i].integer,
                             binding->expr, sizeof(binding->expr));
        copy_text(binding->path, sizeof(binding->path), SpanPath(fn->span));
        body.names.count++;
    }
    body.local_count = expected;
    for(int i = 0; i < owner->define_count; i++) {
        const ZirDefine *definition = &owner->defines[i];
        ZirConst *constant = &body.names.items[body.names.count++];
        copy_text(constant->name, sizeof(constant->name),
                  definition->name);
        copy_text(constant->expr, sizeof(constant->expr),
                  definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    ok = typed_body_statements(&body, 0, fn->stmt_count,
                               &flow, result) && flow == 1 &&
         compile_type_value(fn->return_type, result);
done:
    free(body.names.items);
    return ok;
}

static int
evaluate_typed_function(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result)
{
    static _Thread_local EvaluateTypedFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateTypedFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = evaluate_typed_function_with_buffers(module, name, call_span, arguments, argument_names, argument_count, depth, fuel, result, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers evaluate_typed_integer_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedIntegerFunctionBuffers {
    CompileValue arguments[16];
    char names[16][ZIR_NAME_MAX];
} EvaluateTypedIntegerFunctionBuffers;

int evaluate_typed_integer_function(ZirEval *ev, const char *name,
                                const long *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, long *result);

static int
evaluate_typed_integer_function_with_buffers(ZirEval *ev, const char *name,
                                const long *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, long *result, EvaluateTypedIntegerFunctionBuffers *buffers)
{
    CompileValue value = {0}; memset(buffers->arguments, 0, sizeof(buffers->arguments));
    memset(buffers->names, 0, sizeof(buffers->names));
    int local_fuel = 10000;
    int *fuel = ev->fuel != NULL ? ev->fuel : &local_fuel;
    if(ev->module == NULL || argument_count < 0 || argument_count > 16 ||
       ev->depth >= 16) return 0;
    for(int i = 0; i < argument_count; i++) {
        buffers->arguments[i].kind = COMPILE_INTEGER;
        buffers->arguments[i].integer = values[i];
        if(!compile_value_literal(&buffers->arguments[i])) return 0;
        copy_text(buffers->names[i], sizeof(buffers->names[i]), argument_names[i]);
    }
    if(!evaluate_typed_function(ev->module, name,
                                Span(ev->lookup_path, ev->source_line, 1),
                                buffers->arguments, buffers->names, argument_count,
                                ev->depth + 1, fuel, &value) ||
       value.kind != COMPILE_INTEGER) return 0;
    *result = value.integer;
    return 1;
}

int
evaluate_typed_integer_function(ZirEval *ev, const char *name,
                                const long *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, long *result)
{
    static _Thread_local EvaluateTypedIntegerFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateTypedIntegerFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = evaluate_typed_integer_function_with_buffers(ev, name, values, argument_names, argument_count, result, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

int
eval_typed_condition(const char *source, const ZirModule *module,
                     const ZirConsts *names, ZirSourceSpan span,
                     long *result)
{
    int fuel = 10000, truth;
    CompileValue value = {0};
    if(!evaluate_typed_expression(module, names, source, span, 0,
                                  &fuel, &value) ||
       !compile_truth(&value, &truth)) return 0;
    *result = truth;
    return 1;
}

int
EvaluateCompileExpression(const ZirModule *module, const char *source,
                          ZirSourceSpan span, int executing, long *value)
{
    ZirConsts constants = {0};
    char expanded[ZIR_TEXT_MAX];
    if(module == NULL || source == NULL || value == NULL) return 0;
    if(module->define_count > 0) {
        constants.items = calloc((size_t)module->define_count,
                                 sizeof(*constants.items));
        if(constants.items == NULL) return 0;
        constants.count = module->define_count;
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *definition = &module->defines[i];
            ZirConst *constant = &constants.items[i];
            copy_text(constant->name, sizeof(constant->name),
                      definition->name);
            copy_text(constant->expr, sizeof(constant->expr),
                      definition->value);
            copy_text(constant->path, sizeof(constant->path),
                      SpanPath(definition->span));
            constant->is_file_private = definition->is_file_private;
        }
    }
    expand_compile_expr(expanded, sizeof(expanded), &constants,
                        source, SpanPath(span));
    if(strstr(expanded, "size_of") != NULL)
        lower_size_of_value(expanded, sizeof(expanded), module, span);
    int ok = eval_const_condition(expanded, value, module, &constants,
                                  SpanPath(span), span.line, executing ? 1 : 0);
    free(constants.items);
    return ok;
}

int
EvaluateCompileLiteral(const ZirModule *module, const char *source,
                       ZirSourceSpan span, int executing, char *literal,
                       size_t literal_size,
                       const ZirModule **type_owner)
{
    ZirConsts constants = {0};
    CompileValue value = {0};
    int fuel = 10000;
    if(module == NULL || source == NULL || literal == NULL ||
       literal_size == 0) return 0;
    constants.count = module->define_count;
    constants.items = calloc((size_t)constants.count + 1,
                             sizeof(*constants.items));
    if(constants.items == NULL) return 0;
    for(int i = 0; i < constants.count; i++) {
        const ZirDefine *definition = &module->defines[i];
        ZirConst *constant = &constants.items[i];
        copy_text(constant->name, sizeof(constant->name),
                  definition->name);
        copy_text(constant->expr, sizeof(constant->expr),
                  definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    int ok = evaluate_typed_expression(module, &constants, source,
                                       span, executing ? 1 : 0,
                                       &fuel, &value) &&
             value.kind != COMPILE_INVALID &&
             strlen(value.literal) < literal_size;
    if(ok) {
        copy_text(literal, literal_size, value.literal);
        if(type_owner != NULL) *type_owner = value.type_owner;
    }
    free(constants.items);
    return ok;
}

/* Evaluate a boolean expression with integer variables bound, for `forall`
 * laws. Bindings shadow nothing: a bound name must not be a define. */
int
EvaluateCompileConditionBound(const ZirModule *module, const char *source,
                              ZirSourceSpan span, const char names[][ZIR_NAME_MAX],
                              const char types[][ZIR_NAME_MAX],
                              const char exprs[][ZIR_NAME_MAX],
                              const long *values, int count, int *truth)
{
    ZirConsts constants = {0};
    CompileValue value = {0};
    int fuel = 100000, ok;
    if(module == NULL || source == NULL || truth == NULL || count < 0 ||
       (count > 0 && (names == NULL || types == NULL || exprs == NULL ||
                      values == NULL)))
        return 0;
    constants.count = module->define_count + count;
    constants.items = calloc((size_t)constants.count + 1,
                             sizeof(*constants.items));
    if(constants.items == NULL) return 0;
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *definition = &module->defines[i];
        ZirConst *constant = &constants.items[i];
        copy_text(constant->name, sizeof(constant->name), definition->name);
        copy_text(constant->expr, sizeof(constant->expr), definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    for(int i = 0; i < count; i++) {
        ZirConst *constant = &constants.items[module->define_count + i];
        copy_text(constant->name, sizeof(constant->name), names[i]);
        copy_text(constant->type, sizeof(constant->type),
                  types[i][0] ? types[i] : "s64");
        snprintf(constant->expr, sizeof(constant->expr), "%ld", values[i]);
        if(exprs[i][0])
            copy_text(constant->expr, sizeof(constant->expr), exprs[i]);
        else
            enum_member_text(module, types[i], values[i], constant->expr,
                             sizeof(constant->expr));
        copy_text(constant->path, sizeof(constant->path), SpanPath(span));
    }
    law_enum_members = 1;
    ok = evaluate_typed_expression(module, &constants, source, span, 0,
                                   &fuel, &value) &&
         compile_truth(&value, truth);
    law_enum_members = 0;
    free(constants.items);
    return ok;
}
