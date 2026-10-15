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

const char *
AssemblyBlockText(const char *header, ZirSourceSpan span)
{
    const char *start = skip_ws(header), *at = start;
    int count = 0;
    while(!strncmp(at, "#asm", 4) && !is_ident_char(at[4])) {
        Assembly assembly;
        if(!read_assembly(&at, &assembly, span)) return NULL;
        count++;
        at = skip_ws(at);
    }
    if(!count || *at != '{' || *skip_ws(at + 1)) {
        assembly_error(span, "#asm block requires variants followed by a portable body");
        return NULL;
    }
    size_t length = (size_t)(at - start);
    while(length && isspace((unsigned char)start[length - 1])) length--;
    char text[ZIR_TEXT_MAX];
    if(length >= sizeof(text)) {
        assembly_error(span, "#asm block variants exceed size limit");
        return NULL;
    }
    memcpy(text, start, length); text[length] = 0;
    return KeepText(text);
}

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

typedef struct BlockBinding {
    const char *constraint, *alias, *name;
} BlockBinding;

typedef struct BlockBindings {
    int outputs, inputs;
    BlockBinding items[ASSEMBLY_OPERANDS];
} BlockBindings;

static int
assembly_identifier(const char *name)
{
    if(!isalpha((unsigned char)*name) && *name != '_') return 0;
    for(const char *at = name + 1; *at; at++)
        if(!isalnum((unsigned char)*at) && *at != '_') return 0;
    return strcmp(name, "_") != 0;
}

static int
block_bindings(const Assembly *assembly, BlockBindings *bindings, ZirSourceSpan span)
{
    const char *parts[ASSEMBLY_OPERANDS];
    int outputs = operand_list(assembly->part[2], parts);
    int inputs = operand_list(assembly->part[3], parts);
    if(outputs < 0 || inputs < 0 || outputs + inputs > ASSEMBLY_OPERANDS ||
       operand_list(assembly->part[4], parts) < 0)
        return assembly_error(span, "#asm block permits at most 30 operands separated by semicolons");
    bindings->outputs = outputs; bindings->inputs = inputs;
    for(int direction = 0; direction < 2; direction++) {
        int count = operand_list(assembly->part[direction + 2], parts);
        for(int i = 0; i < count; i++) {
            char text[ZIR_NAME_MAX];
            strcpy(text, parts[i]);
            char *separator = strchr(text, ':');
            if(separator == NULL)
                return assembly_error(span, "#asm block operands require constraint:alias=binding (or constraint:binding)");
            *separator++ = 0;
            char *constraint = trim_in_place(text);
            char *alias = trim_in_place(separator), *name = strchr(alias, '=');
            if(name) { *name++ = 0; name = trim_in_place(name); alias = trim_in_place(alias); }
            else name = alias;
            if(!*constraint || !assembly_identifier(alias) || !assembly_identifier(name) ||
               (!direction && (constraint[0] != '=' || !constraint[1])) ||
               (direction && (strchr(constraint, '=') || strchr(constraint, '+'))))
                return assembly_error(span, "invalid #asm block binding or constraint; use = outputs and tied inputs");
            int position = direction ? outputs + i : i;
            for(int previous = 0; previous < position; previous++)
                if(!strcmp(bindings->items[previous].alias, alias))
                    return assembly_error(span, "duplicate #asm block operand alias");
            bindings->items[position] = (BlockBinding){KeepText(constraint), KeepName(alias), KeepName(name)};
        }
    }
    return 1;
}

/* Resolve only real lexical storage. Closed scopes cannot contribute a
 * binding, and native helpers must use the same declaration as the checker. */
