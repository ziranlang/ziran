#include "zir_py_internal.h"

static const char *py_struct_format(PyEmitter *emitter, const ZirModule *scope,
                                    const char *type)
{
    PyType info;
    (void)emitter;
    if(!py_classify(scope, type, &info))
        return NULL;
    if(info.kind == PY_BOOL)
        return "?";
    if(info.kind == PY_FLOAT)
        return info.bits == 32 ? "f" : "d";
    if(info.kind != PY_INT)
        return NULL;
    switch(info.bits) {
    case 8: return info.is_signed ? "b" : "B";
    case 16: return info.is_signed ? "h" : "H";
    case 32: return info.is_signed ? "i" : "I";
    default: return info.is_signed ? "q" : "Q";
    }
}

static void emit_union_class(PyEmitter *emitter, FILE *output, const ZirModule *module,
                             const ZirType *record, const char *name)
{
    size_t size = 0, alignment = 0;
    size_t offset = 0;
    ZirTypeField field;
    if(!TypeLayout(module, record->name, &size, &alignment)) {
        DiagnosticTarget(record->span, "zir_py.union", "py", "unions.layout", "cannot lay out union %s", record->name);
        exit(1);
    }
    fprintf(output, "class %s:\n", name);
    fputs("    \"\"\"A union: every field reads the same bytes.\"\"\"\n\n", output);
    fputs("    __slots__ = (\"_bytes\",)\n\n", output);
    fputs("    def __init__(self, **fields):\n", output);
    fprintf(output, "        self._bytes = bytearray(%zu)\n", size);
    fputs("        for field, value in fields.items():\n", output);
    fputs("            setattr(self, field, value)\n\n", output);
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        const char *format = py_struct_format(emitter, module, field.type);
        PyType array;
        if(format == NULL && py_classify(module, field.type, &array) &&
           array.kind == PY_ARRAY &&
           (format = py_struct_format(emitter, module, array.element)) != NULL) {
            py_field_name(field.name, field_name, sizeof(field_name));
            fprintf(output, "    @property\n    def %s(self):\n", field_name);
            fprintf(output, "        return ZiranUnionArray(self._bytes, \"%s\", %d)\n\n",
                    format, array.capacity);
            fprintf(output, "    @%s.setter\n    def %s(self, value):\n", field_name, field_name);
            fprintf(output, "        ZiranUnionArray(self._bytes, \"%s\", %d)[:] = value\n\n",
                    format, array.capacity);
            continue;
        }
        if(format == NULL) {
            DiagnosticTarget(record->span, "zir_py.union", "py", "unions.scalar",
                    "the Python target lowers only scalar union fields: %s", field.name);
            exit(1);
        }
        py_field_name(field.name, field_name, sizeof(field_name));
        fprintf(output, "    @property\n    def %s(self):\n", field_name);
        fprintf(output, "        return struct.unpack_from(\"=%s\", self._bytes)[0]\n\n", format);
        fprintf(output, "    @%s.setter\n    def %s(self, value):\n", field_name, field_name);
        fprintf(output, "        struct.pack_into(\"=%s\", self._bytes, 0, value)\n\n", format);
    }
    fprintf(output, "    def copy(self):\n        result = %s()\n", name);
    fputs("        result._bytes[:] = self._bytes\n        return result\n\n", output);
    fputs("    def assign(self, other):\n        self._bytes[:] = other._bytes\n\n\n", output);
}

