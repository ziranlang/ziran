#include "zir_memory.h"
static void free_scratch_SameFileBytesBuffers(void *pointer);
#include "zir.h"
#include "zir_string.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_text.h"
#include "compiler_type.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int
TargetNameKnown(const char *target)
{
    return target != NULL && compiler_type_TargetKnown(StringView(target, strlen(target)));
}

/* The maintained Ziran type module owns the target policy. This boundary
 * walks checked declarations and attaches their source locations. */
int
CheckTargetCapabilities(const ZirProgram *program, const char *target)
{
    if(program == NULL || !TargetNameKnown(target)) return 0;
    String selected = StringView(target, strlen(target));
    for(int m = 0; m < program->module_count; m++) {
        const ZirModule *module = &program->modules[m];
        for(int t = 0; t < module->type_count; t++) {
            const ZirType *type = &module->types[t];
            if(type->is_procedure_type &&
               !compiler_type_ProcedureTypeSupported(selected, type->is_c_call)) {
                DiagnosticTarget(type->span, "check.record", target, "procedures.c-call",
                    "target does not support #c_call procedure types: %s", type->name);
                return 0;
            }
            if(type->is_union && !type->is_record_template &&
               (!strcmp(target, "zx") || !strcmp(target, "go")) &&
               !UnionScalarFields(module, type)) {
                if(!strcmp(target, "zx"))
                    DiagnosticTarget(type->span, "zx.union", target, "unions.scalar",
                        "portable unions support scalar fields only: %s", type->name);
                else
                    DiagnosticTarget(type->span, "zir_go.union", target, "unions.scalar",
                        "Go unions support scalar fields only: %s", type->name);
                return 0;
            }
            String required = compiler_type_ForeignTypeTarget(
                StringView(type->foreign_target, strlen(type->foreign_target)), type->is_map);
            if(required.length != 0 && !StringEqual(required, selected)) {
                DiagnosticDetails details = {0};
                details.target = target;
                details.capability = type->is_map ? "maps.go" : "types.go";
                DiagnosticDetailed(type->span, "check.record", &details,
                           "%s require the Go target: %s",
                           type->is_map ? "maps" : "foreign Go types", type->name);
                return 0;
            }
        }
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *import = &module->imports[i];
            if(import->kind != ZIR_IMPORT_EXTERN) continue;
            const char *kind = import->extern_kind == ZIR_EXTERN_GO ? "go" :
                import->extern_kind == ZIR_EXTERN_HOST ? "host" : "c";
            if(!compiler_type_ForeignImportSupported(selected, StringView(kind, strlen(kind)))) {
                DiagnosticDetails details = {0};
                details.target = target;
                details.capability = import->extern_kind == ZIR_EXTERN_GO ? "ffi.go" : "ffi.c";
                if(import->extern_kind == ZIR_EXTERN_C)
                    DiagnosticDetailed(import->span, "check.foreign", &details,
                        "C foreign imports require a native target or a checked host binding: %s",
                        import->name);
                else
                    DiagnosticDetailed(import->span, "check.foreign", &details,
                               "Go foreign imports require the Go target: %s", import->name);
                return 0;
            }
        }
    }
    return 1;
}

int
MapTypeParts(const ZirModule *module, const char *name,
              char *key, size_t key_size, char *value, size_t value_size)
{
    const ZirType *type = module ? FindType(module, name, NULL) : NULL;
    if(type == NULL || !type->is_map || type->is_record_template ||
       type->is_extern || type->is_enum || type->is_procedure_type || type->is_union)
        return 0;
    ZirTypeField k, v, end;
    size_t offset = 0;
    if(TypeNextField(type, &offset, &k) != 1 || strcmp(k.name, "key") ||
       TypeNextField(type, &offset, &v) != 1 || strcmp(v.name, "value") ||
       TypeNextField(type, &offset, &end) != 0 || k.is_using || v.is_using ||
       k.go_tag[0] || v.go_tag[0] || !k.type[0] || !v.type[0])
        return 0;
    if(key != NULL) {
        if(strlen(k.type) >= key_size) return 0;
        strcpy(key, k.type);
    }
    if(value != NULL) {
        if(strlen(v.type) >= value_size) return 0;
        strcpy(value, v.type);
    }
    return 1;
}

int
MapKeyComparable(const ZirModule *module, const char *type, int depth)
{
    if(depth > 32 || !strcmp(type, "void")) return 0;
    if(type[0] == '*') return 1;
    if(*ScalarType(type)) return strcmp(type, "void") != 0;
    char element[ZIR_NAME_MAX];
    if(ArrayElementType(type, element, sizeof(element), NULL))
        return MapKeyComparable(module, element, depth + 1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL || record->is_map || record->is_owned_vec ||
       record->is_extern || record->is_procedure_type || record->is_record_template ||
       record->is_union)
        return 0;
    if(record->is_enum) return 1;
    size_t offset = 0;
    ZirTypeField field;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(!MapKeyComparable(owner ? owner : module, field.type, depth + 1))
            return 0;
    return status == 0;
}

static int
same_map_argument(const ZirModule *a_owner, const char *a_name,
                   const ZirModule *b_owner, const char *b_name, int depth)
{
    if(depth > 32) return 0;
    const char *a_scalar = ScalarType(a_name), *b_scalar = ScalarType(b_name);
    if(*a_scalar || *b_scalar) return !strcmp(a_scalar, b_scalar);
    char a_element[ZIR_NAME_MAX], b_element[ZIR_NAME_MAX];
    int a_count, b_count;
    if(SliceElementType(a_name, a_element, sizeof(a_element)) &&
       SliceElementType(b_name, b_element, sizeof(b_element)))
        return same_map_argument(a_owner, a_element, b_owner, b_element, depth + 1);
    if(ArrayElementType(a_name, a_element, sizeof(a_element), &a_count) &&
       ArrayElementType(b_name, b_element, sizeof(b_element), &b_count))
        return a_count >= 0 && a_count == b_count &&
            same_map_argument(a_owner, a_element, b_owner, b_element, depth + 1);
    if(a_name[0] == '*' && b_name[0] == '*')
        return same_map_argument(a_owner, a_name + 1, b_owner, b_name + 1, depth + 1);
    const ZirModule *a_scope = NULL, *b_scope = NULL;
    const ZirType *a = FindType(a_owner, a_name, &a_scope);
    const ZirType *b = FindType(b_owner, b_name, &b_scope);
    if(a == NULL || b == NULL) return 0;
    if(a == b) return 1;
    if(a->foreign_target[0] && !strcmp(a->foreign_target, b->foreign_target))
        return 1;
    if(a->is_map && b->is_map) {
        char ak[ZIR_NAME_MAX], av[ZIR_NAME_MAX], bk[ZIR_NAME_MAX], bv[ZIR_NAME_MAX];
        return MapTypeParts(a_owner, a_name, ak, sizeof(ak), av, sizeof(av)) &&
            MapTypeParts(b_owner, b_name, bk, sizeof(bk), bv, sizeof(bv)) &&
            same_map_argument(a_scope ? a_scope : a_owner, ak,
                              b_scope ? b_scope : b_owner, bk, depth + 1) &&
            same_map_argument(a_scope ? a_scope : a_owner, av,
                              b_scope ? b_scope : b_owner, bv, depth + 1);
    }
    return same_type_application(a_scope, a, b_scope, b);
}

int
SameMapType(const ZirModule *a_owner, const ZirType *a,
              const ZirModule *b_owner, const ZirType *b)
{
    return a != NULL && b != NULL && a->is_map && b->is_map &&
        same_map_argument(a_owner, a->name, b_owner, b->name, 0);
}

int
VecElementType(const ZirModule *module, const char *name,
               char *element, size_t element_size)
{
    if(module == NULL || name == NULL)
        return 0;
    const ZirType *type = FindType(module, name, NULL);
    if(type == NULL || !type->is_owned_vec || type->is_enum || type->is_procedure_type ||
       type->is_record_template || type->is_extern)
        return 0;
    size_t offset = 0;
    ZirTypeField field;
    if(TypeNextField(type, &offset, &field) != 1 ||
       strcmp(field.name, "data") || field.type[0] != '*' ||
       !field.type[1])
        return 0;
    char item[ZIR_NAME_MAX];
    strcpy(item, field.type + 1);
    if(TypeNextField(type, &offset, &field) != 1 ||
       strcmp(field.name, "count") || strcmp(field.type, "s64"))
        return 0;
    if(TypeNextField(type, &offset, &field) != 1 ||
       strcmp(field.name, "capacity") || strcmp(field.type, "s64"))
        return 0;
    if(TypeNextField(type, &offset, &field) != 0)
        return 0;
    if(element != NULL) {
        if(strlen(item) >= element_size) return 0;
        strcpy(element, item);
    }
    return 1;
}