static const char *
block_binding_type(const ZirFunction *fn, int statement, const char *name)
{
    int closed = 0;
    for(int i = statement - 1; i >= 0; i--) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->kind == ZIR_STMT_BLOCK_CLOSE) closed++;
        else if(st->kind == ZIR_STMT_BLOCK_OPEN || st->kind == ZIR_STMT_IF ||
                st->kind == ZIR_STMT_WHILE || st->kind == ZIR_STMT_FOR) {
            if(closed) closed--;
        } else if(!closed && st->kind == ZIR_STMT_DECL && !strcmp(st->name, name))
            return st->type;
    }
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    for(int i = 0; i < parameters->count; i++)
        if(parameters->items[i].name && !strcmp(parameters->items[i].name, name))
            return parameters->items[i].type;
    return NULL;
}

int
CheckAssemblyBlock(const ZirFunction *fn, int statement,
                   ZirAssemblyBinding binding, void *context)
{
    const ZirStmt *st = &fn->stmts[statement];
    int spelled = !strncmp(st->text, "#asm", 4) && !is_ident_char(st->text[4]);
    if(!StatementHasAssembly(st))
        return !spelled || assembly_error(st->span, "missing #asm block metadata");
    if(st->kind != ZIR_STMT_BLOCK_OPEN || fn->is_template || fn->is_specialization)
        return assembly_error(st->span, "#asm blocks require an ordinary procedure scope");
    if(st->expr_root != -1 || st->lhs_root != -1 || *st->type ||
       *st->assignment_op || st->is_using || st->is_else || st->is_parallel || st->is_gpu)
        return assembly_error(st->span, "invalid #asm block statement metadata");
    const char *canonical = AssemblyBlockText(st->text, st->span);
    if(canonical == NULL || strcmp(canonical, st->assembly_text))
        return assembly_error(st->span, "#asm block metadata does not match its source");
    const char *cursor = st->assembly_text;
    int architectures_seen[ASSEMBLY_OPERANDS], variants = 0;
    uint64_t features_seen[ASSEMBLY_OPERANDS];
    BlockBindings first = {0};
    while(*skip_ws(cursor)) {
        Assembly assembly;
        BlockBindings current = {0};
        if(!read_assembly(&cursor, &assembly, st->span) ||
           !block_bindings(&assembly, &current, st->span)) return 0;
        if(variants == ASSEMBLY_OPERANDS)
            return assembly_error(st->span, "too many #asm block variants");
        for(int i = 0; i < variants; i++)
            if(architectures_seen[i] == assembly.architecture &&
               (features_seen[i] & assembly.features) == features_seen[i])
                return assembly_error(st->span, "duplicate or shadowed #asm block variant");
        architectures_seen[variants] = assembly.architecture;
        features_seen[variants] = assembly.features;
        if(!variants) first = current;
        if(first.inputs != current.inputs || first.outputs != current.outputs)
            return assembly_error(st->span, "#asm block variants must bind the same operands in the same order");
        for(int i = 0; i < current.inputs + current.outputs; i++) {
            const BlockBinding *operand = &current.items[i];
            if(strcmp(first.items[i].alias, operand->alias) || strcmp(first.items[i].name, operand->name))
                return assembly_error(st->span, "#asm block variants must bind the same operands in the same order");
            const char *type = binding ? binding(context, operand->name) :
                block_binding_type(fn, statement, operand->name);
            if(!operand_type(type))
                return assembly_error(st->span, "#asm block bindings must be scalar/pointer locals or parameters");
        }
        variants++;
    }
    /* Native assembly falls through. Its portable replacement must do so
     * too; control inside a loop wholly contained in the block is fine. */
    int depth = 1;
    for(int i = statement + 1; i < fn->stmt_count; i++) {
        const ZirStmt *body = &fn->stmts[i];
        if(body->kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0) break;
        if(body->kind == ZIR_STMT_BLOCK_OPEN || body->kind == ZIR_STMT_IF ||
           body->kind == ZIR_STMT_WHILE || body->kind == ZIR_STMT_FOR) depth++;
        if(body->kind == ZIR_STMT_RETURN || body->kind == ZIR_STMT_UNREACHABLE)
            return assembly_error(body->span, "#asm portable block must fall through");
        if(body->kind == ZIR_STMT_BREAK || body->kind == ZIR_STMT_CONTINUE) {
            int closed = 0, local_loop = 0;
            for(int j = i - 1; j > statement; j--) {
                const ZirStmt *scope = &fn->stmts[j];
                if(scope->kind == ZIR_STMT_BLOCK_CLOSE) closed++;
                else if(scope->kind == ZIR_STMT_BLOCK_OPEN || scope->kind == ZIR_STMT_IF ||
                        scope->kind == ZIR_STMT_WHILE || scope->kind == ZIR_STMT_FOR) {
                    if(closed) closed--;
                    else if(scope->kind == ZIR_STMT_WHILE &&
                            (!body->target_id || scope->loop_id == body->target_id)) { local_loop = 1; break; }
                }
            }
            if(!local_loop) return assembly_error(body->span, "#asm portable block cannot leave its scope");
        }
    }
    return variants > 0;
}

