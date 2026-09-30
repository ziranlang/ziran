/* Named law obligations (LANGUAGE_DIRECTION.md): evaluation, gate, and the
 * machine-readable result schema shared by `ziran check` and bundles. */
#include "zir.h"
#include "zir_parse.h"
#include "zir_diagnostic.h"
#include "zir_check.h"
#include "zir_text.h"
#include "zir_law.h"
#include "zir_proof.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    LAW_PROVED = 0,
    LAW_DISPROVED,
    LAW_UNKNOWN,
    LAW_INVALID
} LawStatus;

static _Thread_local ZirLawEvidence *active_evidence;

static const char *
law_status_name(LawStatus status)
{
    return status == LAW_PROVED ? "proved" :
           status == LAW_DISPROVED ? "disproved" :
           status == LAW_INVALID ? "invalid" : "unknown";
}

/* kind: type ------------------------------------------------------------ */

static LawStatus
evaluate_type_law(const ZirModule *module, const ZirLaw *law,
                  char *detail, size_t size)
{
    const ZirType *type = FindType(module, law->payload, NULL);
    if(type == NULL) {
        snprintf(detail, size, "no visible type named %s", law->payload);
        return LAW_DISPROVED;
    }
    if(type->is_record_template || type->is_extern ||
       type->is_procedure_type) {
        snprintf(detail, size,
                 "%s is generic or external; no concrete layout to check",
                 law->payload);
        return LAW_UNKNOWN;
    }
    if(type->is_enum) {
        if(EnumMemberValue(type, NULL, NULL)) {
            snprintf(detail, size, "%s is a concrete enum", law->payload);
            return LAW_PROVED;
        }
        snprintf(detail, size, "%s has no valid member", law->payload);
        return LAW_DISPROVED;
    }
    {
        size_t bytes, alignment;
        if(TypeLayout(module, law->payload, &bytes, &alignment)) {
            snprintf(detail, size, "%s lays out as %zu bytes", law->payload,
                     bytes);
            return LAW_PROVED;
        }
        snprintf(detail, size, "%s has an incomplete field layout",
                 law->payload);
        return LAW_DISPROVED;
    }
}
/* Buffers evaluate_bounds_law keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateBoundsLawBuffers {
    char type[ZIR_TEXT_MAX];
    char resolved[ZIR_TEXT_MAX];
    char buffer[ZIR_TEXT_MAX];
} EvaluateBoundsLawBuffers;

static LawStatus evaluate_bounds_law(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size);

static LawStatus
evaluate_bounds_law_with_buffers(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size, EvaluateBoundsLawBuffers *buffers)
{
    {
        /* `Type == N` / `>= N` / `<= N` compare the resolved bound. */
        const char *ops[] = {"==", ">=", "<="};
        for(int o = 0; o < 3; o++) {
            const char *op = strstr(law->payload, ops[o]);
            if(op == NULL)
                continue;
            char wanted_text[ZIR_NAME_MAX];
            long wanted = 0;
            long bound = 0;
            size_t length = (size_t)(op - law->payload);
            if(length == 0 || length >= sizeof(buffers->type))
                break;
            memcpy(buffers->type, law->payload, length);
            buffers->type[length] = '\0';
            trim_in_place(buffers->type);
            snprintf(wanted_text, sizeof(wanted_text), "%s",
                     skip_ws(op + 2));
            trim_in_place(wanted_text);
            if(!EvaluateCompileExpression(module, wanted_text, law->span,
                                          0, &wanted)) {
                snprintf(detail, size,
                         "comparison bound %s is not a compile-time value",
                         wanted_text);
                return LAW_UNKNOWN;
            }
            {
                const char *cursor = buffers->type;
                for(int depth = 0; depth < 8; depth++) {
                    const ZirDefine *found = NULL;
                    if(cursor[0] == '[')
                        break;
                    for(int d = 0; d < module->define_count; d++)
                        if(strcmp(module->defines[d].name, cursor) == 0) {
                            found = &module->defines[d];
                            break;
                        }
                    if(found == NULL)
                        break;
                    cursor = skip_ws(found->value);
                }
                snprintf(buffers->resolved, sizeof(buffers->resolved), "%s", cursor);
                trim_in_place(buffers->resolved);
                {
                    const char *close = strchr(buffers->resolved, ']');
                    char bound_text[ZIR_NAME_MAX];
                    if(close == NULL ||
                       (size_t)(close - buffers->resolved - 1) >=
                           sizeof(bound_text)) {
                        snprintf(detail, size,
                                 "%s does not name a fixed-array type", buffers->type);
                        return LAW_DISPROVED;
                    }
                    memcpy(bound_text, buffers->resolved + 1,
                           (size_t)(close - buffers->resolved - 1));
                    bound_text[close - buffers->resolved - 1] = '\0';
                    trim_in_place(bound_text);
                    if(!EvaluateCompileExpression(module, bound_text,
                                                  law->span, 0, &bound)) {
                        snprintf(detail, size,
                                 "%s has a bound the evaluator cannot fold",
                                 buffers->type);
                        return LAW_UNKNOWN;
                    }
                }
            }
            {
                int holds = o == 0 ? bound == wanted :
                             o == 1 ? bound >= wanted : bound <= wanted;
                snprintf(detail, size, "%s bound %ld %s %ld", buffers->type, bound,
                         ops[o], wanted);
                return holds ? LAW_PROVED : LAW_DISPROVED;
            }
        }
    }
    {
    const char *payload = skip_ws(law->payload);
    const char *resolved = payload;
    char element[ZIR_NAME_MAX];
    int capacity;
    for(int depth = 0; depth < 8; depth++) {
        if(resolved[0] == '[')
            break;
        {
            const ZirDefine *found = NULL;
            for(int d = 0; d < module->define_count; d++)
                if(strcmp(module->defines[d].name, resolved) == 0) {
                    found = &module->defines[d];
                    break;
                }
            if(found == NULL)
                break;
            resolved = skip_ws(found->value);
        }
    }
    if(strlen(resolved) >= sizeof(buffers->buffer) ||
       !ArrayElementType(resolved, element, sizeof(element), &capacity)) {
        snprintf(detail, size, "%s does not name a fixed-array type",
                 law->payload);
        return LAW_DISPROVED;
    }
    if(capacity > 0) {
        snprintf(detail, size, "%s has the resolved bound %d",
                 law->payload, capacity);
        return LAW_PROVED;
    }
    {
        const char *close = strchr(resolved, ']');
        char bound[ZIR_NAME_MAX];
        long value = 0;
        size_t length;
        if(close == NULL || (length = (size_t)(close - resolved - 1)) == 0 ||
            length >= sizeof(bound)) {
            snprintf(detail, size, "%s has a malformed bound",
                     law->payload);
            return LAW_DISPROVED;
        }
        memcpy(bound, resolved + 1, length);
        bound[length] = '\0';
        trim_in_place(bound);
        if(EvaluateCompileExpression(module, bound, law->span, 0, &value)) {
            if(value > 0) {
                snprintf(detail, size, "%s folds its bound to %ld",
                         law->payload, value);
                return LAW_PROVED;
            }
            snprintf(detail, size, "%s folds its bound to %ld",
                     law->payload, value);
            return LAW_DISPROVED;
        }
    }
    snprintf(detail, size,
             "%s has a bound the compile-time evaluator cannot fold",
             law->payload);
    return LAW_UNKNOWN;
    }
}

