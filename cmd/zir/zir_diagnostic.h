#ifndef ZIR_DIAGNOSTIC_H
#define ZIR_DIAGNOSTIC_H

#include "zir.h"
#include <stdarg.h>

typedef struct DiagnosticDetails {
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
void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...);
void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args);
void DiagnosticDetailed(ZirSourceSpan span, const char *code,
                        const DiagnosticDetails *details, const char *format, ...);
void DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target,
                      const char *capability, const char *format, ...);
/* A note that does not fail the command: "path:line:col: warning: ...". */
void Warning(ZirSourceSpan span, const char *code, const char *format, ...);

#endif
