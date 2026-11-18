/* The C services cmd/compiler_services.zi declares for the Ziran compiler
 * modules. A Ziran string arrives as a String view with no closing NUL. */
#include "zir.h"
#include "zir_body.h"
#include "zir_scratch.h"
#include "compiler_portable_body.h"
#include "zir_diagnostic.h"
#include "zir_memory.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Native stdio details used by the saved-IR inspector. */
int ZiranInspectRewind(FILE *file) { return fseek(file, 0, SEEK_SET); }
const char *ZiranInspectIOError(void) { return strerror(errno); }
int ZiranInspectOutputError(void) { return ferror(stdout); }

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

/* The typed-body checker uses one thread-local memo per invocation and
 * reuses the context's cleared expression marks between calls. */
#define BODY_MEMO_SLOTS 512

typedef struct BodyMemoSlot {
    const ZirModule *module;
    const char *type;
    unsigned pass;
    int answer;
} BodyMemoSlot;

typedef struct BodyMemo {
    unsigned pass;
    BodyMemoSlot slots[BODY_MEMO_SLOTS];
} BodyMemo;

static _Thread_local BodyMemo body_memo;

int
MemoPortableBodyType(const ZirModule *module, const char *type)
{
    BodyMemo *memo = &body_memo;
    uintptr_t key = ((uintptr_t)type >> 3) ^ ((uintptr_t)module >> 6);
    BodyMemoSlot *slot = &memo->slots[(key ^ (key >> 9)) % BODY_MEMO_SLOTS];
    if(slot->pass == memo->pass && slot->type == type && slot->module == module)
        return slot->answer;
    int answer = CanEmitTypeZiran((ZirModule *)module, (uint8_t *)type);
    *slot = (BodyMemoSlot){module, type, memo->pass, answer};
    return answer;
}

typedef struct BodySupport {
    size_t capacity;
    uint8_t marks[];
} BodySupport;

int
CanEmitBody(const ZirModule *module, const ZirFunction *fn)
{
    if(!fn->checked || fn->is_extern || fn->expr_count < 0) return 0;
    static const char key;
    static _Thread_local CompilerScratchCache cache;
    CompilerScratch *scratch = CompilerScratchCached(&cache, &key, free);
    BodySupport *support = CompilerMemoryRecovering() ? NULL : CompilerScratchTake(scratch);
    size_t count = (size_t)fn->expr_count;
    if(support == NULL || count > support->capacity) {
        free(support);
        support = AllocateOrExit(sizeof(*support) + count);
        support->capacity = count;
    }
    if(count != 0) memset(support->marks, 0, count);
    BodyMemo *memo = &body_memo;
    if(++memo->pass == 0) {
        memset(memo->slots, 0, sizeof(memo->slots));
        memo->pass = 1;
    }
    int answer = CanEmitBodyZiran((ZirModule *)module, (ZirFunction *)fn, support->marks);
    if(CompilerMemoryRecovering() || !CompilerScratchPut(scratch, support))
        free(support);
    return answer;
}
