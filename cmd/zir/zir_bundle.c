#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_law.h"
#include "zir_proof.h"
#include "zir_packages.h"
#include "zir_diagnostic.h"
#include "zir_emit.h"
#include "zir_serial.h"
#include "zir_text.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { ZIB_VERSION = 25, ZIB_MAX_IR_BYTES = 256 * 1024 * 1024,
       ZIB_MAX_CAPABILITIES = 4096 };

typedef struct CapabilityName {
    char module[ZIR_NAME_MAX];
    char function[ZIR_NAME_MAX];
} CapabilityName;

static int
split_binding_name(const char *text, char *module, char *function)
{
    const char *separator = text ? strrchr(text, ':') : NULL;
    size_t length;
    if(separator == NULL || separator == text || separator[1] == '\0')
        return 0;
    length = (size_t)(separator - text);
    if(length >= ZIR_NAME_MAX || strlen(separator + 1) >= ZIR_NAME_MAX)
        return 0;
    memcpy(module, text, length);
    module[length] = '\0';
    strcpy(function, separator + 1);
    return 1;
}

/* A binding may name a module by its import path, such as
 * kryon/raster_text:RasterText, when a project supplies the package map. */
static int
resolve_binding_module(char *module)
{
    if(strchr(module, '/') == NULL) return 1;
    const char *path = getenv("ZIRAN_PACKAGE_MAP");
    if(path == NULL || path[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "host binding %s names a package module; run it with --project",
                   module);
        return 0;
    }
    ZirPackageMap *map = PackageMapLoad(path);
    char identity[ZIR_NAME_MAX];
    int found = map != NULL &&
        PackageDependencyModule(map, "root", module, identity, sizeof(identity));
    PackageMapFree(map);
    if(!found) {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "host binding names %s, which no direct dependency has", module);
        return 0;
    }
    snprintf(module, ZIR_NAME_MAX, "%s", identity);
    return 1;
}

int
BindHostProvider(ZirProgram *program, const char *spec)
{
    const char *equals = strchr(spec, '=');
    char source[2 * ZIR_NAME_MAX], provider[2 * ZIR_NAME_MAX];
    char source_module[ZIR_NAME_MAX], source_function[ZIR_NAME_MAX];
    char provider_module[ZIR_NAME_MAX], provider_function[ZIR_NAME_MAX];
    ZirImport *matched = NULL;
    const ZirFunction *implementation = NULL;
    if(equals == NULL || (size_t)(equals - spec) >= sizeof(source) ||
       strlen(equals + 1) >= sizeof(provider)) {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "invalid host binding: %s", spec);
        return 0;
    }
    memcpy(source, spec, (size_t)(equals - spec));
    source[equals - spec] = '\0';
    strcpy(provider, equals + 1);
    if(!split_binding_name(source, source_module, source_function) ||
       !split_binding_name(provider, provider_module, provider_function)) {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "invalid host binding: %s", spec);
        return 0;
    }
    if(!resolve_binding_module(source_module) ||
       !resolve_binding_module(provider_module))
        return 0;
    for(int m = 0; m < program->module_count; m++) {
        ZirModule *module = &program->modules[m];
        if(strcmp(module->name, source_module) == 0)
            for(int i = 0; i < module->import_count; i++)
                if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
                   module->imports[i].extern_kind == ZIR_EXTERN_HOST &&
                   strcmp(module->imports[i].name, source_function) == 0) {
                    if(matched != NULL) {
                        Diagnostic(module->imports[i].span, "host.bind",
                                   "ambiguous host capability: %s", source);
                        return 0;
                    }
                    matched = &module->imports[i];
                }
        if(strcmp(module->name, provider_module) == 0)
            for(int i = 0; i < module->function_count; i++)
                if(strcmp(module->functions[i].name, provider_function) == 0 &&
                   module->functions[i].exported &&
                   !module->functions[i].is_extern) {
                    if(implementation != NULL) {
                        Diagnostic(module->functions[i].span, "host.bind",
                                   "ambiguous Ziran provider: %s", provider);
                        return 0;
                    }
                    implementation = &module->functions[i];
                }
    }
    if(matched == NULL || implementation == NULL ||
       strncmp(matched->target, "ziran:", 6) == 0) {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "host capability or exported Ziran provider is missing or already bound: %s",
                   spec);
        return 0;
    }
    snprintf(matched->target, sizeof(matched->target), "ziran:%s",
             provider_module);
    snprintf(matched->extern_symbol, sizeof(matched->extern_symbol), "%s",
             provider_function);
    return 1;
}

/* Binds every host capability no --bind has claimed to the exported Ziran
 * function of the same name in provider_module, as a C program links a
 * host's exported symbols by name. One Ziran host module, such as Kryon's
 * pixmap backend, then serves every target. */
int
BindHostModule(ZirProgram *program, const char *spec)
{
    char provider_module[ZIR_NAME_MAX];
    const ZirModule *provider = NULL;
    if(strlen(spec) >= sizeof(provider_module) || spec[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "invalid host provider module: %s", spec);
        return 0;
    }
    strcpy(provider_module, spec);
    if(!resolve_binding_module(provider_module))
        return 0;
    for(int m = 0; m < program->module_count; m++)
        if(strcmp(program->modules[m].name, provider_module) == 0)
            provider = &program->modules[m];
    if(provider == NULL) {
        Diagnostic(Span("<command>", 1, 1), "host.bind",
                   "host provider module %s is not in the program", spec);
        return 0;
    }
    for(int m = 0; m < program->module_count; m++) {
        ZirModule *module = &program->modules[m];
        for(int i = 0; i < module->import_count; i++) {
            ZirImport *import = &module->imports[i];
            if(import->kind != ZIR_IMPORT_EXTERN ||
               import->extern_kind != ZIR_EXTERN_HOST ||
               strncmp(import->target, "ziran:", 6) == 0)
                continue;
            for(int f = 0; f < provider->function_count; f++) {
                const ZirFunction *function = &provider->functions[f];
                if(function->exported && !function->is_extern &&
                   strcmp(function->name, import->name) == 0) {
                    snprintf(import->target, sizeof(import->target), "ziran:%s",
                             provider_module);
                    snprintf(import->extern_symbol, sizeof(import->extern_symbol),
                             "%s", function->name);
                    break;
                }
            }
        }
    }
    return 1;
}

static int
module_name_order(const void *left, const void *right)
{
    const ZirModule *a = left;
    const ZirModule *b = right;
    return strcmp(a->name, b->name);
}

/* An extern kept only so a law stays decidable demands no host binding:
 * capabilities list imports the linked code actually calls. */
static int
import_is_called(const ZirModule *module, const ZirImport *import)
{
    for(int f = 0; f < module->function_count; f++) {
        const ZirFunction *fn = &module->functions[f];
        for(int e = 0; e < fn->expr_count; e++)
            if(fn->exprs[e].kind == ZIR_EXPR_CALL &&
               fn->exprs[e].slot_type[0] == '\0' &&
               strcmp(fn->exprs[e].name, import->name) == 0)
                return 1;
    }
    return 0;
}

static int
count_capabilities(const ZirProgram *program)
{
    int count = 0;
    for(int m = 0; m < program->module_count; m++)
        for(int i = 0; i < program->modules[m].import_count; i++)
            count += program->modules[m].imports[i].kind == ZIR_IMPORT_EXTERN &&
                     (program->modules[m].imports[i].extern_kind != ZIR_EXTERN_HOST ||
                      strncmp(program->modules[m].imports[i].target,
                              "ziran:", 6) != 0) &&
                     import_is_called(&program->modules[m],
                                      &program->modules[m].imports[i]);
    return count;
}

static int
write_u32(FILE *out, uint32_t value)
{
    for(int i = 0; i < 4; i++) {
        if(fputc((int)(value & 255u), out) == EOF)
            return 0;
        value >>= 8;
    }
    return 1;
}

static int
read_u32(FILE *in, uint32_t *value)
{
    uint32_t result = 0;
    for(int i = 0; i < 4; i++) {
        int byte = fgetc(in);
        if(byte == EOF)
            return 0;
        result |= (uint32_t)(unsigned char)byte << (8 * i);
    }
    *value = result;
    return 1;
}

static int
write_name(FILE *out, const char *name)
{
    size_t length = strnlen(name, ZIR_NAME_MAX);
    return length > 0 && length < ZIR_NAME_MAX &&
           write_u32(out, (uint32_t)length) &&
           fwrite(name, 1, length, out) == length;
}

static int
read_name(FILE *in, char *name, size_t capacity)
{
    uint32_t length;
    if(!read_u32(in, &length) || length == 0 || length >= capacity)
        return 0;
    if(fread(name, 1, length, in) != length || memchr(name, 0, length))
        return 0;
    name[length] = 0;
    return 1;
}
static int write_text(FILE *out, const char *text, size_t capacity)
{
    size_t length = strnlen(text, capacity);
    return length < capacity && write_u32(out, (uint32_t)length) &&
           fwrite(text, 1, length, out) == length;
}
static int read_text(FILE *in, char *text, size_t capacity)
{
    uint32_t length;
    if(!read_u32(in, &length) || length >= capacity ||
       fread(text, 1, length, in) != length || memchr(text, 0, length)) return 0;
    text[length] = 0;
    return 1;
}
/* Buffers copy_bytes keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CopyBytesBuffers {
    unsigned char buffer[8192];
} CopyBytesBuffers;

static int copy_bytes(FILE *in, FILE *out, uint32_t count);

static int
copy_bytes_with_buffers(FILE *in, FILE *out, uint32_t count, CopyBytesBuffers *buffers)
{
    while(count > 0) {
        size_t amount = count < sizeof(buffers->buffer) ? count : sizeof(buffers->buffer);
        if(fread(buffers->buffer, 1, amount, in) != amount ||
           fwrite(buffers->buffer, 1, amount, out) != amount)
            return 0;
        count -= (uint32_t)amount;
    }
    return 1;
}

static int
copy_bytes(FILE *in, FILE *out, uint32_t count)
{
    static _Thread_local CopyBytesBuffers *spares[16];
    static _Thread_local int spare_count;
    CopyBytesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = copy_bytes_with_buffers(in, out, count, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
copy_function(ZirFunction *target, const ZirFunction *source)
{
    *target = *source;
    target->stmts = NULL;
    target->exprs = NULL;
    if(source->stmt_count > 0) {
        target->stmts = malloc((size_t)source->stmt_count * sizeof(*target->stmts));
        if(target->stmts == NULL)
            return 0;
        memcpy(target->stmts, source->stmts,
               (size_t)source->stmt_count * sizeof(*target->stmts));
    }
    if(source->expr_count > 0) {
        target->exprs = malloc((size_t)source->expr_count * sizeof(*target->exprs));
        if(target->exprs == NULL)
            return 0;
        memcpy(target->exprs, source->exprs,
               (size_t)source->expr_count * sizeof(*target->exprs));
    }
    return 1;
}

/* The entry linker works on a private checked graph. Its source may also be
 * used for a different entry, so pruning must never change that graph. */
