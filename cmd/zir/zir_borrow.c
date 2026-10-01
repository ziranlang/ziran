#include "zir_borrow.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Origin {
    uint64_t parameters;
    int depth;
    int invalid;
    int unknown;
    const uint64_t *globals;
} Origin;

/* Origins share immutable bit sets until the lifetime check finishes. */
typedef struct GlobalBits {
    struct GlobalBits *next;
    uint64_t words[];
} GlobalBits;

typedef struct BorrowFunction {
    const ZirModule *module;
    const ZirFunction *fn;
    Origin returned;
    Origin *locals;
} BorrowFunction;

typedef struct BorrowBinding BorrowBinding;

typedef struct BorrowPath {
    struct BorrowPath *parent;
    struct BorrowPath *next_allocation;
    const char *field;
    int depth;
    int index;
} BorrowPath;

typedef struct BorrowPlace {
    BorrowBinding *root;
    BorrowPath *path;
} BorrowPlace;

typedef struct BorrowSource {
    BorrowPlace place;
    struct BorrowSource *next;
    struct BorrowSource *next_allocation;
} BorrowSource;

struct BorrowBinding {
    const ZirModule *module;
    const ZirGlobal *global;
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    Origin origin;
    int local;
    int depth;
    int captured;
    int global_index;
    int global_backing_state;
    BorrowPlace text_backing;
    BorrowSource *text_backings;
    BorrowPlace address_backing;
    int address_known;
};

typedef struct ActiveTextBorrow {
    BorrowPlace backing;
    int depth;
} ActiveTextBorrow;

/* Whether a type holds text, answered once per module and type name: the
 * answer walks every field, and the check asks for the same types many
 * times over its passes. Types no longer change once checking is done. */
typedef struct TextTypeAnswer {
    const ZirModule *module;
    char type[ZIR_NAME_MAX];
    int holds_text;
} TextTypeAnswer;

typedef struct BorrowCheck {
    TextTypeAnswer *text_types; /* open addressing, capacity a power of two */
    size_t text_type_capacity, text_type_count;
    BorrowFunction *functions;
    int count;
    BorrowFunction *current;
    BorrowBinding *bindings;
    int binding_count;
    BorrowBinding *globals;
    int global_count;
    size_t global_words;
    Origin *global_origins;
    GlobalBits *global_bits;
    ActiveTextBorrow *active;
    int active_count;
    int active_capacity;
    BorrowPath *paths;
    BorrowSource *sources;
    int depth;
    int changed;
    int failed;
} BorrowCheck;

static void
reject(BorrowCheck *check, ZirSourceSpan span, const char *message)
{
    if(!check->failed)
        Diagnostic(span, "check.slice_lifetime", "%s", message);
    check->failed = 1;
}

static uint64_t *
new_global_bits(BorrowCheck *check)
{
    GlobalBits *bits = calloc(1, sizeof(*bits) +
                                check->global_words * sizeof(bits->words[0]));
    if(bits == NULL) {
        reject(check, check->current->fn->span,
               "out of memory checking global text aliases");
        return NULL;
    }
    bits->next = check->global_bits;
    check->global_bits = bits;
    return bits->words;
}

static const uint64_t *
global_bit(BorrowCheck *check, int index)
{
    uint64_t *bits = new_global_bits(check);
    if(bits != NULL)
        bits[(size_t)index / 64] = UINT64_C(1) << (index % 64);
    return bits;
}

static int
has_global(Origin origin, int index)
{
    return origin.globals != NULL &&
        (origin.globals[(size_t)index / 64] &
         (UINT64_C(1) << (index % 64))) != 0;
}

static Origin
merge(BorrowCheck *check, Origin a, Origin b)
{
    Origin result = a;
    result.parameters |= b.parameters;
    if(b.depth > result.depth)
        result.depth = b.depth;
    result.invalid |= b.invalid;
    result.unknown |= b.unknown;
    if(a.globals == NULL) {
        result.globals = b.globals;
    } else if(b.globals != NULL && a.globals != b.globals) {
        int a_extra = 0;
        int b_extra = 0;
        for(size_t i = 0; i < check->global_words; i++) {
            a_extra |= (a.globals[i] & ~b.globals[i]) != 0;
            b_extra |= (b.globals[i] & ~a.globals[i]) != 0;
        }
        if(!b_extra) {
            result.globals = a.globals;
        } else if(!a_extra) {
            result.globals = b.globals;
        } else {
            uint64_t *bits = new_global_bits(check);
            if(bits != NULL) {
                for(size_t i = 0; i < check->global_words; i++)
                    bits[i] = a.globals[i] | b.globals[i];
            }
            result.globals = bits;
        }
    }
    return result;
}

