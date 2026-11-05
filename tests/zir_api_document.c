#include "zir_document.h"
#include "zir_parse.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char source[] = "// é\nAnswer :: () -> s32 { return 42 }\n";
    for(int run = 0; run < 32; run++) {
        CompilerContext *context = CompilerContextOpen();
        assert(context != NULL);
        CompilerContext *previous = CompilerContextSelect(context);
        ZirProgram *program = parse_source_text("snapshot.zi", source);
        assert(program != NULL && program->module_count == 1);
        const ZirDocument *document = program->modules[0].document;
        assert(document && document->bytes == sizeof(source)-1);
        assert(document == DocumentFind("snapshot.zi"));
        size_t offset;
        assert(DocumentOffset(document, 2, 1, &offset) && offset == 6);
        assert(!DocumentOffset(document, 0, 1, &offset));
        assert(!DocumentOffset(document, 1, 100, &offset));
        char hash[65]; DocumentDigest("abc", 3, hash);
        assert(!strcmp(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        /* Identity remains available if a large offset table is omitted. */
        size_t count = 140000;
        char *lines = malloc(count); assert(lines != NULL); memset(lines, '\n', count);
        const ZirDocument *large = DocumentRecord("large.zi", lines, count);
        assert(large && large->bytes == count && large->line_offsets == NULL);
        assert(!DocumentOffset(large, 1, 1, &offset));
        free(lines);
        const char windows[] = "// é\r\nAnswer :: () -> s32 { return 42 }\r\n";
        const ZirDocument *crlf = DocumentRecord("crlf.zi", windows, sizeof(windows)-1);
        assert(crlf && DocumentOffset(crlf, 2, 1, &offset) && offset == 7);
        const ZirDocument *empty = DocumentRecord("empty.zi", "", 0);
        assert(empty && DocumentOffset(empty, 1, 1, &offset) && offset == 0);
        assert(!DocumentOffset(empty, 2, 1, &offset));
        int before = SourceFile("lookup-before.zi");
        char missing[64];
        for(int i = 0; i < 128; i++) {
            snprintf(missing, sizeof(missing), "missing-%d.zi", i);
            assert(DocumentFind(missing) == NULL);
        }
        assert(SourceFile("lookup-after.zi") == before + 1);
        char version[32]; const ZirDocument *last = NULL;
        for(int i = 0; i < 4092; i++) {
            int n = snprintf(version, sizeof(version), "version %d", i);
            last = DocumentRecord("versions.zi", version, (size_t)n);
            assert(last);
            if(i) assert(last->file == DocumentFind("versions.zi")->file);
        }
        assert(!DocumentRecord("versions.zi", "excess", 6));
        ProgramFree(program);
        CompilerContextSelect(previous);
        assert(CompilerContextClose(context));
    }
    return 0;
}
