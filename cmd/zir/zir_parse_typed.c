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
/* The constants a condition's defined(NAME) probes, in either mode. */
static _Thread_local const ZirConsts *condition_names;

/* Storage one #run evaluation owns besides its value texts: the bytes of
 * each Vec(u8) builder, and the bindings pointers and slices refer to. A
 * value names either through a marker literal, "\x1fB<n>\x1f" for builder n
 * or "\x1fP<n>\x1f" for reference n, the way a long literal travels as a
 * raw-text marker. Source cannot spell one (string text escapes control
 * bytes), and no marker leaves the outermost evaluation, which frees the
 * store. */
typedef struct CompileReference {
    TypedBody *body; /* NULL once that call has returned */
    int serial;
} CompileReference;

typedef struct CompileStore {
    CompileText *builders;
    int builder_count, builder_capacity;
    CompileReference *references;
    int reference_count, reference_capacity;
    int users;
} CompileStore;

static _Thread_local CompileStore compile_store;
/* The procedure calls being evaluated, innermost first. */
static _Thread_local TypedBody *active_bodies;

static void
compile_store_leave(void)
{
    if(--compile_store.users > 0) return;
    for(int i = 0; i < compile_store.builder_count; i++)
        free(compile_store.builders[i].data);
    free(compile_store.builders);
    free(compile_store.references);
    memset(&compile_store, 0, sizeof(compile_store));
}

static int
compile_marker(CompileValue *value, CompileKind kind, int index)
{
    char text[32];
    snprintf(text, sizeof(text), "\"\x1f" "%c%d\x1f\"",
             kind == COMPILE_BUILDER ? 'B' : 'P', index);
    value->kind = kind;
    value->integer = index;
    copy_text(value->type, sizeof(value->type),
              kind == COMPILE_BUILDER ? "Vec(u8)" : "");
    compile_set_literal(value, text);
    return 1;
}

/* The builder or reference a marker literal names, or 0 for other text. */
static int
compile_marker_value(const char *literal, CompileValue *value)
{
    if(literal[0] != '"' || literal[1] != '\x1f' ||
       (literal[2] != 'B' && literal[2] != 'P') ||
       !isdigit((unsigned char)literal[3])) return 0;
    char *end;
    long index = strtol(literal + 3, &end, 10);
    if(end[0] != '\x1f' || end[1] != '"' || end[2] != '\0') return 0;
    CompileKind kind = literal[2] == 'B' ? COMPILE_BUILDER : COMPILE_REFERENCE;
    if(index >= (kind == COMPILE_BUILDER ? compile_store.builder_count :
                 compile_store.reference_count)) return 0;
    return compile_marker(value, kind, (int)index);
}

static int
compile_new_builder(CompileValue *value)
{
    CompileStore *store = &compile_store;
    if(store->users == 0) return 0;
    if(store->builder_count == store->builder_capacity) {
        int capacity = store->builder_capacity ? store->builder_capacity * 2 : 16;
        CompileText *grown = realloc(store->builders,
                                     (size_t)capacity * sizeof(*grown));
        if(grown == NULL) return 0;
        store->builders = grown;
        store->builder_capacity = capacity;
    }
    store->builders[store->builder_count] = (CompileText){0};
    return compile_marker(value, COMPILE_BUILDER, store->builder_count++);
}

static CompileText *
compile_builder(const CompileValue *value)
{
    return value->kind == COMPILE_BUILDER && value->integer >= 0 &&
           value->integer < compile_store.builder_count ?
           &compile_store.builders[value->integer] : NULL;
}

static int
compile_new_reference(CompileValue *value, TypedBody *body, int serial)
{
    CompileStore *store = &compile_store;
    if(store->users == 0) return 0;
    if(store->reference_count == store->reference_capacity) {
        int capacity = store->reference_capacity ? store->reference_capacity * 2 : 16;
        CompileReference *grown = realloc(store->references,
                                          (size_t)capacity * sizeof(*grown));
        if(grown == NULL) return 0;
        store->references = grown;
        store->reference_capacity = capacity;
    }
    store->references[store->reference_count] = (CompileReference){body, serial};
    return compile_marker(value, COMPILE_REFERENCE, store->reference_count++);
}

/* The binding a live reference names, and the call that holds it. */
static ZirConst *
compile_referenced(const CompileValue *value, TypedBody **owner)
{
    if(value->kind != COMPILE_REFERENCE || value->integer < 0 ||
       value->integer >= compile_store.reference_count) return NULL;
    const CompileReference *reference = &compile_store.references[value->integer];
    TypedBody *body = reference->body;
    for(int i = 0; body != NULL && i < body->local_count; i++)
        if(body->serials[i] == reference->serial) {
            *owner = body;
            return &body->names.items[i];
        }
    return NULL;
}

/* The call whose bindings the expression being evaluated reads. */
static TypedBody *
current_body(void)
{
    for(TypedBody *body = active_bodies; body != NULL; body = body->outer)
        if(&body->names == evaluation_names) return body;
    return NULL;
}

/* Whether NAME is a constant visible from PATH, as defined(NAME) asks. */
static int
compile_name_defined(const ZirModule *module, const char *path, const char *name)
{
    const ZirConsts *names = condition_names;
    for(int i = 0; names != NULL && i < names->count; i++)
        if(!strcmp(names->items[i].name, name) &&
           (!names->items[i].is_file_private || !strcmp(names->items[i].path, path)))
            return 1;
    for(int i = 0; module != NULL && i < module->define_count; i++)
        if(!strcmp(module->defines[i].name, name) &&
           (!module->defines[i].is_file_private ||
            !strcmp(SpanPath(module->defines[i].span), path)))
            return 1;
    return 0;
}

/* Language OS/CPU/TARGET are selected-output facts. Host facts live only
 * under Compiler, so cross-compilation never selects a host branch. */
