/* Translate checked GNU operands into leaf Go ABI0 assembly. Only represented
 * instructions are emitted; an unsupported variant keeps its checked body.
 * Runtime/stack contract: https://go.dev/doc/asm#runtime-coordination */
#include "zir_go_amd64.h"
#include "zir_check.h"
#include "zir_emit.h"
#include "zir_scalar.h"
#include "zir_stream.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const registers[] = {
    "AX", "BX", "CX", "DX", "SI", "DI", "R8", "R9", "R10", "R11", "R12", "R13",
    "X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7", "X8", "X9", "X10", "X11", "X12", "X13", "X14"
};
enum { GP_REGISTERS = 12, GO_REGISTERS = sizeof(registers) / sizeof(*registers) };

static const char *const feature_names[] = {
    "popcnt", "sse2", "ssse3", "sse4.1", "sse4.2", "avx", "avx2",
    "avx512f", "bmi", "bmi2", "lzcnt", "fma", "aes", "pclmul"
};

typedef struct GoOperand {
    int width, reg, tied, floating, offset, storage;
} GoOperand;

typedef struct GoLabel {
    char *name;
    size_t position;
} GoLabel;

struct GoAmd64Plan {
    AssemblyVariant source;
    GoOperand operands[ASSEMBLY_OPERANDS];
    GoLabel *labels;
    int label_count, supported, arguments, available;
    uint64_t features;
    char *text, *normalized;
};

static int
width(const char *type)
{
    if(type[0] == '*') return 0;
    if(!strcmp(type, "bool")) return 8;
    if(!strcmp(type, "float32")) return 32;
    if(!strcmp(type, "float64") || !strcmp(type, "real")) return 64;
    if(!strcmp(type, "isize") || !strcmp(type, "usize")) return 64;
    int bits = (int)ScalarWidth(type);
    return bits == 8 || bits == 16 || bits == 32 || bits == 64 ? bits : 0;
}

static int
floating(const char *type)
{
    return !strcmp(type, "float32") || !strcmp(type, "float64") || !strcmp(type, "real");
}

static int
physical(const char *name, int *bits)
{
    static const char *const aliases[][4] = {
        {"al", "ax", "eax", "rax"}, {"bl", "bx", "ebx", "rbx"},
        {"cl", "cx", "ecx", "rcx"}, {"dl", "dx", "edx", "rdx"},
        {"sil", "si", "esi", "rsi"}, {"dil", "di", "edi", "rdi"},
        {"r8b", "r8w", "r8d", "r8"}, {"r9b", "r9w", "r9d", "r9"},
        {"r10b", "r10w", "r10d", "r10"}, {"r11b", "r11w", "r11d", "r11"},
        {"r12b", "r12w", "r12d", "r12"}, {"r13b", "r13w", "r13d", "r13"}
    };
    for(int r = 0; r < GP_REGISTERS; r++) for(int w = 0; w < 4; w++)
        if(!strcmp(name, aliases[r][w])) { *bits = 8 << w; return r; }
    for(int r = 0; r < 15; r++) {
        char xmm[16]; snprintf(xmm, sizeof(xmm), "xmm%d", r);
        if(!strcmp(name, xmm)) { *bits = 128; return GP_REGISTERS + r; }
    }
    return -1;
}

static int
feature_mask(const char *target, uint64_t *mask)
{
    const char *next = strchr(target, '+');
    size_t arch = next ? (size_t)(next - target) : strlen(target);
    if(arch != 6 || strncmp(target, "x86_64", 6)) return 0;
    *mask = 0;
    while(next) {
        const char *start = next + 1;
        next = strchr(start, '+');
        size_t length = next ? (size_t)(next - start) : strlen(start);
        int found = 0;
        for(size_t i = 0; i < sizeof(feature_names) / sizeof(*feature_names); i++)
            if(strlen(feature_names[i]) == length && !strncmp(start, feature_names[i], length)) {
                *mask |= UINT64_C(1) << i; found = 1; break;
            }
        if(!found) return 0;
    }
    return 1;
}

