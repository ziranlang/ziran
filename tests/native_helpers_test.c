#include "zir_emit.h"
#include "zir_stream.h"
#include <assert.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *invalid[] = {
        "/* no dependency marks */\nint value;\n",
        NUMBER_HELPERS_MARK RUNTIME_INCLUDES_MARK "int value;\n"
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        TextStream stream;
        char *text = NULL;
        size_t size = 0;
        FILE *output = TextStreamOpen(&stream, &text, &size);
        assert(output != NULL);
        fputs("preserve", output);
        assert(EmitNativeHelpers(output, invalid[i]) == -1);
        assert(TextStreamClose(&stream) == 0);
        assert(size == 8 && !strcmp(text, "preserve"));
        free(text);
        FILE *file = fopen(argv[1], "wb");
        assert(file != NULL);
        assert(fputs(invalid[i], file) >= 0 && fclose(file) == 0);
        assert(EmitResolveNativeHelpers(argv[1]) == -1);
        file = fopen(argv[1], "rb");
        char bytes[256];
        assert(file != NULL);
        size_t length = fread(bytes, 1, sizeof(bytes), file);
        assert(!ferror(file) && fclose(file) == 0);
        assert(length == strlen(invalid[i]) && !memcmp(bytes, invalid[i], length));
    }
    TextStream stream;
    char *text = NULL;
    size_t size = 0;
    FILE *output = TextStreamOpen(&stream, &text, &size);
    assert(output != NULL);
    assert(EmitNativeHelpers(output, "/* prefix */\n" RUNTIME_INCLUDES_MARK
                             NUMBER_HELPERS_MARK "int value;\n") == 0);
    assert(TextStreamClose(&stream) == 0);
    assert(!strcmp(text, "/* prefix */\nint value;\n"));
    free(text);
    puts("Native helper resolution preserves invalid inputs and writes resolved text");
    return 0;
}
