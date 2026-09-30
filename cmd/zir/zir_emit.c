#include "zir_emit_internal.h"

static void readable_module_name(const char *name, char *out, size_t size, int upper);

static struct {
    const ZirProgram *const *programs;
    int count;
    ModuleIdentity *slots;
    size_t capacity;
    size_t used;
} module_identities;

/* Go numeric helpers, named for what they do. Each is written once per
 * package, in the first generated file that calls it. */
static const struct {
    const char *name;
    const char *definition;
} go_number_helpers[] = {
    {"signedBits", "func signedBits(x int64) uint64 { return uint64(x) }\n"},
    {"wrapAdd", "func wrapAdd(a, b uint64) uint64 { return a + b }\n"},
    {"wrapSub", "func wrapSub(a, b uint64) uint64 { return a - b }\n"},
    {"wrapMul", "func wrapMul(a, b uint64) uint64 { return a * b }\n"},
    {"floatToInt",
     "// floatToInt converts x to a w-bit integer, panicking when it does not fit.\n"
     "func floatToInt(x float64, w uint, signed bool) uint64 {\n"
     "\tbits := w\n\tif signed {\n\t\tbits--\n\t}\n"
     "\tbound := float64(uint64(1) << bits)\n"
     "\tlower := float64(0)\n\tif signed {\n\t\tlower = -bound\n\t}\n"
     "\tif !(x >= lower && x < bound) {\n\t\tpanic(\"float conversion out of range\")\n\t}\n"
     "\tif signed {\n\t\treturn uint64(int64(x))\n\t}\n\treturn uint64(x)\n}\n"},
    {"integerOp",
     "// integerOp applies op to the low w bits of a and b: 0 convert, 1 add,\n"
     "// 2 subtract, 3 multiply, 4 divide, 5 remainder, 6 shift left,\n"
     "// 7 shift right, 8 and, 9 or, 10 xor. Division by zero and\n"
     "// out-of-range shifts panic.\n"
     "func integerOp(a, b uint64, w uint, signed bool, op int) uint64 {\n"
     "\tmask := ^uint64(0)\n\tif w < 64 {\n\t\tmask = uint64(1)<<w - 1\n\t}\n"
     "\tshift := b\n\ta &= mask\n\tb &= mask\n"
     "\tswitch op {\n"
     "\tcase 0:\n\t\treturn a\n"
     "\tcase 1:\n\t\treturn (a + b) & mask\n"
     "\tcase 2:\n\t\treturn (a - b) & mask\n"
     "\tcase 3:\n\t\treturn (a * b) & mask\n"
     "\tcase 4, 5:\n"
     "\t\tif b == 0 {\n\t\t\tpanic(\"integer division by zero\")\n\t\t}\n"
     "\t\tif signed {\n"
     "\t\t\tx := int64(a<<(64-w)) >> (64 - w)\n"
     "\t\t\ty := int64(b<<(64-w)) >> (64 - w)\n"
     "\t\t\tif op == 4 {\n\t\t\t\treturn uint64(x/y) & mask\n\t\t\t}\n"
     "\t\t\treturn uint64(x%y) & mask\n\t\t}\n"
     "\t\tif op == 4 {\n\t\t\treturn a / b\n\t\t}\n\t\treturn a % b\n"
     "\tcase 6, 7:\n"
     "\t\tif shift >= uint64(w) {\n\t\t\tpanic(\"invalid shift count\")\n\t\t}\n"
     "\t\tif op == 6 {\n\t\t\treturn (a << shift) & mask\n\t\t}\n"
     "\t\tif shift == 0 {\n\t\t\treturn a\n\t\t}\n"
     "\t\tresult := a >> shift\n"
     "\t\tif signed && a&(uint64(1)<<(w-1)) != 0 {\n\t\t\tresult |= mask ^ (mask >> shift)\n\t\t}\n"
     "\t\treturn result\n"
     "\tcase 8:\n\t\treturn a & b\n"
     "\tcase 9:\n\t\treturn a | b\n"
     "\tcase 10:\n\t\treturn a ^ b\n"
     "\t}\n\tpanic(\"invalid numeric operation\")\n}\n"},
};

int
format(char *out, size_t size, const char *format_string, ...)
{
    va_list ap;
    va_start(ap,format_string);
    int n=vsnprintf(out,size,format_string,ap);
    va_end(ap);
    if(n<0 || (size_t)n>=size) {
        Diagnostic((ZirSourceSpan){0}, "zir.output",
                   "generated expression exceeds output limit");
        exit(1);
    }
    return n;
}

int
ArrayValueType(const char *type)
{
    char element[ZIR_NAME_MAX];
    /* Host char buffers retain their explicit C-string interop convention. */
    return ArrayElementType(type, element, sizeof(element), NULL) &&
           strcmp(element, "char") != 0 && strcmp(element, "const char") != 0;
}

static int
type_has_zero_array(const ZirModule *module, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    int capacity;
    if(depth > 16 || module == NULL || type == NULL) return 0;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        if(capacity < 0) {
            const char *close = strchr(type, ']');
            char bound[ZIR_NAME_MAX];
            long folded;
            size_t length = close ? (size_t)(close - type - 1) : 0;
            if(length > 0 && length < sizeof(bound)) {
                memcpy(bound, type + 1, length);
                bound[length] = '\0';
                if(EvaluateCompileExpression(module, bound, Span("", 0, 0),
                                             0, &folded) && folded == 0)
                    return 1;
            }
        }
        return capacity == 0 ||
               type_has_zero_array(module, element, depth + 1);
    }
    if(type[0] == '*') return 0;
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record != NULL && !record->is_enum && !record->is_extern &&
       !record->is_procedure_type && !record->is_record_template) {
        ZirTypeField field;
        size_t offset = 0;
        while(TypeNextField(record, &offset, &field) == 1)
            if(type_has_zero_array(owner ? owner : module, field.type,
                                   depth + 1)) return 1;
    }
    for(int i = 0; i < module->define_count; i++)
        if(!strcmp(module->defines[i].name, type))
            return type_has_zero_array(module, module->defines[i].value,
                                       depth + 1);
    return 0;
}

int
TypeHasZeroArray(const ZirModule *module, const char *type)
{
    return type_has_zero_array(module, type, 0);
}

