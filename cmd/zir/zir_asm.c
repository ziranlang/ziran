#include "zir.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Six literal strings: architecture, template, outputs, inputs, clobbers,
 * options. Semicolons separate operands without stealing constraint commas.
 * Keep only one pointer on functions that have no assembly. */
typedef struct Assembly {
    const char *part[6];
    int architecture;
    uint64_t features;
} Assembly;

enum { ASM_X86_64, ASM_X86, ASM_AARCH64 };
static const struct { const char *name, *guard; } architectures[] = {
    {"x86_64", "defined(__x86_64__)"},
    {"x86", "defined(__i386__)"},
    {"aarch64", "defined(__aarch64__)"},
    {"arm", "defined(__arm__)"},
    {"riscv64", "defined(__riscv) && __riscv_xlen == 64"},
    {"riscv32", "defined(__riscv) && __riscv_xlen == 32"},
    {"powerpc64", "defined(__powerpc64__)"},
    {"powerpc", "defined(__powerpc__) && !defined(__powerpc64__)"},
    {"s390x", "defined(__s390x__)"},
    {"loongarch64", "defined(__loongarch64)"}
};

/* Runtime predicates and code-generation options are separate: an isolated
 * helper may use these instructions, while its caller keeps the build's ISA. */
static const struct {
    const char *name, *x86, *aarch64, *hwcap;
} cpu_features[] = {
    {"popcnt", "popcnt", NULL, NULL},
    {"sse2", "sse2", NULL, NULL},
    {"ssse3", "ssse3", NULL, NULL},
    {"sse4.1", "sse4.1", NULL, NULL},
    {"sse4.2", "sse4.2", NULL, NULL},
    {"avx", "avx", NULL, NULL},
    {"avx2", "avx2", NULL, NULL},
    {"avx512f", "avx512f", NULL, NULL},
    {"bmi", "bmi", NULL, NULL},
    {"bmi2", "bmi2", NULL, NULL},
    {"lzcnt", "lzcnt", NULL, NULL},
    {"fma", "fma", NULL, NULL},
    {"aes", "aes", "+aes", "HWCAP_AES"},
    {"pclmul", "pclmul", NULL, NULL},
    {"crc32", NULL, "+crc", "HWCAP_CRC32"},
    {"pmull", NULL, "+aes", "HWCAP_PMULL"},
    {"sha2", NULL, "+sha2", "HWCAP_SHA2"},
    {"atomics", NULL, "+lse", "HWCAP_ATOMICS"},
    {"simd", NULL, "+simd", "HWCAP_ASIMD"}
};
#define CPU_FEATURES (sizeof(cpu_features) / sizeof(*cpu_features))

static int
assembly_error(ZirSourceSpan span, const char *message)
{
    Diagnostic(span, "check.assembly", "%s", message);
    return 0;
}

static int
assembly_target(Assembly *assembly, ZirSourceSpan span)
{
    const char *name = assembly->part[0], *end = strchr(name, '+');
    size_t length = end ? (size_t)(end - name) : strlen(name);
    assembly->architecture = -1;
    assembly->features = 0;
    for(size_t i = 0; i < sizeof(architectures) / sizeof(*architectures); i++)
        if(strlen(architectures[i].name) == length && !memcmp(name, architectures[i].name, length))
            assembly->architecture = (int)i;
    if(assembly->architecture < 0) return assembly_error(span, "unknown #asm architecture");
    while(end) {
        name = end + 1;
        end = strchr(name, '+');
        length = end ? (size_t)(end - name) : strlen(name);
        int feature = -1;
        for(size_t i = 0; i < CPU_FEATURES; i++)
            if(strlen(cpu_features[i].name) == length && !memcmp(name, cpu_features[i].name, length) &&
               ((assembly->architecture <= ASM_X86 && cpu_features[i].x86) ||
                (assembly->architecture == ASM_AARCH64 && cpu_features[i].aarch64)))
                feature = (int)i;
        if(feature < 0) return assembly_error(span, "unknown or unsupported #asm CPU feature for architecture");
        uint64_t bit = UINT64_C(1) << feature;
        if(assembly->features & bit) return assembly_error(span, "duplicate #asm CPU feature");
        assembly->features |= bit;
    }
    return 1;
}