static void emit_record_class(PyEmitter *emitter, FILE *output, const ZirModule *module,
                              const ZirType *record, const char *name)
{
    size_t offset = 0;
    ZirTypeField field;
    char *slots = py_copy_string("");
    char *parameters = py_copy_string("");
    int count = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        PyType info;
        char *joined;
        py_require_type(module, record->span, field.type, &info);
        py_field_name(field.name, field_name, sizeof(field_name));
        joined = py_format("%s%s\"%s\"", slots, count ? ", " : "", field_name);
        free(slots);
        slots = joined;
        joined = py_format("%s, %s", parameters, field_name);
        free(parameters);
        parameters = joined;
        count++;
    }
    fprintf(output, "class %s:\n", name);
    fprintf(output, "    __slots__ = (%s%s)\n\n", slots, count == 1 ? "," : "");
    fprintf(output, "    def __init__(self%s):\n", parameters);
    if(count == 0)
        fputs("        pass\n", output);
    offset = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        py_field_name(field.name, field_name, sizeof(field_name));
        fprintf(output, "        self.%s = %s\n", field_name, field_name);
    }
    fprintf(output, "\n    def copy(self):\n        return %s(", name);
    offset = 0;
    count = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        char *access, *copied;
        py_field_name(field.name, field_name, sizeof(field_name));
        access = py_format("self.%s", field_name);
        copied = py_copy_text(emitter, module, field.type, access, 0);
        fprintf(output, "%s%s", count ? ", " : "", copied);
        free(access);
        free(copied);
        count++;
    }
    fputs(")\n\n    def assign(self, other):\n", output);
    if(count == 0)
        fputs("        pass\n", output);
    offset = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        char field_name[ZIR_NAME_MAX];
        PyType info, element;
        py_field_name(field.name, field_name, sizeof(field_name));
        py_classify(module, field.type, &info);
        if(info.kind == PY_ARRAY) {
            if(py_classify(module, info.element, &element) && py_aggregate(element.kind))
                fprintf(output, "        _assign_items(self.%s, other.%s)\n", field_name,
                        field_name);
            else
                fprintf(output, "        self.%s[:] = other.%s\n", field_name, field_name);
        } else if(py_aggregate(info.kind))
            fprintf(output, "        self.%s.assign(other.%s)\n", field_name, field_name);
        else
            fprintf(output, "        self.%s = other.%s\n", field_name, field_name);
    }
    fputs("\n\n", output);
    free(slots);
    free(parameters);
}

static void emit_classes(PyEmitter *emitter, FILE *output)
{
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int type_index = 0; type_index < module->type_count; type_index++) {
                const ZirType *record = &module->types[type_index];
                PyType info;
                char name[ZIR_NAME_MAX];
                if(record->is_record_template)
                    continue;
                if(!py_classify(module, record->name, &info)) {
                    Diagnostic(record->span, "zir_py.type",
                            "the Python target cannot lower this type: %s", record->name);
                    exit(1);
                }
                if(info.kind != PY_RECORD && info.kind != PY_UNION)
                    continue;
                if(info.declared != record)
                    continue;
                py_class_name(&info, name, sizeof(name));
                if(info.kind == PY_UNION)
                    emit_union_class(emitter, output, module, record, name);
                else
                    emit_record_class(emitter, output, module, record, name);
            }
        }
    }
}

/* Shared libraries named in LDLIBS (-lNAME) when the program was built. */
static const char *const *py_linked_libraries;
static int py_linked_library_count;

void py_set_linked_libraries(const char *const *names, int count)
{
    py_linked_libraries = names;
    py_linked_library_count = count;
}

/* Buffers emit_foreign_bindings keeps on the heap: py_lower inlines it, and
 * some compilers add every inlined frame together past FRAMEFLAGS. */
typedef struct PyForeignBuffers {
    char symbol[ZIR_NAME_MAX * 2];
    char parameters[32][ZIR_NAME_MAX];
    char library[ZIR_PATH_MAX];
} PyForeignBuffers;

