#include "zx_exec_run.h"
#include "zir_asm_lower.h"
#include "compiler_exec_link.h"
#include "compiler_opt.h"
#include "compiler_lowered_verify.h"
#include "zir_diagnostic.h"
#include "zir_scalar.h"
#include "zir_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int
ZxExecRunProgram(const LirProgram *program, LirExecLimits limits, int heap_report)
{
    int failed = 125;
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
        const char *text = ZxExecProblemText(result.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.execute",
            "instruction execution failed: %s (problem=%.*s function=%d block=%d instruction=%d origin=%d trap=%llu work_remaining=%lld fuel_remaining=%lld)",
            text != NULL ? text : "failed", (int)problem.length, problem.data, result.function, result.block, result.instruction, result.origin,
            (unsigned long long)result.trap, (long long)result.work_remaining, (long long)result.fuel_remaining);
        status = failed;
    } else if(heap_report)
        fprintf(stderr, "heap: %lld live blocks\n", (long long)compiler_exec_LirExecHeapLive(&instance));
    fflush(stdout);
    compiler_exec_LirExecInstanceClose(&instance);
    compiler_exec_LirExecPlanFree(&plan);
    return status;
}

/* A bundle's entry: a zero-argument Ziran function returning an integer,
 * bool, an enum or nothing. */
static int
portable_entry(const ZirProgram *program, const char *module, const char *function)
{
    const ZirModule *owner = NULL;
    const ZirFunction *entry = NULL;
    for(int m = 0; m < program->module_count && entry == NULL; m++) {
        if(strcmp(program->modules[m].name, module) != 0) continue;
        for(int f = 0; f < program->modules[m].function_count && entry == NULL; f++)
            if(strcmp(program->modules[m].functions[f].name, function) == 0) {
                owner = &program->modules[m];
                entry = &owner->functions[f];
            }
    }
    if(entry == NULL || entry->is_extern) return 0;
    const ZirParameters *parameters = ParametersOf(FunctionArgs(entry));
    if(parameters == NULL || parameters->count != 0) return 0;
    const char *result = entry->return_type[0] ? entry->return_type : "void";
    const ZirType *enumeration = FindType(owner, result, NULL);
    if(enumeration != NULL && enumeration->is_enum)
        result = enumeration->enum_backing[0] ? enumeration->enum_backing : "s64";
    return ScalarWidth(result) != 0 || !strcmp(result, "bool") || !strcmp(result, "void");
}

unsigned char *
ZxExecBuild(struct ZirProgram *program, const char *module, const char *function, Slice assets, size_t *size)
{
    unsigned char *bytes = NULL;
    *size = 0;
    LirOptOwner optimized = {0};
    if(!portable_entry(program, module, function)) {
        Diagnostic(Span("<bundle>", 1, 1), "zx.entry",
                   "entry must be one unique zero-argument integer, bool, or void Ziran function");
        return NULL;
    }
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
    if(interface == NULL) goto done;
    String interface_text = {interface, interface_length};
    ZxExecResult measured = compiler_exec_link_ZxExecWrite(optimized_program, assets, (Slice){0}, interface_text);
    if(measured.problem != ZxExecProblem_None || measured.size <= 0 || measured.size > 268435456) {
        String problem = compiler_exec_link_ZxExecProblemName(measured.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.container",
            "cannot measure executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
            (int)problem.length, problem.data, measured.container, measured.codec, measured.diagnostic.problem);
        goto done;
    }
    bytes = malloc((size_t)measured.size);
    if(bytes == NULL) goto done;
    ZxExecResult encoded = compiler_exec_link_ZxExecWrite(optimized_program, assets,
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