static int
read_assembly(const char **cursor, Assembly *assembly, ZirSourceSpan span)
{
    const char *at = skip_ws(*cursor);
    if(strncmp(at, "#asm", 4) || *skip_ws(at + 4) != '(')
        return assembly_error(span, "#asm requires six literal strings in parentheses");
    at = skip_ws(at + 4) + 1;
    for(int i = 0; i < 6; i++) {
        at = skip_ws(at);
        const char *start = at;
        if(*at++ != '"')
            return assembly_error(span, "#asm requires six literal strings in parentheses");
        while(*at && *at != '"') {
            if(*at == '\\' && at[1]) at++;
            at++;
        }
        if(*at != '"') return assembly_error(span, "unterminated #asm string");
        at++;
        size_t length = (size_t)(at - start), decoded_length = 0;
        char literal[ZIR_TEXT_MAX];
        unsigned char decoded[ZIR_TEXT_MAX];
        if(length >= sizeof(literal))
            return assembly_error(span, "#asm string exceeds size limit");
        memcpy(literal, start, length);
        literal[length] = 0;
        if(!DecodeStringLiteral(literal, decoded, sizeof(decoded) - 1, &decoded_length) ||
           memchr(decoded, 0, decoded_length) != NULL)
            return assembly_error(span, "invalid #asm string (embedded NUL is forbidden)");
        decoded[decoded_length] = 0;
        assembly->part[i] = KeepText((const char *)decoded);
        at = skip_ws(at);
        if(*at++ != (i == 5 ? ')' : ','))
            return assembly_error(span, "#asm requires exactly six literal strings");
    }
    if(!assembly_target(assembly, span)) return 0;
    if(*assembly->part[5] && strcmp(assembly->part[5], "volatile"))
        return assembly_error(span, "#asm options must be empty or volatile");
    *cursor = at;
    return 1;
}

const char *
ExtractAssemblyModifiers(char *header, ZirSourceSpan span)
{
    /* Ordinary procedures never need the annotation scanner. */
    if(strstr(header, "#asm") == NULL) return NULL;
    char kept[ZIR_TEXT_MAX];
    size_t used = 0;
    int parentheses = 0, parameters_complete = 0;
    /* Scan outside strings: #asm, braces, and quotes inside an instruction
     * template must never be interpreted by the ordinary source parser. */
    for(char *at = header; *at && !(*at == '{' && parentheses == 0);) {
        if(*at == '"' || *at == '\'') {
            char quote = *at++;
            while(*at && *at != quote) {
                if(*at == '\\' && at[1]) at++;
                at++;
            }
            if(*at) at++;
        } else if(!strncmp(at, "#asm", 4) && !is_ident_char(at[4])) {
            if(parentheses != 0 || !parameters_complete) {
                assembly_error(span, "#asm must follow the procedure signature");
                exit(1);
            }
            const char *end = at;
            Assembly assembly;
            if(!read_assembly(&end, &assembly, span)) exit(1);
            size_t length = (size_t)(end - at);
            if(used + length + 2 > sizeof(kept)) {
                assembly_error(span, "#asm variants exceed size limit");
                exit(1);
            }
            memcpy(kept + used, at, length);
            used += length;
            kept[used++] = ' ';
            memmove(at, end, strlen(end) + 1);
        } else {
            if(*at == '(') parentheses++;
            else if(*at == ')' && parentheses > 0 && --parentheses == 0)
                parameters_complete = 1;
            at++;
        }
    }
    if(used == 0) return NULL;
    kept[used] = 0;
    return KeepText(kept);
}

enum { ASSEMBLY_OPERANDS = 30 };