static int
allocate_operands(GoAmd64Plan *plan, int column)
{
    uint32_t excluded = 0, literal = 0, used = 0;
    const AssemblyVariant *source = &plan->source;
    char list[1024];
    if(strlen(source->clobbers) >= sizeof(list)) return 0;
    strcpy(list, source->clobbers);
    for(char *item = list; item && *item;) {
        char *end = strchr(item, ';');
        if(end) *end = 0;
        const char *name = trim_in_place(item);
        if(strcmp(name, "cc") && strcmp(name, "memory")) {
            int bits, reg = physical(name, &bits);
            if(reg < 0) return 0;
            excluded |= UINT32_C(1) << reg;
        }
        item = end ? end + 1 : NULL;
    }
    for(const char *at = plan->normalized; *at; at++) {
        if(at[0] != '%' || at[1] != '%') continue;
        at += 2;
        const char *start = at;
        while(isalnum((unsigned char)*at)) at++;
        size_t length = (size_t)(at - start);
        char name[16];
        if(length == 0 || length >= sizeof(name)) return 0;
        memcpy(name, start, length); name[length] = 0;
        int bits, reg = physical(name, &bits);
        if(reg < 0) return 0;
        literal |= UINT32_C(1) << reg;
        at--;
    }
    for(int i = 0; i < source->outputs + source->inputs; i++) {
        GoOperand *operand = &plan->operands[i];
        operand->reg = operand->tied = -1;
        const char *constraint = AssemblyConstraintAlternative(source->operands[i].constraint, column);
        while(*constraint && strchr("=&%!*?", *constraint)) constraint++;
        if(i >= source->outputs) {
            operand->tied = AssemblyTiedOutput(source, constraint);
            if(operand->tied >= 0) {
                GoOperand *output = &plan->operands[operand->tied];
                if(output->width != operand->width || output->floating != operand->floating) return 0;
                for(int prior = source->outputs; prior < i; prior++)
                    if(plan->operands[prior].tied == operand->tied) return 0;
                continue;
            }
        }
        int reg = -1;
        if(strlen(constraint) == 1) {
            const char *classes = "abcdSD";
            const char *fixed = *constraint ? strchr(classes, *constraint) : NULL;
            if(fixed) reg = (int)(fixed - classes);
        }
        if(reg >= 0) {
            if(operand->floating || ((excluded | used) & (UINT32_C(1) << reg))) return 0;
            operand->reg = reg; used |= UINT32_C(1) << reg;
        } else if(operand->floating ? strcmp(constraint, "x") && strcmp(constraint, "v") :
                   !strpbrk(constraint, "rgXqQ")) return 0;
        if(operand->storage) literal |= UINT32_C(1) << 11; /* input-load scratch R13 */
    }
    for(int i = 0; i < source->outputs + source->inputs; i++) {
        GoOperand *operand = &plan->operands[i];
        if(operand->tied >= 0) {
            operand->reg = plan->operands[operand->tied].reg;
            if(operand->reg < 0) return 0;
            continue;
        }
        if(operand->reg >= 0) continue;
        for(int reg = operand->floating ? GP_REGISTERS : 0;
            reg < (operand->floating ? GO_REGISTERS : GP_REGISTERS); reg++) {
            if((excluded | literal | used) & (UINT32_C(1) << reg)) continue;
            operand->reg = reg; used |= UINT32_C(1) << reg; break;
        }
        if(operand->reg < 0) return 0;
    }
    return 1;
}

/* Select AT&T text from GNU dialect braces before splitting instructions. */
static char *
dialect(const char *text)
{
    char *result = malloc(strlen(text) + 1);
    if(result == NULL) return NULL;
    size_t used = 0;
    for(const char *at = text; *at;) {
        if(*at == '#') { while(*at && *at != '\n') at++; continue; }
        if(at[0] == '/' && at[1] == '*') goto failed;
        if(*at == '{') {
            at++;
            while(*at && *at != '|' && *at != '}') {
                if(*at == '#') { while(*at && *at != '\n') at++; continue; }
                if(*at == '{') goto failed;
                result[used++] = *at++;
            }
            if(*at == '|') while(*at && *at != '}') { if(*at == '{') goto failed; at++; }
            if(*at != '}') goto failed;
            at++;
        } else {
            if(*at == '}') goto failed;
            result[used++] = *at++;
        }
    }
    result[used] = 0;
    return result;
failed:
    free(result); return NULL;
}

