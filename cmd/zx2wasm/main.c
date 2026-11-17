#include "compiler_zx_wasm.h"
#include "zir.h"
#include "zir_diagnostic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { Zx2WasmInputLimit = 268435456 };

static int
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
        "usage: zx2wasm [-o FILE] INPUT.zx");
    return 2;
}

static unsigned char *
read_zx(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    unsigned char *bytes = NULL;
    if(stream == NULL || fseek(stream, 0, SEEK_END) != 0) goto failure;
    long length = ftell(stream);
    if(length < 0 || length > Zx2WasmInputLimit ||
       fseek(stream, 0, SEEK_SET) != 0) goto failure;
    bytes = malloc((size_t)length + 1);
    if(bytes == NULL) { DiagnosticOutOfMemory(); goto failure; }
    if(fread(bytes, 1, (size_t)length, stream) != (size_t)length ||
       fgetc(stream) != EOF || ferror(stream) || fclose(stream) != 0) {
        stream = NULL;
        goto failure;
    }
    *size = (size_t)length;
    return bytes;
failure:
    if(stream != NULL) fclose(stream);
    free(bytes);
    Diagnostic((ZirSourceSpan){0}, "wasm.input",
        "cannot read bounded verified ZX input: %s", path);
    return NULL;
}

static int
default_output(const char *input, char *output, size_t capacity)
{
    size_t length = strlen(input);
    size_t stem = length > 3 && !strcmp(input + length - 3, ".zx")
        ? length - 3 : length;
    if(stem + sizeof(".wasm") > capacity) return 0;
    memcpy(output, input, stem);
    memcpy(output + stem, ".wasm", sizeof(".wasm"));
    return 1;
}

int
main(int argc, char **argv)
{
    const char *output = NULL;
    const char *input = NULL;
    SetDiagnosticFormatFromArguments(argc, argv);
    for(int index = 1; index < argc; index++) {
        if(!strcmp(argv[index], "-o") && index + 1 < argc)
            output = argv[++index];
        else if(!strncmp(argv[index], "--diagnostics=", 14)) {
            if(!SetDiagnosticFormat(argv[index] + 14)) return usage();
        } else if(argv[index][0] == '-' || input != NULL)
            return usage();
        else
            input = argv[index];
    }
    if(input == NULL) return usage();
    size_t input_length = strlen(input);
    if(input_length <= 3 || strcmp(input + input_length - 3, ".zx"))
        return usage();

    char derived[ZIR_PATH_MAX];
    if(output == NULL) {
        if(!default_output(input, derived, sizeof(derived))) {
            Diagnostic((ZirSourceSpan){0}, "wasm.output",
                "derived output path is too long: %s", input);
            return 1;
        }
        output = derived;
    }

    size_t size = 0;
    unsigned char *bytes = read_zx(input, &size);
    if(bytes == NULL) return 1;
    ZxWasmImage image = {0};
    int status = 1;
    if(!compiler_zx_wasm_LowerExecutable(bytes, size, &image)) goto done;
    if(!strcmp(output, "-")) {
        if(fwrite(image.bytes, 1, image.size, stdout) == image.size &&
           fflush(stdout) == 0)
            status = 0;
        else
            Diagnostic((ZirSourceSpan){0}, "wasm.output",
                "cannot write WebAssembly to standard output");
        goto done;
    }

    char temporary[ZIR_PATH_MAX] = {0};
    FILE *stream = GeneratedOutputOpen(output, temporary, sizeof(temporary));
    if(stream == NULL) {
        Diagnostic((ZirSourceSpan){0}, "wasm.output",
            "cannot create output: %s", output);
        goto done;
    }
    int written = fwrite(image.bytes, 1, image.size, stream) == image.size;
    if(fclose(stream) != 0) written = 0;
    if(!written || GeneratedOutputReplace(temporary, output) != 0) {
        remove(temporary);
        Diagnostic((ZirSourceSpan){0}, "wasm.output",
            "cannot finish output: %s", output);
        goto done;
    }
    status = 0;
done:
    compiler_zx_wasm_ImageFree(&image);
    free(bytes);
    return status;
}