static void
accumulate(BorrowCheck *check, Origin *destination, Origin source)
{
    Origin next = merge(check, *destination, source);
    if(next.parameters != destination->parameters ||
       next.depth != destination->depth ||
       next.invalid != destination->invalid ||
       next.unknown != destination->unknown ||
       next.globals != destination->globals) {
        *destination = next;
        check->changed = 1;
    }
}

static BorrowPlace
root_place(BorrowBinding *binding)
{
    return (BorrowPlace){binding, NULL};
}

static BorrowPlace
append_place(BorrowCheck *check, BorrowPlace place, const char *field, int index)
{
    if(place.root == NULL)
        return place;
    BorrowPath *path = calloc(1, sizeof(*path));
    if(path == NULL) {
        reject(check, check->current->fn->span,
               "out of memory checking text borrow paths");
        return (BorrowPlace){0};
    }
    path->parent = place.path;
    path->depth = place.path == NULL ? 1 : place.path->depth + 1;
    path->field = field;
    path->index = index;
    path->next_allocation = check->paths;
    check->paths = path;
    place.path = path;
    return place;
}

static int
paths_overlap(BorrowPath *left, BorrowPath *right)
{
    if(left == NULL || right == NULL)
        return 1;
    while(left->depth > right->depth)
        left = left->parent;
    while(right->depth > left->depth)
        right = right->parent;
    if(left == right)
        return 1;
    if(!paths_overlap(left->parent, right->parent))
        return 0;
    return left->index || right->index ||
        strcmp(left->field, right->field) == 0;
}

static int
places_overlap(BorrowPlace left, BorrowPlace right)
{
    return left.root != NULL && left.root == right.root &&
        paths_overlap(left.path, right.path);
}

static int
same_path(BorrowPath *left, BorrowPath *right)
{
    if(left == right)
        return 1;
    if(left == NULL || right == NULL || left->index != right->index)
        return 0;
    if(!left->index && strcmp(left->field, right->field) != 0)
        return 0;
    return same_path(left->parent, right->parent);
}

static int
same_place(BorrowPlace left, BorrowPlace right)
{
    return left.root == right.root && same_path(left.path, right.path);
}

static BorrowSource *
add_source(BorrowCheck *check, BorrowSource *sources, BorrowPlace place)
{
    if(place.root == NULL)
        return sources;
    for(BorrowSource *source = sources; source != NULL; source = source->next)
        if(same_place(source->place, place))
            return sources;
    BorrowSource *source = calloc(1, sizeof(*source));
    if(source == NULL) {
        reject(check, check->current->fn->span,
               "out of memory checking text borrow sources");
        return sources;
    }
    source->place = place;
    source->next = sources;
    source->next_allocation = check->sources;
    check->sources = source;
    return source;
}

static BorrowSource *
merge_sources(BorrowCheck *check, BorrowSource *destination,
              BorrowSource *sources)
{
    for(BorrowSource *source = sources; source != NULL; source = source->next)
        destination = add_source(check, destination, source->place);
    return destination;
}

static BorrowPlace
first_source(BorrowSource *sources)
{
    return sources == NULL ? (BorrowPlace){0} : sources->place;
}

static int
only_global(BorrowCheck *check, Origin origin, int index)
{
    if(index < 0 || !has_global(origin, index))
        return 0;
    for(size_t word = 0; word < check->global_words; word++) {
        uint64_t expected = word == (size_t)index / 64 ?
            UINT64_C(1) << (index % 64) : 0;
        if(origin.globals[word] != expected)
            return 0;
    }
    return 1;
}

static int
text_backing_conflict(BorrowCheck *check, BorrowPlace destination)
{
    for(int i = 0; i < check->active_count; i++)
        if(places_overlap(check->active[i].backing, destination))
            return 1;
    return 0;
}

static void
add_active_text_borrows(BorrowCheck *check, BorrowSource *sources, int depth)
{
    for(BorrowSource *source = sources; source != NULL; source = source->next) {
        if(check->active_count == check->active_capacity) {
            int capacity = check->active_capacity * 2;
            ActiveTextBorrow *active = realloc(check->active,
                (size_t)capacity * sizeof(*active));
            if(active == NULL) {
                reject(check, check->current->fn->span,
                       "out of memory checking active text borrows");
                return;
            }
            check->active = active;
            check->active_capacity = capacity;
        }
        check->active[check->active_count].backing = source->place;
        check->active[check->active_count].depth = depth;
        check->active_count++;
    }
}

