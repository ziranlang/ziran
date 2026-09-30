#include "zir_check_internal.h"
/* Buffers expression_type keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ExpressionTypeBuffers {
    char left[ZIR_NAME_MAX];
    char right[ZIR_NAME_MAX];
    char operand[ZIR_TEXT_MAX];
    ZirFunction probe;
    char replacement[ZIR_TEXT_MAX];
    char literal[ZIR_TEXT_MAX];
    char specialized_args[ZIR_TEXT_MAX];
} ExpressionTypeBuffers;

const char *expression_type(Checker *c, int index);

static int
fit_bits(const char *type)
{
    const char *digits = type;
    while(*digits && !isdigit((unsigned char)*digits)) digits++;
    return *digits ? atoi(digits) : 64;
}

/* Bind type parameters named in a generic record application such as
 * *Table(K, V) from ARGUMENT, an instance of that record like
 * *Table(string, s32). Bindings already made are left as they are. */
static void
bind_from_application(Checker *c, const char *parameters, const char *wanted,
                      const char *argument, char (*bound)[ZIR_NAME_MAX])
{
    while(*wanted == '*' && *argument == '*') {
        wanted = skip_ws(wanted + 1);
        argument = skip_ws(argument + 1);
    }
    const char *open = strchr(wanted, '(');
    const ZirType *instance = FindType(c->module, argument, NULL);
    if(open == NULL || instance == NULL || !instance->is_synthetic_application)
        return;
    size_t base_length = (size_t)(open - wanted);
    while(base_length > 0 && isspace((unsigned char)wanted[base_length - 1])) base_length--;
    if(strlen(instance->template_name) != base_length ||
       strncmp(instance->template_name, wanted, base_length))
        return;
    char inner[ZIR_TEXT_MAX];
    const char *close = strrchr(open, ')');
    if(close == NULL || (size_t)(close - open - 1) >= sizeof(inner))
        return;
    memcpy(inner, open + 1, (size_t)(close - open - 1));
    inner[close - open - 1] = '\0';
    char (*names)[ZIR_TEXT_MAX] = calloc(16, sizeof(*names));
    char (*types)[ZIR_TEXT_MAX] = calloc(16, sizeof(*types));
    if(names != NULL && types != NULL) {
        int name_count = split_top_level(inner, names[0], 16, sizeof(names[0]));
        int type_count = split_top_level(instance->template_args, types[0], 16,
                                         sizeof(types[0]));
        for(int i = 0; i < name_count && i < type_count; i++) {
            const char *name = names[i][0] == '$' ? names[i] + 1 : names[i];
            int which = TemplateParameterIndex(parameters, name, strlen(name));
            if(which >= 0 && which < 16 && !bound[which][0])
                copy_text(bound[which], ZIR_NAME_MAX, types[i]);
        }
    }
    free(names);
    free(types);
}

/* x: type_of(a + b) declares x with the checked type of that expression.
 * The concrete type replaces TYPE, so saved IR and emitters never see
 * type_of. Returns 0 after reporting an operand that has no type. */
int
resolve_declared_type_of(Checker *c, char *type, size_t capacity, ZirSourceSpan span)
{
    char operand[ZIR_TEXT_MAX];
    if(!TypeOfOperand(type, operand, sizeof(operand)))
        return 1;
    ZirFunction *probe = calloc(1, sizeof(*probe));
    if(probe == NULL) {
        c->failed = 1;
        return 0;
    }
    ZirFunction *saved_fn = c->fn;
    ZirStmt *saved_stmt = c->current_stmt;
    char saved_expected[ZIR_NAME_MAX];
    copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
    c->expected_type[0] = '\0';
    int errors = c->errors;
    int root = ParseExpr(probe, c->module, operand, span);
    c->fn = probe;
    c->current_stmt = NULL;
    const char *inferred = root >= 0 ? expression_type(c, root) : "";
    char resolved[ZIR_NAME_MAX];
    copy_text(resolved, sizeof(resolved),
              !strcmp(inferred, "integer") ? "s64" :
              !strcmp(inferred, "real") ? "float64" : inferred);
    c->fn = saved_fn;
    c->current_stmt = saved_stmt;
    copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
    free(probe->exprs);
    free(probe);
    if(!resolved[0] || !strcmp(resolved, "void") || !strcmp(resolved, "null")) {
        if(c->errors == errors)
            error(c, span, "type_of operand has no type", operand);
        return 0;
    }
    copy_text(type, capacity, resolved);
    return 1;
}

/* How well an argument of type FOUND fits parameter type WANTED, lower is
 * better: 0 exact, 100 an untyped literal's usual type, 200 another literal
 * type, 200 plus the added bits for a widening, 400 another checked
 * conversion, -1 not at all. */
static int
overload_fit(Checker *c, const ZirModule *owner, const char *wanted, const char *found)
{
    const char *scalar = ScalarType(wanted);
    if(*scalar) wanted = scalar;
    if(!strcmp(wanted, found)) return 0;
    /* A procedure from another module names its records in its own scope:
     * V there is M.V here. */
    if(owner != NULL && owner != c->module) {
        const char *w = wanted, *f = found;
        while(*w == '*' && *f == '*') {
            w = skip_ws(w + 1);
            f = skip_ws(f + 1);
        }
        const ZirType *declared = FindType(owner, w, NULL);
        if(declared != NULL && declared == FindType(c->module, f, NULL))
            return 0;
    }
    if(!strcmp(found, "integer") && integer_type(wanted))
        return !strcmp(wanted, "s64") ? 100 : 200;
    if((!strcmp(found, "integer") || !strcmp(found, "real")) && wanted[0] == 'f')
        return !strcmp(wanted, "float64") && !strcmp(found, "real") ? 100 : 200;
    if(widens_losslessly(wanted, found))
        return 200 + fit_bits(wanted) - fit_bits(found);
    if(compatible_checked(c, wanted, found)) return 400;
    return -1;
}

typedef struct OverloadCandidate {
    const ZirFunction *fn;
    int score;
    const ZirModule *owner;
} OverloadCandidate;

/* Score FN, declared in OWNER, for the call's arguments, or -1 when it
 * cannot take them. */
static int
overload_score(Checker *c, const ZirModule *owner, const ZirFunction *fn,
               char (*types)[ZIR_NAME_MAX],
               char (*names)[ZIR_NAME_MAX], int count)
{
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
    int filled[64] = {0}, score = 0, total = -1, next = 0;
    if(parameters == NULL || defaults == NULL)
        goto done;
    total = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 64, sizeof(parameters[0])) : 0;
    if(fn->default_args[0])
        split_top_level(fn->default_args, defaults[0], 64, sizeof(defaults[0]));
    if(count > total) { score = -1; goto done; }
    for(int argument = 0; argument < count && score >= 0; argument++) {
        int position = -1;
        if(names[argument][0]) {
            for(int p = 0; p < total && position < 0; p++) {
                const char *colon = strchr(parameters[p], ':');
                size_t length = colon ? (size_t)(colon - parameters[p]) : 0;
                while(length > 0 && isspace((unsigned char)parameters[p][length - 1])) length--;
                if(colon && strlen(names[argument]) == length &&
                   !strncmp(parameters[p], names[argument], length))
                    position = p;
            }
        } else {
            while(next < total && filled[next]) next++;
            position = next < total ? next : -1;
        }
        if(position < 0 || filled[position]) { score = -1; break; }
        filled[position] = 1;
        const char *colon = strchr(parameters[position], ':');
        char wanted[ZIR_NAME_MAX];
        copy_text(wanted, sizeof(wanted), colon ? skip_ws(colon + 1) : "");
        char *assignment = top_level_assignment(wanted);
        if(assignment) *assignment = '\0';
        trim_in_place(wanted);
        int fit = overload_fit(c, owner, wanted, types[argument]);
        if(fit < 0) score = -1;
        else score += fit;
    }
    for(int p = 0; p < total && score >= 0; p++)
        if(!filled[p] && top_level_assignment(defaults[p]) == NULL &&
           top_level_assignment(parameters[p]) == NULL)
            score = -1;
done:
    free(parameters);
    free(defaults);
    return score;
}

/* Defaults are expanded while parsing a call, by name, which cannot choose
 * an overload. Once one is chosen, append its defaults for the parameters
 * the call leaves out, as that expansion would. */
static void
append_overload_defaults(Checker *c, int index, const ZirFunction *fn,
                         const char *alias)
{
    if(!fn->default_args[0])
        return;
    char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
    char (*defaults)[ZIR_TEXT_MAX] = calloc(64, sizeof(*defaults));
    unsigned char used[64] = {0};
    if(parameters == NULL || defaults == NULL) {
        free(parameters);
        free(defaults);
        c->failed = 1;
        return;
    }
    int count = split_top_level(fn->args, parameters[0], 64, sizeof(parameters[0]));
    if(split_top_level(fn->default_args, defaults[0], 64, sizeof(defaults[0])) != count)
        count = 0;
    int last = -1;
    for(int child = c->fn->exprs[index].first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        last = child;
        const char *named = c->fn->exprs[child].argument_name;
        int position = -1;
        for(int i = 0; i < count && position < 0; i++) {
            if(*named) {
                const char *colon = strchr(parameters[i], ':');
                size_t length = colon ? (size_t)(colon - parameters[i]) : 0;
                while(length > 0 && isspace((unsigned char)parameters[i][length - 1])) length--;
                if(colon && strlen(named) == length && !strncmp(parameters[i], named, length))
                    position = i;
            } else if(!used[i])
                position = i;
        }
        if(position >= 0) used[position] = 1;
    }
    for(int i = 0; i < count; i++) {
        char *assignment = top_level_assignment(defaults[i]);
        char *colon = strchr(parameters[i], ':');
        if(used[i] || assignment == NULL || colon == NULL)
            continue;
        *colon = '\0';
        char name[ZIR_NAME_MAX], value[ZIR_TEXT_MAX];
        copy_text(name, sizeof(name), trim(parameters[i]));
        copy_text(value, sizeof(value), skip_ws(assignment + 1));
        if(!strcmp(value, "#caller_location")) {
            if(!CallerLocationLiteral(c->module, c->fn->exprs[index].span,
                                      value, sizeof(value))) {
                c->failed = 1;
                break;
            }
        } else if(!DefaultIsLiteral(value)) {
            char helper[ZIR_NAME_MAX];
            FunctionDefaultHelperName(fn, i, helper, sizeof(helper));
            if(alias[0])
                snprintf(value, sizeof(value), "%s.%s()", alias, helper);
            else
                snprintf(value, sizeof(value), "%s()", helper);
        }
        int root = ParseExpr(c->fn, c->module, value, c->fn->exprs[index].span);
        if(root < 0) {
            c->failed = 1;
            break;
        }
        copy_text(c->fn->exprs[root].argument_name,
                  sizeof(c->fn->exprs[root].argument_name), name);
        c->fn->exprs[root].next_sibling = -1;
        if(last < 0)
            c->fn->exprs[index].first_child = root;
        else
            c->fn->exprs[last].next_sibling = root;
        last = root;
        c->conversions_applied = 1;
    }
    free(parameters);
    free(defaults);
}

/* A call to an overloaded procedure names the member whose parameters take
 * its arguments best; rewrite the call to that member's unique name. */
