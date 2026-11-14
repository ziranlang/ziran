#include "zx_exec_run.h"
#include "zir_asm_lower.h"
#include "compiler_exec_link.h"
#include "compiler_opt.h"
#include "compiler_lowered_verify.h"
#include "zir_diagnostic.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

LirExecLimits
ZxExecCommandLimits(void)
{
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    limits.work = INT64_MAX;
    limits.fuel = INT64_MAX;
    limits.frames = 1000000;
    return limits;
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
        LirExecProblem bound = ZxBindConsole(&instance, &plan.program);
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

unsigned char *
ZxExecBuild(struct ZirProgram *program, const char *module, const char *function, size_t *size)
{
    unsigned char *bytes = NULL;
    *size = 0;
    LirOptOwner optimized = {0};
    AssemblyProgram *lowered = AssemblyLower(program, module, function);
    if(lowered == NULL) return NULL;
    const LirProgram *view = AssemblyView(lowered);
    LirDiagnostic diagnostic = compiler_lowered_verify_LirVerify(*view, compiler_lowered_ir_LirDefaultLimits());
    if(diagnostic.problem != LirProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.verify",
            "lowered IR rejected: problem=%d function=%d block=%d instruction=%d value=%d origin=%d",
            diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
            diagnostic.value, diagnostic.origin);
        goto done;
    }
    LirOptResult optimization = compiler_opt_LirOptBuild(&optimized, *view,
        compiler_lowered_ir_LirDefaultLimits(), compiler_opt_LirOptDefaultOptions());
    if(optimization.problem != LirOptProblem_None) {
        LirDiagnostic rejected = optimization.diagnostic;
        Diagnostic((ZirSourceSpan){0}, "assembly.optimize",
            "optimization rejected: problem=%d verifier=%d function=%d block=%d instruction=%d value=%d origin=%d",
            optimization.problem, rejected.problem, rejected.function, rejected.block,
            rejected.instruction, rejected.value, rejected.origin);
        goto done;
    }
    LirProgram optimized_program = compiler_opt_LirOptProgram(&optimized);
    size_t interface_length = 0;
    const char *interface = AssemblyHostInterface(lowered, &interface_length);
    String interface_text = {interface ? interface : "", interface_length};
    ZxExecResult measured = compiler_exec_link_ZxExecWrite(optimized_program, (Slice){0}, (Slice){0}, interface_text);
    if(measured.problem != ZxExecProblem_None || measured.size <= 0 || measured.size > 268435456) {
        String problem = compiler_exec_link_ZxExecProblemName(measured.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.container",
            "cannot measure executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
            (int)problem.length, problem.data, measured.container, measured.codec, measured.diagnostic.problem);
        goto done;
    }
    bytes = malloc((size_t)measured.size);
    if(bytes == NULL) goto done;
    ZxExecResult encoded = compiler_exec_link_ZxExecWrite(optimized_program, (Slice){0},
        (Slice){bytes, measured.size}, interface_text);
    if(encoded.problem != ZxExecProblem_None || encoded.size != measured.size) {
        String problem = compiler_exec_link_ZxExecProblemName(encoded.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.container",
            "cannot encode executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
            (int)problem.length, problem.data, encoded.container, encoded.codec, encoded.diagnostic.problem);
        free(bytes);
        bytes = NULL;
        goto done;
    }
    *size = (size_t)measured.size;
done:
    compiler_opt_LirOptFree(&optimized);
    AssemblyFree(lowered);
    return bytes;
}
