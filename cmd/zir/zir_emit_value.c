#include "zir_emit_internal.h"

int zir_minify_output;

void
EmitUseMinifiedOutput(int enabled)
{
    zir_minify_output = enabled != 0;
}

int
plain_identifier(const char *text)
{
    if(!(*text == '_' || (*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z')))
        return 0;
    for(const char *p = text + 1; *p; p++)
        if(!(*p == '_' || (*p >= 'a' && *p <= 'z') ||
             (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')))
            return 0;
    return 1;
}

/* A materialized temporary only re-copies a value that earlier statements
 * already captured, so identifiers always pass through. Pure expressions
 * inline while they stay short enough to read. C arrays are not values, so
 * array results keep their temporary. */
int
folds_text(const Emitter *e, const char *text, const char *type)
{
    if(e->target != ZIR_GO && ArrayElementType(type, NULL, 0, NULL))
        return 0;
    if(plain_identifier(text))
        return 1;
    return e->pure && (e->minify || strlen(text) <= ZIR_INLINE_MAX);
}

void
line(Emitter *e, const char *format, ...)
{
    va_list ap;
    /* Go indents with tabs, as gofmt writes it. */
    for(int i = 0; i < e->indent; i++) fputs(e->target == ZIR_GO ? "\t" : "    ", e->out);
    va_start(ap, format); vfprintf(e->out, format, ap); va_end(ap);
    fputc('\n', e->out);
}

void
fatal(const ZirExpr *expr, const char *message)
{
    Diagnostic(expr->span, "emit.expression", "%s: %s", message, expr->text);
    exit(1);
}

void
fresh(Emitter *e, char *name)
{
    int collision;
    do {
        format(name, ZIR_NAME_MAX, "value_%d", e->serial++);
        collision = function_mentions(e->fn, name);
        for(int i = 0; i < e->module->global_count; i++) collision |= !strcmp(e->module->globals[i].name, name);
        for(int i = 0; i < e->module->define_count; i++) collision |= !strcmp(e->module->defines[i].name, name);
        for(int i = 0; i < e->module->function_count; i++) collision |= !strcmp(e->module->functions[i].name, name);
    } while(collision);
}

void
assign_value(Emitter *e, const char *destination, const char *type, const char *source)
{
    char plain[ZIR_TEXT_MAX];
    source = bare(source, plain, sizeof(plain));
    if(ArrayElementType(type, NULL, 0, NULL) &&
       (e->target == ZIR_C || e->target == ZIR_CPP)) {
        line(e, "memmove(%s, %s, sizeof(%s));", destination, source, destination);
    } else {
        line(e, "%s = %s%s", destination, source, e->target == ZIR_GO ? "" : ";");
    }
}

/* A fixed array type's element type and bound list in the target: for
 * [4][96]u8, uint8_t and [4][96]. */
void
array_target_type(Emitter *e, const char *type, char *target_element,
                  size_t element_size, char *bounds, size_t bounds_size)
{
    char element[ZIR_NAME_MAX];
    /* Nested fixed arrays ([4][96]u8) flatten to one C dimension list:
     * uint8_t name[4][96]. Collect every bound outside-in. */
    bounds[0] = '\0';
    char working[ZIR_NAME_MAX * 2];
    int capacity;

    copy_text(working, sizeof(working), type);
    for(;;) {
        char inner[ZIR_NAME_MAX];
        char bound[ZIR_NAME_MAX];

        if(!ArrayElementType(working, inner, sizeof(inner), &capacity))
            break;
        if(capacity >= 0) {
            format(bound, sizeof(bound), "[%d]", capacity);
        } else {
            char symbol[ZIR_NAME_MAX];
            format(symbol, sizeof(symbol), "%.*s", (int)(strchr(working, ']') - working - 1), working + 1);
            e->resolve(e->context, symbol, bound, sizeof(bound));
            char wrapped[ZIR_NAME_MAX];
            format(wrapped, sizeof(wrapped), "[%s]", bound);
            copy_text(bound, sizeof(bound), wrapped);
        }
        copy_text(working, sizeof(working), inner);
        if(bounds[0] == '\0') {
            copy_text(bounds, bounds_size, bound);
        } else if(strlen(bounds) + strlen(bound) < bounds_size) {
            strcat(bounds, bound);
        }
    }
    copy_text(element, sizeof(element), working);
    const char *scalar = TargetType(element, e->target);
    if(scalar != NULL) {
        copy_text(target_element, element_size, scalar);
    } else if(element[0] == '*') {
        /* Pointer elements lower through the same naming as slot types:
         * [4]*u8 emits uint8_t* name[4]. */
        char native[ZIR_NAME_MAX * 2];
        slot_native_type(element, e->target, native, sizeof(native));
        copy_text(target_element, element_size, native);
    } else {
        e->resolve(e->context, element, target_element, element_size);
    }
}

void
declare_array(Emitter *e, const char *name, const char *type, const char *value)
{
    char target_element[ZIR_NAME_MAX * 2];
    char bounds[ZIR_NAME_MAX * 2];
    array_target_type(e, type, target_element, sizeof(target_element),
                      bounds, sizeof(bounds));
    if(e->target == ZIR_GO) {
        int short_form = e->short_declaration;
        e->short_declaration = 0;
        if(value != NULL && *value && short_form)
            line(e, "%s := %s", name, value);
        else if(value != NULL && *value)
            line(e, "var %s %s%s = %s", name, bounds, target_element, value);
        else
            line(e, "var %s %s%s", name, bounds, target_element);
    } else if(e->target == ZIR_C || e->target == ZIR_CPP) {
        int zero = TypeHasZeroArray(e->module, type);
        /* A brace list initializes the array in place. */
        int listed = value != NULL && value[0] == '{';
        line(e, "%s%s %s%s = %s;", zero ? "__extension__ " : "",
             target_element, name, bounds,
             listed ? value : zero || e->target == ZIR_CPP ? "{}" : "{0}");
        if(value != NULL && *value && !listed)
            assign_value(e, name, type, value);
    } else {
        Diagnostic(e->fn->span, "emit.array_target", "array values are supported only by native targets");
        exit(1);
    }
}

/* A number or character constant, which Go leaves untyped. */
static int
numeric_literal(const char *text)
{
    while(*text == '-' || *text == '+' || *text == '(')
        text++;
    return isdigit((unsigned char)*text) || *text == '.' || *text == '\'';
}

void
declare(Emitter *e, const char *name, const char *type, const char *value)
{
    char binding[ZIR_NAME_MAX];
    char plain[ZIR_TEXT_MAX];
    int short_form = e->short_declaration;
    if(value != NULL)
        value = bare(value, plain, sizeof(plain));
    /* An untyped Go constant would take Go's default type, so a literal
     * keeps the declared type. */
    if(value == NULL || !*value || numeric_literal(value))
        short_form = 0;
    if(ArrayElementType(type, NULL, 0, NULL))
        e->short_declaration = short_form;
    else
        e->short_declaration = 0;
    TargetBindingName(e->fn, e->target, name, binding, sizeof(binding));
    name = binding;
    if(ArrayElementType(type, NULL, 0, NULL)) {
        declare_array(e, name, type, value);
        return;
    }
    const char *target_type = TargetType(type, e->target);
    char resolved_type[ZIR_NAME_MAX * 2];
    if(type[0] == '*') {
        const char *pointee = type;
        char base_type[ZIR_NAME_MAX];
        size_t pointer_depth = 0;
        while(pointee[pointer_depth] == '*')
            pointer_depth++;
        pointee += pointer_depth;
        const char *scalar = TargetType(pointee, e->target);
        if(e->target == ZIR_GO && !strcmp(pointee, "void"))
            copy_text(base_type, sizeof(base_type), "byte");
        else if(scalar != NULL)
            copy_text(base_type, sizeof(base_type), scalar);
        else
            e->resolve(e->context, pointee, base_type, sizeof(base_type));
        if(e->target == ZIR_GO) {
            size_t used = 0;
            for(size_t depth = 0;
                depth < pointer_depth && used + 1 < sizeof(resolved_type);
                depth++)
                resolved_type[used++] = '*';
            resolved_type[used] = '\0';
            copy_text(resolved_type + used, sizeof(resolved_type) - used,
                      base_type);
        } else {
            format(resolved_type, sizeof(resolved_type), "%s", base_type);
            size_t used = strlen(resolved_type);
            for(size_t depth = 0;
                depth < pointer_depth && used + 1 < sizeof(resolved_type);
                depth++)
                resolved_type[used++] = '*';
            resolved_type[used] = '\0';
        }
        target_type = resolved_type;
    } else if(target_type == NULL) {
        e->resolve(e->context, type, resolved_type, sizeof(resolved_type));
        target_type = resolved_type;
    }
    if(enum_type(e->module, type)) {
        /* A value that already has the enum type needs no conversion. */
        if(short_form && e->target == ZIR_GO)
            line(e, "%s := %s", name, value);
        else if(short_form)
            line(e, "%s %s = %s;", target_type, name, value);
        else if(e->target == ZIR_GO)
            line(e, "var %s %s = %s(%s)", name, target_type, target_type, value);
        else
            line(e, "%s %s = (%s)(%s);", target_type, name, target_type,
                 value);
        return;
    }
    if(e->target == ZIR_GO && short_form) line(e, "%s := %s", name, value);
    else if(e->target == ZIR_GO) line(e, "var %s %s = %s", name, target_type, value);
    else line(e, "%s%s %s = %s;",
              TypeHasZeroArray(e->module, type) ? "__extension__ " : "",
              target_type, name, value);
}

void
resolve(Emitter *e, const char *name, char *out, size_t size)
{
    for(int i = e->local_count - 1; i >= 0; i--)
        if(!strcmp(e->locals[i].name, name)) {
            TargetBindingName(e->fn, e->target, name, out, size);
            return;
        }
    e->resolve(e->context, name, out, size);
}

static void
drop_owned_value(Emitter *e, const char *value, const char *type,
                 const ZirModule *module, int depth)
{
    char element[ZIR_NAME_MAX];
    if(depth > 32 || value == NULL || type == NULL)
        return;
    if(VecElementType(module, type, NULL, 0)) {
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = nil", value);
            line(e, "%s.Capacity = 0", value);
            line(e, "%s.Count = 0", value);
        } else {
            line(e, "free(%s.data);", value);
            line(e, "%s.data = NULL;", value);
            line(e, "%s.capacity = 0;", value);
            line(e, "%s.count = 0;", value);
        }
        return;
    }
    int capacity = -1;
    if(ArrayElementType(type, element, sizeof(element), &capacity) &&
       capacity != 0) {
        char index[ZIR_NAME_MAX], item[ZIR_TEXT_MAX];
        fresh(e, index);
        if(e->target == ZIR_GO)
            line(e, "for %s := 0; %s < len(%s); %s++ {",
                 index, index, value, index);
        else
            line(e, "for (size_t %s = 0; %s < sizeof(%s) / sizeof((%s)[0]); ++%s) {",
                 index, index, value, value, index);
        e->indent++;
        format(item, sizeof(item), "(%s)[%s]", value, index);
        drop_owned_value(e, item, element, module, depth + 1);
        e->indent--;
        line(e, "}");
        return;
    }
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1) {
        char member[ZIR_TEXT_MAX], native[ZIR_NAME_MAX];
        TargetFieldName(record, e->target, field.name, native, sizeof(native));
        if(e->target == ZIR_GO)
            go_field_ident(native, native, sizeof(native));
        format(member, sizeof(member), "(%s).%s", value, native);
        drop_owned_value(e, member, field.type,
                         owner ? owner : module, depth + 1);
    }
}

void
clear_owned_value(Emitter *e, const char *value, const char *type,
                  const ZirModule *module, int depth)
{
    char element[ZIR_NAME_MAX];
    if(depth > 32 || value == NULL || type == NULL)
        return;
    if(VecElementType(module, type, NULL, 0)) {
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = nil", value);
            line(e, "%s.Capacity = 0", value);
            line(e, "%s.Count = 0", value);
        } else {
            line(e, "%s.data = NULL;", value);
            line(e, "%s.capacity = 0;", value);
            line(e, "%s.count = 0;", value);
        }
        return;
    }
    int capacity = -1;
    if(ArrayElementType(type, element, sizeof(element), &capacity) &&
       capacity != 0) {
        char index[ZIR_NAME_MAX], item[ZIR_TEXT_MAX];
        fresh(e, index);
        if(e->target == ZIR_GO)
            line(e, "for %s := 0; %s < len(%s); %s++ {",
                 index, index, value, index);
        else
            line(e, "for (size_t %s = 0; %s < sizeof(%s) / sizeof((%s)[0]); ++%s) {",
                 index, index, value, value, index);
        e->indent++;
        format(item, sizeof(item), "(%s)[%s]", value, index);
        clear_owned_value(e, item, element, module, depth + 1);
        e->indent--;
        line(e, "}");
        return;
    }
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1) {
        char member[ZIR_TEXT_MAX], native[ZIR_NAME_MAX];
        TargetFieldName(record, e->target, field.name, native, sizeof(native));
        if(e->target == ZIR_GO)
            go_field_ident(native, native, sizeof(native));
        format(member, sizeof(member), "(%s).%s", value, native);
        clear_owned_value(e, member, field.type,
                          owner ? owner : module, depth + 1);
    }
}

void
track_local(Emitter *e, const char *name, const char *type)
{
    Local *local = &e->locals[e->local_count++];
    /* Slots are reused when a lexical block ends. A scalar in the next
     * block must not inherit an owned Vec's now out-of-scope drop alias. */
    memset(local, 0, sizeof(*local));
    copy_text(local->name, sizeof(local->name), name);
    copy_text(local->type, sizeof(local->type), type);
    local->depth = e->depth;
    if(VecElementType(e->module, type, NULL, 0)) {
        char pointer_type[ZIR_NAME_MAX * 2];
        char binding[ZIR_NAME_MAX];
        char address[ZIR_NAME_MAX + 2];
        TargetBindingName(e->fn, e->target, name, binding, sizeof(binding));
        fresh(e, local->drop_alias);
        format(pointer_type, sizeof(pointer_type), "*%s", type);
        format(address, sizeof(address), "&%s", binding);
        declare(e, local->drop_alias, pointer_type, address);
    }
}

void
drop_locals(Emitter *e, int first)
{
    for(int i = e->local_count - 1; i >= first; i--) {
        Local *local = &e->locals[i];
        const char *alias = local->drop_alias;
        if(!*alias) {
            char binding[ZIR_NAME_MAX];
            if(!emitter_type_contains_vec(e->module, local->type, 0))
                continue;
            TargetBindingName(e->fn, e->target, local->name, binding,
                              sizeof(binding));
            drop_owned_value(e, binding, e->locals[i].type, e->module, 0);
            continue;
        }
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = nil", alias);
            line(e, "%s.Capacity = 0", alias);
            line(e, "%s.Count = 0", alias);
        } else {
            line(e, "free(%s->data);", alias);
            line(e, "%s->data = NULL;", alias);
            line(e, "%s->capacity = 0;", alias);
            line(e, "%s->count = 0;", alias);
        }
    }
}

void
drop_temporary_vec(Emitter *e, const char *name)
{
    if(e->target == ZIR_GO) {
        line(e, "%s.Data = nil", name);
        line(e, "%s.Capacity = 0", name);
        line(e, "%s.Count = 0", name);
    } else {
        line(e, "free(%s.data);", name);
        line(e, "%s.data = NULL;", name);
        line(e, "%s.capacity = 0;", name);
        line(e, "%s.count = 0;", name);
    }
}

int
has_owned_locals(const Emitter *e)
{
    for(int i = 0; i < e->local_count; i++)
        if(*e->locals[i].drop_alias ||
           emitter_type_contains_vec(e->module, e->locals[i].type, 0))
            return 1;
    return 0;
}

int
operation(const char *op)
{
    static const char *const ops[] = {"", "+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"};
    for(int i = 1; i <= 10; i++) if(!strcmp(op,ops[i])) return i;
    return 0;
}

static void
go_bits_operand(const char *value, int sign,
                char *out, size_t size)
{
    if(!strcmp(value, "^uint64(0)")) {
        copy_text(out, size, value);
        return;
    }
    /* Go rejects uint64(-1) as a constant conversion. A negative integer
     * literal here represents its two's-complement bits, not an arithmetic
     * conversion of a Go constant. Runtime expressions already convert. */
    if(value[0] == '-' && isdigit((unsigned char)value[1])) {
        char *end;
        long long signed_value;
        errno = 0;
        signed_value = strtoll(value, &end, 0);
        if(errno == 0 && *end == '\0') {
            format(out, size, "uint64(%llu)",
                   (unsigned long long)(uint64_t)signed_value);
            return;
        }
    }
    if(sign)
        format(out, size, "signedBits(int64(%s))", value);
    else
        format(out, size, "uint64(%s)", value);
}

/* An emitted integer literal: optional sign, decimal or hex digits, and C
 * suffixes. Its two's-complement bits go to *bits. */
int
integer_literal_bits(const char *text, uint64_t *bits)
{
    const char *p = text;
    int negative = 0;
    char *end;
    while(*p == '(') p++;
    if(*p == '-') { negative = 1; p++; }
    if(!isdigit((unsigned char)*p))
        return 0;
    errno = 0;
    unsigned long long value = strtoull(p, &end, 0);
    if(errno != 0)
        return 0;
    while(*end == 'u' || *end == 'U' || *end == 'l' || *end == 'L') end++;
    while(*end == ')') end++;
    if(*end != '\0')
        return 0;
    *bits = negative ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
    return 1;
}

/* Fold op on literal operands with the checked width rule, as the number
 * helpers would at run time. Division by zero and out-of-range shifts are
 * left to fail when they execute. */
static int
fold_number(uint64_t a, uint64_t b, int w, int sign, int op, uint64_t *result)
{
    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;
    a &= mask;
    b &= mask;
    int64_t sa = sign && w < 64 && (a >> (w - 1)) ? (int64_t)(a | ~mask) : (int64_t)a;
    int64_t sb = sign && w < 64 && (b >> (w - 1)) ? (int64_t)(b | ~mask) : (int64_t)b;
    switch(op) {
    case 1: *result = (a + b) & mask; return 1;
    case 2: *result = (a - b) & mask; return 1;
    case 3: *result = (a * b) & mask; return 1;
    case 4: case 5:
        if(b == 0) return 0;
        if(sign) {
            if(sa == INT64_MIN && sb == -1) { *result = op == 4 ? a : 0; return 1; }
            if(w < 64 && sa == -(int64_t)(mask >> 1) - 1 && sb == -1) {
                *result = op == 4 ? a : 0;
                return 1;
            }
            *result = (uint64_t)(op == 4 ? sa / sb : sa % sb) & mask;
        } else
            *result = op == 4 ? a / b : a % b;
        return 1;
    case 6: case 7:
        if(b >= (uint64_t)w) return 0;
        if(op == 6) *result = (a << b) & mask;
        else if(sign && (a >> (w - 1)))
            *result = ((a >> b) | (mask ^ (mask >> b))) & mask;
        else
            *result = a >> b;
        return 1;
    case 8: *result = a & b; return 1;
    case 9: *result = a | b; return 1;
    case 10: *result = a ^ b; return 1;
    }
    return 0;
}

/* Fold a checked integer operation for backends outside this emitter. */
int
FoldIntegerOperation(const char *op, const char *type, uint64_t a, uint64_t b,
                     uint64_t *result)
{
    int w = width(canonical(type));
    return w > 0 && operation(op) > 0 &&
           fold_number(a, b, w, signed_type(canonical(type)), operation(op), result);
}

static void
number_literal(const Emitter *e, const char *type, uint64_t bits, char *out, size_t size)
{
    int w = width(type), sign = signed_type(type);
    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;
    bits &= mask;
    if(sign && (bits >> (w - 1))) {
        int64_t value = w == 64 ? (int64_t)bits : (int64_t)(bits | ~mask);
        if(value == INT64_MIN)
            format(out, size, e->target == ZIR_GO ? "(-9223372036854775807 - 1)" :
                   "(-9223372036854775807LL - 1)");
        else
            format(out, size, e->target == ZIR_GO || w < 64 ? "(%lld)" : "(%lldLL)",
                   (long long)value);
    } else if(sign)
        format(out, size, e->target == ZIR_GO || w < 64 ? "%llu" : "%lluLL",
               (unsigned long long)bits);
    else
        format(out, size, e->target == ZIR_GO || w < 64 ? "%llu" : "%lluULL",
               (unsigned long long)bits);
}

static int
all_ones_operand(const char *text)
{
    return !strcmp(text, "^uint64(0)") || !strcmp(text, "UINT64_MAX");
}

/* Whether text is one balanced parenthesized group. */
int
enclosed(const char *text)
{
    int depth = 0;
    size_t length = strlen(text);
    if(length < 2 || text[0] != '(' || text[length - 1] != ')')
        return 0;
    for(size_t i = 0; i < length; i++) {
        if(text[i] == '(') depth++;
        else if(text[i] == ')' && --depth == 0 && i + 1 < length) return 0;
    }
    return depth == 0;
}

/* A whole expression standing alone needs no outer parentheses. */
const char *
bare(const char *text, char *out, size_t size)
{
    copy_text(out, size, text);
    if(enclosed(out)) {
        memmove(out, out + 1, strlen(out));
        out[strlen(out) - 1] = '\0';
    }
    return out;
}

static int
same_number_type(const char *operand, const char *type)
{
    return operand == NULL || !strcmp(operand, "integer") ||
           !strcmp(canonical(operand), type);
}

/* A C operand in the unsigned wide type W. A result of this same wrapping
 * arithmetic at the same width is already W modulo 2^w, so it is reused
 * without converting to the signed type and back. */
static void
c_wide_operand(const char *text, const char *optype, const char *type,
               const char *native, const char *wide, char *out, size_t size)
{
    uint64_t bits;
    char inner[ZIR_TEXT_MAX];
    size_t prefix;
    int w = width(type);
    if(integer_literal_bits(text, &bits) && text[0] != '-' && text[1] != '-') {
        if(w == 64) format(out, size, "UINT64_C(%llu)", (unsigned long long)bits);
        else format(out, size, "%lluu", (unsigned long long)(bits & 0xffffffffu));
        return;
    }
    copy_text(inner, sizeof(inner), text);
    while(enclosed(inner)) {
        memmove(inner, inner + 1, strlen(inner));
        inner[strlen(inner) - 1] = '\0';
    }
    /* (T)(X) at this width: C conversions are modular, so X widens to W
     * directly. X keeps its text only when it already starts with a W cast;
     * a narrower group such as ((uint32_t)x & 65535u) would otherwise shift
     * or multiply at its own width. */
    prefix = strlen(native) + 2;
    (void)optype;
    if(inner[0] == '(' && !strncmp(inner + 1, native, strlen(native)) &&
       inner[prefix - 1] == ')' && enclosed(inner + prefix)) {
        const char *group = inner + prefix;
        size_t wide_length = strlen(wide);
        if(group[1] == '(' && !strncmp(group + 2, wide, wide_length) &&
           group[2 + wide_length] == ')')
            copy_text(out, size, group);
        else
            format(out, size, "(%s)%s", wide, group);
        return;
    }
    if(plain_identifier(text))
        format(out, size, "(%s)%s", wide, text);
    else
        format(out, size, "(%s)(%s)", wide, text);
}
/* Buffers number keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct NumberBuffers {
    char bits[ZIR_TEXT_MAX];
    char left[ZIR_TEXT_MAX];
    char right[ZIR_TEXT_MAX];
} NumberBuffers;

void number(Emitter *e, const char *type, const char *a, const char *a_type,
       const char *b, const char *b_type, int op, char *out, size_t size);

static void
number_with_buffers(Emitter *e, const char *type, const char *a, const char *a_type,
       const char *b, const char *b_type, int op, char *out, size_t size, NumberBuffers *buffers)
{
    int w = width(type), sign = signed_type(type);
    uint64_t left_bits, right_bits, folded;
    int left_literal = integer_literal_bits(a, &left_bits);
    int right_literal = all_ones_operand(b) ? (right_bits = UINT64_MAX, 1) :
                        integer_literal_bits(b, &right_bits);
    const char *native = TargetType(type, e->target);
    char float_helper[80];
    if(e->target == ZIR_GO)
        copy_text(float_helper, sizeof(float_helper), "floatToInt(");
    else
        copy_text(float_helper, sizeof(float_helper), "FloatToInt(");
    /* An integer conversion keeps the low w bits: fold literals, convert
     * other integers directly. Float operands keep the range-checked helper. */
    if(op == 0 && native != NULL && strncmp(a, float_helper, strlen(float_helper)) != 0) {
        if(left_literal)
            number_literal(e, type, left_bits, out, size);
        else if(a_type != NULL && !strcmp(canonical(a_type), type))
            copy_text(out, size, a);
        else if(e->target == ZIR_GO)
            format(out, size, "%s(%s)", native, a);
        else
            format(out, size, "(%s)(%s)", native, a);
        return;
    }
    if(op >= 1 && left_literal && right_literal &&
       fold_number(left_bits, right_bits, w, sign, op, &folded)) {
        number_literal(e, type, folded, out, size);
        return;
    }
    /* + - * and bitwise operators wrap by construction: Go's sized integers
     * wrap, and C computes them in an unsigned type no narrower than int. */
    if(native != NULL && ((op >= 1 && op <= 3) || (op >= 8 && op <= 10))) {
        static const char *const symbols[] = {"", "+", "-", "*", "", "", "", "", "&", "|", "^"};
        if(e->target == ZIR_GO) {
            if(op == 10 && all_ones_operand(b)) {
                if(same_number_type(a_type, type) && !left_literal)
                    format(out, size, "^%s", a);
                else
                    format(out, size, "^%s(%s)", native, a);
                return;
            }
            /* A literal wrapped to the width always fits the Go type. */
            if(left_literal) number_literal(e, type, left_bits, buffers->left, sizeof(buffers->left));
            else if(same_number_type(a_type, type)) copy_text(buffers->left, sizeof(buffers->left), a);
            else format(buffers->left, sizeof(buffers->left), "%s(%s)", native, a);
            if(right_literal) number_literal(e, type, right_bits, buffers->right, sizeof(buffers->right));
            else if(same_number_type(b_type, type)) copy_text(buffers->right, sizeof(buffers->right), b);
            else format(buffers->right, sizeof(buffers->right), "%s(%s)", native, b);
            if(left_literal && right_literal)
                format(buffers->left, sizeof(buffers->left), "%s(%s)", native, a);
            format(out, size, "%s %s %s", buffers->left, symbols[op], buffers->right);
            return;
        }
        const char *wide = w == 64 ? "uint64_t" : "uint32_t";
        c_wide_operand(a, a_type, type, native, wide, buffers->left, sizeof(buffers->left));
        if(op == 10 && all_ones_operand(b)) {
            format(out, size, "(%s)~%s", native, buffers->left);
            return;
        }
        c_wide_operand(b, b_type, type, native, wide, buffers->right, sizeof(buffers->right));
        format(out, size, "(%s)(%s %s %s)", native, buffers->left, symbols[op], buffers->right);
        return;
    }
    /* Division and remainder: Go's own / and % panic on zero and wrap the
     * one overflow, as Ziran does. C divides natively by a constant that is
     * neither zero nor -1. A shift by a constant below the width is a plain
     * shift, except a signed right shift in C, whose sign fill C leaves
     * to the compiler. */
    if(native != NULL && op >= 4 && op <= 7) {
        uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;
        uint64_t divisor = right_bits & mask;
        int plain = op <= 5 ?
            e->target == ZIR_GO ||
                (right_literal && divisor != 0 && !(sign && divisor == mask)) :
            right_literal && right_bits < (uint64_t)w &&
                (e->target == ZIR_GO || op == 6 || !sign);
        if(plain) {
            static const char *const symbols[] = {"", "", "", "", "/", "%", "<<", ">>"};
            if(op >= 6)
                format(buffers->right, sizeof(buffers->right), "%llu", (unsigned long long)right_bits);
            else if(right_literal)
                number_literal(e, type, right_bits, buffers->right, sizeof(buffers->right));
            else if(same_number_type(b_type, type))
                copy_text(buffers->right, sizeof(buffers->right), b);
            else
                format(buffers->right, sizeof(buffers->right), "%s(%s)", native, b);
            if(e->target == ZIR_GO) {
                if(left_literal) format(buffers->left, sizeof(buffers->left), "%s(%s)", native, a);
                else if(same_number_type(a_type, type)) copy_text(buffers->left, sizeof(buffers->left), a);
                else format(buffers->left, sizeof(buffers->left), "%s(%s)", native, a);
                format(out, size, "%s %s %s", buffers->left, symbols[op], buffers->right);
            } else if(op == 6) {
                /* Left shifts run unsigned, where C defines every result. */
                c_wide_operand(a, a_type, type, native,
                               w == 64 ? "uint64_t" : "uint32_t", buffers->left, sizeof(buffers->left));
                format(out, size, "(%s)(%s << %s)", native, buffers->left, buffers->right);
            } else {
                if(left_literal) number_literal(e, type, left_bits, buffers->left, sizeof(buffers->left));
                else if(same_number_type(a_type, type)) copy_text(buffers->left, sizeof(buffers->left), a);
                else format(buffers->left, sizeof(buffers->left), "(%s)(%s)", native, a);
                /* Narrower types promote to int and convert back; 32- and
                 * 64-bit operands already compute in their own type. */
                if(w >= 32 && (left_literal || same_number_type(a_type, type)) &&
                   (right_literal || same_number_type(b_type, type)))
                    format(out, size, "%s %s %s", buffers->left, symbols[op], buffers->right);
                else
                    format(out, size, "(%s)(%s %s %s)", native, buffers->left, symbols[op], buffers->right);
            }
            return;
        }
    }
    if(e->target == ZIR_GO) {
        go_bits_operand(a, sign, buffers->left, sizeof(buffers->left));
        go_bits_operand(b, sign, buffers->right, sizeof(buffers->right));
        if(op >= 1 && op <= 3) {
            const char *name = op == 1 ? "wrapAdd" : op == 2 ? "wrapSub" : "wrapMul";
            /* Narrowing the uint64 result below keeps the low w bits. This
             * matches the checked wrapping rule while giving Go a small
             * inlinable operation instead of the general switch helper. */
            format(buffers->bits, sizeof(buffers->bits), "%s(%s, %s)", name, buffers->left, buffers->right);
        } else {
            format(buffers->bits, sizeof(buffers->bits), "integerOp(%s, %s, %d, %s, %d)", buffers->left, buffers->right, w, sign ? "true" : "false", op);
        }
        format(out,size,"%s(%s)",TargetType(type,e->target),buffers->bits);
    } else {
        format(buffers->bits, sizeof(buffers->bits), "IntegerOp((uint64_t)(%s), (uint64_t)(%s), %d, %d, %d)", a, b, w, sign, op);
        if(sign) format(out, size, "(%s)SignedBits(%s, %d)", TargetType(type, e->target), buffers->bits, w);
        else format(out,size,"(%s)(%s)",TargetType(type,e->target),buffers->bits);
    }
}

