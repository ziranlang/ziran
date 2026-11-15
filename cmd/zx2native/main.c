#include "zir_asm_lower.h"
#include "zx_exec_run.h"
#include "zir_check.h"
#include "compiler_lowered_verify.h"
#include "compiler_exec_debug.h"
#include "compiler_exec_link.h"
#include "compiler_exec.h"
#include "compiler_machine_ir.h"
#include "compiler_machine_pipeline.h"
#include "compiler_opt.h"
#include "compiler_target.h"
#include "zir_command.h"
#include "zir_link.h"
#include "zir_serial.h"
#include "zir_sha256.h"
#include "zir_text.h"

#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

static int
assembly_usage(void)
{
    fputs("usage: zx2native [--target x86_64|aarch64|armv7] [--platform linux|android] [--emit elf|map|image] [--fuel N] -o FILE INPUT.zx\n"
          "map selection: [--function-id N [--block-id N]] [--address-begin N --address-end N]\n"
          "--fuel sets an ELF's starting fuel; it is unbounded by default, as for a bundle run\n"
          "zx2native reads only decoded and verified ZX and uses only the no-fallback machine-IR path\n"
          "image is reserved for the relocatable in-memory image and is a named gap until that loader lands\n", stderr);
    return 2;
}

/* Validate the stable ARCH-OS row before reading input or opening output. The
 * current experimental writer owns only the Linux x86-64 entry, write_all and
 * exit_group veneer; accepting a described row must never imply a fallback. */
static int
assembly_target_supported(const char *name, TargetDescription *native_target)
{
    const char *dash = strchr(name, '-');
    if(dash == NULL || dash == name || dash[1] == 0 || strchr(dash + 1, '-') != NULL) {
        Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "unknown zx2native target row %s; use ARCH and OS IDs from ziran capabilities --json", name);
        return 0;
    }
    TargetArch arch = compiler_target_TargetArchFromName(
        (String){(char *)name, (size_t)(dash - name)});
    TargetOS os = compiler_target_TargetOSFromName(
        (String){(char *)(dash + 1), strlen(dash + 1)});
    TargetResult described = compiler_target_TargetDescribe(arch, os);
    if(arch == TargetArch_Invalid || os == TargetOS_Invalid ||
       described.problem != TargetProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "unknown zx2native target row %s; use ARCH and OS IDs from ziran capabilities --json", name);
        return 0;
    }
    if(!compiler_target_TargetExecutableNativeSupported(described.target)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-platform-service-unavailable",
            "assembly.gap.machine-platform-service-unavailable: selected target %s has no process-entry/write_all/exit_group veneer; no libc, host binding or alternate engine was selected",
            name);
        return 0;
    }
    *native_target = described.target;
    return 1;
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
assembly_machine_source_gap(const ZirProgram *program)
{
    for(int module = 0; module < program->module_count; module++) {
        const ZirModule *current = &program->modules[module];
        for(int index = 0; index < current->import_count; index++) {
            const ZirImport *import = &current->imports[index];
            if(import->kind != ZIR_IMPORT_EXTERN || import->extern_kind == ZIR_EXTERN_HOST) continue;
            Diagnostic(import->span, "assembly.gap.machine-host",
                "assembly.gap.machine-host: machine ELF does not implement foreign service %s; no alternate engine was selected",
                import->extern_symbol[0] ? import->extern_symbol : import->name);
            return 1;
        }
        for(int function = 0; function < current->function_count; function++) {
            const ZirFunction *body = &current->functions[function];
            if(body->is_extern && body->extern_kind != ZIR_EXTERN_HOST) {
                Diagnostic(body->span, "assembly.gap.machine-host",
                    "assembly.gap.machine-host: machine ELF does not implement foreign service %s; no alternate engine was selected",
                    body->extern_symbol[0] ? body->extern_symbol : body->name);
                return 1;
            }
            for(int statement = 0; statement < body->stmt_count; statement++) {
                if(!body->stmts[statement].is_parallel && !body->stmts[statement].is_gpu) continue;
                Diagnostic(body->stmts[statement].span, "assembly.gap.machine-parallel",
                    "machine ELF does not implement parallel or GPU statements; no alternate engine was selected");
                return 1;
            }
        }
    }
    return 0;
}

