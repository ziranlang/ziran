#include "zir_emit.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_text.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int
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
            int64_t folded;
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
        if(SliceElementType(fn->return_type, NULL, 0) || strstr(fn->args, "[]") != NULL)
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
static int
function_mentions(const ZirFunction *fn, const char *name)
{
    if(strstr(fn->args, name) != NULL)
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

static int
native_top_name_conflict(const ZirModule *left, const char *left_source,
                         const ZirModule *right, const char *right_source,
                         int left_global, int right_global)
{
    for(int target = ZIR_C; target <= ZIR_GO; target++) {
        char left_name[ZIR_NAME_MAX * 2], right_name[ZIR_NAME_MAX * 2];
        target_top_name(left, (ZirTarget)target, left_source,
                        left_global, left_name, sizeof(left_name));
        target_top_name(right, (ZirTarget)target, right_source,
                        right_global, right_name, sizeof(right_name));
        if(!strcmp(left_name, right_name)) return 1;
    }
    return 0;
}

int
NativeGlobalNameConflict(const ZirModule *left, const ZirGlobal *a,
                         const ZirModule *right, const ZirGlobal *b)
{
    return native_top_name_conflict(left, a->name, right, b->name, 1, 1);
}

int
NativeDefineNameConflict(const ZirModule *left, const ZirDefine *a,
                         const ZirModule *right, const ZirDefine *b)
{
    return native_top_name_conflict(left, a->name, right, b->name, 0, 0);
}

int
NativeValueNameConflict(const ZirModule *global_module, const ZirGlobal *global,
                        const ZirModule *define_module, const ZirDefine *define)
{
    return native_top_name_conflict(global_module, global->name,
                                    define_module, define->name, 1, 0);
}

int
NativeFunctionValueNameConflict(const ZirProgram *const *programs, int count,
                                const ZirModule *function_module,
                                const ZirFunction *function,
                                const ZirModule *value_module,
                                const char *value_name, int is_global)
{
    char function_name[ZIR_NAME_MAX * 2];
    char value_native[ZIR_NAME_MAX * 2];
    NativeCFunctionName(function_module, function, function_name,
                        sizeof(function_name));
    for(int target = ZIR_C; target <= ZIR_CPP; target++) {
        target_top_name(value_module, (ZirTarget)target, value_name,
                        is_global, value_native, sizeof(value_native));
        if(!strcmp(function_name, value_native)) return 1;
    }
    NativeGoFunctionName(programs, count, function_module, function,
                         function_name, sizeof(function_name));
    target_top_name(value_module, ZIR_GO, value_name, is_global,
                    value_native, sizeof(value_native));
    return !strcmp(function_name, value_native);
}

static uint64_t
native_top_hash(const ZirModule *module, ZirSourceSpan span,
                const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *parts[] = {module->source_path, span.path, name};
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

void
ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size)
{
    if(fn->return_type[0] != '[' && strchr(fn->args, '[') == NULL) {
        copy_text(out, size, fn->args);
        return;
    }
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(fn->args) ?
        split_top_level(fn->args, parameters[0], 64, sizeof(parameters[0])) : 0;
    size_t used = 0;
    out[0] = '\0';
    if(ArrayElementType(fn->return_type, NULL, 0, NULL)) {
        char name[ZIR_NAME_MAX];
        ArrayAbiName(fn, -1, name, sizeof(name));
        used += (size_t)format(out, size, "%s: %s", name, fn->return_type);
    }
    for(int i = 0; i < count; i++) {
        char *colon = strchr(parameters[i], ':');
        if(colon != NULL && ArrayValueType(skip_ws(colon + 1))) {
            char name[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, name, sizeof(name));
            used += (size_t)format(out + used, size - used, "%s%s: %s",
                                   used ? ", " : "", name, skip_ws(colon + 1));
        } else {
            used += (size_t)format(out + used, size - used, "%s%s",
                                   used ? ", " : "", parameters[i]);
        }
    }
}

static const char *
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
    const char *paths[] = {owner->source_path, type->span.path};
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
    if(declared == NULL || owner == NULL || declared->is_extern ||
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

static void readable_module_name(const char *name, char *out, size_t size, int upper);

void
NativeCModuleInitName(const ZirModule *module, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    readable_module_name(module->name, name, sizeof(name), 0);
    format(out, size, "ziran_init_%s", name);
}

typedef struct ModuleVisits {
    const ZirModule **items;
    size_t count;
    size_t capacity;
} ModuleVisits;

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

void
NativeGoModuleIdentity(const ZirProgram *const *programs, int count,
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

static void
slot_native_type(const char *source, ZirTarget target, char *out, size_t size)
{
    if(*source == '*') {
        char pointee[ZIR_NAME_MAX];
        slot_native_type(skip_ws(source + 1), target, pointee, sizeof(pointee));
        format(out, size, "%s*", pointee);
        return;
    }
    const char *scalar = TargetType(source, target);
    copy_text(out, size, scalar ? scalar : source);
}

void
EmitSlotType(FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context)
{
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, parameters[0], 64, sizeof(parameters[0])) : 0;
    if(slot->is_c_call) {
        char result[ZIR_NAME_MAX];
        slot_native_type(slot->procedure_return_type, target, result,
                         sizeof(result));
        fprintf(out, "typedef %s (*%s)(", result, slot->name);
        for(int i = 0; i < count; i++) {
            const char *source = skip_ws(strchr(parameters[i], ':') + 1);
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
        char *colon = strchr(parameters[i], ':');
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

static int width(const char *type) { return (*type == 's' || *type == 'u') ? atoi(type + 1) : 0; }
static int signed_type(const char *type) { return *type == 's'; }

static int
enum_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_enum;
}

static int
enum_flags_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && declared->is_enum_flags;
}

static int
record_type(const ZirModule *module, const char *type)
{
    const ZirType *declared = FindType(module, type, NULL);
    return declared != NULL && !declared->is_enum && !declared->is_procedure_type;
}

static const ZirType *
field_record(const ZirModule *module, const char *type)
{
    const char *base = skip_ws(type);
    while(*base == '*') base = skip_ws(base + 1);
    return FindType(module, base, NULL);
}

static void
emit_field_path(const ZirModule *module, ZirTarget target,
                const char *base_type, const char *path,
                const char *base_expression, char *out, size_t size)
{
    char result[ZIR_TEXT_MAX], current_type[ZIR_NAME_MAX];
    copy_text(result, sizeof(result), base_expression);
    copy_text(current_type, sizeof(current_type), base_type);
    for(const char *part = path; *part;) {
        const char *dot = strchr(part, '.');
        size_t length = dot == NULL ? strlen(part) : (size_t)(dot - part);
        char name[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX], next[ZIR_TEXT_MAX];
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
                    format(next, sizeof(next),
                           "*(*%s)(unsafe.Pointer(&%s.data[0]))", go_type,
                           result);
                    copy_text(result, sizeof(result), next);
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
        int chain = result[0] != '\0' && !isdigit((unsigned char)result[0]);
        for(const char *c = result; *c && chain; c++)
            chain = is_ident_char((unsigned char)*c) || *c == '.' ||
                    (c[0] == '-' && c[1] == '>') || (c[0] == '>' && c > result && c[-1] == '-');
        format(next, sizeof(next), chain ? "%s%s%s" : "(%s)%s%s", result,
               target != ZIR_GO && *base == '*' ? "->" : ".", mapped);
        copy_text(result, sizeof(result), next);
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
    copy_text(out, size, result);
}

void
EmitStringType(FILE *out)
{
    fputs("#include \"zir_string.h\"\n", out);
}

static const char *
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

typedef struct TypePath {
    const ZirType *record;
    const struct TypePath *parent;
} TypePath;

static int
portable_type_path(const ZirModule *module, const char *type, const TypePath *path)
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
    if(record->is_enum)
        return 1;
    if(record->is_procedure_type) {
        if(record->is_c_call)
            return 1; /* Native callback; the VM and Go target reject it. */
        char parameters[64][ZIR_TEXT_MAX];
        int count = *skip_ws(record->body) ?
            split_top_level(record->body, parameters[0], 64, sizeof(parameters[0])) : 0;
        for(int i = 0; i < count; i++) {
            char *colon = strchr(parameters[i], ':');
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
    case ZIR_EXPR_CAST:
        if(e->name[0] != '*' && !TargetType(e->name, ZIR_C) &&
           !enum_type(module, e->name)) return 0;
        break;
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

int
CanEmitBody(const ZirModule *module, const ZirFunction *fn)
{
    char params[64][ZIR_TEXT_MAX];
    int count;
    int result;
    /* Eligibility follows the typed function body and its operations.
     * Host calls and unsupported composition fail the same checks. */
    if(!fn->checked || fn->is_extern || !portable_type(module, fn->return_type)) return 0;
    count = *skip_ws(fn->args) ? split_top_level(fn->args, params[0], 64, sizeof(params[0])) : 0;
    for(int i = 0; i < count; i++) {
        char *colon = strchr(params[i], ':');
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

static void
number_prefix(const ZirModule *module, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    readable_module_name(module->name, name, sizeof(name), 0);
    format(out, size, "number_%s", name);
}

static int operation(const char *op);

void
EmitNumbers(FILE *out, const ZirModule *module, ZirTarget target)
{
    char p[64];
    int used = 0;
    for(int i = 0; i < module->function_count && !used; i++) {
        const ZirFunction *fn = &module->functions[i];
        if(!CanEmitBody(module, fn))
            continue;
        /* + - * and bitwise operators and integer conversions lower
         * directly; division, remainder, shifts, and float-to-integer
         * conversions keep the checked helpers. */
        for(int e = 0; e < fn->expr_count; e++) {
            const ZirExpr *expr = &fn->exprs[e];
            int op = expr->kind == ZIR_EXPR_BINARY ? operation(expr->op) : 0;
            if((op >= 4 && op <= 7) ||
               (expr->kind == ZIR_EXPR_CAST && expr->right >= 0 &&
                canonical(fn->exprs[expr->right].type)[0] == 'f')) {
                used = 1;
                break;
            }
        }
        for(int s = 0; s < fn->stmt_count; s++) {
            char op[4];
            size_t length;
            copy_text(op, sizeof(op), fn->stmts[s].assignment_op);
            length = strlen(op);
            if(length > 0 && op[length - 1] == '=')
                op[length - 1] = '\0';
            if(fn->stmts[s].kind == ZIR_STMT_ASSIGN &&
               operation(op) >= 4 && operation(op) <= 7)
                used = 1;
        }
    }
    if(!used) return;
    number_prefix(module, p, sizeof(p));
    EmitNumberSupport(out, target, p);
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

void
EmitNumberSupport(FILE *out, ZirTarget target, const char *p)
{
    if(target == ZIR_C || target == ZIR_CPP) {
        fprintf(out, "#include <stdint.h>\n#include <stdbool.h>\n#include <stdlib.h>\n\n");
        fprintf(out,
            "static inline int64_t %s_signed(uint64_t x, int w) {\n"
            "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
            "    x &= mask;\n"
            "    return x <= (mask >> 1) ? (int64_t)x : -1 - (int64_t)(mask - x);\n}\n", p);
        fprintf(out,
            "static inline uint64_t %s_bits(uint64_t a, uint64_t b, int w, int sign, int op) {\n"
            "    uint64_t mask = w == 64 ? UINT64_MAX : (UINT64_C(1) << w) - 1;\n"
            "    uint64_t shift = b; a &= mask; b &= mask;\n"
            "    switch(op) {\n"
            "    case 0: return a; case 1: return (a + b) & mask;\n"
            "    case 2: return (a - b) & mask; case 3: return (a * b) & mask;\n"
            "    case 4: case 5:\n"
            "        if(!b) abort();\n"
            "        if(sign) { int64_t x = %s_signed(a,w), y = %s_signed(b,w);\n"
            "            if(x == INT64_MIN && y == -1) return op == 4 ? a : 0;\n"
            "            return (uint64_t)(op == 4 ? x / y : x %% y) & mask; }\n"
            "        return op == 4 ? a / b : a %% b;\n"
            "    case 6: case 7:\n"
            "        if(shift >= (uint64_t)w) abort();\n"
            "        if(op == 6) return (a << shift) & mask;\n"
            "        if(!shift) return a;\n"
            "        return (a >> shift) | ((sign && (a & (UINT64_C(1) << (w-1)))) ? mask ^ (mask >> shift) : 0);\n"
            "    case 8: return a & b; case 9: return a | b; case 10: return a ^ b;\n"
            "    default: abort(); }\n    return 0;\n}\n\n", p, p, p);
        fprintf(out,
            "static inline uint64_t %s_float(double x, int w, int sign) {\n"
            "    double bound = 1;\n"
            "    int i;\n"
            "    for(i = 0; i < w-sign; i++) bound *= 2;\n"
            "    if(!(x >= (sign ? -bound : 0) && x < bound)) abort();\n"
            "    return sign ? (uint64_t)(int64_t)x : (uint64_t)x;\n}\n", p);
    } else if(target == ZIR_GO) {
        fprintf(out,
            "func %s_signed_bits(x int64) uint64 { return uint64(x) }\n", p);
        fprintf(out,
            "func %s_add(a, b uint64) uint64 { return a+b }\n"
            "func %s_sub(a, b uint64) uint64 { return a-b }\n"
            "func %s_mul(a, b uint64) uint64 { return a*b }\n", p, p, p);
        fprintf(out,
            "func %s_float(x float64, w uint, sign bool) uint64 {\n"
            "    bits := w; if sign { bits-- }; bound := float64(1); for i := uint(0); i < bits; i++ { bound *= 2 }; lower := float64(0); if sign { lower = -bound }\n"
            "    if !(x >= lower && x < bound) { panic(\"float conversion out of range\") }; if sign { return uint64(int64(x)) }; return uint64(x)\n}\n", p);
        fprintf(out,
            "func %s_bits(a, b uint64, w uint, sign bool, op int) uint64 {\n"
            "    mask := ^uint64(0); if w < 64 { mask = (uint64(1) << w) - 1 }; shift := b; a &= mask; b &= mask\n"
            "    switch op {\n"
            "    case 0: return a\n    case 1: return (a+b)&mask\n    case 2: return (a-b)&mask\n    case 3: return (a*b)&mask\n"
            "    case 4,5:\n        if b == 0 { panic(\"integer division by zero\") }\n"
            "        if sign { x := int64(a << (64-w)) >> (64-w); y := int64(b << (64-w)) >> (64-w); if op == 4 { return uint64(x/y)&mask }; return uint64(x%%y)&mask }; if op == 4 { return a/b }; return a%%b\n"
            "    case 6,7:\n        if shift >= uint64(w) { panic(\"invalid shift count\") }; if op == 6 { return (a<<shift)&mask }; if shift == 0 { return a }; result := a>>shift; if sign && (a & (uint64(1)<<(w-1))) != 0 { result |= mask ^ (mask>>shift) }; return result\n"
            "    case 8: return a&b\n    case 9: return a|b\n    case 10: return a^b\n    }; panic(\"invalid numeric operation\")\n}\n\n", p);
    }
}

static int
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

typedef struct Local {
    char name[ZIR_NAME_MAX];
    char type[ZIR_NAME_MAX];
    char drop_alias[ZIR_NAME_MAX];
    int depth;
} Local;
typedef struct Emitter {
    FILE *out;
    const ZirModule *module;
    const ZirFunction *fn;
    ZirTarget target;
    ZirResolveTarget resolve;
    void *context;
    int indent, serial, depth, local_count;
    Local *locals;
    int loop_count;
    int loop_start[128];
    int loop_id[128];
    int sequence_terminated;
    char numbers[64];
    /* Expression folding (Go target): "pure" marks the last emitted expression
     * as free of side effects, so it can be inlined into its consumer instead
     * of being captured in a value_N temporary. "minify" removes the inline
     * length bound for callers that want the densest possible output. */
    int pure;
    int minify;
} Emitter;

/* Bound for readable inlined expressions. Longer results stay in named
 * temporaries so the generated code keeps human-auditable steps. */
#define ZIR_INLINE_MAX 96

static int zir_minify_output;

void
EmitUseMinifiedOutput(int enabled)
{
    zir_minify_output = enabled != 0;
}

static int
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
static int
folds_text(const Emitter *e, const char *text, const char *type)
{
    if(e->target != ZIR_GO && ArrayElementType(type, NULL, 0, NULL))
        return 0;
    if(plain_identifier(text))
        return 1;
    return e->pure && (e->minify || strlen(text) <= ZIR_INLINE_MAX);
}

static void
line(Emitter *e, const char *format, ...)
{
    va_list ap;
    /* Go indents with tabs, as gofmt writes it. */
    for(int i = 0; i < e->indent; i++) fputs(e->target == ZIR_GO ? "\t" : "    ", e->out);
    va_start(ap, format); vfprintf(e->out, format, ap); va_end(ap);
    fputc('\n', e->out);
}

static void
fatal(const ZirExpr *expr, const char *message)
{
    Diagnostic(expr->span, "emit.expression", "%s: %s", message, expr->text);
    exit(1);
}

static void
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

/* Stream the declared shape instead of expanding a nested record into a
 * bounded expression buffer. Copies retain C/Go's independent value semantics. */

/* Array expressions are values. Their source is captured before any write,
 * so even a self-assignment or an overlapping record destination is safe. */
static int enclosed(const char *text);
static const char *bare(const char *text, char *out, size_t size);

static void
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

static void
declare_array(Emitter *e, const char *name, const char *type, const char *value)
{
    char element[ZIR_NAME_MAX];
    char target_element[ZIR_NAME_MAX * 2];
    /* Nested fixed arrays ([4][96]u8) flatten to one C dimension list:
     * uint8_t name[4][96]. Collect every bound outside-in. */
    char bounds[ZIR_NAME_MAX * 2] = "";
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
            copy_text(bounds, sizeof(bounds), bound);
        } else if(strlen(bounds) + strlen(bound) < sizeof(bounds)) {
            strcat(bounds, bound);
        }
    }
    copy_text(element, sizeof(element), working);
    const char *scalar = TargetType(element, e->target);
    if(scalar != NULL) {
        copy_text(target_element, sizeof(target_element), scalar);
    } else if(element[0] == '*') {
        /* Pointer elements lower through the same naming as slot types:
         * [4]*u8 emits uint8_t* name[4]. */
        char native[ZIR_NAME_MAX * 2];
        slot_native_type(element, e->target, native, sizeof(native));
        copy_text(target_element, sizeof(target_element), native);
    } else {
        e->resolve(e->context, element, target_element, sizeof(target_element));
    }
    if(e->target == ZIR_GO) {
        if(value != NULL && *value)
            line(e, "var %s %s%s = %s", name, bounds, target_element, value);
        else
            line(e, "var %s %s%s", name, bounds, target_element);
    } else if(e->target == ZIR_C || e->target == ZIR_CPP) {
        int zero = TypeHasZeroArray(e->module, type);
        line(e, "%s%s %s%s = %s;", zero ? "__extension__ " : "",
             target_element, name, bounds,
             zero || e->target == ZIR_CPP ? "{}" : "{0}");
        if(value != NULL && *value)
            assign_value(e, name, type, value);
    } else {
        Diagnostic(e->fn->span, "emit.array_target", "array values are supported only by native targets");
        exit(1);
    }
}

static void
declare(Emitter *e, const char *name, const char *type, const char *value)
{
    char binding[ZIR_NAME_MAX];
    char plain[ZIR_TEXT_MAX];
    if(value != NULL)
        value = bare(value, plain, sizeof(plain));
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
        if(e->target == ZIR_GO)
            line(e, "var %s %s = %s(%s)", name, target_type, target_type, value);
        else
            line(e, "%s %s = (%s)(%s);", target_type, name, target_type,
                 value);
        return;
    }
    if(e->target == ZIR_GO) line(e, "var %s %s = %s", name, target_type, value);
    else line(e, "%s%s %s = %s;",
              TypeHasZeroArray(e->module, type) ? "__extension__ " : "",
              target_type, name, value);
}

static void
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

static void
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

static void
track_local(Emitter *e, const char *name, const char *type)
{
    Local *local = &e->locals[e->local_count++];
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

static void
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

static void
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

static int
has_owned_locals(const Emitter *e)
{
    for(int i = 0; i < e->local_count; i++)
        if(*e->locals[i].drop_alias ||
           emitter_type_contains_vec(e->module, e->locals[i].type, 0))
            return 1;
    return 0;
}

static int
operation(const char *op)
{
    static const char *const ops[] = {"", "+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"};
    for(int i = 1; i <= 10; i++) if(!strcmp(op,ops[i])) return i;
    return 0;
}

static void
go_bits_operand(const char *value, int sign, const char *prefix,
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
        format(out, size, "%s_signed_bits(int64(%s))", prefix, value);
    else
        format(out, size, "uint64(%s)", value);
}

/* An emitted integer literal: optional sign, decimal or hex digits, and C
 * suffixes. Its two's-complement bits go to *bits. */
static int
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
static int
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
static const char *
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
     * directly. X that is already this arithmetic's W group stays as is. */
    prefix = strlen(native) + 2;
    (void)optype;
    if(inner[0] == '(' && !strncmp(inner + 1, native, strlen(native)) &&
       inner[prefix - 1] == ')' && enclosed(inner + prefix)) {
        const char *group = inner + prefix;
        if(group[1] == '(')
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

static void
number(Emitter *e, const char *type, const char *a, const char *a_type,
       const char *b, const char *b_type, int op, char *out, size_t size)
{
    char bits[ZIR_TEXT_MAX];
    int w = width(type), sign = signed_type(type);
    uint64_t left_bits, right_bits, folded;
    int left_literal = integer_literal_bits(a, &left_bits);
    int right_literal = all_ones_operand(b) ? (right_bits = UINT64_MAX, 1) :
                        integer_literal_bits(b, &right_bits);
    const char *native = TargetType(type, e->target);
    char float_helper[80];
    format(float_helper, sizeof(float_helper), "%s_float(", e->numbers);
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
            char left[ZIR_TEXT_MAX], right[ZIR_TEXT_MAX];
            if(op == 10 && all_ones_operand(b)) {
                if(same_number_type(a_type, type) && !left_literal)
                    format(out, size, "^%s", a);
                else
                    format(out, size, "^%s(%s)", native, a);
                return;
            }
            /* A literal wrapped to the width always fits the Go type. */
            if(left_literal) number_literal(e, type, left_bits, left, sizeof(left));
            else if(same_number_type(a_type, type)) copy_text(left, sizeof(left), a);
            else format(left, sizeof(left), "%s(%s)", native, a);
            if(right_literal) number_literal(e, type, right_bits, right, sizeof(right));
            else if(same_number_type(b_type, type)) copy_text(right, sizeof(right), b);
            else format(right, sizeof(right), "%s(%s)", native, b);
            if(left_literal && right_literal)
                format(left, sizeof(left), "%s(%s)", native, a);
            format(out, size, "%s %s %s", left, symbols[op], right);
            return;
        }
        const char *wide = w == 64 ? "uint64_t" : "uint32_t";
        char left[ZIR_TEXT_MAX], right[ZIR_TEXT_MAX];
        c_wide_operand(a, a_type, type, native, wide, left, sizeof(left));
        if(op == 10 && all_ones_operand(b)) {
            format(out, size, "(%s)~%s", native, left);
            return;
        }
        c_wide_operand(b, b_type, type, native, wide, right, sizeof(right));
        format(out, size, "(%s)(%s %s %s)", native, left, symbols[op], right);
        return;
    }
    if(e->target == ZIR_GO) {
        char left[ZIR_TEXT_MAX], right[ZIR_TEXT_MAX];
        go_bits_operand(a, sign, e->numbers, left, sizeof(left));
        go_bits_operand(b, sign, e->numbers, right, sizeof(right));
        if(op >= 1 && op <= 3) {
            const char *name = op == 1 ? "add" : op == 2 ? "sub" : "mul";
            /* Narrowing the uint64 result below keeps the low w bits. This
             * matches the checked wrapping rule while giving Go a small
             * inlinable operation instead of the general switch helper. */
            format(bits, sizeof(bits), "%s_%s(%s,%s)", e->numbers,
                   name, left, right);
        } else {
            format(bits,sizeof(bits),"%s_bits(%s,%s,%d,%s,%d)",e->numbers,left,right,w,sign?"true":"false",op);
        }
        format(out,size,"%s(%s)",TargetType(type,e->target),bits);
    } else {
        format(bits,sizeof(bits),"%s_bits((uint64_t)(%s),(uint64_t)(%s),%d,%d,%d)",e->numbers,a,b,w,sign,op);
        if(sign) format(out,size,"(%s)%s_signed(%s,%d)",TargetType(type,e->target),e->numbers,bits,w);
        else format(out,size,"(%s)(%s)",TargetType(type,e->target),bits);
    }
}

static void emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size);
static void zero_record(Emitter *e, const char *type, char *out, size_t size);

static void
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
static void
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

static int member_path(const ZirFunction *fn, int index);

static void
emit_destination(Emitter *e, int index, char *out, size_t size)
{
    const ZirExpr *expr = &e->fn->exprs[index];
    if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "*")) {
        char pointer[ZIR_TEXT_MAX];
        emit_expr(e, expr->right, e->fn->exprs[expr->right].type,
                  pointer, sizeof(pointer));
        format(out, size, "*(%s)", pointer);
        return;
    }
    if(expr->kind == ZIR_EXPR_IDENT) {
        resolve(e, expr->name, out, size);
        e->pure = 1;
        return;
    }
    if(expr->kind == ZIR_EXPR_MEMBER || expr->kind == ZIR_EXPR_POINTER_MEMBER) {
        char base[ZIR_TEXT_MAX];
        if(expr->kind == ZIR_EXPR_POINTER_MEMBER &&
           !member_path(e->fn, expr->left))
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type,
                      base, sizeof(base));
        else
            emit_destination(e, expr->left, base, sizeof(base));
        emit_field_path(e->module, e->target,
                        e->fn->exprs[expr->left].type, expr->name,
                        base, out, size);
        e->pure = 1;
        return;
    }
    if(expr->kind == ZIR_EXPR_INDEX) {
        char base[ZIR_TEXT_MAX], index[ZIR_TEXT_MAX];
        int vector = VecElementType(e->module,
            e->fn->exprs[expr->left].type, NULL, 0);

        if(!strcmp(e->fn->exprs[expr->left].type, "string"))
            fatal(expr, "string bytes are read-only");
        if(SliceElementType(e->fn->exprs[expr->left].type, NULL, 0))
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, base, sizeof(base));
        else
            emit_destination(e, expr->left, base, sizeof(base));
        {
            int base_pure = e->pure;
            emit_expr(e, expr->right, "s32", index, sizeof(index));
            e->pure = base_pure && e->pure;
        }
        if(SliceElementType(e->fn->exprs[expr->left].type, NULL, 0))
            slice_index(e, e->fn->exprs[expr->left].type, base, index, out, size);
        else if(e->target == ZIR_GO &&
                e->fn->exprs[expr->left].type[0] == '*')
            go_pointer_index(e, base, index, out, size);
        else if(vector && (e->target == ZIR_C || e->target == ZIR_CPP))
            format(out, size, "ZIRAN_VEC_INDEX((%s).data, (%s).count, %s)",
                   base, base, index);
        else if(vector)
            format(out, size, "(%s).Data[%s]", base, index);
        else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
           ArrayElementType(e->fn->exprs[expr->left].type, NULL, 0, NULL))
            format(out, size, "ZIRAN_INDEX(%s, sizeof(%s) / sizeof(%s[0]), %s)",
                   base, base, base, index);
        else
            format(out, size, "%s[%s]", base, index);
        return;
    }
    fatal(expr, "unsupported assignment destination");
}

