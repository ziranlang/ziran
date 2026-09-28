#include "zir_emit_internal.h"

/* Keep collection lowering outside recursive expression lowering: its large
 * target buffers must not increase every nested expression stack frame. */
static void
emit_vec_call(Emitter *e, const ZirExpr *expr, char *out, size_t size)
{
    int first = expr->first_child;
    int push = !strcmp(expr->name, "VecPush");
    char vector[ZIR_TEXT_MAX], value[ZIR_TEXT_MAX];
    char item_name[ZIR_NAME_MAX], result_name[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    if(first < 0 || !VecElementType(e->module,
        e->fn->exprs[first].type, element, sizeof(element)))
        fatal(expr, "invalid Vec operation");
    emit_destination(e, first, vector, sizeof(vector));
    if(!strcmp(expr->name, "VecSwap")) {
        int second = e->fn->exprs[first].next_sibling;
        char other[ZIR_TEXT_MAX], tmp[ZIR_NAME_MAX];
        emit_destination(e, second, other, sizeof(other));
        fresh(e, tmp);
        declare(e, tmp, e->fn->exprs[first].type, vector);
        line(e, "%s = %s%s", vector, other,
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s = %s%s", other, tmp,
             e->target == ZIR_GO ? "" : ";");
        out[0] = '\0'; e->pure = 0; return;
    }
    if(push) {
        int second = e->fn->exprs[first].next_sibling;
        if(second < 0) fatal(expr, "VecPush requires a value");
        emit_expr(e, second, element, value, sizeof(value));
        fresh(e, item_name);
        declare(e, item_name, element, value);
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s)",
                 vector, vector, item_name);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            char grown[ZIR_NAME_MAX];
            fresh(e, grown);
            line(e, "void *%s = ZirVecGrow((void *)(%s).data, &(%s).capacity, (%s).count, sizeof(*(%s).data));",
                 grown, vector, vector, vector, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", vector, mapped, grown);
            line(e, "(%s).data[(%s).count++] = %s;",
                 vector, vector, item_name);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecPop") || !strcmp(expr->name, "VecGet")) {
        int get = !strcmp(expr->name, "VecGet");
        char index[ZIR_TEXT_MAX];
        char initializer[ZIR_NAME_MAX * 2];
        zero_record(e, expr->type, initializer, sizeof(initializer));
        fresh(e, result_name);
        declare(e, result_name, expr->type, initializer);
        if(get) {
            int second = e->fn->exprs[first].next_sibling;
            if(second < 0) fatal(expr, "VecGet requires an index");
            emit_expr(e, second, "s64", index, sizeof(index));
        }
        if(e->target == ZIR_GO) {
            if(get)
                line(e, "if %s >= 0 && int64(%s) < %s.Count {", index, index,
                     vector);
            else
                line(e, "if %s.Count > 0 {", vector);
            e->indent++;
            if(get)
                line(e, "%s.Value = %s.Data[%s]", result_name, vector, index);
            else {
                line(e, "%s.Count--", vector);
                line(e, "%s.Value = %s.Data[%s.Count]", result_name, vector,
                     vector);
            }
            line(e, "%s.HasValue = true", result_name);
            e->indent--;
            line(e, "}");
        } else {
            char field[ZIR_TEXT_MAX * 2];
            if(get)
                line(e, "if (%s >= 0 && %s < (%s).count) {", index, index,
                     vector);
            else
                line(e, "if ((%s).count > 0) {", vector);
            e->indent++;
            if(get)
                format(field, sizeof(field), "(%s).data[%s]", vector, index);
            else {
                line(e, "(%s).count--;", vector);
                format(field, sizeof(field), "(%s).data[(%s).count]", vector,
                       vector);
            }
            {
                char destination[ZIR_NAME_MAX * 2];
                format(destination, sizeof(destination), "%s.value",
                       result_name);
                assign_value(e, destination, element, field);
            }
            line(e, "%s.has_value = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecClone")) {
        int second = e->fn->exprs[first].next_sibling;
        char source[ZIR_TEXT_MAX], grown[ZIR_NAME_MAX];
        if(second < 0) fatal(expr, "VecClone requires a source Vec");
        emit_destination(e, second, source, sizeof(source));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s.Data...)",
                 vector, vector, source);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            fresh(e, grown);
            line(e, "if ((%s).count > 0) {", source);
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (%s).count, sizeof(*(%s).data));",
                 grown, vector, vector, vector, source, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", vector, mapped, grown);
            line(e, "memcpy((%s).data, (%s).data, (size_t)(%s).count * sizeof(*(%s).data));",
                 vector, source, source, vector);
            line(e, "(%s).count = (%s).count;", vector, source);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
            e->indent--;
            line(e, "} else {");
            e->indent++;
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecSlice")) {
        int second = e->fn->exprs[first].next_sibling;
        int third = second >= 0 ? e->fn->exprs[second].next_sibling : -1;
        char low[ZIR_TEXT_MAX], high[ZIR_TEXT_MAX];
        char view[ZIR_NAME_MAX];
        if(second < 0 || third < 0)
            fatal(expr, "VecSlice requires low and high bounds");
        emit_expr(e, second, "s64", low, sizeof(low));
        emit_expr(e, third, "s64", high, sizeof(high));
        fresh(e, view);
        if(e->target == ZIR_GO) {
            line(e, "if %s < 0 || %s < %s || %s > %s.Count { panic(\"slice range out of bounds\") }",
                 low, high, low, high, vector);
            format(out, size, "%s.Data[%s:%s]", vector, low, high);
        } else {
            line(e, "Slice %s = {(%s).data, (%s).count};", view, vector,
                 vector);
            format(out, size, "SliceRange(%s, %s, %s, sizeof(*(%s).data))",
                   view, low, high, vector);
        }
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "BuilderAppend")) {
        int second = e->fn->exprs[first].next_sibling;
        char text[ZIR_TEXT_MAX];
        if(second < 0) fatal(expr, "BuilderAppend requires text");
        emit_expr(e, second, "string", text, sizeof(text));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s...)",
                 vector, vector, text);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            char grown[ZIR_NAME_MAX];
            fresh(e, grown);
            line(e, "if ((%s).length == 0) {", text);
            e->indent++;
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "} else {");
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (int64_t)(%s).length, sizeof(*(%s).data));",
                 grown, vector, vector, vector, text, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (uint8_t *)%s;", vector, grown);
            line(e, "memcpy((%s).data + (%s).count, (%s).data, (size_t)(%s).length);",
                 vector, vector, text, text);
            line(e, "(%s).count += (int64_t)(%s).length;", vector, text);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "BuilderFinish")) {
        char finished[ZIR_NAME_MAX];
        fresh(e, finished);
        if(e->target == ZIR_GO) {
            declare(e, finished, "string", "\"\"");
            line(e, "%s = string(%s.Data[:%s.Count])",
                 finished, vector, vector);
            line(e, "%s.Data = nil", vector);
        } else {
            declare(e, finished, "string",
                    e->target == ZIR_CPP ? "{}" : "{NULL, 0}");
            line(e, "%s.data = (%s).count > 0 ? (const char *)(%s).data : \"\";",
                 finished, vector, vector);
            line(e, "%s.length = (size_t)(%s).count;", finished, vector);
            /* The finished string borrows the builder's bytes; the builder
             * detaches without freeing them. */
            line(e, "(%s).data = NULL;", vector);
        }
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Count" : "count",
             e->target == ZIR_GO ? "" : ";");
        copy_text(out, size, finished);
        e->pure = 0;
        return;
    }
    if(!strcmp(expr->name, "VecFree")) {
        if(e->target == ZIR_GO)
            line(e, "%s.Data = nil", vector);
        else {
            line(e, "free((%s).data);", vector);
            line(e, "(%s).data = NULL;", vector);
        }
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
    } else if(e->target == ZIR_GO)
        line(e, "%s.Data = %s.Data[:0]", vector, vector);
    line(e, "%s.%s = 0%s", vector,
         e->target == ZIR_GO ? "Count" : "count",
         e->target == ZIR_GO ? "" : ";");
    out[0] = '\0';
    e->pure = 0;
}