/* Foreign host calls are the only decoded-program preflight gap. All portable
 * platform services are lowered by the machine pipeline itself. */
static int
assembly_machine_gap(const LirProgram *program)
{
    const LirFunction *functions = program->functions.data;
    for(int64_t index = 0; index < program->functions.length; index++) {
        if(!functions[index].external) continue;
        int service = ZxPlatformService(program, index);
        if(service != ZX_SERVICE_NONE) continue;
        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-host",
            "assembly.gap.machine-host: machine ELF does not implement host capability %.*s; no alternate engine was selected",
            (int)functions[index].name.length, functions[index].name.data);
        return 1;
    }
    return 0;
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

typedef struct AssemblyNativeRange {
    int32_t function;
    int32_t block;
    int32_t instruction;
    int32_t origin;
    int64_t begin;
    int64_t end;
} AssemblyNativeRange;

static int
assembly_map_selected(AssemblyNativeRange range, const AssemblyMapSelection *selection)
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
assembly_map_range(AssemblyNativeRange *ranges, int64_t capacity, int64_t *count,
                   int function, int block, int instruction, int origin,
                   int64_t begin, int64_t end)
{
    if(*count >= capacity || begin < 0 || end < begin) return 0;
    ranges[(*count)++] = (AssemblyNativeRange){.function = function, .block = block,
        .instruction = instruction, .origin = origin, .begin = begin, .end = end};
    return 1;
}