static void
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

static void
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

int
ScalarLiteral(const char *type, const char *text, ZirTarget target,
                  ZirSourceSpan span, char *out, size_t size)
{
    ZirFunction fn={0};Emitter e={0};int ok=0;
    type=canonical(type);e.target=target;
    if(!*type)return 0;
    if(!strcmp(type,"bool")) {
        if(!strcmp(text,"true") || !strcmp(text,"false")) {copy_text(out,size,text);return 1;}
        return 0;
    }
    int index=ParseExpr(&fn,NULL,text,span);
    if(index>=0) {
        ZirExpr *expr=&fn.exprs[index];
        if(expr->kind == ZIR_EXPR_STRING && !strcmp(type, "string")) {
            char value[ZIR_TEXT_MAX];
            string_literal(expr, target, value, sizeof(value));
            if(target == ZIR_C || target == ZIR_CPP)
                format(out, size, "{%s, sizeof(%s) - 1}", value, value);
            else
                copy_text(out, size, value);
            ok = 1;
        } else if(expr->kind==ZIR_EXPR_INT && width(type)) {
            literal(&e,expr,type,0,out,size);ok=1;
        } else if(expr->kind==ZIR_EXPR_UNARY && !strcmp(expr->op,"-") &&
                  fn.exprs[expr->right].kind==ZIR_EXPR_INT && width(type)) {
            literal(&e,&fn.exprs[expr->right],type,1,out,size);ok=1;
        } else if((expr->kind==ZIR_EXPR_FLOAT || expr->kind==ZIR_EXPR_INT) && type[0]=='f') {
            copy_text(out,size,text);size_t n=strlen(out);
            if(n && (out[n-1]=='f'||out[n-1]=='F'))out[n-1]=0;
            ok=1;
        }
    }
    free(fn.exprs);return ok;
}

typedef struct {
    const ZirModule *module;
    const ZirGlobal *global;
    const ZirFunction *probe;
    ZirTarget target;
    ZirSourceSpan span;
    ZirGlobalScalarRewrite scalar;
    ZirGlobalTypeRewrite type_name;
    ZirGlobalFieldRewrite field_name;
    void *context;
} GlobalLiteralEmit;

static void
global_slot_wrapper_name(const ZirModule *module, const ZirGlobal *global,
                         int index, char *out, size_t size)
{
    char prefix[64];
    number_prefix(module, prefix, sizeof(prefix));
    format(out, size, "%s_global_slot_%ld_%d", prefix,
           (long)(global - module->globals), index);
}

static int
append_global_literal(char *out, size_t size, size_t *used,
                      const char *format_string, ...)
{
    va_list ap;
    int written;
    if(*used >= size) return 0;
    va_start(ap, format_string);
    written = vsnprintf(out + *used, size - *used, format_string, ap);
    va_end(ap);
    if(written < 0 || (size_t)written >= size - *used) return 0;
    *used += (size_t)written;
    return 1;
}

static int
global_field(const ZirType *record, const char *name, int ordinal,
             ZirTypeField *found)
{
    ZirTypeField field;
    size_t offset = 0;
    int position = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        if((name[0] && !strcmp(name, field.name)) ||
           (!name[0] && position == ordinal)) {
            *found = field;
            return 1;
        }
        position++;
    }
    return 0;
}

static const ZirType *
global_function_slot(const ZirModule *module, const ZirModule *scope,
                     const char *type, const ZirExpr *expr)
{
    const ZirType *slot = FindType(scope, type, NULL);
    const ZirModule *owner = NULL;
    const ZirFunction *declaration = NULL;
    if(slot == NULL || !slot->is_procedure_type ||
       expr->kind != ZIR_EXPR_IDENT ||
       ResolveFunction(module, expr->name, &owner, &declaration) != 1 ||
       declaration->is_extern)
        return NULL;
    return slot;
}

