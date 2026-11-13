#include "zir.h"
#include "zir_serial.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void
quoted(const char *value)
{
    putchar('"');
    for(const unsigned char *cursor = (const unsigned char *)value;
        *cursor; cursor++) {
        unsigned char byte = *cursor;
        if(byte == '"' || byte == '\\') {
            putchar('\\');
            putchar(byte);
        } else if(byte == '\n') {
            fputs("\\n", stdout);
        } else if(byte == '\r') {
            fputs("\\r", stdout);
        } else if(byte == '\t') {
            fputs("\\t", stdout);
        } else if(byte >= 32 && byte < 127) {
            putchar(byte);
        } else {
            printf("\\x%02x", byte);
        }
    }
    putchar('"');
}

/* Source text as written, for a trailing comment: only control bytes are
 * escaped, so a statement reads as it does in its file. */
static void
raw(const char *value)
{
    for(const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++) {
        unsigned char byte = *cursor;
        if(byte == '\n') fputs("\\n", stdout);
        else if(byte == '\r') fputs("\\r", stdout);
        else if(byte == '\t') fputs("\\t", stdout);
        else if(byte < 32 || byte == 127) printf("\\x%02x", byte);
        else putchar(byte);
    }
}

/* Each expression's statement: the first whose expression trees reach it,
 * or -1. */
static int *
expression_owners(const ZirFunction *function)
{
    int count = function->expr_count;
    if(count <= 0) return NULL;
    int *owners = malloc((size_t)count * sizeof(*owners));
    int *stack = malloc((size_t)count * sizeof(*stack));
    if(owners == NULL || stack == NULL) {
        free(owners);
        free(stack);
        return NULL;
    }
    for(int i = 0; i < count; i++) owners[i] = -1;
    for(int s = 0; s < function->stmt_count; s++) {
        int top = 0;
        int roots[2] = {function->stmts[s].expr_root, function->stmts[s].lhs_root};
        for(int r = 0; r < 2; r++)
            if(roots[r] >= 0 && roots[r] < count && owners[roots[r]] < 0) {
                owners[roots[r]] = s;
                stack[top++] = roots[r];
            }
        while(top > 0) {
            const ZirExpr *expression = &function->exprs[stack[--top]];
            int links[5] = {expression->left, expression->right, expression->first_child,
                            expression->next_sibling, expression->third};
            for(int l = 0; l < 5; l++)
                if(links[l] >= 0 && links[l] < count && owners[links[l]] < 0) {
                    owners[links[l]] = s;
                    stack[top++] = links[l];
                }
        }
    }
    free(stack);
    return owners;
}

/* Whether an expression's text only repeats part of its statement's, which
 * the listing already shows once. */
static int
text_in_statement(const ZirFunction *function, const int *owners, int index)
{
    const char *text = function->exprs[index].text;
    if(text == NULL || !text[0]) return 1;
    int owner = owners ? owners[index] : -1;
    if(owner >= 0 && function->stmts[owner].text && strstr(function->stmts[owner].text, text)) return 1;
    for(int s = 0; s < function->stmt_count; s++)
        if(function->stmts[s].text && strstr(function->stmts[s].text, text)) return 1;
    return 0;
}

