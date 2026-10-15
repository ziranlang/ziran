#include "zir_rust_asm.h"
#include "zir_asm.h"
#include "zir_check.h"
#include "zir_scalar.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Rust has a different register/template interface from GNU extended asm.
 * Consume the checked operands, never reparse Ziran expressions here. */
typedef struct RustOperand {
    const char *type;
    const char *reg;
    char fixed[16];
    int width;
    int tied_to, tied_input, early, memory;
} RustOperand;

typedef struct NumericLabel {
    const char *text;
    size_t length;
} NumericLabel;

typedef struct RustVariant {
    AssemblyVariant source;
    RustOperand operands[ASSEMBLY_OPERANDS];
    char arch[32], features[256];
    const char *clobbers[ASSEMBLY_OPERANDS];
    int clobber_count, x86, bits;
    const char *unsupported;
    char *text;
    NumericLabel *labels;
    size_t label_count, label_capacity;
} RustVariant;

typedef struct RustAssembly {
    FILE *out;
    const ZirModule *module;
    const ZirFunction *function;
    const RustAssemblyTarget *target;
    int statement, indent, count, capacity;
    char prefix[96];
    char selector[112];
    RustVariant **variants;
} RustAssembly;

static void line(RustAssembly *assembly, const char *format, ...)
{
    for(int i = 0; i < assembly->indent; i++) fputs("    ", assembly->out);
    va_list args;
    va_start(args, format);
    vfprintf(assembly->out, format, args);
    va_end(args);
    fputc('\n', assembly->out);
}

