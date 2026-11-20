#include "ziran_zx_module.h"
#include "zir_command.h"
#include "zir_zx_module_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DISPATCH "__zx_module_dispatch"
#define SERVICES "zx_module"
#define MODULE_IMAGE_LIMIT 268435456

typedef struct ModuleImage ModuleImage;
typedef struct ModuleScope ModuleScope;
typedef struct ModuleSlot ModuleSlot;

typedef struct ModuleExport {
    const char *name;
    size_t *parameters;
    size_t count;
    size_t result;
} ModuleExport;

typedef struct ModuleRequirement {
    const char *interface, *name;
    ModuleImage *image;
} ModuleRequirement;

typedef struct ModuleImport {
    const char *module, *function, *interface, *name;
    ModuleRequirement *require;
    size_t capability, ordinal;
} ModuleImport;

typedef enum BindingKind {
    EXTERNAL, IMPORT, SELECT, ARGUMENT, RESULT,
    LOAD, RELEASE, EXPORT_COUNT, EXPORT_NAME, PARAMETER_COUNT, PARAMETER_TYPE, RESULT_TYPE,
    REPLACE, PROBLEM
} BindingKind;

typedef struct BindingPlan {
    BindingKind kind;
    size_t ordinal, parameter;
} BindingPlan;

struct ModuleImage {
    const Bundle *bundle;
    const ExecBundle *exec;
    char *text;
    const char *name;
    ModuleExport *exports;
    ModuleRequirement *requires;
    ModuleImport *imports;
    BindingPlan *plans;
    size_t export_count, require_count, import_count, external_count;
    size_t export_capacity, require_capacity, import_capacity;
    size_t next_requirement;
    enum { UNCHECKED, VISITING, VERIFIED } mark;
    ModuleImage *next;
};

typedef struct ModuleFrame {
    size_t ordinal, next_argument;
    const HostValue *args;
    int selected, returned;
} ModuleFrame;

struct ModuleScope {
    ZxModuleLoader *loader;
    ModuleImage *image;
    BundleInstance *instance;
    struct BoundModuleHost *hosts;
    ModuleSlot *children;
    ModuleFrame *frame;
    HostValue result;
    int owns_image;
};

typedef struct BoundModuleHost {
    ModuleScope *scope;
    const BindingPlan *plan;
} BoundModuleHost;

struct ModuleSlot {
    uint64_t handle;
    ModuleScope *scope;
    ModuleSlot *next;
};

struct ZxModuleProgram {
    ModuleImage image;
    ModuleScope scope;
};

struct ZxModuleLoader {
    ModuleImage *cache;
    ModuleScope root;
    char **roots;
    size_t root_count;
    const HostBinding *bindings;
    size_t binding_count;
    BundleInstanceOptions options;
    ZxModuleResolve resolve;
    void *resolve_context;
    BundleConsoleWrite write;
    void *console_context;
    unsigned depth;
    char error[512];
};

static int
module_problem(ZxModuleLoader *loader, const char *format, ...)
{
    if(loader != NULL) {
        va_list args;
        va_start(args, format);
        vsnprintf(loader->error, sizeof(loader->error), format, args);
        va_end(args);
    }
    return 0;
}

static int
memory_problem(ZxModuleLoader *loader)
{
    return module_problem(loader, "out of memory in ZX module loader");
}

const char *
ZxModuleLoaderError(const ZxModuleLoader *loader)
{
    return loader != NULL ? loader->error : "ZX module loader unavailable";
}

static void *
module_grow(void *items, size_t *capacity, size_t size)
{
    size_t next = *capacity ? *capacity * 2 : 8;
    if(next <= *capacity || next > SIZE_MAX / size) return NULL;
    void *grown = realloc(items, next * size);
    if(grown != NULL) *capacity = next;
    return grown;
}

#define RESERVE(items, count, capacity) \
    ((count) < (capacity) || module_reserve((void *)(items), &(capacity), sizeof(*(items)), &(items)))

/* Copy a pointer through memcpy to avoid accessing a typed pointer as void **. */
static int
module_reserve(void *items, size_t *capacity, size_t size, void *destination)
{
    void *grown = module_grow(items, capacity, size);
    if(grown == NULL) return 0;
    memcpy(destination, &grown, sizeof(grown));
    return 1;
}