static void emit_foreign_bindings(PyEmitter *emitter, FILE *output)
{
    int any = 0, linked = 0;
    char *python_modules = py_copy_string("");
    PyForeignBuffers *buffers = py_allocate(sizeof(*buffers));
    char *symbol = buffers->symbol;
    char (*parameters)[ZIR_NAME_MAX] = buffers->parameters;
    char *library = buffers->library;
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            for(int import_index = 0; import_index < module->import_count; import_index++) {
                const ZirImport *import = &module->imports[import_index];
                const char *dot, *link, *returned;
                char *argtypes;
                int count;
                if(import->kind != ZIR_IMPORT_EXTERN)
                    continue;
                if(import->extern_kind == ZIR_EXTERN_HOST)
                    continue;
                if(import->extern_kind == ZIR_EXTERN_PY) {
                    char python_module[ZIR_PATH_MAX], module_symbol[ZIR_PATH_MAX * 2];
                    char *line;
                    if(!PyForeignCallParts(import->target, python_module, sizeof(python_module),
                                           NULL, 0, NULL, 0)) {
                        Diagnostic(import->span, "zir_py.import",
                                   "invalid Python foreign target: %s", import->target);
                        exit(1);
                    }
                    py_module_symbol(python_module, module_symbol, sizeof(module_symbol));
                    line = py_format("%s = _py_module(\"%s\")\n", module_symbol, python_module);
                    if(strstr(python_modules, line) == NULL) {
                        char *joined = py_format("%s%s", python_modules, line);
                        fputs(line, output);
                        free(python_modules);
                        python_modules = joined;
                    }
                    free(line);
                    any = 1;
                    continue;
                }
                if(import->extern_kind != ZIR_EXTERN_C) {
                    Diagnostic(import->span, "zir_py.import",
                            "the Python target runs C and py: foreign imports, not Go: %s",
                            import->name);
                    exit(1);
                }
                /* C foreign symbols resolve in the process, as a C build links
                 * them; libraries named in LDLIBS join it first. */
                if(!linked)
                    for(int index = 0; index < py_linked_library_count; index++)
                        fprintf(output, "_link_library(\"%s\")\n", py_linked_libraries[index]);
                count = foreign_parameters(import, parameters, 32);
                foreign_symbol(emitter, module, import, symbol, sizeof(buffers->symbol));
                snprintf(library, sizeof(buffers->library), "%s", import->target);
                dot = strrchr(library, '.');
                if(dot != NULL)
                    library[dot - library] = '\0';
                link = import->extern_symbol[0] ? import->extern_symbol : import->name;
                argtypes = py_copy_string("");
                for(int index = 0; index < count; index++) {
                    const char *ctype = py_pointer_ctype(emitter, parameters[index]);
                    char *pointer = NULL;
                    char *joined;
                    if(ctype != NULL)
                        pointer = py_format("ctypes.POINTER(%s)", ctype);
                    else if((ctype = py_ctype(emitter, parameters[index])) == NULL) {
                        Diagnostic(import->span, "zir_py.import",
                                "unsupported foreign parameter type in %s", import->name);
                        exit(1);
                    }
                    joined = py_format("%s%s%s", argtypes, index ? ", " : "",
                                       pointer != NULL ? pointer : ctype);
                    free(argtypes);
                    free(pointer);
                    argtypes = joined;
                }
                returned = py_ctype(emitter, import->return_type[0] ? import->return_type : "void");
                if(returned == NULL) {
                    Diagnostic(import->span, "zir_py.import",
                            "unsupported foreign return type: %s", import->name);
                    exit(1);
                }
                fprintf(output, "%s = _foreign(_library(\"%s\"), \"%s\", [%s], %s)\n",
                        symbol, library, link, argtypes, returned);
                free(argtypes);
                any = linked = 1;
            }
        }
    }
    free(buffers);
    free(python_modules);
    if(any)
        fputs("\n\n", output);
}

/* ----- globals ----- */