static int
operand_list(const char *text, const char **items)
{
    int count = 0;
    if(!*text) return 0;
    for(const char *at = text;;) {
        const char *end = strchr(at, ';');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        char item[ZIR_NAME_MAX];
        if(length >= sizeof(item) || count == ASSEMBLY_OPERANDS) return -1;
        memcpy(item, at, length);
        item[length] = 0;
        if(!*trim_in_place(item)) return -1;
        items[count++] = KeepText(item);
        if(!end) break;
        at = end + 1;
    }
    return count;
}

typedef struct AssemblyOutputs {
    char names[ASSEMBLY_OPERANDS][ZIR_NAME_MAX + 4];
    ZirTypeField fields[ASSEMBLY_OPERANDS];
    int count, record;
} AssemblyOutputs;

static int
operand_type(const char *type)
{
    if(!type || !*type) return 0;
    if(type[0] == '*') return 1;
    const char *scalar = ScalarType(type);
    return TargetType(scalar, ZIR_C) != NULL && strcmp(scalar, "void") &&
           strcmp(scalar, "string");
}

static int
assembly_outputs(const ZirModule *module, const ZirFunction *fn, AssemblyOutputs *out)
{
    if(!strcmp(fn->return_type, "void")) return 1;
    if(operand_type(fn->return_type)) {
        strcpy(out->names[0], "out");
        out->count = 1;
        return 1;
    }
    const ZirType *type = FindType(module, fn->return_type, NULL);
    if(!type || type->is_union || type->is_extern || type->is_procedure_type) return 0;
    size_t offset = 0;
    ZirTypeField field;
    int status;
    out->record = 1;
    while((status = TypeNextField(type, &offset, &field)) == 1) {
        if(out->count == ASSEMBLY_OPERANDS || !operand_type(field.type) ||
           field.is_using || !strcmp(field.name, "_")) return 0;
        out->fields[out->count] = field;
        snprintf(out->names[out->count++], sizeof(out->names[0]), "out_%s", field.name);
    }
    return status == 0 && out->count > 0;
}

int
CheckAssembly(const ZirModule *module, const ZirFunction *fn)
{
    if(!fn->assembly_text || !*fn->assembly_text) return 1;
    if(fn->is_extern || fn->is_template || fn->is_specialization ||
       fn->is_global_initializer)
        return assembly_error(fn->span, "#asm requires an ordinary procedure with a portable body");
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs) return assembly_error(fn->span, "out of memory checking #asm");
    int valid_outputs = assembly_outputs(module, fn, outputs);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    if(!valid_outputs || parameters->count + outputs->count > ASSEMBLY_OPERANDS) {
        free(outputs);
        return assembly_error(fn->span, "#asm needs scalar/pointer operands or a record of scalar/pointer results (at most 30 operands)");
    }
    for(int i = 0; i < parameters->count; i++) {
        if(!parameters->items[i].name || !operand_type(parameters->items[i].type)) {
            free(outputs);
            return assembly_error(fn->span, "#asm parameters must be named scalars or pointers");
        }
        for(int j = 0; j < outputs->count; j++)
            if(!strcmp(parameters->items[i].name, outputs->names[j])) {
                free(outputs);
                return assembly_error(fn->span, "#asm parameter conflicts with an output operand name");
            }
    }
    int architecture[ASSEMBLY_OPERANDS];
    uint64_t features[ASSEMBLY_OPERANDS];
    int variants = 0, ok = 1;
    const char *cursor = fn->assembly_text;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) { ok = 0; break; }
        for(int i = 0; i < variants; i++) {
            if(architecture[i] != assembly.architecture) continue;
            if(features[i] == assembly.features) ok = 0;
            else if((features[i] & assembly.features) == features[i]) {
                free(outputs);
                return assembly_error(fn->span, "#asm variant is shadowed by an earlier feature or baseline variant");
            }
        }
        if(!ok || variants == ASSEMBLY_OPERANDS) {
            ok = assembly_error(fn->span, "duplicate or too many #asm architecture variants");
            break;
        }
        architecture[variants] = assembly.architecture;
        features[variants++] = assembly.features;
        const char *out[ASSEMBLY_OPERANDS], *in[ASSEMBLY_OPERANDS], *clobber[ASSEMBLY_OPERANDS];
        int no = operand_list(assembly.part[2], out);
        int ni = operand_list(assembly.part[3], in);
        int nc = operand_list(assembly.part[4], clobber);
        if(no != outputs->count || ni != parameters->count || nc < 0) {
            ok = assembly_error(fn->span, "#asm constraint counts must match results and parameters; separate items with semicolons");
            break;
        }
        for(int i = 0; i < no; i++)
            if(out[i][0] != '=' || !out[i][1]) ok = 0;
        for(int i = 0; i < ni; i++)
            if(strchr(in[i], '=') || strchr(in[i], '+')) ok = 0;
        if(!ok) {
            assembly_error(fn->span, "#asm outputs need = constraints; use tied inputs for read/write operands");
            break;
        }
    }
    free(outputs);
    return ok && variants > 0;
}

