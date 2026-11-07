#include "zir_memory.h"
#include "zir.h"
#include "zir_asm.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_parse.h"
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
    int tinycc; /* TinyCC's assembler and constraints can build it */
} Assembly;

static void assembly_string(FILE *out, const char *text);
static int operand_list(const char *text, const char **items);
static int tinycc_variant(const Assembly *assembly);
static void assembly_predicate(FILE *out, const Assembly *assembly);

enum { ASM_X86_64, ASM_X86, ASM_AARCH64, ASM_ARM, ASM_WASM32 };
static const struct { const char *name, *guard; } architectures[] = {
    {"x86_64", "defined(__x86_64__)"},
    {"x86", "defined(__i386__)"},
    {"aarch64", "defined(__aarch64__)"},
    {"arm", "defined(__arm__)"},
    {"wasm32", "defined(__wasm32__)"},
    {"riscv64", "defined(__riscv) && __riscv_xlen == 64"},
    {"riscv32", "defined(__riscv) && __riscv_xlen == 32"},
    {"powerpc64", "defined(__powerpc64__)"},
    {"powerpc", "defined(__powerpc__) && !defined(__powerpc64__)"},
    {"s390x", "defined(__s390x__)"},
    {"loongarch64", "defined(__loongarch64)"}
};

/* Runtime predicates and code-generation options are separate: an isolated
 * helper may use these instructions, while its caller keeps the build's ISA.
 * Each architecture column is the helper's target option, or NULL where the
 * feature does not exist. hwcap is the Linux AT_HWCAP bit on AArch64/ARMv7.
 * WebAssembly features are build options instead; see runtime_selected.
 * tier is the lowest ZIRAN_SIMD_TIER (1 SSE2 .. 4 AVX2, as for #target
 * clones) a CPU with an x86 vector feature reaches, 5 beyond AVX2; a forced
 * lower tier hides the feature. 0 marks features outside the tiers. */
static const struct {
    const char *name, *x86, *aarch64, *hwcap, *arm, *wasm32;
    int tier;
} cpu_features[] = {
    {"popcnt", "popcnt", NULL, NULL, NULL, NULL, 0},
    {"sse2", "sse2", NULL, NULL, NULL, NULL, 0},
    {"ssse3", "ssse3", NULL, NULL, NULL, NULL, 2},
    {"sse4.1", "sse4.1", NULL, NULL, NULL, NULL, 3},
    {"sse4.2", "sse4.2", NULL, NULL, NULL, NULL, 4},
    {"avx", "avx", NULL, NULL, NULL, NULL, 4},
    {"avx2", "avx2", NULL, NULL, NULL, NULL, 4},
    {"avx512f", "avx512f", NULL, NULL, NULL, NULL, 5},
    {"bmi", "bmi", NULL, NULL, NULL, NULL, 0},
    {"bmi2", "bmi2", NULL, NULL, NULL, NULL, 0},
    {"lzcnt", "lzcnt", NULL, NULL, NULL, NULL, 0},
    {"fma", "fma", NULL, NULL, NULL, NULL, 4},
    {"aes", "aes", "+aes", "HWCAP_AES", NULL, NULL, 0},
    {"pclmul", "pclmul", NULL, NULL, NULL, NULL, 0},
    {"crc32", NULL, "+crc", "HWCAP_CRC32", NULL, NULL, 0},
    {"pmull", NULL, "+aes", "HWCAP_PMULL", NULL, NULL, 0},
    {"sha2", NULL, "+sha2", "HWCAP_SHA2", NULL, NULL, 0},
    {"atomics", NULL, "+lse", "HWCAP_ATOMICS", NULL, NULL, 0},
    {"simd", NULL, "+simd", "HWCAP_ASIMD", NULL, NULL, 0},
    {"neon", NULL, NULL, "HWCAP_NEON", "neon", NULL, 0},
    {"simd128", NULL, NULL, NULL, NULL, "simd128", 0}
};
#define CPU_FEATURES (sizeof(cpu_features) / sizeof(*cpu_features))

static const char *
feature_option(int architecture, size_t feature)
{
    switch(architecture) {
    case ASM_X86_64: case ASM_X86: return cpu_features[feature].x86;
    case ASM_AARCH64: return cpu_features[feature].aarch64;
    case ASM_ARM: return cpu_features[feature].arm;
    case ASM_WASM32: return cpu_features[feature].wasm32;
    default: return NULL;
    }
}

/* An engine validates a WebAssembly module whole, so a module holding SIMD
 * instructions cannot load where SIMD is missing. Its feature variants are
 * therefore compiled in only when the build enables the feature, inline and
 * without a helper or a cached runtime check. */
static int
runtime_selected(const Assembly *assembly)
{
    return assembly->features && assembly->architecture != ASM_WASM32;
}

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
               feature_option(assembly->architecture, i))
                feature = (int)i;
        if(feature < 0) return assembly_error(span, "unknown or unsupported #asm CPU feature for architecture");
        uint64_t bit = UINT64_C(1) << feature;
        if(assembly->features & bit) return assembly_error(span, "duplicate #asm CPU feature");
        assembly->features |= bit;
    }
    return 1;
}

/* One #asm argument at *CURSOR, which moves past it: a literal string,
 * quotes included, from *START for *LENGTH bytes (1), or the name of a
 * constant string, `NAME` or `Module.NAME` (2). 0 for anything else and -1
 * for a string without its closing quote. */
static int
assembly_argument(const char **cursor, const char **start, size_t *length)
{
    const char *at = skip_ws(*cursor);
    *start = at;
    if(*at == '"') {
        for(at++; *at && *at != '"'; at++)
            if(*at == '\\' && at[1]) at++;
        if(*at != '"') return -1;
        at++;
    } else {
        for(;;) {
            if(!isalpha((unsigned char)*at) && *at != '_') return 0;
            while(is_ident_char(*at)) at++;
            if(*at != '.') break;
            at++;
        }
    }
    *length = (size_t)(at - *start);
    *cursor = at;
    return **start == '"' ? 1 : 2;
}

/* Four to six arguments: clobbers and options may be left out. A template
 * can be a raw #string body of any length; the source reader keeps a long
 * one aside behind a marker, which KeepText expands. With NAMES, as the
 * source is read, the template, operands and clobbers may name constant
 * strings; they stay empty here until ResolveAssemblyConstants puts each
 * constant's literal in place of its name. */
static int
assembly_variant(const char **cursor, Assembly *assembly, ZirSourceSpan span,
                 int names)
{
    const char *at = skip_ws(*cursor);
    if(strncmp(at, "#asm", 4) || *skip_ws(at + 4) != '(')
        return assembly_error(span, "#asm requires four to six literal strings in parentheses");
    at = skip_ws(at + 4) + 1;
    int named = 0;
    for(int i = 0; i < 6; i++) assembly->part[i] = "";
    for(int i = 0; i < 6; i++) {
        const char *start;
        size_t length, decoded_length = 0;
        int kind = assembly_argument(&at, &start, &length);
        if(kind < 0) return assembly_error(span, "unterminated #asm string");
        if(kind == 2 && names && i >= 1 && i <= 4) named = 1;
        else if(kind == 2 && !names && i >= 1 && i <= 4)
            return assembly_error(span, "#asm constant name was not resolved");
        else if(kind != 1)
            return assembly_error(span, i >= 1 && i <= 4 ?
                "#asm template, operands and clobbers must be literal strings or constant string names" :
                "#asm architecture and options must be literal strings");
        else {
            char *literal = malloc(length + 1);
            unsigned char *decoded = malloc(length + 1);
            if(literal == NULL || decoded == NULL) {
                free(literal); free(decoded);
                return assembly_error(span, "out of memory reading #asm");
            }
            memcpy(literal, start, length);
            literal[length] = 0;
            char *expanded = strchr(literal, '\x1f') ? RawTextExpand(literal) : NULL;
            int valid;
            if(expanded) {
                /* A marker stands for the body of a long literal, quotes and
                 * escapes included; decode that body like any other. */
                free(decoded);
                decoded = malloc(strlen(expanded) + 1);
                valid = decoded != NULL &&
                    DecodeStringLiteral(expanded, decoded, strlen(expanded), &decoded_length);
                free(expanded);
            } else valid = DecodeStringLiteral(literal, decoded, length, &decoded_length);
            free(literal);
            if(!valid || memchr(decoded, 0, decoded_length) != NULL) {
                free(decoded);
                return assembly_error(span, "invalid #asm string (embedded NUL is forbidden)");
            }
            decoded[decoded_length] = 0;
            assembly->part[i] = KeepText((const char *)decoded);
            free(decoded);
        }
        at = skip_ws(at);
        char next = *at++;
        if(next == ')' && i >= 3) break;
        if(next != (i == 5 ? ')' : ','))
            return assembly_error(span, "#asm requires four to six literal strings");
    }
    if(!assembly_target(assembly, span)) return 0;
    if(*assembly->part[5] && strcmp(assembly->part[5], "volatile"))
        return assembly_error(span, "#asm options must be empty or volatile");
    assembly->tinycc = !named && tinycc_variant(assembly);
    *cursor = at;
    return 1;
}

/* A variant after ResolveAssemblyConstants: every argument is a literal. */
static int
read_assembly(const char **cursor, Assembly *assembly, ZirSourceSpan span)
{
    return assembly_variant(cursor, assembly, span, 0);
}

typedef struct AssemblyText {
    char *data;
    size_t used, capacity;
} AssemblyText;