static void
select_overload(Checker *c, int index)
{
    ZirExpr *e = &c->fn->exprs[index];
    if(e->is_this || !e->name[0] || *lookup(c, e->name))
        return;
    char alias[ZIR_NAME_MAX] = "", base[ZIR_NAME_MAX];
    const char *dot = strchr(e->name, '.');
    if(dot != NULL) {
        snprintf(alias, sizeof(alias), "%.*s", (int)(dot - e->name), e->name);
        copy_text(base, sizeof(base), dot + 1);
    } else
        copy_text(base, sizeof(base), e->name);
    const ZirModule *modules[65];
    int module_count = 0;
    if(!alias[0])
        modules[module_count++] = c->module;
    for(int i = 0; i < c->module->import_count && module_count < 65; i++) {
        const ZirImport *import = &c->module->imports[i];
        if(import->resolved_module == NULL) continue;
        if(alias[0] ? import->kind == ZIR_IMPORT_MODULE && !strcmp(import->name, alias) :
                      import->kind == ZIR_IMPORT_OPEN ||
                      (import->kind == ZIR_IMPORT_MODULE && import->is_using))
            modules[module_count++] = import->resolved_module;
    }
    OverloadCandidate candidates[64];
    int candidate_count = 0;
    for(int m = 0; m < module_count; m++)
        for(int f = 0; f < modules[m]->function_count && candidate_count < 64; f++) {
            const ZirFunction *fn = &modules[m]->functions[f];
            if(strcmp(fn->overload_name, base) ||
               (modules[m] != c->module && !fn->is_public))
                continue;
            candidates[candidate_count++] = (OverloadCandidate){fn, 0, modules[m]};
        }
    if(candidate_count == 0)
        return;
    char (*types)[ZIR_NAME_MAX] = calloc(64, sizeof(*types));
    char (*names)[ZIR_NAME_MAX] = calloc(64, sizeof(*names));
    int count = 0;
    if(types == NULL || names == NULL) {
        free(types);
        free(names);
        c->failed = 1;
        return;
    }
    int saved_inference = c->inference_only;
    c->inference_only = 1;
    for(int child = c->fn->exprs[index].first_child; child >= 0 && count < 64;
        child = c->fn->exprs[child].next_sibling, count++) {
        copy_text(types[count], sizeof(types[count]), expression_type(c, child));
        copy_text(names[count], sizeof(names[count]), c->fn->exprs[child].argument_name);
    }
    c->inference_only = saved_inference;
    e = &c->fn->exprs[index];
    const ZirFunction *best = NULL;
    int best_score = -1, tied = 0;
    for(int i = 0; i < candidate_count; i++) {
        int score = overload_score(c, candidates[i].owner, candidates[i].fn, types,
                                   names, count);
        if(score < 0) continue;
        if(best == NULL || score < best_score) {
            best = candidates[i].fn;
            best_score = score;
            tied = 0;
        } else if(score == best_score)
            tied = 1;
    }
    char shown[ZIR_TEXT_MAX];
    size_t used = (size_t)snprintf(shown, sizeof(shown), "%s(", base);
    for(int i = 0; i < count && used < sizeof(shown); i++)
        used += (size_t)snprintf(shown + used, sizeof(shown) - used, "%s%s",
                                 i ? ", " : "", !strcmp(types[i], "integer") ?
                                 "integer literal" : !strcmp(types[i], "real") ?
                                 "float literal" : types[i]);
    if(used < sizeof(shown))
        snprintf(shown + used, sizeof(shown) - used, ")");
    free(types);
    free(names);
    if(best == NULL) {
        if(!c->inference_only)
            error(c, e->span, "no overload accepts these arguments", shown);
        return;
    }
    if(tied) {
        if(!c->inference_only)
            error(c, e->span, "overloads match these arguments equally well", shown);
        return;
    }
    if(alias[0])
        snprintf(e->name, sizeof(e->name), "%s.%s", alias, best->name);
    else
        copy_text(e->name, sizeof(e->name), best->name);
    append_overload_defaults(c, index, best, alias);
}

/* print shows an enum value by its member name: rewrite argument INDEX into
 * a call of the enum's generated name procedure, queued for creation.
 * Returns "string", or NULL when the argument is not a plain enum. */
static const char *
print_enum_by_name(Checker *c, int index)
{
    ZirModule *owner = NULL;
    const ZirType *enumeration = FindType(c->module, c->fn->exprs[index].type,
                                          (const ZirModule **)&owner);
    if(enumeration == NULL || !enumeration->is_enum || enumeration->is_enum_flags ||
       owner == NULL)
        return NULL;
    char function[ZIR_NAME_MAX], call_name[ZIR_NAME_MAX];
    if(snprintf(function, sizeof(function), "zi_enum_name_%s", enumeration->name) >=
       (int)sizeof(function))
        return NULL;
    const char *dot = strrchr(c->fn->exprs[index].type, '.');
    if(owner != c->module && dot != NULL)
        snprintf(call_name, sizeof(call_name), "%.*s.%s",
                 (int)(dot - c->fn->exprs[index].type), c->fn->exprs[index].type,
                 function);
    else
        copy_text(call_name, sizeof(call_name), function);
    if(c->inference_only)
        return "string";
    int known = 0;
    for(int f = 0; f < owner->function_count && !known; f++)
        known = !strcmp(owner->functions[f].name, function);
    for(int r = 0; r < c->enum_name_count && !known; r++)
        known = c->enum_names[r].owner == owner &&
                !strcmp(c->enum_names[r].function, function);
    if(!known) {
        if(c->enum_name_count == c->enum_name_capacity) {
            int capacity = c->enum_name_capacity ? c->enum_name_capacity * 2 : 8;
            EnumNameRequest *next = realloc(c->enum_names,
                                            (size_t)capacity * sizeof(*next));
            if(next == NULL) { c->failed = 1; return NULL; }
            c->enum_names = next;
            c->enum_name_capacity = capacity;
        }
        EnumNameRequest *request = &c->enum_names[c->enum_name_count++];
        request->owner = owner;
        request->args = request->statements = NULL;
        copy_text(request->type, sizeof(request->type), enumeration->name);
        copy_text(request->function, sizeof(request->function), function);
        request->span = enumeration->span;
    }
    ZirFunction *fn = c->fn;
    char name[ZIR_NAME_MAX];
    copy_text(name, sizeof(name), fn->exprs[index].name);
    ZirExpr *copy_slot = FunctionAddExpr(fn, fn->exprs[index].kind, name,
                                         fn->exprs[index].span);
    if(copy_slot == NULL) { c->failed = 1; return NULL; }
    ZirExpr saved = fn->exprs[index];
    int copy_index = (int)(copy_slot - fn->exprs);
    *copy_slot = saved;
    copy_slot->next_sibling = -1;
    copy_slot->argument_index = 0; /* the name procedure's one parameter */
    copy_slot->argument_name[0] = '\0';
    ZirExpr *call = &fn->exprs[index];
    memset(call, 0, sizeof(*call));
    call->kind = ZIR_EXPR_CALL;
    call->text = saved.text;
    copy_text(call->name, sizeof(call->name), call_name);
    copy_text(call->type, sizeof(call->type), "string");
    call->first_child = copy_index;
    call->left = call->right = call->third = -1;
    call->next_sibling = saved.next_sibling;
    call->argument_index = saved.argument_index;
    call->span = saved.span;
    c->conversions_applied = 1;
    return call->type;
}

/* print shows a record as {x = 1, y = 2}, nesting records and showing up
 * to 16 elements of a fixed array. print_expands says whether TYPE takes
 * that form. */
static int
print_expands(Checker *c, const char *type)
{
    if(ArrayElementType(type, NULL, 0, NULL))
        return 1;
    const ZirType *record = FindType(c->module, type, NULL);
    return record != NULL && !record->is_enum && !record->is_procedure_type &&
           !record->is_map && !record->is_extern && !record->is_record_template &&
           !record->is_results;
}

enum { PRINT_EXPANSION_MAX = ZIR_TEXT_MAX * 8, PRINT_STATEMENT_PLACEHOLDERS = 24 };

/* The statements of a generated print procedure, one per line. Each print
 * keeps under PRINT_PIECES_MAX pieces; a long expansion continues in the
 * next statement. */
typedef struct PrintExpansion {
    char format[ZIR_TEXT_MAX];
    char args[ZIR_TEXT_MAX];
    int placeholders;
    char *statements;
    size_t used;
    int failed;
} PrintExpansion;

static void
print_append(char *buffer, size_t capacity, const char *text, int *failed)
{
    size_t used = strlen(buffer), length = strlen(text);
    if(used + length >= capacity) {
        *failed = 1;
        return;
    }
    memcpy(buffer + used, text, length + 1);
}

static void
print_flush(PrintExpansion *out)
{
    if(!out->format[0] && !out->args[0])
        return;
    char statement[ZIR_TEXT_MAX * 2 + 16];
    int length = snprintf(statement, sizeof(statement), "print(\"%s\"%s);\n",
                          out->format, out->args);
    if(length < 0 || (size_t)length >= sizeof(statement) ||
       out->used + (size_t)length >= PRINT_EXPANSION_MAX) {
        out->failed = 1;
        return;
    }
    memcpy(out->statements + out->used, statement, (size_t)length + 1);
    out->used += (size_t)length;
    out->format[0] = out->args[0] = '\0';
    out->placeholders = 0;
}

static void
print_literal(PrintExpansion *out, const char *text)
{
    print_append(out->format, sizeof(out->format), text, &out->failed);
}

static void
print_placeholder(PrintExpansion *out, const char *before, const char *access,
                  const char *after)
{
    if(out->placeholders >= PRINT_STATEMENT_PLACEHOLDERS)
        print_flush(out);
    print_literal(out, before);
    print_literal(out, "%");
    print_literal(out, after);
    print_append(out->args, sizeof(out->args), ", ", &out->failed);
    print_append(out->args, sizeof(out->args), access, &out->failed);
    out->placeholders++;
}

/* Append VALUE (read through ACCESS) of TYPE, named in MODULE's scope. */
static void
print_expand(Checker *c, const ZirModule *module, const char *type,
             const char *access, PrintExpansion *out, int depth)
{
    char element[ZIR_NAME_MAX], nested[ZIR_TEXT_MAX];
    int capacity = 0;
    const ZirModule *owner = NULL;
    if(out->failed)
        return;
    if(depth > 8) {
        print_literal(out, "{...}");
        return;
    }
    if(!strcmp(type, "string")) {
        print_placeholder(out, "\\\"", access, "\\\"");
        return;
    }
    if(*ScalarType(type)) {
        print_placeholder(out, "", access, "");
        return;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        print_literal(out, "[");
        for(int i = 0; i < capacity && i < 16; i++) {
            if(i) print_literal(out, ", ");
            snprintf(nested, sizeof(nested), "%s[%d]", access, i);
            print_expand(c, module, element, nested, out, depth + 1);
        }
        print_literal(out, capacity > 16 ? ", ...]" : "]");
        return;
    }
    if(SliceElementType(type, NULL, 0) || VecElementType(module, type, NULL, 0)) {
        snprintf(nested, sizeof(nested), "%s.count", access);
        print_placeholder(out, "[count = ", nested, "]");
        return;
    }
    if(*type == '*') {
        print_literal(out, "(pointer)");
        return;
    }
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL) {
        print_literal(out, "(?)");
        return;
    }
    if(record->is_enum && record->is_enum_flags) {
        snprintf(nested, sizeof(nested), "cast(%s) %s",
                 record->enum_backing[0] ? record->enum_backing : "u64", access);
        print_placeholder(out, "", nested, "");
        return;
    }
    if(record->is_enum) {
        print_placeholder(out, "", access, "");
        return;
    }
    if(record->is_procedure_type || record->is_extern || record->is_map) {
        print_literal(out, "(opaque)");
        return;
    }
    if(record->is_union) {
        print_literal(out, "(union)");
        return;
    }
    print_literal(out, "{");
    size_t offset = 0;
    ZirTypeField field;
    int first = 1;
    while(TypeNextField(record, &offset, &field) == 1) {
        char label[ZIR_NAME_MAX + 8];
        snprintf(label, sizeof(label), "%s%s = ", first ? "" : ", ", field.name);
        print_literal(out, label);
        snprintf(nested, sizeof(nested), "%s.%s", access, field.name);
        print_expand(c, owner != NULL ? owner : module, field.type, nested, out,
                     depth + 1);
        first = 0;
    }
    print_literal(out, "}");
}

/* A print call with a record argument becomes a call of a generated
 * procedure taking the same arguments, so each is evaluated once, in order.
 * Its body prints the format with every record expanded into its fields.
 * A value holding a Vec is passed by pointer rather than moved. Returns 1
 * after rewriting call INDEX. */