static void
show_expression(const ZirFunction *function, const int *owners, int e)
{
    const ZirExpr *expression = &function->exprs[e];
    printf("    %%%d %s", e, ExprKindName(expression->kind));
    int literal = expression->kind == ZIR_EXPR_INT || expression->kind == ZIR_EXPR_FLOAT ||
                  expression->kind == ZIR_EXPR_STRING;
    if(literal) {
        /* A literal is its value. */
        printf(" %s", expression->text);
        if(expression->type[0] && strcmp(expression->type, "string") != 0)
            printf(" -> %s", expression->type);
        if(expression->next_sibling >= 0) printf(" next=%%%d", expression->next_sibling);
        putchar('\n');
        return;
    }
    if(expression->kind == ZIR_EXPR_CALL) {
        /* A call is its callee and arguments. */
        putchar(' ');
        if(expression->name[0]) fputs(expression->name, stdout);
        else if(expression->left >= 0) printf("%%%d", expression->left);
        putchar('(');
        int guard = 0;
        for(int child = expression->first_child; child >= 0 && child < function->expr_count &&
            guard < function->expr_count; child = function->exprs[child].next_sibling, guard++) {
            if(guard) fputs(", ", stdout);
            if(function->exprs[child].argument_name[0])
                printf("%s=", function->exprs[child].argument_name);
            printf("%%%d", child);
        }
        printf(") -> %s", expression->type[0] ? expression->type : "void");
        if(expression->next_sibling >= 0) printf(" next=%%%d", expression->next_sibling);
        if(!text_in_statement(function, owners, e)) {
            fputs(" text=", stdout);
            quoted(expression->text);
        }
        putchar('\n');
        return;
    }
    if(expression->type[0]) printf(" type=%s", expression->type);
    if(expression->name[0]) printf(" name=%s", expression->name);
    if(expression->op[0]) printf(" op=%s", expression->op);
    if(expression->left >= 0) printf(" left=%%%d", expression->left);
    if(expression->right >= 0) printf(" right=%%%d", expression->right);
    if(expression->first_child >= 0)
        printf(" child=%%%d", expression->first_child);
    if(expression->next_sibling >= 0)
        printf(" next=%%%d", expression->next_sibling);
    if(expression->third >= 0)
        printf(" third=%%%d", expression->third);
    if(!text_in_statement(function, owners, e)) {
        fputs(" text=", stdout);
        quoted(expression->text);
    }
    putchar('\n');
}

static void
location(const ZirSourceSpan *span)
{
    if(SpanPath(*span)[0])
        printf(" @ %s:%d:%d", SpanPath(*span), span->line, span->column);
}

static void
show_module(const ZirModule *module)
{
    printf("module %s from %s\n", module->name, module->source_path);
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *item = &module->imports[i];
        const char *kind = item->kind == ZIR_IMPORT_EXTERN ? "foreign" :
            item->kind == ZIR_IMPORT_MODULE ? "module" : "open";
        printf("  import %s %s", kind, item->name);
        if(item->target[0]) printf(" -> %s", item->target);
        if(item->extern_symbol[0]) printf(" symbol %s", item->extern_symbol);
        location(&item->span);
        putchar('\n');
    }
    for(int i = 0; i < module->define_count; i++) {
        const ZirDefine *item = &module->defines[i];
        printf("  constant %s = ", item->name);
        quoted(item->value);
        location(&item->span);
        putchar('\n');
    }
    for(int i = 0; i < module->global_count; i++) {
        const ZirGlobal *item = &module->globals[i];
        printf("  global %s: %s", item->name, item->type);
        if(item->init[0]) {
            fputs(" = ", stdout);
            quoted(item->init);
        }
        location(&item->span);
        putchar('\n');
    }
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *item = &module->types[i];
        printf("  type %s", item->name);
        if(item->is_enum) fputs(" enum", stdout);
        if(item->is_record_template) fputs(" template", stdout);
        if(item->is_procedure_type) fputs(" procedure", stdout);
        location(&item->span);
        fputs(" = ", stdout);
        quoted(item->body);
        putchar('\n');
    }
    for(int i = 0; i < module->function_count; i++) {
        const ZirFunction *function = &module->functions[i];
        printf("  function %s(%s) -> %s", function->name, FunctionArgs(function),
               function->return_type[0] ? function->return_type : "void");
        if(function->exported) fputs(" export", stdout);
        if(function->is_extern) fputs(" foreign", stdout);
        location(&function->span);
        putchar('\n');
        for(int s = 0; s < function->stmt_count; s++) {
            const ZirStmt *statement = &function->stmts[s];
            printf("    stmt %d %s", s, StmtKindName(statement->kind));
            if(statement->name[0]) printf(" name=%s", statement->name);
            if(statement->type[0]) printf(" type=%s", statement->type);
            if(statement->expr_root >= 0)
                printf(" expr=%%%d", statement->expr_root);
            if(statement->lhs_root >= 0)
                printf(" lhs=%%%d", statement->lhs_root);
            if(statement->loop_id) printf(" loop=%d", statement->loop_id);
            if(statement->target_id)
                printf(" target=%d", statement->target_id);
            if(statement->is_else) fputs(" else", stdout);
            if(statement->text && statement->text[0]) {
                fputs("  // ", stdout);
                raw(statement->text);
            }
            putchar('\n');
        }
        int *owners = expression_owners(function);
        for(int e = 0; e < function->expr_count; e++)
            show_expression(function, owners, e);
        free(owners);
    }
}