int
AssemblyBlockOperands(const ZirFunction *fn, int statement,
                       const char **names, const char **types, int capacity,
                       int *outputs)
{
    const ZirStmt *st = &fn->stmts[statement];
    if(outputs) *outputs = 0;
    if(!StatementHasAssembly(st)) return 0;
    const char *cursor = st->assembly_text;
    Assembly assembly;
    BlockBindings bindings = {0};
    if(!read_assembly(&cursor, &assembly, st->span) ||
       !block_bindings(&assembly, &bindings, st->span)) return -1;
    int count = bindings.inputs + bindings.outputs;
    if(count > capacity) return -1;
    if(outputs) *outputs = bindings.outputs;
    for(int i = 0; i < count; i++) {
        names[i] = bindings.items[i].name;
        types[i] = block_binding_type(fn, statement, names[i]);
    }
    return count;
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
    if(fn->is_template || fn->is_specialization)
        for(int i = 0; i < fn->stmt_count; i++)
            if(StatementHasAssembly(&fn->stmts[i]))
                return assembly_error(fn->stmts[i].span, "#asm blocks require an ordinary procedure scope");
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

static _Thread_local int portable_blocks;

void
EmitUsePortableAssembly(int enabled)
{
    portable_blocks = enabled;
}

static void
block_helper_name(const ZirModule *module, const ZirFunction *fn, ZirTarget target,
                  int statement, int variant, char *name, size_t size)
{
    int function = (int)(fn - module->functions);
    for(int serial = 0;; serial++) {
        snprintf(name, size, "ziran_assembly_block_%d_%d_%d_%d", function, statement, variant, serial);
        if(!assembly_name_used(module, target, name)) return;
    }
}

/* Register-only scalar outputs can use the native return register. Memory
 * constraints retain the caller's actual storage, including pointer aliases. */
static int
block_returns_register(const BlockBindings *bindings)
{
    return bindings->outputs == 1 &&
        (!strcmp(bindings->items[0].constraint, "=r") ||
         !strcmp(bindings->items[0].constraint, "=&r"));
}

static void
block_assembly_body(FILE *out, const ZirFunction *fn, ZirTarget target,
                    const Assembly *assembly, const BlockBindings *bindings, int helper)
{
    fputs("        __asm__", out);
    if(*assembly->part[5]) fputs(" __volatile__", out);
    fputc('(', out); assembly_string(out, assembly->part[1]);
    for(int direction = 0; direction < 2; direction++) {
        fputs("\n            : ", out);
        int count = direction ? bindings->inputs : bindings->outputs;
        for(int i = 0; i < count; i++) {
            int position = direction ? bindings->outputs + i : i;
            const BlockBinding *binding = &bindings->items[position];
            if(i) fputs(", ", out);
            fprintf(out, "[%s] ", binding->alias); assembly_string(out, binding->constraint);
            if(helper && !direction && block_returns_register(bindings)) fputs(" (ziran_result)", out);
            else if(helper) fprintf(out, " (%sziran_operand_%d)", direction ? "" : "*", position);
            else {
                char native[ZIR_NAME_MAX * 4];
                TargetBindingName(fn, target, binding->name, native, sizeof(native));
                fprintf(out, " (%s)", native);
            }
        }
    }
    const char *clobbers[ASSEMBLY_OPERANDS];
    int count = operand_list(assembly->part[4], clobbers);
    fputs("\n            : ", out);
    for(int i = 0; i < count; i++) {
        if(i) fputs(", ", out);
        assembly_string(out, clobbers[i]);
    }
    fputs(");\n", out);
}

static void
block_native_type(const ZirModule *module, const char *type, ZirTarget target,
                  char *out, size_t size, ZirSourceSpan span)
{
    int pointers = 0;
    while(type && *type == '*') { pointers++; type = skip_ws(type + 1); }
    const char *scalar = type ? TargetType(ScalarType(type), target) : NULL;
    if(scalar) snprintf(out, size, "%s", scalar);
    else {
        const ZirModule *owner = NULL;
        const ZirType *declared = type ? FindType(module, type, &owner) : NULL;
        if(declared == NULL) {
            assembly_error(span, "cannot lower #asm block binding type");
            exit(1);
        }
        NativeTypeName(owner, declared, out, size);
    }
    size_t used = strlen(out);
    if(used + (size_t)pointers + 1 > size) exit(1);
    while(pointers--) out[used++] = '*';
    out[used] = 0;
}

void
EmitAssemblyBlockHelpers(FILE *out, const ZirModule *module,
                         const ZirFunction *fn, ZirTarget target)
{
    for(int statement = 0; statement < fn->stmt_count; statement++) {
        const ZirStmt *st = &fn->stmts[statement];
        if(!StatementHasAssembly(st)) continue;
        if(!CheckAssemblyBlock(fn, statement, NULL, NULL)) exit(1);
        const char *cursor = st->assembly_text;
        int variant = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            BlockBindings bindings = {0};
            if(!read_assembly(&cursor, &assembly, st->span) ||
               !block_bindings(&assembly, &bindings, st->span)) exit(1);
            if(assembly.features) {
                if(assembly.architecture == ASM_AARCH64)
                    fputs("#if !defined(ZIRAN_NO_ASM) && defined(__linux__) && defined(__aarch64__)\n"
                          "#include <sys/auxv.h>\n#include <asm/hwcap.h>\n#endif\n", out);
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                char name[ZIR_NAME_MAX];
                block_helper_name(module, fn, target, statement, variant, name, sizeof(name));
                fputs("__attribute__((noinline, unused, target(\"", out);
                int first = 1;
                for(size_t i = 0; i < CPU_FEATURES; i++) {
                    if(!(assembly.features & (UINT64_C(1) << i))) continue;
                    if(!first) fputc(',', out);
                    fputs(assembly.architecture == ASM_AARCH64 ? cpu_features[i].aarch64 : cpu_features[i].x86, out);
                    first = 0;
                }
                char returned[ZIR_NAME_MAX * 4] = "void";
                int first_argument = block_returns_register(&bindings);
                if(first_argument)
                    block_native_type(module, block_binding_type(fn, statement, bindings.items[0].name),
                                      target, returned, sizeof(returned), st->span);
                fprintf(out, "\"))) static %s\n%s(", returned, name);
                for(int i = first_argument; i < bindings.outputs + bindings.inputs; i++) {
                    const char *type = block_binding_type(fn, statement, bindings.items[i].name);
                    char native[ZIR_NAME_MAX * 4];
                    block_native_type(module, type, target, native, sizeof(native), st->span);
                    fprintf(out, "%s%s%sziran_operand_%d", i > first_argument ? ", " : "", native,
                            i < bindings.outputs ? " *" : " ", i);
                }
                if(first_argument == bindings.outputs + bindings.inputs) fputs("void", out);
                fputs(")\n{\n", out);
                if(first_argument) fprintf(out, "    %s ziran_result;\n", returned);
                block_assembly_body(out, fn, target, &assembly, &bindings, 1);
                if(first_argument) fputs("    return ziran_result;\n", out);
                fputs("}\n#endif\n", out);
            }
            variant++;
        }
    }
}