static int
label_token(const char *start, size_t length)
{
    if(length == 0) return 0;
    for(size_t i = 0; i < length; i++) {
        if(isalnum((unsigned char)start[i]) || start[i] == '.' || start[i] == '_') continue;
        if(start[i] == '%' && i + 1 < length && start[i + 1] == '=') { i++; continue; }
        return 0;
    }
    return 1;
}

static int
labels(GoAmd64Plan *plan, const char *text)
{
    for(const char *at = text; *at;) {
        while(isspace((unsigned char)*at) || *at == ';') at++;
        const char *begin = at, *end = at;
        while(*end && *end != ':' && !isspace((unsigned char)*end) && *end != ';') end++;
        const char *colon = skip_ws(end);
        if(*colon == ':' && label_token(begin, (size_t)(end - begin))) {
            if(plan->label_count >= 128) return 0;
            size_t length = (size_t)(end - begin);
            char *name = malloc(length + 1);
            if(name == NULL) return 0;
            GoLabel *items = realloc(plan->labels, (size_t)(plan->label_count + 1) * sizeof(*items));
            if(items == NULL) { free(name); return 0; }
            plan->labels = items;
            memcpy(name, begin, length); name[length] = 0;
            plan->labels[plan->label_count++] = (GoLabel){name, (size_t)(begin - text)};
            at = colon + 1;
        } else {
            while(*at && *at != '\n' && *at != ';') at++;
        }
    }
    return 1;
}

static int
label_reference(const GoAmd64Plan *plan, const char *name, size_t position)
{
    size_t length = strlen(name);
    int direction = length > 1 && isdigit((unsigned char)name[0]) ? name[length - 1] : 0;
    if(direction != 'b' && direction != 'f') direction = 0;
    if(!direction && isdigit((unsigned char)name[0])) return -1;
    int result = -1;
    for(int i = 0; i < plan->label_count; i++) {
        const GoLabel *label = &plan->labels[i];
        if(strlen(label->name) != length - (direction != 0) ||
           strncmp(label->name, name, length - (direction != 0))) continue;
        if(!direction) {
            if(result >= 0) return -1;
            result = i;
        } else if(direction == 'b' && label->position < position &&
                  (result < 0 || label->position > plan->labels[result].position)) result = i;
        else if(direction == 'f' && label->position > position &&
                  (result < 0 || label->position < plan->labels[result].position)) result = i;
    }
    return result;
}

typedef struct GoArgument {
    char text[128];
    int width, reg, immediate, memory;
} GoArgument;

static int
argument(const GoAmd64Plan *plan, const char *source, GoArgument *argument)
{
    *argument = (GoArgument){.reg = -1};
    if(source[0] == '$') {
        char *end;
        (void)strtoull(source + 1, &end, 0);
        if(end == source + 1 || *end || strlen(source) >= sizeof(argument->text)) return 0;
        strcpy(argument->text, source); argument->immediate = 1; return 1;
    }
    int reg = -1, bits = 0, high = 0;
    if(source[0] == '%' && source[1] == '%') reg = physical(source + 2, &bits);
    else if(source[0] == '%') {
        const char *at = source + 1;
        if(*at && strchr("bwhkq", *at)) {
            high = *at == 'h';
            bits = *at == 'b' || high ? 8 : *at == 'w' ? 16 : *at == 'k' ? 32 : 64;
            at++;
        }
        int index = -1;
        if(*at == '[') {
            size_t length = strlen(at);
            if(length < 3 || at[length - 1] != ']') return 0;
            for(int i = 0; i < plan->source.outputs + plan->source.inputs; i++)
                if(strlen(plan->source.operands[i].alias) == length - 2 &&
                   !strncmp(plan->source.operands[i].alias, at + 1, length - 2)) { index = i; break; }
        } else {
            char *end;
            long number = strtol(at, &end, 10);
            if(end == at || *end || number < 0 || number >= ASSEMBLY_OPERANDS) return 0;
            index = (int)number;
        }
        if(index < 0 || index >= plan->source.outputs + plan->source.inputs) return 0;
        reg = plan->operands[index].reg;
        if(!bits) bits = plan->operands[index].width;
    }
    if(reg < 0 || (high && reg >= 4)) return 0;
    snprintf(argument->text, sizeof(argument->text), "%s", registers[reg]);
    if(high) argument->text[1] = 'H';
    argument->reg = reg; argument->width = bits;
    return 1;
}

