#include "zir.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_text.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
GoForeignTargetValid(const char *target)
{
    if(strncmp(target, "go:", 3) != 0)
        return 0;
    const char *dot = strrchr(target + 3, '.');
    if(dot == NULL || dot == target + 3 ||
       !(isalpha((unsigned char)dot[1]) || dot[1] == '_'))
        return 0;
    if(!strncmp(target, "go:builtin.", 11))
        return dot == target + 10 &&
            (!strcmp(dot + 1, "any") || !strcmp(dot + 1, "error"));
    for(const unsigned char *p = (const unsigned char *)target + 3;
        p < (const unsigned char *)dot; p++)
        if(!(isalnum(*p) || *p == '_' || *p == '/' || *p == '.' || *p == '-'))
            return 0;
    for(const unsigned char *p = (const unsigned char *)dot + 2; *p; p++)
        if(!(isalnum(*p) || *p == '_'))
            return 0;
    return 1;
}

int
RejectForeignGoTypes(const ZirProgram *program)
{
    for(int m = 0; m < program->module_count; m++)
        for(int t = 0; t < program->modules[m].type_count; t++) {
            const ZirType *type = &program->modules[m].types[t];
            if(type->foreign_target[0] || type->is_map) {
                Diagnostic(type->span, "check.record",
                           "%s require the Go target: %s",
                           type->is_map ? "maps" : "foreign Go types", type->name);
                return 0;
            }
        }
    return 1;
}