static void
assembly_text_add(AssemblyText *text, const char *bytes, size_t length)
{
    if(text->used + length + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 256;
        while(text->used + length + 1 > capacity) capacity *= 2;
        char *data = realloc(text->data, capacity);
        if(data == NULL) {
            fputs("out of memory resolving #asm constants\n", stderr);
            exit(1);
        }
        text->data = data;
        text->capacity = capacity;
    }
    memcpy(text->data + text->used, bytes, length);
    text->used += length;
    text->data[text->used] = 0;
}

/* TEXT, a run of #asm variants as the source reader kept them, with each
 * constant name replaced by the literal of the string it names in MODULE.
 * TEXT itself when it names none; NULL after a diagnostic. */
static const char *
resolve_assembly_text(const ZirModule *module, const char *text, ZirSourceSpan span)
{
    if(strstr(text, "#asm") == NULL) return text;
    AssemblyText out = {0};
    const char *at = text, *copied = text;
    int changed = 0;
    while(*(at = skip_ws(at))) {
        if(strncmp(at, "#asm", 4) || *skip_ws(at + 4) != '(') break;
        at = skip_ws(at + 4) + 1;
        for(int i = 0; i < 6; i++) {
            const char *start;
            size_t length;
            if(assembly_argument(&at, &start, &length) == 2) {
                char name[ZIR_NAME_MAX];
                const char *literal = NULL;
                if(length >= sizeof(name)) {
                    free(out.data);
                    assembly_error(span, "#asm constant name is too long");
                    return NULL;
                }
                memcpy(name, start, length);
                name[length] = 0;
                if(!EvaluateCompileLiteral(module, name, span, 0, &literal, NULL) ||
                   literal == NULL || literal[0] != '"') {
                    free(out.data);
                    Diagnostic(span, "check.assembly",
                               "#asm argument %s is not a constant string", name);
                    return NULL;
                }
                assembly_text_add(&out, copied, (size_t)(start - copied));
                assembly_text_add(&out, literal, strlen(literal));
                copied = at;
                changed = 1;
            }
            at = skip_ws(at);
            if(*at != ',') break;
            at++;
        }
        if(*at == ')') at++;
    }
    if(!changed) return text;
    assembly_text_add(&out, copied, strlen(copied));
    const char *resolved = KeepText(out.data);
    free(out.data);
    return resolved;
}

/* Whether RESOLVED is SOURCE, variants as the source reader kept them, with
 * each constant name argument in SOURCE replaced by a literal string. */
static int
assembly_text_matches(const char *source, const char *resolved)
{
    for(;;) {
        const char *a, *b;
        size_t na, nb;
        source = skip_ws(source);
        resolved = skip_ws(resolved);
        int ka = assembly_argument(&source, &a, &na);
        if(ka == 0) {
            if(*source != *resolved) return 0;
            if(!*source) return 1;
            source++;
            resolved++;
            continue;
        }
        int kb = assembly_argument(&resolved, &b, &nb);
        if(ka < 0 || kb <= 0) return 0;
        if(ka == 2 && kb == 1) continue;
        if(ka != kb || na != nb || memcmp(a, b, na)) return 0;
    }
}