static const char *
condition(const char *name)
{
    static const struct { const char *gnu, *go; } conditions[] = {
        {"e","EQ"}, {"z","EQ"}, {"ne","NE"}, {"nz","NE"},
        {"l","LT"}, {"ge","GE"}, {"le","LE"}, {"g","GT"},
        {"b","CS"}, {"c","CS"}, {"nae","CS"}, {"ae","CC"},
        {"nb","CC"}, {"nc","CC"}, {"be","LS"}, {"na","LS"},
        {"a","HI"}, {"nbe","HI"}, {"s","MI"}, {"ns","PL"},
        {"o","OS"}, {"no","OC"}, {"p","PS"}, {"np","PC"}
    };
    for(size_t i = 0; i < sizeof(conditions) / sizeof(*conditions); i++)
        if(!strcmp(name, conditions[i].gnu)) return conditions[i].go;
    return NULL;
}

static int
arity(const char *name)
{
    static const char *const unary[] = {"inc","dec","neg","not","div","idiv","mul","bswap"};
    static const char *const binary[] = {"mov","add","sub","adc","sbb","and","or","xor","cmp","test","lea","imul","shl","shr","sar","rol","ror","popcnt","lzcnt","tzcnt","bsf","bsr","xchg"};
    for(size_t i = 0; i < sizeof(unary) / sizeof(*unary); i++) if(!strcmp(name, unary[i])) return 1;
    for(size_t i = 0; i < sizeof(binary) / sizeof(*binary); i++) if(!strcmp(name, binary[i])) return 2;
    return 0;
}