static int
file_scope_visible(const ZirModule *module, int is_file_private,
                   ZirSourceSpan span)
{
    return !is_file_private || module->lookup_path[0] == '\0' ||
           strcmp(module->lookup_path, SpanPath(span)) == 0;
}

static int
file_scope_visible_at(const char *source_path, int is_file_private,
                      ZirSourceSpan span)
{
    return !is_file_private || source_path == NULL || !source_path[0] ||
           strcmp(source_path, SpanPath(span)) == 0;
}

/* Resolution follows an import graph. Cache each scope within one lookup,
 * including misses and ambiguous results, rather than rewalking shared
 * dependencies for every path. The entry scope exposes ordinary imports;
 * a nested scope exposes only imports marked using. */
typedef struct SymbolVisit {
    const ZirModule *module;
    const char *name;
    const char *source_path;
    int entry_scope;
    int status;
    const ZirModule *owner;
    const void *symbol;
    struct SymbolVisit *next;
} SymbolVisit;

static SymbolVisit *
symbol_visit(SymbolVisit **visits, const ZirModule *module, const char *name,
             const char *source_path, int depth, int *created)
{
    const char *path = source_path == NULL ? "" : source_path;
    for(SymbolVisit *visit = *visits; visit != NULL; visit = visit->next)
        if(visit->module == module && visit->entry_scope == (depth == 0) &&
           strcmp(visit->name, name) == 0 &&
           strcmp(visit->source_path, path) == 0) {
            *created = 0;
            return visit;
        }
    SymbolVisit *visit = AllocateOrExit(sizeof(*visit));
    *visit = (SymbolVisit){.module = module, .name = name,
        .source_path = path, .entry_scope = depth == 0, .next = *visits};
    *visits = visit;
    *created = 1;
    return visit;
}

static void
free_symbol_visits(SymbolVisit *visits)
{
    while(visits != NULL) {
        SymbolVisit *next = visits->next;
        free(visits);
        visits = next;
    }
}

static int resolve_function_depth(const ZirModule *module, const char *name,
                       const char *source_path, const ZirModule **owner,
                       const ZirFunction **function, int depth, SymbolVisit **visits);

static int
resolve_function_uncached(const ZirModule *module, const char *name,
                       const char *source_path, const ZirModule **owner,
                       const ZirFunction **function, int depth, SymbolVisit **visits)
{
    *owner = NULL;
    *function = NULL;
    const char *dot = strchr(name, '.');
    if(dot != NULL) {
        size_t alias_length = (size_t)(dot - name);
        if(alias_length == 0 || dot[1] == '\0' || strchr(dot + 1, '.') != NULL)
            return 0;
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *import = &module->imports[i];
            const ZirModule *target = import->resolved_module;
            if(import->kind != ZIR_IMPORT_MODULE || target == NULL ||
               !file_scope_visible_at(source_path, import->is_file_private,
                                      import->span) ||
               strlen(import->name) != alias_length ||
               strncmp(import->name, name, alias_length) != 0)
                continue;
            for(int f = 0; f < target->function_count; f++) {
                const ZirFunction *candidate = &target->functions[f];
                if(candidate->is_public && !strcmp(candidate->name, dot + 1)) {
                    if(*function != NULL && *function != candidate) {
                        *owner = NULL;
                        *function = NULL;
                        return -1;
                    }
                    *owner = target;
                    *function = candidate;
                }
            }
            const ZirModule *nested_owner = NULL;
            const ZirFunction *nested = NULL;
            int status = resolve_function_depth(target, dot + 1, NULL,
                &nested_owner, &nested, depth + 1, visits);
            if(status < 0) return -1;
            if(status > 0 && nested->is_public && !nested->is_file_private) {
                if(*function != NULL && *function != nested) return -1;
                *owner = nested_owner;
                *function = nested;
            }
        }
        return *function != NULL;
    }
    for(int i = 0; i < module->function_count; i++) {
        const ZirFunction *candidate = &module->functions[i];
        if(strcmp(candidate->name, name) != 0 ||
           !file_scope_visible_at(source_path, candidate->is_file_private,
                                  candidate->span))
            continue;
        if(candidate->is_file_private) {
            *owner = module;
            *function = candidate;
            return 1;
        }
        if(*function != NULL) {
            *owner = NULL;
            *function = NULL;
            return -1;
        }
        *owner = module;
        *function = candidate;
    }
    if(*function != NULL)
        return 1;
    for(int i = 0; i < module->import_count; i++) {
        if(depth > 0 && !module->imports[i].is_using) continue;
        if((module->imports[i].kind != ZIR_IMPORT_OPEN &&
            !(module->imports[i].kind == ZIR_IMPORT_MODULE &&
              module->imports[i].is_using)) ||
           !file_scope_visible_at(source_path,
                                  module->imports[i].is_file_private,
                                  module->imports[i].span))
            continue;
        const ZirModule *imported = module->imports[i].resolved_module;
        if(imported == NULL)
            continue;
        for(int f = 0; f < imported->function_count; f++) {
            const ZirFunction *candidate = &imported->functions[f];
            if(!candidate->is_public || strcmp(candidate->name, name) != 0)
                continue;
            if(*function != NULL && *function != candidate) {
                *owner = NULL;
                *function = NULL;
                return -1;
            }
            *owner = imported;
            *function = candidate;
        }
        const ZirModule *nested_owner = NULL;
        const ZirFunction *nested = NULL;
        int status = resolve_function_depth(imported, name, NULL,
            &nested_owner, &nested, depth + 1, visits);
        if(status < 0) return -1;
        if(status > 0 && nested->is_public && !nested->is_file_private) {
            if(*function != NULL && *function != nested) return -1;
            *owner = nested_owner;
            *function = nested;
        }
    }
    return *function != NULL;
}

static int
resolve_function_depth(const ZirModule *module, const char *name,
                       const char *source_path, const ZirModule **owner,
                       const ZirFunction **function, int depth, SymbolVisit **visits)
{
    int created;
    SymbolVisit *visit = symbol_visit(visits, module, name, source_path, depth, &created);
    if(created) {
        visit->status = resolve_function_uncached(module, name, source_path,
                                                  owner, function, depth, visits);
        visit->owner = *owner;
        visit->symbol = *function;
    }
    *owner = visit->owner;
    *function = visit->symbol;
    return visit->status;
}

static int
resolve_function_at(const ZirModule *module, const char *name,
                  const char *source_path, const ZirModule **owner,
                  const ZirFunction **function)
{
    SymbolVisit *visits = NULL;
    int status = resolve_function_depth(module, name, source_path, owner,
                                       function, 0, &visits);
    free_symbol_visits(visits);
    return status;
}

int
ResolveFunction(const ZirModule *module, const char *name,
                const ZirModule **owner, const ZirFunction **function)
{
    return ResolveFunctionAt(module, name, module->lookup_path,
                             owner, function);
}

static int resolve_global_depth(const ZirModule *module, const char *name,
                     const char *source_path, const ZirModule **owner,
                     const ZirGlobal **global, int depth, SymbolVisit **visits);

static int
resolve_global_uncached(const ZirModule *module, const char *name,
                     const char *source_path, const ZirModule **owner,
                     const ZirGlobal **global, int depth, SymbolVisit **visits)
{
    const char *dot = strchr(name, '.');
    *owner = NULL;
    *global = NULL;
    if(dot == NULL) {
        for(int g = 0; g < module->global_count; g++) {
            const ZirGlobal *candidate = &module->globals[g];
            if(strcmp(candidate->name, name) != 0 ||
               !file_scope_visible_at(source_path,
                                      candidate->is_file_private,
                                      candidate->span))
                continue;
            if(candidate->is_file_private) {
                *owner = module;
                *global = candidate;
                return 1;
            }
            if(*global != NULL) {
                *owner = NULL;
                *global = NULL;
                return -1;
            }
            *owner = module;
            *global = candidate;
        }
        if(*global != NULL)
            return 1;
    } else if(dot == name || dot[1] == '\0' || strchr(dot + 1, '.') != NULL)
        return 0;

    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(depth > 0 && !import->is_using) continue;
        const ZirModule *target = import->resolved_module;
        if(target == NULL ||
           !file_scope_visible_at(source_path, import->is_file_private,
                                  import->span))
            continue;
        if(dot != NULL) {
            size_t alias_length = (size_t)(dot - name);
            if(import->kind != ZIR_IMPORT_MODULE ||
               strlen(import->name) != alias_length ||
               strncmp(import->name, name, alias_length) != 0)
                continue;
        } else if(import->kind != ZIR_IMPORT_OPEN &&
                  !(import->kind == ZIR_IMPORT_MODULE && import->is_using))
            continue;
        const char *symbol = dot == NULL ? name : dot + 1;
        for(int g = 0; g < target->global_count; g++) {
            const ZirGlobal *candidate = &target->globals[g];
            if(candidate->is_static || candidate->is_file_private ||
               strcmp(candidate->name, symbol) != 0)
                continue;
            if(*global != NULL && *global != candidate) {
                *owner = NULL;
                *global = NULL;
                return -1;
            }
            *owner = target;
            *global = candidate;
        }
        const ZirModule *nested_owner = NULL;
        const ZirGlobal *nested = NULL;
        int status = resolve_global_depth(target, symbol, NULL,
            &nested_owner, &nested, depth + 1, visits);
        if(status < 0) return -1;
        if(status > 0 && !nested->is_file_private && !nested->is_static) {
            if(*global != NULL && *global != nested) return -1;
            *owner = nested_owner;
            *global = nested;
        }
    }
    return *global != NULL;
}