int
EmitAssemblyBlock(FILE *out, const ZirModule *module, const ZirFunction *fn,
                  int statement, ZirTarget target)
{
    if((portable_blocks && target == ZIR_C) || (target != ZIR_C && target != ZIR_CPP)) return 0;
    const ZirStmt *st = &fn->stmts[statement];
    if(!StatementHasAssembly(st)) return 0;
    if(!CheckAssemblyBlock(fn, statement, NULL, NULL)) exit(1);
    const char *cursor = st->assembly_text;
    int features = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, st->span)) exit(1);
        features |= assembly.features != 0;
    }
    char done[ZIR_NAME_MAX], state[ZIR_NAME_MAX], selected[ZIR_NAME_MAX];
    ArrayAbiName(fn, 1000 + statement * 3, done, sizeof(done));
    ArrayAbiName(fn, 1001 + statement * 3, state, sizeof(state));
    ArrayAbiName(fn, 1002 + statement * 3, selected, sizeof(selected));
    fprintf(out, "    {\n        int %s = 0;\n", done);
    if(features) {
        cursor = st->assembly_text;
        int guards = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, st->span)) exit(1);
            fputs(guards++ ? " || (" : "#if (", out);
            assembly_guard(out, &assembly); fputc(')', out);
        }
        fprintf(out, "\n        static int %s;\n"
                     "        int %s = __atomic_load_n(&%s, __ATOMIC_ACQUIRE);\n"
                     "        if(%s == 0) {\n            %s = 1;\n",
                     state, selected, state, selected, selected);
        fputs("#if (defined(__x86_64__) || defined(__i386__)) && !defined(_MSC_VER)\n"
              "            __builtin_cpu_init();\n#endif\n", out);
        cursor = st->assembly_text;
        int variant = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, st->span)) exit(1);
            fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
            fprintf(out, "            if(%s == 1", selected);
            if(assembly.features) { fputs(" && (", out); assembly_predicate(out, &assembly); fputc(')', out); }
            fprintf(out, ") %s = %d;\n#endif\n", selected, variant + 2);
            variant++;
        }
        fprintf(out, "            __atomic_store_n(&%s, %s, __ATOMIC_RELEASE);\n        }\n#endif\n", state, selected);
    }
    cursor = st->assembly_text;
    int variant = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        BlockBindings bindings = {0};
        if(!read_assembly(&cursor, &assembly, st->span) ||
           !block_bindings(&assembly, &bindings, st->span)) exit(1);
        fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
        if(features) fprintf(out, "        if(%s == %d) {\n", selected, variant + 2);
        else fprintf(out, "        if(!%s) {\n", done);
        if(assembly.features) {
            char helper[ZIR_NAME_MAX];
            block_helper_name(module, fn, target, statement, variant, helper, sizeof(helper));
            int first_argument = block_returns_register(&bindings);
            fputs("            ", out);
            if(first_argument) {
                char name[ZIR_NAME_MAX * 4];
                TargetBindingName(fn, target, bindings.items[0].name, name, sizeof(name));
                fprintf(out, "%s = ", name);
            }
            fprintf(out, "%s(", helper);
            for(int i = first_argument; i < bindings.outputs + bindings.inputs; i++) {
                char name[ZIR_NAME_MAX * 4];
                TargetBindingName(fn, target, bindings.items[i].name, name, sizeof(name));
                fprintf(out, "%s%s%s", i > first_argument ? ", " : "", i < bindings.outputs ? "&" : "", name);
            }
            fputs(");\n", out);
        } else block_assembly_body(out, fn, target, &assembly, &bindings, 0);
        fprintf(out, "            %s = 1;\n        }\n#endif\n", done);
        variant++;
    }
    fprintf(out, "        if(!%s) {\n", done);
    return 1;
}