static void fill_literal_types(PyEmitter *emitter, ZirFunction *literal, int index,
                               const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    PyType info;
    int position = 0;
    if(index < 0 || index >= literal->expr_count || depth > 32)
        return;
    ZirExpr *node = &literal->exprs[index];
    if(node->type[0] == '\0' || !strcmp(node->type, "integer") ||
       !strcmp(node->type, "real") || node->kind == ZIR_EXPR_COMPOUND)
        node->type = KeepNameFormat("%s", type);
    if(node->kind != ZIR_EXPR_COMPOUND)
        return;
    int array = ArrayElementType(type, element, sizeof(element), NULL);
    if(!array && (!py_classify(emitter->module, type, &info) || info.kind != PY_RECORD))
        return;
    for(int child = node->first_child; child >= 0;
        child = literal->exprs[child].next_sibling, position++) {
        ZirExpr *entry = &literal->exprs[child];
        char field_type[ZIR_NAME_MAX] = "";
        if(array)
            snprintf(field_type, sizeof(field_type), "%s", element);
        else {
            size_t offset = 0;
            int ordinal = 0;
            ZirTypeField field;
            while(TypeNextField(info.declared, &offset, &field) == 1) {
                if(entry->name[0] ? !strcmp(field.name, entry->name) : ordinal == position) {
                    snprintf(field_type, sizeof(field_type), "%s", field.type);
                    if(!entry->name[0])
                        entry->name = KeepNameFormat("%s", field.name);
                    break;
                }
                ordinal++;
            }
            if(field_type[0] && info.owner != emitter->module &&
               FindType(emitter->module, field_type, NULL) == NULL)
                for(int import = 0; import < emitter->module->import_count; import++)
                    if(emitter->module->imports[import].resolved_module == info.owner &&
                       emitter->module->imports[import].name[0]) {
                        char spelled[ZIR_NAME_MAX];
                        snprintf(spelled, sizeof(spelled), "%s.%s",
                                 emitter->module->imports[import].name, field_type);
                        snprintf(field_type, sizeof(field_type), "%s", spelled);
                        break;
                    }
        }
        if(!field_type[0])
            continue;
        if(entry->kind == ZIR_EXPR_FIELD_INIT) {
            entry->type = KeepNameFormat("%s", field_type);
            fill_literal_types(emitter, literal, entry->right, field_type, depth + 1);
        } else
            fill_literal_types(emitter, literal, child, field_type, depth + 1);
    }
}

static char *global_initializer(PyEmitter *emitter, const ZirGlobal *global)
{
    PyType info;
    py_classify(emitter->module, global->type, &info);
    if(global->init[0] != '\0' && info.kind == PY_STRING) {
        unsigned char *bytes = py_allocate(ZIR_TEXT_MAX);
        size_t length;
        if(DecodeStringLiteral(global->init, bytes, ZIR_TEXT_MAX, &length)) {
            char *result = py_string_literal(bytes, length);
            free(bytes);
            return result;
        }
        free(bytes);
    }
    if(global->init[0] != '\0' && info.kind == PY_INT && info.declared != NULL &&
       info.declared->is_enum) {
        const char *member = strrchr(global->init, '.');
        int64_t value;
        if(member != NULL && EnumMemberValue(info.declared, member + 1, &value))
            return py_format("%lld", (long long)value);
    }
    if(global->init[0] != '\0' && strchr(global->init, '(') == NULL) {
        long long integer;
        char *end;
        double real;
        if(info.kind == PY_BOOL && (!strcmp(global->init, "true") ||
                                    !strcmp(global->init, "false")))
            return py_copy_string(!strcmp(global->init, "true") ? "True" : "False");
        if(info.kind == PY_INT && py_integer_literal(global->init, &integer)) {
            if(!info.is_signed)
                return py_format("%llu", (unsigned long long)integer &
                                 (info.bits >= 64 ? ~0ULL : ((1ULL << info.bits) - 1)));
            return py_format("%lld", integer);
        }
        if(info.kind == PY_FLOAT) {
            real = strtod(global->init, &end);
            if(*end == '\0') {
                if(info.bits == 32)
                    return py_round_float32_literal(real);
                if(strpbrk(global->init, ".eEn") == NULL)
                    return py_format("%s.0", global->init);
                return py_format("%.17g", real);
            }
        }
        if((info.kind == PY_POINTER || info.kind == PY_PROCEDURE) &&
           !strcmp(global->init, "null"))
            return py_copy_string("None");
    }
    if(global->init[0] != '\0' && (info.kind == PY_RECORD || info.kind == PY_ARRAY)) {
        ZirFunction *probe = py_allocate(sizeof(*probe));
        int root = ParseExprTyped(probe, emitter->module, global->init, global->span,
                                  global->type);
        int constant = root >= 0;
        for(int index = 0; index < probe->expr_count && constant; index++)
            constant = probe->exprs[index].kind != ZIR_EXPR_IDENT &&
                       probe->exprs[index].kind != ZIR_EXPR_CALL;
        if(constant && probe->exprs[root].kind == ZIR_EXPR_COMPOUND) {
            const ZirFunction *saved = emitter->function;
            char *result;
            fill_literal_types(emitter, probe, root, global->type, 0);
            emitter->function = probe;
            result = emit_value(emitter, root, global->type);
            emitter->function = saved;
            free(probe->exprs);
            free(probe);
            return result;
        }
        free(probe->exprs);
        free(probe);
    }
    return py_zero(emitter, emitter->module, global->type);
}