int
MapPrimitiveName(const char *name)
{
    return !strcmp(name, "MapInit") || !strcmp(name, "MapSet") ||
           !strcmp(name, "MapGet") || !strcmp(name, "MapContains") ||
           !strcmp(name, "MapDelete") || !strcmp(name, "MapClear") ||
           !strcmp(name, "MapCount") || !strcmp(name, "MapKeys") ||
           !strcmp(name, "MapLookup");
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
TypeNextField(const ZirType *record, size_t *offset, ZirTypeField *field)
{
    size_t length = strlen(record->body);

    memset(field, 0, sizeof(*field));
    if(record->is_enum || record->is_procedure_type)
        return -1;
    while(*offset < length) {
        const char *start = record->body + *offset;
        const char *end = start;
        int quote = 0;
        while(*end != '\0') {
            if(quote) {
                if(*end == '\\' && end[1] != '\0')
                    end++;
                else if(*end == '"')
                    quote = 0;
            } else if(*end == '"') {
                quote = 1;
            } else if(*end == ';' || *end == '\n') {
                break;
            }
            end++;
        }
        if(quote)
            return -1;
        const char *colon;
        const char *name_end;
        const char *type_start;
        size_t name_length;
        size_t type_length;

        *offset = *end != '\0' ? (size_t)(end - record->body) + 1 : length;
        while(start < end && isspace((unsigned char)*start))
            start++;
        while(end > start && isspace((unsigned char)end[-1]))
            end--;
        if(end > start && end[-1] == ';')
            end--;
        while(end > start && isspace((unsigned char)end[-1]))
            end--;
        if(start == end)
            continue;
        if((size_t)(end - start) >= 6 &&
           strncmp(start, "using", 5) == 0 &&
           isspace((unsigned char)start[5])) {
            field->is_using = 1;
            start += 5;
            while(start < end && isspace((unsigned char)*start))
                start++;
        }
        colon = memchr(start, ':', (size_t)(end - start));
        if(colon == NULL)
            return -1;
        name_end = colon;
        while(name_end > start && isspace((unsigned char)name_end[-1]))
            name_end--;
        type_start = colon + 1;
        while(type_start < end && isspace((unsigned char)*type_start))
            type_start++;
        name_length = (size_t)(name_end - start);
        const char *type_end = end;
        const char *tag = memchr(type_start, '#', (size_t)(end - type_start));
        if(tag != NULL) {
            static const char directive[] = "#go_tag";
            if(record->is_union ||
               (size_t)(end - tag) < sizeof(directive) ||
               strncmp(tag, directive, sizeof(directive) - 1) != 0 ||
               !isspace((unsigned char)tag[sizeof(directive) - 1]))
                return -1;
            const char *literal = tag + sizeof(directive) - 1;
            while(literal < end && isspace((unsigned char)*literal))
                literal++;
            size_t tag_length = (size_t)(end - literal);
            if(tag_length == 0 || tag_length >= sizeof(field->go_tag))
                return -1;
            memcpy(field->go_tag, literal, tag_length);
            unsigned char decoded[sizeof(field->go_tag)];
            size_t decoded_length;
            if(!DecodeStringLiteral(field->go_tag, decoded, sizeof(decoded),
                                    &decoded_length) ||
               memchr(decoded, '\0', decoded_length) != NULL)
                return -1;
            type_end = tag;
            while(type_end > type_start && isspace((unsigned char)type_end[-1]))
                type_end--;
        }
        type_length = (size_t)(type_end - type_start);
        if(name_length == 0 || name_length >= sizeof(field->name) ||
           type_length == 0 || type_length >= sizeof(field->type))
            return -1;
        if(!isalpha((unsigned char)*start) && *start != '_')
            return -1;
        for(const char *cursor = start + 1; cursor < name_end; cursor++) {
            if(!isalnum((unsigned char)*cursor) && *cursor != '_')
                return -1;
        }
        memcpy(field->name, start, name_length);
        memcpy(field->type, type_start, type_length);
        return 1;
    }
    return 0;
}

int
SliceElementType(const char *type, char *element, size_t element_size)
{
    if(type == NULL || type[0] != '[' || type[1] != ']')
        return 0;
    const char *start = type + 2;
    while(isspace((unsigned char)*start))
        start++;
    size_t length = strlen(start);
    while(length > 0 && isspace((unsigned char)start[length - 1]))
        length--;
    if(length == 0)
        return 0;
    if(element != NULL) {
        if(length >= element_size)
            return 0;
        memcpy(element, start, length);
        element[length] = '\0';
    }
    return 1;
}

int
ArrayElementType(const char *type, char *element, size_t element_size,
                    int *capacity)
{
    const char *cursor;
    const char *close;
    size_t length;
    long count = 0;
    int digits = 0;

    if(type == NULL || type[0] != '[')
        return 0;
    cursor = type + 1;
    while(*cursor && *cursor != ']')
        cursor++;
    if(*cursor != ']' || cursor == type + 1)
        return 0;
    for(const char *digit = type + 1; digit < cursor; digit++)
        digits += *digit >= '0' && *digit <= '9';
    if(type + 1 + digits == cursor) {
        for(const char *digit = type + 1; digit < cursor; digit++) {
            count = count * 10 + (*digit - '0');
            if(count > 1048576)
                return 0;
        }
    } else {
        count = -1; /* A constant expression, resolved by the checker. */
    }
    close = cursor + 1;
    while(isspace((unsigned char)*close))
        close++;
    length = strlen(close);
    while(length > 0 && isspace((unsigned char)close[length - 1]))
        length--;
    if(length == 0)
        return 0;
    if(element != NULL) {
        if(length >= element_size)
            return 0;
        memcpy(element, close, length);
        element[length] = '\0';
    }
    if(capacity != NULL)
        *capacity = (int)count;
    return 1;
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

static int
resolve_function_depth(const ZirModule *module, const char *name,
                       const char *source_path, const ZirModule **owner,
                       const ZirFunction **function, int depth)
{
    *owner = NULL;
    *function = NULL;
    if(depth > 32) return 0;
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
                &nested_owner, &nested, depth + 1);
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
            &nested_owner, &nested, depth + 1);
        if(status < 0) return -1;
        if(status > 0 && nested->is_public && !nested->is_file_private) {
            if(*function != NULL && *function != nested) return -1;
            *owner = nested_owner;
            *function = nested;
        }
    }
    return *function != NULL;
}

int
ResolveFunctionAt(const ZirModule *module, const char *name,
                  const char *source_path, const ZirModule **owner,
                  const ZirFunction **function)
{
    return resolve_function_depth(module, name, source_path, owner,
                                  function, 0);
}