static void
release_active_text_borrows(BorrowCheck *check, int depth)
{
    int kept = 0;
    for(int i = 0; i < check->active_count; i++) {
        if(check->active[i].depth == depth) {
            continue;
        }
        check->active[kept++] = check->active[i];
    }
    check->active_count = kept;
}

static BorrowBinding *
global_binding(BorrowCheck *check, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(ResolveGlobal(check->current->module, name, &owner, &global) != 1)
        return NULL;
    for(int i = 0; i < check->global_count; i++)
        if(check->globals[i].module == owner && check->globals[i].global == global)
            return &check->globals[i];
    return NULL;
}

static BorrowBinding *
binding(BorrowCheck *check, const char *name)
{
    for(int i = check->binding_count - 1; i >= 0; i--)
        if(!strcmp(check->bindings[i].name, name))
            return &check->bindings[i];
    return global_binding(check, name);
}

static int
contains_view(const ZirModule *module, const char *type, int depth)
{
    if(depth > 32 || type == NULL || !*type || *type == '*')
        return 0;
    if(!strcmp(type, "string") || SliceElementType(type, NULL, 0))
        return 1;
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return contains_view(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(contains_view(owner ? owner : module, field.type, depth + 1))
            return 1;
    return 0;
}

static int
view_type(BorrowCheck *check, const char *type)
{
    return contains_view(check->current->module, type, 0);
}

static int
contains_text(const ZirModule *module, const char *type, int depth)
{
    if(depth > 32 || type == NULL || !*type || *type == '*')
        return 0;
    if(!strcmp(type, "string"))
        return 1;
    char element[ZIR_NAME_MAX];
    if(SliceElementType(type, element, sizeof(element)) ||
       ArrayElementType(type, element, sizeof(element), NULL))
        return contains_text(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(contains_text(owner ? owner : module, field.type, depth + 1))
            return 1;
    return 0;
}

static uint64_t
text_type_hash(const ZirModule *module, const char *type)
{
    uint64_t hash = UINT64_C(14695981039346656037) ^ (uint64_t)(uintptr_t)module;
    for(const unsigned char *p = (const unsigned char *)type; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    return hash;
}

static int
text_view_type(BorrowCheck *check, const char *type)
{
    const ZirModule *module = check->current->module;
    if(strlen(type) >= ZIR_NAME_MAX)
        return contains_text(module, type, 0);
    if((check->text_type_count + 1) * 4 > check->text_type_capacity * 3) {
        size_t capacity = check->text_type_capacity ? check->text_type_capacity * 2 : 1024;
        TextTypeAnswer *grown = calloc(capacity, sizeof(*grown));
        if(grown == NULL)
            return contains_text(module, type, 0);
        for(size_t i = 0; i < check->text_type_capacity; i++) {
            const TextTypeAnswer *old = &check->text_types[i];
            if(old->module == NULL) continue;
            size_t slot = text_type_hash(old->module, old->type) & (capacity - 1);
            while(grown[slot].module != NULL) slot = (slot + 1) & (capacity - 1);
            grown[slot] = *old;
        }
        free(check->text_types);
        check->text_types = grown;
        check->text_type_capacity = capacity;
    }
    size_t mask = check->text_type_capacity - 1;
    size_t slot = text_type_hash(module, type) & mask;
    while(check->text_types[slot].module != NULL) {
        TextTypeAnswer *answer = &check->text_types[slot];
        if(answer->module == module && !strcmp(answer->type, type))
            return answer->holds_text;
        slot = (slot + 1) & mask;
    }
    TextTypeAnswer *answer = &check->text_types[slot];
    answer->module = module;
    copy_text(answer->type, sizeof(answer->type), type);
    answer->holds_text = contains_text(module, type, 0);
    check->text_type_count++;
    return answer->holds_text;
}

static const ZirExpr *
destination_root(const ZirFunction *fn, int index)
{
    while(index >= 0) {
        const ZirExpr *expression = &fn->exprs[index];
        if(expression->kind == ZIR_EXPR_IDENT)
            return expression;
        if(expression->kind != ZIR_EXPR_MEMBER &&
           expression->kind != ZIR_EXPR_POINTER_MEMBER &&
           expression->kind != ZIR_EXPR_INDEX)
            return NULL;
        index = expression->left;
    }
    return NULL;
}

static Origin expression_origin(BorrowCheck *check, int index);

static BorrowPlace
storage_place(BorrowCheck *check, int index, int follow_address)
{
    if(index < 0)
        return (BorrowPlace){0};
    const ZirExpr *expression = &check->current->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_IDENT) {
        BorrowBinding *source = binding(check, expression->name);
        if(source != NULL && follow_address && source->address_known &&
           source->address_backing.root != NULL)
            return source->address_backing;
        return root_place(source);
    }
    if(expression->kind == ZIR_EXPR_MEMBER ||
       expression->kind == ZIR_EXPR_POINTER_MEMBER)
        return append_place(check, storage_place(check, expression->left, 1),
                            expression->name, 0);
    if(expression->kind == ZIR_EXPR_INDEX)
        return append_place(check, storage_place(check, expression->left, 1),
                            NULL, 1);
    if(expression->kind == ZIR_EXPR_UNARY &&
       (!strcmp(expression->op, "&") || !strcmp(expression->op, "*")))
        return storage_place(check, expression->right, 1);
    if(expression->kind == ZIR_EXPR_SLICE)
        return storage_place(check, expression->left, 1);
    return (BorrowPlace){0};
}

static BorrowPlace
address_backing(BorrowCheck *check, int index, int *known)
{
    *known = 0;
    if(index < 0)
        return (BorrowPlace){0};
    const ZirExpr *expression = &check->current->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_UNARY && !strcmp(expression->op, "&")) {
        BorrowPlace backing = storage_place(check, expression->right, 1);
        *known = backing.root != NULL;
        return backing;
    }
    if(expression->kind != ZIR_EXPR_IDENT)
        return (BorrowPlace){0};
    BorrowBinding *source = binding(check, expression->name);
    if(source == NULL || source->type[0] != '*')
        return (BorrowPlace){0};
    *known = source->address_known;
    if(source->address_known)
        return source->address_backing;
    /* An opaque pointer parameter has no known storage place. Its binding
     * belongs to this function's temporary analysis and cannot be retained
     * by a global alias after the function check frees that storage. Global
     * bindings themselves remain alive for the entire analysis. */
    return source->global_index >= 0 ? root_place(source) : (BorrowPlace){0};
}

static BorrowSource *
text_view_backings(BorrowCheck *check, int index)
{
    const ZirFunction *fn = check->current->fn;
    const ZirExpr *expression = index < 0 ? NULL : &fn->exprs[index];
    if(expression == NULL)
        return NULL;
    if(expression->kind == ZIR_EXPR_IDENT) {
        BorrowBinding *source = binding(check, expression->name);
        if(source == NULL)
            return NULL;
        if(source->address_known && source->address_backing.root != NULL)
            return add_source(check, NULL, source->address_backing);
        if(source->text_backings != NULL)
            return source->text_backings;
        if(source->address_backing.root != NULL &&
           source->address_backing.root->local < 0)
            return add_source(check, NULL, source->address_backing);
        return source->local < 0 ?
            add_source(check, NULL, root_place(source)) : NULL;
    }
    if(expression->kind == ZIR_EXPR_MEMBER ||
       expression->kind == ZIR_EXPR_POINTER_MEMBER ||
       expression->kind == ZIR_EXPR_INDEX) {
        BorrowSource *result = NULL;
        BorrowSource *sources = text_view_backings(check, expression->left);
        for(BorrowSource *source = sources; source != NULL; source = source->next)
            result = add_source(check, result,
                append_place(check, source->place,
                    expression->kind == ZIR_EXPR_INDEX ? NULL : expression->name,
                    expression->kind == ZIR_EXPR_INDEX));
        return result;
    }
    if(expression->kind == ZIR_EXPR_SLICE) {
        BorrowSource *sources = text_view_backings(check, expression->left);
        if(sources != NULL)
            return sources;
        const ZirExpr *base = &fn->exprs[expression->left];
        return ArrayElementType(base->type, NULL, 0, NULL) ?
            add_source(check, NULL, storage_place(check, expression->left, 1)) :
            NULL;
    }
    if(expression->kind == ZIR_EXPR_CONDITIONAL)
        return merge_sources(check,
            text_view_backings(check, expression->right),
            text_view_backings(check, expression->third));
    if(expression->kind == ZIR_EXPR_FIELD_INIT)
        return text_view_backings(check, expression->right);
    if(expression->kind == ZIR_EXPR_COMPOUND) {
        BorrowSource *result = NULL;
        for(int child = expression->first_child; child >= 0;
            child = fn->exprs[child].next_sibling)
            if(view_type(check, fn->exprs[child].type))
                result = merge_sources(check, result,
                    text_view_backings(check, child));
        return result;
    }
    if(expression->kind != ZIR_EXPR_CALL)
        return NULL;
    if(!strcmp(expression->name, "TextView") ||
       !strcmp(expression->name, "VecSlice")) {
        if(expression->first_child < 0 ||
           fn->exprs[expression->first_child].next_sibling >= 0)
            return NULL;
        BorrowSource *sources =
            text_view_backings(check, expression->first_child);
        if(sources != NULL)
            return sources;
        const ZirExpr *argument = &fn->exprs[expression->first_child];
        return !view_type(check, argument->type) ?
            add_source(check, NULL,
                       storage_place(check, expression->first_child, 1)) : NULL;
    }
    if(!strcmp(expression->name, "BuilderFinish") ||
       !view_type(check, expression->type))
        return NULL;

    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(ResolveFunction(check->current->module, expression->name,
                       &owner, &callee) <= 0)
        return NULL;
    BorrowFunction *summary = NULL;
    for(int i = 0; i < check->count; i++)
        if(check->functions[i].fn == callee)
            summary = &check->functions[i];
    if(summary == NULL)
        return NULL;
    BorrowSource *result = NULL;
    for(int child = expression->first_child; child >= 0;
        child = fn->exprs[child].next_sibling) {
        int parameter = fn->exprs[child].argument_index;
        if(parameter < 0 || parameter >= 64 ||
           !(summary->returned.parameters & (UINT64_C(1) << parameter)))
            continue;
        BorrowSource *candidate = text_view_backings(check, child);
        if(candidate == NULL && !view_type(check, fn->exprs[child].type))
            candidate = add_source(check, NULL, storage_place(check, child, 1));
        result = merge_sources(check, result, candidate);
    }
    return result;
}

static Origin
call_origin(BorrowCheck *check, const ZirExpr *expression)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(ResolveFunction(check->current->module, expression->name, &owner, &callee) <= 0)
        return (Origin){0, 0, 1};
    BorrowFunction *summary = NULL;
    for(int i = 0; i < check->count; i++)
        if(check->functions[i].fn == callee)
            summary = &check->functions[i];
    if(summary == NULL)
        return (Origin){0, 0, 1};
    Origin result = {0};
    result.depth = summary->returned.depth;
    result.invalid = summary->returned.invalid;
    result.unknown = summary->returned.unknown;
    result.globals = summary->returned.globals;
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling) {
        int parameter = check->current->fn->exprs[child].argument_index;
        if(parameter >= 0 && parameter < 64 &&
           (summary->returned.parameters & (UINT64_C(1) << parameter)))
            result = merge(check, result, expression_origin(check, child));
    }
    return result;
}

