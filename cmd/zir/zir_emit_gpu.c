/* #parallel_gpu regions as OpenGL ES 3.1 compute shaders. Only what a shader
 * computes as the CPU does is lowered: 32-bit integers, bytes and booleans
 * exactly, float32 within the rounding docs/LANGUAGE_DIRECTION.md allows. A
 * region with anything else stays on CPU threads, with the reason noted in the
 * generated C. The shader stops on a failed bounds check or a used up loop
 * budget by raising zir_fault, and the CPU worker then reruns the region. */

#include "zir_emit_internal.h"

typedef struct GpuText {
    char *data;
    size_t used, capacity;
} GpuText;

typedef struct GpuLocal {
    const char *name;
    const char *type;
    int depth;
    int is_binder;
} GpuLocal;

typedef struct GpuCapture {
    const char *name;
    char element[ZIR_NAME_MAX];
    int is_buffer;
    int writes;
    int slot; /* buffer binding or uniform location */
} GpuCapture;

#define GPU_MAX_CALLEES 32
#define GPU_MAX_LOCALS 256
#define GPU_MAX_DEPTH 64

typedef struct GpuLowering {
    const ZirModule *module;
    const ZirFunction *fn;
    int in_region;
    int frame, local_count, depth, loops;
    GpuLocal locals[GPU_MAX_LOCALS];
    GpuCapture captures[16];
    int capture_count;
    const ZirFunction *callees[GPU_MAX_CALLEES];
    int callee_done[GPU_MAX_CALLEES];
    int callee_count;
    GpuText functions;
    char reason[160];
} GpuLowering;

static void
gpu_append(GpuText *text, const char *format_string, ...)
{
    va_list args;
    va_start(args, format_string);
    int length = vsnprintf(NULL, 0, format_string, args);
    va_end(args);
    if(length < 0)
        exit(1);
    if(text->used + (size_t)length + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 1024;
        while(text->used + (size_t)length + 1 > capacity)
            capacity *= 2;
        text->data = realloc(text->data, capacity);
        if(text->data == NULL)
            exit(1);
        text->capacity = capacity;
    }
    va_start(args, format_string);
    vsnprintf(text->data + text->used, (size_t)length + 1, format_string, args);
    va_end(args);
    text->used += (size_t)length;
}

static int
gpu_refuse(GpuLowering *g, const char *format_string, const char *detail)
{
    if(!g->reason[0]) {
        const char *safe = detail != NULL ? detail : "";
        size_t used = 0;
        for(const char *p = format_string; *p && used + 1 < sizeof(g->reason); p++) {
            if(p[0] == '%' && p[1] == 's') {
                /* Generated C keeps this inside a comment. */
                for(; *safe && used + 1 < sizeof(g->reason); safe++)
                    if(!(safe[0] == '*' && safe[1] == '/') && !(safe[0] == '/' && safe[1] == '*'))
                        g->reason[used++] = *safe;
                p++;
                continue;
            }
            g->reason[used++] = *p;
        }
        g->reason[used] = '\0';
    }
    return 0;
}

/* The checked scalar a shader holds without changing its value, or NULL. */
static const char *
gpu_scalar(const char *type)
{
    static const char *const names[] = {"s32", "u32", "float32", "u8", "bool"};
    const char *name = canonical(type);
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if(!strcmp(name, names[i]))
            return names[i];
    return NULL;
}

static const char *
gpu_type_name(const char *scalar)
{
    return !strcmp(scalar, "s32") ? "int" : !strcmp(scalar, "float32") ? "float" :
           !strcmp(scalar, "bool") ? "bool" : "uint";
}

static const char *
gpu_storage_name(const char *scalar)
{
    return !strcmp(scalar, "bool") ? "uint" : gpu_type_name(scalar);
}

static int
gpu_kind(const char *scalar)
{
    return !strcmp(scalar, "s32") ? 0 : !strcmp(scalar, "u32") ? 1 :
           !strcmp(scalar, "float32") ? 2 : !strcmp(scalar, "u8") ? 3 : 4;
}

static int
gpu_untyped(const ZirFunction *fn, int index)
{
    const char *type = fn->exprs[index].type;
    return !strcmp(type, "integer") || !strcmp(type, "real");
}