int
ResolveFunction(const ZirModule *module, const char *name,
                const ZirModule **owner, const ZirFunction **function)
{
    return ResolveFunctionAt(module, name, module->lookup_path,
                             owner, function);
}

static int
resolve_global_depth(const ZirModule *module, const char *name,
                     const char *source_path, const ZirModule **owner,
                     const ZirGlobal **global, int depth)
{
    const char *dot = strchr(name, '.');
    *owner = NULL;
    *global = NULL;
    if(depth > 32) return 0;
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
            &nested_owner, &nested, depth + 1);
        if(status < 0) return -1;
        if(status > 0 && !nested->is_file_private && !nested->is_static) {
            if(*global != NULL && *global != nested) return -1;
            *owner = nested_owner;
            *global = nested;
        }
    }
    return *global != NULL;
}

int
ResolveGlobalAt(const ZirModule *module, const char *name,
                const char *source_path, const ZirModule **owner,
                const ZirGlobal **global)
{
    return resolve_global_depth(module, name, source_path, owner, global, 0);
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

/* Finds the module named `name` among those `module` reaches through its
 * imports. A type written as `module_name.Type` names a type that is visible
 * to its user only through another module, such as the type of a field of an
 * imported record. Package modules carry unique internal names. */
static const ZirModule *
reachable_module(const ZirModule *module, const char *name, size_t length,
                 const ZirModule **seen, int *seen_count, int capacity)
{
    for(int s = 0; s < *seen_count; s++)
        if(seen[s] == module) return NULL;
    if(*seen_count >= capacity) return NULL;
    seen[(*seen_count)++] = module;
    if(strlen(module->name) == length &&
       strncmp(module->name, name, length) == 0)
        return module;
    for(int i = 0; i < module->import_count; i++) {
        const ZirModule *target = module->imports[i].resolved_module;
        if(target == NULL) continue;
        const ZirModule *found = reachable_module(target, name, length,
                                                  seen, seen_count, capacity);
        if(found != NULL) return found;
    }
    return NULL;
}

static const ZirType *
find_type_depth(const ZirModule *module, const char *name,
                const ZirModule **owner, int depth)
{
    const ZirType *found = NULL;
    const ZirModule *scope = NULL;
    const char *dot = strchr(name, '.');

    if(owner)
        *owner = NULL;
    if(depth > 32) return NULL;

    if(dot != NULL) {
        size_t alias_length = (size_t)(dot - name);
        if(alias_length == 0 || dot[1] == '\0' || strchr(dot + 1, '.') != NULL)
            return NULL;
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *import = &module->imports[i];
            const ZirModule *target = import->resolved_module;
            if(import->kind != ZIR_IMPORT_MODULE || target == NULL ||
               !file_scope_visible(module, import->is_file_private,
                                   import->span) ||
               strlen(import->name) != alias_length ||
               strncmp(import->name, name, alias_length) != 0)
                continue;
            for(int t = 0; t < target->type_count; t++) {
                const ZirType *candidate = &target->types[t];
                if(!candidate->is_public || strcmp(candidate->name, dot + 1))
                    continue;
                if(found != NULL && found != candidate)
                    return NULL;
                found = candidate;
                scope = target;
            }
            const ZirModule *nested_owner = NULL;
            const ZirType *nested = find_type_depth(target, dot + 1,
                                                    &nested_owner, depth + 1);
            if(nested != NULL && nested->is_public && !nested->is_file_private) {
                if(found != NULL && found != nested) return NULL;
                found = nested;
                scope = nested_owner;
            }
        }
        if(found == NULL) {
            const ZirModule *seen[512];
            int seen_count = 0;
            const ZirModule *target = reachable_module(module, name,
                                                       alias_length, seen,
                                                       &seen_count, 512);
            for(int t = 0; target != NULL && t < target->type_count; t++) {
                const ZirType *candidate = &target->types[t];
                if(candidate->is_public && !candidate->is_file_private &&
                   strcmp(candidate->name, dot + 1) == 0) {
                    found = candidate;
                    scope = target;
                    break;
                }
            }
        }
        if(owner)
            *owner = scope;
        return found;
    }

    for(int i = 0; i < module->type_count; i++) {
        const ZirType *candidate = &module->types[i];
        if(candidate->is_file_private &&
           strcmp(candidate->name, name) == 0 &&
           file_scope_visible(module, 1, candidate->span)) {
            if(owner) *owner = module;
            return candidate;
        }
    }
    for(int i = 0; i < module->type_count; i++) {
        if(strcmp(module->types[i].name, name) == 0 &&
           file_scope_visible(module, module->types[i].is_file_private,
                              module->types[i].span)) {
            if(owner)
                *owner = module;
            return &module->types[i];
        }
    }
    for(int i = 0; i < module->import_count; i++) {
        const ZirImport *import = &module->imports[i];
        if(depth > 0 && !import->is_using) continue;
        const ZirModule *target = import->resolved_module;
        if((import->kind != ZIR_IMPORT_OPEN &&
            !(import->kind == ZIR_IMPORT_MODULE && import->is_using)) ||
           target == NULL ||
           !file_scope_visible(module, import->is_file_private,
                               import->span))
            continue;
        for(int j = 0; j < target->type_count; j++) {
            const ZirType *candidate = &target->types[j];
            if(!candidate->is_public || strcmp(candidate->name, name) != 0)
                continue;
            if(found && found != candidate)
                return NULL;
            found = candidate;
            scope = target;
        }
        const ZirModule *nested_owner = NULL;
        const ZirType *nested = find_type_depth(target, name,
                                                &nested_owner, depth + 1);
        if(nested != NULL && nested->is_public && !nested->is_file_private) {
            if(found != NULL && found != nested) return NULL;
            found = nested;
            scope = nested_owner;
        }
    }
    if(owner)
        *owner = scope;
    if(found != NULL)
        return found;
    found = BuiltinType(name);
    if(found != NULL && owner != NULL)
        *owner = module;
    return found;
}

