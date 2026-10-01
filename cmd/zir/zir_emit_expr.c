#include "zir_emit_internal.h"

typedef struct EmitMapCallBuffers {
    char map[ZIR_TEXT_MAX];
    char argument[ZIR_TEXT_MAX];
    char initializer[ZIR_TEXT_MAX];
} EmitMapCallBuffers;

static void
emit_map_call(Emitter *e, const ZirExpr *expr, char *out, size_t size)
{
    if(e->target != ZIR_GO)
        fatal(expr, "map operations require the Go target");
    EmitMapCallBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    int first = expr->first_child;
    int second = e->fn->exprs[first].next_sibling;
    int third = second >= 0 ? e->fn->exprs[second].next_sibling : -1;
    const char *map_type = e->fn->exprs[first].type;
    char key_type[ZIR_NAME_MAX], value_type[ZIR_NAME_MAX];
    char map_name[ZIR_NAME_MAX], key_name[ZIR_NAME_MAX], value_name[ZIR_NAME_MAX];
    char result[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    if(!MapTypePartsAtUse(e->module, map_type, key_type, sizeof(key_type),
                     value_type, sizeof(value_type)))
        fatal(expr, "invalid Map operation");
    int init = !strcmp(expr->name, "MapInit");
    int set = !strcmp(expr->name, "MapSet");
    fresh(e, map_name);
    if(init || set) {
        emit_destination(e, first, buffers->map, sizeof(buffers->map));
        char pointer_type[ZIR_NAME_MAX * 2];
        format(pointer_type, sizeof(pointer_type), "*%s", map_type);
        format(buffers->initializer, sizeof(buffers->initializer), "&(%s)", buffers->map);
        declare(e, map_name, pointer_type, buffers->initializer);
        format(buffers->map, sizeof(buffers->map), "(*%s)", map_name);
    } else {
        emit_expr(e, first, map_type, buffers->argument, sizeof(buffers->argument));
        declare(e, map_name, map_type, buffers->argument);
        copy_text(buffers->map, sizeof(buffers->map), map_name);
    }
    if(second >= 0) {
        fresh(e, key_name);
        emit_expr(e, second, key_type, buffers->argument, sizeof(buffers->argument));
        declare(e, key_name, key_type, buffers->argument);
    }
    if(third >= 0) {
        fresh(e, value_name);
        emit_expr(e, third, value_type, buffers->argument, sizeof(buffers->argument));
        declare(e, value_name, value_type, buffers->argument);
    }
    if(init || set) {
        e->resolve(e->context, map_type, mapped, sizeof(mapped));
        line(e, "if %s == nil {", buffers->map);
        e->indent++;
        line(e, "%s = make(%s)", buffers->map, mapped);
        e->indent--;
        line(e, "}");
        if(set)
            line(e, "%s[%s] = %s", buffers->map, key_name, value_name);
    } else if(!strcmp(expr->name, "MapDelete"))
        line(e, "delete(%s, %s)", buffers->map, key_name);
    else if(!strcmp(expr->name, "MapClear"))
        line(e, "clear(%s)", buffers->map);
    else {
        fresh(e, result);
        if(!strcmp(expr->name, "MapContains")) {
            declare(e, result, "bool", "false");
            line(e, "_, %s = %s[%s]", result, buffers->map, key_name);
        } else if(!strcmp(expr->name, "MapLookup")) {
            zero_record(e, expr->type, buffers->initializer, sizeof(buffers->initializer));
            declare(e, result, expr->type, buffers->initializer);
            line(e, "%s.Value, %s.HasValue = %s[%s]", result, result, buffers->map, key_name);
        } else if(!strcmp(expr->name, "MapGet")) {
            format(buffers->initializer, sizeof(buffers->initializer), "%s[%s]", buffers->map, key_name);
            declare(e, result, expr->type, buffers->initializer);
        } else if(!strcmp(expr->name, "MapCount")) {
            format(buffers->initializer, sizeof(buffers->initializer), "int64(len(%s))", buffers->map);
            declare(e, result, "s64", buffers->initializer);
        } else if(!strcmp(expr->name, "MapKeys")) {
            e->resolve(e->context, key_type, mapped, sizeof(mapped));
            format(buffers->initializer, sizeof(buffers->initializer), "make([]%s, 0, len(%s))", mapped, buffers->map);
            declare(e, result, expr->type, buffers->initializer);
            fresh(e, key_name);
            line(e, "for %s := range %s {", key_name, buffers->map);
            e->indent++;
            line(e, "%s = append(%s, %s)", result, result, key_name);
            e->indent--;
            line(e, "}");
        } else
            fatal(expr, "unknown Map operation");
        copy_text(out, size, result);
        e->pure = 1;
        free(buffers);
        return;
    }
    out[0] = '\0';
    e->pure = 0;
    free(buffers);
}
/* Buffers emit_vec_call keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitVecCallBuffers {
    char vector[ZIR_TEXT_MAX];
    char value[ZIR_TEXT_MAX];
    char other[ZIR_TEXT_MAX];
    char index[ZIR_TEXT_MAX];
    char field[ZIR_TEXT_MAX * 2];
    char source[ZIR_TEXT_MAX];
    char low[ZIR_TEXT_MAX];
    char high[ZIR_TEXT_MAX];
    char text[ZIR_TEXT_MAX];
} EmitVecCallBuffers;

static void emit_vec_call(Emitter *e, const ZirExpr *expr, char *out, size_t size);

static void
emit_vec_call_with_buffers(Emitter *e, const ZirExpr *expr, char *out, size_t size, EmitVecCallBuffers *buffers)
{
    int first = expr->first_child;
    int push = !strcmp(expr->name, "VecPush");
    char item_name[ZIR_NAME_MAX], result_name[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    if(first < 0 || !VecElementType(e->module,
        e->fn->exprs[first].type, element, sizeof(element)))
        fatal(expr, "invalid Vec operation");
    emit_destination(e, first, buffers->vector, sizeof(buffers->vector));
    if(!strcmp(expr->name, "VecSwap")) {
        int second = e->fn->exprs[first].next_sibling;
        char tmp[ZIR_NAME_MAX];
        emit_destination(e, second, buffers->other, sizeof(buffers->other));
        fresh(e, tmp);
        declare(e, tmp, e->fn->exprs[first].type, buffers->vector);
        line(e, "%s = %s%s", buffers->vector, buffers->other,
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s = %s%s", buffers->other, tmp,
             e->target == ZIR_GO ? "" : ";");
        out[0] = '\0'; e->pure = 0; return;
    }
    if(push) {
        int second = e->fn->exprs[first].next_sibling;
        if(second < 0) fatal(expr, "VecPush requires a value");
        emit_expr(e, second, element, buffers->value, sizeof(buffers->value));
        fresh(e, item_name);
        declare(e, item_name, element, buffers->value);
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s)",
                 buffers->vector, buffers->vector, item_name);
            line(e, "%s.Count = int64(len(%s.Data))", buffers->vector, buffers->vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", buffers->vector, buffers->vector);
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
                 grown, buffers->vector, buffers->vector, buffers->vector, buffers->vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", buffers->vector, mapped, grown);
            line(e, "(%s).data[(%s).count++] = %s;",
                 buffers->vector, buffers->vector, item_name);
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
        char initializer[ZIR_NAME_MAX * 2];
        zero_record(e, expr->type, initializer, sizeof(initializer));
        fresh(e, result_name);
        declare(e, result_name, expr->type, initializer);
        if(get) {
            int second = e->fn->exprs[first].next_sibling;
            if(second < 0) fatal(expr, "VecGet requires an index");
            emit_expr(e, second, "s64", buffers->index, sizeof(buffers->index));
        }
        if(e->target == ZIR_GO) {
            if(get)
                line(e, "if %s >= 0 && int64(%s) < %s.Count {", buffers->index, buffers->index,
                     buffers->vector);
            else
                line(e, "if %s.Count > 0 {", buffers->vector);
            e->indent++;
            if(get)
                line(e, "%s.Value = %s.Data[%s]", result_name, buffers->vector, buffers->index);
            else {
                line(e, "%s.Count--", buffers->vector);
                line(e, "%s.Value = %s.Data[%s.Count]", result_name, buffers->vector,
                     buffers->vector);
            }
            line(e, "%s.HasValue = true", result_name);
            e->indent--;
            line(e, "}");
        } else {
            if(get)
                line(e, "if (%s >= 0 && %s < (%s).count) {", buffers->index, buffers->index,
                     buffers->vector);
            else
                line(e, "if ((%s).count > 0) {", buffers->vector);
            e->indent++;
            if(get)
                format(buffers->field, sizeof(buffers->field), "(%s).data[%s]", buffers->vector, buffers->index);
            else {
                line(e, "(%s).count--;", buffers->vector);
                format(buffers->field, sizeof(buffers->field), "(%s).data[(%s).count]", buffers->vector,
                       buffers->vector);
            }
            {
                char destination[ZIR_NAME_MAX * 2];
                format(destination, sizeof(destination), "%s.value",
                       result_name);
                assign_value(e, destination, element, buffers->field);
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
        char grown[ZIR_NAME_MAX];
        if(second < 0) fatal(expr, "VecClone requires a source Vec");
        emit_destination(e, second, buffers->source, sizeof(buffers->source));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s.Data...)",
                 buffers->vector, buffers->vector, buffers->source);
            line(e, "%s.Count = int64(len(%s.Data))", buffers->vector, buffers->vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", buffers->vector, buffers->vector);
            line(e, "%s = true", result_name);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            fresh(e, grown);
            line(e, "if ((%s).count > 0) {", buffers->source);
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (%s).count, sizeof(*(%s).data));",
                 grown, buffers->vector, buffers->vector, buffers->vector, buffers->source, buffers->vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", buffers->vector, mapped, grown);
            line(e, "memcpy((%s).data, (%s).data, (size_t)(%s).count * sizeof(*(%s).data));",
                 buffers->vector, buffers->source, buffers->source, buffers->vector);
            line(e, "(%s).count = (%s).count;", buffers->vector, buffers->source);
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
        char view[ZIR_NAME_MAX];
        if(second < 0 || third < 0)
            fatal(expr, "VecSlice requires low and high bounds");
        emit_expr(e, second, "s64", buffers->low, sizeof(buffers->low));
        emit_expr(e, third, "s64", buffers->high, sizeof(buffers->high));
        fresh(e, view);
        if(e->target == ZIR_GO) {
            line(e, "if %s < 0 || %s < %s || %s > %s.Count { panic(\"slice range out of bounds\") }",
                 buffers->low, buffers->high, buffers->low, buffers->high, buffers->vector);
            format(out, size, "%s.Data[%s:%s]", buffers->vector, buffers->low, buffers->high);
        } else {
            line(e, "Slice %s = {(%s).data, (%s).count};", view, buffers->vector,
                 buffers->vector);
            format(out, size, "SliceRange(%s, %s, %s, sizeof(*(%s).data))",
                   view, buffers->low, buffers->high, buffers->vector);
        }
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "BuilderAppend")) {
        int second = e->fn->exprs[first].next_sibling;
        if(second < 0) fatal(expr, "BuilderAppend requires text");
        emit_expr(e, second, "string", buffers->text, sizeof(buffers->text));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s...)",
                 buffers->vector, buffers->vector, buffers->text);
            line(e, "%s.Count = int64(len(%s.Data))", buffers->vector, buffers->vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", buffers->vector, buffers->vector);
            line(e, "%s = true", result_name);
        } else {
            char grown[ZIR_NAME_MAX];
            fresh(e, grown);
            line(e, "if ((%s).length == 0) {", buffers->text);
            e->indent++;
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "} else {");
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (int64_t)(%s).length, sizeof(*(%s).data));",
                 grown, buffers->vector, buffers->vector, buffers->vector, buffers->text, buffers->vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (uint8_t *)%s;", buffers->vector, grown);
            line(e, "memcpy((%s).data + (%s).count, (%s).data, (size_t)(%s).length);",
                 buffers->vector, buffers->vector, buffers->text, buffers->text);
            line(e, "(%s).count += (int64_t)(%s).length;", buffers->vector, buffers->text);
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
                 finished, buffers->vector, buffers->vector);
            line(e, "%s.Data = nil", buffers->vector);
        } else {
            declare(e, finished, "string",
                    e->target == ZIR_CPP ? "{}" : "{NULL, 0}");
            line(e, "%s.data = (%s).count > 0 ? (const char *)(%s).data : \"\";",
                 finished, buffers->vector, buffers->vector);
            line(e, "%s.length = (size_t)(%s).count;", finished, buffers->vector);
            /* The finished string borrows the builder's bytes; the builder
             * detaches without freeing them. */
            line(e, "(%s).data = NULL;", buffers->vector);
        }
        line(e, "%s.%s = 0%s", buffers->vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s.%s = 0%s", buffers->vector,
             e->target == ZIR_GO ? "Count" : "count",
             e->target == ZIR_GO ? "" : ";");
        copy_text(out, size, finished);
        e->pure = 0;
        return;
    }
    if(!strcmp(expr->name, "VecFree")) {
        if(e->target == ZIR_GO)
            line(e, "%s.Data = nil", buffers->vector);
        else {
            line(e, "free((%s).data);", buffers->vector);
            line(e, "(%s).data = NULL;", buffers->vector);
        }
        line(e, "%s.%s = 0%s", buffers->vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
    } else if(e->target == ZIR_GO)
        line(e, "%s.Data = %s.Data[:0]", buffers->vector, buffers->vector);
    line(e, "%s.%s = 0%s", buffers->vector,
         e->target == ZIR_GO ? "Count" : "count",
         e->target == ZIR_GO ? "" : ";");
    out[0] = '\0';
    e->pure = 0;
}

