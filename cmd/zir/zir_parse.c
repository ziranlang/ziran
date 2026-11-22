#include "zir_memory.h"
#include "zir_parse_internal.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "compiler_source.h"

#include <errno.h>

_Static_assert(ZIR_STMT_UNKNOWN == StatementKind_Unknown &&
               ZIR_STMT_BLOCK_OPEN == StatementKind_BlockOpen &&
               ZIR_STMT_BLOCK_CLOSE == StatementKind_BlockClose &&
               ZIR_STMT_DECL == StatementKind_Declaration &&
               ZIR_STMT_ASSIGN == StatementKind_Assignment &&
               ZIR_STMT_EXPR == StatementKind_Expression &&
               ZIR_STMT_IF == StatementKind_If &&
               ZIR_STMT_WHILE == StatementKind_While &&
               ZIR_STMT_FOR == StatementKind_For &&
               ZIR_STMT_CASE == StatementKind_Case &&
               ZIR_STMT_RETURN == StatementKind_Return &&
               ZIR_STMT_BREAK == StatementKind_Break &&
               ZIR_STMT_CONTINUE == StatementKind_Continue &&
               ZIR_STMT_DEFER == StatementKind_Defer &&
               ZIR_STMT_UNUSED == StatementKind_Unused &&
               ZIR_STMT_UNREACHABLE == StatementKind_Unreachable &&
               ZIR_STMT_IF_CASE == StatementKind_IfCase,
               "C frontend statement kinds must match the Ziran scanner");

typedef struct ParseRecovery {
    jmp_buf jump;
    struct ParseRecovery *previous;
    CompilerMemory *memory;
    void *expression_cache;
    int law;
} ParseRecovery;

static _Thread_local ParseRecovery *ZirParseRecovery;
_Thread_local int ZirLawEvaluation;

int *
ZirLawEvaluationAddress(void)
{
    return &ZirLawEvaluation;
}

double
ParseDecimalRange(const char *text, char **end, int *range_error)
{
    errno = 0;
    double value = ParseDecimal(text, end);
    *range_error = errno == ERANGE;
    return value;
}

/* Keep the jump target live across the Ziran parser and nested import calls.
 * Each operation owns its temporary allocations and source streams. */
ZirProgram *
ParserWithRecovery(const char *path, const char *root, const char *source,
                   ZirCompileImportResolver resolver, void *context,
                   const char *const *defines, int define_count, const ZirImport *site)
{
    CompilerContext *previous = CompilerContextCurrent();
    CompilerContext *owner = previous ? previous : CompilerContextOpen();
    if(!owner) {
        DiagnosticOutOfMemory();
        return NULL;
    }
    CompilerContextSelect(owner);
    ParseRecovery *recovery = calloc(1, sizeof(*recovery));
    if(!recovery) goto failed;
    recovery->previous = ZirParseRecovery;
    recovery->law = ZirLawEvaluation;
    recovery->expression_cache = ParseCacheEnter();
    if(!recovery->expression_cache) {
        free(recovery);
        goto failed;
    }
    recovery->memory = CompilerMemoryBegin();
    if(!recovery->memory) {
        ParseCacheLeave(recovery->expression_cache);
        free(recovery);
        goto failed;
    }
    ZirParseRecovery = recovery;
    ZirProgram *program = NULL;
    if(setjmp(recovery->jump) == 0)
        program = ParserParseOperationBody(path, root, source, resolver, context,
                                           defines, define_count, site);
    ParseCacheLeave(recovery->expression_cache);
    ZirParseRecovery = recovery->previous;
    ZirLawEvaluation = recovery->law;
    CompilerMemoryEnd(recovery->memory, program != NULL);
    free(recovery);
    SymbolLookupsChanged();
    if(program && !program->context_retained) {
        CompilerContextRetain(owner);
        program->context_retained = 1;
    }
    if(program && !previous) program->owns_context = 1;
    CompilerContextSelect(previous);
    if(!program && !previous) CompilerContextClose(owner);
    return program;

failed:
    CompilerContextSelect(previous);
    if(!previous) CompilerContextClose(owner);
    DiagnosticOutOfMemory();
    return NULL;
}

void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    DiagnosticV(Span("", 0, 0), "parse.fatal", fmt, ap);
    va_end(ap);
    if(ZirParseRecovery) longjmp(ZirParseRecovery->jump, 1);
    exit(1);
}

void
parse_stop(void)
{
    if(ZirParseRecovery) longjmp(ZirParseRecovery->jump, 1);
    exit(1);
}

void
die_at(ZirSourceSpan span, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    DiagnosticV(span, "parse.syntax", fmt, args);
    va_end(args);
    if(ZirParseRecovery) longjmp(ZirParseRecovery->jump, 1);
    exit(1);
}