const ZirType *
FindType(const ZirModule *module, const char *name, const ZirModule **owner)
{
    return find_type_depth(module, name, owner, 0);
}

static int
direct_record_field(const ZirType *record, const char *name,
                    ZirTypeField *found)
{
    size_t offset = 0;
    ZirTypeField field;
    int status;
    while((status = TypeNextField(record, &offset, &field)) == 1)
        if(strcmp(field.name, name) == 0) {
            *found = field;
            return 1;
        }
    return status < 0 ? -1 : 0;
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
    int status = direct_record_field(record, name, &direct);
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
    if(direct_record_field(record, name, &field) != 1) return 0;
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
    snprintf(dst, dst_size, "%s", src);
}

ZirProgram *
ProgramNew(void)
{
    return calloc(1, sizeof(ZirProgram));
}

void
ProgramFree(ZirProgram *program)
{
    int i;

    if(program == NULL)
        return;
    for(i = 0; i < program->module_count; i++) {
        ZirModule *m = &program->modules[i];
        int j;

        for(j = 0; j < m->function_count; j++) {
            free(m->functions[j].stmts);
            free(m->functions[j].exprs);
        }
        free(m->globals);
        free(m->imports);
        free(m->functions);
        free(m->defines);
        free(m->asserts);
        free(m->usings);
        free(m->types);
    }
    free(program->modules);
    free(program);
}

/* Every span names its file, and the compiler makes many spans, so each
 * path is stored once here and a span keeps its number. Number 0 is the
 * empty path, so a zeroed span has none. */
static struct {
    char **paths;
    int count;
    int capacity;
    int *slots; /* open addressing over path numbers; 0 is an empty slot */
    size_t slot_count;
} source_files;

static size_t
source_file_slot(const char *path)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(const unsigned char *p = (const unsigned char *)path; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    size_t slot = (size_t)hash & (source_files.slot_count - 1);
    while(source_files.slots[slot] != 0 &&
          strcmp(source_files.paths[source_files.slots[slot]], path) != 0)
        slot = (slot + 1) & (source_files.slot_count - 1);
    return slot;
}