static int
global_type_at_use(const ZirModule *module, const ZirModule *scope,
                   const char *type, char *out, size_t size)
{
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL)) {
        const char *close = strchr(type, ']');
        char mapped[ZIR_NAME_MAX];
        if(close == NULL ||
           !global_type_at_use(module, scope, element, mapped,
                               sizeof(mapped))) return 0;
        int written = snprintf(out, size, "%.*s%s",
                               (int)(close - type + 1), type, mapped);
        return written >= 0 && (size_t)written < size;
    }
    const ZirType *declared = FindType(scope, type, NULL);
    if(scope == module || declared == NULL ||
       FindType(module, type, NULL) == declared) {
        copy_text(out, size, type);
        return strlen(type) < size;
    }
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(import->kind != ZIR_IMPORT_MODULE ||
           import->resolved_module != scope) continue;
        int written = snprintf(out, size, "%s.%s", import->name, type);
        if(written >= 0 && (size_t)written < size &&
           FindType(module, out, NULL) == declared)
            return 1;
    }
    return 0;
}

static int
emit_global_literal_node(GlobalLiteralEmit *emit, int index,
                         const ZirModule *scope, const char *type,
                         char *out, size_t size,
                         size_t *used, int depth)
{
    if(index < 0 || index >= emit->probe->expr_count || depth > 32)
        return 0;
    const ZirExpr *expr = &emit->probe->exprs[index];
    if(expr->kind == ZIR_EXPR_COMPOUND) {
        char element[ZIR_NAME_MAX];
        const ZirType *record = NULL;
        const ZirModule *record_owner = NULL;
        int array = ArrayElementType(type, element, sizeof(element), NULL);
        if(!array) record = FindType(scope, type, &record_owner);
        if(!array && (record == NULL || record->is_enum ||
                      record->is_procedure_type || record->is_union))
            return 0;
        if(emit->target == ZIR_GO) {
            char mapped[ZIR_NAME_MAX * 2];
            char visible[ZIR_NAME_MAX];
            if(emit->type_name == NULL ||
               !global_type_at_use(emit->module, scope, type, visible,
                                   sizeof(visible)) ||
               !emit->type_name(emit->module, visible, mapped,
                                sizeof(mapped), emit->context) ||
               !append_global_literal(out, size, used, "%s{", mapped))
                return 0;
        } else if(!append_global_literal(out, size, used, "{"))
            return 0;
        if(record != NULL && emit->target == ZIR_CPP) {
            ZirTypeField field;
            size_t offset = 0;
            int field_index = 0;
            while(TypeNextField(record, &offset, &field) == 1) {
                int selected = -1, entry_ordinal = 0;
                for(int child = expr->first_child; child >= 0;
                    child = emit->probe->exprs[child].next_sibling) {
                    const ZirExpr *entry = &emit->probe->exprs[child];
                    if((entry->name[0] &&
                        !strcmp(entry->name, field.name)) ||
                       (!entry->name[0] && entry_ordinal == field_index)) {
                        selected = entry->right;
                        break;
                    }
                    entry_ordinal++;
                }
                if(field_index++ &&
                   !append_global_literal(out, size, used, ", ")) return 0;
                if(selected >= 0) {
                    if(!emit_global_literal_node(emit, selected,
                                                 record_owner, field.type,
                                                 out, size, used, depth + 1))
                        return 0;
                } else if(!append_global_literal(out, size, used, "{}"))
                    return 0;
            }
        } else {
            int position = 0;
            for(int child = expr->first_child; child >= 0;
                child = emit->probe->exprs[child].next_sibling) {
                const ZirExpr *entry = &emit->probe->exprs[child];
                const char *field_type = element;
                ZirTypeField field;
                if(entry->right < 0 ||
                   (record != NULL &&
                    !global_field(record, entry->name, position, &field)))
                    return 0;
                if(position &&
                   !append_global_literal(out, size, used, ", ")) return 0;
                if(record != NULL) {
                    char mapped[ZIR_NAME_MAX * 2];
                    field_type = field.type;
                    if(emit->field_name != NULL)
                        emit->field_name(record, field.name, mapped,
                                         sizeof(mapped), emit->context);
                    else
                        TargetFieldName(record, emit->target, field.name,
                                        mapped, sizeof(mapped));
                    if(!append_global_literal(out, size, used,
                         emit->target == ZIR_GO ? "%s: " : ".%s = ",
                         mapped)) return 0;
                }
                if(!emit_global_literal_node(emit, entry->right,
                                             record != NULL ? record_owner :
                                             scope, field_type,
                                             out, size, used, depth + 1))
                    return 0;
                position++;
            }
        }
        return append_global_literal(out, size, used, "}");
    }
    const ZirType *slot = global_function_slot(emit->module, scope,
                                                type, expr);
    if(slot != NULL) {
        char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
        size_t length;
        if(emit->target == ZIR_GO || slot->is_c_call) {
            if(emit->scalar == NULL) return 0;
            format(call, sizeof(call), "%s()", expr->name);
            if(!emit->scalar(emit->module, call, resolved,
                             sizeof(resolved), emit->context)) return 0;
            length = strlen(resolved);
            if(length < 2 || strcmp(resolved + length - 2, "()")) return 0;
            resolved[length - 2] = '\0';
            return append_global_literal(out, size, used, "%s", resolved);
        }
        char wrapper[ZIR_NAME_MAX];
        global_slot_wrapper_name(emit->module, emit->global, index,
                                 wrapper, sizeof(wrapper));
        return append_global_literal(out, size, used,
                                     emit->target == ZIR_C ? "{NULL, %s}" :
                                                             "{nullptr, %s}",
                                     wrapper);
    }
    char scalar[ZIR_TEXT_MAX * 2];
    if(!ScalarLiteral(type, expr->text, emit->target, emit->span,
                      scalar, sizeof(scalar))) {
        if(emit->scalar == NULL ||
           !emit->scalar(emit->module, expr->text, scalar,
                         sizeof(scalar), emit->context)) return 0;
    }
    return append_global_literal(out, size, used, "%s", scalar);
}

int
EmitGlobalInitializer(const ZirModule *module, const ZirGlobal *global,
                      ZirTarget target, ZirGlobalScalarRewrite scalar,
                      ZirGlobalTypeRewrite type_name,
                      ZirGlobalFieldRewrite field_name, void *context,
                      char *out, size_t size)
{
    ZirFunction probe = {0};
    int root, ok;
    size_t used = 0;
    if(size == 0 || !global->init[0]) return 0;
    out[0] = '\0';
    root = ParseExprTyped(&probe, module, global->init,
                          global->span, global->type);
    if(root < 0 || (probe.exprs[root].kind != ZIR_EXPR_COMPOUND &&
       global_function_slot(module, module, global->type,
                            &probe.exprs[root]) == NULL)) {
        free(probe.exprs);
        return 0;
    }
    GlobalLiteralEmit emit = {module, global, &probe, target, global->span,
                              scalar, type_name, field_name, context};
    ok = emit_global_literal_node(&emit, root, module, global->type, out, size,
                                  &used, 0);
    free(probe.exprs);
    return ok ? 1 : -1;
}

static void
emit_global_slot_wrappers_node(FILE *out, const ZirModule *module,
                               const ZirGlobal *global,
                               const ZirFunction *probe, int index,
                               const ZirModule *scope, const char *type,
                               ZirTarget target, ZirResolveTarget resolver,
                               void *context, int depth)
{
    if(index < 0 || index >= probe->expr_count || depth > 32) return;
    const ZirExpr *value = &probe->exprs[index];
    if(value->kind == ZIR_EXPR_COMPOUND) {
        char element[ZIR_NAME_MAX];
        const ZirModule *record_owner = NULL;
        int array = ArrayElementType(type, element, sizeof(element), NULL);
        const ZirType *record = array ? NULL :
            FindType(scope, type, &record_owner);
        int position = 0;
        for(int child = value->first_child; child >= 0;
            child = probe->exprs[child].next_sibling) {
            const ZirExpr *entry = &probe->exprs[child];
            ZirTypeField field;
            if(entry->right >= 0 &&
               (array || (record != NULL &&
                          global_field(record, entry->name, position,
                                       &field))))
                emit_global_slot_wrappers_node(out, module, global, probe,
                    entry->right, record ? record_owner : scope,
                    record ? field.type : element, target, resolver,
                    context, depth + 1);
            position++;
        }
        return;
    }
    const ZirType *slot = global_function_slot(module, scope, type, value);
    if(slot == NULL || slot->is_c_call) return;
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, parameters[0], 64,
                        sizeof(parameters[0])) : 0;
    char wrapper[ZIR_NAME_MAX], call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
    char result_type[ZIR_NAME_MAX], resolved_result[ZIR_NAME_MAX];
    slot_native_type(slot->procedure_return_type, target,
                     result_type, sizeof(result_type));
    resolver(context, result_type, resolved_result,
             sizeof(resolved_result));
    global_slot_wrapper_name(module, global, index,
                             wrapper, sizeof(wrapper));
    fprintf(out, "static %s %s(void *context", resolved_result, wrapper);
    size_t length = (size_t)format(call, sizeof(call), "%s(", value->name);
    for(int argument = 0; argument < count; argument++) {
        const char *separator = strchr(parameters[argument], ':');
        if(separator == NULL) break;
        const char *source = skip_ws(separator + 1);
        char native_type[ZIR_NAME_MAX], resolved_type[ZIR_NAME_MAX];
        slot_native_type(source, target, native_type, sizeof(native_type));
        resolver(context, native_type, resolved_type, sizeof(resolved_type));
        fprintf(out, ", %s slot_arg_%d", resolved_type, argument);
        length += (size_t)format(call + length, sizeof(call) - length,
                                  "%sslot_arg_%d",
                                  argument ? ", " : "", argument);
    }
    format(call + length, sizeof(call) - length, ")");
    resolver(context, call, resolved, sizeof(resolved));
    fprintf(out, ")\n{\n    (void)context;\n    %s%s;\n}\n",
            strcmp(slot->procedure_return_type, "void") ? "return " : "",
            resolved);
}

void
EmitGlobalSlotWrappers(FILE *out, const ZirModule *module,
                       const ZirGlobal *global, ZirTarget target,
                       ZirResolveTarget resolver, void *context)
{
    ZirFunction probe = {0};
    if((target != ZIR_C && target != ZIR_CPP) || !global->init[0])
        return;
    int root = ParseExprTyped(&probe, module, global->init,
                              global->span, global->type);
    if(root >= 0)
        emit_global_slot_wrappers_node(out, module, global, &probe, root,
                                       module, global->type, target,
                                       resolver, context, 0);
    free(probe.exprs);
}

static void
call_parameter_type(Emitter *e, const ZirExpr *call, int ordinal,
                    char *out, size_t size)
{
    const char *signature = NULL;
    if(call->slot_type[0]) {
        const ZirType *slot = FindType(e->module, call->slot_type, NULL);
        if(slot != NULL && slot->is_procedure_type)
            signature = slot->body;
    } else {
        const ZirModule *owner = NULL;
        const ZirFunction *function = NULL;
        if(ResolveFunction(e->module, call->name, &owner, &function) == 1 &&
           function != NULL)
            signature = function->args;
        else
            for(int i = 0; i < e->module->import_count; i++)
                if(e->module->imports[i].kind == ZIR_IMPORT_EXTERN &&
                   !strcmp(e->module->imports[i].name, call->name)) {
                    signature = e->module->imports[i].args;
                    break;
                }
    }
    if(signature == NULL || !*signature)
        return;
    char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL)
        return;
    int count = split_top_level(signature, parts[0], 64, sizeof(parts[0]));
    if(ordinal >= 0 && ordinal < count) {
        char *colon = strchr(parts[ordinal], ':');
        if(colon != NULL) {
            colon = (char *)skip_ws(colon + 1);
            trim_in_place(colon);
            copy_text(out, size, colon);
        }
    }
    free(parts);
}

