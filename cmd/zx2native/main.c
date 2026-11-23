#include "zir_exec_console.h"
#include "compiler_lowered_verify.h"
#include "compiler_exec_debug.h"
#include "compiler_exec_link.h"
#include "compiler_machine_ir.h"
#include "compiler_machine_pe.h"
#include "compiler_machine_pipeline.h"
#include "compiler_target.h"
#include "zir_command.h"
#include "zir_sha256.h"
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int
assembly_usage(void)
{
    fputs("usage: zx2native [--target x86_64|aarch64|armv7] [--platform linux|android|windows] [--emit elf|map|image] [--fuel N] -o FILE INPUT.zx\n"
          "elf writes the platform's executable: an ELF on Linux, a PE32+ .exe on Windows\n"
          "x86-64 Linux ELF and Windows PE carry the files and clock services themselves and link other capabilities from registered host libraries\n"
          "map selection: [--function-id N [--block-id N]] [--address-begin N --address-end N]\n"
          "--fuel sets an executable's starting fuel; it is unbounded by default, as for a bundle run\n"
          "zx2native reads only decoded and verified ZX and uses only the no-fallback machine-IR path\n"
          "image is reserved for the relocatable in-memory image and is a named gap until that loader lands\n", stderr);
    return 2;
}

/* Validate the stable ARCH-OS row before reading input or opening output. Only
 * a row whose registry entry names an executable container (an ELF for
 * x86_64-linux, a PE for x86_64-windows) has the process-entry, console and
 * exit runtime; accepting a described row must never imply a fallback. */
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
            "assembly.gap.machine-platform-service-unavailable: selected target %s has no executable writer or process-entry/write_all/exit runtime; no libc, host binding or alternate engine was selected",
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

static unsigned char *
assembly_read(const char *path, size_t *size)
{
    unsigned char *bytes = CommandReadBounded(path, 268435456, size);
    if(bytes == NULL)
        Diagnostic((ZirSourceSpan){0}, "assembly.codec", "cannot read bounded instruction input: %s", path);
    return bytes;
}

/* Host libraries. A `NAME.library` registration beside the `.host` ones that
 * `ziran run` dispatches to holds one absolute path to a shared library that
 * defines host capabilities by name, with the C ABI the C target declares
 * for them. Registrations are read where `ziran run` finds providers:
 * ZIRAN_HOST_PATH's absolute directories, or $XDG_DATA_HOME/ziran/hosts
 * (~/.local/share/ziran/hosts) and then $XDG_DATA_DIRS (/usr/local/share and
 * /usr/share) with /ziran/hosts, each directory's in name order. A Linux
 * x86-64 ELF or Windows PE links each capability the image does not carry
 * itself from the first library that defines it; a capability none defines
 * is a gap. */
typedef struct AssemblyHostLibrary {
    char *path;
    unsigned char *bytes;
    size_t size;
    int32_t linked;
} AssemblyHostLibrary;

typedef struct AssemblyHosts {
    AssemblyHostLibrary *libraries;
    size_t count, capacity;
    String *paths;
    int32_t *providers;
    MachineHostLinks links;
} AssemblyHosts;

static void
assembly_hosts_free(AssemblyHosts *hosts)
{
    for(size_t at = 0; at < hosts->count; at++) {
        free(hosts->libraries[at].path);
        free(hosts->libraries[at].bytes);
    }
    free(hosts->libraries);
    free(hosts->paths);
    free(hosts->providers);
    memset(hosts, 0, sizeof(*hosts));
}