static int
copy_laws(ZirModule *to, const ZirModule *from)
{
    if(from->law_count) {
        to->laws = calloc((size_t)from->law_count, sizeof(*to->laws));
        if(!to->laws) return 0;
        to->law_count = to->law_cap = from->law_count;
        for(int l = 0; l < from->law_count; l++) {
            to->laws[l] = from->laws[l];
            to->laws[l].claim = NULL; to->laws[l].proof = NULL;
            if(from->laws[l].claim) {
                to->laws[l].claim = calloc(1, sizeof(ZirFunction));
                if(!to->laws[l].claim || !copy_function(to->laws[l].claim, from->laws[l].claim)) return 0;
            }
        }
    }
    if(from->law_waiver_count) {
        to->law_waivers = malloc((size_t)from->law_waiver_count * sizeof(*to->law_waivers));
        if(!to->law_waivers) return 0;
        memcpy(to->law_waivers, from->law_waivers, (size_t)from->law_waiver_count * sizeof(*to->law_waivers));
        to->law_waiver_count = to->law_waiver_cap = from->law_waiver_count;
    }
    if(from->proof_count) {
        to->proofs = calloc((size_t)from->proof_count, sizeof(*to->proofs));
        if(!to->proofs) return 0;
        to->proof_count = to->proof_cap = from->proof_count;
        for(int p = 0; p < from->proof_count; p++) {
            ZirProof *proof = &to->proofs[p];
            *proof = from->proofs[p]; proof->steps = NULL; proof->owner = NULL;
            memset(&proof->terms, 0, sizeof(proof->terms));
            if(!copy_function(&proof->terms, &from->proofs[p].terms)) return 0;
            if(proof->step_count) {
                proof->steps = malloc((size_t)proof->step_count * sizeof(*proof->steps));
                if(!proof->steps) return 0;
                memcpy(proof->steps, from->proofs[p].steps, (size_t)proof->step_count * sizeof(*proof->steps));
            }
        }
    }
    return 1;
}

static ZirProgram *
copy_program(const ZirProgram *source)
{
    ZirProgram *result = ProgramNew();
    if(result == NULL)
        return NULL;
    result->modules = calloc((size_t)(source->module_count ?
                             source->module_count : 1), sizeof(*result->modules));
    if(result->modules == NULL)
        goto failed;
    result->module_count = result->module_cap = source->module_count;
    for(int m = 0; m < source->module_count; m++) {
        const ZirModule *from = &source->modules[m];
        ZirModule *to = &result->modules[m];
        *to = *from;
        to->globals = NULL; to->global_count = to->global_cap = 0;
        to->defines = NULL; to->define_count = to->define_cap = 0;
        to->asserts = NULL; to->assert_count = to->assert_cap = 0;
        to->usings = NULL; to->using_count = to->using_cap = 0;
        to->types = NULL; to->type_count = to->type_cap = 0;
        to->imports = NULL; to->import_count = to->import_cap = 0;
        TypeLookupsChanged();
        to->functions = NULL; to->function_count = to->function_cap = 0;
        to->laws = NULL; to->law_count = to->law_cap = 0;
        to->law_waivers = NULL; to->law_waiver_count = to->law_waiver_cap = 0;
        to->proofs = NULL; to->proof_count = to->proof_cap = 0;
        if(!copy_laws(to, from)) goto failed;
#define COPY_DECLARATIONS(member, count, capacity) do { \
        if(from->count > 0) { \
            to->member = malloc((size_t)from->count * sizeof(*to->member)); \
            if(to->member == NULL) goto failed; \
            memcpy(to->member, from->member, \
                   (size_t)from->count * sizeof(*to->member)); \
            to->count = to->capacity = from->count; \
        } \
    } while(0)
        COPY_DECLARATIONS(globals, global_count, global_cap);
        COPY_DECLARATIONS(defines, define_count, define_cap);
        COPY_DECLARATIONS(asserts, assert_count, assert_cap);
        COPY_DECLARATIONS(types, type_count, type_cap);
        COPY_DECLARATIONS(imports, import_count, import_cap);
#undef COPY_DECLARATIONS
        if(from->function_count > 0) {
            to->functions = calloc((size_t)from->function_count,
                                   sizeof(*to->functions));
            if(to->functions == NULL)
                goto failed;
            to->function_count = to->function_cap = from->function_count;
            for(int f = 0; f < from->function_count; f++)
                if(!copy_function(&to->functions[f], &from->functions[f]))
                    goto failed;
        }
    }
    if(!LinkImports(&result, 1))
        goto failed;
    if(!CheckLawGates(&result, 1)) goto failed;
    return result;
failed:
    ProgramFree(result);
    return NULL;
}

/* Dead-code pruning follows statements and conditions as deep as the VM
 * runs them; deeper code is reported, never dropped silently. */
enum { LIVE_NESTING_MAX = 128 };

/* A known result here also means evaluating the expression has no effect.
 * In particular, `false && Call()` is known; `Call() && false` is not. */
static int
constant_bool(const ZirFunction *fn, int index, int depth)
{
    if(index < 0 || index >= fn->expr_count || depth > LIVE_NESTING_MAX)
        return -1;
    const ZirExpr *expr = &fn->exprs[index];
    if(expr->kind == ZIR_EXPR_IDENT) {
        if(!strcmp(expr->name, "true")) return 1;
        if(!strcmp(expr->name, "false")) return 0;
    }
    if(expr->kind == ZIR_EXPR_UNARY && !strcmp(expr->op, "!")) {
        int value = constant_bool(fn, expr->right, depth + 1);
        return value < 0 ? -1 : !value;
    }
    if(expr->kind == ZIR_EXPR_BINARY &&
       (!strcmp(expr->op, "&&") || !strcmp(expr->op, "||"))) {
        int left = constant_bool(fn, expr->left, depth + 1);
        if(left < 0) return -1;
        if(!strcmp(expr->op, "&&") && !left) return 0;
        if(!strcmp(expr->op, "||") && left) return 1;
        return constant_bool(fn, expr->right, depth + 1);
    }
    if(expr->kind == ZIR_EXPR_CONDITIONAL) {
        int condition = constant_bool(fn, expr->left, depth + 1);
        if(condition >= 0)
            return constant_bool(fn, condition ? expr->right : expr->third,
                                 depth + 1);
    }
    return -1;
}

static int
statement_close(const ZirFunction *fn, int begin, int end)
{
    int depth = 1;
    for(int i = begin + 1; i < end; i++) {
        ZirStmtKind kind = fn->stmts[i].kind;
        if(kind == ZIR_STMT_IF || kind == ZIR_STMT_WHILE ||
           kind == ZIR_STMT_BLOCK_OPEN)
            depth++;
        else if(kind == ZIR_STMT_BLOCK_CLOSE && --depth == 0)
            return i;
    }
    return -1;
}

static int
append_statement(ZirFunction *to, const ZirStmt *statement)
{
    if(to->stmt_count >= to->stmt_cap)
        return 0;
    to->stmts[to->stmt_count++] = *statement;
    return 1;
}

static int
copy_live_sequence(const ZirFunction *from, ZirFunction *to,
                   int begin, int end, int depth, int *stops)
{
    if(depth > LIVE_NESTING_MAX) {
        Diagnostic(from->span, "zib.statement",
                   "%s nests blocks deeper than %d levels", from->name,
                   LIVE_NESTING_MAX);
        return 0;
    }
    *stops = 0;
    for(int i = begin; i < end; i++) {
        const ZirStmt *statement = &from->stmts[i];
        if(statement->kind == ZIR_STMT_IF && !statement->is_else) {
            int branch = i, unknown = 0, chosen = 0;
            int all_branches_stop = 1;
            do {
                const ZirStmt *head = &from->stmts[branch];
                int close = statement_close(from, branch, end);
                if(close < 0) return 0;
                int known = head->expr_root < 0 ? 1 :
                    constant_bool(from, head->expr_root, 0);
                if(!chosen && known != 0) {
                    int branch_stops = 0;
                    ZirStmt live = *head;
                    if(known == 1 && !unknown) {
                        live.kind = ZIR_STMT_BLOCK_OPEN;
                        live.expr_root = -1;
                        live.is_else = 0;
                    } else {
                        live.is_else = unknown;
                        if(known == 1) live.expr_root = -1;
                    }
                    if(!append_statement(to, &live) ||
                       !copy_live_sequence(from, to, branch + 1, close,
                                           depth + 1, &branch_stops) ||
                       !append_statement(to, &from->stmts[close]))
                        return 0;
                    all_branches_stop &= branch_stops;
                    if(known == 1) chosen = 1;
                    else unknown = 1;
                }
                branch = close + 1;
            } while(branch < end && from->stmts[branch].kind == ZIR_STMT_IF &&
                    from->stmts[branch].is_else);
            i = branch - 1;
            if(chosen && all_branches_stop) {
                *stops = 1;
                break;
            }
        } else if(statement->kind == ZIR_STMT_WHILE ||
                  statement->kind == ZIR_STMT_BLOCK_OPEN) {
            int close = statement_close(from, i, end);
            if(close < 0) return 0;
            if(statement->kind != ZIR_STMT_WHILE ||
               constant_bool(from, statement->expr_root, 0) != 0) {
                int body_stops = 0;
                if(!append_statement(to, statement) ||
                   !copy_live_sequence(from, to, i + 1, close, depth + 1,
                                       &body_stops) ||
                   !append_statement(to, &from->stmts[close]))
                    return 0;
                if(statement->kind == ZIR_STMT_BLOCK_OPEN && body_stops) {
                    *stops = 1;
                    break;
                }
            }
            i = close;
        } else {
            if(!append_statement(to, statement)) return 0;
            if(statement->kind == ZIR_STMT_RETURN ||
               statement->kind == ZIR_STMT_UNREACHABLE ||
               statement->kind == ZIR_STMT_BREAK ||
               statement->kind == ZIR_STMT_CONTINUE) {
                *stops = 1;
                break;
            }
        }
    }
    return 1;
}