static int
resolve_global_depth(const ZirModule *module, const char *name,
                     const char *source_path, const ZirModule **owner,
                     const ZirGlobal **global, int depth, SymbolVisit **visits)
{
    int created;
    SymbolVisit *visit = symbol_visit(visits, module, name, source_path, depth, &created);
    if(created) {
        visit->status = resolve_global_uncached(module, name, source_path,
                                                owner, global, depth, visits);
        visit->owner = *owner;
        visit->symbol = *global;
    }
    *owner = visit->owner;
    *global = visit->symbol;
    return visit->status;
}

int
ResolveGlobalAt(const ZirModule *module, const char *name,
                const char *source_path, const ZirModule **owner,
                const ZirGlobal **global)
{
    SymbolVisit *visits = NULL;
    int status = resolve_global_depth(module, name, source_path, owner, global, 0, &visits);
    free_symbol_visits(visits);
    return status;
}

int
ResolveGlobal(const ZirModule *module, const char *name,
              const ZirModule **owner, const ZirGlobal **global)
{
    return ResolveGlobalAt(module, name, module->lookup_path, owner, global);
}

const ZirType *
BuiltinType(const char *name)
{
    static const ZirType location = {
        .name = "Source_Code_Location",
        .body = "fully_pathed_filename: string;\nline_number: s64;\n",
        .is_public = 1
    };
    return strcmp(name, location.name) == 0 ? &location : NULL;
}

typedef struct ModuleVisits {
    const ZirModule *small[64];
    const ZirModule **slots;
    size_t count, capacity;
} ModuleVisits;

static size_t
module_visit_slot(const ZirModule *const *slots, size_t capacity,
                  const ZirModule *module)
{
    size_t hash = (uintptr_t)module >> 3;
    hash ^= hash >> 16;
    hash *= UINT32_C(0x85ebca6b);
    hash ^= hash >> 13;
    size_t slot = hash & (capacity - 1);
    while(slots[slot] != NULL && slots[slot] != module)
        slot = (slot + 1) & (capacity - 1);
    return slot;
}

static int
visit_module(ModuleVisits *visits, const ZirModule *module)
{
    size_t slot = module_visit_slot(visits->slots, visits->capacity, module);
    if(visits->slots[slot] != NULL) return 0;
    if(visits->count == visits->capacity / 2) {
        if(visits->capacity > SIZE_MAX / 2 / sizeof(*visits->slots)) {
            DiagnosticOutOfMemory(); exit(1);
        }
        size_t capacity = visits->capacity * 2;
        const ZirModule **slots = AllocateOrExit(capacity * sizeof(*slots));
        memset(slots, 0, capacity * sizeof(*slots));
        for(size_t i = 0; i < visits->capacity; i++)
            if(visits->slots[i] != NULL)
                slots[module_visit_slot(slots, capacity, visits->slots[i])] = visits->slots[i];
        if(visits->slots != visits->small) free(visits->slots);
        visits->slots = slots; visits->capacity = capacity;
        slot = module_visit_slot(slots, capacity, module);
    }
    visits->slots[slot] = module;
    visits->count++;
    return 1;
}

/* Finds the module named `name` among those `module` reaches through its
 * imports. A type written as `module_name.Type` names a type that is visible
 * to its user only through another module, such as the type of a field of an
 * imported record. Package modules carry unique internal names. Preserve
 * depth-first import order, with indexed visits and a heap-grown stack for
 * large or deep graphs. Small walks need no heap scratch storage. */
static const ZirModule *
reachable_module(const ZirModule *module, const char *name, size_t length)
{
    if(strlen(module->name) == length &&
       strncmp(module->name, name, length) == 0)
        return module;
    typedef struct ModuleFrame { const ZirModule *module; int next; } ModuleFrame;
    ModuleFrame small[16], *frames = small;
    size_t count = 1, capacity = sizeof(small) / sizeof(*small);
    frames[0] = (ModuleFrame){.module = module};
    ModuleVisits visits = {0};
    visits.slots = visits.small;
    visits.capacity = sizeof(visits.small) / sizeof(*visits.small);
    visit_module(&visits, module);
    const ZirModule *found = NULL;
    while(count) {
        ModuleFrame *frame = &frames[count - 1];
        if(frame->next >= frame->module->import_count) { count--; continue; }
        const ZirModule *target = frame->module->imports[frame->next++].resolved_module;
        if(target == NULL) continue;
        if(strlen(target->name) == length && !strncmp(target->name, name, length)) {
            found = target; break;
        }
        if(!visit_module(&visits, target)) continue;
        if(count == capacity) {
            if(capacity > SIZE_MAX / 2 / sizeof(*frames)) {
                DiagnosticOutOfMemory(); exit(1);
            }
            capacity *= 2;
            ModuleFrame *grown = AllocateOrExit(capacity * sizeof(*grown));
            memcpy(grown, frames, count * sizeof(*grown));
            if(frames != small) free(frames);
            frames = grown;
        }
        frames[count++] = (ModuleFrame){.module = target};
    }
    if(visits.slots != visits.small) free(visits.slots);
    if(frames != small) free(frames);
    return found;
}

static const ZirType *
local_type(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *type = &module->types[i];
        if(type->is_file_private && !strcmp(type->name, name) &&
           file_scope_visible(module, 1, type->span)) return type;
    }
    for(int i = 0; i < module->type_count; i++) {
        const ZirType *type = &module->types[i];
        if(!strcmp(type->name, name) &&
           file_scope_visible(module, type->is_file_private, type->span)) return type;
    }
    return NULL;
}

typedef struct TypeSearch {
    ModuleVisits visits;
    const ZirModule *small[32];
    const ZirModule **pending;
    size_t count, capacity;
} TypeSearch;

static void
push_type_module(TypeSearch *search, const ZirModule *module)
{
    if(!visit_module(&search->visits, module)) return;
    if(search->count == search->capacity) {
        if(search->capacity > SIZE_MAX / 2 / sizeof(*search->pending)) {
            DiagnosticOutOfMemory(); exit(1);
        }
        search->capacity *= 2;
        const ZirModule **grown = AllocateOrExit(search->capacity * sizeof(*grown));
        memcpy(grown, search->pending, search->count * sizeof(*grown));
        if(search->pending != search->small) free(search->pending);
        search->pending = grown;
    }
    search->pending[search->count++] = module;
}

/* Resolve one unqualified spelling over the public re-export graph. Each
 * module contributes its own declaration or its using imports, never both.
 * Collect declarations globally so a back edge cannot turn an unfinished
 * per-module answer into a kept miss or hide a conflicting declaration. */
static int
imported_type(const ZirModule *module, const char *name,
              const char *alias, size_t alias_length,
              const ZirType **type, const ZirModule **owner,
              const ZirModule **alias_scope)
{
    TypeSearch search = {0};
    search.visits.slots = search.visits.small;
    search.visits.capacity = sizeof(search.visits.small) / sizeof(*search.visits.small);
    search.pending = search.small;
    search.capacity = sizeof(search.small) / sizeof(*search.small);
    *type = NULL; *owner = NULL;
    if(alias_scope) *alias_scope = NULL;
    if(alias == NULL) visit_module(&search.visits, module);
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        const ZirModule *target = import->resolved_module;
        if(target == NULL || !file_scope_visible(module, import->is_file_private, import->span)) continue;
        if(alias != NULL) {
            if(import->kind != ZIR_IMPORT_MODULE || strlen(import->name) != alias_length ||
               strncmp(import->name, alias, alias_length)) continue;
            if(alias_scope) *alias_scope = target;
        } else if(import->kind != ZIR_IMPORT_OPEN &&
                  !(import->kind == ZIR_IMPORT_MODULE && import->is_using)) continue;
        push_type_module(&search, target);
    }
    int status = 0;
    while(search.count) {
        const ZirModule *scope = search.pending[--search.count];
        int declared = 0;
        for(int i = 0; i < scope->type_count; i++) {
            const ZirType *candidate = &scope->types[i];
            if(candidate->is_file_private || strcmp(candidate->name, name)) continue;
            declared = 1;
            if(!candidate->is_public) continue;
            if(*type != NULL && *type != candidate) { status = -1; goto done; }
            *type = candidate; *owner = scope;
        }
        /* Module locals shadow imports; file locals do not form part of
         * another module's exported namespace. */
        if(declared) continue;
        for(int i = 0; i < scope->import_count; i++) {
            const ZirImport *import = &scope->imports[i];
            if(!import->is_using || !import->is_public || import->is_file_private ||
               (import->kind != ZIR_IMPORT_OPEN && import->kind != ZIR_IMPORT_MODULE) ||
               import->resolved_module == NULL) continue;
            push_type_module(&search, import->resolved_module);
        }
    }
    status = *type != NULL;