static int
compile_target_value(const char *name, CompileValue *result)
{
    static const char *const os_members[] = {
        ".NONE", ".LINUX", ".WINDOWS", ".MACOS", ".ANDROID", ".WEB"
    };
    static const char *const cpu_members[] = {
        ".NONE", ".X64", ".ARM64", ".WASM32"
    };
    static const char *const target_members[] = {".C", ".GO", ".WASM", ".ZX"};
    ZirTargetFacts facts = CompilerTargetFacts();
    int64_t value = 0;
    const char *type = NULL;
    if(!strcmp(name, "OS")) {
        value = facts.os;
        type = "OS";
    } else if(!strcmp(name, "CPU")) {
        value = facts.cpu;
        type = "CPU";
    } else if(!strcmp(name, "TARGET")) {
        value = facts.target;
        type = "TARGET";
    } else {
        for(int i = ZIR_OS_NONE; i <= ZIR_OS_WEB && type == NULL; i++)
            if(!strcmp(name, os_members[i])) { value = i; type = "OS"; }
        for(int i = ZIR_CPU_NONE; i <= ZIR_CPU_WASM32 && type == NULL; i++)
            if(!strcmp(name, cpu_members[i])) { value = i; type = "CPU"; }
        for(int i = ZIR_TARGET_C; i <= ZIR_TARGET_ZX && type == NULL; i++)
            if(!strcmp(name, target_members[i])) { value = i; type = "TARGET"; }
    }
    if(type == NULL) return 0;
    result->kind = COMPILE_INTEGER;
    result->integer = value;
    copy_text(result->type, sizeof(result->type), type);
    return compile_value_literal(result);
}

static int
compile_compiler_value(const char *member, CompileValue *result)
{
    ZirTargetFacts facts = CompilerTargetFacts();
    const char *type = NULL;
    int64_t value = 0;
    if(!strcmp(member, "HostOS")) { value = CompilerHostOS(); type = "OS"; }
    else if(!strcmp(member, "HostCPU")) { value = CompilerHostCPU(); type = "CPU"; }
    else if(!strcmp(member, "Target")) { value = facts.target; type = "TARGET"; }
    else if(!strcmp(member, "TargetOS")) { value = facts.os; type = "OS"; }
    else if(!strcmp(member, "TargetCPU")) { value = facts.cpu; type = "CPU"; }
    else if(!strcmp(member, "Options")) {
        char literal[160];
        int written = snprintf(literal, sizeof(literal), "\"%s\"",
                               facts.options ? facts.options : "");
        if(written < 0 || (size_t)written >= sizeof(literal)) return 0;
        result->kind = COMPILE_STRING;
        copy_text(result->type, sizeof(result->type), "string");
        compile_set_literal(result, literal);
        return 1;
    } else return 0;
    result->kind = COMPILE_INTEGER;
    result->integer = value;
    copy_text(result->type, sizeof(result->type), type);
    return compile_value_literal(result);
}

int evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result);
static int compile_literal_member(const CompileValue *compound, const char *member,
                                  const ZirModule *module, const char *path,
                                  int depth, int *fuel, CompileValue *result);
static int compile_literal_index(const CompileValue *compound, int64_t index,
                                 const ZirModule *module, const char *path,
                                 int depth, int *fuel, CompileValue *result);

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
           !compile_literal(value)[0] && value->type_owner == NULL;
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
    compile_set_literal(result, kept->literal);
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
                     KeepText(compile_literal(value)));
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

/* BINDING's value: `cast(TYPE) (value)` for scalars and bools, otherwise
 * `(value)`. */
static int evaluate_cached_binding(const ZirModule *module,
                                   const ZirConsts *names,
                                   const ZirConst *binding, const char *path,
                                   int depth, int *fuel, CompileValue *result);

static int
evaluate_binding(const ZirModule *module, const ZirConsts *names,
                 const ZirConst *binding, const char *path, int depth,
                 int *fuel, CompileValue *result)
{
    if(compile_marker_value(binding->expr, result)) return 1;
    CompileText text = {0};
    if(ScalarWidth(binding->type) || !strcmp(binding->type, "bool")) {
        compile_text_put(&text, "cast(");
        compile_text_put(&text, binding->type);
        compile_text_put(&text, ") ");
    }
    compile_text_put(&text, "(");
    compile_text_put(&text, binding->expr);
    compile_text_put(&text, ")");
    int ok = !text.failed &&
             evaluate_typed_expression(module, names, text.data,
                                       Span(binding->path[0] ? binding->path : path, 1, 1),
                                       depth + 1, fuel, result);
    free(text.data);
    return ok;
}

/* VALUE in place of the reference it holds: the binding's current value. */
static int
compile_dereference(CompileValue *value, int depth, int *fuel)
{
    TypedBody *owner = NULL;
    const ZirConst *binding = compile_referenced(value, &owner);
    if(binding == NULL) return 0;
    CompileValue target;
    compile_value_clear(&target);
    if(!evaluate_cached_binding(owner->module, &owner->names, binding,
                                SpanPath(owner->fn->span), depth, fuel,
                                &target) ||
       target.kind == COMPILE_REFERENCE) return 0;
    *value = target;
    return 1;
}

static int
evaluate_cached_binding(const ZirModule *module, const ZirConsts *names,
                        const ZirConst *binding, const char *path, int depth,
                        int *fuel, CompileValue *result)
{
    size_t index = (size_t)(binding - names->items);
    ZirBoundRead *read = names->reads != NULL && binding->path[0] &&
                         index < (size_t)names->count &&
                         compile_value_is_clear(result) ?
                         &names->reads[index] : NULL;
    if(read != NULL && read->expr == binding->expr &&
       read->module == module &&
       kept_value_applies(&read->value, depth, fuel, result))
        return 1;
    unsigned long lookups = binding_lookups;
    int before = *fuel;
    int ok = evaluate_binding(module, names, binding, path, depth, fuel,
                              result);
    if(ok && read != NULL && binding_lookups == lookups) {
        read->expr = binding->expr;
        read->module = module;
        keep_value(&read->value, depth, before - *fuel, result);
    }
    return ok;
}