int
ResolveAssemblyConstants(ZirModule *module)
{
    for(int f = 0; f < module->function_count; f++) {
        ZirFunction *fn = &module->functions[f];
        if(fn->assembly_text && *fn->assembly_text) {
            const char *text = resolve_assembly_text(module, fn->assembly_text, fn->span);
            if(text == NULL) return 0;
            fn->assembly_text = text;
        }
        for(int i = 0; i < fn->stmt_count; i++) {
            ZirStmt *st = &fn->stmts[i];
            if(!StatementHasAssembly(st)) continue;
            const char *text = resolve_assembly_text(module, st->assembly_text, st->span);
            if(text == NULL) return 0;
            /* A named argument binds saved metadata to the constant's value,
             * just as a literal binds it to its source spelling. */
            const char *source = AssemblyBlockText(st->text, st->span);
            if(source == NULL ||
               (source = resolve_assembly_text(module, source, st->span)) == NULL)
                return 0;
            if(!assembly_text_matches(source, text))
                return assembly_error(st->span, "#asm block metadata does not match its source");
            st->assembly_text = text;
        }
    }
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
            if(!assembly_variant(&end, &assembly, span, 1)) exit(1);
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


const char *
AssemblyBlockText(const char *header, ZirSourceSpan span)
{
    const char *start = skip_ws(header), *at = start;
    int count = 0;
    while(!strncmp(at, "#asm", 4) && !is_ident_char(at[4])) {
        Assembly assembly;
        if(!assembly_variant(&at, &assembly, span, 1)) return NULL;
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

/* A slice parameter reaches assembly as its data pointer. */
static int
slice_operand(const char *type)
{
    char element[ZIR_NAME_MAX];
    return type && SliceElementType(type, element, sizeof(element)) && operand_type(element);
}

/* A declaration can bind a scalar or a record. Resolve that choice at each
 * specialization; known aggregates still cannot be input/block operands. */
static int
template_operand_type(const ZirFunction *fn, const char *type)
{
    if(operand_type(type)) return 1;
    if(!fn->is_template || type == NULL) return 0;
    if(*type == '$') type++;
    return TemplateParameterIndex(fn->template_param, type, strlen(type)) >= 0;
}

static int
template_result_type(const ZirFunction *fn)
{
    if(!fn->is_template) return 0;
    for(const char *at = fn->return_type; *at;) {
        if(!is_ident_char(*at)) { at++; continue; }
        const char *start = at++;
        while(is_ident_char(*at) ||
              (*at == '.' && (isalpha((unsigned char)at[1]) || at[1] == '_'))) at++;
        if(TemplateParameterIndex(fn->template_param, start, (size_t)(at - start)) >= 0)
            return 1;
    }
    return 0;
}

/* Registers, as families a clobber covers: an x86 vector register whatever
 * its width (xmm3, ymm3, zmm3), an AArch64 one (v3, q3, d3, s3, h3, b3), a
 * general register in any width, and an ARMv7 q register as its d halves.
 * Slots: x86 rax rbx rcx rdx rsi rdi rbp rsp r8..r15, vectors 16..47, k
 * 48..55, mm 56..63, st 64; AArch64 x0..x30, sp 31, vectors 32..63; ARMv7
 * r0..r15, d16..47 for d0..d31. Other architectures are not modelled. */
typedef struct RegisterSet { uint64_t bits[2]; } RegisterSet;

static int
modelled(int architecture)
{
    return architecture == ASM_X86_64 || architecture == ASM_X86 ||
        architecture == ASM_AARCH64 || architecture == ASM_ARM;
}

static void
register_add(RegisterSet *set, int slot) { set->bits[slot >> 6] |= UINT64_C(1) << (slot & 63); }

static int
register_has(const RegisterSet *set, int slot) { return (set->bits[slot >> 6] >> (slot & 63)) & 1; }

/* The decimal number spelled by text[0..length), or -1. */
static int
register_number(const char *text, size_t length)
{
    if(length == 0 || length > 2 || (length == 2 && text[0] == '0')) return -1;
    int value = 0;
    for(size_t i = 0; i < length; i++) {
        if(!isdigit((unsigned char)text[i])) return -1;
        value = value * 10 + text[i] - '0';
    }
    return value;
}

/* Slots name[0..length) covers: the count (0 when it names no register),
 * filling slots. */
static int
register_slots(int architecture, const char *name, size_t length, int slots[2])
{
    char text[16];
    if(length == 0 || length >= sizeof(text)) return 0;
    for(size_t i = 0; i < length; i++) text[i] = (char)tolower((unsigned char)name[i]);
    text[length] = 0;
    if(architecture == ASM_X86_64 || architecture == ASM_X86) {
        static const char *const legacy[8][5] = {
            {"rax", "eax", "ax", "al", "ah"}, {"rbx", "ebx", "bx", "bl", "bh"},
            {"rcx", "ecx", "cx", "cl", "ch"}, {"rdx", "edx", "dx", "dl", "dh"},
            {"rsi", "esi", "si", "sil", ""}, {"rdi", "edi", "di", "dil", ""},
            {"rbp", "ebp", "bp", "bpl", ""}, {"rsp", "esp", "sp", "spl", ""}};
        for(int r = 0; r < 8; r++)
            for(int w = 0; w < 5; w++)
                if(*legacy[r][w] && !strcmp(text, legacy[r][w])) { slots[0] = r; return 1; }
        int n;
        if(text[0] == 'r') {
            size_t digits = 1;
            while(isdigit((unsigned char)text[digits])) digits++;
            n = register_number(text + 1, digits - 1);
            const char *suffix = text + digits;
            if(n >= 8 && n <= 15 && (!*suffix || !strcmp(suffix, "d") || !strcmp(suffix, "w") || !strcmp(suffix, "b"))) {
                slots[0] = n; return 1;
            }
            return 0;
        }
        if((!strncmp(text, "xmm", 3) || !strncmp(text, "ymm", 3) || !strncmp(text, "zmm", 3)) &&
           (n = register_number(text + 3, length - 3)) >= 0 && n < 32) { slots[0] = 16 + n; return 1; }
        if(text[0] == 'k' && (n = register_number(text + 1, length - 1)) >= 0 && n < 8) { slots[0] = 48 + n; return 1; }
        if(!strncmp(text, "mm", 2) && (n = register_number(text + 2, length - 2)) >= 0 && n < 8) { slots[0] = 56 + n; return 1; }
        if(!strcmp(text, "st")) { slots[0] = 64; return 1; }
        return 0;
    }
    if(architecture == ASM_AARCH64) {
        int n = register_number(text + 1, length - 1);
        if((text[0] == 'x' || text[0] == 'w') && n >= 0 && n <= 30) { slots[0] = n; return 1; }
        if(strchr("vqdshb", text[0]) && n >= 0 && n < 32) { slots[0] = 32 + n; return 1; }
        if(!strcmp(text, "lr")) { slots[0] = 30; return 1; }
        if(!strcmp(text, "fp")) { slots[0] = 29; return 1; }
        if(!strcmp(text, "sp") || !strcmp(text, "wsp")) { slots[0] = 31; return 1; }
        return 0;
    }
    if(architecture == ASM_ARM) {
        static const char *const aliases[] = {"ip", "r12", "sp", "r13", "lr", "r14", "pc", "r15", "fp", "r11", "sl", "r10", "sb", "r9"};
        for(size_t i = 0; i < sizeof(aliases) / sizeof(*aliases); i += 2)
            if(!strcmp(text, aliases[i])) return register_slots(architecture, aliases[i + 1], strlen(aliases[i + 1]), slots);
        int n = register_number(text + 1, length - 1);
        if(text[0] == 'r' && n >= 0 && n <= 15) { slots[0] = n; return 1; }
        if(text[0] == 'd' && n >= 0 && n < 32) { slots[0] = 16 + n; return 1; }
        if(text[0] == 's' && n >= 0 && n < 32) { slots[0] = 16 + n / 2; return 1; }
        if(text[0] == 'q' && n >= 0 && n < 16) { slots[0] = 16 + 2 * n; slots[1] = 17 + 2 * n; return 2; }
        return 0;
    }
    return 0;
}

/* Registers templates may read without declaring them, and clobbers may
 * never name: the stack pointer always, and the frame pointer, which GCC
 * and Clang reserve in frame-pointer builds (Thumb-2 uses r7, ARM r11);
 * AArch64 x18 is the platform register on Android. */
static int
register_reserved(int architecture, int slot)
{
    switch(architecture) {
    case ASM_X86_64: case ASM_X86: return slot == 6 || slot == 7;
    case ASM_AARCH64: return slot == 18 || slot == 29 || slot == 31;
    case ASM_ARM: return slot == 7 || slot == 11 || slot == 13 || slot == 15;
    default: return 0;
    }
}

static const char *
register_clobber_name(int architecture, int slot, char *out, size_t size)
{
    static const char *const x86[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp"};
    static const char *const x86_32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp"};
    if(architecture == ASM_X86_64 || architecture == ASM_X86) {
        if(slot < 8) snprintf(out, size, "%s", architecture == ASM_X86 ? x86_32[slot] : x86[slot]);
        else if(slot < 16) snprintf(out, size, "r%d", slot);
        else if(slot < 48) snprintf(out, size, "xmm%d", slot - 16);
        else if(slot < 56) snprintf(out, size, "k%d", slot - 48);
        else if(slot < 64) snprintf(out, size, "mm%d", slot - 56);
        else snprintf(out, size, "st");
    } else if(architecture == ASM_AARCH64) {
        if(slot < 31) snprintf(out, size, "x%d", slot);
        else snprintf(out, size, "v%d", slot - 32);
    } else {
        if(slot == 14) snprintf(out, size, "lr");
        else if(slot < 16) snprintf(out, size, "r%d", slot);
        else snprintf(out, size, "d%d", slot - 16);
    }
    return out;
}

/* Parse one clobber item: a register, a range such as xmm0..xmm15 or
 * q0..q7, "cc" or "memory". Slots go into set; 0 with a diagnostic when the
 * item is unknown or reserved. */
static int
clobber_item(int architecture, const char *item, RegisterSet *set, int *memory, ZirSourceSpan span)
{
    if(!strcmp(item, "memory")) { *memory = 1; return 1; }
    if(!strcmp(item, "cc")) return 1;
    if(!modelled(architecture)) return 1;
    if((architecture == ASM_X86_64 || architecture == ASM_X86) &&
       (!strcmp(item, "dirflag") || !strcmp(item, "fpsr") || !strcmp(item, "flags")))
        return 1;
    const char *dots = strstr(item, "..");
    size_t first_length = dots ? (size_t)(dots - item) : strlen(item);
    int slots[2], count = register_slots(architecture, item, first_length, slots);
    if(count == 0) {
        Diagnostic(span, "check.assembly", "unknown #asm clobber '%s' for %s", item,
                   architectures[architecture].name);
        return 0;
    }
    int last = slots[count - 1];
    if(dots) {
        /* Both ends share a spelling, as xmm0..xmm15 or d8..d15 do. */
        const char *second = dots + 2;
        size_t prefix = 0;
        while(prefix < first_length && !isdigit((unsigned char)item[prefix])) prefix++;
        int other[2], other_count = register_slots(architecture, second, strlen(second), other);
        if(other_count == 0 || strncmp(item, second, prefix) || isdigit((unsigned char)second[prefix]) == 0 ||
           other[other_count - 1] < slots[0]) {
            Diagnostic(span, "check.assembly", "invalid #asm clobber range '%s'", item);
            return 0;
        }
        last = other[other_count - 1];
    }
    for(int slot = slots[0]; slot <= last; slot++) {
        if(register_reserved(architecture, slot)) {
            char name[16];
            Diagnostic(span, "check.assembly", "#asm cannot clobber %s, the stack, frame or platform register",
                       register_clobber_name(architecture, slot, name, sizeof(name)));
            return 0;
        }
        register_add(set, slot);
    }
    return 1;
}

/* All clobbers of a variant; 0 after a diagnostic. */
static int
clobber_set(const Assembly *assembly, RegisterSet *set, int *memory, ZirSourceSpan span)
{
    const char *items[ASSEMBLY_OPERANDS];
    int count = operand_list(assembly->part[4], items);
    memset(set, 0, sizeof(*set));
    *memory = 0;
    if(count < 0) return assembly_error(span, "#asm permits at most 30 clobbers separated by semicolons");
    for(int i = 0; i < count; i++)
        if(!clobber_item(assembly->architecture, items[i], set, memory, span)) return 0;
    return 1;
}

/* The expanded clobber list as C strings: ranges become registers, ARMv7 q
 * registers their d halves, and memory is added where operands imply it. */
static void
emit_clobbers(FILE *out, const Assembly *assembly, int implied_memory)
{
    RegisterSet set;
    int memory = 0, first = 1;
    const char *items[ASSEMBLY_OPERANDS];
    int count = operand_list(assembly->part[4], items);
    if(!modelled(assembly->architecture)) {
        for(int i = 0; i < count; i++) {
            if(!first) fputs(", ", out);
            assembly_string(out, items[i]);
            first = 0;
            memory |= !strcmp(items[i], "memory");
        }
    } else {
        if(!clobber_set(assembly, &set, &memory, (ZirSourceSpan){0})) exit(1);
        for(int slot = 0; slot < 128; slot++) {
            if(!register_has(&set, slot)) continue;
            char name[16];
            if(!first) fputs(", ", out);
            assembly_string(out, register_clobber_name(assembly->architecture, slot, name, sizeof(name)));
            first = 0;
        }
        for(int i = 0; i < count; i++)
            if(!strcmp(items[i], "cc")) { fputs(first ? "\"cc\"" : ", \"cc\"", out); first = 0; }
    }
    if(memory || implied_memory) fputs(first ? "\"memory\"" : ", \"memory\"", out);
}

/* Registers a constraint fixes, which the template may then name. */
static void
pinned_registers(int architecture, const char *constraint, RegisterSet *set)
{
    if(architecture != ASM_X86_64 && architecture != ASM_X86) return;
    for(const char *at = constraint; *at; at++) {
        const char *fixed = strchr("abcdSD", *at);
        if(*at && fixed) register_add(set, (int)(fixed - "abcdSD"));
        if(*at == 'A') { register_add(set, 0); register_add(set, 3); }
        if(*at == 't' || *at == 'u') register_add(set, 64);
    }
}

/* Check that every register a template names directly is declared, and
 * every operand it references exists. x86 templates name registers as
 * %%reg; AArch64 and ARMv7 ones as bare words, with ranges such as
 * {d0-d3}. names holds the operand aliases, outputs first. */
static int
check_template(const Assembly *assembly, const char *const *names, int count,
               const RegisterSet *allowed, ZirSourceSpan span)
{
    const char *text = assembly->part[1];
    int architecture = assembly->architecture;
    int x86 = architecture == ASM_X86_64 || architecture == ASM_X86;
    int previous_slot = -1;
    for(const char *at = text; *at;) {
        /* Comments run to the end of the line. */
        if((x86 && at[0] == '#') || (architecture == ASM_AARCH64 && at[0] == '/' && at[1] == '/') ||
           (architecture == ASM_ARM && at[0] == '@')) {
            while(*at && *at != '\n') at++;
            continue;
        }
        if(at[0] == '%' && at[1] == '%') {
            at += 2;
            if(!x86) continue;
            const char *start = at;
            while(isalnum((unsigned char)*at)) at++;
            int slots[2], used = register_slots(architecture, start, (size_t)(at - start), slots);
            for(int i = 0; i < used; i++) {
                /* The stack and frame pointers may be read, never clobbered. */
                if(register_reserved(architecture, slots[i])) continue;
                if(!register_has(allowed, slots[i])) {
                    Diagnostic(span, "check.assembly",
                               "#asm template uses %%%.*s, which its clobbers and constraints do not declare",
                               (int)(at - start), start);
                    return 0;
                }
            }
            continue;
        }
        if(at[0] == '%') {
            /* An operand: %[name], %w[name], %0, %x1. */
            const char *operand = at + 1;
            while(isalpha((unsigned char)*operand)) operand++;
            if(*operand == '[') {
                const char *end = strchr(operand, ']');
                int found = 0;
                for(int i = 0; end && i < count && !found; i++)
                    found = strlen(names[i]) == (size_t)(end - operand - 1) &&
                        !strncmp(names[i], operand + 1, (size_t)(end - operand - 1));
                if(!found) {
                    Diagnostic(span, "check.assembly", "#asm template names unknown operand %%%.*s",
                               end ? (int)(end - at) : 1, end ? at + 1 : "[");
                    return 0;
                }
                at = end + 1;
            } else if(isdigit((unsigned char)*operand)) {
                int index = 0;
                while(isdigit((unsigned char)*operand)) index = index * 10 + *operand++ - '0';
                if(index >= count) {
                    Diagnostic(span, "check.assembly", "#asm template operand %%%d exceeds its %d operands", index, count);
                    return 0;
                }
                at = operand;
            } else at = operand > at + 1 ? operand : at + 1;
            previous_slot = -1;
            continue;
        }
        if(x86 || !modelled(architecture)) { at++; continue; }
        if(isalpha((unsigned char)*at) || *at == '_') {
            const char *start = at;
            while(isalnum((unsigned char)*at) || *at == '_') at++;
            /* Type suffixes (.s16, .8h) and directives are not registers. */
            int suffix = start > text && (start[-1] == '.' || isdigit((unsigned char)start[-1]));
            int slots[2], used = suffix ? 0 : register_slots(architecture, start, (size_t)(at - start), slots);
            int range_from = previous_slot;
            previous_slot = -1;
            if(used == 0) continue;
            if(used == 1 && register_reserved(architecture, slots[0])) continue;
            int low = range_from >= 0 && range_from < slots[0] ? range_from : slots[0];
            for(int slot = low; slot <= slots[used - 1]; slot++)
                if(!register_has(allowed, slot)) {
                    Diagnostic(span, "check.assembly",
                               "#asm template uses %.*s, which its clobbers do not declare",
                               (int)(at - start), start);
                    return 0;
                }
            /* A range continues to the next register: {d0-d3}, {v0.8h-v3.8h}. */
            const char *next = at;
            while(*next == '.' || isalnum((unsigned char)*next)) next++;
            if(*next == '-') { previous_slot = slots[used - 1]; at = next; }
            continue;
        }
        if(*at != '-' && *at != ' ') previous_slot = -1;
        at++;
    }
    return 1;
}

/* Check one variant's clobbers and template; aliases name its operands,
 * outputs first, with their constraints. */
static int
check_variant(const Assembly *assembly, const char *const *aliases,
              const char *const *constraints, int count, ZirSourceSpan span)
{
    RegisterSet clobbers, allowed;
    int memory;
    if(!clobber_set(assembly, &clobbers, &memory, span)) return 0;
    allowed = clobbers;
    for(int i = 0; i < count; i++) {
        RegisterSet pinned = {{0}};
        pinned_registers(assembly->architecture, constraints[i], &pinned);
        for(int slot = 0; slot < 128; slot++) {
            if(!register_has(&pinned, slot)) continue;
            if(register_has(&clobbers, slot)) {
                char name[16];
                Diagnostic(span, "check.assembly", "#asm clobbers %s, which operand %s already holds",
                           register_clobber_name(assembly->architecture, slot, name, sizeof(name)), aliases[i]);
                return 0;
            }
            register_add(&allowed, slot);
        }
    }
    return check_template(assembly, aliases, count, &allowed, span);
}

/* TinyCC (`ziran run`, CC=tcc) assembles x86-64 GNU syntax through SSE4.2
 * with xmm0..xmm15 and allocates general registers and memory operands; it
 * has no AVX encoding and no "x" constraint. Other variants keep to GCC and
 * Clang. CPU features it can select read cpuid leaf 1 directly. */
static const struct { const char *name; char reg; int bit; } tinycc_features[] = {
    {"sse2", 'd', 26}, {"ssse3", 'c', 9}, {"sse4.1", 'c', 19}, {"sse4.2", 'c', 20}, {"popcnt", 'c', 23}
};

static int
tinycc_feature(size_t feature)
{
    for(size_t i = 0; i < sizeof(tinycc_features) / sizeof(*tinycc_features); i++)
        if(!strcmp(tinycc_features[i].name, cpu_features[feature].name)) return (int)i;
    return -1;
}

static int
tinycc_variant(const Assembly *assembly)
{
    if(assembly->architecture != ASM_X86_64) return 0;
    for(size_t i = 0; i < CPU_FEATURES; i++)
        if((assembly->features & (UINT64_C(1) << i)) && tinycc_feature(i) < 0) return 0;
    for(int part = 2; part <= 3; part++) {
        const char *items[ASSEMBLY_OPERANDS];
        int count = operand_list(assembly->part[part], items);
        if(count < 0) return 0;
        for(int i = 0; i < count; i++)
            for(const char *at = items[i]; *at && *at != ':'; at++) {
                if(*at == '[') { at = strchr(at, ']'); if(!at) return 0; continue; }
                if(!strchr("=+&%,0123456789abcdSDAqrRpmgieNMI", *at)) return 0;
            }
    }
    const char *items[ASSEMBLY_OPERANDS];
    int count = operand_list(assembly->part[4], items);
    for(int i = 0; i < count; i++) {
        if(!strcmp(items[i], "memory") || !strcmp(items[i], "cc")) continue;
        const char *dots = strstr(items[i], "..");
        int slots[2];
        size_t length = dots ? (size_t)(dots - items[i]) : strlen(items[i]);
        if(register_slots(ASM_X86_64, items[i], length, slots) != 1 || slots[0] >= 32) return 0;
        if(dots && (register_slots(ASM_X86_64, dots + 2, strlen(dots + 2), slots) != 1 || slots[0] >= 32))
            return 0;
        if(!strncmp(items[i], "ymm", 3) || !strncmp(items[i], "zmm", 3)) return 0;
    }
    return 1;
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
    if(st->kind != ZIR_STMT_BLOCK_OPEN || fn->is_extern || fn->is_global_initializer)
        return assembly_error(st->span, "#asm blocks require an ordinary procedure scope");
    if(st->expr_root != -1 || st->lhs_root != -1 || *st->type ||
       *st->assignment_op || st->is_using || st->is_else || st->is_parallel || st->is_gpu)
        return assembly_error(st->span, "invalid #asm block statement metadata");
    const char *canonical = AssemblyBlockText(st->text, st->span);
    if(canonical == NULL || !assembly_text_matches(canonical, st->assembly_text))
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
        const char *aliases[ASSEMBLY_OPERANDS], *constraints[ASSEMBLY_OPERANDS];
        for(int i = 0; i < current.inputs + current.outputs; i++) {
            aliases[i] = current.items[i].alias;
            constraints[i] = current.items[i].constraint;
        }
        if(!check_variant(&assembly, aliases, constraints, current.inputs + current.outputs, st->span))
            return 0;
        if(!variants) first = current;
        if(first.inputs != current.inputs || first.outputs != current.outputs)
            return assembly_error(st->span, "#asm block variants must bind the same operands in the same order");
        for(int i = 0; i < current.inputs + current.outputs; i++) {
            const BlockBinding *operand = &current.items[i];
            if(strcmp(first.items[i].alias, operand->alias) || strcmp(first.items[i].name, operand->name))
                return assembly_error(st->span, "#asm block variants must bind the same operands in the same order");
            const char *type = binding ? binding(context, operand->name) :
                block_binding_type(fn, statement, operand->name);
            if(!(fn->is_template && type != NULL && !*type) &&
               !template_operand_type(fn, type))
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
AssemblyVariantNext(const ZirModule *module, const ZirFunction *fn,
                     int statement, const char **cursor, AssemblyVariant *variant)
{
    if(cursor == NULL || *cursor == NULL || !*skip_ws(*cursor))
        return 0;
    ZirSourceSpan span = statement < 0 ? fn->span : fn->stmts[statement].span;
    Assembly assembly;
    if(!read_assembly(cursor, &assembly, span))
        return -1;
    memset(variant, 0, sizeof(*variant));
    variant->target = assembly.part[0];
    variant->text = assembly.part[1];
    variant->clobbers = assembly.part[4];
    variant->options = assembly.part[5];
    if(statement >= 0) {
        BlockBindings bindings = {0};
        if(!block_bindings(&assembly, &bindings, span)) return -1;
        variant->outputs = bindings.outputs;
        variant->inputs = bindings.inputs;
        for(int i = 0; i < bindings.outputs + bindings.inputs; i++) {
            const char *type = block_binding_type(fn, statement, bindings.items[i].name);
            if(type == NULL) return -1;
            variant->operands[i] = (AssemblyOperand){bindings.items[i].constraint,
                bindings.items[i].alias, bindings.items[i].name, type};
        }
    } else {
        AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
        if(outputs == NULL) return -1;
        const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
        const char *out[ASSEMBLY_OPERANDS], *in[ASSEMBLY_OPERANDS];
        int valid = assembly_outputs(module, fn, outputs);
        int no = operand_list(assembly.part[2], out), ni = operand_list(assembly.part[3], in);
        if(!valid || no != outputs->count || ni != parameters->count || no + ni > ASSEMBLY_OPERANDS) {
            free(outputs);
            return -1;
        }
        variant->outputs = no;
        variant->inputs = ni;
        variant->record = outputs->record;
        for(int i = 0; i < no; i++)
            variant->operands[i] = (AssemblyOperand){out[i], KeepText(outputs->names[i]),
                outputs->record ? KeepText(outputs->fields[i].name) : "out",
                outputs->record ? KeepText(outputs->fields[i].type) : fn->return_type};
        for(int i = 0; i < ni; i++)
            variant->operands[no + i] = (AssemblyOperand){in[i], parameters->items[i].name,
                parameters->items[i].name, parameters->items[i].type};
        free(outputs);
    }
    return 1;
}

int
CheckAssembly(const ZirModule *module, const ZirFunction *fn)
{
    if(!fn->assembly_text || !*fn->assembly_text) return 1;
    if(fn->is_extern || fn->is_global_initializer)
        return assembly_error(fn->span, "#asm requires an ordinary procedure with a portable body");
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs) return assembly_error(fn->span, "out of memory checking #asm");
    int deferred_outputs = template_result_type(fn);
    int valid_outputs = deferred_outputs || assembly_outputs(module, fn, outputs);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    if(!valid_outputs || parameters->count + outputs->count > ASSEMBLY_OPERANDS) {
        free(outputs);
        return assembly_error(fn->span, "#asm needs scalar/pointer operands or a record of scalar/pointer results (at most 30 operands)");
    }
    for(int i = 0; i < parameters->count; i++) {
        if(!parameters->items[i].name || (!template_operand_type(fn, parameters->items[i].type) &&
                                          !slice_operand(parameters->items[i].type))) {
            free(outputs);
            return assembly_error(fn->span, "#asm parameters must be named scalars, pointers or slices of scalars");
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
        if(no < 0 || ni < 0 || no + ni > ASSEMBLY_OPERANDS || nc < 0 ||
           (!deferred_outputs && no != outputs->count) ||
           ni != parameters->count) {
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
        if(!deferred_outputs) {
            const char *aliases[ASSEMBLY_OPERANDS], *constraints[ASSEMBLY_OPERANDS];
            for(int i = 0; i < no; i++) { aliases[i] = outputs->names[i]; constraints[i] = out[i]; }
            for(int i = 0; i < ni; i++) { aliases[no + i] = parameters->items[i].name; constraints[no + i] = in[i]; }
            if(!check_variant(&assembly, aliases, constraints, no + ni, fn->span)) { ok = 0; break; }
        }
    }
    free(outputs);
    return ok && variants > 0;
}

/* Quote decoded bytes, never interpolate them as host-language source. */
static void
assembly_byte(FILE *out, unsigned char byte)
{
    if(byte == '"' || byte == '\\') fprintf(out, "\\%c", byte);
    else if(byte < 32 || byte >= 127) fprintf(out, "\\%03o", byte);
    else fputc(byte, out);
}

static void
assembly_string(FILE *out, const char *text)
{
    fputc('"', out);
    for(const unsigned char *at = (const unsigned char *)text; *at; at++)
        assembly_byte(out, *at);
    fputc('"', out);
}

/* GNU operand aliases are C identifiers too. Emit positional references
 * so a legal Ziran alias such as 'register' never becomes a host keyword.
 * Keep escaped percent signs, modifiers, unknown names and literal data. */
static void
assembly_operand_text(FILE *out, const char *text, const char **names, int count,
                      int template)
{
    fputc('"', out);
    for(const char *at = text; *at;) {
        if(template && at[0] == '%' && at[1] == '%') {
            fputs("%%", out); at += 2; continue;
        }
        const char *name = at;
        if(template && *name == '%') {
            name++;
            while(isalpha((unsigned char)*name)) name++;
        } else if(template) name = NULL;
        const char *end = name && *name == '[' ? strchr(name + 1, ']') : NULL;
        int index = -1;
        if(end)
            for(int i = 0; i < count; i++)
                if(strlen(names[i]) == (size_t)(end - name - 1) &&
                   !strncmp(names[i], name + 1, (size_t)(end - name - 1))) {
                    index = i; break;
                }
        if(index >= 0) {
            while(at < name) assembly_byte(out, (unsigned char)*at++);
            fprintf(out, "%d", index);
            at = end + 1;
        } else assembly_byte(out, (unsigned char)*at++);
    }
    fputc('"', out);
}

/* Both helper definitions and their calls use exactly the same guards. */
static void
assembly_guard(FILE *out, const Assembly *assembly)
{
    fprintf(out, "!defined(ZIRAN_NO_ASM) && (((defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__))%s) && (%s)",
            assembly->tinycc ? " || defined(__TINYC__)" : "", architectures[assembly->architecture].guard);
    if(!assembly->features) return;
    if(assembly->architecture <= ASM_X86)
        fputs(" && !defined(_MSC_VER)", out);
    /* NEON exists on ARMv7-A and later application cores; a soft-float ABI
     * has no VFP/NEON registers, and bionic declares getauxval from API 18. */
    if(assembly->architecture == ASM_ARM)
        fputs(" && defined(__linux__) && !defined(__SOFTFP__) && __ARM_ARCH >= 7 &&"
              " __ARM_ARCH_PROFILE == 'A' && (!defined(__ANDROID__) || __ANDROID_API__ >= 18)", out);
    if(assembly->architecture == ASM_AARCH64)
        fputs(" && defined(__linux__)", out);
    for(size_t i = 0; i < CPU_FEATURES; i++) {
        if(!(assembly->features & (UINT64_C(1) << i))) continue;
        if(assembly->architecture == ASM_AARCH64 || assembly->architecture == ASM_ARM)
            fprintf(out, " && defined(%s)", cpu_features[i].hwcap);
        else if(assembly->architecture == ASM_WASM32)
            fprintf(out, " && defined(__wasm_%s__)", cpu_features[i].wasm32);
    }
}

/* The highest #target tier an x86 variant's features need, or 0. */
static int
assembly_tier(const Assembly *assembly)
{
    int tier = 0;
    if(assembly->architecture != ASM_X86_64 && assembly->architecture != ASM_X86) return 0;
    for(size_t i = 0; i < CPU_FEATURES; i++)
        if((assembly->features & (UINT64_C(1) << i)) && cpu_features[i].tier > tier)
            tier = cpu_features[i].tier;
    return tier;
}

/* Linux AArch64 and ARMv7 detect features through the kernel's AT_HWCAP;
 * x86 vector features also read ZIRAN_SIMD_TIER from the environment. */
static void
assembly_includes(FILE *out, const Assembly *assembly)
{
    /* The selection cache; TinyCC has no __atomic_load_n, and on x86-64,
     * its only assembly target, plain int accesses are atomic and ordered. */
    fputs("#ifndef ZIRAN_ASM_LOAD\n#if defined(__TINYC__)\n"
          "#define ZIRAN_ASM_LOAD(p) (*(volatile int *)(p))\n"
          "#define ZIRAN_ASM_STORE(p, v) (*(volatile int *)(p) = (v))\n#else\n"
          "#define ZIRAN_ASM_LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)\n"
          "#define ZIRAN_ASM_STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)\n"
          "#endif\n#endif\n", out);
    if(assembly_tier(assembly))
        fputs("#if !defined(ZIRAN_NO_ASM)\n#include <stdlib.h>\n#endif\n", out);
    if(!assembly->features ||
       (assembly->architecture != ASM_AARCH64 && assembly->architecture != ASM_ARM)) return;
    fprintf(out, "#if !defined(ZIRAN_NO_ASM) && defined(__linux__) && defined(%s)\n"
                 "#include <sys/auxv.h>\n#include <asm/hwcap.h>\n#endif\n",
            assembly->architecture == ASM_ARM ? "__arm__" : "__aarch64__");
}

/* ARMv7 helpers always run in A32 state, so one template serves Thumb-2 and
 * ARM builds. GCC selects the FPU; Clang names the LLVM feature. */
static void
assembly_attribute(FILE *out, const Assembly *assembly)
{
    /* TinyCC assembles any instruction it knows and never inlines; it has no
     * target attribute. Only x86-64 variants are TinyCC variants. */
    if(assembly->tinycc) fputs("\n#if !defined(__TINYC__)\n", out);
    for(int compiler = 0; compiler < (assembly->architecture == ASM_ARM ? 2 : 1); compiler++) {
        if(assembly->architecture == ASM_ARM)
            fputs(compiler ? "#else\n" : "#if defined(__clang__)\n", out);
        fputs("__attribute__((noinline, unused, target(\"", out);
        int first = 1;
        if(assembly->architecture == ASM_ARM) { fputs("arm", out); first = 0; }
        for(size_t i = 0; i < CPU_FEATURES; i++) {
            if(!(assembly->features & (UINT64_C(1) << i))) continue;
            if(!first) fputc(',', out);
            if(compiler) fputs("fpu=", out);
            fputs(feature_option(assembly->architecture, i), out);
            first = 0;
        }
        /* NEON needs the full D-register bank even when the caller was
         * compiled with -mfpu=vfpv3-d16. Clang keeps d32 separate from neon. */
        if(assembly->architecture == ASM_ARM && !compiler && assembly->features)
            fputs(",d32", out);
        fputs("\")))", out);
        if(assembly->architecture == ASM_ARM) fputc('\n', out);
    }
    if(assembly->architecture == ASM_ARM) fputs("#endif\n", out);
    if(assembly->tinycc) fputs("\n#endif\n", out);
}

/* Clang assembles inline ARM templates for the translation unit's FPU, not
 * the helper's target attribute; GCC already emits the same directive. */
static void
assembly_template_prefix(FILE *out, const Assembly *assembly)
{
    if(assembly->architecture != ASM_ARM || !assembly->features) return;
    for(size_t i = 0; i < CPU_FEATURES; i++)
        if(assembly->features & (UINT64_C(1) << i))
            fprintf(out, "\".fpu %s\\n\\t\" ", cpu_features[i].arm);
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
              const char *result_type, const Assembly *assembly, const AssemblyOutputs *outputs,
              const char *available)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const char *names[ASSEMBLY_OPERANDS];
    for(int i = 0; i < outputs->count; i++) names[i] = outputs->names[i];
    for(int i = 0; i < parameters->count; i++) names[outputs->count + i] = parameters->items[i].name;
    char result_name[ZIR_NAME_MAX];
    ArrayAbiName(fn, -1, result_name, sizeof(result_name));
    if(outputs->count) fprintf(out, "        %s %s;\n", result_type, result_name);
    if(available) fprintf(out, "        *%s = 1;\n", available);
    fputs("        __asm__", out);
    if(*assembly->part[5]) fputs(" __volatile__", out);
    fputc('(', out);
    assembly_template_prefix(out, assembly);
    assembly_operand_text(out, assembly->part[1], names, outputs->count + parameters->count, 1);
    const char *constraints[ASSEMBLY_OPERANDS];
    operand_list(assembly->part[2], constraints);
    fputs("\n            : ", out);
    for(int i = 0; i < outputs->count; i++) {
        if(i) fputs(", ", out);
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
    int memory = 0;
    for(int i = 0; i < parameters->count; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
        if(i) fputs(", ", out);
        assembly_operand_text(out, constraints[i], names, outputs->count, 0);
        int slice = slice_operand(parameters->items[i].type);
        fprintf(out, slice ? " ((%s).data)" : " (%s)", name);
        memory |= slice || (parameters->items[i].type && parameters->items[i].type[0] == '*');
    }
    /* Assembly may reach any memory a pointer or slice operand points to. */
    fputs("\n            : ", out);
    emit_clobbers(out, assembly, memory);
    fputs(");\n        return", out);
    if(outputs->count) fprintf(out, " %s", result_name);
    fputs(";\n", out);
}

static void
assembly_helpers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                 ZirTarget target, const char *result_type, const char *args,
                 const char *available)
{
    if(!fn->assembly_text || !*fn->assembly_text) return;
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs || !CheckAssembly(module, fn) || !assembly_outputs(module, fn, outputs)) exit(1);
    const char *cursor = fn->assembly_text;
    int ordinal = 0;
    if(!available)
        fputs("#if defined(ZIRAN_ASM_CHECK) && !defined(ZIRAN_NO_ASM)\n"
              "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#endif\n", out);
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        int selected = runtime_selected(&assembly);
        /* A check build calls every variant on its own, inline ones too. */
        if(selected || !available) {
            assembly_includes(out, &assembly);
            fputs(selected ? "#if " : "#if defined(ZIRAN_ASM_CHECK) && ", out);
            assembly_guard(out, &assembly); fputc('\n', out);
            char helper[ZIR_NAME_MAX];
            assembly_helper_name(module, fn, target, ordinal, helper, sizeof(helper));
            if(selected) assembly_attribute(out, &assembly);
            else fputs("__attribute__((noinline, unused))", out);
            fprintf(out, " static %s\n%s(%s)\n{\n", result_type, helper, args);
            assembly_body(out, module, fn, target, result_type, &assembly, outputs, available);
            fputs("}\n#endif\n", out);
        }
        ordinal++;
    }
    free(outputs);
}

/* The memory a parameter's assembly may change, which a check compares:
 * a slice's elements, or the scalar a pointer points to. Writes the byte
 * count and address as C expressions; 0 when there is none. */
static int
check_region(const ZirFunction *fn, ZirTarget target, const ZirParameter *parameter,
             char *size, char *address, size_t length)
{
    char name[ZIR_NAME_MAX * 4], element[ZIR_NAME_MAX];
    const char *type = parameter->type;
    TargetBindingName(fn, target, parameter->name, name, sizeof(name));
    if(type && SliceElementType(type, element, sizeof(element)) && operand_type(element)) {
        const char *scalar = element[0] == '*' ? "void *" : TargetType(ScalarType(element), target);
        snprintf(size, length, "(size_t)(%s).length * sizeof(%s)", name, scalar);
        snprintf(address, length, "(%s).data", name);
        return 1;
    }
    if(!type || type[0] != '*') return 0;
    const char *pointee = skip_ws(type + 1);
    if(!operand_type(pointee) || (pointee[0] != '*' && !strcmp(ScalarType(pointee), "void"))) return 0;
    snprintf(size, length, "sizeof(*(%s))", name);
    snprintf(address, length, "(void *)(%s)", name);
    return 1;
}

static void
check_failure(FILE *out, const ZirFunction *fn, const Assembly *assembly, const char *what)
{
    fputs(" {\n                    fprintf(stderr, \"ziran: #asm check failed: %s variant \\\"%s\\\" differs from its portable body in %s\\n\", ", out);
    assembly_string(out, fn->name); fputs(", ", out);
    assembly_string(out, assembly->part[0]); fputs(", ", out);
    assembly_string(out, what);
    fputs(");\n                    abort();\n                }\n", out);
}

/* A ZIRAN_ASM_CHECK build runs the portable body and then each variant
 * this CPU can run on the same inputs, aborts on any difference in the
 * result or the memory check_region covers, and returns the portable
 * result. ZIRAN_SIMD_TIER limits which variants run as it does dispatch. */
static void
assembly_check(FILE *out, const ZirModule *module, const ZirFunction *fn, ZirTarget target,
               const char *result_type, const AssemblyOutputs *outputs)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    char native[ZIR_NAME_MAX * 4], flag[ZIR_NAME_MAX], call[ZIR_TEXT_MAX];
    char size[ZIR_NAME_MAX * 6], address[ZIR_NAME_MAX * 6];
    size_t used = 0;
    NativeCFunctionName(module, fn, native, sizeof(native));
    ArrayAbiName(fn, 65, flag, sizeof(flag));
    call[0] = 0;
    for(int i = 0; i < parameters->count; i++) {
        char name[ZIR_NAME_MAX * 4];
        TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
        used += (size_t)snprintf(call + used, sizeof(call) - used, "%s%s", i ? ", " : "", name);
        if(used >= sizeof(call)) exit(1);
    }
    fprintf(out, "#if defined(ZIRAN_ASM_CHECK) && !defined(ZIRAN_NO_ASM)\n    {\n"
                 "#if defined(__TINYC__)\n        static int %s;\n#else\n        static __thread int %s;\n#endif\n"
                 "        if(!%s) {\n", flag, flag, flag);
    for(int i = 0; i < parameters->count; i++) {
        if(!check_region(fn, target, &parameters->items[i], size, address, sizeof(size))) continue;
        fprintf(out, "            size_t ziran_size_%d = %s;\n"
                     "            unsigned char *ziran_in_%d = malloc(ziran_size_%d + 1), *ziran_port_%d = malloc(ziran_size_%d + 1);\n"
                     "            if(!ziran_in_%d || !ziran_port_%d) abort();\n"
                     "            if(ziran_size_%d) memcpy(ziran_in_%d, %s, ziran_size_%d);\n",
                i, size, i, i, i, i, i, i, i, i, address, i);
    }
    fprintf(out, "            %s = 1;\n            ", flag);
    if(outputs->count) fprintf(out, "%s ziran_portable = ", result_type);
    fprintf(out, "%s(%s);\n            %s = 0;\n", native, call, flag);
    for(int i = 0; i < parameters->count; i++)
        if(check_region(fn, target, &parameters->items[i], size, address, sizeof(size)))
            fprintf(out, "            if(ziran_size_%d) memcpy(ziran_port_%d, %s, ziran_size_%d);\n", i, i, address, i);
    const char *cursor = fn->assembly_text;
    int ordinal = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        char helper[ZIR_NAME_MAX];
        assembly_helper_name(module, fn, target, ordinal++, helper, sizeof(helper));
        fputs("#if ", out); assembly_guard(out, &assembly); fputs("\n            if(", out);
        if(runtime_selected(&assembly)) assembly_predicate(out, &assembly);
        else fputc('1', out);
        fputs(") {\n", out);
        for(int i = 0; i < parameters->count; i++)
            if(check_region(fn, target, &parameters->items[i], size, address, sizeof(size)))
                fprintf(out, "                if(ziran_size_%d) memcpy(%s, ziran_in_%d, ziran_size_%d);\n", i, address, i, i);
        fputs("                ", out);
        if(outputs->count) fprintf(out, "%s ziran_variant = ", result_type);
        fprintf(out, "%s(%s);\n", helper, call);
        if(outputs->count && outputs->record) {
            const ZirType *record = FindType(module, fn->return_type, NULL);
            for(int i = 0; i < outputs->count; i++) {
                char field[ZIR_NAME_MAX * 4];
                TargetFieldName(record, target, outputs->fields[i].name, field, sizeof(field));
                fprintf(out, "                if(memcmp(&ziran_variant.%s, &ziran_portable.%s, sizeof(ziran_portable.%s)))",
                        field, field, field);
                check_failure(out, fn, &assembly, outputs->fields[i].name);
            }
        } else if(outputs->count) {
            fputs("                if(memcmp(&ziran_variant, &ziran_portable, sizeof(ziran_portable)))", out);
            check_failure(out, fn, &assembly, "its result");
        }
        for(int i = 0; i < parameters->count; i++)
            if(check_region(fn, target, &parameters->items[i], size, address, sizeof(size))) {
                fprintf(out, "                if(ziran_size_%d && memcmp(%s, ziran_port_%d, ziran_size_%d))", i, address, i, i);
                check_failure(out, fn, &assembly, parameters->items[i].name);
            }
        fputs("            }\n#endif\n", out);
    }
    for(int i = 0; i < parameters->count; i++)
        if(check_region(fn, target, &parameters->items[i], size, address, sizeof(size)))
            fprintf(out, "            if(ziran_size_%d) memcpy(%s, ziran_port_%d, ziran_size_%d);\n"
                         "            free(ziran_in_%d); free(ziran_port_%d);\n", i, address, i, i, i, i);
    fprintf(out, "            return%s;\n        }\n    }\n#else\n", outputs->count ? " ziran_portable" : "");
}

/* Clang before 19 has no "lzcnt" for __builtin_cpu_supports, so it reads
 * the CPUID bit (extended leaf 0x80000001, ECX bit 5) directly. */
static const char lzcnt_cpuid[] =
    "__extension__ ({\n"
    "    unsigned int zir_leaf = 0x80000000u, zir_b, zir_c = 0, zir_d, zir_lzcnt = 0;\n"
    "    __asm__(\"cpuid\" : \"+a\"(zir_leaf), \"=b\"(zir_b), \"+c\"(zir_c), \"=d\"(zir_d));\n"
    "    if(zir_leaf >= 0x80000001u) {\n"
    "        zir_leaf = 0x80000001u; zir_c = 0;\n"
    "        __asm__(\"cpuid\" : \"+a\"(zir_leaf), \"=b\"(zir_b), \"+c\"(zir_c), \"=d\"(zir_d));\n"
    "        zir_lzcnt = (zir_c & 0x20u) != 0;\n"
    "    }\n"
    "    zir_lzcnt; })";

static void
assembly_predicate(FILE *out, const Assembly *assembly)
{
    int first = 1;
    for(size_t i = 0; i < CPU_FEATURES; i++) {
        if(!(assembly->features & (UINT64_C(1) << i))) continue;
        if(!first) fputs(" && ", out);
        if(assembly->architecture == ASM_AARCH64 || assembly->architecture == ASM_ARM)
            fprintf(out, "(getauxval(AT_HWCAP) & %s) != 0", cpu_features[i].hwcap);
        else if(!strcmp(cpu_features[i].name, "lzcnt"))
            fprintf(out, "(\n#if defined(__clang__) && __clang_major__ < 19\n%s\n#else\n"
                         "__builtin_cpu_supports(\"lzcnt\")\n#endif\n)", lzcnt_cpuid);
        else if(assembly->tinycc)
            fprintf(out, "(\n#if defined(__TINYC__)\n__extension__ ({\n"
                         "    unsigned int zir_a = 1u, zir_b, zir_c = 0u, zir_d;\n"
                         "    __asm__(\"cpuid\" : \"+a\"(zir_a), \"=b\"(zir_b), \"+c\"(zir_c), \"=d\"(zir_d));\n"
                         "    (zir_%c >> %du) & 1u; })\n#else\n__builtin_cpu_supports(\"%s\")\n#endif\n)",
                    tinycc_features[tinycc_feature(i)].reg, tinycc_features[tinycc_feature(i)].bit,
                    cpu_features[i].name);
        else fprintf(out, "__builtin_cpu_supports(\"%s\")", cpu_features[i].name);
        first = 0;
    }
    /* ZIRAN_SIMD_TIER, read as ziran_simd.h reads it for #target clones,
     * hides a feature beyond the forced tier. */
    int tier = assembly_tier(assembly);
    if(tier)
        fprintf(out, " && !__extension__ ({\n"
                     "    const char *zir_tier = getenv(\"ZIRAN_SIMD_TIER\");\n"
                     "    zir_tier != NULL && zir_tier[0] >= '1' && zir_tier[0] < '%c' && zir_tier[1] == '\\0'; })",
                '0' + tier);
}

static void
assembly_function(FILE *out, const ZirModule *module, const ZirFunction *fn, ZirTarget target,
                  const char *result_type, const char *available)
{
    if(!fn->assembly_text || !*fn->assembly_text) return;
    AssemblyOutputs *outputs = calloc(1, sizeof(*outputs));
    if(!outputs || !CheckAssembly(module, fn) || !assembly_outputs(module, fn, outputs)) exit(1);
    const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
    const char *cursor = fn->assembly_text;
    int features = 0;
    if(!available) assembly_check(out, module, fn, target, result_type, outputs);
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        if(runtime_selected(&assembly)) {
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
                     "        int %s = ZIRAN_ASM_LOAD(&%s);\n"
                     "        if(%s == 0) {\n            %s = 1;\n",
                     state, selected, state, selected, selected);
        fputs("#if (defined(__x86_64__) || defined(__i386__)) && !defined(__TINYC__)\n"
              "            __builtin_cpu_init();\n#endif\n", out);
        cursor = fn->assembly_text;
        int ordinal = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
            if(runtime_selected(&assembly)) {
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                fprintf(out, "            if(%s == 1 && (", selected);
                assembly_predicate(out, &assembly);
                fprintf(out, ")) %s = %d;\n#endif\n", selected, ordinal + 2);
            }
            ordinal++;
        }
        fprintf(out, "            ZIRAN_ASM_STORE(&%s, %s);\n        }\n", state, selected);
        cursor = fn->assembly_text;
        ordinal = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
            if(runtime_selected(&assembly)) {
                char helper[ZIR_NAME_MAX];
                assembly_helper_name(module, fn, target, ordinal, helper, sizeof(helper));
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                fprintf(out, "        if(%s == %d) { ", selected, ordinal + 2);
                if(outputs->count) fputs("return ", out);
                fprintf(out, "%s(", helper);
                for(int i = 0; i < parameters->count; i++) {
                    char name[ZIR_NAME_MAX * 4];
                    TargetBindingName(fn, target, parameters->items[i].name, name, sizeof(name));
                    fprintf(out, "%s%s", i ? ", " : "", name);
                }
                if(available) fprintf(out, "%s%s", parameters->count ? ", " : "", available);
                fprintf(out, ");%s }\n#endif\n", outputs->count ? "" : " return;");
            }
            ordinal++;
        }
        fputs("    }\n#endif\n", out);
    }
    cursor = fn->assembly_text;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        if(runtime_selected(&assembly)) continue;
        fputs("#if ", out); assembly_guard(out, &assembly); fputs("\n    {\n", out);
        assembly_body(out, module, fn, target, result_type, &assembly, outputs, available);
        fputs("    }\n#endif\n", out);
    }
    if(!available) fputs("#endif\n", out);
    free(outputs);
}