static void
native_size_expression(Emitter *e, const char *type, char *out, size_t size)
{
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        char item_size[ZIR_TEXT_MAX], bound[ZIR_NAME_MAX];
        native_size_expression(e, element, item_size, sizeof(item_size));
        if(capacity >= 0) snprintf(bound, sizeof(bound), "%d", capacity);
        else {
            const char *close = strchr(type, ']');
            char source_bound[ZIR_NAME_MAX];
            format(source_bound, sizeof(source_bound), "%.*s",
                   (int)(close - type - 1), type + 1);
            e->resolve(e->context, source_bound, bound, sizeof(bound));
        }
        format(out, size, "((%s) * (%s))", bound, item_size);
        return;
    }
    if(!strcmp(ScalarType(type), "void")) {
        copy_text(out, size, "0");
        return;
    }
    if(type[0] == '*') {
        copy_text(out, size, e->target == ZIR_GO ?
                  "unsafe.Sizeof((*byte)(nil))" : "sizeof(void *)");
        return;
    }
    if(e->target == ZIR_GO && SliceElementType(type, NULL, 0)) {
        copy_text(out, size, "unsafe.Sizeof([]byte(nil))");
        return;
    }
    const char *native = TargetType(type, e->target);
    if(native != NULL) copy_text(mapped, sizeof(mapped), native);
    else e->resolve(e->context, type, mapped, sizeof(mapped));
    if(e->target == ZIR_GO)
        format(out, size, "unsafe.Sizeof(*new(%s))", mapped);
    else
        format(out, size, "sizeof(%s)", mapped);
}

/* Binding strength shared by C and Go for the operators whose order they
 * agree on; bitwise operators, which C ranks below comparisons, have none. */
