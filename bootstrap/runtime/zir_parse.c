#include "zir_memory.h"
#include "zir_parse_internal.h"
#include "compiler_context.h"
#include "compiler_source.h"

#include <errno.h>
#include <setjmp.h>
#include <string.h>

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

/* Statement-level parse recovery. The parser stops at its first syntax error;
 * to report several errors from one file, ParserWithRecovery blanks the
 * failing top-level declaration and parses the file again. Every retry keeps
 * the file's line numbers, so the extra diagnostics point at real source. A
 * recovered parse still fails the command: the blanked text only gathers
 * diagnostics and is never compiled. PARSE_RECOVERY_LIMIT caps how many
 * regions one file reports, so a damaged file cannot loop or flood output. */
#define PARSE_RECOVERY_LIMIT 8

/* The parser reports the text its spans refer to (the lowered source, with
 * Jai multiline strings folded) and the line of the error that stopped it.
 * ParserAttempt saves and restores both around each operation, so a nested
 * import parse cannot overwrite the enclosing file's. */
static _Thread_local const char *ZirParseSourceText;
static _Thread_local int ZirParseErrorLine;
/* A retry can reach the same error again when an earlier construct is still
 * unterminated. Print each distinct error once, then keep blanking. */
static _Thread_local int ZirParseRetry;
static _Thread_local int ZirParseSeenFile;
static _Thread_local int ZirParseSeenLine;
static _Thread_local const char *ZirParseSeenMessage;

void
ParseRememberSource(const char *text)
{
    ZirParseSourceText = text;
}

static const char *
LineContent(const char *line)
{
    while(*line == ' ' || *line == '\t') line++;
    return line;
}

static int
LineIsBlank(const char *line)
{
    const char *content = LineContent(line);
    return *content == 0 || *content == '\n' || *content == '\r';
}

/* A column-one line that is not a closing brace opens a top-level region. */
static int
LineOpensRegion(const char *line)
{
    if(line != LineContent(line) || LineIsBlank(line)) return 0;
    return *line != '}';
}

static const char *
PreviousLine(const char *text, const char *line)
{
    if(line <= text) return NULL;
    const char *at = line - 1;
    while(at > text && at[-1] != '\n') at--;
    return at;
}

static const char *
NextLine(const char *line)
{
    const char *at = strchr(line, '\n');
    return at ? at + 1 : NULL;
}

static const char *
LineWithNumber(const char *text, int number)
{
    if(number < 1) return NULL;
    const char *at = text;
    int line = 1;
    while(line < number && *at) {
        if(*at == '\n') line++;
        at++;
    }
    return line == number ? at : NULL;
}

static char *
CopyText(const char *text)
{
    /* Copies outlive the operation's memory scope: allocate them untracked so
     * the scope's failure cleanup cannot free what recovery still owns. */
    CompilerMemory *paused = CompilerMemoryPause();
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if(copy) memcpy(copy, text, size);
    CompilerMemoryResume(paused);
    return copy;
}

/* Blank the top-level declaration containing line: copy the source and
 * replace that declaration's characters with spaces, keeping every newline
 * so later diagnostics keep their line numbers. The declaration runs from the
 * nearest column-one line at or above the error to just before the next
 * column-one line. Directives are never blanked, since removing one changes
 * how the rest of the file parses. Returns a malloc'd copy, or NULL when the
 * error is not inside a declaration that can be removed. */
static char *
BlankDeclaration(const char *text, int line)
{
    const char *error = LineWithNumber(text, line);
    if(!error || !*error) return NULL;
    const char *start = error;
    while(!LineOpensRegion(start)) {
        const char *previous = PreviousLine(text, start);
        if(!previous) return NULL;
        start = previous;
    }
    if(*start == '#') return NULL;
    const char *end = start;
    for(;;) {
        const char *next = NextLine(end);
        if(!next || !*next) { end = text + strlen(text); break; }
        if(LineOpensRegion(next)) break;
        end = next;
    }
    char *copy = CopyText(text);
    if(!copy) return NULL;
    if(start >= end) { free(copy); return NULL; }
    for(const char *at = start; at < end; at++)
        copy[at - text] = *at == '\n' ? '\n' : ' ';
    return copy;
}