static int
show_hex(FILE *file)
{
    unsigned char bytes[16];
    size_t count, offset = 0;
    if(fseek(file, 0, SEEK_SET) != 0) return 0;
    while((count = fread(bytes, 1, sizeof(bytes), file)) != 0) {
        printf("%08zx  ", offset);
        for(size_t i = 0; i < sizeof(bytes); i++) {
            if(i < count) printf("%02x ", bytes[i]);
            else fputs("   ", stdout);
            if(i == 7) putchar(' ');
        }
        fputs(" |", stdout);
        for(size_t i = 0; i < count; i++)
            putchar(bytes[i] >= 32 && bytes[i] < 127 ? bytes[i] : '.');
        fputs("|\n", stdout);
        offset += count;
    }
    return !ferror(file);
}

int
main(int argc, char **argv)
{
    int hex = 0, first = 1;
    SetDiagnosticFormatFromArguments(argc, argv);
    while(first < argc && argv[first][0] == '-') {
        if(!strcmp(argv[first], "--hex")) hex = 1;
        else if(!strncmp(argv[first], "--diagnostics=", 14) &&
                SetDiagnosticFormat(argv[first] + 14)) { }
        else {
            Diagnostic((ZirSourceSpan){0}, "command.arguments", "unknown inspect option: %s", argv[first]);
            return 2;
        }
        first++;
    }
    if(first == argc) {
        Diagnostic((ZirSourceSpan){0}, "command.arguments",
                   "usage: ziran inspect [--hex] [--diagnostics=text|json] file.zir|file.zx ...");
        return 2;
    }
    for(int i = first; i < argc; i++) {
        const char *path = argv[i];
        size_t length = strlen(path);
        int bundle = length > 3 && !strcmp(path + length - 3, ".zx");
        if(!PathIsIR(path) && !bundle) {
            Diagnostic(Span(path, 0, 0), "zir.input", "expected .zir or .zx file: %s", path);
            return 2;
        }
        FILE *file = fopen(path, "rb");
        if(file == NULL) {
            Diagnostic(Span(path, 0, 0), "zir.input", "cannot open %s: %s", path, strerror(errno));
            return 1;
        }
        char entry_module[ZIR_NAME_MAX] = {0}, entry_function[ZIR_NAME_MAX] = {0};
        ZxLawTable laws = {0};
        ZxAssets assets = {0};
        ZirProgram *program = bundle ? BundleRead(file, path, entry_module, sizeof(entry_module),
                                                  entry_function, sizeof(entry_function), &laws, &assets)
                                     : ProgramRead(file, path);
        if(program == NULL) {
            ZxLawTableFree(&laws);
            ZxAssetsFree(&assets);
            fclose(file);
            return 1;
        }
        printf("file %s\n", path);
        if(hex) {
            if(!show_hex(file)) {
                Diagnostic(Span(path, 0, 0), "zir.input", "cannot read %s: %s", path, strerror(errno));
                ProgramFree(program);
                fclose(file);
                return 1;
            }
        } else {
            if(bundle) {
                printf("bundle entry %s:%s\n", entry_module, entry_function);
                printf("  laws %d waivers %d assets %zu\n", laws.law_count, laws.waiver_count, assets.count);
                for(size_t a = 0; a < assets.count; a++)
                    printf("  asset %s bytes=%u\n", assets.items[a].name, assets.items[a].size);
            }
            for(int m = 0; m < program->module_count; m++)
                show_module(&program->modules[m]);
        }
        ProgramFree(program);
        ZxLawTableFree(&laws);
        ZxAssetsFree(&assets);
        fclose(file);
    }
    return ferror(stdout) ? 1 : 0;
}
