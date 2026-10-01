#include "zir.h"
#include "compiler_enum.h"

#include <stdlib.h>
#include <string.h>

/* The C IR owns the declaration and workspace. All enum syntax, arithmetic,
 * backing validation, and member lookup live in cmd/compiler_enum.zi. */
int
EnumMemberValue(const ZirType *type, const char *wanted, int64_t *value)
{
    if(!type->is_enum)
        return 0;
    size_t length = strlen(type->body);
    size_t capacity = length + 1;
    Entry *entries = calloc(capacity, sizeof(*entries));
    if(entries == NULL)
        return 0;
    Evaluation result = compiler_enum_MemberValue(
        StringView(type->body, length),
        StringView(type->enum_backing, strlen(type->enum_backing)),
        wanted != NULL ? StringView(wanted, strlen(wanted)) : (String){0},
        (Slice){entries, (int64_t)capacity}, ZIR_NAME_MAX);
    free(entries);
    int valid = result.valid && (wanted == NULL || result.found);
    if(valid && wanted != NULL && value != NULL)
        *value = result.value;
    return valid;
}