static int
identifier_byte(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static int
valid_name(const char *name, int logical)
{
    char separator = logical ? '/' : '.';
    if(name == NULL) return 0;
    const char *segment = name;
    for(const char *at = name; ; at++) {
        char c = *at;
        if(c == separator || c == 0) {
            size_t length = (size_t)(at - segment);
            if(length == 0 || (logical && ((length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.')))) return 0;
            if(c == 0) return 1;
            segment = at + 1;
        } else if(!identifier_byte(c) && !(logical && c == '.')) return 0;
    }
}

static void
image_clear(ModuleImage *image)
{
    for(size_t i = 0; i < image->export_count; i++) free(image->exports[i].parameters);
    free(image->exports);
    free(image->requires);
    free(image->imports);
    free(image->plans);
    free(image->text);
}

static ModuleRequirement *
find_requirement(const ModuleImage *image, const char *interface)
{
    size_t low = 0, high = image->require_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        int order = strcmp(interface, image->requires[mid].interface);
        if(order == 0) return &image->requires[mid];
        if(order < 0) high = mid;
        else low = mid + 1;
    }
    return NULL;
}

static size_t
find_export(const ModuleImage *image, const char *name)
{
    size_t low = 0, high = image->export_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        int order = strcmp(name, image->exports[mid].name);
        if(order == 0) return mid;
        if(order < 0) high = mid;
        else low = mid + 1;
    }
    return SIZE_MAX;
}

static int
read_linkage(ZxModuleLoader *loader, ModuleImage *image)
{
    size_t length = 0;
    const char *linkage = ExecBundleModuleLinkage(image->exec, &length);
    if(length == 0) return 1;
    if(memchr(linkage, 0, length) != NULL)
        return module_problem(loader, "invalid ZX module linkage: embedded NUL");
    image->text = malloc(length + 1);
    if(image->text == NULL) return memory_problem(loader);
    memcpy(image->text, linkage, length);
    image->text[length] = 0;
    unsigned group = 0;
    size_t line_number = 0;
    for(char *line = image->text; *line; line_number++) {
        char *end = strchr(line, '\n');
        if(end == NULL) goto invalid;
        *end = 0;
        char *field[5] = {line};
        size_t count = 1;
        for(char *tab = strchr(line, '\t'); tab != NULL; tab = strchr(tab + 1, '\t')) {
            if(count == 5) goto invalid;
            *tab = 0;
            field[count++] = tab + 1;
        }
        if(count == 2 && !strcmp(field[0], "module")) {
            if(line_number != 0 || !valid_name(field[1], 1)) goto invalid;
            image->name = field[1];
        } else if(count == 2 && !strcmp(field[0], "export")) {
            size_t i = image->export_count;
            if(group > 1 || image->name == NULL || !valid_name(field[1], 0) ||
               (i != 0 && strcmp(image->exports[i - 1].name, field[1]) >= 0)) goto invalid;
            if(!RESERVE(image->exports, i, image->export_capacity)) return memory_problem(loader);
            image->exports[image->export_count++] = (ModuleExport){.name = field[1], .result = SIZE_MAX};
            group = 1;
        } else if(count == 3 && !strcmp(field[0], "require")) {
            size_t i = image->require_count;
            if(group > 2 || !valid_name(field[1], 0) || !valid_name(field[2], 1) ||
               (i != 0 && strcmp(image->requires[i - 1].interface, field[1]) >= 0)) goto invalid;
            if(!RESERVE(image->requires, i, image->require_capacity)) return memory_problem(loader);
            image->requires[image->require_count++] = (ModuleRequirement){field[1], field[2], NULL};
            group = 2;
        } else if(count == 5 && !strcmp(field[0], "import")) {
            size_t i = image->import_count;
            for(size_t j = 1; j < 5; j++) if(!valid_name(field[j], 0)) goto invalid;
            ModuleRequirement *require = find_requirement(image, field[3]);
            if(require == NULL) goto invalid;
            if(i != 0) {
                ModuleImport *last = &image->imports[i - 1];
                int order = strcmp(last->module, field[1]);
                if(order > 0 || (order == 0 && strcmp(last->function, field[2]) >= 0)) goto invalid;
            }
            if(!RESERVE(image->imports, i, image->import_capacity)) return memory_problem(loader);
            image->imports[image->import_count++] = (ModuleImport){field[1], field[2], field[3], field[4], require, 0, 0};
            group = 3;
        } else goto invalid;
        line = end + 1;
    }
    return 1;
invalid:
    return module_problem(loader, "invalid ZX module linkage at line %zu", line_number + 1);
}

typedef struct ModuleService {
    const char *name;
    BindingKind kind;
    const char *types[5]; /* Result, then parameters, ending in NULL. */
} ModuleService;

static const ModuleService services[] = {
    {"HostLoad", LOAD, {"u64", "string"}},
    {"HostRelease", RELEASE, {"bool", "u64"}},
    {"HostExportCount", EXPORT_COUNT, {"s32", "u64"}},
    {"HostExportName", EXPORT_NAME, {"string", "u64", "s32"}},
    {"HostParameterCount", PARAMETER_COUNT, {"s32", "u64", "s32"}},
    {"HostParameterType", PARAMETER_TYPE, {"string", "u64", "s32", "s32"}},
    {"HostResultType", RESULT_TYPE, {"string", "u64", "s32"}},
    {"HostReplace", REPLACE, {"bool", "u64", "string"}},
    {"HostProblem", PROBLEM, {"string"}}
};

static const ModuleService *
module_service(const char *module, const char *name)
{
    if(strcmp(module, SERVICES) != 0) return NULL;
    for(size_t i = 0; i < sizeof(services) / sizeof(services[0]); i++)
        if(strcmp(services[i].name, name) == 0) return &services[i];
    return NULL;
}

static int
has_type(const ModuleImage *image, size_t cap, int parameter, const char *expected)
{
    const char *type = ExecBundleModuleType(image->exec, cap, parameter);
    return type != NULL && strcmp(type, expected) == 0;
}

static int
ordinal_field(const char **cursor, size_t *number)
{
    const char *at = *cursor;
    if(*at < '0' || *at > '9' || (*at == '0' && at[1] >= '0' && at[1] <= '9')) return 0;
    size_t n = 0;
    do {
        unsigned digit = (unsigned)(*at++ - '0');
        if(n > (SIZE_MAX - digit) / 10) return 0;
        n = n * 10 + digit;
    } while(*at >= '0' && *at <= '9');
    *cursor = at;
    *number = n;
    return 1;
}

static int
read_dispatch(ModuleImage *image, size_t cap, const char *name)
{
    if(image->name == NULL || ExecBundleModuleType(image->exec, cap, -1) == NULL) return 0;
    int arity = ExecBundleModuleParameterCount(image->exec, cap);
    if(strcmp(name, "select") == 0) {
        if(arity != 0 || !has_type(image, cap, -1, "s32")) return 0;
        image->plans[cap].kind = SELECT;
        return 1;
    }
    int argument = strncmp(name, "arg_", 4) == 0;
    if(!argument && strncmp(name, "result_", 7) != 0) return 0;
    const char *cursor = name + (argument ? 4 : 7);
    size_t ordinal, parameter = 0;
    if(!ordinal_field(&cursor, &ordinal) || ordinal >= image->export_count) return 0;
    ModuleExport *method = &image->exports[ordinal];
    if(argument) {
        if(*cursor++ != '_' || !ordinal_field(&cursor, &parameter) || *cursor || arity != 0 ||
           has_type(image, cap, -1, "void")) return 0;
        method->count++;
    } else {
        if(*cursor || method->result != SIZE_MAX || (arity != 0 && arity != 1) ||
           !has_type(image, cap, -1, "void") ||
           (arity == 1 && ExecBundleModuleType(image->exec, cap, 0) == NULL)) return 0;
        method->result = cap;
    }
    image->plans[cap] = (BindingPlan){argument ? ARGUMENT : RESULT, ordinal, parameter};
    return 1;
}

static size_t
find_capability(const Bundle *bundle, const char *module, const char *function)
{
    size_t cap = SIZE_MAX;
    for(size_t i = 0; i < BundleCapabilityCount(bundle); i++) {
        if(strcmp(module, BundleCapabilityModule(bundle, i)) || strcmp(function, BundleCapabilityFunction(bundle, i))) continue;
        if(cap != SIZE_MAX) return SIZE_MAX;
        cap = i;
    }
    return cap;
}

static int
image_read(ZxModuleLoader *loader, ModuleImage *image, const Bundle *bundle)
{
    image->bundle = bundle;
    image->exec = BundleVerifiedExec(bundle);
    if(image->exec == NULL || !read_linkage(loader, image)) return 0;
    if(ExecBundleModuleEntry(image->exec) != (image->name != NULL) ||
       (image->name != NULL && image->export_count == 0))
        return module_problem(loader, "invalid ZX module dispatcher entry or missing exports");
    size_t cap_count = BundleCapabilityCount(bundle);
    image->plans = calloc(cap_count ? cap_count : 1, sizeof(*image->plans));
    if(image->plans == NULL) return memory_problem(loader);
    size_t selectors = 0;
    unsigned services_seen = 0;
    for(size_t i = 0; i < cap_count; i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        const char *name = BundleCapabilityFunction(bundle, i);
        const ModuleService *service = module_service(module, name);
        if(strcmp(module, DISPATCH) == 0) {
            if(!read_dispatch(image, i, name))
                return module_problem(loader, "invalid ZX module dispatch ABI: %s", name);
            selectors += image->plans[i].kind == SELECT;
        } else if(service != NULL) {
            unsigned flag = 1u << (service->kind - LOAD);
            if(services_seen & flag)
                return module_problem(loader, "duplicate ZX module runtime service: %s", name);
            services_seen |= flag;
            int arity = 0;
            while(service->types[arity + 1] != NULL) arity++;
            int valid = has_type(image, i, -1, service->types[0]) &&
                arity == ExecBundleModuleParameterCount(image->exec, i);
            for(int p = 0; valid && p < arity; p++) valid = has_type(image, i, p, service->types[p + 1]);
            if(!valid) return module_problem(loader, "invalid ZX module runtime service ABI: %s", name);
            image->plans[i].kind = service->kind;
        } else image->external_count++;
    }
    if(image->name != NULL && selectors != 1)
        return module_problem(loader, "ZX module requires exactly one dispatch selector");
    for(size_t i = 0; i < image->export_count; i++) {
        ModuleExport *method = &image->exports[i];
        if(method->result == SIZE_MAX)
            return module_problem(loader, "missing ZX module dispatch result for %s", method->name);
        if(method->count == 0) continue;
        method->parameters = calloc(method->count, sizeof(*method->parameters));
        if(method->parameters == NULL) return memory_problem(loader);
        for(size_t p = 0; p < method->count; p++) method->parameters[p] = SIZE_MAX;
    }
    for(size_t i = 0; i < cap_count; i++) {
        const BindingPlan *plan = &image->plans[i];
        if(plan->kind != ARGUMENT) continue;
        ModuleExport *method = &image->exports[plan->ordinal];
        if(plan->parameter >= method->count || method->parameters[plan->parameter] != SIZE_MAX)
            return module_problem(loader, "invalid ZX module dispatch parameter sequence: %s", method->name);
        method->parameters[plan->parameter] = i;
    }
    for(size_t i = 0; i < image->import_count; i++) {
        ModuleImport *import = &image->imports[i];
        size_t cap = find_capability(bundle, import->module, import->function);
        if(cap == SIZE_MAX || image->plans[cap].kind != EXTERNAL || ExecBundleModuleType(image->exec, cap, -1) == NULL ||
           !has_type(image, cap, 0, "u64"))
            return module_problem(loader, "invalid ZX module import ABI: %s:%s (expected a leading u64 handle)",
                                  import->module, import->function);
        import->capability = cap;
        image->plans[cap] = (BindingPlan){IMPORT, i, 0};
        image->external_count--;
    }
    return 1;
}

static int
verify_imports(ZxModuleLoader *loader, ModuleImage *image)
{
    for(size_t i = 0; i < image->import_count; i++) {
        ModuleImport *import = &image->imports[i];
        ModuleImage *provider = import->require->image;
        size_t ordinal = find_export(provider, import->name);
        if(ordinal == SIZE_MAX)
            return module_problem(loader, "ZX module %s has no export %s", provider->name, import->name);
        const ModuleExport *method = &provider->exports[ordinal];
        int count = ExecBundleModuleParameterCount(image->exec, import->capability);
        if(count < 1 || (size_t)(count - 1) != method->count)
            return module_problem(loader, "ZX module import signature mismatch: %s:%s -> %s:%s (arity)",
                                  import->module, import->function, provider->name, method->name);
        for(int p = 1; p < count; p++)
            if(!ExecBundleModuleTypeEqual(image->exec, import->capability, p,
                                         provider->exec, method->parameters[p - 1], -1))
                return module_problem(loader, "ZX module import signature mismatch: %s:%s -> %s:%s (parameter %d)",
                                      import->module, import->function, provider->name, method->name, p);
        int result = ExecBundleModuleParameterCount(provider->exec, method->result) ? 0 : -1;
        if(!ExecBundleModuleTypeEqual(image->exec, import->capability, -1,
                                     provider->exec, method->result, result))
            return module_problem(loader, "ZX module import signature mismatch: %s:%s -> %s:%s (result)",
                                  import->module, import->function, provider->name, method->name);
        import->ordinal = ordinal;
    }
    return 1;
}

static char *
candidate_path(ZxModuleLoader *loader, size_t index, const char *name)
{
    const char *root = loader->roots[index];
    size_t root_length = strlen(root), name_length = strlen(name);
    char *path = NULL;
    if(name_length <= SIZE_MAX - sizeof("/.zx") && root_length <= SIZE_MAX - name_length - sizeof("/.zx")) {
        /* Keep guest paths canonical at the root join. Host roots retain
         * their existing spelling, including platform-specific // roots. */
        const char *separator = loader->resolve != NULL && root[root_length - 1] == '/' ? "" : "/";
        size_t length = root_length + name_length + sizeof("/.zx");
        path = malloc(length);
        if(path != NULL) snprintf(path, length, "%s%s%s.zx", root, separator, name);
    }
    if(path == NULL) memory_problem(loader);
    return path;
}

static char *
resolve_host_path(ZxModuleLoader *loader, const char *name)
{
    char *selected = NULL;
    struct stat selected_info = {0};
    for(size_t i = 0; i < loader->root_count; i++) {
        char *path = candidate_path(loader, i, name);
        if(path == NULL) goto fail;
        struct stat info;
        if(stat(path, &info) != 0) {
            int missing = errno == ENOENT || errno == ENOTDIR;
            if(!missing) module_problem(loader, "cannot resolve ZX module %s at %s", name, path);
            free(path);
            if(missing) continue;
            goto fail;
        }
        if(!S_ISREG(info.st_mode)) {
            module_problem(loader, "ZX module candidate is not a regular file: %s", path);
            free(path);
            goto fail;
        }
        if(selected == NULL) {
            selected = path;
            selected_info = info;
        } else {
            if(info.st_dev != selected_info.st_dev || info.st_ino != selected_info.st_ino) {
                module_problem(loader, "ambiguous ZX module %s: %s and %s", name, selected, path);
                free(path);
                goto fail;
            }
            free(path);
        }
    }
    if(selected == NULL) module_problem(loader, "ZX module not found: %s", name);
    return selected;
fail:
    free(selected);
    return NULL;
}

static Bundle *
verify_guest_image(ZxModuleLoader *loader, const ZxModuleImage *image, const char *path)
{
    Bundle *bundle = NULL;
    if(image->size <= MODULE_IMAGE_LIMIT) {
        if(image->release == NULL) bundle = BundleOpenBytes(image->data, image->size);
        else bundle = BundleOpenOwnedBytes(image->data, image->size, image->release, loader->resolve_context);
    } else if(image->release != NULL) image->release(loader->resolve_context, image->data, image->size);
    if(bundle == NULL) module_problem(loader, "cannot open verified ZX module from %s", path);
    return bundle;
}

static Bundle *
resolve_guest_bundle(ZxModuleLoader *loader, const char *name, char **selected_path)
{
    Bundle *selected = NULL;
    for(size_t i = 0; i < loader->root_count; i++) {
        char *path = candidate_path(loader, i, name);
        if(path == NULL) goto failed;
        if(*selected_path != NULL && strcmp(*selected_path, path) == 0) {
            free(path);
            continue;
        }
        ZxModuleImage image = {0};
        ZxModuleResolveResult status = loader->resolve(loader->resolve_context, path, &image);
        if(status == ZX_MODULE_IMAGE_FOUND && selected == NULL) {
            *selected_path = path;
            selected = verify_guest_image(loader, &image, path);
            if(selected == NULL) goto failed;
            continue;
        }
        if(status != ZX_MODULE_IMAGE_MISSING) {
            if(status == ZX_MODULE_IMAGE_FOUND) {
                if(image.release != NULL) image.release(loader->resolve_context, image.data, image.size);
                module_problem(loader, "ambiguous ZX module %s: %s and %s", name, *selected_path, path);
            } else {
                module_problem(loader, "cannot resolve ZX module %s at %s", name, path);
            }
            free(path);
            goto failed;
        }
        free(path);
    }
    if(selected == NULL) module_problem(loader, "ZX module not found: %s", name);
    return selected;
failed:
    BundleClose(selected);
    return NULL;
}

static void
image_close(ModuleImage *image)
{
    if(image == NULL) return;
    image_clear(image);
    BundleClose((Bundle *)image->bundle);
    free(image);
}

static const HostBinding *
library_binding(const ZxModuleLoader *loader, const Bundle *bundle, size_t capability)
{
    const char *module = BundleCapabilityModule(bundle, capability);
    const char *function = BundleCapabilityFunction(bundle, capability);
    const HostBinding *found = NULL;
    for(size_t i = 0; i < loader->binding_count; i++) {
        const HostBinding *binding = &loader->bindings[i];
        if(strcmp(module, binding->module) || strcmp(function, binding->function)) continue;
        if(found != NULL) return NULL;
        found = binding;
    }
    return found;
}

static ModuleImage *
open_image(ZxModuleLoader *loader, Bundle *bundle, const char *path, const char *name)
{
    if(bundle == NULL) return NULL;
    ModuleImage *image = calloc(1, sizeof(*image));
    if(image == NULL) {
        memory_problem(loader);
        BundleClose(bundle);
        return NULL;
    }
    if(!image_read(loader, image, bundle)) goto failed;
    if(image->name == NULL || strcmp(image->name, name) != 0) {
        module_problem(loader, "ZX module name mismatch at %s: expected %s, found %s",
                       path, name, image->name ? image->name : "an application");
        goto failed;
    }
    if(image->external_count != 0) {
        for(size_t cap = 0; cap < BundleCapabilityCount(bundle); cap++) {
            if(image->plans[cap].kind != EXTERNAL || library_binding(loader, bundle, cap) != NULL) continue;
            module_problem(loader, "ZX module %s needs one loader host binding for %s:%s", name,
                           BundleCapabilityModule(bundle, cap), BundleCapabilityFunction(bundle, cap));
            goto failed;
        }
    }
    return image;
failed:
    image_close(image);
    return NULL;
}

static Bundle *
open_path_bundle(ZxModuleLoader *loader, const char *path, const char *name)
{
    if(loader->resolve != NULL) {
        ZxModuleImage image = {0};
        ZxModuleResolveResult status = loader->resolve(loader->resolve_context, path, &image);
        if(status == ZX_MODULE_IMAGE_FOUND) return verify_guest_image(loader, &image, path);
        if(status == ZX_MODULE_IMAGE_MISSING)
            module_problem(loader, "ZX module not found: %s at %s", name, path);
        else
            module_problem(loader, "cannot resolve ZX module %s at %s", name, path);
        return NULL;
    }
    size_t size = 0;
    unsigned char *bytes = CommandReadBounded(path, MODULE_IMAGE_LIMIT, &size);
    Bundle *bundle = BundleOpenBytes(bytes, size);
    free(bytes);
    if(bundle == NULL) module_problem(loader, "cannot open verified ZX module %s from %s", name, path);
    return bundle;
}

static ModuleImage *
open_logical_image(ZxModuleLoader *loader, const char *artifact, const char *name)
{
    if(!valid_name(artifact, 1)) {
        module_problem(loader, "invalid ZX module logical name");
        return NULL;
    }
    char *path = NULL;
    Bundle *bundle;
    if(loader->resolve != NULL) {
        bundle = resolve_guest_bundle(loader, artifact, &path);
    } else {
        path = resolve_host_path(loader, artifact);
        bundle = path != NULL ? open_path_bundle(loader, path, name) : NULL;
    }
    ModuleImage *image = open_image(loader, bundle, path, name);
    free(path);
    return image;
}

static ModuleImage *
resolve_image(ZxModuleLoader *loader, const char *name)
{
    for(ModuleImage *image = loader->cache; image != NULL; image = image->next)
        if(strcmp(name, image->name) == 0) return image;
    ModuleImage *image = open_logical_image(loader, name, name);
    if(image != NULL) {
        image->next = loader->cache;
        loader->cache = image;
    }
    return image;
}

static void
rewind_cache(ZxModuleLoader *loader, ModuleImage *previous)
{
    while(loader->cache != previous) {
        ModuleImage *image = loader->cache;
        loader->cache = image->next;
        image_close(image);
    }
}

/* Walk declared edges even when an application never loads the dependency.
 * An explicit stack keeps graph verification independent of the C stack. */
static int
verify_graph(ZxModuleLoader *loader, ModuleImage *root)
{
    if(root->mark == VERIFIED) return 1;
    ModuleImage **stack = NULL;
    size_t count = 0, capacity = 0;
    if(!RESERVE(stack, count, capacity)) return memory_problem(loader);
    stack[count++] = root;
    root->mark = VISITING;
    while(count != 0) {
        ModuleImage *image = stack[count - 1];
        if(image->next_requirement == image->require_count) {
            if(!verify_imports(loader, image)) break;
            image->mark = VERIFIED;
            count--;
            continue;
        }
        ModuleRequirement *edge = &image->requires[image->next_requirement++];
        ModuleImage *dependency = resolve_image(loader, edge->name);
        if(dependency == NULL) break;
        if(dependency->mark == VISITING) {
            module_problem(loader, "ZX module dependency cycle: %s -> %s",
                           image->name ? image->name : "<application>", edge->name);
            break;
        }
        edge->image = dependency;
        if(dependency->mark == VERIFIED) continue;
        if(!RESERVE(stack, count, capacity)) {
            memory_problem(loader);
            break;
        }
        stack[count++] = dependency;
        dependency->mark = VISITING;
    }
    int valid = count == 0;
    while(count != 0) {
        ModuleImage *image = stack[--count];
        image->mark = UNCHECKED;
        image->next_requirement = 0;
    }
    free(stack);
    return valid;
}

static int
bind_call(void *context, const char *module, const char *function,
          const HostValue *args, int count, HostValue *result);

static int
scope_open(ModuleScope *scope, const HostBinding *external, size_t external_count)
{
    const ModuleImage *image = scope->image;
    size_t count = BundleCapabilityCount(image->bundle);
    if(external_count != 0 && external == NULL)
        return module_problem(scope->loader, "missing ZX application host binding array");
    if(count > SIZE_MAX - external_count) return memory_problem(scope->loader);
    size_t capacity = count + external_count;
    HostBinding *bindings = calloc(capacity ? capacity : 1, sizeof(*bindings));
    scope->hosts = count ? calloc(count, sizeof(*scope->hosts)) : NULL;
    if(bindings == NULL || (count != 0 && scope->hosts == NULL)) {
        free(bindings);
        return memory_problem(scope->loader);
    }
    if(external_count != 0) memcpy(bindings, external, external_count * sizeof(*external));
    size_t used = external_count;
    for(size_t i = 0; i < count; i++) {
        const BindingPlan *plan = &image->plans[i];
        if(plan->kind == EXTERNAL) {
            if(image->name != NULL) bindings[used++] = *library_binding(scope->loader, image->bundle, i);
            continue;
        }
        scope->hosts[i] = (BoundModuleHost){scope, plan};
        bindings[used++] = (HostBinding){BundleCapabilityModule(image->bundle, i),
            BundleCapabilityFunction(image->bundle, i), bind_call, &scope->hosts[i]};
    }
    ZxModuleLoader *loader = scope->loader;
    scope->instance = BundleInstantiateWithOptionsAndConsole(image->bundle, bindings, used, &loader->options,
                                                             loader->write, loader->console_context);
    free(bindings);
    if(scope->instance == NULL && *ZxModuleLoaderError(scope->loader) == 0)
        module_problem(scope->loader, "ZX module initialization failed: %s",
                       image->name ? image->name : BundleEntryModuleName(image->bundle));
    return scope->instance != NULL;
}

static void slot_close(ModuleSlot *slot);

static void
scope_close(ModuleScope *scope)
{
    BundleInstanceClose(scope->instance);
    BundleFreeHostValue(&scope->result);
    free(scope->hosts);
    while(scope->children != NULL) {
        ModuleSlot *slot = scope->children;
        scope->children = slot->next;
        slot_close(slot);
    }
}

static void
slot_close(ModuleSlot *slot)
{
    ModuleScope *scope = slot->scope;
    scope_close(scope);
    if(scope->owns_image) image_close(scope->image);
    free(scope);
    free(slot);
}

static ModuleSlot *
find_slot(const ModuleScope *owner, uint64_t handle)
{
    if(handle != 0) for(ModuleSlot *slot = owner->children; slot != NULL; slot = slot->next)
        if(slot->handle == handle) return slot;
    return NULL;
}

static int
release_slot(ModuleScope *owner, uint64_t handle)
{
    ModuleSlot **at = &owner->children;
    while(*at != NULL && (*at)->handle != handle) at = &(*at)->next;
    if(*at == NULL || (*at)->scope->frame != NULL)
        return module_problem(owner->loader, "invalid, released, or active ZX module handle");
    ModuleSlot *slot = *at;
    *at = slot->next;
    slot_close(slot);
    return 1;
}

static uint64_t
next_handle(void)
{
    static atomic_flag lock = ATOMIC_FLAG_INIT;
    static uint64_t issued;
    while(atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {}
    uint64_t handle = issued == UINT64_MAX ? 0 : ++issued;
    atomic_flag_clear_explicit(&lock, memory_order_release);
    return handle;
}

static uint64_t
load_slot(ModuleScope *owner, ModuleImage *image)
{
    if(image == NULL) return 0;
    ZxModuleLoader *loader = owner->loader;
    if(loader->depth >= 64) {
        module_problem(loader, "ZX module call or initialization depth exceeded");
        return 0;
    }
    ModuleSlot *slot = calloc(1, sizeof(*slot));
    ModuleScope *scope = calloc(1, sizeof(*scope));
    if(slot == NULL || scope == NULL) {
        free(scope);
        free(slot);
        memory_problem(loader);
        return 0;
    }
    *scope = (ModuleScope){.loader = loader, .image = image};
    *slot = (ModuleSlot){.handle = next_handle(), .scope = scope};
    if(slot->handle == 0) {
        module_problem(loader, "ZX module handle space exhausted");
        slot_close(slot);
        return 0;
    }
    loader->depth++;
    int ok = scope_open(scope, NULL, 0);
    loader->depth--;
    if(!ok) {
        slot_close(slot);
        return 0;
    }
    slot->next = owner->children;
    owner->children = slot;
    return slot->handle;
}

static int
call_slot(ModuleSlot *slot, size_t ordinal, const HostValue *args, size_t count, HostValue *result)
{
    ModuleScope *scope = slot->scope;
    ZxModuleLoader *loader = scope->loader;
    if(ordinal >= scope->image->export_count || count != scope->image->exports[ordinal].count ||
       (count != 0 && args == NULL) || result == NULL)
        return module_problem(loader, "invalid ZX module export ordinal or argument count");
    if(scope->frame != NULL || loader->depth >= 64)
        return module_problem(loader, "ZX module reentry or call depth exceeded");
    ModuleFrame frame = {.ordinal = ordinal, .args = args};
    /* A caller may pass the previous result back to the same instance. */
    HostValue previous = scope->result;
    scope->result = (HostValue){0};
    scope->frame = &frame;
    loader->depth++;
    long long ignored;
    int has_result;
    int ok = BundleInstanceRun(scope->instance, &ignored, &has_result);
    loader->depth--;
    scope->frame = NULL;
    BundleFreeHostValue(&previous);
    if(!ok || has_result || !frame.returned) {
        if(*ZxModuleLoaderError(loader) == 0)
            module_problem(loader, "ZX module call failed: %s:%s", scope->image->name,
                           scope->image->exports[ordinal].name);
        return 0;
    }
    *result = scope->result;
    return 1;
}

static const ModuleExport *
export_at(const ModuleSlot *slot, size_t ordinal)
{
    return slot != NULL && ordinal < slot->scope->image->export_count ?
        &slot->scope->image->exports[ordinal] : NULL;
}

static const char *
parameter_type(const ModuleSlot *slot, const ModuleExport *method, size_t index)
{
    return method != NULL && index < method->count ?
        ExecBundleModuleType(slot->scope->image->exec, method->parameters[index], -1) : NULL;
}

static const char *
result_type(const ModuleImage *image, const ModuleExport *method)
{
    int param = ExecBundleModuleParameterCount(image->exec, method->result) ? 0 : -1;
    return ExecBundleModuleType(image->exec, method->result, param);
}

static int
same_linkage(const ModuleImage *a, const ModuleImage *b)
{
    if(a->require_count != b->require_count || a->import_count != b->import_count ||
       a->export_count != b->export_count) return 0;
    for(size_t i = 0; i < a->require_count; i++)
        if(strcmp(a->requires[i].interface, b->requires[i].interface) ||
           strcmp(a->requires[i].name, b->requires[i].name)) return 0;
    for(size_t i = 0; i < a->import_count; i++) {
        const ModuleImport *left = &a->imports[i], *right = &b->imports[i];
        if(strcmp(left->module, right->module) || strcmp(left->function, right->function) ||
           strcmp(left->interface, right->interface) || strcmp(left->name, right->name)) return 0;
    }
    for(size_t i = 0; i < a->export_count; i++) {
        const ModuleExport *left = &a->exports[i], *right = &b->exports[i];
        if(strcmp(left->name, right->name) || left->count != right->count) return 0;
        for(size_t p = 0; p < left->count; p++)
            if(!ExecBundleModuleTypeEqual(a->exec, left->parameters[p], -1,
                                         b->exec, right->parameters[p], -1)) return 0;
        int l = ExecBundleModuleParameterCount(a->exec, left->result) ? 0 : -1;
        int r = ExecBundleModuleParameterCount(b->exec, right->result) ? 0 : -1;
        if(!ExecBundleModuleTypeEqual(a->exec, left->result, l, b->exec, right->result, r)) return 0;
    }
    return 1;
}

static int
replace_slot(ModuleSlot *slot, const char *artifact, int search_roots, BundleStateMigrate migrate, void *context)
{
    ModuleScope *old = slot->scope;
    ZxModuleLoader *loader = old->loader;
    if(artifact == NULL || *artifact == 0)
        return module_problem(loader, "ZX module replacement requires an artifact path");
    if(old->frame != NULL || old->children != NULL || loader->depth >= 64)
        return module_problem(loader, "ZX module replacement requires an idle instance without live child handles");
    const char *name = old->image->name;
    ModuleImage *image = search_roots ? open_logical_image(loader, artifact, name) :
        open_image(loader, open_path_bundle(loader, artifact, name), artifact, name);
    if(image == NULL) return 0;
    if(!same_linkage(old->image, image) ||
       ((old->image->external_count != 0 || image->external_count != 0) &&
        !BundleAbiCompatible(old->image->bundle, image->bundle))) {
        image_close(image);
        return module_problem(loader, "ZX module replacement has incompatible linkage or export types");
    }
    ModuleImage *previous = loader->cache;
    if(!verify_graph(loader, image)) {
        rewind_cache(loader, previous);
        image_close(image);
        return 0;
    }
    ModuleScope *candidate = calloc(1, sizeof(*candidate));
    if(candidate == NULL) {
        image_close(image);
        return memory_problem(loader);
    }
    *candidate = (ModuleScope){.loader = loader, .image = image, .owns_image = 1};
    BundleState *before = NULL, *after = NULL;
    const char *failure = NULL;
    loader->depth++;
    if(!BundleInstanceExportState(old->instance, &before))
        failure = "live state is not portable";
    else if(!scope_open(candidate, NULL, 0))
        failure = "candidate initialization failed";
    else if(candidate->children != NULL)
        failure = "candidate initialization left live child handles";
    else if(migrate == NULL) {
        if(!BundleInstanceImportState(candidate->instance, before))
            failure = "existing state has no compatible replacement storage";
    } else if(!BundleInstanceExportState(candidate->instance, &after) ||
              !migrate(context, before, after) || !BundleInstanceImportState(candidate->instance, after)) {
        failure = "state migration failed";
    }
    loader->depth--;
    BundleStateClose(before);
    BundleStateClose(after);
    if(failure != NULL) {
        scope_close(candidate);
        image_close(image);
        free(candidate);
        return module_problem(loader, "ZX module replacement refused: %s", failure);
    }
    slot->scope = candidate;
    scope_close(old);
    if(old->owns_image) image_close(old->image);
    free(old);
    return 1;
}

static void
set_number(HostValue *result, uint64_t number)
{
    result->kind = strcmp(result->type, "u64") == 0 ? HOST_UNSIGNED : HOST_INTEGER;
    result->bits = number;
    result->integer = (int64_t)number;
}

static void
set_text(HostValue *result, const char *text)
{
    result->kind = HOST_STRING;
    result->data = (const unsigned char *)text;
    result->length = text != NULL ? strlen(text) : 0;
}

static char *
read_text(const HostValue *value)
{
    if(value->length == 0 || value->length == SIZE_MAX || memchr(value->data, 0, value->length)) return NULL;
    char *text = malloc(value->length + 1);
    if(text != NULL) {
        memcpy(text, value->data, value->length);
        text[value->length] = 0;
    }
    return text;
}

static int
service_call(ModuleScope *scope, BindingKind kind, const HostValue *args, HostValue *result)
{
    ZxModuleLoader *loader = scope->loader;
    if(kind == PROBLEM) {
        set_text(result, loader->error);
        return 1;
    }
    if(kind == LOAD) {
        char *name = read_text(args);
        const ModuleRequirement *require = name != NULL ? find_requirement(scope->image, name) : NULL;
        if(require == NULL) module_problem(loader, "undeclared ZX module interface: %s", name ? name : "<invalid>");
        uint64_t handle = require != NULL ? load_slot(scope, require->image) : 0;
        free(name);
        set_number(result, handle);
        return 1;
    }
    uint64_t handle = args[0].bits;
    if(kind == RELEASE) {
        set_number(result, release_slot(scope, handle));
        return 1;
    }
    ModuleSlot *slot = find_slot(scope, handle);
    if(slot == NULL) module_problem(loader, "invalid or released ZX module handle");
    if(kind == REPLACE) {
        char *name = read_text(&args[1]);
        int ok = slot != NULL && replace_slot(slot, name, 1, NULL, NULL);
        set_number(result, ok);
        free(name);
    } else if(kind == EXPORT_COUNT) {
        set_number(result, slot != NULL ? slot->scope->image->export_count : 0);
    } else {
        const ModuleExport *method = export_at(slot, (size_t)args[1].integer);
        if(kind == EXPORT_NAME) set_text(result, method != NULL ? method->name : NULL);
        if(kind == PARAMETER_COUNT) set_number(result, method != NULL ? method->count : 0);
        if(kind == PARAMETER_TYPE) set_text(result, parameter_type(slot, method, (size_t)args[2].integer));
        if(kind == RESULT_TYPE) set_text(result, method != NULL ? result_type(slot->scope->image, method) : NULL);
    }
    return 1;
}

static int
bind_call(void *context, const char *module, const char *function,
          const HostValue *args, int count, HostValue *result)
{
    const BoundModuleHost *host = context;
    ModuleScope *scope = host->scope;
    const BindingPlan *plan = host->plan;
    if(plan->kind == IMPORT) {
        const ModuleImport *import = &scope->image->imports[plan->ordinal];
        ModuleSlot *slot = find_slot(scope, args[0].bits);
        if(slot == NULL || strcmp(slot->scope->image->name, import->require->name) != 0)
            return module_problem(scope->loader, "invalid ZX module handle for %s:%s (expected %s)",
                                  module, function, import->require->name);
        return call_slot(slot, import->ordinal, args + 1, (size_t)count - 1, result);
    }
    if(plan->kind >= LOAD) return service_call(scope, plan->kind, args, result);
    ModuleFrame *frame = scope->frame;
    if(frame != NULL && !frame->returned) {
        if(plan->kind == SELECT && !frame->selected) {
            frame->selected = 1;
            set_number(result, frame->ordinal);
            return 1;
        }
        if(frame->selected && frame->ordinal == plan->ordinal) {
            if(plan->kind == ARGUMENT && frame->next_argument == plan->parameter) {
                *result = frame->args[frame->next_argument++];
                return 1;
            }
            if(plan->kind == RESULT && frame->next_argument == scope->image->exports[frame->ordinal].count) {
                HostValue empty = {.kind = HOST_VOID, .type = "void"};
                frame->returned = BundleCopyHostValue(count == 0 ? &empty : args, &scope->result);
                return frame->returned || module_problem(scope->loader, "cannot copy ZX module result");
            }
        }
    }
    return module_problem(scope->loader, "ZX module called dispatch service out of sequence: %s", function);
}

ZxModuleLoader *
ZxModuleLoaderOpen(const char *const *roots, size_t count, const BundleInstanceOptions *options)
{
    return ZxModuleLoaderOpenWithBindings(roots, count, options, NULL, 0);
}

ZxModuleLoader *
ZxModuleLoaderOpenWithBindings(const char *const *roots, size_t count, const BundleInstanceOptions *options,
                               const HostBinding *bindings, size_t binding_count)
{
    const ZxModuleLoaderConfig config = {
        .roots = roots, .root_count = count,
        .options = options != NULL ? *options : (BundleInstanceOptions){0},
        .library_bindings = bindings, .library_binding_count = binding_count,
    };
    return ZxModuleLoaderOpenConfigured(&config);
}

ZxModuleLoader *
ZxModuleLoaderOpenConfigured(const ZxModuleLoaderConfig *config)
{
    if(config == NULL) return NULL;
    size_t count = config->root_count, binding_count = config->library_binding_count;
    const char *const *roots = config->roots;
    const HostBinding *bindings = config->library_bindings;
    if((count != 0 && roots == NULL) || (binding_count != 0 && bindings == NULL)) return NULL;
    for(size_t i = 0; i < binding_count; i++)
        if(bindings[i].module == NULL || bindings[i].function == NULL || bindings[i].call == NULL) return NULL;
    ZxModuleLoader *loader = calloc(1, sizeof(*loader));
    if(loader == NULL) return NULL;
    loader->root.loader = loader;
    loader->bindings = bindings;
    loader->binding_count = binding_count;
    loader->options = config->options;
    loader->resolve = config->resolve;
    loader->resolve_context = config->resolve_context;
    loader->write = config->write;
    loader->console_context = config->console_context;
    loader->roots = count != 0 ? calloc(count, sizeof(*loader->roots)) : NULL;
    if(count != 0 && loader->roots == NULL) goto failed;
    while(loader->root_count < count) {
        size_t i = loader->root_count;
        if(roots[i] == NULL || *roots[i] == 0 || (loader->roots[i] = strdup(roots[i])) == NULL) goto failed;
        loader->root_count++;
    }
    return loader;
failed:
    ZxModuleLoaderClose(loader);
    return NULL;
}

void
ZxModuleLoaderClose(ZxModuleLoader *loader)
{
    if(loader == NULL) return;
    scope_close(&loader->root);
    rewind_cache(loader, NULL);
    for(size_t i = 0; i < loader->root_count; i++) free(loader->roots[i]);
    free(loader->roots);
    free(loader);
}

int
ZxModuleProgramNeedsLoader(const Bundle *bundle)
{
    const ExecBundle *exec = BundleVerifiedExec(bundle);
    if(exec == NULL) return 0;
    size_t length;
    ExecBundleModuleLinkage(exec, &length);
    if(length != 0 || ExecBundleModuleEntry(exec)) return 1;
    for(size_t i = 0; i < BundleCapabilityCount(bundle); i++) {
        const char *module = BundleCapabilityModule(bundle, i);
        if(strcmp(module, DISPATCH) == 0 || module_service(module, BundleCapabilityFunction(bundle, i)) != NULL)
            return 1;
    }
    return 0;
}

int
ZxModuleProgramHasExternalHosts(const Bundle *bundle)
{
    if(!ZxModuleProgramNeedsLoader(bundle)) return BundleCapabilityCount(bundle) != 0;
    ModuleImage image = {0};
    /* Invalid linkage goes to ProgramOpen for its specific diagnostic. */
    int external = image_read(NULL, &image, bundle) && image.external_count != 0;
    image_clear(&image);
    return external;
}

ZxModuleProgram *
ZxModuleProgramOpen(ZxModuleLoader *loader, const Bundle *bundle, const HostBinding *bindings, size_t count)
{
    if(loader == NULL || bundle == NULL) return NULL;
    *loader->error = 0;
    ZxModuleProgram *program = calloc(1, sizeof(*program));
    if(program == NULL) {
        memory_problem(loader);
        return NULL;
    }
    program->scope = (ModuleScope){.loader = loader, .image = &program->image};
    if(!image_read(loader, &program->image, bundle)) goto failed;
    if(program->image.name != NULL) {
        module_problem(loader, "cannot run ZX library %s as an application", program->image.name);
        goto failed;
    }
    ModuleImage *previous = loader->cache;
    if(!verify_graph(loader, &program->image)) {
        rewind_cache(loader, previous);
        goto failed;
    }
    if(!scope_open(&program->scope, bindings, count)) goto failed;
    return program;
failed:
    ZxModuleProgramClose(program);
    return NULL;
}

int
ZxModuleProgramRun(ZxModuleProgram *program, long long *result, int *has_result)
{
    if(program == NULL) return 0;
    ZxModuleLoader *loader = program->scope.loader;
    *loader->error = 0;
    long long ignored_result;
    int ignored_has_result;
    return BundleInstanceRun(program->scope.instance, result ? result : &ignored_result,
                             has_result ? has_result : &ignored_has_result);
}

int
ZxModuleProgramBorrowHostString(ZxModuleProgram *program, const char *module,
                               const char *function, size_t parameter)
{
    return program != NULL &&
           BundleInstanceBorrowHostString(program->scope.instance, module, function, parameter);
}

BundleExecutionBackend
ZxModuleProgramBackend(const ZxModuleProgram *program)
{
    return program != NULL ? BundleInstanceBackend(program->scope.instance) : BUNDLE_EXECUTION_NONE;
}

void
ZxModuleProgramClose(ZxModuleProgram *program)
{
    if(program == NULL) return;
    scope_close(&program->scope);
    image_clear(&program->image);
    free(program);
}

uint64_t
ZxModuleLoad(ZxModuleLoader *loader, const char *name)
{
    if(loader == NULL) return 0;
    *loader->error = 0;
    if(!valid_name(name, 1)) return module_problem(loader, "invalid ZX module logical name");
    ModuleImage *previous = loader->cache;
    ModuleImage *image = resolve_image(loader, name);
    if(image == NULL || !verify_graph(loader, image)) {
        rewind_cache(loader, previous);
        return 0;
    }
    return load_slot(&loader->root, image);
}

int
ZxModuleRelease(ZxModuleLoader *loader, uint64_t handle)
{
    if(loader == NULL) return 0;
    *loader->error = 0;
    return release_slot(&loader->root, handle);
}

size_t
ZxModuleExportCount(const ZxModuleLoader *loader, uint64_t handle)
{
    const ModuleSlot *slot = loader != NULL ? find_slot(&loader->root, handle) : NULL;
    return slot != NULL ? slot->scope->image->export_count : 0;
}

const char *
ZxModuleExportName(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal)
{
    const ModuleExport *method = export_at(loader != NULL ? find_slot(&loader->root, handle) : NULL, ordinal);
    return method != NULL ? method->name : NULL;
}

size_t
ZxModuleParameterCount(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal)
{
    const ModuleExport *method = export_at(loader != NULL ? find_slot(&loader->root, handle) : NULL, ordinal);
    return method != NULL ? method->count : 0;
}

const char *
ZxModuleParameterType(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal, size_t index)
{
    const ModuleSlot *slot = loader != NULL ? find_slot(&loader->root, handle) : NULL;
    return parameter_type(slot, export_at(slot, ordinal), index);
}

const char *
ZxModuleResultType(const ZxModuleLoader *loader, uint64_t handle, size_t ordinal)
{
    const ModuleSlot *slot = loader != NULL ? find_slot(&loader->root, handle) : NULL;
    const ModuleExport *method = export_at(slot, ordinal);
    return method != NULL ? result_type(slot->scope->image, method) : NULL;
}

int
ZxModuleCall(ZxModuleLoader *loader, uint64_t handle, size_t ordinal, const HostValue *args,
             size_t count, HostValue *result)
{
    if(loader == NULL) return 0;
    *loader->error = 0;
    ModuleSlot *slot = find_slot(&loader->root, handle);
    return slot != NULL ? call_slot(slot, ordinal, args, count, result) :
        module_problem(loader, "invalid or released ZX module handle");
}

int
ZxModuleReplace(ZxModuleLoader *loader, uint64_t handle, const char *path,
                BundleStateMigrate migrate, void *context)
{
    if(loader == NULL) return 0;
    *loader->error = 0;
    ModuleSlot *slot = find_slot(&loader->root, handle);
    return slot != NULL ? replace_slot(slot, path, 0, migrate, context) :
        module_problem(loader, "invalid or released ZX module handle");
}