int
SourceFile(const char *path)
{
    if(path == NULL || *path == '\0')
        return 0;
    if((size_t)source_files.count * 2 >= source_files.slot_count) {
        size_t old_count = source_files.slot_count;
        int *old_slots = source_files.slots;
        source_files.slot_count = old_count ? old_count * 2 : 256;
        source_files.slots = calloc(source_files.slot_count, sizeof(int));
        if(source_files.slots == NULL) {
            fprintf(stderr, "out of memory recording source paths\n");
            exit(1);
        }
        for(size_t i = 0; i < old_count; i++)
            if(old_slots[i] != 0)
                source_files.slots[source_file_slot(
                    source_files.paths[old_slots[i]])] = old_slots[i];
        free(old_slots);
    }
    if(source_files.count == 0)
        source_files.count = 1;
    size_t slot = source_file_slot(path);
    if(source_files.slots[slot] != 0)
        return source_files.slots[slot];
    if(source_files.count >= source_files.capacity) {
        int capacity = source_files.capacity ? source_files.capacity * 2 : 64;
        char **paths = realloc(source_files.paths, (size_t)capacity * sizeof(*paths));
        if(paths == NULL) {
            fprintf(stderr, "out of memory recording source paths\n");
            exit(1);
        }
        source_files.paths = paths;
        source_files.capacity = capacity;
    }
    char *copy = strdup(path);
    if(copy == NULL) {
        fprintf(stderr, "out of memory recording source paths\n");
        exit(1);
    }
    source_files.paths[source_files.count] = copy;
    source_files.slots[slot] = source_files.count;
    return source_files.count++;
}

const char *
SpanPath(ZirSourceSpan span)
{
    if(span.file <= 0 || span.file >= source_files.count)
        return "";
    return source_files.paths[span.file];
}

/* Statement and expression text is immutable and shared. Each distinct
 * text is stored once, packed into blocks that live as long as the
 * process, so copied nodes share it and reloading a program adds nothing.
 * Text keeps the ZIR_TEXT_MAX limit node buffers always had. */
static struct {
    const char **texts;
    size_t count;
    size_t slot_count;
    char *block;
    size_t block_used;
    size_t block_size;
} kept_texts;