static int
assembly_machine_map(const LirProgram *program, FILE *stream, const AssemblyMapSelection *selection,
             const MachineNativeOwner *owner, const unsigned char *bytes, int64_t size, int optimize)
{
    if(selection->function >= program->functions.length ||
       (selection->block >= 0 && selection->block >= ((LirFunction *)program->functions.data)[selection->function].blocks.length)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.inspect", "native map function or block selector is outside the program");
        return 0;
    }
    int filtered = selection->function >= 0 || selection->begin >= 0;
    if(owner == NULL || !owner->valid || !owner->machine.valid || !owner->encoded.valid) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map has no published machine image");
        return 0;
    }
    MachineProgram machine = owner->machine.program;
    MachineX64Image encoded = owner->encoded.image;
    /* Text-relative coordinates start after the process entry stub. */
    int64_t startup = compiler_machine_elf_MachineElfStartupBytes(machine);
    MachineElfResult image = compiler_machine_elf_MachineElfEncode(machine, encoded, (Slice){0},
        INT64_MAX);
    if(image.diagnostic.problem != MachineProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "cannot measure native machine image layout: problem=%d",
            image.diagnostic.problem);
        return 0;
    }
    if(bytes == NULL || size != image.size || machine.functions.length != encoded.functions.length ||
       machine.blocks.length != encoded.blocks.length || machine.instructions.length != encoded.instructions.length) {
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
    if(machine.instructions.length < 0 || machine.blocks.length < 0 || machine.functions.length < 0 ||
       machine.instructions.length > (INT64_MAX - 4) / 2 - machine.blocks.length - machine.functions.length) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map range count is outside supported bounds");
        return 0;
    }
    int64_t range_capacity = (machine.instructions.length + machine.blocks.length +
        machine.functions.length) * 2 + 4;
    if(range_capacity <= 0 || (uint64_t)range_capacity > SIZE_MAX / sizeof(AssemblyNativeRange)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "native map range allocation is outside supported bounds");
        return 0;
    }
    AssemblyNativeRange *ranges = malloc((size_t)range_capacity * sizeof(*ranges));
    if(ranges == NULL) { DiagnosticOutOfMemory(); return 0; }
    int64_t mapped = 0;
    AssemblyFunctionExtent *extents = calloc((size_t)program->functions.length, sizeof(*extents));
    if(extents == NULL) { free(ranges); DiagnosticOutOfMemory(); return 0; }
    MachineFunction *machine_functions = machine.functions.data;
    MachineBlock *machine_blocks = machine.blocks.data;
    MachineInstruction *machine_instructions = machine.instructions.data;
    MachineX64Function *encoded_functions = encoded.functions.data;
    int64_t *encoded_blocks = encoded.blocks.data;
    MachineX64Range *encoded_instructions = encoded.instructions.data;
    int64_t cursor = 0;
    if(!assembly_map_range(ranges, range_capacity, &mapped, -1, -1, -1, -1,
                           cursor, startup)) goto invalid_map;
    cursor = startup;
    for(int64_t function_id = 0; function_id < machine.functions.length; function_id++) {
        MachineFunction function = machine_functions[function_id];
        MachineX64Function encoded_function = encoded_functions[function_id];
        int lir_function = function.lir_function;
        int64_t function_begin = startup + encoded_function.begin;
        int64_t function_body = startup + encoded_function.body;
        int64_t function_end = startup + encoded_function.end;
        if(lir_function < -1 || lir_function >= program->functions.length ||
           function.origin < -1 || function.origin >= program->origins.length ||
           function_begin < cursor || function_body < function_begin || function_end < function_body ||
           function.block_begin < 0 || function.block_count < 0 ||
           (int64_t)function.block_begin + function.block_count > machine.blocks.length) goto invalid_map;
        if(cursor < function_begin &&
           !assembly_map_range(ranges, range_capacity, &mapped, -1, -1, -1, -1,
                               cursor, function_begin)) goto invalid_map;
        if(function_begin < function_body &&
           !assembly_map_range(ranges, range_capacity, &mapped, lir_function, -1, -1,
                               function.origin, function_begin, function_body)) goto invalid_map;
        cursor = function_body;
        for(int64_t block_id = function.block_begin;
            block_id < (int64_t)function.block_begin + function.block_count; block_id++) {
            MachineBlock block = machine_blocks[block_id];
            int64_t block_begin = startup + encoded_blocks[block_id];
            if(block.function != function_id || block.lir_block < -1 ||
               (block.lir_block >= 0 &&
                block.lir_block >= ((LirFunction *)program->functions.data)[lir_function].blocks.length) ||
               block.instruction_begin < 0 || block.instruction_count < 0 ||
               (int64_t)block.instruction_begin + block.instruction_count > machine.instructions.length ||
               block_begin < cursor || block_begin > function_end) goto invalid_map;
            if(cursor < block_begin &&
               !assembly_map_range(ranges, range_capacity, &mapped, lir_function, -1, -1,
                                   function.origin, cursor, block_begin)) goto invalid_map;
            cursor = block_begin;
            for(int64_t instruction_id = block.instruction_begin;
                instruction_id < (int64_t)block.instruction_begin + block.instruction_count; instruction_id++) {
                MachineInstruction instruction = machine_instructions[instruction_id];
                MachineX64Range encoded_instruction = encoded_instructions[instruction_id];
                int64_t begin = startup + encoded_instruction.begin;
                int64_t end = startup + encoded_instruction.end;
                if(instruction.origin < -1 || instruction.origin >= program->origins.length ||
                   begin < cursor || end < begin || end > function_end) goto invalid_map;
                if(cursor < begin &&
                   !assembly_map_range(ranges, range_capacity, &mapped, lir_function, block.lir_block,
                                       -1, instruction.origin, cursor, begin)) goto invalid_map;
                if(!assembly_map_range(ranges, range_capacity, &mapped, lir_function, block.lir_block,
                                       (int)instruction_id, instruction.origin, begin, end)) goto invalid_map;
                cursor = end;
            }
        }
        if(cursor < function_end &&
           !assembly_map_range(ranges, range_capacity, &mapped, lir_function, -1, -1,
                               function.origin, cursor, function_end)) goto invalid_map;
        cursor = function_end;
        if(lir_function >= 0) {
            extents[lir_function].begin = function_begin;
            extents[lir_function].end = function_end;
            extents[lir_function].seen = 1;
        }
    }
    if(cursor < startup + encoded.code.length &&
       !assembly_map_range(ranges, range_capacity, &mapped, -1, -1, -1, -1,
                           cursor, startup + encoded.code.length)) goto invalid_map;
    cursor = startup + encoded.code.length;
    if(cursor > image.text_size ||
       !assembly_map_range(ranges, range_capacity, &mapped, -1, -1, -1, -1,
                           cursor, image.text_size)) goto invalid_map;
    cursor = image.text_size;
    if(mapped <= 0 || cursor != image.text_size) goto invalid_map;
    unsigned char *selected_origins = NULL;
    if(filtered && program->origins.length > 0) {
        selected_origins = calloc((size_t)program->origins.length, 1);
        if(selected_origins == NULL) { free(extents); free(ranges); DiagnosticOutOfMemory(); return 0; }
    }
    int64_t selected_ranges = 0;
    for(int64_t index = 0; index < mapped; index++) {
        AssemblyNativeRange range = ranges[index];
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
        (long long)image.size, (long long)image.text_offset,
        (unsigned long long)(MachineElfBase + image.text_offset),
        (long long)image.text_size, (long long)image.data_offset,
        (unsigned long long)(MachineElfBase + image.data_offset),
        (long long)image.data_size);
    if(filtered) {
        fprintf(stream, "{\"function_id\":%lld,\"block_id\":%lld,\"begin\":%lld,\"end\":%lld}",
            (long long)selection->function, (long long)selection->block, (long long)selection->begin, (long long)selection->end);
    } else fputs("null", stream);
    fprintf(stream, ",\"total_ranges\":%lld,\"selected_ranges\":%lld,\"ranges\":[",
        (long long)mapped, (long long)selected_ranges);
    int separator = 0;
    for(int64_t index = 0; index < mapped; index++) {
        AssemblyNativeRange range = ranges[index];
        if(!assembly_map_selected(range, selection)) continue;
        if(separator++) fputc(',', stream);
        fprintf(stream, "{\"begin\":%lld,\"end\":%lld,\"function_id\":%d,\"block\":%d,\"instruction\":%d,\"origin\":%d",
            (long long)range.begin, (long long)range.end, range.function, range.block, range.instruction, range.origin);
        if(range.function >= 0) {
            LirFunction function = ((LirFunction *)program->functions.data)[range.function];
            fputs(",\"module\":", stream); assembly_json(stream, ((String *)program->modules.data)[function.module]);
            fputs(",\"function\":", stream); assembly_json(stream, function.name);
        }
        if(range.origin >= 0) {
            LirOrigin origin = ((LirOrigin *)program->origins.data)[range.origin];
            fputs(",\"file\":", stream); assembly_json(stream, ((String *)program->sources.data)[origin.file]);
            fprintf(stream, ",\"line\":%d,\"column\":%d,\"end_line\":%d,\"end_column\":%d",
                origin.line, origin.column, origin.end_line, origin.end_column);
        }
        fputc('}', stream);
    }
    fprintf(stream, "],\"text_size\":%lld,\"functions\":[", (long long)image.text_size);
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

