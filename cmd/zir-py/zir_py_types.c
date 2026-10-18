#include "zir_py_internal.h"

static const char *const py_reserved[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield", "match", "case", "type",
    /* Builtins and modules the generated code names. */
    "abs", "bool", "bytes", "bytearray", "enumerate", "float", "getattr",
    "int", "isinstance", "len", "list", "print", "range", "setattr", "str",
    "super", "tuple", "object", "id", "hash", "max", "min", "repr", "iter",
    "zip", "format", "self", "sys", "math", "struct", "ctypes", "decimal",
    "host", NULL
};

void *py_allocate(size_t size)
{
    void *memory = calloc(1, size);
    if(memory == NULL) {
        DiagnosticOutOfMemory();
        exit(1);
    }
    return memory;
}

char *py_format(const char *format, ...)
{
    va_list arguments;
    char *text = NULL;
    va_start(arguments, format);
    if(vasprintf(&text, format, arguments) < 0) {
        DiagnosticOutOfMemory();
        exit(1);
    }
    va_end(arguments);
    return text;
}

char *py_copy_string(const char *text)
{
    return py_format("%s", text);
}

int identifier_character(int character)
{
    return isalnum((unsigned char)character) || character == '_';
}

/* A Python identifier for a Ziran name. Names that begin with an
 * underscore are left to the runtime, and reserved words get a trailing
 * underscore, as Python code spells class_ and from_. */
void py_identifier(const char *source, char *output, size_t size)
{
    size_t used = 0;
    if(source == NULL || *source == '\0') {
        snprintf(output, size, "ziran_empty");
        return;
    }
    if(!isalpha((unsigned char)*source)) {
        used = (size_t)snprintf(output, size, "ziran%s", *source == '_' ? "" : "_");
        if(used >= size)
            used = size - 1;
    }
    for(const unsigned char *p = (const unsigned char *)source;
        *p && used + 1 < size; p++)
        output[used++] = identifier_character(*p) ? (char)*p : '_';
    output[used] = '\0';
    for(int index = 0; py_reserved[index] != NULL; index++)
        if(strcmp(output, py_reserved[index]) == 0 && used + 2 < size) {
            output[used++] = '_';
            output[used] = '\0';
            break;
        }
}

int names_has(const PyNames *names, const char *name)
{
    for(int index = 0; index < names->count; index++)
        if(strcmp(names->items[index], name) == 0)
            return 1;
    return 0;
}

