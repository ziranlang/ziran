#include "zir_parse.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compare parsed payloads, preserving every byte inside quoted literals. */
static char *payload(const char *text)
{
    char *result = malloc(strlen(text) + 1);
    assert(result);
    size_t count = 0;
    int quote = 0;
    while(*text) {
        unsigned char byte = (unsigned char)*text++;
        if(quote) {
            result[count++] = (char)byte;
            if(byte == '\\' && *text) result[count++] = *text++;
            else if(byte == quote) quote = 0;
        } else if(byte == '"' || byte == '\'') {
            quote = byte;
            result[count++] = (char)byte;
        } else if(!isspace(byte)) result[count++] = (char)byte;
    }
    result[count] = 0;
    return result;
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    ZirProgram *before = parse_file(argv[1], argv[3]);
    ZirProgram *after = parse_file(argv[2], argv[3]);
    assert(before && after && before->module_count == 1 && after->module_count == 1);
    const ZirModule *left = &before->modules[0], *right = &after->modules[0];
    assert(left->law_count > 0 && left->law_count == right->law_count);
    for(int i = 0; i < left->law_count; i++) {
        assert(!strcmp(left->laws[i].name, right->laws[i].name));
        assert(!strcmp(left->laws[i].kind, right->laws[i].kind));
        char *a = payload(left->laws[i].payload), *b = payload(right->laws[i].payload);
        assert(!strcmp(a, b));
        free(a);
        free(b);
    }
    printf("Parsed-law equivalence: %d names, kinds, domains and predicates unchanged\n", left->law_count);
    ProgramFree(before);
    ProgramFree(after);
    return 0;
}
