#include "zir_emit.h"
#include "zir_stream.h"
#include <assert.h>
#include <string.h>

int main(void)
{
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