/* Quote decoded bytes, never interpolate them as host-language source. */
static void
assembly_string(FILE *out, const char *text)
{
    fputc('"', out);
    for(const unsigned char *at = (const unsigned char *)text; *at; at++) {
        if(*at == '"' || *at == '\\') fprintf(out, "\\%c", *at);
        else if(*at < 32 || *at >= 127) fprintf(out, "\\%03o", *at);
        else fputc(*at, out);
    }
    fputc('"', out);
}

/* Both helper definitions and their calls use exactly the same guards. */
static void
assembly_guard(FILE *out, const Assembly *assembly)
{
    fprintf(out, "!defined(ZIRAN_NO_ASM) && (defined(__GNUC__) || defined(__clang__)) && (%s)",
            architectures[assembly->architecture].guard);
    if(assembly->features && assembly->architecture <= ASM_X86)
        fputs(" && !defined(_MSC_VER)", out);
    if(assembly->features && assembly->architecture == ASM_AARCH64) {
        fputs(" && defined(__linux__)", out);
        for(size_t i = 0; i < CPU_FEATURES; i++)
            if(assembly->features & (UINT64_C(1) << i))
                fprintf(out, " && defined(%s)", cpu_features[i].hwcap);
    }
}

static int
assembly_name_used(const ZirModule *module, ZirTarget target, const char *name)
{
    const ZirModule **modules = NULL;
    size_t count = 0, capacity = 0;
    int used = 0;
    for(size_t next = 0;; next++) {
        if(next == 0 || next < count) {
            const ZirModule *current = next == 0 ? module : modules[next];
            if(next == 0) {
                capacity = 16;
                modules = AllocateOrExit(capacity * sizeof(*modules));
                modules[count++] = current;
            }
            char native[ZIR_NAME_MAX * 4];
            for(int i = 0; i < current->function_count && !used; i++) {
                NativeCFunctionName(current, &current->functions[i], native, sizeof(native));
                used = !strcmp(native, name) || !strcmp(current->functions[i].name, name);
            }
            for(int i = 0; i < current->global_count && !used; i++) {
                TargetGlobalName(current, target, current->globals[i].name, native, sizeof(native));
                used = !strcmp(native, name);
            }
            for(int i = 0; i < current->define_count && !used; i++) {
                TargetDefineName(current, target, current->defines[i].name, native, sizeof(native));
                used = !strcmp(native, name);
            }
            for(int i = 0; i < current->type_count && !used; i++) {
                const ZirType *type = &current->types[i];
                NativeTypeName(current, type, native, sizeof(native));
                used = !strcmp(native, name);
                size_t length = strlen(native);
                if(type->is_enum && !strncmp(name, native, length) && name[length] == '_') {
                    const char *member = name + length + 1;
                    for(const char *at = strstr(type->body, member); at; at = strstr(at + 1, member)) {
                        if((at == type->body || !is_ident_char(at[-1])) &&
                           !is_ident_char(at[strlen(member)])) { used = 1; break; }
                    }
                }
            }
            for(int i = 0; i < current->import_count && !used; i++) {
                const ZirImport *import = &current->imports[i];
                if(import->kind == ZIR_IMPORT_EXTERN) {
                    NativeCForeignName(current, import, native, sizeof(native));
                    used = !strcmp(native, name) || !strcmp(import->extern_symbol, name);
                }
                const ZirModule *dependency = import->resolved_module;
                if(!dependency) continue;
                int seen = 0;
                for(size_t m = 0; m < count; m++) seen |= modules[m] == dependency;
                if(seen) continue;
                if(count == capacity) {
                    capacity *= 2;
                    const ZirModule **grown = realloc(modules, capacity * sizeof(*modules));
                    if(!grown) { free(modules); exit(1); }
                    modules = grown;
                }
                modules[count++] = dependency;
            }
            if(used) break;
        } else break;
    }
    free(modules);
    return used;
}