done:
    if(search.visits.slots != search.visits.small) free(search.visits.slots);
    if(search.pending != search.small) free(search.pending);
    if(status < 0) { *type = NULL; *owner = NULL; }
    return status;
}

static const ZirType *
find_type(const ZirModule *module, const char *name, const ZirModule **owner)
{
    if(owner) *owner = NULL;
    const char *dot = strchr(name, '.');
    if(dot == NULL) {
        const ZirType *local = local_type(module, name);
        if(local != NULL) {
            if(owner) *owner = module;
            return local;
        }
    } else if(dot == name || !dot[1] || strchr(dot + 1, '.') != NULL) return NULL;
    const ZirType *type = NULL;
    const ZirModule *scope = NULL, *alias_scope = NULL;
    int status = imported_type(module, dot ? dot + 1 : name,
        dot ? name : NULL, dot ? (size_t)(dot - name) : 0,
        &type, &scope, &alias_scope);
    if(status < 0) return NULL;
    if(type == NULL && dot != NULL) {
        const ZirModule *target = reachable_module(module, name, (size_t)(dot - name));
        for(int i = 0; target != NULL && i < target->type_count; i++) {
            const ZirType *candidate = &target->types[i];
            if(candidate->is_public && !candidate->is_file_private && !strcmp(candidate->name, dot + 1)) {
                type = candidate; scope = target; break;
            }
        }
    }
    if(type == NULL && (dot == NULL || alias_scope != NULL)) {
        type = BuiltinType(dot ? dot + 1 : name);
        if(type != NULL) scope = dot ? alias_scope : module;
    }
    if(owner) *owner = scope;
    return type;
}

static void kept_text_failed(void)
{
    DiagnosticOutOfMemory();
    exit(1);
}

/* Keep entry-scope type and procedure answers, including misses and
 * ambiguity. Recursive import walks keep their own cycle/visit state; a
 * nested answer is not interchangeable with an entry-scope answer. Names
 * and paths are kept text, so their pointers identify immutable spellings. */
typedef enum LookupKind { LOOKUP_TYPE, LOOKUP_FUNCTION } LookupKind;
typedef struct SymbolLookup {
    const ZirModule *module;
    const char *name;
    const char *path;
    unsigned long generation;
    LookupKind kind;
    int status;
    const void *symbol;
    const ZirModule *owner;
} SymbolLookup;

typedef struct SymbolLookups {
    SymbolLookup *items;
    size_t count, slots;
    unsigned long generation;
    int verify;
} SymbolLookups;
static void free_symbol_lookups(void *value) { free(((SymbolLookups *)value)->items); }
static SymbolLookups *context_symbol_lookups(void)
{
    SymbolLookups *lookups = CompilerContextStorage(CompilerContextCurrent(), COMPILER_SYMBOLS, sizeof(SymbolLookups), free_symbol_lookups);
    if(!lookups->generation) { lookups->generation = 1; lookups->verify = -1; }
    return lookups;
}
#define symbol_lookups (*context_symbol_lookups())
static atomic_flag symbol_lookup_lock = ATOMIC_FLAG_INIT;

void
SymbolLookupsChanged(void)
{
    while(atomic_flag_test_and_set_explicit(&symbol_lookup_lock, memory_order_acquire)) {}
    if(++symbol_lookups.generation == 0) {
        if(symbol_lookups.slots)
            memset(symbol_lookups.items, 0,
                   symbol_lookups.slots * sizeof(*symbol_lookups.items));
        symbol_lookups.count = 0;
        symbol_lookups.generation = 1;
    }
    atomic_flag_clear_explicit(&symbol_lookup_lock, memory_order_release);
}

static size_t
symbol_lookup_slot(const SymbolLookup *items, size_t slots, const ZirModule *module,
                   const char *name, const char *path, LookupKind kind)
{
    size_t slot = (((uintptr_t)module >> 4) * 31 + ((uintptr_t)name >> 3) * 17 +
                   ((uintptr_t)path >> 3) + (unsigned)kind) * 11400714819323198485ull;
    slot &= slots - 1;
    while(items[slot].name != NULL &&
          (items[slot].module != module || items[slot].name != name ||
           items[slot].path != path || items[slot].kind != kind))
        slot = (slot + 1) & (slots - 1);
    return slot;
}

static SymbolLookup
fresh_symbol_lookup(const ZirModule *module, const char *name,
                    const char *path, LookupKind kind)
{
    SymbolLookup answer = {.module = module, .name = name, .path = path,
        .generation = symbol_lookups.generation, .kind = kind};
    if(kind == LOOKUP_TYPE) {
        answer.symbol = find_type(module, name, &answer.owner);
        answer.status = answer.symbol != NULL;
    } else {
        const ZirFunction *function = NULL;
        answer.status = resolve_function_at(module, name, path,
                                             &answer.owner, &function);
        answer.symbol = function;
    }
    return answer;
}

static const void *
lookup_symbol_persistent(const ZirModule *module, const char *name,
              const char *path, LookupKind kind,
              const ZirModule **owner, int *status)
{
    while(atomic_flag_test_and_set_explicit(&symbol_lookup_lock, memory_order_acquire)) {}
    if(symbol_lookups.verify < 0) {
        const char *verify = getenv("ZIRAN_VERIFY_SYMBOL_LOOKUPS");
        symbol_lookups.verify = verify != NULL && !strcmp(verify, "1");
    }
    if(symbol_lookups.count * 2 >= symbol_lookups.slots) {
        /* Grow, keeping only answers from the current generation. */
        size_t old_slots = symbol_lookups.slots;
        SymbolLookup *old = symbol_lookups.items;
        size_t live = 0;
        for(size_t i = 0; i < old_slots; i++)
            live += old[i].name != NULL && old[i].generation == symbol_lookups.generation;
        size_t slots = 256;
        while(slots < live * 4)
            slots *= 2;
        symbol_lookups.items = calloc(slots, sizeof(*symbol_lookups.items));
        if(symbol_lookups.items == NULL)
            kept_text_failed();
        symbol_lookups.slots = slots;
        symbol_lookups.count = 0;
        for(size_t i = 0; i < old_slots; i++)
            if(old[i].name != NULL && old[i].generation == symbol_lookups.generation) {
                symbol_lookups.items[symbol_lookup_slot(symbol_lookups.items, slots,
                    old[i].module, old[i].name, old[i].path, old[i].kind)] = old[i];
                symbol_lookups.count++;
            }
        free(old);
    }
    const char *kept_name = KeepText(name);
    const char *kept_path = KeepText(path == NULL ? "" : path);
    size_t slot = symbol_lookup_slot(symbol_lookups.items, symbol_lookups.slots,
                                     module, kept_name, kept_path, kind);
    SymbolLookup *lookup = &symbol_lookups.items[slot];
    if(lookup->name != NULL && lookup->generation == symbol_lookups.generation) {
        ProfileCount(kind == LOOKUP_TYPE ? "type_lookup_hit" : "function_lookup_hit");
        if(symbol_lookups.verify) {
            SymbolLookup fresh = fresh_symbol_lookup(module, name, path, kind);
            if(fresh.symbol != lookup->symbol || fresh.owner != lookup->owner ||
               fresh.status != lookup->status) {
                Diagnostic(module->span, "compiler.internal",
                        "kept %s lookup of %s in %s is stale",
                        kind == LOOKUP_TYPE ? "type" : "procedure", name, module->name);
                abort();
            }
        }
    } else {
        ProfileCount(kind == LOOKUP_TYPE ? "type_lookup_miss" : "function_lookup_miss");
        if(lookup->name == NULL)
            symbol_lookups.count++;
        *lookup = fresh_symbol_lookup(module, kept_name, kept_path, kind);
    }
    const void *symbol = lookup->symbol;
    if(owner)
        *owner = lookup->owner;
    if(status)
        *status = lookup->status;
    atomic_flag_clear_explicit(&symbol_lookup_lock, memory_order_release);
    return symbol;
}

