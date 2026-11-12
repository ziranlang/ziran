#include "zir_asm_lower.h"
#include "zir_check.h"
#include "compiler_lowered_verify.h"
#include "compiler_lowered_x64.h"
#include "compiler_exec_serial.h"
#include "compiler_exec.h"
#include "compiler_opt.h"
#include "zir_command.h"
#include "zir_serial.h"
#include "zir_sha256.h"

#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

static int
assembly_usage(void)
{
    fputs("usage: zi2asm --root DIR --entry MODULE:FUNCTION [-I DIR] [--opt] [--emit lir|map|exec|elf|opt-report] -o FILE INPUT...\n"
          "       zi2asm --input exec [--opt] [--emit lir|map|exec|elf|opt-report] -o FILE INPUT\n"
          "       zi2asm --run [--opt] [--fuel N] [--work N] [--frames N] INPUT...\n"
          "map selection: [--function-id N [--block-id N]] [--address-begin N --address-end N]\n"
          "experimental direct backend; opt-report requires --opt; native target: Linux x86-64 ELF\n", stderr);
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
    int selected;
} AssemblyFunctionExtent;

typedef struct AssemblyMapSelection {
    int64_t function;
    int64_t block;
    int64_t begin;
    int64_t end;
} AssemblyMapSelection;

static int
assembly_map_selected(LirNativeRange range, const AssemblyMapSelection *selection)
{
    if(selection->function >= 0 && range.function != selection->function) return 0;
    if(selection->block >= 0 && range.block != selection->block) return 0;
    if(selection->begin < 0) return 1;
    if(range.begin == range.end) return range.begin >= selection->begin && range.begin < selection->end;
    return range.begin < selection->end && range.end > selection->begin;
}

static void
assembly_select_origin(const LirProgram *program, int origin, unsigned char *selected)
{
    LirOrigin *origins = program->origins.data;
    while(origin >= 0 && !selected[origin]) {
        selected[origin] = 1;
        origin = origins[origin].parent;
    }
}

static void
assembly_opt_pass(FILE *stream, const char *name, const LirOptPassCounters *counters)
{
    fprintf(stream, "{\"name\":\"%s\",\"runs\":%lld,\"instructions\":%lld,\"values\":%lld,\"blocks\":%lld,\"constants\":%lld}",
        name, (long long)counters->runs, (long long)counters->instructions, (long long)counters->values,
        (long long)counters->blocks, (long long)counters->constants);
}

static int
assembly_opt_report(FILE *stream, const LirOptResult *result)
{
    const LirOptCounters *counters = &result->counters;
    fprintf(stream, "{\"version\":1,\"work\":%lld,\"scratch_bytes\":%lld,\"input\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},",
        (long long)result->work, (long long)result->scratch_bytes, (long long)counters->input_functions,
        (long long)counters->input_blocks, (long long)counters->input_values, (long long)counters->input_instructions);
    fprintf(stream, "\"output\":{\"functions\":%lld,\"blocks\":%lld,\"values\":%lld,\"instructions\":%lld},\"passes\":[",
        (long long)counters->output_functions, (long long)counters->output_blocks,
        (long long)counters->output_values, (long long)counters->output_instructions);
    assembly_opt_pass(stream, "fold", &counters->fold); fputc(',', stream);
    assembly_opt_pass(stream, "copy", &counters->copy); fputc(',', stream);
    assembly_opt_pass(stream, "cfg", &counters->cfg); fputc(',', stream);
    assembly_opt_pass(stream, "dead", &counters->dead);
    fputs("]}\n", stream);
    return !ferror(stream);
}

