/* GNU assembly uses the same checked C emitter as native C. Go retains
 * its typed body and enables the native bridge with cgo and ziran_asm. */
#include "zir_go_asm.h"
#include "zir_go_amd64.h"
#include "zir_asm.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"

#include <stdlib.h>
#include <string.h>

typedef struct GoAssemblySite {
    const ZirFunction *function;
    int statement, by_value;
    unsigned char storage[ASSEMBLY_OPERANDS];
    AssemblyVariant variant;
    GoAmd64Plan **direct;
    int direct_count;
    char name[ZIR_NAME_MAX];
} GoAssemblySite;

struct GoAssembly {
    const ZirModule *module;
    int count;
    GoAssemblySite *sites;
};

/* Passing a pointer to scalar-only storage is valid under cgo's pointer
 * rules. Go pointer-bearing objects need a separate pinning/lifetime design;
 * leave their checked Go body in place rather than disabling cgocheck. */
static int
pointer_free(const ZirModule *module, const char *type, int depth)
{
    if(depth >= 32 || type[0] == '*' || !strcmp(type, "string") ||
       !strcmp(type, "void")) return 0;
    if(ScalarType(type)[0]) return 1;
    char element[ZIR_NAME_MAX];
    int capacity;
    if(ArrayElementType(type, element, sizeof(element), &capacity))
        return capacity >= 0 && pointer_free(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_extern || record->is_procedure_type ||
       record->is_map || record->is_owned_vec || record->foreign_target[0]) return 0;
    if(record->is_enum) return 1;
    size_t offset = 0;
    ZirTypeField field;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(!pointer_free(owner ? owner : module, field.type, depth + 1)) return 0;
    return status == 0;
}

static int
bridge_type(const ZirModule *module, const char *type)
{
    return type[0] == '*' ? pointer_free(module, type + 1, 0) :
        ScalarType(type)[0] && strcmp(type, "string") && strcmp(type, "void");
}

static void
c_type(const char *type, char *out, size_t size)
{
    const char *native = type[0] == '*' ? "void *" : TargetType(ScalarType(type), ZIR_C);
    if(native == NULL) abort();
    snprintf(out, size, "%s", native);
}

static void
go_type(const ZirModule *module, const char *type, char *out, size_t size)
{
    if(!NativeGoType(module, type, out, size)) abort();
}

static int
name_used(const ZirProgram *const *programs, int count, const char *name)
{
    char native[ZIR_NAME_MAX * 4];
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        const ZirModule *module = &programs[p]->modules[m];
        for(int f = 0; f < module->function_count; f++) {
            const ZirFunction *fn = &module->functions[f];
            NativeGoFunctionName(programs, count, module, fn, native, sizeof(native));
            if(!strncmp(native, name, strlen(name)) || !strncmp(fn->export_symbol, name, strlen(name))) return 1;
        }
        for(int g = 0; g < module->global_count; g++) {
            TargetGlobalName(module, ZIR_GO, module->globals[g].name, native, sizeof(native));
            if(!strncmp(native, name, strlen(name))) return 1;
        }
        for(int t = 0; t < module->type_count; t++) {
            NativeTypeName(module, &module->types[t], native, sizeof(native));
            if(!strncmp(native, name, strlen(name))) return 1;
        }
        for(int d = 0; d < module->define_count; d++) {
            TargetDefineName(module, ZIR_GO, module->defines[d].name, native, sizeof(native));
            if(!strncmp(native, name, strlen(name))) return 1;
        }
    }
    return 0;
}

static int
local_name_used(const ZirFunction *fn, const char *name)
{
    char native[ZIR_NAME_MAX * 4];
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++) {
        TargetBindingName(fn, ZIR_GO, parameters->items[i].name, native, sizeof(native));
        if(!strcmp(native, name)) return 1;
    }
    for(int i = 0; i < fn->stmt_count; i++) if(fn->stmts[i].kind == ZIR_STMT_DECL) {
        TargetBindingName(fn, ZIR_GO, fn->stmts[i].name, native, sizeof(native));
        if(!strcmp(native, name)) return 1;
    }
    return 0;
}

