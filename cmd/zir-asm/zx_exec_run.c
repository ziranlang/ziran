#include "zx_exec_run.h"
#include "compiler_exec_link.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
zx_text_equal(String text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && !memcmp(text.data, expected, length);
}

int
ZxPlatformService(const LirProgram *program, int64_t function)
{
    if(function < 0 || function >= program->functions.length) return ZX_SERVICE_NONE;
    const LirFunction *declared = &((const LirFunction *)program->functions.data)[function];
    if(declared->module < 0 || declared->module >= program->modules.length ||
       !zx_text_equal(((const String *)program->modules.data)[declared->module], "ziran.platform"))
        return ZX_SERVICE_NONE;
    String name = declared->name;
    if(zx_text_equal(name, "ziran.platform.write_all")) return ZX_SERVICE_PRINT_BYTES;
    if(zx_text_equal(name, "ziran.print.signed")) return ZX_SERVICE_PRINT_SIGNED;
    if(zx_text_equal(name, "ziran.print.unsigned")) return ZX_SERVICE_PRINT_UNSIGNED;
    if(zx_text_equal(name, "ziran.print.bool")) return ZX_SERVICE_PRINT_BOOL;
    if(zx_text_equal(name, "ziran.print.float32")) return ZX_SERVICE_PRINT_FLOAT32;
    if(zx_text_equal(name, "ziran.print.float64")) return ZX_SERVICE_PRINT_FLOAT64;
    return ZX_SERVICE_NONE;
}

LirExecLimits
ZxExecCommandLimits(void)
{
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    limits.work = INT64_MAX;
    limits.fuel = INT64_MAX;
    limits.frames = 1000000;
    return limits;
}

/* Output uses the same spelling as the checked language's print on every
 * other route. */
static bool
zx_run_write(void *closure, void *context, uint64_t descriptor, Slice bytes)
{
    (void)closure;
    (void)context;
    FILE *stream = descriptor == 2 ? stderr : descriptor == 1 ? stdout : NULL;
    if(stream == NULL) return false;
    return bytes.length == 0 || fwrite(bytes.data, 1, (size_t)bytes.length, stream) == (size_t)bytes.length;
}

static bool
zx_run_print(void *closure, void *context, int32_t function, Slice arguments, uint64_t *result)
{
    (void)closure;
    const LirProgram *program = context;
    const LirFunction *functions = program->functions.data;
    const uint64_t *values = arguments.data;
    *result = 0;
    if(function < 0 || function >= program->functions.length || arguments.length != 1) return false;
    char text[64];
    (void)functions;
    int service = ZxPlatformService(program, function);
    if(service == ZX_SERVICE_PRINT_SIGNED)
        snprintf(text, sizeof(text), "%lld", (long long)values[0]);
    else if(service == ZX_SERVICE_PRINT_UNSIGNED)
        snprintf(text, sizeof(text), "%llu", (unsigned long long)values[0]);
    else if(service == ZX_SERVICE_PRINT_BOOL)
        snprintf(text, sizeof(text), "%s", values[0] ? "true" : "false");
    else if(service == ZX_SERVICE_PRINT_FLOAT32) {
        uint32_t bits = (uint32_t)values[0];
        float value;
        memcpy(&value, &bits, sizeof(value));
        FormatPrintFloat(value, 1, text, sizeof(text));
    } else if(service == ZX_SERVICE_PRINT_FLOAT64) {
        double value;
        memcpy(&value, &values[0], sizeof(value));
        FormatPrintFloat(value, 0, text, sizeof(text));
    } else return false;
    return fputs(text, stdout) >= 0;
}

/* Bind every declared platform console service; other host capabilities stay
 * unbound, so calling one fails as a named host problem. */
static LirExecProblem
zx_run_bind(LirExecInstance *instance, const LirProgram *program)
{
    const LirFunction *functions = program->functions.data;
    for(int64_t index = 0; index < program->functions.length; index++) {
        if(!functions[index].external) continue;
        int service = ZxPlatformService(program, index);
        LirExecProblem bound = LirExecProblem_None;
        if(service == ZX_SERVICE_PRINT_BYTES)
            bound = compiler_exec_LirExecBindPlatformWrite(instance, (int32_t)index,
                (LirExecPlatformWrite){NULL, zx_run_write}, NULL);
        else if(service != ZX_SERVICE_NONE)
            bound = compiler_exec_LirExecBindHost(instance, (int32_t)index,
                (LirExecHostCall){NULL, zx_run_print}, (void *)program);
        if(bound != LirExecProblem_None) return bound;
    }
    return LirExecProblem_None;
}

