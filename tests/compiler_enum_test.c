#include "zir_string.h"
#include "compiler_enum.h"
#include "zir.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static String text(const char *source)
{
    return StringView(source, strlen(source));
}

static void *concurrent_values(void *argument)
{
    intptr_t lane = (intptr_t)argument;
    ZirType *type = calloc(1, sizeof(*type));
    assert(type);
    type->is_enum = 1;
    for(int i = 0; i < 1024; i++) {
        int64_t base = lane * 61 + i % 64;
        snprintf(type->body, sizeof(type->body), "A = %lld, B = A + 2, C = B * 2",
                 (long long)base);
        strcpy(type->enum_backing, "s64");
        int64_t value = -1;
        assert(EnumMemberValue(type, "C", &value) && value == (base + 2) * 2);
        strcpy(type->enum_backing, "u8");
        int valid = (base + 2) * 2 <= 255;
        value = -1;
        assert(EnumMemberValue(type, "C", &value) == valid);
        assert(value == (valid ? (base + 2) * 2 : -1));
        /* Invalid later members must also invalidate a requested prefix. */
        assert(EnumMemberValue(type, "A", NULL) == valid);
    }
    free(type);
    return NULL;
}

int main(void)
{
    Entry entries[3] = {0};
    entries[2] = (Entry){text("sentinel"), 1234};
    const char *source = "A = 1 + 2 * 3, B = A / -1";
    Evaluation result = compiler_enum_MemberValue(
        text(source), text("s64"), text("B"), (Slice){entries, 2}, 128);
    assert(result.valid && result.found && result.value == -7 && result.count == 2);
    assert(entries[0].name.data == source && entries[0].name.length == 1);
    assert(StringEqual(entries[2].name, text("sentinel")) && entries[2].value == 1234);
    assert(!compiler_enum_MemberValue(text(source), text("s64"), text("A"),
                                     (Slice){entries, 1}, 128).valid);
    assert(!compiler_enum_MemberValue((String){0}, text("s64"), (String){0},
                                     (Slice){0}, 128).valid);
    assert(!compiler_enum_MemberValue(text("A"), text("s64"), text("A"),
                                     (Slice){0}, 128).valid);

    ZirType *type = calloc(1, sizeof(*type));
    assert(type != NULL);
    type->is_enum = 1;
    strcpy(type->enum_backing, "s64");
    strcpy(type->body, source);
    int64_t value = 99;
    assert(EnumMemberValue(type, NULL, NULL));
    assert(EnumMemberValue(type, "B", &value) && value == -7);
    assert(EnumMemberValue(type, "A", NULL));
    assert(!EnumMemberValue(type, "missing", &value) && value == -7);
    strcpy(type->body, "A = 42, B = invalid");
    assert(!EnumMemberValue(type, "A", &value) && value == -7);
    assert(!EnumMemberValue(type, NULL, NULL));
    strcpy(type->body, "A = -9223372036854775808, B = A / 2");
    assert(EnumMemberValue(type, "A", &value) && value == INT64_MIN);
    assert(EnumMemberValue(type, "B", &value) && value == INT64_MIN / 2);
    strcpy(type->enum_backing, "u8");
    strcpy(type->body, "A = 256");
    assert(!EnumMemberValue(type, NULL, NULL));
    strcpy(type->enum_backing, "s64");
    strcpy(type->body, "A = 255, B");
    assert(EnumMemberValue(type, "B", &value) && value == 256);
    strcpy(type->enum_backing, "u8");
    assert(!EnumMemberValue(type, "A", &value) && value == 256);
    strcpy(type->enum_backing, "s64");
    assert(EnumMemberValue(type, "B", &value) && value == 256);
    ZirType *relocated = malloc(sizeof(*relocated));
    assert(relocated);
    *relocated = *type;
    memset(type->body, 'X', sizeof(type->body));
    assert(!EnumMemberValue(type, "A", &value) && value == 256);
    assert(EnumMemberValue(relocated, "B", &value) && value == 256);
    free(relocated);
    type->is_enum = 0;
    strcpy(type->body, "A = 1");
    assert(!EnumMemberValue(type, NULL, NULL));
    free(type);
    pthread_t workers[8];
    for(intptr_t i = 0; i < 8; i++)
        assert(!pthread_create(&workers[i], NULL, concurrent_values, (void *)i));
    for(int i = 0; i < 8; i++) assert(!pthread_join(workers[i], NULL));
    return 0;
}