int
ModuleUsesSlices(const ZirModule *module)
{
    for(int i = 0; i < module->global_count; i++)
        if(SliceElementType(module->globals[i].type, NULL, 0))
            return 1;
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *record = &module->types[i];
        if(record->is_enum)
            continue;
        if(record->is_procedure_type) {
            if(SliceElementType(record->procedure_return_type, NULL, 0) ||
               strstr(record->body, "[]") != NULL)
                return 1;
            continue;
        }
        size_t offset = 0;
        ZirTypeField field;
        while(TypeNextField(record, &offset, &field) > 0)
            if(SliceElementType(field.type, NULL, 0))
                return 1;
    }
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *imp = &module->imports[i];
        if(imp->kind == ZIR_IMPORT_EXTERN &&
           (SliceElementType(imp->return_type, NULL, 0) ||
            strstr(imp->args, "[]") != NULL))
            return 1;
    }
    for(int i = 0; i < module->function_count; i++) {
        const ZirFunction *fn = &module->functions[i];
        if(SliceElementType(fn->return_type, NULL, 0) || strstr(FunctionArgs(fn), "[]") != NULL)
            return 1;
        for(int j = 0; j < fn->stmt_count; j++)
            if(SliceElementType(fn->stmts[j].type, NULL, 0))
                return 1;
        for(int j = 0; j < fn->expr_count; j++)
            if(SliceElementType(fn->exprs[j].type, NULL, 0))
                return 1;
    }
    return 0;
}

int
ModuleUsesVecOperations(const ZirModule *module)
{
    for(int f = 0; f < module->function_count; f++)
        for(int e = 0; e < module->functions[f].expr_count; e++) {
            const ZirExpr *expr = &module->functions[f].exprs[e];
            if(expr->kind == ZIR_EXPR_CALL &&
               (!strcmp(expr->name, "VecPush") ||
                !strcmp(expr->name, "VecClear") ||
                !strcmp(expr->name, "VecFree") ||
                !strcmp(expr->name, "VecSwap") ||
                !strcmp(expr->name, "VecPop") ||
                !strcmp(expr->name, "VecGet") ||
                !strcmp(expr->name, "VecClone") ||
                !strcmp(expr->name, "VecSlice") ||
                !strcmp(expr->name, "BuilderAppend") ||
                !strcmp(expr->name, "BuilderFinish")))
                return 1;
            if(expr->kind == ZIR_EXPR_INDEX && expr->left >= 0 &&
               VecElementType(module,
                   module->functions[f].exprs[expr->left].type,
                   NULL, 0))
                return 1;
        }
    return 0;
}

/* Generated names must be chosen from structured bindings and expressions.
 * Saved statement text is diagnostic metadata and may differ from the graph. */
int
function_mentions(const ZirFunction *fn, const char *name)
{
    if(strstr(FunctionArgs(fn), name) != NULL)
        return 1;
    for(int i = 0; i < fn->stmt_count; i++)
        if(strcmp(fn->stmts[i].name, name) == 0)
            return 1;
    for(int i = 0; i < fn->expr_count; i++)
        if(strcmp(fn->exprs[i].name, name) == 0)
            return 1;
    return 0;
}

void
TargetBindingName(const ZirFunction *fn, ZirTarget target,
                  const char *name, char *out, size_t size)
{
    static const char *const c_keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
        "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex",
        "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
        "_Thread_local", NULL
    };
    static const char *const cpp_keywords[] = {
        "alignas", "alignof", "asm", "bool", "catch", "class", "constexpr",
        "delete", "explicit", "false", "friend", "mutable", "namespace",
        "new", "noexcept", "nullptr", "operator", "private", "protected",
        "public", "template", "this", "throw", "true", "try", "typename",
        "using", "virtual", "and", "and_eq", "bitand", "bitor",
        "char16_t", "char32_t", "compl", "const_cast", "decltype",
        "dynamic_cast", "export", "not", "not_eq", "or", "or_eq",
        "reinterpret_cast", "static_assert", "static_cast", "thread_local",
        "typeid", "wchar_t", "xor", "xor_eq", NULL
    };
    static const char *const go_keywords[] = {
        "break", "case", "chan", "const", "continue", "default", "defer",
        "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
        "interface", "map", "package", "range", "return", "select",
        "struct", "switch", "type", "var", "fmt", "strconv", "math",
        "formatFloat", NULL
    };
    const char *const *lists[] = {c_keywords, cpp_keywords, go_keywords};
    int reserved = 0;
    for(const char *const *word = lists[0]; *word; word++)
        reserved |= strcmp(name, *word) == 0;
    if(target == ZIR_CPP)
        for(const char *const *word = lists[1]; *word; word++)
            reserved |= strcmp(name, *word) == 0;
    if(target == ZIR_GO) {
        reserved = 0;
        for(const char *const *word = lists[2]; *word; word++)
            reserved |= strcmp(name, *word) == 0;
    }
    if(!reserved) {
        copy_text(out, size, name);
        return;
    }
    for(int serial = 0; ; serial++) {
        format(out, size, "ziran_keyword_%s_%d", name, serial);
        if(fn == NULL || !function_mentions(fn, out))
            return;
    }
}

void
TargetFieldName(const ZirType *record, ZirTarget target,
                const char *name, char *out, size_t size)
{
    TargetBindingName(NULL, target, name, out, size);
    if(strcmp(out, name) == 0 || record == NULL) return;
    for(int serial = 0; ; serial++) {
        size_t offset = 0;
        ZirTypeField field;
        int collision = 0;
        format(out, size, "ziran_keyword_%s_%d", name, serial);
        while(TypeNextField(record, &offset, &field) == 1)
            if(strcmp(field.name, name) != 0 &&
               strcmp(field.name, out) == 0) {
                collision = 1;
                break;
            }
        if(!collision) return;
    }
}

static void
target_top_name(const ZirModule *module, ZirTarget target, const char *name,
                int is_global, char *out, size_t size)
{
    TargetBindingName(NULL, target, name, out, size);
    if(strcmp(out, name) == 0 || module == NULL) return;
    for(int serial = 0; ; serial++) {
        int collision = 0;
        format(out, size, "ziran_keyword_%s_%d", name, serial);
        for(int i = 0; i < module->global_count; i++)
            collision |= (!is_global ||
                          strcmp(module->globals[i].name, name) != 0) &&
                         strcmp(module->globals[i].name, out) == 0;
        for(int i = 0; i < module->define_count; i++)
            collision |= (is_global ||
                          strcmp(module->defines[i].name, name) != 0) &&
                         strcmp(module->defines[i].name, out) == 0;
        for(int i = 0; i < module->type_count; i++)
            collision |= strcmp(module->types[i].name, out) == 0;
        for(int i = 0; i < module->function_count; i++)
            collision |= strcmp(module->functions[i].name, out) == 0;
        if(!collision) return;
    }
}

/* Native names of every global and constant, bucketed by hash, so each
 * procedure name is matched once instead of against every value. */