static int
identifier_byte(char c)
{
    return c == '_' || (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void
replace_call_placeholders(const char *text, char arguments[][ZIR_NAME_MAX],
                          int count, char *out, size_t size)
{
    static const char prefix[] = "zir_call_argument_";
    size_t used = 0;
    const char *p = text;
    while(*p && used + 1 < size) {
        int ordinal = 0;
        const char *digits = p + sizeof(prefix) - 1;
        const char *end = digits;
        if((p == text || !identifier_byte(p[-1])) &&
           !strncmp(p, prefix, sizeof(prefix) - 1)) {
            while(*end >= '0' && *end <= '9')
                ordinal = ordinal * 10 + (*end++ - '0');
        }
        if(end > digits && !identifier_byte(*end) && ordinal < count) {
            used += strlen(arguments[ordinal]);
            if(used >= size)
                break;
            memcpy(out + used - strlen(arguments[ordinal]), arguments[ordinal],
                   strlen(arguments[ordinal]));
            p = end;
            continue;
        }
        out[used++] = *p++;
    }
    out[used < size ? used : size - 1] = '\0';
}

static void
emit_call(Emitter *e, const ZirExpr *expr, const char *array_result, char *out, size_t size)
{
    char text[ZIR_TEXT_MAX];
    char arguments[64][ZIR_NAME_MAX] = {{0}};
    int count=0;
    int argument_count = 0;
    int has_named = 0;
    size_t n;
    for(int child = expr->first_child; child >= 0;
        child = e->fn->exprs[child].next_sibling)
        has_named |= e->fn->exprs[child].argument_name[0] != '\0';
    if(*expr->slot_type) {
        char callable[ZIR_NAME_MAX];
        fresh(e, callable);
        char source[ZIR_TEXT_MAX];
        if(expr->name[0]) {
            resolve(e, expr->name, source, sizeof(source));
        } else {
            /* Anonymous slot call: the callee expression (a record field,
             * a local, or an index) names the callable. */
            source[0] = '\0';
            emit_expr(e, expr->left, expr->slot_type, source, sizeof(source));
        }
        declare(e, callable, expr->slot_type, source);
        if(e->target == ZIR_C || e->target == ZIR_CPP) {
            const ZirType *slot = FindType(e->module, expr->slot_type, NULL);
            if(slot != NULL && slot->is_c_call) {
                n = (size_t)format(text, sizeof(text), "%s(", callable);
            } else {
                n = (size_t)format(text, sizeof(text), "%s.call(%s.context", callable, callable);
                count = 1;
            }
        } else {
            n = (size_t)format(text, sizeof(text), "%s(", callable);
        }
    } else {
        n = (size_t)format(text, sizeof(text), "%s(", expr->name);
    }
    if(array_result != NULL) {
        n += (size_t)format(text + n, sizeof(text) - n, "%s%s", count ? "," : "", array_result);
        count++;
    }
    for(int child=expr->first_child;child>=0;child=e->fn->exprs[child].next_sibling) {
        int ordinal = e->fn->exprs[child].argument_index;
        char argument[ZIR_TEXT_MAX];
        char argument_type[ZIR_NAME_MAX];
        if(ordinal < 0 || ordinal >= 64 || arguments[ordinal][0])
            fatal(expr, "invalid checked call argument order");
        copy_text(argument_type, sizeof(argument_type),
                  e->fn->exprs[child].type);
        if(!strcmp(argument_type, "null") ||
           !strcmp(argument_type, "integer")) {
            char parameter_type[ZIR_NAME_MAX] = "";
            call_parameter_type(e, expr, ordinal, parameter_type,
                                sizeof(parameter_type));
            if(!strcmp(argument_type, "null") ||
               (enum_type(e->module, parameter_type) &&
                FindType(e->module, parameter_type, NULL)->is_enum_flags))
                copy_text(argument_type, sizeof(argument_type), parameter_type);
        }
        emit_expr(e,child,argument_type,argument,sizeof(argument));
        /* The whole call text passes through the target resolver, which only
         * understands identifiers as arguments — the shape every emitted
         * argument had before folding. Capture folded expressions in a
         * temporary first; sub-expressions inside the argument still fold. */
        if(has_named || !plain_identifier(argument)) {
            char captured[ZIR_NAME_MAX];
            fresh(e, captured);
            declare(e, captured, argument_type, argument);
            copy_text(argument, sizeof(argument), captured);
        }
        copy_text(arguments[ordinal], sizeof(arguments[ordinal]), argument);
        argument_count++;
    }
    /* Arguments are already lowered. The resolver maps every identifier in
     * the call text to its global, so it sees placeholders instead: a local
     * or parameter must never become an imported name it shadows. */
    for(int ordinal = 0; ordinal < argument_count; ordinal++) {
        char placeholder[ZIR_NAME_MAX];
        if(!arguments[ordinal][0])
            fatal(expr, "missing checked call argument");
        format(placeholder, sizeof(placeholder), "zir_call_argument_%d", ordinal);
        n += (size_t)format(text+n, sizeof(text)-n, "%s%s", count ? "," : "",
                            *expr->slot_type ? arguments[ordinal] : placeholder);
        count++;
    }
    format(text+n,sizeof(text)-n,")");
    if(*expr->slot_type) {
        copy_text(out, size, text);
        return;
    }
    char resolved[ZIR_TEXT_MAX];
    e->resolve(e->context, text, resolved, sizeof(resolved));
    replace_call_placeholders(resolved, arguments, argument_count, out, size);
}

static void
slot_wrapper_name(const ZirModule *module, const ZirFunction *fn, int index,
                   char *out, size_t size)
{
    char prefix[64];
    number_prefix(module, prefix, sizeof(prefix));
    format(out, size, "%s_slot_%ld_%d", prefix, (long)(fn - module->functions), index);
}

/* C has no lexical function values. A file-scope adapter supplies the uniform
 * borrowed-context slot ABI while ordinary Ziran functions keep their own ABI. */
void
EmitSlotWrappers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                    ZirTarget target, ZirResolveTarget resolver, void *context)
{
    if((target != ZIR_C && target != ZIR_CPP) || !CanEmitBody(module, fn))
        return;
    for(int index = 0; index < fn->expr_count; index++) {
        const ZirExpr *value = &fn->exprs[index];
        if(!value->is_function_value)
            continue;
        const ZirType *slot = FindType(module, value->type, NULL);
        if(slot == NULL || slot->is_c_call)
            continue;
        char parameters[64][ZIR_TEXT_MAX];
        int count = *skip_ws(slot->body) ?
            split_top_level(slot->body, parameters[0], 64, sizeof(parameters[0])) : 0;
        char wrapper[ZIR_NAME_MAX], call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
        char result_type[ZIR_NAME_MAX], resolved_result[ZIR_NAME_MAX];
        slot_native_type(slot->procedure_return_type, target,
                         result_type, sizeof(result_type));
        resolver(context, result_type, resolved_result,
                 sizeof(resolved_result));
        slot_wrapper_name(module, fn, index, wrapper, sizeof(wrapper));
        fprintf(out, "static %s %s(void *context", resolved_result, wrapper);
        size_t length = (size_t)format(call, sizeof(call), "%s(", value->name);
        for(int argument = 0; argument < count; argument++) {
            const char *source = skip_ws(strchr(parameters[argument], ':') + 1);
            char native_type[ZIR_NAME_MAX], resolved_type[ZIR_NAME_MAX];
            slot_native_type(source, target, native_type,
                             sizeof(native_type));
            resolver(context, native_type, resolved_type,
                     sizeof(resolved_type));
            fprintf(out, ", %s slot_arg_%d", resolved_type, argument);
            length += (size_t)format(call + length, sizeof(call) - length,
                                      "%sslot_arg_%d", argument ? ", " : "", argument);
        }
        format(call + length, sizeof(call) - length, ")");
        resolver(context, call, resolved, sizeof(resolved));
        fprintf(out, ")\n{\n    (void)context;\n    %s%s;\n}\n",
                strcmp(slot->procedure_return_type, "void") ? "return " : "", resolved);
    }
}

static void
emit_function_value(Emitter *e, int index, char *out, size_t size)
{
    const ZirExpr *value = &e->fn->exprs[index];
    const ZirType *slot = FindType(e->module, value->type, NULL);
    if(e->target == ZIR_C || e->target == ZIR_CPP) {
        if(slot != NULL && slot->is_c_call) {
            char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
            size_t length;
            format(call, sizeof(call), "%s()", value->name);
            e->resolve(e->context, call, resolved, sizeof(resolved));
            length = strlen(resolved);
            if(length >= 2 && !strcmp(resolved + length - 2, "()"))
                resolved[length - 2] = '\0';
            copy_text(out, size, resolved);
            return;
        }
        char wrapper[ZIR_NAME_MAX];
        slot_wrapper_name(e->module, e->fn, index, wrapper, sizeof(wrapper));
        format(out, size, "(%s){NULL, %s}", value->type, wrapper);
        return;
    }
    char parameters[64][ZIR_TEXT_MAX];
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, parameters[0], 64, sizeof(parameters[0])) : 0;
    char arguments[ZIR_TEXT_MAX] = "", signature[ZIR_TEXT_MAX] = "";
    char call[ZIR_TEXT_MAX], resolved[ZIR_TEXT_MAX];
    size_t length = 0, signature_length = 0;
    for(int argument = 0; argument < count; argument++) {
        char parameter[ZIR_NAME_MAX];
        fresh(e, parameter);
        length += (size_t)format(arguments + length, sizeof(arguments) - length,
                                  "%s%s", argument ? ", " : "", parameter);
        if(e->target == ZIR_GO) {
            const char *source = skip_ws(strchr(parameters[argument], ':') + 1);
            const char *scalar = TargetType(source, ZIR_GO);
            char type[ZIR_NAME_MAX];
            if(scalar)
                copy_text(type, sizeof(type), scalar);
            else
                e->resolve(e->context, source, type, sizeof(type));
            signature_length += (size_t)format(signature + signature_length,
                sizeof(signature) - signature_length, "%s%s %s", argument ? ", " : "", parameter, type);
        }
    }
    format(call, sizeof(call), "%s(%s)", value->name, arguments);
    e->resolve(e->context, call, resolved, sizeof(resolved));
    if(e->target == ZIR_GO) {
        char result_type[ZIR_NAME_MAX] = "";
        if(strcmp(slot->procedure_return_type, "void")) {
            const char *scalar = TargetType(slot->procedure_return_type, ZIR_GO);
            if(scalar)
                copy_text(result_type, sizeof(result_type), scalar);
            else
                e->resolve(e->context, slot->procedure_return_type,
                           result_type, sizeof(result_type));
        }
        format(out, size, "func(%s)%s%s { %s%s }", signature,
               result_type[0] ? " " : "", result_type,
               result_type[0] ? "return " : "", resolved);
    }
    else
        format(out, size, "(%s) => %s", arguments, resolved);
}

static int
member_path(const ZirFunction *fn, int index)
{
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_IDENT)
        return 1;
    return (expr->kind == ZIR_EXPR_MEMBER ||
            expr->kind == ZIR_EXPR_POINTER_MEMBER) && member_path(fn, expr->left);
}

static void zero_record(Emitter *e, const char *type, char *out, size_t size);

/* Checked `print`: evaluate every argument left to right, then write the
 * format's literal pieces and argument values to standard output in order. */
static int
expression_calls(const ZirFunction *fn, int index)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_CALL)
        return 1;
    for(int child = expr->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(expression_calls(fn, child))
            return 1;
    return expression_calls(fn, expr->left) || expression_calls(fn, expr->right) ||
           expression_calls(fn, expr->third);
}

/* A run of print output for one standard call: the format text, the same
 * text unescaped for a call without values, and the value arguments. */
typedef struct PrintRun {
    unsigned char format[ZIR_TEXT_MAX];
    size_t format_length;
    unsigned char plain[ZIR_TEXT_MAX];
    size_t plain_length;
    char arguments[ZIR_TEXT_MAX];
    int values;
} PrintRun;

static void
print_run_text(PrintRun *run, const unsigned char *bytes, size_t length)
{
    for(size_t i = 0; i < length; i++) {
        if(run->plain_length + 1 < sizeof(run->plain))
            run->plain[run->plain_length++] = bytes[i];
        if(run->format_length + 2 < sizeof(run->format)) {
            run->format[run->format_length++] = bytes[i];
            if(bytes[i] == '%')
                run->format[run->format_length++] = '%';
        }
    }
}

static void
print_run_value(PrintRun *run, const char *verb, const char *argument)
{
    size_t used = strlen(run->arguments);
    for(const char *c = verb; *c && run->format_length + 1 < sizeof(run->format); c++)
        run->format[run->format_length++] = (unsigned char)*c;
    format(run->arguments + used, sizeof(run->arguments) - used, ", %s", argument);
    run->values++;
}

/* Bytes as a Ziran literal, so each target spells it through string_literal. */
static void
ziran_literal_of(const unsigned char *bytes, size_t length, char *out, size_t size)
{
    size_t used = 0;
    out[used++] = '"';
    for(size_t i = 0; i < length && used + 6 < size; i++) {
        unsigned char byte = bytes[i];
        if(byte == '"' || byte == '\\') {
            out[used++] = '\\';
            out[used++] = (char)byte;
        } else if(byte == '\n') {
            out[used++] = '\\';
            out[used++] = 'n';
        } else if(byte == '\t') {
            out[used++] = '\\';
            out[used++] = 't';
        } else if(byte == '\r') {
            out[used++] = '\\';
            out[used++] = 'r';
        } else if(byte < 0x20 || byte == 0x7f)
            used += (size_t)snprintf(out + used, size - used, "\\x%02x", byte);
        else
            out[used++] = (char)byte;
    }
    out[used++] = '"';
    out[used] = '\0';
}

static void
print_run_flush(Emitter *e, PrintRun *run, int format_expr)
{
    char source[ZIR_TEXT_MAX], literal[ZIR_TEXT_MAX];
    ZirExpr piece;
    if(run->format_length == 0) {
        memset(run, 0, sizeof(*run));
        return;
    }
    piece = e->fn->exprs[format_expr];
    /* Go prints text alone with fmt.Print; C always uses printf. */
    if(run->values == 0 && e->target == ZIR_GO)
        ziran_literal_of(run->plain, run->plain_length, source, sizeof(source));
    else
        ziran_literal_of(run->format, run->format_length, source, sizeof(source));
    copy_text(piece.text, sizeof(piece.text), source);
    string_literal(&piece, e->target, literal, sizeof(literal));
    if(e->target == ZIR_GO)
        line(e, run->values ? "fmt.Printf(%s%s)" : "fmt.Print(%s%s)",
             literal, run->arguments);
    else
        line(e, "printf(%s%s);", literal, run->arguments);
    memset(run, 0, sizeof(*run));
}

static void
emit_print(Emitter *e, const ZirExpr *expr)
{
    PrintPiece *pieces = calloc(PRINT_PIECES_MAX, sizeof(*pieces));
    char (*values)[ZIR_TEXT_MAX] = calloc(PRINT_PIECES_MAX, sizeof(*values));
    int capture = 0;
    const char *types[PRINT_PIECES_MAX];
    char value[ZIR_TEXT_MAX], literal[ZIR_TEXT_MAX];
    int first = expr->first_child, count, argument = 0;
    if(pieces == NULL || values == NULL || first < 0 ||
       (count = PrintFormatPieces(e->fn->exprs[first].text, pieces,
                                  PRINT_PIECES_MAX)) < 0)
        fatal(expr, "invalid checked print format");
    /* Without calls no argument has an effect, so they read in place. */
    for(int child = e->fn->exprs[first].next_sibling; child >= 0;
        child = e->fn->exprs[child].next_sibling)
        capture |= expression_calls(e->fn, child);
    for(int child = e->fn->exprs[first].next_sibling; child >= 0;
        child = e->fn->exprs[child].next_sibling, argument++) {
        types[argument] = ScalarType(e->fn->exprs[child].type);
        emit_expr(e, child, e->fn->exprs[child].type, value, sizeof(value));
        if(capture) {
            fresh(e, values[argument]);
            declare(e, values[argument], types[argument], value);
        } else
            copy_text(values[argument], ZIR_TEXT_MAX, value);
    }
    /* Each language's standard printing: one printf or fmt.Printf per run
     * of text and values. C has no shortest round-trip float format, so a
     * float prints through ZirPrintFloat between printf calls. */
    PrintRun run = {0};
    argument = 0;
    for(int i = 0; i < count; i++) {
        const char *type, *name;
        char operand[ZIR_TEXT_MAX];
        if(!pieces[i].is_argument) {
            unsigned char bytes[ZIR_TEXT_MAX];
            size_t length;
            if(!DecodeStringLiteral(pieces[i].literal, bytes, sizeof(bytes), &length))
                fatal(expr, "invalid checked print format");
            if(e->target != ZIR_GO && memchr(bytes, 0, length) != NULL) {
                /* printf formats end at NUL; such text is written as bytes. */
                ZirExpr piece = e->fn->exprs[first];
                print_run_flush(e, &run, first);
                copy_text(piece.text, sizeof(piece.text), pieces[i].literal);
                string_literal(&piece, e->target, literal, sizeof(literal));
                line(e, "fwrite(%s, 1, %zu, stdout);", literal, length);
                continue;
            }
            print_run_text(&run, bytes, length);
            continue;
        }
        type = types[argument];
        name = values[argument++];
        if(plain_identifier(name))
            copy_text(operand, sizeof(operand), name);
        else
            format(operand, sizeof(operand), "(%s)", name);
        if(e->target == ZIR_GO) {
            if(!strcmp(type, "string"))
                print_run_value(&run, "%s", name);
            else if(!strcmp(type, "bool"))
                print_run_value(&run, "%t", name);
            else if(!strcmp(type, "float64") || !strcmp(type, "float32")) {
                char text[ZIR_TEXT_MAX];
                format(text, sizeof(text), !strcmp(type, "float64") ?
                       "formatFloat(%s, 64)" : "formatFloat(float64(%s), 32)", name);
                print_run_value(&run, "%s", text);
            } else
                print_run_value(&run, "%d", name);
            continue;
        }
        if(!strcmp(type, "float32") || !strcmp(type, "float64")) {
            print_run_flush(e, &run, first);
            line(e, !strcmp(type, "float32") ? "ZirPrintFloat((double)%s, 1);" :
                 "ZirPrintFloat(%s, 0);", name);
        } else if(!strcmp(type, "string")) {
            char text[ZIR_TEXT_MAX];
            format(text, sizeof(text), "(int)%s.length, %s.data", operand, operand);
            print_run_value(&run, "%.*s", text);
        } else if(!strcmp(type, "bool")) {
            char text[ZIR_TEXT_MAX];
            format(text, sizeof(text), "%s ? \"true\" : \"false\"", operand);
            print_run_value(&run, "%s", text);
        } else {
            /* A literal is written as a long long constant; other values
             * convert to the width printf expects. */
            char text[ZIR_TEXT_MAX];
            uint64_t bits;
            int is_unsigned = type[0] == 'u';
            if(integer_literal_bits(name, &bits))
                format(text, sizeof(text), is_unsigned ? "%lluULL" : "%lldLL",
                       is_unsigned ? (unsigned long long)bits : (unsigned long long)(long long)bits);
            else
                format(text, sizeof(text), is_unsigned ? "(unsigned long long)%s" :
                       "(long long)%s", operand);
            print_run_value(&run, is_unsigned ? "%llu" : "%lld", text);
        }
    }
    print_run_flush(e, &run, first);
    free(pieces);
    free(values);
}

/* Keep collection lowering outside recursive expression lowering: its large
 * target buffers must not increase every nested expression stack frame. */
