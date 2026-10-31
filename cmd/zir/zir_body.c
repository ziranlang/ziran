#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_body.h"
#include "zir_check.h"
#include "zir_native.h"
#include "zir_text.h"

typedef struct TypePath {
    const ZirType *record;
    const struct TypePath *parent;
} TypePath;
static void free_scratch_CanEmitBodyBuffers(void *pointer);
static int record_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared && !declared->is_enum && !declared->is_procedure_type;
}
static int enum_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared && declared->is_enum;
}

/* Buffers portable_type_path keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct PortableTypePathBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} PortableTypePathBuffers;

static int portable_type_path(const ZirModule *module, const char *type, const TypePath *path);

static int
portable_type_path_with_buffers(const ZirModule *module, const char *type, const TypePath *path, PortableTypePathBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirType *record;
    size_t offset = 0;
    const ZirTypeField *field;
    int fields = 0;
    int status;
    char slice_element[ZIR_NAME_MAX];
    if(SliceElementType(type, slice_element, sizeof(slice_element))) {
        /* A slice is a pointer and a count, so like `*T` it may refer back
         * to a record whose layout is still being checked. */
        const ZirModule *element_owner = NULL;
        const ZirType *element = FindType(module, slice_element, &element_owner);
        for(const TypePath *ancestor = path; element && ancestor; ancestor = ancestor->parent)
            if(ancestor->record == element)
                return 1;
        return portable_type_path(module, slice_element, path);
    }
    if(!strcmp(type, "null")) return 1;
    if(TargetType(type, ZIR_C)) return 1;
    if(strchr(type, '*') != NULL) return 1;
    {
        char element[ZIR_NAME_MAX];
        if(ArrayElementType(type, element, sizeof(element), NULL))
            return portable_type_path(module, element, path);
    }
    record = FindType(module, type, &owner);
    if(record == NULL)
        return 0;
    if(record->foreign_target[0] || record->is_map)
        return 1; /* Go can pass opaque values; other targets reject them. */
    if(record->is_enum)
        return 1;
    if(record->is_procedure_type) {
        if(record->is_c_call)
            return 1; /* Native callback; the VM and Go target reject it. */
        int count = *skip_ws(record->body) ?
            split_top_level(record->body, buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
        for(int i = 0; i < count; i++) {
            char *colon = strchr(buffers->parameters[i], ':');
            if(colon == NULL || !portable_type_path(owner, skip_ws(colon + 1), path))
                return 0;
        }
        return portable_type_path(owner, record->procedure_return_type, path);
    }
    for(const TypePath *ancestor = path; ancestor; ancestor = ancestor->parent) {
        if(ancestor->record == record)
            return 0;
    }
    TypePath current = {record, path};
    if(TypeHasOnlyPrimitiveFields(record)) return 1;
    while((status = TypeNextFieldShared(record, &offset, &field)) == 1) {
        if(!portable_type_path(owner, field->type, &current) || !strcmp(field->type, "void"))
            return 0;
        fields++;
    }
    return status == 0 && fields > 0;
}

COMPILER_SCRATCH_RETURN(static, int, portable_type_path,
    (const ZirModule *module, const char *type, const TypePath *path),
    PortableTypePathBuffers, RECOVERABLE, free,
    portable_type_path_with_buffers(module, type, path, buffers))

static int
portable_type(const ZirModule *module, const char *type)
{
    return portable_type_path(module, type, NULL);
}

static int supported_expression(const ZirModule *module, const ZirFunction *fn,
                                int index, unsigned char *support);

static int
supported_expression_node(const ZirModule *module, const ZirFunction *fn,
                          int index, unsigned char *support)
{
    const ZirExpr *e;
    e = &fn->exprs[index];
    if(!portable_type(module, e->type)) return 0;
    switch(e->kind) {
    case ZIR_EXPR_COMPOUND:
        if(!record_type(module, e->name) && !ArrayElementType(e->name, NULL, 0, NULL)) return 0;
        break;
    case ZIR_EXPR_FIELD_INIT: break;
    case ZIR_EXPR_INT: case ZIR_EXPR_FLOAT: case ZIR_EXPR_IDENT:
    case ZIR_EXPR_STRING: case ZIR_EXPR_COMPILE_TIME:
    case ZIR_EXPR_SIZE_OF: break;
    case ZIR_EXPR_MEMBER: case ZIR_EXPR_POINTER_MEMBER:
    case ZIR_EXPR_INDEX: case ZIR_EXPR_SLICE: break;
    case ZIR_EXPR_BINARY: case ZIR_EXPR_CONDITIONAL: break;
    case ZIR_EXPR_UNARY: break;
    case ZIR_EXPR_CAST: {
        const ZirType *declared = FindType(module, e->name, NULL);
        if(e->name[0] != '*' && !TargetType(e->name, ZIR_C) &&
           !enum_type(module, e->name) &&
           !(declared && declared->foreign_target[0])) return 0;
        break;
    }
    case ZIR_EXPR_CALL:
        if(!e->name[0] &&
           (e->left < 0 || !portable_type(module, fn->exprs[e->left].type)))
            return 0;
        break;
    default: return 0;
    }
    if(!supported_expression(module, fn, e->left, support) ||
       !supported_expression(module, fn, e->right, support) ||
       !supported_expression(module, fn, e->third, support)) return 0;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(!supported_expression(module, fn, child, support)) return 0;
    return 1;
}