GoAssembly *
GoAssemblyOpen(const ZirProgram *const *programs, int count, const ZirModule *module)
{
    GoAssembly *assembly = AllocateOrExit(sizeof(*assembly));
    memset(assembly, 0, sizeof(*assembly));
    assembly->module = module;
    int capacity = 0;
    int global = 0;
    for(int p = 0; p < count; p++) for(int m = 0; m < programs[p]->module_count; m++) {
        const ZirModule *current = &programs[p]->modules[m];
        if(current != module) { global += current->function_count; continue; }
        for(int f = 0; f < current->function_count; f++, global++) {
            const ZirFunction *fn = &current->functions[f];
            if(fn->is_template || fn->is_extern) continue;
            for(int statement = -1; statement < fn->stmt_count; statement++) {
                const char *cursor = statement < 0 ? fn->assembly_text :
                    StatementHasAssembly(&fn->stmts[statement]) ? fn->stmts[statement].assembly_text : NULL;
                if(cursor == NULL || !cursor[0]) continue;
                GoAssemblySite candidate = {0};
                GoAssemblySite *site = &candidate;
                if(AssemblyVariantNext(module, fn, statement, &cursor, &site->variant) != 1) abort();
                int eligible = 1;
                site->by_value = 1;
                AssemblyVariant variant = site->variant;
                do {
                    for(int i = 0; i < variant.outputs + variant.inputs; i++) {
                        const AssemblyOperand *operand = &variant.operands[i];
                        eligible &= bridge_type(module, operand->type);
                        if(statement >= 0) {
                            if(i < variant.outputs) {
                                if(strcmp(operand->constraint, "=r") && strcmp(operand->constraint, "=&r"))
                                    site->by_value = 0;
                                if(operand->type[0] == '*') eligible = 0;
                            } else if(AssemblyInputNeedsStorage(operand->constraint)) {
                                site->storage[i] = 1;
                                if(operand->type[0] == '*') eligible = 0;
                            }
                            /* The shared C feature helper currently names a
                             * pointed-to record, but not an array declarator. */
                            if(operand->type[0] == '*' && operand->type[1] == '[') eligible = 0;
                        }
                    }
                } while(AssemblyVariantNext(module, fn, statement, &cursor, &variant) == 1);
                if(!eligible) continue;
                cursor = statement < 0 ? fn->assembly_text : fn->stmts[statement].assembly_text;
                while(AssemblyVariantNext(module, fn, statement, &cursor, &variant) == 1) {
                    GoAmd64Plan *plan = GoAmd64Prepare(&variant, statement >= 0,
                        site->by_value, site->storage);
                    if(plan == NULL) continue;
                    GoAmd64Plan **plans = realloc(site->direct,
                        (size_t)(site->direct_count + 1) * sizeof(*plans));
                    if(plans == NULL) { GoAmd64Close(plan); break; }
                    site->direct = plans;
                    site->direct[site->direct_count++] = plan;
                }
                site->function = fn;
                site->statement = statement;
                for(int serial = 0;; serial++) {
                    char handler[ZIR_NAME_MAX + 8];
                    snprintf(site->name, sizeof(site->name), "__ziranAssembly%d_%d_%d", global, statement + 1, serial);
                    snprintf(handler, sizeof(handler), "%sNative", site->name);
                    if(!name_used(programs, count, site->name) && !name_used(programs, count, handler) &&
                       !local_name_used(fn, site->name)) break;
                }
                if(assembly->count == capacity) {
                    capacity = capacity ? capacity * 2 : 8;
                    GoAssemblySite *sites = realloc(assembly->sites, (size_t)capacity * sizeof(*sites));
                    if(sites == NULL) {
                        Diagnostic(module->span, "zir_go.body", "out of memory preparing assembly bridges");
                        exit(1);
                    }
                    assembly->sites = sites;
                }
                assembly->sites[assembly->count++] = candidate;
            }
        }
    }
    return assembly;
}