static int
copy_live_expression(const ZirFunction *from, ZirFunction *to,
                     int *mapping, int source, int depth)
{
    if(source < 0) return -1;
    if(source >= from->expr_count || depth > 128) return -2;
    if(mapping[source] >= 0) return mapping[source];
    const ZirExpr *expr = &from->exprs[source];
    if(expr->kind == ZIR_EXPR_CONDITIONAL) {
        int known = constant_bool(from, expr->left, 0);
        if(known >= 0) {
            int selected = known ? expr->right : expr->third;
            int result = copy_live_expression(from, to, mapping, selected,
                                              depth + 1);
            if(result >= 0) {
                /* A conditional can be a named or positional call argument.
                 * The selected arm inherits that call-site binding when the
                 * enclosing expression disappears during pruning. */
                to->exprs[result].argument_index = expr->argument_index;
                to->exprs[result].argument_name = expr->argument_name;
                if(expr->argument_index >= 0 &&
                   (!strcmp(to->exprs[result].type, "integer") ||
                    !strcmp(to->exprs[result].type, "real")) &&
                   strcmp(expr->type, "integer") != 0 &&
                   strcmp(expr->type, "real") != 0)
                    to->exprs[result].type = KeepName(expr->type);
            }
            mapping[source] = result;
            return result;
        }
    }
    if(expr->kind == ZIR_EXPR_BINARY &&
       (!strcmp(expr->op, "&&") || !strcmp(expr->op, "||"))) {
        int left = constant_bool(from, expr->left, 0);
        if(left >= 0) {
            if((!strcmp(expr->op, "&&") && left) ||
               (!strcmp(expr->op, "||") && !left)) {
                int result = copy_live_expression(from, to, mapping,
                                                  expr->right, depth + 1);
                if(result >= 0) {
                    to->exprs[result].argument_index = expr->argument_index;
                    to->exprs[result].argument_name = expr->argument_name;
                }
                mapping[source] = result;
                return result;
            }
            /* A short circuit has no run-time evaluation. */
            int result = to->expr_count++;
            if(result >= to->expr_cap) return -2;
            to->exprs[result] = *expr;
            to->exprs[result].kind = ZIR_EXPR_IDENT;
            to->exprs[result].name = KeepName(left ? "true" : "false");
            to->exprs[result].left = to->exprs[result].right =
                to->exprs[result].third = to->exprs[result].first_child =
                to->exprs[result].next_sibling = -1;
            mapping[source] = result;
            return result;
        }
    }
    int children[] = {expr->left, expr->right, expr->third};
    int copied_children[3];
    for(int i = 0; i < 3; i++) {
        copied_children[i] = copy_live_expression(from, to, mapping,
                                                  children[i], depth + 1);
        if(copied_children[i] < -1) return -2;
    }
    int first_child = -1, last_child = -1;
    for(int child = expr->first_child; child >= 0;
        child = from->exprs[child].next_sibling) {
        int copied = copy_live_expression(from, to, mapping, child, depth + 1);
        if(copied < 0) return -2;
        if(first_child < 0) first_child = copied;
        if(last_child >= 0) to->exprs[last_child].next_sibling = copied;
        to->exprs[copied].next_sibling = -1;
        last_child = copied;
    }
    int result = to->expr_count++;
    if(result >= to->expr_cap) return -2;
    to->exprs[result] = *expr;
    mapping[source] = result;
    to->exprs[result].left = copied_children[0];
    to->exprs[result].right = copied_children[1];
    to->exprs[result].third = copied_children[2];
    to->exprs[result].first_child = first_child;
    to->exprs[result].next_sibling = -1;
    return result;
}
/* Buffers prune_function keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct PruneFunctionBuffers {
    ZirFunction original;
    ZirFunction output;
} PruneFunctionBuffers;

static int prune_function(ZirFunction *fn);

static int
prune_function_with_buffers(ZirFunction *fn, PruneFunctionBuffers *buffers)
{
    buffers->original = *fn;
    buffers->output = buffers->original;
    buffers->output.stmts = calloc((size_t)(buffers->original.stmt_count ? buffers->original.stmt_count : 1),
                          sizeof(*buffers->output.stmts));
    buffers->output.exprs = calloc((size_t)(buffers->original.expr_count ? buffers->original.expr_count : 1),
                          sizeof(*buffers->output.exprs));
    int *mapping = malloc((size_t)(buffers->original.expr_count ? buffers->original.expr_count : 1) *
                          sizeof(*mapping));
    if(buffers->output.stmts == NULL || buffers->output.exprs == NULL || mapping == NULL) {
        free(buffers->output.stmts); free(buffers->output.exprs); free(mapping);
        return 0;
    }
    buffers->output.stmt_count = buffers->output.expr_count = 0;
    buffers->output.stmt_cap = buffers->original.stmt_count;
    buffers->output.expr_cap = buffers->original.expr_count;
    for(int i = 0; i < buffers->original.expr_count; i++) mapping[i] = -1;
    int stops = 0;
    int ok = copy_live_sequence(&buffers->original, &buffers->output, 0, buffers->original.stmt_count,
                                0, &stops);
    for(int s = 0; ok && s < buffers->output.stmt_count; s++) {
        ZirStmt *statement = &buffers->output.stmts[s];
        statement->expr_root = copy_live_expression(&buffers->original, &buffers->output,
            mapping, statement->expr_root, 0);
        statement->lhs_root = copy_live_expression(&buffers->original, &buffers->output,
            mapping, statement->lhs_root, 0);
        ok = statement->expr_root >= -1 && statement->lhs_root >= -1;
    }
    free(mapping);
    if(!ok) {
        free(buffers->output.stmts); free(buffers->output.exprs);
        return 0;
    }
    free(buffers->original.stmts); free(buffers->original.exprs);
    *fn = buffers->output;
    return 1;
}

static int
prune_function(ZirFunction *fn)
{
    static _Thread_local PruneFunctionBuffers *spares[16];
    static _Thread_local int spare_count;
    PruneFunctionBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = prune_function_with_buffers(fn, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

typedef struct FieldUse {
    ZirType *type;
    unsigned char *fields;
    int count;
    int shared; /* fields belong to an earlier copy of the same application */
} FieldUse;

static FieldUse *
find_field_use(FieldUse *uses, int count, const ZirType *type)
{
    if(type == NULL)
        return NULL;
    for(int i = 0; i < count; i++)
        if(uses[i].type == type)
            return &uses[i];
    return NULL;
}

/* Mirrors the VM's scalar-union rule for linked bundles. */
static int
bundle_portable_union(const ZirModule *module, const ZirType *record)
{
    size_t offset = 0;
    ZirTypeField field;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        const char *backing = field.type;
        const ZirType *enumeration = FindType(module, field.type, NULL);
        if(enumeration != NULL && enumeration->is_enum)
            backing = enumeration->enum_backing;
        if(strcmp(backing, "s8") && strcmp(backing, "u8") &&
           strcmp(backing, "bool") && strcmp(backing, "s16") &&
           strcmp(backing, "u16") && strcmp(backing, "s32") &&
           strcmp(backing, "u32") && strcmp(backing, "float32") &&
           strcmp(backing, "s64") && strcmp(backing, "u64") &&
           strcmp(backing, "float64"))
            return 0;
    }
    return status == 0;
}

/* Laws keep the extern imports they name so their status stays decidable
 * inside a linked bundle. */
static int
law_names_import(const ZirModule *module, const ZirImport *import)
{
    for(int l = 0; l < module->law_count; l++)
        if((!strcmp(module->laws[l].kind, "effect") ||
            !strcmp(module->laws[l].kind, "abi")) &&
           strcmp(module->laws[l].payload, import->name) == 0)
            return 1;
    return 0;
}

static int
builtin_call(const char *name)
{
    return MapPrimitiveName(name) || !strcmp(name, "TextView") ||
           !strcmp(name, "VecPush") || !strcmp(name, "VecClear") ||
           !strcmp(name, "VecFree") || !strcmp(name, "VecSwap") ||
           !strcmp(name, "VecPop") || !strcmp(name, "VecGet") ||
           !strcmp(name, "VecClone") || !strcmp(name, "VecSlice") ||
           !strcmp(name, "BuilderAppend") || !strcmp(name, "BuilderFinish") ||
           !strcmp(name, "print") || !strcmp(name, "zi_new") ||
           !strcmp(name, "zi_free");
}

static const char *
record_name(const char *type)
{
    while(*type == '*' || *type == ' ')
        type++;
    return type;
}

static void
mark_all_fields(const ZirModule *module, const char *name,
                FieldUse *uses, int use_count)
{
    char element[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    if(ArrayElementType(name, element, sizeof(element), NULL) ||
       SliceElementType(name, element, sizeof(element))) {
        mark_all_fields(module, element, uses, use_count);
        return;
    }
    const ZirType *type = FindType(module, record_name(name), &owner);
    FieldUse *use = find_field_use(uses, use_count, type);
    if(use == NULL)
        return;
    size_t offset = 0;
    ZirTypeField field;
    int index = 0;
    while(TypeNextField(type, &offset, &field) == 1 && index < use->count) {
        if(!use->fields[index]) {
            use->fields[index] = 1;
            mark_all_fields(owner, field.type, uses, use_count);
        }
        index++;
    }
}

static void
mark_member_field(const ZirModule *module, const char *record_type,
                  const char *field_name, FieldUse *uses, int use_count)
{
    const ZirModule *owner = NULL;
    const ZirType *type = FindType(module, record_name(record_type), &owner);
    FieldUse *use = find_field_use(uses, use_count, type);
    if(use == NULL)
        return;
    const char *dot = strchr(field_name, '.');
    size_t name_length = dot == NULL ? strlen(field_name) :
                         (size_t)(dot - field_name);
    size_t offset = 0;
    ZirTypeField field;
    int index = 0;
    while(TypeNextField(type, &offset, &field) == 1 && index < use->count) {
        if(strlen(field.name) == name_length &&
           strncmp(field.name, field_name, name_length) == 0) {
            use->fields[index] = 1;
            if(dot != NULL && field.is_using)
                mark_member_field(owner, field.type, dot + 1,
                                  uses, use_count);
            return;
        }
        index++;
    }
}

static void
mark_signature_fields(const ZirModule *module, const char *args,
                      const char *result, FieldUse *uses, int use_count)
{
    mark_all_fields(module, result, uses, use_count);
    if(args == NULL || args[0] == '\0')
        return;
    char (*parts)[ZIR_TEXT_MAX] = calloc(64, sizeof(*parts));
    if(parts == NULL)
        return;
    int count = split_top_level(args, parts[0], 64, sizeof(parts[0]));
    for(int i = 0; i < count; i++) {
        const char *colon = strchr(parts[i], ':');
        if(colon != NULL)
            mark_all_fields(module, colon + 1, uses, use_count);
    }
    free(parts);
}
/* Buffers prune_record_fields keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct PruneRecordFieldsBuffers {
    char body[sizeof(((ZirType *)0)->body)];
} PruneRecordFieldsBuffers;

static int prune_record_fields(ZirProgram *program, const char *entry_module,
                    const char *entry_function);

static int
prune_record_fields_with_buffers(ZirProgram *program, const char *entry_module,
                    const char *entry_function, PruneRecordFieldsBuffers *buffers)
{
    int use_count = 0;
    for(int m = 0; m < program->module_count; m++)
        use_count += program->modules[m].type_count;
    FieldUse *uses = calloc((size_t)(use_count ? use_count : 1), sizeof(*uses));
    if(uses == NULL)
        return 0;
    const ZirModule **owners = calloc((size_t)(use_count ? use_count : 1),
                                      sizeof(*owners));
    if(owners == NULL) {
        free(uses);
        return 0;
    }
    int next = 0;
    for(int m = 0; m < program->module_count; m++) {
        ZirModule *module = &program->modules[m];
        for(int t = 0; t < module->type_count; t++) {
            ZirType *type = &module->types[t];
            FieldUse *use = &uses[next];
            owners[next++] = module;
            if(type->is_enum || type->is_union || type->is_procedure_type ||
               type->is_extern || type->is_record_template || type->is_map)
                continue;
            use->type = type;
            size_t offset = 0;
            ZirTypeField field;
            while(TypeNextField(type, &offset, &field) == 1)
                use->count++;
            /* Each module that spells Table(string, s32) holds its own copy
             * of that one type; every copy keeps the fields any of them use. */
            for(int earlier = 0; earlier < next - 1 && type->is_synthetic_application;
                earlier++)
                if(uses[earlier].type != NULL && !uses[earlier].shared &&
                   same_type_application(owners[earlier], uses[earlier].type,
                                         module, type)) {
                    use->fields = uses[earlier].fields;
                    use->shared = 1;
                    break;
                }
            if(use->count > 0 && !use->shared) {
                use->fields = calloc((size_t)use->count, 1);
                if(use->fields == NULL)
                    goto failed;
            }
        }
    }
    for(int m = 0; m < program->module_count; m++) {
        ZirModule *module = &program->modules[m];
        for(int t = 0; t < module->type_count; t++)
            if(VecElementType(module, module->types[t].name, NULL, 0))
                mark_all_fields(module, module->types[t].name,
                                uses, use_count);
        /* A native callback's records cross the foreign ABI even when the
         * only ordinary extern parameter is the callback or its containing
         * record. Preserve their complete layout, including unused fields. */
        for(int t = 0; t < module->type_count; t++) {
            const ZirType *type = &module->types[t];
            if(type->is_procedure_type && type->is_c_call)
                mark_signature_fields(module, type->body,
                    type->procedure_return_type, uses, use_count);
        }
        for(int i = 0; i < module->import_count; i++)
            if(module->imports[i].kind == ZIR_IMPORT_EXTERN)
                mark_signature_fields(module, module->imports[i].args,
                    module->imports[i].return_type, uses, use_count);
        for(int l = 0; l < module->law_count; l++) {
            const ZirLaw *law = &module->laws[l];
            if(!strcmp(law->kind, "size") || !strcmp(law->kind, "type")) {
                const char *ops[] = {"==", ">=", "<="};
                const char *op = NULL;
                char name[ZIR_NAME_MAX];
                size_t length;
                for(int o = 0; o < 3 && op == NULL; o++)
                    op = strstr(law->payload, ops[o]);
                length = op != NULL ? (size_t)(op - law->payload) :
                                      strlen(law->payload);
                if(length == 0 || length >= sizeof(name))
                    continue;
                memcpy(name, law->payload, length);
                name[length] = '\0';
                trim_in_place(name);
                mark_all_fields(module, name, uses, use_count);
            }
        }
        for(int f = 0; f < module->function_count; f++) {
            ZirFunction *fn = &module->functions[f];
            if(strcmp(module->name, entry_module) == 0 &&
               strcmp(fn->name, entry_function) == 0)
                mark_signature_fields(module, FunctionArgs(fn), fn->return_type,
                                      uses, use_count);
            for(int e = 0; e < fn->expr_count; e++) {
                ZirExpr *expr = &fn->exprs[e];
                if((expr->kind == ZIR_EXPR_MEMBER ||
                    expr->kind == ZIR_EXPR_POINTER_MEMBER) &&
                   expr->left >= 0 && expr->left < fn->expr_count)
                    mark_member_field(module, fn->exprs[expr->left].type,
                                      expr->name, uses, use_count);
                else if(expr->kind == ZIR_EXPR_COMPOUND)
                    mark_all_fields(module, expr->name, uses, use_count);
                else if(expr->kind == ZIR_EXPR_CALL &&
                        (!strcmp(expr->name, "VecPop") ||
                         !strcmp(expr->name, "VecGet")))
                    mark_all_fields(module, expr->type, uses, use_count);
                else if(expr->kind == ZIR_EXPR_SIZE_OF &&
                        expr->name[0])
                    mark_all_fields(module, expr->name, uses, use_count);
            }
        }
    }
    for(int i = 0; i < use_count; i++) {
        FieldUse *use = &uses[i];
        if(use->type == NULL || use->count == 0)
            continue;
        buffers->body[0] = '\0';
        size_t length = 0, offset = 0;
        ZirTypeField field;
        int index = 0;
        while(TypeNextField(use->type, &offset, &field) == 1) {
            if(use->fields[index]) {
                int written = snprintf(buffers->body + length, sizeof(buffers->body) - length,
                                       "%s%s: %s\n",
                                       field.is_using ? "using " : "",
                                       field.name, field.type);
                if(written < 0 || (size_t)written >= sizeof(buffers->body) - length)
                    goto failed;
                length += (size_t)written;
            }
            index++;
        }
        if(length == 0)
            strcpy(buffers->body, "_unused: u8\n");
        strcpy(use->type->body, buffers->body);
    }
    for(int i = 0; i < use_count; i++)
        if(!uses[i].shared)
            free(uses[i].fields);
    free(owners);
    free(uses);
    return 1;