static int
print_through_procedure(Checker *c, int index)
{
    ZirFunction *fn = c->fn;
    int format = fn->exprs[index].first_child;
    const char *literal = fn->exprs[format].text;
    if(c->inference_only || literal == NULL || literal[0] != '"')
        return 0;
    PrintExpansion *out = calloc(1, sizeof(*out));
    char *args = calloc(1, ZIR_TEXT_MAX * 2);
    char *identity = calloc(1, ZIR_TEXT_MAX * 4);
    if(out != NULL) out->statements = calloc(1, PRINT_EXPANSION_MAX);
    if(out == NULL || args == NULL || identity == NULL || out->statements == NULL) {
        c->failed = 1;
        goto failed;
    }
    /* Walk the raw literal so escapes and %% stay as written. */
    int argument = fn->exprs[format].next_sibling, number = 0;
    const char *cursor = literal + 1;
    while(*cursor && !(*cursor == '"' && cursor[1] == '\0')) {
        if(*cursor == '\\' && cursor[1]) {
            char escape[3] = { cursor[0], cursor[1], '\0' };
            print_literal(out, escape);
            cursor += 2;
            continue;
        }
        if(*cursor == '%' && cursor[1] == '%') {
            print_literal(out, "%%");
            cursor += 2;
            continue;
        }
        if(*cursor != '%') {
            char one[2] = { *cursor, '\0' };
            print_literal(out, one);
            cursor++;
            continue;
        }
        cursor++;
        if(argument < 0) goto failed;
        const char *type = fn->exprs[argument].type;
        char name[32], parameter[ZIR_TEXT_MAX];
        snprintf(name, sizeof(name), "value_%d", ++number);
        int by_pointer = print_expands(c, type) && contains_vec(c->module, type, 0);
        if(by_pointer && !assignable(c, argument)) {
            error(c, fn->exprs[argument].span,
                  "print a value holding a Vec through a variable", type);
            goto failed;
        }
        snprintf(parameter, sizeof(parameter), "%s%s: %s%s", number > 1 ? ", " : "",
                 name, by_pointer ? "*" : "", type);
        print_append(args, ZIR_TEXT_MAX * 2, parameter, &out->failed);
        print_append(identity, ZIR_TEXT_MAX * 4, parameter, &out->failed);
        if(print_expands(c, type))
            print_expand(c, c->module, type, name, out, 0);
        else
            print_placeholder(out, "", name, "");
        argument = fn->exprs[argument].next_sibling;
    }
    print_flush(out);
    if(out->failed || argument >= 0)
        goto failed;
    print_append(identity, ZIR_TEXT_MAX * 4, literal, &out->failed);
    print_append(identity, ZIR_TEXT_MAX * 4, c->module->name, &out->failed);
    uint64_t hash = UINT64_C(14695981039346656037);
    for(const unsigned char *p = (const unsigned char *)identity; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    char function[ZIR_NAME_MAX];
    snprintf(function, sizeof(function), "zi_print_%016llx", (unsigned long long)hash);
    int known = 0;
    for(int f = 0; f < c->module->function_count && !known; f++)
        known = !strcmp(c->module->functions[f].name, function);
    for(int r = 0; r < c->enum_name_count && !known; r++)
        known = c->enum_names[r].owner == c->module &&
                !strcmp(c->enum_names[r].function, function);
    if(!known) {
        if(c->enum_name_count == c->enum_name_capacity) {
            int capacity = c->enum_name_capacity ? c->enum_name_capacity * 2 : 8;
            EnumNameRequest *next = realloc(c->enum_names,
                                            (size_t)capacity * sizeof(*next));
            if(next == NULL) { c->failed = 1; goto failed; }
            c->enum_names = next;
            c->enum_name_capacity = capacity;
        }
        EnumNameRequest *request = &c->enum_names[c->enum_name_count++];
        request->owner = c->module;
        copy_text(request->type, sizeof(request->type), "print");
        copy_text(request->function, sizeof(request->function), function);
        request->span = fn->exprs[index].span;
        request->args = args;
        request->statements = out->statements;
        args = NULL;
        out->statements = NULL;
    }
    /* Pass Vec holders by address, then drop the format argument. */
    argument = fn->exprs[format].next_sibling;
    for(int position = 0; argument >= 0; position++) {
        int next = fn->exprs[argument].next_sibling;
        const char *type = fn->exprs[argument].type;
        if(print_expands(c, type) && contains_vec(c->module, type, 0)) {
            ZirExpr *copy_slot = FunctionAddExpr(fn, fn->exprs[argument].kind,
                                                 fn->exprs[argument].name,
                                                 fn->exprs[argument].span);
            if(copy_slot == NULL) { c->failed = 1; goto failed; }
            fn = c->fn;
            int copy_index = (int)(copy_slot - fn->exprs);
            ZirExpr saved = fn->exprs[argument];
            *copy_slot = saved;
            copy_slot->next_sibling = -1;
            copy_slot->argument_index = -1;
            ZirExpr *address = &fn->exprs[argument];
            memset(address, 0, sizeof(*address));
            address->kind = ZIR_EXPR_UNARY;
            address->text = saved.text;
            copy_text(address->op, sizeof(address->op), "&");
            snprintf(address->type, sizeof(address->type), "*%s", saved.type);
            address->left = address->third = address->first_child = -1;
            address->right = copy_index;
            address->next_sibling = next;
            address->span = saved.span;
        }
        fn->exprs[argument].argument_index = position;
        argument = next;
    }
    ZirExpr *call = &fn->exprs[index];
    call->first_child = fn->exprs[format].next_sibling;
    copy_text(call->name, sizeof(call->name), function);
    copy_text(call->type, sizeof(call->type), "void");
    c->conversions_applied = 1;
    free(args);
    free(identity);
    free(out->statements);
    free(out);
    return 1;
failed:
    free(args);
    free(identity);
    if(out != NULL) free(out->statements);
    free(out);
    return 0;
}

/* Whether TYPE is a record an operator procedure may take. */
static int
operator_operand(Checker *c, const char *type)
{
    const ZirType *record = *type ? FindType(c->module, type, NULL) : NULL;
    return record != NULL && !record->is_enum && !record->is_map &&
           !record->is_procedure_type && !record->is_record_template &&
           !record->is_results;
}

/* The call name for operator procedure NAME as visible here: NAME itself,
 * or ALIAS.NAME through a named import. Returns 0 when none is visible. */
static int
visible_operator(Checker *c, const char *name, char *callee, size_t size)
{
    for(int f = 0; f < c->module->function_count; f++) {
        const ZirFunction *fn = &c->module->functions[f];
        if(!strcmp(fn->name, name) || !strcmp(fn->overload_name, name)) {
            copy_text(callee, size, name);
            return 1;
        }
    }
    for(int i = 0; i < c->module->import_count; i++) {
        const ZirImport *import = &c->module->imports[i];
        const ZirModule *other = import->resolved_module;
        if(other == NULL || !in_lookup_file(c->module, import->is_file_private, import->span) ||
           (import->kind != ZIR_IMPORT_OPEN && import->kind != ZIR_IMPORT_MODULE))
            continue;
        for(int f = 0; f < other->function_count; f++) {
            const ZirFunction *fn = &other->functions[f];
            if(!fn->is_public || (strcmp(fn->name, name) && strcmp(fn->overload_name, name)))
                continue;
            if(import->kind == ZIR_IMPORT_MODULE && !import->is_using)
                snprintf(callee, size, "%s.%s", import->name, name);
            else
                copy_text(callee, size, name);
            return 1;
        }
    }
    return 0;
}

/* Jai's `operator + :: (a: V, b: V) -> V` gives records their operators:
 * binary expression INDEX with a record operand becomes a call of the
 * visible operator procedure. `a != b` falls back to `!(a == b)` when only
 * operator == is declared. Returns 1 after rewriting. */
static int
operator_call(Checker *c, int index, const char *left, const char *right)
{
    ZirExpr *e = &c->fn->exprs[index];
    const char *name = OperatorProcedureName(e->op);
    char callee[ZIR_NAME_MAX];
    int negate = 0;
    if(name == NULL || (!operator_operand(c, left) && !operator_operand(c, right)))
        return 0;
    if(!visible_operator(c, name, callee, sizeof(callee))) {
        if(strcmp(e->op, "!=") ||
           !visible_operator(c, OperatorProcedureName("=="), callee, sizeof(callee)))
            return 0;
        negate = 1;
    }
    int call_index = index;
    if(negate) {
        /* Keep INDEX as the ! and move the comparison into a new call. */
        ZirExpr *added = FunctionAddExpr(c->fn, ZIR_EXPR_CALL, callee, e->span);
        if(added == NULL) { c->failed = 1; return 0; }
        call_index = (int)(added - c->fn->exprs);
        e = &c->fn->exprs[index];
    }
    int l = e->left, r = e->right;
    ZirExpr *call = &c->fn->exprs[call_index];
    call->kind = ZIR_EXPR_CALL;
    copy_text(call->name, sizeof(call->name), callee);
    call->op[0] = '\0';
    call->type[0] = '\0';
    call->text = e->text;
    call->span = e->span;
    call->first_child = l;
    call->left = call->right = call->third = -1;
    call->next_sibling = -1;
    call->argument_index = -1;
    c->fn->exprs[l].next_sibling = r;
    c->fn->exprs[l].argument_index = 0;
    c->fn->exprs[l].argument_name[0] = '\0';
    c->fn->exprs[r].next_sibling = -1;
    c->fn->exprs[r].argument_index = 1;
    c->fn->exprs[r].argument_name[0] = '\0';
    if(negate) {
        e = &c->fn->exprs[index];
        e->kind = ZIR_EXPR_UNARY;
        copy_text(e->op, sizeof(e->op), "!");
        e->left = -1;
        e->right = call_index;
        e->type[0] = '\0';
    }
    c->conversions_applied = 1;
    return 1;
}

/* A copy of expression tree INDEX appended to FN, or -1 for none; sets
 * c->failed when memory runs out. */
static int
copy_expression_tree(Checker *c, int index)
{
    if(index < 0 || c->failed)
        return -1;
    ZirExpr *added = FunctionAddExpr(c->fn, c->fn->exprs[index].kind,
                                     c->fn->exprs[index].name,
                                     c->fn->exprs[index].span);
    if(added == NULL) {
        c->failed = 1;
        return -1;
    }
    int copy = (int)(added - c->fn->exprs);
    c->fn->exprs[copy] = c->fn->exprs[index];
    int left = copy_expression_tree(c, c->fn->exprs[index].left);
    int right = copy_expression_tree(c, c->fn->exprs[index].right);
    int third = copy_expression_tree(c, c->fn->exprs[index].third);
    int first = -1, last = -1;
    for(int child = c->fn->exprs[index].first_child; child >= 0;
        child = c->fn->exprs[child].next_sibling) {
        int copied = copy_expression_tree(c, child);
        if(copied < 0) break;
        if(last < 0) first = copied;
        else c->fn->exprs[last].next_sibling = copied;
        last = copied;
    }
    ZirExpr *node = &c->fn->exprs[copy];
    node->left = left;
    node->right = right;
    node->third = third;
    node->first_child = first;
    node->next_sibling = -1;
    return copy;
}

/* `a += b` on a record becomes `a = a + b` through its operator procedure;
 * without one it is an error. Returns 0 after reporting. */
int
compound_operator_assignment(Checker *c, ZirStmt *st)
{
    char op[4];
    size_t length = strlen(st->assignment_op);
    if(st->lhs_root < 0 || length < 2 || st->assignment_op[length - 1] != '=')
        return 1;
    memcpy(op, st->assignment_op, length - 1);
    op[length - 1] = '\0';
    int saved_inference = c->inference_only;
    c->inference_only = 1;
    char lhs[ZIR_NAME_MAX];
    copy_text(lhs, sizeof(lhs), expression_type(c, st->lhs_root));
    c->inference_only = saved_inference;
    if(!operator_operand(c, lhs))
        return 1;
    const char *name = OperatorProcedureName(op);
    char callee[ZIR_NAME_MAX];
    if(name == NULL || !visible_operator(c, name, callee, sizeof(callee))) {
        error(c, st->span, "record compound assignment needs an operator procedure",
              st->assignment_op);
        return 0;
    }
    int destination = copy_expression_tree(c, st->lhs_root);
    ZirExpr *binary = destination < 0 ? NULL :
        FunctionAddExpr(c->fn, ZIR_EXPR_BINARY, "", st->span);
    if(binary == NULL) {
        c->failed = 1;
        return 0;
    }
    copy_text(binary->op, sizeof(binary->op), op);
    binary->text = st->text;
    binary->left = destination;
    binary->right = st->expr_root;
    binary->third = binary->first_child = binary->next_sibling = -1;
    binary->argument_index = -1;
    st->expr_root = (int)(binary - c->fn->exprs);
    copy_text(st->assignment_op, sizeof(st->assignment_op), "=");
    return 1;
}

/* Edit distance between two names, or LIMIT + 1 once it exceeds LIMIT. */
static int
name_distance(const char *a, const char *b, int limit)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    if(la - lb > limit || lb - la > limit || la >= ZIR_NAME_MAX || lb >= ZIR_NAME_MAX)
        return limit + 1;
    int previous[ZIR_NAME_MAX], current[ZIR_NAME_MAX];
    for(int j = 0; j <= lb; j++) previous[j] = j;
    for(int i = 1; i <= la; i++) {
        int best = current[0] = i;
        for(int j = 1; j <= lb; j++) {
            int substitute = previous[j - 1] + (a[i - 1] != b[j - 1]);
            int remove = previous[j] + 1, insert = current[j - 1] + 1;
            current[j] = substitute < remove ? substitute : remove;
            if(insert < current[j]) current[j] = insert;
            if(current[j] < best) best = current[j];
        }
        if(best > limit) return limit + 1;
        memcpy(previous, current, (size_t)(lb + 1) * sizeof(int));
    }
    return previous[lb];
}