/* Globals are lowered outside any function. A ZirFunction is 9 KB, too big
 * for py_lower's frame once emit_globals is inlined into it. */
static const ZirFunction py_no_function;

static void emit_globals(PyEmitter *emitter, FILE *output)
{
    int any = 0;
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            emitter->module = module;
            emitter->function = &py_no_function;
            for(int global_index = 0; global_index < module->global_count; global_index++) {
                const ZirGlobal *global = &module->globals[global_index];
                char symbol[ZIR_NAME_MAX * 2];
                PyType info;
                char *value, *bare;
                py_require_type(module, global->span, global->type, &info);
                global_symbol(emitter, module, global, symbol, sizeof(symbol));
                value = global_initializer(emitter, global);
                bare = py_bare(value);
                fprintf(output, "%s = %s\n", symbol, bare);
                free(value);
                free(bare);
                any = 1;
            }
        }
    }
    if(any)
        fputs("\n", output);
}

typedef struct PyModuleVisit {
    const ZirModule *module;
    int state;
} PyModuleVisit;

typedef struct PyModuleVisits {
    PyModuleVisit *items;
    size_t count;
    size_t capacity;
} PyModuleVisits;

static PyModuleVisit *module_visit(PyModuleVisits *visits, const ZirModule *module)
{
    for(size_t index = 0; index < visits->count; index++)
        if(visits->items[index].module == module)
            return &visits->items[index];
    if(visits->count == visits->capacity) {
        size_t capacity = visits->capacity ? visits->capacity * 2 : 16;
        PyModuleVisit *items = realloc(visits->items, capacity * sizeof(*items));
        if(items == NULL) {
            DiagnosticOutOfMemory();
            exit(1);
        }
        visits->items = items;
        visits->capacity = capacity;
    }
    visits->items[visits->count].module = module;
    visits->items[visits->count].state = 0;
    return &visits->items[visits->count++];
}

static void visit_startup_module(PyModuleVisits *visits, PyModuleVisits *ordered,
                                 const ZirModule *module)
{
    PyModuleVisit *visit = module_visit(visits, module);
    if(visit->state == 1) {
        Diagnostic(module->span, "zir_py.startup", "cyclic module startup is unsupported");
        exit(1);
    }
    if(visit->state == 2)
        return;
    visit->state = 1;
    /* As in C and Go, only dependencies with something to set up take part:
     * an import cycle through modules without globals orders nothing. */
    for(int index = 0; index < module->import_count; index++)
        if(module->imports[index].resolved_module != NULL &&
           ModuleNeedsStartup(module->imports[index].resolved_module))
            visit_startup_module(visits, ordered, module->imports[index].resolved_module);
    visit = module_visit(visits, module);
    visit->state = 2;
    for(int index = 0; index < module->function_count; index++)
        if(module->functions[index].is_global_initializer) {
            int present = 0;
            for(size_t scan = 0; scan < ordered->count; scan++)
                present |= ordered->items[scan].module == module;
            if(!present)
                module_visit(ordered, module);
            break;
        }
}

