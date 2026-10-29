#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_serial.h"
#include "zir_stream.h"
#include "ziran_host.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int temporary_calls;

/* A checked IR input uses no temporary file. Bundle writing must work even
 * when the host cannot create one, including when stdout is a real pipe. */
FILE *tmpfile(void)
{
    temporary_calls++;
    errno = EACCES;
    return NULL;
}

static unsigned char *snapshot(const ZirProgram *program, size_t *size)
{
    unsigned char *bytes = NULL;
    FILE *out = ZirWriteMemory(&bytes, size);
    assert(out && ProgramWrite(program, out) && fclose(out) == 0);
    return bytes;
}

static void check_large_field(ZirProgram *program)
{
    /* Trailing whitespace preserves the checked statement's meaning, while
     * a single retained text field exceeds the serializer's block buffer. */
    ZirStmt *statement = &program->modules[0].functions[0].stmts[0];
    const char *original = statement->text;
    size_t length = strlen(original), padding = 70 * 1024;
    char *text = malloc(length + padding + 1);
    assert(text);
    memcpy(text, original, length);
    memset(text + length, ' ', padding); text[length + padding] = 0;
    statement->text = text;
    size_t measured = 0, written = 0, size = 0;
    unsigned char *bytes = NULL;
    FILE *memory = ZirWriteMemory(&bytes, &size);
    assert(memory && ProgramMeasurePortable(program, &measured));
    assert(ProgramWritePortable(program, memory, &written) && fclose(memory) == 0);
    assert(size == written && written == measured && size > padding);
    statement->text = original;
    free(bytes); free(text);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *input = fopen(argv[1], "rb");
    assert(input);
    ZirProgram *program = ProgramRead(input, argv[1]);
    assert(program && fclose(input) == 0);
    assert(CheckCanonicalPrograms(&program, 1, NULL));
    size_t before_size;
    unsigned char *before = snapshot(program, &before_size);
    int annotations = 0;
    for(int m = 0; m < program->module_count; m++)
        for(int f = 0; f < program->modules[m].function_count; f++) {
            const ZirFunction *fn = &program->modules[m].functions[f];
            annotations += fn->assembly_text && *fn->assembly_text;
            for(int s = 0; s < fn->stmt_count; s++) annotations += StatementHasAssembly(&fn->stmts[s]);
        }
    assert(annotations >= 2);
    check_large_field(program);
    size_t measured = 0, written = 0;
    assert(ProgramMeasurePortable(program, &measured) && measured < before_size);
    unsigned char *portable = NULL; size_t portable_size = 0;
    FILE *memory = ZirWriteMemory(&portable, &portable_size);
    assert(memory && ProgramWritePortable(program, memory, &written) && fclose(memory) == 0);
    assert(measured == written && written == portable_size);
    assert(!memmem(portable, portable_size, "NATIVE_FN_CANARY", 16));
    assert(!memmem(portable, portable_size, "NATIVE_BLOCK_CANARY", 19));
    assert(memmem(portable, portable_size, "literal #asm survives", 21));
    free(portable);

    FILE *out;
    if(!strcmp(argv[2], "pipe")) out = stdout;
    else if(!strcmp(argv[2], "readonly")) out = fopen(argv[1], "rb");
    else out = fopen(argv[3], !strcmp(argv[2], "append") ? "ab" : "wb");
    assert(out);
    static const unsigned char asset_data[] = "NATIVE_BLOCK_CANARY\0asset #asm survives";
    ZxAsset asset = {"sample.bin", (unsigned char *)asset_data, sizeof(asset_data)};
    ZxAssets assets = {&asset, 1};
    int invalid = !strcmp(argv[2], "invalid");
    int oversized = !strcmp(argv[2], "oversized");
    ZirFunction *first = &program->modules[0].functions[0];
    ZirStmt *saved_statements = first->stmts;
    int saved_count = first->stmt_count, saved_capacity = first->stmt_cap;
    char *large_text = NULL;
    if(oversized) {
        /* One shared string, written once, stands for more than the 256 MiB
         * of text a reader accepts, without a large source fixture. */
        large_text = malloc(ZIR_KEPT_TEXT_MAX);
        assert(large_text);
        memset(large_text, 'x', ZIR_KEPT_TEXT_MAX - 1); large_text[ZIR_KEPT_TEXT_MAX - 1] = 0;
        first->stmts = calloc(400, sizeof(*first->stmts)); assert(first->stmts);
        first->stmt_count = first->stmt_cap = 400;
        for(int i = 0; i < 400; i++) { first->stmts[i] = saved_statements[0]; first->stmts[i].text = large_text; }
    }
    char saved_name = program->modules[0].functions[0].name[0];
    if(invalid) program->modules[0].functions[0].name[0] = 0;
    int ok = BundleWrite(out, program, "program", "Answer", &assets);
    if(invalid) program->modules[0].functions[0].name[0] = saved_name;
    if(oversized) {
        free(first->stmts); free(large_text);
        first->stmts = saved_statements; first->stmt_count = saved_count; first->stmt_cap = saved_capacity;
    }
    int failure = invalid || oversized || !strcmp(argv[2], "readonly") || !strcmp(argv[2], "full");
    assert(ok == !failure);
    if(!failure) assert(fclose(out) == 0);
    else fclose(out);
    assert(temporary_calls == 0);
    if(!strcmp(argv[2], "file")) {
        Bundle *bundle = BundleOpen(argv[3]);
        assert(bundle && BundleLawCount(bundle) == 2 && BundleAssetCount(bundle) == 1);
        assert(BundleAssetSize(bundle, 0) == sizeof(asset_data));
        assert(!memcmp(BundleAssetData(bundle, 0), asset_data, sizeof(asset_data)));
        const ZirProgram *loaded = BundleProgram(bundle);
        int blocks = 0;
        for(int m = 0; m < loaded->module_count; m++)
            for(int f = 0; f < loaded->modules[m].function_count; f++) {
                const ZirFunction *fn = &loaded->modules[m].functions[f];
                assert(!fn->assembly_text || !*fn->assembly_text);
                for(int s = 0; s < fn->stmt_count; s++) {
                    assert(!StatementHasAssembly(&fn->stmts[s]));
                    if(fn->stmts[s].kind == ZIR_STMT_BLOCK_OPEN) {
                        assert(!strcmp(fn->stmts[s].text, "{")); blocks++;
                    }
                }
            }
        assert(blocks > 0);
        BundleInstance *instance = BundleInstantiate(bundle, NULL, 0);
        assert(instance);
        for(int i = 0; i < 2; i++) {
            long long result; int has_result;
            assert(BundleInstanceRun(instance, &result, &has_result));
            assert(has_result && result == 42 + i);
        }
        BundleInstanceClose(instance); BundleClose(bundle);
    }
    assert(temporary_calls == 0);
    size_t after_size;
    unsigned char *after = snapshot(program, &after_size);
    assert(before_size == after_size && !memcmp(before, after, before_size));
    free(before); free(after); ProgramFree(program);
    return 0;
}