failed:
    for(int i = 0; i < use_count; i++)
        if(!uses[i].shared)
            free(uses[i].fields);
    free(owners);
    free(uses);
    return 0;
}

/* Entry links are closed programs. A field unused by their checked graph
 * need not occupy every element of a large retained array. Host signatures
 * and the entry ABI keep their complete layouts. */
static int
prune_record_fields(ZirProgram *program, const char *entry_module,
                    const char *entry_function)
{
    static _Thread_local PruneRecordFieldsBuffers *spares[16];
    static _Thread_local int spare_count;
    PruneRecordFieldsBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = prune_record_fields_with_buffers(program, entry_module, entry_function, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
mark_type_pointer(const ZirProgram *program, const ZirModule *owner,
                  const ZirType *type, unsigned char **keep_types,
                  int *changed)
{
    for(int m = 0; m < program->module_count; m++) {
        if(owner != &program->modules[m])
            continue;
        for(int t = 0; t < owner->type_count; t++)
            if(type == &owner->types[t]) {
                if(!keep_types[m][t]) {
                    keep_types[m][t] = 1;
                    *changed = 1;
                }
                return 1;
            }
    }
    return 0;
}

static int
mark_type(const ZirProgram *program, const ZirModule *scope,
          const char *name, unsigned char **keep_types, int *changed)
{
    char element[ZIR_NAME_MAX];
    while(*name == '*') name++;
    if(SliceElementType(name, element, sizeof(element)))
        return mark_type(program, scope, element, keep_types, changed);
    if(ArrayElementType(name, element, sizeof(element), NULL))
        return mark_type(program, scope, element, keep_types, changed);
    const ZirModule *owner = NULL;
    const ZirType *type = FindType(scope, name, &owner);
    if(type != NULL && type == BuiltinType(name))
        return 1;
    return type == NULL ||
           mark_type_pointer(program, owner, type, keep_types, changed);
}

static int
mark_parameters(const ZirProgram *program, const ZirModule *module,
                const ZirFunction *function, unsigned char **keep_types,
                int *changed)
{
    const char *cursor = FunctionArgs(function);
    while(*cursor) {
        const char *colon = strchr(cursor, ':');
        if(colon == NULL)
            return 0;
        cursor = colon + 1;
        while(isspace((unsigned char)*cursor))
            cursor++;
        char type[ZIR_NAME_MAX];
        size_t length = 0;
        while(*cursor == '*' && length + 1 < sizeof(type))
            type[length++] = *cursor++;
        if(*cursor == '[') {
            do {
                if(length + 1 >= sizeof(type))
                    return 0;
                type[length++] = *cursor++;
            } while(*cursor && type[length - 1] != ']');
            if(type[length - 1] != ']')
                return 0;
        }
        while((isalnum((unsigned char)*cursor) || *cursor == '_' || *cursor == '.') &&
              length + 1 < sizeof(type))
            type[length++] = *cursor++;
        type[length] = 0;
        if(length == 0 || !mark_type(program, module, type,
                                      keep_types, changed))
            return 0;
        cursor = strchr(cursor, ',');
        if(cursor == NULL)
            break;
        cursor++;
    }
    return 1;
}

static void mark_proof_function(const ZirProgram *program, const ZirModule *scope,
                                const char *name, unsigned char **keep)
{
    const ZirModule *owner = NULL; const ZirFunction *fn = NULL;
    if(ResolveFunction(scope, name, &owner, &fn) != 1) return;
    for(int m = 0; m < program->module_count; m++)
        if(owner == &program->modules[m]) keep[m][fn - owner->functions] = 1;
}
static int mark_proof_graph(const ZirProgram *program, const ZirModule *scope,
                            const ZirFunction *graph, unsigned char **keep,
                            unsigned char **keep_types)
{
    int changed = 0;
    if(!mark_parameters(program, scope, graph, keep_types, &changed)) return 0;
    for(int e = 0; e < graph->expr_count; e++) {
        const ZirExpr *expr = &graph->exprs[e];
        if(expr->kind == ZIR_EXPR_CAST &&
           !mark_type(program, scope, expr->name, keep_types, &changed)) return 0;
        if(expr->kind != ZIR_EXPR_CALL) continue;
        char name[ZIR_NAME_MAX];
        copy_text(name, sizeof(name), expr->name);
        if(!name[0] && expr->left >= 0 && expr->left < graph->expr_count) {
            const ZirExpr *member = &graph->exprs[expr->left];
            if(member->kind != ZIR_EXPR_MEMBER || member->left < 0 || member->left >= graph->expr_count ||
               graph->exprs[member->left].kind != ZIR_EXPR_IDENT) continue;
            if(snprintf(name, sizeof(name), "%s.%s", graph->exprs[member->left].name,
                        member->name) >= (int)sizeof(name)) continue;
        }
        mark_proof_function(program, scope, name, keep);
    }
    return 1;
}

static int
mentions_identifier(const char *text, const char *name)
{
    size_t length = strlen(name);
    for(const char *match = strstr(text, name); match != NULL;
        match = strstr(match + 1, name)) {
        unsigned char before = match == text ? 0 : (unsigned char)match[-1];
        unsigned char after = (unsigned char)match[length];
        if((before == 0 || (!isalnum(before) && before != '_')) &&
           (after == 0 || (!isalnum(after) && after != '_')))
            return 1;
    }
    return 0;
}

static int
global_is_used(const ZirProgram *program, unsigned char **keep,
               const ZirModule *owner, const ZirGlobal *global)
{
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int f = 0; f < module->function_count; f++) {
            if(!keep[m][f]) continue;
            const ZirFunction *function = &module->functions[f];
            for(int e = 0; e < function->expr_count; e++) {
                const ZirExpr *expression = &function->exprs[e];
                const ZirModule *resolved_owner = NULL;
                const ZirGlobal *resolved_global = NULL;
                if(expression->kind == ZIR_EXPR_IDENT &&
                   ResolveGlobalAt(module, expression->name,
                                   SpanPath(expression->span), &resolved_owner,
                                   &resolved_global) == 1 &&
                   resolved_owner == owner && resolved_global == global)
                    return 1;
            }
        }
    }
    return 0;
}

static int
uses_constant_name(const ZirProgram *program, unsigned char **keep,
                   const ZirModule *module,
                   const unsigned char *keep_types,
                   const unsigned char *keep_defines, const char *name)
{
    const unsigned char *retained = keep[module - program->modules];
    for(int f = 0; f < module->function_count; f++) {
        if(!retained[f])
            continue;
        for(int s = 0; s < module->functions[f].stmt_count; s++)
            if(mentions_identifier(module->functions[f].stmts[s].text, name))
                return 1;
        for(int e = 0; e < module->functions[f].expr_count; e++)
            if(mentions_identifier(module->functions[f].exprs[e].text, name))
                return 1;
    }
    for(int t = 0; t < module->type_count; t++)
        if(keep_types[t] &&
           mentions_identifier(module->types[t].body, name))
            return 1;
    for(int g = 0; g < module->global_count; g++)
        if(global_is_used(program, keep, module, &module->globals[g]) &&
           (mentions_identifier(module->globals[g].type, name) ||
            mentions_identifier(module->globals[g].init, name)))
            return 1;
    for(int d = 0; d < module->define_count; d++)
        if(keep_defines[d] &&
           mentions_identifier(module->defines[d].value, name))
            return 1;
    return 0;
}