static int
compile_bool(CompileValue *result, int truth)
{
    result->kind = COMPILE_INTEGER;
    result->integer = truth != 0;
    copy_text(result->type, sizeof(result->type), "bool");
    return compile_value_literal(result);
}

/* A pointer or full slice `*name` or `name[:]` to a binding of the call
 * being evaluated. A Vec(u8) is its own reference, and a slice of a slice
 * the same one. */
static int
compile_reference_to(const char *name, int pointer, CompileValue *result)
{
    TypedBody *body = current_body();
    if(body == NULL) return 0;
    for(int i = 0; i < body->local_count; i++) {
        const ZirConst *binding = &body->names.items[i];
        if(strcmp(binding->name, name)) continue;
        /* A reference passed in names its storage already. */
        if(compile_marker_value(binding->expr, result)) {
            copy_text(result->type, sizeof(result->type), binding->type);
            return 1;
        }
        char element[ZIR_NAME_MAX];
        int capacity = 0;
        if(!pointer &&
           !ArrayElementType(binding->type, element, sizeof(element), &capacity))
            return 0;
        if(!compile_new_reference(result, body, body->serials[i])) return 0;
        snprintf(result->type, sizeof(result->type), "%s%s",
                 pointer ? "*" : "[]", pointer ? binding->type : element);
        return 1;
    }
    return 0;
}

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
        return evaluate_cached_binding(module, names, binding, path, depth,
                                       fuel, result);
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

/* The builder primitives on a #run Vec(u8): BuilderAppend, BuilderFinish,
 * and the Append that BuilderPrint expands to, for the string, integer and
 * bool texts std/format writes. Returns -1 for any other call. */
static int
compile_builder_call(const char *name, CompileValue *args, int count,
                     CompileValue *result)
{
    int append = !strcmp(name, "BuilderAppend");
    int finish = !strcmp(name, "BuilderFinish");
    if(!append && !finish && strcmp(name, "Append")) return -1;
    CompileText *builder = count >= 1 ? compile_builder(&args[0]) : NULL;
    if(builder == NULL) return append || finish ? 0 : -1;
    if(finish) {
        if(count != 1) return 0;
        CompileText text = {0};
        compile_text_string(&text, (const unsigned char *)builder->data,
                            builder->used);
        builder->used = 0;
        result->kind = COMPILE_STRING;
        if(!text.failed) compile_set_literal(result, text.data);
        free(text.data);
        return !text.failed;
    }
    if(count != 2) return append ? 0 : -1;
    const CompileValue *value = &args[1];
    if(value->kind == COMPILE_STRING) {
        size_t length = 0;
        unsigned char *bytes = compile_string_bytes(value, &length);
        if(bytes == NULL) return 0;
        compile_text_add(builder, (const char *)bytes, length);
        free(bytes);
        return !builder->failed;
    }
    if(append || value->kind != COMPILE_INTEGER) return append ? 0 : -1;
    compile_text_put(builder, !strcmp(value->type, "bool") ?
                     (value->integer ? "true" : "false") :
                     compile_literal(value));
    return !builder->failed;
}

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
        return !ZirLawEvaluation || !strcmp(compile_literal(result), expression->text);
    case ZIR_EXPR_STRING:
        if(compile_marker_value(expression->text, result)) return 1;
        result->kind = COMPILE_STRING;
        compile_set_literal(result, expression->text);
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
           !strcmp(expression->name, "false"))
            return compile_bool(result, !strcmp(expression->name, "true"));
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
                    expression->name, depth + 1, fuel, result) ||
               compile_target_value(expression->name, result);
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
        /* The parser spells address-of `*x` as "&" and `<<p` as "*". */
        if(!strcmp(expression->op, "&") &&
           operand->kind == ZIR_EXPR_IDENT && evaluation_names != NULL)
            return compile_reference_to(operand->name, 1, result);
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(right.kind == COMPILE_BUILDER &&
           (!strcmp(expression->op, "&") || !strcmp(expression->op, "*"))) {
            *result = right;
            return 1;
        }
        if(!strcmp(expression->op, "*")) {
            if(!compile_dereference(&right, depth + 1, fuel)) return 0;
            *result = right;
            return 1;
        }
        if(right.kind == COMPILE_BUILDER || right.kind == COMPILE_REFERENCE)
            return 0;
        if(!strcmp(expression->op, "!")) {
            if(!compile_truth(&right, &truth)) return 0;
            return compile_bool(result, !truth);
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
               (!strcmp(op, "||") && truth))
                return compile_bool(result, truth);
        }
        if(!evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right)) return 0;
        if(!strcmp(op, "==") || !strcmp(op, "!=")) {
            int equal;
            if(left.kind == COMPILE_BUILDER || left.kind == COMPILE_REFERENCE ||
               !compile_values_equal(&left, &right, &equal)) return 0;
            return compile_bool(result, !strcmp(op, "==") ? equal : !equal);
        }
        if(!strcmp(op, "&&") || !strcmp(op, "||")) {
            if(!compile_truth(&right, &truth)) return 0;
            return compile_bool(result, truth);
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
            if(left.kind == COMPILE_INTEGER && right.kind == COMPILE_INTEGER)
                return compile_bool(result, !strcmp(op, "<") ? cmp < 0 :
                                  !strcmp(op, "<=") ? cmp <= 0 :
                                  !strcmp(op, ">") ? cmp > 0 : cmp >= 0);
            return compile_bool(result, !strcmp(op, "<") ? a < b :
                                  !strcmp(op, "<=") ? a <= b :
                                  !strcmp(op, ">") ? a > b : a >= b);
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
            if(!strcmp(probe->exprs[expression->left].name, "Compiler") &&
               compile_compiler_value(expression->name, result))
                return 1;
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
                                depth + 1, fuel, &left) ||
           (left.kind == COMPILE_REFERENCE &&
            !compile_dereference(&left, depth + 1, fuel))) return 0;
        if(left.kind == COMPILE_BUILDER) {
            const CompileText *builder = compile_builder(&left);
            if(builder == NULL || strcmp(expression->name, "count")) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = (int64_t)builder->used;
            return compile_value_literal(result);
        }
        if(left.kind == COMPILE_STRING &&
           !strcmp(expression->name, "count")) {
            size_t length = 0;
            unsigned char *bytes = compile_string_bytes(&left, &length);
            free(bytes);
            if(bytes == NULL || length > LONG_MAX) return 0;
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
            if(compile_literal_member(&left, expression->name, module, path,
                                      depth, fuel, result))
                return 1;
            return compile_compound_member(&left, expression->name,
                                           module, path, depth, fuel, result);
        }
        return 0;
    case ZIR_EXPR_SLICE: {
        /* Only the whole of an array local: `name[:]`. */
        const ZirExpr *base = expression->left >= 0 ?
            &probe->exprs[expression->left] : NULL;
        if(base == NULL || base->kind != ZIR_EXPR_IDENT ||
           expression->right >= 0 || expression->third >= 0) return 0;
        return compile_reference_to(base->name, 0, result);
    }
    case ZIR_EXPR_INDEX:
        if(!evaluate_typed_node(probe, expression->left, module, path,
                                depth + 1, fuel, &left) ||
           (left.kind == COMPILE_REFERENCE &&
            !compile_dereference(&left, depth + 1, fuel)) ||
           !evaluate_typed_node(probe, expression->right, module, path,
                                depth + 1, fuel, &right) ||
           left.kind != COMPILE_COMPOUND ||
           right.kind != COMPILE_INTEGER) return 0;
        if(compile_literal_index(&left, right.integer, module, path,
                                 depth, fuel, result))
            return 1;
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
        if(!strcmp(name, "defined")) {
            int probe_name = expression->first_child;
            if(probe_name < 0 || probe->exprs[probe_name].kind != ZIR_EXPR_IDENT ||
               probe->exprs[probe_name].next_sibling >= 0) return 0;
            result->kind = COMPILE_INTEGER;
            result->integer = compile_name_defined(module, path,
                                                   probe->exprs[probe_name].name);
            return compile_value_literal(result);
        }
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
        int builtin = compile_builder_call(name, buffers->args, count, result);
        if(builtin >= 0) return builtin;
        ZirSourceSpan call_span = expression->left >= 0 ?
            probe->exprs[expression->left].span : expression->span;
        return evaluate_typed_function(module, name, call_span, buffers->args, buffers->names,
                                       count, depth + 1, fuel, result);
    }
    default:
        return 0;
    }
}

