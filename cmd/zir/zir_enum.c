#include "zir.h"
#include "compiler_enum.h"
#include "zir_profile.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* The C IR owns immutable evaluated snapshots. Enum syntax, arithmetic and
 * backing validation remain in cmd/compiler_enum.zi. */
typedef struct EnumValues {
    const char *body, *backing;
    Entry *entries;
    size_t count;
    int valid;
} EnumValues;

static struct {
    EnumValues **slots;
    size_t count, capacity;
} enum_values;
static atomic_flag enum_values_lock = ATOMIC_FLAG_INIT;

static size_t
enum_slot(EnumValues **slots, size_t capacity,
          const char *body, const char *backing)
{
    size_t slot = (((uintptr_t)body >> 3) * 31 +
                   ((uintptr_t)backing >> 3)) & (capacity - 1);
    while(slots[slot] &&
          (slots[slot]->body != body || slots[slot]->backing != backing))
        slot = (slot + 1) & (capacity - 1);
    return slot;
}

static void
free_enum_values(void)
{
    for(size_t i = 0; i < enum_values.capacity; i++) {
        EnumValues *values = enum_values.slots[i];
        if(values) {
            free(values->entries);
            free(values);
        }
    }
    free(enum_values.slots);
    memset(&enum_values, 0, sizeof(enum_values));
}

/* The lock protects publication and table growth. Entries borrow interned
 * bytes, so a source rewrite, relocated type or freed program cannot leave
 * stale values or dangling member names in a published snapshot. */
static EnumValues *
values_of(const ZirType *type)
{
    size_t length = strnlen(type->body, sizeof(type->body));
    size_t backing_length = strnlen(type->enum_backing, sizeof(type->enum_backing));
    if(length == sizeof(type->body) || backing_length == sizeof(type->enum_backing))
        return NULL;
    const char *body = KeepText(type->body);
    const char *backing = KeepText(type->enum_backing);
    if(enum_values.count * 2 >= enum_values.capacity) {
        size_t capacity = enum_values.capacity ? enum_values.capacity * 2 : 128;
        EnumValues **slots = calloc(capacity, sizeof(*slots));
        if(slots == NULL) return NULL;
        for(size_t i = 0; i < enum_values.capacity; i++) {
            EnumValues *values = enum_values.slots[i];
            if(values)
                slots[enum_slot(slots, capacity, values->body, values->backing)] = values;
        }
        if(enum_values.capacity == 0) atexit(free_enum_values);
        free(enum_values.slots);
        enum_values.slots = slots;
        enum_values.capacity = capacity;
    }
    size_t slot = enum_slot(enum_values.slots, enum_values.capacity, body, backing);
    if(enum_values.slots[slot]) {
        ProfileCount("enum_values_reuse");
        return enum_values.slots[slot];
    }
    Entry *entries = malloc((length + 1) * sizeof(*entries));
    if(entries == NULL) return NULL;
    ProfileCount("enum_value_parse");
    Evaluation result = compiler_enum_MemberValue(
        StringView(body, length), StringView(backing, backing_length),
        (String){0}, (Slice){entries, (int64_t)(length + 1)}, ZIR_NAME_MAX);
    EnumValues *values = calloc(1, sizeof(*values));
    if(values == NULL) { free(entries); return NULL; }
    values->body = body;
    values->backing = backing;
    values->valid = result.valid;
    if(result.valid) {
        values->count = (size_t)result.count;
        values->entries = malloc(values->count * sizeof(*values->entries));
        if(values->entries == NULL) { free(values); free(entries); return NULL; }
        memcpy(values->entries, entries, values->count * sizeof(*entries));
    }
    free(entries);
    enum_values.slots[slot] = values;
    enum_values.count++;
    return values;
}

int
EnumMemberValue(const ZirType *type, const char *wanted, int64_t *value)
{
    if(!type->is_enum)
        return 0;
    while(atomic_flag_test_and_set_explicit(&enum_values_lock, memory_order_acquire)) {}
    const EnumValues *values = values_of(type);
    atomic_flag_clear_explicit(&enum_values_lock, memory_order_release);
    if(values == NULL || !values->valid) return 0;
    if(wanted == NULL) return 1;
    String name = StringView(wanted, strlen(wanted));
    for(size_t i = 0; i < values->count; i++)
        if(StringEqual(values->entries[i].name, name)) {
            if(value != NULL) *value = values->entries[i].value;
            return 1;
        }
    return 0;
}