typedef struct NativeValueName {
    char *name;
    int *collision;
    int next;
} NativeValueName;

typedef struct NativeValueNames {
    NativeValueName *items;
    int count, capacity;
    int *buckets;
    int bucket_count;
} NativeValueNames;

static uint64_t
native_name_hash(const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(const unsigned char *p = (const unsigned char *)name; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    return hash;
}

static int
native_names_add(NativeValueNames *names, const char *name, int *collision)
{
    if(names->count == names->capacity) {
        int capacity = names->capacity ? names->capacity * 2 : 256;
        NativeValueName *items = realloc(names->items,
                                         (size_t)capacity * sizeof(*items));
        if(items == NULL) return 0;
        names->items = items;
        names->capacity = capacity;
    }
    char *copy = strdup(name);
    if(copy == NULL) return 0;
    size_t bucket = native_name_hash(name) % (size_t)names->bucket_count;
    names->items[names->count] = (NativeValueName){copy, collision,
                                                   names->buckets[bucket]};
    names->buckets[bucket] = names->count++;
    return 1;
}

static void
native_names_mark(const NativeValueNames *names, const char *name)
{
    size_t bucket = native_name_hash(name) % (size_t)names->bucket_count;
    for(int i = names->buckets[bucket]; i >= 0; i = names->items[i].next)
        if(!strcmp(names->items[i].name, name))
            *names->items[i].collision = 1;
}

static void
native_names_free(NativeValueNames *names)
{
    for(int i = 0; i < names->count; i++)
        free(names->items[i].name);
    free(names->items);
    free(names->buckets);
}

/* Two values of one kind or of different kinds collide when some target
 * gives them the same native name. */
static void
native_names_mark_shared(const NativeValueNames *names)
{
    for(int bucket = 0; bucket < names->bucket_count; bucket++)
        for(int i = names->buckets[bucket]; i >= 0; i = names->items[i].next)
            for(int j = names->items[i].next; j >= 0; j = names->items[j].next)
                if(names->items[i].collision != names->items[j].collision &&
                   !strcmp(names->items[i].name, names->items[j].name))
                    *names->items[i].collision = *names->items[j].collision = 1;
}

int
MarkNativeNameCollisions(ZirProgram **programs, int count)
{
    const ZirProgram *native_programs[count > 0 ? count : 1];
    NativeValueNames names[3] = {{0}, {0}, {0}}; /* C, C++, Go */
    int values = 0, ok = 1;
    for(int p = 0; p < count; p++) {
        native_programs[p] = programs[p];
        for(int m = 0; m < programs[p]->module_count; m++)
            values += programs[p]->modules[m].global_count +
                      programs[p]->modules[m].define_count;
    }
    for(int t = 0; t < 3; t++) {
        names[t].bucket_count = values * 2 + 1;
        names[t].buckets = malloc((size_t)names[t].bucket_count * sizeof(int));
        if(names[t].buckets == NULL) ok = 0;
        else memset(names[t].buckets, 0xff,
                    (size_t)names[t].bucket_count * sizeof(int));
    }
    for(int p = 0; ok && p < count; p++)
        for(int m = 0; ok && m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int v = 0; ok && v < module->global_count + module->define_count; v++) {
                int is_global = v < module->global_count;
                const char *name = is_global ? module->globals[v].name :
                    module->defines[v - module->global_count].name;
                int *collision = is_global ?
                    &module->globals[v].native_name_collision :
                    &module->defines[v - module->global_count].native_name_collision;
                for(int t = 0; ok && t < 3; t++) {
                    char native[ZIR_NAME_MAX * 2];
                    target_top_name(module, t == 0 ? ZIR_C : t == 1 ? ZIR_CPP : ZIR_GO,
                                    name, is_global, native, sizeof(native));
                    ok = native_names_add(&names[t], native, collision);
                }
            }
        }
    for(int t = 0; ok && t < 3; t++)
        native_names_mark_shared(&names[t]);
    for(int p = 0; ok && p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *function = &module->functions[f];
                char native[ZIR_NAME_MAX * 2];
                if(function->is_extern || function->is_template) continue;
                NativeCFunctionName(module, function, native, sizeof(native));
                native_names_mark(&names[0], native);
                native_names_mark(&names[1], native);
                NativeGoFunctionName(native_programs, count, module, function,
                                     native, sizeof(native));
                native_names_mark(&names[2], native);
            }
        }
    for(int t = 0; t < 3; t++)
        native_names_free(&names[t]);
    return ok;
}

static uint64_t
native_top_hash(const ZirModule *module, ZirSourceSpan span,
                const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *parts[] = {module->source_path, SpanPath(span), name};
    for(size_t part = 0; part < 3; part++) {
        for(const unsigned char *p = (const unsigned char *)parts[part];
            *p; p++)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
        hash = (hash ^ 0xffu) * UINT64_C(1099511628211);
    }
    const unsigned coordinates[] = {
        (unsigned)span.line, (unsigned)span.column
    };
    for(size_t c = 0; c < 2; c++)
        for(size_t byte = 0; byte < sizeof(coordinates[c]); byte++)
            hash = (hash ^ ((coordinates[c] >> (byte * 8)) & 0xffu)) *
                   UINT64_C(1099511628211);
    return hash;
}

void
TargetGlobalName(const ZirModule *module, ZirTarget target,
                 const char *name, char *out, size_t size)
{
    if(module != NULL)
        for(int i = 0; i < module->global_count; i++) {
            const ZirGlobal *global = &module->globals[i];
            if(strcmp(global->name, name) ||
               !global->native_name_collision) continue;
            uint64_t hash = native_top_hash(module, global->span,
                                            global->name);
            format(out, size, "zir_g_%016llx", (unsigned long long)hash);
            return;
        }
    target_top_name(module, target, name, 1, out, size);
}

void
TargetDefineName(const ZirModule *module, ZirTarget target,
                 const char *name, char *out, size_t size)
{
    if(module != NULL)
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *define = &module->defines[i];
            if(strcmp(define->name, name) ||
               !define->native_name_collision) continue;
            uint64_t hash = native_top_hash(module, define->span,
                                            define->name);
            format(out, size, "zir_d_%016llx", (unsigned long long)hash);
            return;
        }
    target_top_name(module, target, name, 0, out, size);
}