static const GoAssemblySite *
find_site(const GoAssembly *assembly, const ZirFunction *function, int statement)
{
    for(int i = 0; i < assembly->count; i++)
        if(assembly->sites[i].function == function && assembly->sites[i].statement == statement) return &assembly->sites[i];
    return NULL;
}

static void
bridge_go_type(const char *type, char *out, size_t size)
{
    const char *native = type[0] == '*' ? "unsafe.Pointer" : TargetType(ScalarType(type), ZIR_GO);
    if(native == NULL) abort();
    snprintf(out, size, "%s", native);
}

static void
go_signature(FILE *out, const ZirModule *module, const GoAssemblySite *site, int named)
{
    (void)module;
    const AssemblyVariant *v = &site->variant;
    int block = site->statement >= 0;
    int first = block && !site->by_value ? 0 : v->outputs;
    for(int i = first; i < v->outputs + v->inputs; i++) {
        char type[ZIR_NAME_MAX * 4];
        bridge_go_type(v->operands[i].type, type, sizeof(type));
        fprintf(out, "%s", i > first ? ", " : "");
        if(named) fprintf(out, "a%d ", i);
        fprintf(out, "%s%s", block && (i < v->outputs || site->storage[i]) ? "*" : "", type);
    }
    if((!block || site->by_value) && v->outputs) {
        fputs(") (", out);
        for(int i = 0; i < v->outputs; i++) {
            char type[ZIR_NAME_MAX * 4];
            bridge_go_type(v->operands[i].type, type, sizeof(type));
            fprintf(out, "%s, ", type);
        }
        fputs("bool)", out);
    } else fputs(") bool", out);
}

void
GoAssemblyFunction(FILE *out, const GoAssembly *assembly, const ZirFunction *function)
{
    const GoAssemblySite *site = find_site(assembly, function, -1);
    if(site == NULL) return;
    const AssemblyVariant *v = &site->variant;
    char available[ZIR_NAME_MAX];
    ArrayAbiName(function, 600000, available, sizeof(available));
    fprintf(out, "    if zirGoAssemblyEnabled && %s != nil {\n        if ", site->name);
    for(int i = 0; i < v->outputs; i++) {
        char value[ZIR_NAME_MAX]; ArrayAbiName(function, 600001 + i, value, sizeof(value));
        fprintf(out, "%s, ", value);
    }
    fprintf(out, "%s := %s(", available, site->name);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
    for(int i = 0; i < parameters->count; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(function, ZIR_GO, parameters->items[i].name, name, sizeof(name));
        fprintf(out, "%s%s%s%s", i ? ", " : "", parameters->items[i].type[0] == '*' ? "unsafe.Pointer(" : "",
                name, parameters->items[i].type[0] == '*' ? ")" : "");
    }
    fprintf(out, "); %s { return", available);
    if(v->record) {
        char type[ZIR_NAME_MAX * 4];
        go_type(assembly->module, function->return_type, type, sizeof(type));
        fprintf(out, " %s{", type);
    }
    const ZirModule *owner = NULL;
    if(v->record) FindType(assembly->module, function->return_type, &owner);
    for(int i = 0; i < v->outputs; i++) {
        char value[ZIR_NAME_MAX], type[ZIR_NAME_MAX * 4];
        ArrayAbiName(function, 600001 + i, value, sizeof(value));
        if(v->record) {
            char field[ZIR_NAME_MAX]; camel_ident(v->operands[i].binding, field, sizeof(field));
            fprintf(out, "%s%s: ", i ? ", " : "", field);
        } else fputc(' ', out);
        if(v->operands[i].type[0] == '*') {
            go_type(owner ? owner : assembly->module, v->operands[i].type, type, sizeof(type));
            fprintf(out, "(%s)(%s)", type, value);
        } else fputs(value, out);
    }
    if(v->record) fputc('}', out);
    fputs(" }\n    }\n", out);
}