static void
emit_vec_call(Emitter *e, const ZirExpr *expr, char *out, size_t size)
{
    int first = expr->first_child;
    int push = !strcmp(expr->name, "VecPush");
    char vector[ZIR_TEXT_MAX], value[ZIR_TEXT_MAX];
    char item_name[ZIR_NAME_MAX], result_name[ZIR_NAME_MAX];
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    if(first < 0 || !VecElementType(e->module,
        e->fn->exprs[first].type, element, sizeof(element)))
        fatal(expr, "invalid Vec operation");
    emit_destination(e, first, vector, sizeof(vector));
    if(!strcmp(expr->name, "VecSwap")) {
        int second = e->fn->exprs[first].next_sibling;
        char other[ZIR_TEXT_MAX], tmp[ZIR_NAME_MAX];
        emit_destination(e, second, other, sizeof(other));
        fresh(e, tmp);
        declare(e, tmp, e->fn->exprs[first].type, vector);
        line(e, "%s = %s%s", vector, other,
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s = %s%s", other, tmp,
             e->target == ZIR_GO ? "" : ";");
        out[0] = '\0'; e->pure = 0; return;
    }
    if(push) {
        int second = e->fn->exprs[first].next_sibling;
        if(second < 0) fatal(expr, "VecPush requires a value");
        emit_expr(e, second, element, value, sizeof(value));
        fresh(e, item_name);
        declare(e, item_name, element, value);
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s)",
                 vector, vector, item_name);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            char grown[ZIR_NAME_MAX];
            fresh(e, grown);
            line(e, "void *%s = ZirVecGrow((void *)(%s).data, &(%s).capacity, (%s).count, sizeof(*(%s).data));",
                 grown, vector, vector, vector, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", vector, mapped, grown);
            line(e, "(%s).data[(%s).count++] = %s;",
                 vector, vector, item_name);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecPop") || !strcmp(expr->name, "VecGet")) {
        int get = !strcmp(expr->name, "VecGet");
        char index[ZIR_TEXT_MAX];
        char initializer[ZIR_NAME_MAX * 2];
        zero_record(e, expr->type, initializer, sizeof(initializer));
        fresh(e, result_name);
        declare(e, result_name, expr->type, initializer);
        if(get) {
            int second = e->fn->exprs[first].next_sibling;
            if(second < 0) fatal(expr, "VecGet requires an index");
            emit_expr(e, second, "s64", index, sizeof(index));
        }
        if(e->target == ZIR_GO) {
            if(get)
                line(e, "if %s >= 0 && int64(%s) < %s.Count {", index, index,
                     vector);
            else
                line(e, "if %s.Count > 0 {", vector);
            e->indent++;
            if(get)
                line(e, "%s.Value = %s.Data[%s]", result_name, vector, index);
            else {
                line(e, "%s.Count--", vector);
                line(e, "%s.Value = %s.Data[%s.Count]", result_name, vector,
                     vector);
            }
            line(e, "%s.HasValue = true", result_name);
            e->indent--;
            line(e, "}");
        } else {
            char field[ZIR_TEXT_MAX * 2];
            if(get)
                line(e, "if (%s >= 0 && %s < (%s).count) {", index, index,
                     vector);
            else
                line(e, "if ((%s).count > 0) {", vector);
            e->indent++;
            if(get)
                format(field, sizeof(field), "(%s).data[%s]", vector, index);
            else {
                line(e, "(%s).count--;", vector);
                format(field, sizeof(field), "(%s).data[(%s).count]", vector,
                       vector);
            }
            {
                char destination[ZIR_NAME_MAX * 2];
                format(destination, sizeof(destination), "%s.value",
                       result_name);
                assign_value(e, destination, element, field);
            }
            line(e, "%s.has_value = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecClone")) {
        int second = e->fn->exprs[first].next_sibling;
        char source[ZIR_TEXT_MAX], grown[ZIR_NAME_MAX];
        if(second < 0) fatal(expr, "VecClone requires a source Vec");
        emit_destination(e, second, source, sizeof(source));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s.Data...)",
                 vector, vector, source);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            fresh(e, grown);
            line(e, "if ((%s).count > 0) {", source);
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (%s).count, sizeof(*(%s).data));",
                 grown, vector, vector, vector, source, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (%s *)%s;", vector, mapped, grown);
            line(e, "memcpy((%s).data, (%s).data, (size_t)(%s).count * sizeof(*(%s).data));",
                 vector, source, source, vector);
            line(e, "(%s).count = (%s).count;", vector, source);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
            e->indent--;
            line(e, "} else {");
            e->indent++;
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "VecSlice")) {
        int second = e->fn->exprs[first].next_sibling;
        int third = second >= 0 ? e->fn->exprs[second].next_sibling : -1;
        char low[ZIR_TEXT_MAX], high[ZIR_TEXT_MAX];
        char view[ZIR_NAME_MAX];
        if(second < 0 || third < 0)
            fatal(expr, "VecSlice requires low and high bounds");
        emit_expr(e, second, "s64", low, sizeof(low));
        emit_expr(e, third, "s64", high, sizeof(high));
        fresh(e, view);
        if(e->target == ZIR_GO) {
            line(e, "if %s < 0 || %s < %s || %s > %s.Count { panic(\"slice range out of bounds\") }",
                 low, high, low, high, vector);
            format(out, size, "%s.Data[%s:%s]", vector, low, high);
        } else {
            line(e, "Slice %s = {(%s).data, (%s).count};", view, vector,
                 vector);
            format(out, size, "SliceRange(%s, %s, %s, sizeof(*(%s).data))",
                   view, low, high, vector);
        }
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "BuilderAppend")) {
        int second = e->fn->exprs[first].next_sibling;
        char text[ZIR_TEXT_MAX];
        if(second < 0) fatal(expr, "BuilderAppend requires text");
        emit_expr(e, second, "string", text, sizeof(text));
        fresh(e, result_name);
        declare(e, result_name, "bool", "false");
        if(e->target == ZIR_GO) {
            line(e, "%s.Data = append(%s.Data, %s...)",
                 vector, vector, text);
            line(e, "%s.Count = int64(len(%s.Data))", vector, vector);
            line(e, "%s.Capacity = int64(cap(%s.Data))", vector, vector);
            line(e, "%s = true", result_name);
        } else {
            char grown[ZIR_NAME_MAX];
            fresh(e, grown);
            line(e, "if ((%s).length == 0) {", text);
            e->indent++;
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "} else {");
            e->indent++;
            line(e, "void *%s = ZirVecReserve((void *)(%s).data, &(%s).capacity, (%s).count, (int64_t)(%s).length, sizeof(*(%s).data));",
                 grown, vector, vector, vector, text, vector);
            line(e, "if (%s != NULL) {", grown);
            e->indent++;
            line(e, "(%s).data = (uint8_t *)%s;", vector, grown);
            line(e, "memcpy((%s).data + (%s).count, (%s).data, (size_t)(%s).length);",
                 vector, vector, text, text);
            line(e, "(%s).count += (int64_t)(%s).length;", vector, text);
            line(e, "%s = true;", result_name);
            e->indent--;
            line(e, "}");
            e->indent--;
            line(e, "}");
        }
        copy_text(out, size, result_name);
        e->pure = 1;
        return;
    }
    if(!strcmp(expr->name, "BuilderFinish")) {
        char finished[ZIR_NAME_MAX];
        fresh(e, finished);
        if(e->target == ZIR_GO) {
            declare(e, finished, "string", "\"\"");
            line(e, "%s = string(%s.Data[:%s.Count])",
                 finished, vector, vector);
            line(e, "%s.Data = nil", vector);
        } else {
            declare(e, finished, "string",
                    e->target == ZIR_CPP ? "{}" : "{NULL, 0}");
            line(e, "%s.data = (%s).count > 0 ? (const char *)(%s).data : \"\";",
                 finished, vector, vector);
            line(e, "%s.length = (size_t)(%s).count;", finished, vector);
            /* The finished string borrows the builder's bytes; the builder
             * detaches without freeing them. */
            line(e, "(%s).data = NULL;", vector);
        }
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Count" : "count",
             e->target == ZIR_GO ? "" : ";");
        copy_text(out, size, finished);
        e->pure = 0;
        return;
    }
    if(!strcmp(expr->name, "VecFree")) {
        if(e->target == ZIR_GO)
            line(e, "%s.Data = nil", vector);
        else {
            line(e, "free((%s).data);", vector);
            line(e, "(%s).data = NULL;", vector);
        }
        line(e, "%s.%s = 0%s", vector,
             e->target == ZIR_GO ? "Capacity" : "capacity",
             e->target == ZIR_GO ? "" : ";");
    } else if(e->target == ZIR_GO)
        line(e, "%s.Data = %s.Data[:0]", vector, vector);
    line(e, "%s.%s = 0%s", vector,
         e->target == ZIR_GO ? "Count" : "count",
         e->target == ZIR_GO ? "" : ";");
    out[0] = '\0';
    e->pure = 0;
}

static void
native_size_expression(Emitter *e, const char *type, char *out, size_t size)
{
    char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX * 2];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        char item_size[ZIR_TEXT_MAX], bound[ZIR_NAME_MAX];
        native_size_expression(e, element, item_size, sizeof(item_size));
        if(capacity >= 0) snprintf(bound, sizeof(bound), "%d", capacity);
        else {
            const char *close = strchr(type, ']');
            char source_bound[ZIR_NAME_MAX];
            format(source_bound, sizeof(source_bound), "%.*s",
                   (int)(close - type - 1), type + 1);
            e->resolve(e->context, source_bound, bound, sizeof(bound));
        }
        format(out, size, "((%s) * (%s))", bound, item_size);
        return;
    }
    if(!strcmp(ScalarType(type), "void")) {
        copy_text(out, size, "0");
        return;
    }
    if(type[0] == '*') {
        copy_text(out, size, e->target == ZIR_GO ?
                  "unsafe.Sizeof((*byte)(nil))" : "sizeof(void *)");
        return;
    }
    if(e->target == ZIR_GO && SliceElementType(type, NULL, 0)) {
        copy_text(out, size, "unsafe.Sizeof([]byte(nil))");
        return;
    }
    const char *native = TargetType(type, e->target);
    if(native != NULL) copy_text(mapped, sizeof(mapped), native);
    else e->resolve(e->context, type, mapped, sizeof(mapped));
    if(e->target == ZIR_GO)
        format(out, size, "unsafe.Sizeof(*new(%s))", mapped);
    else
        format(out, size, "sizeof(%s)", mapped);
}