static int
instruction(GoAmd64Plan *plan, FILE *out, char *line, size_t position)
{
    char *at = line;
    while(*at && !isspace((unsigned char)*at)) at++;
    if((size_t)(at - line) >= 32) return 0;
    char mnemonic[32]; memcpy(mnemonic, line, (size_t)(at - line)); mnemonic[at - line] = 0;
    at = trim_in_place(at);
    if(mnemonic[0] == 'j') {
        const char *suffix = !strcmp(mnemonic, "jmp") ? "MP" : condition(mnemonic + 1);
        int label = label_reference(plan, at, position);
        if(suffix == NULL || label < 0) return 0;
        fprintf(out, "\tJ%s zlabel%d\n", suffix, label); return 1;
    }
    if(!strcmp(mnemonic, "nop") || !strcmp(mnemonic, "pause") ||
       !strcmp(mnemonic, "lfence") || !strcmp(mnemonic, "mfence") || !strcmp(mnemonic, "sfence")) {
        if(*at) return 0;
        for(char *p = mnemonic; *p; p++) *p = (char)toupper((unsigned char)*p);
        fprintf(out, "\t%s\n", mnemonic); return 1;
    }
    GoArgument args[3]; int count = 0;
    while(*at) {
        char *end = at; int nesting = 0;
        for(; *end; end++) {
            if(*end == '(') nesting++;
            if(*end == ')') nesting--;
            if(*end == ',' && nesting == 0) break;
        }
        if(count >= 3 || nesting != 0) return 0;
        int more = *end != 0;
        if(more) *end = 0;
        char *part = trim_in_place(at);
        if(!strncmp(mnemonic, "lea", 3) && count == 0) {
            char *open = strchr(part, '('), *close = strrchr(part, ')');
            if(open == NULL || close == NULL || close[1]) return 0;
            *open = *close = 0;
            char *tail;
            (void)strtol(part, &tail, 0);
            if(tail == part || *tail) return 0;
            GoArgument base;
            if(!argument(plan, open + 1, &base) || base.reg < 0 || base.reg >= GP_REGISTERS) return 0;
            args[count] = (GoArgument){.reg = -1, .width = 64, .memory = 1};
            if(snprintf(args[count].text, sizeof(args[count].text), "%s(%s)", part, base.text) >= (int)sizeof(args[count].text)) return 0;
        } else if(!argument(plan, part, &args[count])) return 0;
        count++;
        at = more ? trim_in_place(end + 1) : end;
    }
    char op[32];
    if(!strncmp(mnemonic, "set", 3)) {
        const char *suffix = condition(mnemonic + 3);
        if(suffix == NULL || count != 1 || args[0].reg < 0 || args[0].reg >= GP_REGISTERS) return 0;
        snprintf(op, sizeof(op), "SET%s", suffix);
    } else {
        static const struct { const char *gnu, *go; } special[] = {
            {"movzbl","MOVBLZX"}, {"movzbq","MOVBQZX"}, {"movzwl","MOVWLZX"}, {"movzwq","MOVWQZX"},
            {"movsbl","MOVBLSX"}, {"movsbq","MOVBQSX"}, {"movswl","MOVWLSX"}, {"movswq","MOVWQSX"}, {"movslq","MOVLQSX"},
            {"movss","MOVSS"}, {"movsd","MOVSD"}, {"addss","ADDSS"}, {"addsd","ADDSD"},
            {"subss","SUBSS"}, {"subsd","SUBSD"}, {"mulss","MULSS"}, {"mulsd","MULSD"},
            {"divss","DIVSS"}, {"divsd","DIVSD"}, {"sqrtss","SQRTSS"}, {"sqrtsd","SQRTSD"}
        };
        const char *mapped = NULL;
        for(size_t i = 0; i < sizeof(special) / sizeof(*special); i++)
            if(!strcmp(mnemonic, special[i].gnu)) mapped = special[i].go;
        if(mapped) {
            if(count != 2) return 0;
            snprintf(op, sizeof(op), "%s", mapped);
        } else {
            int bits = count ? args[count - 1].width : 0;
            size_t n = strlen(mnemonic);
            int operands = arity(mnemonic);
            if(!operands && n && strchr("bwlq", mnemonic[n - 1])) {
                bits = mnemonic[n - 1] == 'b' ? 8 : mnemonic[n - 1] == 'w' ? 16 : mnemonic[n - 1] == 'l' ? 32 : 64;
                mnemonic[--n] = 0;
                operands = arity(mnemonic);
            }
            if(!operands || count != operands || (bits != 8 && bits != 16 && bits != 32 && bits != 64)) return 0;
            if((!strcmp(mnemonic, "shl") || !strcmp(mnemonic, "shr") || !strcmp(mnemonic, "sar") ||
                !strcmp(mnemonic, "rol") || !strcmp(mnemonic, "ror")) && !args[0].immediate && args[0].reg != 2) return 0;
            if(!strcmp(mnemonic, "cmp")) { GoArgument swap = args[0]; args[0] = args[1]; args[1] = swap; }
            for(size_t i = 0; i < n; i++) op[i] = (char)toupper((unsigned char)mnemonic[i]);
            op[n++] = bits == 8 ? 'B' : bits == 16 ? 'W' : bits == 32 ? 'L' : 'Q'; op[n] = 0;
        }
    }
    fprintf(out, "\t%s ", op);
    for(int i = 0; i < count; i++) fprintf(out, "%s%s", i ? ", " : "", args[i].text);
    fputc('\n', out);
    return 1;
}

static int
translate(GoAmd64Plan *plan)
{
    char *text = strdup(plan->normalized);
    if(text == NULL || !labels(plan, text)) { free(text); return 0; }
    char *output = NULL; size_t length;
    TextStream stream;
    FILE *out = TextStreamOpen(&stream, &output, &length);
    if(out == NULL) { free(text); return 0; }
    int valid = 1;
    for(char *at = text; valid && *at;) {
        while(isspace((unsigned char)*at) || *at == ';') at++;
        if(!*at) break;
        if(*at == '#') { while(*at && *at != '\n') at++; continue; }
        char *end = at;
        while(*end && !isspace((unsigned char)*end) && *end != ':' && *end != ';') end++;
        char *colon = end;
        while(*colon == ' ' || *colon == '\t') colon++;
        if(*colon == ':' && label_token(at, (size_t)(end - at))) {
            int id = -1;
            for(int i = 0; i < plan->label_count; i++)
                if(plan->labels[i].position == (size_t)(at - text)) { id = i; break; }
            if(id < 0) { valid = 0; break; }
            fprintf(out, "zlabel%d:\n", id); at = colon + 1; continue;
        }
        end = at;
        while(*end && *end != '\n' && *end != ';' && *end != '#') end++;
        char saved = *end; *end = 0;
        valid = instruction(plan, out, trim_in_place(at), (size_t)(at - text));
        *end = saved;
        at = end;
        if(*at == '#') while(*at && *at != '\n') at++;
    }
    if(TextStreamClose(&stream) != 0) valid = 0;
    free(text);
    if(valid) plan->text = output;
    else free(output);
    return valid;
}