static GpuLocal *
gpu_find_local(GpuLowering *g, const char *name)
{
    for(int i = g->local_count - 1; i >= g->frame; i--)
        if(!strcmp(g->locals[i].name, name))
            return &g->locals[i];
    return NULL;
}

static GpuCapture *
gpu_find_capture(GpuLowering *g, const char *name)
{
    if(g->in_region)
        for(int i = 0; i < g->capture_count; i++)
            if(!strcmp(g->captures[i].name, name))
                return &g->captures[i];
    return NULL;
}

static int
gpu_add_local(GpuLowering *g, const char *name, const char *type, int is_binder)
{
    if(g->local_count == GPU_MAX_LOCALS)
        return gpu_refuse(g, "too many locals", NULL);
    g->locals[g->local_count++] = (GpuLocal){name, type, g->depth, is_binder};
    return 1;
}

/* A decimal or hexadecimal integer literal. */
static int
gpu_integer_literal(const ZirExpr *expr, uint64_t *value)
{
    char digits[64];
    size_t used = 0;
    if(expr->kind != ZIR_EXPR_INT)
        return 0;
    for(const char *p = expr->text; *p; p++)
        if(*p != '_') {
            if(used + 1 >= sizeof(digits))
                return 0;
            digits[used++] = *p;
        }
    digits[used] = '\0';
    int hexadecimal = digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X');
    if(!used || (digits[0] == '0' && digits[1] && !hexadecimal))
        return 0;
    for(const char *p = hexadecimal ? digits + 2 : digits; *p; p++)
        if(hexadecimal ? !isxdigit((unsigned char)*p) : !isdigit((unsigned char)*p))
            return 0;
    errno = 0;
    *value = strtoull(digits, NULL, 0);
    return errno == 0;
}

static const char *
gpu_literal(GpuLowering *g, const ZirExpr *expr, const char *want, GpuText *out)
{
    uint64_t value;
    if(want == NULL) {
        gpu_refuse(g, "a literal has no 32-bit type", NULL);
        return NULL;
    }
    if(expr->kind == ZIR_EXPR_FLOAT) {
        if(strcmp(want, "float32")) {
            gpu_refuse(g, "a real literal is not float32", NULL);
            return NULL;
        }
        for(const char *p = expr->text; *p; p++)
            if(!isdigit((unsigned char)*p) && !strchr(".eE+-_", *p)) {
                gpu_refuse(g, "unsupported float literal", NULL);
                return NULL;
            }
        char digits[64];
        size_t used = 0;
        for(const char *p = expr->text; *p; p++)
            if(*p != '_' && used + 1 < sizeof(digits))
                digits[used++] = *p;
        digits[used] = '\0';
        gpu_append(out, "%s", digits);
        return want;
    }
    if(!gpu_integer_literal(expr, &value)) {
        gpu_refuse(g, "unsupported integer literal", NULL);
        return NULL;
    }
    if(!strcmp(want, "s32") && value <= INT32_MAX)
        gpu_append(out, "%llu", (unsigned long long)value);
    else if((!strcmp(want, "u32") && value <= UINT32_MAX) || (!strcmp(want, "u8") && value <= 255))
        gpu_append(out, "%lluu", (unsigned long long)value);
    else if(!strcmp(want, "float32") && value <= (UINT64_C(1) << 24))
        gpu_append(out, "%llu.0", (unsigned long long)value);
    else {
        gpu_refuse(g, "an integer literal does not fit its type", NULL);
        return NULL;
    }
    return want;
}

/* Whether a shader computes TYPE OP RIGHT as C does. Signed division and
 * remainder round differently on some drivers for negative values, and
 * dividing by zero or shifting past the width has no defined result. Bytes are
 * computed wider in C, so only operators that stay within a byte apply. */
static int
gpu_operator(GpuLowering *g, const char *type, const char *op, int right)
{
    const ZirExpr *divisor = &g->fn->exprs[right];
    uint64_t value = 0;
    int literal = gpu_integer_literal(divisor, &value);
    int integer = !strcmp(type, "s32") || !strcmp(type, "u32");
    if(!strcmp(type, "bool"))
        return gpu_refuse(g, "operator %s on bool", op);
    if(!strcmp(op, "&") || !strcmp(op, "|") || !strcmp(op, "^")) {
        if(strcmp(type, "float32"))
            return 1;
    } else if(!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*")) {
        if(strcmp(type, "u8"))
            return 1;
    } else if(!strcmp(op, "/")) {
        if(!strcmp(type, "float32") || (!strcmp(type, "u32") && literal && value != 0))
            return 1;
    } else if(!strcmp(op, "%")) {
        if(!strcmp(type, "u32") && literal && value != 0)
            return 1;
    } else if(!strcmp(op, "<<") || !strcmp(op, ">>")) {
        if(integer && literal && value < 32)
            return 1;
    }
    return gpu_refuse(g, "operator %s on this type", op);
}

