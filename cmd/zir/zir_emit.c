#include "zir_scratch.h"
#include "zir_memory.h"
#include "zir_emit_internal.h"
#include "zir_scalar.h"

static void readable_module_name(const char *name, char *out, size_t size, int upper);

typedef struct ModuleIdentities {
    uintptr_t programs;
    int count;
    ModuleIdentity *slots;
    size_t capacity;
    size_t used;
} ModuleIdentities;

static void free_module_identities(void *value) { free(((ModuleIdentities *)value)->slots); }
static ModuleIdentities *context_module_identities(void)
{
    return CompilerContextStorage(CompilerContextCurrent(), COMPILER_NATIVE_IDENTITIES,
        sizeof(ModuleIdentities), free_module_identities);
}
#define module_identities (*context_module_identities())
void NativeNamesReset(void)
{
    ModuleIdentities *identities = context_module_identities();
    free(identities->slots);
    memset(identities, 0, sizeof(*identities));
}

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
     "\tbound := float64(uint64(1) << (bits - 1)) * 2\n"
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

static int
header_type_uses_import(const ZirModule *module, const char *type,
                        const ZirImport *import)
{
    if(type == NULL || import->resolved_module == NULL) return 0;
    char base[ZIR_NAME_MAX];
    copy_text(base, sizeof(base), type);
    for(int depth = 0; depth < 64; depth++) {
        char element[ZIR_NAME_MAX];
        if(ArrayElementType(base, element, sizeof(element), NULL) ||
           SliceElementType(base, element, sizeof(element))) {
            copy_text(base, sizeof(base), element);
            continue;
        }
        if(base[0] == '*') {
            memmove(base, base + 1, strlen(base));
            continue;
        }
        break;
    }
    const ZirModule *owner = NULL;
    return FindType(module, base, &owner) != NULL &&
           owner == import->resolved_module;
}

static int
header_parameters_use_import(const ZirModule *module, const char *args,
                              const ZirImport *import)
{
    const ZirParameters *parameters = ParametersOf(args);
    for(int index = 0; index < parameters->count; index++)
        if(header_type_uses_import(module, parameters->items[index].type, import))
            return 1;
    return 0;
}

int
NativeHeaderImportUse(const ZirModule *module, const ZirImport *import)
{
    if(import->kind != ZIR_IMPORT_OPEN && import->kind != ZIR_IMPORT_MODULE)
        return 0;
    if(import->required) return 1;
    int private_use = 0;
    for(int index = 0; index < module->type_count; index++) {
        const ZirType *type = &module->types[index];
        if(type->is_extern || type->is_enum || type->is_record_template) continue;
        int used = 0;
        if(type->is_procedure_type) {
            used = header_type_uses_import(module, type->procedure_return_type, import) ||
                   header_parameters_use_import(module, type->body, import);
        } else {
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(type, &offset, &field) == 1)
                if(header_type_uses_import(module, field.type, import)) { used = 1; break; }
        }
        if(used && type->is_public) return 1;
        if(used) private_use = 1;
    }
    for(int index = 0; index < module->function_count; index++) {
        const ZirFunction *function = &module->functions[index];
        if(function->is_template || !function->is_public) continue;
        if(header_type_uses_import(module, function->return_type, import) ||
           header_parameters_use_import(module, FunctionArgs(function), import)) return 1;
    }
    for(int index = 0; index < module->global_count; index++) {
        const ZirGlobal *global = &module->globals[index];
        if(!global->is_static && header_type_uses_import(module, global->type, import)) return 1;
    }
    for(int index = 0; index < module->import_count; index++) {
        const ZirImport *foreign = &module->imports[index];
        if(foreign->kind != ZIR_IMPORT_EXTERN || !foreign->is_public) continue;
        if(header_type_uses_import(module, foreign->return_type, import) ||
           header_parameters_use_import(module, foreign->args, import)) return 1;
    }
    return private_use ? 2 : 0;
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

/* Globals are emitted under their own names, so a local or parameter
 * named like a global the function reads would hide it: `count :=
 * Media.count + 1` read the new, uninitialized local. Such a binding takes
 * another name. The globals each function reads are listed once, since
 * every use of a local asks for its name. */