/* A returned record need not be spelled as the return type spells it: the
 * procedure may name the type through an import, `Pad.Hold` for a `Hold`
 * made in `pad`. */
static int
compile_return_value(const ZirModule *module, const char *type, CompileValue *value)
{
    if(compile_type_value(type, value)) return 1;
    if(value->kind != COMPILE_COMPOUND || !value->type[0]) return 0;
    const ZirType *declared = FindType(module, type, NULL);
    const ZirModule *made_in = value->type_owner != NULL ? value->type_owner : module;
    return declared != NULL && FindType(made_in, value->type, NULL) == declared;
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
} EvaluateTypedExpressionBuffers;

int evaluate_typed_expression(const ZirModule *module, const ZirConsts *names,
                          const char *source, ZirSourceSpan span, int depth,
                          int *fuel, CompileValue *result);

/* SOURCE as an expression evaluates it: trimmed, without a final ';'. The
 * caller frees it. */
static char *
expression_input(const char *source)
{
    char *out = strdup(source);
    if(out == NULL) return NULL;
    trim_in_place(out);
    size_t length = strlen(out);
    if(length && out[length - 1] == ';') {
        out[length - 1] = '\0';
        trim_in_place(out);
    }
    return out;
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
    int root, ok;
    const char *path = SpanPath(span);
    if(depth > 32 || source == NULL || --*fuel < 0) return 0;
    char *text = expression_input(source);
    if(text == NULL || !text[0]) {
        free(text);
        return 0;
    }
    if(names == NULL) names = &empty;
    const ZirConsts *outer_names = evaluation_names;
    const ZirConsts *outer_conditions = condition_names;
    condition_names = names;
    evaluation_names = names;
    compile_store.users++;
    const ZirFunction *probe;
    root = CachedParse(module, text, span, &buffers->probe, &probe);
    ok = root >= 0 && (depth != 0 || !holds_compile_time(probe));
    if(ok)
        ok = evaluate_typed_node(probe, root, module, path,
                                 depth + 1, fuel, result);
    CachedParseDone(probe, &buffers->probe);
    free(text);
    /* A builder or reference ends with the evaluation that made it. */
    if(compile_store.users == 1 && ok &&
       strchr(compile_literal(result), '\x1f') != NULL) ok = 0;
    compile_store_leave();
    evaluation_names = outer_names;
    condition_names = outer_conditions;
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
    memmove(body->serials, body->serials + removed,
            (size_t)(body->names.count - removed) * sizeof(*body->serials));
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
    memmove(body->serials + 1, body->serials,
            (size_t)body->names.count * sizeof(*body->serials));
    body->names.reads[0].expr = NULL;
    body->serials[0] = body->next_serial++;
    ZirConst *local = &body->names.items[0];
    memset(local, 0, sizeof(*local));
    local->expr = "";
    copy_text(local->name, sizeof(local->name), name);
    copy_text(local->type, sizeof(local->type), type);
    copy_text(local->path, sizeof(local->path), SpanPath(body->fn->span));
    if(value != NULL)
        local->expr = KeepText(compile_literal(value));
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
zero_array_literal(const char *type, CompileText *out)
{
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(!ArrayElementType(type, element, sizeof(element), &capacity) ||
       capacity < 1 || !eval_integer_type(element, 0))
        return 0;
    compile_text_put(out, element);
    compile_text_put(out, ".[");
    for(int i = 0; i < capacity; i++)
        compile_text_put(out, i ? ", 0" : "0");
    compile_text_put(out, "]");
    return !out->failed;
}

/* The extent of each element of a canonical array literal `T.[a, b, c]`:
 * calls VISIT with the element's text until it returns 0. Returns the
 * element count, or -1 when LITERAL is not such a literal. */
static int64_t
array_elements(const char *literal,
               int (*visit)(void *context, int64_t ordinal,
                            const char *start, size_t length),
               void *context)
{
    const char *cursor = strstr(literal, ".[");
    int64_t ordinal = 0;
    if(cursor == NULL) return -1;
    cursor += 2;
    if(*skip_ws(cursor) == ']') return 0;
    for(;;) {
        int depth = 0, quoted = 0;
        cursor = skip_ws(cursor);
        const char *start = cursor;
        while(*cursor && (quoted || depth > 0 ||
                          (*cursor != ',' && *cursor != ']'))) {
            if(quoted) {
                if(*cursor == '\\' && cursor[1]) cursor++;
                else if(*cursor == '"') quoted = 0;
            } else if(*cursor == '"') quoted = 1;
            else if(strchr("([{", *cursor)) depth++;
            else if(strchr(")]}", *cursor)) depth--;
            cursor++;
        }
        if(*cursor == '\0') return -1;
        size_t length = (size_t)(cursor - start);
        while(length && isspace((unsigned char)start[length - 1])) length--;
        if(visit != NULL && !visit(context, ordinal, start, length))
            return ordinal + 1;
        ordinal++;
        if(*cursor == ']') return ordinal;
        cursor++;
    }
}

typedef struct ElementEdit {
    int64_t index;
    const char *replacement;
    CompileText *out;
    int found;
} ElementEdit;

static int
copy_element(void *context, int64_t ordinal, const char *start, size_t length)
{
    ElementEdit *edit = context;
    if(ordinal) compile_text_put(edit->out, ", ");
    if(ordinal == edit->index) {
        compile_text_put(edit->out, skip_ws(edit->replacement));
        edit->found = 1;
    } else compile_text_add(edit->out, start, length);
    return 1;
}

static int
take_element(void *context, int64_t ordinal, const char *start, size_t length)
{
    ElementEdit *edit = context;
    if(ordinal != edit->index) return 1;
    compile_text_add(edit->out, start, length);
    edit->found = 1;
    return 0;
}

/* Replace element `index` of a canonical array literal `T.[a, b, c]`. */
static int
store_array_element(const char *literal, int64_t index, const char *replacement,
                    CompileText *out)
{
    const char *open = strstr(literal, ".[");
    ElementEdit edit = {index, replacement, out, 0};
    if(open == NULL || index < 0) return 0;
    compile_text_add(out, literal, (size_t)(open - literal) + 2);
    if(array_elements(literal, copy_element, &edit) < 0) return 0;
    compile_text_put(out, "]");
    return edit.found && !out->failed;
}

/* Text of element `index` of a canonical array literal `T.[a, b, c]`. */
static int
array_element_text(const char *literal, int64_t index, CompileText *out)
{
    ElementEdit edit = {index, NULL, out, 0};
    return index >= 0 && array_elements(literal, take_element, &edit) >= 0 &&
           edit.found && !out->failed;
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
 * evaluator writes it, added to OUT. Returns 1 when present, 0 when absent,
 * -1 when the text is not such a literal. */
static int
record_field_text(const char *literal, const char *name, CompileText *out)
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
            compile_text_add(out, value, length);
            return out->failed ? -1 : 1;
        }
        if(*cursor == ',') cursor++;
    }
}