GoAmd64Plan *
GoAmd64Prepare(const AssemblyVariant *source, int block, int by_value, const unsigned char *storage)
{
    uint64_t features;
    if(!feature_mask(source->target, &features)) return NULL;
    GoAmd64Plan *plan = calloc(1, sizeof(*plan));
    if(plan == NULL) return NULL;
    plan->source = *source; plan->features = features;
    if(block && !by_value) return plan;
    int bytes = 0;
    for(int i = 0; i < source->outputs + source->inputs; i++) {
        GoOperand *operand = &plan->operands[i];
        operand->width = width(source->operands[i].type);
        operand->floating = floating(source->operands[i].type);
        operand->storage = block && i >= source->outputs && storage[i];
        if(!operand->width) return plan;
    }
    for(int i = source->outputs; i < source->outputs + source->inputs; i++) {
        GoOperand *operand = &plan->operands[i];
        int size = operand->storage ? 8 : operand->width / 8;
        bytes = (bytes + size - 1) & -size;
        operand->offset = bytes; bytes += size;
    }
    bytes = (bytes + 7) & -8;
    for(int i = 0; i < source->outputs; i++) {
        GoOperand *operand = &plan->operands[i];
        int size = operand->width / 8;
        bytes = (bytes + size - 1) & -size;
        operand->offset = bytes; bytes += size;
    }
    plan->available = bytes++;
    plan->arguments = bytes;
    plan->normalized = dialect(source->text);
    if(plan->normalized == NULL) return plan;
    int alternatives = AssemblyAlternativeCount(source);
    for(int column = 0; column < alternatives; column++) {
        if(allocate_operands(plan, column) && translate(plan)) { plan->supported = 1; break; }
        for(int i = 0; i < plan->label_count; i++) free(plan->labels[i].name);
        free(plan->labels); plan->labels = NULL; plan->label_count = 0;
    }
    return plan;
}

int GoAmd64Supported(const GoAmd64Plan *plan) { return plan->supported; }
uint64_t GoAmd64Features(const GoAmd64Plan *plan) { return plan->features; }

void
GoAmd64Write(FILE *go, FILE *assembly, const GoAmd64Plan *plan, const char *name)
{
    const AssemblyVariant *source = &plan->source;
    fputs("//go:noescape\nfunc ", go); fputs(name, go); fputc('(', go);
    for(int i = source->outputs; i < source->outputs + source->inputs; i++)
        fprintf(go, "%sa%d %s%s", i > source->outputs ? ", " : "", i,
            plan->operands[i].storage ? "*" : "", TargetType(ScalarType(source->operands[i].type), ZIR_GO));
    fputs(") (", go);
    for(int i = 0; i < source->outputs; i++)
        fprintf(go, "r%d %s, ", i, TargetType(ScalarType(source->operands[i].type), ZIR_GO));
    fputs("ok bool)\n\n", go);
    fprintf(assembly, "TEXT ·%s(SB), NOSPLIT, $0-%d\n", name, plan->arguments);
    for(int i = source->outputs; i < source->outputs + source->inputs; i++) {
        const GoOperand *operand = &plan->operands[i];
        char input[64];
        snprintf(input, sizeof(input), "a%d+%d(FP)", i, operand->offset);
        if(operand->storage) {
            fprintf(assembly, "\tMOVQ %s, R13\n", input);
            strcpy(input, "0(R13)");
        }
        const char *load = operand->floating ? operand->width == 32 ? "MOVSS" : "MOVSD" :
            operand->width == 8 ? "MOVBQZX" : operand->width == 16 ? "MOVWQZX" :
            operand->width == 32 ? "MOVL" : "MOVQ";
        fprintf(assembly, "\t%s %s, %s\n", load, input, registers[operand->reg]);
    }
    fputs(plan->text, assembly);
    for(int i = 0; i < source->outputs; i++) {
        const GoOperand *operand = &plan->operands[i];
        if(!strcmp(source->operands[i].type, "bool"))
            fprintf(assembly, "\tTESTB %s, %s\n\tSETNE %s\n", registers[operand->reg], registers[operand->reg], registers[operand->reg]);
        const char *store = operand->floating ? operand->width == 32 ? "MOVSS" : "MOVSD" :
            operand->width == 8 ? "MOVB" : operand->width == 16 ? "MOVW" :
            operand->width == 32 ? "MOVL" : "MOVQ";
        fprintf(assembly, "\t%s %s, r%d+%d(FP)\n", store, registers[operand->reg], i, operand->offset);
    }
    fprintf(assembly, "\tMOVB $1, ok+%d(FP)\n\tRET\n\n", plan->available);
}