int
ZxExecRunProgram(const LirProgram *program, LirExecLimits limits, int heap_report, int print_result)
{
    int failed = print_result ? 1 : 125;
    if(program->entry < 0 || program->entry >= program->functions.length) {
        Diagnostic((ZirSourceSpan){0}, "assembly.execute", "instruction input has no entry procedure");
        return failed;
    }
    LirFunction entry = ((LirFunction *)program->functions.data)[program->entry];
    LirType result_type = ((LirType *)program->types.data)[entry.result];
    if(entry.parameters.length != 0 || (result_type.kind != LirTypeKind_Integer &&
       result_type.kind != LirTypeKind_Bool && result_type.kind != LirTypeKind_Void)) {
        Diagnostic((ZirSourceSpan){0}, "assembly.execute", "command entry must have no parameters and return void, bool or an integer");
        return failed;
    }
    LirExecPlan plan = {0};
    LirExecInstance instance = {0};
    LirExecResult result = compiler_exec_LirExecPlanBuild(*program, compiler_lowered_ir_LirDefaultLimits(), &plan);
    if(result.problem == LirExecProblem_None)
        result = compiler_exec_LirExecInstanceOpen(&plan, limits, 1, &instance);
    if(result.problem == LirExecProblem_None) {
        LirExecProblem bound = zx_run_bind(&instance, &plan.program);
        if(bound != LirExecProblem_None) result.problem = bound;
    }
    if(result.problem == LirExecProblem_None) result = compiler_exec_LirExecRun(&instance);
    int status = result_type.kind == LirTypeKind_Void ? 0 : (int)(result.value.bits & 255);
    if(result.problem != LirExecProblem_None) {
        fflush(stdout);
        String problem = compiler_exec_LirExecProblemName(result.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.execute",
            "instruction execution failed: problem=%.*s function=%d block=%d instruction=%d origin=%d trap=%llu work_remaining=%lld fuel_remaining=%lld",
            (int)problem.length, problem.data, result.function, result.block, result.instruction, result.origin,
            (unsigned long long)result.trap, (long long)result.work_remaining, (long long)result.fuel_remaining);
        status = failed;
    } else {
        if(print_result && result_type.kind != LirTypeKind_Void) {
            /* The value as the entry's type holds it, as zi2zx run prints. */
            uint64_t bits = result.value.bits;
            int width = result_type.kind == LirTypeKind_Bool ? 1 : result_type.bits;
            if(result_type.kind == LirTypeKind_Integer && !result_type.is_signed)
                printf("%llu\n", (unsigned long long)bits);
            else {
                if(width < 64 && (bits >> (width - 1)) & 1) bits |= ~((UINT64_C(1) << width) - 1);
                printf("%lld\n", (long long)bits);
            }
            status = 0;
        }
        if(heap_report)
            fprintf(stderr, "heap: %lld live blocks\n", (long long)compiler_exec_LirExecHeapLive(&instance));
    }
    fflush(stdout);
    compiler_exec_LirExecInstanceClose(&instance);
    compiler_exec_LirExecPlanFree(&plan);
    return status;
}

int
ZxExecRunBytes(const unsigned char *bytes, size_t size, LirExecLimits limits, int print_result, int *status)
{
    /* Only a file whose header names revision 28 is read here; any other
     * file, a short or foreign one included, goes to the bundle reader, which
     * names what is wrong with it. */
    if(size < 8 || memcmp(bytes, "ZX\0\0", 4) ||
       (bytes[4] | bytes[5] << 8 | bytes[6] << 16 | (uint32_t)bytes[7] << 24) !=
           compiler_bundle_ExecutableVersionNumber()) return 0;
    ZxExecOwner owner = {0};
    ZxExecResult read = compiler_exec_link_ZxExecRead(&owner, (String){(const char *)bytes, size},
        compiler_lowered_ir_LirDefaultLimits());
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        String container = compiler_bundle_ExecutableMessage(read.container);
        Diagnostic((ZirSourceSpan){0}, "assembly.container",
            "executable ZX rejected: problem=%.*s container=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, (int)container.length, container.data,
            read.codec, (long long)read.offset, read.diagnostic.problem);
        compiler_exec_link_ZxExecReadFree(&owner);
        return -1;
    }
    LirProgram program = compiler_exec_link_ZxExecReadProgram(&owner);
    *status = ZxExecRunProgram(&program, limits, 0, print_result);
    compiler_exec_link_ZxExecReadFree(&owner);
    return 1;
}
