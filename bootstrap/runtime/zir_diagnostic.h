#ifndef ZIR_DIAGNOSTIC_H
#define ZIR_DIAGNOSTIC_H

#include "zir.h"
#include <stdarg.h>

typedef struct DiagnosticDetails {
    const char *semantic_id;
    const char *cause_kind; /* fixed taxonomy, never inferred from the message */
    const char *subject;
    const struct ZirDocument *document;
    const char *related_relation;
    const char *expected_type;
    const char *actual_type;
    ZirSourceSpan related_span;
    const char *related_message;
    const char *target;
    const char *capability;
    const char *suggested_name;
    ZirSourceSpan edit_span;
    const char *replacement;
    const char *original;
    const char *edit_message;
} DiagnosticDetails;

int SetDiagnosticFormat(const char *format);
void SetDiagnosticFormatFromArguments(int argc, char *const *argv);
int DiagnosticJsonEnabled(void);
void DiagnosticOutOfMemory(void);
/* Editor collection: while enabled, every diagnostic also appends one compact
 * JSON entry to a bounded buffer. EndCollection returns a malloc(d),
 * comma-joined entry list (empty string when none) and disables collection;
 * the caller frees the result. The streamed output keeps its exact shape. */
void ZiranDiagnosticsBeginCollection(void);
char *ZiranDiagnosticsEndCollection(void);
void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...);
void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args);
void DiagnosticDetailed(ZirSourceSpan span, const char *code,
                        const DiagnosticDetails *details, const char *format, ...);
void DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target,
                      const char *capability, const char *format, ...);
/* A note that does not fail the command: "path:line:col: warning: ...". */
void Warning(ZirSourceSpan span, const char *code, const char *format, ...);
/* Error diagnostics this process has reported. */
int DiagnosticErrorCount(void);
/* Count an error another process, such as a forked worker, already reported. */
void DiagnosticCountReportedError(void);
/* A failing command always says why. Pass its exit status on return: a
 * failure that reported no error gets one naming the command. */
int DiagnosticEnsureFailureReported(int status, const char *command);

#endif