invalid_map:
    free(extents);
    free(ranges);
    Diagnostic((ZirSourceSpan){0}, "assembly.encode", "machine metadata does not describe a complete native map");
    return 0;
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *entry = NULL;
    const char *output = NULL;
    const char *emit = "elf";
    const char *target = "x86_64";
    const char *platform = "linux";
    const char *fuel = NULL;
    const char *work = NULL;
    const char *frames = NULL;
    const char *function_id = NULL;
    const char *block_id = NULL;
    const char *address_begin = NULL;
    const char *address_end = NULL;
    const char *elf_fuel_text = NULL;
    int64_t elf_fuel = INT64_MAX;
    AssemblyMapSelection selection = {.function = -1, .block = -1, .begin = -1, .end = -1};
    int run = 0;
    int heap_report = 0;
    int optimize = 0;
    const char **module_paths = CommandModulePaths(argc);
    int module_path_count = 0;
    const char *bindings[64];
    int binding_count = 0;
    const char *defines[65];
    int define_count = 0;
    if(module_paths == NULL) return 1;
    SetDiagnosticFormatFromArguments(argc, argv);
    CommandLowerPriority();
    CommandOption options[] = {
        COMMAND_TEXT("-o", output), COMMAND_TEXT("--emit", emit),
        COMMAND_TEXT("--target", target), COMMAND_TEXT("--platform", platform),
        COMMAND_TEXT("--function-id", function_id), COMMAND_TEXT("--block-id", block_id),
        COMMAND_TEXT("--address-begin", address_begin), COMMAND_TEXT("--address-end", address_end),
        COMMAND_TEXT("--fuel", elf_fuel_text)
    };
    int input = ReadCommandOptions(argc, argv, options, sizeof(options) / sizeof(*options));
    char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
    int serialized = 1;
    LirExecLimits execution_limits = compiler_exec_LirExecDefaultLimits();
    size_t input_length = input > 0 ? strlen(argv[input]) : 0;
    if(input <= 0 || output == NULL || argc - input != 1 ||
       input_length <= 3 || strcmp(argv[input] + input_length - 3, ".zx") ||
       !assembly_limit(function_id, &selection.function) || !assembly_limit(block_id, &selection.block) ||
       !assembly_limit(address_begin, &selection.begin) || !assembly_limit(address_end, &selection.end) ||
       !assembly_limit(elf_fuel_text, &elf_fuel) || (elf_fuel_text != NULL && strcmp(emit, "elf")) ||
       selection.function > INT32_MAX || selection.block > INT32_MAX ||
       (selection.block >= 0 && selection.function < 0) ||
       ((selection.begin < 0) != (selection.end < 0)) ||
       (selection.begin >= 0 && selection.end <= selection.begin) ||
       ((function_id != NULL || block_id != NULL || address_begin != NULL || address_end != NULL) &&
        strcmp(emit, "map")) ||
       (strcmp(target, "x86_64") && strcmp(target, "aarch64") && strcmp(target, "armv7")) ||
       (strcmp(platform, "linux") && strcmp(platform, "android")) ||
       (strcmp(emit, "elf") && strcmp(emit, "map") && strcmp(emit, "image")))
        return assembly_usage();
    char target_row[64];
    TargetDescription native_target = {0};
    if(snprintf(target_row, sizeof(target_row), "%s-%s", target, platform) < 0 ||
       !assembly_target_supported(target_row, &native_target)) return 1;
    if(!strcmp(emit, "image")) {
        Diagnostic((ZirSourceSpan){0}, "assembly.gap.target-unimplemented",
            "relocatable in-memory machine images are not implemented for %s; no alternate output was selected",
            target_row);
        return 1;
    }
    int status = 1;
    ProgramSet sources = {0};
    ZirProgram merged = {0};
    AssemblyProgram *lowered = NULL;
    ZxExecOwner zx_owner = {0};
    ZxDebugOwner debug_owner = {0};
    LirProgram decoded = {0};
    LirProgram origins = {0};
    LirOptOwner optimized_owner = {0};
    LirOptResult optimization = {0};
    LirProgram optimized_program = {0};
    MachineNativeOwner machine_owner = {0};
    const LirProgram *view = NULL;
    unsigned char *bytes = NULL;
    unsigned char *zx_bytes = NULL;
    int64_t zx_size = 0;
    FILE *stream = NULL;
    char temporary[ZIR_PATH_MAX] = {0};
    if(serialized) {
        size_t input_size = 0;
        zx_bytes = assembly_read(argv[input], &input_size);
        if(zx_bytes == NULL) goto done;
        zx_size = (int64_t)input_size;
        {
            ZxExecResult read = compiler_exec_link_ZxExecRead(&zx_owner,
                (String){(char *)zx_bytes, input_size}, compiler_lowered_ir_LirDefaultLimits());
            if(read.problem != ZxExecProblem_None) {
                String problem = compiler_exec_link_ZxExecProblemName(read.problem);
                String container = compiler_bundle_ExecutableMessage(read.container);
                Diagnostic((ZirSourceSpan){0}, "assembly.container",
                    "executable ZX rejected: problem=%.*s container=%.*s codec=%d offset=%lld verifier=%d",
                    (int)problem.length, problem.data, (int)container.length, container.data,
                    read.codec, (long long)read.offset, read.diagnostic.problem);
                goto done;
            }
            decoded = compiler_exec_link_ZxExecReadProgram(&zx_owner);
        }
        view = &decoded;
    } else {
        /* A wasm32 module targets the web, so compile-time conditions on the
         * target, OS and CPU select its branches. */
        if(!ProgramsLoadWithDefines(&sources, root, module_paths, module_path_count, defines, define_count,
                                    (const char *const *)(argv + input), argc - input) ||
           !CheckCanonicalPrograms(sources.programs, sources.count, (const char *const *)sources.paths) ||
           !CommandMergePrograms(&sources, &merged)) goto done;
        ZirProgram *programs[] = {&merged};
        if(!LinkImports(programs, 1)) goto done;
        for(int binding = 0; binding < binding_count; binding++)
            if(!BindHostProvider(&merged, bindings[binding])) goto done;
        if(assembly_machine_source_gap(&merged)) goto done;
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
    int reporting = !strcmp(emit, "opt-report");
    int canonical = run || (!listing && !reporting);
    if(!serialized && (optimize || canonical)) {
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
    int mapping = !strcmp(emit, "map");
    int executable = !strcmp(emit, "elf");
    int64_t size = 0;
    if(!serialized && canonical) {
        size_t interface_length = 0;
        const char *interface = AssemblyHostInterface(lowered, &interface_length);
        if(interface == NULL) goto done;
        String interface_text = {interface, interface_length};
        ZxExecResult measured = compiler_exec_link_ZxExecWrite(*view, (Slice){0}, (Slice){0}, interface_text,
            false);
        if(measured.problem != ZxExecProblem_None || measured.size <= 0 || measured.size > 268435456) {
            String problem = compiler_exec_link_ZxExecProblemName(measured.problem);
            Diagnostic((ZirSourceSpan){0}, "assembly.container",
                "cannot measure executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
                (int)problem.length, problem.data, measured.container, measured.codec, measured.diagnostic.problem);
            goto done;
        }
        zx_size = measured.size;
        zx_bytes = malloc((size_t)zx_size);
        if(zx_bytes == NULL) { DiagnosticOutOfMemory(); goto done; }
        ZxExecResult encoded = compiler_exec_link_ZxExecWrite(*view, (Slice){0}, (Slice){zx_bytes, zx_size},
            interface_text, false);
        if(encoded.problem != ZxExecProblem_None || encoded.size != zx_size) {
            String problem = compiler_exec_link_ZxExecProblemName(encoded.problem);
            Diagnostic((ZirSourceSpan){0}, "assembly.container",
                "cannot encode executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
                (int)problem.length, problem.data, encoded.container, encoded.codec, encoded.diagnostic.problem);
            goto done;
        }
        /* Both entry routes lower only the decoded, verified portable image.
         * Never bypass this boundary with the source-side owner or debug data. */
        ZxExecResult read = compiler_exec_link_ZxExecRead(&zx_owner,
            (String){(char *)zx_bytes, (size_t)zx_size}, compiler_lowered_ir_LirDefaultLimits());
        if(read.problem != ZxExecProblem_None) {
            Diagnostic((ZirSourceSpan){0}, "assembly.container",
                "generated executable ZX rejected: problem=%d codec=%d verifier=%d",
                read.problem, read.codec, read.diagnostic.problem);
            goto done;
        }
        decoded = compiler_exec_link_ZxExecReadProgram(&zx_owner);
        view = &decoded;
    }
    if(run) { status = ZxExecRunProgram(view, execution_limits, heap_report); goto done; }
    /* A map lowers the same verified program with the attachment's checked
     * origins restored, so machine instructions carry them; the encoded
     * bytes are the same either way. */
    if(mapping) {
        ZxDebugProblem restored = compiler_exec_link_ZxExecReadDebug(&zx_owner, &debug_owner,
            compiler_lowered_ir_LirDefaultLimits());
        if(restored == ZxDebugProblem_None) {
            origins = compiler_exec_debug_ZxDebugProgram(&debug_owner);
            view = &origins;
        } else if(restored != ZxDebugProblem_Absent) {
            String problem = compiler_exec_debug_ZxDebugProblemName(restored);
            Diagnostic((ZirSourceSpan){0}, "zx.debug",
                "the ZX debug attachment does not belong to this program: %.*s",
                (int)problem.length, problem.data);
            goto done;
        }
    }
    if(!strcmp(emit, "zx")) {
        /* Preserve all declared assets and optional sections when rewriting. */
        bytes = zx_bytes;
        zx_bytes = NULL;
        size = zx_size;
    } else if(executable || mapping) {
        if(assembly_machine_gap(view)) goto done;
        MachineDiagnostic diagnostic = compiler_machine_pipeline_MachineNativeBuild(
            *view, native_target, compiler_machine_ir_MachineDefaultLimits(),
            &machine_owner, elf_fuel);
        if(diagnostic.problem != MachineProblem_None) {
            Diagnostic((ZirSourceSpan){0}, "assembly.machine",
                "machine pipeline rejected the program: problem=%d function=%d block=%d instruction=%d operand=%d virtual=%d; no alternate engine was selected",
                diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
                diagnostic.operand, diagnostic.virtual);
            goto done;
        }
        Slice native = compiler_machine_pipeline_MachineNativeView(&machine_owner);
        if(native.length <= 0 || native.length > 268435456) {
            Diagnostic((ZirSourceSpan){0}, "assembly.machine",
                "machine pipeline produced an invalid ELF extent; no alternate engine was selected");
            goto done;
        }
        size = native.length;
        bytes = malloc((size_t)size);
        if(bytes == NULL) { DiagnosticOutOfMemory(); goto done; }
        memcpy(bytes, native.data, (size_t)size);
    }
    if(!strcmp(output, "-") && !executable) {
        int written = reporting ? assembly_opt_report(stdout, &optimization) :
            listing ? AssemblyPrintLir(view, stdout) : mapping ?
                assembly_machine_map(view, stdout, &selection, &machine_owner, bytes, size, 1) :
            fwrite(bytes, 1, (size_t)size, stdout) == (size_t)size;
        status = written && fflush(stdout) == 0 ? 0 : 1;
        goto done;
    }
    if(!strcmp(output, "-")) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "ELF output requires a file path");
        goto done;
    }
    /* The output's directory is made as needed. */
    {
        char parent[ZIR_PATH_MAX];
        size_t length = strlen(output);
        if(length < sizeof(parent)) {
            memcpy(parent, output, length + 1);
            for(char *at = parent + 1; *at; at++) {
                if(*at != '/') continue;
                *at = 0;
                mkdir(parent, 0755);
                *at = '/';
            }
        }
    }
    stream = GeneratedOutputOpen(output, temporary, sizeof(temporary));
    if(stream == NULL) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "cannot create output: %s", output);
        goto done;
    }
    int written = reporting ? assembly_opt_report(stream, &optimization) :
        listing ? AssemblyPrintLir(view, stream) : mapping ?
            assembly_machine_map(view, stream, &selection, &machine_owner, bytes, size, 1) :
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
    free(zx_bytes);
    compiler_machine_pipeline_MachineNativeFree(&machine_owner);
    compiler_opt_LirOptFree(&optimized_owner);
    AssemblyFree(lowered);
    compiler_exec_debug_ZxDebugFree(&debug_owner);
    compiler_exec_link_ZxExecReadFree(&zx_owner);
    free(merged.modules);
    ProgramsFree(&sources);
    return status;
}