static Origin
expression_origin(BorrowCheck *check, int index)
{
    if(index < 0)
        return (Origin){0};
    const ZirFunction *fn = check->current->fn;
    const ZirExpr *expression = &fn->exprs[index];
    switch(expression->kind) {
    case ZIR_EXPR_IDENT: {
        BorrowBinding *source = binding(check, expression->name);
        if(expression->type[0] != '[' && strchr(expression->type, '*') != NULL) {
            /* A direct pointer parameter owns no storage, but its caller
             * keeps the pointee live for this invocation. Views of its
             * fixed-array fields may be used here, never returned. */
            if(source != NULL && source->local < 0 &&
               source->origin.depth == 1 && !source->origin.invalid)
                return source->origin;
            if(source != NULL && source->address_known &&
               source->address_backing.root != NULL) {
                Origin pointee = {0};
                pointee.depth = source->address_backing.root->depth;
                if(source->address_backing.root->global_index >= 0)
                    pointee.globals = global_bit(check,
                        source->address_backing.root->global_index);
                return pointee;
            }
            Origin unknown = {0};
            unknown.unknown = 1;
            return unknown;
        }
        if(source == NULL)
            return (Origin){0}; /* Typed module-owned array or record storage. */
        if(view_type(check, source->type))
            return source->local >= 0 ? check->current->locals[source->local] : source->origin;
        return (Origin){0, source->depth, 0};
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER:
    case ZIR_EXPR_INDEX:
        return expression_origin(check, expression->left);
    case ZIR_EXPR_SLICE: {
        const ZirExpr *base = &fn->exprs[expression->left];
        if(ArrayElementType(base->type, NULL, 0, NULL)) {
            BorrowPlace place = storage_place(check, expression->left, 1);
            if(place.root != NULL) {
                Origin origin = {0};
                origin.depth = place.root->depth;
                if(place.root->global_index >= 0)
                    origin.globals = global_bit(check,
                                                place.root->global_index);
                return origin;
            }
        }
        return expression_origin(check, expression->left);
    }
    case ZIR_EXPR_CONDITIONAL:
        return merge(check, expression_origin(check, expression->right),
                     expression_origin(check, expression->third));
    case ZIR_EXPR_FIELD_INIT:
        return expression_origin(check, expression->right);
    case ZIR_EXPR_CAST: {
        const ZirType *source = expression->right >= 0 ?
            FindType(check->current->module, fn->exprs[expression->right].type, NULL) : NULL;
        if(SliceElementType(expression->type, NULL, 0) && source &&
           !strncmp(source->foreign_target, "go:", 3))
            return (Origin){0}; /* Native Go slices keep their backing storage live. */
        return expression_origin(check, expression->right);
    }
    case ZIR_EXPR_COMPOUND: {
        Origin result = {0};
        for(int child = expression->first_child; child >= 0;
            child = fn->exprs[child].next_sibling)
            if(view_type(check, fn->exprs[child].type))
                result = merge(check, result, expression_origin(check, child));
        return result;
    }
    case ZIR_EXPR_CALL:
        if(!strcmp(expression->name, "TextView"))
            return expression_origin(check, expression->first_child);
        if(!strcmp(expression->name, "BuilderFinish"))
            return (Origin){0};
        if(!strcmp(expression->name, "VecSlice")) {
            /* The view borrows its Vec argument's binding; the move rules
             * keep that binding alive while the view is in scope. */
            return expression_origin(check, expression->first_child);
        }
        if(view_type(check, expression->type)) {
            /* Host slice returns own their storage: the VM copies the
             * elements before the value reaches the caller. Ordinary
             * checked calls keep their summarized origins. */
            const ZirModule *owner = NULL;
            const ZirFunction *callee = NULL;
            if(ResolveFunction(check->current->module, expression->name,
                               &owner, &callee) <= 0)
                return (Origin){0};
            return call_origin(check, expression);
        }
        return (Origin){0, 0, 1}; /* A returned array/record is temporary storage. */
    case ZIR_EXPR_STRING:
        return (Origin){0};
    default:
        return (Origin){0, 0, 1};
    }
}

/* Inspect every range, including ranges passed directly to void/scalar calls. */
static void
check_ranges(BorrowCheck *check, int index)
{
    if(index < 0 || check->failed)
        return;
    const ZirExpr *expression = &check->current->fn->exprs[index];
    if(expression->kind == ZIR_EXPR_SLICE &&
       strcmp(check->current->fn->exprs[expression->left].type, "string") != 0 &&
       expression_origin(check, index).invalid)
        reject(check, expression->span, "slice range requires live owned backing storage");
    check_ranges(check, expression->left);
    check_ranges(check, expression->right);
    check_ranges(check, expression->third);
    for(int child = expression->first_child; child >= 0;
        child = check->current->fn->exprs[child].next_sibling)
        check_ranges(check, child);
}

static BorrowBinding *
add_binding(BorrowCheck *check, const char *name, const char *type,
            Origin origin, int local, int depth, int captured)
{
    BorrowBinding *item = &check->bindings[check->binding_count++];
    memset(item, 0, sizeof(*item));
    copy_text(item->name, sizeof(item->name), name);
    copy_text(item->type, sizeof(item->type), type);
    item->origin = origin;
    item->local = local;
    item->depth = depth;
    item->captured = captured;
    item->global_index = -1;
    return item;
}

static void check_function(BorrowCheck *check, BorrowFunction *function);

static void
check_function(BorrowCheck *check, BorrowFunction *function)
{
    const ZirFunction *fn = function->fn;
    check->current = function;
    check->binding_count = 0;
    check->active_count = 0;
    check->depth = 1;
    check->bindings = calloc((size_t)fn->stmt_count + 65, sizeof(*check->bindings));
    if(check->bindings == NULL) {
        reject(check, fn->span, "out of memory checking slice lifetimes");
        return;
    }
    check->active = calloc((size_t)fn->stmt_count + 1, sizeof(*check->active));
    if(check->active == NULL) {
        free(check->bindings);
        check->bindings = NULL;
        reject(check, fn->span, "out of memory checking text borrows");
        return;
    }
    check->active_capacity = fn->stmt_count + 1;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++) {
        const char *name = parameters->items[i].name;
        const char *colon = parameters->items[i].type;
        if(colon == NULL)
            continue;
        Origin origin = {0};
        int local = -1;
        if(view_type(check, colon)) {
            origin.parameters = UINT64_C(1) << i;
            local = fn->stmt_count + i;
            accumulate(check, &function->locals[local], origin);
        } else if(colon[0] == '*') {
            origin.depth = 1;
        }
        add_binding(check, name, colon, origin, local, 1, 0);
    }
    for(int i = 0; i < fn->stmt_count && !check->failed; i++) {
        const ZirStmt *statement = &fn->stmts[i];
        if(statement->kind == ZIR_STMT_BLOCK_CLOSE) {
            release_active_text_borrows(check, check->depth);
            while(check->binding_count && check->bindings[check->binding_count - 1].depth == check->depth)
                check->binding_count--;
            check->depth--;
            continue;
        }
        check_ranges(check, statement->lhs_root);
        check_ranges(check, statement->expr_root);
        if(statement->kind == ZIR_STMT_ASSIGN && statement->lhs_root >= 0) {
            BorrowPlace destination = storage_place(check, statement->lhs_root, 0);
            int global_alias = 0;
            if(destination.root != NULL &&
               destination.root->global_index >= 0)
                for(int g = 0; g < check->global_count; g++)
                    if(contains_text(check->globals[g].module,
                                     check->globals[g].type, 0) &&
                       has_global(check->global_origins[g],
                                  destination.root->global_index)) {
                        if(check->globals[g].global_backing_state == 1 &&
                           check->globals[g].text_backing.root == destination.root &&
                           !places_overlap(check->globals[g].text_backing,
                                           destination))
                            continue;
                        global_alias = 1;
                        break;
                    }
            if(destination.root != NULL &&
               (text_backing_conflict(check, destination) || global_alias))
                reject(check, statement->span,
                       "mutating text backing storage while its view is live");
        }
        if(statement->kind == ZIR_STMT_DECL) {
            if(view_type(check, statement->type)) {
                Origin source = expression_origin(check, statement->expr_root);
                int direct_view = SliceElementType(statement->type, NULL, 0) ||
                    !strcmp(statement->type, "string");
                accumulate(check, &function->locals[i], source);
                if((source.invalid && direct_view) ||
                   source.depth > check->depth)
                    reject(check, statement->span,
                           !strcmp(statement->type, "string") ?
                           "text view initializer outlives its backing storage" :
                           SliceElementType(statement->type, NULL, 0) ?
                           "slice initializer outlives its backing storage" :
                           "view initializer outlives its backing storage");
            }
            BorrowBinding *declared = add_binding(check, statement->name,
                    statement->type, (Origin){0}, i, check->depth, 0);
            declared->text_backings =
                text_view_backings(check, statement->expr_root);
            declared->text_backing = first_source(declared->text_backings);
            declared->address_backing = statement->type[0] == '*' ?
                address_backing(check, statement->expr_root,
                                &declared->address_known) : (BorrowPlace){0};
            if(text_view_type(check, statement->type))
                add_active_text_borrows(check, declared->text_backings,
                                        check->depth);
        } else if(statement->kind == ZIR_STMT_ASSIGN && statement->lhs_root >= 0) {
            const ZirExpr *destination = &fn->exprs[statement->lhs_root];
            const ZirExpr *assignment_root =
                destination_root(fn, statement->lhs_root);
            BorrowBinding *assignment_target =
                assignment_root == NULL ? NULL :
                binding(check, assignment_root->name);
            if(destination->kind == ZIR_EXPR_IDENT &&
               destination->type[0] == '*' && assignment_target != NULL) {
                assignment_target->address_backing =
                    address_backing(check, statement->expr_root,
                                    &assignment_target->address_known);
                /* A field or array element never redirects its container.
                 * Global pointer aliases may only retain global bindings;
                 * this function's local binding table is freed below. */
                if(assignment_target->global_index >= 0 &&
                   assignment_target->address_backing.root != NULL &&
                   assignment_target->address_backing.root->global_index < 0) {
                    assignment_target->address_backing = (BorrowPlace){0};
                    assignment_target->address_known = 0;
                }
            }
            if(view_type(check, destination->type)) {
                const ZirExpr *root = destination_root(fn, statement->lhs_root);
                BorrowBinding *target = root == NULL ? NULL : binding(check, root->name);
                Origin source = expression_origin(check, statement->expr_root);
                /* A zero value or a value backed only by static storage
                 * cannot escape a borrow, even through a pointer target. */
                int static_storage =
                    !source.invalid && !source.unknown && source.depth == 0 &&
                    source.parameters == 0;
                int direct_view = destination->kind == ZIR_EXPR_IDENT &&
                    (SliceElementType(destination->type, NULL, 0) ||
                     !strcmp(destination->type, "string"));
                if(!static_storage &&
                   (target == NULL ||
                    (target->captured && target->global_index < 0) ||
                    (source.invalid && direct_view) ||
                    source.depth > target->depth)) {
                    reject(check, statement->span,
                           !strcmp(destination->type, "string") ?
                           "text view assignment may escape its backing storage" :
                           SliceElementType(destination->type, NULL, 0) ?
                           "slice assignment may escape its backing storage" :
                           "view assignment may escape its backing storage");
                } else if(target != NULL && target->local >= 0) {
                    accumulate(check, &function->locals[target->local], source);
                    target->text_backings = merge_sources(check,
                        target->text_backings,
                        text_view_backings(check, statement->expr_root));
                    target->text_backing = first_source(target->text_backings);
                } else if(target != NULL && target->global_index >= 0 &&
                          static_storage) {
                    BorrowSource *backings =
                        text_view_backings(check, statement->expr_root);
                    BorrowPlace backing = first_source(backings);
                    accumulate(check, &check->global_origins[target->global_index],
                               source);
                    target->origin = check->global_origins[target->global_index];
                    if(source.globals != NULL) {
                        int precise = backings != NULL && backings->next == NULL &&
                            only_global(check, source,
                                        backing.root->global_index);
                        if(!precise) {
                            target->global_backing_state = 2;
                        } else if(target->global_backing_state == 0) {
                            target->text_backing = backing;
                            target->global_backing_state = 1;
                        } else if(target->global_backing_state == 1 &&
                                  !same_place(target->text_backing, backing)) {
                            target->global_backing_state = 2;
                        }
                    }
                    target->text_backings = merge_sources(check,
                        target->text_backings, backings);
                }
                if(text_view_type(check, destination->type) &&
                   !static_storage && target != NULL &&
                   !(target->captured && target->global_index < 0))
                    add_active_text_borrows(check,
                        text_view_backings(check, statement->expr_root),
                        target->depth);
            }
        } else if(statement->kind == ZIR_STMT_RETURN &&
                  view_type(check, fn->return_type)) {
            Origin source = expression_origin(check, statement->expr_root);
            if(source.invalid || source.unknown || source.depth > 0)
                reject(check, statement->span,
                       !strcmp(fn->return_type, "string") ?
                       "returned text view borrows local or temporary storage" :
                       SliceElementType(fn->return_type, NULL, 0) ?
                       "returned slice borrows local or temporary storage" :
                       "returned value borrows local or temporary storage");
            accumulate(check, &function->returned, source);
        }
        if(statement->kind == ZIR_STMT_IF || statement->kind == ZIR_STMT_WHILE ||
           statement->kind == ZIR_STMT_BLOCK_OPEN)
            check->depth++;
    }
    free(check->bindings);
    check->bindings = NULL;
    free(check->active);
    check->active = NULL;
    check->active_capacity = 0;
}