static void emit_startup(PyEmitter *emitter, FILE *output)
{
    PyModuleVisits visits = {0}, ordered = {0};
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++)
            if(ModuleNeedsStartup(&program->modules[module_index]))
                visit_startup_module(&visits, &ordered, &program->modules[module_index]);
    }
    for(size_t index = 0; index < ordered.count; index++) {
        const ZirModule *module = ordered.items[index].module;
        for(int function_index = 0; function_index < module->function_count; function_index++) {
            const ZirFunction *function = &module->functions[function_index];
            char symbol[ZIR_NAME_MAX * 2];
            if(!function->is_global_initializer)
                continue;
            function_symbol(emitter, module, function, symbol, sizeof(symbol));
            fprintf(output, "%s()\n", symbol);
        }
    }
    if(ordered.count > 0)
        fputs("\n", output);
    free(visits.items);
    free(ordered.items);
}

/* ----- output ----- */

static int text_names(const char *text, size_t length, const char *name)
{
    size_t size = strlen(name);
    for(size_t index = 0; index + size <= length; index++) {
        if(memcmp(text + index, name, size) != 0)
            continue;
        if(index > 0 && (identifier_character(text[index - 1]) || text[index - 1] == '.'))
            continue;
        if(index + size < length && identifier_character(text[index + size]))
            continue;
        return 1;
    }
    return 0;
}

static int text_mentions(const char *text, size_t length, const char *needle)
{
    size_t size = strlen(needle);
    for(size_t index = 0; index + size <= length; index++)
        if(memcmp(text + index, needle, size) == 0)
            return 1;
    return 0;
}

/* The runtime items the program names, directly or through another kept
 * item, in runtime order. */
static void write_runtime(FILE *output, const char *body, size_t body_size,
                          int *kept, int *module_needed)
{
    int changed = 1, count = 0;
    while(py_runtime_items[count].name != NULL)
        count++;
    for(int index = 0; index < count; index++)
        kept[index] = text_names(body, body_size, py_runtime_items[index].name);
    while(changed) {
        changed = 0;
        for(int index = 0; index < count; index++) {
            if(kept[index])
                continue;
            for(int other = 0; other < count && !kept[index]; other++)
                if(kept[other] && other != index &&
                   text_names(py_runtime_items[other].text,
                              strlen(py_runtime_items[other].text),
                              py_runtime_items[index].name))
                    kept[index] = changed = 1;
        }
    }
    (void)module_needed;
    (void)output;
}

static void write_imports(FILE *output, const char *body, size_t body_size, const int *kept)
{
    static const char *const modules[] = {
        "ctypes", "ctypes.util", "decimal", "importlib", "math", "struct", "sys", NULL
    };
    int any = 0;
    for(int module = 0; modules[module] != NULL; module++) {
        char prefix[64];
        int needed = 0;
        snprintf(prefix, sizeof(prefix), "%s.", modules[module]);
        for(int index = 0; py_runtime_items[index].name != NULL && !needed; index++) {
            const char *imports = py_runtime_items[index].imports;
            if(!kept[index])
                continue;
            for(const char *at = imports; (at = strstr(at, modules[module])) != NULL;
                at += strlen(modules[module])) {
                size_t length = strlen(modules[module]);
                if((at == imports || at[-1] == ' ') && (at[length] == '\0' || at[length] == ' '))
                    needed = 1;
            }
        }
        if(!needed && strcmp(modules[module], "ctypes.util") != 0)
            needed = text_names(body, body_size, modules[module]) &&
                     text_mentions(body, body_size, prefix);
        if(needed) {
            fprintf(output, "import %s\n", modules[module]);
            any = 1;
        }
    }
    if(any)
        fputs("\n", output);
}