/* kind: bounds ---------------------------------------------------------- */

static LawStatus
evaluate_bounds_law(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size)
{
    static _Thread_local EvaluateBoundsLawBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateBoundsLawBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    LawStatus returned = evaluate_bounds_law_with_buffers(module, law, detail, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* kind: effect ---------------------------------------------------------- */

static int
statement_writes_global(const ZirModule *module, const ZirFunction *fn,
                        int index)
{
    const ZirStmt *st = &fn->stmts[index];
    const char *name;
    if(st->kind != ZIR_STMT_ASSIGN || st->lhs_root < 0)
        return 0;
    if(fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT)
        return 0;
    name = fn->exprs[st->lhs_root].name;
    for(int g = 0; g < module->global_count; g++)
        if(strcmp(module->globals[g].name, name) == 0)
            return 1;
    return 0;
}

static int
body_calls_external(const ZirModule *module, const ZirFunction *fn)
{
    for(int e = 0; e < fn->expr_count; e++) {
        const ZirExpr *expr = &fn->exprs[e];
        const ZirModule *owner = NULL;
        const ZirFunction *callee = NULL;
        if(expr->kind != ZIR_EXPR_CALL || !expr->name[0])
            continue;
        if(ResolveFunction(module, expr->name, &owner, &callee) <= 0) {
            for(int i = 0; i < module->import_count; i++)
                if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
                   strcmp(module->imports[i].name, expr->name) == 0)
                    return 1;
            continue;
        }
        if(callee != NULL && callee->is_extern)
            return 1;
    }
    return 0;
}

static const ZirImport *
find_extern_import(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
           strcmp(module->imports[i].name, name) == 0)
            return &module->imports[i];
    return NULL;
}

/* `Proc == class` compares the derived effect class; a bare name keeps the
 * historical yes/no foreign check. */
static LawStatus
evaluate_effect_law(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    const ZirImport *import = NULL;
    char payload[ZIR_TEXT_MAX];
    char *comparison = NULL;
    copy_text(payload, sizeof(payload), law->payload);
    trim_in_place(payload);
    comparison = strstr(payload, "==");
    if(comparison != NULL) {
        char name[ZIR_NAME_MAX], expected[16];
        const char *wanted;
        size_t name_length = (size_t)(comparison - payload);
        *comparison = '\0';
        trim_in_place(payload);
        wanted = skip_ws(comparison + 2);
        snprintf(name, sizeof(name), "%.*s", (int)name_length, payload);
        snprintf(expected, sizeof(expected), "%s", wanted);
        trim_in_place(expected);
        if(effect_rank(expected) > 3 || expected[0] == '\0' ||
           name[0] == '\0') {
            snprintf(detail, size,
                     "expected class pure, observing, mutating, or external");
            return LAW_DISPROVED;
        }
        if(ResolveFunction(module, name, &owner, &fn) <= 0 || fn == NULL) {
            import = find_extern_import(module, name);
            if(import != NULL) {
                snprintf(detail, size, "%s is foreign; class is external",
                         name);
                return strcmp(expected, "external") == 0 ?
                       LAW_PROVED : LAW_DISPROVED;
            }
            snprintf(detail, size, "no visible procedure named %s", name);
            return LAW_DISPROVED;
        }
        if(strcmp(fn->effect_class, expected) == 0) {
            snprintf(detail, size, "%s is %s", name, expected);
            return LAW_PROVED;
        }
        snprintf(detail, size, "%s is %s, not %s", name,
                 fn->effect_class, expected);
        return LAW_DISPROVED;
    }
    if(ResolveFunction(module, law->payload, &owner, &fn) <= 0 ||
        fn == NULL) {
        import = find_extern_import(module, law->payload);
        if(import != NULL) {
            snprintf(detail, size,
                     "%s is foreign; its effects are not visible",
                     law->payload);
            return LAW_UNKNOWN;
        }
        snprintf(detail, size, "no visible procedure named %s",
                 law->payload);
        return LAW_DISPROVED;
    }
    if(fn->is_extern) {
        snprintf(detail, size,
                 "%s is foreign; its effects are not visible", law->payload);
        return LAW_UNKNOWN;
    }
    if(body_calls_external(module, fn)) {
        snprintf(detail, size,
                 "%s calls foreign code; its effects are not visible",
                 law->payload);
        return LAW_UNKNOWN;
    }
    for(int s = 0; s < fn->stmt_count; s++)
        if(statement_writes_global(module, fn, s)) {
            snprintf(detail, size,
                     "%s writes a file-scope global", law->payload);
            return LAW_DISPROVED;
        }
    snprintf(detail, size,
             "%s reads only parameters and calls checked code",
             law->payload);
    return LAW_PROVED;
}

/* kind: abi ------------------------------------------------------------- */

/* Local portable-signature shape check; the VM's portable_type stays private
 * to its own translation unit. */
static int
portable_signature_type(const ZirModule *module, const char *type, int depth)
{
    const ZirType *record;
    const char *dot;
    char name[ZIR_NAME_MAX];
    if(depth >= 32)
        return 0;
    type = skip_ws(type);
    if(*ScalarType(type))
        return 1;
    dot = strchr(type, '.');
    if(dot != NULL && dot - type < (ptrdiff_t)sizeof(name)) {
        memcpy(name, type, (size_t)(dot - type));
        name[dot - type] = '\0';
        type = name;
    }
    record = FindType(module, type, NULL);
    if(record == NULL)
        return 0;
    if(record->is_enum)
        return EnumMemberValue(record, NULL, NULL);
    if(record->is_extern || record->is_record_template ||
       record->is_procedure_type || record->is_union)
        return 0;
    {
        size_t offset = 0;
        ZirTypeField field;
        int status;
        while((status = TypeNextField(record, &offset, &field)) == 1)
            if(!portable_signature_type(module, field.type, depth + 1))
                return 0;
        return status == 0;
    }
}
/* Buffers evaluate_abi_law keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EvaluateAbiLawBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} EvaluateAbiLawBuffers;

static LawStatus evaluate_abi_law(const ZirModule *module, const ZirLaw *law,
                 char *detail, size_t size);

static LawStatus
evaluate_abi_law_with_buffers(const ZirModule *module, const ZirLaw *law,
                 char *detail, size_t size, EvaluateAbiLawBuffers *buffers)
{
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    int count;
    if(ResolveFunction(module, law->payload, &owner, &fn) <= 0 ||
        fn == NULL) {
        if(find_extern_import(module, law->payload) != NULL) {
            snprintf(detail, size,
                     "%s is foreign; its ABI is the host contract",
                     law->payload);
            return LAW_UNKNOWN;
        }
        snprintf(detail, size, "no visible procedure named %s",
                 law->payload);
        return LAW_DISPROVED;
    }
    if(fn->is_extern) {
        snprintf(detail, size,
                 "%s is foreign; its ABI is the host contract",
                 law->payload);
        return LAW_UNKNOWN;
    }
    count = *skip_ws(fn->args) ?
        split_top_level(fn->args, buffers->parameters[0], 64,
                        sizeof(buffers->parameters[0])) : 0;
    for(int p = 0; p < count; p++) {
        char *colon = strchr(buffers->parameters[p], ':');
        if(colon == NULL ||
           !portable_signature_type(module, skip_ws(colon + 1), 0)) {
            snprintf(detail, size,
                     "%s parameter %d is outside the portable ABI",
                     law->payload, p);
            return LAW_DISPROVED;
        }
    }
    if(!portable_signature_type(module, fn->return_type, 0)) {
        snprintf(detail, size, "%s result is outside the portable ABI",
                 law->payload);
        return LAW_DISPROVED;
    }
    snprintf(detail, size, "%s has a portable signature", law->payload);
    return LAW_PROVED;
}

static LawStatus
evaluate_abi_law(const ZirModule *module, const ZirLaw *law,
                 char *detail, size_t size)
{
    static _Thread_local EvaluateAbiLawBuffers *spares[16];
    static _Thread_local int spare_count;
    EvaluateAbiLawBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    LawStatus returned = evaluate_abi_law_with_buffers(module, law, detail, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* kind: size ------------------------------------------------------------ */