static void
consider_name(const char *name, const char *candidate, int *best,
              const char **choice)
{
    if(!candidate[0] || !strcmp(name, candidate)) return;
    int distance = name_distance(name, candidate, *best);
    if(distance < *best) {
        *best = distance;
        *choice = candidate;
    }
}

/* "NAME (did you mean CLOSE?)" when a visible name is a likely typo. */
static const char *
unresolved_detail(Checker *c, const char *name, int procedures,
                  char *out, size_t size)
{
    int length = (int)strlen(name);
    int best = length <= 3 ? 1 : length <= 8 ? 2 : 3;
    const char *choice = NULL;
    best++;
    if(!procedures)
        for(int i = 0; i < c->count; i++)
            consider_name(name, c->bindings[i].name, &best, &choice);
    const ZirModule *module = c->module;
    for(int i = 0; !procedures && i < module->global_count; i++)
        consider_name(name, module->globals[i].name, &best, &choice);
    for(int i = 0; !procedures && i < module->define_count; i++)
        consider_name(name, module->defines[i].name, &best, &choice);
    for(int i = 0; i < module->function_count; i++)
        if(!module->functions[i].is_specialization &&
           !module->functions[i].is_global_initializer)
            consider_name(name, module->functions[i].name, &best, &choice);
    for(int i = 0; procedures && i < module->type_count; i++)
        consider_name(name, module->types[i].name, &best, &choice);
    if(choice == NULL)
        return name;
    snprintf(out, size, "%s (did you mean %s?)", name, choice);
    return out;
}