int
GoAssemblyBlock(FILE *out, const ZirModule *module, const ZirFunction *fn,
                 int statement, void *context)
{
    (void)module;
    const GoAssemblySite *site = find_site(context, fn, statement);
    if(site == NULL) return 0;
    const AssemblyVariant *v = &site->variant;
    char done[ZIR_NAME_MAX], ok[ZIR_NAME_MAX];
    ArrayAbiName(fn, 700000 + statement * 32, done, sizeof(done));
    ArrayAbiName(fn, 700001 + statement * 32, ok, sizeof(ok));
    fprintf(out, "    {\n        %s := false\n        if zirGoAssemblyEnabled && %s != nil {\n            ", done, site->name);
    if(site->by_value) for(int i = 0; i < v->outputs; i++) {
        char value[ZIR_NAME_MAX];
        ArrayAbiName(fn, 700002 + statement * 32 + i, value, sizeof(value));
        fprintf(out, "%s, ", value);
    }
    fprintf(out, "%s := %s(", ok, site->name);
    int first = site->by_value ? v->outputs : 0;
    for(int i = first; i < v->outputs + v->inputs; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, ZIR_GO, v->operands[i].binding, name, sizeof(name));
        fprintf(out, "%s%s%s%s", i > first ? ", " : "",
                i < v->outputs || site->storage[i] ? "&" : v->operands[i].type[0] == '*' ? "unsafe.Pointer(" : "",
                name, v->operands[i].type[0] == '*' ? ")" : "");
    }
    fprintf(out, ")\n            if %s {\n", ok);
    if(site->by_value) for(int i = 0; i < v->outputs; i++) {
        char value[ZIR_NAME_MAX], name[ZIR_NAME_MAX * 4];
        ArrayAbiName(fn, 700002 + statement * 32 + i, value, sizeof(value));
        TargetBindingName(fn, ZIR_GO, v->operands[i].binding, name, sizeof(name));
        fprintf(out, "                %s = %s\n", name, value);
    }
    fprintf(out, "                %s = true\n            }\n        }\n        if !%s {\n", done, done);
    return 1;
}

static void
native_record(FILE *out, const ZirModule *module, const GoAssemblySite *site,
              char *type, size_t size)
{
    if(!site->variant.record) { c_type(site->function->return_type, type, size); return; }
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, site->function->return_type, &owner);
    NativeTypeName(owner ? owner : module, record, type, size);
    fprintf(out, "#ifndef %s_DEFINED\n#define %s_DEFINED\ntypedef struct %s {\n", type, type, type);
    size_t offset = 0;
    ZirTypeField field;
    while(TypeNextField(record, &offset, &field) == 1) {
        char scalar[ZIR_NAME_MAX], name[ZIR_NAME_MAX * 4];
        c_type(field.type, scalar, sizeof(scalar));
        TargetFieldName(record, ZIR_C, field.name, name, sizeof(name));
        fprintf(out, "    %s %s;\n", scalar, name);
    }
    fprintf(out, "} %s;\n#endif\n", type);
}

