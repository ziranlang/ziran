#include "zir_asm_lower.h"
#include "zx_exec_run.h"
#include "zx_wasm_lower.h"
#include "zir_check.h"
#include "compiler_lowered_verify.h"
#include "compiler_lowered_x64.h"
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
    fputs("usage: zi2asm --root DIR --entry MODULE:FUNCTION [-I DIR] [--define NAME] [--bind CALLER:CAPABILITY=PROVIDER:FUNCTION] [--target ARCH-OS] [--opt] [--machine] [--emit lir|map|zx|elf|wasm|opt-report] -o FILE INPUT...\n"
          "       zi2asm --input zx [--target ARCH-OS] [--machine] [--emit lir|map|zx|elf|wasm] -o FILE INPUT\n"
          "       zi2asm --run [--opt] [--fuel N] [--work N] [--frames N] [--heap-report] INPUT...\n"
          "map selection: [--function-id N [--block-id N]] [--address-begin N --address-end N]\n"
          "--machine requires --emit elf and selects the no-fallback machine-IR Linux x86-64 path\n"
          "--heap-report prints the heap blocks still live after --run to standard error\n"
          "pipeline: source -> ZIR -> verified LIR -> optimization -> portable ZX -> interpreter OR direct machine code/ELF OR wasm32 module\n"
          "--emit wasm lowers the same verified ZX to one wasm32 module; it takes no --target, and unsupported forms are named wasm.gap errors\n"
          "native always reads verified ZX; C/Go generated binaries are separate routes, never native\n"
          "--opt selects optimized source LIR inspection; ZX production always optimizes; opt-report requires source and --opt\n"
          "experimental direct backend; native target: Linux x86-64 ELF; host capabilities are platform bindings\n"
          "native services: typed target-owned ziran.platform.write_all; unavailable services and float formatting are named hard gaps\n", stderr);
    return 2;
}

/* Validate the stable ARCH-OS row before reading input or opening output. The
 * current experimental writer owns only the Linux x86-64 entry, write_all and
 * exit_group veneer; accepting a described row must never imply a fallback. */
static int
assembly_target_supported(const char *name)
{
    const char *dash = strchr(name, '-');
    if(dash == NULL || dash == name || dash[1] == 0 || strchr(dash + 1, '-') != NULL) {
        Diagnostic((ZirSourceSpan){0}, "command.arguments",
            "unknown zi2asm target row %s; use ARCH-OS IDs from ziran capabilities --json", name);
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
            "unknown zi2asm target row %s; use ARCH-OS IDs from ziran capabilities --json", name);
        return 0;
    }
    if(!compiler_target_TargetMachineLoweringSupported(described.target)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-platform-service-unavailable",
            "assembly.gap.native-platform-service-unavailable: selected target %s has no process-entry/write_all/exit_group veneer; no libc, host binding or alternate engine was selected",
            name);
        return 0;
    }
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
assembly_text_contains(String text, const char *needle)
{
    size_t length = strlen(needle);
    if(length == 0 || length > text.length) return 0;
    for(size_t at = 0; at + length <= text.length; at++)
        if(!memcmp(text.data + at, needle, length)) return 1;
    return 0;
}

static int
assembly_parallel_function(const LirProgram *program, int symbol)
{
    if(symbol < 0 || symbol >= program->functions.length) return 0;
    LirFunction function = ((LirFunction *)program->functions.data)[symbol];
    String module = {0};
    if(function.module >= 0 && function.module < program->modules.length)
        module = ((String *)program->modules.data)[function.module];
    return assembly_text_contains(module, "parallel") ||
           assembly_text_contains(function.name, "parallel") ||
           assembly_text_contains(function.name, "Parallel");
}