void
GoAmd64Cpu(FILE *go, FILE *assembly, const char *prefix)
{
    fprintf(go, "func %sCpuid(leaf, sub uint32) (ra, rb, rc, rd uint32)\nfunc %sXgetbv() uint64\n", prefix, prefix);
    fprintf(go, "var %sFeatures = func() uint64 {\n"
        "    var flags uint64\n    maximum, _, _, _ := %sCpuid(0, 0)\n"
        "    var avx bool\n    var xcr uint64\n    if maximum >= 1 {\n"
        "        _, _, c, d := %sCpuid(1, 0)\n"
        "        for _, feature := range []struct{bit uint; mask uint64}{{23,1<<0},{9,1<<2},{19,1<<3},{20,1<<4},{25,1<<12},{1,1<<13}} { if c&(1<<feature.bit)!=0 {flags |= feature.mask} }\n"
        "        if d&(1<<26)!=0 {flags |= 1<<1}\n"
        "        if c&(1<<26|1<<27)==1<<26|1<<27 {xcr=%sXgetbv()}\n"
        "        avx = c&(1<<28)!=0 && xcr&6==6\n"
        "        if avx {flags |= 1<<5; if c&(1<<12)!=0 {flags |= 1<<11}}\n    }\n"
        "    if maximum >= 7 {\n        _, b, _, _ := %sCpuid(7, 0)\n"
        "        if b&(1<<3)!=0 {flags |= 1<<8}; if b&(1<<8)!=0 {flags |= 1<<9}\n"
        "        if avx && b&(1<<5)!=0 {flags |= 1<<6}\n"
        "        if avx && xcr&0xe6==0xe6 && b&(1<<16)!=0 {flags |= 1<<7}\n    }\n"
        "    maximum, _, _, _ = %sCpuid(0x80000000, 0)\n"
        "    if maximum >= 0x80000001 {_, _, c, _ := %sCpuid(0x80000001, 0); if c&(1<<5)!=0 {flags |= 1<<10}}\n"
        "    return flags\n}()\n\n", prefix, prefix, prefix, prefix, prefix, prefix, prefix);
    fprintf(assembly, "TEXT ·%sCpuid(SB), NOSPLIT, $0-24\n"
        "\tMOVL leaf+0(FP), AX\n\tMOVL sub+4(FP), CX\n\tCPUID\n"
        "\tMOVL AX, ra+8(FP)\n\tMOVL BX, rb+12(FP)\n\tMOVL CX, rc+16(FP)\n\tMOVL DX, rd+20(FP)\n\tRET\n\n"
        "TEXT ·%sXgetbv(SB), NOSPLIT, $0-8\n\tXORL CX, CX\n\tXGETBV\n"
        "\tSHLQ $32, DX\n\tORQ DX, AX\n\tMOVQ AX, ret+0(FP)\n\tRET\n\n", prefix, prefix);
}

void
GoAmd64Close(GoAmd64Plan *plan)
{
    if(plan == NULL) return;
    for(int i = 0; i < plan->label_count; i++) free(plan->labels[i].name);
    free(plan->labels); free(plan->text); free(plan->normalized); free(plan);
}
