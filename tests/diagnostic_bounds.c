#include "zir_diagnostic.h"
#include "zir_document.h"
#include "zir_parse.h"
#include "zir_check.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    SetDiagnosticFormat("json");
    char *large=malloc(200001); assert(large); memset(large, 1, 200000); large[200000]=0;
    ZirSourceSpan span=SpanEnd("bounds.zi",1,1,1,2);
    const ZirDocument *doc=DocumentRecord("bounds.zi","x",1); assert(doc);
    DiagnosticDetails details={.expected_type=large,.actual_type="s32",.document=doc,
        .related_span=span,.related_message=large,.edit_span=span,.original="x",.replacement=large};
    DiagnosticDetailed(span,"check.type",&details,"%s",large);
    Diagnostic(span,"check.type","%s","🙂é");
    free(large);
    FILE *file=fopen("stale.zi","wb"); assert(file);
    fputs("main :: () {}",file); assert(!fclose(file));
    ZirProgram *program=parse_source_text("stale.zi", "main :: () { count := 1; print(\"%\", cuont); }"); assert(program);
    assert(!CheckPrograms(&program,1)); ProgramFree(program);
    return 0;
}