#if defined(__unix__) || defined(__APPLE__)
static int
assembly_name_order(const void *left, const void *right)
{
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* A registration's library: its trimmed text, one absolute path without
 * control bytes. An unreadable or invalid registration is skipped. */
static int
assembly_hosts_register(AssemblyHosts *hosts, const char *registration)
{
    size_t size = 0;
    char *text = (char *)CommandReadBounded(registration, 16384, &size);
    if(text == NULL) return 1;
    size_t begin = 0, end = size;
    while(begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' || text[begin] == '\r')) begin++;
    while(end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
    int valid = end > begin && text[begin] == '/';
    for(size_t at = begin; valid && at < end; at++)
        if((unsigned char)text[at] < 32 || (unsigned char)text[at] == 127) valid = 0;
    if(!valid) { free(text); return 1; }
    memmove(text, text + begin, end - begin);
    text[end - begin] = 0;
    AssemblyHostLibrary library = {.path = text, .linked = -1};
    library.bytes = CommandReadBounded(text, 268435456, &library.size);
    if(library.bytes == NULL) { free(text); return 1; }
    if(hosts->count == hosts->capacity) {
        size_t capacity = hosts->capacity ? hosts->capacity * 2 : 4;
        AssemblyHostLibrary *grown = realloc(hosts->libraries, capacity * sizeof(*grown));
        if(grown == NULL) { free(library.bytes); free(text); DiagnosticOutOfMemory(); return 0; }
        hosts->libraries = grown;
        hosts->capacity = capacity;
    }
    hosts->libraries[hosts->count++] = library;
    return 1;
}

static int
assembly_hosts_directory(AssemblyHosts *hosts, const char *directory, size_t length, const char *suffix)
{
    char path[ZIR_PATH_MAX];
    if(length == 0 || directory[0] != '/' ||
       snprintf(path, sizeof(path), "%.*s%s", (int)length, directory, suffix) >= (int)sizeof(path)) return 1;
    DIR *cursor = opendir(path);
    if(cursor == NULL) return 1;
    char **names = NULL;
    size_t count = 0, capacity = 0;
    int ok = 1;
    for(struct dirent *entry = readdir(cursor); entry != NULL; entry = readdir(cursor)) {
        size_t name = strlen(entry->d_name);
        if(name <= 8 || strcmp(entry->d_name + name - 8, ".library")) continue;
        if(count == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            char **grown = realloc(names, capacity * sizeof(*grown));
            if(grown == NULL) { ok = 0; break; }
            names = grown;
        }
        if((names[count] = strdup(entry->d_name)) == NULL) { ok = 0; break; }
        count++;
    }
    closedir(cursor);
    if(ok && count) qsort(names, count, sizeof(*names), assembly_name_order);
    for(size_t at = 0; ok && at < count; at++) {
        char registration[ZIR_PATH_MAX];
        if(snprintf(registration, sizeof(registration), "%s/%s", path, names[at]) < (int)sizeof(registration))
            ok = assembly_hosts_register(hosts, registration);
    }
    for(size_t at = 0; at < count; at++) free(names[at]);
    free(names);
    if(!ok) DiagnosticOutOfMemory();
    return ok;
}

static int
assembly_hosts_directories(AssemblyHosts *hosts, const char *list, const char *suffix)
{
    for(const char *start = list; ; ) {
        const char *end = strchr(start, ':');
        size_t length = end ? (size_t)(end - start) : strlen(start);
        if(!assembly_hosts_directory(hosts, start, length, suffix)) return 0;
        if(end == NULL) return 1;
        start = end + 1;
    }
}

static int
assembly_hosts_load(AssemblyHosts *hosts)
{
    const char *paths = getenv("ZIRAN_HOST_PATH");
    if(paths && *paths) return assembly_hosts_directories(hosts, paths, "");
    const char *data = getenv("XDG_DATA_HOME");
    char home[ZIR_PATH_MAX];
    if(data == NULL || *data == 0) {
        const char *user = getenv("HOME");
        if(user && *user && snprintf(home, sizeof(home), "%s/.local/share", user) < (int)sizeof(home)) data = home;
    }
    if(data && !assembly_hosts_directory(hosts, data, strlen(data), "/ziran/hosts")) return 0;
    const char *shared = getenv("XDG_DATA_DIRS");
    return assembly_hosts_directories(hosts, shared && *shared ? shared : "/usr/local/share:/usr/share",
        "/ziran/hosts");
}
#else
static int
assembly_hosts_load(AssemblyHosts *hosts)
{
    (void)hosts;
    return 1;
}
#endif

/* Every host capability of PROGRAM needs a provider. Linux ELF and Windows PE
 * on x86-64 carry the ZX Runner's files and clock services themselves (a -2
 * provider, MachineX64HostBuiltin) and link every other capability from a
 * library that defines it; other rows are gaps. Platform services are lowered
 * by the machine pipeline itself. Fills HOSTS->links. */
static int
assembly_machine_hosts(const LirProgram *program, TargetDescription target, AssemblyHosts *hosts)
{
    const LirFunction *functions = program->functions.data;
    const String *modules = program->modules.data;
    int linking = target.arch == TargetArch_X86_64 &&
        (target.os == TargetOS_Linux || target.os == TargetOS_Windows);
    int allocated = 0;
    int loaded = 0;
    for(int64_t index = 0; index < program->functions.length; index++) {
        if(!functions[index].external || ZxPlatformService(program, index) != ZX_SERVICE_NONE) continue;
        String name = functions[index].name;
        int builtin = linking && functions[index].module >= 0 &&
            functions[index].module < program->modules.length &&
            compiler_target_TargetBuiltinHost(modules[functions[index].module], name) != TargetService_Invalid;
        if(linking && !allocated) {
            allocated = 1;
            hosts->providers = malloc((size_t)program->functions.length * sizeof(*hosts->providers));
            if(hosts->providers == NULL) { DiagnosticOutOfMemory(); return 1; }
            for(int64_t at = 0; at < program->functions.length; at++) hosts->providers[at] = -1;
            /* Every host function has its provider entry from here on, so a
             * program whose capabilities are all built in loads no library. */
            hosts->links.providers = (Slice){hosts->providers, program->functions.length};
        }
        if(builtin) {
            hosts->providers[index] = MachineX64HostBuiltin;
            continue;
        }
        /* The libraries are read once, when the first capability needs one. */
        if(linking && !loaded) {
            loaded = 1;
            if(!assembly_hosts_load(hosts)) return 1;
            hosts->paths = calloc(hosts->count ? hosts->count : 1, sizeof(*hosts->paths));
            if(hosts->paths == NULL) { DiagnosticOutOfMemory(); return 1; }
        }
        size_t provider = 0;
        while(linking && provider < hosts->count &&
              !(target.os == TargetOS_Windows ? compiler_machine_pe_MachinePeLibraryDefines(
                  (Slice){hosts->libraries[provider].bytes, (int64_t)hosts->libraries[provider].size}, name) :
                compiler_machine_elf_MachineElfLibraryDefines(
                  (Slice){hosts->libraries[provider].bytes, (int64_t)hosts->libraries[provider].size}, name)))
            provider++;
        if(!linking || provider == hosts->count) {
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-host", linking ?
                "assembly.gap.machine-host: machine %s does not implement host capability %.*s: no registered host library defines it; no alternate engine was selected" :
                "assembly.gap.machine-host: machine %s does not implement host capability %.*s; no alternate engine was selected",
                target.os == TargetOS_Windows ? "PE" : "ELF", (int)name.length, name.data);
            return 1;
        }
        AssemblyHostLibrary *library = &hosts->libraries[provider];
        if(library->linked < 0) {
            library->linked = (int32_t)hosts->links.libraries.length;
            const char *path = library->path;
            if(target.os == TargetOS_Windows) {
                const char *slash = strrchr(path, '/');
                if(slash) path = slash + 1;
            }
            hosts->paths[library->linked] = (String){(char *)path, strlen(path)};
            hosts->links.libraries = (Slice){hosts->paths, hosts->links.libraries.length + 1};
        }
        hosts->providers[index] = library->linked;
    }
    return 0;
}

/* A host capability whose parameters or result the C ABI veneer cannot carry. */
static void
assembly_host_abi_gap(const LirProgram *program, int32_t function, TargetDescription target)
{
    String name = function >= 0 && function < program->functions.length ?
        ((const LirFunction *)program->functions.data)[function].name : (String){"?", 1};
    Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-host-abi",
        "assembly.gap.machine-host-abi: machine %s cannot pass host capability %.*s's parameters or result through the C ABI: "
        "only bool, integer, pointer and float values and records of one or two 8-byte integers or pointers cross it; "
        "no alternate engine was selected", target.os == TargetOS_Windows ? "PE" : "ELF", (int)name.length, name.data);
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

/* Where the executable container put the text and data: a PE on Windows, an
 * ELF on Linux. Virtual addresses are the preferred ones; 0 without data. */
typedef struct AssemblyImageFacts {
    const char *target;
    const char *kind;
    const char *address_space;
    const char *scope;
    int64_t startup;
    int64_t size;
    int64_t text_offset;
    uint64_t text_address;
    int64_t text_size;
    int64_t data_offset;
    uint64_t data_address;
    int64_t data_size;
    int problem;
} AssemblyImageFacts;

static AssemblyImageFacts
assembly_image_facts(const LirProgram *program, const MachineNativeOwner *owner, MachineHostLinks links)
{
    MachineProgram machine = owner->machine.program;
    if(machine.target.os == TargetOS_Windows) {
        MachinePeResult pe = compiler_machine_pipeline_MachineNativePeLayout(*program,
            (MachineNativeOwner *)owner, links);
        return (AssemblyImageFacts){.target = "x86_64-windows", .kind = "aot-pe",
            .address_space = "pe-virtual", .scope = "complete-pe", .startup = pe.startup_size,
            .size = pe.size, .text_offset = pe.text_offset,
            .text_address = MachinePeImageBase + (uint64_t)pe.text_rva, .text_size = pe.text_size,
            .data_offset = pe.data_offset,
            .data_address = pe.data_rva ? MachinePeImageBase + (uint64_t)pe.data_rva : 0,
            .data_size = pe.data_size, .problem = pe.diagnostic.problem};
    }
    MachineElfResult elf = compiler_machine_pipeline_MachineNativeElfLayout(*program,
        (MachineNativeOwner *)owner, links);
    return (AssemblyImageFacts){.target = "x86_64-linux", .kind = "aot-elf",
        .address_space = "elf-virtual", .scope = "complete-elf", .startup = elf.startup_size,
        .size = elf.size, .text_offset = elf.text_offset,
        .text_address = MachineElfBase + (uint64_t)elf.text_offset, .text_size = elf.text_size,
        .data_offset = elf.data_offset,
        .data_address = elf.data_address ? MachineElfBase + (uint64_t)elf.data_address : 0,
        .data_size = elf.data_size, .problem = elf.diagnostic.problem};
}

static int
assembly_machine_map(const LirProgram *program, FILE *stream, const AssemblyMapSelection *selection,
             const MachineNativeOwner *owner, MachineHostLinks links, const unsigned char *bytes, int64_t size,
             int optimize)
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
    AssemblyImageFacts image = assembly_image_facts(program, owner, links);
    if(image.problem != MachineProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.encode", "cannot measure native machine image layout: problem=%d",
            image.problem);
        return 0;
    }
    /* Text-relative coordinates start after the process entry stub. */
    int64_t startup = image.startup;
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
    fprintf(stream, "{\"format\":\"ziran-native-map\",\"version\":1,\"target\":\"%s\","
          "\"coordinate\":\"text-relative\",\"variant\":\"x86-64-baseline\",\"abi\":\"ziran-internal\",", image.target);
    fprintf(stream, "\"optimization_request\":\"%s\",\"identity\":{\"algorithm\":\"sha256\",\"scope\":\"%s\",\"digest\":\"%s\"},",
        optimize ? "verified-basic" : "none", image.scope, identity);
    fprintf(stream, "\"image\":{\"kind\":\"%s\",\"address_space\":\"%s\",", image.kind, image.address_space);
    fprintf(stream, "\"file_size\":%lld,\"text_file_offset\":%lld,\"text_virtual_address\":%llu,\"text_size\":%lld,"
        "\"data_file_offset\":%lld,\"data_virtual_address\":%llu,\"data_size\":%lld},\"selection\":",
        (long long)image.size, (long long)image.text_offset,
        (unsigned long long)image.text_address,
        (long long)image.text_size, (long long)image.data_offset,
        (unsigned long long)image.data_address,
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
    const char *output = NULL;
    const char *emit = "elf";
    const char *target = "x86_64";
    const char *platform = "linux";
    const char *function_id = NULL;
    const char *block_id = NULL;
    const char *address_begin = NULL;
    const char *address_end = NULL;
    const char *elf_fuel_text = NULL;
    int64_t elf_fuel = INT64_MAX;
    AssemblyMapSelection selection = {.function = -1, .block = -1, .begin = -1, .end = -1};
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
       (strcmp(platform, "linux") && strcmp(platform, "android") && strcmp(platform, "windows")) ||
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
    ZxExecOwner zx_owner = {0};
    ZxDebugOwner debug_owner = {0};
    LirProgram decoded = {0};
    LirProgram origins = {0};
    MachineNativeOwner machine_owner = {0};
    AssemblyHosts hosts = {0};
    const LirProgram *view = NULL;
    unsigned char *bytes = NULL;
    unsigned char *zx_bytes = NULL;
    FILE *stream = NULL;
    char temporary[ZIR_PATH_MAX] = {0};
    size_t input_size = 0;
    zx_bytes = assembly_read(argv[input], &input_size);
    if(zx_bytes == NULL) goto done;
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
    view = &decoded;
    LirDiagnostic diagnostic = compiler_lowered_verify_LirVerify(*view, compiler_lowered_ir_LirDefaultLimits());
    if(diagnostic.problem != LirProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.verify",
            "lowered IR rejected: problem=%d function=%d block=%d instruction=%d value=%d origin=%d",
            diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
            diagnostic.value, diagnostic.origin);
        goto done;
    }
    int mapping = !strcmp(emit, "map");
    int executable = !strcmp(emit, "elf");
    int64_t size = 0;
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
    {
        if(assembly_machine_hosts(view, native_target, &hosts)) goto done;
        MachineDiagnostic diagnostic = compiler_machine_pipeline_MachineNativeBuild(
            *view, native_target, compiler_machine_ir_MachineDefaultLimits(),
            &machine_owner, elf_fuel, hosts.links);
        if(diagnostic.problem == MachineProblem_HostAbi) {
            assembly_host_abi_gap(view, diagnostic.function, native_target);
            goto done;
        }
        if(diagnostic.problem == MachineProblem_UnsupportedTarget) {
            Diagnostic((ZirSourceSpan){0}, "assembly.gap.machine-platform-service-unavailable",
                "assembly.gap.machine-platform-service-unavailable: selected target %s has no formatter, host binding or executable runtime for what this program calls; no alternate engine was selected",
                target_row);
            goto done;
        }
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
        int written = assembly_machine_map(view, stdout, &selection, &machine_owner, hosts.links, bytes, size, 1);
        status = written && fflush(stdout) == 0 ? 0 : 1;
        goto done;
    }
    if(!strcmp(output, "-")) {
        Diagnostic((ZirSourceSpan){0}, "assembly.output", "executable output requires a file path");
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
    int written = mapping ? assembly_machine_map(view, stream, &selection, &machine_owner, hosts.links, bytes, size, 1) :
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
    assembly_hosts_free(&hosts);
    compiler_exec_debug_ZxDebugFree(&debug_owner);
    compiler_exec_link_ZxExecReadFree(&zx_owner);
    return status;
}