static const char *
expression_type_with_buffers(Checker *c, int index, ExpressionTypeBuffers *buffers)
{
    ZirExpr *e;
    const char *type = "", *left = "", *right = "";
    char member_type[ZIR_NAME_MAX] = "";
    if(index < 0 || index >= c->fn->expr_count) return "";
    e = &c->fn->exprs[index];
    if(e->is_this && e->kind != ZIR_EXPR_IDENT && e->kind != ZIR_EXPR_CALL) {
        error(c, e->span, "invalid #this expression", e->name);
        return "";
    }
    if(e->kind == ZIR_EXPR_CALL && e->type[0]) {
        const char *unqualified = strrchr(e->name, '.');
        unqualified = unqualified == NULL ? e->name : unqualified + 1;
        for(int request = 0; request < c->specialization_count; request++)
            if(!strcmp(unqualified, c->specializations[request].name))
                return e->type;
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *middle = &c->fn->exprs[e->left];
        if(middle->left >= 0 &&
           c->fn->exprs[middle->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[middle->left].name;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, middle->name);
            if(!*lookup_lexical(c, alias) && length >= 0 &&
               (size_t)length < sizeof(qualified)) {
                const ZirType *enumeration = FindType(c->module, qualified,
                                                      NULL);
                if(enumeration != NULL && enumeration->is_enum) {
                    if(lower_enum_reference(c, e, enumeration, e->name)) {
                        copy_text(e->type, sizeof(e->type), qualified);
                        return e->type;
                    }
                    return "";
                }
            }
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const char *alias = c->fn->exprs[e->left].name;
        for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *import = &c->module->imports[i];
            if(import->kind != ZIR_IMPORT_MODULE ||
               !in_lookup_file(c->module, import->is_file_private,
                               import->span) ||
               strcmp(import->name, alias) != 0)
                continue;
            char qualified[ZIR_NAME_MAX];
            int length = snprintf(qualified, sizeof(qualified), "%s.%s",
                                  alias, e->name);
            if(length < 0 || (size_t)length >= sizeof(qualified)) {
                error(c, e->span, "qualified name is too long", alias);
                return "";
            }
            e->kind = ZIR_EXPR_IDENT;
            e->left = -1;
            copy_text(e->name, sizeof(e->name), qualified);
            break;
        }
    }
    if(e->kind == ZIR_EXPR_MEMBER && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
        const ZirType *enumeration = FindType(c->module,
            c->fn->exprs[e->left].name, NULL);
        if(enumeration != NULL && enumeration->is_enum) {
            if(lower_enum_reference(c, e, enumeration, e->name))
                return e->type;
            return "";
        }
    }
    if(e->kind == ZIR_EXPR_IDENT && e->name[0] == '.') {
        const ZirType *enumeration = FindType(c->module, c->expected_type, NULL);
        if(enumeration == NULL || !enumeration->is_enum) {
            error(c, e->span, "inferred enum member needs an enum type", e->name);
            return "";
        }
        if(lower_enum_reference(c, e, enumeration, e->name + 1))
            return e->type;
        return "";
    }
    if(e->kind == ZIR_EXPR_CONDITIONAL && !strcmp(e->op, "#ifx")) {
        error(c, e->span, "unlowered #ifx expression", e->text);
        return "";
    }
    if(e->kind == ZIR_EXPR_SIZE_OF) {
        size_t size, alignment;
        char resolved[ZIR_NAME_MAX];
        const char *sized_type = e->name;
        int from_value = TypeOfOperand(e->name, buffers->operand, sizeof(buffers->operand));
        if(!from_value && !JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        if(from_value) {
            memset(&buffers->probe, 0, sizeof(buffers->probe));
            ZirFunction *saved_fn = c->fn;
            ZirStmt *saved_stmt = c->current_stmt;
            int root = ParseExpr(&buffers->probe, c->module, buffers->operand, e->span);
            c->fn = &buffers->probe;
            c->current_stmt = NULL;
            const char *inferred = expression_type(c, root);
            copy_text(resolved, sizeof(resolved),
                      !strcmp(inferred, "integer") ? "s64" :
                      !strcmp(inferred, "real") ? "float64" : inferred);
            c->fn = saved_fn;
            c->current_stmt = saved_stmt;
            free(buffers->probe.exprs);
            sized_type = resolved;
        }
        if(!TypeLayout(c->module, sized_type, &size, &alignment)) {
            error(c, e->span, "size_of requires a known sized type", e->name);
            return "";
        }
        snprintf(buffers->replacement, sizeof(buffers->replacement), "size_of(%s)", sized_type);
        if(!rewrite_checked_text(c, e, buffers->replacement)) {
            error(c, e->span, "cannot lower size_of expression", e->text);
            return "";
        }
        if(!c->fn->from_ir)
            e->text = KeepText(buffers->replacement);
        if(sized_type != e->name)
            copy_text(e->name, sizeof(e->name), sized_type);
        e->left = e->right = e->third = -1;
        copy_text(e->type, sizeof(e->type), "integer");
        return e->type;
    }
    if(e->is_function_value)
        return e->type;
    if(e->kind == ZIR_EXPR_CALL && e->name[0] == '\0' && e->left >= 0 &&
       c->fn->exprs[e->left].kind == ZIR_EXPR_MEMBER) {
        const ZirExpr *member = &c->fn->exprs[e->left];
        if(member->left >= 0 &&
           c->fn->exprs[member->left].kind == ZIR_EXPR_IDENT) {
            const char *alias = c->fn->exprs[member->left].name;
            for(int i = 0; i < c->module->import_count; i++) {
                const ZirImport *import = &c->module->imports[i];
                if(import->kind != ZIR_IMPORT_MODULE ||
                   !in_lookup_file(c->module, import->is_file_private,
                                   import->span) ||
                   strcmp(import->name, alias) != 0)
                    continue;
                int length = snprintf(e->name, sizeof(e->name),
                                      "%s.%s", alias, member->name);
                if(length < 0 || (size_t)length >= sizeof(e->name))
                    error(c, e->span, "qualified call name is too long", alias);
                else
                    e->left = -1;
                break;
            }
        }
    }
    /* Checking an operand can append expressions, such as a widening cast,
     * and move the array: the returned type points into it, and so does e. */
    if(e->left >= 0 && !(e->kind == ZIR_EXPR_CALL && e->name[0])) {
        copy_text(buffers->left, sizeof(buffers->left),
                  expression_type(c, e->left));
        left = buffers->left;
        e = &c->fn->exprs[index];
    }
    if(e->right >= 0) {
        char saved_expected[ZIR_NAME_MAX];
        copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
        if(e->kind == ZIR_EXPR_BINARY) {
            const ZirType *enumeration = FindType(c->module, left, NULL);
            if(enumeration != NULL && enumeration->is_enum)
                copy_text(c->expected_type, sizeof(c->expected_type), left);
        }
        copy_text(buffers->right, sizeof(buffers->right),
                  expression_type(c, e->right));
        right = buffers->right;
        e = &c->fn->exprs[index];
        copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
    }
    switch(e->kind) {
    case ZIR_EXPR_COMPOUND: {
        if(!e->name[0]) {
            if(!c->expected_type[0]) {
                error(c, e->span,
                      "inferred record literal needs a record type", e->text);
                break;
            }
            copy_text(e->name, sizeof(e->name), c->expected_type);
        }
        if(SliceElementType(e->name, NULL, 0)) {
            error(c, e->span, "slice literals require a backing range", e->name);
            break;
        }
        if(e->name[0] == '[') {
            char element[ZIR_NAME_MAX];
            int capacity = 0;
            const char *problem = local_storage_error(c->module, e->name);
            if(problem != NULL) {
                error(c, e->span, problem, e->name);
                break;
            }
            normalize_array(c->module, e->name, sizeof(e->name));
            ArrayElementType(e->name, element, sizeof(element), &capacity);
            if(capacity < 0)
                error(c, e->span, "array literals require a resolved capacity", e->name);
            int count = 0;
            for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
                ZirExpr *entry = &c->fn->exprs[child];
                if(!strcmp(entry->op, "="))
                    error(c, entry->span, "array literals require positional elements", entry->name);
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                copy_text(c->expected_type, sizeof(c->expected_type), element);
                const char *value_type = expression_type(c, entry->right);
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(!compatible_checked(c, element, value_type))
                    error(c, entry->span, "array initializer element type mismatch", element);
                copy_text(entry->type, sizeof(entry->type), element);
                count++;
            }
            if(capacity >= 0 && count > capacity)
                error(c, e->span, "too many array initializer elements", e->name);
            type = e->name;
            break;
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, e->name, &record_owner);
        int ordinal = 0;
        int mode = -1;
        if(record == NULL || record->is_enum || record->is_procedure_type ||
           record->is_record_template || record->foreign_target[0] || record->is_map) {
            error(c, e->span, "initializer requires a declared record type", e->name);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            ZirExpr *entry = &c->fn->exprs[child];
            int named = !strcmp(entry->op, "=");
            if(mode >= 0 && mode != named)
                error(c, entry->span, "cannot mix named and positional record fields", e->name);
            mode = named;
            size_t offset = 0;
            ZirTypeField field;
            int position = 0;
            int found = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                if(named ? !strcmp(field.name, entry->name) : position == ordinal) {
                    found = 1;
                    break;
                }
                position++;
            }
            if(found) {
                normalize_array(record_owner, field.type, sizeof(field.type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             e->name, field.type,
                                             sizeof(field.type)))
                    error(c, entry->span,
                          "imported record field type is shadowed",
                          field.name);
                ZirExpr *initializer = &c->fn->exprs[entry->right];
                if(initializer->kind == ZIR_EXPR_COMPOUND)
                    normalize_array(record_owner, initializer->name, sizeof(initializer->name));
            }
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(found)
                copy_text(c->expected_type, sizeof(c->expected_type), field.type);
            const char *value_type = expression_type(c, entry->right);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(!found) {
                error(c, entry->span, named ? "unknown initializer field" : "too many positional record fields", entry->name);
                ordinal++;
                continue;
            }
            for(int previous = e->first_child; previous != child; previous = c->fn->exprs[previous].next_sibling) {
                if(!strcmp(c->fn->exprs[previous].name, field.name))
                    error(c, entry->span, "duplicate initializer field", field.name);
            }
            copy_text(entry->name, sizeof(entry->name), field.name);
            copy_text(entry->type, sizeof(entry->type), field.type);
            if(!compatible_checked(c, field.type, value_type))
                error(c, entry->span, "initializer field type mismatch", field.name);
            if(contains_vec(c->module, field.type, 0) &&
               !owned_initializer_shape(c, entry->right))
                error(c, entry->span,
                      "owned record field moves a binding or takes a call result",
                      field.name);
            ordinal++;
        }
        type = e->name;
        break;
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER: {
        char record_name[ZIR_NAME_MAX] = "";
        if(e->kind == ZIR_EXPR_MEMBER && left[0] == '*') {
            e->kind = ZIR_EXPR_POINTER_MEMBER;
            copy_text(e->op, sizeof(e->op), "->");
        }
        if(e->kind == ZIR_EXPR_POINTER_MEMBER) {
            const char *base = skip_ws(left);
            if(!strncmp(base, "const ", 6))
                base = skip_ws(base + 6);
            if(*base == '*') {
                base = skip_ws(base + 1);
                if(!*base || strlen(base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                copy_text(record_name, sizeof(record_name), base);
            } else {
                const char *star = strchr(base, '*');
                if(star == NULL || *skip_ws(star + 1) ||
                   star == base || (size_t)(star - base) >= sizeof(record_name)) {
                    error(c, e->span, "pointer member requires a record pointer", left);
                    break;
                }
                size_t length = (size_t)(star - base);
                while(length > 0 && isspace((unsigned char)base[length - 1]))
                    length--;
                memcpy(record_name, base, length);
                record_name[length] = '\0';
            }
        } else {
            copy_text(record_name, sizeof(record_name), left);
        }
        const ZirModule *record_owner = NULL;
        const ZirType *record = FindType(c->module, record_name, &record_owner);
        if(e->kind == ZIR_EXPR_MEMBER && !strcmp(e->name, "length") &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            error(c, e->span, "length is not Jai syntax; use count", e->name);
            break;
        }
        if(e->kind == ZIR_EXPR_MEMBER && record == NULL &&
           (!strcmp(left, "string") || SliceElementType(left, NULL, 0) ||
            ArrayElementType(left, NULL, 0, NULL))) {
            if(!strcmp(e->name, "count")) {
                type = "s64";
                break;
            }
            char element[ZIR_NAME_MAX];
            if(!strcmp(e->name, "data") &&
               ArrayElementType(left, element, sizeof(element), NULL)) {
                snprintf(member_type, sizeof(member_type), "*%s", element);
                type = member_type;
                break;
            }
        }
        if(record != NULL && record->is_map) {
            error(c, e->span, "Map storage is private; use map operations", e->name);
            break;
        }
        if(VecElementType(c->module, record_name, NULL, 0) &&
           strcmp(e->name, "count") && strcmp(e->name, "capacity")) {
            error(c, e->span, "Vec storage is private", e->name);
            break;
        }
        if(record != NULL && !record->is_enum) {
            if(strchr(e->name, '.') != NULL) {
                if(!RecordFieldPathType(record_owner, record, e->name,
                                        member_type, sizeof(member_type)))
                    error(c, e->span, "invalid promoted record field path",
                          e->name);
            } else {
                char path[ZIR_NAME_MAX];
                int found = ResolveRecordField(record_owner, record, e->name,
                                               path, sizeof(path), member_type,
                                               sizeof(member_type));
                if(found < 0)
                    error(c, e->span, "ambiguous using record field", e->name);
                else if(found > 0)
                    copy_text(e->name, sizeof(e->name), path);
            }
            if(*member_type) {
                normalize_array(record_owner, member_type, sizeof(member_type));
                if(!record_field_type_at_use(c->module, record_owner,
                                             record_name, member_type,
                                             sizeof(member_type)))
                    error(c, e->span,
                          "imported record field type is shadowed",
                          e->name);
            }
        }
        if(!*member_type)
            error(c, e->span, "unknown record field", e->name);
        type = member_type;
        check_moved_path_use(c, index);
        break;
    }
    case ZIR_EXPR_SLICE: {
        char element[ZIR_NAME_MAX];
        int array = ArrayElementType(left, element, sizeof(element), NULL);
        int string = !strcmp(left, "string");
        if(!string && !array && !SliceElementType(left, element, sizeof(element))) {
            error(c, e->span, "slice source requires a string, array, or slice", e->text);
            break;
        }
        if(array && !assignable(c, e->left))
            error(c, e->span, "slice source requires persistent array storage", e->text);
        if(!string && ((!strcmp(element, "char") || !strcmp(element, "const char")) || element[0] == '['))
            error(c, e->span, "unsupported slice element type", element);
        if(e->right >= 0 && !integer_type(right))
            error(c, e->span, "slice lower bound requires an integer", e->text);
        if(e->third >= 0 && !integer_type(expression_type(c, e->third)))
            error(c, e->span, "slice upper bound requires an integer", e->text);
        if(string) type = "string";
        else {
            snprintf(member_type, sizeof(member_type), "[]%s", element);
            type = member_type;
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        char element[ZIR_NAME_MAX];
        const char *open = e->text != NULL ? strchr(e->text, '[') : NULL;
        /* Jai writes ranges only in for loops. */
        if(open != NULL && strstr(open, "..") != NULL) {
            error(c, e->span, "slices are written value[start:end]", e->text);
            right = "s64";
        }
        /* Native pointer indexing follows the same element type as a
         * borrowed array. Portable bundles reject reachable raw pointers. */
        if(!strcmp(left, "string")) {
            if(!integer_type(right))
                error(c, e->span, "string index requires an integer operand", e->text);
            copy_text(element, sizeof(element), "u8");
        } else if(*left == '*' && *skip_ws(left + 1) &&
                  strcmp(skip_ws(left + 1), "void")) {
            copy_text(element, sizeof(element), skip_ws(left + 1));
            if(!integer_type(right))
                error(c, e->span, "pointer index requires an integer operand", e->text);
        } else if(!ArrayElementType(left, element, sizeof(element), NULL) &&
                  !SliceElementType(left, element, sizeof(element)) &&
                  !VecElementType(c->module, left, element, sizeof(element))) {
            error(c, e->span, "index requires an array, slice, pointer, or string", e->text);
            copy_text(element, sizeof(element), "s32");
        } else if(!integer_type(right)) {
            error(c, e->span, "array index requires an integer operand", e->text);
        }
        copy_text(member_type, sizeof(member_type), element);
        type = member_type;
        break;
    }
    case ZIR_EXPR_INT: {
        const ZirType *enumeration = FindType(c->module, e->type, NULL);
        type = enumeration != NULL && enumeration->is_enum ? e->type : "integer";
        break;
    }
    case ZIR_EXPR_FLOAT: type = "real"; break;
    case ZIR_EXPR_COMPILE_TIME: type = "bool"; break;
    case ZIR_EXPR_STRING: type = "string"; break;
    case ZIR_EXPR_IDENT:
        if(e->is_this) {
            error(c, e->span, "#this needs a procedure type or call", e->name);
            break;
        }
        if(!strcmp(e->name, "true") || !strcmp(e->name, "false")) type = "bool";
        else if(!strcmp(e->name, "null")) type = "null";
        else {
            const ZirType *enumeration = NULL;
            char source_member[ZIR_NAME_MAX];
            int enum_member = !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) == NULL ?
                resolve_using_enum(c, e->name, &enumeration, source_member,
                                   sizeof(source_member)) : 0;
            if(enum_member < 0) {
                error(c, e->span, "ambiguous using enum member", e->name);
                break;
            }
            if(enum_member == 1) {
                if(!lower_enum_reference(c, e, enumeration, source_member))
                    break;
                type = "integer";
                break;
            }
            type = lookup(c, e->name);
            e->is_global_value = !*lookup_lexical(c, e->name) &&
                                  global_binding(c, e->name) != NULL;
            if(e->is_global_value) {
                const ZirModule *owner = NULL;
                const ZirGlobal *global = NULL;
                if(ResolveGlobal(c->module, e->name,
                                 &owner, &global) == 1) {
                    int qualified = qualified_global_type(c->module,
                        e->name, owner, global->type,
                        e->type, sizeof(e->type));
                    if(qualified < 0)
                        error(c, e->span, "qualified global type is too long",
                              e->name);
                    else if(qualified > 0)
                        type = e->type;
                }
            }
            for(int i = c->count - 1; i >= 0; i--)
                if(!c->bindings[i].is_using_namespace &&
                   !strcmp(c->bindings[i].name, e->name)) {
                    c->bindings[i].touched = 1;
                    if(c->bindings[i].moved && !c->assign_destination)
                        error(c, e->span, "Vec binding is used after moving",
                              e->name);
                    break;
                }
            check_moved_path_use(c, index);
        }
        if(!*type) {
            CompoundConstant compound = {0};
            if(bound_compound_constant(c->module, e->name, 0,
                                       &compound) == 1) {
                if(!inline_compound_constant(c, index, &compound)) {
                    error(c, e->span,
                          "cannot bind aggregate constant", e->name);
                    break;
                }
                return expression_type(c, index);
            }
            /* A bare procedure name is a value when the surrounding context
             * expects a matching slot (return, assignment, argument). */
            if(c->expected_type[0] && !e->is_this &&
               !*lookup_lexical(c, e->name)) {
                const ZirType *expected_slot =
                    FindType(c->module, c->expected_type, NULL);
                const ZirModule *decl_owner = NULL;
                const ZirFunction *declaration = NULL;
                if(expected_slot != NULL && expected_slot->is_procedure_type &&
                   ResolveFunction(c->module, e->name, &decl_owner,
                                   &declaration) == 1) {
                    contextual_slot(c, index, c->expected_type);
                    if(c->failed)
                        break;
                    if(e->is_function_value) {
                        type = e->type;
                        break;
                    }
                }
            }
            int string_status = bound_string_constant(c->module, e->name, 0,
                                                       buffers->literal, sizeof(buffers->literal));
            int real_status = string_status == 0 ?
                bound_real_constant(c->module, e->name, 0,
                                    buffers->literal, sizeof(buffers->literal)) : 0;
            int64_t value = 0;
            int status = string_status == 0 && real_status == 0 ?
                integer_constant(c->module, e->name, &value) :
                string_status != 0 ? string_status : real_status;
            if(string_status == 1) {
                e->text = KeepText(buffers->literal);
                e->kind = ZIR_EXPR_STRING;
                type = "string";
            } else if(real_status == 1) {
                e->text = KeepText(buffers->literal);
                e->kind = ZIR_EXPR_FLOAT;
                type = "real";
            } else if(status == 1) {
                char digits[32];
                snprintf(digits, sizeof(digits), "%lld", (long long)value);
                e->text = KeepText(digits);
                e->kind = ZIR_EXPR_INT;
                type = "integer";
            } else if(status < 0) {
                error(c, e->span, "invalid or ambiguous constant", e->name);
            } else {
                char detail[ZIR_TEXT_MAX];
                error(c, e->span, "unresolved name",
                      unresolved_detail(c, e->name, 0, detail, sizeof(detail)));
            }
        }
        break;
    case ZIR_EXPR_CALL: {
        char display_name[ZIR_NAME_MAX];
        copy_text(display_name, sizeof(display_name), e->name);
        if(!strcmp(e->name, "TextView")) {
            int first = e->first_child;
            if(first < 0 || c->fn->exprs[first].next_sibling >= 0 ||
               c->fn->exprs[first].argument_name[0] ||
               strcmp(expression_type(c, first), "[]u8"))
                error(c, e->span, "TextView requires one []u8 argument", e->name);
            type = "string";
            break;
        }
        if(!strcmp(e->name, "print")) {
            PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
            int first = e->first_child, placeholders = 0, arguments = 0;
            int count, expanded = 0, errors_before = c->errors;
            for(int child = first; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                const char *arg_type = expression_type(c, child);
                {
                    const char *selected = child == first ? NULL :
                        select_first_result(c, child);
                    if(selected != NULL) {
                        arg_type = selected;
                        e = &c->fn->exprs[index];
                    }
                    const char *named = child == first ? NULL :
                        print_enum_by_name(c, child);
                    if(named != NULL) {
                        arg_type = named;
                        e = &c->fn->exprs[index];
                    }
                }
                const char *scalar = ScalarType(arg_type);
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "print has no named parameters",
                          c->fn->exprs[child].argument_name);
                if(child == first)
                    continue;
                arguments++;
                if(!strcmp(arg_type, "integer"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "s64");
                else if(!strcmp(arg_type, "real"))
                    copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, "float64");
                else if(*arg_type && print_expands(c, arg_type))
                    expanded++;
                else if(*arg_type &&
                        (scalar == NULL ||
                        (!integer_type(arg_type) &&
                         strcmp(scalar, "bool") &&
                         strcmp(scalar, "float32") &&
                         strcmp(scalar, "float64") &&
                         strcmp(scalar, "string"))))
                    error(c, c->fn->exprs[child].span,
                          "print argument must be an integer, float, bool, string, enum, record, or array",
                          arg_type);
            }
            if(pieces == NULL) {
                c->failed = 1;
                break;
            }
            if(first < 0 || c->fn->exprs[first].kind != ZIR_EXPR_STRING)
                error(c, e->span, "print requires a string literal format",
                      e->name);
            else if((count = PrintFormatPieces(c->fn->exprs[first].text,
                                               pieces, PRINT_PIECES_MAX)) < 0)
                error(c, c->fn->exprs[first].span,
                      "print format is invalid or too long", e->name);
            else {
                for(int i = 0; i < count; i++)
                    placeholders += pieces[i].is_argument;
                if(placeholders != arguments) {
                    char detail[64];
                    snprintf(detail, sizeof(detail), "%d %% for %d argument%s",
                             placeholders, arguments, arguments == 1 ? "" : "s");
                    error(c, e->span,
                          "print format placeholders do not match arguments",
                          detail);
                }
            }
            free(pieces);
            if(expanded && c->errors == errors_before &&
               !print_through_procedure(c, index) && !c->inference_only &&
               c->errors == errors_before)
                error(c, e->span, "print format is too long to show these values",
                      e->name);
            e = &c->fn->exprs[index];
            type = "void";
            break;
        }
        if(MapPrimitiveName(e->name)) {
            int first = e->first_child;
            int second = first >= 0 ? c->fn->exprs[first].next_sibling : -1;
            int third = second >= 0 ? c->fn->exprs[second].next_sibling : -1;
            int set = !strcmp(e->name, "MapSet");
            int get = !strcmp(e->name, "MapGet");
            int lookup = !strcmp(e->name, "MapLookup");
            int contains = !strcmp(e->name, "MapContains");
            int remove = !strcmp(e->name, "MapDelete");
            int keys = !strcmp(e->name, "MapKeys");
            int count = !strcmp(e->name, "MapCount");
            int keyed = set || get || lookup || contains || remove;
            for(int child = first; child >= 0; child = c->fn->exprs[child].next_sibling)
                if(c->fn->exprs[child].argument_name[0])
                    error(c, e->span, "map operations have no named parameters", e->name);
            if(first < 0) {
                error(c, e->span, "map operation requires a Map value", e->name);
                break;
            }
            char map_type[ZIR_NAME_MAX], key[ZIR_NAME_MAX], value[ZIR_NAME_MAX];
            copy_text(map_type, sizeof(map_type), expression_type(c, first));
            if(!MapTypePartsAtUse(c->module, map_type, key, sizeof(key), value, sizeof(value))) {
                error(c, e->span, "map operation requires a Map value", e->name);
                break;
            }
            if((set || !strcmp(e->name, "MapInit")) && !assignable(c, first))
                error(c, e->span, "map initialization requires mutable storage", e->name);
            int actual = third >= 0 ? (c->fn->exprs[third].next_sibling >= 0 ? 4 : 3) :
                         second >= 0 ? 2 : 1;
            if(actual != (set ? 3 : keyed ? 2 : 1)) {
                error(c, e->span, "wrong number of map operation arguments", e->name);
                break;
            }
            if(keyed && !compatible_checked(c, key, expression_type(c, second)))
                error(c, e->span, "map key type mismatch", key);
            if(set) {
                char saved[ZIR_NAME_MAX];
                copy_text(saved, sizeof(saved), c->expected_type);
                copy_text(c->expected_type, sizeof(c->expected_type), value);
                const char *given = expression_type(c, third);
                copy_text(c->expected_type, sizeof(c->expected_type), saved);
                if(!compatible_checked(c, value, given))
                    error(c, e->span, "map value type mismatch", value);
            }
            if(lookup) {
                if(vec_option_result_type(c, value, e->span, e->type, sizeof(e->type)))
                    type = e->type;
            } else if(get) {
                copy_text(e->type, sizeof(e->type), value);
                type = e->type;
            } else if(keys) {
                if(snprintf(e->type, sizeof(e->type), "[]%s", key) >= (int)sizeof(e->type))
                    error(c, e->span, "map key slice type is too long", key);
                type = e->type;
            } else
                type = contains ? "bool" : count ? "s64" : "void";
            break;
        }
        if(!strcmp(e->name, "VecPush") || !strcmp(e->name, "VecClear") ||
           !strcmp(e->name, "VecFree") || !strcmp(e->name, "VecSwap") ||
           !strcmp(e->name, "VecPop") || !strcmp(e->name, "VecGet") ||
           !strcmp(e->name, "VecClone") || !strcmp(e->name, "VecSlice") ||
           !strcmp(e->name, "BuilderAppend") ||
           !strcmp(e->name, "BuilderFinish")) {
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling)
                if(c->fn->exprs[child].argument_name[0])
                    error(c, c->fn->exprs[child].span,
                          "Vec operation has no named parameters",
                          c->fn->exprs[child].argument_name);
            int first = e->first_child;
            int second = first >= 0 ? c->fn->exprs[first].next_sibling : -1;
            int push = !strcmp(e->name, "VecPush");
            int swap = !strcmp(e->name, "VecSwap");
            int pop = !strcmp(e->name, "VecPop");
            int get = !strcmp(e->name, "VecGet");
            int text_append = !strcmp(e->name, "BuilderAppend");
            int text_finish = !strcmp(e->name, "BuilderFinish");
            int byte_builder = text_append || text_finish;
            char element[ZIR_NAME_MAX] = "";
            Binding *first_binding = lexical_vec_binding(c, first);
            int first_touched = first_binding != NULL &&
                                first_binding->touched;
            const char *vector_type = expression_type(c, first);
            int clone = !strcmp(e->name, "VecClone");
            int view = !strcmp(e->name, "VecSlice");
            int vector_known = first >= 0 &&
                VecElementType(c->module, vector_type, element, sizeof(element));
            if(!vector_known || !(get || assignable(c, first)))
                error(c, e->span, "Vec operation requires mutable Vec storage",
                      first >= 0 && c->fn->exprs[first].kind == ZIR_EXPR_UNARY ?
                      "pass the Vec variable itself, as in VecPush(values, 1)" :
                      e->name);
            if(vector_known && byte_builder && strcmp(element, "u8"))
                error(c, e->span,
                      "string builder requires a Vec(u8) place", element);
            if(first_binding != NULL && first_binding->borrow_count > 0 &&
               !get && !view)
                error(c, e->span,
                      "cannot mutate a Vec with a live borrowed view",
                      e->name);
            if(push) {
                if(contains_vec(c->module, element, 0))
                    error(c, e->span, "nested Vec elements are not supported", element);
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecPush requires a value", e->name);
                else if(vector_known) {
                    /* The element type gives an inferred .{...} its record. */
                    char saved_expected[ZIR_NAME_MAX];
                    copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                    copy_text(c->expected_type, sizeof(c->expected_type), element);
                    const char *item_type = expression_type(c, second);
                    copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                    e = &c->fn->exprs[index];
                    if(!compatible_checked(c, element, item_type))
                        error(c, e->span, "VecPush element type mismatch", element);
                    else if(!strcmp(item_type, "integer") ||
                            !strcmp(item_type, "real")) {
                        const char *scalar = ScalarType(element);
                        if(*scalar)
                            copy_text(c->fn->exprs[second].type,
                                      ZIR_NAME_MAX, scalar);
                    }
                }
            } else if(swap) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0 ||
                   !assignable(c, second) ||
                   strcmp(vector_type, expression_type(c, second)))
                    error(c, e->span, "VecSwap requires two matching Vec places", e->name);
            } else if(pop || text_finish) {
                if(second >= 0)
                    error(c, e->span, "Vec operation takes one argument", e->name);
            } else if(get) {
                const char *index_type;
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "VecGet requires an index", e->name);
                else {
                    index_type = expression_type(c, second);
                    if(!integer_type(index_type))
                        error(c, e->span, "VecGet requires an integer index",
                              index_type);
                }
            } else if(text_append) {
                if(second < 0 || c->fn->exprs[second].next_sibling >= 0)
                    error(c, e->span, "BuilderAppend requires text", e->name);
                else if(strcmp(expression_type(c, second), "string"))
                    error(c, e->span, "BuilderAppend requires string text",
                          expression_type(c, second));
            } else if(clone) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                if(second < 0 || third >= 0)
                    error(c, e->span, "VecClone requires a source Vec", e->name);
                else {
                    const char *source_type = expression_type(c, second);
                    if(!VecElementType(c->module, source_type, NULL, 0) ||
                       strcmp(vector_type, source_type))
                        error(c, e->span,
                              "VecClone requires a matching Vec source",
                              e->name);
                    else {
                        Binding *destination = lexical_vec_binding(c, first);
                        if(c->fn->exprs[first].kind != ZIR_EXPR_IDENT)
                            error(c, e->span,
                                  "VecClone requires a simple binding destination",
                                  e->name);
                        else if(destination == NULL)
                            error(c, e->span,
                                  "global Vec storage cannot clone; use a local",
                                  e->name);
                        else if(destination->borrow_count > 0)
                            error(c, e->span,
                                  "cannot mutate a Vec with a live borrowed view",
                                  e->name);
                        else if(first_touched && !destination->moved)
                            error(c, e->span,
                                  "clone destination must be fresh or moved-from",
                                  destination->name);
                    }
                }
            } else if(view) {
                int third = second >= 0 ?
                    c->fn->exprs[second].next_sibling : -1;
                Binding *source = lexical_vec_binding(c, first);
                if(source == NULL)
                    error(c, e->span,
                          "VecSlice requires a local Vec binding", e->name);
                if(second < 0 || third < 0 ||
                   c->fn->exprs[third].next_sibling >= 0)
                    error(c, e->span, "VecSlice requires low and high bounds",
                          e->name);
                else {
                    if(!integer_type(expression_type(c, second)) ||
                       !integer_type(expression_type(c, third)))
                        error(c, e->span,
                              "VecSlice requires integer bounds", e->name);
                }
            } else if(second >= 0)
                error(c, e->span, "Vec operation takes one argument", e->name);
            if(pop) {
                /* The result outlives this block, so it goes in e->type. */
                if(vec_option_result_type(c, element, e->span, e->type,
                                           sizeof(e->type)))
                    type = e->type;
                else
                    type = "";
            } else if(get) {
                /* The result outlives this block, so it goes in e->type. */
                if(vec_option_result_type(c, element, e->span, e->type,
                                           sizeof(e->type)))
                    type = e->type;
                else
                    type = "";
            } else if(view) {
                if(snprintf(c->vec_slice_type, sizeof(c->vec_slice_type),
                            "[]%s", element) < (int)sizeof(c->vec_slice_type))
                    type = c->vec_slice_type;
                else
                    type = "";
            } else if(push || text_append || clone)
                type = "bool";
            else if(text_finish)
                type = "string";
            else
                type = "void";
            break;
        }
        select_overload(c, index);
        e = &c->fn->exprs[index];
        const char *binding = e->is_this ? "" : lookup(c, e->name);
        e->is_global_value = !e->is_this && !*lookup_lexical(c, e->name) &&
                              global_binding(c, e->name) != NULL;
        const ZirType *slot = FindType(c->module, binding, NULL);
        if(slot != NULL && !slot->is_procedure_type)
            slot = NULL;
        copy_text(e->slot_type, sizeof(e->slot_type), slot ? binding : "");
        const ZirFunction *callee = *binding ? NULL : function(c, e->name, e->span);
        const ZirModule *callee_owner = NULL;
        if(callee != NULL) {
            const ZirFunction *resolved = NULL;
            if(ResolveFunctionAt(c->module, e->name, SpanPath(e->span),
                                 &callee_owner, &resolved) != 1 ||
               resolved != callee) callee_owner = NULL;
        }
        buffers->specialized_args[0] = '\0';
        char specialized_return[ZIR_NAME_MAX] = "";
        if(callee != NULL && callee->is_template) {
            const ZirModule *owner = NULL;
            const ZirFunction *resolved = NULL;
            if(ResolveFunction(c->module, e->name, &owner, &resolved) != 1 ||
               resolved != callee || owner == NULL) {
                error(c, e->span, "cannot resolve polymorphic procedure", e->name);
                break;
            }
            char (*parameters)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parameters));
            if(parameters == NULL) { c->failed = 1; break; }
            int parameter_count = *skip_ws(callee->args) ?
                split_top_level(callee->args, parameters[0], 64,
                                sizeof(parameters[0])) : 0;
            if(!bind_call_arguments(c, e, parameters, parameter_count,
                                    display_name, callee->default_args)) {
                free(parameters);
                break;
            }
            /* One concrete type per $Name, in the procedure's list order. */
            char (*bound)[ZIR_NAME_MAX] = calloc(16, sizeof(*bound));
            if(bound == NULL) { free(parameters); c->failed = 1; break; }
            int bound_count = 0;
            for(const char *list = callee->template_param; *list; bound_count++) {
                const char *comma = strchr(list, ',');
                if(comma == NULL) { bound_count++; break; }
                list = comma + 1;
            }
            for(int child = e->first_child; child >= 0;
                child = c->fn->exprs[child].next_sibling) {
                int argument = c->fn->exprs[child].argument_index;
                const char *colon = strchr(parameters[argument], ':');
                if(colon == NULL) continue;
                const char *parameter_type = skip_ws(colon + 1);
                int prefix = TemplateBinderPrefix(parameter_type);
                int which = prefix < 0 ? -1 :
                    TemplateParameterIndex(callee->template_param,
                                           parameter_type + prefix + 1,
                                           strlen(parameter_type + prefix + 1));
                if(prefix < 0 && strchr(parameter_type, '(') != NULL) {
                    /* *Table(K, V) takes K and V from the argument's own
                     * instance of Table. */
                    bind_from_application(c, callee->template_param,
                                          parameter_type,
                                          expression_type(c, child), bound);
                    e = &c->fn->exprs[index];
                    continue;
                }
                if(which < 0 || which >= 16)
                    continue;
                char *concrete = bound[which];
                char saved_expected[ZIR_NAME_MAX];
                copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
                if(concrete[0] && prefix == 0)
                    copy_text(c->expected_type, sizeof(c->expected_type), concrete);
                const char *actual_type = expression_type(c, child);
                /* Checking the argument can move the expression array. */
                e = &c->fn->exprs[index];
                copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
                if(prefix > 0) {
                    /* `[]$T` binds a slice's element type and `*$T` a pointer's
                     * target type. */
                    if(strncmp(actual_type, parameter_type, (size_t)prefix)) {
                        error(c, c->fn->exprs[child].span,
                              prefix == 2 ? "polymorphic slice parameter needs a slice" :
                              "polymorphic pointer parameter needs a pointer", e->name);
                        continue;
                    }
                    actual_type += prefix;
                }
                if(!strcmp(actual_type, "integer") || !strcmp(actual_type, "real")) {
                    if(concrete[0]) actual_type = concrete;
                    else if(!strcmp(callee->return_type, parameter_type + prefix + 1) &&
                            ScalarType(saved_expected)[0])
                        actual_type = saved_expected;
                    else {
                        error(c, c->fn->exprs[child].span,
                              "polymorphic literal needs a concrete type", e->name);
                        continue;
                    }
                }
                if(!*actual_type || !strcmp(actual_type, "null")) {
                    error(c, c->fn->exprs[child].span,
                          "cannot infer polymorphic type", e->name);
                    continue;
                }
                if(!concrete[0])
                    copy_text(concrete, ZIR_NAME_MAX, actual_type);
                else if(strcmp(concrete, actual_type))
                    signature_error(c, c->fn->exprs[child].span,
                                    "polymorphic type mismatch", e->name);
            }
            free(parameters);
            char concrete[ZIR_NAME_MAX] = "";
            int inferred = bound_count > 0;
            for(int i = 0; i < bound_count && inferred; i++) {
                size_t used = strlen(concrete);
                if(!bound[i][0] ||
                   snprintf(concrete + used, sizeof(concrete) - used, "%s%s",
                            i ? "," : "", bound[i]) >= (int)(sizeof(concrete) - used))
                    inferred = 0;
            }
            free(bound);
            if(!inferred) {
                error(c, e->span, "cannot infer polymorphic type", e->name);
                break;
            }
            if(!replace_template_type(buffers->specialized_args, sizeof(buffers->specialized_args),
                                      callee->args, callee->template_param, concrete) ||
               !replace_template_type(specialized_return,
                                      sizeof(specialized_return),
                                      callee->return_type,
                                      callee->template_param, concrete)) {
                error(c, e->span, "specialized signature is too long", e->name);
                break;
            }
            /* Table(string, s32) in the signature names the same instance
             * type the arguments have. */
            if(strchr(buffers->specialized_args, '(') != NULL ||
               strchr(specialized_return, '(') != NULL) {
                char canonical[ZIR_TEXT_MAX];
                if(!rewrite_type_applications(c->module, buffers->specialized_args,
                                              canonical, sizeof(canonical), e->span, 0))
                    break;
                copy_text(buffers->specialized_args, sizeof(buffers->specialized_args), canonical);
                if(!rewrite_type_applications(c->module, specialized_return, canonical,
                                              sizeof(canonical), e->span, 0) ||
                   !fill_late_type_instances(c->module, e->span))
                    break;
                copy_text(specialized_return, sizeof(specialized_return), canonical);
                e = &c->fn->exprs[index];
            }
            char name[ZIR_NAME_MAX];
            /* An instance lives with the caller when one of its types is
             * visible there but not in the template's module. */
            const ZirModule *instance_owner = owner;
            if(owner != c->module) {
                char (*types)[ZIR_NAME_MAX] = calloc(16, sizeof(*types));
                if(types == NULL) { c->failed = 1; break; }
                int type_count = split_top_level(concrete, types[0], 16, sizeof(types[0]));
                for(int i = 0; i < type_count; i++)
                    if(FindType(owner, types[i], NULL) == NULL &&
                       FindType(c->module, types[i], NULL) != NULL)
                        instance_owner = c->module;
                free(types);
            }
            if(!queue_specialization(c, owner, instance_owner,
                                     callee, concrete,
                                     name, sizeof(name), e->span)) break;
            const char *dot = instance_owner == owner ?
                strchr(e->name, '.') : NULL;
            if(dot) {
                char qualified[ZIR_NAME_MAX];
                int written = snprintf(qualified, sizeof(qualified), "%.*s.%s",
                                       (int)(dot - e->name), e->name, name);
                if(written < 0 || (size_t)written >= sizeof(qualified)) {
                    error(c, e->span, "specialized name is too long", e->name);
                    break;
                }
                copy_text(e->name, sizeof(e->name), qualified);
            } else copy_text(e->name, sizeof(e->name), name);
        }
        if(callee != NULL && callee->is_extern && callee->extern_kind == ZIR_EXTERN_HOST)
            c->fn->uses_host = 1;
        const char *args = slot ? slot->body :
                           buffers->specialized_args[0] ? buffers->specialized_args :
                           callee ? callee->args : NULL;
        const char *return_type = slot ? slot->procedure_return_type :
                                  specialized_return[0] ? specialized_return :
                                  callee ? callee->return_type : "";
        char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
        int actual = 0, expected;
        int varargs = 0;
        if(!parts) { c->errors++; c->failed=1; break; }
        if(*binding && slot == NULL)
            error(c, e->span, "binding is not a callable function", e->name);
        if(!callee && slot == NULL) for(int i = 0; i < c->module->import_count; i++) {
            const ZirImport *imp = &c->module->imports[i];
            if(imp->kind == ZIR_IMPORT_EXTERN &&
               in_lookup_file(c->module, imp->is_file_private, imp->span) &&
               !strcmp(imp->name, e->name)) {
                if(imp->extern_kind == ZIR_EXTERN_HOST)
                    c->fn->uses_host = 1;
                args = imp->args; return_type = imp->return_type;
                varargs = imp->is_varargs;
                if(imp->go_defer && (!c->current_stmt ||
                   c->current_stmt->kind != ZIR_STMT_EXPR ||
                   c->current_stmt->expr_root != index))
                    error(c, e->span, "#go_defer requires a standalone call statement", e->name);
                break;
            }
        }
        /* Calling a procedure-typed record field (value.is_active(app)) or a
         * field behind a record pointer (practice.draw(app)): resolve the
         * field's slot type and type the call from its signature. Anonymous
         * calls name their callee in e->left. */
        if(!callee && slot == NULL) {
            if(e->name[0] == '\0' && e->left >= 0) {
                const ZirType *indirect = FindType(c->module, left, NULL);
                if(indirect != NULL && indirect->is_procedure_type) {
                    copy_text(e->slot_type, sizeof(e->slot_type), left);
                    slot = indirect;
                    args = indirect->body;
                    return_type = indirect->procedure_return_type;
                }
            } else if(strchr(e->name, '.') != NULL) {
                char base[ZIR_NAME_MAX];
                copy_text(base, sizeof(base), e->name);
                char *field = strrchr(base, '.');
                *field++ = '\0';
                const char *base_type = lookup(c, base);
                const char *record_name = base_type;
                if(*record_name == '*')
                    record_name = skip_ws(record_name + 1);
                const ZirModule *field_owner = NULL;
                const ZirType *record = FindType(c->module, record_name, &field_owner);
                if(record != NULL && !record->is_enum) {
                    char member_type[ZIR_NAME_MAX] = "";
                    char field_path[ZIR_NAME_MAX];
                    int found = strchr(field, '.') != NULL ?
                        RecordFieldPathType(field_owner, record, field,
                                            member_type, sizeof(member_type)) :
                        ResolveRecordField(field_owner, record, field,
                                           field_path, sizeof(field_path),
                                           member_type, sizeof(member_type));
                    const ZirType *field_slot = *member_type ?
                        FindType(c->module, member_type, NULL) : NULL;
                    if(found > 0 && field_slot != NULL &&
                       field_slot->is_procedure_type) {
                        copy_text(e->slot_type, sizeof(e->slot_type), member_type);
                        slot = field_slot;
                        args = field_slot->body;
                        return_type = field_slot->procedure_return_type;
                    }
                }
            }
        }
        expected = args && *skip_ws(args) ? split_top_level(args, parts[0], 64, sizeof(parts[0])) : 0;
        if(callee_owner != NULL && callee_owner != c->module)
            for(int parameter = 0; parameter < expected; parameter++) {
                char *colon = strchr(parts[parameter], ':');
                if(colon == NULL) continue;
                char mapped[ZIR_NAME_MAX];
                copy_text(mapped, sizeof(mapped), skip_ws(colon + 1));
                trim_in_place(mapped);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, mapped,
                                             sizeof(mapped))) {
                    error(c, e->span,
                          "imported procedure parameter type is shadowed",
                          e->name);
                    continue;
                }
                int written = snprintf(colon + 1,
                    sizeof(parts[parameter]) - (size_t)(colon + 1 - parts[parameter]),
                    " %s", mapped);
                if(written < 0 || (size_t)written >=
                   sizeof(parts[parameter]) -
                       (size_t)(colon + 1 - parts[parameter]))
                    error(c, e->span, "imported parameter type is too long",
                          e->name);
            }
        int fixed = varargs && expected > 0 ? expected - 1 : expected;
        if(varargs && expected > 0) {
            /* bind_call_arguments assigns slots from the fixed parameters;
             * variadic extras take the next sequential slots. */
            if(!bind_varargs_call(c, e, parts, fixed, display_name)) {
                free(parts);
                break;
            }
        } else if(args && !bind_call_arguments(c, e, parts, expected, display_name,
                                       callee ? callee->default_args : NULL)) {
            free(parts);
            break;
        }
        for(int child = e->first_child; child >= 0; child = c->fn->exprs[child].next_sibling) {
            int parameter = c->fn->exprs[child].argument_index;
            const char *expected_type = parameter >= 0 && parameter < expected &&
                parameter < fixed ?
                strchr(parts[parameter], ':') : NULL;
            char saved_expected[ZIR_NAME_MAX];
            copy_text(saved_expected, sizeof(saved_expected), c->expected_type);
            if(expected_type != NULL)
                contextual_slot(c, child, skip_ws(expected_type + 1));
            if(expected_type != NULL)
                copy_text(c->expected_type, sizeof(c->expected_type),
                          skip_ws(expected_type + 1));
            const char *arg_type = expression_type(c, child);
            if(contains_vec(c->module, arg_type, 0) &&
               c->fn->exprs[child].kind != ZIR_EXPR_IDENT &&
               c->fn->exprs[child].kind != ZIR_EXPR_CALL &&
               !owned_initializer_shape(c, child))
                error(c, c->fn->exprs[child].span,
                      "Vec arguments move a binding or pass a call result",
                      display_name);
            if(contains_vec(c->module, arg_type, 0) &&
               global_vec_source(c, child))
                error(c, c->fn->exprs[child].span,
                      "global Vec storage cannot move; use a local",
                      c->fn->exprs[child].name);
            copy_text(c->expected_type, sizeof(c->expected_type), saved_expected);
            if(args && parameter >= 0 && parameter < fixed) {
                char *colon = strchr(parts[parameter], ':');
                if(colon && !compatible_checked(c, skip_ws(colon + 1), arg_type)) {
                    const char *converted = try_conversion(c, child,
                                                           skip_ws(colon + 1),
                                                           c->fn->exprs[child].span);
                    e = &c->fn->exprs[index];
                    if(converted != NULL)
                        arg_type = converted;
                    else {
                        char detail[ZIR_TEXT_MAX];
                        signature_error(c, c->fn->exprs[child].span,
                                        "argument type mismatch",
                                        mismatch_detail(detail, sizeof(detail),
                                                        display_name,
                                                        skip_ws(colon + 1),
                                                        arg_type));
                    }
                }
                if(colon && (!strcmp(arg_type, "integer") || !strcmp(arg_type, "real"))) {
                    const char *context = ScalarType(skip_ws(colon + 1));
                    if(*context) copy_text(c->fn->exprs[child].type, ZIR_NAME_MAX, context);
                }
            }
            actual++;
        }
        if(args) {
            char saved_checked_type[ZIR_NAME_MAX];
            copy_text(saved_checked_type, sizeof(saved_checked_type), e->type);
            type = return_type;
            if(specialized_return[0]) {
                copy_text(e->type, sizeof(e->type), specialized_return);
                type = e->type;
            }
            if(callee_owner != NULL && callee_owner != c->module) {
                if(type != e->type)
                    copy_text(e->type, sizeof(e->type), type);
                if(!record_field_type_at_use(c->module, callee_owner,
                                             e->name, e->type,
                                             sizeof(e->type)))
                    error(c, e->span,
                          "imported procedure result type is shadowed",
                          e->name);
                if(saved_checked_type[0] &&
                   same_declared_type(c->module, e->type,
                                      saved_checked_type, 0))
                    copy_text(e->type, sizeof(e->type), saved_checked_type);
                type = e->type;
            }
            if(actual < fixed && !c->inference_only) {
                char detail[ZIR_TEXT_MAX];
                snprintf(detail, sizeof(detail), "%s (takes %d, given %d)",
                         display_name, fixed, actual);
                signature_error(c, e->span, "argument count mismatch", detail);
            }
        } else {
            char detail[ZIR_TEXT_MAX];
            error(c, e->span, "unresolved function",
                  unresolved_detail(c, e->name, 1, detail, sizeof(detail)));
        }
        free(parts);
        break;
    }
    case ZIR_EXPR_BINARY: {
        {
            const char *selected = select_first_result(c, e->left);
            if(selected != NULL) {
                e = &c->fn->exprs[index];
                left = selected;
            }
            selected = select_first_result(c, e->right);
            if(selected != NULL) {
                e = &c->fn->exprs[index];
                right = selected;
            }
        }
        if(operator_call(c, index, left, right)) {
            /* Check the call (or its negation) this node now is. */
            type = expression_type(c, index);
            e = &c->fn->exprs[index];
            break;
        }
        if((operator_operand(c, left) || operator_operand(c, right)) &&
           OperatorProcedureName(e->op) != NULL &&
           strcmp(e->op, "==") && strcmp(e->op, "!=")) {
            error(c, e->span, "record operation needs an operator procedure, "
                  "such as operator + :: (a: T, b: T) -> T", e->op);
            type = operator_operand(c, left) ? left : right;
            break;
        }
        if(left[0] == '[' || right[0] == '[')
            error(c, e->span, "array values do not support binary operations", e->op);
        const ZirType *left_slot = FindType(c->module, left, NULL);
        const ZirType *right_slot = FindType(c->module, right, NULL);
        int map_null_compare =
            (!strcmp(e->op, "==") || !strcmp(e->op, "!=")) &&
            ((left_slot != NULL && left_slot->is_map && !strcmp(right, "null")) ||
             (right_slot != NULL && right_slot->is_map && !strcmp(left, "null")));
        if(((left_slot && left_slot->is_map) || (right_slot && right_slot->is_map)) &&
           !map_null_compare)
            error(c, e->span, "maps only support comparison with null", e->op);
        /* Procedure slots compare against null (and identical slot types)
         * the same way pointers do; every other operation stays rejected. */
        int slot_null_compare =
            (!strcmp(e->op, "==") || !strcmp(e->op, "!=")) &&
            ((left_slot != NULL && left_slot->is_procedure_type &&
              (!strcmp(right, "null") ||
               (right_slot != NULL && right_slot->is_procedure_type &&
                !strcmp(left, right)))) ||
             (right_slot != NULL && right_slot->is_procedure_type &&
              !strcmp(left, "null")));
        if(((left_slot && left_slot->is_procedure_type) ||
            (right_slot && right_slot->is_procedure_type)) && !slot_null_compare)
            error(c, e->span, "slot values do not support binary operations", e->op);
        if((text_type(left) || text_type(right)) &&
           strcmp(e->op, "==") && strcmp(e->op, "!="))
            error(c, e->span, "string operation is not supported", e->op);
        if((!strcmp(e->op, "&") || !strcmp(e->op, "|") || !strcmp(e->op, "^") ||
            !strcmp(e->op, "<<") || !strcmp(e->op, ">>") || !strcmp(e->op, "%")) &&
           (left[0] == 'f' || right[0] == 'f' || !strcmp(left, "real") || !strcmp(right, "real")))
            error(c, e->span, "integer operands required", e->op);
        if((!strcmp(e->op, "&&") || !strcmp(e->op, "||")) &&
           (strcmp(left, "bool") || strcmp(right, "bool")))
            error(c, e->span, "logical operands require bool", e->op);
        const ZirType *left_flags = flags_type(c, left);
        const ZirType *right_flags = flags_type(c, right);
        if(left_flags != NULL || right_flags != NULL) {
            const char *flag_name = left_flags != NULL ? left : right;
            const char *other = left_flags != NULL ? right : left;
            if((left_flags != NULL && right_flags != NULL && strcmp(left, right)) ||
               (strcmp(other, flag_name) && !integer_type(other)))
                error(c, e->span, "flag operands require the same enum or an integer", e->op);
            if(!strcmp(e->op, "==") || !strcmp(e->op, "!="))
                type = "bool";
            else if(!strcmp(e->op, "&") || !strcmp(e->op, "|") ||
                    !strcmp(e->op, "^") || !strcmp(e->op, "+") ||
                    !strcmp(e->op, "-"))
                type = flag_name;
            else
                error(c, e->span, "unsupported enum_flags operation", e->op);
            break;
        }
        if(!compatible_checked(c, left, right) &&
           !compatible_checked(c, right, left) && !slot_null_compare &&
           !map_null_compare) {
            /* A narrower operand widens to the other operand's type. */
            int widen_left = widens_losslessly(right, left);
            if(widen_left || widens_losslessly(left, right)) {
                const char *wider = ScalarType(widen_left ? right : left);
                if(!c->inference_only &&
                   widen_expression(c, widen_left ? e->left : e->right, wider) == NULL)
                    return "";
                e = &c->fn->exprs[index];
                left = right = wider;
            } else
                error(c, e->span, "operand types differ; use an explicit cast", e->op);
        }
        if(!strcmp(e->op, "==") || !strcmp(e->op, "!=") || !strcmp(e->op, "<") ||
           !strcmp(e->op, "<=") || !strcmp(e->op, ">") || !strcmp(e->op, ">=") ||
           !strcmp(e->op, "&&") || !strcmp(e->op, "||")) type = "bool";
        else if(numeric(left) && numeric(right))
            type = (!strcmp(left, "integer") || !strcmp(left, "real")) ? right : left;
        else if(*left && *right) error(c, e->span, "numeric operands required", e->op);
        break;
    }
    case ZIR_EXPR_UNARY:
        if(strcmp(e->op, "+") && strcmp(e->op, "-") &&
           strcmp(e->op, "!") && strcmp(e->op, "~") &&
           strcmp(e->op, "&") && strcmp(e->op, "*"))
            error(c, e->span, "unsupported unary operator", e->op);
        if(!strcmp(e->op, "!") && strcmp(right, "bool"))
            error(c, e->span, "logical operand requires bool", e->op);
        if(!strcmp(e->op, "~") && !integer_type(right))
            error(c, e->span, "bitwise operand requires an integer", e->op);
        if(!strcmp(e->op, "&")) {
            if(!assignable(c, e->right))
                error(c, e->span, "address-of requires an assignable expression", e->text);
            if(!*right || !strcmp(right, "null") || strlen(right) + 1 >= sizeof(e->type))
                error(c, e->span, "address-of requires a known type", e->text);
            snprintf(e->type, sizeof(e->type), "*%s", right);
            type = e->type;
        } else if(!strcmp(e->op, "*")) {
            if(right[0] != '*' || !*skip_ws(right + 1))
                error(c, e->span, "dereference requires a pointer", right);
            type = skip_ws(right + 1);
        } else if(!strcmp(e->op, "!")) type = "bool";
        else if(numeric(right)) type = right;
        else
            error(c, e->span, "unresolved unary operation", e->op);
        break;
    case ZIR_EXPR_CAST: {
        if(!JaiTypeSpelling(e->span, e->name)) {
            c->errors++;
            return "";
        }
        const ZirType *destination = FindType(c->module, e->name, NULL);
        const ZirType *source = FindType(c->module, right, NULL);
        if(right[0] == '[' || e->name[0] == '[') {
            const ZirType *foreign = right[0] == '[' ? destination : source;
            const char *slice = right[0] == '[' ? right : e->name;
            char element[ZIR_NAME_MAX];
            if(!SliceElementType(slice, element, sizeof(element)) ||
               !foreign || strncmp(foreign->foreign_target, "go:", 3) ||
               contains_vec(c->module, element, 0))
                error(c, e->span, "slice casts require a foreign Go type without owned elements", e->name);
        }
        if(destination != NULL && destination->is_enum &&
           !numeric(right) && strcmp(right, "bool") &&
           (source == NULL || !source->is_enum))
            error(c, e->span, "enum casts require a numeric, bool, or enum value", e->name);
        if((text_type(right) || text_type(e->name)) && strcmp(right, e->name))
            error(c, e->span, "string casts require an explicit conversion API", e->name);
        type = e->name;
        break;
    }
    case ZIR_EXPR_CONDITIONAL: {
        const char *third = expression_type(c, e->third);
        if(strcmp(left, "bool")) error(c, e->span, "conditional requires bool", left);
        if(!compatible_checked(c, right, third) &&
           !compatible_checked(c, third, right))
            error(c, e->span, "conditional arms have different types", "");
        type = !strcmp(right, "integer") || !strcmp(right, "null") ?
               third : right;
        break;
    }
    default: error(c, e->span, "expression is not supported by language checking", e->text); break;
    }
    if(*ScalarType(type)) type = ScalarType(type);
    if(type != e->type)
        copy_text(e->type, sizeof(e->type), type);
    normalize_array(c->module, e->type, sizeof(e->type));
    return e->type;
}

