#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
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
    /* Run from PATH, argv[0] is only "ziran"; the tools live beside the
     * executable itself. */
    static char launcher[4096];
    ssize_t length = readlink("/proc/self/exe", launcher, sizeof(launcher) - 1);
    if(length <= 0 || (size_t)length >= sizeof(launcher) - 1) {
        length = (ssize_t)strlen(argv[0]);
        if((size_t)length >= sizeof(launcher)) length = sizeof(launcher) - 1;
        memcpy(launcher, argv[0], (size_t)length);
    }
    launcher[length] = '\0';
    int code = PackageMain(StringView(launcher, (size_t)length), slice);
    free(arguments);
    return code;
}