static LawStatus
evaluate_size_law(const ZirModule *module, const ZirLaw *law,
                  char *detail, size_t size)
{
    const char *ops[] = {"==", ">=", "<="};
    for(int o = 0; o < 3; o++) {
        const char *op = strstr(law->payload, ops[o]);
        char type[ZIR_TEXT_MAX], wanted_text[ZIR_NAME_MAX];
        size_t bytes = 0, alignment = 0;
        long wanted = 0;
        size_t length;
        if(op == NULL)
            continue;
        length = (size_t)(op - law->payload);
        if(length == 0 || length >= sizeof(type))
            break;
        memcpy(type, law->payload, length);
        type[length] = '\0';
        trim_in_place(type);
        snprintf(wanted_text, sizeof(wanted_text), "%s", skip_ws(op + 2));
        trim_in_place(wanted_text);
        if(!EvaluateCompileExpression(module, wanted_text, law->span, 0,
                                      &wanted)) {
            snprintf(detail, size,
                     "comparison size %s is not a compile-time value",
                     wanted_text);
            return LAW_UNKNOWN;
        }
        if(!TypeLayout(module, type, &bytes, &alignment)) {
            snprintf(detail, size, "%s has no concrete layout", type);
            return LAW_DISPROVED;
        }
        {
            int holds = o == 0 ? bytes == (size_t)wanted :
                         o == 1 ? bytes >= (size_t)wanted :
                                  bytes <= (size_t)wanted;
            snprintf(detail, size, "%s lays out as %zu bytes %s %ld", type,
                     bytes, ops[o], wanted);
            return holds ? LAW_PROVED : LAW_DISPROVED;
        }
    }
    snprintf(detail, size,
             "size laws need `Type == N`, `>= N`, or `<= N`");
    return LAW_DISPROVED;
}

/* kind: custom ---------------------------------------------------------- */

static LawStatus
evaluate_custom_law(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size)
{
    int truth = 0;
    if(EvaluateCompileConditionBound(module, law->payload, law->span,
                                     NULL, NULL, NULL, NULL, 0, &truth)) {
        snprintf(detail, size, "payload evaluated to %d", truth);
        return truth ? LAW_PROVED : LAW_DISPROVED;
    }
    snprintf(detail, size, "payload is outside the compile-time evaluator");
    return LAW_UNKNOWN;
}

/* kind: forall ---------------------------------------------------------- */

#define FORALL_MAX_VARIABLES 8
#define FORALL_BUDGET 1000000L

/* `NAME: LO..HI, NAME: LO..HI => condition`: exhaustive over the inclusive
 * integer ranges. A domain above the budget is unknown, never a pass. */
#define FORALL_ENUM_MAX 256

typedef struct ForallBuffers {
    char names[FORALL_MAX_VARIABLES][ZIR_NAME_MAX];
    char types[FORALL_MAX_VARIABLES][ZIR_NAME_MAX];
    char head[ZIR_TEXT_MAX];
    char parts[FORALL_MAX_VARIABLES][ZIR_TEXT_MAX];
    long members[FORALL_MAX_VARIABLES][FORALL_ENUM_MAX];
    char exprs[FORALL_MAX_VARIABLES][ZIR_NAME_MAX];
} ForallBuffers;

static LawStatus evaluate_forall_law_with(const ZirModule *module,
                    const ZirLaw *law, char *detail, size_t size,
                    ForallBuffers *buffers);

static LawStatus
evaluate_forall_law(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size)
{
    ForallBuffers *buffers = AllocateOrExit(sizeof(*buffers));
    LawStatus status = evaluate_forall_law_with(module, law, detail, size,
                                                buffers);
    free(buffers);
    return status;
}

/* Collect the values of every member of an enum, in declaration order. */
static int
forall_enum_members(const ZirType *type, long *values, int max)
{
    const char *cursor = type->body;
    int count = 0;
    while(*cursor) {
        char member[ZIR_NAME_MAX];
        size_t length = 0;
        int64_t value;
        while(*cursor == ',' || isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '\0')
            break;
        while((isalnum((unsigned char)*cursor) || *cursor == '_') &&
              length + 1 < sizeof(member))
            member[length++] = *cursor++;
        member[length] = '\0';
        while(*cursor && *cursor != ',' && *cursor != '\n')
            cursor++;
        if(length == 0 || count >= max ||
           !EnumMemberValue(type, member, &value))
            return -1;
        values[count++] = (long)value;
    }
    return count;
}

/* `NAME: LO..HI` or `NAME: EnumType`, comma separated, then `=> condition`.
 * Exhaustive over every combination. A domain above the budget, or a
 * condition the evaluator cannot decide, is unknown, never a pass. */