void
number(Emitter *e, const char *type, const char *a, const char *a_type,
       const char *b, const char *b_type, int op, char *out, size_t size)
{
    static _Thread_local NumberBuffers *spares[16];
    static _Thread_local int spare_count;
    NumberBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    number_with_buffers(e, type, a, a_type, b, b_type, op, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

/* C text that starts with a prefix operator or a cast, such as *(p) or
 * (uint8_t*)(p), binds looser than a postfix subscript; wrap it first. */
void
postfix_base(const char *text, char *out, size_t size)
{
    int prefix = text[0] == '*' || text[0] == '&' || text[0] == '-' ||
                 text[0] == '!' || text[0] == '~' ||
                 (text[0] == '(' && !enclosed(text));
    if(prefix)
        format(out, size, "(%s)", text);
    else
        copy_text(out, size, text);
}

void
slice_index(Emitter *e, const char *type, const char *base, const char *index,
            char *out, size_t size)
{
    if(e->target == ZIR_GO) {
        format(out, size, "%s[%s]", base, index);
        return;
    }
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
    SliceElementType(type, element, sizeof(element));
    const char *scalar = TargetType(element, e->target);
    if(scalar != NULL)
        copy_text(mapped, sizeof(mapped), scalar);
    else
        e->resolve(e->context, element, mapped, sizeof(mapped));
    format(out, size, "((%s *)%s.data)[SliceIndex(%s, (int64_t)%s)]", mapped, base, base, index);
}

/* A Go slice needs the index for its temporary length and element access.
 * Evaluate it once so an indexed call cannot run twice. */
void
go_pointer_index(Emitter *e, const char *base, const char *index,
                 char *out, size_t size)
{
    char temporary[ZIR_NAME_MAX];
    fresh(e, temporary);
    line(e, "%s := %s", temporary, index);
    format(out, size, "unsafe.Slice(%s, int(%s)+1)[%s]",
           base, temporary, temporary);
    e->pure = 0;
}
/* Buffers emit_destination keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitDestinationBuffers {
    char pointer[ZIR_TEXT_MAX];
    char base[ZIR_TEXT_MAX];
    char wrapped[ZIR_TEXT_MAX + 2];
    char index[ZIR_TEXT_MAX];
} EmitDestinationBuffers;

void emit_destination(Emitter *e, int index, char *out, size_t size);

static void
emit_destination_with_buffers(Emitter *e, int index, char *out, size_t size, EmitDestinationBuffers *buffers)
{
    const ZirExpr *expr = &e->fn->exprs[index];
    if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "*")) {
        emit_expr(e, expr->right, e->fn->exprs[expr->right].type,
                  buffers->pointer, sizeof(buffers->pointer));
        /* Parenthesized whole, so a field access after it applies to the
         * pointed-to value, not to the pointer. */
        format(out, size, "(*(%s))", buffers->pointer);
        return;
    }
    if(expr->kind == ZIR_EXPR_IDENT) {
        resolve(e, expr->name, out, size);
        e->pure = 1;
        return;
    }
    if(expr->kind == ZIR_EXPR_MEMBER || expr->kind == ZIR_EXPR_POINTER_MEMBER) {
        if(expr->kind == ZIR_EXPR_POINTER_MEMBER &&
           !member_path(e->fn, expr->left))
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type,
                      buffers->base, sizeof(buffers->base));
        else
            emit_destination(e, expr->left, buffers->base, sizeof(buffers->base));
        emit_field_path(e->module, e->target,
                        e->fn->exprs[expr->left].type, expr->name,
                        buffers->base, out, size);
        e->pure = 1;
        return;
    }
    if(expr->kind == ZIR_EXPR_INDEX) {
        int vector = VecElementType(e->module,
            e->fn->exprs[expr->left].type, NULL, 0);
        if(!strcmp(e->fn->exprs[expr->left].type, "string"))
            fatal(expr, "string bytes are read-only");
        if(SliceElementType(e->fn->exprs[expr->left].type, NULL, 0))
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, buffers->base, sizeof(buffers->base));
        else
            emit_destination(e, expr->left, buffers->base, sizeof(buffers->base));
        {
            int base_pure = e->pure;
            emit_expr(e, expr->right, "s32", buffers->index, sizeof(buffers->index));
            e->pure = base_pure && e->pure;
        }
        if(SliceElementType(e->fn->exprs[expr->left].type, NULL, 0))
            slice_index(e, e->fn->exprs[expr->left].type, buffers->base, buffers->index, out, size);
        else if(e->target == ZIR_GO &&
                e->fn->exprs[expr->left].type[0] == '*')
            go_pointer_index(e, buffers->base, buffers->index, out, size);
        else if(vector && (e->target == ZIR_C || e->target == ZIR_CPP))
            format(out, size, "ZIRAN_VEC_INDEX((%s).data, (%s).count, %s)",
                   buffers->base, buffers->base, buffers->index);
        else if(vector)
            format(out, size, "(%s).Data[%s]", buffers->base, buffers->index);
        else {
            const char *base = buffers->wrapped;
            postfix_base(buffers->base, buffers->wrapped, sizeof(buffers->wrapped));
            if((e->target == ZIR_C || e->target == ZIR_CPP) &&
               ArrayElementType(e->fn->exprs[expr->left].type, NULL, 0, NULL))
                format(out, size, "ZIRAN_INDEX(%s, sizeof(%s) / sizeof(%s[0]), %s)",
                       base, base, base, buffers->index);
            else
                format(out, size, "%s[%s]", base, buffers->index);
        }
        return;
    }
    fatal(expr, "unsupported assignment destination");
}

