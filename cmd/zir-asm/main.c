#include "zir_asm_lower.h"
#include "zir_check.h"
#include "compiler_lowered_verify.h"
#include "compiler_lowered_x64.h"
#include "compiler_exec_serial.h"
#include "compiler_exec.h"
#include "zir_command.h"
#include "zir_serial.h"

#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

static int
assembly_usage(void)
{
    fputs("usage: zi2asm --root DIR --entry MODULE:FUNCTION [-I DIR] [--emit lir|map|exec|elf] -o FILE INPUT...\n"
          "       zi2asm --input exec [--emit lir|map|exec|elf] -o FILE INPUT\n"
          "       zi2asm --run [--fuel N] [--work N] [--frames N] INPUT...\n"
          "experimental direct backend: scalar integers/bools/floats; Linux x86-64 ELF\n", stderr);
    return 2;
}

static int
assembly_limit(const char *text, int64_t *value)
{
    if(text == NULL) return 1;
    if(*text == 0) return 0;
    int64_t amount = 0;
    for(const char *cursor = text; *cursor; cursor++) {
        if(*cursor < '0' || *cursor > '9') return 0;
        int digit = *cursor - '0';
        if(amount > (INT64_MAX - digit) / 10) return 0;
        amount = amount * 10 + digit;
    }
    *value = amount;
    return 1;
}

static int
assembly_run(const LirProgram *program, LirExecLimits limits)
{
    if(program->entry < 0 || program->entry >= program->functions.length) {
        Diagnostic((ZirSourceSpan){0}, "assembly.execute", "instruction input has no entry procedure");
        return 1;
    }
    LirFunction entry = ((LirFunction *)program->functions.data)[program->entry];
    LirType result_type = ((LirType *)program->types.data)[entry.result];
    if(entry.parameters.length != 0 || (result_type.kind != LirTypeKind_Integer &&
       result_type.kind != LirTypeKind_Bool && result_type.kind != LirTypeKind_Void)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.execute", "command entry must have no parameters and return void, bool or an integer");
        return 1;
    }
    LirExecPlan plan = {0};
    LirExecInstance instance = {0};
    LirExecResult result = compiler_exec_LirExecPlanBuild(*program, compiler_lowered_ir_LirDefaultLimits(), &plan);
    if(result.problem == LirExecProblem_None)
        result = compiler_exec_LirExecInstanceOpen(&plan, limits, 1, &instance);
    if(result.problem == LirExecProblem_None) result = compiler_exec_LirExecRun(&instance);
    int status = result_type.kind == LirTypeKind_Void ? 0 : (int)(result.value.bits & 255);
    if(result.problem != LirExecProblem_None) {
        String problem = compiler_exec_LirExecProblemName(result.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.execute",
            "instruction execution failed: problem=%.*s function=%d block=%d instruction=%d origin=%d trap=%llu work_remaining=%lld fuel_remaining=%lld",
            (int)problem.length, problem.data, result.function, result.block, result.instruction, result.origin,
            (unsigned long long)result.trap, (long long)result.work_remaining, (long long)result.fuel_remaining);
        status = 125;
    }
    compiler_exec_LirExecInstanceClose(&instance);
    compiler_exec_LirExecPlanFree(&plan);
    return status;
}

static unsigned char *
assembly_read(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    unsigned char *bytes = NULL;
    if(stream == NULL) goto failure;
    if(fseek(stream, 0, SEEK_END) != 0) goto failure;
    long length = ftell(stream);
    if(length < 0 || length > 268435456 || fseek(stream, 0, SEEK_SET) != 0) goto failure;
    bytes = malloc((size_t)length + 1);
    if(bytes == NULL) { DiagnosticOutOfMemory(); goto failure; }
    if(fread(bytes, 1, (size_t)length, stream) != (size_t)length ||
       fgetc(stream) != EOF || ferror(stream)) goto failure;
    if(fclose(stream) != 0) { stream = NULL; goto failure; }
    *size = (size_t)length;
    return bytes;
failure:
    if(stream) fclose(stream);
    free(bytes);
    Diagnostic((ZirSourceSpan){0}, "assembly.codec", "cannot read bounded instruction input: %s", path);
    return NULL;
}