/* Negative parameter denotes the hidden array result. */
void
ArrayAbiName(const ZirFunction *fn, int parameter, char *out, size_t size)
{
    int serial = 0;
    int collision;
    do {
        format(out, size, "array_%s_%d_%d", parameter < 0 ? "result" : "input",
               parameter < 0 ? 0 : parameter, serial++);
        collision = function_mentions(fn, out);
    } while(collision);
}
/* Buffers ArrayAbiArgs keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ArrayAbiArgsBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} ArrayAbiArgsBuffers;

void ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size);

static void
ArrayAbiArgs_with_buffers(const ZirFunction *fn, char *out, size_t size, ArrayAbiArgsBuffers *buffers)
{
    if(fn->return_type[0] != '[' && strchr(FunctionArgs(fn), '[') == NULL) {
        copy_text(out, size, FunctionArgs(fn));
        return;
    }
    int count = *skip_ws(FunctionArgs(fn)) ?
        split_top_level(FunctionArgs(fn), buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
    size_t used = 0;
    out[0] = '\0';
    if(ArrayElementType(fn->return_type, NULL, 0, NULL)) {
        char name[ZIR_NAME_MAX];
        ArrayAbiName(fn, -1, name, sizeof(name));
        used += (size_t)format(out, size, "%s: %s", name, fn->return_type);
    }
    for(int i = 0; i < count; i++) {
        char *colon = strchr(buffers->parameters[i], ':');
        if(colon != NULL && ArrayValueType(skip_ws(colon + 1))) {
            char name[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, name, sizeof(name));
            used += (size_t)format(out + used, size - used, "%s%s: %s",
                                   used ? ", " : "", name, skip_ws(colon + 1));
        } else {
            used += (size_t)format(out + used, size - used, "%s%s",
                                   used ? ", " : "", buffers->parameters[i]);
        }
    }
}

void
ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size)
{
    static _Thread_local ArrayAbiArgsBuffers *spares[16];
    static _Thread_local int spare_count;
    ArrayAbiArgsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    ArrayAbiArgs_with_buffers(fn, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

const char *
canonical(const char *type)
{
    const char *result = ScalarType(type);
    if(!strcmp(type, "integer")) return "s64";
    if(!strcmp(type, "real")) return "float64";
    return *result ? result : type;
}

const char *
TargetType(const char *type, ZirTarget target)
{
    static const struct { const char *type, *c, *go; } map[] = {
        {"s8", "int8_t", "int8"}, {"s16", "int16_t", "int16"},
        {"s32", "int32_t", "int32"}, {"s64", "int64_t", "int64"},
        {"isize", "ptrdiff_t", "int"}, {"usize", "size_t", "uint"},
        {"u8", "uint8_t", "uint8"}, {"u16", "uint16_t", "uint16"},
        {"u32", "uint32_t", "uint32"}, {"u64", "uint64_t", "uint64"},
        {"float32", "float", "float32"}, {"float64", "double", "float64"},
        {"bool", "bool", "bool"}, {"void", "void", ""},
        {"char", "char", "byte"},
        {"string", "String", "string"}, {NULL, NULL, NULL}
    };
    type = canonical(type);
    if(SliceElementType(type, NULL, 0) && (target == ZIR_C || target == ZIR_CPP))
        return "Slice";
    for(int i = 0; map[i].type; i++)
        if(!strcmp(type, map[i].type)) return target == ZIR_GO ? map[i].go : map[i].c;
    return NULL;
}

void
NativeTypeName(const ZirModule *owner, const ZirType *type,
               char *out, size_t size)
{
    if(!type->native_name_mangled) {
        copy_text(out, size, type->name);
        return;
    }
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *paths[] = {owner->source_path, SpanPath(type->span)};
    for(size_t part = 0; part < 2; part++) {
        for(const unsigned char *p = (const unsigned char *)paths[part];
            *p; p++)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
        hash = (hash ^ 0xffu) * UINT64_C(1099511628211);
    }
    const unsigned coordinates[] = {
        (unsigned)type->span.line, (unsigned)type->span.column
    };
    for(size_t i = 0; i < 2; i++)
        for(size_t byte = 0; byte < sizeof(coordinates[i]); byte++)
            hash = (hash ^ ((coordinates[i] >> (byte * 8)) & 0xffu)) *
                   UINT64_C(1099511628211);
    int length = snprintf(out, size, "zir_%016llx_",
                          (unsigned long long)hash);
    if(length < 0 || (size_t)length >= size) {
        if(size) out[0] = '\0';
        return;
    }
    size_t used = (size_t)length;
    for(const unsigned char *p = (const unsigned char *)type->name;
        *p && used + 1 < size; p++)
        out[used++] = isalnum(*p) || *p == '_' ? *p : '_';
    out[used] = '\0';
}

int
NativeTypeAtUse(const ZirModule *module, const char *type,
                char *out, size_t size)
{
    const ZirModule *owner = NULL;
    const ZirType *declared = FindType(module, type, &owner);
    if(declared == NULL || owner == NULL ||
       (declared->is_extern && !declared->foreign_target[0]) ||
       (!declared->native_name_mangled && strchr(type, '.') == NULL) ||
       BuiltinType(declared->name) == declared) return 0;
    NativeTypeName(owner, declared, out, size);
    return out[0] != '\0';
}

int
NativeEnumMemberName(const ZirModule *owner, const ZirType *type,
                     const char *member, char *out, size_t size)
{
    if(!type->is_enum || !type->native_name_mangled) return 0;
    char native[ZIR_NAME_MAX * 2];
    NativeTypeName(owner, type, native, sizeof(native));
    format(out, size, "%s_%s", native, member);
    return 1;
}

void
NativeExportName(const ZirModule *module, const ZirFunction *fn,
                 char *out, size_t size)
{
    const char *symbol = fn->export_symbol[0] ? fn->export_symbol : fn->name;
    TargetBindingName(NULL, ZIR_CPP, symbol, out, size);
    if(!strcmp(out, symbol)) return;
    for(int serial = 0; ; serial++) {
        int collision = 0;
        format(out, size, "ziran_keyword_%s_%d", symbol, serial);
        for(int i = 0; i < module->global_count; i++)
            collision |= !strcmp(out, module->globals[i].name);
        for(int i = 0; i < module->define_count; i++)
            collision |= !strcmp(out, module->defines[i].name);
        for(int i = 0; i < module->type_count; i++)
            collision |= !strcmp(out, module->types[i].name);
        for(int i = 0; i < module->function_count; i++)
            if(&module->functions[i] != fn)
                collision |= !strcmp(out, module->functions[i].name) ||
                             !strcmp(out, module->functions[i].export_symbol);
        if(!collision) return;
    }
}

void
NativeCFunctionName(const ZirModule *module, const ZirFunction *fn,
                    char *out, size_t size)
{
    if(fn->exported) {
        NativeExportName(module, fn, out, size);
        return;
    }
    if(module->name[0] && strcmp(module->name, "main")) {
        char prefix[256];
        size_t used = 0;
        int needs_encoding = isdigit((unsigned char)module->name[0]);
        for(const unsigned char *p = (const unsigned char *)module->name;
            *p; p++)
            if(!isalnum(*p) && *p != '_')
                needs_encoding = 1;
        if(needs_encoding) {
            uint64_t hash = UINT64_C(14695981039346656037);
            for(const unsigned char *p = (const unsigned char *)module->name;
                *p; p++)
                hash = (hash ^ *p) * UINT64_C(1099511628211);
            format(out, size, "zir_m_%016llx_%s",
                   (unsigned long long)hash, fn->name);
            return;
        }
        for(const char *p = module->name;
            *p && used + 1 < sizeof(prefix); p++)
            prefix[used++] = *p == '.' ? '_' : *p;
        prefix[used] = '\0';
        format(out, size, "%s_%s", prefix, fn->name);
    } else
        copy_text(out, size, fn->name);
}

void
NativeCModuleInitName(const ZirModule *module, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    readable_module_name(module->name, name, sizeof(name), 0);
    format(out, size, "ziran_init_%s", name);
}

static int
module_needs_startup(const ZirModule *module, ModuleVisits *visits)
{
    if(module == NULL) return 0;
    for(size_t i = 0; i < visits->count; i++)
        if(visits->items[i] == module) return 0;
    if(visits->count == visits->capacity) {
        size_t capacity = visits->capacity ? visits->capacity * 2 : 64;
        const ZirModule **items = realloc(visits->items,
                                           capacity * sizeof(*items));
        if(items == NULL) abort();
        visits->items = items;
        visits->capacity = capacity;
    }
    visits->items[visits->count++] = module;
    for(int f = 0; f < module->function_count; f++)
        if(module->functions[f].is_global_initializer) return 1;
    for(int i = 0; i < module->import_count; i++)
        if(module_needs_startup(module->imports[i].resolved_module,
                                visits)) return 1;
    return 0;
}

int
ModuleNeedsStartup(const ZirModule *module)
{
    ModuleVisits visits = {0};
    int result = module_needs_startup(module, &visits);
    free(visits.items);
    return result;
}

static void
go_file_stem(const char *source, char *out, size_t size)
{
    const char *base = strrchr(source, '/');
    size_t length;
    base = base ? base + 1 : source;
    length = strlen(base);
    if(length > 3 && !strcmp(base + length - 3, ".zi")) length -= 3;
    if(length >= size) length = size - 1;
    memcpy(out, base, length);
    out[length] = '\0';
}

static int
same_folded_name(const char *left, const char *right)
{
    while(*left && *right) {
        if(tolower((unsigned char)*left) !=
           tolower((unsigned char)*right)) return 0;
        left++; right++;
    }
    return *left == *right;
}

static void
module_identity_uncached(const ZirProgram *const *programs, int count,
                         const ZirModule *module, char *file_stem,
                         size_t file_size, char *guard, size_t guard_size)
{
    go_file_stem(module->source_path, file_stem, file_size);
    camel_ident(file_stem, guard, guard_size);
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *other = &programs[p]->modules[m];
            char other_stem[ZIR_PATH_MAX], other_guard[256];
            if(other == module) continue;
            go_file_stem(other->source_path, other_stem,
                         sizeof(other_stem));
            camel_ident(other_stem, other_guard, sizeof(other_guard));
            if(!same_folded_name(file_stem, other_stem) &&
               strcmp(guard, other_guard)) continue;
            uint64_t hash = UINT64_C(14695981039346656037);
            for(const unsigned char *cursor =
                    (const unsigned char *)module->source_path;
                *cursor; cursor++)
                hash = (hash ^ *cursor) * UINT64_C(1099511628211);
            format(file_stem, file_size, "zir_%016llx",
                   (unsigned long long)hash);
            format(guard, guard_size, "Zir_%016llx",
                   (unsigned long long)hash);
            return;
        }
}

static size_t
module_identity_slot(const ZirModule *module, size_t capacity)
{
    uintptr_t key = (uintptr_t)module;
    size_t slot = (size_t)((key >> 4) * UINT64_C(11400714819323198485)) &
                  (capacity - 1);
    while(module_identities.slots[slot].module != NULL &&
          module_identities.slots[slot].module != module)
        slot = (slot + 1) & (capacity - 1);
    return slot;
}

static void
reset_module_identities(const ZirProgram *const *programs, int count)
{
    size_t modules = 0;
    for(int p = 0; p < count; p++)
        modules += (size_t)programs[p]->module_count;
    size_t capacity = 64;
    while(capacity < modules * 2)
        capacity *= 2;
    free(module_identities.slots);
    module_identities.slots = calloc(capacity, sizeof(ModuleIdentity));
    if(module_identities.slots == NULL) abort();
    module_identities.capacity = capacity;
    module_identities.used = 0;
    module_identities.programs = programs;
    module_identities.count = count;
}

void
NativeGoModuleIdentity(const ZirProgram *const *programs, int count,
                       const ZirModule *module, char *file_stem,
                       size_t file_size, char *guard, size_t guard_size)
{
    if(module_identities.slots == NULL ||
       module_identities.programs != programs ||
       module_identities.count != count ||
       (module_identities.used + 1) * 2 > module_identities.capacity)
        reset_module_identities(programs, count);
    size_t slot = module_identity_slot(module, module_identities.capacity);
    ModuleIdentity *entry = &module_identities.slots[slot];
    if(entry->module == NULL) {
        module_identity_uncached(programs, count, module,
                                 entry->file_stem, sizeof(entry->file_stem),
                                 entry->guard, sizeof(entry->guard));
        entry->module = module;
        module_identities.used++;
    }
    copy_text(file_stem, file_size, entry->file_stem);
    copy_text(guard, guard_size, entry->guard);
}

void
NativeGoFunctionName(const ZirProgram *const *programs, int count,
                     const ZirModule *module, const ZirFunction *fn,
                     char *out, size_t size)
{
    char stem[ZIR_PATH_MAX], guard[256], name[256];
    NativeGoModuleIdentity(programs, count, module, stem, sizeof(stem),
                           guard, sizeof(guard));
    camel_ident(fn->name, name, sizeof(name));
    format(out, size, "%s_%s", guard, name);
}

/* A module name as an identifier fragment: hello, site_examples_hello. */
static void
readable_module_name(const char *name, char *out, size_t size, int upper)
{
    size_t used = 0;
    for(const unsigned char *p = (const unsigned char *)name; *p && used + 1 < size; p++)
        out[used++] = isalnum(*p) ? (char)(upper ? toupper(*p) : *p) : '_';
    out[used] = '\0';
}