void
EmitAssemblyHelpers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                    ZirTarget target, const char *result_type, const char *args)
{
    assembly_helpers(out, module, fn, target, result_type, args, NULL);
}

void
EmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *fn,
             ZirTarget target, const char *result_type)
{
    assembly_function(out, module, fn, target, result_type, NULL);
}

void
EmitAssemblyTryHelpers(FILE *out, const ZirModule *module, const ZirFunction *fn,
                       ZirTarget target, const char *result_type, const char *args,
                       const char *available)
{
    assembly_helpers(out, module, fn, target, result_type, args, available);
}

void
EmitAssemblyTry(FILE *out, const ZirModule *module, const ZirFunction *fn,
                ZirTarget target, const char *result_type, const char *available)
{
    assembly_function(out, module, fn, target, result_type, available);
}

void
EmitAssemblyAvailable(FILE *out, const ZirFunction *fn, int statement)
{
    const char *cursor = statement < 0 ? fn->assembly_text : fn->stmts[statement].assembly_text;
    while(cursor && *skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, fn->span)) exit(1);
        fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
        if(runtime_selected(&assembly)) {
            if(assembly.architecture == ASM_X86 || assembly.architecture == ASM_X86_64)
                fputs("#if !defined(__TINYC__)\n    __builtin_cpu_init();\n#endif\n", out);
            fputs("    if(", out); assembly_predicate(out, &assembly); fputs(") return 1;\n", out);
        } else fputs("    return 1;\n", out);
        fputs("#endif\n", out);
    }
    fputs("    return 0;\n", out);
}