static const char *gpu_expr(GpuLowering *g, int index, const char *want, GpuText *out);

/* A buffer's slot, as a position its length guards. */
static int
gpu_slot(GpuLowering *g, const GpuCapture *capture, int index, GpuText *out)
{
    const ZirExpr *position = &g->fn->exprs[index];
    gpu_append(out, "z_%s[zir_i(", capture->name);
    GpuLocal *local = position->kind == ZIR_EXPR_IDENT ? gpu_find_local(g, position->name) : NULL;
    if(local != NULL && local->is_binder) {
        gpu_append(out, "z_%s", local->name);
    } else {
        GpuText inner = {0};
        const char *type = gpu_expr(g, index, gpu_untyped(g->fn, index) ? "s32" : NULL, &inner);
        if(type == NULL || !strcmp(type, "float32") || !strcmp(type, "bool")) {
            free(inner.data);
            return gpu_refuse(g, "a slice index is not 32-bit", NULL);
        }
        gpu_append(out, !strcmp(type, "s32") ? "%s" : "int(%s)", inner.data);
        free(inner.data);
    }
    gpu_append(out, ",z_%s.length())]", capture->name);
    return 1;
}

static const char *
gpu_cast(GpuLowering *g, const ZirExpr *expr, GpuText *out)
{
    const char *target = gpu_scalar(expr->name);
    const ZirExpr *operand = &g->fn->exprs[expr->right];
    if(target == NULL || !strcmp(target, "bool")) {
        gpu_refuse(g, "a cast to %s", expr->name);
        return NULL;
    }
    if(gpu_untyped(g->fn, expr->right))
        return gpu_expr(g, expr->right, target, out);
    GpuText inner = {0};
    const char *from = "s32";
    GpuLocal *local = operand->kind == ZIR_EXPR_IDENT ? gpu_find_local(g, operand->name) : NULL;
    if(local != NULL && local->is_binder)
        gpu_append(&inner, "z_%s", local->name); /* in int32 by the runtime's range check */
    else if((from = gpu_expr(g, expr->right, NULL, &inner)) == NULL) {
        free(inner.data);
        return NULL;
    }
    if(!strcmp(from, target))
        gpu_append(out, "%s", inner.data);
    else if(!strcmp(target, "u8"))
        gpu_append(out, "(uint(%s)&255u)", inner.data);
    else
        gpu_append(out, "%s(%s)", gpu_type_name(target), inner.data);
    free(inner.data);
    return target;
}

static const char *
gpu_binary(GpuLowering *g, const ZirExpr *expr, const char *want, GpuText *out)
{
    const ZirFunction *fn = g->fn;
    const char *op = expr->op;
    int compares = !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") ||
                   !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=");
    int logical = !strcmp(op, "&&") || !strcmp(op, "||");
    int shifts = !strcmp(op, "<<") || !strcmp(op, ">>");
    GpuText left = {0}, right = {0};
    const char *left_type, *right_type, *result = NULL;
    const char *operand = compares ? NULL : want;
    if(logical) {
        left_type = gpu_expr(g, expr->left, "bool", &left);
        right_type = left_type ? gpu_expr(g, expr->right, "bool", &right) : NULL;
        result = right_type ? "bool" : NULL;
    } else if(gpu_untyped(fn, expr->left) && !gpu_untyped(fn, expr->right) && !shifts) {
        right_type = gpu_expr(g, expr->right, operand, &right);
        left_type = right_type ? gpu_expr(g, expr->left, right_type, &left) : NULL;
    } else {
        left_type = gpu_expr(g, expr->left, operand, &left);
        right_type = left_type ? gpu_expr(g, expr->right, left_type, &right) : NULL;
    }
    if(!logical && right_type != NULL) {
        if(compares) {
            int ordered = strcmp(op, "==") && strcmp(op, "!=");
            if(ordered && !strcmp(left_type, "bool"))
                gpu_refuse(g, "operator %s on bool", op);
            else
                result = "bool";
        } else if(gpu_operator(g, left_type, op, expr->right)) {
            result = left_type;
        }
    }
    if(result != NULL)
        gpu_append(out, "(%s%s%s)", left.data, op, right.data);
    free(left.data);
    free(right.data);
    return result;
}