static int
compile_literal_member(const CompileValue *compound, const char *member,
                       const ZirModule *module, const char *path, int depth,
                       int *fuel, CompileValue *result)
{
    const ZirModule *owner = NULL;
    const ZirType *record = literal_record(module, compound->type, &owner);
    if(record == NULL) return 0;
    CompileText text = {0};
    int present = record_field_text(compile_literal(compound), member, &text);
    int found = 0;
    if(present > 0) {
        ZirTypeField field;
        size_t offset = 0;
        while(TypeNextField(record, &offset, &field) == 1) {
            if(strcmp(field.name, member)) continue;
            found = evaluate_typed_expression(owner, evaluation_names, text.data,
                                              Span(path, 1, 1), depth + 1,
                                              fuel, result) &&
                    compile_type_value(field.type, result);
            break;
        }
    }
    free(text.data);
    return present > 0 && found;
}

static int
compile_literal_index(const CompileValue *compound, int64_t index,
                      const ZirModule *module, const char *path, int depth,
                      int *fuel, CompileValue *result)
{
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(!ArrayElementType(compound->type, element, sizeof(element), &capacity) ||
       index < 0 || index >= capacity) return 0;
    CompileText text = {0};
    int found = array_element_text(compile_literal(compound), index, &text);
    int ok = found &&
             evaluate_typed_expression(module, evaluation_names, text.data,
                                       Span(path, 1, 1), depth + 1, fuel,
                                       result) &&
             compile_type_value(element, result);
    free(text.data);
    return ok;
}

static int record_literal(const ZirModule *module, const ZirType *record,
                          const char *type, const char *literal,
                          const char *path, const char *replacement,
                          CompileText *out, int depth);

/* Zero text for a value of `type`, as a zero-initialized local holds it:
 * 0 for integers, bools and enums, 0.0 for floats, "" for strings, and
 * arrays and records of zeros. */