int
AssemblyInputNeedsStorage(const char *constraint)
{
    for(const char *p = constraint; *p; p++) {
        if(*p == '[') { while(p[1] && *p != ']') p++; if(*p != ']') return 1; }
        else if(strchr("0123456789,=+&%!*#? rfvxwabcdqSDAt", *p) == NULL) return 1;
    }
    return 0;
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
block_assembly_body(FILE *out, const ZirFunction *fn, int statement, ZirTarget target,
                    const Assembly *assembly, const BlockBindings *bindings, int helper,
                    ZirAssemblyOperandResolver resolver, void *context)
{
    const char *names[ASSEMBLY_OPERANDS];
    for(int i = 0; i < bindings->outputs + bindings->inputs; i++) names[i] = bindings->items[i].alias;
    fputs("        __asm__", out);
    if(*assembly->part[5]) fputs(" __volatile__", out);
    fputc('(', out);
    if(helper) assembly_template_prefix(out, assembly);
    assembly_operand_text(out, assembly->part[1], names, bindings->outputs + bindings->inputs, 1);
    for(int direction = 0; direction < 2; direction++) {
        fputs("\n            : ", out);
        int count = direction ? bindings->inputs : bindings->outputs;
        for(int i = 0; i < count; i++) {
            int position = direction ? bindings->outputs + i : i;
            const BlockBinding *binding = &bindings->items[position];
            if(i) fputs(", ", out);
            assembly_operand_text(out, binding->constraint, names, bindings->outputs, 0);
            if(helper && !direction && block_returns_register(bindings)) fputs(" (ziran_result)", out);
            else if(helper) fprintf(out, " (%sziran_operand_%d)",
                !direction || AssemblyInputNeedsStorage(binding->constraint) ? "*" : "", position);
            else {
                char native[ZIR_NAME_MAX * 4];
                if(resolver) resolver(context, position, native, sizeof(native));
                else TargetBindingName(fn, target, binding->name, native, sizeof(native));
                fprintf(out, " (%s)", native);
            }
        }
    }
    int memory = 0;
    for(int i = 0; i < bindings->outputs + bindings->inputs; i++) {
        const char *type = block_binding_type(fn, statement, bindings->items[i].name);
        memory |= type && type[0] == '*';
    }
    fputs("\n            : ", out);
    emit_clobbers(out, assembly, memory);
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

static void
assembly_block_helpers(FILE *out, const ZirModule *module,
                       const ZirFunction *fn, ZirTarget target, int begin, int end)
{
    for(int statement = begin; statement < end; statement++) {
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
            if(runtime_selected(&assembly)) {
                assembly_includes(out, &assembly);
                fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
                char name[ZIR_NAME_MAX];
                block_helper_name(module, fn, target, statement, variant, name, sizeof(name));
                assembly_attribute(out, &assembly);
                char returned[ZIR_NAME_MAX * 4] = "void";
                int first_argument = block_returns_register(&bindings);
                if(first_argument)
                    block_native_type(module, block_binding_type(fn, statement, bindings.items[0].name),
                                      target, returned, sizeof(returned), st->span);
                fprintf(out, " static %s\n%s(", returned, name);
                for(int i = first_argument; i < bindings.outputs + bindings.inputs; i++) {
                    const char *type = block_binding_type(fn, statement, bindings.items[i].name);
                    char native[ZIR_NAME_MAX * 4];
                    block_native_type(module, type, target, native, sizeof(native), st->span);
                    fprintf(out, "%s%s%sziran_operand_%d", i > first_argument ? ", " : "", native,
                            i < bindings.outputs ? " *" :
                            AssemblyInputNeedsStorage(bindings.items[i].constraint) ? " const *" : " ", i);
                }
                if(first_argument == bindings.outputs + bindings.inputs) fputs("void", out);
                fputs(")\n{\n", out);
                if(first_argument) fprintf(out, "    %s ziran_result;\n", returned);
                block_assembly_body(out, fn, statement, target, &assembly, &bindings, 1, NULL, NULL);
                if(first_argument) fputs("    return ziran_result;\n", out);
                fputs("}\n#endif\n", out);
            }
            variant++;
        }
    }
}

static int
assembly_block(FILE *out, const ZirModule *module, const ZirFunction *fn,
               int statement, ZirTarget target,
               ZirAssemblyOperandResolver resolver, void *context)
{
    if(target != ZIR_C) return 0;
    const ZirStmt *st = &fn->stmts[statement];
    if(!StatementHasAssembly(st)) return 0;
    if(!CheckAssemblyBlock(fn, statement, NULL, NULL)) exit(1);
    const char *cursor = st->assembly_text;
    int features = 0;
    while(*skip_ws(cursor)) {
        Assembly assembly;
        if(!read_assembly(&cursor, &assembly, st->span)) exit(1);
        features |= runtime_selected(&assembly);
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
                     "        int %s = ZIRAN_ASM_LOAD(&%s);\n"
                     "        if(%s == 0) {\n            %s = 1;\n",
                     state, selected, state, selected, selected);
        fputs("#if (defined(__x86_64__) || defined(__i386__)) && !defined(_MSC_VER) && !defined(__TINYC__)\n"
              "            __builtin_cpu_init();\n#endif\n", out);
        cursor = st->assembly_text;
        int variant = 0;
        while(*skip_ws(cursor)) {
            Assembly assembly;
            if(!read_assembly(&cursor, &assembly, st->span)) exit(1);
            fputs("#if ", out); assembly_guard(out, &assembly); fputc('\n', out);
            fprintf(out, "            if(%s == 1", selected);
            if(runtime_selected(&assembly)) { fputs(" && (", out); assembly_predicate(out, &assembly); fputc(')', out); }
            fprintf(out, ") %s = %d;\n#endif\n", selected, variant + 2);
            variant++;
        }
        fprintf(out, "            ZIRAN_ASM_STORE(&%s, %s);\n        }\n#endif\n", state, selected);
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
        if(runtime_selected(&assembly)) {
            char helper[ZIR_NAME_MAX];
            block_helper_name(module, fn, target, statement, variant, helper, sizeof(helper));
            int first_argument = block_returns_register(&bindings);
            fputs("            ", out);
            if(first_argument) {
                char name[ZIR_NAME_MAX * 4];
                if(resolver) resolver(context, 0, name, sizeof(name));
                else TargetBindingName(fn, target, bindings.items[0].name, name, sizeof(name));
                fprintf(out, "%s = ", name);
            }
            fprintf(out, "%s(", helper);
            for(int i = first_argument; i < bindings.outputs + bindings.inputs; i++) {
                char name[ZIR_NAME_MAX * 4];
                if(resolver) resolver(context, i, name, sizeof(name));
                else TargetBindingName(fn, target, bindings.items[i].name, name, sizeof(name));
                fprintf(out, "%s%s%s", i > first_argument ? ", " : "", i < bindings.outputs || AssemblyInputNeedsStorage(bindings.items[i].constraint) ? "&" : "", name);
            }
            fputs(");\n", out);
        } else block_assembly_body(out, fn, statement, target, &assembly, &bindings, 0, resolver, context);
        fprintf(out, "            %s = 1;\n        }\n#endif\n", done);
        variant++;
    }
    fprintf(out, "        if(!%s) {\n", done);
    return 1;
}