static const char *gpu_call(GpuLowering *g, const ZirExpr *expr, GpuText *out);

static const char *
gpu_expr_kind(GpuLowering *g, int index, const char *want, GpuText *out)
{
    const ZirExpr *expr = &g->fn->exprs[index];
    switch(expr->kind) {
    case ZIR_EXPR_INT:
    case ZIR_EXPR_FLOAT:
        return gpu_literal(g, expr, want, out);
    case ZIR_EXPR_IDENT: {
        GpuLocal *local = gpu_find_local(g, expr->name);
        GpuCapture *capture = gpu_find_capture(g, expr->name);
        if(local != NULL && local->is_binder) {
            gpu_refuse(g, "the loop index is 64-bit outside a slice index or cast", NULL);
            return NULL;
        }
        if(local != NULL) {
            gpu_append(out, "z_%s", local->name);
            return local->type;
        }
        if(capture != NULL && !capture->is_buffer) {
            const char *scalar = gpu_scalar(capture->element);
            gpu_append(out, !strcmp(scalar, "bool") ? "(z_%s!=0u)" : "z_%s", capture->name);
            return scalar;
        }
        if(!expr->is_global_value && (!strcmp(expr->name, "true") || !strcmp(expr->name, "false"))) {
            gpu_append(out, "%s", expr->name);
            return "bool";
        }
        gpu_refuse(g, "it reads %s, not a 32-bit local or capture", expr->name);
        return NULL;
    }
    case ZIR_EXPR_INDEX: {
        const ZirExpr *base = &g->fn->exprs[expr->left];
        GpuLocal *local = base->kind == ZIR_EXPR_IDENT ? gpu_find_local(g, base->name) : NULL;
        GpuCapture *capture = base->kind == ZIR_EXPR_IDENT && local == NULL ? gpu_find_capture(g, base->name) : NULL;
        if(capture == NULL || !capture->is_buffer) {
            gpu_refuse(g, "it indexes something other than a captured slice", NULL);
            return NULL;
        }
        const char *scalar = gpu_scalar(capture->element);
        if(!strcmp(scalar, "bool")) {
            gpu_append(out, "(");
            if(!gpu_slot(g, capture, expr->right, out))
                return NULL;
            gpu_append(out, "!=0u)");
        } else if(!gpu_slot(g, capture, expr->right, out)) {
            return NULL;
        }
        return scalar;
    }
    case ZIR_EXPR_CAST:
        return gpu_cast(g, expr, out);
    case ZIR_EXPR_BINARY:
        return gpu_binary(g, expr, want, out);
    case ZIR_EXPR_UNARY: {
        GpuText inner = {0};
        const char *type = gpu_expr(g, expr->right, !strcmp(expr->op, "!") ? "bool" : want, &inner);
        int valid = type != NULL &&
            ((!strcmp(expr->op, "-") && (!strcmp(type, "s32") || !strcmp(type, "float32"))) ||
             (!strcmp(expr->op, "!") && !strcmp(type, "bool")) ||
             (!strcmp(expr->op, "~") && (!strcmp(type, "s32") || !strcmp(type, "u32"))));
        if(valid)
            gpu_append(out, "(%s%s)", expr->op, inner.data);
        else if(type != NULL)
            gpu_refuse(g, "unary operator %s on this type", expr->op);
        free(inner.data);
        return valid ? type : NULL;
    }
    case ZIR_EXPR_CONDITIONAL: {
        GpuText condition = {0}, chosen = {0}, other = {0};
        const char *type = NULL;
        if(gpu_expr(g, expr->left, "bool", &condition)) {
            if(gpu_untyped(g->fn, expr->right) && !gpu_untyped(g->fn, expr->third)) {
                type = gpu_expr(g, expr->third, want, &other);
                type = type ? gpu_expr(g, expr->right, type, &chosen) : NULL;
            } else {
                type = gpu_expr(g, expr->right, want, &chosen);
                type = type ? gpu_expr(g, expr->third, type, &other) : NULL;
            }
        }
        if(type != NULL)
            gpu_append(out, "(%s?%s:%s)", condition.data, chosen.data, other.data);
        free(condition.data);
        free(chosen.data);
        free(other.data);
        return type;
    }
    case ZIR_EXPR_CALL:
        return gpu_call(g, expr, out);
    default:
        gpu_refuse(g, "it uses an expression shaders do not hold: %s", expr->text);
        return NULL;
    }
}

