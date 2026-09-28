#include "zir.h"
#include "zir_serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        printf("  function %s(%s) -> %s", function->name, function->args,
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
            fputs(" text=", stdout);
            quoted(statement->text);
            putchar('\n');
        }
        for(int e = 0; e < function->expr_count; e++) {
            const ZirExpr *expression = &function->exprs[e];
            printf("    %%%d %s", e, ExprKindName(expression->kind));
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
            if(expression->text[0]) {
                fputs(" text=", stdout);
                quoted(expression->text);
            }
            putchar('\n');
        }
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
    if(argc > 1 && strcmp(argv[1], "--hex") == 0) {
        hex = 1;
        first++;
    }
    if(first == argc) {
        fputs("usage: ziran inspect [--hex] file.zir ...\n", stderr);
        return 2;
    }
    for(int i = first; i < argc; i++) {
        const char *path = argv[i];
        if(!PathIsIR(path)) {
            fprintf(stderr, "ziran inspect: expected .zir file: %s\n", path);
            return 2;
        }
        FILE *file = fopen(path, "rb");
        if(file == NULL) {
            perror(path);
            return 1;
        }
        ZirProgram *program = ProgramRead(file, path);
        if(program == NULL) {
            fclose(file);
            return 1;
        }
        printf("file %s\n", path);
        if(hex) {
            if(!show_hex(file)) {
                perror(path);
                ProgramFree(program);
                fclose(file);
                return 1;
            }
        } else {
            for(int m = 0; m < program->module_count; m++)
                show_module(&program->modules[m]);
        }
        ProgramFree(program);
        fclose(file);
    }
    return ferror(stdout) ? 1 : 0;
}
