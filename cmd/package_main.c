#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "package.h"

#ifndef ZIRAN_VERSION
#error "build with -DZIRAN_VERSION from the VERSION file"
#endif

int main(int argc, char **argv)
{
    if(argc == 2 && (strcmp(argv[1], "version") == 0 ||
                     strcmp(argv[1], "--version") == 0)) {
        printf("ziran %s\n", ZIRAN_VERSION);
        return 0;
    }
    String *arguments = calloc((size_t)(argc > 1 ? argc - 1 : 1),
                               sizeof(*arguments));
    if(!arguments) return 2;
    for(int i = 1; i < argc; i++)
        arguments[i - 1] = StringView(argv[i], strlen(argv[i]));
    Slice slice = {arguments, argc - 1};
    int code = PackageMain(StringView(argv[0], strlen(argv[0])), slice);
    free(arguments);
    return code;
}