int *
ZirLawEvaluationAddress(void)
{
    return &ZirLawEvaluation;
}

double
ParseDecimalRange(const char *text, char **end, int *range_error)
{
    /* The generated scanner's adapter takes the byte pointers it emits. */
    uint8_t *at = (uint8_t *)text;
    errno = 0;
    double value = ParseDecimal(at, &at);
    if(end) *end = (char *)at;
    *range_error = errno == ERANGE;
    return value;
}

/* One parse operation. Keep the jump target live across the Ziran parser and
 * nested import calls. Each operation owns its temporary allocations and
 * source streams. On failure *failed_source receives a heap copy of the text
 * the parser's spans refer to and *error_line the reported line, or NULL and
 * zero when the failure carries no source. */
static ZirProgram *
ParserAttempt(const char *path, const char *root, const char *source,
              ZirCompileImportResolver resolver, void *context,
              const char *const *defines, int define_count, const ZirImport *site,
              char **failed_source, int *error_line)
{
    *failed_source = NULL;
    *error_line = 0;
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
    const char *outer_source = ZirParseSourceText;
    int outer_line = ZirParseErrorLine;
    ZirParseSourceText = NULL;
    ZirParseErrorLine = 0;
    ZirProgram *volatile program = NULL;
    if(setjmp(recovery->jump) == 0)
        program = ParserParseOperationBody(path, root, source, resolver, context,
                                           defines, define_count, site);
    /* The lowered source belongs to the operation's memory, which ends next;
     * copy it first so recovery can blank the failing declaration. */
    if(!program && ZirParseSourceText) *failed_source = CopyText(ZirParseSourceText);
    *error_line = program ? 0 : ZirParseErrorLine;
    ZirParseSourceText = outer_source;
    ZirParseErrorLine = outer_line;
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

ZirProgram *
ParserWithRecovery(const char *path, const char *root, const char *source,
                   ZirCompileImportResolver resolver, void *context,
                   const char *const *defines, int define_count, const ZirImport *site)
{
    char *failed = NULL;
    int line = 0;
    ZirParseSeenMessage = NULL;
    ZirProgram *program = ParserAttempt(path, root, source, resolver, context,
                                        defines, define_count, site, &failed, &line);
    if(program) return program;
    /* A syntax error was reported. Blank its declaration and parse again to
     * reach the next one. A retry that reaches the same error means an earlier
     * construct is still unterminated: the duplicate is not printed, and
     * blanking continues from the line the error names. Stop at the cap or
     * when the error is not inside a removable declaration. */
    for(int retry = 0; failed && retry < PARSE_RECOVERY_LIMIT; retry++) {
        char *blanked = BlankDeclaration(failed, line);
        free(failed);
        failed = NULL;
        if(!blanked) break;
        line = 0;
        ZirParseRetry = 1;
        program = ParserAttempt(path, root, blanked, resolver, context,
                                defines, define_count, site, &failed, &line);
        ZirParseRetry = 0;
        free(blanked);
        if(program) {
            /* The rest of the file parses. The blanked copy was only a way to
             * gather diagnostics, so the command still fails. */
            ProgramFree(program);
            break;
        }
    }
    free(failed);
    return NULL;
}

void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    DiagnosticV(Span((uint8_t *)"", 0, 0), "parse.fatal", fmt, ap);
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
    if(ZirParseRetry && ZirParseRecovery && span.file == ZirParseSeenFile &&
       span.line == ZirParseSeenLine && fmt == ZirParseSeenMessage) {
        va_end(args);
        longjmp(ZirParseRecovery->jump, 1);
    }
    ZirParseErrorLine = span.line;
    DiagnosticV(span, "parse.syntax", fmt, args);
    va_end(args);
    ZirParseSeenFile = span.file;
    ZirParseSeenLine = span.line;
    ZirParseSeenMessage = fmt;
    if(ZirParseRecovery) longjmp(ZirParseRecovery->jump, 1);
    exit(1);
}