void
emit_destination(Emitter *e, int index, char *out, size_t size)
{
    static _Thread_local EmitDestinationBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitDestinationBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    emit_destination_with_buffers(e, index, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

void
literal(Emitter *e, const ZirExpr *expr, const char *type, int negative, char *out, size_t size)
{
    char raw[ZIR_TEXT_MAX], *end;
    size_t length = 0;
    unsigned long long value;
    for(const char *p = expr->text; *p; p++) if(*p != '_') raw[length++] = *p;
    raw[length] = 0;
    if(raw[0] == '-') {
        negative = !negative;
        memmove(raw, raw + 1, length);
    }
    errno = 0;
    value = strtoull(raw,&end,0);
    if(end == raw || errno == ERANGE) fatal(expr,"integer literal is out of range");
    if(*end && strcmp(end,"u") && strcmp(end,"U") && strcmp(end,"l") && strcmp(end,"L") &&
       strcmp(end,"ll") && strcmp(end,"LL") && strcmp(end,"ull") && strcmp(end,"ULL")) fatal(expr,"invalid integer literal");
    int w = width(type);
    if(w) {
        uint64_t max = w == 64 ? UINT64_MAX : (UINT64_C(1)<<w)-1;
        if(signed_type(type)) max = (max>>1) + (negative ? 1 : 0);
        if(value > max || (negative && !signed_type(type) && value != 0)) fatal(expr,"integer literal does not fit its type");
    }
    if(e->target == ZIR_GO) format(out,size,"%s%llu",negative?"-":"",value);
    else if(negative && value == (UINT64_C(1)<<63)) format(out,size,"(-INT64_C(9223372036854775807)-1)");
    else format(out,size,"%s%llu%s",negative?"-":"",value,w>32?(signed_type(type)?"LL":"ULL"):"");
}

void
string_literal(const ZirExpr *expr, ZirTarget target, char *out, size_t size)
{
    size_t used = 0;
    int remaining = 0;
    unsigned int scalar = 0, minimum = 0;
    out[used++] = '"';
    const unsigned char *cursor = (const unsigned char *)expr->text + 1;
    while(*cursor && *cursor != '"') {
        unsigned int value = *cursor++;
        int unicode_escape = 0;
        if(value == '\\') {
            value = *cursor++;
            const char *escapes = "0abfnrtv\\\"";
            const unsigned char values[] = {0, 7, 8, 12, 10, 13, 9, 11, '\\', '"'};
            const char *found = value ? strchr(escapes, (int)value) : NULL;
            if(found) {
                value = values[found - escapes];
            } else if(value == 'x' || value == 'u' || value == 'U') {
                unicode_escape = value != 'x';
                int digits = value == 'x' ? 2 : value == 'u' ? 4 : 8;
                value = 0;
                for(int index = 0; index < digits; index++) {
                    int digit = *cursor++;
                    if(digit >= '0' && digit <= '9') digit -= '0';
                    else if(digit >= 'a' && digit <= 'f') digit -= 'a' - 10;
                    else if(digit >= 'A' && digit <= 'F') digit -= 'A' - 10;
                    else fatal(expr, "invalid string escape");
                    value = value * 16 + (unsigned int)digit;
                }
                if(value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                    fatal(expr, "string escape is not a Unicode scalar value");
            } else {
                fatal(expr, "unknown string escape");
            }
        }
        unsigned char bytes[4];
        int count = 1;
        bytes[0] = (unsigned char)value;
        /* Raw source bytes are already UTF-8. Escaped scalar values above
         * ASCII are encoded explicitly, independently of the C compiler. */
        if(unicode_escape && value >= 128) {
            if(value < 0x800) {
                bytes[0] = 0xc0 | (value >> 6);
                bytes[1] = 0x80 | (value & 63);
                count = 2;
            } else if(value < 0x10000) {
                bytes[0] = 0xe0 | (value >> 12);
                bytes[1] = 0x80 | ((value >> 6) & 63);
                bytes[2] = 0x80 | (value & 63);
                count = 3;
            } else {
                bytes[0] = 0xf0 | (value >> 18);
                bytes[1] = 0x80 | ((value >> 12) & 63);
                bytes[2] = 0x80 | ((value >> 6) & 63);
                bytes[3] = 0x80 | (value & 63);
                count = 4;
            }
        }
        for(int index = 0; index < count; index++) {
            unsigned char byte = bytes[index];
            if(remaining) {
                if((byte & 0xc0) != 0x80) fatal(expr, "string is not valid UTF-8");
                scalar = (scalar << 6) | (byte & 63);
                remaining--;
                if(!remaining && (scalar < minimum || scalar > 0x10ffff ||
                    (scalar >= 0xd800 && scalar <= 0xdfff)))
                    fatal(expr, "string is not valid UTF-8");
            } else if(byte >= 128) {
                if(byte >= 0xc2 && byte <= 0xdf) {
                    remaining = 1;
                    scalar = byte & 31;
                    minimum = 0x80;
                } else if(byte >= 0xe0 && byte <= 0xef) {
                    remaining = 2;
                    scalar = byte & 15;
                    minimum = 0x800;
                } else if(byte >= 0xf0 && byte <= 0xf4) {
                    remaining = 3;
                    scalar = byte & 7;
                    minimum = 0x10000;
                } else {
                    fatal(expr, "string is not valid UTF-8");
                }
            }
            if(used + 8 >= size) fatal(expr, "string literal exceeds output limit");
            /* Named escapes read like hand-written code. C keeps octal for
             * other control and non-ASCII bytes so the source stays ASCII,
             * and escapes ? so a C99 trigraph cannot form. */
            const char *named = byte == '\n' ? "\\n" : byte == '\t' ? "\\t" :
                byte == '\r' ? "\\r" : byte == '"' ? "\\\"" :
                byte == '\\' ? "\\\\" : NULL;
            if(named != NULL)
                used += (size_t)format(out + used, size - used, "%s", named);
            else if((target == ZIR_C || target == ZIR_CPP) && byte == '?')
                used += (size_t)format(out + used, size - used, "\\?");
            else if((target == ZIR_C || target == ZIR_CPP) && (byte < 32 || byte >= 127))
                used += (size_t)format(out + used, size - used, "\\%03o", byte);
            else if(byte < 32 || byte == 127)
                used += (size_t)format(out + used, size - used, "\\x%02x", byte);
            else
                out[used++] = (char)byte;
        }
    }
    if(*cursor != '"') fatal(expr, "unterminated string literal");
    if(remaining) fatal(expr, "string is not valid UTF-8");
    out[used++] = '"';
    out[used] = 0;
}
/* Buffers ScalarLiteral keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ScalarLiteralBuffers {
    ZirFunction fn;
    char value[ZIR_TEXT_MAX];
} ScalarLiteralBuffers;

int ScalarLiteral(const char *type, const char *text, ZirTarget target,
                  ZirSourceSpan span, char *out, size_t size);

static int
ScalarLiteral_with_buffers(const char *type, const char *text, ZirTarget target,
                  ZirSourceSpan span, char *out, size_t size, ScalarLiteralBuffers *buffers)
{
    memset(&buffers->fn, 0, sizeof(buffers->fn));Emitter e={0};int ok=0;
    type=canonical(type);e.target=target;
    if(!*type)return 0;
    if(!strcmp(type,"bool")) {
        if(!strcmp(text,"true") || !strcmp(text,"false")) {copy_text(out,size,text);return 1;}
        return 0;
    }
    int index=ParseExpr(&buffers->fn,NULL,text,span);
    if(index>=0) {
        ZirExpr *expr=&buffers->fn.exprs[index];
        if(expr->kind == ZIR_EXPR_STRING && !strcmp(type, "string")) {
            string_literal(expr, target, buffers->value, sizeof(buffers->value));
            if(target == ZIR_C || target == ZIR_CPP)
                format(out, size, "{%s, sizeof(%s) - 1}", buffers->value, buffers->value);
            else
                copy_text(out, size, buffers->value);
            ok = 1;
        } else if(expr->kind==ZIR_EXPR_INT && width(type)) {
            literal(&e,expr,type,0,out,size);ok=1;
        } else if(expr->kind==ZIR_EXPR_UNARY && !strcmp(expr->op,"-") &&
                  buffers->fn.exprs[expr->right].kind==ZIR_EXPR_INT && width(type)) {
            literal(&e,&buffers->fn.exprs[expr->right],type,1,out,size);ok=1;
        } else if((expr->kind==ZIR_EXPR_FLOAT || expr->kind==ZIR_EXPR_INT) && type[0]=='f') {
            copy_text(out,size,text);size_t n=strlen(out);
            if(n && (out[n-1]=='f'||out[n-1]=='F'))out[n-1]=0;
            ok=1;
        }
    }
    free(buffers->fn.exprs);return ok;
}

int
ScalarLiteral(const char *type, const char *text, ZirTarget target,
                  ZirSourceSpan span, char *out, size_t size)
{
    static _Thread_local ScalarLiteralBuffers *spares[16];
    static _Thread_local int spare_count;
    ScalarLiteralBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = ScalarLiteral_with_buffers(type, text, target, span, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
