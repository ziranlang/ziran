#include "zir.h"
#include "compiler_type.h"
#include <string.h>

/* IR storage and C strings stay at this boundary. All spelling, field,
 * container, and foreign-symbol rules come from cmd/compiler_type.zi. */
static String type_text(const char *source)
{
    return source == NULL ? (String){0} : StringView(source, strlen(source));
}

static int copy_part(char *output, size_t capacity, String part)
{
    if(output == NULL) return 1;
    if(part.length < 0 || (uint64_t)part.length >= capacity) return 0;
    if(part.length > 0) memcpy(output, part.data, (size_t)part.length);
    output[part.length] = '\0';
    return 1;
}

int TypeNextField(const ZirType *record, size_t *offset, ZirTypeField *field)
{
    field->name[0] = field->type[0] = field->go_tag[0] = '\0';
    field->is_using = 0;
    if(record->is_enum || record->is_procedure_type || *offset > sizeof(record->body))
        return -1;
    unsigned char tag_workspace[sizeof(field->go_tag)];
    RecordField result = compiler_type_NextField(
        StringView(record->body, sizeof(record->body)), (int64_t)*offset,
        record->is_union != 0, sizeof(field->name), sizeof(field->type),
        (Slice){tag_workspace, sizeof(tag_workspace)});
    *offset = (size_t)result.next;
    if(result.status == 1) {
        copy_part(field->name, sizeof(field->name), result.name);
        copy_part(field->type, sizeof(field->type), result.type);
        copy_part(field->go_tag, sizeof(field->go_tag), result.tag);
        field->is_using = result.is_using;
    }
    return result.status;
}

int SliceElementType(const char *type, char *element, size_t element_size)
{
    ElementType result = compiler_type_SliceElement(type_text(type));
    return result.valid && copy_part(element, element_size, result.element);
}

int ArrayElementType(const char *type, char *element, size_t element_size, int *capacity)
{
    ElementType result = compiler_type_ArrayElement(type_text(type));
    if(!result.valid || !copy_part(element, element_size, result.element)) return 0;
    if(capacity != NULL) *capacity = (int)result.capacity;
    return 1;
}

int BuiltinTypeName(const char *name)
{
    return compiler_type_BuiltinName(type_text(name));
}

int MapPrimitiveName(const char *name)
{
    return compiler_type_MapPrimitive(type_text(name));
}

int GoForeignCallParts(const char *target, char *package, size_t package_size,
                      char *receiver, size_t receiver_size, char *symbol, size_t symbol_size)
{
    ForeignCall result = compiler_type_GoCall(type_text(target), ZIR_NAME_MAX);
    if(result.kind == 0 || !copy_part(package, package_size, result.module) ||
       !copy_part(receiver, receiver_size, result.receiver) ||
       !copy_part(symbol, symbol_size, result.symbol)) return 0;
    return result.kind;
}

int GoForeignTargetValid(const char *target)
{
    return compiler_type_GoTypeTarget(type_text(target));
}

int GoCHeader(const char *package, char *header, size_t header_size)
{
    if(strncmp(package, "C/", 2) || !package[2]) return 0;
    const char *path = package + 2;
    const char *segment = path;
    for(const char *p = path; ; p++) {
        if(*p == '/' || *p == '\0') {
            size_t length = (size_t)(p - segment);
            if(length == 0 || (length == 1 && segment[0] == '.') ||
               (length == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if(!*p) break;
            segment = p + 1;
        } else if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.')) {
            return 0;
        }
    }
    return copy_part(header, header_size, type_text(path));
}

int PyForeignCallParts(const char *target, char *module, size_t module_size,
                      char *receiver, size_t receiver_size, char *symbol, size_t symbol_size)
{
    ForeignCall result = compiler_type_PythonCall(type_text(target));
    if(result.kind == 0 || !copy_part(module, module_size, result.module) ||
       !copy_part(receiver, receiver_size, result.receiver) ||
       !copy_part(symbol, symbol_size, result.symbol)) return 0;
    return result.kind;
}

int PyForeignTargetValid(const char *target)
{
    return compiler_type_PythonCall(type_text(target)).kind == 1;
}