static LawStatus
evaluate_forall_law_with(const ZirModule *module, const ZirLaw *law,
                    char *detail, size_t size, ForallBuffers *buffers)
{
    char (*names)[ZIR_NAME_MAX] = buffers->names;
    char (*types)[ZIR_NAME_MAX] = buffers->types;
    char *head = buffers->head;
    char (*parts)[ZIR_TEXT_MAX] = buffers->parts;
    long low[FORALL_MAX_VARIABLES], span_count[FORALL_MAX_VARIABLES];
    long index[FORALL_MAX_VARIABLES], values[FORALL_MAX_VARIABLES];
    long element_span[FORALL_MAX_VARIABLES];
    int is_enum[FORALL_MAX_VARIABLES], array_length[FORALL_MAX_VARIABLES];
    char (*exprs)[ZIR_NAME_MAX] = buffers->exprs;
    const char *arrow = strstr(law->payload, "=>");
    const char *body;
    long total = 1, checked = 0;
    int count;
    if(arrow == NULL) {
        snprintf(detail, size, "forall laws need `NAME: LO..HI => condition`");
        return LAW_DISPROVED;
    }
    body = skip_ws(arrow + 2);
    snprintf(head, ZIR_TEXT_MAX, "%.*s", (int)(arrow - law->payload),
             law->payload);
    count = split_top_level(head, parts[0], FORALL_MAX_VARIABLES,
                            sizeof(parts[0]));
    if(count <= 0 || *body == '\0') {
        snprintf(detail, size, "forall laws need variables and a condition");
        return LAW_DISPROVED;
    }
    for(int v = 0; v < count; v++) {
        char *colon = strchr(parts[v], ':');
        char *dots;
        if(colon == NULL) {
            snprintf(detail, size, "variable %d needs `NAME: LO..HI`", v);
            return LAW_DISPROVED;
        }
        *colon = '\0';
        trim_in_place(parts[v]);
        snprintf(names[v], ZIR_NAME_MAX, "%s", parts[v]);
        if(!isalpha((unsigned char)names[v][0]) && names[v][0] != '_') {
            snprintf(detail, size, "invalid quantified binding"); return LAW_DISPROVED;
        }
        for(const char *n = names[v]; *n; n++)
            if(!isalnum((unsigned char)*n) && *n != '_') {
                snprintf(detail, size, "invalid quantified binding"); return LAW_DISPROVED;
            }
        for(int prior = 0; prior < v; prior++)
            if(!strcmp(names[prior], names[v])) {
                snprintf(detail, size, "duplicate quantified binding %s", names[v]); return LAW_DISPROVED;
            }
        for(int d = 0; d < module->define_count; d++)
            if(!strcmp(module->defines[d].name, names[v])) {
                snprintf(detail, size, "quantified binding %s shadows a constant", names[v]); return LAW_DISPROVED;
            }
        types[v][0] = '\0';
        is_enum[v] = 0;
        array_length[v] = 0;
        element_span[v] = 0;
        dots = strstr(colon + 1, "..");
        if(*skip_ws(colon + 1) == '[' && dots != NULL) {
            /* `NAME: [N]LO..HI`: every sequence of N elements in LO..HI. */
            const char *open = skip_ws(colon + 1);
            const char *close = strchr(open, ']');
            char length_text[ZIR_NAME_MAX], lo_text[ZIR_NAME_MAX];
            char hi_text[ZIR_NAME_MAX];
            long length = 0, high = 0, cases = 1;
            if(close == NULL || close > dots ||
               (size_t)(close - open - 1) >= sizeof(length_text)) {
                snprintf(detail, size, "%s needs `[N]LO..HI`", names[v]);
                return LAW_DISPROVED;
            }
            snprintf(length_text, sizeof(length_text), "%.*s",
                     (int)(close - open - 1), open + 1);
            snprintf(lo_text, sizeof(lo_text), "%.*s", (int)(dots - close - 1),
                     close + 1);
            snprintf(hi_text, sizeof(hi_text), "%s", dots + 2);
            trim_in_place(length_text);
            trim_in_place(lo_text);
            trim_in_place(hi_text);
            if(!EvaluateCompileExpression(module, length_text, law->span, 0,
                                          &length) ||
               !EvaluateCompileExpression(module, lo_text, law->span, 0,
                                          &low[v]) ||
               !EvaluateCompileExpression(module, hi_text, law->span, 0,
                                          &high)) {
                snprintf(detail, size,
                         "sequence bounds of %s are not compile-time values",
                         names[v]);
                return LAW_UNKNOWN;
            }
            if(length < 1 || length > 16 || high < low[v]) {
                snprintf(detail, size,
                         "%s needs a length of 1..16 and a non-empty range",
                         names[v]);
                return LAW_DISPROVED;
            }
            uint64_t distance = (uint64_t)high - (uint64_t)low[v];
            if(distance >= FORALL_BUDGET || low[v] < INT32_MIN || high > INT32_MAX) {
                snprintf(detail, size, "sequence domain exceeds evaluator width or budget"); return LAW_UNKNOWN;
            }
            element_span[v] = (long)distance + 1;
            array_length[v] = (int)length;
            for(long e = 0; e < length; e++) {
                if(cases > FORALL_BUDGET / element_span[v]) {
                    snprintf(detail, size,
                             "domain exceeds the budget of %ld cases",
                             FORALL_BUDGET);
                    return LAW_UNKNOWN;
                }
                cases *= element_span[v];
            }
            span_count[v] = cases;
            snprintf(types[v], ZIR_NAME_MAX, "[%ld]s32", length);
        } else if(dots == NULL) {
            const ZirType *type;
            char type_name[ZIR_NAME_MAX];
            int members;
            snprintf(type_name, sizeof(type_name), "%s", colon + 1);
            trim_in_place(type_name);
            type = FindType(module, type_name, NULL);
            if(type == NULL || !type->is_enum) {
                snprintf(detail, size,
                         "%s needs a range LO..HI or an enum type", names[v]);
                return LAW_DISPROVED;
            }
            members = forall_enum_members(type, buffers->members[v],
                                          FORALL_ENUM_MAX);
            if(members <= 0) {
                snprintf(detail, size, "enum %s has no usable members",
                         type_name);
                return LAW_DISPROVED;
            }
            is_enum[v] = 1;
            snprintf(types[v], ZIR_NAME_MAX, "%s", type->name);
            low[v] = 0;
            span_count[v] = members;
        } else {
            char lo_text[ZIR_NAME_MAX], hi_text[ZIR_NAME_MAX];
            long high = 0;
            snprintf(lo_text, sizeof(lo_text), "%.*s",
                     (int)(dots - colon - 1), colon + 1);
            snprintf(hi_text, sizeof(hi_text), "%s", dots + 2);
            trim_in_place(lo_text);
            trim_in_place(hi_text);
            if(!EvaluateCompileExpression(module, lo_text, law->span, 0,
                                          &low[v]) ||
               !EvaluateCompileExpression(module, hi_text, law->span, 0,
                                          &high)) {
                snprintf(detail, size,
                         "range of %s is not a compile-time value", names[v]);
                return LAW_UNKNOWN;
            }
            if(high < low[v]) {
                snprintf(detail, size, "range of %s is empty", names[v]);
                return LAW_DISPROVED;
            }
            uint64_t distance = (uint64_t)high - (uint64_t)low[v];
            if(distance >= FORALL_BUDGET) {
                snprintf(detail, size, "domain exceeds the budget of %ld cases", FORALL_BUDGET); return LAW_UNKNOWN;
            }
            span_count[v] = (long)distance + 1;
        }
        if(span_count[v] > FORALL_BUDGET ||
           total > FORALL_BUDGET / span_count[v]) {
            snprintf(detail, size, "domain exceeds the budget of %ld cases",
                     FORALL_BUDGET);
            return LAW_UNKNOWN;
        }
        total *= span_count[v];
        index[v] = 0;
    }
    for(;;) {
        int truth = 0;
        for(int v = 0; v < count; v++) {
            exprs[v][0] = '\0';
            values[v] = is_enum[v] ? buffers->members[v][index[v]] :
                        low[v] + index[v];
            if(array_length[v] > 0) {
                /* Digits of the case number, most significant first. */
                long digits[16], rest = index[v];
                size_t used = (size_t)snprintf(exprs[v], ZIR_NAME_MAX, "s32.[");
                for(int e = array_length[v] - 1; e >= 0; e--) {
                    digits[e] = low[v] + rest % element_span[v];
                    rest /= element_span[v];
                }
                for(int e = 0; e < array_length[v] && used < ZIR_NAME_MAX; e++)
                    used += (size_t)snprintf(exprs[v] + used,
                                             ZIR_NAME_MAX - used, "%s%ld",
                                             e ? ", " : "", digits[e]);
                if(used + 2 < ZIR_NAME_MAX)
                    snprintf(exprs[v] + used, ZIR_NAME_MAX - used, "]");
            }
        }
        if(!EvaluateCompileConditionBound(module, body, law->span,
                                          (const char (*)[ZIR_NAME_MAX])names,
                                          (const char (*)[ZIR_NAME_MAX])types,
                                          (const char (*)[ZIR_NAME_MAX])exprs,
                                          values, count, &truth)) {
            size_t used = (size_t)snprintf(detail, size,
                "condition is outside the compile-time evaluator at");
            for(int v = 0; v < count && used < size; v++)
                used += (size_t)snprintf(detail + used, size - used, "%s%s=%ld",
                                         v ? ", " : " ", names[v], values[v]);
            return LAW_UNKNOWN;
        }
        if(active_evidence) active_evidence->cases_checked = (uint64_t)checked + 1;
        if(!truth) {
            if(active_evidence) {
                char *out = active_evidence->counterexample;
                size_t used = (size_t)snprintf(out, ZIR_TEXT_MAX, "{");
                for(int v = 0; v < count && used < ZIR_TEXT_MAX; v++) {
                    if(exprs[v][0])
                        used += (size_t)snprintf(out + used, ZIR_TEXT_MAX - used,
                            "%s\"%s\":\"%s\"", v ? "," : "", names[v], exprs[v]);
                    else
                        used += (size_t)snprintf(out + used, ZIR_TEXT_MAX - used,
                            "%s\"%s\":%ld", v ? "," : "", names[v], values[v]);
                }
                if(used < ZIR_TEXT_MAX) snprintf(out + used, ZIR_TEXT_MAX - used, "}");
            }
            size_t used = (size_t)snprintf(detail, size, "counterexample");
            for(int v = 0; v < count && used < size; v++) {
                if(exprs[v][0])
                    used += (size_t)snprintf(detail + used, size - used,
                                             "%s%s=%s", v ? ", " : " ",
                                             names[v], exprs[v]);
                else
                    used += (size_t)snprintf(detail + used, size - used,
                                             "%s%s=%ld", v ? ", " : " ",
                                             names[v], values[v]);
            }
            return LAW_DISPROVED;
        }
        checked++;
        {
            int v = count - 1;
            while(v >= 0 && index[v] == span_count[v] - 1) {
                index[v] = 0;
                v--;
            }
            if(v < 0) break;
            index[v]++;
        }
    }
    snprintf(detail, size, "held for all %ld cases", checked);
    return LAW_PROVED;
}