static const char *
gpu_expr(GpuLowering *g, int index, const char *want, GpuText *out)
{
    if(index < 0 || index >= g->fn->expr_count) {
        gpu_refuse(g, "malformed expression", NULL);
        return NULL;
    }
    const char *type = gpu_expr_kind(g, index, want, out);
    if(type != NULL && want != NULL && strcmp(type, want)) {
        gpu_refuse(g, "it converts implicitly to %s", want);
        return NULL;
    }
    return type;
}

static int gpu_statements(GpuLowering *g, int begin, int end, const char *result, GpuText *out);

/* Function NAME's shader form, written before its callers; its ordinal, or -1. */
static int
gpu_callee(GpuLowering *g, const ZirModule *owner, const ZirFunction *function)
{
    for(int i = 0; i < g->callee_count; i++)
        if(g->callees[i] == function) {
            if(!g->callee_done[i])
                gpu_refuse(g, "%s is recursive", function->name);
            return g->callee_done[i] ? i : -1;
        }
    if(g->callee_count == GPU_MAX_CALLEES) {
        gpu_refuse(g, "it calls too many functions", NULL);
        return -1;
    }
    /* Emission follows checking; saved IR functions do not set `checked`. */
    if(function->is_extern || function->is_template || (function->assembly_text != NULL && function->assembly_text[0]) ||
       function->stmt_count == 0) {
        gpu_refuse(g, "%s has no Ziran body", function->name);
        return -1;
    }
    const char *result = !strcmp(canonical(function->return_type), "void") || !function->return_type[0] ?
                         "void" : gpu_scalar(function->return_type);
    if(result == NULL) {
        gpu_refuse(g, "%s returns a type shaders do not hold", function->name);
        return -1;
    }
    int ordinal = g->callee_count++;
    g->callees[ordinal] = function;
    g->callee_done[ordinal] = 0;

    const ZirModule *saved_module = g->module;
    const ZirFunction *saved_fn = g->fn;
    int saved_region = g->in_region, saved_frame = g->frame, saved_locals = g->local_count;
    int saved_depth = g->depth, saved_loops = g->loops;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
    GpuText text = {0};
    int ok = 1;
    g->module = owner;
    g->fn = function;
    g->in_region = 0;
    g->frame = g->local_count;
    g->depth = 0;
    g->loops = 0;
    gpu_append(&text, "%s zf_%d(", !strcmp(result, "void") ? "void" : gpu_type_name(result), ordinal);
    for(int i = 0; ok && i < parameters->count; i++) {
        const ZirParameter *parameter = &parameters->items[i];
        const char *scalar = gpu_scalar(parameter->value_type);
        if(scalar == NULL)
            ok = gpu_refuse(g, "a parameter has type %s", parameter->value_type);
        else
            ok = gpu_add_local(g, parameter->binding_name, scalar, 0);
        if(ok)
            gpu_append(&text, "%s%s z_%s", i ? "," : "", gpu_type_name(scalar), parameter->binding_name);
    }
    gpu_append(&text, "){\n");
    if(ok)
        ok = gpu_statements(g, 0, function->stmt_count, result, &text);
    gpu_append(&text, "}\n");
    if(ok)
        gpu_append(&g->functions, "%s", text.data);
    free(text.data);
    g->module = saved_module;
    g->fn = saved_fn;
    g->in_region = saved_region;
    g->frame = saved_frame;
    g->local_count = saved_locals;
    g->depth = saved_depth;
    g->loops = saved_loops;
    g->callee_done[ordinal] = ok;
    return ok ? ordinal : -1;
}