static int
assembly_float_service(int service)
{
    return service == ZX_SERVICE_PRINT_FLOAT32 ||
           service == ZX_SERVICE_PRINT_FLOAT64;
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

/* Whether a pointer value can be traced to something other than one of this
 * procedure's arguments. The native encoders resolve record fields through
 * the procedure's own allocations, globals and pointers loaded from them; a
 * field reached through a pointer argument names another procedure's frame
 * and would trap at run time, so it is named as a gap. Block parameters are
 * followed to every incoming edge; `seen` breaks loops. */
static int
assembly_local_root(const LirFunction *function, int32_t value, unsigned char *seen, int depth)
{
    if(depth > 256 || value < 0 || value >= function->values.length) return 0;
    if(seen[value]) return 1;
    seen[value] = 1;
    const LirBlock *blocks = function->blocks.data;
    for(int64_t block = 0; block < function->blocks.length; block++) {
        const LirInstruction *instructions = blocks[block].instructions.data;
        for(int64_t at = 0; at < blocks[block].instructions.length; at++) {
            if(instructions[at].result != value) continue;
            if((instructions[at].op == LirOp_FieldAddress || instructions[at].op == LirOp_ElementAddress) &&
               instructions[at].operands.length > 0)
                return assembly_local_root(function, ((const int32_t *)instructions[at].operands.data)[0],
                    seen, depth + 1);
            return 1;
        }
    }
    for(int64_t block = 0; block < function->blocks.length; block++) {
        const int32_t *parameters = blocks[block].parameters.data;
        for(int64_t position = 0; position < blocks[block].parameters.length; position++) {
            if(parameters[position] != value) continue;
            /* The entry block's parameters are the procedure's arguments. */
            if(block == function->entry) return 0;
            for(int64_t from = 0; from < function->blocks.length; from++) {
                int64_t count = blocks[from].instructions.length;
                if(count == 0) continue;
                const LirInstruction *last = &((const LirInstruction *)blocks[from].instructions.data)[count - 1];
                const LirEdge *edges = last->edges.data;
                for(int64_t edge = 0; edge < last->edges.length; edge++) {
                    if(edges[edge].block != block || position >= edges[edge].arguments.length) continue;
                    if(!assembly_local_root(function, ((const int32_t *)edges[edge].arguments.data)[position],
                        seen, depth + 1)) return 0;
                }
            }
            return 1;
        }
    }
    return 0;
}

static int
assembly_pointer_field(const LirFunction *function, LirInstruction instruction)
{
    if(instruction.op != LirOp_FieldAddress || instruction.operands.length != 1) return 0;
    unsigned char *seen = calloc((size_t)function->values.length + 1, 1);
    if(seen == NULL) return 1;
    int local = assembly_local_root(function, ((const int32_t *)instruction.operands.data)[0], seen, 0);
    free(seen);
    return !local;
}

/* A checked view element address (data, index, count[, high]) whose element
 * is a record, fixed array or procedure value. Portable execution supports
 * it; the native encoders address only scalar view elements. */
static int
assembly_aggregate_view(const LirProgram *program, const LirFunction *function, LirInstruction instruction)
{
    if(instruction.op != LirOp_ElementAddress || instruction.operands.length < 3) return 0;
    int value = ((int *)instruction.operands.data)[0];
    if(value < 0 || value >= function->values.length) return 0;
    const LirType *types = program->types.data;
    LirType pointer = types[((int *)function->values.data)[value]];
    if(pointer.kind != LirTypeKind_Pointer || pointer.element < 0 || pointer.element >= program->types.length) return 0;
    LirTypeKind kind = types[pointer.element].kind;
    return kind == LirTypeKind_Record || kind == LirTypeKind_Array || kind == LirTypeKind_Function;
}

/* Every known unsupported machine-AOT surface has a stable, named hard gap.
 * This runs before the pipeline, so no other backend or execution engine can
 * be selected after one of these diagnostics. */
static int
assembly_machine_gap(const LirProgram *program)
{
    const LirFunction *functions = program->functions.data;
    for(int64_t index = 0; index < program->functions.length; index++) {
        if(!functions[index].external) continue;
        int service = ZxPlatformService(program, index);
        if(assembly_float_service(service))
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-float-format",
                "assembly.gap.machine-float-format: machine ELF has no native float print formatter; no alternate engine was selected");
        else if(service != ZX_SERVICE_NONE)
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-platform-service-unavailable",
                "assembly.gap.machine-platform-service-unavailable: machine ELF does not implement platform service %.*s; no alternate engine was selected",
                (int)functions[index].name.length, functions[index].name.data);
        else if(assembly_parallel_function(program, (int)index))
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-parallel",
                "machine ELF does not implement parallel runtime calls; no alternate engine was selected");
        else
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-host",
                "machine ELF does not implement host capability %.*s; no alternate engine was selected",
                (int)functions[index].name.length, functions[index].name.data);
        return 1;
    }
    const LirGlobal *globals = program->globals.data;
    for(int64_t index = 0; index < program->globals.length; index++) {
        if(!globals[index].writable) {
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-string",
                "machine ELF does not implement read-only string/data globals; no alternate engine was selected");
            return 1;
        }
        if(globals[index].constant >= 0) {
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-global-init",
                "machine ELF does not implement explicit global initial data; no alternate engine was selected");
            return 1;
        }
    }
    if(program->initializers.length != 0) {
        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-global-init",
            "machine ELF does not implement runtime module initialization; no alternate engine was selected");
        return 1;
    }
    for(int64_t function = 0; function < program->functions.length; function++) {
        const LirBlock *blocks = functions[function].blocks.data;
        for(int64_t block = 0; block < functions[function].blocks.length; block++) {
            const LirInstruction *instructions = blocks[block].instructions.data;
            for(int64_t instruction = 0; instruction < blocks[block].instructions.length; instruction++) {
                LirInstruction current = instructions[instruction];
                if(current.op == LirOp_Resize) {
                    Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-heap",
                        "assembly.gap.native-heap: machine ELF does not implement heap storage; no alternate engine was selected");
                    return 1;
                }
                if(current.op == LirOp_BytesEqual) {
                    Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-string",
                        "machine ELF does not implement string comparison; no alternate engine was selected");
                    return 1;
                }
                if(assembly_aggregate_view(program, &functions[function], current)) {
                    Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-aggregate-view",
                        "assembly.gap.native-aggregate-view: machine ELF does not implement checked views of records, arrays or procedure values; no alternate engine was selected");
                    return 1;
                }
                if(assembly_pointer_field(&functions[function], current)) {
                    Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-pointer-field",
                        "assembly.gap.native-pointer-field: machine ELF does not implement record fields reached through a pointer parameter; no alternate engine was selected");
                    return 1;
                }
                if(current.op == LirOp_ElementAddress && current.operands.length == 4) {
                    Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-slice",
                        "machine ELF does not implement borrowed slices; no alternate engine was selected");
                    return 1;
                }
                if(current.op == LirOp_HostCall) {
                    int service = current.symbol >= 0 && current.symbol < program->functions.length ?
                        ZxPlatformService(program, current.symbol) : ZX_SERVICE_NONE;
                    if(assembly_float_service(service))
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-float-format",
                            "assembly.gap.machine-float-format: machine ELF has no native float print formatter; no alternate engine was selected");
                    else if(service != ZX_SERVICE_NONE)
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-platform-service-unavailable",
                            "assembly.gap.machine-platform-service-unavailable: machine ELF does not implement platform service %.*s; no alternate engine was selected",
                            (int)functions[current.symbol].name.length, functions[current.symbol].name.data);
                    else if(assembly_parallel_function(program, current.symbol))
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-parallel",
                            "machine ELF does not implement parallel runtime calls; no alternate engine was selected");
                    else
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-host",
                            "machine ELF does not implement host calls; no alternate engine was selected");
                    return 1;
                }
            }
        }
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
    const char *target = NULL;
    const char *fuel = NULL;
    const char *work = NULL;
    const char *frames = NULL;
    const char *function_id = NULL;
    const char *block_id = NULL;
    const char *address_begin = NULL;
    const char *address_end = NULL;
    AssemblyMapSelection selection = {.function = -1, .block = -1, .begin = -1, .end = -1};
    int run = 0;
    int heap_report = 0;
    int optimize = 0;
    int machine = 0;
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
        COMMAND_TEXT("--root", root), COMMAND_TEXT("--entry", entry),
        COMMAND_TEXT("-o", output), COMMAND_TEXT("--emit", emit),
        COMMAND_TEXT("--input", input_kind), COMMAND_TEXT("--target", target),
        COMMAND_FLAG("--run", run), COMMAND_FLAG("--opt", optimize), COMMAND_FLAG("--machine", machine),
        COMMAND_FLAG("--heap-report", heap_report),
        COMMAND_TEXT("--fuel", fuel),
        COMMAND_TEXT("--work", work), COMMAND_TEXT("--frames", frames),
        COMMAND_LIST("--bind", bindings, binding_count, 64, NULL),
        COMMAND_LIST("--define", defines, define_count, 64, valid_define),
        COMMAND_TEXT("--function-id", function_id), COMMAND_TEXT("--block-id", block_id),
        COMMAND_TEXT("--address-begin", address_begin), COMMAND_TEXT("--address-end", address_end),
        COMMAND_LIST("--module-path", module_paths, module_path_count, argc, NULL),
        COMMAND_LIST("-I", module_paths, module_path_count, argc, NULL)
    };
    int input = ReadCommandOptions(argc, argv, options, sizeof(options) / sizeof(*options));
    char module[ZIR_NAME_MAX], function[ZIR_NAME_MAX];
    int serialized = !strcmp(input_kind, "zx");
    LirExecLimits execution_limits = compiler_exec_LirExecDefaultLimits();
    if(input <= 0 || (!run && output == NULL) ||
       (run && (output != NULL || strcmp(emit, "elf"))) ||
       (!run && (fuel != NULL || work != NULL || frames != NULL || heap_report)) ||
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
       (serialized && (argc - input != 1 || root != NULL || entry != NULL ||
        module_path_count != 0 || binding_count != 0 || define_count != 0)) ||
       (machine && (run || strcmp(emit, "elf"))) ||
       (!strcmp(emit, "opt-report") && (!optimize || serialized)) ||
       (!strcmp(emit, "wasm") && (target != NULL || optimize)) ||
       (strcmp(emit, "lir") && strcmp(emit, "map") && strcmp(emit, "zx") && strcmp(emit, "elf") && strcmp(emit, "wasm") && strcmp(emit, "opt-report"))) return assembly_usage();
    /* Wasm is its own lowering of portable ZX, not a native ARCH-OS row. */
    int wasm = !strcmp(emit, "wasm");
    if(!wasm && !assembly_target_supported(target != NULL ? target : "x86_64-linux")) return 1;
    int status = 1;
    ProgramSet sources = {0};
    ZirProgram merged = {0};
    AssemblyProgram *lowered = NULL;
    ZxExecOwner zx_owner = {0};
    LirProgram decoded = {0};
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
        if(wasm) CompilerConfigureTarget(ZIR_TARGET_WASM, ZIR_OS_WEB, ZIR_CPU_WASM32);
        if(!ProgramsLoadWithDefines(&sources, root, module_paths, module_path_count, defines, define_count,
                                    (const char *const *)(argv + input), argc - input) ||
           !CheckCanonicalPrograms(sources.programs, sources.count, (const char *const *)sources.paths) ||
           !CommandMergePrograms(&sources, &merged)) goto done;
        ZirProgram *programs[] = {&merged};
        if(!LinkImports(programs, 1)) goto done;
        for(int binding = 0; binding < binding_count; binding++)
            if(!BindHostProvider(&merged, bindings[binding])) goto done;
        if(machine && assembly_machine_source_gap(&merged)) goto done;
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
        ZxExecResult measured = compiler_exec_link_ZxExecWrite(*view, (Slice){0}, (Slice){0}, interface_text);
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
            interface_text);
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
    if(!strcmp(emit, "zx")) {
        /* Preserve all declared assets and optional sections when rewriting. */
        bytes = zx_bytes;
        zx_bytes = NULL;
        size = zx_size;
    } else if(wasm) {
        /* The module comes from the verified ZX bytes alone, so source and
         * saved-ZX input give identical modules. */
        ZxWasmImage image = {0};
        if(!ZxWasmLowerExecutable(zx_bytes, (size_t)zx_size, &image)) goto done;
        size = (int64_t)image.size;
        bytes = image.bytes;
        image.bytes = NULL;
        ZxWasmImageFree(&image);
    } else if(executable || mapping) {
        if(machine) {
            if(assembly_machine_gap(view)) goto done;
            MachineDiagnostic diagnostic = compiler_machine_pipeline_MachineNativeBuild(
                *view, compiler_target_TargetLinuxX64(), compiler_machine_ir_MachineDefaultLimits(),
                &machine_owner);
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
        } else {
        const LirFunction *functions = view->functions.data;
        for(int64_t index = 0; index < view->functions.length; index++) {
            if(!functions[index].external) continue;
            int service = ZxPlatformService(view, index);
            if(assembly_float_service(service)) {
                Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-float-format",
                    "assembly.gap.native-float-format: selected target linux-x86_64-elf has no native float print formatter; no alternate engine was selected");
                goto done;
            }
            if(service == ZX_SERVICE_NONE) {
                Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-platform-service-unavailable",
                    "assembly.gap.native-platform-service-unavailable: selected target linux-x86_64-elf does not implement platform service %.*s; no alternate engine was selected",
                    (int)functions[index].name.length, functions[index].name.data);
                goto done;
            }
        }
        for(int64_t function = 0; function < view->functions.length; function++) {
            const LirBlock *blocks = functions[function].blocks.data;
            for(int64_t block = 0; block < functions[function].blocks.length; block++) {
                const LirInstruction *instructions = blocks[block].instructions.data;
                for(int64_t instruction = 0; instruction < blocks[block].instructions.length; instruction++) {
                    if(instructions[instruction].op == LirOp_Resize) {
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-heap",
                            "assembly.gap.native-heap: selected target linux-x86_64-elf does not implement heap storage; no alternate engine was selected");
                        goto done;
                    }
                    if(instructions[instruction].op == LirOp_BytesEqual) {
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-string-compare",
                            "assembly.gap.native-string-compare: selected target linux-x86_64-elf does not implement string comparison; no alternate engine was selected");
                        goto done;
                    }
                    if(assembly_aggregate_view(view, &functions[function], instructions[instruction])) {
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-aggregate-view",
                            "assembly.gap.native-aggregate-view: selected target linux-x86_64-elf does not implement checked views of records, arrays or procedure values; no alternate engine was selected");
                        goto done;
                    }
                    if(assembly_pointer_field(&functions[function], instructions[instruction])) {
                        Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-pointer-field",
                            "assembly.gap.native-pointer-field: selected target linux-x86_64-elf does not implement record fields reached through a pointer parameter; no alternate engine was selected");
                        goto done;
                    }
                }
            }
        }
        LirNativeResult measured = compiler_lowered_x64_LirX64Elf(*view, (Slice){0});
        if(measured.problem != LirProblem_None || measured.size <= 0 || measured.size > 268435456) {
            /* The encoder's two expected refusals are gaps of this target,
             * not encoder faults: its bounded tables, and forms it has no
             * code for yet. */
            if(measured.problem == LirProblem_Limit)
                Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-limit",
                    "assembly.gap.native-limit: selected target linux-x86_64-elf encoder bounds on addresses, frame size or compile work do not admit this program; no alternate engine was selected");
            else if(measured.problem == LirProblem_Unsupported)
                Diagnostic((ZirSourceSpan){0}, "assembly.gap.native-unsupported",
                    "assembly.gap.native-unsupported: selected target linux-x86_64-elf has no code for an instruction or entry signature in this program; no alternate engine was selected");
            else
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
    }
    if(!strcmp(output, "-") && !executable) {
        int written = reporting ? assembly_opt_report(stdout, &optimization) :
            listing ? AssemblyPrintLir(view, stdout) : mapping ? assembly_map(view, stdout, &selection, bytes, size, 1) :
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
        listing ? AssemblyPrintLir(view, stream) : mapping ? assembly_map(view, stream, &selection, bytes, size, 1) :
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
    compiler_exec_link_ZxExecReadFree(&zx_owner);
    free(merged.modules);
    ProgramsFree(&sources);
    return status;
}