static int
operator_rank(const char *op)
{
    static const char *const ranks[][7] = {
        {"||"}, {"&&"}, {"==", "!=", "<", "<=", ">", ">="},
        {"+", "-", "*", "/", "%", "<<", ">>"},
    };
    for(int rank = 0; rank < 4; rank++)
        for(int i = 0; i < 7 && ranks[rank][i] != NULL; i++)
            if(!strcmp(ranks[rank][i], op))
                return rank + 1;
    return 0;
}

/* An operand that binds tighter than its operator needs no parentheses:
 * n % 15 == 0 rather than (n % 15) == 0. */
static void
loose_operand(const Emitter *e, int child, const char *op, char *text, size_t size)
{
    const ZirExpr *operand = &e->fn->exprs[child];
    int parent = operator_rank(op), inner;
    char plain[ZIR_TEXT_MAX];
    if(operand->kind != ZIR_EXPR_BINARY || !enclosed(text))
        return;
    inner = operator_rank(operand->op);
    if(!inner || !parent || inner < parent ||
       (inner == parent && parent != 1 && parent != 2))
        return;
    copy_text(text, size, bare(text, plain, sizeof(plain)));
}

void
emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size)
{
    const ZirExpr *expr=&e->fn->exprs[index];
    const char *type=canonical(expr->type);
    char a[ZIR_TEXT_MAX],b[ZIR_TEXT_MAX],result[ZIR_TEXT_MAX],temp[ZIR_NAME_MAX];
    int pure=0;
    int call_in_place = e->call_in_place;
    e->call_in_place = 0;
    /* Binary-shaped results re-bind when spliced into a parent expression, so
     * the tail parenthesizes them; identifiers, literals, calls, and slices
     * are operand-safe without wrapping. */
    int atom=1;
    if(!strcmp(expr->type,"integer") || !strcmp(expr->type,"real")) {
        const char *want=canonical(expected); if(*want && strcmp(want,"bool") && strcmp(want,"void")) type=want;
    }
    if(!strcmp(expr->type, "null") && strchr(expected, '*') != NULL)
        type = canonical(expected);
    switch(expr->kind) {
    case ZIR_EXPR_SIZE_OF:
        native_size_expression(e, expr->name, a, sizeof(a));
        if(e->target == ZIR_GO)
            format(result, sizeof(result), "%s(%s)", TargetType(type, e->target), a);
        else
            copy_text(result, sizeof(result), a);
        pure = 1;
        break;
    case ZIR_EXPR_COMPOUND: {
        /* Go spells a record or array value as one literal, Point{X: 3, Y: 4}:
         * its zero value is T{}, so unnamed fields keep their zero. */
        if(e->target == ZIR_GO) {
            int is_array = ArrayElementType(type, NULL, 0, NULL) != 0;
            size_t used;
            pure = 1;
            if(is_array) {
                char element[ZIR_NAME_MAX * 2], bounds[ZIR_NAME_MAX * 2];
                array_target_type(e, type, element, sizeof(element), bounds, sizeof(bounds));
                used = (size_t)format(result, sizeof(result), "%s%s{", bounds, element);
            } else {
                e->resolve(e->context, type, b, sizeof(b));
                used = (size_t)format(result, sizeof(result), "%s{", b);
            }
            for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                char field_name[ZIR_NAME_MAX];
                emit_expr(e, entry->right, entry->type, a, sizeof(a));
                pure &= e->pure;
                if(!is_array)
                    go_field_ident(entry->name, field_name, sizeof(field_name));
                used += (size_t)format(result + used, used < sizeof(result) ? sizeof(result) - used : 0,
                                       "%s%s%s%s", child == expr->first_child ? "" : ", ",
                                       is_array ? "" : field_name, is_array ? "" : ": ", a);
            }
            if(used + 2 >= sizeof(result))
                fatal(expr, "record value is too long");
            copy_text(result + used, sizeof(result) - used, "}");
            break;
        }
        if(ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare(e, temp, type, NULL);
            int ordinal = 0;
            for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                emit_expr(e, entry->right, entry->type, a, sizeof(a));
                format(b, sizeof(b), "%s[%d]", temp, ordinal++);
                assign_value(e, b, entry->type, a);
            }
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        zero_record(e, type, a, sizeof(a));
        fresh(e, temp);
        declare(e, temp, type, a);
        for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
            const ZirExpr *field = &e->fn->exprs[child];
            char field_name[ZIR_NAME_MAX];
            if(e->target == ZIR_GO)
                go_field_ident(field->name, field_name, sizeof(field_name));
            else
                TargetFieldName(field_record(e->module, type), e->target,
                                field->name, field_name, sizeof(field_name));
            emit_expr(e, field->right, field->type, a, sizeof(a));
            format(b, sizeof(b), "%s.%s", temp, field_name);
            assign_value(e, b, field->type, a);
        }
        copy_text(out, size, temp);
        e->pure = 1;
        return;
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER: {
        int base_pure;
        /* Reading a field needs a snapshot of that field, not a copy of every
         * enclosing record. Calls and other computed bases still evaluate once. */
        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, a, sizeof(a));
        else
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, a, sizeof(a));
        base_pure = e->pure;
        const char *base_type = e->fn->exprs[expr->left].type;
        int capacity;
        if(!strcmp(expr->name, "count") &&
           ArrayElementType(base_type, NULL, 0, &capacity)) {
            if(capacity < 0) {
                Diagnostic(expr->span, "emit.array_count",
                           "fixed array count requires a resolved bound");
                exit(1);
            }
            format(result, sizeof(result), "%d", capacity);
            pure = base_pure;
            break;
        }
        if(!strcmp(expr->name, "data") &&
           ArrayElementType(base_type, NULL, 0, &capacity)) {
            if(capacity == 0) {
                if(e->target == ZIR_GO) {
                    char element[ZIR_NAME_MAX];
                    char mapped[ZIR_NAME_MAX * 2];
                    ArrayElementType(base_type, element, sizeof(element), NULL);
                    const char *scalar = TargetType(element, ZIR_GO);
                    if(scalar != NULL) copy_text(mapped, sizeof(mapped), scalar);
                    else e->resolve(e->context, element, mapped,
                                    sizeof(mapped));
                    format(result, sizeof(result), "(*%s)(nil)", mapped);
                } else
                    copy_text(result, sizeof(result), "NULL");
            } else if(e->target == ZIR_GO)
                format(result, sizeof(result), "&(%s)[0]", a);
            else
                copy_text(result, sizeof(result), a);
            pure = base_pure;
            break;
        }
        if((!strcmp(base_type, "string") ||
            SliceElementType(base_type, NULL, 0)) &&
           !strcmp(expr->name, "count")) {
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "int64(len(%s))", a);
            else
                format(result, sizeof(result), "(int64_t)(%s).length", a);
            pure = base_pure;
            break;
        }
        emit_field_path(e->module, e->target, base_type, expr->name,
                        a, result, sizeof(result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, result);
            clear_owned_value(e, result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = base_pure;
        break;
    }
    case ZIR_EXPR_SLICE: {
        const char *base_type = e->fn->exprs[expr->left].type;
        char source[ZIR_TEXT_MAX], low[ZIR_TEXT_MAX], high[ZIR_TEXT_MAX];
        char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
        char view[ZIR_NAME_MAX];
        int capacity = 0;
        int base_pure;
        int low_pure = 1;
        int high_pure = 1;
        if(!strcmp(base_type, "string")) {
            emit_expr(e, expr->left, "string", source, sizeof(source));
            base_pure = e->pure;
            fresh(e, view);
            if(e->target == ZIR_GO)
                line(e, "%s := %s", view, source);
            else
                line(e, "String %s = %s;", view, source);
            if(expr->right >= 0) {
                emit_expr(e, expr->right, "s64", low, sizeof(low));
                low_pure = e->pure;
            } else copy_text(low, sizeof(low), "0");
            if(expr->third >= 0) {
                emit_expr(e, expr->third, "s64", high, sizeof(high));
                high_pure = e->pure;
            } else format(high, sizeof(high), e->target == ZIR_GO ?
                          "len(%s)" : "%s.length", view);
            pure = base_pure && low_pure && high_pure;
            if(e->target == ZIR_GO) {
                line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) { panic(\"string range out of bounds\") }",
                     low, high, low, high, view);
                format(result, sizeof(result), "%s[%s:%s]", view, low, high);
            } else {
                format(result, sizeof(result), "StringRange(%s, (int64_t)%s, (int64_t)%s)",
                       view, low, high);
            }
            break;
        }
        int array = ArrayElementType(base_type, element, sizeof(element), &capacity);
        if(array) {
            emit_destination(e, expr->left, source, sizeof(source));
        } else {
            SliceElementType(base_type, element, sizeof(element));
            emit_expr(e, expr->left, base_type, source, sizeof(source));
        }
        base_pure = e->pure;
        /* A fixed array's length is its capacity, so Go's own bounds check
         * on arr[low:high:high] is exactly Ziran's. */
        if(array && capacity > 0 && e->target == ZIR_GO) {
            if(expr->right >= 0) {
                emit_expr(e, expr->right, "s64", low, sizeof(low));
                low_pure = e->pure;
            } else copy_text(low, sizeof(low), "0");
            if(expr->third >= 0) {
                emit_expr(e, expr->third, "s64", high, sizeof(high));
                high_pure = e->pure;
            } else format(high, sizeof(high), "%d", capacity);
            pure = base_pure && low_pure && high_pure;
            /* Go rejects constant bounds outside the array at compile time;
             * Ziran stops at run time, so those keep the checked form. */
            uint64_t low_bits = 0, high_bits = 0;
            int low_constant = integer_literal_bits(low, &low_bits);
            int high_constant = integer_literal_bits(high, &high_bits);
            if((low_constant && (low[0] == '-' || low_bits > (uint64_t)capacity)) ||
               (high_constant && (high[0] == '-' || high_bits > (uint64_t)capacity)) ||
               (low_constant && high_constant && low_bits > high_bits)) {
                fresh(e, view);
                line(e, "%s := %s[:]", view, source);
                line(e, "if %s < 0 || %s < %s || %s > int64(len(%s)) {", low, high, low, high, view);
                e->indent++;
                line(e, "panic(\"slice range out of bounds\")");
                e->indent--;
                line(e, "}");
                format(result, sizeof(result), "%s[%s:%s:%s]", view, low, high, high);
                break;
            }
            if(plain_identifier(high) || high_constant)
                format(result, sizeof(result), "%s[%s:%s:%s]", source, low, high, high);
            else {
                fresh(e, temp);
                declare(e, temp, "s64", high);
                format(result, sizeof(result), "%s[%s:%s:%s]", source, low, temp, temp);
            }
            break;
        }
        fresh(e, view);
        if(array && capacity == 0 && e->target == ZIR_GO) {
            const char *scalar = TargetType(element, ZIR_GO);
            if(scalar != NULL) copy_text(mapped, sizeof(mapped), scalar);
            else e->resolve(e->context, element, mapped, sizeof(mapped));
            line(e, "var %s []%s", view, mapped);
        } else if(array && capacity == 0) {
            line(e, "Slice %s = {NULL, 0};", view);
        } else if(e->target == ZIR_GO && !array && plain_identifier(source) &&
                  !expression_calls(e->fn, expr->right) &&
                  !expression_calls(e->fn, expr->third)) {
            /* A named slice is its own view while nothing can reassign it. */
            copy_text(view, sizeof(view), source);
        } else if(e->target == ZIR_GO) {
            line(e, "%s := %s[:]", view, source);
        } else if(array) {
            line(e, "Slice %s = {%s, %d};", view, source, capacity);
        } else {
            line(e, "Slice %s = %s;", view, source);
        }
        if(expr->right >= 0) {
            emit_expr(e, expr->right, "s64", low, sizeof(low));
            low_pure = e->pure;
        }
        else
            copy_text(low, sizeof(low), "0");
        if(expr->third >= 0) {
            emit_expr(e, expr->third, "s64", high, sizeof(high));
            high_pure = e->pure;
        }
        else
            format(high, sizeof(high), e->target == ZIR_GO ? "int64(len(%s))" : "%s.length", view);
        pure = base_pure && low_pure && high_pure;
        if(e->target == ZIR_GO) {
            line(e, "if %s < 0 || %s < %s || %s > int64(len(%s)) {", low, high, low, high, view);
            e->indent++;
            line(e, "panic(\"slice range out of bounds\")");
            e->indent--;
            line(e, "}");
            format(result, sizeof(result), "%s[%s:%s:%s]", view, low, high, high);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            format(result, sizeof(result), "SliceRange(%s, (int64_t)%s, (int64_t)%s, sizeof(%s))",
                   view, low, high, mapped);
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        const char *base_type = e->fn->exprs[expr->left].type;
        int capacity = 0;
        int base_pure;

        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, a, sizeof(a));
        else
            emit_expr(e, expr->left, base_type, a, sizeof(a));
        base_pure = e->pure;
        emit_expr(e, expr->right, "s32", b, sizeof(b));
        pure = base_pure && e->pure;
        int fixed_array = ArrayElementType(base_type, NULL, 0, &capacity);
        if(VecElementType(e->module, base_type, NULL, 0) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            format(result, sizeof(result),
                   "ZIRAN_VEC_INDEX((%s).data, (%s).count, %s)", a, a, b);
        } else if(VecElementType(e->module, base_type, NULL, 0)) {
            format(result, sizeof(result), "(%s).Data[%s]", a, b);
        } else if(SliceElementType(base_type, NULL, 0)) {
            slice_index(e, base_type, a, b, result, sizeof(result));
        } else if(e->target == ZIR_GO && base_type[0] == '*') {
            go_pointer_index(e, a, b, result, sizeof(result));
        } else if(!strcmp(base_type, "string")) {
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "%s[%s]", a, b);
            else
                format(result, sizeof(result), "(uint8_t)ZIRAN_INDEX(%s.data, %s.length, %s)", a, a, b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity == 0) {
            format(result, sizeof(result), "ZIRAN_EMPTY_INDEX(%s, %s)", a, b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity > 0) {
            /* Fixed-capacity arrays are bounds-checked in debug builds. */
            format(result, sizeof(result), "ZIRAN_INDEX(%s, %d, %s)",
                   a, capacity, b);
        } else {
            format(result, sizeof(result), "%s[%s]", a, b);
        }
        break;
    }
    case ZIR_EXPR_COMPILE_TIME:
        copy_text(result, sizeof(result), "false");
        pure = 1;
        break;
    case ZIR_EXPR_IDENT:
        if(expr->is_function_value) {
            emit_function_value(e, index, result, sizeof(result));
            break;
        }
        if(!strcmp(expr->name, "null")) {
            copy_text(result, sizeof(result), e->target == ZIR_GO ? "nil" :
                      e->target == ZIR_CPP ? "nullptr" : "((void *)0)");
            pure = 1;
            break;
        }
        resolve(e, expr->name, result, sizeof(result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, result);
            clear_owned_value(e, result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = 1;
        break;
    case ZIR_EXPR_STRING:
        string_literal(expr, e->target, a, sizeof(a));
        if(e->target == ZIR_C || e->target == ZIR_CPP)
            format(result, sizeof(result), "StringLiteral(%s)", a);
        else
            copy_text(result, sizeof(result), a);
        pure = 1;
        break;
    case ZIR_EXPR_INT:
        literal(e,expr,type,0,result,sizeof(result));
        if(e->target == ZIR_CPP && enum_type(e->module, type)) {
            copy_text(a, sizeof(a), result);
            char native[ZIR_NAME_MAX * 2];
            e->resolve(e->context, type, native, sizeof(native));
            format(result, sizeof(result), "(%s)(%s)", native, a);
        }
        pure=1;
        break;
    case ZIR_EXPR_FLOAT: {
        copy_text(result,sizeof(result),expr->text);size_t n=strlen(result);
        if(n && (result[n-1]=='f' || result[n-1]=='F')) result[n-1]=0;
        pure = 1;
        break;
    }
    case ZIR_EXPR_CALL:
        if(!strcmp(expr->name, "TextView")) {
            if(expr->first_child < 0 ||
               e->fn->exprs[expr->first_child].next_sibling >= 0)
                fatal(expr, "TextView requires one []u8 argument");
            emit_expr(e, expr->first_child, "[]u8", a, sizeof(a));
            if(e->target == ZIR_GO) {
                format(out, size, "string(%s)", a);
            } else {
                fresh(e, temp);
                line(e, "Slice %s = %s;", temp, a);
                format(out, size,
                       "StringView((const char *)(%s).data, (size_t)(%s).length)",
                       temp, temp);
            }
            e->pure = 1;
            return;
        }
        if(!strcmp(expr->name, "VecPush") ||
           !strcmp(expr->name, "VecClear") ||
           !strcmp(expr->name, "VecFree") ||
           !strcmp(expr->name, "VecSwap") ||
           !strcmp(expr->name, "VecPop") ||
           !strcmp(expr->name, "VecGet") ||
           !strcmp(expr->name, "VecClone") ||
           !strcmp(expr->name, "VecSlice") ||
           !strcmp(expr->name, "BuilderAppend") ||
           !strcmp(expr->name, "BuilderFinish")) {
            emit_vec_call(e, expr, out, size);
            return;
        }
        if(!strcmp(expr->name, "print")) {
            emit_print(e, expr);
            out[0] = '\0';
            e->pure = 0;
            return;
        }
        if((e->target == ZIR_C || e->target == ZIR_CPP) &&
           ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare_array(e, temp, type, NULL);
            emit_call(e, expr, temp, result, sizeof(result));
            line(e, "%s;", result);
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        emit_call(e, expr, NULL, result, sizeof(result));
        if(!strcmp(type,"void")) {line(e,"%s%s",result,e->target==ZIR_GO?"":";");out[0]=0;e->pure=0;return;}
        break;
    case ZIR_EXPR_CONDITIONAL:
        emit_expr(e,expr->left,"bool",a,sizeof(a));fresh(e,temp);
        const ZirType *declared = FindType(e->module, type, NULL);
        if(declared != NULL && declared->is_procedure_type) {
            if(e->target == ZIR_C || e->target == ZIR_CPP)
                format(b, sizeof(b), "(%s){0}", type);
            else
                copy_text(b, sizeof(b), e->target == ZIR_GO ? "nil" : "null");
        } else if(record_type(e->module, type))
            zero_record(e, type, b, sizeof(b));
        else
            copy_text(b, sizeof(b), zero_value(type, e->target));
        declare(e, temp, type, b);
        line(e, e->target == ZIR_GO ? "if %s {" : "if (%s) {", a);
        e->indent++;
        emit_expr(e, expr->right, type, b, sizeof(b));
        assign_value(e, temp, type, b);
        e->indent--;
        line(e, "} else {");
        e->indent++;
        emit_expr(e, expr->third, type, b, sizeof(b));
        assign_value(e, temp, type, b);
        e->indent--;
        line(e, "}");
        copy_text(out, size, temp);
        e->pure = 1;
        return;
    case ZIR_EXPR_BINARY: {
        const char *operand_type=type;
        int left_pure;
        if(!strcmp(type,"bool")) {
            operand_type=canonical(e->fn->exprs[expr->left].type);
            if(!strcmp(e->fn->exprs[expr->left].type,"integer") || !strcmp(e->fn->exprs[expr->left].type,"real")) operand_type=canonical(e->fn->exprs[expr->right].type);
            if(!strcmp(e->fn->exprs[expr->left].type, "null"))
                operand_type = canonical(e->fn->exprs[expr->right].type);
        }
        /* Procedure slots compare like pointers: against null through the
         * callable entry, and against another slot field by field. */
        const ZirType *operand_slot = FindType(e->module, operand_type, NULL);
        int slot_compare = operand_slot != NULL &&
                           operand_slot->is_procedure_type &&
                           (!strcmp(expr->op, "==") || !strcmp(expr->op, "!="));
        int left_is_null = e->fn->exprs[expr->left].kind == ZIR_EXPR_IDENT &&
                           !strcmp(e->fn->exprs[expr->left].name, "null");
        int right_is_null = e->fn->exprs[expr->right].kind == ZIR_EXPR_IDENT &&
                            !strcmp(e->fn->exprs[expr->right].name, "null");
        if(slot_compare && (left_is_null || right_is_null)) {
            int value_expr = left_is_null ? expr->right : expr->left;
            emit_expr(e, value_expr, operand_type, a, sizeof(a));
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "%s %s nil", a, expr->op);
            else if(operand_slot->is_c_call)
                format(result, sizeof(result), "%s %s NULL", a, expr->op);
            else
                format(result, sizeof(result), "%s.call %s NULL", a, expr->op);
            atom = 0;
            pure = e->pure;
            break;
        }
        /* A call on the left stays in place when the right reads nothing it
         * could change and, in C, which orders no calls, makes no call. */
        int logical = !strcmp(expr->op, "&&") || !strcmp(expr->op, "||");
        e->call_in_place = !logical && !call_can_change(e, expr->right) &&
                           (e->target == ZIR_GO || !expression_calls(e->fn, expr->right));
        emit_expr(e,expr->left,operand_type,a,sizeof(a));
        left_pure = e->pure;
        /* The left side reads first: a call on the right that could change
         * it runs only after the left value is taken. */
        if(expression_calls(e->fn, expr->right) && call_can_change(e, expr->left)) {
            fresh(e, temp);
            declare(e, temp, operand_type, a);
            copy_text(a, sizeof(a), temp);
            left_pure = 1;
        }
        if(!strcmp(expr->op,"&&") || !strcmp(expr->op,"||")) {
            /* A right side that needs no setup statements stays in place,
             * so the target's own && and || keep the short circuit. */
            char *scratch_text = NULL;
            size_t scratch_size = 0;
            FILE *saved_out = e->out;
            int saved_serial = e->serial;
            FILE *scratch = open_memstream(&scratch_text, &scratch_size);
            if(scratch != NULL) {
                e->out = scratch;
                emit_expr(e,expr->right,"bool",b,sizeof(b));
                fclose(scratch);
                e->out = saved_out;
                int inline_right = scratch_size == 0;
                free(scratch_text);
                if(inline_right) {
                    loose_operand(e, expr->left, expr->op, a, sizeof(a));
                    loose_operand(e, expr->right, expr->op, b, sizeof(b));
                    format(result, sizeof(result), "%s %s %s", a, expr->op, b);
                    atom = 0;
                    pure = left_pure && e->pure;
                    break;
                }
                e->serial = saved_serial;
            }
            fresh(e,temp);declare(e,temp,"bool",a);
            line(e,e->target==ZIR_GO?"if %s%s {":"if (%s%s) {",!strcmp(expr->op,"||")?"!":"",temp);e->indent++;
            emit_expr(e,expr->right,"bool",b,sizeof(b));line(e,"%s = %s%s",temp,b,e->target==ZIR_GO?"":";");
            e->indent--;line(e,"}");copy_text(out,size,temp);e->pure=1;return;
        }
        /* The left value is taken or cannot change, so a call on the right
         * runs last and stays in place. */
        e->call_in_place = 1;
        emit_expr(e,expr->right,(!strcmp(expr->op,"<<")||!strcmp(expr->op,">>"))?"s32":operand_type,b,sizeof(b));
        pure = left_pure && e->pure;
        if(slot_compare && !operand_slot->is_c_call &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            if(!strcmp(expr->op, "=="))
                format(result, sizeof(result),
                       "(%s.call == %s.call && %s.context == %s.context)", a, b, a, b);
            else
                format(result, sizeof(result),
                       "(%s.call != %s.call || %s.context != %s.context)", a, b, a, b);
            atom = 0;
            break;
        }
        if(!strcmp(operand_type, "string") && (e->target == ZIR_C || e->target == ZIR_CPP))
            format(result, sizeof(result), "%sStringEqual(%s, %s)", !strcmp(expr->op, "!=") ? "!" : "", a, b);
        else if(width(type) && operation(expr->op)) number(e,type,a,e->fn->exprs[expr->left].type,b,e->fn->exprs[expr->right].type,operation(expr->op),result,sizeof(result));
        else {
            loose_operand(e, expr->left, expr->op, a, sizeof(a));
            loose_operand(e, expr->right, expr->op, b, sizeof(b));
            format(result,sizeof(result),"%s %s %s",a,expr->op,b);
        }
        if(enum_flags_type(e->module, type) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            copy_text(a, sizeof(a), result);
            format(result, sizeof(result), "(%s)(%s)", type, a);
        }
        atom=0;
        break;
    }
    case ZIR_EXPR_UNARY:
        if(!strcmp(expr->op,"-") && e->fn->exprs[expr->right].kind==ZIR_EXPR_INT) {
            literal(e,&e->fn->exprs[expr->right],type,1,result,sizeof(result));pure=1;break;
        }
        if(!strcmp(expr->op, "&"))
            emit_destination(e, expr->right, a, sizeof(a));
        else
            emit_expr(e,expr->right,e->fn->exprs[expr->right].type,a,sizeof(a));
        pure = e->pure;
        if(width(type) && !strcmp(expr->op,"-")) number(e,type,"0",NULL,a,e->fn->exprs[expr->right].type,2,result,sizeof(result));
        else if(width(type) && !strcmp(expr->op,"~")) {
            number(e,type,a,e->fn->exprs[expr->right].type,e->target==ZIR_GO?"^uint64(0)":"UINT64_MAX",NULL,10,result,sizeof(result));
        } else if(!strcmp(expr->op, "&") || !strcmp(expr->op, "*"))
            format(result, sizeof(result), "%s(%s)", expr->op, a);
        else format(result,sizeof(result),"%s%s",expr->op,a);
        break;
    case ZIR_EXPR_CAST: {
        const char *declared_type = type;
        const char *operand_type = e->fn->exprs[expr->right].type;
        int right_pure;
        if(enum_type(e->module, type))
            type = "s32";
        if(e->fn->exprs[expr->right].kind == ZIR_EXPR_INT &&
           width(type) >= 32 && !signed_type(type))
            operand_type = type;
        emit_expr(e,expr->right,operand_type,a,sizeof(a));
        right_pure = e->pure;
        pure = right_pure;
        if(!strcmp(type, "string")) {
            copy_text(result, sizeof(result), a);
        } else if(!strcmp(type,"bool")) {
            format(result,sizeof(result),"%s != %s",a,!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")?"false":"0");
            atom=0;
        } else if(!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")) {
            fresh(e,temp);declare(e,temp,type,"0");
            line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",a);e->indent++;
            line(e,"%s = 1%s",temp,e->target==ZIR_GO?"":";");
            e->indent--;
            line(e, "}");
            copy_text(result, sizeof(result), temp);
        } else if(width(type)) {
            if(canonical(e->fn->exprs[expr->right].type)[0]=='f') {
                if(e->target==ZIR_GO) {
                    /* floatToInt checks the range; the conversion keeps the low bits. */
                    const char *from = canonical(e->fn->exprs[expr->right].type);
                    format(result, sizeof(result), strcmp(from, "float64") ?
                           "%s(floatToInt(float64(%s), %d, %s))" : "%s(floatToInt(%s, %d, %s))",
                           TargetType(type, e->target), a, width(type),
                           signed_type(type) ? "true" : "false");
                } else {
                    /* FloatToInt checks the range; SignedBits reads a signed result. */
                    if(signed_type(type))
                        format(result, sizeof(result), "(%s)SignedBits(FloatToInt(%s, %d, 1), %d)",
                               TargetType(type, e->target), a, width(type), width(type));
                    else
                        format(result, sizeof(result), "(%s)FloatToInt(%s, %d, 0)",
                               TargetType(type, e->target), a, width(type));
                }
            } else number(e,type,a,e->fn->exprs[expr->right].type,"0",NULL,0,result,sizeof(result));
        }
        else {
            char cast_native[ZIR_NAME_MAX];
            slot_native_type(type, e->target, cast_native, sizeof(cast_native));
            if(e->target==ZIR_GO) format(result,sizeof(result),"%s(%s)",cast_native,a);
            else format(result,sizeof(result),"(%s)(%s)",cast_native,a);
        }
        type = declared_type;
        break;
    }
    default: fatal(expr,"unsupported structured expression");
    }
    e->pure = pure;
    {
        /* A folded constant is a single operand, not a binary expression. */
        uint64_t constant;
        if(integer_literal_bits(result, &constant))
            atom = 1;
    }
    /* A consumer that runs nothing after this expression takes its calls in
     * place; otherwise they are captured below, in order. */
    int in_place = call_in_place &&
                   (e->minify || strlen(result) <= ZIR_INLINE_MAX) &&
                   (e->target == ZIR_GO || !ArrayElementType(type, NULL, 0, NULL));
    if(in_place || folds_text(e, result, type)) {
        /* declare() applies this cast for named enum types; inlined text has
         * to carry it so Go sees matching operand types. An explicit cast
         * keeps it even on a bare name: returning an s32 as an enum is not
         * an implicit conversion in C++ or Go. */
        if((!plain_identifier(result) || expr->kind == ZIR_EXPR_CAST) &&
           enum_type(e->module, type)) {
            const char *scalar = TargetType(type, e->target);
            char resolved[ZIR_NAME_MAX * 2];
            if(scalar == NULL) {
                e->resolve(e->context, type, resolved, sizeof(resolved));
                scalar = resolved;
            }
            if(e->target == ZIR_GO)
                format(out, size, "%s(%s)", scalar, result);
            else
                format(out, size, "((%s)(%s))", scalar, result);
        } else if(atom) {
            copy_text(out, size, result);
        } else {
            format(out, size, "(%s)", result);
        }
        e->pure = plain_identifier(result) ? 1 : pure;
        return;
    }
    fresh(e,temp);declare(e,temp,type,result);copy_text(out,size,temp);
    e->pure = 1;
}