static int
uses_imported_constant(const ZirModule *module, const unsigned char *keep,
                       const unsigned char *keep_types,
                       const unsigned char *keep_defines,
                       const unsigned char *dependency_defines,
                       const ZirModule *dependency)
{
    for(int d = 0; d < dependency->define_count; d++) {
        if(!dependency_defines[d] || !dependency->defines[d].is_public)
            continue;
        const char *name = dependency->defines[d].name;
        for(int f = 0; f < module->function_count; f++)
            if(keep[f])
                for(int s = 0; s < module->functions[f].stmt_count; s++)
                    if(mentions_identifier(module->functions[f].stmts[s].text,
                                           name))
                        return 1;
        for(int t = 0; t < module->type_count; t++)
            if(keep_types[t] &&
               mentions_identifier(module->types[t].body, name))
                return 1;
        for(int local = 0; local < module->define_count; local++)
            if(keep_defines[local] &&
               mentions_identifier(module->defines[local].value, name))
                return 1;
    }
    return 0;
}

/* True when `via` is `target` or reaches it through `using` imports, the
 * way a public module entry re-exports the modules it names. */
static int
reexports_module(const ZirModule *via, const ZirModule *target, int depth)
{
    if(via == NULL || depth > 32) return 0;
    if(via == target) return 1;
    for(int i = 0; i < via->import_count; i++)
        if(via->imports[i].is_using &&
           reexports_module(via->imports[i].resolved_module, target, depth + 1))
            return 1;
    return 0;
}

/* True when a module reached through `via` (itself or a re-export) keeps a
 * declaration, so an importer can still resolve it through `via`. */
static int
reexports_kept(const ZirProgram *program, const ZirModule *via,
               unsigned char **keep_all, unsigned char **keep_types)
{
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *candidate = &program->modules[m];
        if(candidate == via || !reexports_module(via, candidate, 0))
            continue;
        if(memchr(keep_all[m], 1, (size_t)candidate->function_count) != NULL ||
           memchr(keep_types[m], 1, (size_t)candidate->type_count) != NULL)
            return 1;
    }
    return 0;
}

static int
import_is_used(const ZirProgram *program, const ZirModule *module,
               const unsigned char *keep, unsigned char **keep_all,
               unsigned char **keep_types, unsigned char **keep_defines,
               const ZirImport *import)
{
    if(import->kind == ZIR_IMPORT_EXTERN) {
        for(int f = 0; f < module->function_count; f++)
            if(keep[f])
                for(int e = 0; e < module->functions[f].expr_count; e++)
                    if(module->functions[f].exprs[e].kind == ZIR_EXPR_CALL &&
                       strcmp(module->functions[f].exprs[e].name,
                              import->name) == 0)
                        return 1;
        return 0;
    }
    if((import->kind != ZIR_IMPORT_OPEN &&
        import->kind != ZIR_IMPORT_MODULE) ||
       import->resolved_module == NULL)
        return 0;
    if(reexports_kept(program, import->resolved_module, keep_all, keep_types))
        return 1;
    /* A module kept only for a program export that a foreign call binds
     * to is still reached through this import when output is reloaded. */
    for(int m = 0; m < program->module_count; m++) {
        if(import->resolved_module != &program->modules[m])
            continue;
        for(int f = 0; f < program->modules[m].function_count; f++)
            if(keep_all[m][f] && program->modules[m].functions[f].exported &&
               !program->modules[m].functions[f].is_extern)
                return 1;
    }
    for(int m = 0; m < program->module_count; m++)
        if(import->resolved_module == &program->modules[m] &&
           uses_imported_constant(module, keep,
               keep_types[module - program->modules],
               keep_defines[module - program->modules],
               keep_defines[m], import->resolved_module))
            return 1;
    for(int f = 0; f < module->function_count; f++) {
        if(!keep[f])
            continue;
        const ZirFunction *function = &module->functions[f];
        for(int e = 0; e < function->expr_count; e++) {
            const ZirModule *owner = NULL;
            const ZirFunction *callee = NULL;
            if((function->exprs[e].kind == ZIR_EXPR_CALL &&
                function->exprs[e].slot_type[0] == '\0') ||
               function->exprs[e].is_function_value) {
                if(ResolveFunction(module, function->exprs[e].name,
                                    &owner, &callee) == 1 &&
                   owner == import->resolved_module && callee != NULL)
                    return 1;
            }
        }
    }
    for(int f = 0; f < module->function_count; f++) {
        if(!keep[f]) continue;
        const ZirFunction *function = &module->functions[f];
        for(int e = 0; e < function->expr_count; e++) {
            const ZirExpr *expression = &function->exprs[e];
            const ZirModule *owner = NULL;
            const ZirGlobal *global = NULL;
            if(expression->kind == ZIR_EXPR_IDENT &&
               ResolveGlobalAt(module, expression->name,
                               SpanPath(expression->span), &owner, &global) == 1 &&
               owner == import->resolved_module && global != NULL)
                return 1;
        }
    }
    for(int m = 0; m < program->module_count; m++) {
        if(import->resolved_module != &program->modules[m])
            continue;
        for(int t = 0; t < program->modules[m].type_count; t++)
            if(keep_types[m][t])
                return 1;
        break;
    }
    return 0;
}

static int
import_has_startup_path(const ZirProgram *program,
                        const unsigned char *startup_path,
                        const ZirImport *import)
{
    for(int m = 0; m < program->module_count; m++)
        if(import->resolved_module == &program->modules[m])
            return startup_path[m] != 0;
    return 0;
}
/* Buffers link_checked_entry keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LinkCheckedEntryBuffers {
    ZirFunction signature;
} LinkCheckedEntryBuffers;

static ZirProgram *link_checked_entry(const ZirProgram *program, const char *entry_module,
           const char *entry_function, int native);

static ZirProgram *
link_checked_entry_with_buffers(const ZirProgram *program, const char *entry_module,
           const char *entry_function, int native, LinkCheckedEntryBuffers *buffers)
{
    unsigned char **keep = NULL;
    unsigned char **keep_types = NULL;
    unsigned char **keep_defines = NULL;
    unsigned char *keep_modules = NULL;
    unsigned char *startup_path = NULL;
    ZirProgram *linked = NULL;
    int entry_m = -1, entry_f = -1;
    int selected_modules = 0;
    if(program == NULL || program->module_count <= 0 ||
       entry_module == NULL || entry_function == NULL)
        return NULL;
    keep = calloc((size_t)program->module_count, sizeof(*keep));
    keep_types = calloc((size_t)program->module_count, sizeof(*keep_types));
    keep_defines = calloc((size_t)program->module_count, sizeof(*keep_defines));
    keep_modules = calloc((size_t)program->module_count, 1);
    startup_path = calloc((size_t)program->module_count, 1);
    if(keep == NULL || keep_types == NULL || keep_defines == NULL ||
       keep_modules == NULL || startup_path == NULL)
        goto failed;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int other = 0; other < m; other++)
            if(strcmp(program->modules[other].name, module->name) == 0) {
                Diagnostic(module->span, "zib.module",
                           "duplicate module identity: %s", module->name);
                goto failed;
            }
        keep[m] = calloc((size_t)(module->function_count > 0 ?
                                  module->function_count : 1), 1);
        keep_types[m] = calloc((size_t)(module->type_count > 0 ?
                                        module->type_count : 1), 1);
        keep_defines[m] = calloc((size_t)(module->define_count > 0 ?
                                          module->define_count : 1), 1);
        if(keep[m] == NULL || keep_types[m] == NULL || keep_defines[m] == NULL)
            goto failed;
        if(strcmp(module->name, entry_module) != 0)
            continue;
        for(int f = 0; f < module->function_count; f++)
            if(strcmp(module->functions[f].name, entry_function) == 0) {
                if(entry_f >= 0) {
                    Diagnostic(module->functions[f].span, "zib.entry",
                               "ambiguous bundle entry: %s", entry_function);
                    goto failed;
                }
                entry_m = m;
                entry_f = f;
            }
    }
    if(entry_m < 0) {
        Diagnostic(Span("<bundle>", 1, 1), "zib.entry",
                   "bundle entry was not found: %s:%s",
                   entry_module, entry_function);
        goto failed;
    }
    keep[entry_m][entry_f] = 1;
    /* Verification roots retain the code and enum domains named by their
     * obligations even when the executable entry does not call that code. */
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        if(!module->law_count && !module->proof_count) continue;
        keep_modules[m] = 1;
        for(int d = 0; d < module->define_count; d++) keep_defines[m][d] = 1;
        for(int t = 0; t < module->type_count; t++)
            if(module->types[t].is_enum) keep_types[m][t] = 1;
        for(int l = 0; l < module->law_count; l++)
            if(module->laws[l].claim) {
                if(!mark_proof_graph(program, module, module->laws[l].claim, keep, keep_types)) goto failed;
            }
        for(int p = 0; p < module->proof_count; p++) {
            const ZirProof *proof = &module->proofs[p];
            const ZirModule *scope = module;
            const ZirModule *law_owner = NULL;
            if(FindVisibleLaw(module, proof->name, &law_owner)) scope = law_owner;
            if(!mark_proof_graph(program, scope, &proof->terms, keep, keep_types)) goto failed;
            for(int s = 0; s < proof->step_count; s++)
                if(proof->steps[s].kind == ZIR_PROOF_UNFOLD && proof->steps[s].target[0]) {
                    mark_proof_function(program, scope, proof->steps[s].target, keep);
                    mark_proof_function(program, module, proof->steps[s].target, keep);
                }
        }
        for(int n = 0; n < module->law_count + module->proof_count; n++) {
            if(n >= module->law_count || module->laws[n].claim) continue;
            const char *text = n < module->law_count ? module->laws[n].payload :
                               module->proofs[n - module->law_count].source;
            for(const char *p = text; *p;) {
                if(!isalpha((unsigned char)*p) && *p != '_') { p++; continue; }
                const char *start = p;
                while(isalnum((unsigned char)*p) || *p == '_' || *p == '.') p++;
                size_t length = (size_t)(p - start);
                if(length >= ZIR_NAME_MAX) continue;
                char name[ZIR_NAME_MAX]; memcpy(name, start, length); name[length] = 0;
                const ZirModule *owner = NULL; const ZirFunction *fn = NULL;
                if(ResolveFunction(module, name, &owner, &fn) == 1)
                    for(int target = 0; target < program->module_count; target++)
                        if(owner == &program->modules[target])
                            keep[target][fn - owner->functions] = 1;
            }
        }
    }
