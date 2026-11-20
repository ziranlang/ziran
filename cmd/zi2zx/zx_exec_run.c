#include "zx_exec_run.h"
#include "compiler_zir_lower.h"
#include "compiler_exec_link.h"
#include "compiler_opt.h"
#include "compiler_lowered_verify.h"
#include "zir_command.h"
#include "zir_diagnostic.h"
#include "zir_scalar.h"
#include "zir_sha256.h"
#include "zir_text.h"
#include "ziran_zx_module.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
    ZxConsole console = {0};
    LirExecResult result = compiler_exec_LirExecPlanBuild(*program, compiler_lowered_ir_LirDefaultLimits(), &plan);
    if(result.problem == LirExecProblem_None)
        result = compiler_exec_LirExecInstanceOpen(&plan, limits, 1, &instance);
    if(result.problem == LirExecProblem_None) {
        LirExecProblem bound = ZxBindConsole(&instance, &plan.program, &console);
        if(bound != LirExecProblem_None) result.problem = bound;
    }
    if(result.problem == LirExecProblem_None) result = compiler_exec_LirExecRun(&instance);
    ZxConsoleFlush(&console);
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

static int
hex_digit(char digit)
{
    if(digit >= '0' && digit <= '9') return digit - '0';
    if(digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

int
ZxRunModuleImageParse(const char *spec, ZxRunModuleImage *image)
{
    const char *name_end = strchr(spec, '=');
    const char *digest = name_end == NULL ? NULL : name_end + 1;
    const char *digest_end = digest == NULL ? NULL : strchr(digest, '=');
    if(name_end == spec || digest_end == NULL || digest_end - digest != 64 || !digest_end[1]) return 0;
    for(size_t index = 0; index < sizeof(image->sha256); index++) {
        int high = hex_digit(digest[2 * index]), low = hex_digit(digest[2 * index + 1]);
        if(high < 0 || low < 0) return 0;
        image->sha256[index] = (unsigned char)(high * 16 + low);
    }
    image->name = spec;
    image->name_length = (size_t)(name_end - spec);
    image->path = digest_end + 1;
    return 1;
}

typedef struct ZxRunImageMap {
    const ZxRunModuleImage *images;
    size_t count;
} ZxRunImageMap;

static void
release_image(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    (void)size;
    free((void *)bytes);
}

static ZxModuleResolveResult
resolve_mapped_image(void *context, const char *path, ZxModuleImage *image)
{
    const ZxRunImageMap *map = context;
    size_t length = strlen(path);
    if(length <= 4 || path[0] != '/' || strcmp(path + length - 3, ".zx")) return ZX_MODULE_IMAGE_MISSING;
    for(size_t index = 0; index < map->count; index++) {
        const ZxRunModuleImage *pin = &map->images[index];
        if(pin->name_length != length - 4 || memcmp(pin->name, path + 1, pin->name_length)) continue;
        size_t size = 0;
        unsigned char *bytes = CommandReadBounded(pin->path, 268435456, &size);
        if(bytes == NULL) return ZX_MODULE_IMAGE_ERROR;
        unsigned char digest[32];
        Sha256 hash;
        Sha256Begin(&hash);
        Sha256Add(&hash, bytes, size);
        Sha256End(&hash, digest);
        if(memcmp(digest, pin->sha256, sizeof(digest))) {
            free(bytes);
            return ZX_MODULE_IMAGE_ERROR;
        }
        *image = (ZxModuleImage){bytes, size, release_image};
        return ZX_MODULE_IMAGE_FOUND;
    }
    return ZX_MODULE_IMAGE_MISSING;
}

int
ZxExecRunBundle(const Bundle *bundle, const char *path, const char *const *roots,
                size_t root_count, const ZxRunModuleImage *images, size_t image_count,
                const BundleInstanceOptions *options, long long *result, int *has_result)
{
    if(!ZxModuleProgramNeedsLoader(bundle)) {
        BundleInstance *instance = BundleInstantiateWithOptions(bundle, NULL, 0, options);
        int ok = instance != NULL && BundleInstanceRun(instance, result, has_result);
        BundleInstanceClose(instance);
        return ok;
    }

    char *directory = NULL;
    const char *default_root = ".";
    if(image_count != 0) {
        default_root = "/";
        roots = &default_root;
        root_count = 1;
    } else if(root_count == 0) {
        const char *slash = path == NULL ? NULL : strrchr(path, '/');
        if(slash != NULL) {
            directory = strndup(path, slash == path ? 1 : (size_t)(slash - path));
            if(directory == NULL) {
                Diagnostic((ZirSourceSpan){0}, "zx.module-run", "cannot allocate the module root path");
                return 0;
            }
            default_root = directory;
        }
        roots = &default_root;
        root_count = 1;
    }
    ZxRunImageMap map = {images, image_count};
    ZxModuleLoaderConfig config = {.roots = roots, .root_count = root_count, .options = *options};
    if(image_count != 0) {
        config.resolve = resolve_mapped_image;
        config.resolve_context = &map;
    }
    ZxModuleLoader *loader = ZxModuleLoaderOpenConfigured(&config);
    free(directory);
    ZxModuleProgram *program = loader == NULL ? NULL : ZxModuleProgramOpen(loader, bundle, NULL, 0);
    int ok = program != NULL && ZxModuleProgramRun(program, result, has_result);
    if(!ok) {
        const char *problem = ZxModuleLoaderError(loader);
        if(*problem) Diagnostic((ZirSourceSpan){0}, "zx.module-run", "%s", problem);
    }
    ZxModuleProgramClose(program);
    ZxModuleLoaderClose(loader);
    return ok;
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

static int
same_name(String name, const char *text)
{
    size_t length = strlen(text);
    return name.length == length && memcmp(name.data, text, length) == 0;
}

static int
has_module_import(const LirProgram *program, const ZxImportRoot *root)
{
    const LirFunction *functions = program->functions.data;
    const String *modules = program->modules.data;
    for(int64_t i = 0; i < program->functions.length; i++) {
        const LirFunction *function = &functions[i];
        if(function->external && function->calling == LirCalling_Host &&
           same_name(modules[function->module], root->module) &&
           same_name(function->name, root->function))
            return 1;
    }
    return 0;
}

int
ZxLowerBuild(struct ZirProgram *program, const char *module, const char *function,
             const ZxModuleBuild *link, int optimize, ZxLowerOwner *owner)
{
    memset(owner, 0, sizeof(*owner));
    if(link->export_count > 0) {
        owner->lowered = AssemblyLowerModule(program, link->exports, link->export_count);
    } else if(!portable_entry(program, module, function)) {
        Diagnostic(Span("<bundle>", 1, 1), "zx.entry",
                   "entry must be one unique zero-argument integer, bool, or void Ziran function");
        return 0;
    } else {
        owner->lowered = AssemblyLower(program, (uint8_t *)module, (uint8_t *)function);
    }
    if(owner->lowered == NULL) return 0;
    const LirProgram *view = AssemblyView(owner->lowered);
    LirDiagnostic diagnostic = compiler_lowered_verify_LirVerify(*view, compiler_lowered_ir_LirDefaultLimits());
    if(diagnostic.problem != LirProblem_None) {
        Diagnostic((ZirSourceSpan){0}, "assembly.verify",
            "lowered IR rejected: problem=%d function=%d block=%d instruction=%d value=%d origin=%d",
            diagnostic.problem, diagnostic.function, diagnostic.block, diagnostic.instruction,
            diagnostic.value, diagnostic.origin);
        ZxLowerFree(owner);
        return 0;
    }
    owner->program = *view;
    if(optimize) {
        owner->optimization = compiler_opt_LirOptBuild(&owner->optimized, *view,
            compiler_lowered_ir_LirDefaultLimits(), compiler_opt_LirOptDefaultOptions());
        if(owner->optimization.problem != LirOptProblem_None) {
            LirDiagnostic rejected = owner->optimization.diagnostic;
            Diagnostic((ZirSourceSpan){0}, "assembly.optimize",
                "optimization rejected: problem=%d verifier=%d function=%d block=%d instruction=%d value=%d origin=%d",
                owner->optimization.problem, rejected.problem, rejected.function, rejected.block,
                rejected.instruction, rejected.value, rejected.origin);
            ZxLowerFree(owner);
            return 0;
        }
        owner->program = compiler_opt_LirOptProgram(&owner->optimized);
        owner->has_optimization = 1;
    }
    for(int i = 0; i < link->import_count; i++) {
        const ZxImportRoot *root = &link->imports[i];
        if(!has_module_import(&owner->program, root)) {
            Diagnostic(Span("<command>", 1, 1), "zx.module-import",
                       "module import is not reachable: %s:%s", root->module, root->function);
            ZxLowerFree(owner);
            return 0;
        }
    }
    return 1;
}

const LirProgram *
ZxLowerView(const ZxLowerOwner *owner)
{
    return owner->lowered == NULL ? NULL : &owner->program;
}

const LirOptResult *
ZxLowerOptimization(const ZxLowerOwner *owner)
{
    return owner->has_optimization ? &owner->optimization : NULL;
}

void
ZxLowerFree(ZxLowerOwner *owner)
{
    compiler_opt_LirOptFree(&owner->optimized);
    AssemblyFree(owner->lowered);
    memset(owner, 0, sizeof(*owner));
}

unsigned char *
ZxExecBuild(struct ZirProgram *program, const char *module, const char *function,
            const ZxModuleBuild *link, Slice assets, int debug, size_t *size)
{
    unsigned char *bytes = NULL;
    *size = 0;
    ZxLowerOwner lower = {0};
    if(!ZxLowerBuild(program, module, function, link, 1, &lower)) return NULL;
    const LirProgram *optimized_program = ZxLowerView(&lower);
    size_t interface_length = 0;
    const char *interface = (const char *)AssemblyHostInterface(lower.lowered, &interface_length);
    if(interface == NULL) goto done;
    String interface_text = {interface, interface_length};
    ZxExecResult measured = compiler_exec_link_ZxExecWriteModule(*optimized_program, assets,
        (Slice){0}, interface_text, debug != 0, link->linkage);
    if(measured.problem != ZxExecProblem_None || measured.size <= 0 || measured.size > 268435456) {
        String problem = compiler_exec_link_ZxExecProblemName(measured.problem);
        Diagnostic((ZirSourceSpan){0}, "assembly.container",
            "cannot measure executable ZX output: problem=%.*s container=%d codec=%d verifier=%d",
            (int)problem.length, problem.data, measured.container, measured.codec, measured.diagnostic.problem);
        goto done;
    }
    bytes = malloc((size_t)measured.size);
    if(bytes == NULL) goto done;
    ZxExecResult encoded = compiler_exec_link_ZxExecWriteModule(*optimized_program, assets,
        (Slice){bytes, measured.size}, interface_text, debug != 0, link->linkage);
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
    ZxLowerFree(&lower);
    return bytes;
}