static const char *
gpu_call(GpuLowering *g, const ZirExpr *expr, GpuText *out)
{
    const ZirModule *owner;
    const ZirFunction *function;
    if((expr->slot_type != NULL && expr->slot_type[0]) || expr->is_function_value ||
       ResolveFunction(g->module, expr->name, &owner, &function) != 1) {
        gpu_refuse(g, "it calls %s, not one plain function", expr->name);
        return NULL;
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
    int ordinal = gpu_callee(g, owner, function);
    if(ordinal < 0)
        return NULL;
    gpu_append(out, "zf_%d(", ordinal);
    int count = 0;
    for(int child = expr->first_child; child >= 0; child = g->fn->exprs[child].next_sibling) {
        if(count == parameters->count || (g->fn->exprs[child].argument_name != NULL && g->fn->exprs[child].argument_name[0])) {
            gpu_refuse(g, "named or extra arguments to %s", expr->name);
            return NULL;
        }
        gpu_append(out, count ? "," : "");
        if(!gpu_expr(g, child, gpu_scalar(parameters->items[count].value_type), out))
            return NULL;
        count++;
    }
    gpu_append(out, ")");
    if(count != parameters->count) {
        gpu_refuse(g, "default arguments to %s", expr->name);
        return NULL;
    }
    return !strcmp(canonical(function->return_type), "void") || !function->return_type[0] ?
           "void" : gpu_scalar(function->return_type);
}

/* Assign to a local, or to a captured slice at the region's own iteration:
 * the one slot this iteration owns on every device. */
static int
gpu_assign(GpuLowering *g, const ZirStmt *st, GpuText *out)
{
    const ZirExpr *target = st->lhs_root >= 0 && st->lhs_root < g->fn->expr_count ? &g->fn->exprs[st->lhs_root] : NULL;
    const char *type = NULL;
    GpuCapture *capture = NULL;
    GpuText place = {0}, value = {0};
    int ok = 0;
    char op[4];
    if(target == NULL)
        return gpu_refuse(g, "malformed assignment", NULL);
    copy_text(op, sizeof(op), st->assignment_op);
    if(op[0])
        op[strlen(op) - 1] = '\0'; /* "=" to "", "+=" to "+" */
    if(target->kind == ZIR_EXPR_IDENT) {
        GpuLocal *local = gpu_find_local(g, target->name);
        if(local == NULL || local->is_binder)
            return gpu_refuse(g, "it assigns %s, not a local", target->name);
        type = local->type;
        gpu_append(&place, "z_%s", local->name);
    } else if(target->kind == ZIR_EXPR_INDEX && g->fn->exprs[target->left].kind == ZIR_EXPR_IDENT) {
        const ZirExpr *base = &g->fn->exprs[target->left];
        const ZirExpr *position = &g->fn->exprs[target->right];
        GpuLocal *binder = position->kind == ZIR_EXPR_IDENT ? gpu_find_local(g, position->name) : NULL;
        capture = gpu_find_local(g, base->name) == NULL ? gpu_find_capture(g, base->name) : NULL;
        if(capture == NULL || !capture->is_buffer || binder == NULL || !binder->is_binder)
            return gpu_refuse(g, "it writes a slot other than capture[index]", NULL);
        capture->writes = 1;
        type = gpu_scalar(capture->element);
        if(!gpu_slot(g, capture, target->right, &place))
            goto done;
    } else {
        return gpu_refuse(g, "it assigns through something other than a slice slot", NULL);
    }
    if(op[0] && !gpu_operator(g, type, op, st->expr_root))
        goto done;
    if(!gpu_expr(g, st->expr_root, type, &value))
        goto done;
    if(capture != NULL && !strcmp(type, "bool"))
        gpu_append(out, "%s=uint(%s);\n", place.data, value.data);
    else
        gpu_append(out, "%s%s=%s;\n", place.data, op, value.data);
    ok = 1;
done:
    free(place.data);
    free(value.data);
    return ok;
}

static int
gpu_statement(GpuLowering *g, const ZirStmt *st, const char *result, int *kinds, GpuText *out)
{
    GpuText value = {0};
    const char *type;
    int ok = 1;
    if(st->is_using || st->is_parallel || st->for_form || st->for_step || st->target_id)
        return gpu_refuse(g, "it uses nested for loops, nested regions or labels", NULL);
    switch(st->kind) {
    case ZIR_STMT_DECL:
        type = gpu_scalar(st->type);
        if(type == NULL)
            return gpu_refuse(g, "a local has type %s", st->type);
        if(st->expr_root >= 0) {
            if(!gpu_expr(g, st->expr_root, type, &value)) {
                free(value.data);
                return 0;
            }
        } else {
            gpu_append(&value, !strcmp(type, "float32") ? "0.0" : !strcmp(type, "bool") ? "false" :
                       !strcmp(type, "s32") ? "0" : "0u");
        }
        gpu_append(out, "%s z_%s=%s;\n", gpu_type_name(type), st->name, value.data);
        free(value.data);
        return gpu_add_local(g, st->name, type, 0);
    case ZIR_STMT_ASSIGN:
        return gpu_assign(g, st, out);
    case ZIR_STMT_EXPR:
        if(st->expr_root < 0 || g->fn->exprs[st->expr_root].kind != ZIR_EXPR_CALL)
            return gpu_refuse(g, "an expression statement is not a call", NULL);
        ok = gpu_expr(g, st->expr_root, NULL, &value) != NULL;
        if(ok)
            gpu_append(out, "%s;\n", value.data);
        free(value.data);
        return ok;
    case ZIR_STMT_IF:
        if(g->depth == GPU_MAX_DEPTH)
            return gpu_refuse(g, "blocks nest too deeply", NULL);
        kinds[g->depth++] = 0;
        if(st->is_else && st->expr_root < 0) {
            gpu_append(out, "else{\n");
            return 1;
        }
        ok = gpu_expr(g, st->expr_root, "bool", &value) != NULL;
        if(ok)
            gpu_append(out, "%sif(%s){\n", st->is_else ? "else " : "", value.data);
        free(value.data);
        return ok;
    case ZIR_STMT_WHILE:
        if(g->depth == GPU_MAX_DEPTH)
            return gpu_refuse(g, "blocks nest too deeply", NULL);
        kinds[g->depth++] = 1;
        g->loops++;
        ok = gpu_expr(g, st->expr_root, "bool", &value) != NULL;
        if(ok)
            gpu_append(out, "while(true){\nif(zir_fuel==0u){zir_fault=1u;break;}\nzir_fuel--;\nif(!%s)break;\n", value.data);
        free(value.data);
        return ok;
    case ZIR_STMT_BLOCK_OPEN:
        if(StatementHasAssembly(st))
            return gpu_refuse(g, "it holds assembly", NULL);
        if(g->depth == GPU_MAX_DEPTH)
            return gpu_refuse(g, "blocks nest too deeply", NULL);
        kinds[g->depth++] = 0;
        gpu_append(out, "{\n");
        return 1;
    case ZIR_STMT_BLOCK_CLOSE:
        if(g->depth == 0)
            return gpu_refuse(g, "malformed block", NULL);
        g->depth--;
        g->loops -= kinds[g->depth];
        while(g->local_count > g->frame && g->locals[g->local_count - 1].depth > g->depth)
            g->local_count--;
        gpu_append(out, "}\n");
        return 1;
    case ZIR_STMT_BREAK:
    case ZIR_STMT_CONTINUE:
        if(g->loops == 0)
            return gpu_refuse(g, "it leaves an iteration with break or continue", NULL);
        gpu_append(out, st->kind == ZIR_STMT_BREAK ? "break;\n" : "continue;\n");
        return 1;
    case ZIR_STMT_RETURN:
        if(result == NULL)
            return gpu_refuse(g, "it returns from a region", NULL);
        if(st->expr_root < 0) {
            gpu_append(out, "return;\n");
            return 1;
        }
        ok = gpu_expr(g, st->expr_root, !strcmp(result, "void") ? NULL : result, &value) != NULL;
        if(ok)
            gpu_append(out, "return %s;\n", value.data);
        free(value.data);
        return ok;
    default:
        return gpu_refuse(g, "it uses a statement shaders do not hold", NULL);
    }
}

static int
gpu_statements(GpuLowering *g, int begin, int end, const char *result, GpuText *out)
{
    int kinds[GPU_MAX_DEPTH];
    for(int i = begin; i < end; i++)
        if(!gpu_statement(g, &g->fn->stmts[i], result, kinds, out))
            return 0;
    return 1;
}

/* REGION's shader, with captures resolved. Returns 1 and fills CODE, or 0 with
 * the reason in CODE. */
int
GpuLowerRegion(const ZirModule *module, const ZirFunction *fn, const ParallelRegion *region,
               GpuRegionCode *code)
{
    GpuLowering *g = calloc(1, sizeof(*g));
    GpuText body = {0}, header = {0};
    int ok = 0, buffers = 0, writes = 0, uniforms = 0;
    if(g == NULL)
        exit(1);
    memset(code, 0, sizeof(*code));
    g->module = module;
    g->fn = fn;
    g->in_region = 1;
    for(int c = 0; c < region->capture_count; c++) {
        GpuCapture *capture = &g->captures[g->capture_count++];
        const char *type = region->capture_types[c];
        int capacity;
        capture->name = region->captures[c];
        if(strcmp(type, "string") && SliceElementType(type, capture->element, sizeof(capture->element)))
            capture->is_buffer = 1;
        else if(ArrayValueType(type) &&
                ArrayElementType(type, capture->element, sizeof(capture->element), &capacity))
            capture->is_buffer = 1;
        else
            copy_text(capture->element, sizeof(capture->element), type);
        if(gpu_scalar(capture->element) == NULL) {
            gpu_refuse(g, "it captures %s", type);
            goto done;
        }
        capture->slot = capture->is_buffer ? ++buffers : 2 + uniforms++;
    }
    if(!gpu_add_local(g, region->binder, "s64", 1))
        goto done;
    gpu_append(&body, "void main(){\nint z_%s=zir_it+int(gl_GlobalInvocationID.x);\nif(z_%s>zir_last)return;\n",
               region->binder, region->binder);
    if(!gpu_statements(g, region->body_begin, region->body_end, NULL, &body))
        goto done;
    gpu_append(&body, "}\n");
    gpu_append(&header,
               "#version 310 es\nprecision highp float;\nprecision highp int;\n"
               "layout(local_size_x=64) in;\n"
               "layout(std430,binding=0) buffer ZF{uint zir_fault;};\n"
               "layout(location=0) uniform int zir_it;\nlayout(location=1) uniform int zir_last;\n"
               "uint zir_fuel=1048576u;\n");
    for(int c = 0; c < g->capture_count; c++) {
        const GpuCapture *capture = &g->captures[c];
        const char *scalar = gpu_scalar(capture->element);
        code->kinds[c] = gpu_kind(scalar);
        code->is_buffer[c] = capture->is_buffer;
        code->writes[c] = capture->writes;
        writes += capture->writes;
        if(capture->is_buffer)
            gpu_append(&header, "layout(std430,binding=%d) %sbuffer ZB%d{%s z_%s[];};\n", capture->slot,
                       capture->writes ? "" : "readonly ", capture->slot,
                       gpu_storage_name(scalar), capture->name);
        else
            gpu_append(&header, "layout(location=%d) uniform %s z_%s;\n", capture->slot,
                       gpu_storage_name(scalar), capture->name);
    }
    if(!writes) {
        gpu_refuse(g, "it writes no captured slice", NULL);
        goto done;
    }
    gpu_append(&header, "int zir_i(int i,int n){if(i<0||i>=n){zir_fault=1u;return 0;}return i;}\n");
    gpu_append(&header, "%s%s", g->functions.data ? g->functions.data : "", body.data);
    code->text = header.data;
    header.data = NULL;
    ok = 1;
done:
    copy_text(code->reason, sizeof(code->reason), g->reason);
    free(body.data);
    free(header.data);
    free(g->functions.data);
    free(g);
    return ok;
}

/* The shader as C string pieces, within every C compiler's string limit. */
void
GpuEmitShaderLines(Emitter *e, const char *text)
{
    size_t length = strlen(text);
    for(size_t at = 0; at < length; at += 800) {
        size_t piece = length - at < 800 ? length - at : 800;
        char quoted[1700];
        size_t used = 0;
        quoted[used++] = '"';
        for(size_t i = 0; i < piece; i++) {
            char c = text[at + i];
            if(c == '\n') {
                quoted[used++] = '\\';
                quoted[used++] = 'n';
            } else {
                quoted[used++] = c;
            }
        }
        quoted[used++] = '"';
        quoted[used++] = ',';
        quoted[used] = '\0';
        line(e, "%s", quoted);
    }
}
