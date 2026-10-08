#include "zir_profile.h"
#include "zir_diagnostic.h"
#include <stdio.h>
#include <stdlib.h>

ZirSourceSpan Span(const char *path, int line, int column)
{
    (void)path;
    return (ZirSourceSpan){0, line, column, line, column};
}

void Diagnostic(ZirSourceSpan span, const char *code, const char *format, ...)
{
    (void)span;
    (void)format;
    fprintf(stderr, "unexpected profile diagnostic: %s\n", code);
    exit(2);
}

int main(void)
{
    uint64_t started = ProfileStart();
    int enabled = getenv("ZIRAN_PROFILE") != NULL;
    if ((started != 0) != enabled)
        return 3;
    ProfileAllocation(4096);
    ProfileCount("portable");
    volatile unsigned long sum = 0;
    for (unsigned long i = 0; i < 100000; i++)
        sum += i;
    ProfileEnd("portable", started);
    return sum == 0 ? 4 : 0;
}
