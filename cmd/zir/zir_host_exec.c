/*
 * zir_host_exec.c - revision-28 executable ZX behind the embedding API.
 *
 * A bundle file is read and verified once and planned once; each instance
 * runs it on the portable instruction engine (generated from
 * cmd/compiler_runtime.zi). A host capability crosses as the engine's host
 * image: scalars and pointers as bits, everything else as the value's bytes
 * followed by the elements of each view inside it, a view's two words being
 * the offset of its elements and their count. The host interface section
 * names what an embedder sees, so each call is translated between that image
 * and VmHostValue trees by the rules the semantic VM follows: a value's type
 * is the spelling declared at its position, a spelling that starts with `u`
 * is unsigned, a returned record gives every declared field by name and in
 * order unless fields are matched by name.
 *
 * Bootstrap builds have no engine; they compile the stubs at the end.
 */
#include "zir_host_exec.h"

#ifndef ZIRAN_NO_ENGINE

#include "compiler_exec.h"
#include "compiler_exec_link.h"
#include "compiler_lowered_ir.h"
#include "zir_diagnostic.h"
#include "zir_exec_console.h"
#include "zir_text.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Platform console services ---------------------------------------- */

static int
exec_text_equal(String text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && !memcmp(text.data, expected, length);
}

int
ZxPlatformService(const LirProgram *program, int64_t function)
{
    if(function < 0 || function >= program->functions.length) return ZX_SERVICE_NONE;
    const LirFunction *declared = &((const LirFunction *)program->functions.data)[function];
    if(!declared->external || declared->module < 0 || declared->module >= program->modules.length ||
       !exec_text_equal(((const String *)program->modules.data)[declared->module], "ziran.platform"))
        return ZX_SERVICE_NONE;
    String name = declared->name;
    if(exec_text_equal(name, "ziran.platform.write_all")) return ZX_SERVICE_PRINT_BYTES;
    if(exec_text_equal(name, "ziran.print.signed")) return ZX_SERVICE_PRINT_SIGNED;
    if(exec_text_equal(name, "ziran.print.unsigned")) return ZX_SERVICE_PRINT_UNSIGNED;
    if(exec_text_equal(name, "ziran.print.bool")) return ZX_SERVICE_PRINT_BOOL;
    if(exec_text_equal(name, "ziran.print.float32")) return ZX_SERVICE_PRINT_FLOAT32;
    if(exec_text_equal(name, "ziran.print.float64")) return ZX_SERVICE_PRINT_FLOAT64;
    return ZX_SERVICE_NONE;
}

/* Output uses the same spelling as the checked language's print on every
 * other route. */
static bool
exec_console_write(void *closure, void *context, uint64_t descriptor, Slice bytes)
{
    (void)closure;
    (void)context;
    FILE *stream = descriptor == 2 ? stderr : descriptor == 1 ? stdout : NULL;
    if(stream == NULL) return false;
    return bytes.length == 0 || fwrite(bytes.data, 1, (size_t)bytes.length, stream) == (size_t)bytes.length;
}