int
CheckSliceLifetimes(ZirProgram **programs, int count)
{
    BorrowCheck check = {0};
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            check.count += programs[p]->modules[m].function_count;
    check.functions = calloc((size_t)check.count, sizeof(*check.functions));
    if(check.functions == NULL && check.count != 0)
        return 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++)
            check.global_count += programs[p]->modules[m].global_count;
    check.global_words = ((size_t)check.global_count + 63) / 64;
    check.globals = calloc((size_t)check.global_count, sizeof(*check.globals));
    check.global_origins = calloc((size_t)check.global_count,
                                  sizeof(*check.global_origins));
    if((check.globals == NULL || check.global_origins == NULL) &&
       check.global_count != 0) {
        free(check.functions);
        free(check.globals);
        free(check.global_origins);
        return 0;
    }
    int global_index = 0;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++) {
                BorrowBinding *item = &check.globals[global_index];
                item->module = module;
                item->global = &module->globals[g];
                copy_text(item->name, sizeof(item->name),
                          module->globals[g].name);
                copy_text(item->type, sizeof(item->type),
                          module->globals[g].type);
                item->local = -1;
                item->captured = 1;
                item->global_index = global_index++;
            }
        }
    int index = 0;
    for(int p = 0; p < count; p++) {
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                BorrowFunction *function = &check.functions[index++];
                function->module = module;
                function->fn = &module->functions[f];
                function->locals = calloc((size_t)function->fn->stmt_count + 64, sizeof(Origin));
                if(function->locals == NULL)
                    check.failed = 1;
            }
        }
    }
    do {
        check.changed = 0;
        for(int i = 0; i < check.count && !check.failed; i++) {
            if(check.functions[i].fn->checked && !check.functions[i].fn->is_extern)
                check_function(&check, &check.functions[i]);
        }
    } while(check.changed && !check.failed);
    for(int i = 0; i < check.count; i++)
        free(check.functions[i].locals);
    while(check.global_bits != NULL) {
        GlobalBits *next = check.global_bits->next;
        free(check.global_bits);
        check.global_bits = next;
    }
    while(check.paths != NULL) {
        BorrowPath *next = check.paths->next_allocation;
        free(check.paths);
        check.paths = next;
    }
    while(check.sources != NULL) {
        BorrowSource *next = check.sources->next_allocation;
        free(check.sources);
        check.sources = next;
    }
    free(check.functions);
    free(check.globals);
    free(check.global_origins);
    free(check.text_types);
    return !check.failed;
}