static void
c_function(FILE *out, const ZirModule *module, const GoAssemblySite *site)
{
    const ZirFunction *fn = site->function;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    char result_type[ZIR_NAME_MAX * 4], available[ZIR_NAME_MAX];
    native_record(out, module, site, result_type, sizeof(result_type));
    ArrayAbiName(fn, 900000, available, sizeof(available));
    char *args = AllocateOrExit(ASSEMBLY_OPERANDS * ZIR_NAME_MAX * 5);
    args[0] = 0;
    size_t used = 0, capacity = ASSEMBLY_OPERANDS * ZIR_NAME_MAX * 5;
    for(int i = 0; i < parameters->count; i++) {
        char type[ZIR_NAME_MAX], name[ZIR_NAME_MAX * 4];
        c_type(parameters->items[i].type, type, sizeof(type));
        TargetBindingName(fn, ZIR_C, parameters->items[i].name, name, sizeof(name));
        int n = snprintf(args + used, capacity - used, "%s%s %s", i ? ", " : "", type, name);
        if(n < 0 || (size_t)n >= capacity - used) abort();
        used += (size_t)n;
    }
    snprintf(args + used, capacity - used, "%sint *%s", used ? ", " : "", available);
    EmitAssemblyTryHelpers(out, module, fn, ZIR_C, result_type, args, available);
    fprintf(out, "static %s %sAsm(%s) {\n    *%s = 0;\n", result_type, site->name, args, available);
    for(int i = 0; i < parameters->count; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, ZIR_C, parameters->items[i].name, name, sizeof(name));
        fprintf(out, "    (void)%s;\n", name);
    }
    EmitAssemblyTry(out, module, fn, ZIR_C, result_type, available);
    if(!strcmp(fn->return_type, "void")) fputs("    return;\n}\n", out);
    else fprintf(out, "    return (%s){0};\n}\n", result_type);
    fprintf(out, "typedef struct { int available;\n");
    for(int i = 0; i < site->variant.outputs; i++) {
        char type[ZIR_NAME_MAX]; c_type(site->variant.operands[i].type, type, sizeof(type));
        fprintf(out, "    %s v%d;\n", type, i);
    }
    fprintf(out, "} %sResult;\nstatic %sResult %sC(", site->name, site->name, site->name);
    for(int i = 0; i < parameters->count; i++) {
        char type[ZIR_NAME_MAX]; c_type(parameters->items[i].type, type, sizeof(type));
        fprintf(out, "%s%s a%d", i ? ", " : "", type, i);
    }
    if(!parameters->count) fputs("void", out);
    fprintf(out, ") {\n    %sResult result = {0};\n    ", site->name);
    if(site->variant.outputs) fprintf(out, "%s value = ", result_type);
    fprintf(out, "%sAsm(", site->name);
    for(int i = 0; i < parameters->count; i++) fprintf(out, "a%d, ", i);
    fputs("&result.available);\n", out);
    for(int i = 0; i < site->variant.outputs; i++) {
        fprintf(out, "    result.v%d = value", i);
        if(site->variant.record) {
            const ZirType *record = FindType(module, fn->return_type, NULL);
            char name[ZIR_NAME_MAX * 4];
            TargetFieldName(record, ZIR_C, site->variant.operands[i].binding, name, sizeof(name));
            fprintf(out, ".%s", name);
        }
        fputs(";\n", out);
    }
    fputs("    return result;\n}\n\n", out);
    free(args);
}

static void
go_conversion(FILE *out, const ZirModule *module, const char *type, int i)
{
    (void)module;
    char native[ZIR_NAME_MAX * 4]; bridge_go_type(type, native, sizeof(native));
    if(type[0] == '*') fprintf(out, "(%s)(unsafe.Pointer(result.v%d))", native, i);
    else fprintf(out, "%s(result.v%d)", native, i);
}

static void
block_parameter(const ZirFunction *fn, int statement, int operand, char *out, size_t size)
{
    ArrayAbiName(fn, 800000 + statement * 32 + operand, out, size);
}

static void
block_operand(void *context, int operand, char *out, size_t size)
{
    const GoAssemblySite *site = context;
    char name[ZIR_NAME_MAX];
    int output = operand < site->variant.outputs;
    block_parameter(site->function, site->statement,
        output && site->by_value ? ASSEMBLY_OPERANDS + operand : operand, name, sizeof(name));
    snprintf(out, size, "%s%s%s", (output && !site->by_value) || site->storage[operand] ? "(*" : "",
             name, (output && !site->by_value) || site->storage[operand] ? ")" : "");
}