static const void *
lookup_symbol(const ZirModule *module, const char *name, const char *path,
              LookupKind kind, const ZirModule **owner, int *status)
{
    CompilerContext *previous = CompilerContextSelect(module->context);
    CompilerMemory *memory = CompilerMemoryPause();
    const void *result = lookup_symbol_persistent(module, name, path, kind, owner, status);
    CompilerMemoryResume(memory);
    CompilerContextSelect(previous);
    return result;
}

int
ResolveFunctionAt(const ZirModule *module, const char *name,
                  const char *source_path, const ZirModule **owner,
                  const ZirFunction **function)
{
    /* Tiny import-free scopes are cheaper to scan directly. They cannot
     * recurse, so no import-graph visit storage is needed either. */
    if(module->import_count == 0 && module->function_count <= 4)
        return resolve_function_uncached(module, name, source_path,
                                          owner, function, 0, NULL);
    int status;
    *function = lookup_symbol(module, name, source_path, LOOKUP_FUNCTION,
                               owner, &status);
    return status;
}

const ZirType *
FindType(const ZirModule *module, const char *name, const ZirModule **owner)
{
    /* Scalars, pointers, arrays, and slices are never declared types, and
     * they are most lookups; skip searching every imported module. */
    if(module == NULL || name == NULL || !*name ||
       *name == '*' || *name == '[' || BuiltinTypeName(name)) {
        if(owner)
            *owner = NULL;
        return NULL;
    }
    return lookup_symbol(module, name, module->lookup_path, LOOKUP_TYPE,
                          owner, NULL);
}

static const ZirType *
using_field_record(const ZirModule *owner, const char *type,
                   const ZirModule **next_owner)
{
    while(isspace((unsigned char)*type)) type++;
    if(*type == '*') {
        type++;
        while(isspace((unsigned char)*type)) type++;
    }
    const ZirType *record = FindType(owner, type, next_owner);
    return record != NULL && !record->is_enum &&
           !record->is_procedure_type && !record->is_record_template ?
           record : NULL;
}

static int
resolve_record_field(const ZirModule *owner, const ZirType *record,
                     const char *name, char *path, size_t path_size,
                     char *type, size_t type_size, int depth)
{
    if(depth > 16 || record == NULL) return -1;
    ZirTypeField direct;
    int status = TypeFindField(record, name, &direct);
    if(status != 0) {
        if(status < 0 || strlen(name) >= path_size ||
           strlen(direct.type) >= type_size)
            return -1;
        copy_text(path, path_size, name);
        copy_text(type, type_size, direct.type);
        return 1;
    }
    int matches = 0;
    size_t offset = 0;
    ZirTypeField field;
    while((status = TypeNextField(record, &offset, &field)) == 1) {
        if(!field.is_using) continue;
        const ZirModule *nested_owner = NULL;
        const ZirType *nested = using_field_record(owner, field.type,
                                                    &nested_owner);
        if(nested == NULL) return -1;
        char tail[ZIR_NAME_MAX], leaf[ZIR_NAME_MAX];
        int found = resolve_record_field(nested_owner, nested, name,
                                         tail, sizeof(tail), leaf,
                                         sizeof(leaf), depth + 1);
        if(found < 0) return -1;
        if(found == 0) continue;
        if(matches++) return -1;
        int written = snprintf(path, path_size, "%s.%s", field.name, tail);
        if(written < 0 || (size_t)written >= path_size ||
           strlen(leaf) >= type_size)
            return -1;
        copy_text(type, type_size, leaf);
    }
    return status < 0 ? -1 : matches;
}

int
ResolveRecordField(const ZirModule *owner, const ZirType *record,
                   const char *name, char *path, size_t path_size,
                   char *type, size_t type_size)
{
    if(owner == NULL || record == NULL || name == NULL ||
       path == NULL || type == NULL || !path_size || !type_size)
        return -1;
    return resolve_record_field(owner, record, name, path, path_size,
                                type, type_size, 0);
}

int
RecordFieldPathType(const ZirModule *owner, const ZirType *record,
                    const char *path, char *type, size_t type_size)
{
    if(owner == NULL || record == NULL || path == NULL ||
       type == NULL || !type_size)
        return 0;
    const char *dot = strchr(path, '.');
    size_t length = dot == NULL ? strlen(path) : (size_t)(dot - path);
    if(length == 0 || length >= ZIR_NAME_MAX) return 0;
    char name[ZIR_NAME_MAX];
    memcpy(name, path, length);
    name[length] = '\0';
    ZirTypeField field;
    if(TypeFindField(record, name, &field) != 1) return 0;
    if(dot == NULL) {
        if(strlen(field.type) >= type_size) return 0;
        copy_text(type, type_size, field.type);
        return 1;
    }
    if(!field.is_using) return 0;
    const ZirModule *nested_owner = NULL;
    const ZirType *nested = using_field_record(owner, field.type,
                                                &nested_owner);
    return nested != NULL &&
           RecordFieldPathType(nested_owner, nested, dot + 1,
                               type, type_size);
}

static void *
realloc_array(void *ptr, int *cap, int count, size_t elem_size)
{
    void *next;
    int ncap;

    if(count < *cap)
        return ptr;
    ncap = *cap == 0 ? 4 : *cap * 2;
    next = realloc(ptr, (size_t)ncap * elem_size);
    if(next == NULL)
        return NULL;
    memset((char *)next + (size_t)(*cap) * elem_size, 0,
           (size_t)(ncap - *cap) * elem_size);
    *cap = ncap;
    return next;
}