static void quote(FILE *out, const char *text)
{
    fputc('"', out);
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if(*p == '"' || *p == '\\') fprintf(out, "\\%c", *p);
        else if(*p < 32 || *p == 127) fprintf(out, "\\u{%x}", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}

static int utf8(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    while(*p) {
        unsigned first = *p++, value, extra, minimum;
        if(first < 128) continue;
        if(first >= 0xc2 && first <= 0xdf) { value = first & 31; extra = 1; minimum = 128; }
        else if(first >= 0xe0 && first <= 0xef) { value = first & 15; extra = 2; minimum = 2048; }
        else if(first >= 0xf0 && first <= 0xf4) { value = first & 7; extra = 3; minimum = 65536; }
        else return 0;
        while(extra--) {
            if((*p & 0xc0) != 0x80) return 0;
            value = (value << 6) | (*p++ & 63);
        }
        if(value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
    }
    return 1;
}

static int list(const char *text, const char **items)
{
    int count = 0;
    while(*text) {
        const char *end = strchr(text, ';');
        size_t length = end ? (size_t)(end - text) : strlen(text);
        char word[ZIR_NAME_MAX];
        if(length >= sizeof(word) || count == ASSEMBLY_OPERANDS) return -1;
        memcpy(word, text, length); word[length] = 0;
        items[count++] = KeepText(trim_in_place(word));
        if(!end) break;
        text = end + 1;
    }
    return count;
}

static int word_width(const char *type, int bits)
{
    if(type[0] == '*' || !strcmp(type, "isize") || !strcmp(type, "usize")) return bits;
    if(!strcmp(type, "bool")) return 8;
    if(!strcmp(type, "float32")) return 32;
    if(!strcmp(type, "float64") || !strcmp(type, "real")) return 64;
    return (int)ScalarWidth(type);
}

static const char *feature(const char *arch, const char *name)
{
    if(!strcmp(arch, "aarch64")) {
        if(!strcmp(name, "simd")) return "neon";
        if(!strcmp(name, "crc32")) return "crc";
        if(!strcmp(name, "atomics")) return "lse";
    } else {
        if(!strcmp(name, "bmi")) return "bmi1";
        if(!strcmp(name, "pclmul")) return "pclmulqdq";
    }
    return name;
}

static void features(RustVariant *variant)
{
    const char *next = strchr(variant->source.target, '+');
    size_t used = 0;
    while(next) {
        const char *start = next + 1;
        next = strchr(start, '+');
        size_t length = next ? (size_t)(next - start) : strlen(start);
        char name[64];
        if(length >= sizeof(name)) { variant->unsupported = "CPU feature spelling"; return; }
        memcpy(name, start, length); name[length] = 0;
        /* PMULL shares LLVM's aes code-generation feature, but the runtime
         * predicate below still checks the requested PMULL capability. */
        const char *mapped = !strcmp(variant->arch, "aarch64") && !strcmp(name, "pmull") ?
            "aes" : feature(variant->arch, name);
        int wrote = snprintf(variant->features + used, sizeof(variant->features) - used,
                             "%s%s", used ? "," : "", mapped);
        if(wrote < 0 || (size_t)wrote >= sizeof(variant->features) - used) {
            variant->unsupported = "CPU feature list"; return;
        }
        used += (size_t)wrote;
    }
}

static int reserved(const RustVariant *variant, const char *reg)
{
    if(variant->x86) {
        if(strstr(reg, "sp") || strstr(reg, "bp")) return 1;
        if(variant->bits == 64 && (!strcmp(reg, "rbx") || !strcmp(reg, "ebx") ||
                                   !strcmp(reg, "bx") || !strcmp(reg, "bl") || !strcmp(reg, "bh"))) return 1;
        if(variant->bits == 32 && (!strcmp(reg, "esi") || !strcmp(reg, "si"))) return 1;
    }
    return 0;
}

static const char *fixed_x86(char letter, int width, int bits)
{
    if(width == 8) {
        switch(letter) { case 'a': return "al"; case 'b': return "bl";
            case 'c': return "cl"; case 'd': return "dl";
            case 'S': return bits == 64 ? "sil" : NULL;
            case 'D': return bits == 64 ? "dil" : NULL; }
    }
    if(width == 16) {
        switch(letter) { case 'a': return "ax"; case 'b': return "bx";
            case 'c': return "cx"; case 'd': return "dx"; case 'S': return "si"; case 'D': return "di"; }
    }
    if(width == 32) {
        switch(letter) { case 'a': return "eax"; case 'b': return "ebx";
            case 'c': return "ecx"; case 'd': return "edx"; case 'S': return "esi"; case 'D': return "edi"; }
    }
    if(width == 64 && bits == 64) {
        switch(letter) { case 'a': return "rax"; case 'b': return "rbx";
            case 'c': return "rcx"; case 'd': return "rdx"; case 'S': return "rsi"; case 'D': return "rdi"; }
    }
    return NULL;
}

static int tie(const AssemblyVariant *source, const char *constraint)
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

static int register_operand(RustVariant *variant, int index)
{
    RustOperand *operand = &variant->operands[index];
    const char *constraint = variant->source.operands[index].constraint;
    operand->tied_to = operand->tied_input = -1;
    operand->width = word_width(variant->source.operands[index].type, variant->bits);
    if(index >= variant->source.outputs) {
        int output = tie(&variant->source, constraint);
        if(output >= 0) {
            RustOperand *destination = &variant->operands[output];
            if(destination->memory || destination->tied_input >= 0 || destination->width != operand->width) return 0;
            if(strcmp(destination->type, operand->type) &&
               (destination->type[0] == 'f' || operand->type[0] == 'f')) return 0;
            destination->tied_input = index;
            operand->tied_to = output;
            return 1;
        }
    }
    while(*constraint && strchr("=&%!*?", *constraint)) {
        if(*constraint == '&') operand->early = 1;
        constraint++;
    }
    if(strchr(constraint, ',')) return 0;
    if(!strcmp(constraint, "m") || !strcmp(constraint, "o")) {
        operand->memory = 1;
        operand->width = variant->bits;
        operand->reg = "reg";
        return 1;
    }
    if(strpbrk(constraint, "rgX") || (variant->x86 && strchr(constraint, 'q'))) {
        operand->reg = variant->x86 && operand->width == 8 ? "reg_byte" : "reg";
        if(!strcmp(operand->reg, "reg") && operand->width > variant->bits) return 0;
        return 1;
    }
    if(variant->x86 && strlen(constraint) == 1) {
        const char *fixed = fixed_x86(*constraint, operand->width, variant->bits);
        if(fixed && !reserved(variant, fixed)) {
            snprintf(operand->fixed, sizeof(operand->fixed), "%s", fixed);
            operand->reg = operand->fixed;
            return 1;
        }
        if(*constraint == 'Q' && operand->width >= 16) operand->reg = "reg_abcd";
        if(*constraint == 'x' || *constraint == 'v') operand->reg = "xmm_reg";
        if(*constraint == 'y') return 0; /* Rust exposes MMX only as clobbers. */
    }
    if(!strcmp(variant->arch, "aarch64")) {
        if(!strcmp(constraint, "w")) operand->reg = "vreg";
        if(!strcmp(constraint, "x")) operand->reg = "vreg_low16";
    }
    if(!strcmp(variant->arch, "arm") && !strcmp(constraint, "t")) operand->reg = "sreg";
    if(!strncmp(variant->arch, "riscv", 5) && !strcmp(constraint, "f")) operand->reg = "freg";
    if(!strncmp(variant->arch, "powerpc", 7) && !strcmp(constraint, "f")) operand->reg = "freg";
    return operand->reg != NULL;
}

static void prepare(RustAssembly *assembly, RustVariant *variant)
{
    const char *plus = strchr(variant->source.target, '+');
    size_t length = plus ? (size_t)(plus - variant->source.target) : strlen(variant->source.target);
    if(length >= sizeof(variant->arch)) { variant->unsupported = "architecture spelling"; return; }
    memcpy(variant->arch, variant->source.target, length); variant->arch[length] = 0;
    variant->x86 = !strcmp(variant->arch, "x86") || !strcmp(variant->arch, "x86_64");
    variant->bits = strstr(variant->arch, "64") || !strcmp(variant->arch, "s390x") ? 64 : 32;
    features(variant);
    int count = variant->source.outputs + variant->source.inputs;
    for(int i = 0; i < count; i++) {
        char type[ZIR_NAME_MAX * 2];
        const char *source_type = variant->source.operands[i].type;
        assembly->target->type(assembly->target->context, source_type, type, sizeof(type));
        variant->operands[i].type = !strcmp(source_type, "bool") ? "u8" : KeepText(type);
        if(!register_operand(variant, i)) {
            variant->unsupported = "register constraint or tied operand width";
            return;
        }
        if(variant->operands[i].memory) variant->operands[i].type = KeepText(type);
    }
    variant->clobber_count = list(variant->source.clobbers, variant->clobbers);
    if(variant->clobber_count < 0) { variant->unsupported = "clobber list"; return; }
    for(int i = 0; i < variant->clobber_count; i++) {
        const char *name = variant->clobbers[i];
        if(!strcmp(name, "memory") || !strcmp(name, "cc")) continue;
        if(reserved(variant, name)) { variant->unsupported = "reserved Rust register"; return; }
    }
    if(!utf8(variant->source.text)) variant->unsupported = "non-UTF-8 assembler text";
}

static int operand_index(const AssemblyVariant *source, const char *name, size_t length)
{
    for(int i = 0; i < source->outputs + source->inputs; i++)
        if(strlen(source->operands[i].alias) == length && !strncmp(name, source->operands[i].alias, length)) return i;
    return -1;
}

static int modifier(const RustVariant *variant, int index, int code)
{
    const RustOperand *operand = &variant->operands[index];
    if(operand->tied_to >= 0) operand = &variant->operands[operand->tied_to];
    if(variant->x86) {
        if(!strcmp(operand->reg, "reg_byte")) return !code || code == 'b' ? 0 : -1;
        if(!strcmp(operand->reg, "xmm_reg")) return !code || code == 'x' ? 0 : -1;
        if(!code) return operand->width == 16 ? 'x' : operand->width == 32 ? 'e' : 'r';
        switch(code) { case 'b': return 'l'; case 'h': return !strcmp(operand->reg, "reg_abcd") ? 'h' : -1;
            case 'w': return 'x'; case 'k': return 'e'; case 'q': return 'r'; }
        return -1;
    }
    if(!strcmp(variant->arch, "aarch64")) {
        if(!strcmp(operand->reg, "reg")) return !code ? (operand->width <= 32 ? 'w' : 'x') :
            (code == 'w' || code == 'x' ? code : -1);
        if(!code) return operand->width == 8 ? 'b' : operand->width == 16 ? 'h' :
            operand->width == 32 ? 's' : 'd';
        return strchr("bhsdq", code) ? code : -1;
    }
    return code ? -1 : 0;
}

static int placeholder(FILE *out, const RustVariant *variant, int index, int code)
{
    if(index < 0 || index >= variant->source.outputs + variant->source.inputs) return 0;
    int root = variant->operands[index].tied_to >= 0 ? variant->operands[index].tied_to : index;
    const RustOperand *operand = &variant->operands[root];
    if(operand->memory) {
        if(code) return 0;
        if(variant->x86) fprintf(out, "({op%d:%c})", root, variant->bits == 64 ? 'r' : 'e');
        else if(!strcmp(variant->arch, "aarch64")) fprintf(out, "[{op%d:x}]", root);
        else if(!strcmp(variant->arch, "arm")) fprintf(out, "[{op%d}]", root);
        else if(!strncmp(variant->arch, "riscv", 5) || !strncmp(variant->arch, "loongarch", 9))
            fprintf(out, "0({op%d})", root);
        else return 0;
        return 1;
    }
    if(*operand->fixed) {
        const char *reg = operand->fixed;
        if(code) {
            char letter = strstr(reg, "si") ? 'S' : strstr(reg, "di") ? 'D' :
                strchr(reg, 'a') ? 'a' : strchr(reg, 'b') ? 'b' :
                strchr(reg, 'c') ? 'c' : strchr(reg, 'd') ? 'd' :
                0;
            int width = code == 'b' ? 8 : code == 'w' ? 16 : code == 'k' ? 32 : code == 'q' ? 64 : 0;
            reg = fixed_x86(letter, width, variant->bits);
            if(reg == NULL) return 0;
        }
        fprintf(out, "%%%s", reg);
        return 1;
    }
    int format = modifier(variant, index, code);
    if(format < 0) return 0;
    fprintf(out, "{op%d", root);
    if(format) fprintf(out, ":%c", format);
    fputc('}', out);
    return 1;
}

static int label_number(RustVariant *variant, const char *text, size_t length)
{
    while(length > 1 && *text == '0') { text++; length--; }
    for(size_t i = 0; i < variant->label_count; i++)
        if(variant->labels[i].length == length && !strncmp(variant->labels[i].text, text, length)) return (int)i + 20;
    if(variant->label_count == variant->label_capacity) {
        size_t capacity = variant->label_capacity ? variant->label_capacity * 2 : 16;
        NumericLabel *items = realloc(variant->labels, capacity * sizeof(*items));
        if(items == NULL) return -1;
        variant->labels = items; variant->label_capacity = capacity;
    }
    variant->labels[variant->label_count++] = (NumericLabel){text, length};
    return (int)variant->label_count + 19;
}

static int identifier(int byte) { return isalnum((unsigned char)byte) || byte == '_' || byte == '.'; }

static void translate(RustVariant *variant)
{
    if(variant->unsupported) return;
    size_t size = 0;
    FILE *out = open_memstream(&variant->text, &size);
    if(out == NULL) { variant->unsupported = "template allocation"; return; }
    const char *start = variant->source.text, *p = start;
    int quoted = 0, comment = 0, block_comment = 0, dialect = 0;
    while(*p) {
        if(*p == '%') {
            p++;
            if(*p == '%' || *p == '{' || *p == '}' || *p == '|') {
                if(*p == '{' || *p == '}') fputc(*p, out);
                fputc(*p++, out); continue;
            }
            if(*p == '=') { variant->unsupported = "GNU unique-label placeholder"; break; }
            int code = 0, index = -1;
            if(isalpha((unsigned char)*p)) code = *p++;
            if(*p == '[') {
                const char *name = ++p, *end = strchr(p, ']');
                if(!end) { variant->unsupported = "GNU operand placeholder"; break; }
                index = operand_index(&variant->source, name, (size_t)(end - name));
                p = end + 1;
            } else if(isdigit((unsigned char)*p)) {
                char *end;
                long value = strtol(p, &end, 10);
                if(value >= 0 && value < ASSEMBLY_OPERANDS) index = (int)value;
                p = end;
            }
            if(!placeholder(out, variant, index, code)) { variant->unsupported = "GNU operand modifier"; break; }
            continue;
        }
        if(!comment && !block_comment && variant->x86 && *p == '{') {
            const char *end = strchr(p + 1, '}');
            if(quoted || dialect || !end || memchr(p + 1, '{', (size_t)(end - p - 1))) {
                variant->unsupported = "assembler dialect alternative"; break;
            }
            if(end) {
                /* AT&T is dialect zero. Process its operands normally, then
                 * discard the Intel arm when the separator is reached. */
                dialect = 1; p++; continue;
            }
        }
        if(dialect && *p == '}') { dialect = 0; p++; continue; }
        if(dialect && *p == '|') {
            const char *end = strchr(p, '}');
            if(!end) { variant->unsupported = "assembler dialect alternative"; break; }
            dialect = 0; p = end + 1; continue;
        }
        if(!quoted && !comment && !block_comment && isdigit((unsigned char)*p) &&
           (p == start || !identifier(p[-1]))) {
            const char *end = p;
            while(isdigit((unsigned char)*end)) end++;
            if(*end == ':' || ((*end == 'f' || *end == 'b') && !identifier(end[1]))) {
                int number = label_number(variant, p, (size_t)(end - p));
                if(number < 0) { variant->unsupported = "numeric label allocation"; break; }
                fprintf(out, "%d", number); p = end;
                if(*p == 'f' || *p == 'b') fputc(*p++, out);
                continue;
            }
        }
        if(!comment && !block_comment && *p == '"') {
            size_t escapes = 0;
            for(const char *q = p; q > start && q[-1] == '\\'; q--) escapes++;
            if(!(escapes & 1)) quoted = !quoted;
        }
        if(!quoted && !comment && p[0] == '/' && p[1] == '*') block_comment = 1;
        else if(block_comment && p[0] == '*' && p[1] == '/') {
            fputs("*/", out); p += 2; block_comment = 0; continue;
        }
        if(!quoted && !block_comment && ((*p == '#' && strcmp(variant->arch, "aarch64")) ||
           (*p == '@' && !strcmp(variant->arch, "arm")) || (p[0] == '/' && p[1] == '/'))) comment = 1;
        if(*p == '\n') comment = 0;
        if(*p == '{' || *p == '}') fputc(*p, out);
        fputc(*p++, out);
    }
    if(!variant->unsupported) {
        fputs("\n/* operands:", out);
        for(int i = 0; i < variant->source.outputs + variant->source.inputs; i++)
            if(variant->operands[i].tied_to < 0 && !*variant->operands[i].fixed)
                if(!placeholder(out, variant, i, 0)) variant->unsupported = "register template format";
        fputs(" */", out);
    }
    if(fclose(out) != 0) variant->unsupported = "template output";
}

static void return_type(RustAssembly *assembly, const RustVariant *variant)
{
    FILE *out = assembly->out;
    if(assembly->statement < 0) {
        char type[ZIR_NAME_MAX * 2];
        assembly->target->type(assembly->target->context, assembly->function->return_type, type, sizeof(type));
        fputs(*type ? type : "()", out);
    } else {
        fputc('(', out);
        for(int i = 0; i < variant->source.outputs; i++) {
            if(variant->operands[i].memory) continue;
            char type[ZIR_NAME_MAX * 2];
            assembly->target->type(assembly->target->context, variant->source.operands[i].type, type, sizeof(type));
            fprintf(out, "%s%s,", i ? " " : "", type);
        }
        fputc(')', out);
    }
}

static void input_value(RustAssembly *assembly, const RustVariant *variant, int index)
{
    fprintf(assembly->out, "%sa%d", assembly->prefix, index);
    if(!strcmp(variant->source.operands[index].type, "bool")) fputs(" as u8", assembly->out);
}

static void result_value(RustAssembly *assembly, const RustVariant *variant, int index)
{
    if(variant->operands[index].memory) {
        fprintf(assembly->out, "unsafe { %sm%d.assume_init() }", assembly->prefix, index);
        return;
    }
    fprintf(assembly->out, "%so%d", assembly->prefix, index);
    if(!strcmp(variant->source.operands[index].type, "bool")) fputs(" != 0", assembly->out);
}

static void emit_native(RustAssembly *assembly, const RustVariant *variant)
{
    FILE *out = assembly->out;
    for(int i = 0; i < variant->source.outputs; i++) {
        if(variant->operands[i].memory) {
            if(assembly->statement < 0)
                line(assembly, "let mut %sm%d = core::mem::MaybeUninit::<%s>::uninit();",
                     assembly->prefix, i, variant->operands[i].type);
        } else line(assembly, "let %so%d: %s;", assembly->prefix, i, variant->operands[i].type);
    }
    line(assembly, "unsafe {");
    assembly->indent++;
    for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
    fputs("core::arch::asm!(", out); quote(out, variant->text); fputs(",\n", out);
    assembly->indent++;
    /* Named operands precede explicit registers in Rust. */
    for(int fixed = 0; fixed < 2; fixed++)
        for(int i = 0; i < variant->source.outputs + variant->source.inputs; i++) {
            const RustOperand *operand = &variant->operands[i];
            if(operand->tied_to >= 0 || !!*operand->fixed != fixed) continue;
            for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
            if(!fixed) fprintf(out, "op%d = ", i);
            int output = i < variant->source.outputs && !operand->memory;
            fputs(output ? (operand->tied_input >= 0 ? (operand->early ? "inout(" : "inlateout(") :
                           (operand->early ? "out(" : "lateout(")) : "in(", out);
            if(fixed) quote(out, operand->fixed);
            else fputs(operand->reg, out);
            fputs(") ", out);
            if(output) {
                if(operand->tied_input >= 0) {
                    fputc('(', out); input_value(assembly, variant, operand->tied_input);
                    fprintf(out, ") as %s => ", operand->type);
                }
                fprintf(out, "%so%d", assembly->prefix, i);
            } else if(operand->memory) {
                if(i < variant->source.outputs && assembly->statement < 0)
                    fprintf(out, "%sm%d.as_mut_ptr()", assembly->prefix, i);
                else fprintf(out, "%sa%d", assembly->prefix, i);
            } else input_value(assembly, variant, i);
            fputs(",\n", out);
        }
    for(int i = 0; i < variant->clobber_count; i++) {
        const char *name = variant->clobbers[i];
        if(!strcmp(name, "memory") || !strcmp(name, "cc")) continue;
        for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
        fputs("out(", out); quote(out, name); fputs(") _,\n", out);
    }
    /* Rust's nomem/pure/preserves_flags promises include synchronization and
     * floating-point status. Absence of a GNU memory/cc clobber does not prove
     * those stronger promises, so preserve Rust's conservative defaults. */
    if(variant->x86) line(assembly, "options(att_syntax),");
    assembly->indent--;
    line(assembly, ");");
    assembly->indent--;
    line(assembly, "}");
    for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
    if(assembly->statement >= 0) {
        fputc('(', out);
        for(int i = 0; i < variant->source.outputs; i++) {
            if(variant->operands[i].memory) continue;
            result_value(assembly, variant, i); fputs(", ", out);
        }
        fputs(")\n", out);
    } else if(variant->source.record) {
        char type[ZIR_NAME_MAX * 2];
        assembly->target->type(assembly->target->context, assembly->function->return_type, type, sizeof(type));
        fprintf(out, "%s { ", type);
        for(int i = 0; i < variant->source.outputs; i++) {
            char field[ZIR_NAME_MAX * 2];
            assembly->target->field(assembly->target->context, variant->source.operands[i].binding, field, sizeof(field));
            fprintf(out, "%s: ", field); result_value(assembly, variant, i); fputs(", ", out);
        }
        fputs("}\n", out);
    } else if(variant->source.outputs) {
        result_value(assembly, variant, 0); fputc('\n', out);
    } else fputs("()\n", out);
}

static void emit_helper(RustAssembly *assembly, const RustVariant *variant, int index)
{
    FILE *out = assembly->out;
    line(assembly, "#[cfg(target_arch = \"%s\")]", variant->arch);
    if(*variant->features) {
        line(assembly, "#[target_feature(enable = \"%s\")]", variant->features);
        line(assembly, "#[inline(never)]");
    } else line(assembly, "#[inline(always)]");
    for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
    fprintf(out, "%sfn %sv%d(", *variant->features ? "unsafe " : "", assembly->prefix, index);
    const char *separator = "";
    for(int i = 0; i < variant->source.outputs + variant->source.inputs; i++) {
        if(i < variant->source.outputs &&
           (assembly->statement < 0 || !variant->operands[i].memory)) continue;
        char type[ZIR_NAME_MAX * 2];
        assembly->target->type(assembly->target->context, variant->source.operands[i].type, type, sizeof(type));
        fprintf(out, "%s%sa%d: %s%s", separator, assembly->prefix, i,
                variant->operands[i].memory ? (i < variant->source.outputs ? "*mut " : "*const ") : "", type);
        separator = ", ";
    }
    fputs(") -> ", out); return_type(assembly, variant); fputs(" {\n", out);
    assembly->indent++;
    emit_native(assembly, variant);
    assembly->indent--;
    line(assembly, "}");
}

static void predicate(RustAssembly *assembly, const RustVariant *variant)
{
    const char *next = strchr(variant->source.target, '+'), *separator = "";
    if(!next) { fputs("true", assembly->out); return; }
    while(next) {
        const char *start = next + 1;
        next = strchr(start, '+');
        size_t length = next ? (size_t)(next - start) : strlen(start);
        char name[64];
        memcpy(name, start, length); name[length] = 0;
        fprintf(assembly->out, "%sstd::arch::is_%s_feature_detected!(\"%s\")", separator,
                variant->x86 ? "x86" : "aarch64", feature(variant->arch, name));
        separator = " && ";
    }
}

static int prefix_used(const ZirFunction *function, const char *prefix)
{
    const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
    size_t length = strlen(prefix);
    for(int i = 0; i < parameters->count; i++)
        if(!strncasecmp(parameters->items[i].name, prefix, length)) return 1;
    for(int i = 0; i < function->stmt_count; i++)
        if(!strncasecmp(function->stmts[i].name, prefix, length)) return 1;
    return 0;
}

int RustEmitAssembly(FILE *out, const ZirModule *module, const ZirFunction *function,
                     int statement, int indent, const RustAssemblyTarget *target)
{
    const char *cursor = statement < 0 ? function->assembly_text : function->stmts[statement].assembly_text;
    if(cursor == NULL || !*cursor) return 0;
    RustAssembly *assembly = calloc(1, sizeof(*assembly));
    if(assembly == NULL) exit(1);
    assembly->out = out; assembly->module = module; assembly->function = function;
    assembly->statement = statement; assembly->target = target; assembly->indent = indent;
    for(int serial = 0;; serial++) {
        snprintf(assembly->prefix, sizeof(assembly->prefix), "__zir_asm_%d_%d_", statement + 1, serial);
        if(!prefix_used(function, assembly->prefix)) break;
    }
    snprintf(assembly->selector, sizeof(assembly->selector), "%sSELECT", assembly->prefix);
    for(char *p = assembly->selector; *p; p++) *p = (char)toupper((unsigned char)*p);
    int status = 1;
    while(*skip_ws(cursor)) {
        RustVariant *variant = calloc(1, sizeof(*variant));
        if(variant == NULL) exit(1);
        if(AssemblyVariantNext(module, function, statement, &cursor, &variant->source) != 1) {
            free(variant); status = 0; break;
        }
        prepare(assembly, variant);
        translate(variant);
        if(variant->unsupported) {
            line(assembly, "// Rust uses the checked portable body for this variant: %s.", variant->unsupported);
            free(variant->labels); free(variant->text); free(variant); continue;
        }
        if(assembly->count == assembly->capacity) {
            int capacity = assembly->capacity ? assembly->capacity * 2 : 8;
            RustVariant **variants = realloc(assembly->variants, (size_t)capacity * sizeof(*variants));
            if(!variants) exit(1);
            assembly->variants = variants; assembly->capacity = capacity;
        }
        assembly->variants[assembly->count++] = variant;
    }
    if(!status || assembly->count == 0) goto done;
    if(statement >= 0) {
        line(assembly, "{"); assembly->indent++;
        line(assembly, "#[allow(unused_mut)]");
        line(assembly, "let mut %sused = false;", assembly->prefix);
    }
    line(assembly, "#[allow(unused_unsafe)]");
    line(assembly, "#[cfg(not(ziran_no_asm))]");
    line(assembly, "unsafe {"); assembly->indent++;
    for(int i = 0; i < assembly->count; i++) emit_helper(assembly, assembly->variants[i], i);
    int cache = 0;
    for(int i = 0; i < assembly->count; i++) cache |= *assembly->variants[i]->features != 0;
    if(cache) {
        line(assembly, "static %s: core::sync::atomic::AtomicUsize = core::sync::atomic::AtomicUsize::new(0);", assembly->selector);
        line(assembly, "let mut %schoice = %s.load(core::sync::atomic::Ordering::Relaxed);", assembly->prefix, assembly->selector);
        line(assembly, "if %schoice == 0 {", assembly->prefix); assembly->indent++;
        line(assembly, "%schoice = 1;", assembly->prefix);
        for(int i = 0; i < assembly->count; i++) {
            const RustVariant *variant = assembly->variants[i];
            line(assembly, "#[cfg(target_arch = \"%s\")]", variant->arch);
            for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
            fprintf(out, "if %schoice == 1 && (", assembly->prefix); predicate(assembly, variant);
            fprintf(out, ") { %schoice = %d; }\n", assembly->prefix, i + 2);
        }
        line(assembly, "%s.store(%schoice, core::sync::atomic::Ordering::Relaxed);", assembly->selector, assembly->prefix);
        assembly->indent--; line(assembly, "}");
    }
    for(int i = 0; i < assembly->count; i++) {
        const RustVariant *variant = assembly->variants[i];
        line(assembly, "#[cfg(target_arch = \"%s\")]", variant->arch);
        if(cache) line(assembly, "if %schoice == %d {", assembly->prefix, i + 2);
        else line(assembly, "if true {");
        assembly->indent++;
        if(statement >= 0) {
            for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
            fputs("let (", out);
            for(int j = 0; j < variant->source.outputs; j++)
                if(!variant->operands[j].memory) fprintf(out, "%sr%d, ", assembly->prefix, j);
            fputs(") = ", out);
        } else {
            for(int j = 0; j < assembly->indent; j++) fputs("    ", out);
            fputs("return ", out);
        }
        if(*variant->features) fputs("unsafe { ", out);
        fprintf(out, "%sv%d(", assembly->prefix, i);
        const char *separator = "";
        for(int j = 0; j < variant->source.outputs + variant->source.inputs; j++) {
            if(j < variant->source.outputs && (statement < 0 || !variant->operands[j].memory)) continue;
            char name[ZIR_NAME_MAX * 2];
            target->name(target->context, variant->source.operands[j].binding, name, sizeof(name));
            fputs(separator, out);
            if(variant->operands[j].memory)
                fprintf(out, "core::ptr::addr_of%s!(%s)", j < variant->source.outputs ? "_mut" : "", name);
            else fputs(name, out);
            separator = ", ";
        }
        fputc(')', out);
        if(*variant->features) fputs(" }", out);
        fputs(";\n", out);
        if(statement >= 0) {
            for(int j = 0; j < variant->source.outputs; j++) {
                if(variant->operands[j].memory) continue;
                char name[ZIR_NAME_MAX * 2];
                target->name(target->context, variant->source.operands[j].binding, name, sizeof(name));
                line(assembly, "%s = %sr%d;", name, assembly->prefix, j);
            }
            line(assembly, "%sused = true;", assembly->prefix);
        }
        assembly->indent--; line(assembly, "}");
    }
    assembly->indent--; line(assembly, "}");
    if(statement >= 0) line(assembly, "if !%sused {", assembly->prefix);
done:
    status = status && assembly->count > 0;
    for(int i = 0; i < assembly->count; i++) {
        free(assembly->variants[i]->labels); free(assembly->variants[i]->text); free(assembly->variants[i]);
    }
    free(assembly->variants);
    free(assembly);
    return status;
}