static int
function_reads_global(const ZirFunction *fn, const char *name)
{
    static _Thread_local const ZirFunction *function;
    static _Thread_local const ZirExpr *exprs;
    static _Thread_local int expr_count;
    static _Thread_local const char **globals;
    static _Thread_local int global_count, global_capacity;
    if(function != fn || exprs != fn->exprs || expr_count != fn->expr_count) {
        function = fn;
        exprs = fn->exprs;
        expr_count = fn->expr_count;
        global_count = 0;
        for(int i = 0; i < fn->expr_count; i++) {
            const ZirExpr *expr = &fn->exprs[i];
            if(!expr->is_global_value || expr->name == NULL)
                continue;
            if(global_count == global_capacity) {
                int capacity = global_capacity ? global_capacity * 2 : 16;
                const char **grown = AllocateOrExit(sizeof(*grown) * (size_t)capacity);
                if(global_count)
                    memcpy(grown, globals, sizeof(*grown) * (size_t)global_count);
                free(globals);
                globals = grown;
                global_capacity = capacity;
            }
            const char *bare = strrchr(expr->name, '.');
            globals[global_count++] = bare ? bare + 1 : expr->name;
        }
    }
    for(int i = 0; i < global_count; i++)
        if(!strcmp(globals[i], name))
            return 1;
    return 0;
}

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
/* C++ hosts include generated C headers, so exported identifiers and
 * type names also avoid these words. */
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
    "formatFloat", "unsafe", "zirGoAssemblyEnabled", NULL
};

static int
keyword(const char *name, const char *const *words)
{
    for(; *words; words++)
        if(!strcmp(name, *words))
            return 1;
    return 0;
}

int
NativeHeaderKeyword(const char *name)
{
    return keyword(name, c_keywords) || keyword(name, cpp_keywords);
}