/* The generated header's include guard, named after its path: ZIRAN_HELLO_H.
 * A capital letter gets a leading underscore, so foo and Foo differ. */
void
NativeHeaderGuard(const char *stem, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    size_t used = 0;
    for(const unsigned char *p = (const unsigned char *)stem; *p && used + 2 < sizeof(name); p++) {
        if(isupper(*p))
            name[used++] = '_';
        name[used++] = isalnum(*p) ? (char)toupper(*p) : '_';
    }
    name[used] = '\0';
    if((size_t)format(out, size, "ZIRAN_%s_H", name) >= size) {
        Diagnostic((ZirSourceSpan){0}, "zir.output",
                   "generated header guard exceeds output limit");
        exit(1);
    }
}

void
slot_native_type(const char *source, ZirTarget target, char *out, size_t size)
{
    if(*source == '*') {
        /* Callback parameters are abstract C declarators. A pointer to an
         * array must keep its parentheses and element stride: T (*)[N],
         * rather than the invalid [N]T* or an array of pointers. */
        const char *array = source;
        int pointers = 0;
        while(*array == '*') {
            pointers++;
            array = skip_ws(array + 1);
        }
        if((target == ZIR_C || target == ZIR_CPP) && *array == '[') {
            char dimensions[ZIR_NAME_MAX], element[ZIR_NAME_MAX];
            const char *end = array;
            while(*end == '[') {
                const char *close = strchr(end, ']');
                if(close == NULL)
                    break;
                end = skip_ws(close + 1);
            }
            size_t length = (size_t)(end - array);
            if(length >= sizeof(dimensions))
                length = sizeof(dimensions) - 1;
            memcpy(dimensions, array, length);
            dimensions[length] = '\0';
            slot_native_type(end, target, element, sizeof(element));
            format(out, size, "%s (%.*s)%s", element, pointers,
                   "****************************************************************", dimensions);
            return;
        }
        char pointee[ZIR_NAME_MAX];
        slot_native_type(skip_ws(source + 1), target, pointee, sizeof(pointee));
        format(out, size, "%s*", pointee);
        return;
    }
    const char *scalar = TargetType(source, target);
    copy_text(out, size, scalar ? scalar : source);
}
/* Buffers EmitSlotType keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitSlotTypeBuffers {
    char parameters[64][ZIR_TEXT_MAX];
} EmitSlotTypeBuffers;

void EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context);

static void
EmitSlotType_with_buffers(FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context, EmitSlotTypeBuffers *buffers)
{
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
    if(slot->is_c_call) {
        char result[ZIR_NAME_MAX];
        slot_native_type(slot->procedure_return_type, target, result,
                         sizeof(result));
        fprintf(out, "typedef %s (*%s)(", result, slot->name);
        for(int i = 0; i < count; i++) {
            const char *source = skip_ws(strchr(buffers->parameters[i], ':') + 1);
            char type[ZIR_NAME_MAX];
            slot_native_type(source, target, type, sizeof(type));
            fprintf(out, "%s%s", i ? ", " : "", type);
        }
        fprintf(out, "%s);\n", count ? "" : "void");
        return;
    }
    const char *result_source = slot->procedure_return_type;
    char result_type[ZIR_NAME_MAX];
    if(target == ZIR_GO) {
        const char *result_scalar = TargetType(result_source, target);
        copy_text(result_type, sizeof(result_type),
                  result_scalar ? result_scalar : result_source);
    } else {
        slot_native_type(result_source, target, result_type,
                         sizeof(result_type));
    }
    if(resolve_type && strcmp(result_source, "void"))
        resolve_type(context, result_source, result_type, sizeof(result_type));
    if(target == ZIR_GO)
        fprintf(out, "type %s func(", slot->name);
    else
        fprintf(out, "typedef struct %s {\n    void *context;\n    %s (*call)(void *", slot->name, result_type);
    for(int i = 0; i < count; i++) {
        char *colon = strchr(buffers->parameters[i], ':');
        char type[ZIR_NAME_MAX];
        const char *source = skip_ws(colon + 1);
        if(target == ZIR_GO) {
            const char *scalar = TargetType(source, target);
            copy_text(type, sizeof(type), scalar ? scalar : source);
        } else {
            slot_native_type(source, target, type, sizeof(type));
        }
        if(resolve_type)
            resolve_type(context, source, type, sizeof(type));
        fprintf(out, "%s%s", i || target != ZIR_GO ? ", " : "", type);
    }
    if(target == ZIR_GO)
        fprintf(out, ")%s%s\n\n", result_type[0] ? " " : "", result_type);
    else
        fprintf(out, ");\n} %s;\n", slot->name);
}

void
EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context)
{
    static _Thread_local EmitSlotTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitSlotTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    EmitSlotType_with_buffers(out, slot, target, resolve_type, context, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

int width(const char *type) { return (*type == 's' || *type == 'u') ? atoi(type + 1) : 0; }
int signed_type(const char *type) { return *type == 's'; }

int
enum_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_enum;
}

int
enum_flags_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_enum_flags;
}

int
record_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && !declared->is_enum && !declared->is_procedure_type;
}

const ZirType *
field_record(const ZirModule *module, const char *type)
{
    const char *base = skip_ws(type);
    while(*base == '*') base = skip_ws(base + 1);
    return FindType(module, base, NULL);
}
/* Buffers emit_field_path keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitFieldPathBuffers {
    char result[ZIR_TEXT_MAX];
    char next[ZIR_TEXT_MAX];
} EmitFieldPathBuffers;

void emit_field_path(const ZirModule *module, ZirTarget target,
                const char *base_type, const char *path,
                const char *base_expression, char *out, size_t size);

static void
emit_field_path_with_buffers(const ZirModule *module, ZirTarget target,
                const char *base_type, const char *path,
                const char *base_expression, char *out, size_t size, EmitFieldPathBuffers *buffers)
{
    char current_type[ZIR_NAME_MAX];
    copy_text(buffers->result, sizeof(buffers->result), base_expression);
    copy_text(current_type, sizeof(current_type), base_type);
    for(const char *part = path; *part;) {
        const char *dot = strchr(part, '.');
        size_t length = dot == NULL ? strlen(part) : (size_t)(dot - part);
        char name[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
        if(length == 0 || length >= sizeof(name)) {
            Diagnostic(module->span, "emit.expression",
                       "invalid checked record field path");
            exit(1);
        }
        memcpy(name, part, length);
        name[length] = '\0';
        const ZirType *record = field_record(module, current_type);
        if(target == ZIR_GO && record != NULL && record->is_union) {
            /* Unions keep a byte backing; every Go access reinterprets it
             * through the field's declared type. */
            const char *field_type = NULL;
            size_t find = 0;
            ZirTypeField found;
            while(TypeNextField(record, &find, &found) == 1)
                if(strcmp(found.name, name) == 0) {
                    field_type = found.type;
                    break;
                }
            if(field_type != NULL) {
                const ZirType *enumeration = FindType(module, field_type, NULL);
                const char *backing = enumeration != NULL &&
                                      enumeration->is_enum ?
                    enumeration->enum_backing : field_type;
                const char *go_type = TargetType(backing, ZIR_GO);
                if(go_type == NULL && enumeration != NULL &&
                   enumeration->is_enum)
                    go_type = field_type;
                if(go_type != NULL) {
                    format(buffers->next, sizeof(buffers->next),
                           "*(*%s)(unsafe.Pointer(&%s.data[0]))", go_type,
                           buffers->result);
                    copy_text(buffers->result, sizeof(buffers->result), buffers->next);
                    if(dot == NULL) break;
                    part = dot + 1;
                    continue;
                }
            }
        }
        if(target == ZIR_GO)
            go_field_ident(name, mapped, sizeof(mapped));
        else
            TargetFieldName(record, target, name, mapped, sizeof(mapped));
        const char *base = skip_ws(current_type);
        /* A name or member chain binds tighter than . and -> already. */
        int chain = buffers->result[0] != '\0' && !isdigit((unsigned char)buffers->result[0]);
        for(const char *c = buffers->result; *c && chain; c++)
            chain = is_ident_char((unsigned char)*c) || *c == '.' ||
                    (c[0] == '-' && c[1] == '>') || (c[0] == '>' && c > buffers->result && c[-1] == '-');
        format(buffers->next, sizeof(buffers->next), chain ? "%s%s%s" : "(%s)%s%s", buffers->result,
               target != ZIR_GO && *base == '*' ? "->" : ".", mapped);
        copy_text(buffers->result, sizeof(buffers->result), buffers->next);
        if(dot == NULL) break;
        size_t offset = 0;
        ZirTypeField field;
        int found = 0;
        while(record != NULL && TypeNextField(record, &offset, &field) == 1)
            if(strcmp(field.name, name) == 0) {
                copy_text(current_type, sizeof(current_type), field.type);
                found = 1;
                break;
            }
        if(!found) {
            Diagnostic(module->span, "emit.expression",
                       "invalid checked record field path");
            exit(1);
        }
        part = dot + 1;
    }
    copy_text(out, size, buffers->result);
}