const char *
expression_type(Checker *c, int index)
{
    static _Thread_local ExpressionTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    ExpressionTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    const char *returned = expression_type_with_buffers(c, index, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
/* Buffers InferExpressionType keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct InferExpressionTypeBuffers {
    ZirModule lookup;
    ZirFunction probe;
    Checker checker;
} InferExpressionTypeBuffers;

int InferExpressionType(const ZirModule *module, const char *expression,
                    ZirSourceSpan span, char *type, size_t capacity);

static int
InferExpressionType_with_buffers(const ZirModule *module, const char *expression,
                    ZirSourceSpan span, char *type, size_t capacity, InferExpressionTypeBuffers *buffers)
{
    buffers->lookup = *module;
    memset(&buffers->probe, 0, sizeof(buffers->probe));
    memset(&buffers->checker, 0, sizeof(buffers->checker));
    int root, valid;
    const char *inferred;
    copy_text(buffers->lookup.lookup_path, sizeof(buffers->lookup.lookup_path), SpanPath(span));
    buffers->probe.span = span;
    root = ParseExprNoDefaults(&buffers->probe, &buffers->lookup, expression, span);
    buffers->checker.module = &buffers->lookup;
    buffers->checker.fn = &buffers->probe;
    buffers->checker.inference_only = 1;
    inferred = root >= 0 ? expression_type(&buffers->checker, root) : "";
    if(strcmp(inferred, "integer") == 0) inferred = "s64";
    else if(strcmp(inferred, "real") == 0) inferred = "float64";
    valid = buffers->checker.errors == 0 && *inferred != '\0' &&
            strlen(inferred) < capacity;
    if(valid) copy_text(type, capacity, inferred);
    free(buffers->checker.bindings);
    free(buffers->checker.specializations);
    free(buffers->probe.exprs);
    return valid;
}

int
InferExpressionType(const ZirModule *module, const char *expression,
                    ZirSourceSpan span, char *type, size_t capacity)
{
    static _Thread_local InferExpressionTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    InferExpressionTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = InferExpressionType_with_buffers(module, expression, span, type, capacity, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
