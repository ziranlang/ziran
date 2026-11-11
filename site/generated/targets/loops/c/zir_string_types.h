#ifndef ZIR_STRING_VALUE_DEFINED
#define ZIR_STRING_VALUE_DEFINED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Immutable borrowed UTF-8 bytes; length preserves embedded nulls. */
typedef struct String { const char *data; size_t length; } String;
typedef struct Source_Code_Location {
    String fully_pathed_filename;
    int64_t line_number;
} Source_Code_Location;

static inline String StringView(const char *data, size_t length) {
    String value = {data, length};
    return value;
}

/* Keep long literals in generated expressions once, including embedded NUL. */
#define StringLiteral(text) StringView((text), sizeof(text) - 1)

#endif