static void
c_block(FILE *out, const ZirModule *module, const GoAssemblySite *site)
{
    const ZirFunction *fn = site->function;
    const AssemblyVariant *v = &site->variant;
    for(int i = v->outputs; i < v->outputs + v->inputs; i++) {
        const char *type = v->operands[i].type;
        if(type[0] != '*') continue;
        const ZirModule *owner = NULL;
        const ZirType *record = FindType(module, type + 1, &owner);
        if(record) {
            char native[ZIR_NAME_MAX * 4];
            NativeTypeName(owner ? owner : module, record, native, sizeof(native));
            fprintf(out, "#ifndef %s_DECLARED\n#define %s_DECLARED\ntypedef struct %s %s;\n#endif\n",
                    native, native, native, native);
        }
    }
    EmitAssemblyBlockHelpersAt(out, module, fn, site->statement);
    fprintf(out, "typedef struct { int available;\n");
    if(site->by_value) for(int i = 0; i < v->outputs; i++) {
        char type[ZIR_NAME_MAX]; c_type(v->operands[i].type, type, sizeof(type));
        fprintf(out, "    %s v%d;\n", type, i);
    }
    fprintf(out, "} %sResult;\nstatic %sResult %sC(", site->name, site->name, site->name);
    int first = site->by_value ? v->outputs : 0;
    for(int i = first; i < v->outputs + v->inputs; i++) {
        char type[ZIR_NAME_MAX], name[ZIR_NAME_MAX];
        c_type(v->operands[i].type, type, sizeof(type));
        block_parameter(fn, site->statement, i, name, sizeof(name));
        fprintf(out, "%s%s %s%s", i > first ? ", " : "", type,
                i < v->outputs || site->storage[i] ? "*" : "", name);
    }
    if(first == v->outputs + v->inputs) fputs("void", out);
    fprintf(out, ") {\n");
    if(site->by_value) for(int i = 0; i < v->outputs; i++) {
        char type[ZIR_NAME_MAX], name[ZIR_NAME_MAX];
        c_type(v->operands[i].type, type, sizeof(type));
        block_parameter(fn, site->statement, ASSEMBLY_OPERANDS + i, name, sizeof(name));
        fprintf(out, "    %s %s = {0};\n", type, name);
    }
    for(int i = first; i < v->outputs + v->inputs; i++) {
        char parameter[ZIR_NAME_MAX];
        block_parameter(fn, site->statement, i, parameter, sizeof(parameter));
        fprintf(out, "    (void)%s;\n", parameter);
    }
    if(!EmitAssemblyBlockMapped(out, module, fn, site->statement, block_operand, (void *)site)) abort();
    fprintf(out, "            return (%sResult){0};\n    }\n    }\n", site->name);
    fprintf(out, "    return (%sResult){1", site->name);
    if(site->by_value) for(int i = 0; i < v->outputs; i++) {
        char name[ZIR_NAME_MAX];
        block_parameter(fn, site->statement, ASSEMBLY_OPERANDS + i, name, sizeof(name));
        fprintf(out, ", %s", name);
    }
    fputs("};\n", out);
    fputs("}\n\n", out);
}

