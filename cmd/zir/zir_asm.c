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
} Assembly;

static int
assembly_error(ZirSourceSpan span, const char *message)
{
    Diagnostic(span, "check.assembly", "%s", message);
    return 0;
}

static const char *
architecture_guard(const char *name)
{
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
    for(size_t i = 0; i < sizeof(architectures) / sizeof(*architectures); i++)
        if(!strcmp(name, architectures[i].name)) return architectures[i].guard;
    return NULL;
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
    if(architecture_guard(assembly->part[0]) == NULL)
        return assembly_error(span, "unknown #asm architecture");
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
    const char *architectures[ASSEMBLY_OPERANDS];
    int variants = 0, ok = 1;
    const char *cursor = fn->assembly_text;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) { ok = 0; break; }
        for(int i = 0; i < variants; i++)
            if(!strcmp(architectures[i], assembly.part[0])) ok = 0;
        if(!ok || variants == ASSEMBLY_OPERANDS) {
            ok = assembly_error(fn->span, "duplicate or too many #asm architecture variants");
            break;
        }
        architectures[variants++] = assembly.part[0];
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

int
EmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *fn, ZirTarget target,
             const char *result_type)
{
    if(!fn->assembly_text || !*fn->assembly_text) return 0;
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs || !CheckAssembly(module, fn) || !assembly_outputs(module, fn, outputs)) {
        free(outputs);
        exit(1);
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    char result_name[ZIR_NAME_MAX];
    ArrayAbiName(fn, -1, result_name, sizeof(result_name));
    const char *cursor = fn->assembly_text;
    int variants = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        fprintf(out, "%s !defined(ZIRAN_NO_ASM) && (defined(__GNUC__) || defined(__clang__)) && (%s)\n",
                variants++ ? "#elif" : "#if", architecture_guard(assembly.part[0]));
        fputs("    {\n", out);
        if(outputs->count)
            fprintf(out, "        %s %s;\n", result_type, result_name);
        fputs("        __asm__", out);
        if(*assembly.part[5]) fputs(" __volatile__", out);
        fputc('(', out);
        assembly_string(out, assembly.part[1]);
        const char *constraints[ASSEMBLY_OPERANDS];
        operand_list(assembly.part[2], constraints);
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
        operand_list(assembly.part[3], constraints);
        fputs("\n            : ", out);
        for(int i = 0; i < parameters->count; i++) {
            char name[ZIR_NAME_MAX * 4];
            TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
            if(i) fputs(", ", out);
            fprintf(out, "[%s] ", parameters->items[i].name);
            assembly_string(out, constraints[i]);
            fprintf(out, " (%s)", name);
        }
        int nc = operand_list(assembly.part[4], constraints);
        fputs("\n            : ", out);
        for(int i = 0; i < nc; i++) {
            if(i) fputs(", ", out);
            assembly_string(out, constraints[i]);
        }
        fputs(");\n        return", out);
        if(outputs->count) fprintf(out, " %s", result_name);
        fputs(";\n    }\n", out);
    }
    free(outputs);
    fputs("#else\n", out);
    return variants != 0;
}