void names_add(PyNames *names, const char *name)
{
    if(names_has(names, name))
        return;
    if(names->count == names->capacity) {
        int capacity = names->capacity ? names->capacity * 2 : 64;
        void *items = realloc(names->items, (size_t)capacity * sizeof(*names->items));
        if(items == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        names->items = items;
        names->capacity = capacity;
    }
    snprintf(names->items[names->count++], ZIR_NAME_MAX, "%s", name);
}

void write_line(PyEmitter *emitter, const char *format, ...)
{
    va_list arguments;
    for(int index = 0; index < emitter->indent; index++)
        fputs("    ", emitter->output);
    va_start(arguments, format);
    vfprintf(emitter->output, format, arguments);
    va_end(arguments);
    fputc('\n', emitter->output);
    emitter->lines++;
}

/* Parenthesized text without its one enclosing pair, for a whole statement. */
char *py_bare(const char *text)
{
    size_t length = strlen(text);
    int depth = 0, whole = length >= 2 && text[0] == '(' && text[length - 1] == ')';
    char quote = 0;
    for(size_t index = 0; whole && index < length; index++) {
        char c = text[index];
        if(quote) {
            if(c == '\\') index++;
            else if(c == quote) quote = 0;
            continue;
        }
        if(c == '"' || c == '\'') quote = c;
        else if(c == '(' || c == '[') depth++;
        else if((c == ')' || c == ']') && --depth == 0 && index + 1 < length) whole = 0;
    }
    if(whole)
        return py_format("%.*s", (int)(length - 2), text + 1);
    return py_copy_string(text);
}

/* ----- types ----- */

int py_integer_width(const char *type, int *bits, int *is_signed)
{
    static const struct { const char *name; int bits, is_signed; } integers[] = {
        {"s8", 8, 1}, {"s16", 16, 1}, {"s32", 32, 1}, {"s64", 64, 1},
        {"u8", 8, 0}, {"u16", 16, 0}, {"u32", 32, 0}, {"u64", 64, 0},
        {"isize", 64, 1}, {"usize", 64, 0}, {"integer", 64, 1}, {"int", 64, 1},
        {"char", 8, 0}, {NULL, 0, 0}
    };
    for(int index = 0; integers[index].name != NULL; index++)
        if(strcmp(type, integers[index].name) == 0) {
            *bits = integers[index].bits;
            *is_signed = integers[index].is_signed;
            return 1;
        }
    return 0;
}

int py_classify(const ZirModule *scope, const char *type, PyType *out)
{
    const ZirModule *owner = NULL;
    const ZirType *declared;
    char element[ZIR_NAME_MAX];
    int capacity = 0;
    memset(out, 0, sizeof(*out));
    if(type == NULL || *type == '\0' || strcmp(type, "void") == 0) {
        out->kind = PY_VOID;
        return 1;
    }
    if(strcmp(type, "bool") == 0) {
        out->kind = PY_BOOL;
        return 1;
    }
    if(py_integer_width(type, &out->bits, &out->is_signed)) {
        out->kind = PY_INT;
        return 1;
    }
    if(strcmp(type, "float32") == 0 || strcmp(type, "float64") == 0 ||
       strcmp(type, "real") == 0) {
        out->kind = PY_FLOAT;
        out->bits = strcmp(type, "float32") == 0 ? 32 : 64;
        return 1;
    }
    if(strcmp(type, "string") == 0) {
        out->kind = PY_STRING;
        return 1;
    }
    if(strcmp(type, "null") == 0) {
        out->kind = PY_NULL;
        return 1;
    }
    if(type[0] == '*' && type[1] != '\0') {
        out->kind = PY_POINTER;
        snprintf(out->element, sizeof(out->element), "%s", type + 1);
        return 1;
    }
    if(SliceElementType(type, element, sizeof(element))) {
        out->kind = PY_SLICE;
        snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    if(ArrayElementType(type, element, sizeof(element), &capacity)) {
        out->kind = PY_ARRAY;
        out->capacity = capacity;
        snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    declared = scope != NULL ? FindType(scope, type, &owner) : NULL;
    if(declared == NULL)
        return 0;
    out->owner = owner != NULL ? owner : scope;
    out->declared = declared;
    if(declared->is_enum) {
        const char *backing = declared->enum_backing[0] ? declared->enum_backing : "s64";
        if(!py_integer_width(backing, &out->bits, &out->is_signed))
            return 0;
        out->kind = PY_INT;
        return 1;
    }
    if(declared->is_procedure_type) {
        out->kind = PY_PROCEDURE;
        return 1;
    }
    if(declared->is_owned_vec || VecElementType(scope, type, element, sizeof(element))) {
        ZirTypeField field;
        size_t offset = 0;
        out->kind = PY_VEC;
        if(TypeNextField(declared, &offset, &field) == 1 && field.type[0] == '*')
            snprintf(out->element, sizeof(out->element), "%s", field.type + 1);
        else if(VecElementType(scope, type, element, sizeof(element)))
            snprintf(out->element, sizeof(out->element), "%s", element);
        return 1;
    }
    if(declared->is_extern) {
        /* A host-owned record crosses only as an opaque handle. */
        out->kind = PY_POINTER;
        snprintf(out->element, sizeof(out->element), "void");
        return 1;
    }
    if(declared->is_record_template || declared->is_type_instance)
        return 0;
    out->kind = declared->is_union ? PY_UNION : PY_RECORD;
    return 1;
}

int py_aggregate(int kind)
{
    return kind == PY_RECORD || kind == PY_UNION || kind == PY_ARRAY || kind == PY_VEC;
}

void py_require_type(const ZirModule *scope, ZirSourceSpan span,
                            const char *type, PyType *out)
{
    if(!py_classify(scope, type, out)) {
        Diagnostic(span, "zir_py.type", "the Python target cannot lower this type: %s", type);
        exit(1);
    }
}

/* A class name. A record the checker made from a type call, like
 * Option(s32), has only a hashed name; Option records read as Option_s32. */
void py_class_name(const PyType *info, char *output, size_t size)
{
    char native[ZIR_NAME_MAX];
    ZirTypeField fields[3];
    size_t offset = 0;
    if(strncmp(info->declared->name, "__type_", 7) == 0 &&
       TypeNextField(info->declared, &offset, &fields[0]) == 1 &&
       TypeNextField(info->declared, &offset, &fields[1]) == 1 &&
       TypeNextField(info->declared, &offset, &fields[2]) == 0 &&
       !strcmp(fields[0].name, "has_value") && !strcmp(fields[1].name, "value")) {
        snprintf(native, sizeof(native), "Option_%s", fields[1].type);
        for(char *c = native; *c; c++)
            if(!identifier_character(*c))
                *c = '_';
    } else if(strncmp(info->declared->name, "__type_", 7) == 0)
        snprintf(native, sizeof(native), "Type_%s", info->declared->name + 7);
    else
        NativeTypeName(info->owner, info->declared, native, sizeof(native));
    py_identifier(native, output, size);
}

const char *py_wrap_name(int bits, int is_signed)
{
    switch(bits) {
    case 8: return is_signed ? "_s8" : "_u8";
    case 16: return is_signed ? "_s16" : "_u16";
    case 32: return is_signed ? "_s32" : "_u32";
    default: return is_signed ? "_s64" : "_u64";
    }
}

/* ----- names ----- */

/* Record classes define copy and assign, so fields by those names move. */
void py_field_name(const char *source, char *output, size_t size)
{
    py_identifier(source, output, size);
    if((!strcmp(output, "copy") || !strcmp(output, "assign")) && strlen(output) + 2 < size)
        strcat(output, "_");
}

void function_symbol(PyEmitter *emitter, const ZirModule *module,
                            const ZirFunction *function, char *output, size_t size)
{
    char native[ZIR_NAME_MAX * 2];
    if(function->export_symbol[0]) {
        py_identifier(function->export_symbol, output, size);
        return;
    }
    if(function->exported && strcmp(function->name, "main") != 0) {
        py_identifier(function->name, output, size);
        return;
    }
    NativeGoFunctionName(emitter->programs, emitter->program_count, module,
                         function, native, sizeof(native));
    py_identifier(native, output, size);
}

static void module_guard(PyEmitter *emitter, const ZirModule *module,
                         char *output, size_t size)
{
    char file_stem[1024];
    char guard[1024];
    NativeGoModuleIdentity(emitter->programs, emitter->program_count, module,
                           file_stem, sizeof(file_stem), guard, sizeof(guard));
    py_identifier(guard, output, size);
}

void global_symbol(PyEmitter *emitter, const ZirModule *module,
                          const ZirGlobal *global, char *output, size_t size)
{
    char guard[ZIR_NAME_MAX];
    char name[ZIR_NAME_MAX];
    char joined[ZIR_NAME_MAX * 2 + 2];
    module_guard(emitter, module, guard, sizeof(guard));
    snprintf(name, sizeof(name), "%s", global->name);
    for(char *c = name; *c; c++)
        if(!identifier_character(*c)) *c = '_';
    snprintf(joined, sizeof(joined), "%s_%s", guard, name);
    py_identifier(joined, output, size);
}

void foreign_symbol(PyEmitter *emitter, const ZirModule *module,
                           const ZirImport *import, char *output, size_t size)
{
    char guard[ZIR_NAME_MAX];
    char joined[ZIR_NAME_MAX * 2 + 2];
    module_guard(emitter, module, guard, sizeof(guard));
    snprintf(joined, sizeof(joined), "%s_%s", guard, import->name);
    py_identifier(joined, output, size);
}

/* ----- locals ----- */

PyLocal *find_local(PyEmitter *emitter, const char *source)
{
    for(int index = emitter->local_count - 1; index >= 0; index--)
        if(strcmp(emitter->locals[index].source, source) == 0)
            return &emitter->locals[index];
    return NULL;
}

/* A Python name unused by live locals and every top-level name. */
PyLocal *declare_local(PyEmitter *emitter, const char *source)
{
    char base[ZIR_NAME_MAX];
    char candidate[ZIR_NAME_MAX];
    PyLocal *local;
    py_identifier(source, base, sizeof(base));
    snprintf(candidate, sizeof(candidate), "%s", base);
    for(int serial = 2;; serial++) {
        int taken = names_has(&emitter->module_names, candidate);
        for(int index = 0; index < emitter->local_count && !taken; index++)
            taken = strcmp(emitter->locals[index].python, candidate) == 0;
        if(!taken)
            break;
        snprintf(candidate, sizeof(candidate), "%s_%d", base, serial);
    }
    if(emitter->local_count == emitter->local_capacity) {
        int capacity = emitter->local_capacity ? emitter->local_capacity * 2 : 64;
        PyLocal *locals = realloc(emitter->locals, (size_t)capacity * sizeof(*locals));
        if(locals == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        emitter->locals = locals;
        emitter->local_capacity = capacity;
    }
    local = &emitter->locals[emitter->local_count++];
    memset(local, 0, sizeof(*local));
    snprintf(local->source, sizeof(local->source), "%s", source);
    snprintf(local->python, sizeof(local->python), "%s", candidate);
    local->depth = emitter->depth;
    local->boxed = names_has(&emitter->boxed, source);
    return local;
}

void leave_scope(PyEmitter *emitter)
{
    while(emitter->local_count > 0 &&
          emitter->locals[emitter->local_count - 1].depth >= emitter->depth)
        emitter->local_count--;
    emitter->depth--;
}

int global_reference(PyEmitter *emitter, const char *name,
                            char *output, size_t size, const ZirGlobal **found)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if(find_local(emitter, name) != NULL ||
       ResolveGlobalAt(emitter->module, name, SpanPath(emitter->function->span),
                       &owner, &global) != 1 || owner == NULL)
        return 0;
    global_symbol(emitter, owner, global, output, size);
    if(found != NULL)
        *found = global;
    return 1;
}

const ZirImport *py_foreign_import(const ZirModule *module, const char *name)
{
    for(int index = 0; index < module->import_count; index++) {
        const ZirImport *import = &module->imports[index];
        if(import->kind == ZIR_IMPORT_EXTERN &&
           (import->extern_kind == ZIR_EXTERN_C || import->extern_kind == ZIR_EXTERN_HOST ||
            import->extern_kind == ZIR_EXTERN_PY) &&
           strcmp(import->name, name) == 0)
            return import;
    }
    return NULL;
}

/* ----- values ----- */

char *emit_expression(PyEmitter *emitter, int index);
char *emit_value(PyEmitter *emitter, int index, const char *type);
char *py_zero(PyEmitter *emitter, const ZirModule *scope, const char *type);
