/* Native variadic formatting and Android's diagnostic sink. */
#include "zir_diagnostic.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

void *CompilerStandardError(void) { return stderr; }
void *CompilerStandardOutput(void) { return stdout; }

void
S15PlatformDiagnosticLog(int warning, const char *path, int line, int column,
                         const char *message, const char *code)
{
#ifdef __ANDROID__
    __android_log_print(warning ? ANDROID_LOG_WARN : ANDROID_LOG_ERROR, "Ziran",
                        "%s:%d:%d: %s [%s]", path, line, column, message, code);
#else
    (void)warning; (void)path; (void)line; (void)column; (void)message; (void)code;
#endif
}

void S15DiagnosticMessage(ZirSourceSpan, const char *, const char *,
                          const DiagnosticDetails *, const char *, int);

static void
report(ZirSourceSpan span, const char *severity, const char *code,
       const DiagnosticDetails *details, const char *format, va_list arguments)
{
    char message[ZIR_TEXT_MAX * 2];
    int length = vsnprintf(message, sizeof(message), format, arguments);
    if(length < 0) message[0] = '\0';
    S15DiagnosticMessage(span, severity, code, details, message, length);
}

void DiagnosticV(ZirSourceSpan span, const char *code, const char *format, va_list args)
{
    report(span, "error", code, NULL, format, args);
}

void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    DiagnosticV(span, code, format, args);
    va_end(args);
}

void DiagnosticDetailed(ZirSourceSpan span, const char *code,
                        const DiagnosticDetails *details, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(span, "error", code, details, format, args);
    va_end(args);
}

void Warning(ZirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    report(span, "warning", code, NULL, format, args);
    va_end(args);
}

void DiagnosticTarget(ZirSourceSpan span, const char *code, const char *target,
                      const char *capability, const char *format, ...)
{
    DiagnosticDetails details = {.target = target, .capability = capability,
                                 .cause_kind = "target_restriction"};
    va_list args;
    va_start(args, format);
    report(span, "error", code, &details, format, args);
    va_end(args);
}