resolve_reachability:
    for(int changed = 1; changed;) {
        changed = 0;
        for(int m = 0; m < program->module_count; m++) {
            const ZirModule *module = &program->modules[m];
            int active = m == entry_m || startup_path[m];
            for(int f = 0; f < module->function_count; f++)
                active |= keep[m][f] != 0;
            if(!active)
                for(int g = 0; g < module->global_count; g++)
                    if(global_is_used(program, keep, module,
                                      &module->globals[g])) {
                        active = 1;
                        break;
                    }
            if(active)
                for(int f = 0; f < module->function_count; f++)
                    if(module->functions[f].is_global_initializer &&
                       !keep[m][f]) {
                        keep[m][f] = 1;
                        changed = 1;
                    }
            if(active)
                for(int i = 0; i < module->import_count; i++) {
                    const ZirModule *dependency =
                        module->imports[i].resolved_module;
                    if(!ModuleNeedsStartup(dependency)) continue;
                    for(int d = 0; d < program->module_count; d++)
                        if(dependency == &program->modules[d] &&
                           !startup_path[d]) {
                            startup_path[d] = 1;
                            changed = 1;
                        }
                }
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *function = &module->functions[f];
                if(!keep[m][f])
                    continue;
                for(int e = 0; e < function->expr_count; e++) {
                    const ZirExpr *expression = &function->exprs[e];
                    const ZirModule *owner = NULL;
                    const ZirFunction *callee = NULL;
                    int target_m = -1, target_f = -1;
                    if(expression->kind != ZIR_EXPR_CALL &&
                       !expression->is_function_value)
                        continue;
                    if(expression->kind == ZIR_EXPR_CALL &&
                       expression->slot_type[0] != '\0')
                        continue;
                    if(expression->kind == ZIR_EXPR_CALL &&
                       builtin_call(expression->name))
                        continue;
                    if(ResolveFunction(module, expression->name,
                                       &owner, &callee) != 1 ||
                       owner == NULL || callee == NULL) {
                        int external = 0;
                        const ZirImport *external_import = NULL;
                        for(int i = 0; i < module->import_count; i++)
                            if(module->imports[i].kind == ZIR_IMPORT_EXTERN &&
                               strcmp(module->imports[i].name,
                                      expression->name) == 0) {
                                external = 1;
                                const ZirImport *import = &module->imports[i];
                                external_import = import;
                                if(import->extern_kind == ZIR_EXTERN_HOST &&
                                   strncmp(import->target, "ziran:", 6) == 0) {
                                    int found = 0;
                                    for(int candidate = 0;
                                        candidate < program->module_count;
                                        candidate++) {
                                        const ZirModule *provider =
                                            &program->modules[candidate];
                                        if(strcmp(provider->name,
                                                  import->target + 6) != 0)
                                            continue;
                                        for(int export = 0;
                                            export < provider->function_count;
                                            export++) {
                                            const ZirFunction *implementation =
                                                &provider->functions[export];
                                            if(strcmp(implementation->name,
                                                      import->extern_symbol) == 0 &&
                                               implementation->exported &&
                                               !implementation->is_extern) {
                                                found++;
                                                if(!keep[candidate][export]) {
                                                    keep[candidate][export] = 1;
                                                    changed = 1;
                                                }
                                            }
                                        }
                                    }
                                    if(found != 1) {
                                        Diagnostic(import->span, "zib.bind",
                                                   "bound Ziran host provider is missing or ambiguous: %s",
                                                   import->name);
                                        goto failed;
                                    }
                                }
                            }
                        if(external) {
                            /* A native link resolves a foreign call to the
                             * program export with the same linker symbol.
                             * Host-library imports carry no stripped C
                             * symbol; their symbol is the declared name. */
                            const char *symbol =
                                external_import->extern_symbol[0] ?
                                external_import->extern_symbol :
                                external_import->name;
                            if(native)
                                for(int candidate = 0;
                                    candidate < program->module_count;
                                    candidate++)
                                    for(int export = 0; export <
                                        program->modules[candidate].function_count;
                                        export++) {
                                        const ZirFunction *implementation =
                                            &program->modules[candidate].functions[export];
                                        const char *provided =
                                            implementation->export_symbol[0] ?
                                            implementation->export_symbol :
                                            implementation->name;
                                        if(implementation->exported &&
                                           !implementation->is_extern &&
                                           strcmp(provided, symbol) == 0 &&
                                           !keep[candidate][export]) {
                                            keep[candidate][export] = 1;
                                            changed = 1;
                                        }
                                    }
                            continue;
                        }
                        Diagnostic(expression->span, "zib.call",
                                   "unresolved portable call: %s",
                                   expression->name);
                        goto failed;
                    }
                    for(int candidate = 0; candidate < program->module_count;
                        candidate++)
                        if(owner == &program->modules[candidate])
                            target_m = candidate;
                    if(target_m >= 0)
                        for(int candidate = 0;
                            candidate < program->modules[target_m].function_count;
                            candidate++)
                            if(callee == &program->modules[target_m].functions[candidate])
                                target_f = candidate;
                    if(target_f < 0) {
                        Diagnostic(expression->span, "zib.call",
                                   "call is outside the linked program: %s",
                                   expression->name);
                        goto failed;
                    }
                    if(!keep[target_m][target_f]) {
                        keep[target_m][target_f] = 1;
                        changed = 1;
                    }
                }
            }
        }
    }
    for(int changed = 1; changed;) {
        changed = 0;
        for(int m = 0; m < program->module_count; m++) {
            const ZirModule *module = &program->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *function = &module->functions[f];
                if(!keep[m][f])
                    continue;
                if(!mark_type(program, module, function->return_type,
                              keep_types, &changed) ||
                   !mark_parameters(program, module, function,
                                    keep_types, &changed))
                    goto failed;
                for(int s = 0; s < function->stmt_count; s++)
                    if(!mark_type(program, module, function->stmts[s].type,
                                  keep_types, &changed))
                        goto failed;
                for(int e = 0; e < function->expr_count; e++) {
                    const ZirExpr *expression = &function->exprs[e];
                    if(!mark_type(program, module, expression->type,
                                  keep_types, &changed))
                        goto failed;
                    if((expression->kind == ZIR_EXPR_CAST ||
                        expression->kind == ZIR_EXPR_COMPOUND ||
                        expression->kind == ZIR_EXPR_SIZE_OF) &&
                       !mark_type(program, module, expression->name,
                                  keep_types, &changed))
                        goto failed;
                }
            }
            for(int t = 0; t < module->type_count; t++) {
                if(!keep_types[m][t])
                    continue;
                if(module->types[t].is_synthetic_application &&
                   !mark_type(program, module, module->types[t].template_name,
                              keep_types, &changed))
                    goto failed;
                if(module->types[t].is_record_template)
                    continue;
                if(module->types[t].is_procedure_type) {
                    memset(&buffers->signature, 0, sizeof(buffers->signature));
                    if(strlen(module->types[t].body) >= ZIR_TEXT_MAX)
                        goto failed;
                    buffers->signature.args_text = KeepParameters(module->types[t].body);
                    if(!mark_parameters(program, module, &buffers->signature,
                                        keep_types, &changed))
                        goto failed;
                    continue;
                }
                if(module->types[t].is_enum)
                    continue;
                size_t offset = 0;
                ZirTypeField field;
                int status;
                while((status = TypeNextField(&module->types[t], &offset,
                                              &field)) == 1)
                    if(!mark_type(program, module, field.type,
                                  keep_types, &changed))
                        goto failed;
                if(status < 0)
                    goto failed;
            }
        }
    }
    /* Go interfaces dispatch through the receiver's method set, outside the
     * explicit Ziran call graph. Retain methods of every retained record and
     * close their dependencies before pruning fields and declarations. */
    if(native == 2) {
        int added_methods = 0;
        for(int m = 0; m < program->module_count; m++) {
            const ZirModule *module = &program->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *function = &module->functions[f];
                if(keep[m][f] || !function->go_method[0]) continue;
                const ZirParameters *parameters = ParametersOf(FunctionArgs(function));
                const char *receiver = parameters->items[0].type;
                if(receiver[0] == '*') receiver++;
                const ZirType *record = FindType(module, receiver, NULL);
                for(int t = 0; t < module->type_count; t++)
                    if(record == &module->types[t] && keep_types[m][t]) {
                        keep[m][f] = 1;
                        added_methods = 1;
                    }
            }
        }
        if(added_methods) goto resolve_reachability;
    }
    /* Constants are source expressions in saved IR. Keep only constants
     * referenced by retained declarations, then follow their dependencies. */
    for(int changed = 1; changed;) {
        changed = 0;
        for(int d = 0; d < program->module_count; d++) {
            const ZirModule *dependency = &program->modules[d];
            for(int value = 0; value < dependency->define_count; value++) {
                if(keep_defines[d][value])
                    continue;
                const ZirDefine *constant = &dependency->defines[value];
                int used = 0;
                for(int m = 0; m < program->module_count && !used; m++) {
                    const ZirModule *consumer = &program->modules[m];
                    int visible = m == d;
                    if(!visible && constant->is_public)
                        for(int i = 0; i < consumer->import_count; i++)
                            if(consumer->imports[i].resolved_module == dependency) {
                                visible = 1;
                                break;
                            }
                    if(visible)
                        used = uses_constant_name(program, keep, consumer,
                            keep_types[m], keep_defines[m], constant->name);
                }
                if(used) {
                    keep_defines[d][value] = 1;
                    changed = 1;
                }
            }
        }
    }
    for(int m = 0; m < program->module_count; m++)
        keep_modules[m] = program->modules[m].law_count || program->modules[m].proof_count ||
            startup_path[m] ||
            memchr(keep[m], 1, (size_t)program->modules[m].function_count) != NULL ||
            memchr(keep_types[m], 1, (size_t)program->modules[m].type_count) != NULL ||
            memchr(keep_defines[m], 1, (size_t)program->modules[m].define_count) != NULL;
    for(int m = 0; m < program->module_count; m++)
        for(int g = 0; g < program->modules[m].global_count; g++)
            if(global_is_used(program, keep, &program->modules[m],
                              &program->modules[m].globals[g]))
                keep_modules[m] = 1;
    /* A law keeps what it names so its closure stays decidable in the
     * linked bundle. */
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int l = 0; l < module->law_count; l++) {
            const ZirLaw *law = &module->laws[l];
            if(!strcmp(law->kind, "type") || !strcmp(law->kind, "bounds")) {
                const char *payload = skip_ws(law->payload);
                const char *resolved = payload;
                for(int depth = 0; depth < 8; depth++) {
                    const ZirDefine *found = NULL;
                    if(resolved[0] == '[')
                        break;
                    for(int d = 0; d < module->define_count; d++)
                        if(strcmp(module->defines[d].name, resolved) == 0) {
                            found = &module->defines[d];
                            break;
                        }
                    if(found == NULL)
                        break;
                    resolved = skip_ws(found->value);
                }
                if(resolved[0] == '[') {
                    const char *close = strchr(resolved, ']');
                    char bound[ZIR_NAME_MAX];
                    size_t length;
                    if(close != NULL &&
                       (length = (size_t)(close - resolved - 1)) > 0 &&
                       length < sizeof(bound)) {
                        memcpy(bound, resolved + 1, length);
                        bound[length] = '\0';
                        for(int d = 0; d < module->define_count; d++)
                            if(strcmp(module->defines[d].name,
                                      trim_in_place(bound)) == 0)
                                keep_defines[m][d] = 1;
                    }
                } else
                    for(int t = 0; t < module->type_count; t++)
                        if(strcmp(module->types[t].name, resolved) == 0)
                            keep_types[m][t] = 1;
            } else if(!strcmp(law->kind, "size")) {
                const char *ops[] = {"==", ">=", "<="};
                for(int o = 0; o < 3; o++) {
                    const char *op = strstr(law->payload, ops[o]);
                    char name[ZIR_NAME_MAX];
                    size_t length;
                    if(op == NULL)
                        continue;
                    length = (size_t)(op - law->payload);
                    if(length == 0 || length >= sizeof(name))
                        continue;
                    memcpy(name, law->payload, length);
                    name[length] = '\0';
                    trim_in_place(name);
                    for(int t = 0; t < module->type_count; t++)
                        if(strcmp(module->types[t].name, name) == 0)
                            keep_types[m][t] = 1;
                    break;
                }
            } else if(!strcmp(law->kind, "effect") ||
                      !strcmp(law->kind, "abi")) {
                char name[ZIR_NAME_MAX];
                const char *comparison = strstr(law->payload, "==");
                size_t length = comparison != NULL ?
                    (size_t)(comparison - law->payload) :
                    strlen(law->payload);
                if(length >= sizeof(name))
                    length = sizeof(name) - 1;
                memcpy(name, law->payload, length);
                name[length] = '\0';
                trim_in_place(name);
                for(int f = 0; f < module->function_count; f++)
                    if(strcmp(module->functions[f].name, name) == 0)
                        keep[m][f] = 1;
            }
        }
    }
    /* Checked statement text may still use imported constants in array
     * bounds. Carry constants-only modules through the same import closure. */
    for(int changed = 1; changed;) {
        changed = 0;
        for(int m = 0; m < program->module_count; m++) {
            if(!keep_modules[m])
                continue;
            const ZirModule *module = &program->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                const ZirModule *dependency = module->imports[i].resolved_module;
                if((module->imports[i].kind != ZIR_IMPORT_OPEN &&
                    module->imports[i].kind != ZIR_IMPORT_MODULE) ||
                   dependency == NULL ||
                   (!(module->law_count || module->proof_count) &&
                    !uses_imported_constant(module, keep[m], keep_types[m],
                                           keep_defines[m],
                                           keep_defines[dependency - program->modules],
                                           dependency)))
                    continue;
                for(int d = 0; d < program->module_count; d++)
                    if(dependency == &program->modules[d] && !keep_modules[d]) {
                        keep_modules[d] = 1;
                        changed = 1;
                    }
            }
        }
    }
    /* A public module entry may contain only `using Alias :: #import` lines.
     * Its own declarations can be empty even while retained callers need its
     * import graph to resolve types during saved-IR code generation. */
    for(int changed = 1; changed;) {
        changed = 0;
        for(int m = 0; m < program->module_count; m++) {
            if(!keep_modules[m]) continue;
            const ZirModule *module = &program->modules[m];
            for(int i = 0; i < module->import_count; i++) {
                const ZirImport *import = &module->imports[i];
                if(import->resolved_module == NULL)
                    continue;
                /* A plain import keeps a re-exporting module only while it
                 * still leads to something retained. */
                if(!import->is_using &&
                   ((import->kind != ZIR_IMPORT_OPEN &&
                     import->kind != ZIR_IMPORT_MODULE) ||
                    !reexports_kept(program, import->resolved_module, keep,
                                    keep_types)))
                    continue;
                for(int d = 0; d < program->module_count; d++)
                    if(import->resolved_module == &program->modules[d] &&
                       !keep_modules[d]) {
                        keep_modules[d] = 1;
                        changed = 1;
                    }
            }
        }
    }
    for(int m = 0; m < program->module_count; m++)
        selected_modules += keep_modules[m] != 0;
    linked = ProgramNew();
    if(linked == NULL)
        goto failed;
    linked->modules = calloc((size_t)selected_modules, sizeof(*linked->modules));
    if(linked->modules == NULL)
        goto failed;
    linked->module_count = linked->module_cap = selected_modules;
    int out = 0;
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *source = &program->modules[m];
        ZirModule *target;
        int kept_functions = 0, kept_types = 0, kept_imports = 0;
        int kept_defines = 0;
        for(int f = 0; f < source->function_count; f++)
            kept_functions += keep[m][f] != 0;
        for(int t = 0; t < source->type_count; t++) {
            if(!keep_types[m][t])
                continue;
            if(source->types[t].is_union &&
               !bundle_portable_union(source, &source->types[t])) {
                Diagnostic(source->types[t].span, "zib.union",
                           "portable unions support scalar fields only");
                goto failed;
            }
            kept_types++;
        }
        for(int d = 0; d < source->define_count; d++)
            kept_defines += keep_defines[m][d] != 0;
        if(!keep_modules[m])
            continue;
        target = &linked->modules[out++];
        *target = *source;
        target->globals = NULL; target->global_count = target->global_cap = 0;
        target->defines = NULL; target->define_count = target->define_cap = 0;
        target->asserts = NULL; target->assert_count = target->assert_cap = 0;
        target->usings = NULL; target->using_count = target->using_cap = 0;
        target->types = NULL; target->type_count = target->type_cap = 0;
        target->imports = NULL; target->import_count = target->import_cap = 0;
        TypeLookupsChanged();
        target->functions = NULL;
        target->function_count = target->function_cap = 0;
        target->laws = NULL; target->law_count = target->law_cap = 0;
        target->law_waivers = NULL; target->law_waiver_count = target->law_waiver_cap = 0;
        target->proofs = NULL; target->proof_count = target->proof_cap = 0;
        if(!copy_laws(target, source)) goto failed;
        /* Statement text is reparsed when a bundle is loaded. Keep constants
         * so symbolic array declarations retain their checked meaning. */
        if(kept_defines > 0) {
            target->defines = malloc((size_t)kept_defines *
                                     sizeof(*target->defines));
            if(target->defines == NULL)
                goto failed;
            for(int d = 0; d < source->define_count; d++)
                if(keep_defines[m][d])
                    target->defines[target->define_count++] = source->defines[d];
            target->define_cap = kept_defines;
        }
        /* Keep module globals read or written by retained functions. Globals
         * used only by discarded functions must not add bundle state or
         * unsupported initializer requirements. */
        for(int g = 0; g < source->global_count; g++) {
            if(!global_is_used(program, keep, source, &source->globals[g]))
                continue;
            ZirGlobal *next = realloc(target->globals,
                (size_t)(target->global_count + 1) * sizeof(*next));
            if(next == NULL)
                goto failed;
            target->globals = next;
            target->globals[target->global_count++] = source->globals[g];
            target->global_cap = target->global_count;
        }
        if(kept_functions > 0) {
            target->functions = calloc((size_t)kept_functions,
                                       sizeof(*target->functions));
            if(target->functions == NULL)
                goto failed;
            target->function_count = target->function_cap = kept_functions;
            int next = 0;
            for(int f = 0; f < source->function_count; f++)
                if(keep[m][f] && !copy_function(&target->functions[next++],
                                               &source->functions[f]))
                    goto failed;
        }
        if(kept_types > 0) {
            TypeLookupsChanged();
            target->types = calloc((size_t)kept_types, sizeof(*target->types));
            if(target->types == NULL)
                goto failed;
            target->type_count = target->type_cap = kept_types;
            int next = 0;
            for(int t = 0; t < source->type_count; t++)
                if(keep_types[m][t])
                    target->types[next++] = source->types[t];
        }
        for(int i = 0; i < source->import_count; i++)
            kept_imports += source->law_count || source->proof_count || source->imports[i].is_using ||
                           import_has_startup_path(program, startup_path,
                                                   &source->imports[i]) ||
                           import_is_used(program, source, keep[m], keep,
                                           keep_types, keep_defines,
                                           &source->imports[i]) ||
                           law_names_import(source, &source->imports[i]);
        if(kept_imports > 0) {
            target->imports = calloc((size_t)kept_imports,
                                     sizeof(*target->imports));
            if(target->imports == NULL)
                goto failed;
            target->import_count = target->import_cap = kept_imports;
            int next_import = 0;
            for(int i = 0; i < source->import_count; i++)
                if(source->law_count || source->proof_count || source->imports[i].is_using ||
                   import_has_startup_path(program, startup_path,
                                           &source->imports[i]) ||
                   import_is_used(program, source, keep[m], keep, keep_types,
                                  keep_defines,
                                  &source->imports[i]) ||
                   law_names_import(source, &source->imports[i]))
                    target->imports[next_import++] = source->imports[i];
        }
    }
    /* Parse-time imports can load dependencies before their caller, while
     * saved IR loads the caller first. Give both paths one module order. */
    qsort(linked->modules, (size_t)linked->module_count,
          sizeof(*linked->modules), module_name_order);
    if(!LinkImports(&linked, 1) ||
       !prune_record_fields(linked, entry_module, entry_function))
        goto failed;
    if(!CheckLawGates(&linked, 1)) goto failed;
    for(int m = 0; m < program->module_count; m++)
        free(keep[m]);
    for(int m = 0; m < program->module_count; m++)
        free(keep_types[m]);
    for(int m = 0; m < program->module_count; m++)
        free(keep_defines[m]);
    free(keep);
    free(keep_types);
    free(keep_defines);
    free(keep_modules);
    free(startup_path);
    return linked;