static bool
exec_console_print(void *closure, void *context, int32_t function, Slice arguments, uint64_t *result)
{
    (void)closure;
    const LirProgram *program = context;
    const uint64_t *values = arguments.data;
    *result = 0;
    if(arguments.length != 1) return false;
    char text[64];
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

LirExecProblem
ZxBindConsole(LirExecInstance *instance, const LirProgram *program)
{
    for(int64_t index = 0; index < program->functions.length; index++) {
        int service = ZxPlatformService(program, index);
        LirExecProblem bound = LirExecProblem_None;
        if(service == ZX_SERVICE_PRINT_BYTES)
            bound = compiler_exec_LirExecBindPlatformWrite(instance, (int32_t)index,
                (LirExecPlatformWrite){NULL, exec_console_write}, NULL);
        else if(service != ZX_SERVICE_NONE)
            bound = compiler_exec_LirExecBindHost(instance, (int32_t)index,
                (LirExecHostCall){NULL, exec_console_print}, (void *)program);
        if(bound != LirExecProblem_None) return bound;
    }
    return LirExecProblem_None;
}

/* ---- Opened files ------------------------------------------------------- */

enum { NODE_SCALAR, NODE_STRING, NODE_POINTER, NODE_SLICE, NODE_ARRAY, NODE_RECORD };
enum { EXEC_DEPTH = 64 };

typedef struct ExecNode {
    int class;
    const char *spelling;
    int child;
    int field_count;
    const char **field_names;
    int *field_nodes;
} ExecNode;

typedef struct ExecHost {
    int32_t function;
    char *module;
    char *name;
    int result;
    int parameter_count;
    int *parameters;
} ExecHost;

struct ExecBundle {
    ZxExecOwner owner;
    LirProgram program;
    LirExecPlan plan;
    char *text;
    ExecNode *nodes;
    int node_count;
    ExecHost *hosts;
    size_t host_count;
    char *entry_module;
    char *entry_function;
};

static char *
exec_copy(String text)
{
    char *copy = malloc(text.length + 1);
    if(copy == NULL) return NULL;
    if(text.length) memcpy(copy, text.data, text.length);
    copy[text.length] = 0;
    return copy;
}

int
ExecBundleBytes(const unsigned char *bytes, size_t size)
{
    return size >= 8 && !memcmp(bytes, "ZX\0\0", 4) &&
        (uint32_t)(bytes[4] | bytes[5] << 8 | bytes[6] << 16 | (uint32_t)bytes[7] << 24) ==
            compiler_bundle_ExecutableVersionNumber();
}

/* Splits the next tab- or line-ended field off *cursor, ending it in place. */
static char *
exec_field(char **cursor, int *line_end)
{
    char *start = *cursor, *at = start;
    while(*at && *at != '\t' && *at != '\n') at++;
    *line_end = *at != '\t';
    if(*at) *at++ = 0;
    *cursor = at;
    return start;
}

static int
exec_number(const char *text, int *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if(end == text || *end || parsed < -1 || parsed > INT32_MAX) return 0;
    *value = (int)parsed;
    return 1;
}

/* Parses the host interface section. Every node names only earlier nodes. */
static int
exec_parse_interface(ExecBundle *bundle, ExecHost **hosts, int *host_lines)
{
    int lines = 0;
    for(const char *at = bundle->text; *at; at++) lines += *at == '\n';
    bundle->nodes = calloc((size_t)lines + 1, sizeof(*bundle->nodes));
    *hosts = calloc((size_t)lines + 1, sizeof(**hosts));
    if(bundle->nodes == NULL || *hosts == NULL) return 0;
    char *cursor = bundle->text;
    while(*cursor) {
        int end = 0;
        char *kind = exec_field(&cursor, &end);
        char *fields[2 * 256 + 4];
        int count = 0;
        while(!end && count < (int)(sizeof(fields) / sizeof(*fields))) fields[count++] = exec_field(&cursor, &end);
        if(!end) return 0;
        if(!strcmp(kind, "node")) {
            int id;
            if(count < 3 || !exec_number(fields[0], &id) || id != bundle->node_count) return 0;
            ExecNode *node = &bundle->nodes[bundle->node_count];
            node->spelling = fields[2];
            node->child = -1;
            const char *class = fields[1];
            int extra = count - 3;
            if(!strcmp(class, "scalar") || !strcmp(class, "string") || !strcmp(class, "pointer")) {
                if(extra != 0) return 0;
                node->class = !strcmp(class, "scalar") ? NODE_SCALAR : !strcmp(class, "string") ? NODE_STRING : NODE_POINTER;
            } else if(!strcmp(class, "slice") || !strcmp(class, "array")) {
                if(extra != 1 || !exec_number(fields[3], &node->child) || node->child < 0 || node->child >= id) return 0;
                node->class = !strcmp(class, "slice") ? NODE_SLICE : NODE_ARRAY;
            } else if(!strcmp(class, "record")) {
                if(extra % 2) return 0;
                node->class = NODE_RECORD;
                node->field_count = extra / 2;
                node->field_names = calloc((size_t)node->field_count + 1, sizeof(*node->field_names));
                node->field_nodes = calloc((size_t)node->field_count + 1, sizeof(*node->field_nodes));
                if(node->field_names == NULL || node->field_nodes == NULL) return 0;
                for(int field = 0; field < node->field_count; field++) {
                    node->field_names[field] = fields[3 + 2 * field];
                    if(!exec_number(fields[4 + 2 * field], &node->field_nodes[field]) ||
                       node->field_nodes[field] < 0 || node->field_nodes[field] >= id) return 0;
                }
            } else return 0;
            bundle->node_count++;
        } else if(!strcmp(kind, "host")) {
            ExecHost *host = &(*hosts)[*host_lines];
            if(count < 2 || !exec_number(fields[0], &host->function) || !exec_number(fields[1], &host->result) ||
               host->result >= bundle->node_count) return 0;
            host->parameter_count = count - 2;
            host->parameters = calloc((size_t)host->parameter_count + 1, sizeof(*host->parameters));
            if(host->parameters == NULL) return 0;
            for(int at = 0; at < host->parameter_count; at++)
                if(!exec_number(fields[2 + at], &host->parameters[at]) || host->parameters[at] < 0 ||
                   host->parameters[at] >= bundle->node_count) return 0;
            (*host_lines)++;
        } else return 0;
    }
    return 1;
}

void
ExecBundleClose(ExecBundle *bundle)
{
    if(bundle == NULL) return;
    compiler_exec_LirExecPlanFree(&bundle->plan);
    compiler_exec_link_ZxExecReadFree(&bundle->owner);
    for(int index = 0; index < bundle->node_count; index++) {
        free(bundle->nodes[index].field_names);
        free(bundle->nodes[index].field_nodes);
    }
    free(bundle->nodes);
    for(size_t index = 0; index < bundle->host_count; index++) {
        free(bundle->hosts[index].module);
        free(bundle->hosts[index].name);
        free(bundle->hosts[index].parameters);
    }
    free(bundle->hosts);
    free(bundle->text);
    free(bundle->entry_module);
    free(bundle->entry_function);
    free(bundle);
}

ExecBundle *
ExecBundleOpen(const unsigned char *bytes, size_t size, const char *name)
{
    ExecBundle *bundle = calloc(1, sizeof(*bundle));
    if(bundle == NULL) return NULL;
    ExecHost *lines = NULL;
    int line_count = 0;
    ZxExecResult read = compiler_exec_link_ZxExecRead(&bundle->owner, (String){(const char *)bytes, size},
        compiler_lowered_ir_LirDefaultLimits());
    if(read.problem != ZxExecProblem_None) {
        String problem = compiler_exec_link_ZxExecProblemName(read.problem);
        String container = compiler_bundle_ExecutableMessage(read.container);
        Diagnostic(Span(name, 1, 1), "zx.input",
            "executable ZX rejected: problem=%.*s container=%.*s codec=%d offset=%lld verifier=%d",
            (int)problem.length, problem.data, (int)container.length, container.data,
            read.codec, (long long)read.offset, read.diagnostic.problem);
        goto failed;
    }
    bundle->program = compiler_exec_link_ZxExecReadProgram(&bundle->owner);
    LirExecResult planned = compiler_exec_LirExecPlanBuild(bundle->program, compiler_lowered_ir_LirDefaultLimits(), &bundle->plan);
    if(planned.problem != LirExecProblem_None) {
        String problem = compiler_exec_LirExecProblemName(planned.problem);
        Diagnostic(Span(name, 1, 1), "zx.input", "executable ZX cannot run: problem=%.*s",
            (int)problem.length, problem.data);
        goto failed;
    }
    const LirFunction *functions = bundle->program.functions.data;
    const String *modules = bundle->program.modules.data;
    const LirFunction *entry = &functions[bundle->program.entry];
    bundle->entry_module = exec_copy(modules[entry->module]);
    bundle->entry_function = exec_copy(entry->name);
    bundle->text = malloc((size_t)bundle->owner.interface.count + 1);
    if(bundle->entry_module == NULL || bundle->entry_function == NULL || bundle->text == NULL) goto failed;
    if(bundle->owner.interface.count) memcpy(bundle->text, bundle->owner.interface.data, (size_t)bundle->owner.interface.count);
    bundle->text[bundle->owner.interface.count] = 0;
    if(!exec_parse_interface(bundle, &lines, &line_count)) {
        Diagnostic(Span(name, 1, 1), "zx.input", "executable ZX has an invalid host interface section; rebuild it");
        goto failed;
    }
    bundle->hosts = calloc((size_t)bundle->program.functions.length + 1, sizeof(*bundle->hosts));
    if(bundle->hosts == NULL) goto failed;
    for(int64_t index = 0; index < bundle->program.functions.length; index++) {
        const LirFunction *function = &functions[index];
        if(!function->external || function->calling != LirCalling_Host ||
           ZxPlatformService(&bundle->program, index) != ZX_SERVICE_NONE) continue;
        ExecHost *found = NULL;
        for(int line = 0; line < line_count && found == NULL; line++)
            if(lines[line].function == index) found = &lines[line];
        ExecHost *host = &bundle->hosts[bundle->host_count++];
        host->function = (int32_t)index;
        host->module = exec_copy(modules[function->module]);
        host->name = exec_copy(function->name);
        if(host->module == NULL || host->name == NULL) goto failed;
        if(found == NULL || found->parameter_count != function->parameters.length ||
           (found->result < 0) != (((const LirType *)bundle->program.types.data)[function->result].kind == LirTypeKind_Void)) {
            Diagnostic(Span(name, 1, 1), "zx.input",
                "executable ZX names no host interface for capability %s:%s; rebuild it with zi2asm --emit zx",
                host->module, host->name);
            goto failed;
        }
        host->result = found->result;
        host->parameter_count = found->parameter_count;
        host->parameters = found->parameters;
        found->parameters = NULL;
    }
    for(int line = 0; line < line_count; line++) free(lines[line].parameters);
    free(lines);
    return bundle;
failed:
    for(int line = 0; lines != NULL && line < line_count; line++) free(lines[line].parameters);
    free(lines);
    ExecBundleClose(bundle);
    return NULL;
}

size_t ExecBundleCapabilityCount(const ExecBundle *bundle) { return bundle->host_count; }

const char *
ExecBundleCapabilityModule(const ExecBundle *bundle, size_t index)
{
    return index < bundle->host_count ? bundle->hosts[index].module : NULL;
}

const char *
ExecBundleCapabilityFunction(const ExecBundle *bundle, size_t index)
{
    return index < bundle->host_count ? bundle->hosts[index].name : NULL;
}

const char *ExecBundleEntryModule(const ExecBundle *bundle) { return bundle->entry_module; }
const char *ExecBundleEntryFunction(const ExecBundle *bundle) { return bundle->entry_function; }

/* ---- Instances ---------------------------------------------------------- */

typedef struct ExecCall {
    ExecInstance *owner;
    size_t host;
} ExecCall;

typedef struct ExecBlock {
    struct ExecBlock *next;
    max_align_t align[];
} ExecBlock;

struct ExecInstance {
    const ExecBundle *bundle;
    LirExecInstance instance;
    const HostBinding **bound;
    ExecCall *calls;
    /* Each host call's argument values, freed when it returns. */
    ExecBlock *blocks;
    /* The result image of the latest host call. */
    unsigned char *image;
    size_t image_length, image_capacity;
    long long budget;
    int match_fields_by_name;
};

static void *
exec_arena(ExecInstance *instance, size_t count, size_t width)
{
    if(width && count > (SIZE_MAX - sizeof(ExecBlock)) / width) return NULL;
    ExecBlock *block = calloc(1, sizeof(*block) + (count ? count : 1) * width);
    if(block == NULL) return NULL;
    block->next = instance->blocks;
    instance->blocks = block;
    return block->align;
}

static void
exec_arena_free(ExecInstance *instance)
{
    while(instance->blocks) {
        ExecBlock *next = instance->blocks->next;
        free(instance->blocks);
        instance->blocks = next;
    }
}

static const LirType *
exec_type(const ExecBundle *bundle, int32_t type)
{
    return &((const LirType *)bundle->program.types.data)[type];
}

static int64_t
exec_size(const ExecBundle *bundle, int32_t type)
{
    return bundle->plan.type_sizes.data[type];
}

static uint64_t
exec_read(const unsigned char *bytes, int64_t count)
{
    uint64_t value = 0;
    for(int64_t at = 0; at < count; at++) value |= (uint64_t)bytes[at] << (8 * at);
    return value;
}

static void
exec_write(unsigned char *bytes, int64_t count, uint64_t value)
{
    for(int64_t at = 0; at < count; at++) bytes[at] = (unsigned char)(value >> (8 * at));
}

/* The element type of a view type {data: *T, count: s64}, or -1. */
static int32_t
exec_view_element(const ExecBundle *bundle, int32_t type)
{
    const LirType *view = exec_type(bundle, type);
    if(view->kind != LirTypeKind_Record || view->fields.length != 2) return -1;
    const LirType *pointer = exec_type(bundle, ((const int32_t *)view->fields.data)[0]);
    return pointer->kind == LirTypeKind_Pointer ? pointer->element : -1;
}

/* A scalar's bits as the semantic VM gives them to a host. */
static int
exec_scalar_value(const ExecNode *node, const LirType *type, uint64_t bits, VmHostValue *out)
{
    out->type = node->spelling;
    if(type->kind == LirTypeKind_Float) {
        out->kind = VM_HOST_REAL;
        if(type->bits == 32) {
            uint32_t narrow = (uint32_t)bits;
            float value;
            memcpy(&value, &narrow, sizeof(value));
            out->real = value;
        } else memcpy(&out->real, &bits, sizeof(out->real));
        return 1;
    }
    if(type->kind != LirTypeKind_Integer && type->kind != LirTypeKind_Bool) return 0;
    int width = type->kind == LirTypeKind_Bool ? 1 : type->bits;
    int64_t integer = (int64_t)bits;
    if(type->kind == LirTypeKind_Integer && type->is_signed && width < 64 && (bits >> (width - 1)) & 1)
        integer = (int64_t)(bits | ~((UINT64_C(1) << width) - 1));
    out->kind = node->spelling[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    out->integer = integer;
    out->bits = bits;
    return 1;
}

/* Reads the `type` value at image[at] into *out, as `node` names it. */
static int
exec_from_image(ExecInstance *instance, int node_id, int32_t type, const unsigned char *image, int64_t size,
                int64_t at, VmHostValue *out, int depth)
{
    const ExecBundle *bundle = instance->bundle;
    const ExecNode *node = &bundle->nodes[node_id];
    const LirType *shape = exec_type(bundle, type);
    int64_t width = exec_size(bundle, type);
    if(depth > EXEC_DEPTH || at < 0 || width < 0 || at > size || width > size - at) return 0;
    out->type = node->spelling;
    if(node->class == NODE_SCALAR) return exec_scalar_value(node, shape, exec_read(image + at, width), out);
    if(node->class == NODE_POINTER) {
        if(shape->kind != LirTypeKind_Pointer) return 0;
        out->kind = VM_HOST_POINTER;
        out->pointer = (void *)(uintptr_t)exec_read(image + at, 8);
        return 1;
    }
    if(node->class == NODE_ARRAY) {
        if(shape->kind != LirTypeKind_Array) return 0;
        int64_t element = exec_size(bundle, shape->element);
        out->kind = VM_HOST_ARRAY;
        out->length = (size_t)shape->count;
        if(shape->count == 0) return 1;
        out->elements = exec_arena(instance, (size_t)shape->count, sizeof(*out->elements));
        if(out->elements == NULL) return 0;
        for(int64_t index = 0; index < shape->count; index++)
            if(!exec_from_image(instance, node->child, shape->element, image, size, at + index * element,
                                &out->elements[index], depth + 1)) return 0;
        return 1;
    }
    if(node->class == NODE_RECORD) {
        if(shape->kind != LirTypeKind_Record || shape->fields.length != node->field_count) return 0;
        VmHostField *fields = exec_arena(instance, (size_t)node->field_count, sizeof(*fields));
        if(fields == NULL) return 0;
        out->kind = VM_HOST_RECORD;
        out->fields = fields;
        out->field_count = (size_t)node->field_count;
        int64_t offset = at;
        for(int index = 0; index < node->field_count; index++) {
            int32_t field = ((const int32_t *)shape->fields.data)[index];
            fields[index].name = node->field_names[index];
            if(!exec_from_image(instance, node->field_nodes[index], field, image, size, offset,
                                &fields[index].value, depth + 1)) return 0;
            offset += exec_size(bundle, field);
        }
        return 1;
    }
    /* A string or slice: its two words, then its elements in the image. */
    int32_t element = exec_view_element(bundle, type);
    if(element < 0 || width != 16) return 0;
    uint64_t start = exec_read(image + at, 8), count = exec_read(image + at + 8, 8);
    int64_t element_width = exec_size(bundle, element);
    if(element_width <= 0 || start > (uint64_t)size ||
       count > (uint64_t)((size - (int64_t)start) / element_width)) return 0;
    if(node->class == NODE_STRING) {
        if(element_width != 1) return 0;
        out->kind = VM_HOST_STRING;
        out->data = count ? image + start : NULL;
        out->length = (size_t)count;
        return 1;
    }
    out->kind = VM_HOST_SLICE;
    out->length = (size_t)count;
    out->elements = exec_arena(instance, (size_t)count, sizeof(*out->elements));
    if(out->elements == NULL) return 0;
    for(uint64_t index = 0; index < count; index++)
        if(!exec_from_image(instance, node->child, element, image, size, (int64_t)start + (int64_t)index * element_width,
                            &out->elements[index], depth + 1)) return 0;
    return 1;
}

/* Room for `count` more zero bytes at the end of the result image; returns
 * their offset, or -1. */
static int64_t
exec_image_grow(ExecInstance *instance, int64_t count)
{
    if(count < 0 || (size_t)count > SIZE_MAX / 2 - instance->image_length) return -1;
    size_t needed = instance->image_length + (size_t)count;
    if(needed > instance->image_capacity) {
        size_t capacity = instance->image_capacity ? instance->image_capacity : 256;
        while(capacity < needed) capacity *= 2;
        unsigned char *grown = realloc(instance->image, capacity);
        if(grown == NULL) return -1;
        instance->image = grown;
        instance->image_capacity = capacity;
    }
    int64_t at = (int64_t)instance->image_length;
    memset(instance->image + at, 0, (size_t)count);
    instance->image_length = needed;
    return at;
}

/* Whether a host names a returned value's type as declared: the same
 * spelling, or one that differs only by a module qualifier, element by
 * element through arrays, slices and pointers. */
static int
exec_flat_type_matches(const char *qualified, const char *plain)
{
    while(*qualified != '\0' && *plain != '\0') {
        if(*qualified == '.') {
            qualified++;
            continue;
        }
        if(*qualified++ != *plain++) return 0;
    }
    while(*qualified == '.') qualified++;
    return *qualified == '\0' && *plain == '\0';
}

static int
exec_type_matches(const char *expected, const char *actual, int depth)
{
    if(depth >= EXEC_DEPTH || expected == NULL || actual == NULL) return 0;
    if(strcmp(expected, actual) == 0) return 1;
    if(expected[0] == '[' && actual[0] == '[') {
        const char *expected_close = strchr(expected, ']');
        const char *actual_close = strchr(actual, ']');
        if(expected_close == NULL || actual_close == NULL) return 0;
        size_t expected_prefix = (size_t)(expected_close - expected) + 1;
        size_t actual_prefix = (size_t)(actual_close - actual) + 1;
        return expected_prefix == actual_prefix && memcmp(expected, actual, expected_prefix) == 0 &&
               exec_type_matches(expected_close + 1, actual_close + 1, depth + 1);
    }
    if(expected[0] == '*' && actual[0] == '*') return exec_type_matches(expected + 1, actual + 1, depth + 1);
    const char *expected_dot = strrchr(expected, '.');
    const char *actual_dot = strrchr(actual, '.');
    const char *expected_leaf = expected_dot != NULL ? expected_dot + 1 : expected;
    const char *actual_leaf = actual_dot != NULL ? actual_dot + 1 : actual;
    if(expected_dot != NULL && actual_dot != NULL) return 0;
    if(strcmp(expected_leaf, actual_leaf) == 0) return 1;
    if(expected_dot != NULL) return exec_flat_type_matches(expected, actual);
    if(actual_dot != NULL) return exec_flat_type_matches(actual, expected);
    return 0;
}

/* A scalar or pointer result's canonical bits, by the rules of the semantic
 * VM's host returns: the host gives the kind its spelling selects. */
static int
exec_scalar_bits(const ExecNode *node, const LirType *type, const VmHostValue *value, uint64_t *bits)
{
    if(value->field_count != 0 || value->fields != NULL ||
       !exec_type_matches(node->spelling, value->type, 0)) return 0;
    if(node->class == NODE_POINTER) {
        if(type->kind != LirTypeKind_Pointer || value->kind != VM_HOST_POINTER) return 0;
        *bits = (uint64_t)(uintptr_t)value->pointer;
        return 1;
    }
    if(type->kind == LirTypeKind_Float) {
        if(value->kind != VM_HOST_REAL) return 0;
        if(type->bits == 32) {
            float narrow = (float)value->real;
            uint32_t word;
            memcpy(&word, &narrow, sizeof(word));
            *bits = word;
        } else memcpy(bits, &value->real, sizeof(*bits));
        return 1;
    }
    if(type->kind != LirTypeKind_Integer && type->kind != LirTypeKind_Bool) return 0;
    VmHostValueKind expected = node->spelling[0] == 'u' ? VM_HOST_UNSIGNED : VM_HOST_INTEGER;
    if(value->kind != expected) return 0;
    uint64_t raw = expected == VM_HOST_UNSIGNED ? value->bits : (uint64_t)value->integer;
    if(type->kind == LirTypeKind_Bool) *bits = raw != 0;
    else *bits = type->bits < 64 ? raw & ((UINT64_C(1) << type->bits) - 1) : raw;
    return 1;
}

static const VmHostField *
exec_field_named(const VmHostValue *value, const char *name)
{
    for(size_t index = 0; index < value->field_count; index++)
        if(value->fields[index].name != NULL && !strcmp(value->fields[index].name, name))
            return &value->fields[index];
    return NULL;
}

/* Writes the host's `value` as the `type` value at image[at], appending the
 * elements of its views. */
static int
exec_to_image(ExecInstance *instance, int node_id, int32_t type, const VmHostValue *value, int64_t at, int depth)
{
    const ExecBundle *bundle = instance->bundle;
    const ExecNode *node = &bundle->nodes[node_id];
    const LirType *shape = exec_type(bundle, type);
    int64_t width = exec_size(bundle, type);
    if(depth > EXEC_DEPTH || width < 0 || !exec_type_matches(node->spelling, value->type, 0)) return 0;
    if(node->class == NODE_SCALAR || node->class == NODE_POINTER) {
        uint64_t bits;
        if(!exec_scalar_bits(node, shape, value, &bits)) return 0;
        exec_write(instance->image + at, width, bits);
        return 1;
    }
    if(node->class == NODE_ARRAY) {
        if(shape->kind != LirTypeKind_Array || value->kind != VM_HOST_ARRAY ||
           value->length != (size_t)shape->count || (shape->count && value->elements == NULL)) return 0;
        int64_t element = exec_size(bundle, shape->element);
        for(int64_t index = 0; index < shape->count; index++)
            if(!exec_to_image(instance, node->child, shape->element, &value->elements[index],
                              at + index * element, depth + 1)) return 0;
        return 1;
    }
    if(node->class == NODE_RECORD) {
        if(shape->kind != LirTypeKind_Record || shape->fields.length != node->field_count ||
           value->kind != VM_HOST_RECORD || (value->field_count && value->fields == NULL)) return 0;
        if(!instance->match_fields_by_name && value->field_count != (size_t)node->field_count) return 0;
        int64_t offset = at;
        for(int index = 0; index < node->field_count; index++) {
            int32_t field = ((const int32_t *)shape->fields.data)[index];
            const VmHostField *given = NULL;
            if(instance->match_fields_by_name) given = exec_field_named(value, node->field_names[index]);
            else if(value->fields[index].name != NULL && !strcmp(value->fields[index].name, node->field_names[index]))
                given = &value->fields[index];
            else return 0;
            /* A field the host does not give keeps its zero value. */
            if(given != NULL && !exec_to_image(instance, node->field_nodes[index], field, &given->value, offset, depth + 1))
                return 0;
            offset += exec_size(bundle, field);
        }
        return 1;
    }
    int32_t element = exec_view_element(bundle, type);
    int64_t element_width = element >= 0 ? exec_size(bundle, element) : -1;
    if(element < 0 || width != 16 || element_width <= 0) return 0;
    size_t count = value->length;
    if(node->class == NODE_STRING) {
        if(value->kind != VM_HOST_STRING || element_width != 1 || (count && value->data == NULL)) return 0;
        if(count == 0) return 1;
        int64_t start = exec_image_grow(instance, (int64_t)count);
        if(start < 0) return 0;
        memcpy(instance->image + start, value->data, count);
        exec_write(instance->image + at, 8, (uint64_t)start);
        exec_write(instance->image + at + 8, 8, (uint64_t)count);
        return 1;
    }
    if(value->kind != VM_HOST_SLICE || (count && value->elements == NULL) ||
       count > (size_t)(INT64_MAX / element_width)) return 0;
    if(count == 0) return 1;
    int64_t start = exec_image_grow(instance, (int64_t)count * element_width);
    if(start < 0) return 0;
    exec_write(instance->image + at, 8, (uint64_t)start);
    exec_write(instance->image + at + 8, 8, (uint64_t)count);
    for(size_t index = 0; index < count; index++)
        if(!exec_to_image(instance, node->child, element, &value->elements[index],
                          start + (int64_t)index * element_width, depth + 1)) return 0;
    return 1;
}

static bool
exec_host_call(void *closure, void *context, int32_t function, Slice arguments, LirExecArgument *result)
{
    (void)closure;
    ExecCall *call = context;
    ExecInstance *instance = call->owner;
    const ExecBundle *bundle = instance->bundle;
    const ExecHost *host = &bundle->hosts[call->host];
    const LirExecArgument *given = arguments.data;
    bool ok = false;
    if(host->function != function || arguments.length != host->parameter_count) return false;
    VmHostValue *values = exec_arena(instance, (size_t)arguments.length, sizeof(*values));
    if(values == NULL) goto done;
    for(int64_t index = 0; index < arguments.length; index++) {
        const ExecNode *node = &bundle->nodes[host->parameters[index]];
        const LirType *type = exec_type(bundle, given[index].type);
        if(node->class == NODE_SCALAR) {
            if(!exec_scalar_value(node, type, given[index].bits, &values[index])) goto done;
        } else if(node->class == NODE_POINTER) {
            if(type->kind != LirTypeKind_Pointer) goto done;
            values[index].kind = VM_HOST_POINTER;
            values[index].type = node->spelling;
            values[index].pointer = (void *)(uintptr_t)given[index].bits;
        } else if(!exec_from_image(instance, host->parameters[index], given[index].type, given[index].bytes.data,
                                   given[index].bytes.length, 0, &values[index], 0)) goto done;
    }
    const HostBinding *binding = instance->bound[call->host];
    /* As on the semantic VM, the result starts named as declared, so a host
     * may leave its type as it finds it. */
    VmHostValue returned;
    memset(&returned, 0, sizeof(returned));
    returned.type = host->result >= 0 ? bundle->nodes[host->result].spelling : "void";
    if(!binding->call(binding->context, host->module, host->name, values, (int)arguments.length, &returned)) goto done;
    const LirFunction *declared = &((const LirFunction *)bundle->program.functions.data)[function];
    int32_t type = declared->result;
    result->type = type;
    result->bits = 0;
    result->bytes = (Slice){NULL, 0};
    if(host->result < 0) {
        ok = returned.kind == VM_HOST_VOID;
        goto done;
    }
    const ExecNode *node = &bundle->nodes[host->result];
    if(node->class == NODE_SCALAR || node->class == NODE_POINTER) {
        ok = exec_scalar_bits(node, exec_type(bundle, type), &returned, &result->bits);
        goto done;
    }
    instance->image_length = 0;
    if(exec_image_grow(instance, exec_size(bundle, type)) < 0 ||
       !exec_to_image(instance, host->result, type, &returned, 0, 0)) goto done;
    result->bytes = (Slice){instance->image, (int64_t)instance->image_length};
    ok = true;
done:
    exec_arena_free(instance);
    return ok;
}

ExecInstance *
ExecInstanceOpen(const ExecBundle *bundle, ExecFindBinding find, void *context, long long max_steps)
{
    static unsigned identities;
    ExecInstance *instance = calloc(1, sizeof(*instance));
    if(instance == NULL) return NULL;
    instance->bundle = bundle;
    instance->budget = max_steps > 0 ? max_steps : -1;
    instance->bound = calloc(bundle->host_count + 1, sizeof(*instance->bound));
    instance->calls = calloc(bundle->host_count + 1, sizeof(*instance->calls));
    if(instance->bound == NULL || instance->calls == NULL) goto failed;
    for(size_t index = 0; index < bundle->host_count; index++) {
        instance->bound[index] = find(context, bundle->hosts[index].module, bundle->hosts[index].name);
        if(instance->bound[index] == NULL) {
            Diagnostic((ZirSourceSpan){0}, "zx.capability", "host capability %s:%s is not bound",
                bundle->hosts[index].module, bundle->hosts[index].name);
            goto failed;
        }
    }
    LirExecLimits limits = compiler_exec_LirExecDefaultLimits();
    limits.work = INT64_MAX;
    limits.fuel = INT64_MAX;
    limits.frames = 1000000;
    unsigned identity = ++identities;
    if(identity == 0) identity = ++identities;
    LirExecResult opened = compiler_exec_LirExecInstanceOpen((LirExecPlan *)&bundle->plan, limits, identity, &instance->instance);
    LirExecProblem bound = opened.problem == LirExecProblem_None ?
        ZxBindConsole(&instance->instance, &bundle->program) : opened.problem;
    for(size_t index = 0; index < bundle->host_count && bound == LirExecProblem_None; index++) {
        instance->calls[index] = (ExecCall){instance, index};
        bound = compiler_exec_LirExecBindHostTyped(&instance->instance, bundle->hosts[index].function,
            (LirExecTypedHostCall){NULL, exec_host_call}, &instance->calls[index]);
    }
    if(bound != LirExecProblem_None) {
        String problem = compiler_exec_LirExecProblemName(bound);
        Diagnostic((ZirSourceSpan){0}, "zx.capability", "cannot start the instance: problem=%.*s",
            (int)problem.length, problem.data);
        goto failed;
    }
    return instance;
failed:
    ExecInstanceClose(instance);
    return NULL;
}

int
ExecInstanceRun(ExecInstance *instance, long long *result, int *has_result)
{
    const ExecBundle *bundle = instance->bundle;
    if(instance->budget == 0) return 0;
    instance->instance.limits.fuel = instance->budget > 0 ? instance->budget : INT64_MAX;
    LirExecResult ran = compiler_exec_LirExecRun(&instance->instance);
    if(instance->budget > 0) {
        int64_t used = instance->instance.limits.fuel - ran.fuel_remaining;
        instance->budget = used >= instance->budget ? 0 : instance->budget - used;
    }
    fflush(stdout);
    if(ran.problem != LirExecProblem_None) {
        String problem = compiler_exec_LirExecProblemName(ran.problem);
        Diagnostic((ZirSourceSpan){0}, "zx.runtime",
            "portable execution failed in %s: problem=%.*s function=%d block=%d instruction=%d",
            bundle->entry_function, (int)problem.length, problem.data, ran.function, ran.block, ran.instruction);
        return 0;
    }
    const LirFunction *entry = &((const LirFunction *)bundle->program.functions.data)[bundle->program.entry];
    const LirType *type = exec_type(bundle, entry->result);
    int returns = type->kind != LirTypeKind_Void;
    if(has_result != NULL) *has_result = returns;
    if(result != NULL) {
        uint64_t bits = ran.value.bits;
        if(type->kind == LirTypeKind_Integer && type->is_signed && type->bits < 64 && (bits >> (type->bits - 1)) & 1)
            bits |= ~((UINT64_C(1) << type->bits) - 1);
        *result = returns ? (long long)bits : 0;
    }
    return 1;
}

void
ExecInstanceLimitSteps(ExecInstance *instance, long long max_steps)
{
    if(instance != NULL) instance->budget = max_steps > 0 ? max_steps : -1;
}

void
ExecInstanceMatchFieldsByName(ExecInstance *instance, int enabled)
{
    if(instance != NULL) instance->match_fields_by_name = enabled != 0;
}

void
ExecInstanceClose(ExecInstance *instance)
{
    if(instance == NULL) return;
    compiler_exec_LirExecInstanceClose(&instance->instance);
    exec_arena_free(instance);
    free(instance->image);
    free(instance->bound);
    free(instance->calls);
    free(instance);
}

#else

#include "zir_diagnostic.h"

int ExecBundleBytes(const unsigned char *bytes, size_t size) { (void)bytes; (void)size; return 0; }
ExecBundle *ExecBundleOpen(const unsigned char *bytes, size_t size, const char *name)
{
    (void)bytes;
    (void)size;
    Diagnostic(Span(name, 1, 1), "zx.input", "this build has no instruction engine");
    return NULL;
}
void ExecBundleClose(ExecBundle *bundle) { (void)bundle; }
size_t ExecBundleCapabilityCount(const ExecBundle *bundle) { (void)bundle; return 0; }
const char *ExecBundleCapabilityModule(const ExecBundle *bundle, size_t index) { (void)bundle; (void)index; return NULL; }
const char *ExecBundleCapabilityFunction(const ExecBundle *bundle, size_t index) { (void)bundle; (void)index; return NULL; }
const char *ExecBundleEntryModule(const ExecBundle *bundle) { (void)bundle; return NULL; }
const char *ExecBundleEntryFunction(const ExecBundle *bundle) { (void)bundle; return NULL; }
ExecInstance *ExecInstanceOpen(const ExecBundle *bundle, ExecFindBinding find, void *context, long long max_steps)
{
    (void)bundle; (void)find; (void)context; (void)max_steps;
    return NULL;
}
int ExecInstanceRun(ExecInstance *instance, long long *result, int *has_result)
{
    (void)instance; (void)result; (void)has_result;
    return 0;
}
void ExecInstanceLimitSteps(ExecInstance *instance, long long max_steps) { (void)instance; (void)max_steps; }
void ExecInstanceMatchFieldsByName(ExecInstance *instance, int enabled) { (void)instance; (void)enabled; }
void ExecInstanceClose(ExecInstance *instance) { (void)instance; }

#endif