static int
write_direct(const GoAssembly *assembly, const char *path, const char *package)
{
    const GoAssemblySite *first = NULL;
    for(int i = 0; i < assembly->count && first == NULL; i++)
        for(int j = 0; j < assembly->sites[i].direct_count; j++)
            if(GoAmd64Supported(assembly->sites[i].direct[j])) { first = &assembly->sites[i]; break; }
    if(first == NULL) return 0;
    char declaration[ZIR_PATH_MAX], instruction[ZIR_PATH_MAX];
    size_t length = strlen(path);
    if(length < 3 || length + 40 >= sizeof(declaration)) return 1;
    snprintf(declaration, sizeof(declaration), "%.*s.assembly_direct_amd64.go", (int)length - 3, path);
    snprintf(instruction, sizeof(instruction), "%.*s.assembly_direct_amd64.s", (int)length - 3, path);
    GeneratedOutputRecord(declaration); GeneratedOutputRecord(instruction);
    FILE *go = fopen(declaration, "wb"), *native = fopen(instruction, "wb");
    if(go == NULL || native == NULL) {
        if(go) fclose(go);
        if(native) fclose(native);
        Diagnostic(assembly->module->span, "zir_go.body", "cannot create direct assembly companions");
        return 1;
    }
    fprintf(go, "// Code generated by zi2go from %s. DO NOT EDIT.\n"
        "//go:build !cgo && ziran_asm && amd64\n\npackage %s\n\n", assembly->module->source_path, package);
    fprintf(native, "// Code generated by zi2go from %s. DO NOT EDIT.\n"
        "//go:build !cgo && ziran_asm && amd64\n\n#include \"textflag.h\"\n\n", assembly->module->source_path);
    GoAmd64Cpu(go, native, first->name);
    for(int i = 0; i < assembly->count; i++) {
        const GoAssemblySite *site = &assembly->sites[i];
        if(!site->direct_count) continue;
        int supported = 0;
        for(int j = 0; j < site->direct_count; j++) if(GoAmd64Supported(site->direct[j])) {
            char name[ZIR_NAME_MAX + 32];
            snprintf(name, sizeof(name), "%sDirectAmd64%d", site->name, j);
            GoAmd64Write(go, native, site->direct[j], name);
            supported = 1;
        }
        if(!supported) continue;
        fputs("var _ = func() bool {\n", go);
        for(int j = 0; j < site->direct_count; j++) {
            unsigned long long mask = (unsigned long long)GoAmd64Features(site->direct[j]);
            fprintf(go, "    %sif %sFeatures & 0x%llx == 0x%llx {\n", j ? "} else " : "", first->name, mask, mask);
            if(GoAmd64Supported(site->direct[j]))
                fprintf(go, "        %s = %sDirectAmd64%d\n", site->name, site->name, j);
        }
        fputs("    }\n    return true\n}()\n\n", go);
    }
    int failed = ferror(go) || ferror(native);
    failed |= fclose(go) != 0; failed |= fclose(native) != 0;
    if(failed) Diagnostic(assembly->module->span, "zir_go.body", "cannot finish direct assembly companions");
    return failed;
}

