#include "zir_diagnostic.h"
#include "zir_document.h"
#include <assert.h>
#include <stdlib.h>
const ZirDocument *DocumentFind(const char *path) { (void)path; return NULL; }
int DocumentOffset(const ZirDocument *doc, int line, int column, size_t *offset) { (void)doc; (void)line; (void)column; (void)offset; return 0; }
static int prohibit_allocation;
void *AllocateOrExit(size_t size) { assert(!prohibit_allocation); return malloc(size); }
const char *SpanPath(ZirSourceSpan span) { (void)span; return ""; }
int main(void) {
    SetDiagnosticFormat("json");
    Diagnostic((ZirSourceSpan){0}, "command.arguments", "%s", "\xf0\x9f\x99\x82\xff\xc2\xe0\x80\x80\xed\xa0\x80\xf4\x90\x80\x80");
    prohibit_allocation = 1;
    DiagnosticOutOfMemory();
    return 1;
}
