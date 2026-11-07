/* Checking std/simd operation calls; see zir_simd.h. */
#include "zir_check_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

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

/* Lanes(T) and Blocks(T). A fixed vector type's counts fold to constants.
 * A native one's depend on the instruction set the code is built for, so
 * the checked call stays: Lanes with the lanes of one block, Blocks with
 * no operand. */
static int
check_simd_count(Checker *c, int index, const SimdOp *op, const int *children,
                 int count, int checked, const char *display, const char **result_type)
{
    ZirExpr *e = &c->fn->exprs[index];
    if(checked) {
        SimdOperand operand = {.type = "s64"};
        const ZirExpr *lanes = count == 1 ? &c->fn->exprs[children[0]] : NULL;
        if(lanes != NULL && lanes->kind == ZIR_EXPR_INT) {
            operand.is_constant = 1;
            operand.value = strtoll(lanes->text, NULL, 10);
        }
        const char *message = "";
        if(SimdSignature(op, NULL, &operand, count, &message) == NULL ||
           (lanes != NULL && !operand.is_constant) || strcmp(e->type, "s64")) {
            error(c, e->span, "invalid SIMD operation", display);
            return 1;
        }
        *result_type = e->type;
        return 1;
    }
    ZirVector vector;
    const ZirExpr *named = count == 1 ? &c->fn->exprs[children[0]] : NULL;
    if(named == NULL || named->kind != ZIR_EXPR_IDENT || *lookup_lexical(c, named->name) ||
       !VectorType(named->name, &vector)) {
        error(c, e->span, "takes one vector type, such as s16xN", display);
        return 1;
    }
    if(!c->inference_only) {
        if(!vector.is_native)
            fold_constant(c, index, op->id == ZI_OP_LANES ? vector.lanes : vector.bytes / 16);
        else if(op->id == ZI_OP_LANES)
            fold_constant(c, children[0], vector.lanes);
        else
            e->first_child = -1;
        e = &c->fn->exprs[index];
        if(vector.is_native) e->name = KeepName(op->checked);
    }
    e->type = KeepName("s64");
    *result_type = e->type;
    return 1;
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
    if(op->shape == SIMD_COUNT)
        return check_simd_count(c, index, op, children, count, checked, display, result_type);

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

/* Whether a value of `type` holds a 256-bit vector. AVX2 code passes those
 * in YMM registers and other code in memory, so a #target clone never hands
 * one by value across a call. */
static int
holds_wide_vector(const ZirModule *module, const char *type, int depth)
{
    ZirVector vector;
    if(depth > 32 || type == NULL || !*type || *type == '*') return 0;
    if(VectorType(type, &vector)) return vector.lanes * vector.lane_bits == 256;
    if(BuiltinTypeName(type)) return 0;
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return holds_wide_vector(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    const ZirTypeField *field;
    while(TypeNextFieldShared(record, &offset, &field) == 1)
        if(holds_wide_vector(owner ? owner : module, field->type, depth + 1))
            return 1;
    return 0;
}

static int
native_vector(const char *type)
{
    ZirVector vector;
    return type != NULL && VectorType(type, &vector) && vector.is_native;
}

/* A native vector's size depends on the instruction set of the code that
 * holds it, so it is only ever a plain value: a local, parameter or result,
 * never part of an array, record, pointer, slice or procedure type. Nor
 * does it cross into code built elsewhere: #foreign and #program_export
 * procedures and the dispatcher of a #target one take none. */
static void
check_native_vectors(Checker *c, ZirFunction *fn)
{
    static const char inside[] = "a native vector, such as s16xN, is a plain value: "
                                 "it cannot be part of an array, record, pointer, slice or procedure type";
    int boundary = fn->is_extern || fn->exported || fn->simd_target;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++) {
        const char *type = parameters->items[i].type;
        if(HoldsNativeVector(type))
            error(c, fn->span, inside, type);
        else if(boundary && native_vector(type))
            error(c, fn->span, fn->simd_target ? "a #target procedure cannot take a native vector" :
                  "a #foreign or #program_export procedure cannot take a native vector",
                  parameters->items[i].name);
    }
    if(HoldsNativeVector(fn->return_type))
        error(c, fn->span, inside, fn->return_type);
    else if(boundary && native_vector(fn->return_type))
        error(c, fn->span, fn->simd_target ? "a #target procedure cannot return a native vector" :
              "a #foreign or #program_export procedure cannot return a native vector", fn->name);
    for(int i = 0; i < fn->stmt_count; i++)
        if(HoldsNativeVector(fn->stmts[i].type)) {
            error(c, fn->stmts[i].span, inside, fn->stmts[i].type);
            return;
        }
    for(int x = 0; x < fn->expr_count; x++)
        if(HoldsNativeVector(fn->exprs[x].type)) {
            error(c, fn->exprs[x].span, inside, fn->exprs[x].type);
            return;
        }
}

/* #target procedures get one C clone per x86-64 tier behind a dispatcher,
 * which forwards its arguments as they came. */
void
check_simd_procedure(Checker *c, ZirFunction *fn)
{
    check_native_vectors(c, fn);
    if(!fn->simd_target) return;
    if(fn->inlining == ZIR_INLINE_ALWAYS)
        error(c, fn->span, "a #target procedure cannot be #inline", fn->name);
    if(fn->assembly_text && *fn->assembly_text)
        error(c, fn->span, "a #target procedure cannot hold #asm variants", fn->name);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++) {
        const char *type = parameters->items[i].type;
        if(type != NULL && type[0] == '.' && type[1] == '.')
            error(c, fn->span, "a #target procedure cannot take variadic arguments", fn->name);
        else if(holds_wide_vector(c->module, type, 0))
            error(c, fn->span, "a #target procedure cannot take a 256-bit vector by value; "
                  "pass it in an array or through a pointer", parameters->items[i].name);
    }
    if(holds_wide_vector(c->module, fn->return_type, 0))
        error(c, fn->span, "a #target procedure cannot return a 256-bit vector by value; "
              "store it through a pointer", fn->name);
    for(int x = 0; x < fn->expr_count; x++) {
        const ZirExpr *call = &fn->exprs[x];
        if(call->kind != ZIR_EXPR_CALL || call->name == NULL || SimdOpChecked(call->name) != NULL)
            continue;
        int wide = holds_wide_vector(c->module, call->type, 0);
        int native = native_vector(call->type);
        for(int child = call->first_child; child >= 0 && !wide && !native;
            child = fn->exprs[child].next_sibling) {
            wide = holds_wide_vector(c->module, fn->exprs[child].type, 0);
            native = native_vector(fn->exprs[child].type);
        }
        if(wide)
            error(c, call->span, "a #target procedure cannot pass a 256-bit vector by value "
                  "to another procedure", call->name);
        else if(native)
            error(c, call->span, "a #target procedure cannot pass a native vector to another "
                  "procedure, which holds fewer blocks than its AVX2 clone", call->name);
    }
}