void
EmitAssemblyBlockHelpers(FILE *out, const ZirModule *module,
                         const ZirFunction *fn, ZirTarget target)
{
    assembly_block_helpers(out, module, fn, target, 0, fn->stmt_count);
}

void
EmitAssemblyBlockHelpersAt(FILE *out, const ZirModule *module,
                           const ZirFunction *fn, int statement)
{
    assembly_block_helpers(out, module, fn, ZIR_C, statement, statement + 1);
}

int
EmitAssemblyBlock(FILE *out, const ZirModule *module, const ZirFunction *fn,
                  int statement, ZirTarget target)
{
    return assembly_block(out, module, fn, statement, target, NULL, NULL);
}

int
EmitAssemblyBlockMapped(FILE *out, const ZirModule *module, const ZirFunction *fn,
                        int statement, ZirAssemblyOperandResolver resolver, void *context)
{
    return assembly_block(out, module, fn, statement, ZIR_C, resolver, context);
}

int
AssemblyTiedOutput(const AssemblyVariant *source, const char *constraint)
{
    if(isdigit((unsigned char)*constraint)) {
        char *end;
        long index = strtol(constraint, &end, 10);
        return *end == 0 && index >= 0 && index < source->outputs ? (int)index : -1;
    }
    if(*constraint == '[') {
        size_t length = strlen(constraint);
        if(length > 2 && constraint[length - 1] == ']')
            for(int i = 0; i < source->outputs; i++)
                if(strlen(source->operands[i].alias) == length - 2 &&
                   !strncmp(source->operands[i].alias, constraint + 1, length - 2)) return i;
    }
    return -1;
}

int
AssemblyAlternativeCount(const AssemblyVariant *source)
{
    int count = 0;
    for(int i = 0; i < source->outputs + source->inputs; i++) {
        int alternatives = 1;
        for(const char *p = source->operands[i].constraint; *p; p++)
            alternatives += *p == ',';
        if(count && count != alternatives) return 0;
        count = alternatives;
    }
    return count ? count : 1;
}

const char *
AssemblyConstraintAlternative(const char *constraint, int index)
{
    /* Direction is already known; commutativity permits but does not require
     * swapping the inputs. Keep their declared order in native emitters. */
    while(*constraint == '=' || *constraint == '%') constraint++;
    while(index--) {
        const char *comma = strchr(constraint, ',');
        if(comma == NULL) return "";
        constraint = comma + 1;
    }
    const char *end = strchr(constraint, ',');
    size_t length = end ? (size_t)(end - constraint) : strlen(constraint);
    char word[ZIR_NAME_MAX];
    if(length >= sizeof(word)) return "";
    memcpy(word, constraint, length); word[length] = 0;
    return KeepText(word);
}