int
GoAssemblyWrite(const GoAssembly *assembly, const char *path, const char *package)
{
    if(!assembly->count) return 0;
    char header[ZIR_PATH_MAX], companion[ZIR_PATH_MAX], dispatch[ZIR_PATH_MAX];
    size_t length = strlen(path);
    if(length < 3 || length + 24 >= sizeof(header)) return 1;
    snprintf(header, sizeof(header), "%.*s.assembly.h", (int)length - 3, path);
    snprintf(companion, sizeof(companion), "%.*s.assembly.go", (int)length - 3, path);
    snprintf(dispatch, sizeof(dispatch), "%.*s.assembly_dispatch.go", (int)length - 3, path);
    GeneratedOutputRecord(header); GeneratedOutputRecord(companion); GeneratedOutputRecord(dispatch);
    FILE *c = fopen(header, "wb"), *go = fopen(companion, "wb"), *decl = fopen(dispatch, "wb");
    if(c == NULL || go == NULL || decl == NULL) {
        if(c) fclose(c);
        if(go) fclose(go);
        if(decl) fclose(decl);
        Diagnostic(assembly->module->span, "zir_go.body", "cannot create assembly companions");
        return 1;
    }
    fprintf(c, "// Code generated by zi2go from %s. DO NOT EDIT.\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n", assembly->module->source_path);
    const char *base = strrchr(header, '/'); base = base ? base + 1 : header;
    fprintf(go, "// Code generated by zi2go from %s. DO NOT EDIT.\n//go:build cgo && ziran_asm\n\npackage %s\n\n/*\n#include \"%s\"\n*/\nimport \"C\"\nimport \"unsafe\"\n\nvar _ unsafe.Pointer\n\n", assembly->module->source_path, package, base);
    fprintf(decl, "// Code generated by zi2go from %s. DO NOT EDIT.\npackage %s\n\nimport \"unsafe\"\nvar _ unsafe.Pointer\n\n", assembly->module->source_path, package);
    for(int i = 0; i < assembly->count; i++) {
        const GoAssemblySite *site = &assembly->sites[i];
        fprintf(decl, "// Native assembly is installed only when a variant is available.\nvar %s func(", site->name);
        go_signature(decl, assembly->module, site, 0);
        fputs("\n\n", decl);
        const AssemblyVariant *v = &site->variant;
        if(site->statement < 0) c_function(c, assembly->module, site);
        else c_block(c, assembly->module, site);
        fprintf(c, "static int %sAvailable(void) {\n", site->name);
        EmitAssemblyAvailable(c, site->function, site->statement);
        fputs("}\n\n", c);
        fprintf(go, "var _ = func() bool { if C.%sAvailable() != 0 { %s = %sNative }; return true }()\nfunc %sNative(", site->name, site->name, site->name, site->name);
        go_signature(go, assembly->module, site, 1);
        fprintf(go, " {\n    result := C.%sC(", site->name);
        int block = site->statement >= 0;
        int first = block && !site->by_value ? 0 : v->outputs;
        for(int p = first; p < v->outputs + v->inputs; p++) {
            const char *type = v->operands[p].type;
            if(p > first) fputs(", ", go);
            if(block && (p < v->outputs || site->storage[p])) {
                char native[ZIR_NAME_MAX]; c_type(type, native, sizeof(native));
                fprintf(go, "(*C.%s)(unsafe.Pointer(a%d))", native, p);
            } else if(type[0] == '*') fprintf(go, "unsafe.Pointer(a%d)", p);
            else {
                char native[ZIR_NAME_MAX]; c_type(type, native, sizeof(native));
                fprintf(go, "C.%s(a%d)", native, p);
            }
        }
        fputs(")\n    return ", go);
        if(!block || site->by_value) for(int p = 0; p < v->outputs; p++) {
            go_conversion(go, assembly->module, v->operands[p].type, p);
            fputs(", ", go);
        }
        fputs("result.available != 0\n}\n\n", go);
    }
    int failed = ferror(c) || ferror(go) || ferror(decl);
    failed |= fclose(c) != 0; failed |= fclose(go) != 0; failed |= fclose(decl) != 0;
    if(failed) Diagnostic(assembly->module->span, "zir_go.body", "cannot finish assembly companions");
    return failed || write_direct(assembly, path, package);
}

void
GoAssemblyClose(GoAssembly *assembly)
{
    if(assembly) {
        for(int i = 0; i < assembly->count; i++) {
            GoAssemblySite *site = &assembly->sites[i];
            for(int j = 0; j < site->direct_count; j++) GoAmd64Close(site->direct[j]);
            free(site->direct);
        }
        free(assembly->sites); free(assembly);
    }
}

int
GoAssemblyAvailability(const char *directory, const char *package, ZirSourceSpan span)
{
    for(int enabled = 0; enabled <= 1; enabled++) {
        char path[ZIR_PATH_MAX];
        int n = snprintf(path, sizeof(path), "%s/zi2go.assembly_%s.go", directory,
                         enabled ? "enabled" : "disabled");
        if(n < 0 || (size_t)n >= sizeof(path)) {
            Diagnostic(span, "zir_go.body", "assembly availability path exceeds limit");
            return 1;
        }
        GeneratedOutputRecord(path);
        FILE *out = fopen(path, "wb");
        if(out == NULL) {
            Diagnostic(span, "zir_go.body", "cannot create assembly availability: %s", path);
            return 1;
        }
        fprintf(out, "// Code generated by zi2go from assembly availability. DO NOT EDIT.\n"
                     "//go:build %s\n\npackage %s\n\nconst zirGoAssemblyEnabled = %s\n",
                     enabled ? "ziran_asm && (cgo || amd64)" : "!ziran_asm || (!cgo && !amd64)", package,
                     enabled ? "true" : "false");
        int failed = ferror(out) != 0;
        failed |= fclose(out) != 0;
        if(failed) {
            Diagnostic(span, "zir_go.body", "cannot finish assembly availability: %s", path);
            return 1;
        }
    }
    return 0;
}