static void
assembly_json(FILE *stream, String value)
{
    const unsigned char *bytes = (const unsigned char *)value.data;
    fputc('"', stream);
    for(size_t index = 0; index < value.length; index++) {
        unsigned char byte = bytes[index];
        if(byte == '"' || byte == '\\') fprintf(stream, "\\%c", byte);
        else if(byte < 32) fprintf(stream, "\\u%04x", byte);
        else fputc(byte, stream);
    }
    fputc('"', stream);
}

typedef struct AssemblyFunctionExtent {
    int64_t begin;
    int64_t end;
    int seen;
} AssemblyFunctionExtent;

static int
assembly_map(const LirProgram *program, FILE *stream)
{
    LirNativeResult measured = compiler_lowered_x64_LirX64Ranges(*program, (Slice){0});
    if(measured.problem != LirProblem_None || measured.size <= 0 ||
       measured.size > 268435456 / (int64_t)sizeof(LirNativeRange)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "cannot measure native map: problem=%d", measured.problem);
        return 0;
    }
    LirNativeRange *ranges = malloc((size_t)measured.size * sizeof(*ranges));
    if(ranges == NULL) { DiagnosticOutOfMemory(); return 0; }
    LirNativeResult mapped = compiler_lowered_x64_LirX64Ranges(*program, (Slice){ranges, measured.size});
    if(mapped.problem != LirProblem_None || mapped.size != measured.size) {
        free(ranges);
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "cannot write native map: problem=%d", mapped.problem);
        return 0;
    }
    AssemblyFunctionExtent *extents = calloc((size_t)program->functions.length, sizeof(*extents));
    if(extents == NULL) { free(ranges); DiagnosticOutOfMemory(); return 0; }
    for(int64_t index = 0; index < mapped.size; index++) {
        LirNativeRange range = ranges[index];
        if(range.function < 0) continue;
        if(range.function >= program->functions.length) {
            free(extents);
            free(ranges);
            Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map contains an invalid function identity");
            return 0;
        }
        AssemblyFunctionExtent *extent = &extents[range.function];
        if(!extent->seen || range.begin < extent->begin) extent->begin = range.begin;
        if(!extent->seen || range.end > extent->end) extent->end = range.end;
        extent->seen = 1;
    }
    fputs("{\"format\":\"ziran-native-map\",\"version\":1,\"target\":\"x86_64-linux\","
          "\"coordinate\":\"text-relative\",\"ranges\":[", stream);
    for(int64_t index = 0; index < mapped.size; index++) {
        LirNativeRange range = ranges[index];
        if(index) fputc(',', stream);
        fprintf(stream, "{\"begin\":%lld,\"end\":%lld,\"function_id\":%d,\"block\":%d,\"instruction\":%d,\"origin\":%d",
            (long long)range.begin, (long long)range.end, range.function, range.block, range.instruction, range.origin);
        if(range.function >= 0) {
            LirFunction function = ((LirFunction *)program->functions.data)[range.function];
            fputs(",\"module\":", stream); assembly_json(stream, ((String *)program->modules.data)[function.module]);
            fputs(",\"function\":", stream); assembly_json(stream, function.name);
            if(range.block >= 0) {
                LirBlock block = ((LirBlock *)function.blocks.data)[range.block];
                LirInstruction instruction = ((LirInstruction *)block.instructions.data)[range.instruction];
                fputs(",\"op\":", stream);
                assembly_json(stream, compiler_lowered_ir_LirOpName(instruction.op));
            }
        }
        if(range.origin >= 0) {
            LirOrigin origin = ((LirOrigin *)program->origins.data)[range.origin];
            fputs(",\"file\":", stream); assembly_json(stream, ((String *)program->sources.data)[origin.file]);
            fprintf(stream, ",\"line\":%d,\"column\":%d", origin.line, origin.column);
        }
        fputc('}', stream);
    }
    fprintf(stream, "],\"text_size\":%lld,\"functions\":[", (long long)ranges[mapped.size - 1].end);
    LirFunction *functions = program->functions.data;
    for(int64_t index = 0; index < program->functions.length; index++) {
        LirFunction function = functions[index];
        if(index) fputc(',', stream);
        fprintf(stream, "{\"id\":%lld,\"module_id\":%d,\"module\":", (long long)index, function.module);
        assembly_json(stream, ((String *)program->modules.data)[function.module]);
        fputs(",\"name\":", stream); assembly_json(stream, function.name);
        fprintf(stream, ",\"entry\":%d,\"origin\":%d,\"emitted\":%s", function.entry, function.origin,
            extents[index].seen ? "true" : "false");
        if(extents[index].seen)
            fprintf(stream, ",\"begin\":%lld,\"end\":%lld}", (long long)extents[index].begin, (long long)extents[index].end);
        else fputs(",\"begin\":null,\"end\":null}", stream);
    }
    fputs("],\"origins\":[", stream);
    LirOrigin *origins = program->origins.data;
    for(int64_t index = 0; index < program->origins.length; index++) {
        LirOrigin origin = origins[index];
        if(index) fputc(',', stream);
        fprintf(stream, "{\"id\":%lld,\"file_id\":%d,\"file\":", (long long)index, origin.file);
        assembly_json(stream, ((String *)program->sources.data)[origin.file]);
        fprintf(stream, ",\"line\":%d,\"column\":%d,\"begin\":%lld,\"end\":%lld,\"parent\":%d}",
            origin.line, origin.column, (long long)origin.begin, (long long)origin.end, origin.parent);
    }
    fputs("]}\n", stream);
    free(extents);
    free(ranges);
    return !ferror(stream);
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *entry = NULL;
    const char *output = NULL;
    const char *emit = "elf";
    const char *input_kind = "source";
    const char *fuel = NULL;
    const char *work = NULL;
    const char *frames = NULL;
    int run = 0;
    const char **module_paths = CommandModulePaths(argc);
    int module_path_count = 0;
    if(module_paths == NULL) return 1;
    SetDiagnosticFormatFromArguments(argc, argv);
    CommandLowerPriority();
    CommandOption options[] = {
        COMMAND_TEXT("--root", root), COMMAND_TEXT("--entry", entry),
        COMMAND_TEXT("-o", output), COMMAND_TEXT("--emit", emit),
        COMMAND_TEXT("--input", input_kind),
        COMMAND_FLAG("--run", run), COMMAND_TEXT("--fuel", fuel),
        COMMAND_TEXT("--work", work), COMMAND_TEXT("--frames", frames),
        COMMAND_LIST("--module-path", module_paths, module_path_count, argc, NULL),
        COMMAND_LIST("-I", module_paths, module_path_count, argc, NULL)
    };
    int input = ReadCommandOptions(argc, argv, options, sizeof(options) / sizeof(*options));
    char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
    int serialized = !strcmp(input_kind, "exec");
    LirExecLimits execution_limits = compiler_exec_LirExecDefaultLimits();
    if(input <= 0 || (!run && output == NULL) ||
       (run && (output != NULL || strcmp(emit, "elf"))) ||
       (!run && (fuel != NULL || work != NULL || frames != NULL)) ||
       !assembly_limit(fuel, &execution_limits.fuel) || !assembly_limit(work, &execution_limits.work) ||
       !assembly_limit(frames, &execution_limits.frames) || execution_limits.frames < 1 ||
       (strcmp(input_kind, "source") && !serialized) ||
       (!serialized && (root == NULL || entry == NULL || !split_entry(entry, module, function))) ||
       (serialized && (argc - input != 1 || root != NULL || entry != NULL || module_path_count != 0)) ||
       (strcmp(emit, "lir") && strcmp(emit, "map") && strcmp(emit, "exec") && strcmp(emit, "elf"))) return assembly_usage();
    int status = 1;
    ProgramSet sources = {0};
    ZirProgram merged = {0};
    AssemblyProgram *lowered = NULL;
    LirReadOwner read_owner = {0};
    LirProgram decoded = {0};
    const LirProgram *view = NULL;
    unsigned char *bytes = NULL;
    FILE *stream = NULL;
    char temporary[ZIR_PATH_MAX] = {0};
    if(serialized) {
        size_t input_size = 0;
        unsigned char *input_bytes = assembly_read(argv[input], &input_size);
        if(input_bytes == NULL) goto done;
        LirCodecResult read = compiler_exec_serial_LirRead(&read_owner,
            (String){(char *)input_bytes, input_size}, compiler_lowered_ir_LirDefaultLimits());
        free(input_bytes);
        if(read.problem != LirCodecProblem_None) {
            Diagnostic((ZirSourceSpan){0}, "assembly.codec", "instruction input rejected: problem=%d offset=%lld verifier=%d",
                read.problem, (long long)read.offset, read.diagnostic.problem);
            goto done;
        }
        decoded = compiler_exec_serial_LirReadProgram(&read_owner);
        view = &decoded;
    } else {
        if(!ProgramsLoad(&sources, root, module_paths, module_path_count,
                         (const char *const *)(argv + input), argc - input) ||
           !CheckCanonicalPrograms(sources.programs, sources.count, (const char *const *)sources.paths) ||
           !CommandMergePrograms(&sources, &merged)) goto done;
        ZirProgram *programs[] = {&merged};
        if(!LinkImports(programs, 1)) goto done;
        lowered = AssemblyLower(&merged, module, function);
        if(lowered == NULL) goto done;
        view = AssemblyView(lowered);
    }
    LirDiagnostic diagnostic = compiler_lowered_verify_LirVerify(*view, compiler_lowered_ir_LirDefaultLimits());
    if(diagnostic.problem != LirProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.verify",
            "lowered IR rejected: problem=%d function=%d block=%d instruction=%d value=%d origin=%d",
            diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
            diagnostic.value, diagnostic.origin);
        goto done;
    }
    int listing = !strcmp(emit, "lir");
    if(run) { status = assembly_run(view, execution_limits); goto done; }
    int mapping = !strcmp(emit, "map");
    int executable = !strcmp(emit, "elf");
    int64_t size = 0;
    if(!strcmp(emit, "exec")) {
        LirCodecResult measured = compiler_exec_serial_LirWrite(*view, (Slice){0});
        if(measured.problem != LirCodecProblem_None || measured.size <= 0 || measured.size > 268435456) {
            Diagnostic((ZirSourceSpan){0}, "assembly.codec", "cannot measure instruction output: problem=%d", measured.problem);
            goto done;
        }
        size = measured.size;
        bytes = malloc((size_t)size);
        if(bytes == NULL) { DiagnosticOutOfMemory(); goto done; }
        LirCodecResult encoded = compiler_exec_serial_LirWrite(*view, (Slice){bytes, size});
        if(encoded.problem != LirCodecProblem_None || encoded.size != size) {
            Diagnostic((ZirSourceSpan){0}, "assembly.codec", "cannot encode instruction output: problem=%d", encoded.problem);
            goto done;
        }
    } else if(executable) {
        LirNativeResult measured = compiler_lowered_x64_LirX64Elf(*view, (Slice){0});
        if(measured.problem != LirProblem_None || measured.size <= 0 || measured.size > 268435456) {
            Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native encoder rejected the program: problem=%d", measured.problem);
            goto done;
        }
        size = measured.size;
        bytes = malloc((size_t)size);
        if(bytes == NULL) { DiagnosticOutOfMemory(); goto done; }
        LirNativeResult encoded = compiler_lowered_x64_LirX64Elf(*view, (Slice){bytes, size});
        if(encoded.problem != LirProblem_None || encoded.size != size) {
            Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native encoder failed after measuring: problem=%d", encoded.problem);
            goto done;
        }
    }
    if(!strcmp(output, "-") && !executable) {
        int written = listing ? AssemblyPrintLir(view, stdout) : mapping ? assembly_map(view, stdout) :
            fwrite(bytes, 1, (size_t)size, stdout) == (size_t)size;
        status = written && fflush(stdout) == 0 ? 0 : 1;
        goto done;
    }
    if(!strcmp(output, "-")) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "ELF output requires a file path");
        goto done;
    }
    stream = GeneratedOutputOpen(output, temporary, sizeof(temporary));
    if(stream == NULL) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "cannot create output: %s", output);
        goto done;
    }
    int written = listing ? AssemblyPrintLir(view, stream) : mapping ? assembly_map(view, stream) :
        fwrite(bytes, 1, (size_t)size, stream) == (size_t)size;
#if defined(__unix__) || defined(__APPLE__)
    if(executable && fchmod(fileno(stream), 0755) != 0) written = 0;
#endif
    if(fclose(stream) != 0) written = 0;
    stream = NULL;
    if(!written || GeneratedOutputReplace(temporary, output) != 0) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "cannot finish output: %s", output);
        goto done;
    }
    temporary[0] = 0;
#if defined(__unix__) || defined(__APPLE__)
    if(executable && chmod(output, 0755) != 0) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "cannot make output executable: %s", output);
        goto done;
    }
#endif
    status = 0;
done:
    if(stream) fclose(stream);
    if(temporary[0]) remove(temporary);
    free(bytes);
    AssemblyFree(lowered);
    compiler_exec_serial_LirReadFree(&read_owner);
    free(merged.modules);
    ProgramsFree(&sources);
    return status;
}