static int
assembly_map(const LirProgram *program, FILE *stream, const AssemblyMapSelection *selection,
             const unsigned char *bytes, int64_t size, int optimize)
{
    if(selection->function >= program->functions.length ||
       (selection->block >= 0 && selection->block >= ((LirFunction *)program->functions.data)[selection->function].blocks.length)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.inspect", "native map function or block selector is outside the program");
        return 0;
    }
    int filtered = selection->function >= 0 || selection->begin >= 0;
    LirX64ImageLayout image = compiler_lowered_x64_LirX64ElfLayout(*program);
    if(image.problem != LirProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "cannot measure native image layout: problem=%d", image.problem);
        return 0;
    }
    if(bytes == NULL || size != image.file_size) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map image does not match the encoded file size");
        return 0;
    }
    Sha256 hash;
    unsigned char digest[32];
    char identity[65];
    static const char hex[] = "0123456789abcdef";
    Sha256Begin(&hash);
    Sha256Add(&hash, bytes, (size_t)size);
    Sha256End(&hash, digest);
    for(size_t index = 0; index < sizeof(digest); index++) {
        identity[index * 2] = hex[digest[index] >> 4];
        identity[index * 2 + 1] = hex[digest[index] & 15];
    }
    identity[64] = 0;
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
    if(ranges[mapped.size - 1].end != image.text_size) {
        free(ranges);
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map does not match the encoded text extent");
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
    unsigned char *selected_origins = NULL;
    if(filtered && program->origins.length > 0) {
        selected_origins = calloc((size_t)program->origins.length, 1);
        if(selected_origins == NULL) { free(extents); free(ranges); DiagnosticOutOfMemory(); return 0; }
    }
    int64_t selected_ranges = 0;
    for(int64_t index = 0; index < mapped.size; index++) {
        LirNativeRange range = ranges[index];
        if(!assembly_map_selected(range, selection)) continue;
        selected_ranges++;
        if(range.function >= 0) extents[range.function].selected = 1;
        if(filtered) assembly_select_origin(program, range.origin, selected_origins);
    }
    LirFunction *functions = program->functions.data;
    for(int64_t index = 0; filtered && index < program->functions.length; index++) {
        if(index == selection->function) extents[index].selected = 1;
        if(extents[index].selected) assembly_select_origin(program, functions[index].origin, selected_origins);
    }
    fputs("{\"format\":\"ziran-native-map\",\"version\":1,\"target\":\"x86_64-linux\","
          "\"coordinate\":\"text-relative\",\"variant\":\"x86-64-baseline\",\"abi\":\"ziran-internal\",", stream);
    fprintf(stream, "\"optimization_request\":\"%s\",\"identity\":{\"algorithm\":\"sha256\",\"scope\":\"complete-elf\",\"digest\":\"%s\"},",
        optimize ? "verified-basic" : "none", identity);
    fputs("\"image\":{\"kind\":\"aot-elf\",\"address_space\":\"elf-virtual\",", stream);
    fprintf(stream, "\"file_size\":%lld,\"text_file_offset\":%lld,\"text_virtual_address\":%llu,\"text_size\":%lld,"
        "\"data_file_offset\":%lld,\"data_virtual_address\":%llu,\"data_size\":%lld},\"selection\":",
        (long long)image.file_size, (long long)image.text_offset, (unsigned long long)image.text_address,
        (long long)image.text_size, (long long)image.data_offset, (unsigned long long)image.data_address,
        (long long)image.data_size);
    if(filtered) {
        fprintf(stream, "{\"function_id\":%lld,\"block_id\":%lld,\"begin\":%lld,\"end\":%lld}",
            (long long)selection->function, (long long)selection->block, (long long)selection->begin, (long long)selection->end);
    } else fputs("null", stream);
    fprintf(stream, ",\"total_ranges\":%lld,\"selected_ranges\":%lld,\"ranges\":[",
        (long long)mapped.size, (long long)selected_ranges);
    int separator = 0;
    for(int64_t index = 0; index < mapped.size; index++) {
        LirNativeRange range = ranges[index];
        if(!assembly_map_selected(range, selection)) continue;
        if(separator++) fputc(',', stream);
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
            fprintf(stream, ",\"line\":%d,\"column\":%d,\"end_line\":%d,\"end_column\":%d",
                origin.line, origin.column, origin.end_line, origin.end_column);
        }
        fputc('}', stream);
    }
    fprintf(stream, "],\"text_size\":%lld,\"functions\":[", (long long)ranges[mapped.size - 1].end);
    separator = 0;
    for(int64_t index = 0; index < program->functions.length; index++) {
        LirFunction function = functions[index];
        if(filtered && !extents[index].selected) continue;
        if(separator++) fputc(',', stream);
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
    separator = 0;
    for(int64_t index = 0; index < program->origins.length; index++) {
        LirOrigin origin = origins[index];
        if(filtered && !selected_origins[index]) continue;
        if(separator++) fputc(',', stream);
        fprintf(stream, "{\"id\":%lld,\"file_id\":%d,\"file\":", (long long)index, origin.file);
        assembly_json(stream, ((String *)program->sources.data)[origin.file]);
        fprintf(stream, ",\"line\":%d,\"column\":%d,\"end_line\":%d,\"end_column\":%d,\"begin\":%lld,\"end\":%lld,\"parent\":%d}",
            origin.line, origin.column, origin.end_line, origin.end_column,
            (long long)origin.begin, (long long)origin.end, origin.parent);
    }
    fputs("]}\n", stream);
    free(selected_origins);
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
    const char *function_id = NULL;
    const char *block_id = NULL;
    const char *address_begin = NULL;
    const char *address_end = NULL;
    AssemblyMapSelection selection = {.function = -1, .block = -1, .begin = -1, .end = -1};
    int run = 0;
    int optimize = 0;
    const char **module_paths = CommandModulePaths(argc);
    int module_path_count = 0;
    if(module_paths == NULL) return 1;
    SetDiagnosticFormatFromArguments(argc, argv);
    CommandLowerPriority();
    CommandOption options[] = {
        COMMAND_TEXT("--root", root), COMMAND_TEXT("--entry", entry),
        COMMAND_TEXT("-o", output), COMMAND_TEXT("--emit", emit),
        COMMAND_TEXT("--input", input_kind),
        COMMAND_FLAG("--run", run), COMMAND_FLAG("--opt", optimize), COMMAND_TEXT("--fuel", fuel),
        COMMAND_TEXT("--work", work), COMMAND_TEXT("--frames", frames),
        COMMAND_TEXT("--function-id", function_id), COMMAND_TEXT("--block-id", block_id),
        COMMAND_TEXT("--address-begin", address_begin), COMMAND_TEXT("--address-end", address_end),
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
       !assembly_limit(function_id, &selection.function) || !assembly_limit(block_id, &selection.block) ||
       !assembly_limit(address_begin, &selection.begin) || !assembly_limit(address_end, &selection.end) ||
       selection.function > INT32_MAX || selection.block > INT32_MAX ||
       (selection.block >= 0 && selection.function < 0) ||
       ((selection.begin < 0) != (selection.end < 0)) ||
       (selection.begin >= 0 && selection.end <= selection.begin) ||
       ((function_id != NULL || block_id != NULL || address_begin != NULL || address_end != NULL) &&
        (run || strcmp(emit, "map"))) ||
       (strcmp(input_kind, "source") && !serialized) ||
       (!serialized && (root == NULL || entry == NULL || !split_entry(entry, module, function))) ||
       (serialized && (argc - input != 1 || root != NULL || entry != NULL || module_path_count != 0)) ||
       (!strcmp(emit, "opt-report") && !optimize) ||
       (strcmp(emit, "lir") && strcmp(emit, "map") && strcmp(emit, "exec") && strcmp(emit, "elf") && strcmp(emit, "opt-report"))) return assembly_usage();
    int status = 1;
    ProgramSet sources = {0};
    ZirProgram merged = {0};
    AssemblyProgram *lowered = NULL;
    LirReadOwner read_owner = {0};
    LirProgram decoded = {0};
    LirOptOwner optimized_owner = {0};
    LirOptResult optimization = {0};
    LirProgram optimized_program = {0};
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
    if(optimize) {
        optimization = compiler_opt_LirOptBuild(&optimized_owner, *view,
            compiler_lowered_ir_LirDefaultLimits(), compiler_opt_LirOptDefaultOptions());
        if(optimization.problem != LirOptProblem_None) {
            LirDiagnostic rejected = optimization.diagnostic;
            Diagnostic((ZirSourceSpan){0}, "assembly.optimize",
                "optimization rejected: problem=%d verifier=%d function=%d block=%d instruction=%d value=%d origin=%d",
                optimization.problem, rejected.problem, rejected.function, rejected.block,
                rejected.instruction, rejected.value, rejected.origin);
            goto done;
        }
        optimized_program = compiler_opt_LirOptProgram(&optimized_owner);
        view = &optimized_program;
    }
    int listing = !strcmp(emit, "lir");
    int reporting = !strcmp(emit, "opt-report");
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
    } else if(executable || mapping) {
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
        int written = reporting ? assembly_opt_report(stdout, &optimization) :
            listing ? AssemblyPrintLir(view, stdout) : mapping ? assembly_map(view, stdout, &selection, bytes, size, optimize) :
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
    int written = reporting ? assembly_opt_report(stream, &optimization) :
        listing ? AssemblyPrintLir(view, stream) : mapping ? assembly_map(view, stream, &selection, bytes, size, optimize) :
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
    compiler_opt_LirOptFree(&optimized_owner);
    AssemblyFree(lowered);
    compiler_exec_serial_LirReadFree(&read_owner);
    free(merged.modules);
    ProgramsFree(&sources);
    return status;
}