static void collect_module_names(PyEmitter *emitter)
{
    char name[ZIR_NAME_MAX * 2];
    for(int index = 0; py_runtime_items[index].name != NULL; index++)
        names_add(&emitter->module_names, py_runtime_items[index].name);
    names_add(&emitter->module_names, "ziran_jump");
    for(int program_index = 0; program_index < emitter->program_count; program_index++) {
        const ZirProgram *program = emitter->programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            for(int index = 0; index < module->function_count; index++) {
                function_symbol(emitter, module, &module->functions[index], name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
            for(int index = 0; index < module->global_count; index++) {
                global_symbol(emitter, module, &module->globals[index], name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
            for(int index = 0; index < module->import_count; index++)
                if(module->imports[index].kind == ZIR_IMPORT_EXTERN) {
                    foreign_symbol(emitter, module, &module->imports[index], name, sizeof(name));
                    names_add(&emitter->module_names, name);
                }
            for(int index = 0; index < module->type_count; index++) {
                PyType info;
                if(module->types[index].is_record_template ||
                   !py_classify(module, module->types[index].name, &info) ||
                   (info.kind != PY_RECORD && info.kind != PY_UNION))
                    continue;
                py_class_name(&info, name, sizeof(name));
                names_add(&emitter->module_names, name);
            }
        }
    }
}

static void make_directories(const char *path)
{
    char partial[ZIR_PATH_MAX * 2];
    snprintf(partial, sizeof(partial), "%s", path);
    for(size_t index = 1; partial[index]; index++) {
        if(partial[index] != '/')
            continue;
        partial[index] = '\0';
        mkdir(partial, 0755);
        partial[index] = '/';
    }
    mkdir(partial, 0755);
}

static int integer_type(const char *type)
{
    int bits, is_signed;
    return py_integer_width(type, &bits, &is_signed);
}

static int emit_python(const ZirProgram *const *programs, int program_count,
             const char *output_directory, const char *entry_module,
             const char *entry_function, int executable)
{
    PyEmitter *emitter = py_allocate(sizeof(*emitter));
    const ZirModule *entry_owner = NULL;
    const ZirFunction *entry = NULL;
    char *body_text = NULL;
    size_t body_size = 0;
    FILE *body, *output;
    char path[ZIR_PATH_MAX * 2];
    char temporary[ZIR_PATH_MAX * 2 + 64];
    int *kept;
    int runtime_count = 0;
    if(programs == NULL || program_count <= 0 || output_directory == NULL ||
       output_directory[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "zir_py.input",
                   "no checked programs were supplied");
        return 1;
    }
    emitter->programs = programs;
    emitter->program_count = program_count;
    if(executable) {
        for(int program_index = 0; program_index < program_count; program_index++)
            for(int module_index = 0; module_index < programs[program_index]->module_count;
                module_index++) {
                const ZirModule *module = &programs[program_index]->modules[module_index];
                if(strcmp(module->name, entry_module) != 0)
                    continue;
                for(int function_index = 0; function_index < module->function_count;
                    function_index++)
                    if(strcmp(module->functions[function_index].name, entry_function) == 0) {
                        entry_owner = module;
                        entry = &module->functions[function_index];
                    }
            }
        if(entry == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zir_py.entry",
                       "executable entry is missing: %s:%s", entry_module, entry_function);
            return 1;
        }
        if(FunctionArgs(entry)[0] || (strcmp(entry->return_type, "void") != 0 &&
                              !integer_type(entry->return_type) &&
                              strcmp(entry->return_type, "bool") != 0)) {
            Diagnostic(entry->span, "zir_py.entry",
                       "executable entry must take no arguments and return void, bool, or an integer");
            return 1;
        }
    }
    collect_module_names(emitter);
    body = open_memstream(&body_text, &body_size);
    if(body == NULL) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output", "cannot buffer Python output");
        return 1;
    }
    emitter->output = body;
    emit_classes(emitter, body);
    emit_foreign_bindings(emitter, body);
    for(int program_index = 0; program_index < program_count; program_index++) {
        const ZirProgram *program = programs[program_index];
        for(int module_index = 0; module_index < program->module_count; module_index++) {
            const ZirModule *module = &program->modules[module_index];
            for(int function_index = 0; function_index < module->function_count;
                function_index++)
                lower_function(emitter, module, &module->functions[function_index]);
        }
    }
    if(emitter->uses_host)
        fputs("# The embedding program sets host to an object with the host functions.\n"
              "host = None\n\n", body);
    emit_globals(emitter, body);
    emit_startup(emitter, body);
    if(executable) {
        char symbol[ZIR_NAME_MAX * 2];
        function_symbol(emitter, entry_owner, entry, symbol, sizeof(symbol));
        fputs("if __name__ == \"__main__\":\n", body);
        if(strcmp(entry->return_type, "void") == 0)
            fprintf(body, "    _ziran_entry(%s)\n", symbol);
        else if(strcmp(entry->return_type, "bool") == 0)
            fprintf(body, "    sys.exit(1 if _ziran_entry(%s) else 0)\n", symbol);
        else
            fprintf(body, "    sys.exit(_ziran_entry(%s) & 0xFF)\n", symbol);
    }
    if(fclose(body) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output", "cannot buffer Python output");
        return 1;
    }
    /* Trailing blank lines end the file at one newline. */
    while(body_size > 1 && body_text[body_size - 1] == '\n' && body_text[body_size - 2] == '\n')
        body_size--;
    while(py_runtime_items[runtime_count].name != NULL)
        runtime_count++;
    kept = py_allocate(sizeof(*kept) * (size_t)(runtime_count + 1));
    write_runtime(NULL, body_text, body_size, kept, NULL);
    make_directories(output_directory);
    snprintf(path, sizeof(path), "%s/%s", output_directory,
             executable ? "__main__.py" : "__init__.py");
    output = GeneratedOutputOpen(path, temporary, sizeof(temporary));
    if(output == NULL) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output",
                   "cannot create Python output: %s", path);
        return 1;
    }
    if(program_count == 1 && programs[0]->module_count == 1)
        fprintf(output, "# Code generated by zi2py from %s. DO NOT EDIT.\n\n",
                programs[0]->modules[0].source_path);
    else
        fprintf(output, "# Code generated by zi2py from %s. DO NOT EDIT.\n\n",
                entry_owner != NULL ? entry_owner->source_path :
                                      programs[0]->modules[0].source_path);
    write_imports(output, body_text, body_size, kept);
    for(int index = 0; index < runtime_count; index++)
        if(kept[index]) {
            fputs(py_runtime_items[index].text, output);
            fputs("\n\n", output);
        }
    fwrite(body_text, 1, body_size, output);
    free(kept);
    free(body_text);
    if(fclose(output) != 0 || GeneratedOutputReplace(temporary, path) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_py.output",
                   "cannot finish Python output");
        return 1;
    }
    return 0;
}

int py_lower_in_context(const ZirProgram *const *programs, int program_count,
             const char *output_directory, const char *entry_module,
             const char *entry_function, int executable)
{
    uint64_t started = ProfileStart();
    int result = emit_python(programs, program_count, output_directory,
                             entry_module, entry_function, executable);
    ProfileEnd("emit.py", started);
    return result;
}

int py_lower(const ZirProgram *const *programs, int program_count, const char *output_directory, const char *entry_module, const char *entry_function, int executable)
{
    CompilerContext *previous = CompilerContextSelect(program_count > 0 && programs && programs[0] ? programs[0]->context : NULL);
    int result = py_lower_in_context(programs, program_count, output_directory, entry_module, entry_function, executable);
    CompilerContextSelect(previous);
    return result;
}
