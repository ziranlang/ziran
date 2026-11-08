/* The C services cmd/compiler_services.zi declares for the Ziran compiler
 * modules. A Ziran string arrives as a String view with no closing NUL. */
#include "zir.h"
#include "zir_diagnostic.h"
#include "zir_memory.h"

#include <stdlib.h>
#include <string.h>

static char *
terminated(String text)
{
    char *copy = AllocateOrExit(text.length + 1);
    if(text.length) memcpy(copy, text.data, text.length);
    copy[text.length] = '\0';
    return copy;
}

static const char *
keep_with(String text, const char *(*keep)(const char *))
{
    char *copy = terminated(text);
    const char *kept = keep(copy);
    free(copy);
    return kept;
}

const char *ZiranKeepText(String text) { return keep_with(text, KeepText); }
const char *ZiranKeepName(String text) { return keep_with(text, KeepName); }
const char *ZiranKeepParameters(String text) { return keep_with(text, KeepParameters); }

void
ZiranDiagnose(ZirSourceSpan span, String code, String message)
{
    char *kept_code = terminated(code), *kept_message = terminated(message);
    Diagnostic(span, kept_code, "%s", kept_message);
    free(kept_code);
    free(kept_message);
}

ZirImport *
ZiranAddImport(ZirModule *module, ZirImportKind kind, String name,
               String target, String signature, ZirSourceSpan span)
{
    /* The copies outlive the import array, which this call may move. */
    char *kept_name = terminated(name), *kept_target = terminated(target),
         *kept_signature = terminated(signature);
    ZirImport *import = ModuleAddImport(module, kind, kept_name, kept_target,
                                        kept_signature, 0, span);
    free(kept_name);
    free(kept_target);
    free(kept_signature);
    return import;
}

/* Saved IR (cmd/compiler_serial.zi). */
int
ZiranSourceFile(String path)
{
    char *copy = terminated(path);
    int file = SourceFile(copy);
    free(copy);
    return file;
}

void *
ZiranAllocate(int64_t count, int64_t size)
{
    if(count <= 0 || size <= 0 || (uint64_t)count > SIZE_MAX / (uint64_t)size) return NULL;
    return calloc((size_t)count, (size_t)size);
}

bool
ZiranWriteBytes(void *file, String bytes)
{
    return bytes.length == 0 ||
           fwrite(bytes.data, 1, (size_t)bytes.length, file) == (size_t)bytes.length;
}

bool
ZiranMapType(const ZirModule *module, String name)
{
    char *copy = terminated(name);
    int valid = MapTypeParts(module, copy, NULL, 0, NULL, 0);
    free(copy);
    return valid;
}

bool
ZiranVecType(const ZirModule *module, String name)
{
    char *copy = terminated(name);
    int valid = VecElementType(module, copy, NULL, 0);
    free(copy);
    return valid;
}