void
emit_field_path(const ZirModule *module, ZirTarget target,
                const char *base_type, const char *path,
                const char *base_expression, char *out, size_t size)
{
    static _Thread_local EmitFieldPathBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitFieldPathBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    emit_field_path_with_buffers(module, target, base_type, path, base_expression, out, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

void
EmitStringType(FILE *out)
{
    fputs("#include \"zir_string.h\"\n", out);
}

const char *
zero_value(const char *type, ZirTarget target)
{
    if(SliceElementType(type, NULL, 0))
        return target == ZIR_GO ? "nil" : "{0}";
    if(type[0] == '[')
        return "";
    if(type[0] != '[' && strchr(type, '*') != NULL)
        return target == ZIR_GO ? "nil" :
               target == ZIR_CPP ? "nullptr" : "((void *)0)";
    if(!strcmp(canonical(type), "string"))
        return target == ZIR_C || target == ZIR_CPP ? "StringView(NULL, 0)" : "\"\"";
    if(!strcmp(canonical(type), "bool")) return "false";
    return "0";
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
    ZirTypeField field;
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
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        if(!portable_type_path(owner, field.type, &current) || !strcmp(field.type, "void"))
            return 0;
        fields++;
    }
    return status == 0 && fields > 0;
}

static int
portable_type_path(const ZirModule *module, const char *type, const TypePath *path)
{
    static _Thread_local PortableTypePathBuffers *spares[16];
    static _Thread_local int spare_count;
    PortableTypePathBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = portable_type_path_with_buffers(module, type, path, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
portable_type(const ZirModule *module, const char *type)
{
    return portable_type_path(module, type, NULL);
}

static int
supported_expression(const ZirModule *module, const ZirFunction *fn, int index)
{
    const ZirExpr *e;
    if(index < 0) return 1;
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
    if(!supported_expression(module, fn, e->left) || !supported_expression(module, fn, e->right) ||
       !supported_expression(module, fn, e->third)) return 0;
    for(int child = e->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(!supported_expression(module, fn, child)) return 0;
    return 1;
}
/* Buffers CanEmitBody keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CanEmitBodyBuffers {
    char params[64][ZIR_TEXT_MAX];
} CanEmitBodyBuffers;

int CanEmitBody(const ZirModule *module, const ZirFunction *fn);

static int
CanEmitBody_with_buffers(const ZirModule *module, const ZirFunction *fn, CanEmitBodyBuffers *buffers)
{
    int count;
    int result;
    /* Eligibility follows the typed function body and its operations.
     * Host calls and unsupported composition fail the same checks. */
    if(!fn->checked || fn->is_extern || !portable_type(module, fn->return_type)) return 0;
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
        if(!supported_expression(module, fn, st->expr_root) || !supported_expression(module, fn, st->lhs_root)) return 0;
    }
    return 1;
}

int
CanEmitBody(const ZirModule *module, const ZirFunction *fn)
{
    static _Thread_local CanEmitBodyBuffers *spares[16];
    static _Thread_local int spare_count;
    CanEmitBodyBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = CanEmitBody_with_buffers(module, fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

void
number_prefix(const ZirModule *module, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    readable_module_name(module->name, name, sizeof(name), 0);
    format(out, size, "number_%s", name);
}

/* C and C++ sources mark where numeric helpers go. Once the module is
 * written, EmitResolveNumberHelpers puts in the ones its code calls. */
void
EmitNumbers(FILE *out, const ZirModule *module, ZirTarget target)
{
    (void)module;
    if(target == ZIR_C || target == ZIR_CPP)
        fputs(NUMBER_HELPERS_MARK, out);
}

/* Go `print` float text, once per package: nan and inf spelled as on every
 * target, then the shortest decimal that reads back to the same value. */
void
EmitGoPrintSupport(FILE *out)
{
    fputs("func formatFloat(value float64, bits int) string {\n"
          "\tswitch {\n"
          "\tcase math.IsNaN(value):\n"
          "\t\treturn \"nan\"\n"
          "\tcase math.IsInf(value, 1):\n"
          "\t\treturn \"inf\"\n"
          "\tcase math.IsInf(value, -1):\n"
          "\t\treturn \"-inf\"\n"
          "\t}\n"
          "\treturn strconv.FormatFloat(value, 'f', -1, bits)\n"
          "}\n\n", out);
}

/* An exported void main is the C program entry: it returns status 0. */
int
NativeMainReturnsStatus(const ZirFunction *fn)
{
    return fn->exported && !strcmp(fn->name, "main") &&
           (!fn->return_type[0] || !strcmp(fn->return_type, "void"));
}

/* C numeric helpers, named for what they do. IntegerOp reads signed
 * operands through SignedBits, so SignedBits comes first. */
static const struct {
    const char *name;
    const char *definition;
} c_number_helpers[] = {
    {"SignedBits",
     "/* SignedBits reads the low w bits of x as a signed number. */\n"
     "static inline int64_t SignedBits(uint64_t x, int w) {\n"
     "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
     "    x &= mask;\n"
     "    return x <= (mask >> 1) ? (int64_t)x : -1 - (int64_t)(mask - x);\n"
     "}\n"},
    {"IntegerOp",
     "/* IntegerOp applies op to the low w bits of a and b: 0 convert, 1 add,\n"
     " * 2 subtract, 3 multiply, 4 divide, 5 remainder, 6 shift left,\n"
     " * 7 shift right, 8 and, 9 or, 10 xor. Division by zero and\n"
     " * out-of-range shifts abort. */\n"
     "static inline uint64_t IntegerOp(uint64_t a, uint64_t b, int w, int sign, int op) {\n"
     "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
     "    uint64_t shift = b;\n"
     "    a &= mask;\n"
     "    b &= mask;\n"
     "    switch(op) {\n"
     "    case 0: return a;\n"
     "    case 1: return (a + b) & mask;\n"
     "    case 2: return (a - b) & mask;\n"
     "    case 3: return (a * b) & mask;\n"
     "    case 4: case 5:\n"
     "        if(!b) abort();\n"
     "        if(sign) {\n"
     "            int64_t x = SignedBits(a, w), y = SignedBits(b, w);\n"
     "            if(x == INT64_MIN && y == -1) return op == 4 ? a : 0;\n"
     "            return (uint64_t)(op == 4 ? x / y : x % y) & mask;\n"
     "        }\n"
     "        return op == 4 ? a / b : a % b;\n"
     "    case 6: case 7:\n"
     "        if(shift >= (uint64_t)w) abort();\n"
     "        if(op == 6) return (a << shift) & mask;\n"
     "        if(!shift) return a;\n"
     "        return (a >> shift) |\n"
     "            ((sign && (a & (UINT64_C(1) << (w - 1)))) ? mask ^ (mask >> shift) : 0);\n"
     "    case 8: return a & b;\n"
     "    case 9: return a | b;\n"
     "    case 10: return a ^ b;\n"
     "    default: abort();\n"
     "    }\n"
     "    return 0;\n"
     "}\n"},
    {"FloatToInt",
     "/* FloatToInt converts x to a w-bit integer, aborting when it does not fit. */\n"
     "static inline uint64_t FloatToInt(double x, int w, int sign) {\n"
     "    double bound = 1;\n"
     "    int i;\n"
     "    for(i = 0; i < w - sign; i++) bound *= 2;\n"
     "    if(!(x >= (sign ? -bound : 0) && x < bound)) abort();\n"
     "    return sign ? (uint64_t)(int64_t)x : (uint64_t)x;\n"
     "}\n"},
};

/* Replaces the helper mark in a written C or C++ source with the numeric
 * helpers its code calls, or with nothing. Returns 0 on success. */
int
EmitResolveNumberHelpers(const char *path)
{
    FILE *file = fopen(path, "rb");
    char *text = NULL, *mark;
    long length;
    int needed[sizeof(c_number_helpers) / sizeof(c_number_helpers[0])] = {0};
    int any = 0, status = 0;
    if(file == NULL)
        return -1;
    if(fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
       fseek(file, 0, SEEK_SET) != 0 || (text = malloc((size_t)length + 1)) == NULL ||
       fread(text, 1, (size_t)length, file) != (size_t)length) {
        fclose(file);
        free(text);
        return -1;
    }
    fclose(file);
    text[length] = '\0';
    mark = strstr(text, NUMBER_HELPERS_MARK);
    if(mark == NULL) {
        free(text);
        return 0;
    }
    for(size_t i = 0; i < sizeof(needed) / sizeof(needed[0]); i++) {
        char call[32];
        format(call, sizeof(call), "%s(", c_number_helpers[i].name);
        needed[i] = strstr(text, call) != NULL;
    }
    needed[0] |= needed[1];
    for(size_t i = 0; i < sizeof(needed) / sizeof(needed[0]); i++)
        any |= needed[i];
    file = fopen(path, "wb");
    if(file == NULL) {
        free(text);
        return -1;
    }
    fwrite(text, 1, (size_t)(mark - text), file);
    if(any)
        fputs("#include <stdint.h>\n#include <stdlib.h>\n\n", file);
    for(size_t i = 0, written = 0; i < sizeof(needed) / sizeof(needed[0]); i++)
        if(needed[i])
            fprintf(file, "%s%s", written++ ? "\n" : "", c_number_helpers[i].definition);
    fputs(mark + strlen(NUMBER_HELPERS_MARK), file);
    status = ferror(file) ? -1 : 0;
    if(fclose(file) != 0)
        status = -1;
    free(text);
    return status;
}

void
EmitGoNumberHelpers(FILE *out, const char *text, unsigned *written)
{
    for(size_t i = 0; i < sizeof(go_number_helpers) / sizeof(go_number_helpers[0]); i++) {
        char call[32];
        if(*written & (1u << i))
            continue;
        format(call, sizeof(call), "%s(", go_number_helpers[i].name);
        if(strstr(text, call) == NULL)
            continue;
        fprintf(out, "%s\n", go_number_helpers[i].definition);
        *written |= 1u << i;
    }
}

int
emitter_type_contains_vec(const ZirModule *module, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirType *record = NULL;
    if(depth > 32 || module == NULL || type == NULL || !*type || *type == '*')
        return 0;
    if(VecElementType(module, type, NULL, 0))
        return 1;
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return emitter_type_contains_vec(module, element, depth + 1);
    record = FindType(module, type, &owner);
    if(record == NULL || record->is_enum || record->is_procedure_type ||
       record->is_record_template || record->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1)
        if(emitter_type_contains_vec(owner ? owner : module,
                                     field.type, depth + 1))
            return 1;
    return 0;
}
