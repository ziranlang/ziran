#include "zir_diagnostic.h"
#include "zir_document.h"
#include <assert.h>
#include <stdlib.h>
const ZirDocument *DocumentFind(const char *path) { (void)path; return NULL; }
const ZirDocument *DocumentWithSuffix(const char *path) { (void)path; return NULL; }
int DocumentOffset(const ZirDocument *doc, int line, int column, size_t *offset) { (void)doc; (void)line; (void)column; (void)offset; return 0; }
static int prohibit_allocation;
void *AllocateOrExit(size_t size) { assert(!prohibit_allocation); return malloc(size); }
const char *SpanPath(ZirSourceSpan span) { (void)span; return ""; }
int main(int argc, char **argv) {
    SetDiagnosticFormat("json");
    /* A failure that reported nothing gets one diagnostic; success, a failure
     * a worker process reported, and a reported failure add none. */
    if(argc > 1) {
        if(DiagnosticEnsureFailureReported(0, "probe") != 0) return 2;
        if(argv[1][0] == 'w') DiagnosticCountReportedError();
        return DiagnosticEnsureFailureReported(1, "probe");
    }
    Diagnostic((ZirSourceSpan){0}, "command.arguments", "%s", "\xf0\x9f\x99\x82\xff\xc2\xe0\x80\x80\xed\xa0\x80\xf4\x90\x80\x80");
    prohibit_allocation = 1;
    DiagnosticOutOfMemory();
    return DiagnosticEnsureFailureReported(1, "probe");
}