/* Keep collection lowering outside recursive expression lowering: its large
 * target buffers must not increase every nested expression stack frame. */
static void
emit_vec_call(Emitter *e, const ZirExpr *expr, char *out, size_t size)
{
    static _Thread_local EmitVecCallBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitVecCallBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    emit_vec_call_with_buffers(e, expr, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
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

/* A C record value as a compound literal, (Point){.x = 3, .y = 4}, or,
 * initializing a declaration, a brace list: {.x = 3, .y = 4} or, for an
 * array, {1, 2, 3}. Unnamed fields are zero either way. C++17 has no
 * designated initializers, so its records keep field assignments; C arrays
 * are not values, so array elements and fields keep them too. Values with
 * calls use ordered stores so earlier reads happen before later calls. */
static int
c_brace_list(Emitter *e, const ZirExpr *expr, const char *type, int braced,
             char *out, size_t size)
{
    int is_array = ArrayElementType(type, NULL, 0, NULL) != 0;
    const ZirType *record = is_array ? NULL : field_record(e->module, type);
    char value[ZIR_TEXT_MAX], text[ZIR_TEXT_MAX], native[ZIR_NAME_MAX * 2];
    size_t used = 0;
    if(e->target == ZIR_GO || (is_array && !braced) ||
       (!is_array && (e->target != ZIR_C || record == NULL)) ||
       TypeHasZeroArray(e->module, type))
        return 0;
    for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling)
        if(expression_calls(e->fn, e->fn->exprs[child].right) ||
           ArrayElementType(e->fn->exprs[child].type, NULL, 0, NULL) ||
           VecElementType(e->module, e->fn->exprs[child].type, NULL, 0))
            return 0;
    if(!is_array && !braced) {
        e->resolve(e->context, type, native, sizeof(native));
        used = (size_t)format(text, sizeof(text), "(%s)", native);
    }
    used += (size_t)format(text + used, sizeof(text) - used, "{");
    for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
        const ZirExpr *entry = &e->fn->exprs[child];
        char field[ZIR_NAME_MAX] = "", plain[ZIR_TEXT_MAX];
        /* Inside a declaration's brace list a nested value is braces too;
         * inside a compound literal it is its own compound literal. */
        e->braced_initializer = braced &&
            e->fn->exprs[entry->right].kind == ZIR_EXPR_COMPOUND;
        emit_expr(e, entry->right, entry->type, value, sizeof(value));
        e->braced_initializer = 0;
        if(!is_array)
            TargetFieldName(record, e->target, entry->name, field, sizeof(field));
        const char *shown = bare(value, plain, sizeof(plain));
        /* A list too long for one expression, such as a big table, is
         * stored element by element instead. */
        if(used + strlen(shown) + strlen(field) + 16 >= sizeof(text))
            return 0;
        used += (size_t)format(text + used, used < sizeof(text) ? sizeof(text) - used : 0,
                               "%s%s%s%s", child == expr->first_child ? "" : ", ",
                               is_array ? "" : ".", is_array ? "" : field,
                               is_array ? "" : " = ");
        used += (size_t)format(text + used, used < sizeof(text) ? sizeof(text) - used : 0,
                               "%s", shown);
    }
    /* An empty value zeroes every field; C99 has no empty braces. */
    if(expr->first_child < 0)
        used += (size_t)format(text + used, sizeof(text) - used, "0");
    if(used + 2 >= sizeof(text))
        fatal(expr, "record value is too long");
    copy_text(text + used, sizeof(text) - used, "}");
    copy_text(out, size, text);
    return 1;
}

