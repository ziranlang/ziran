#ifndef ZIR_DIAGNOSTIC_H
#define ZIR_DIAGNOSTIC_H

#include "zir.h"
#include <stdarg.h>

int SetDiagnosticFormat(const char *format);
void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...);
void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args);
/* A note that does not fail the command: "path:line:col: warning: ...". */
void Warning(ZirSourceSpan span, const char *code, const char *format, ...);

#endif