static int
supported_expression(const ZirModule *module, const ZirFunction *fn,
                     int index, unsigned char *support)
{
    if(index < 0) return 1;
    if(index >= fn->expr_count) return 0;
    /* A typed expression graph can share nodes. Each eligibility pass owns
     * its answers, so later body rewrites or another scope cannot reuse them.
     * State 1 also stops a malformed cycle before it recurses indefinitely. */
    if(support[index]) {
        ProfileCount("body_expression_reuse");
        return support[index] == 2;
    }
    support[index] = 1;
    ProfileCount("body_expression_check");
    if(!supported_expression_node(module, fn, index, support)) return 0;
    support[index] = 2;
    return 1;
}
/* Buffers CanEmitBody keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CanEmitBodyBuffers {
    char params[64][ZIR_TEXT_MAX];
    unsigned char *expression_support;
    size_t expression_capacity;
} CanEmitBodyBuffers;

int CanEmitBody(const ZirModule *module, const ZirFunction *fn);

static int
CanEmitBody_with_buffers(const ZirModule *module, const ZirFunction *fn, CanEmitBodyBuffers *buffers)
{
    int count;
    /* Eligibility follows the typed function body and its operations.
     * Host calls and unsupported composition fail the same checks. */
    if(!fn->checked || fn->is_extern || fn->expr_count < 0 ||
       !portable_type(module, fn->return_type)) return 0;
    if((size_t)fn->expr_count > buffers->expression_capacity) {
        free(buffers->expression_support);
        buffers->expression_support = AllocateOrExit((size_t)fn->expr_count);
        buffers->expression_capacity = (size_t)fn->expr_count;
    }
    if(fn->expr_count > 0)
        memset(buffers->expression_support, 0, (size_t)fn->expr_count);
    count = *skip_ws(FunctionArgs(fn)) ? split_top_level(FunctionArgs(fn), buffers->params[0], 64, sizeof(buffers->params[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(buffers->params[i], ':');
        if(!colon || !portable_type(module, skip_ws(colon + 1))) return 0;
    }
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        switch(st->kind) {
        case ZIR_STMT_DECL: if(!portable_type(module, st->type)) return 0; break;
        case ZIR_STMT_ASSIGN:
            if(st->lhs_root < 0 || (fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT &&
                fn->exprs[st->lhs_root].kind != ZIR_EXPR_MEMBER &&
                fn->exprs[st->lhs_root].kind != ZIR_EXPR_POINTER_MEMBER &&
                fn->exprs[st->lhs_root].kind != ZIR_EXPR_INDEX &&
                !(fn->exprs[st->lhs_root].kind == ZIR_EXPR_UNARY &&
                  !strcmp(fn->exprs[st->lhs_root].op, "*")))) return 0;
            break;
        case ZIR_STMT_IF: break;
        case ZIR_STMT_BLOCK_OPEN: case ZIR_STMT_BLOCK_CLOSE:
        case ZIR_STMT_WHILE: case ZIR_STMT_RETURN: case ZIR_STMT_BREAK:
        case ZIR_STMT_CONTINUE: case ZIR_STMT_UNUSED: case ZIR_STMT_EXPR:
        case ZIR_STMT_UNREACHABLE: break;
        default: return 0;
        }
        if(!supported_expression(module, fn, st->expr_root, buffers->expression_support) ||
           !supported_expression(module, fn, st->lhs_root, buffers->expression_support)) return 0;
    }
    return 1;
}

int
CanEmitBody(const ZirModule *module, const ZirFunction *fn)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free_scratch_CanEmitBodyBuffers);
    CanEmitBodyBuffers *buffers;
    buffers = !CompilerMemoryRecovering() ? CompilerScratchTake(scratch_hint) : NULL;
    if(buffers == NULL) {
        buffers = AllocateOrExit(sizeof(*buffers));
        buffers->expression_support = NULL;
        buffers->expression_capacity = 0;
    }
    int returned = CanEmitBody_with_buffers(module, fn, buffers);
    if(CompilerMemoryRecovering() || !CompilerScratchPut(scratch_hint, buffers)) {
        free(buffers->expression_support);
        free(buffers);
    }
    return returned;
}


static void free_scratch_CanEmitBodyBuffers(void *pointer)
{
    CanEmitBodyBuffers *buffers = pointer;
    free(buffers->expression_support);
        free(buffers);
}