static int evaluate_law_uncached(const ZirModule *module, const ZirLaw *law,
                                 char *detail, size_t size);

/* Checking, law reports, and bundling all ask for the same results. A
 * result depends only on the law, its module, and its certificate, so it
 * is computed once and kept on the law with its evidence. */
int
EvaluateLaw(const ZirProgram *program, const ZirModule *module,
            const ZirLaw *law, char *detail, size_t size)
{
    ZirLaw *cached = (ZirLaw *)law;
    (void)program;
    if(cached->evaluated_in == module && cached->evaluated_proof == law->proof &&
       cached->evaluated_detail != NULL) {
        copy_text(detail, size, cached->evaluated_detail);
        return cached->evaluated_status;
    }
    int status = evaluate_law_uncached(module, law, detail, size);
    cached->evaluated_in = module;
    cached->evaluated_proof = law->proof;
    cached->evaluated_status = status;
    cached->evaluated_detail = KeepText(detail);
    return status;
}

static int
evaluate_law_uncached(const ZirModule *module, const ZirLaw *law,
                      char *detail, size_t size)
{
    LawStatus status;
    ZirLawEvidence *evidence = &((ZirLaw *)law)->evidence;
    int waived = evidence->waived;
    memset(evidence, 0, sizeof(*evidence));
    evidence->waived = waived;
    copy_text(evidence->method, sizeof(evidence->method),
              !strcmp(law->kind, "theorem") ? "kernel" :
              !strcmp(law->kind, "forall") ? "exhaustive" :
              !strcmp(law->kind, "custom") ? "evaluation" : "structural");
    const char *arrow = strstr(law->payload, "=>");
    if(arrow && (!strcmp(law->kind, "forall") || !strcmp(law->kind, "theorem"))) {
        snprintf(evidence->domain, sizeof(evidence->domain), "%.*s",
                 (int)(arrow - law->payload), law->payload);
        trim_in_place(evidence->domain);
    } else copy_text(evidence->domain, sizeof(evidence->domain), law->payload);
    ZirLawEvidence *prior_evidence = active_evidence;
    active_evidence = evidence;
    detail[0] = '\0';
    if(!strcmp(law->kind, "type"))
        status = evaluate_type_law(module, law, detail, size);
    else if(!strcmp(law->kind, "bounds"))
        status = evaluate_bounds_law(module, law, detail, size);
    else if(!strcmp(law->kind, "effect"))
        status = evaluate_effect_law(module, law, detail, size);
    else if(!strcmp(law->kind, "abi"))
        status = evaluate_abi_law(module, law, detail, size);
    else if(!strcmp(law->kind, "size"))
        status = evaluate_size_law(module, law, detail, size);
    else if(!strcmp(law->kind, "forall"))
        status = evaluate_forall_law(module, law, detail, size);
    else if(!strcmp(law->kind, "custom"))
        status = evaluate_custom_law(module, law, detail, size);
    else if(!strcmp(law->kind, "theorem"))
        status = (LawStatus)EvaluateTheorem(module, law, detail, size);
    else {
        snprintf(detail, size, "no checker for kind %s", law->kind);
        status = LAW_UNKNOWN;
    }
    if(!strcmp(law->kind, "custom") && status != LAW_UNKNOWN)
        evidence->cases_checked = 1;
    active_evidence = prior_evidence;
    return (int)status;
}

const char *
LawStatusName(int status)
{
    return law_status_name((LawStatus)status);
}

static int
law_waived(ZirProgram **programs, int count, const ZirLaw *law)
{
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int w = 0; w < module->law_waiver_count; w++) {
                const ZirModule *owner = NULL;
                if(FindVisibleLaw(module, module->law_waivers[w].name, &owner) == law) return 1;
            }
        }
    return 0;
}

