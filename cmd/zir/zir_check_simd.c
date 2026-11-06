/* Checking std/simd operation calls; see zir_simd.h. */
#include "zir_check_internal.h"
#include "zir_simd.h"

#include <stdio.h>

/* Make of a 32-lane vector has the most operands. */
#define SIMD_OPERANDS_MAX 33

/* A procedure std/simd declares itself, which wins over an operation. */
static int
declares(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->function_count; i++)
        if(!strcmp(module->functions[i].name, name)) return 1;
    return 0;
}

/* The operation an unchecked call names: `Alias.Name` through an import of
 * std/simd, or a bare name inside std/simd itself. */
static const SimdOp *
named_operation(Checker *c, const char *name)
{
    const char *dot = strchr(name, '.');
    const ZirModule *module = NULL;
    const char *member = name;
    if(dot == NULL) {
        if(SimdModule(c->module)) module = c->module;
    } else {
        char alias[ZIR_NAME_MAX];
        size_t length = (size_t)(dot - name);
        if(length == 0 || length >= sizeof(alias) || strchr(dot + 1, '.')) return NULL;
        memcpy(alias, name, length);
        alias[length] = '\0';
        if(*lookup_lexical(c, alias)) return NULL;
        for(int i = 0; i < c->module->import_count && module == NULL; i++) {
            const ZirImport *import = &c->module->imports[i];
            if(import->kind == ZIR_IMPORT_MODULE && !strcmp(import->name, alias) &&
               in_lookup_file(c->module, import->is_file_private, import->span) &&
               SimdModule(import->resolved_module))
                module = import->resolved_module;
        }
        member = dot + 1;
    }
    if(module == NULL || declares(module, member)) return NULL;
    return SimdOpNamed(member);
}

/* Operand positions that must be constants, folded into literals. */
static int
constant_position(const SimdOp *op, int position)
{
    switch(op->shape) {
    case SIMD_LANE: case SIMD_WITH_LANE: case SIMD_BYTE_SHIFT: return position == 1;
    case SIMD_ALIGN: return position == 2;
    case SIMD_SHUFFLE: return position >= 1;
    default: return 0;
    }
}

/* Operand positions that hold one lane value. */
static int
lane_position(const SimdOp *op, int position)
{
    switch(op->shape) {
    case SIMD_MAKE: return 1;
    case SIMD_SPLAT: return position == 0;
    case SIMD_WITH_LANE: return position == 2;
    default: return 0;
    }
}

/* Give a literal or narrower lane value the lane type itself. */
static int
settle_lane(Checker *c, int child, const char *type, const char *lane)
{
    if(!strcmp(type, "integer") || !strcmp(type, "real")) {
        c->fn->exprs[child].type = KeepName(lane);
        return 1;
    }
    if(!strcmp(ScalarType(type), lane)) return 1;
    return widen_expression(c, child, lane) != NULL;
}

static void
fold_constant(Checker *c, int child, int64_t value)
{
    ZirExpr *operand = &c->fn->exprs[child];
    char digits[32];
    snprintf(digits, sizeof(digits), "%lld", (long long)value);
    if(operand->kind != ZIR_EXPR_INT) c->conversions_applied = 1;
    operand->kind = ZIR_EXPR_INT;
    operand->name = "";
    operand->text = KeepText(digits);
    operand->left = operand->right = operand->third = operand->first_child = -1;
    operand->type = KeepName("s64");
}

int
check_simd_call(Checker *c, int index, const char **result_type)
{
    ZirExpr *e = &c->fn->exprs[index];
    const SimdOp *op = SimdOpChecked(e->name);
    int checked = op != NULL;
    if(!checked && (op = named_operation(c, e->name)) == NULL) return 0;
    char display[ZIR_NAME_MAX + 8];
    snprintf(display, sizeof(display), "Simd.%s", op->name);
    *result_type = "";

    int children[SIMD_OPERANDS_MAX + 1], count = 0;
    for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
        if(count == SIMD_OPERANDS_MAX + 1) {
            error(c, e->span, "too many operands", display);
            return 1;
        }
        if(c->fn->exprs[child].argument_name[0]) {
            error(c, c->fn->exprs[child].span, "SIMD operations have no named parameters", display);
            return 1;
        }
        children[count++] = child;
    }

    /* The result type comes first in source and is the call's type once
     * checked. */
    char result[ZIR_NAME_MAX] = "";
    int first = 0;
    if(checked) {
        copy_text(result, sizeof(result), e->type);
    } else if(op->type_argument) {
        ZirVector vector;
        const ZirExpr *named = count > 0 ? &c->fn->exprs[children[0]] : NULL;
        if(named == NULL || named->kind != ZIR_EXPR_IDENT || *lookup_lexical(c, named->name) ||
           !VectorType(named->name, &vector)) {
            error(c, e->span, "takes a vector type first, such as s16x8", display);
            return 1;
        }
        copy_text(result, sizeof(result), vector.name);
        first = 1;
    }

    char types[SIMD_OPERANDS_MAX][ZIR_NAME_MAX];
    SimdOperand operands[SIMD_OPERANDS_MAX];
    int operand_count = count - first;
    if(operand_count > SIMD_OPERANDS_MAX) {
        error(c, e->span, "too many operands", display);
        return 1;
    }
    for(int i = 0; i < operand_count; i++) {
        int child = children[first + i];
        const char *type = expression_type(c, child);
        if(!*type) return 1;
        copy_text(types[i], sizeof(types[i]), type);
        int64_t value = 0;
        operands[i].type = types[i];
        operands[i].is_constant = bound_expression(c->module, c->fn, child, 0, &value) == 1;
        operands[i].value = value;
    }
    e = &c->fn->exprs[index];

    const char *message = "";
    const char *signature = SimdSignature(op, *result ? result : NULL, operands,
                                          operand_count, &message);
    if(signature == NULL) {
        char detail[ZIR_NAME_MAX * 2];
        snprintf(detail, sizeof(detail), "%s %s", display, message);
        error(c, e->span, "invalid SIMD operation", detail);
        return 1;
    }
    if(checked && strcmp(signature, e->type)) {
        error(c, e->span, "SIMD operation has the wrong checked type", display);
        return 1;
    }

    ZirVector vector;
    const char *lane = "";
    if(VectorType(*result ? result : signature, &vector)) lane = vector.lane;
    if(operand_count > 0 && VectorType(types[0], &vector) && op->shape == SIMD_WITH_LANE)
        lane = vector.lane;
    for(int i = 0; i < operand_count && !c->inference_only; i++) {
        int child = children[first + i];
        c->fn->exprs[child].argument_index = i;
        if(constant_position(op, i)) {
            if(c->fn->exprs[child].kind != ZIR_EXPR_INT || strcmp(types[i], "s64"))
                fold_constant(c, child, operands[i].value);
        } else if(lane_position(op, i)) {
            if(!settle_lane(c, child, types[i], lane)) {
                c->failed = 1;
                return 1;
            }
        } else if(!strcmp(types[i], "integer")) {
            c->fn->exprs[child].type = KeepName("s64");
        }
    }
    e = &c->fn->exprs[index];
    if(!c->inference_only) {
        e->name = KeepName(op->checked);
        if(first) e->first_child = count > 1 ? children[1] : -1;
    }
    e->type = KeepName(signature);
    *result_type = e->type;
    return 1;
}