void
copy_text(char *dst, size_t dst_size, const char *src)
{
    if(dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    /* Called for nearly every name the compiler handles; a bounded move is
     * much cheaper than formatting, and stays correct if a caller copies a
     * string onto part of itself. */
    size_t length = strnlen(src, dst_size - 1);
    memmove(dst, src, length);
    dst[length] = '\0';
}

static void free_partial_program(void *pointer) { ProgramFree(pointer); }

ZirProgram *
ProgramNew(void)
{
    ZirProgram *program = calloc(1, sizeof(ZirProgram));
    if(program) {
        CompilerMemoryCleanup(program, free_partial_program);
        program->context = CompilerContextCurrent();
        if(!CompilerMemoryRecovering()) {
            CompilerContextRetain(program->context);
            program->context_retained = 1;
        }
    }
    return program;
}

void
ProgramFree(ZirProgram *program)
{
    int i;

    if(program == NULL)
        return;
    CompilerContext *context = program->context;
    int retained = program->context_retained, owned = program->owns_context;
    CompilerContext *previous = CompilerContextSelect(context);
    SymbolLookupsChanged();
    for(i = 0; i < program->module_count; i++) {
        ZirModule *m = &program->modules[i];
        int j;

        int owned_functions = m->bundle_shared_functions ?
                              m->bundle_own_functions : m->function_count;
        for(j = 0; j < owned_functions; j++) {
            free(m->functions[j].stmts);
            free(m->functions[j].exprs);
        }
        free(m->globals);
        free(m->imports);
        free(m->functions);
        free(m->defines);
        free(m->asserts);
        free(m->usings);
        for(int l = 0; l < m->law_count; l++) {
            if(m->laws[l].claim != NULL) {
                free(m->laws[l].claim->exprs);
                free(m->laws[l].claim->stmts);
                free(m->laws[l].claim);
            }
        }
        for(int p = 0; p < m->proof_count; p++) {
            free(m->proofs[p].steps);
            free(m->proofs[p].terms.exprs);
            free(m->proofs[p].terms.stmts);
        }
        free(m->proofs);
        free(m->laws);
        free(m->law_waivers);
        free(m->types);
    }
    free(program->modules);
    for(int d = 0; d < program->retained_context_count; d++)
        CompilerContextRelease(program->retained_contexts[d]);
    free(program->retained_contexts);
    free(program);
    CompilerContextSelect(previous);
    if(retained) CompilerContextRelease(context);
    if(owned) CompilerContextDispose(context);
}

static int retain_context(ZirProgram *target, CompilerContext *context)
{
    if(!context || context == target->context) return 1;
    for(int i = 0; i < target->retained_context_count; i++)
        if(target->retained_contexts[i] == context) return 1;
    CompilerContext **items = realloc(target->retained_contexts,
        ((size_t)target->retained_context_count + 1) * sizeof(*items));
    if(!items) return 0;
    target->retained_contexts = items;
    items[target->retained_context_count++] = context;
    CompilerContextRetain(context);
    return 1;
}

int ProgramInheritContexts(ZirProgram *target, const ZirProgram *source)
{
    if(!retain_context(target, source->context)) return 0;
    for(int m = 0; m < source->module_count; m++) {
        if(!retain_context(target, source->modules[m].context) ||
           !retain_context(target, source->modules[m].span.context)) return 0;
    }
    for(int i = 0; i < source->retained_context_count; i++)
        if(!retain_context(target, source->retained_contexts[i])) return 0;
    return 1;
}


ZirModule *
ProgramAddModule(ZirProgram *program, const char *name,
                    const char *source_path, ZirSourceSpan span)
{
    ZirModule *modules;
    ZirModule *m;

    if(program == NULL)
        return NULL;
    SymbolLookupsChanged();
    modules = realloc_array(program->modules, &program->module_cap,
                                program->module_count, sizeof(ZirModule));
    if(modules == NULL)
        return NULL;
    program->modules = modules;
    m = &program->modules[program->module_count++];
    memset(m, 0, sizeof(*m));
    m->context = program->context;
    copy_text(m->name, sizeof(m->name), name);
    copy_text(m->source_path, sizeof(m->source_path), source_path);
    m->span = span;
    return m;
}

ZirImport *
ModuleAddImport(ZirModule *module, ZirImportKind kind, const char *name,
                   const char *target, const char *signature, int required,
                   ZirSourceSpan span)
{
    ZirImport *imports;
    ZirImport *imp;

    if(module == NULL)
        return NULL;
    SymbolLookupsChanged();
    imports = realloc_array(module->imports, &module->import_cap,
                                module->import_count, sizeof(ZirImport));
    if(imports == NULL)
        return NULL;
    module->imports = imports;
    imp = &module->imports[module->import_count++];
    memset(imp, 0, sizeof(*imp));
    imp->kind = kind;
    copy_text(imp->name, sizeof(imp->name), name);
    copy_text(imp->target, sizeof(imp->target), target);
    copy_text(imp->signature, sizeof(imp->signature), signature);
    imp->required = required;
    imp->span = span;
    return imp;
}

ZirFunction *
ModuleAddFunction(ZirModule *module, const char *name, const char *args,
                     const char *return_type, int exported, ZirSourceSpan span)
{
    ZirFunction *functions;
    ZirFunction *fn;

    if(module == NULL)
        return NULL;
    SymbolLookupsChanged();
    functions = realloc_array(module->functions, &module->function_cap,
                                  module->function_count, sizeof(ZirFunction));
    if(functions == NULL)
        return NULL;
    module->functions = functions;
    fn = &module->functions[module->function_count++];
    memset(fn, 0, sizeof(*fn));
    copy_text(fn->name, sizeof(fn->name), name);
    fn->args_text = KeepParameters(args);
    copy_text(fn->return_type, sizeof(fn->return_type), return_type);
    fn->exported = exported;
    fn->span = span;
    return fn;
}

int
DefaultIsLiteral(const char *value)
{
    size_t length;
    while(*value == ' ' || *value == '\t') value++;
    length = strlen(value);
    while(length > 0 && (value[length - 1] == ' ' || value[length - 1] == '\t'))
        length--;
    if(length == 0)
        return 0;
    if((length == 4 && !strncmp(value, "true", 4)) ||
       (length == 5 && !strncmp(value, "false", 5)) ||
       (length == 4 && !strncmp(value, "null", 4)))
        return 1;
    if(value[0] == '"') {
        for(size_t i = 1; i + 1 < length; i++) {
            if(value[i] == '\\') { i++; continue; }
            if(value[i] == '"') return 0;
        }
        return length >= 2 && value[length - 1] == '"';
    }
    size_t i = value[0] == '-' ? 1 : 0;
    if(i >= length || !isdigit((unsigned char)value[i]))
        return 0;
    for(; i < length; i++)
        if(!isalnum((unsigned char)value[i]) && value[i] != '.' && value[i] != '_')
            return 0;
    return 1;
}

void
FunctionDefaultHelperName(const ZirFunction *function, int parameter,
                          char *out, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *pieces[] = {SpanPath(function->span), function->name, NULL};
    for(int i = 0; pieces[i] != NULL; i++) {
        for(const unsigned char *p = (const unsigned char *)pieces[i]; *p; p++) {
            hash ^= *p;
            hash *= UINT64_C(1099511628211);
        }
        hash ^= 0xff;
        hash *= UINT64_C(1099511628211);
    }
    unsigned int numbers[] = {(unsigned int)function->span.line,
                              (unsigned int)parameter};
    for(size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
        for(int byte = 0; byte < 4; byte++) {
            hash ^= (numbers[i] >> (byte * 8)) & 0xffu;
            hash *= UINT64_C(1099511628211);
        }
    snprintf(out, size, "zi_default_%016llx", (unsigned long long)hash);
}

void
ModuleAddGlobal(ZirModule *module, const char *name, const char *type,
                   const char *init, ZirSourceSpan span)
{
    ZirGlobal *globals;

    if(module == NULL)
        return;
    globals = realloc_array(module->globals, &module->global_cap,
                                module->global_count, sizeof(ZirGlobal));
    if(globals == NULL)
        return;
    module->globals = globals;
    memset(&module->globals[module->global_count], 0, sizeof(ZirGlobal));
    copy_text(module->globals[module->global_count].name,
             sizeof(module->globals[0].name), name);
    copy_text(module->globals[module->global_count].type,
             sizeof(module->globals[0].type), type);
    copy_text(module->globals[module->global_count].init,
             sizeof(module->globals[0].init), init);
    module->globals[module->global_count].span = span;
    module->global_count++;
}

void
ModuleAddStatic(ZirModule *module, const char *name, const char *type,
                   const char *init, ZirSourceSpan span)
{
    ModuleAddGlobal(module, name, type, init, span);
    if(module != NULL && module->global_count > 0)
        module->globals[module->global_count - 1].is_static = 1;
}

ZirDefine *
ModuleAddDefine(ZirModule *module, const char *name, const char *value,
                   ZirSourceSpan span)
{
    ZirDefine *defines;
    ZirDefine *d;

    if(module == NULL)
        return NULL;
    defines = realloc_array(module->defines, &module->define_cap,
                                module->define_count, sizeof(ZirDefine));
    if(defines == NULL)
        return NULL;
    module->defines = defines;
    d = &module->defines[module->define_count++];
    memset(d, 0, sizeof(*d));
    copy_text(d->name, sizeof(d->name), name);
    copy_text(d->value, sizeof(d->value), value);
    d->is_public = 1;
    d->span = span;
    return d;
}

int
ModuleAddLaw(ZirModule *module, const char *name, const char *kind,
                 const char *payload, ZirSourceSpan span)
{
    ZirLaw *laws = realloc_array(module->laws, &module->law_cap,
                                 module->law_count, sizeof(ZirLaw));
    ZirLaw *law;
    if(module == NULL || laws == NULL)
        return 0;
    module->laws = laws;
    law = &module->laws[module->law_count++];
    memset(law, 0, sizeof(*law));
    copy_text(law->name, sizeof(law->name), name);
    copy_text(law->kind, sizeof(law->kind), kind);
    copy_text(law->payload, sizeof(law->payload), payload);
    law->span = span;
    return 1;
}

int
ModuleAddLawWaiver(ZirModule *module, const char *name,
                   const char *reason, ZirSourceSpan span)
{
    ZirLawWaiver *waivers = realloc_array(module->law_waivers,
        &module->law_waiver_cap, module->law_waiver_count,
        sizeof(ZirLawWaiver));
    ZirLawWaiver *waiver;
    if(module == NULL || waivers == NULL)
        return 0;
    module->law_waivers = waivers;
    waiver = &module->law_waivers[module->law_waiver_count++];
    memset(waiver, 0, sizeof(*waiver));
    copy_text(waiver->name, sizeof(waiver->name), name);
    copy_text(waiver->reason, sizeof(waiver->reason), reason);
    waiver->span = span;
    return 1;
}

int
ModuleAddProof(ZirModule *module, const char *name, const char *source,
              ZirSourceSpan span)
{
    if(module == NULL) return 0;
    ZirProof *proofs = realloc_array(module->proofs, &module->proof_cap,
                                    module->proof_count, sizeof(*proofs));
    if(proofs == NULL) return 0;
    module->proofs = proofs;
    ZirProof *proof = &proofs[module->proof_count++];
    memset(proof, 0, sizeof(*proof));
    copy_text(proof->name, sizeof(proof->name), name);
    copy_text(proof->source, sizeof(proof->source), source);
    proof->span = span;
    return 1;
}

int
ModuleAddAssert(ZirModule *module, const char *condition,
                   const char *message, ZirSourceSpan span)
{
    ZirAssert *asserts;
    ZirAssert *a;

    if(module == NULL)
        return 0;
    asserts = realloc_array(module->asserts, &module->assert_cap,
                                module->assert_count, sizeof(ZirAssert));
    if(asserts == NULL)
        return 0;
    module->asserts = asserts;
    a = &module->asserts[module->assert_count++];
    memset(a, 0, sizeof(*a));
    copy_text(a->condition, sizeof(a->condition), condition);
    copy_text(a->message, sizeof(a->message), message);
    a->span = span;
    return 1;
}

ZirUsing *
ModuleAddUsing(ZirModule *module, const char *path, ZirSourceSpan span)
{
    ZirUsing *usings = realloc_array(module->usings, &module->using_cap,
                                     module->using_count, sizeof(ZirUsing));
    if(usings == NULL) return NULL;
    module->usings = usings;
    ZirUsing *using = &module->usings[module->using_count++];
    memset(using, 0, sizeof(*using));
    copy_text(using->path, sizeof(using->path), path);
    using->span = span;
    return using;
}

ZirType *
ModuleAddType(ZirModule *module, const char *name, ZirSourceSpan span)
{
    ZirType *types;

    if(module == NULL)
        return NULL;
    SymbolLookupsChanged();
    types = realloc_array(module->types, &module->type_cap,
                              module->type_count, sizeof(ZirType));
    if(types == NULL)
        return NULL;
    module->types = types;
    memset(&module->types[module->type_count], 0, sizeof(ZirType));
    copy_text(module->types[module->type_count].name,
             sizeof(module->types[0].name), name);
    module->types[module->type_count].is_public = 1;
    module->types[module->type_count].span = span;
    return &module->types[module->type_count++];
}

ZirStmt *
FunctionAddStmt(ZirFunction *fn, ZirStmtKind kind, const char *text,
                   ZirSourceSpan span)
{
    ZirStmt *stmts;
    ZirStmt *st;

    if(fn == NULL)
        return NULL;
    stmts = realloc_array(fn->stmts, &fn->stmt_cap, fn->stmt_count,
                              sizeof(ZirStmt));
    if(stmts == NULL)
        return NULL;
    fn->stmts = stmts;
    st = &fn->stmts[fn->stmt_count++];
    StmtReset(st);
    st->kind = kind;
    st->text = KeepText(text);
    st->expr_root = -1;
    st->lhs_root = -1;
    st->span = span;
    return st;
}

void
StmtReset(ZirStmt *stmt)
{
    memset(stmt, 0, sizeof(*stmt));
    stmt->name = "";
    stmt->type = "";
}

void
ExprReset(ZirExpr *expr)
{
    memset(expr, 0, sizeof(*expr));
    expr->slot_type = "";
    expr->argument_name = "";
    expr->name = "";
    expr->type = "";
}

ZirExpr *
FunctionAddExpr(ZirFunction *fn, ZirExprKind kind, const char *text,
                   ZirSourceSpan span)
{
    ZirExpr *exprs;
    ZirExpr *expr;

    if(fn == NULL)
        return NULL;
    exprs = realloc_array(fn->exprs, &fn->expr_cap, fn->expr_count,
                              sizeof(ZirExpr));
    if(exprs == NULL)
        return NULL;
    fn->exprs = exprs;
    expr = &fn->exprs[fn->expr_count++];
    ExprReset(expr);
    expr->kind = kind;
    expr->left = -1;
    expr->right = -1;
    expr->first_child = -1;
    expr->next_sibling = -1;
    expr->third = -1;
    expr->argument_index = -1;
    expr->text = KeepText(text);
    expr->span = span;
    return expr;
}

const char *
ImportKindName(ZirImportKind kind)
{
    switch(kind) {
    case ZIR_IMPORT_OPEN: return "open";
    case ZIR_IMPORT_MODULE: return "module";
    case ZIR_IMPORT_EXTERN: return "extern";
    default: return "unknown";
    }
}

const char *
ExternKindName(ZirExternKind kind)
{
    switch(kind) {
    case ZIR_EXTERN_HOST: return "host";
    case ZIR_EXTERN_GO: return "go";
    case ZIR_EXTERN_C: return "c";
    default: return "none";
    }
}

const char *
ExprKindName(ZirExprKind kind)
{
    switch(kind) {
    case ZIR_EXPR_IDENT: return "ident";
    case ZIR_EXPR_INT: return "int";
    case ZIR_EXPR_FLOAT: return "float";
    case ZIR_EXPR_STRING: return "string";
    case ZIR_EXPR_CALL: return "call";
    case ZIR_EXPR_BINARY: return "binary";
    case ZIR_EXPR_UNARY: return "unary";
    case ZIR_EXPR_MEMBER: return "member";
    case ZIR_EXPR_POINTER_MEMBER: return "pointer_member";
    case ZIR_EXPR_INDEX: return "index";
    case ZIR_EXPR_SLICE: return "slice";
    case ZIR_EXPR_COMPILE_TIME: return "compile_time";
    case ZIR_EXPR_CAST: return "cast";
    case ZIR_EXPR_COMPOUND: return "compound";
    case ZIR_EXPR_FIELD_INIT: return "field_initializer";
    case ZIR_EXPR_SIZE_OF: return "size_of";
    case ZIR_EXPR_CONDITIONAL: return "conditional";
    default: return "unknown";
    }
}

const char *
StmtKindName(ZirStmtKind kind)
{
    switch(kind) {
    case ZIR_STMT_BLOCK_OPEN: return "block_open";
    case ZIR_STMT_BLOCK_CLOSE: return "block_close";
    case ZIR_STMT_DECL: return "decl";
    case ZIR_STMT_ASSIGN: return "assign";
    case ZIR_STMT_EXPR: return "expr";
    case ZIR_STMT_IF: return "if";
    case ZIR_STMT_WHILE: return "while";
    case ZIR_STMT_FOR: return "for";
    case ZIR_STMT_CASE: return "case";
    case ZIR_STMT_RETURN: return "return";
    case ZIR_STMT_BREAK: return "break";
    case ZIR_STMT_CONTINUE: return "continue";
    case ZIR_STMT_DEFER: return "defer";
    case ZIR_STMT_UNUSED: return "unused";
    case ZIR_STMT_UNREACHABLE: return "unreachable";
    case ZIR_STMT_IF_CASE: return "if-case";
    default: return "unknown";
    }
}

static void
dump_span(FILE *out, ZirSourceSpan span)
{
    fprintf(out, "%s:%d:%d", SpanPath(span), span.line, span.column);
    if(span.end_line > 0 && span.end_column > 0 &&
       (span.end_line != span.line || span.end_column != span.column))
        fprintf(out, "-%d:%d", span.end_line, span.end_column);
}

static void
dump_expr(const ZirFunction *fn, int index, FILE *out, int indent)
{
    const ZirExpr *expr;

    if(fn == NULL || index < 0 || index >= fn->expr_count)
        return;
    expr = &fn->exprs[index];
    for(int i = 0; i < indent; i++)
        fputs("  ", out);
    fprintf(out, "expr %s text %s name %s op %s span ",
            ExprKindName(expr->kind), expr->text, expr->name, expr->op);
    dump_span(out, expr->span);
    if(expr->type[0]) fprintf(out, " type %s", expr->type);
    fprintf(out, "\n");
    if(expr->left >= 0)
        dump_expr(fn, expr->left, out, indent + 1);
    if(expr->right >= 0)
        dump_expr(fn, expr->right, out, indent + 1);
    if(expr->third >= 0)
        dump_expr(fn, expr->third, out, indent + 1);
    for(int child = expr->first_child; child >= 0 &&
         child < fn->expr_count; child = fn->exprs[child].next_sibling)
        dump_expr(fn, child, out, indent + 1);
}

void
ProgramDump(const ZirProgram *program, FILE *out)
{
    int i;

    if(out == NULL)
        return;
    fprintf(out, "zir 1\n");
    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++) {
        const ZirModule *m = &program->modules[i];
        int j;

        fprintf(out, "module %s source %s span ", m->name, m->source_path);
        dump_span(out, m->span);
        fprintf(out, "\n");
        for(j = 0; j < m->import_count; j++) {
            const ZirImport *imp = &m->imports[j];

            fprintf(out, "  import %s %s target %s",
                    ImportKindName(imp->kind), imp->name, imp->target);
            if(imp->kind == ZIR_IMPORT_EXTERN)
                fprintf(out, " extern_kind %s extern_symbol %s",
                        ExternKindName(imp->extern_kind),
                        imp->extern_symbol);
            fprintf(out, " required %d signature %s span ",
                    imp->required, imp->signature);
            dump_span(out, imp->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->assert_count; j++) {
            const ZirAssert *a = &m->asserts[j];

            fprintf(out, "  assert condition %s message %s span ",
                    a->condition, a->message);
            dump_span(out, a->span);
            fprintf(out, "\n");
        }
        for(j = 0; j < m->function_count; j++) {
            const ZirFunction *fn = &m->functions[j];
            int k;

            fprintf(out, "  function %s args %s return %s exported %d span ",
                    fn->name, FunctionArgs(fn), fn->return_type, fn->exported);
            dump_span(out, fn->span);
            fprintf(out, "\n");
            for(k = 0; k < fn->stmt_count; k++) {
                const ZirStmt *st = &fn->stmts[k];

                fprintf(out, "    stmt %s text %s span ",
                        StmtKindName(st->kind), st->text);
                dump_span(out, st->span);
                fprintf(out, "\n");
                if(st->expr_root >= 0)
                    dump_expr(fn, st->expr_root, out, 3);
                if(st->lhs_root >= 0)
                    dump_expr(fn, st->lhs_root, out, 3);
            }
        }
    }
}

typedef struct GeneratedOutputs {
    char **paths;
    int count, capacity;
} GeneratedOutputs;
static void free_generated_outputs(void *value)
{
    GeneratedOutputs *outputs = value;
    for(int i = 0; i < outputs->count; i++) free(outputs->paths[i]);
    free(outputs->paths);
}
static GeneratedOutputs *context_outputs(void)
{
    return CompilerContextStorage(CompilerContextCurrent(), COMPILER_OUTPUTS,
        sizeof(GeneratedOutputs), free_generated_outputs);
}
#define generated_outputs (context_outputs()->paths)
#define generated_output_count (context_outputs()->count)
#define generated_output_capacity (context_outputs()->capacity)

static const char *
path_base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

void
GeneratedOutputRecord(const char *path)
{
    const char *name = path;
    for(int i = 0; i < generated_output_count; i++)
        if(strcmp(generated_outputs[i], name) == 0) return;
    if(generated_output_count == generated_output_capacity) {
        int capacity = generated_output_capacity ?
            generated_output_capacity * 2 : 64;
        char **grown = realloc(generated_outputs,
                               (size_t)capacity * sizeof(*grown));
        if(grown == NULL) return;
        generated_outputs = grown;
        generated_output_capacity = capacity;
    }
    size_t length = strlen(name);
    char *copy = malloc(length + 1);
    if(copy == NULL) return;
    memcpy(copy, name, length + 1);
    generated_outputs[generated_output_count++] = copy;
}

const char *
GeneratedOutputPath(int index)
{
    return index >= 0 && index < generated_output_count ? generated_outputs[index] : NULL;
}

FILE *
GeneratedOutputOpen(const char *path, char *temp, size_t size)
{
    const char *name = path_base_name(path);
    int length = snprintf(temp, size, "%.*s.%s.tmp",
                          (int)(name - path), path, name);
    if(length < 0 || (size_t)length >= size) return NULL;
    GeneratedOutputRecord(path);
    return fopen(temp, "wb");
}
/* Buffers same_file_bytes keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct SameFileBytesBuffers {
    char abuf[8192];
    char bbuf[8192];
} SameFileBytesBuffers;

static int same_file_bytes(const char *left, const char *right);

static int
same_file_bytes_with_buffers(const char *left, const char *right, SameFileBytesBuffers *buffers)
{
    FILE *a = fopen(left, "rb");
    FILE *b = fopen(right, "rb");
    int same = a != NULL && b != NULL;
    while(same) {
        size_t an = fread(buffers->abuf, 1, sizeof(buffers->abuf), a);
        size_t bn = fread(buffers->bbuf, 1, sizeof(buffers->bbuf), b);
        if(an != bn || memcmp(buffers->abuf, buffers->bbuf, an) != 0) same = 0;
        else if(an == 0) break;
    }
    if(a != NULL) fclose(a);
    if(b != NULL) fclose(b);
    return same;
}

static int
same_file_bytes(const char *left, const char *right)
{
    static const char scratch_key;
    static _Thread_local uint64_t scratch_identity;
    static _Thread_local CompilerScratch *scratch_hint;
    if(scratch_identity != CompilerContextIdentity()) {
        scratch_hint = CompilerContextScratch(&scratch_key, free_scratch_SameFileBytesBuffers);
        scratch_identity = CompilerContextIdentity();
    }
    SameFileBytesBuffers *buffers = !CompilerMemoryRecovering() ? CompilerScratchTake(scratch_hint) : NULL;
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    int returned = same_file_bytes_with_buffers(left, right, buffers);
    if(CompilerMemoryRecovering() || !CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
    return returned;
}

int
GeneratedOutputReplace(const char *temp, const char *path)
{
    if(same_file_bytes(temp, path))
        return remove(temp) == 0 ? 0 : -1;
    if(rename(temp, path) != 0) {
        remove(temp);
        return -1;
    }
    return 0;
}

static int
starts_with_marker(const char *path, const char *marker)
{
    char line[256];
    FILE *file = fopen(path, "rb");
    if(file == NULL) return 0;
    size_t length = strlen(marker);
    int matches = length < sizeof(line) &&
        fread(line, 1, length, file) == length &&
        memcmp(line, marker, length) == 0;
    fclose(file);
    return matches;
}

/* Generated sources mirror their package directories, so pruning visits
 * subdirectories too. Hidden entries, such as the native object cache, and
 * links to directories elsewhere are never entered. */
static int
prune_generated(const char *directory_path, const char *marker, int depth)
{
    DIR *directory = opendir(directory_path);
    if(directory == NULL) return 0;
    int failed = 0;
    struct dirent *entry;
    while((entry = readdir(directory)) != NULL) {
        const char *name = entry->d_name;
        if(name[0] == '.') continue;
        char path[4096];
        int size = snprintf(path, sizeof(path), "%s/%s", directory_path, name);
        if(size < 0 || (size_t)size >= sizeof(path)) continue;
        struct stat info;
#ifdef _WIN32
        int listed = stat(path, &info) == 0;
#else
        int listed = lstat(path, &info) == 0;
#endif
        if(listed && S_ISDIR(info.st_mode)) {
            if(depth < 32 && prune_generated(path, marker, depth + 1) != 0) failed = 1;
            continue;
        }
        int written = 0;
        for(int i = 0; i < generated_output_count && !written; i++)
            written = strcmp(generated_outputs[i], path) == 0;
        if(written) continue;
        if(starts_with_marker(path, marker) && remove(path) != 0)
            failed = 1;
    }
    closedir(directory);
    return failed ? -1 : 0;
}

int
GeneratedOutputPrune(const char *out_dir, const char *marker)
{
    return prune_generated(out_dir, marker, 0);
}

static int
same_type_application_at_depth(const ZirModule *target_owner, const ZirType *target,
                              const ZirModule *source_owner, const ZirType *source,
                              int depth)
{
    if(depth > 16) return 0;
    if(target != NULL && source != NULL && target_owner != NULL &&
       source_owner != NULL && target->is_synthetic_application &&
       source->is_synthetic_application &&
       strcmp(target->body, source->body) == 0) {
        const ZirType *target_template = FindType(target_owner,
                                                  target->template_name, NULL);
        const ZirType *source_template = FindType(source_owner,
                                                  source->template_name, NULL);
        /* A direct application has one identity across its consumers. Keep
         * separate templates and same-spelled local argument types nominal. */
        if(target_template != NULL && target_template == source_template) {
            char target_args[16][ZIR_NAME_MAX], source_args[16][ZIR_NAME_MAX];
            int target_count = split_top_level(target->template_args,
                                              target_args[0], 16, ZIR_NAME_MAX);
            int source_count = split_top_level(source->template_args,
                                              source_args[0], 16, ZIR_NAME_MAX);
            int same = target_count > 0 && target_count < 16 &&
                       target_count == source_count;
            for(int i = 0; i < target_count && same; i++) {
                const char *target_scalar = ScalarType(target_args[i]);
                const char *source_scalar = ScalarType(source_args[i]);
                if(*target_scalar || *source_scalar)
                    same = strcmp(target_scalar, source_scalar) == 0;
                else {
                    const ZirModule *a_owner = NULL, *b_owner = NULL;
                    const ZirType *a = FindType(target_owner, target_args[i], &a_owner);
                    const ZirType *b = FindType(source_owner, source_args[i], &b_owner);
                    same = a != NULL && b != NULL &&
                           (a == b || same_type_application_at_depth(a_owner, a,
                                         b_owner, b, depth + 1));
                }
            }
            if(same)
                return 1;
        }
    }
    return 0;
}

int
same_type_application(const ZirModule *target_owner, const ZirType *target,
                      const ZirModule *source_owner, const ZirType *source)
{
    return same_type_application_at_depth(target_owner, target, source_owner, source, 0);
}

static void free_scratch_SameFileBytesBuffers(void *pointer)
{
    SameFileBytesBuffers *buffers = pointer;
    free(buffers);
}