static void
assembly_helper_name(const ZirModule *module, const ZirFunction *fn, ZirTarget target,
                     int ordinal, char *out, size_t size)
{
    int function = (int)(fn - module->functions);
    for(int serial = 0;; serial++) {
        snprintf(out, size, "ziran_assembly_%d_%d_%d", function, ordinal, serial);
        if(!assembly_name_used(module, target, out)) return;
    }
}

static void
assembly_body(FILE *out, const ZirModule *module, const ZirFunction *fn, ZirTarget target,
              const char *result_type, const Assembly *assembly, const AssemblyOutputs *outputs)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    char result_name[ZIR_NAME_MAX];
    ArrayAbiName(fn, -1, result_name, sizeof(result_name));
    if(outputs->count) fprintf(out, "        %s %s;\n", result_type, result_name);
    fputs("        __asm__", out);
    if(*assembly->part[5]) fputs(" __volatile__", out);
    fputc('(', out);
    assembly_string(out, assembly->part[1]);
    const char *constraints[ASSEMBLY_OPERANDS];
    operand_list(assembly->part[2], constraints);
    fputs("\n            : ", out);
    for(int i = 0; i < outputs->count; i++) {
        if(i) fputs(", ", out);
        fprintf(out, "[%s] ", outputs->names[i]);
        assembly_string(out, constraints[i]);
        fprintf(out, " (%s", result_name);
        if(outputs->record) {
            const ZirType *record = FindType(module, fn->return_type, NULL);
            char field[ZIR_NAME_MAX * 4];
            TargetFieldName(record, target, outputs->fields[i].name, field, sizeof(field));
            fprintf(out, ".%s", field);
        }
        fputc(')', out);
    }
    operand_list(assembly->part[3], constraints);
    fputs("\n            : ", out);
    for(int i = 0; i < parameters->count; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
        if(i) fputs(", ", out);
        fprintf(out, "[%s] ", parameters->items[i].name);
        assembly_string(out, constraints[i]);
        fprintf(out, " (%s)", name);
    }
    int nc = operand_list(assembly->part[4], constraints);
    fputs("\n            : ", out);
    for(int i = 0; i < nc; i++) {
        if(i) fputs(", ", out);
        assembly_string(out, constraints[i]);
    }
    fputs(");\n        return", out);
    if(outputs->count) fprintf(out, " %s", result_name);
    else if(NativeMainReturnsStatus(fn)) fputs(" 0", out);
    fputs(";\n", out);
}

void
EmitAssemblyHelpers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                    ZirTarget target, const char *result_type, const char *args)
{
    if(!fn->assembly_text || !*fn->assembly_text) return;
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs || !CheckAssembly(module, fn) || !assembly_outputs(module, fn, outputs)) exit(1);
    const char *cursor = fn->assembly_text;
    int ordinal = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        if(assembly.features) {
            if(assembly.architecture == ASM_AARCH64)
                fputs("#if !defined(ZIRAN_NO_ASM) && defined(__linux__) && defined(__aarch64__)\n"
                      "#include <sys/auxv.h>\n#include <asm/hwcap.h>\n#endif\n", out);
            fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
            char helper[ZIR_NAME_MAX];
            assembly_helper_name(module, fn, target, ordinal, helper, sizeof(helper));
            fputs("__attribute__((noinline, unused, target(\"", out);
            int first = 1;
            for(size_t i = 0; i < CPU_FEATURES; i++) {
                if(!(assembly.features & (UINT64_C(1) << i))) continue;
                if(!first) fputc(',', out);
                fputs(assembly.architecture == ASM_AARCH64 ? cpu_features[i].aarch64 : cpu_features[i].x86, out);
                first = 0;
            }
            fprintf(out, "\"))) static %s\n%s(%s)\n{\n", result_type, helper, args);
            assembly_body(out, module, fn, target, result_type, &assembly, outputs);
            fputs("}\n#endif\n", out);
        }
        ordinal++;
    }
    free(outputs);
}