static uint64_t
text_hash(const char *text, size_t length)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for(size_t i = 0; i < length; i++) {
        hash ^= (unsigned char)text[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static size_t
kept_text_slot(const char *text, size_t length)
{
    size_t slot = (size_t)text_hash(text, length) & (kept_texts.slot_count - 1);
    while(kept_texts.texts[slot] != NULL &&
          (strncmp(kept_texts.texts[slot], text, length) != 0 ||
           kept_texts.texts[slot][length] != '\0'))
        slot = (slot + 1) & (kept_texts.slot_count - 1);
    return slot;
}

/* Scratch memory the compiler cannot work without; there is no useful
 * way to continue when it is missing. */
void *
AllocateOrExit(size_t size)
{
    void *memory = malloc(size);
    if(memory == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return memory;
}

static void
kept_text_failed(void)
{
    fprintf(stderr, "out of memory keeping source text\n");
    exit(1);
}

const char *
KeepText(const char *text)
{
    if(text == NULL || *text == '\0')
        return "";
    size_t length = strnlen(text, ZIR_TEXT_MAX - 1);
    if(kept_texts.count * 2 >= kept_texts.slot_count) {
        size_t old_count = kept_texts.slot_count;
        const char **old_texts = kept_texts.texts;
        kept_texts.slot_count = old_count ? old_count * 2 : 4096;
        kept_texts.texts = calloc(kept_texts.slot_count, sizeof(*kept_texts.texts));
        if(kept_texts.texts == NULL)
            kept_text_failed();
        for(size_t i = 0; i < old_count; i++)
            if(old_texts[i] != NULL)
                kept_texts.texts[kept_text_slot(old_texts[i],
                                                strlen(old_texts[i]))] = old_texts[i];
        free(old_texts);
    }
    size_t slot = kept_text_slot(text, length);
    if(kept_texts.texts[slot] != NULL)
        return kept_texts.texts[slot];
    if(kept_texts.block == NULL ||
       kept_texts.block_used + length + 1 > kept_texts.block_size) {
        kept_texts.block_size = length + 1 > 65536 ? length + 1 : 65536;
        kept_texts.block = malloc(kept_texts.block_size);
        if(kept_texts.block == NULL)
            kept_text_failed();
        kept_texts.block_used = 0;
    }
    char *copy = kept_texts.block + kept_texts.block_used;
    memcpy(copy, text, length);
    copy[length] = '\0';
    kept_texts.block_used += length + 1;
    kept_texts.texts[slot] = copy;
    kept_texts.count++;
    return copy;
}

ZirSourceSpan
Span(const char *path, int line, int column)
{
    return SpanEnd(path, line, column, line, column);
}

ZirSourceSpan
SpanEnd(const char *path, int line, int column, int end_line, int end_column)
{
    ZirSourceSpan span;

    memset(&span, 0, sizeof(span));
    span.file = SourceFile(path);
    span.line = line;
    span.column = column;
    span.end_line = end_line;
    span.end_column = end_column;
    return span;
}

ZirModule *
ProgramAddModule(ZirProgram *program, const char *name,
                    const char *source_path, ZirSourceSpan span)
{
    ZirModule *modules;
    ZirModule *m;

    if(program == NULL)
        return NULL;
    modules = realloc_array(program->modules, &program->module_cap,
                                program->module_count, sizeof(ZirModule));
    if(modules == NULL)
        return NULL;
    program->modules = modules;
    m = &program->modules[program->module_count++];
    memset(m, 0, sizeof(*m));
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
    functions = realloc_array(module->functions, &module->function_cap,
                                  module->function_count, sizeof(ZirFunction));
    if(functions == NULL)
        return NULL;
    module->functions = functions;
    fn = &module->functions[module->function_count++];
    memset(fn, 0, sizeof(*fn));
    copy_text(fn->name, sizeof(fn->name), name);
    copy_text(fn->args, sizeof(fn->args), args);
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
    memset(st, 0, sizeof(*st));
    st->kind = kind;
    st->text = KeepText(text);
    st->expr_root = -1;
    st->lhs_root = -1;
    st->span = span;
    return st;
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
    memset(expr, 0, sizeof(*expr));
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
                    fn->name, fn->args, fn->return_type, fn->exported);
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

static char **generated_outputs;
static int generated_output_count;
static int generated_output_capacity;

static const char *
path_base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

void
GeneratedOutputRecord(const char *path)
{
    const char *name = path_base_name(path);
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
    static _Thread_local SameFileBytesBuffers *spares[16];
    static _Thread_local int spare_count;
    SameFileBytesBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = same_file_bytes_with_buffers(left, right, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
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

int
GeneratedOutputPrune(const char *out_dir, const char *marker)
{
    DIR *directory = opendir(out_dir);
    if(directory == NULL) return 0;
    int failed = 0;
    struct dirent *entry;
    while((entry = readdir(directory)) != NULL) {
        const char *name = entry->d_name;
        if(name[0] == '.') continue;
        int written = 0;
        for(int i = 0; i < generated_output_count && !written; i++)
            written = strcmp(generated_outputs[i], name) == 0;
        if(written) continue;
        char path[4096];
        int size = snprintf(path, sizeof(path), "%s/%s", out_dir, name);
        if(size < 0 || (size_t)size >= sizeof(path)) continue;
        if(starts_with_marker(path, marker) && remove(path) != 0)
            failed = 1;
    }
    closedir(directory);
    return failed ? -1 : 0;
}

int
same_type_application(const ZirModule *target_owner, const ZirType *target,
                      const ZirModule *source_owner, const ZirType *source)
{
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
                    const ZirType *a = FindType(target_owner, target_args[i], NULL);
                    const ZirType *b = FindType(source_owner, source_args[i], NULL);
                    same = a != NULL && a == b;
                }
            }
            if(same)
                return 1;
        }
    }
    return 0;
}
