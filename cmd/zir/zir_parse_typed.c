#include "zir_scratch.h"
#include "zir_memory.h"
#include <inttypes.h>
#include "zir_parse_internal.h"

/* Text of the qualified member of ENUMERATION holding `value`, so a bound
 * enum value keeps its enum type after textual expansion. */
static int
enum_member_spelling(const ZirType *enumeration, int64_t value, char *out,
                     size_t size)
{
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

/* The enum TYPE names in MODULE, or NULL when it names no enum. */
static const ZirType *
enum_type(const ZirModule *module, const char *type)
{
    const ZirType *enumeration = type[0] ? FindType(module, type, NULL) : NULL;
    return enumeration != NULL && enumeration->is_enum ? enumeration : NULL;
}

/* Replace a constant's text with its enum member spelling when it has one. */
static void
enum_member_expr(const ZirModule *module, const char *type, int64_t value,
                 const char **expr)
{
    char text[ZIR_TEXT_MAX];
    if(enum_member_spelling(enum_type(module, type), value, text, sizeof(text)))
        *expr = KeepText(text);
}

static int evaluate_typed_function(const ZirModule *module, const char *name, ZirSourceSpan call_span, CompileValue *arguments, char argument_names[][ZIR_NAME_MAX], int argument_count, int depth, int *fuel, CompileValue *result);
static int canonical_compound(const ZirModule *module, CompileValue *value);
/* Buffers evaluate_typed_node keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedNodeBuffers {
    unsigned char bytes[ZIR_TEXT_MAX];
    CompileValue args[16];
    char names[16][ZIR_NAME_MAX];
} EvaluateTypedNodeBuffers;

/* Enum members fold to typed integers only while a law is evaluated; other
 * callers rely on member expressions staying symbolic. */
_Thread_local int ZirLawEvaluation;

/* While a law is evaluated, names bound by the enclosing expression's scope
 * (parameters, locals, and module constants) are read from this table when
 * an identifier is evaluated, instead of being spliced into the source text
 * first. Each expression then parses once, whatever values it sees. */
static _Thread_local const ZirConsts *evaluation_names;

int evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result);

/* Law evaluation is pure: a binding read evaluates only the binding's text,
 * and a call only its arguments, procedures and module constants. Such a
 * step's result is kept and reused when the step would repeat exactly: no
 * deeper than when it was kept and with fuel for its cost, since depth and
 * fuel only ever make a step fail, and into a cleared value, since some
 * steps leave fields of their destination as they were. */
typedef struct KeptValue {
    int depth;
    int cost;
    CompileKind kind;
    int64_t integer;
    double real;
    const char *type;    /* kept */
    const char *literal; /* kept */
    const ZirModule *type_owner;
} KeptValue;

static int
compile_value_is_clear(const CompileValue *value)
{
    return value->kind == COMPILE_INVALID && value->integer == 0 &&
           value->real == 0 && !signbit(value->real) && !value->type[0] &&
           !value->literal[0] && value->type_owner == NULL;
}

/* Give RESULT, which is clear, the kept value when the step repeats. */
static int
kept_value_applies(const KeptValue *kept, int depth, int *fuel,
                   CompileValue *result)
{
    if(depth > kept->depth || *fuel < kept->cost) return 0;
    *fuel -= kept->cost;
    result->kind = kept->kind;
    result->integer = kept->integer;
    result->real = kept->real;
    copy_text(result->type, sizeof(result->type), kept->type);
    copy_text(result->literal, sizeof(result->literal), kept->literal);
    result->type_owner = kept->type_owner;
    return 1;
}

/* Keep VALUE with TYPE and LITERAL, copies of its texts that outlive it. */
static void
keep_value_texts(KeptValue *kept, int depth, int cost, const CompileValue *value,
                 const char *type, const char *literal)
{
    kept->depth = depth;
    kept->cost = cost;
    kept->kind = value->kind;
    kept->integer = value->integer;
    kept->real = value->real;
    kept->type = type;
    kept->literal = literal;
    kept->type_owner = value->type_owner;
}

static void
keep_value(KeptValue *kept, int depth, int cost, const CompileValue *value)
{
    keep_value_texts(kept, depth, cost, value, KeepText(value->type),
                     KeepText(value->literal));
}

/* A binding's last read: its text, evaluated in MODULE, gave VALUE. */
struct ZirBoundRead {
    const char *expr; /* NULL when nothing is kept */
    const ZirModule *module;
    KeptValue value;
};

/* Name lookups in non-empty tables so far; a read that makes none depends
 * only on its own text. */
static _Thread_local unsigned long binding_lookups;

/* Evaluate `name` the way splicing `(cast(TYPE) (value))`, or `(value)` for
 * constants and non-scalar locals, into the expression would. Returns -1
 * when no binding in scope has that name. */
static int
evaluate_bound_name(const ZirModule *module, const char *path,
                    const char *name, int depth, int *fuel,
                    CompileValue *result)
{
    const ZirConsts *names = evaluation_names;
    if(names == NULL)
        return -1;
    if(names->count > 0)
        binding_lookups++;
    for(int i = 0; i < names->count; i++) {
        const ZirConst *binding = &names->items[i];
        if(strcmp(binding->name, name) ||
           (binding->is_file_private && strcmp(binding->path, path)))
            continue;
        ZirBoundRead *read = names->reads != NULL && binding->path[0] &&
                             compile_value_is_clear(result) ?
                             &names->reads[i] : NULL;
        if(read != NULL && read->expr == binding->expr &&
           read->module == module &&
           kept_value_applies(&read->value, depth, fuel, result))
            return 1;
        char text[ZIR_TEXT_MAX];
        size_t type_length = strlen(binding->type);
        size_t expr_length = strlen(binding->expr);
        size_t used = 0;
        if(type_length + expr_length + 9 >= sizeof(text))
            return 0;
        if(ScalarWidth(binding->type) || !strcmp(binding->type, "bool")) {
            memcpy(text, "cast(", 5);
            memcpy(text + 5, binding->type, type_length);
            used = 5 + type_length;
            memcpy(text + used, ") ", 2);
            used += 2;
        }
        text[used++] = '(';
        memcpy(text + used, binding->expr, expr_length);
        used += expr_length;
        text[used++] = ')';
        text[used] = '\0';
        unsigned long lookups = binding_lookups;
        int before = *fuel;
        int ok = evaluate_typed_expression(module, names, text,
                                           Span(binding->path[0] ? binding->path : path, 1, 1),
                                           depth + 1, fuel, result);
        if(ok && read != NULL && binding_lookups == lookups) {
            read->expr = binding->expr;
            read->module = module;
            keep_value(&read->value, depth, before - *fuel, result);
        }
        return ok;
    }
    return -1;
}

/* A constant declared in `module` itself, visible from `path`. While a law is
 * evaluated, its value reads further names through the module in turn.
 * Returns -1 when the module declares no such constant. */