static void
emit_expr(Emitter *e, int index, const char *expected, char *out, size_t size)
{
    const ZirExpr *expr=&e->fn->exprs[index];
    const char *type=canonical(expr->type);
    char a[ZIR_TEXT_MAX],b[ZIR_TEXT_MAX],result[ZIR_TEXT_MAX],temp[ZIR_NAME_MAX];
    int pure=0;
    /* Binary-shaped results re-bind when spliced into a parent expression, so
     * the tail parenthesizes them; identifiers, literals, calls, and slices
     * are operand-safe without wrapping. */
    int atom=1;
    if(!strcmp(expr->type,"integer") || !strcmp(expr->type,"real")) {
        const char *want=canonical(expected); if(*want && strcmp(want,"bool") && strcmp(want,"void")) type=want;
    }
    if(!strcmp(expr->type, "null") && strchr(expected, '*') != NULL)
        type = canonical(expected);
    switch(expr->kind) {
    case ZIR_EXPR_SIZE_OF:
        native_size_expression(e, expr->name, a, sizeof(a));
        if(e->target == ZIR_GO)
            format(result, sizeof(result), "%s(%s)", TargetType(type, e->target), a);
        else
            copy_text(result, sizeof(result), a);
        pure = 1;
        break;
    case ZIR_EXPR_COMPOUND: {
        if(ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare(e, temp, type, NULL);
            int ordinal = 0;
            for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
                const ZirExpr *entry = &e->fn->exprs[child];
                emit_expr(e, entry->right, entry->type, a, sizeof(a));
                format(b, sizeof(b), "%s[%d]", temp, ordinal++);
                assign_value(e, b, entry->type, a);
            }
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        zero_record(e, type, a, sizeof(a));
        fresh(e, temp);
        declare(e, temp, type, a);
        for(int child = expr->first_child; child >= 0; child = e->fn->exprs[child].next_sibling) {
            const ZirExpr *field = &e->fn->exprs[child];
            char field_name[ZIR_NAME_MAX];
            if(e->target == ZIR_GO)
                go_field_ident(field->name, field_name, sizeof(field_name));
            else
                TargetFieldName(field_record(e->module, type), e->target,
                                field->name, field_name, sizeof(field_name));
            emit_expr(e, field->right, field->type, a, sizeof(a));
            format(b, sizeof(b), "%s.%s", temp, field_name);
            assign_value(e, b, field->type, a);
        }
        copy_text(out, size, temp);
        e->pure = 1;
        return;
    }
    case ZIR_EXPR_MEMBER:
    case ZIR_EXPR_POINTER_MEMBER: {
        int base_pure;
        /* Reading a field needs a snapshot of that field, not a copy of every
         * enclosing record. Calls and other computed bases still evaluate once. */
        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, a, sizeof(a));
        else
            emit_expr(e, expr->left, e->fn->exprs[expr->left].type, a, sizeof(a));
        base_pure = e->pure;
        const char *base_type = e->fn->exprs[expr->left].type;
        int capacity;
        if(!strcmp(expr->name, "count") &&
           ArrayElementType(base_type, NULL, 0, &capacity)) {
            if(capacity < 0) {
                Diagnostic(expr->span, "emit.array_count",
                           "fixed array count requires a resolved bound");
                exit(1);
            }
            format(result, sizeof(result), "%d", capacity);
            pure = base_pure;
            break;
        }
        if(!strcmp(expr->name, "data") &&
           ArrayElementType(base_type, NULL, 0, &capacity)) {
            if(capacity == 0) {
                if(e->target == ZIR_GO) {
                    char element[ZIR_NAME_MAX];
                    char mapped[ZIR_NAME_MAX * 2];
                    ArrayElementType(base_type, element, sizeof(element), NULL);
                    const char *scalar = TargetType(element, ZIR_GO);
                    if(scalar != NULL) copy_text(mapped, sizeof(mapped), scalar);
                    else e->resolve(e->context, element, mapped,
                                    sizeof(mapped));
                    format(result, sizeof(result), "(*%s)(nil)", mapped);
                } else
                    copy_text(result, sizeof(result), "NULL");
            } else if(e->target == ZIR_GO)
                format(result, sizeof(result), "&(%s)[0]", a);
            else
                copy_text(result, sizeof(result), a);
            pure = base_pure;
            break;
        }
        if((!strcmp(base_type, "string") ||
            SliceElementType(base_type, NULL, 0)) &&
           !strcmp(expr->name, "count")) {
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "int64(len(%s))", a);
            else
                format(result, sizeof(result), "(int64_t)(%s).length", a);
            pure = base_pure;
            break;
        }
        emit_field_path(e->module, e->target, base_type, expr->name,
                        a, result, sizeof(result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, result);
            clear_owned_value(e, result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = base_pure;
        break;
    }
    case ZIR_EXPR_SLICE: {
        const char *base_type = e->fn->exprs[expr->left].type;
        char source[ZIR_TEXT_MAX], low[ZIR_TEXT_MAX], high[ZIR_TEXT_MAX];
        char element[ZIR_NAME_MAX], mapped[ZIR_NAME_MAX];
        char view[ZIR_NAME_MAX];
        int capacity = 0;
        int base_pure;
        int low_pure = 1;
        int high_pure = 1;
        if(!strcmp(base_type, "string")) {
            emit_expr(e, expr->left, "string", source, sizeof(source));
            base_pure = e->pure;
            fresh(e, view);
            if(e->target == ZIR_GO)
                line(e, "%s := %s", view, source);
            else
                line(e, "String %s = %s;", view, source);
            if(expr->right >= 0) {
                emit_expr(e, expr->right, "s64", low, sizeof(low));
                low_pure = e->pure;
            } else copy_text(low, sizeof(low), "0");
            if(expr->third >= 0) {
                emit_expr(e, expr->third, "s64", high, sizeof(high));
                high_pure = e->pure;
            } else format(high, sizeof(high), e->target == ZIR_GO ?
                          "len(%s)" : "%s.length", view);
            pure = base_pure && low_pure && high_pure;
            if(e->target == ZIR_GO) {
                line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) { panic(\"string range out of bounds\") }",
                     low, high, low, high, view);
                format(result, sizeof(result), "%s[%s:%s]", view, low, high);
            } else {
                format(result, sizeof(result), "StringRange(%s, (int64_t)%s, (int64_t)%s)",
                       view, low, high);
            }
            break;
        }
        int array = ArrayElementType(base_type, element, sizeof(element), &capacity);
        if(array) {
            emit_destination(e, expr->left, source, sizeof(source));
        } else {
            SliceElementType(base_type, element, sizeof(element));
            emit_expr(e, expr->left, base_type, source, sizeof(source));
        }
        base_pure = e->pure;
        fresh(e, view);
        if(array && capacity == 0 && e->target == ZIR_GO) {
            const char *scalar = TargetType(element, ZIR_GO);
            if(scalar != NULL) copy_text(mapped, sizeof(mapped), scalar);
            else e->resolve(e->context, element, mapped, sizeof(mapped));
            line(e, "var %s []%s", view, mapped);
        } else if(array && capacity == 0) {
            line(e, "Slice %s = {NULL, 0};", view);
        } else if(e->target == ZIR_GO) {
            line(e, "%s := %s[:]", view, source);
        } else if(array) {
            line(e, "Slice %s = {%s, %d};", view, source, capacity);
        } else {
            line(e, "Slice %s = %s;", view, source);
        }
        if(expr->right >= 0) {
            emit_expr(e, expr->right, "s64", low, sizeof(low));
            low_pure = e->pure;
        }
        else
            copy_text(low, sizeof(low), "0");
        if(expr->third >= 0) {
            emit_expr(e, expr->third, "s64", high, sizeof(high));
            high_pure = e->pure;
        }
        else
            format(high, sizeof(high), e->target == ZIR_GO ? "len(%s)" : "%s.length", view);
        pure = base_pure && low_pure && high_pure;
        if(e->target == ZIR_GO) {
            line(e, "if int64(%s) < 0 || int64(%s) < int64(%s) || int64(%s) > int64(len(%s)) { panic(\"slice range out of bounds\") }",
                 low, high, low, high, view);
            format(result, sizeof(result), "%s[int64(%s):int64(%s):int64(%s)]", view, low, high, high);
        } else {
            const char *scalar = TargetType(element, e->target);
            if(scalar != NULL)
                copy_text(mapped, sizeof(mapped), scalar);
            else
                e->resolve(e->context, element, mapped, sizeof(mapped));
            format(result, sizeof(result), "SliceRange(%s, (int64_t)%s, (int64_t)%s, sizeof(%s))",
                   view, low, high, mapped);
        }
        break;
    }
    case ZIR_EXPR_INDEX: {
        const char *base_type = e->fn->exprs[expr->left].type;
        int capacity = 0;
        int base_pure;

        if(member_path(e->fn, expr->left))
            emit_destination(e, expr->left, a, sizeof(a));
        else
            emit_expr(e, expr->left, base_type, a, sizeof(a));
        base_pure = e->pure;
        emit_expr(e, expr->right, "s32", b, sizeof(b));
        pure = base_pure && e->pure;
        int fixed_array = ArrayElementType(base_type, NULL, 0, &capacity);
        if(VecElementType(e->module, base_type, NULL, 0) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            format(result, sizeof(result),
                   "ZIRAN_VEC_INDEX((%s).data, (%s).count, %s)", a, a, b);
        } else if(VecElementType(e->module, base_type, NULL, 0)) {
            format(result, sizeof(result), "(%s).Data[%s]", a, b);
        } else if(SliceElementType(base_type, NULL, 0)) {
            slice_index(e, base_type, a, b, result, sizeof(result));
        } else if(e->target == ZIR_GO && base_type[0] == '*') {
            go_pointer_index(e, a, b, result, sizeof(result));
        } else if(!strcmp(base_type, "string")) {
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "%s[%s]", a, b);
            else
                format(result, sizeof(result), "(uint8_t)ZIRAN_INDEX(%s.data, %s.length, %s)", a, a, b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity == 0) {
            format(result, sizeof(result), "ZIRAN_EMPTY_INDEX(%s, %s)", a, b);
        } else if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                  fixed_array && capacity > 0) {
            /* Fixed-capacity arrays are bounds-checked in debug builds. */
            format(result, sizeof(result), "ZIRAN_INDEX(%s, %d, %s)",
                   a, capacity, b);
        } else {
            format(result, sizeof(result), "%s[%s]", a, b);
        }
        break;
    }
    case ZIR_EXPR_COMPILE_TIME:
        copy_text(result, sizeof(result), "false");
        pure = 1;
        break;
    case ZIR_EXPR_IDENT:
        if(expr->is_function_value) {
            emit_function_value(e, index, result, sizeof(result));
            break;
        }
        if(!strcmp(expr->name, "null")) {
            copy_text(result, sizeof(result), e->target == ZIR_GO ? "nil" :
                      e->target == ZIR_CPP ? "nullptr" : "((void *)0)");
            pure = 1;
            break;
        }
        resolve(e, expr->name, result, sizeof(result));
        if(expr->is_move) {
            if(!emitter_type_contains_vec(e->module, expr->type, 0))
                fatal(expr, "move requires an owned value");
            fresh(e, temp);
            declare(e, temp, expr->type, result);
            clear_owned_value(e, result, expr->type, e->module, 0);
            copy_text(out, size, temp);
            e->pure = 0;
            return;
        }
        pure = 1;
        break;
    case ZIR_EXPR_STRING:
        string_literal(expr, e->target, a, sizeof(a));
        if(e->target == ZIR_C || e->target == ZIR_CPP)
            format(result, sizeof(result), "StringLiteral(%s)", a);
        else
            copy_text(result, sizeof(result), a);
        pure = 1;
        break;
    case ZIR_EXPR_INT:
        literal(e,expr,type,0,result,sizeof(result));
        if(e->target == ZIR_CPP && enum_type(e->module, type)) {
            copy_text(a, sizeof(a), result);
            char native[ZIR_NAME_MAX * 2];
            e->resolve(e->context, type, native, sizeof(native));
            format(result, sizeof(result), "(%s)(%s)", native, a);
        }
        pure=1;
        break;
    case ZIR_EXPR_FLOAT: {
        copy_text(result,sizeof(result),expr->text);size_t n=strlen(result);
        if(n && (result[n-1]=='f' || result[n-1]=='F')) result[n-1]=0;
        pure = 1;
        break;
    }
    case ZIR_EXPR_CALL:
        if(!strcmp(expr->name, "TextView")) {
            if(expr->first_child < 0 ||
               e->fn->exprs[expr->first_child].next_sibling >= 0)
                fatal(expr, "TextView requires one []u8 argument");
            emit_expr(e, expr->first_child, "[]u8", a, sizeof(a));
            if(e->target == ZIR_GO) {
                format(out, size, "string(%s)", a);
            } else {
                fresh(e, temp);
                line(e, "Slice %s = %s;", temp, a);
                format(out, size,
                       "StringView((const char *)(%s).data, (size_t)(%s).length)",
                       temp, temp);
            }
            e->pure = 1;
            return;
        }
        if(!strcmp(expr->name, "VecPush") ||
           !strcmp(expr->name, "VecClear") ||
           !strcmp(expr->name, "VecFree") ||
           !strcmp(expr->name, "VecSwap") ||
           !strcmp(expr->name, "VecPop") ||
           !strcmp(expr->name, "VecGet") ||
           !strcmp(expr->name, "VecClone") ||
           !strcmp(expr->name, "VecSlice") ||
           !strcmp(expr->name, "BuilderAppend") ||
           !strcmp(expr->name, "BuilderFinish")) {
            emit_vec_call(e, expr, out, size);
            return;
        }
        if(!strcmp(expr->name, "print")) {
            emit_print(e, expr);
            out[0] = '\0';
            e->pure = 0;
            return;
        }
        if((e->target == ZIR_C || e->target == ZIR_CPP) &&
           ArrayElementType(type, NULL, 0, NULL)) {
            fresh(e, temp);
            declare_array(e, temp, type, NULL);
            emit_call(e, expr, temp, result, sizeof(result));
            line(e, "%s;", result);
            copy_text(out, size, temp);
            e->pure = 1;
            return;
        }
        emit_call(e, expr, NULL, result, sizeof(result));
        if(!strcmp(type,"void")) {line(e,"%s%s",result,e->target==ZIR_GO?"":";");out[0]=0;e->pure=0;return;}
        break;
    case ZIR_EXPR_CONDITIONAL:
        emit_expr(e,expr->left,"bool",a,sizeof(a));fresh(e,temp);
        const ZirType *declared = FindType(e->module, type, NULL);
        if(declared != NULL && declared->is_procedure_type) {
            if(e->target == ZIR_C || e->target == ZIR_CPP)
                format(b, sizeof(b), "(%s){0}", type);
            else
                copy_text(b, sizeof(b), e->target == ZIR_GO ? "nil" : "null");
        } else if(record_type(e->module, type))
            zero_record(e, type, b, sizeof(b));
        else
            copy_text(b, sizeof(b), zero_value(type, e->target));
        declare(e, temp, type, b);
        line(e, e->target == ZIR_GO ? "if %s {" : "if (%s) {", a);
        e->indent++;
        emit_expr(e, expr->right, type, b, sizeof(b));
        assign_value(e, temp, type, b);
        e->indent--;
        line(e, "} else {");
        e->indent++;
        emit_expr(e, expr->third, type, b, sizeof(b));
        assign_value(e, temp, type, b);
        e->indent--;
        line(e, "}");
        copy_text(out, size, temp);
        e->pure = 1;
        return;
    case ZIR_EXPR_BINARY: {
        const char *operand_type=type;
        int left_pure;
        if(!strcmp(type,"bool")) {
            operand_type=canonical(e->fn->exprs[expr->left].type);
            if(!strcmp(e->fn->exprs[expr->left].type,"integer") || !strcmp(e->fn->exprs[expr->left].type,"real")) operand_type=canonical(e->fn->exprs[expr->right].type);
            if(!strcmp(e->fn->exprs[expr->left].type, "null"))
                operand_type = canonical(e->fn->exprs[expr->right].type);
        }
        /* Procedure slots compare like pointers: against null through the
         * callable entry, and against another slot field by field. */
        const ZirType *operand_slot = FindType(e->module, operand_type, NULL);
        int slot_compare = operand_slot != NULL &&
                           operand_slot->is_procedure_type &&
                           (!strcmp(expr->op, "==") || !strcmp(expr->op, "!="));
        int left_is_null = e->fn->exprs[expr->left].kind == ZIR_EXPR_IDENT &&
                           !strcmp(e->fn->exprs[expr->left].name, "null");
        int right_is_null = e->fn->exprs[expr->right].kind == ZIR_EXPR_IDENT &&
                            !strcmp(e->fn->exprs[expr->right].name, "null");
        if(slot_compare && (left_is_null || right_is_null)) {
            int value_expr = left_is_null ? expr->right : expr->left;
            emit_expr(e, value_expr, operand_type, a, sizeof(a));
            if(e->target == ZIR_GO)
                format(result, sizeof(result), "%s %s nil", a, expr->op);
            else if(operand_slot->is_c_call)
                format(result, sizeof(result), "%s %s NULL", a, expr->op);
            else
                format(result, sizeof(result), "%s.call %s NULL", a, expr->op);
            atom = 0;
            pure = e->pure;
            break;
        }
        emit_expr(e,expr->left,operand_type,a,sizeof(a));
        left_pure = e->pure;
        if(!strcmp(expr->op,"&&") || !strcmp(expr->op,"||")) {
            fresh(e,temp);declare(e,temp,"bool",a);
            line(e,e->target==ZIR_GO?"if %s%s {":"if (%s%s) {",!strcmp(expr->op,"||")?"!":"",temp);e->indent++;
            emit_expr(e,expr->right,"bool",b,sizeof(b));line(e,"%s = %s%s",temp,b,e->target==ZIR_GO?"":";");
            e->indent--;line(e,"}");copy_text(out,size,temp);e->pure=1;return;
        }
        emit_expr(e,expr->right,(!strcmp(expr->op,"<<")||!strcmp(expr->op,">>"))?"s32":operand_type,b,sizeof(b));
        pure = left_pure && e->pure;
        if(slot_compare && !operand_slot->is_c_call &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            if(!strcmp(expr->op, "=="))
                format(result, sizeof(result),
                       "(%s.call == %s.call && %s.context == %s.context)", a, b, a, b);
            else
                format(result, sizeof(result),
                       "(%s.call != %s.call || %s.context != %s.context)", a, b, a, b);
            atom = 0;
            break;
        }
        if(!strcmp(operand_type, "string") && (e->target == ZIR_C || e->target == ZIR_CPP))
            format(result, sizeof(result), "%sStringEqual(%s, %s)", !strcmp(expr->op, "!=") ? "!" : "", a, b);
        else if(width(type) && operation(expr->op)) number(e,type,a,e->fn->exprs[expr->left].type,b,e->fn->exprs[expr->right].type,operation(expr->op),result,sizeof(result));
        else format(result,sizeof(result),"%s %s %s",a,expr->op,b);
        if(enum_flags_type(e->module, type) &&
           (e->target == ZIR_C || e->target == ZIR_CPP)) {
            copy_text(a, sizeof(a), result);
            format(result, sizeof(result), "(%s)(%s)", type, a);
        }
        atom=0;
        break;
    }
    case ZIR_EXPR_UNARY:
        if(!strcmp(expr->op,"-") && e->fn->exprs[expr->right].kind==ZIR_EXPR_INT) {
            literal(e,&e->fn->exprs[expr->right],type,1,result,sizeof(result));pure=1;break;
        }
        if(!strcmp(expr->op, "&"))
            emit_destination(e, expr->right, a, sizeof(a));
        else
            emit_expr(e,expr->right,e->fn->exprs[expr->right].type,a,sizeof(a));
        pure = e->pure;
        if(width(type) && !strcmp(expr->op,"-")) number(e,type,"0",NULL,a,e->fn->exprs[expr->right].type,2,result,sizeof(result));
        else if(width(type) && !strcmp(expr->op,"~")) {
            number(e,type,a,e->fn->exprs[expr->right].type,e->target==ZIR_GO?"^uint64(0)":"UINT64_MAX",NULL,10,result,sizeof(result));
        } else if(!strcmp(expr->op, "&") || !strcmp(expr->op, "*"))
            format(result, sizeof(result), "%s(%s)", expr->op, a);
        else format(result,sizeof(result),"%s%s",expr->op,a);
        break;
    case ZIR_EXPR_CAST: {
        const char *declared_type = type;
        const char *operand_type = e->fn->exprs[expr->right].type;
        int right_pure;
        if(enum_type(e->module, type))
            type = "s32";
        if(e->fn->exprs[expr->right].kind == ZIR_EXPR_INT &&
           width(type) >= 32 && !signed_type(type))
            operand_type = type;
        emit_expr(e,expr->right,operand_type,a,sizeof(a));
        right_pure = e->pure;
        pure = right_pure;
        if(!strcmp(type, "string")) {
            copy_text(result, sizeof(result), a);
        } else if(!strcmp(type,"bool")) {
            format(result,sizeof(result),"%s != %s",a,!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")?"false":"0");
            atom=0;
        } else if(!strcmp(canonical(e->fn->exprs[expr->right].type),"bool")) {
            fresh(e,temp);declare(e,temp,type,"0");
            line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",a);e->indent++;
            line(e,"%s = 1%s",temp,e->target==ZIR_GO?"":";");
            e->indent--;
            line(e, "}");
            copy_text(result, sizeof(result), temp);
        } else if(width(type)) {
            if(canonical(e->fn->exprs[expr->right].type)[0]=='f') {
                if(e->target==ZIR_GO) format(b,sizeof(b),"%s_float(float64(%s),%d,%s)",e->numbers,a,width(type),signed_type(type)?"true":"false");
                else format(b,sizeof(b),"%s_float(%s,%d,%s)",e->numbers,a,width(type),signed_type(type)?"true":"false");
                number(e,type,b,NULL,"0",NULL,0,result,sizeof(result));
            } else number(e,type,a,e->fn->exprs[expr->right].type,"0",NULL,0,result,sizeof(result));
        }
        else {
            char cast_native[ZIR_NAME_MAX];
            slot_native_type(type, e->target, cast_native, sizeof(cast_native));
            if(e->target==ZIR_GO) format(result,sizeof(result),"%s(%s)",cast_native,a);
            else format(result,sizeof(result),"(%s)(%s)",cast_native,a);
        }
        type = declared_type;
        break;
    }
    default: fatal(expr,"unsupported structured expression");
    }
    e->pure = pure;
    {
        /* A folded constant is a single operand, not a binary expression. */
        uint64_t constant;
        if(integer_literal_bits(result, &constant))
            atom = 1;
    }
    if(folds_text(e, result, type)) {
        /* declare() applies this cast for named enum types; inlined text has
         * to carry it so Go sees matching operand types. An explicit cast
         * keeps it even on a bare name: returning an s32 as an enum is not
         * an implicit conversion in C++ or Go. */
        if((!plain_identifier(result) || expr->kind == ZIR_EXPR_CAST) &&
           enum_type(e->module, type)) {
            const char *scalar = TargetType(type, e->target);
            char resolved[ZIR_NAME_MAX * 2];
            if(scalar == NULL) {
                e->resolve(e->context, type, resolved, sizeof(resolved));
                scalar = resolved;
            }
            if(e->target == ZIR_GO)
                format(out, size, "%s(%s)", scalar, result);
            else
                format(out, size, "((%s)(%s))", scalar, result);
        } else if(atom) {
            copy_text(out, size, result);
        } else {
            format(out, size, "(%s)", result);
        }
        e->pure = plain_identifier(result) ? 1 : pure;
        return;
    }
    fresh(e,temp);declare(e,temp,type,result);copy_text(out,size,temp);
    e->pure = 1;
}