static int
zero_value_literal(const ZirModule *module, const char *type, CompileText *out,
                   int depth)
{
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    if(depth > 16) return 0;
    if(!strcmp(type, "float32") || !strcmp(type, "float64")) {
        compile_text_put(out, "0.0");
        return !out->failed;
    }
    if(ScalarWidth(type) || !strcmp(type, "bool")) {
        compile_text_put(out, "0");
        return !out->failed;
    }
    if(!strcmp(type, "string")) {
        compile_text_put(out, "\"\"");
        return !out->failed;
    }
    if(!strcmp(type, "Vec(u8)")) {
        /* Each zero Vec(u8) is a builder of its own. */
        CompileValue builder;
        compile_value_clear(&builder);
        if(!compile_new_builder(&builder)) return 0;
        compile_text_put(out, compile_literal(&builder));
        return !out->failed;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        CompileText zero = {0};
        int ok = capacity >= 1 &&
                 zero_value_literal(module, element, &zero, depth + 1);
        /* Elements holding builders each need their own. */
        int fresh = ok && strchr(zero.data, '\x1f') != NULL;
        if(ok) {
            compile_text_put(out, element);
            compile_text_put(out, ".[");
            for(int i = 0; ok && i < capacity; i++) {
                if(i) compile_text_put(out, ", ");
                if(fresh && i) {
                    zero.used = 0;
                    ok = zero_value_literal(module, element, &zero, depth + 1);
                }
                compile_text_add(out, zero.data, zero.used);
            }
            compile_text_put(out, "]");
        }
        free(zero.data);
        return ok && !out->failed;
    }
    const ZirModule *owner = NULL;
    const ZirType *found = FindType(module, type, NULL);
    if(found != NULL && found->is_enum) {
        compile_text_put(out, "0");
        return !out->failed;
    }
    const ZirType *record = literal_record(module, type, &owner);
    return record != NULL &&
           record_literal(owner, record, type, NULL, NULL, NULL, out,
                          depth + 1);
}

/* A record literal in canonical form: every field in declaration order,
 * taken from `literal` or zero when it is absent there, so equal records
 * have equal text. A `path` of field names replaces that field's value with
 * `replacement`, descending into nested records for `a.b`. */
static int
record_literal(const ZirModule *module, const ZirType *record,
               const char *type, const char *literal, const char *path,
               const char *replacement, CompileText *out, int depth)
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
    compile_text_put(out, type);
    compile_text_put(out, ".{");
    int ordinal = 0;
    while(ok && !out->failed && TypeNextField(record, &offset, &field) == 1) {
        CompileText current = {0};
        int present = literal != NULL ?
            record_field_text(literal, field.name, &current) : 0;
        if(present < 0 ||
           (!present && !zero_value_literal(module, field.type, &current,
                                            depth + 1))) ok = 0;
        if(ordinal) compile_text_put(out, ", ");
        compile_text_put(out, ".");
        compile_text_put(out, field.name);
        compile_text_put(out, " = ");
        if(ok && path != NULL && !strcmp(field.name, head)) {
            replaced = 1;
            if(rest == NULL) compile_text_put(out, replacement);
            else {
                const ZirModule *owner = NULL;
                const ZirType *inner = literal_record(module, field.type,
                                                      &owner);
                ok = inner != NULL &&
                     record_literal(owner, inner, field.type, current.data,
                                    rest, replacement, out, depth + 1);
            }
        } else if(ok) compile_text_add(out, current.data, current.used);
        free(current.data);
        ordinal++;
    }
    compile_text_put(out, "}");
    return ok && replaced && !out->failed;
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
    CompileText text = {0};
    if(record_literal(owner, record, value->type, compile_literal(value), NULL,
                      NULL, &text, 0))
        compile_set_literal(value, text.data);
    free(text.data);
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
    char control[ZIR_TEXT_MAX];
} TypedBodyStatementsBuffers;

/* `(LEFT) OP (RIGHT)`, the expression `LEFT OP= RIGHT` assigns. */
static int
typed_compound_assignment(TypedBody *body, const char *left, char op,
                          const char *right, CompileValue *value)
{
    CompileText text = {0};
    char spelled[2] = {op, '\0'};
    compile_text_put(&text, "(");
    compile_text_put(&text, left);
    compile_text_put(&text, ") ");
    compile_text_put(&text, spelled);
    compile_text_put(&text, " (");
    compile_text_put(&text, right);
    compile_text_put(&text, ")");
    int ok = !text.failed && typed_body_expression(body, text.data, value);
    free(text.data);
    return ok;
}

/* The literal of TYPE value LITERAL with the part at STEPS, field names
 * after '.' and indexes, replaced by VALUE. */
typedef struct StoreStep {
    char field[ZIR_NAME_MAX]; /* empty for an index */
    int64_t index;
} StoreStep;

static int
store_at_path(const ZirModule *module, const char *type, const char *literal,
              const StoreStep *steps, int count, CompileValue *value,
              CompileText *out)
{
    if(count == 0) {
        if(!compile_type_value(type, value)) return 0;
        compile_text_put(out, compile_literal(value));
        return !out->failed;
    }
    char inner_type[ZIR_NAME_MAX];
    CompileText inner = {0}, stored = {0};
    int ok;
    if(steps[0].field[0]) {
        const ZirModule *owner = NULL;
        const ZirType *record = literal_record(module, type, &owner);
        ok = record != NULL &&
             record_path_type(owner, record, steps[0].field, inner_type,
                              sizeof(inner_type));
        if(ok) {
            int present = record_field_text(literal, steps[0].field, &inner);
            ok = present > 0 ||
                 (present == 0 &&
                  zero_value_literal(owner, inner_type, &inner, 0));
        }
        ok = ok && store_at_path(owner, inner_type, inner.data, steps + 1,
                                 count - 1, value, &stored) &&
             record_literal(owner, record, type, literal, steps[0].field,
                            stored.data, out, 0);
    } else {
        int capacity = 0;
        ok = ArrayElementType(type, inner_type, sizeof(inner_type), &capacity) &&
             steps[0].index >= 0 && steps[0].index < capacity &&
             array_element_text(literal, steps[0].index, &inner) &&
             store_at_path(module, inner_type, inner.data, steps + 1,
                           count - 1, value, &stored) &&
             store_array_element(literal, steps[0].index, stored.data, out);
    }
    free(inner.data);
    free(stored.data);
    return ok && !out->failed;
}