/* A law belongs to its declaring module. Imports use the same qualification
 * rules as proof references; unrelated modules may reuse a short name. */
static int
check_law_identity(const ZirProgram *program)
{
    int failures = 0;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int l = 0; l < module->law_count; l++)
            for(int k = l + 1; k < module->law_count; k++)
                if(!strcmp(module->laws[l].name, module->laws[k].name)) {
                    Diagnostic(module->laws[k].span, "law.identity",
                               "law %s is declared more than once", module->laws[k].name);
                    failures++;
                }
        for(int w = 0; w < module->law_waiver_count; w++) {
            const ZirModule *owner = NULL;
            if(!FindVisibleLaw(module, module->law_waivers[w].name, &owner)) {
                Diagnostic(module->law_waivers[w].span, "law.waiver",
                           "waiver names no law: %s", module->law_waivers[w].name);
                failures++;
            }
            for(int k = w + 1; k < module->law_waiver_count; k++)
                if(!strcmp(module->law_waivers[w].name, module->law_waivers[k].name)) {
                    Diagnostic(module->law_waivers[k].span, "law.waiver", "duplicate law waiver"); failures++;
                }
        }
    }
    return failures;
}

int
CheckLawGates(ZirProgram **programs, int count)
{
    int failures = 0;
    if(!PrepareLawProofs(programs, count)) return 0;
    for(int p = 0; p < count; p++)
        failures += check_law_identity(programs[p]);
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int l = 0; l < module->law_count; l++) {
                const ZirLaw *law = &module->laws[l];
                char detail[ZIR_TEXT_MAX];
                int status = EvaluateLaw(programs[p], module, law, detail,
                                         sizeof(detail));
                ((ZirLaw *)law)->evidence.waived = law_waived(programs, count, law);
                if(status == 2 && law_waived(programs, count, law))
                    continue;
                if(status != 2 && law_waived(programs, count, law)) {
                    Diagnostic(law->span, "law.waiver",
                               "law %s is %s; only unknown laws can be waived",
                               law->name, law_status_name((LawStatus)status));
                    failures++;
                }
                if(status == 0)
                    continue;
                Diagnostic(law->span, "law.gate",
                           "law %s is %s: %s", law->name,
                           law_status_name((LawStatus)status), detail);
                failures++;
            }
        }
    return failures == 0;
}

static void
print_json_string(FILE *out, const char *text)
{
    fputc('"', out);
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if(*p == '"' || *p == '\\')
            fprintf(out, "\\%c", *p);
        else if(*p < 0x20)
            fprintf(out, "\\u%04x", *p);
        else
            fputc(*p, out);
    }
    fputc('"', out);
}

void
PrintLawResults(ZirProgram **programs, int count, FILE *out)
{
    if(out == NULL)
        return;
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int l = 0; l < module->law_count; l++) {
                const ZirLaw *law = &module->laws[l];
                char detail[ZIR_TEXT_MAX];
                int status = EvaluateLaw(programs[p], module, law, detail,
                                         sizeof(detail));
                fprintf(out, "{\"law\":");
                print_json_string(out, law->name);
                fprintf(out, ",\"kind\":");
                print_json_string(out, law->kind);
                fprintf(out, ",\"span\":{\"file\":");
                print_json_string(out, SpanPath(law->span));
                fprintf(out, ",\"line\":%d,\"column\":%d},",
                        law->span.line, law->span.column);
                fprintf(out, "\"status\":\"%s\",\"detail\":",
                        law_status_name((LawStatus)status));
                print_json_string(out, detail);
                fprintf(out, ",\"module\":");
                print_json_string(out, module->name);
                fprintf(out, ",\"evidence\":{\"method\":");
                print_json_string(out, law->evidence.method);
                fprintf(out, ",\"domain\":");
                print_json_string(out, law->evidence.domain);
                fprintf(out, ",\"cases_checked\":%llu,\"counterexample\":%s,\"waived\":%s}}\n",
                        (unsigned long long)law->evidence.cases_checked,
                        law->evidence.counterexample[0] ? law->evidence.counterexample : "null",
                        law->evidence.waived ? "true" : "false");
            }
            for(int w = 0; w < module->law_waiver_count; w++) {
                const ZirLawWaiver *waiver = &module->law_waivers[w];
                fprintf(out, "{\"law\":");
                print_json_string(out, waiver->name);
                fprintf(out, ",\"kind\":\"waiver\",\"span\":{\"file\":");
                print_json_string(out, SpanPath(waiver->span));
                fprintf(out, ",\"line\":%d,\"column\":%d},",
                        waiver->span.line, waiver->span.column);
                fprintf(out, "\"status\":\"waived\",\"detail\":");
                print_json_string(out, waiver->reason);
                fprintf(out, "}\n");
            }
        }
}

/* ---- Effect classes (parallel contract, LANGUAGE_DIRECTION.md) ------- */

static int
vec_operation_name(const char *name)
{
    return !strcmp(name, "TextView") ||
           !strcmp(name, "VecPush") || !strcmp(name, "VecClear") ||
           !strcmp(name, "VecFree") || !strcmp(name, "VecSwap") ||
           !strcmp(name, "VecPop") || !strcmp(name, "VecGet") ||
           !strcmp(name, "VecClone") || !strcmp(name, "VecSlice") ||
           !strcmp(name, "BuilderAppend") || !strcmp(name, "BuilderFinish");
}

int
effect_rank(const char *name)
{
    return !strcmp(name, "external") ? 3 :
           !strcmp(name, "mutating") ? 2 :
           !strcmp(name, "observing") ? 1 : 0;
}

static const char *
effect_name(int rank)
{
    return rank == 3 ? "external" : rank == 2 ? "mutating" :
           rank == 1 ? "observing" : "pure";
}

static int
expr_references_global(const ZirExpr *expr)
{
    return expr->kind == ZIR_EXPR_IDENT && expr->is_global_value;
}

static int
lhs_writes_through_pointer(const ZirFunction *fn, int root)
{
    for(int index = root; index >= 0 && index < fn->expr_count;) {
        const ZirExpr *expr = &fn->exprs[index];
        if(expr->kind == ZIR_EXPR_POINTER_MEMBER ||
           (expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "*")))
            return 1;
        if(expr->kind != ZIR_EXPR_MEMBER)
            return 0;
        index = expr->left;
    }
    return 0;
}

static const char *
imported_callee_class(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->import_count; i++)
        if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
           strcmp(module->imports[i].name, name) == 0)
            return "external";
    return NULL;
}

/* One derivation sweep: fold each body's reads, writes, and callee classes
 * into the highest applicable rank. Iterated to a fixpoint by the caller. */