void
TargetBindingName(const ZirFunction *fn, ZirTarget target,
                  const char *name, char *out, size_t size)
{
    int reserved = keyword(name, target == ZIR_GO ? go_keywords : c_keywords);
    if(!reserved && (fn == NULL || !function_reads_global(fn, name))) {
        copy_text(out, size, name);
        return;
    }
    for(int serial = 0; ; serial++) {
        format(out, size, reserved ? "ziran_keyword_%s_%d" : "ziran_local_%s_%d", name, serial);
        if(fn == NULL || (!function_mentions(fn, out) && !function_reads_global(fn, out)))
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
    int is_type;
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
native_names_add(NativeValueNames *names, const char *name, int *collision,
                 int is_type)
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
    names->items[names->count] = (NativeValueName){copy, collision, is_type,
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

/* C typedefs share the ordinary identifier namespace with values. Rename the type when the final value name remains unchanged;
 * duplicate values already have their own native names. Identical concrete
 * types may share a name, so type/type ownership stays with CheckPrograms. */
static void
native_names_mark_type_values(const NativeValueNames *names)
{
    for(int bucket = 0; bucket < names->bucket_count; bucket++)
        for(int i = names->buckets[bucket]; i >= 0; i = names->items[i].next) {
            const NativeValueName *type = &names->items[i];
            if(!type->is_type || *type->collision) continue;
            for(int j = names->buckets[bucket]; j >= 0; j = names->items[j].next) {
                const NativeValueName *value = &names->items[j];
                if(!value->is_type && !*value->collision &&
                   !strcmp(type->name, value->name)) {
                    *type->collision = 1;
                    break;
                }
            }
        }
}

int
MarkNativeNameCollisions(ZirProgram **programs, int count)
{
    const ZirProgram *native_programs[count > 0 ? count : 1];
    NativeValueNames names[2] = {{0}, {0}}; /* C, Go */
    int values = 0, ok = 1;
    for(int p = 0; p < count; p++) {
        native_programs[p] = programs[p];
        for(int m = 0; m < programs[p]->module_count; m++)
            values += programs[p]->modules[m].global_count +
                      programs[p]->modules[m].define_count +
                      programs[p]->modules[m].type_count;
    }
    for(int t = 0; t < 2; t++) {
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
                for(int t = 0; ok && t < 2; t++) {
                    char native[ZIR_NAME_MAX * 2];
                    target_top_name(module, t == 0 ? ZIR_C : ZIR_GO,
                                    name, is_global, native, sizeof(native));
                    ok = native_names_add(&names[t], native, collision, 0);
                }
            }
        }
    for(int t = 0; ok && t < 2; t++)
        native_names_mark_shared(&names[t]);
    for(int p = 0; ok && p < count; p++)
        for(int m = 0; ok && m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; ok && t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                if(type->is_record_template ||
                   (type->is_extern && !type->foreign_target[0])) continue;
                char native[ZIR_NAME_MAX * 2];
                NativeTypeName(module, type, native, sizeof(native));
                ok = native_names_add(&names[0], native,
                                      &type->native_name_mangled, 1);
            }
        }
    for(int p = 0; ok && p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *function = &module->functions[f];
                char native[ZIR_NAME_MAX * 2];
                if(function->is_template) continue;
                NativeCFunctionName(module, function, native, sizeof(native));
                native_names_mark(&names[0], native);
                if(NativeProgramMain(module, function))
                    native_names_mark(&names[0], "main");
                NativeGoFunctionName(native_programs, count, module, function,
                                     native, sizeof(native));
                native_names_mark(&names[1], native);
                if(function->export_symbol[0])
                    native_names_mark(&names[1], function->export_symbol);
            }
            for(int i = 0; i < module->import_count; i++) {
                const ZirImport *foreign = &module->imports[i];
                if(foreign->kind != ZIR_IMPORT_EXTERN ||
                   strncmp(foreign->target, "c.", 2)) continue;
                char native[ZIR_NAME_MAX * 2];
                NativeCForeignName(module, foreign, native, sizeof(native));
                native_names_mark(&names[0], native);
                /* The generated binding's private C name carries the real
                 * ABI label through __asm__. A value in another translation
                 * unit must not define that label (for example libc open). */
                native_names_mark(&names[0], foreign->target + 2);
            }
        }
    if(ok)
        native_names_mark_type_values(&names[0]);
    for(int t = 0; t < 2; t++)
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
void
ArrayAbiArgs(const ZirFunction *fn, char *out, size_t size)
{
    if(fn->return_type[0] != '[' && strchr(FunctionArgs(fn), '[') == NULL) {
        copy_text(out, size, FunctionArgs(fn));
        return;
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    size_t used = 0;
    out[0] = '\0';
    if(ArrayElementType(fn->return_type, NULL, 0, NULL)) {
        char name[ZIR_NAME_MAX];
        ArrayAbiName(fn, -1, name, sizeof(name));
        used += (size_t)format(out, size, "%s: %s", name, fn->return_type);
    }
    for(int i = 0; i < parameters->count; i++) {
        const ZirParameter *parameter = &parameters->items[i];
        if(parameter->type != NULL && ArrayValueType(parameter->type)) {
            char name[ZIR_NAME_MAX];
            ArrayAbiName(fn, i, name, sizeof(name));
            used += (size_t)format(out + used, size - used, "%s%s: %s",
                                   used ? ", " : "", name, parameter->type);
        } else {
            used += (size_t)format(out + used, size - used, "%s%s",
                                   used ? ", " : "", parameter->text);
        }
    }
}

const char *
canonical(const char *type)
{
    const char *result = ScalarType(type);
    if(!strcmp(type, "integer")) return "s64";
    if(!strcmp(type, "real")) return "float64";
    return *result ? result : type;
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

static const ZirModule *
native_go_constant_owner(const ZirModule *module, const char *name)
{
    for(int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirModule *scope = pass == 0 ? module : module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                if((pass == 0 || scope->defines[j].is_public) &&
                   !strcmp(scope->defines[j].name, name))
                    return scope;
            }
        }
    }
    return NULL;
}

/* Checked Ziran and generated Go scalar type -> Go type. */
int
NativeGoType(const ZirModule *module, const char *type, char *dst, size_t dst_size)
{
    char t[ZIR_NAME_MAX * 2];
    size_t n;

    snprintf(t, sizeof(t), "%s", type);
    n = strlen(t);
    while(n > 0 && isspace((unsigned char)t[n - 1]))
        t[--n] = '\0';
    {
        const char *p = t;

        while(*p != '\0' && isspace((unsigned char)*p))
            p++;
        if(p != t)
            memmove(t, p, strlen(p) + 1);
        n = strlen(t);
    }
    const char *scalar = ScalarType(t);
    if(*scalar && TargetType(t, ZIR_GO)) {
        snprintf(dst, dst_size, "%s", TargetType(t, ZIR_GO));
        return 1;
    }
    if(t[0] == '[') {
        char *close = strchr(t, ']');
        const char *base;

        if(close != NULL) {
            char bound[ZIR_NAME_MAX * 2];
            snprintf(bound, sizeof(bound), "%.*s", (int)(close - t - 1), t + 1);
            if(module != NULL &&
               native_go_constant_owner(module, bound) != NULL) {
                char mapped[ZIR_NAME_MAX * 2];
                TargetDefineName(native_go_constant_owner(module, bound),
                                 ZIR_GO, bound, mapped, sizeof(mapped));
                snprintf(bound, sizeof(bound), "%s", mapped);
            }
            base = close + 1;
            while(*base == ' ' || *base == '\t')
                base++;
            {
                char gt[ZIR_NAME_MAX * 2];

                if(NativeGoType(module, base, gt, sizeof(gt))) {
                    *close = '\0';
                    snprintf(dst, dst_size, "[%s]%s", bound, gt);
                    return 1;
                }
            }
        }
        return 0;
    }
    if(t[0] == '*' && t[1] != '\0') {
        char gt[ZIR_NAME_MAX * 2];

        if(!strcmp(t + 1, "void")) {
            snprintf(dst, dst_size, "*byte");
            return 1;
        }
        if(NativeGoType(module, t + 1, gt, sizeof(gt))) {
            snprintf(dst, dst_size, "*%s", gt);
            return 1;
        }
        return 0;
    }
    /* A name alone is not evidence that a type exists. */
    {
        int identish = t[0] != '\0';

        for(char *c = t; *c != '\0'; c++)
            if(!is_ident_char((unsigned char)*c) && *c != '.')
                identish = 0;
        const ZirModule *owner = NULL;
        const ZirType *declared = identish && module != NULL ?
            FindType(module, t, &owner) : NULL;
        if(declared != NULL) {
            if(owner != NULL)
                NativeTypeName(owner, declared, dst, dst_size);
            else
                snprintf(dst, dst_size, "%s", declared->name);
            return 1;
        }
    }
    return 0;
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
    if(!NativeHeaderKeyword(symbol)) {
        copy_text(out, size, symbol);
        return;
    }
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
    if(NativeProgramMain(module, fn)) {
        uint64_t hash = UINT64_C(14695981039346656037);
        for(const unsigned char *p = (const unsigned char *)module->name; *p; p++)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
        hash *= UINT64_C(1099511628211);
        for(const unsigned char *p = (const unsigned char *)fn->name; *p; p++)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
        /* A long source export name must fit every backend symbol buffer. */
        format(out, size, "zir_main_%016llx", (unsigned long long)hash);
        return;
    }
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

/* Foreign bindings belong to their source module, just like functions. The
 * ABI assembler label is separate and may be shared by independent bindings. */
void
NativeCForeignName(const ZirModule *module, const ZirImport *foreign,
                   char *out, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(const unsigned char *p = (const unsigned char *)module->name; *p; p++)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    format(out, size, "zir_f_%016llx_%s", (unsigned long long)hash, foreign->name);
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
    module_identities.programs = (uintptr_t)programs;
    module_identities.count = count;
}

void
NativeGoModuleIdentity(const ZirProgram *const *programs, int count,
                       const ZirModule *module, char *file_stem,
                       size_t file_size, char *guard, size_t guard_size)
{
    if(module_identities.slots == NULL ||
       module_identities.programs != (uintptr_t)programs ||
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
        if(target == ZIR_C && *array == '[') {
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
slot_type_at_use(const char *source, ZirTarget target,
                  ZirResolveTarget resolve_type, void *context,
                  char *out, size_t size)
{
    const char *base = source;
    while(*base == '*' || *base == '[') {
        if(*base == '*') base = skip_ws(base + 1);
        else {
            const char *close = strchr(base, ']');
            if(close == NULL) break;
            base = skip_ws(close + 1);
        }
    }
    if(resolve_type) {
        char resolved[ZIR_NAME_MAX], canonical[ZIR_NAME_MAX * 2];
        resolve_type(context, base, resolved, sizeof(resolved));
        format(canonical, sizeof(canonical), "%.*s%s",
               (int)(base - source), source, resolved);
        slot_native_type(canonical, target, out, size);
    } else slot_native_type(source, target, out, size);
}

static void
EmitSlotType_with_buffers(FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context, EmitSlotTypeBuffers *buffers)
{
    char name[ZIR_NAME_MAX * 2];
    copy_text(name, sizeof(name), slot->name);
    if(resolve_type)
        resolve_type(context, slot->name, name, sizeof(name));
    int count = *skip_ws(slot->body) ?
        split_top_level(slot->body, buffers->parameters[0], 64, sizeof(buffers->parameters[0])) : 0;
    if(slot->is_c_call) {
        char result[ZIR_NAME_MAX];
        slot_type_at_use(slot->procedure_return_type, target, resolve_type,
                          context, result, sizeof(result));
        fprintf(out, "typedef %s (*%s)(", result, name);
        for(int i = 0; i < count; i++) {
            const char *source = skip_ws(strchr(buffers->parameters[i], ':') + 1);
            char type[ZIR_NAME_MAX];
            slot_type_at_use(source, target, resolve_type, context, type, sizeof(type));
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
        fprintf(out, "type %s func(", name);
    else
        fprintf(out, "typedef struct %s {\n    void *context;\n    %s (*call)(void *", name, result_type);
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
        fprintf(out, ");\n} %s;\n", name);
}

COMPILER_SCRATCH_VOID(, EmitSlotType,
    (FILE *out, const ZirType *slot, ZirTarget target,
                ZirResolveTarget resolve_type, void *context),
    EmitSlotTypeBuffers, RECOVERABLE, free,
    EmitSlotType_with_buffers(out, slot, target, resolve_type, context, buffers))

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

COMPILER_SCRATCH_VOID(, emit_field_path,
    (const ZirModule *module, ZirTarget target,
                const char *base_type, const char *path,
                const char *base_expression, char *out, size_t size),
    EmitFieldPathBuffers, RECOVERABLE, free,
    emit_field_path_with_buffers(module, target, base_type, path, base_expression, out, size, buffers))

void
EmitStringType(FILE *out)
{
    fputs("#include \"zir_string_types.h\"\n", out);
}

const char *
zero_value(const char *type, ZirTarget target)
{
    if(SliceElementType(type, NULL, 0))
        return target == ZIR_GO ? "nil" : "{0}";
    if(type[0] == '[')
        return "";
    if(type[0] != '[' && strchr(type, '*') != NULL)
        return target == ZIR_GO ? "nil" : "((void *)0)";
    if(!strcmp(canonical(type), "string"))
        return target == ZIR_C ? "StringView(NULL, 0)" : "\"\"";
    if(!strcmp(canonical(type), "bool")) return "false";
    return "0";
}

void
number_prefix(const ZirModule *module, char *out, size_t size)
{
    char name[ZIR_PATH_MAX];
    readable_module_name(module->name, name, sizeof(name), 0);
    format(out, size, "number_%s", name);
}

/* C sources mark where numeric helpers go. Once the module is written,
 * EmitNativeHelpers puts in the ones its code calls. */
void
EmitNumbers(FILE *out, const ZirModule *module, ZirTarget target)
{
    (void)module;
    if(target == ZIR_C)
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

int
NativeProgramMain(const ZirModule *module, const ZirFunction *fn)
{
    if(fn->exported)
        return !strcmp(fn->export_symbol[0] ? fn->export_symbol : fn->name, "main");
    return !strcmp(module->name, "main") && !strcmp(fn->name, "main");
}

static void
native_entry_error(const ZirFunction *fn, const char *target, const char *reason)
{
    DiagnosticTarget(fn->span, "zir_c.entry", target, "entry.signature",
                     "native entry %s %s", fn->name, reason);
}

int
CheckNativeEntry(const ZirFunction *fn, const char *target)
{
    int integer_result = ScalarWidth(fn->return_type) || !strcmp(fn->return_type, "isize") || !strcmp(fn->return_type, "usize");
    if(fn->return_type[0] && strcmp(fn->return_type, "void") && !integer_result) {
        native_entry_error(fn, target, "must return nothing or an integer");
        return 0;
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const char *count_type = parameters->count && parameters->items[0].type ? parameters->items[0].type : "";
    int integer_count = ScalarWidth(count_type) || !strcmp(count_type, "isize") || !strcmp(count_type, "usize");
    if(parameters->count && (parameters->count != 2 || !parameters->items[0].type ||
       !integer_count || !parameters->items[1].type ||
       (strcmp(parameters->items[1].type, "**u8") && strcmp(parameters->items[1].type, "**s8")))) {
        native_entry_error(fn, target, "takes nothing or (argc: integer, argv: **u8 or **s8)");
        return 0;
    }
    return 1;
}

void
EmitNativeMainDeclaration(FILE *out)
{
    fputs("#ifdef __cplusplus\n}\nextern \"C++\" {\n#endif\n"
          "int main(int argc, char **argv);\n"
          "#ifdef __cplusplus\n}\nextern \"C\" {\n#endif\n", out);
}

void
EmitSymbolDeclaration(FILE *out, const char *result, const char *name,
                      const char *parameters, const char *symbol)
{
    fprintf(out, "#ifdef __APPLE__\n%s %s(%s) __asm__(\"_%s\");\n#else\n",
            result, name, parameters, symbol);
    fprintf(out, "%s %s(%s) __asm__(\"%s\");\n#endif\n",
            result, name, parameters, symbol);
}

void
EmitNativeMain(FILE *out, const ZirModule *module, const ZirFunction *fn,
               ZirTarget target)
{
    char body[ZIR_NAME_MAX * 2];
    NativeCFunctionName(module, fn, body, sizeof(body));
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    fputs("\nint\nmain(int argc, char **argv)\n{\n    (void)argc;\n    (void)argv;\n", out);
    int returns_value = fn->return_type[0] && strcmp(fn->return_type, "void");
    fprintf(out, "    %s%s(", returns_value ? "return (int)" : "", body);
    if(parameters->count == 2)
        fprintf(out, "(%s)argc, (%s)argv", TargetType(parameters->items[0].type, target),
                !strcmp(parameters->items[1].type, "**s8") ? "int8_t **" : "uint8_t **");
    fputs(");\n", out);
    if(!returns_value) fputs("    return 0;\n", out);
    fputs("}\n", out);
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
     "    /* Powers of two are exact in double, including the unsigned 64-bit bound. */\n"
     "    double bound = w - sign == 64 ? 18446744073709551616.0 :\n"
     "        (double)(UINT64_C(1) << (w - sign));\n"
     "    if(!(x >= (sign ? -bound : 0) && x < bound)) abort();\n"
     "    return sign ? (uint64_t)(int64_t)x : (uint64_t)x;\n"
     "}\n"},
};

enum {
    NATIVE_BOUNDS = 1, NATIVE_STRING = 2, NATIVE_STDIO = 4,
    NATIVE_STDLIB = 8, NATIVE_MEMORY = 16
};

/* Scan whole identifier tokens, including those in literals and comments.
 * False positives only add an include; emitted runtime calls cannot be missed
 * through line length or mistaken for a suffix in a user's mangled name. */
static unsigned
native_runtime_includes(const char *text)
{
    /* Each emitted identifier is compared against these literal names. Keep
     * their lengths with the table rather than calling strlen per token. */
#define RUNTIME_USE(name, include) {name, sizeof(name) - 1, include}
    static const struct { const char *name; size_t length; unsigned include; } uses[] = {
        RUNTIME_USE("BoundsCheck", NATIVE_BOUNDS), RUNTIME_USE("EmptyArrayIndex", NATIVE_BOUNDS),
        RUNTIME_USE("ZIRAN_INDEX", NATIVE_BOUNDS), RUNTIME_USE("ZIRAN_EMPTY_INDEX", NATIVE_BOUNDS),
        RUNTIME_USE("StringRange", NATIVE_STRING), RUNTIME_USE("StringEqual", NATIVE_STRING),
        RUNTIME_USE("print_float", NATIVE_STRING),
        RUNTIME_USE("printf", NATIVE_STDIO), RUNTIME_USE("fwrite", NATIVE_STDIO), RUNTIME_USE("stdout", NATIVE_STDIO),
        RUNTIME_USE("calloc", NATIVE_STDLIB), RUNTIME_USE("free", NATIVE_STDLIB), RUNTIME_USE("abort", NATIVE_STDLIB),
        RUNTIME_USE("memcpy", NATIVE_MEMORY), RUNTIME_USE("memmove", NATIVE_MEMORY)
    };
#undef RUNTIME_USE
    unsigned includes = 0;
    for(const char *p = text; *p;) {
        if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')) {
            p++;
            continue;
        }
        const char *start = p++;
        while((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_') p++;
        size_t length = (size_t)(p - start);
        for(size_t i = 0; i < sizeof(uses) / sizeof(uses[0]); i++)
            if(!(includes & uses[i].include) && uses[i].length == length &&
               *start == *uses[i].name &&
               !memcmp(start, uses[i].name, length))
                includes |= uses[i].include;
    }
    return includes;
}

/* Resolve both dependency marks before writing the final native text. */
int
EmitNativeHelpers(FILE *file, const char *text)
{
    const char *mark, *numbers;
    int needed[sizeof(c_number_helpers) / sizeof(c_number_helpers[0])] = {0};
    int any = 0;
    mark = strstr(text, RUNTIME_INCLUDES_MARK);
    numbers = strstr(text, NUMBER_HELPERS_MARK);
    if(mark == NULL || (numbers != NULL && numbers < mark))
        return -1;
    for(size_t i = 0; numbers && i < sizeof(needed) / sizeof(needed[0]); i++) {
        char call[32];
        format(call, sizeof(call), "%s(", c_number_helpers[i].name);
        needed[i] = strstr(text, call) != NULL;
    }
    needed[0] |= needed[1];
    for(size_t i = 0; i < sizeof(needed) / sizeof(needed[0]); i++)
        any |= needed[i];
    unsigned includes = native_runtime_includes(text);
    if(any) includes |= NATIVE_STDLIB;
    fwrite(text, 1, (size_t)(mark - text), file);
    if(includes & NATIVE_BOUNDS) fputs("#include \"zir_bounds.h\"\n", file);
    if(includes & NATIVE_STRING) fputs("#include \"zir_string.h\"\n", file);
    if(!(includes & NATIVE_STRING)) {
        if((includes & NATIVE_STDIO) && !(includes & NATIVE_BOUNDS))
            fputs("#include <stdio.h>\n", file);
        if((includes & NATIVE_STDLIB) && !(includes & NATIVE_BOUNDS))
            fputs("#include <stdlib.h>\n", file);
        if(includes & NATIVE_MEMORY) fputs("#include <string.h>\n", file);
    }
    mark += strlen(RUNTIME_INCLUDES_MARK);
    if(numbers != NULL)
        fwrite(mark, 1, (size_t)(numbers - mark), file);
    for(size_t i = 0, written = 0; i < sizeof(needed) / sizeof(needed[0]); i++)
        if(needed[i])
            fprintf(file, "%s%s", written++ ? "\n" : "", c_number_helpers[i].definition);
    fputs(numbers ? numbers + strlen(NUMBER_HELPERS_MARK) : mark, file);
    return ferror(file) ? -1 : 0;
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