/* `TARGET = SOURCE`, or `TARGET op= SOURCE`, where TARGET is a local with
 * fields `.name` and indexes `[i]` after it, reached through the pointer
 * or slice the local holds when it holds one. */
static int
typed_store_path(TypedBody *body, char *target, char op, const char *source)
{
    StoreStep steps[16];
    int count = 0;
    CompileValue value;
    compile_value_clear(&value);
    if(op ? !typed_compound_assignment(body, target, op, source, &value) :
            !typed_body_expression(body, source, &value)) return 0;
    /* `<<name` stores through the pointer NAME holds. */
    int through = !strncmp(target, "<<", 2);
    if(through) target = (char *)skip_ws(target + 2);
    char *cursor = target;
    while(is_ident_char((unsigned char)*cursor)) cursor++;
    char root[ZIR_NAME_MAX];
    size_t length = (size_t)(cursor - target);
    if(length == 0 || length >= sizeof(root)) return 0;
    memcpy(root, target, length);
    root[length] = '\0';
    for(;;) {
        cursor = (char *)skip_ws(cursor);
        if(*cursor == '\0') break;
        if(count == 16) return 0;
        StoreStep *step = &steps[count++];
        step->field[0] = '\0';
        if(*cursor == '.') {
            cursor = (char *)skip_ws(cursor + 1);
            char *name = cursor;
            while(is_ident_char((unsigned char)*cursor)) cursor++;
            length = (size_t)(cursor - name);
            if(length == 0 || length >= sizeof(step->field)) return 0;
            memcpy(step->field, name, length);
            step->field[length] = '\0';
            continue;
        }
        if(*cursor != '[') return 0;
        char *open = cursor;
        int depth = 0;
        for(; *cursor; cursor++) {
            if(*cursor == '[') depth++;
            else if(*cursor == ']' && --depth == 0) break;
        }
        if(*cursor != ']') return 0;
        *cursor++ = '\0';
        CompileValue at;
        compile_value_clear(&at);
        if(!typed_body_expression(body, open + 1, &at) ||
           at.kind != COMPILE_INTEGER) return 0;
        step->index = at.integer;
    }
    int local = typed_local_index(body, root);
    if(local < 0) return 0;
    ZirConst *binding = &body->names.items[local];
    TypedBody *owner = body;
    CompileValue held;
    compile_value_clear(&held);
    if((count > 0 || through) && compile_marker_value(binding->expr, &held)) {
        if(held.kind != COMPILE_REFERENCE ||
           (binding = compile_referenced(&held, &owner)) == NULL) return 0;
    } else if(through || !binding->expr[0]) return 0;
    CompileText stored = {0};
    int ok = store_at_path(owner->module, binding->type, binding->expr, steps,
                           count, &value, &stored);
    if(ok) binding->expr = KeepText(stored.data);
    free(stored.data);
    return ok;
}

static int typed_body_statements(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result);