/* The one member of a plain enum holding value, by the target's own
 * constant name: Shape_CIRCLE in C, ShapeCIRCLE in Go. Flags and values
 * two members share keep the number. */
static int
enum_constant(Emitter *e, const char *type, const char *value, char *out, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirType *enumeration = FindType(e->module, type, &owner);
    char member[ZIR_NAME_MAX], found[ZIR_NAME_MAX], qualified[ZIR_NAME_MAX * 2];
    char resolved[ZIR_TEXT_MAX], *end;
    int64_t wanted, candidate;
    int matches = 0;
    if(enumeration == NULL || !enumeration->is_enum || enumeration->is_enum_flags)
        return 0;
    errno = 0;
    wanted = strtoll(value, &end, 0);
    if(errno != 0 || end == value || *end != '\0')
        return 0;
    for(const char *cursor = enumeration->body; *cursor;) {
        size_t length = 0;
        while(*cursor == ',' || isspace((unsigned char)*cursor)) cursor++;
        while((isalnum((unsigned char)cursor[length]) || cursor[length] == '_') &&
              length + 1 < sizeof(member))
            length++;
        if(length == 0)
            break;
        memcpy(member, cursor, length);
        member[length] = '\0';
        if(EnumMemberValue(enumeration, member, &candidate) && candidate == wanted) {
            copy_text(found, sizeof(found), member);
            matches++;
        }
        while(*cursor && *cursor != ',' && *cursor != '\n') cursor++;
    }
    if(matches != 1)
        return 0;
    format(qualified, sizeof(qualified), "%s.%s", type, found);
    e->resolve(e->context, qualified, resolved, sizeof(resolved));
    if(!strcmp(resolved, qualified) || strchr(resolved, '.') != NULL ||
       !plain_identifier(resolved))
        return 0;
    copy_text(out, size, resolved);
    return 1;
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
 * n % 15 == 0 rather than (n % 15) == 0. An && inside || keeps them, as
 * C compilers warn about that grouping under -Wall. */
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
    if(parent == 1 && inner == 2)
        return;
    copy_text(text, size, bare(text, plain, sizeof(plain)));
}
/* Buffers emit_expr keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitExprBuffers {
    char a[ZIR_TEXT_MAX];
    char b[ZIR_TEXT_MAX];
    char result[ZIR_TEXT_MAX];
    char source[ZIR_TEXT_MAX];
    char low[ZIR_TEXT_MAX];
    char high[ZIR_TEXT_MAX];
} EmitExprBuffers;

void emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size);

static void
emit_expr_with_buffers(Emitter *e, int index, const char *expected, char *out, size_t size, EmitExprBuffers *buffers)
{
    const ZirExpr *expr=&e->fn->exprs[index];
    const char *type=canonical(expr->type);
    char temp[ZIR_NAME_MAX];
    int pure=0;
    int call_in_place = e->call_in_place;
    int braced = e->braced_initializer;
    e->braced_initializer = 0;
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
        native_size_expression(e, expr->name, buffers->a, sizeof(buffers->a));
        if(e->target == ZIR_GO)
            format(buffers->result, sizeof(buffers->result), "%s(%s)", TargetType(type, e->target), buffers->a);
        else
            copy_text(buffers->result, sizeof(buffers->result), buffers->a);
        pure = 1;
        break;
    case ZIR_EXPR_COMPOUND: {
        /* Array literals may exceed the expression scratch buffer. Emit
         * their ordered element stores, as the C/C++ path already does,
         * instead of constructing one unbounded Go expression. */
        if(e->target == ZIR_GO && ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare(e, temp, type, NULL);
            int ordinal = 0;
            for(int child = expr->first_child; child >= 0;
                child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                emit_expr(e, entry->right, entry->type,
                          buffers->a, sizeof(buffers->a));
                format(buffers->b, sizeof(buffers->b), "%s[%d]",
                       temp, ordinal++);
                assign_value(e, buffers->b, entry->type, buffers->a);
            }
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        /* Go spells a record value as one literal, Point{X: 3, Y: 4}:
         * its zero value is T{}, so unnamed fields keep their zero. */
        if(e->target == ZIR_GO && !expression_calls(e->fn, index)) {
            size_t used;
            pure = 1;
            e->resolve(e->context, type, buffers->b, sizeof(buffers->b));
            used = (size_t)format(buffers->result, sizeof(buffers->result), "%s{", buffers->b);
            for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                char field_name[ZIR_NAME_MAX];
                emit_expr(e, entry->right, entry->type, buffers->a, sizeof(buffers->a));
                pure &= e->pure;
                go_field_ident(entry->name, field_name, sizeof(field_name));
                used += (size_t)format(buffers->result + used, used < sizeof(buffers->result) ? sizeof(buffers->result) - used : 0,
                                       "%s%s%s%s", child == expr->first_child ? "" : ", ",
                                       field_name, ": ", buffers->a);
            }
            if(used + 2 >= sizeof(buffers->result))
                fatal(expr, "record value is too long");
            copy_text(buffers->result + used, sizeof(buffers->result) - used, "}");
            break;
        }
        if(c_brace_list(e, expr, type, braced, out, size)) {
            e->pure = 1;
            return;
        }
        if(ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare(e, temp, type, NULL);
            int ordinal = 0;
            for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                emit_expr(e, entry->right, entry->type, buffers->a, sizeof(buffers->a));
                format(buffers->b, sizeof(buffers->b), "%s[%d]", temp, ordinal++);
                assign_value(e, buffers->b, entry->type, buffers->a);
            }
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        zero_record(e, type, buffers->a, sizeof(buffers->a));
        fresh(e, temp);
        declare(e, temp, type, buffers->a);
        for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
            const ZirExpr *field = &e->fn->exprs[child];
            char field_name[ZIR_NAME_MAX];
            if(e->target == ZIR_GO)
                go_field_ident(field->name, field_name, sizeof(field_name));
            else
                TargetFieldName(field_record(e->module, type), e->target,
                                field->name, field_name, sizeof(field_name));
            emit_expr(e, field->right, field->type, buffers->a, sizeof(buffers->a));
            format(buffers->b, sizeof(buffers->b), "%s.%s", temp, field_name);
            assign_value(e, buffers->b, field->type, buffers->a);
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
            emit_destination(e, expr->left, buffers->a, sizeof(buffers->a));
        else
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, buffers->a, sizeof(buffers->a));
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
            format(buffers->result, sizeof(buffers->result), "%d", capacity);
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
                    format(buffers->result, sizeof(buffers->result), "(*%s)(nil)", mapped);
                } else
                    copy_text(buffers->result, sizeof(buffers->result), "NULL");
            } else if(e->target == ZIR_GO)
                format(buffers->result, sizeof(buffers->result), "&(%s)[0]", buffers->a);
            else
                copy_text(buffers->result, sizeof(buffers->result), buffers->a);
            pure = base_pure;
            break;
        }
        if((!strcmp(base_type, "string") ||
            SliceElementType(base_type, NULL, 0)) &&
           !strcmp(expr->name, "count")) {
            if(e->target == ZIR_GO)
                format(buffers->result, sizeof(buffers->result), "int64(len(%s))", buffers->a);
            else
                format(buffers->result, sizeof(buffers->result), "(int64_t)(%s).length", buffers->a);
            pure = base_pure;
            break;
        }
        emit_field_path(e->module, e->target, base_type, expr->name,
                        buffers->a, buffers->result, sizeof(buffers->result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, buffers->result);
            clear_owned_value(e, buffers->result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = base_pure;
        break;
    }
    case ZIR_EXPR_SLICE: {
        const char *base_type = e->fn->exprs[expr->left].type;
        char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
        char view[ZIR_NAME_MAX];
        int capacity = 0;
        int base_pure;
        int low_pure = 1;
        int high_pure = 1;
        if(!strcmp(base_type, "string")) {
            emit_expr(e, expr->left, "string", buffers->source, sizeof(buffers->source));
            base_pure = e->pure;
            /* Go checks a string slice's bounds itself; constant bounds it
             * would reject at compile time keep the explicit check. */
            if(e->target == ZIR_GO && plain_identifier(buffers->source) &&
               !expression_calls(e->fn, expr->right) &&
               !expression_calls(e->fn, expr->third)) {
                uint64_t low_bits = 0, high_bits = 0;
                int low_constant = 1, high_constant = 0;
                buffers->low[0] = buffers->high[0] = '\0';
                if(expr->right >= 0) {
                    emit_expr(e, expr->right, "s64", buffers->low, sizeof(buffers->low));
                    low_pure = e->pure;
                    low_constant = integer_literal_bits(buffers->low, &low_bits);
                }
                if(expr->third >= 0) {
                    emit_expr(e, expr->third, "s64", buffers->high, sizeof(buffers->high));
                    high_pure = e->pure;
                    high_constant = integer_literal_bits(buffers->high, &high_bits);
                }
                if(!(low_constant && buffers->low[0] == '-') && !(high_constant && buffers->high[0] == '-') &&
                   !(low_constant && high_constant && low_bits > high_bits)) {
                    pure = base_pure && low_pure && high_pure;
                    format(buffers->result, sizeof(buffers->result), "%s[%s:%s]", buffers->source, buffers->low, buffers->high);
                    break;
                }
                fresh(e, view);
                line(e, "%s := %s", view, buffers->source);
                if(!buffers->low[0]) copy_text(buffers->low, sizeof(buffers->low), "0");
                if(!buffers->high[0]) format(buffers->high, sizeof(buffers->high), "int64(len(%s))", view);
                line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) {", buffers->low, buffers->high, buffers->low, buffers->high, view);
                e->indent++;
                line(e, "panic(\"string range out of bounds\")");
                e->indent--;
                line(e, "}");
                pure = base_pure && low_pure && high_pure;
                format(buffers->result, sizeof(buffers->result), "%s[%s:%s]", view, buffers->low, buffers->high);
                break;
            }
            fresh(e, view);
            if(e->target == ZIR_GO)
                line(e, "%s := %s", view, buffers->source);
            else
                line(e, "String %s = %s;", view, buffers->source);
            if(expr->right >= 0) {
                emit_expr(e, expr->right, "s64", buffers->low, sizeof(buffers->low));
                low_pure = e->pure;
            } else copy_text(buffers->low, sizeof(buffers->low), "0");
            if(expr->third >= 0) {
                emit_expr(e, expr->third, "s64", buffers->high, sizeof(buffers->high));
                high_pure = e->pure;
            } else format(buffers->high, sizeof(buffers->high), e->target == ZIR_GO ?
                          "len(%s)" : "%s.length", view);
            pure = base_pure && low_pure && high_pure;
            if(e->target == ZIR_GO) {
                line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) { panic(\"string range out of bounds\") }",
                     buffers->low, buffers->high, buffers->low, buffers->high, view);
                format(buffers->result, sizeof(buffers->result), "%s[%s:%s]", view, buffers->low, buffers->high);
            } else {
                format(buffers->result, sizeof(buffers->result), "StringRange(%s, (int64_t)%s, (int64_t)%s)",
                       view, buffers->low, buffers->high);
            }
            break;
        }
        int array = ArrayElementType(base_type, element, sizeof(element), &capacity);
        if(array) {
            emit_destination(e, expr->left, buffers->source, sizeof(buffers->source));
        } else {
            SliceElementType(base_type, element, sizeof(element));
            emit_expr(e, expr->left, base_type, buffers->source, sizeof(buffers->source));
        }
        base_pure = e->pure;
        /* A fixed array's length is its capacity, so Go's own bounds check
         * on arr[low:high:high] is exactly Ziran's. */
        if(array && capacity > 0 && e->target == ZIR_GO) {
            if(expr->right >= 0) {
                emit_expr(e, expr->right, "s64", buffers->low, sizeof(buffers->low));
                low_pure = e->pure;
            } else copy_text(buffers->low, sizeof(buffers->low), "0");
            if(expr->third >= 0) {
                emit_expr(e, expr->third, "s64", buffers->high, sizeof(buffers->high));
                high_pure = e->pure;
            } else format(buffers->high, sizeof(buffers->high), "%d", capacity);
            pure = base_pure && low_pure && high_pure;
            /* Go rejects constant bounds outside the array at compile time;
             * Ziran stops at run time, so those keep the checked form. */
            uint64_t low_bits = 0, high_bits = 0;
            int low_constant = integer_literal_bits(buffers->low, &low_bits);
            int high_constant = integer_literal_bits(buffers->high, &high_bits);
            if((low_constant && (buffers->low[0] == '-' || low_bits > (uint64_t)capacity)) ||
               (high_constant && (buffers->high[0] == '-' || high_bits > (uint64_t)capacity)) ||
               (low_constant && high_constant && low_bits > high_bits)) {
                fresh(e, view);
                line(e, "%s := %s[:]", view, buffers->source);
                line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) {", buffers->low, buffers->high, buffers->low, buffers->high, view);
                e->indent++;
                line(e, "panic(\"slice range out of bounds\")");
                e->indent--;
                line(e, "}");
                format(buffers->result, sizeof(buffers->result), "%s[%s:%s:%s]", view, buffers->low, buffers->high, buffers->high);
                break;
            }
            if(plain_identifier(buffers->high) || high_constant)
                format(buffers->result, sizeof(buffers->result), "%s[%s:%s:%s]", buffers->source, buffers->low, buffers->high, buffers->high);
            else {
                /* Go takes any integer type as an index, so the bound keeps
                 * its own type; an s32 field is not assignable to int64. */
                const char *high_type = e->fn->exprs[expr->third].type;
                fresh(e, temp);
                declare(e, temp, width(high_type) ? high_type : "s64", buffers->high);
                format(buffers->result, sizeof(buffers->result), "%s[%s:%s:%s]", buffers->source, buffers->low, temp, temp);
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
        } else if(e->target == ZIR_GO && !array && plain_identifier(buffers->source) &&
                  !expression_calls(e->fn, expr->right) &&
                  !expression_calls(e->fn, expr->third)) {
            /* A named slice is its own view while nothing can reassign it. */
            copy_text(view, sizeof(view), buffers->source);
        } else if(e->target == ZIR_GO) {
            line(e, "%s := %s[:]", view, buffers->source);
        } else if(array) {
            line(e, "Slice %s = {%s, %d};", view, buffers->source, capacity);
        } else {
            line(e, "Slice %s = %s;", view, buffers->source);
        }
        if(expr->right >= 0) {
            emit_expr(e, expr->right, "s64", buffers->low, sizeof(buffers->low));
            low_pure = e->pure;
        }
        else
            copy_text(buffers->low, sizeof(buffers->low), "0");
        if(expr->third >= 0) {
            emit_expr(e, expr->third, "s64", buffers->high, sizeof(buffers->high));
            high_pure = e->pure;
        }
        else
            format(buffers->high, sizeof(buffers->high), e->target == ZIR_GO ? "int64(len(%s))" : "%s.length", view);
        pure = base_pure && low_pure && high_pure;
        if(e->target == ZIR_GO) {
            /* A bound keeps its own integer type, such as s32, and Go
             * compares only equal types, so the check widens each. */
            line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) {", buffers->low, buffers->high, buffers->low, buffers->high, view);
            e->indent++;
            line(e, "panic(\"slice range out of bounds\")");
            e->indent--;
            line(e, "}");
            format(buffers->result, sizeof(buffers->result), "%s[%s:%s:%s]", view, buffers->low, buffers->high, buffers->high);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            format(buffers->result, sizeof(buffers->result), "SliceRange(%s, (int64_t)%s, (int64_t)%s, sizeof(%s))",
                   view, buffers->low, buffers->high, mapped);
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        const char *base_type = e->fn->exprs[expr->left].type;
        int capacity = 0;
        int base_pure;
        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, buffers->a, sizeof(buffers->a));
        else
            emit_expr(e, expr->left, base_type, buffers->a, sizeof(buffers->a));
        base_pure = e->pure;
        emit_expr(e, expr->right, "s32", buffers->b, sizeof(buffers->b));
        pure = base_pure && e->pure;
        int fixed_array = ArrayElementType(base_type, NULL, 0, &capacity);
        if(VecElementType(e->module, base_type, NULL, 0) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            format(buffers->result, sizeof(buffers->result),
                   "ZIRAN_VEC_INDEX((%s).data, (%s).count, %s)", buffers->a, buffers->a, buffers->b);
        } else if(VecElementType(e->module, base_type, NULL, 0)) {
            format(buffers->result, sizeof(buffers->result), "(%s).Data[%s]", buffers->a, buffers->b);
        } else if(SliceElementType(base_type, NULL, 0)) {
            slice_index(e, base_type, buffers->a, buffers->b, buffers->result, sizeof(buffers->result));
        } else if(e->target == ZIR_GO && base_type[0] == '*') {
            go_pointer_index(e, buffers->a, buffers->b, buffers->result, sizeof(buffers->result));
        } else if(!strcmp(base_type, "string")) {
            if(e->target == ZIR_GO)
                format(buffers->result, sizeof(buffers->result), "%s[%s]", buffers->a, buffers->b);
            else
                format(buffers->result, sizeof(buffers->result), "(uint8_t)ZIRAN_INDEX(%s.data, %s.length, %s)", buffers->a, buffers->a, buffers->b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity == 0) {
            format(buffers->result, sizeof(buffers->result), "ZIRAN_EMPTY_INDEX(%s, %s)", buffers->a, buffers->b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity > 0) {
            /* Fixed-capacity arrays are bounds-checked in debug builds. */
            format(buffers->result, sizeof(buffers->result), "ZIRAN_INDEX(%s, %d, %s)",
                   buffers->a, capacity, buffers->b);
        } else {
            postfix_base(buffers->a, buffers->source, sizeof(buffers->source));
            format(buffers->result, sizeof(buffers->result), "%s[%s]", buffers->source, buffers->b);
        }
        break;
    }
    case ZIR_EXPR_COMPILE_TIME:
        copy_text(buffers->result, sizeof(buffers->result), "false");
        pure = 1;
        break;
    case ZIR_EXPR_IDENT:
        if(expr->is_function_value) {
            emit_function_value(e, index, buffers->result, sizeof(buffers->result));
            break;
        }
        if(!strcmp(expr->name, "null")) {
            copy_text(buffers->result, sizeof(buffers->result), e->target == ZIR_GO ? "nil" :
                      e->target == ZIR_CPP ? "nullptr" : "((void *)0)");
            pure = 1;
            break;
        }
        resolve(e, expr->name, buffers->result, sizeof(buffers->result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, buffers->result);
            clear_owned_value(e, buffers->result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = 1;
        break;
    case ZIR_EXPR_STRING:
        string_literal(expr, e->target, buffers->a, sizeof(buffers->a));
        if(e->target == ZIR_C || e->target == ZIR_CPP)
            format(buffers->result, sizeof(buffers->result), "StringLiteral(%s)", buffers->a);
        else
            copy_text(buffers->result, sizeof(buffers->result), buffers->a);
        pure = 1;
        break;
    case ZIR_EXPR_INT:
        literal(e,expr,type,0,buffers->result,sizeof(buffers->result));
        if(enum_constant(e, type, expr->text, buffers->result, sizeof(buffers->result))) {
            pure = 1;
            break;
        }
        if(e->target == ZIR_CPP && enum_type(e->module, type)) {
            copy_text(buffers->a, sizeof(buffers->a), buffers->result);
            char native[ZIR_NAME_MAX * 2];
            e->resolve(e->context, type, native, sizeof(native));
            format(buffers->result, sizeof(buffers->result), "(%s)(%s)", native, buffers->a);
        }
        pure=1;
        break;
    case ZIR_EXPR_FLOAT: {
        copy_text(buffers->result,sizeof(buffers->result),expr->text);size_t n=strlen(buffers->result);
        if(n && (buffers->result[n-1]=='f' || buffers->result[n-1]=='F')) buffers->result[n-1]=0;
        pure = 1;
        break;
    }
    case ZIR_EXPR_CALL:
        if(MapPrimitiveName(expr->name)) {
            emit_map_call(e, expr, out, size);
            return;
        }
        if(!strcmp(expr->name, "zi_new") && expr->type[0] == '*') {
            /* New(T): zeroed storage for one T. Go's collector frees it. */
            const char *target = skip_ws(expr->type + 1);
            char mapped[ZIR_NAME_MAX * 2];
            const char *native = TargetType(target, e->target);
            if(native != NULL) copy_text(mapped, sizeof(mapped), native);
            else e->resolve(e->context, target, mapped, sizeof(mapped));
            if(e->target == ZIR_GO)
                format(out, size, "new(%s)", mapped);
            else
                format(out, size, "((%s *)calloc(1, sizeof(%s)))", mapped, mapped);
            e->pure = 0;
            return;
        }
        if(!strcmp(expr->name, "zi_free") && expr->first_child >= 0) {
            emit_expr(e, expr->first_child, e->fn->exprs[expr->first_child].type,
                      buffers->a, sizeof(buffers->a));
            if(e->target == ZIR_GO)
                line(e, "_ = %s", buffers->a);
            else
                line(e, "free(%s);", buffers->a);
            out[0] = '\0';
            e->pure = 0;
            return;
        }
        if(!strcmp(expr->name, "TextView")) {
            if(expr->first_child < 0 ||
               e->fn->exprs[expr->first_child].next_sibling >= 0)
                fatal(expr, "TextView requires one []u8 argument");
            emit_expr(e, expr->first_child, "[]u8", buffers->a, sizeof(buffers->a));
            if(e->target == ZIR_GO) {
                format(out, size, "string(%s)", buffers->a);
            } else {
                fresh(e, temp);
                line(e, "Slice %s = %s;", temp, buffers->a);
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
            emit_call(e, expr, temp, buffers->result, sizeof(buffers->result));
            line(e, "%s;", buffers->result);
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        emit_call(e, expr, NULL, buffers->result, sizeof(buffers->result));
        if(!strcmp(type,"void")) {line(e,"%s%s",buffers->result,e->target==ZIR_GO?"":";");out[0]=0;e->pure=0;return;}
        break;
    case ZIR_EXPR_CONDITIONAL:
        emit_expr(e,expr->left,"bool",buffers->a,sizeof(buffers->a));fresh(e,temp);
        const ZirType *declared = FindType(e->module, type, NULL);
        if(declared != NULL && declared->is_procedure_type) {
            if(e->target == ZIR_C || e->target == ZIR_CPP)
                format(buffers->b, sizeof(buffers->b), "(%s){0}", type);
            else
                copy_text(buffers->b, sizeof(buffers->b), e->target == ZIR_GO ? "nil" : "null");
        } else if(record_type(e->module, type))
            zero_record(e, type, buffers->b, sizeof(buffers->b));
        else
            copy_text(buffers->b, sizeof(buffers->b), zero_value(type, e->target));
        declare(e, temp, type, buffers->b);
        line(e, e->target == ZIR_GO ? "if %s {" : "if (%s) {", buffers->a);
        e->indent++;
        emit_expr(e, expr->right, type, buffers->b, sizeof(buffers->b));
        assign_value(e, temp, type, buffers->b);
        e->indent--;
        line(e, "} else {");
        e->indent++;
        emit_expr(e, expr->third, type, buffers->b, sizeof(buffers->b));
        assign_value(e, temp, type, buffers->b);
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
            emit_expr(e, value_expr, operand_type, buffers->a, sizeof(buffers->a));
            if(e->target == ZIR_GO)
                format(buffers->result, sizeof(buffers->result), "%s %s nil", buffers->a, expr->op);
            else if(operand_slot->is_c_call)
                format(buffers->result, sizeof(buffers->result), "%s %s NULL", buffers->a, expr->op);
            else
                format(buffers->result, sizeof(buffers->result), "%s.call %s NULL", buffers->a, expr->op);
            atom = 0;
            pure = e->pure;
            break;
        }
        /* A call on the left stays in place when the right reads nothing it
         * could change and, in C, which orders no calls, makes no call. */
        int logical = !strcmp(expr->op, "&&") || !strcmp(expr->op, "||");
        /* 32-bit C compilers lower wide comparisons through register pairs.
         * Load checked addresses separately before comparing those pairs:
         * native 8c aborts on two indexed wide fields in one comparison. */
        int wide_compare = !strcmp(type, "bool") && width(operand_type) == 64 &&
                           (e->target == ZIR_C || e->target == ZIR_CPP);
        uint64_t literal_bits;
        e->call_in_place = !logical && !call_can_change(e, expr->right) &&
                           (e->target == ZIR_GO || !expression_calls(e->fn, expr->right));
        emit_expr(e,expr->left,operand_type,buffers->a,sizeof(buffers->a));
        left_pure = e->pure;
        /* The left side reads first: a call on the right that could change
         * it runs only after the left value is taken. */
        if((wide_compare && !plain_identifier(buffers->a) &&
            !integer_literal_bits(buffers->a, &literal_bits)) ||
           (expression_calls(e->fn, expr->right) && call_can_change(e, expr->left))) {
            fresh(e, temp);
            declare(e, temp, operand_type, buffers->a);
            copy_text(buffers->a, sizeof(buffers->a), temp);
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
                emit_expr(e,expr->right,"bool",buffers->b,sizeof(buffers->b));
                fclose(scratch);
                e->out = saved_out;
                int inline_right = scratch_size == 0;
                free(scratch_text);
                if(inline_right) {
                    loose_operand(e, expr->left, expr->op, buffers->a, sizeof(buffers->a));
                    loose_operand(e, expr->right, expr->op, buffers->b, sizeof(buffers->b));
                    format(buffers->result, sizeof(buffers->result), "%s %s %s", buffers->a, expr->op, buffers->b);
                    atom = 0;
                    pure = left_pure && e->pure;
                    break;
                }
                e->serial = saved_serial;
            }
            fresh(e,temp);declare(e,temp,"bool",buffers->a);
            line(e,e->target==ZIR_GO?"if %s%s {":"if (%s%s) {",!strcmp(expr->op,"||")?"!":"",temp);e->indent++;
            emit_expr(e,expr->right,"bool",buffers->b,sizeof(buffers->b));line(e,"%s = %s%s",temp,buffers->b,e->target==ZIR_GO?"":";");
            e->indent--;line(e,"}");copy_text(out,size,temp);e->pure=1;return;
        }
        /* The left value is taken or cannot change, so a call on the right
         * runs last and stays in place. */
        e->call_in_place = 1;
        emit_expr(e,expr->right,(!strcmp(expr->op,"<<")||!strcmp(expr->op,">>"))?"s32":operand_type,buffers->b,sizeof(buffers->b));
        if(wide_compare && !plain_identifier(buffers->b) &&
           !integer_literal_bits(buffers->b, &literal_bits)) {
            fresh(e, temp);
            declare(e, temp, operand_type, buffers->b);
            copy_text(buffers->b, sizeof(buffers->b), temp);
            e->pure = 1;
        }
        pure = left_pure && e->pure;
        if(slot_compare && !operand_slot->is_c_call &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            if(!strcmp(expr->op, "=="))
                format(buffers->result, sizeof(buffers->result),
                       "(%s.call == %s.call && %s.context == %s.context)", buffers->a, buffers->b, buffers->a, buffers->b);
            else
                format(buffers->result, sizeof(buffers->result),
                       "(%s.call != %s.call || %s.context != %s.context)", buffers->a, buffers->b, buffers->a, buffers->b);
            atom = 0;
            break;
        }
        if(!strcmp(operand_type, "string") && (e->target == ZIR_C || e->target == ZIR_CPP))
            format(buffers->result, sizeof(buffers->result), "%sStringEqual(%s, %s)", !strcmp(expr->op, "!=") ? "!" : "", buffers->a, buffers->b);
        else if(width(type) && operation(expr->op)) number(e,type,buffers->a,e->fn->exprs[expr->left].type,buffers->b,e->fn->exprs[expr->right].type,operation(expr->op),buffers->result,sizeof(buffers->result));
        else {
            loose_operand(e, expr->left, expr->op, buffers->a, sizeof(buffers->a));
            loose_operand(e, expr->right, expr->op, buffers->b, sizeof(buffers->b));
            format(buffers->result,sizeof(buffers->result),"%s %s %s",buffers->a,expr->op,buffers->b);
        }
        if(enum_flags_type(e->module, type) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            copy_text(buffers->a, sizeof(buffers->a), buffers->result);
            format(buffers->result, sizeof(buffers->result), "(%s)(%s)", type, buffers->a);
        }
        atom=0;
        break;
    }
    case ZIR_EXPR_UNARY:
        if(!strcmp(expr->op,"-") && e->fn->exprs[expr->right].kind==ZIR_EXPR_INT) {
            literal(e,&e->fn->exprs[expr->right],type,1,buffers->result,sizeof(buffers->result));pure=1;break;
        }
        if(!strcmp(expr->op, "&"))
            emit_destination(e, expr->right, buffers->a, sizeof(buffers->a));
        else
            emit_expr(e,expr->right,e->fn->exprs[expr->right].type,buffers->a,sizeof(buffers->a));
        pure = e->pure;
        if(width(type) && !strcmp(expr->op,"-")) number(e,type,"0",NULL,buffers->a,e->fn->exprs[expr->right].type,2,buffers->result,sizeof(buffers->result));
        else if(width(type) && !strcmp(expr->op,"~")) {
            number(e,type,buffers->a,e->fn->exprs[expr->right].type,e->target==ZIR_GO?"^uint64(0)":"UINT64_MAX",NULL,10,buffers->result,sizeof(buffers->result));
        } else if(!strcmp(expr->op, "&") || !strcmp(expr->op, "*"))
            format(buffers->result, sizeof(buffers->result), "%s(%s)", expr->op, buffers->a);
        else format(buffers->result,sizeof(buffers->result),"%s%s",expr->op,buffers->a);
        break;
    case ZIR_EXPR_CAST: {
        const char *declared_type = type;
        const char *operand_type = e->fn->exprs[expr->right].type;
        int right_pure;
        /* A cast to the enum a value already has changes nothing. */
        if(enum_type(e->module, type) && !strcmp(canonical(operand_type), type)) {
            emit_expr(e, expr->right, operand_type, out, size);
            return;
        }
        if(enum_type(e->module, type))
            type = "s32";
        if(e->fn->exprs[expr->right].kind == ZIR_EXPR_INT &&
           width(type) >= 32 && !signed_type(type))
            operand_type = type;
        emit_expr(e,expr->right,operand_type,buffers->a,sizeof(buffers->a));
        right_pure = e->pure;
        pure = right_pure;
        if(!strcmp(type, "string")) {
            copy_text(buffers->result, sizeof(buffers->result), buffers->a);
        } else if(!strcmp(type,"bool")) {
            format(buffers->result,sizeof(buffers->result),"%s != %s",buffers->a,!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")?"false":"0");
            atom=0;
        } else if(!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")) {
            fresh(e,temp);declare(e,temp,type,"0");
            line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",buffers->a);e->indent++;
            line(e,"%s = 1%s",temp,e->target==ZIR_GO?"":";");
            e->indent--;
            line(e, "}");
            copy_text(buffers->result, sizeof(buffers->result), temp);
        } else if(width(type)) {
            if(canonical(e->fn->exprs[expr->right].type)[0]=='f') {
                if(e->target==ZIR_GO) {
                    /* floatToInt checks the range; the conversion keeps the low bits. */
                    const char *from = canonical(e->fn->exprs[expr->right].type);
                    format(buffers->result, sizeof(buffers->result), strcmp(from, "float64") ?
                           "%s(floatToInt(float64(%s), %d, %s))" : "%s(floatToInt(%s, %d, %s))",
                           TargetType(type, e->target), buffers->a, width(type),
                           signed_type(type) ? "true" : "false");
                } else {
                    /* FloatToInt checks the range; SignedBits reads a signed result. */
                    if(signed_type(type))
                        format(buffers->result, sizeof(buffers->result), "(%s)SignedBits(FloatToInt(%s, %d, 1), %d)",
                               TargetType(type, e->target), buffers->a, width(type), width(type));
                    else
                        format(buffers->result, sizeof(buffers->result), "(%s)FloatToInt(%s, %d, 0)",
                               TargetType(type, e->target), buffers->a, width(type));
                }
            } else number(e,type,buffers->a,e->fn->exprs[expr->right].type,"0",NULL,0,buffers->result,sizeof(buffers->result));
        }
        else {
            char cast_native[ZIR_NAME_MAX];
            if(e->target == ZIR_GO)
                e->resolve(e->context, type, cast_native, sizeof(cast_native));
            else {
                const char *leaf = type;
                char native_leaf[ZIR_NAME_MAX], source[ZIR_NAME_MAX * 2];
                while(*leaf == '*' || *leaf == '[') {
                    if(*leaf == '*') leaf = skip_ws(leaf + 1);
                    else {
                        const char *close = strchr(leaf, ']');
                        if(close == NULL) break;
                        leaf = skip_ws(close + 1);
                    }
                }
                if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                   NativeTypeAtUse(e->module, leaf, native_leaf, sizeof(native_leaf))) {
                    format(source, sizeof(source), "%.*s%s", (int)(leaf - type), type, native_leaf);
                    slot_native_type(source, e->target, cast_native, sizeof(cast_native));
                } else slot_native_type(type, e->target, cast_native, sizeof(cast_native));
            }
            if(e->target == ZIR_GO && type[0] == '*') {
                if(canonical(operand_type)[0] == '*')
                    format(buffers->result, sizeof(buffers->result),
                           "(%s)(unsafe.Pointer(%s))", cast_native, buffers->a);
                else {
                    char operand_native[ZIR_NAME_MAX];
                    const char *scalar = TargetType(operand_type, ZIR_GO);
                    if(scalar == NULL) {
                        e->resolve(e->context, operand_type, operand_native, sizeof(operand_native));
                        scalar = operand_native;
                    }
                    format(buffers->result, sizeof(buffers->result),
                           "(%s)(unsafe.Pointer(func(value %s) uintptr { return uintptr(value) }(%s)))",
                           cast_native, scalar, buffers->a);
                }
            }
            else if(e->target==ZIR_GO) format(buffers->result,sizeof(buffers->result),"%s(%s)",cast_native,buffers->a);
            else format(buffers->result,sizeof(buffers->result),"(%s)(%s)",cast_native,buffers->a);
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
        if(integer_literal_bits(buffers->result, &constant))
            atom = 1;
    }
    /* A consumer that runs nothing after this expression takes its calls in
     * place; otherwise they are captured below, in order. */
    int in_place = call_in_place &&
                   (e->minify || strlen(buffers->result) <= ZIR_INLINE_MAX) &&
                   (e->target == ZIR_GO || !ArrayElementType(type, NULL, 0, NULL));
    if(in_place || folds_text(e, buffers->result, type)) {
        /* declare() applies this cast for named enum types; inlined text has
         * to carry it so Go sees matching operand types. An explicit cast
         * keeps it even on a bare name: returning an s32 as an enum is not
         * an implicit conversion in C++ or Go. */
        if((!plain_identifier(buffers->result) || expr->kind == ZIR_EXPR_CAST) &&
           enum_type(e->module, type)) {
            const char *scalar = TargetType(type, e->target);
            char resolved[ZIR_NAME_MAX * 2];
            if(scalar == NULL) {
                e->resolve(e->context, type, resolved, sizeof(resolved));
                scalar = resolved;
            }
            if(e->target == ZIR_GO)
                format(out, size, "%s(%s)", scalar, buffers->result);
            else
                format(out, size, "((%s)(%s))", scalar, buffers->result);
        } else if(atom) {
            copy_text(out, size, buffers->result);
        } else {
            format(out, size, "(%s)", buffers->result);
        }
        e->pure = plain_identifier(buffers->result) ? 1 : pure;
        return;
    }
    fresh(e,temp);declare(e,temp,type,buffers->result);copy_text(out,size,temp);
    e->pure = 1;
}

void
emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size)
{
    static _Thread_local EmitExprBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitExprBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    emit_expr_with_buffers(e, index, expected, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