static int
evaluate_module_define(const ZirModule *module, const char *path,
                       const char *name, int depth, int *fuel,
                       CompileValue *result)
{
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *definition = &module->defines[i];
        if(strcmp(definition->name, name) ||
           (definition->is_file_private &&
            strcmp(SpanPath(definition->span), path)))
            continue;
        ZirConsts none = {0};
        return evaluate_typed_expression(module, &none, definition->value,
                                         definition->span, depth + 1, fuel,
                                         result);
    }
    return -1;
}

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
    CompileValue left, right;
    compile_value_clear(&left);
    compile_value_clear(&right);
    char *end;
    int64_t integer;
    unsigned long long literal_bits;
    double real;
    int truth;
    switch(expression->kind) {
    case ZIR_EXPR_INT:
        if(!ZirIntegerLiteral(expression->text, &literal_bits)) return 0;
        uint64_t bits = literal_bits;
        integer = ScalarSigned(bits, 64);
        result->kind = COMPILE_INTEGER;
        result->integer = integer;
        if(bits > INT64_MAX) copy_text(result->type, sizeof(result->type), "u64");
        return compile_value_literal(result);
    case ZIR_EXPR_FLOAT:
        errno = 0;
        real = ParseDecimal(expression->text, &end);
        if(errno == ERANGE || end == expression->text || *end ||
           !isfinite(real)) return 0;
        result->kind = COMPILE_REAL;
        result->real = real;
        if(!compile_value_literal(result)) return 0;
        /* A law accepts only a literal that spells its double exactly, the
         * way the evaluator writes a real passed through a call or local;
         * other decimal text would round. */
        return !ZirLawEvaluation || !strcmp(result->literal, expression->text);
    case ZIR_EXPR_STRING:
        result->kind = COMPILE_STRING;
        copy_text(result->literal, sizeof(result->literal),
                  expression->text);
        return 1;
    case ZIR_EXPR_COMPOUND:
        return compile_compound_value(probe, expression, module, path,
                                      depth, fuel, result) &&
               canonical_compound(module, result);
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
        {
            int bound = evaluate_bound_name(module, path, expression->name,
                                            depth, fuel, result);
            if(bound < 0 && evaluation_names != NULL)
                bound = evaluate_module_define(module, path, expression->name,
                                               depth, fuel, result);
            if(bound >= 0)
                return bound;
        }
        return evaluate_imported_typed_define(module, path,
                    expression->name, depth + 1, fuel, result);
    case ZIR_EXPR_SIZE_OF: {
        size_t size, alignment;
        if(!TypeLayout(module, expression->name, &size, &alignment) ||
           size > LONG_MAX) return 0;
        result->kind = COMPILE_INTEGER;
        result->integer = (int64_t)size;
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
            else if(right.kind == COMPILE_INTEGER) {
                if(!ZirLawEvaluation && right.integer == INT64_MIN) return 0;
                result->integer = ScalarStored(UINT64_C(0) - (uint64_t)right.integer,
                                               right.type);
            }
            else return 0;
        } else if(!strcmp(expression->op, "~") &&
                  right.kind == COMPILE_INTEGER) {
            *result = right;
            result->integer = ScalarStored(~(uint64_t)right.integer, right.type);
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
        if(ZirLawEvaluation &&
           (left.kind == COMPILE_REAL || right.kind == COMPILE_REAL) &&
           (!compile_law_exact_double(&left) ||
            !compile_law_exact_double(&right))) return 0;
        if(!strcmp(op, "<") || !strcmp(op, "<=") ||
           !strcmp(op, ">") || !strcmp(op, ">=")) {
            int cmp = 0;
            if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER) {
                int unsigned64 = !strcmp(left.type, "u64") || !strcmp(right.type, "u64");
                cmp = unsigned64 ? ScalarCompare((uint64_t)left.integer,
                    (uint64_t)right.integer, 64, 1) :
                    (left.integer < right.integer ? -1 : left.integer > right.integer);
            }
            result->kind = COMPILE_INTEGER;
            if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER)
                result->integer = !strcmp(op, "<") ? cmp < 0 :
                                  !strcmp(op, "<=") ? cmp <= 0 :
                                  !strcmp(op, ">") ? cmp > 0 : cmp >= 0;
            else
                result->integer = !strcmp(op, "<") ? a < b :
                                  !strcmp(op, "<=") ? a <= b :
                                  !strcmp(op, ">") ? a > b : a >= b;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER) {
            int64_t x = left.integer, y = right.integer, folded;
            const char *type = ScalarWidth(left.type) ? left.type :
                               ScalarWidth(right.type) ? right.type : "s64";
            uint64_t xb = (uint64_t)x, yb = (uint64_t)y;
            /* Ordinary #run retains its checked host-width overflow gate.
             * Laws evaluate declared runtime widths, including 64-bit wrap. */
            if(!ZirLawEvaluation && strcmp(type, "u64")) {
                int64_t checked;
                if((!strcmp(op, "+") && __builtin_add_overflow(x, y, &checked)) ||
                   (!strcmp(op, "-") && __builtin_sub_overflow(x, y, &checked)) ||
                   (!strcmp(op, "*") && __builtin_mul_overflow(x, y, &checked))) return 0;
            }
            if(!strcmp(op, "+")) {
                folded = ScalarStored(xb + yb, type);
            } else if(!strcmp(op, "-")) {
                folded = ScalarStored(xb - yb, type);
            } else if(!strcmp(op, "*")) {
                folded = ScalarStored(xb * yb, type);
            } else if((!strcmp(op, "/") || !strcmp(op, "%")) && yb &&
                      (!strcmp(left.type, "u64") || !strcmp(right.type, "u64")))
                folded = ScalarStored(!strcmp(op, "/") ? xb / yb : xb % yb, type);
            else if(!strcmp(op, "/") && y != 0)
                folded = x == INT64_MIN && y == -1 ? x : x / y;
            else if(!strcmp(op, "%") && y != 0)
                folded = x == INT64_MIN && y == -1 ? 0 : x % y;
            else if(!strcmp(op, "&")) folded = x & y;
            else if(!strcmp(op, "|")) folded = x | y;
            else if(!strcmp(op, "^")) folded = x ^ y;
            else if(!strcmp(op, "<<") && y >= 0 && y < ScalarWidth(type))
                folded = ScalarStored(xb << y, type);
            else if(!strcmp(op, ">>") && y >= 0 && y < ScalarWidth(type)) {
                unsigned width = ScalarWidth(type);
                uint64_t shifted = (xb & ScalarMask(width)) >> y;
                if(type[0] != 'u' && x < 0 && y > 0)
                    shifted |= ScalarMask(width) ^ (ScalarMask(width) >> y);
                folded = ScalarStored(shifted, type);
            }
            else return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = folded;
            if(wrap_compile_integer(type, &result->integer))
                copy_text(result->type, sizeof(result->type), type);
            return compile_value_literal(result);
        }
        /* Each operation rounds once to its declared width, as unfused
         * IEEE arithmetic does on every target: float32 when either
         * operand is float32 and neither is float64, otherwise float64. */
        int single = (!strcmp(left.type, "float32") || !strcmp(right.type, "float32")) &&
                     strcmp(left.type, "float64") && strcmp(right.type, "float64");
        result->kind = COMPILE_REAL;
        if(!strcmp(op, "+")) result->real = a + b;
        else if(!strcmp(op, "-")) result->real = a - b;
        else if(!strcmp(op, "*")) result->real = a * b;
        else if(!strcmp(op, "/") && b != 0.0) result->real = a / b;
        else return 0;
        if(single)
            result->real = (float)result->real;
        copy_text(result->type, sizeof(result->type), single ? "float32" : "float64");
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
            if(ZirLawEvaluation && enumeration != NULL && enumeration->is_enum &&
               EnumMemberValue(enumeration, expression->name,
                               &member_value)) {
                result->kind = COMPILE_INTEGER;
                result->integer = (int64_t)member_value;
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
            result->integer = (int64_t)length;
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
        if(result->kind == COMPILE_INTEGER) {
            const ZirType *type = FindType(module, expression->name, NULL);
            const char *scalar = type && type->is_enum ? type->enum_backing : expression->name;
            if(!strcmp(expression->name, "bool")) result->integer = result->integer != 0;
            else if(wrap_compile_integer(scalar, &result->integer)) {
                copy_text(result->type, sizeof(result->type), expression->name);
                if(type && type->is_enum) return compile_value_literal(result);
            }
            if(ZirLawEvaluation && (!strcmp(scalar, "float32") || !strcmp(scalar, "float64"))) {
                /* An integer converts to a float exactly within the
                 * significand; outside it the rounding is left unproved. */
                int64_t limit = !strcmp(scalar, "float32") ? INT64_C(1) << 24 : INT64_C(1) << 53;
                if(!strcmp(result->type, "u64") ?
                   (uint64_t)result->integer > (uint64_t)limit :
                   (result->integer < -limit || result->integer > limit)) return 0;
                result->real = (double)result->integer;
                result->kind = COMPILE_REAL;
            }
        }
        if(eval_integer_type(expression->name, 0) &&
           result->kind == COMPILE_REAL) {
            if(result->real < (double)LONG_MIN ||
               result->real >= -(double)LONG_MIN) return 0;
            result->integer = (int64_t)result->real;
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
            if(count < 16) {
                compile_value_clear(&buffers->args[count]);
                buffers->names[count][0] = '\0';
            }
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

COMPILER_SCRATCH_RETURN(, int, evaluate_typed_node,
    (const ZirFunction *probe, int index,
                    const ZirModule *module, const char *path,
                    int depth, int *fuel, CompileValue *result),
    EvaluateTypedNodeBuffers, RECOVERABLE, free,
    evaluate_typed_node_with_buffers(probe, index, module, path, depth, fuel, result, buffers))
typedef struct ParseCacheEntry {
    const ZirModule *module;
    uint64_t hash;
    char *text;
    ZirFunction probe;
    int root;
} ParseCacheEntry;

enum { PARSE_CACHE_SLOTS = 8192, PARSE_CACHE_BYTES = 32 * 1024 * 1024 };

typedef struct ExpressionCache {
    ParseCacheEntry *entries;
    size_t count, bytes;
    int users;
} ExpressionCache;
static void free_expression_cache(void *value)
{
    ExpressionCache *cache = value;
    for(size_t i = 0; cache->entries && i < PARSE_CACHE_SLOTS; i++) {
        free(cache->entries[i].text);
        free(cache->entries[i].probe.exprs);
    }
    free(cache->entries);
}
static ExpressionCache *expression_cache(void)
{
    return CompilerCurrentStorage(COMPILER_EXPRESSIONS, sizeof(ExpressionCache), free_expression_cache);
}
#define parse_cache (expression_cache()->entries)
#define parse_cache_count (expression_cache()->count)
#define parse_cache_bytes (expression_cache()->bytes)
#define parse_cache_users (expression_cache()->users)

static uint64_t
parse_cache_hash(const ZirModule *module, const char *text)
{
    uint64_t hash = UINT64_C(1469598103934665603) ^ (uintptr_t)module;
    for(const unsigned char *p = (const unsigned char *)text; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    return hash;
}

static void
parse_cache_clear(void)
{
    for(size_t i = 0; parse_cache != NULL && i < PARSE_CACHE_SLOTS; i++) {
        free(parse_cache[i].text);
        free(parse_cache[i].probe.exprs);
    }
    free(parse_cache);
    parse_cache = NULL;
    parse_cache_count = parse_cache_bytes = 0;
}

static int
CachedParse_persistent(const ZirModule *module, const char *text, ZirSourceSpan span,
            ZirFunction *scratch, const ZirFunction **probe)
{
    uint64_t hash = parse_cache_hash(module, text);
    size_t slot = (size_t)hash & (PARSE_CACHE_SLOTS - 1);
    if(parse_cache != NULL)
        for(size_t step = 0; step < PARSE_CACHE_SLOTS; step++) {
            ParseCacheEntry *entry = &parse_cache[(slot + step) & (PARSE_CACHE_SLOTS - 1)];
            if(entry->text == NULL)
                break;
            if(entry->hash == hash && entry->module == module &&
               !strcmp(entry->text, text)) {
                parse_cache_users++;
                *probe = &entry->probe;
                return entry->root;
            }
        }
    /* Entries in use by an enclosing evaluation cannot be freed, so a full
     * cache only clears when nothing holds one. */
    if(parse_cache_users == 0 &&
       (parse_cache_count * 2 >= PARSE_CACHE_SLOTS ||
        parse_cache_bytes >= PARSE_CACHE_BYTES))
        parse_cache_clear();
    memset(scratch, 0, sizeof(*scratch));
    int root = ParseExpr(scratch, module, text, span);
    *probe = scratch;
    if(root < 0 || parse_cache_count * 2 >= PARSE_CACHE_SLOTS ||
       parse_cache_bytes >= PARSE_CACHE_BYTES)
        return root;
    if(parse_cache == NULL &&
       (parse_cache = calloc(PARSE_CACHE_SLOTS, sizeof(*parse_cache))) == NULL)
        return root;
    char *copy = strdup(text);
    if(copy == NULL)
        return root;
    while(parse_cache[slot].text != NULL)
        slot = (slot + 1) & (PARSE_CACHE_SLOTS - 1);
    ParseCacheEntry *entry = &parse_cache[slot];
    entry->module = module;
    entry->hash = hash;
    entry->text = copy;
    entry->probe = *scratch;
    entry->root = root;
    memset(scratch, 0, sizeof(*scratch));
    parse_cache_count++;
    parse_cache_bytes += strlen(text) + 1 +
        (size_t)entry->probe.expr_cap * sizeof(ZirExpr);
    parse_cache_users++;
    *probe = &entry->probe;
    return root;
}

void
CachedParseDone(const ZirFunction *probe, ZirFunction *scratch)
{
    if(probe == scratch) {
        free(scratch->exprs);
        scratch->exprs = NULL;
    } else
        parse_cache_users--;
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

/* SOURCE as an expression evaluates it: trimmed, without a final ';'. */
static void
expression_input(char *out, size_t size, const char *source)
{
    copy_text(out, size, source);
    trim_in_place(out);
    size_t length = strlen(out);
    if(length && out[length - 1] == ';') {
        out[length - 1] = '\0';
        trim_in_place(out);
    }
}

/* Whether a parsed expression holds a compile-time directive, which a
 * top-level evaluation refuses. */
static int
holds_compile_time(const ZirFunction *probe)
{
    for(int i = 0; i < probe->expr_count; i++)
        if(probe->exprs[i].kind == ZIR_EXPR_COMPILE_TIME)
            return 1;
    return 0;
}

static int
evaluate_typed_expression_with_buffers(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result, EvaluateTypedExpressionBuffers *buffers)
{
    ZirConsts empty = {0};
    memset(&buffers->probe, 0, sizeof(buffers->probe));
    int64_t integer;
    int root, ok;
    const char *path = SpanPath(span);
    if(depth > 32 || source == NULL || --*fuel < 0) return 0;
    expression_input(buffers->input, sizeof(buffers->input), source);
    if(!buffers->input[0]) return 0;
    if(names == NULL) names = &empty;
    const ZirConsts *outer_names = evaluation_names;
    const char *text = buffers->input;
    if(ZirLawEvaluation)
        evaluation_names = names;
    else {
        evaluation_names = NULL;
        expand_compile_expr(buffers->expanded, sizeof(buffers->expanded), names, buffers->input, path);
        text = buffers->expanded;
        if(eval_const_condition_with_fuel(buffers->expanded, &integer, module, names,
                                          path, span.line, depth, fuel)) {
            evaluation_names = outer_names;
            result->kind = COMPILE_INTEGER;
            result->integer = integer;
            return compile_value_literal(result);
        }
    }
    const ZirFunction *probe;
    root = CachedParse(module, text, span, &buffers->probe, &probe);
    ok = root >= 0 && (depth != 0 || !holds_compile_time(probe));
    if(ok)
        ok = evaluate_typed_node(probe, root, module, path,
                                 depth + 1, fuel, result);
    CachedParseDone(probe, &buffers->probe);
    evaluation_names = outer_names;
    return ok;
}

COMPILER_SCRATCH_RETURN(, int, evaluate_typed_expression,
    (const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result),
    EvaluateTypedExpressionBuffers, RECOVERABLE, free,
    evaluate_typed_expression_with_buffers(module, names, source, span, depth, fuel, result, buffers))

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
    CompileValue value;
    compile_value_clear(&value);
    return typed_body_expression(body, buffers->condition, &value) &&
           compile_truth(&value, truth);
}

COMPILER_SCRATCH_RETURN(static, int, typed_body_condition,
    (TypedBody *body, const char *header,
                     const char *word, int *truth),
    TypedBodyConditionBuffers, RECOVERABLE, free,
    typed_body_condition_with_buffers(body, header, word, truth, buffers))

static void
typed_leave_scope(TypedBody *body, int saved)
{
    int removed = body->local_count - saved;
    if(removed <= 0) return;
    memmove(body->names.items, body->names.items + removed,
            (size_t)(body->names.count - removed) *
                sizeof(*body->names.items));
    memmove(body->names.reads, body->names.reads + removed,
            (size_t)(body->names.count - removed) *
                sizeof(*body->names.reads));
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
    memmove(body->names.reads + 1, body->names.reads,
            (size_t)body->names.count * sizeof(*body->names.reads));
    body->names.reads[0].expr = NULL;
    ZirConst *local = &body->names.items[0];
    memset(local, 0, sizeof(*local));
    local->expr = "";
    copy_text(local->name, sizeof(local->name), name);
    copy_text(local->type, sizeof(local->type), type);
    copy_text(local->path, sizeof(local->path), SpanPath(body->fn->span));
    if(value != NULL)
        local->expr = KeepText(value->literal);
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
static int typed_store_element(TypedBody *body, const char *type,
                               const char *literal, char **indexes, int count,
                               const char *source, char *out, size_t size);
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
store_array_element(const char *literal, int64_t index, const char *replacement,
                    char *out, size_t size)
{
    const char *open = strstr(literal, ".[");
    const char *cursor;
    size_t used;
    int64_t ordinal = 0;
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

/* Text of element `index` of a canonical array literal `T.[a, b, c]`. */
static int
array_element_text(const char *literal, int64_t index, char *out, size_t size)
{
    const char *cursor = strstr(literal, ".[");
    int64_t ordinal = 0;
    if(cursor == NULL || index < 0) return 0;
    cursor += 2;
    for(;;) {
        int depth = 0;
        cursor = skip_ws(cursor);
        const char *start = cursor;
        while(*cursor && !(depth == 0 && (*cursor == ',' || *cursor == ']'))) {
            if(*cursor == '[' || *cursor == '{' || *cursor == '(') depth++;
            if(*cursor == ']' || *cursor == '}' || *cursor == ')') depth--;
            cursor++;
        }
        if(*cursor == '\0') return 0;
        if(ordinal == index) {
            size_t length = (size_t)(cursor - start);
            while(length && isspace((unsigned char)start[length - 1])) length--;
            if(length >= size) return 0;
            memcpy(out, start, length);
            out[length] = '\0';
            return 1;
        }
        if(*cursor == ']') return 0;
        cursor++;
        ordinal++;
    }
}

/* Writes `literal`, an array of `type`, to `out` with the element at
 * `indexes` (one per level of nesting) set to the value of `source`. */
static int
typed_store_element(TypedBody *body, const char *type, const char *literal,
                    char **indexes, int count, const char *source,
                    char *out, size_t size)
{
    char element_type[ZIR_NAME_MAX];
    int capacity = 0;
    CompileValue at, element;
    compile_value_clear(&at);
    compile_value_clear(&element);
    if(count < 1 ||
       !ArrayElementType(type, element_type, sizeof(element_type), &capacity) ||
       !typed_body_expression(body, indexes[0], &at) ||
       at.kind != COMPILE_INTEGER || at.integer < 0 || at.integer >= capacity)
        return 0;
    if(count == 1)
        return typed_body_expression(body, source, &element) &&
               compile_type_value(element_type, &element) &&
               store_array_element(literal, at.integer, element.literal,
                                   out, size);
    char *row = AllocateOrExit(ZIR_TEXT_MAX);
    char *stored = AllocateOrExit(ZIR_TEXT_MAX);
    int ok = array_element_text(literal, at.integer, row, ZIR_TEXT_MAX) &&
             typed_store_element(body, element_type, row, indexes + 1,
                                 count - 1, source, stored, ZIR_TEXT_MAX) &&
             store_array_element(literal, at.integer, stored, out, size);
    free(row);
    free(stored);
    return ok;
}

/* Records the evaluator can hold as a literal: plain named records. */
static const ZirType *
literal_record(const ZirModule *module, const char *type,
               const ZirModule **owner)
{
    const ZirModule *found = NULL;
    const ZirType *record = FindType(module, type, &found);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern ||
       record->is_owned_vec || record->is_map) return NULL;
    if(owner != NULL) *owner = found != NULL ? found : module;
    return record;
}

/* Value text of field `name` in a record literal `T.{.a = x, .b = y}` as the
 * evaluator writes it. Returns 1 when present, 0 when absent, -1 when the
 * text is not such a literal. */
static int
record_field_text(const char *literal, const char *name, char *out,
                  size_t size)
{
    const char *cursor = literal != NULL ? strstr(literal, ".{") : NULL;
    size_t name_length = strlen(name);
    if(cursor == NULL) return -1;
    cursor += 2;
    for(;;) {
        cursor = skip_ws(cursor);
        if(*cursor == '}') return 0;
        if(*cursor != '.') return -1;
        const char *field = ++cursor;
        while(isalnum((unsigned char)*cursor) || *cursor == '_') cursor++;
        size_t field_length = (size_t)(cursor - field);
        cursor = skip_ws(cursor);
        if(*cursor != '=') return -1;
        cursor = skip_ws(cursor + 1);
        const char *value = cursor;
        int depth = 0, quoted = 0;
        while(*cursor && (quoted || depth > 0 ||
                          (*cursor != ',' && *cursor != '}'))) {
            if(quoted) {
                if(*cursor == '\\' && cursor[1]) cursor++;
                else if(*cursor == '"') quoted = 0;
            } else if(*cursor == '"') quoted = 1;
            else if(strchr("([{", *cursor)) depth++;
            else if(strchr(")]}", *cursor)) depth--;
            cursor++;
        }
        if(*cursor == '\0') return -1;
        if(field_length == name_length &&
           !strncmp(field, name, name_length)) {
            size_t length = (size_t)(cursor - value);
            while(length && isspace((unsigned char)value[length - 1]))
                length--;
            if(length >= size) return -1;
            memcpy(out, value, length);
            out[length] = '\0';
            return 1;
        }
        if(*cursor == ',') cursor++;
    }
}

static int record_literal(const ZirModule *module, const ZirType *record,
                          const char *type, const char *literal,
                          const char *path, const char *replacement,
                          char *out, size_t size, int depth);

/* Zero text for a value of `type`, as a zero-initialized local holds it:
 * 0 for integers, bools and enums, 0.0 for floats, "" for strings, and
 * arrays and records of zeros. */
static int
zero_value_literal(const ZirModule *module, const char *type, char *out,
                   size_t size, int depth)
{
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(depth > 16 || size < 3) return 0;
    if(!strcmp(type, "float32") || !strcmp(type, "float64"))
        return snprintf(out, size, "0.0") < (int)size;
    if(ScalarWidth(type) || !strcmp(type, "bool"))
        return snprintf(out, size, "0") < (int)size;
    if(!strcmp(type, "string"))
        return snprintf(out, size, "\"\"") < (int)size;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        char *zero = AllocateOrExit(ZIR_TEXT_MAX);
        int ok = capacity >= 1 && capacity <= 256 &&
                 zero_value_literal(module, element, zero, ZIR_TEXT_MAX,
                                    depth + 1);
        size_t used = ok ? (size_t)snprintf(out, size, "%s.[", element) : 0;
        for(int i = 0; ok && i < capacity; i++) {
            int written = snprintf(out + used, size - used, "%s%s",
                                   i ? ", " : "", zero);
            if(written < 0 || (size_t)written >= size - used) ok = 0;
            else used += (size_t)written;
        }
        free(zero);
        if(!ok || used + 2 > size) return 0;
        out[used] = ']';
        out[used + 1] = '\0';
        return 1;
    }
    const ZirModule *owner = NULL;
    const ZirType *found = FindType(module, type, NULL);
    if(found != NULL && found->is_enum)
        return snprintf(out, size, "0") < (int)size;
    const ZirType *record = literal_record(module, type, &owner);
    return record != NULL &&
           record_literal(owner, record, type, NULL, NULL, NULL, out, size,
                          depth + 1);
}

/* A record literal in canonical form: every field in declaration order,
 * taken from `literal` or zero when it is absent there, so equal records
 * have equal text. A `path` of field names replaces that field's value with
 * `replacement`, descending into nested records for `a.b`. */
static int
record_literal(const ZirModule *module, const ZirType *record,
               const char *type, const char *literal, const char *path,
               const char *replacement, char *out, size_t size, int depth)
{
    char head[ZIR_NAME_MAX] = "";
    const char *rest = NULL;
    ZirTypeField field;
    size_t offset = 0;
    int ok = 1, replaced = path == NULL;
    if(depth > 16) return 0;
    if(path != NULL) {
        const char *dot = strchr(path, '.');
        size_t length = dot != NULL ? (size_t)(dot - path) : strlen(path);
        if(length == 0 || length >= sizeof(head)) return 0;
        memcpy(head, path, length);
        head[length] = '\0';
        rest = dot != NULL ? dot + 1 : NULL;
    }
    char *value = AllocateOrExit(ZIR_TEXT_MAX);
    char *current = AllocateOrExit(ZIR_TEXT_MAX);
    int written = snprintf(out, size, "%s.{", type);
    size_t used = written < 0 ? size : (size_t)written;
    int ordinal = 0;
    while(ok && used < size && TypeNextField(record, &offset, &field) == 1) {
        int present = literal != NULL ?
            record_field_text(literal, field.name, current, ZIR_TEXT_MAX) : 0;
        if(present < 0) { ok = 0; break; }
        if(!present && !zero_value_literal(module, field.type, current,
                                           ZIR_TEXT_MAX, depth + 1)) {
            ok = 0;
            break;
        }
        if(path != NULL && !strcmp(field.name, head)) {
            replaced = 1;
            if(rest == NULL) copy_text(value, ZIR_TEXT_MAX, replacement);
            else {
                const ZirModule *owner = NULL;
                const ZirType *inner = literal_record(module, field.type,
                                                      &owner);
                ok = inner != NULL &&
                     record_literal(owner, inner, field.type, current, rest,
                                    replacement, value, ZIR_TEXT_MAX,
                                    depth + 1);
            }
        } else copy_text(value, ZIR_TEXT_MAX, current);
        written = snprintf(out + used, size - used, "%s.%s = %s",
                           ordinal ? ", " : "", field.name, value);
        if(written < 0 || (size_t)written >= size - used) ok = 0;
        else used += (size_t)written;
        ordinal++;
    }
    free(value);
    free(current);
    if(!ok || !replaced || used + 2 > size) return 0;
    out[used] = '}';
    out[used + 1] = '\0';
    return 1;
}

/* Record literals written with fields missing or out of order still get one
 * text per value, since the evaluator compares records by their text. A
 * record whose zero cannot be written keeps the text it was given. */
static int
canonical_compound(const ZirModule *module, CompileValue *value)
{
    const ZirModule *owner = NULL;
    const ZirType *record;
    int capacity = 0;
    if(value->kind != COMPILE_COMPOUND ||
       ArrayElementType(value->type, NULL, 0, &capacity)) return 1;
    record = literal_record(value->type_owner != NULL ? value->type_owner :
                            module, value->type, &owner);
    if(record == NULL) return 1;
    char *text = AllocateOrExit(ZIR_TEXT_MAX);
    if(record_literal(owner, record, value->type, value->literal, NULL, NULL,
                      text, ZIR_TEXT_MAX, 0))
        copy_text(value->literal, sizeof(value->literal), text);
    free(text);
    return 1;
}

/* The declared type of field path `a.b` inside `record`. */
static int
record_path_type(const ZirModule *module, const ZirType *record,
                 const char *path, char *out, size_t size)
{
    char head[ZIR_NAME_MAX];
    const char *dot = strchr(path, '.');
    size_t length = dot != NULL ? (size_t)(dot - path) : strlen(path);
    ZirTypeField field;
    size_t offset = 0;
    if(length == 0 || length >= sizeof(head)) return 0;
    memcpy(head, path, length);
    head[length] = '\0';
    while(TypeNextField(record, &offset, &field) == 1) {
        if(strcmp(field.name, head)) continue;
        if(dot == NULL) {
            copy_text(out, size, field.type);
            return 1;
        }
        const ZirModule *owner = NULL;
        const ZirType *inner = literal_record(module, field.type, &owner);
        return inner != NULL &&
               record_path_type(owner, inner, dot + 1, out, size);
    }
    return 0;
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
            CompileValue value;
            compile_value_clear(&value);
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
            } else if(equals == NULL &&
                      zero_value_literal(body->module, type,
                                         buffers->combined,
                                         sizeof(buffers->combined), 0)) {
                /* A local without an initializer holds its type's zero. */
                if(!typed_body_expression(body, buffers->combined, &value) ||
                   !compile_type_value(type, &value)) goto failed;
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
                    /* `name[i] = value`, or `name[i][j] = value` on an
                     * array of arrays, on an array local. */
                    char *indexes[8];
                    int count = 0, target;
                    for(char *cursor = bracket; *cursor; ) {
                        int depth = 0;
                        char *open = cursor;
                        if(*open != '[' || count == 8) goto failed;
                        for(; *cursor; cursor++) {
                            if(*cursor == '[') depth++;
                            else if(*cursor == ']' && --depth == 0) break;
                        }
                        if(*cursor != ']') goto failed;
                        *open = '\0';
                        *cursor++ = '\0';
                        indexes[count++] = open + 1;
                    }
                    trim_in_place(buffers->text);
                    target = typed_local_index(body, buffers->text);
                    if(target < 0 ||
                       !typed_store_element(body, body->names.items[target].type,
                                            body->names.items[target].expr,
                                            indexes, count, equals,
                                            buffers->combined,
                                            sizeof(buffers->combined)))
                        goto failed;
                    body->names.items[target].expr = KeepText(buffers->combined);
                    continue;
                }
            }
            {
                char *dot = strchr(buffers->text, '.');
                if(dot != NULL) {
                    /* `name.field = value`, or `name.a.b op= value`, on a
                     * record local: rebuild the record with that field. */
                    const ZirModule *owner = NULL;
                    const ZirType *record;
                    char field_type[ZIR_NAME_MAX];
                    CompileValue value;
                    compile_value_clear(&value);
                    int target;
                    if(op) {
                        int written = snprintf(buffers->combined,
                            sizeof(buffers->combined), "(%s) %c (%s)",
                            buffers->text, op, equals);
                        if(written < 0 ||
                           (size_t)written >= sizeof(buffers->combined) ||
                           !typed_body_expression(body, buffers->combined,
                                                  &value)) goto failed;
                    } else if(!typed_body_expression(body, equals, &value))
                        goto failed;
                    *dot++ = '\0';
                    trim_in_place(buffers->text);
                    target = typed_local_index(body, buffers->text);
                    if(target < 0 || !body->names.items[target].expr[0])
                        goto failed;
                    record = literal_record(body->module,
                                            body->names.items[target].type,
                                            &owner);
                    if(record == NULL ||
                       !record_path_type(owner, record, dot, field_type,
                                         sizeof(field_type)) ||
                       !compile_type_value(field_type, &value) ||
                       !record_literal(owner, record,
                                       body->names.items[target].type,
                                       body->names.items[target].expr, dot,
                                       value.literal, buffers->combined,
                                       sizeof(buffers->combined), 0))
                        goto failed;
                    body->names.items[target].expr = KeepText(buffers->combined);
                    continue;
                }
            }
            int local = typed_local_index(body, buffers->text);
            if(local < 0) goto failed;
            CompileValue value;
            compile_value_clear(&value);
            if(op) {
                int written = snprintf(buffers->combined, sizeof(buffers->combined),
                    "(%s) %c (%s)", body->names.items[local].expr, op,
                    equals);
                if(written < 0 || (size_t)written >= sizeof(buffers->combined) ||
                   !typed_body_expression(body, buffers->combined, &value)) goto failed;
            } else if(!typed_body_expression(body, equals, &value)) goto failed;
            if(!compile_type_value(body->names.items[local].type,
                                   &value)) goto failed;
            body->names.items[local].expr = KeepText(value.literal);
            continue;
        }
        if(statement->kind == ZIR_STMT_EXPR) {
            CompileValue ignored;
            compile_value_clear(&ignored);
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

COMPILER_SCRATCH_RETURN(static, int, typed_body_statements,
    (TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result),
    TypedBodyStatementsBuffers, RECOVERABLE, free,
    typed_body_statements_with_buffers(body, start, stop, flow, result, buffers))
/* A procedure's parameters as a call binds them, split once for each
 * parameter text: kept texts are one pointer for one spelling, so the two
 * pointers identify the split. */
typedef struct TypedParameter {
    const char *name;          /* NULL when the parameter has no ':' */
    const char *type;
    const char *default_value; /* NULL without a default */
    int is_identifier;
} TypedParameter;

typedef struct TypedSignature {
    const char *args;
    const char *defaults;
    int count;                 /* -1 when the defaults do not pair up */
    TypedParameter parameters[16];
} TypedSignature;

typedef struct TypedSignatures {
    TypedSignature **slots;
    size_t count, capacity;
} TypedSignatures;

static void
free_typed_signatures(void *value)
{
    TypedSignatures *signatures = value;
    for(size_t i = 0; i < signatures->capacity; i++)
        free(signatures->slots[i]);
    free(signatures->slots);
}

static size_t
typed_signature_slot(const TypedSignatures *signatures, const char *args,
                     const char *defaults)
{
    uint64_t hash = ((uint64_t)(uintptr_t)args * UINT64_C(0x9e3779b97f4a7c15)) ^
                    ((uint64_t)(uintptr_t)defaults * UINT64_C(0xc2b2ae3d27d4eb4f));
    size_t slot = (size_t)(hash >> 17) & (signatures->capacity - 1);
    while(signatures->slots[slot] != NULL &&
          (signatures->slots[slot]->args != args ||
           signatures->slots[slot]->defaults != defaults))
        slot = (slot + 1) & (signatures->capacity - 1);
    return slot;
}

/* Split ARGS and DEFAULTS the way a call reads them: at most 16
 * parameters, each `name: type` with a default from DEFAULTS or after its
 * own '='. */
static TypedSignature *
split_typed_signature(const char *args, const char *defaults)
{
    TypedSignature *signature = AllocateOrExit(sizeof(*signature));
    char (*parts)[ZIR_TEXT_MAX] = AllocateOrExit(32 * sizeof(*parts));
    char (*default_parts)[ZIR_TEXT_MAX] = parts + 16;
    memset(signature, 0, sizeof(*signature));
    signature->args = args;
    signature->defaults = defaults;
    for(int i = 0; i < 16; i++)
        default_parts[i][0] = '\0';
    int count = *skip_ws(args) ?
        split_top_level(args, parts[0], 16, sizeof(parts[0])) : 0;
    if(defaults[0] &&
       split_top_level(defaults, default_parts[0], 16,
                       sizeof(default_parts[0])) != count)
        count = -1;
    signature->count = count;
    for(int i = 0; i < count; i++) {
        TypedParameter *parameter = &signature->parameters[i];
        char *part = trim(parts[i]);
        char *colon = strchr(part, ':');
        if(colon == NULL) continue;
        *colon++ = '\0';
        trim_in_place(part);
        char *type = trim(colon);
        char *default_value = top_level_assignment(default_parts[i]);
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
        parameter->name = KeepText(part);
        parameter->type = KeepText(type);
        parameter->default_value = default_value ? KeepText(default_value) : NULL;
        parameter->is_identifier = is_identifier_text(part);
    }
    free(parts);
    return signature;
}

static const TypedSignature *
typed_signature(const ZirFunction *fn)
{
    CompilerMemory *memory = CompilerMemoryPause();
    TypedSignatures *signatures = CompilerCurrentStorage(COMPILER_SIGNATURES, sizeof(*signatures), free_typed_signatures);
    const char *args = FunctionArgs(fn), *defaults = FunctionDefaultArgs(fn);
    if(signatures->count * 2 >= signatures->capacity) {
        TypedSignatures grown = {
            .slots = calloc(signatures->capacity ? signatures->capacity * 2 : 256,
                            sizeof(*grown.slots)),
            .count = signatures->count,
            .capacity = signatures->capacity ? signatures->capacity * 2 : 256};
        if(grown.slots == NULL) die("out of memory evaluating #run procedure");
        for(size_t i = 0; i < signatures->capacity; i++)
            if(signatures->slots[i] != NULL)
                grown.slots[typed_signature_slot(&grown, signatures->slots[i]->args,
                                                 signatures->slots[i]->defaults)] =
                    signatures->slots[i];
        free(signatures->slots);
        *signatures = grown;
    }
    size_t slot = typed_signature_slot(signatures, args, defaults);
    if(signatures->slots[slot] == NULL) {
        signatures->slots[slot] = split_typed_signature(args, defaults);
        signatures->count++;
    }
    const TypedSignature *signature = signatures->slots[slot];
    CompilerMemoryResume(memory);
    return signature;
}

/* One call kept while a law is evaluated (see KeptValue). A body reads its
 * parameters only through their bindings, so the procedure and the kept
 * texts bound to its parameters decide the result. */
typedef struct KeptCall {
    uint64_t hash;
    const ZirFunction *fn;
    int parameter_count;
    KeptValue value;
    const char *bound[];  /* each parameter's binding text, kept */
} KeptCall;

enum { KEPT_CALL_SLOTS = 4096, KEPT_CALL_BYTES = 1 << 18 };

/* A law's kept calls: an open-addressed table over entries in one block,
 * both forgotten together when either fills. */
typedef struct KeptCalls {
    KeptCall **slots;
    unsigned char *bytes;
    size_t count, used;
} KeptCalls;

/* The calls of the law being evaluated; read only while ZirLawEvaluation is
 * set, which parse recovery restores. */
static _Thread_local KeptCalls *law_calls;

static uint64_t
call_hash(const ZirFunction *fn, const char *const *bound, int count)
{
    uint64_t hash = (uint64_t)(uintptr_t)fn * UINT64_C(0x9E3779B97F4A7C15);
    for(int i = 0; i < count; i++) {
        hash ^= (uint64_t)(uintptr_t)bound[i];
        hash *= UINT64_C(0xBF58476D1CE4E5B9);
        hash ^= hash >> 31;
    }
    return hash;
}

static size_t
call_slot(const KeptCalls *calls, uint64_t hash, const ZirFunction *fn,
          const char *const *bound, int count)
{
    size_t slot = (size_t)(hash >> 32) & (KEPT_CALL_SLOTS - 1);
    for(const KeptCall *call; (call = calls->slots[slot]) != NULL;
        slot = (slot + 1) & (KEPT_CALL_SLOTS - 1))
        if(call->hash == hash && call->fn == fn &&
           call->parameter_count == count &&
           !memcmp(call->bound, bound, (size_t)count * sizeof(*bound)))
            break;
    return slot;
}

static const KeptCall *
find_call(const KeptCalls *calls, uint64_t hash, const ZirFunction *fn,
          const char *const *bound, int count)
{
    if(calls->slots == NULL) return NULL;
    return calls->slots[call_slot(calls, hash, fn, bound, count)];
}

/* Keep a call's VALUE. The same call reads the same bindings, so it gives
 * the same value at the same cost again; only a deeper depth is new. */
static void
keep_call(KeptCalls *calls, uint64_t hash, const ZirFunction *fn,
          const char *const *bound, int count, int depth, int cost,
          const CompileValue *value)
{
    if(calls->slots == NULL) {
        calls->slots = calloc(KEPT_CALL_SLOTS, sizeof(*calls->slots));
        calls->bytes = malloc(KEPT_CALL_BYTES);
        if(calls->slots == NULL || calls->bytes == NULL) {
            free(calls->slots);
            free(calls->bytes);
            calls->slots = NULL;
            calls->bytes = NULL;
            return;
        }
    }
    size_t slot = call_slot(calls, hash, fn, bound, count);
    KeptCall *call = calls->slots[slot];
    if(call != NULL) {
        if(depth > call->value.depth) call->value.depth = depth;
        return;
    }
    size_t header = sizeof(KeptCall) + (size_t)count * sizeof(*bound);
    size_t type = strlen(value->type) + 1, literal = strlen(value->literal) + 1;
    size_t size = (header + type + literal + 15) & ~(size_t)15;
    if(size > KEPT_CALL_BYTES) return;
    if(calls->count * 2 >= KEPT_CALL_SLOTS ||
       calls->used + size > KEPT_CALL_BYTES) {
        memset(calls->slots, 0, KEPT_CALL_SLOTS * sizeof(*calls->slots));
        calls->count = calls->used = 0;
        slot = call_slot(calls, hash, fn, bound, count);
    }
    call = (KeptCall *)(calls->bytes + calls->used);
    calls->used += size;
    calls->count++;
    call->hash = hash;
    call->fn = fn;
    call->parameter_count = count;
    memcpy(call->bound, bound, (size_t)count * sizeof(*bound));
    char *texts = (char *)call + header;
    memcpy(texts, value->type, type);
    memcpy(texts + type, value->literal, literal);
    keep_value_texts(&call->value, depth, cost, value, texts, texts + type);
    calls->slots[slot] = call;
}

/* Buffers evaluate_typed_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedFunctionBuffers {
    CompileValue fallback;
} EvaluateTypedFunctionBuffers;

static int evaluate_typed_function(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result);

/* Call NAME with ARGUMENTS, which are converted to the parameter types in
 * place: callers pass values they no longer need. */
static int
evaluate_typed_function_with_buffers(const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result, EvaluateTypedFunctionBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    int bound[16];
    const char *bound_texts[16];
    TypedBody body = {0};
    int expected, flow = 0, ok = 0;
    if(depth >= 32 || module == NULL || !name[0] ||
       ResolveFunctionAt(module, name, SpanPath(call_span), &owner, &fn) != 1 ||
       fn == NULL || fn->is_extern || fn->is_template ||
       fn->stmt_count == 0) return 0;
    const TypedSignature *signature = typed_signature(fn);
    expected = signature->count;
    if(expected < 0 || argument_count > expected) return 0;
    for(int i = 0; i < expected; i++)
        bound[i] = -1;
    for(int argument = 0; argument < argument_count; argument++) {
        int position = -1;
        if(argument_names[argument][0]) {
            for(int i = 0; i < expected; i++)
                if(signature->parameters[i].name != NULL &&
                   !strcmp(signature->parameters[i].name,
                           argument_names[argument])) {
                    position = i;
                    break;
                }
        } else {
            for(int i = 0; i < expected; i++)
                if(bound[i] < 0) { position = i; break; }
        }
        if(position < 0 || bound[position] >= 0) return 0;
        bound[position] = argument;
    }
    /* Law evaluation reads module constants through the module itself, so
     * only non-law evaluation, which splices names into text, copies them. */
    int copy_defines = !ZirLawEvaluation;
    body.capacity = expected + (copy_defines ? owner->define_count : 0) +
                    fn->stmt_count + 1;
    body.names.items = malloc((size_t)body.capacity *
                              sizeof(*body.names.items));
    body.names.reads = malloc((size_t)body.capacity *
                              sizeof(*body.names.reads));
    if(body.names.items == NULL || body.names.reads == NULL)
        die("out of memory evaluating #run procedure");
    body.module = owner;
    body.fn = fn;
    body.depth = depth;
    body.fuel = fuel;
    const char *path = SpanPath(fn->span);
    for(int i = 0; i < expected; i++) {
        const TypedParameter *parameter = &signature->parameters[i];
        CompileValue *value = bound[i] >= 0 ? &arguments[bound[i]] :
                              &buffers->fallback;
        if(parameter->name == NULL) goto done;
        if(bound[i] < 0) {
            compile_value_clear(value);
            if(parameter->default_value == NULL) goto done;
            if(!strcmp(parameter->default_value, "#caller_location")) {
                value->kind = COMPILE_COMPOUND;
                copy_text(value->type, sizeof(value->type),
                          "Source_Code_Location");
                value->type_owner = owner;
                if(!CallerLocationLiteral(module, call_span, value->literal,
                                          sizeof(value->literal))) goto done;
            } else if(!evaluate_typed_expression(owner, &body.names,
                       parameter->default_value, fn->span, depth + 1, fuel,
                       value)) goto done;
        }
        if(!parameter->is_identifier ||
           !compile_type_value(parameter->type, value)) goto done;
        ZirConst *binding = &body.names.items[i];
        memset(binding, 0, sizeof(*binding));
        body.names.reads[i].expr = NULL;
        copy_text(binding->name, sizeof(binding->name), parameter->name);
        copy_text(binding->type, sizeof(binding->type), parameter->type);
        binding->expr = KeepText(value->literal);
        if(value->kind == COMPILE_INTEGER)
            enum_member_expr(owner, parameter->type, value->integer,
                             &binding->expr);
        copy_text(binding->path, sizeof(binding->path), path);
        bound_texts[i] = binding->expr;
        body.names.count++;
    }
    body.local_count = expected;
    for(int i = 0; copy_defines && i < owner->define_count; i++) {
        const ZirDefine *definition = &owner->defines[i];
        body.names.reads[body.names.count].expr = NULL;
        ZirConst *constant = &body.names.items[body.names.count++];
        memset(constant, 0, sizeof(*constant));
        copy_text(constant->name, sizeof(constant->name),
                  definition->name);
        constant->expr = KeepText(definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    /* Law evaluation reads only the bindings, so a call already evaluated
     * with the same bound texts is answered from it. */
    KeptCalls *calls = ZirLawEvaluation && compile_value_is_clear(result) ?
                       law_calls : NULL;
    uint64_t hash = 0;
    int before = *fuel;
    if(calls != NULL) {
        hash = call_hash(fn, bound_texts, expected);
        const KeptCall *call = find_call(calls, hash, fn, bound_texts, expected);
        if(call != NULL && kept_value_applies(&call->value, depth, fuel, result)) {
            ok = 1;
            goto done;
        }
    }
    ok = typed_body_statements(&body, 0, fn->stmt_count,
                               &flow, result) && flow == 1 &&
         compile_type_value(fn->return_type, result);
    if(ok && calls != NULL)
        keep_call(calls, hash, fn, bound_texts, expected, depth,
                  before - *fuel, result);
done:
    free(body.names.items);
    free(body.names.reads);
    return ok;
}

COMPILER_SCRATCH_RETURN(static, int, evaluate_typed_function,
    (const ZirModule *module, const char *name,
                        ZirSourceSpan call_span, CompileValue *arguments,
                        char argument_names[][ZIR_NAME_MAX],
                        int argument_count, int depth, int *fuel,
                        CompileValue *result),
    EvaluateTypedFunctionBuffers, RECOVERABLE, free,
    evaluate_typed_function_with_buffers(module, name, call_span, arguments, argument_names, argument_count, depth, fuel, result, buffers))
/* Buffers evaluate_typed_integer_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateTypedIntegerFunctionBuffers {
    CompileValue arguments[16];
    char names[16][ZIR_NAME_MAX];
} EvaluateTypedIntegerFunctionBuffers;

int evaluate_typed_integer_function(ZirEval *ev, const char *name,
                                const int64_t *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, int64_t *result);

static int
evaluate_typed_integer_function_with_buffers(ZirEval *ev, const char *name,
                                const int64_t *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, int64_t *result, EvaluateTypedIntegerFunctionBuffers *buffers)
{
    CompileValue value;
    compile_value_clear(&value);
    for(int i = 0; i < 16; i++) compile_value_clear(&buffers->arguments[i]);
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

COMPILER_SCRATCH_RETURN(, int, evaluate_typed_integer_function,
    (ZirEval *ev, const char *name,
                                const int64_t *values,
                                const char argument_names[][ZIR_NAME_MAX],
                                int argument_count, int64_t *result),
    EvaluateTypedIntegerFunctionBuffers, RECOVERABLE, free,
    evaluate_typed_integer_function_with_buffers(ev, name, values, argument_names, argument_count, result, buffers))

int
eval_typed_condition(const char *source, const ZirModule *module,
                     const ZirConsts *names, ZirSourceSpan span,
                     int64_t *result)
{
    int fuel = 10000, truth;
    CompileValue value;
    compile_value_clear(&value);
    if(!evaluate_typed_expression(module, names, source, span, 0,
                                  &fuel, &value) ||
       !compile_truth(&value, &truth)) return 0;
    *result = truth;
    return 1;
}

int
EvaluateCompileExpression(const ZirModule *module, const char *source,
                          ZirSourceSpan span, int executing, int64_t *value)
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
            constant->expr = KeepText(definition->value);
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
    CompileValue value;
    compile_value_clear(&value);
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
        constant->expr = KeepText(definition->value);
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

/* A law condition prepared once: its module's constants and one slot per
 * bound variable, and the condition parsed. Each case only rewrites the
 * slots' values. */
struct ZirBoundCondition {
    const ZirModule *module;
    ZirSourceSpan span;
    const char *path;
    ZirConsts names;
    int first_variable;
    int variable_count;
    const ZirType **enums;   /* each variable's enum type, or NULL */
    char (*values)[ZIR_TEXT_MAX];
    char input[ZIR_TEXT_MAX];
    ZirFunction scratch;
    const ZirFunction *probe; /* NULL when the condition cannot evaluate */
    int root;
    KeptCalls *calls;         /* kept across the law's cases */
};

ZirBoundCondition *
PrepareBoundCondition(const ZirModule *module, const char *source,
                      ZirSourceSpan span, const char names[][ZIR_NAME_MAX],
                      const char types[][ZIR_NAME_MAX], int count)
{
    if(module == NULL || source == NULL || count < 0 ||
       (count > 0 && (names == NULL || types == NULL)))
        return NULL;
    ZirBoundCondition *condition = calloc(1, sizeof(*condition));
    if(condition == NULL) return NULL;
    condition->module = module;
    condition->span = span;
    condition->path = SpanPath(span);
    condition->first_variable = module->define_count;
    condition->variable_count = count;
    condition->names.count = module->define_count + count;
    condition->names.items = calloc((size_t)condition->names.count + 1,
                                    sizeof(*condition->names.items));
    condition->names.reads = calloc((size_t)condition->names.count + 1,
                                    sizeof(*condition->names.reads));
    condition->enums = calloc((size_t)count + 1, sizeof(*condition->enums));
    condition->values = calloc((size_t)count + 1, sizeof(*condition->values));
    condition->calls = calloc(1, sizeof(*condition->calls));
    if(condition->names.items == NULL || condition->names.reads == NULL ||
       condition->enums == NULL || condition->values == NULL ||
       condition->calls == NULL) {
        FreeBoundCondition(condition);
        return NULL;
    }
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *definition = &module->defines[i];
        ZirConst *constant = &condition->names.items[i];
        copy_text(constant->name, sizeof(constant->name), definition->name);
        constant->expr = KeepText(definition->value);
        copy_text(constant->path, sizeof(constant->path),
                  SpanPath(definition->span));
        constant->is_file_private = definition->is_file_private;
    }
    for(int i = 0; i < count; i++) {
        ZirConst *variable = &condition->names.items[module->define_count + i];
        copy_text(variable->name, sizeof(variable->name), names[i]);
        copy_text(variable->type, sizeof(variable->type),
                  types[i][0] ? types[i] : "s64");
        copy_text(variable->path, sizeof(variable->path), condition->path);
        variable->expr = condition->values[i];
        condition->enums[i] = enum_type(module, types[i]);
    }
    /* As evaluate_typed_expression reads a top-level condition. */
    expression_input(condition->input, sizeof(condition->input), source);
    if(condition->input[0]) {
        condition->root = CachedParse(module, condition->input, span,
                                      &condition->scratch, &condition->probe);
        if(condition->root < 0 || holds_compile_time(condition->probe)) {
            CachedParseDone(condition->probe, &condition->scratch);
            condition->probe = NULL;
        }
    }
    return condition;
}

int
EvaluateBoundCondition(ZirBoundCondition *condition,
                       const char exprs[][ZIR_NAME_MAX], const int64_t *values,
                       int *truth)
{
    CompileValue value;
    compile_value_clear(&value);
    int fuel = 100000, ok;
    if(condition == NULL || truth == NULL ||
       (condition->variable_count > 0 && (exprs == NULL || values == NULL)))
        return 0;
    for(int i = 0; i < condition->variable_count; i++) {
        char text[ZIR_TEXT_MAX];
        if(exprs[i][0])
            copy_text(text, sizeof(text), exprs[i]);
        else if(!enum_member_spelling(condition->enums[i], values[i], text,
                                      sizeof(text)))
            snprintf(text, sizeof(text), "%" PRId64, values[i]);
        /* A variable's slot keeps its text, so a new value forgets the
         * value read from the old one. */
        if(strcmp(text, condition->values[i])) {
            copy_text(condition->values[i], ZIR_TEXT_MAX, text);
            condition->names.reads[condition->first_variable + i].expr = NULL;
        }
    }
    /* The top-level step evaluate_typed_expression takes before the tree. */
    if(--fuel < 0 || condition->probe == NULL) return 0;
    int prior = ZirLawEvaluation;
    const ZirConsts *outer_names = evaluation_names;
    KeptCalls *outer_calls = law_calls;
    ZirLawEvaluation = 1;
    evaluation_names = &condition->names;
    law_calls = condition->calls;
    ok = evaluate_typed_node(condition->probe, condition->root,
                             condition->module, condition->path, 1, &fuel,
                             &value) &&
         compile_truth(&value, truth);
    law_calls = outer_calls;
    evaluation_names = outer_names;
    ZirLawEvaluation = prior;
    return ok;
}

void
FreeBoundCondition(ZirBoundCondition *condition)
{
    if(condition == NULL) return;
    if(condition->probe != NULL)
        CachedParseDone(condition->probe, &condition->scratch);
    if(condition->calls != NULL) {
        free(condition->calls->slots);
        free(condition->calls->bytes);
    }
    free(condition->calls);
    free(condition->names.items);
    free(condition->names.reads);
    free(condition->enums);
    free(condition->values);
    free(condition);
}

/* Evaluate a boolean expression with integer variables bound, for `forall`
 * laws. Bindings shadow nothing: a bound name must not be a define. */
int
EvaluateCompileConditionBound(const ZirModule *module, const char *source,
                              ZirSourceSpan span, const char names[][ZIR_NAME_MAX],
                              const char types[][ZIR_NAME_MAX],
                              const char exprs[][ZIR_NAME_MAX],
                              const int64_t *values, int count, int *truth)
{
    if(count > 0 && (exprs == NULL || values == NULL)) return 0;
    ZirBoundCondition *condition = PrepareBoundCondition(module, source, span,
                                                         names, types, count);
    int ok = EvaluateBoundCondition(condition, exprs, values, truth);
    FreeBoundCondition(condition);
    return ok;
}

int CachedParse(const ZirModule *module, const char *text, ZirSourceSpan span, ZirFunction *scratch, const ZirFunction **probe)
{
    CompilerMemory *memory = CompilerMemoryPause();
    int result = CachedParse_persistent(module, text, span, scratch, probe);
    CompilerMemoryResume(memory);
    return result;
}

void *ParseCacheEnter(void)
{
    CompilerMemory *memory = CompilerMemoryPause();
    ExpressionCache *previous = malloc(sizeof(*previous));
    if(previous) {
        ExpressionCache *cache = expression_cache();
        *previous = *cache;
        memset(cache, 0, sizeof(*cache));
    }
    CompilerMemoryResume(memory);
    return previous;
}

void ParseCacheLeave(void *saved)
{
    CompilerMemory *memory = CompilerMemoryPause();
    ExpressionCache *cache = expression_cache();
    free_expression_cache(cache);
    *cache = *(ExpressionCache *)saved;
    free(saved);
    CompilerMemoryResume(memory);
}
