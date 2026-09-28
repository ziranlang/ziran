#ifndef ZIR_STRING_VALUE_DEFINED
#define ZIR_STRING_VALUE_DEFINED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static inline String StringRange(String source, int64_t low, int64_t high) {
    if (low < 0 || high < low || (uint64_t)high > source.length ||
        (source.data == NULL && source.length != 0)) {
        fprintf(stderr, "ziran: string range out of bounds\n");
        abort();
    }
    return StringView(source.data == NULL ? "" : source.data + low,
                      (size_t)(high - low));
}

static inline bool StringEqual(String a, String b) {
    return a.length == b.length &&
        (a.length == 0 || memcmp(a.data, b.data, a.length) == 0);
}

/* `print` output. Floats use the shortest decimal that reads back to the
 * same value, in plain positional notation, so every target agrees. */
static inline void ZirPrintString(String text) {
    if (text.length > 0)
        fwrite(text.data, 1, text.length, stdout);
}

static inline void ZirPrintBool(bool value) {
    fputs(value ? "true" : "false", stdout);
}

static inline void ZirPrintSigned(int64_t value) {
    printf("%lld", (long long)value);
}

static inline void ZirPrintUnsigned(uint64_t value) {
    printf("%llu", (unsigned long long)value);
}

static inline void ZirPrintFloat(double value, int single) {
    char scientific[40], digits[24];
    int precision, exponent, count = 0;
    const char *p;
    if (value != value) { fputs("nan", stdout); return; }
    if (value - value != 0) { fputs(value < 0 ? "-inf" : "inf", stdout); return; }
    for (precision = 1; precision <= 17; precision++) {
        snprintf(scientific, sizeof(scientific), "%.*e", precision - 1, value);
        if (single ? strtof(scientific, NULL) == (float)value
                   : strtod(scientific, NULL) == value)
            break;
    }
    p = scientific;
    if (*p == '-') { fputc('-', stdout); p++; }
    for (; *p != 'e'; p++)
        if (*p != '.') digits[count++] = *p;
    exponent = atoi(p + 1);
    while (count > 1 && digits[count - 1] == '0') count--;
    if (exponent < 0) {
        fputs("0.", stdout);
        for (int i = 1; i < -exponent; i++) fputc('0', stdout);
        fwrite(digits, 1, (size_t)count, stdout);
    } else {
        for (int i = 0; i <= exponent; i++)
            fputc(i < count ? digits[i] : '0', stdout);
        if (count > exponent + 1) {
            fputc('.', stdout);
            fwrite(digits + exponent + 1, 1, (size_t)(count - exponent - 1), stdout);
        }
    }
}

#endif
