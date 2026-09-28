#include "zir_parse_internal.h"

static int evaluate_typed_function(const ZirModule *module, const char *name, ZirSourceSpan call_span, CompileValue *arguments, char argument_names[][ZIR_NAME_MAX], int argument_count, int depth, int *fuel, CompileValue *result);

int
evaluate_typed_node(const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result)
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
        }
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left)) return 0;
        if(left.kind == COMPILE_STRING &&
           !strcmp(expression->name, "count")) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(!DecodeStringLiteral(left.literal, bytes, sizeof(bytes),
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
        CompileValue args[16] = {{0}};
        char names[16][ZIR_NAME_MAX] = {{0}};
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
                                    depth + 1, fuel, &args[count])) return 0;
            copy_text(names[count], sizeof(names[count]),
                      probe->exprs[child].argument_name);
            count++;
        }
        ZirSourceSpan call_span = expression->left >= 0 ?
            probe->exprs[expression->left].span : expression->span;
        return evaluate_typed_function(module, name, call_span, args, names,
                                       count, depth + 1, fuel, result);
    }
    default:
        return 0;
    }
}

int
evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result)
{
    ZirConsts empty = {0};
    ZirFunction probe = {0};
    char input[ZIR_TEXT_MAX], expanded[ZIR_TEXT_MAX];
    long integer;
    int root, ok;
    const char *path = span.path;
    if(depth > 32 || source == NULL || --*fuel < 0) return 0;
    copy_text(input, sizeof(input), source);
    trim_in_place(input);
    size_t length = strlen(input);
    if(length && input[length - 1] == ';') {
        input[length - 1] = '\0';
        trim_in_place(input);
    }
    if(!input[0]) return 0;
    if(names == NULL) names = &empty;
    expand_compile_expr(expanded, sizeof(expanded), names, input, path);
    if(eval_const_condition_with_fuel(expanded, &integer, module, names,
                                     path, span.line, depth, fuel)) {
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        return compile_value_literal(result);
    }
    root = ParseExpr(&probe, module, expanded, span);
    ok = root >= 0;
    if(depth == 0)
        for(int i = 0; i < probe.expr_count; i++)
            if(probe.exprs[i].kind == ZIR_EXPR_COMPILE_TIME)
                ok = 0;
    if(ok)
        ok = evaluate_typed_node(&probe, root, module, path,
                                 depth + 1, fuel, result);
    free(probe.exprs);
    return ok;
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

static int
typed_body_condition(TypedBody *body, const char *header,
                     const char *word, int *truth)
{
    char condition[ZIR_TEXT_MAX];
    const char *source = skip_ws(header + strlen(word));
    copy_text(condition, sizeof(condition), source);
    trim_in_place(condition);
    size_t length = strlen(condition);
    if(!length || condition[length - 1] != '{') return 0;
    condition[length - 1] = '\0';
    trim_in_place(condition);
    length = strlen(condition);
    if(length >= 4 && !strcmp(condition + length - 4, "then") &&
       (length == 4 || isspace((unsigned char)condition[length - 5]))) {
        condition[length - 4] = '\0';
        trim_in_place(condition);
    }
    CompileValue value = {0};
    return typed_body_expression(body, condition, &value) &&
           compile_truth(&value, truth);
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
    copy_text(local->path, sizeof(local->path), body->fn->span.path);
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

static int
typed_body_statements(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result)
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
            char text[ZIR_TEXT_MAX];
            copy_text(text, sizeof(text), source);
            char *colon = strchr(text, ':');
            if(colon == NULL) goto failed;
            *colon++ = '\0';
            trim_in_place(text);
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
            if(!typed_add_local(body, text, type,
                                equals != NULL ? &value : NULL)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_ASSIGN) {
            char text[ZIR_TEXT_MAX];
            copy_text(text, sizeof(text), source);
            char *equals = strchr(text, '=');
            if(equals == NULL) goto failed;
            char op = equals > text ? equals[-1] : '\0';
            if(op && strchr("+-*/%", op) != NULL) equals[-1] = '\0';
            else op = '\0';
            *equals++ = '\0';
            trim_in_place(text);
            int local = typed_local_index(body, text);
            if(local < 0) goto failed;
            CompileValue value = {0};
            if(op) {
                char combined[ZIR_TEXT_MAX];
                int written = snprintf(combined, sizeof(combined),
                    "(%s) %c (%s)", body->names.items[local].expr, op,
                    equals);
                if(written < 0 || (size_t)written >= sizeof(combined) ||
                   !typed_body_expression(body, combined, &value)) goto failed;
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
            char control[ZIR_TEXT_MAX];
            copy_text(control, sizeof(control), source);
            trim_in_place(control);
            size_t length = strlen(control);
            if(length && control[length - 1] == ';')
                control[length - 1] = '\0';
            if(strcmp(control, statement->kind == ZIR_STMT_BREAK ?
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
evaluate_typed_function(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    char parameters[16][ZIR_TEXT_MAX];
    char defaults[16][ZIR_TEXT_MAX] = {{0}};
    CompileValue ordered[16] = {{0}};
    unsigned used = 0;
    TypedBody body = {0};
    int expected, flow = 0, ok = 0;
    if(depth >= 32 || module == NULL || !name[0] ||
       ResolveFunctionAt(module, name, call_span.path, &owner, &fn) != 1 ||
       fn == NULL || fn->is_extern || fn->is_template ||
       fn->stmt_count == 0) return 0;
    expected = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 16,
                        sizeof(parameters[0])) : 0;
    if(expected < 0 || argument_count > expected) return 0;
    if(fn->default_args[0] &&
       split_top_level(fn->default_args, defaults[0], 16,
                       sizeof(defaults[0])) != expected) return 0;
    for(int argument = 0; argument < argument_count; argument++) {
        int position = -1;
        if(argument_names[argument][0]) {
            size_t length = strlen(argument_names[argument]);
            for(int i = 0; i < expected; i++) {
                const char *start = skip_ws(parameters[i]);
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
        ordered[position] = arguments[argument];
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
        char *part = trim(parameters[i]);
        char *colon = strchr(part, ':');
        if(colon == NULL) goto done;
        *colon++ = '\0';
        trim_in_place(part);
        char *type = trim(colon);
        char *default_value = top_level_assignment(defaults[i]);
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
                ordered[i].kind = COMPILE_COMPOUND;
                copy_text(ordered[i].type, sizeof(ordered[i].type),
                          "Source_Code_Location");
                ordered[i].type_owner = owner;
                if(!CallerLocationLiteral(module, call_span,
                                          ordered[i].literal,
                                          sizeof(ordered[i].literal))) goto done;
            } else if(!evaluate_typed_expression(owner, &body.names,
                       default_value, fn->span, depth + 1, fuel,
                       &ordered[i])) goto done;
        }
        if(!is_identifier_text(part) ||
           !compile_type_value(type, &ordered[i])) goto done;
        ZirConst *binding = &body.names.items[i];
        copy_text(binding->name, sizeof(binding->name), part);
        copy_text(binding->type, sizeof(binding->type), type);
        copy_text(binding->expr, sizeof(binding->expr),
                  ordered[i].literal);
        copy_text(binding->path, sizeof(binding->path), fn->span.path);
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
                  definition->span.path);
        constant->is_file_private = definition->is_file_private;
    }
    ok = typed_body_statements(&body, 0, fn->stmt_count,
                               &flow, result) && flow == 1 &&
         compile_type_value(fn->return_type, result);
done:
    free(body.names.items);
    return ok;
}

int
evaluate_typed_integer_function(ZirEval *ev, const char *name,
                                const long *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, long *result)
{
    CompileValue arguments[16] = {{0}}, value = {0};
    char names[16][ZIR_NAME_MAX] = {{0}};
    int local_fuel = 10000;
    int *fuel = ev->fuel != NULL ? ev->fuel : &local_fuel;
    if(ev->module == NULL || argument_count < 0 || argument_count > 16 ||
       ev->depth >= 16) return 0;
    for(int i = 0; i < argument_count; i++) {
        arguments[i].kind = COMPILE_INTEGER;
        arguments[i].integer = values[i];
        if(!compile_value_literal(&arguments[i])) return 0;
        copy_text(names[i], sizeof(names[i]), argument_names[i]);
    }
    if(!evaluate_typed_function(ev->module, name,
                                Span(ev->lookup_path, ev->source_line, 1),
                                arguments, names, argument_count,
                                ev->depth + 1, fuel, &value) ||
       value.kind != COMPILE_INTEGER) return 0;
    *result = value.integer;
    return 1;
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
                      definition->span.path);
            constant->is_file_private = definition->is_file_private;
        }
    }
    expand_compile_expr(expanded, sizeof(expanded), &constants,
                        source, span.path);
    if(strstr(expanded, "size_of") != NULL)
        lower_size_of_value(expanded, sizeof(expanded), module, span);
    int ok = eval_const_condition(expanded, value, module, &constants,
                                  span.path, span.line, executing ? 1 : 0);
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
                  definition->span.path);
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
