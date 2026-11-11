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
        assert(EmitNativeHelpers(output, invalid[i], NULL) == -1);
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
                             NUMBER_HELPERS_MARK "int value;\n", NULL) == 0);
    assert(TextStreamClose(&stream) == 0);
    assert(!strcmp(text, "/* prefix */\nint value;\n"));
    free(text);
    const char *cases[] = {
        "/* printf free StringView */\nconst char *message = \"fwrite StringLiteral abort\";\n",
        "int32_t value;\n",
        "bool enabled = true;\n",
        "String value = StringLiteral(\"hello\");\n"
    };
    const char *wanted[] = {NULL, "#include <stdint.h>", "#include <stdbool.h>", "#include \"zir_string_types.h\""};
    for(size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        char input[512];
        snprintf(input, sizeof(input), "%s%s", RUNTIME_INCLUDES_MARK, cases[i]);
        text = NULL; size = 0;
        output = TextStreamOpen(&stream, &text, &size);
        assert(output != NULL);
        assert(EmitNativeHelpers(output, input, NULL) == 0);
        assert(TextStreamClose(&stream) == 0);
        if(wanted[i]) assert(strstr(text, wanted[i]));
        else assert(!strstr(text, "#include"));
        assert(!strstr(text, "#include <stdio.h>"));
        assert(!strstr(text, "#include <stdlib.h>"));
        free(text);
    }
    puts("Native helper resolution preserves invalid inputs and writes resolved text");
    return 0;
}