static void
assembly_predicate(FILE *out, const Assembly *assembly)
{
    int first = 1;
    for(size_t i = 0; i < CPU_FEATURES; i++) {
        if(!(assembly->features & (UINT64_C(1) << i))) continue;
        if(!first) fputs(" && ", out);
        if(assembly->architecture == ASM_AARCH64)
            fprintf(out, "(getauxval(AT_HWCAP) & %s) != 0", cpu_features[i].hwcap);
        else fprintf(out, "__builtin_cpu_supports(\"%s\")", cpu_features[i].name);
        first = 0;
    }
}

void
EmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *fn, ZirTarget target,
             const char *result_type)
{
    if(!fn->assembly_text || !*fn->assembly_text) return;
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs || !CheckAssembly(module, fn) || !assembly_outputs(module, fn, outputs)) exit(1);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const char *cursor = fn->assembly_text;
    int features = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        if(assembly.features) {
            if(!features++) fputs("#if ", out);
            else fputs(" || (", out);
            assembly_guard(out, &assembly);
            if(features > 1) fputc(')', out);
        }
    }
    if(features) {
        char state[ZIR_NAME_MAX], selected[ZIR_NAME_MAX];
        ArrayAbiName(fn, -1, state, sizeof(state));
        ArrayAbiName(fn, 64, selected, sizeof(selected));
        fprintf(out, "\n    {\n        static int %s;\n"
                     "        int %s = __atomic_load_n(&%s, __ATOMIC_ACQUIRE);\n"
                     "        if(%s == 0) {\n            %s = 1;\n",
                     state, selected, state, selected, selected);
        fputs("#if defined(__x86_64__) || defined(__i386__)\n            __builtin_cpu_init();\n#endif\n", out);
        cursor = fn->assembly_text;
        int ordinal = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
            if(assembly.features) {
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                fprintf(out, "            if(%s == 1 && (", selected);
                assembly_predicate(out, &assembly);
                fprintf(out, ")) %s = %d;\n#endif\n", selected, ordinal + 2);
            }
            ordinal++;
        }
        fprintf(out, "            __atomic_store_n(&%s, %s, __ATOMIC_RELEASE);\n        }\n", state, selected);
        cursor = fn->assembly_text;
        ordinal = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
            if(assembly.features) {
                char helper[ZIR_NAME_MAX];
                assembly_helper_name(module, fn, target, ordinal, helper, sizeof(helper));
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                fprintf(out, "        if(%s == %d) { ", selected, ordinal + 2);
                if(outputs->count || NativeMainReturnsStatus(fn)) fputs("return ", out);
                fprintf(out, "%s(", helper);
                for(int i = 0; i < parameters->count; i++) {
                    char name[ZIR_NAME_MAX * 4];
                    TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
                    fprintf(out, "%s%s", i ? ", " : "", name);
                }
                fprintf(out, ");%s }\n#endif\n", outputs->count || NativeMainReturnsStatus(fn) ? "" : " return;");
            }
            ordinal++;
        }
        fputs("    }\n#endif\n", out);
    }
    cursor = fn->assembly_text;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        if(assembly.features) continue;
        fputs("#if ", out); assembly_guard(out, &assembly); fputs("\n    {\n", out);
        assembly_body(out, module, fn, target, result_type, &assembly, outputs);
        fputs("    }\n#endif\n", out);
    }
    free(outputs);
}