static int
block_end(const ZirFunction *fn,int begin,int end)
{
    int depth=1;
    for(int i=begin+1;i<end;i++) {
        ZirStmtKind k=fn->stmts[i].kind;
        if(k==ZIR_STMT_IF||k==ZIR_STMT_WHILE||k==ZIR_STMT_BLOCK_OPEN)depth++;
        if(k==ZIR_STMT_BLOCK_CLOSE && !--depth)return i;
    }
    return end;
}

static void emit_sequence(Emitter *e,int begin,int end);

/* ---- #parallel for regions (native threading) ------------------------ */

typedef struct ParallelRegion {
    int while_index;
    int close;
    int body_begin;
    int body_end;
    char first[ZIR_NAME_MAX];
    char last[ZIR_NAME_MAX];
    char cursor[ZIR_NAME_MAX];
    char binder[ZIR_NAME_MAX];
    int forward;
    char captures[16][ZIR_NAME_MAX];
    char capture_types[16][ZIR_NAME_MAX];
    int capture_count;
    char worker[ZIR_NAME_MAX * 2];
} ParallelRegion;

static int
parallel_region_at(const ZirFunction *fn, int while_index,
                   ParallelRegion *region)
{
    const ZirStmt *st = &fn->stmts[while_index];
    const ZirStmt *cursor_decl, *last_decl, *first_decl;
    const ZirStmt *binder_decl, *index_decl, *advance;
    int close = -1, depth = 1;
    if(st->kind != ZIR_STMT_WHILE || !st->is_parallel || while_index < 3)
        return 0;
    cursor_decl = &fn->stmts[while_index - 1];
    last_decl = &fn->stmts[while_index - 2];
    first_decl = &fn->stmts[while_index - 3];
    binder_decl = &fn->stmts[while_index + 1];
    index_decl = &fn->stmts[while_index + 2];
    if(cursor_decl->kind != ZIR_STMT_DECL ||
       last_decl->kind != ZIR_STMT_DECL ||
       first_decl->kind != ZIR_STMT_DECL ||
       binder_decl->kind != ZIR_STMT_DECL ||
       index_decl->kind != ZIR_STMT_DECL ||
       strcmp(index_decl->name, "it_index") != 0)
        return 0;
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
    if(close < 0 || close - 1 <= while_index + 2)
        return 0;
    advance = &fn->stmts[close - 1];
    if(advance->kind != ZIR_STMT_ASSIGN ||
       advance->lhs_root < 0 ||
       fn->exprs[advance->lhs_root].kind != ZIR_EXPR_IDENT ||
       strcmp(fn->exprs[advance->lhs_root].name, cursor_decl->name))
        return 0;
    memset(region, 0, sizeof(*region));
    region->while_index = while_index;
    region->close = close;
    region->body_begin = while_index + 3;
    region->body_end = close - 1;
    /* The lowered tail carries the cursor advance and an inclusive-range
     * `if cursor == last { break; }` guard; the worker drives indices
     * itself, so both stay out of its body. */
    while(region->body_end > region->body_begin) {
        const ZirStmt *tail = &fn->stmts[region->body_end - 1];
        if(tail->kind == ZIR_STMT_ASSIGN && tail->lhs_root >= 0 &&
           fn->exprs[tail->lhs_root].kind == ZIR_EXPR_IDENT &&
           !strcmp(fn->exprs[tail->lhs_root].name, cursor_decl->name)) {
            region->body_end--;
            continue;
        }
        if(tail->kind == ZIR_STMT_BLOCK_CLOSE &&
           region->body_end - 2 > while_index &&
           fn->stmts[region->body_end - 2].kind == ZIR_STMT_BREAK &&
           fn->stmts[region->body_end - 3].kind == ZIR_STMT_IF) {
            const ZirStmt *guard = &fn->stmts[region->body_end - 3];
            int mentions_cursor = 0;
            int stack[64];
            int top = 0;
            if(guard->expr_root >= 0 && guard->expr_root < fn->expr_count)
                stack[top++] = guard->expr_root;
            while(top > 0 && !mentions_cursor) {
                const ZirExpr *expr = &fn->exprs[stack[--top]];
                if(expr->kind == ZIR_EXPR_IDENT &&
                   !strcmp(expr->name, cursor_decl->name))
                    mentions_cursor = 1;
                if(expr->left >= 0 && expr->left < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->left;
                if(expr->right >= 0 && expr->right < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->right;
                if(expr->third >= 0 && expr->third < fn->expr_count &&
                    top < 64)
                    stack[top++] = expr->third;
                for(int child = expr->first_child;
                    child >= 0 && child < fn->expr_count && top < 64;
                    child = fn->exprs[child].next_sibling)
                    stack[top++] = child;
            }
            if(mentions_cursor) {
                region->body_end -= 3;
                continue;
            }
        }
        break;
    }
    copy_text(region->first, sizeof(region->first), first_decl->name);
    copy_text(region->last, sizeof(region->last), last_decl->name);
    copy_text(region->cursor, sizeof(region->cursor), cursor_decl->name);
    copy_text(region->binder, sizeof(region->binder), binder_decl->name);
    region->forward = strstr(st->text, "<=") != NULL &&
                      strstr(st->text, ">=") == NULL;
    return 1;
}

static int
name_in_list(char list[][ZIR_NAME_MAX], int count, const char *name)
{
    for(int i = 0; i < count; i++)
        if(!strcmp(list[i], name))
            return 1;
    return 0;
}

static void
parallel_region_captures(const ZirModule *module, const ZirFunction *fn,
                         ParallelRegion *region)
{
    static char declared[64][ZIR_NAME_MAX];
    int declared_count = 0;
    if(region->body_end - region->body_begin > 4000)
        return;
    copy_text(declared[declared_count++], sizeof(declared[0]),
              region->binder);
    copy_text(declared[declared_count++], sizeof(declared[0]), "it_index");
    for(int i = region->body_begin; i < region->body_end &&
        declared_count < 60; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_DECL && st->name[0] &&
           !name_in_list(declared, declared_count, st->name)) {
            copy_text(declared[declared_count], sizeof(declared[0]),
                      st->name);
            declared_count++;
        }
    }
    for(int i = region->body_begin; i < region->body_end &&
        region->capture_count < 16; i++) {
        const ZirStmt *st = &fn->stmts[i];
        int roots[2] = {st->expr_root, st->lhs_root};
        for(int r = 0; r < 2; r++)
            for(int x = roots[r]; x >= 0 && x < fn->expr_count;
                x = fn->exprs[x].next_sibling) {
                const ZirExpr *expr = &fn->exprs[x];
                int global, taken, block_open, depth;
                const ZirStmt *decl = NULL;
                char type[ZIR_NAME_MAX];
                if(expr->kind != ZIR_EXPR_IDENT || !expr->name[0])
                    continue;
                if(name_in_list(declared, declared_count, expr->name) ||
                   !strcmp(expr->name, region->first) ||
                   !strcmp(expr->name, region->last) ||
                   !strcmp(expr->name, region->cursor))
                    continue;
                global = 0;
                for(int g = 0; g < module->global_count; g++)
                    if(!strcmp(module->globals[g].name, expr->name)) {
                        global = 1;
                        break;
                    }
                if(global)
                    continue;
                if(ResolveFunction(module, expr->name, NULL, NULL) == 1)
                    continue;
                block_open = -1;
                depth = 0;
                for(int s2 = region->while_index; s2 >= 0; s2--) {
                    ZirStmtKind kind = fn->stmts[s2].kind;
                    if(kind == ZIR_STMT_BLOCK_CLOSE)
                        depth++;
                    else if(kind == ZIR_STMT_BLOCK_OPEN ||
                            kind == ZIR_STMT_IF ||
                            kind == ZIR_STMT_WHILE ||
                            kind == ZIR_STMT_FOR ||
                            kind == ZIR_STMT_IF_CASE) {
                        if(depth == 0) {
                            block_open = s2;
                            break;
                        }
                        depth--;
                    }
                }
                for(int s2 = block_open; s2 >= 0 && decl == NULL; s2--) {
                    const ZirStmt *candidate = &fn->stmts[s2];
                    if(candidate->kind == ZIR_STMT_DECL &&
                       !strcmp(candidate->name, expr->name))
                        decl = candidate;
                }
                if(decl == NULL || !decl->type[0])
                    continue;
                taken = 0;
                for(int c = 0; c < region->capture_count; c++)
                    if(!strcmp(region->captures[c], expr->name)) {
                        taken = 1;
                        break;
                    }
                if(taken)
                    continue;
                copy_text(type, sizeof(type), decl->type);
                if(!strcmp(type, "null"))
                    continue;
                copy_text(region->captures[region->capture_count],
                          sizeof(region->captures[0]), expr->name);
                copy_text(region->capture_types[region->capture_count],
                          sizeof(region->capture_types[0]), type);
                region->capture_count++;
            }
    }
}

static int
parallel_region_fits(const ZirFunction *fn, int while_index)
{
    ParallelRegion region;
    return parallel_region_at(fn, while_index, &region) && region.forward;
}

static void
parallel_region_dispatch(Emitter *e, const ParallelRegion *region)
{
    char start_value[ZIR_TEXT_MAX], end_value[ZIR_TEXT_MAX];
    const ZirStmt *first_decl = &e->fn->stmts[region->while_index - 3];
    const ZirStmt *last_decl = &e->fn->stmts[region->while_index - 2];
    char ctx[ZIR_NAME_MAX];
    emit_expr(e, first_decl->expr_root, "s64", start_value,
              sizeof(start_value));
    emit_expr(e, last_decl->expr_root, "s64", end_value,
              sizeof(end_value));
    fresh(e, ctx);
    line(e, "{");
    e->indent++;
    line(e, "struct %s_ctx %s;", region->worker, ctx);
    line(e, "%s.start = %s;", ctx, start_value);
    line(e, "%s.end = %s;", ctx, end_value);
    {
        ParallelRegion full;
        parallel_region_at(e->fn, region->while_index, &full);
        parallel_region_captures(e->module, e->fn, &full);
        for(int c = 0; c < full.capture_count; c++)
            line(e, "%s.%s = %s;", ctx, full.captures[c],
                 full.captures[c]);
    }
    line(e, "ziran_parallel_run(%s_worker, %s.start, %s.end, &%s);",
         region->worker, ctx, ctx, ctx);
    e->indent--;
    line(e, "}");
}

int
FunctionHasParallelRegions(const ZirFunction *fn)
{
    for(int i = 0; i < fn->stmt_count; i++)
        if(fn->stmts[i].kind == ZIR_STMT_WHILE &&
           fn->stmts[i].is_parallel)
            return 1;
    return 0;
}

static void
emit_parallel_worker(Emitter *e, const ZirFunction *fn,
                     ParallelRegion *region, int ordinal)
{
    char function_name[ZIR_NAME_MAX];
    char mapped[ZIR_NAME_MAX * 2];
    const char *scalar = TargetType("s64", e->target);
    TargetBindingName(fn, e->target, fn->name, function_name,
                      sizeof(function_name));
    snprintf(region->worker, sizeof(region->worker), "ziran_par_%s_%d",
             function_name, ordinal);
    fprintf(e->out, "struct %s_ctx {\n", region->worker);
    fprintf(e->out, "    int64_t start;\n    int64_t end;\n");
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        const char *cap = TargetType(region->capture_types[c], e->target);
        if(cap != NULL)
            copy_text(type_mapped, sizeof(type_mapped), cap);
        else
            e->resolve(e->context, region->capture_types[c], type_mapped,
                       sizeof(type_mapped));
        fprintf(e->out, "    %s %s;\n", type_mapped,
                region->captures[c]);
    }
    fprintf(e->out, "};\n\n");
    fprintf(e->out,
            "static void %s_worker(int64_t low, int64_t high, "
            "void *opaque)\n{\n", region->worker);
    e->indent = 1;
    fprintf(e->out, "    struct %s_ctx *ziran_ctx = "
            "(struct %s_ctx *)opaque;\n", region->worker,
            region->worker);
    copy_text(mapped, sizeof(mapped), scalar ? scalar : "int64_t");
    line(e, "%s %s = ziran_ctx->start;", mapped, region->first);
    line(e, "%s %s = ziran_ctx->end;", mapped, region->last);
    for(int c = 0; c < region->capture_count; c++) {
        char type_mapped[ZIR_NAME_MAX * 2];
        const char *cap = TargetType(region->capture_types[c], e->target);
        if(cap != NULL)
            copy_text(type_mapped, sizeof(type_mapped), cap);
        else
            e->resolve(e->context, region->capture_types[c], type_mapped,
                       sizeof(type_mapped));
        line(e, "%s %s = ziran_ctx->%s;", type_mapped,
             region->captures[c], region->captures[c]);
    }
    line(e, "for (int64_t ziran_i = low; ziran_i <= high; "
            "ziran_i += 1) {");
    e->indent++;
    line(e, "%s %s = ziran_i;", mapped, region->binder);
    line(e, "%s it_index = ziran_i - %s;", mapped, region->first);
    emit_sequence(e, region->body_begin, region->body_end);
    e->indent--;
    line(e, "}");
    fprintf(e->out, "}\n\n");
}