static int
sweep_effect_class(ZirProgram *program)
{
    int changed = 0;
    for(int m = 0; m < program->module_count; m++) {
        ZirModule *module = &program->modules[m];
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *fn = &module->functions[f];
            int rank = effect_rank(fn->effect_class);
            int reads_global = 0;
            if(fn->is_extern)
                rank = 3;
            for(int s = 0; s < fn->stmt_count; s++) {
                const ZirStmt *st = &fn->stmts[s];
                if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0) {
                    const ZirExpr *lhs = &fn->exprs[st->lhs_root];
                    if(expr_references_global(lhs) ||
                       lhs_writes_through_pointer(fn, st->lhs_root))
                        rank = rank < 2 ? 2 : rank;
                }
            }
            for(int e = 0; e < fn->expr_count; e++) {
                const ZirExpr *expr = &fn->exprs[e];
                const ZirModule *owner = NULL;
                const ZirFunction *callee = NULL;
                const char *imported;
                if(expr_references_global(expr))
                    reads_global = 1;
                if(expr->kind != ZIR_EXPR_CALL || !expr->name[0] ||
                   expr->slot_type[0])
                    continue;
                if(MapPrimitiveName(expr->name)) {
                    int map_rank = !strcmp(expr->name, "MapSet") ||
                        !strcmp(expr->name, "MapInit") || !strcmp(expr->name, "MapDelete") ||
                        !strcmp(expr->name, "MapClear") ? 2 : 1;
                    if(rank < map_rank) rank = map_rank;
                    continue;
                }
                imported = imported_callee_class(module, expr->name);
                if(imported != NULL || !strcmp(expr->name, "print")) {
                    rank = 3;
                    continue;
                }
                if(ResolveFunction(module, expr->name, &owner, &callee) == 1 &&
                   callee != NULL) {
                    int callee_rank = effect_rank(callee->effect_class);
                    if(callee_rank == 3)
                        rank = 3;
                    else if(callee_rank == 2 && rank < 2)
                        rank = 2;
                    else if(callee_rank == 1 && rank < 1)
                        rank = 1;
                }
            }
            if(rank == 0 && reads_global)
                rank = 1;
            {
                const char *name = effect_name(rank);
                if(strcmp(fn->effect_class, name) != 0) {
                    copy_text(fn->effect_class, sizeof(fn->effect_class),
                              name);
                    changed = 1;
                }
            }
        }
    }
    return changed;
}

void
DeriveEffectClasses(ZirProgram **programs, int count)
{
    for(int pass = 0; pass < 64; pass++) {
        int changed = 0;
        for(int p = 0; p < count; p++)
            changed |= sweep_effect_class(programs[p]);
        if(!changed)
            break;
    }
}

/* True when the type is a pointer, a record with pointer fields, or a
 * fixed array of such. Vec storage counts through its data pointer. */
static int
gpu_type_holds_pointer(const ZirModule *module, const char *type, int depth)
{
    const ZirType *record;
    size_t offset = 0;
    ZirTypeField field;
    int status;
    if(depth >= 16)
        return 0;
    type = skip_ws(type);
    if(type[0] == '*')
        return 1;
    record = FindType(module, type, NULL);
    if(record != NULL && record->is_map)
        return 1;
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template)
        return 0;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(field.type[0] == '*' ||
           gpu_type_holds_pointer(module, field.type, depth + 1))
            return 1;
    return 0;
}

/* ---- #parallel for regions ------------------------------------------- */

static int
stmt_declares_name(const ZirStmt *st, const char *name)
{
    return st->kind == ZIR_STMT_DECL && strcmp(st->name, name) == 0;
}

static int
destination_identifier(const ZirFunction *fn, int root, char *out,
                       size_t size, int *through_pointer)
{
    const ZirExpr *expr = &fn->exprs[root];
    *through_pointer = 0;
    if(expr->kind == ZIR_EXPR_IDENT) {
        snprintf(out, size, "%s", expr->name);
        return 1;
    }
    for(int index = root; index >= 0 && index < fn->expr_count;) {
        const ZirExpr *cursor = &fn->exprs[index];
        if(cursor->kind == ZIR_EXPR_IDENT) {
            snprintf(out, size, "%s", cursor->name);
            return 1;
        }
        if(cursor->kind == ZIR_EXPR_POINTER_MEMBER ||
           (cursor->kind == ZIR_EXPR_UNARY && !strcmp(cursor->op, "*")))
            *through_pointer = 1;
        if(cursor->kind != ZIR_EXPR_MEMBER &&
           !(cursor->kind == ZIR_EXPR_POINTER_MEMBER) &&
           !(cursor->kind == ZIR_EXPR_UNARY && !strcmp(cursor->op, "*")))
            return 0;
        index = cursor->left;
    }
    return 0;
}

static int
region_declares(const ZirFunction *fn, int begin, int end, const char *name)
{
    for(int i = begin; i < end; i++)
        if(stmt_declares_name(&fn->stmts[i], name))
            return 1;
    return 0;
}

static const ZirFunction *
parallel_callee(const ZirModule *module, const ZirExpr *expr)
{
    const ZirModule *owner = NULL;
    const ZirFunction *callee = NULL;
    if(expr->slot_type[0])
        return NULL;
    if(ResolveFunction(module, expr->name, &owner, &callee) == 1)
        return callee;
    return NULL;
}