failed:
    if(keep != NULL)
        for(int m = 0; m < program->module_count; m++)
            free(keep[m]);
    if(keep_types != NULL)
        for(int m = 0; m < program->module_count; m++)
            free(keep_types[m]);
    if(keep_defines != NULL)
        for(int m = 0; m < program->module_count; m++)
            free(keep_defines[m]);
    free(keep);
    free(keep_types);
    free(keep_defines);
    free(keep_modules);
    free(startup_path);
    ProgramFree(linked);
    return NULL;
}

static ZirProgram *
link_checked_entry(const ZirProgram *program, const char *entry_module,
           const char *entry_function, int native)
{
    static _Thread_local LinkCheckedEntryBuffers *spares[16];
    static _Thread_local int spare_count;
    LinkCheckedEntryBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    ZirProgram *returned = link_checked_entry_with_buffers(program, entry_module, entry_function, native, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

ZirProgram *
BundleLink(const ZirProgram *program, const char *entry_module,
           const char *entry_function)
{
    ZirProgram *optimized = copy_program(program);
    if(optimized == NULL) return NULL;
    for(int m = 0; m < optimized->module_count; m++)
        for(int f = 0; f < optimized->modules[m].function_count; f++)
            if(!prune_function(&optimized->modules[m].functions[f])) {
                ProgramFree(optimized);
                return NULL;
            }
    ZirProgram *linked = link_checked_entry(optimized, entry_module,
                                            entry_function, 0);
    ProgramFree(optimized);
    return linked;
}

static ZirProgram *
native_link(const ZirProgram *program, const char *entry_module,
            const char *entry_function, int native)
{
    ZirProgram *optimized = copy_program(program);
    if(optimized == NULL) return NULL;
    for(int m = 0; m < optimized->module_count; m++)
        for(int f = 0; f < optimized->modules[m].function_count; f++)
            if(!prune_function(&optimized->modules[m].functions[f])) {
                ProgramFree(optimized);
                return NULL;
            }
    ZirProgram *linked = link_checked_entry(optimized, entry_module,
                                            entry_function, native);
    ProgramFree(optimized);
    return linked;
}

ZirProgram *
NativeLink(const ZirProgram *program, const char *entry_module,
           const char *entry_function)
{
    return native_link(program, entry_module, entry_function, 1);
}

ZirProgram *
NativeGoLink(const ZirProgram *program, const char *entry_module,
             const char *entry_function)
{
    return native_link(program, entry_module, entry_function, 2);
}

int
BundleWrite(FILE *out, const ZirProgram *program,
               const char *entry_module, const char *entry_function)
{
    FILE *payload;
    long length;
    int ok;
    if(out == NULL || program == NULL || entry_module == NULL ||
       entry_function == NULL)
        return 0;
    payload = tmpfile();
    if(payload == NULL)
        return 0;
    ok = ProgramWrite(program, payload);
    length = ok ? ftell(payload) : -1;
    if(length <= 0 || length > ZIB_MAX_IR_BYTES || fseek(payload, 0, SEEK_SET))
        ok = 0;
    int capabilities = count_capabilities(program);
    if(capabilities > ZIB_MAX_CAPABILITIES)
        ok = 0;
    if(ok) {
        ok = fwrite("ZIB\0", 1, 4, out) == 4 &&
             write_u32(out, ZIB_VERSION) &&
             write_name(out, entry_module) &&
             write_name(out, entry_function) &&
             write_u32(out, (uint32_t)capabilities);
        for(int m = 0; ok && m < program->module_count; m++)
            for(int i = 0; ok && i < program->modules[m].import_count; i++)
                if(program->modules[m].imports[i].kind == ZIR_IMPORT_EXTERN &&
                   (program->modules[m].imports[i].extern_kind != ZIR_EXTERN_HOST ||
                    strncmp(program->modules[m].imports[i].target,
                            "ziran:", 6) != 0) &&
                   import_is_called(&program->modules[m],
                                    &program->modules[m].imports[i]))
                    ok = write_name(out, program->modules[m].name) &&
                         write_name(out, program->modules[m].imports[i].name);
        {
            int laws = 0, waivers = 0;
            for(int m = 0; m < program->module_count; m++) {
                laws += program->modules[m].law_count;
                waivers += program->modules[m].law_waiver_count;
            }
            if(ok)
                ok = write_u32(out, (uint32_t)laws);
            for(int m = 0; ok && m < program->module_count; m++)
                for(int l = 0; ok && l < program->modules[m].law_count;
                    l++) {
                    const ZirLaw *law = &program->modules[m].laws[l];
                    char detail[ZIR_TEXT_MAX];
                    int status = EvaluateLaw(program,
                        &program->modules[m], law, detail,
                        sizeof(detail));
                    ok = write_name(out, program->modules[m].name) &&
                         write_name(out, law->name) &&
                         write_name(out, law->kind) &&
                         write_name(out, LawStatusName(status)) &&
                         write_name(out, detail) &&
                         write_name(out, law->evidence.method) &&
                         write_text(out, law->evidence.domain, sizeof(law->evidence.domain)) &&
                         write_u32(out, (uint32_t)law->evidence.cases_checked) &&
                         write_u32(out, (uint32_t)(law->evidence.cases_checked >> 32)) &&
                         write_text(out, law->evidence.counterexample, sizeof(law->evidence.counterexample)) &&
                         write_u32(out, (uint32_t)law->evidence.waived);
                }
            if(ok)
                ok = write_u32(out, (uint32_t)waivers);
            for(int m = 0; ok && m < program->module_count; m++)
                for(int w = 0; ok &&
                    w < program->modules[m].law_waiver_count; w++) {
                    const ZirLawWaiver *waiver =
                        &program->modules[m].law_waivers[w];
                    ok = write_name(out, program->modules[m].name) &&
                         write_name(out, waiver->name) &&
                         write_name(out, waiver->reason);
                }
        }
        ok = ok && write_u32(out, (uint32_t)length) &&
             copy_bytes(payload, out, (uint32_t)length) && fflush(out) == 0;
    }
    fclose(payload);
    return ok;
}

void
ZibLawTableFree(ZibLawTable *table)
{
    if(table == NULL)
        return;
    free(table->laws);
    free(table->waivers);
    table->laws = NULL;
    table->waivers = NULL;
    table->law_count = 0;
    table->waiver_count = 0;
}

ZirProgram *
BundleRead(FILE *in, const char *path,
           char *entry_module, size_t module_size,
           char *entry_function, size_t function_size,
           ZibLawTable *laws)
{
    unsigned char signature[4];
    uint32_t version, capability_count, length;
    FILE *payload = NULL;
    ZirProgram *program = NULL;
    CapabilityName *capabilities = NULL;
    ZibLawTable discarded = {0};
    int discard_laws = laws == NULL;
    if(discard_laws) laws = &discarded;
    const char *problem = "invalid or truncated bundle";
    if(in == NULL || path == NULL || entry_module == NULL ||
       entry_function == NULL)
        return NULL;
    if(fread(signature, 1, 4, in) != 4 || memcmp(signature, "ZIB\0", 4))
        goto failed;
    if(!read_u32(in, &version))
        goto failed;
    if(version != ZIB_VERSION) {
        problem = "unsupported ZIB version";
        goto failed;
    }
    if(!read_name(in, entry_module, module_size) ||
       !read_name(in, entry_function, function_size) ||
       !read_u32(in, &capability_count))
        goto failed;
    if(capability_count > ZIB_MAX_CAPABILITIES) {
        problem = "bundle has too many host capabilities";
        goto failed;
    }
    capabilities = calloc(capability_count ? capability_count : 1,
                          sizeof(*capabilities));
    if(capabilities == NULL)
        goto failed;
    for(uint32_t i = 0; i < capability_count; i++)
        if(!read_name(in, capabilities[i].module,
                      sizeof(capabilities[i].module)) ||
           !read_name(in, capabilities[i].function,
                      sizeof(capabilities[i].function)))
            goto failed;
    if(laws != NULL) {
        uint32_t law_count = 0, waiver_count = 0;
        if(!read_u32(in, &law_count) ||
           law_count > ZIB_MAX_CAPABILITIES * 64)
            goto failed;
        laws->laws = law_count ? calloc(law_count,
                                        sizeof(*laws->laws)) : NULL;
        if(law_count != 0 && laws->laws == NULL)
            goto failed;
        for(uint32_t i = 0; i < law_count; i++) {
            ZibLawRecord *record = &laws->laws[i];
            uint32_t lo, hi, waived;
            if(!read_name(in, record->module, sizeof(record->module)) ||
               !read_name(in, record->name, sizeof(record->name)) ||
               !read_name(in, record->kind, sizeof(record->kind)) ||
               !read_name(in, record->status, sizeof(record->status)) ||
               !read_name(in, record->detail, sizeof(record->detail)) ||
               !read_name(in, record->evidence.method, sizeof(record->evidence.method)) ||
               !read_text(in, record->evidence.domain, sizeof(record->evidence.domain)) ||
               !read_u32(in, &lo) || !read_u32(in, &hi) ||
               !read_text(in, record->evidence.counterexample, sizeof(record->evidence.counterexample)) ||
               !read_u32(in, &waived) || waived > 1)
                goto failed;
            record->evidence.cases_checked = (uint64_t)lo | ((uint64_t)hi << 32);
            record->evidence.waived = (int)waived;
        }
        laws->law_count = (int)law_count;
        if(!read_u32(in, &waiver_count) ||
           waiver_count > ZIB_MAX_CAPABILITIES * 64)
            goto failed;
        laws->waivers = waiver_count ? calloc(waiver_count,
                                              sizeof(*laws->waivers)) : NULL;
        if(waiver_count != 0 && laws->waivers == NULL)
            goto failed;
        for(uint32_t i = 0; i < waiver_count; i++) {
            ZibLawWaiverRecord *record = &laws->waivers[i];
            if(!read_name(in, record->module, sizeof(record->module)) ||
               !read_name(in, record->name, sizeof(record->name)) ||
               !read_name(in, record->reason, sizeof(record->reason)))
                goto failed;
        }
        laws->waiver_count = (int)waiver_count;
    }
    if(!read_u32(in, &length) || length == 0 || length > ZIB_MAX_IR_BYTES)
        goto failed;
    payload = tmpfile();
    if(payload == NULL || !copy_bytes(in, payload, length) ||
       fgetc(in) != EOF || ferror(in) || fseek(payload, 0, SEEK_SET))
        goto failed;
    program = ProgramRead(payload, path);
    if(program == NULL) {
        problem = "invalid embedded ZIR";
        goto failed;
    }
    if(capability_count != (uint32_t)count_capabilities(program)) {
        problem = "bundle capability list differs from linked IR";
        goto failed;
    }
    uint32_t next_capability = 0;
    for(int m = 0; m < program->module_count; m++)
        for(int i = 0; i < program->modules[m].import_count; i++)
            if(program->modules[m].imports[i].kind == ZIR_IMPORT_EXTERN &&
               (program->modules[m].imports[i].extern_kind != ZIR_EXTERN_HOST ||
                strncmp(program->modules[m].imports[i].target,
                        "ziran:", 6) != 0) &&
               import_is_called(&program->modules[m],
                                &program->modules[m].imports[i])) {
                if(strcmp(capabilities[next_capability].module,
                          program->modules[m].name) != 0 ||
                   strcmp(capabilities[next_capability].function,
                          program->modules[m].imports[i].name) != 0) {
                    problem = "bundle capability list differs from linked IR";
                    goto failed;
                }
                next_capability++;
            }
    if(!CheckCanonicalPrograms(&program, 1, NULL)) {
        problem = "embedded ZIR failed semantic checking";
        goto failed;
    }
    if(laws) {
        int next = 0, next_waiver = 0;
        for(int m = 0; m < program->module_count; m++) {
            const ZirModule *module = &program->modules[m];
            for(int l = 0; l < module->law_count; l++) {
                const ZirLaw *law = &module->laws[l];
                char detail[ZIR_TEXT_MAX];
                int status = EvaluateLaw(program, module, law, detail, sizeof(detail));
                if(next >= laws->law_count) { problem = "bundle law table differs from checked IR"; goto failed; }
                const ZibLawRecord *record = &laws->laws[next++];
                if(strcmp(record->module, module->name) || strcmp(record->name, law->name) ||
                   strcmp(record->kind, law->kind) || strcmp(record->status, LawStatusName(status)) ||
                   strcmp(record->detail, detail) || strcmp(record->evidence.method, law->evidence.method) ||
                   strcmp(record->evidence.domain, law->evidence.domain) ||
                   strcmp(record->evidence.counterexample, law->evidence.counterexample) ||
                   record->evidence.cases_checked != law->evidence.cases_checked ||
                   record->evidence.waived != law->evidence.waived) {
                    problem = "bundle law table differs from checked IR"; goto failed;
                }
            }
            for(int w = 0; w < module->law_waiver_count; w++) {
                if(next_waiver >= laws->waiver_count) { problem = "bundle waiver table differs from checked IR"; goto failed; }
                const ZibLawWaiverRecord *record = &laws->waivers[next_waiver++];
                if(strcmp(record->module, module->name) || strcmp(record->name, module->law_waivers[w].name) ||
                   strcmp(record->reason, module->law_waivers[w].reason)) {
                    problem = "bundle waiver table differs from checked IR"; goto failed;
                }
            }
        }
        if(next != laws->law_count || next_waiver != laws->waiver_count) {
            problem = "bundle law table differs from checked IR"; goto failed;
        }
    }
    fclose(payload);
    free(capabilities);
    if(discard_laws) ZibLawTableFree(&discarded);
    return program;
failed:
    Diagnostic(Span(path, 1, 1), "zib.invalid", "%s", problem);
    if(payload != NULL)
        fclose(payload);
    free(capabilities);
    ProgramFree(program);
    if(discard_laws) ZibLawTableFree(&discarded);
    return NULL;
}