void
EmitParallelWorkers(FILE *out, const ZirModule *module,
                    const ZirFunction *fn, ZirTarget target,
                    ZirResolveTarget resolve, void *context)
{
    Emitter e = {0};
    int ordinal = 0;
    if(!FunctionHasParallelRegions(fn))
        return;
    e.out = out;
    e.module = module;
    e.fn = fn;
    e.target = target;
    e.resolve = resolve;
    e.context = context;
    e.indent = 0;
    e.locals = calloc((size_t)fn->stmt_count + 65, sizeof(*e.locals));
    if(e.locals == NULL)
        return;
    e.minify = zir_minify_output;
    fprintf(out, "#include \"ziran_parallel.h\"\n\n");
    fprintf(out, "/* ziran: no GPU device capability; #parallel_gpu regions "
                 "run on CPU threads. */\n\n");
    for(int i = 0; i < fn->stmt_count; i++) {
        ParallelRegion region;
        if(!parallel_region_at(fn, i, &region))
            continue;
        if(!region.forward) {
            fprintf(out, "/* ziran: reverse #parallel region keeps serial "
                         "execution%s */\n",
                    fn->stmts[i].is_gpu ? "; GPU region on CPU" : "");
            continue;
        }
        parallel_region_captures(module, fn, &region);
        emit_parallel_worker(&e, fn, &region, ordinal++);
    }
    free(e.locals);
}

static void
zero_record(Emitter *e, const char *type, char *out, size_t size)
{
    if(e->target == ZIR_GO) {
        char target_type[ZIR_NAME_MAX * 2];
        e->resolve(e->context, type, target_type, sizeof(target_type));
        format(out, size, "%s{}", target_type);
        return;
    }
    copy_text(out, size, e->target == ZIR_CPP ||
              TypeHasZeroArray(e->module, type) ? "{}" : "{0}");
}

static int
emit_if(Emitter *e,int i,int end)
{
    char cond[ZIR_TEXT_MAX];
    int close=block_end(e->fn,i,end);
    char plain[ZIR_TEXT_MAX];
    emit_expr(e,e->fn->stmts[i].expr_root,"bool",cond,sizeof(cond));
    line(e,e->target==ZIR_GO?"if %s {":"if (%s) {",bare(cond,plain,sizeof(plain)));e->indent++;
    emit_sequence(e,i+1,close);e->indent--;
    if(close+1<end && e->fn->stmts[close+1].kind==ZIR_STMT_IF && e->fn->stmts[close+1].is_else) {
        int next=close+1;
        line(e,"} else {");e->indent++;
        if(e->fn->stmts[next].expr_root>=0)close=emit_if(e,next,end);
        else {close=block_end(e->fn,next,end);emit_sequence(e,next+1,close);}
        e->indent--;
    }
    line(e,"}");return close;
}

static int
expression_reads(const ZirFunction *fn, int index, const char *name)
{
    if(index < 0 || index >= fn->expr_count)
        return 0;
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_IDENT && !strcmp(expr->name, name))
        return 1;
    /* An array's count and data, including an array field's, can lower to
     * constants that never read the base. */
    if(expr->kind == ZIR_EXPR_MEMBER && expr->left >= 0 &&
       (!strcmp(expr->name, "count") || !strcmp(expr->name, "data")) &&
       ArrayElementType(fn->exprs[expr->left].type, NULL, 0, NULL))
        return 0;
    /* Slicing an empty array likewise lowers without its base; the bounds
     * are still read. */
    if(expr->kind == ZIR_EXPR_SLICE && expr->left >= 0 &&
       ArrayElementType(fn->exprs[expr->left].type, NULL, 0, NULL))
        return expression_reads(fn, expr->right, name) ||
               expression_reads(fn, expr->third, name);
    for(int child = expr->first_child; child >= 0; child = fn->exprs[child].next_sibling)
        if(expression_reads(fn, child, name))
            return 1;
    return expression_reads(fn, expr->left, name) ||
           expression_reads(fn, expr->right, name) ||
           expression_reads(fn, expr->third, name);
}

/* Go rejects a local that is never read. A binding needs no `_ =` when a
 * later statement reads it and no other declaration reuses its name, so
 * the read cannot belong to a shadowing binding. Assigning to the whole
 * variable is not a read. */
static int
go_binding_read_later(const ZirFunction *fn, int declaration)
{
    const char *name = fn->stmts[declaration].name;
    int read = 0;
    /* A fixed array's count, data, and empty slices can lower to constants
     * that never read the variable. */
    if(ArrayElementType(fn->stmts[declaration].type, NULL, 0, NULL))
        return 0;
    for(int s = 0; s < fn->stmt_count; s++) {
        const ZirStmt *st = &fn->stmts[s];
        if(s != declaration && st->kind == ZIR_STMT_DECL && !strcmp(st->name, name))
            return 0;
        if(s <= declaration || read)
            continue;
        for(int root = st->expr_root; root >= 0 && !read; root = fn->exprs[root].next_sibling)
            read = expression_reads(fn, root, name);
        if(!read && st->lhs_root >= 0 && fn->exprs[st->lhs_root].kind != ZIR_EXPR_IDENT)
            read = expression_reads(fn, st->lhs_root, name);
    }
    return read;
}

static void
emit_sequence(Emitter *e,int begin,int end)
{
    int saved=e->local_count;e->depth++;
    e->sequence_terminated=0;
    for(int i=begin;i<end;i++) {
        const ZirStmt *st=&e->fn->stmts[i];
        char value[ZIR_TEXT_MAX],lhs[ZIR_TEXT_MAX],result[ZIR_TEXT_MAX];
        switch(st->kind) {
        case ZIR_STMT_DECL:
            if(st->expr_root>=0)emit_expr(e,st->expr_root,st->type,value,sizeof(value));
            else if(record_type(e->module, st->type)) {
                zero_record(e, st->type, value, sizeof(value));
            }
            else copy_text(value, sizeof(value), zero_value(st->type, e->target));
            declare(e,st->name,st->type,value);
            if(e->target == ZIR_GO && !go_binding_read_later(e->fn, i)) {
                char binding[ZIR_NAME_MAX];
                TargetBindingName(e->fn, e->target, st->name, binding,
                                  sizeof(binding));
                line(e, "_ = %s", binding);
            }
            track_local(e, st->name, st->type);
            break;
        case ZIR_STMT_ASSIGN:
            emit_destination(e, st->lhs_root, lhs, sizeof(lhs));
            if(strcmp(st->assignment_op,"=")) {
                /* The target's old value reads in place unless a call on
                 * either side could run first or change it. */
                char old[ZIR_TEXT_MAX];
                if(expression_calls(e->fn, st->lhs_root) ||
                   expression_calls(e->fn, st->expr_root) ||
                   ArrayElementType(e->fn->exprs[st->lhs_root].type, NULL, 0, NULL)) {
                    fresh(e,old);
                    declare(e,old,e->fn->exprs[st->lhs_root].type,lhs);
                } else
                    copy_text(old, sizeof(old), lhs);
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                char op[4];copy_text(op,sizeof(op),st->assignment_op);op[strlen(op)-1]=0;
                const char *type=canonical(e->fn->exprs[st->lhs_root].type);
                if(width(type))number(e,type,old,type,value,e->fn->exprs[st->expr_root].type,operation(op),result,sizeof(result));
                else format(result,sizeof(result),"%s %s %s",old,op,value);
            } else {
                emit_expr(e,st->expr_root,e->fn->exprs[st->lhs_root].type,value,sizeof(value));
                copy_text(result,sizeof(result),value);
            }
            assign_value(e, lhs, e->fn->exprs[st->lhs_root].type, result);
            break;
        case ZIR_STMT_RETURN:
            if(st->expr_root >= 0) {
                emit_expr(e, st->expr_root, e->fn->return_type, value, sizeof(value));
                if((e->target == ZIR_C || e->target == ZIR_CPP) &&
                   ArrayElementType(e->fn->return_type, NULL, 0, NULL)) {
                    char output[ZIR_NAME_MAX];
                    ArrayAbiName(e->fn, -1, output, sizeof(output));
                    /* value is a captured true array; output is an ABI pointer. */
                    line(e, "memmove(%s, %s, sizeof(%s));", output, value, value);
                    drop_locals(e, 0);
                    line(e, e->target != ZIR_GO && NativeMainReturnsStatus(e->fn) ?
                         "return 0;" : "return;");
                } else {
                    if(has_owned_locals(e)) {
                        char returned[ZIR_NAME_MAX];
                        fresh(e, returned);
                        declare(e, returned, e->fn->return_type, value);
                        drop_locals(e, 0);
                        copy_text(value, sizeof(value), returned);
                    }
                    char plain[ZIR_TEXT_MAX];
                    line(e, "return %s%s", bare(value, plain, sizeof(plain)),
                         e->target == ZIR_GO ? "" : ";");
                }
            }
            else {
                drop_locals(e, 0);
                line(e,e->target==ZIR_GO?"return":
                       NativeMainReturnsStatus(e->fn)?"return 0;":"return;");
            }
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_UNREACHABLE:
            drop_locals(e, 0);
            line(e,e->target==ZIR_GO?"panic(\"unreachable\")":"abort();");
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_IF:i=emit_if(e,i,end);break;
        case ZIR_STMT_WHILE: {
            int close=block_end(e->fn,i,end);
            int labeled=0;
            if(st->is_parallel && e->target==ZIR_GO)
                line(e, st->is_gpu ?
                    "// ziran: #parallel_gpu region downgraded to serial (no GPU device)" :
                    "// ziran: #parallel region downgraded to serial");
            if(st->is_parallel && (e->target==ZIR_C || e->target==ZIR_CPP)) {
                ParallelRegion region;
                if(parallel_region_at(e->fn,i,&region) && region.forward) {
                    char function_name[ZIR_NAME_MAX];
                    int ordinal = 0;
                    TargetBindingName(e->fn,e->target,e->fn->name,
                                     function_name,sizeof(function_name));
                    for(int s2=0;s2<i;s2++)
                        if(e->fn->stmts[s2].kind==ZIR_STMT_WHILE &&
                           e->fn->stmts[s2].is_parallel &&
                           parallel_region_fits(e->fn,s2))
                            ordinal++;
                    snprintf(region.worker,sizeof(region.worker),
                             "ziran_par_%s_%d", function_name, ordinal);
                    parallel_region_dispatch(e,&region);
                    i=close;break;
                }
                line(e,"/* ziran: #parallel region downgraded to serial */");
            }
            for(int target=0;target<e->fn->stmt_count;target++)
                if(st->loop_id && e->fn->stmts[target].target_id==st->loop_id)
                    labeled=1;
            if(labeled && e->target==ZIR_GO)
                line(e,"zir_loop_%d:",st->loop_id);
            {
                /* A condition that needs no setup statements goes in the
                 * loop header; otherwise it runs first in each iteration. */
                char *scratch_text = NULL;
                size_t scratch_size = 0;
                FILE *saved_out = e->out;
                int saved_serial = e->serial;
                FILE *scratch = open_memstream(&scratch_text, &scratch_size);
                int header = 0;
                if(scratch != NULL) {
                    e->out = scratch;
                    emit_expr(e,st->expr_root,"bool",value,sizeof(value));
                    fclose(scratch);
                    e->out = saved_out;
                    header = scratch_size == 0;
                    free(scratch_text);
                }
                if(header) {
                    char plain[ZIR_TEXT_MAX];
                    line(e,e->target==ZIR_GO?"for %s {":"while (%s) {",
                         bare(value,plain,sizeof(plain)));
                    e->indent++;
                } else {
                    e->serial = saved_serial;
                    line(e,e->target==ZIR_GO?"for {":"while (true) {");e->indent++;
                    emit_expr(e,st->expr_root,"bool",value,sizeof(value));
                    line(e,e->target==ZIR_GO?"if !%s { break }":"if (!%s) { break; }",value);
                }
            }
            if(labeled && e->target!=ZIR_GO) {
                line(e,"{");e->indent++;
            }
            if(e->loop_count >= (int)(sizeof(e->loop_start) / sizeof(e->loop_start[0]))) {
                Diagnostic(st->span, "emit.loop_nesting",
                           "too many nested loops during emission");
                exit(1);
            }
            e->loop_start[e->loop_count] = e->local_count;
            e->loop_id[e->loop_count++] = st->loop_id;
            emit_sequence(e,i+1,close);
            e->loop_count--;
            if(labeled && e->target!=ZIR_GO) {
                e->indent--;line(e,"}");
                line(e,"zir_loop_continue_%d: ;",st->loop_id);
            }
            e->indent--;line(e,"}");
            if(labeled && e->target!=ZIR_GO)
                line(e,"zir_loop_break_%d: ;",st->loop_id);
            i=close;break;
        }
        case ZIR_STMT_BLOCK_OPEN: {
            int close=block_end(e->fn,i,end);line(e,"{");e->indent++;emit_sequence(e,i+1,close);e->indent--;line(e,"}");i=close;break;
        }
        case ZIR_STMT_BREAK:case ZIR_STMT_CONTINUE:
            if(e->loop_count > 0) {
                int target = e->loop_count - 1;
                if(st->target_id)
                    while(target >= 0 && e->loop_id[target] != st->target_id)
                        target--;
                if(target >= 0)
                    drop_locals(e, e->loop_start[target]);
            }
            if(st->target_id) {
                if(e->target==ZIR_GO)
                    line(e,"%s zir_loop_%d",st->kind==ZIR_STMT_BREAK?"break":"continue",st->target_id);
                else
                    line(e,"goto zir_loop_%s_%d;",st->kind==ZIR_STMT_BREAK?"break":"continue",st->target_id);
            } else
                line(e,"%s%s",st->kind==ZIR_STMT_BREAK?"break":"continue",e->target==ZIR_GO?"":";");
            e->local_count=saved;e->depth--;e->sequence_terminated=1;return;
        case ZIR_STMT_EXPR:case ZIR_STMT_UNUSED:
            if(st->expr_root>=0) {
                const ZirExpr *expr = &e->fn->exprs[st->expr_root];
                emit_expr(e, st->expr_root, expr->type, value,
                          sizeof(value));
                if(*value && expr->kind == ZIR_EXPR_CALL &&
                   VecElementType(e->module, expr->type, NULL, 0)) {
                    char temporary[ZIR_NAME_MAX];
                    fresh(e, temporary);
                    declare(e, temporary, expr->type, value);
                    drop_temporary_vec(e, temporary);
                } else if(*value)
                    line(e,e->target==ZIR_GO?"_ = %s":"(void)%s;",value);
            }break;
        default:break;
        }
    }
    drop_locals(e, saved);
    e->local_count=saved;e->depth--;e->sequence_terminated=0;
}

int
EmitBody(FILE *out,const ZirModule *module,const ZirFunction *fn,ZirTarget target,
            ZirResolveTarget resolver,void *context,const char *number_support)
{
    Emitter e={0};char params[64][ZIR_TEXT_MAX];int count;
    if(!CanEmitBody(module, fn))return 0;
    e.out=out;e.module=module;e.fn=fn;e.target=target;e.resolve=resolver;e.context=context;e.indent=1;
    e.minify = zir_minify_output;
    e.locals=calloc((size_t)fn->stmt_count+65,sizeof(*e.locals));
    if(!e.locals) {
        Diagnostic(fn->span, "emit.expression",
                   "out of memory during scalar emission");
        exit(1);
    }
    if(number_support && *number_support)
        copy_text(e.numbers, sizeof(e.numbers), number_support);
    else
        number_prefix(module,e.numbers,sizeof(e.numbers));
    count=*skip_ws(fn->args)?split_top_level(fn->args,params[0],64,sizeof(params[0])):0;
    for(int i=0;i<count;i++) {
        char *colon=strchr(params[i],':');*colon++=0;trim_in_place(params[i]);
        const char *type=canonical(skip_ws(colon));
        if((target == ZIR_C || target == ZIR_CPP) &&
           ArrayValueType(type)) {
            char incoming[ZIR_NAME_MAX];
            char binding[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, incoming, sizeof(incoming));
            TargetBindingName(fn, target, params[i], binding, sizeof(binding));
            declare_array(&e, binding, type, incoming);
        }
        track_local(&e, params[i], type);
    }
    emit_sequence(&e,0,fn->stmt_count);
    if(!e.sequence_terminated) {
        drop_locals(&e, 0);
        if(strcmp(fn->return_type, "void"))
            line(&e, target == ZIR_GO ? "panic(\"unreachable\")" : "abort();");
    }
    free(e.locals);return 1;
}