static int
check_parallel_region(const ZirProgram *program, const ZirModule *module,
                      const ZirFunction *fn, int while_index)
{
    int close = -1, depth = 1;
    int preamble_begin = while_index;
    for(int i = while_index - 1; i >= 0 && preamble_begin == while_index;
        i--) {
        if(depth == 1)
            preamble_begin = i + 1;
    }
    (void)preamble_begin;
    for(int i = while_index + 1; i < fn->stmt_count; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_FOR || kind == ZIR_STMT_BLOCK_OPEN ||
           kind == ZIR_STMT_IF_CASE)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0) {
            close = i;
            break;
        }
    }
    if(close < 0) {
        Diagnostic(fn->stmts[while_index].span, "parallel.region",
                   "unterminated #parallel region");
        return 0;
    }
    /* Region-local names: declarations from the enclosing block open through
     * the while header (the lowered range preamble) and everything declared
     * inside the region body. */
    int block_open = -1, scan = 0;
    depth = 0;
    for(int i = while_index - 1; i >= 0; i--) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_BLOCK_CLOSE)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_OPEN || kind == ZIR_STMT_IF ||
                kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR ||
                kind == ZIR_STMT_IF_CASE) {
            if(depth == 0) {
                block_open = i;
                break;
            }
            depth--;
        }
    }
    scan = block_open >= 0 ? block_open + 1 : 0;
    if(fn->stmts[while_index].is_gpu) {
        /* A GPU offload validates before execution: pointer storage and
         * pointer arguments cannot cross to the device, and allocator
         * backed Vec mutation has no device heap. */
        for(int s = scan; s <= close; s++) {
            const ZirStmt *st = &fn->stmts[s];
            if(st->kind == ZIR_STMT_DECL && st->type[0] != '\0' &&
               gpu_type_holds_pointer(module, st->type, 0)) {
                Diagnostic(st->span, "gpu.pointer",
                           "#parallel_gpu storage cannot hold pointers: %s",
                           st->name);
                return 0;
            }
            {
                int roots[2] = {st->expr_root, st->lhs_root};
                for(int r = 0; r < 2; r++)
                    for(int e = roots[r]; e >= 0 && e < fn->expr_count;
                        e = fn->exprs[e].next_sibling) {
                        const ZirExpr *expr = &fn->exprs[e];
                        int child;
                        if(expr->kind != ZIR_EXPR_CALL || !expr->name[0])
                            continue;
                        if(vec_operation_name(expr->name) &&
                           strcmp(expr->name, "VecGet") != 0) {
                            Diagnostic(st->span, "gpu.allocator",
                                       "#parallel_gpu cannot allocate: %s",
                                       expr->name);
                            return 0;
                        }
                        child = expr->first_child;
                        while(child >= 0 && child < fn->expr_count) {
                            const char *arg_type = fn->exprs[child].type;
                            if(arg_type[0] != '\0' &&
                               gpu_type_holds_pointer(module, arg_type, 0)) {
                                Diagnostic(st->span, "gpu.pointer",
                                           "#parallel_gpu argument holds a pointer in %s",
                                           expr->name);
                                return 0;
                            }
                            child = fn->exprs[child].next_sibling;
                        }
                    }
            }
        }
    }
    {
        /* The lowered tail is `if cursor == last { break; } advance;`; the
         * region drives indices itself, so leave-controls there are fine.
         * Everything else that exits the region breaks threaded execution. */
        int region_loop = fn->stmts[while_index].loop_id;
        int body_end = close - 1;
        const char *cursor_name = NULL;
        if(while_index >= 1)
            cursor_name = fn->stmts[while_index - 1].kind ==
                          ZIR_STMT_DECL ?
                          fn->stmts[while_index - 1].name : NULL;
        while(body_end > while_index + 3 && cursor_name != NULL) {
            const ZirStmt *tail = &fn->stmts[body_end - 1];
            if(tail->kind == ZIR_STMT_ASSIGN && tail->lhs_root >= 0 &&
               fn->exprs[tail->lhs_root].kind == ZIR_EXPR_IDENT &&
               !strcmp(fn->exprs[tail->lhs_root].name, cursor_name)) {
                body_end--;
                continue;
            }
            if(tail->kind == ZIR_STMT_BLOCK_CLOSE &&
               body_end - 2 > while_index &&
               fn->stmts[body_end - 2].kind == ZIR_STMT_BREAK &&
               fn->stmts[body_end - 3].kind == ZIR_STMT_IF) {
                body_end -= 3;
                continue;
            }
            break;
        }
        {
            int nested_loops = 0;
            for(int s = while_index + 1; s < body_end; s++) {
                const ZirStmt *st = &fn->stmts[s];
                ZirStmtKind kind = st->kind;
                if(kind == ZIR_STMT_WHILE || kind == ZIR_STMT_FOR)
                    nested_loops++;
                else if(kind == ZIR_STMT_BLOCK_CLOSE &&
                        nested_loops > 0)
                    nested_loops--;
                if(kind == ZIR_STMT_RETURN) {
                    Diagnostic(st->span, "parallel.leave",
                               "#parallel region cannot return from inside");
                    return 0;
                }
                if(kind == ZIR_STMT_BREAK || kind == ZIR_STMT_CONTINUE) {
                    if(st->target_id == region_loop ||
                       (st->target_id == 0 && nested_loops == 0)) {
                        Diagnostic(st->span, "parallel.leave",
                                   "#parallel region cannot break or continue out");
                        return 0;
                    }
                }
            }
        }
    }
    for(int s = scan; s <= close; s++) {
        const ZirStmt *st = &fn->stmts[s];
        int through_pointer = 0;
        char name[ZIR_NAME_MAX];
        int is_body = s > while_index && s < close;
        if(st->kind == ZIR_STMT_WHILE && st->is_parallel && is_body) {
            Diagnostic(st->span, "parallel.nest",
                       "#parallel regions cannot nest");
            return 0;
        }
        if(st->kind == ZIR_STMT_ASSIGN && st->lhs_root >= 0 && is_body) {
            if(!destination_identifier(fn, st->lhs_root, name, sizeof(name),
                                       &through_pointer)) {
                Diagnostic(st->span, "parallel.memory",
                           "#parallel writes need an iteration-local binding");
                return 0;
            }
            if(through_pointer ||
               !region_declares(fn, scan, close + 1, name)) {
                Diagnostic(st->span, "parallel.memory",
                           "#parallel writes need an iteration-local binding: %s",
                           name);
                return 0;
            }
        }
        if(st->expr_root >= 0 || st->lhs_root >= 0) {
            int roots[2] = {st->expr_root, st->lhs_root};
            for(int r = 0; r < 2; r++) {
                for(int e = roots[r]; e >= 0 && e < fn->expr_count;
                    e = fn->exprs[e].next_sibling) {
                    const ZirExpr *expr = &fn->exprs[e];
                    const ZirFunction *callee;
                    (void)program;
                    if(expr->kind != ZIR_EXPR_CALL || !expr->name[0])
                        continue;
                    if(MapPrimitiveName(expr->name) &&
                       (!strcmp(expr->name, "MapSet") || !strcmp(expr->name, "MapInit") ||
                        !strcmp(expr->name, "MapDelete") || !strcmp(expr->name, "MapClear"))) {
                        Diagnostic(st->span, "parallel.memory",
                                   "#parallel cannot mutate shared map storage: %s", expr->name);
                        return 0;
                    }
                    if(vec_operation_name(expr->name)) {
                        int first = expr->first_child;
                        if(!strcmp(expr->name, "VecGet"))
                            continue;
                        if(first >= 0 && first < fn->expr_count &&
                            fn->exprs[first].kind == ZIR_EXPR_IDENT &&
                            !region_declares(fn, scan, close + 1,
                                             fn->exprs[first].name) &&
                            !stmt_declares_name(st,
                                                fn->exprs[first].name)) {
                            Diagnostic(st->span, "parallel.memory",
                                       "#parallel mutates storage outside the region: %s",
                                       fn->exprs[first].name);
                            return 0;
                        }
                        continue;
                    }
                    if(imported_callee_class(module, expr->name) != NULL) {
                        Diagnostic(st->span, "parallel.effect",
                                   "#parallel cannot call foreign code: %s",
                                   expr->name);
                        return 0;
                    }
                    if(!strcmp(expr->name, "print")) {
                        Diagnostic(st->span, "parallel.effect",
                                   "#parallel cannot print; output order would depend on scheduling");
                        return 0;
                    }
                    callee = parallel_callee(module, expr);
                    if(callee != NULL &&
                       effect_rank(callee->effect_class) > 1) {
                        Diagnostic(st->span, "parallel.effect",
                                   "#parallel calls %s code: %s",
                                   callee->effect_class, expr->name);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

int
CheckParallelRegions(ZirProgram **programs, int count)
{
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *fn = &module->functions[f];
                for(int s = 0; s < fn->stmt_count; s++)
                    if(fn->stmts[s].kind == ZIR_STMT_WHILE &&
                       fn->stmts[s].is_parallel &&
                       !check_parallel_region(programs[p], module, fn, s))
                        return 0;
            }
        }
    return 1;
}