static int
typed_body_statements_with_buffers(TypedBody *body, int start, int stop,
                      int *flow, CompileValue *result, TypedBodyStatementsBuffers *buffers)
{
    int saved = body->local_count;
    /* The declaration or assignment being taken apart, owned. */
    char *text = NULL;
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
            const char *value = skip_ws(source + 6);
            /* An untyped `.{...}` is a literal of the return type. */
            if(value[0] == '.' && value[1] == '{' && body->fn->return_type[0]) {
                free(text);
                size_t length = strlen(body->fn->return_type) + strlen(value) + 1;
                if((text = malloc(length)) == NULL) goto failed;
                snprintf(text, length, "%s%s", body->fn->return_type, value);
                value = text;
            }
            if(*value != '\0' && *value != ';' &&
               !typed_body_expression(body, value, result)) goto failed;
            *flow = 1;
            break;
        }
        if(statement->kind == ZIR_STMT_DECL) {
            free(text);
            if((text = strdup(source)) == NULL) goto failed;
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
            CompileValue value;
            compile_value_clear(&value);
            if(equals != NULL) {
                if(!typed_body_expression(body, equals, &value)) goto failed;
            }
            const char *type = colon[0] ? colon :
                value.kind == COMPILE_REAL ? "float64" :
                value.kind == COMPILE_STRING ? "string" :
                value.kind == COMPILE_COMPOUND || value.kind == COMPILE_REFERENCE ||
                value.kind == COMPILE_BUILDER ? value.type : "s64";
            if(equals != NULL && !compile_type_value(type, &value))
                goto failed;
            if(equals == NULL) {
                CompileText zero = {0};
                int ok = 1;
                if(zero_array_literal(type, &zero)) {
                    value.kind = COMPILE_COMPOUND;
                    copy_text(value.type, sizeof(value.type), type);
                    compile_set_literal(&value, zero.data);
                    equals = (char *)"";
                } else {
                    free(zero.data);
                    zero = (CompileText){0};
                    /* A local without an initializer holds its type's zero. */
                    if(zero_value_literal(body->module, type, &zero, 0)) {
                        ok = typed_body_expression(body, zero.data, &value) &&
                             compile_type_value(type, &value);
                        equals = (char *)"";
                    }
                }
                /* A large zero costs as much fuel as the values it holds. */
                *body->fuel -= (int)(zero.used / 4);
                free(zero.data);
                if(!ok || *body->fuel < 0) goto failed;
            }
            if(!typed_add_local(body, text, type,
                                equals != NULL ? &value : NULL)) goto failed;
            continue;
        }
        if(statement->kind == ZIR_STMT_ASSIGN) {
            free(text);
            if((text = strdup(source)) == NULL) goto failed;
            char *equals = strchr(text, '=');
            if(equals == NULL) goto failed;
            char op = equals > text ? equals[-1] : '\0';
            if(op && strchr("+-*/%", op) != NULL) equals[-1] = '\0';
            else op = '\0';
            *equals++ = '\0';
            trim_in_place(text);
            if(strpbrk(text, ".[") != NULL || !strncmp(text, "<<", 2)) {
                if(!typed_store_path(body, text, op, equals)) goto failed;
                continue;
            }
            int local = typed_local_index(body, text);
            if(local < 0) goto failed;
            CompileValue value;
            compile_value_clear(&value);
            if(op) {
                if(!typed_compound_assignment(body, body->names.items[local].expr,
                                              op, equals, &value)) goto failed;
            } else if(!typed_body_expression(body, equals, &value)) goto failed;
            if(!compile_type_value(body->names.items[local].type,
                                   &value)) goto failed;
            body->names.items[local].expr = KeepText(compile_literal(&value));
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
    free(text);
    typed_leave_scope(body, saved);
    return 1;
failed:
    free(text);
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
    size_t type = strlen(value->type) + 1, literal = strlen(compile_literal(value)) + 1;
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
    memcpy(texts + type, compile_literal(value), literal);
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
    /* Module constants are read through the module itself. */
    body.capacity = expected + fn->stmt_count + 1;
    body.names.items = malloc((size_t)body.capacity *
                              sizeof(*body.names.items));
    body.names.reads = malloc((size_t)body.capacity *
                              sizeof(*body.names.reads));
    body.serials = malloc((size_t)body.capacity * sizeof(*body.serials));
    if(body.names.items == NULL || body.names.reads == NULL ||
       body.serials == NULL)
        die("out of memory evaluating #run procedure");
    body.module = owner;
    body.fn = fn;
    body.depth = depth;
    body.fuel = fuel;
    body.outer = active_bodies;
    active_bodies = &body;
    compile_store.users++;
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
                char location[ZIR_TEXT_MAX];
                if(!CallerLocationLiteral(module, call_span, location,
                                          sizeof(location))) goto done;
                compile_set_literal(value, location);
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
        binding->expr = KeepText(compile_literal(value));
        if(value->kind == COMPILE_INTEGER)
            enum_member_expr(owner, parameter->type, value->integer,
                             &binding->expr);
        copy_text(binding->path, sizeof(binding->path), path);
        bound_texts[i] = binding->expr;
        body.serials[i] = body.next_serial++;
        body.names.count++;
    }
    body.local_count = expected;
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
    ok = typed_body_statements(&body, 0, fn->stmt_count, &flow, result) &&
         (fn->return_type[0] && strcmp(fn->return_type, "void") ?
          flow == 1 && compile_return_value(owner, fn->return_type, result) :
          result->kind == COMPILE_INVALID);
    if(ok && calls != NULL)
        keep_call(calls, hash, fn, bound_texts, expected, depth,
                  before - *fuel, result);
done:
    /* Pointers into this call's bindings no longer refer to anything. */
    for(int i = 0; i < compile_store.reference_count; i++)
        if(compile_store.references[i].body == &body)
            compile_store.references[i].body = NULL;
    active_bodies = body.outer;
    compile_store_leave();
    free(body.names.items);
    free(body.names.reads);
    free(body.serials);
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
    int fuel = 10000;
    CompileValue result;
    compile_value_clear(&result);
    int ok = evaluate_typed_expression(module, &constants, expanded, span,
                                       executing ? 1 : 0, &fuel, &result) &&
             result.kind == COMPILE_INTEGER;
    if(ok) *value = result.integer;
    free(constants.items);
    return ok;
}

/* SOURCE evaluated in MODULE with FUEL steps, its constants in scope. */
static int
evaluate_compile_value(const ZirModule *module, const char *source,
                       ZirSourceSpan span, int executing, int *fuel,
                       CompileValue *value)
{
    ZirConsts constants = {0};
    compile_value_clear(value);
    if(module == NULL || source == NULL) return 0;
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
                                       fuel, value) &&
             value->kind != COMPILE_INVALID;
    free(constants.items);
    return ok;
}

int
EvaluateCompileLiteral(const ZirModule *module, const char *source,
                       ZirSourceSpan span, int executing, const char **literal,
                       const ZirModule **type_owner)
{
    CompileValue value;
    int fuel = 10000;
    if(literal == NULL ||
       !evaluate_compile_value(module, source, span, executing, &fuel, &value))
        return 0;
    *literal = KeepText(compile_literal(&value));
    if(type_owner != NULL) *type_owner = value.type_owner;
    return 1;
}

int
EvaluateCompileRun(const ZirModule *module, const char *source,
                   ZirSourceSpan span, const char **literal,
                   const ZirModule **type_owner, int *exhausted)
{
    CompileValue value;
    int fuel = COMPILE_RUN_FUEL;
    *exhausted = 0;
    /* size_of lowers first where it appears, through the integer path. */
    if(strstr(source, "size_of") != NULL) {
        int64_t integer = 0;
        if(EvaluateCompileExpression(module, source, span, 1, &integer)) {
            char digits[32];
            snprintf(digits, sizeof(digits), "%" PRId64, integer);
            *literal = KeepText(digits);
            if(type_owner != NULL) *type_owner = NULL;
            return 1;
        }
    }
    if(!evaluate_compile_value(module, source, span, 1, &fuel, &value)) {
        *exhausted = fuel < 0;
        return 0;
    }
    if(value.kind == COMPILE_INTEGER) {
        char digits[32];
        snprintf(digits, sizeof(digits), "%" PRId64, value.integer);
        *literal = KeepText(digits);
    } else
        *literal = KeepText(compile_literal(&value));
    if(type_owner != NULL) *type_owner = value.type_owner;
    return 1;
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
    char *input = expression_input(source);
    copy_text(condition->input, sizeof(condition->input),
              input != NULL ? input : "");
    free(input);
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
