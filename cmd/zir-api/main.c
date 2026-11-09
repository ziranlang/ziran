#include "zir.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_text.h"
#include "zir_document.h"
#include "zir_semantic.h"
#include "zir_law.h"
#include "zir_stream.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One invocation renders to a private stream, then publishes only a complete
 * JSON value that fits the requested byte budget (including its metadata). */
static FILE *api_output;
#define printf(...) fprintf(api_output, __VA_ARGS__)
#define putchar(c) fputc((c), api_output)
static const char *filter_module, *filter_symbol;
static int imports_only, symbol_limit = 256, emitted, matched;
static size_t byte_limit = 65536;

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
          "usage: ziran api [--json] [--diagnostics=text|json] "
          "[--module NAME] [--symbol NAME|ID] [--imports-only] [--max-symbols N] [--max-bytes N] [--module-path DIR] [--define NAME] --root DIR file.zi|file.zir ...");
}

static void
json_string(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");

    putchar('"');
    for(; *cursor != '\0'; cursor++) {
        if(*cursor == '"' || *cursor == '\\') {
            putchar('\\');
            putchar(*cursor);
        } else if(*cursor < 0x20) {
            printf("\\u%04x", *cursor);
        } else {
            putchar(*cursor);
        }
    }
    putchar('"');
}

static void
json_member(const char *key, const char *value)
{
    json_string(key);
    putchar(':');
    json_string(value);
}

static int take_symbol(const ZirModule *module, const char *kind, const char *name,
                       const char *signature, const char *result)
{
    char id[68], operator_name[16];
    if(!strcmp(kind, "function") && OperatorOfProcedure(name)) {
        snprintf(operator_name, sizeof(operator_name), "operator %s", OperatorOfProcedure(name));
        name = operator_name;
    }
    if(imports_only && strcmp(kind, "import")) return 0;
    SemanticId(module, kind, name, signature, result, id);
    if(filter_symbol && strcmp(filter_symbol, name) && strcmp(filter_symbol, id)) return 0;
    matched++;
    if(emitted >= symbol_limit) return 0;
    emitted++;
    return 1;
}
static void document_json(const ZirDocument *document)
{
    if(document == NULL) { fputs("{\"available\":false,\"sha256\":null,\"bytes\":null}", api_output); return; }
    fputs("{\"available\":true,", api_output);
    json_member("sha256", document->sha256);
    printf(",\"bytes\":%zu,\"rewritten\":%s}", document->bytes, document->rewritten ? "true" : "false");
}
static void symbol_metadata(const ZirModule *module, const char *kind, const char *name,
                            const char *signature, const char *result, ZirSourceSpan span)
{
    char id[68]; size_t begin, end;
    const ZirDocument *document = DocumentForSpan(module, span);
    SemanticId(module, kind, name, signature, result, id);
    putchar(','); json_member("id", id);
    putchar(','); json_member("symbol_kind", kind);
    fputs(",\"document\":", api_output); document_json(document);
    fputs(",\"span\":{\"kind\":\"declaration_anchor\",", api_output);
    json_member("coordinate_space", document && document->rewritten ? "rewritten_source" : "source");
    putchar(','); json_member("path", SpanPath(span));
    printf(",\"line\":%d,\"column\":%d,\"end_line\":%d,\"end_column\":%d,\"start_byte\":",
           span.line, span.column, span.end_line, span.end_column);
    int known = DocumentOffset(document, span.line, span.column, &begin) &&
                DocumentOffset(document, span.end_line, span.end_column, &end) && begin <= end;
    if(known) printf("%zu,\"end_byte\":%zu}", begin, end);
    else fputs("null,\"end_byte\":null}", api_output);
}
/* Ownership comes from checked type identity and fields, not a spelling
 * heuristic. A template variable's ownership remains explicitly unknown. */
static int owned_type(const ZirModule *module, const char *type, int depth)
{
    char element[ZIR_NAME_MAX];
    if(depth > 32) return -1;
    if(VecElementType(module, type, NULL, 0)) return 1;
    if(type[0] == '*' || SliceElementType(type, NULL, 0) || *ScalarType(type)) return 0;
    if(ArrayElementType(type, element, sizeof(element), NULL)) return owned_type(module, element, depth+1);
    const ZirModule *owner = NULL;
    const ZirType *record = FindType(module, type, &owner);
    if(record == NULL) return -1;
    if(record->is_enum || record->is_procedure_type || record->is_map || record->is_extern) return 0;
    size_t offset = 0; ZirTypeField field; int unknown = 0;
    while(TypeNextField(record, &offset, &field) == 1) {
        int owned = owned_type(owner ? owner : module, field.type, depth+1);
        if(owned == 1) return 1;
        if(owned < 0) unknown = 1;
    }
    return unknown ? -1 : 0;
}
static void ownership_mode(const ZirModule *module, const char *type)
{
    if(type == NULL || !*type || !strcmp(type, "void")) json_string("none");
    else if(type[0] == '*' || SliceElementType(type, NULL, 0)) json_string("borrow");
    else { int owned = owned_type(module, type, 0); if(owned < 0) fputs("null", api_output); else json_string(owned ? "move" : "copy"); }
}

/* The checker names each type application, such as Vec(u8), by a generated
 * __type_ name; write it back the way the source spells it. */
static void
source_spelling(const ZirModule *module, const char *text, char *out,
                size_t size, int depth)
{
    size_t used = 0;
    for(const char *cursor = text; *cursor && used + 1 < size; ) {
        size_t length = 0;
        if(strncmp(cursor, "__type_", 7) == 0 &&
           (cursor == text || (!isalnum((unsigned char)cursor[-1]) &&
                               cursor[-1] != '_' && cursor[-1] != '.'))) {
            length = 7;
            while(isxdigit((unsigned char)cursor[length])) length++;
        }
        char name[ZIR_NAME_MAX];
        const ZirType *type = NULL;
        if(length > 7 && length < sizeof(name) && depth < 16) {
            memcpy(name, cursor, length);
            name[length] = '\0';
            type = FindType(module, name, NULL);
        }
        if(type == NULL || !type->is_synthetic_application) {
            out[used++] = *cursor++;
            continue;
        }
        char arguments[ZIR_TEXT_MAX], spaced[ZIR_TEXT_MAX];
        source_spelling(module, type->template_args, arguments,
                        sizeof(arguments), depth + 1);
        size_t written = 0;
        for(const char *a = arguments; *a && written + 3 < sizeof(spaced); a++) {
            spaced[written++] = *a;
            if(*a == ',' && a[1] != ' ') spaced[written++] = ' ';
        }
        spaced[written] = '\0';
        int count = snprintf(out + used, size - used, "%s(%s)",
                             type->template_name, spaced);
        if(count < 0 || (size_t)count >= size - used) break;
        used += (size_t)count;
        cursor += length;
    }
    out[used] = '\0';
}

static void
show_function(const ZirModule *module, const ZirFunction *fn, int json)
{
    /* Overloads share the name callers write; fn->name is unique. An
     * operator procedure is listed as declared: operator +. */
    const char *name = fn->overload_name[0] ? fn->overload_name : fn->name;
    char operator_name[16];
    if(OperatorOfProcedure(name) != NULL) {
        snprintf(operator_name, sizeof(operator_name), "operator %s",
                 OperatorOfProcedure(name));
        name = operator_name;
    }
    struct { char args[ZIR_TEXT_MAX * 2], result[ZIR_TEXT_MAX], spelled[ZIR_TEXT_MAX]; }
        *text = calloc(1, sizeof(*text));
    if(text == NULL) {
        DiagnosticOutOfMemory();
        exit(1);
    }
    char *args = text->args, *result = text->result;
    const size_t result_size = sizeof(text->result);
    source_spelling(module, FunctionDefaultArgs(fn)[0] && !json ? FunctionDefaultArgs(fn) : FunctionArgs(fn),
                    args, sizeof(text->args), 0);
    source_spelling(module, fn->return_type[0] ? fn->return_type : "void",
                    result, result_size, 0);
    /* Several results are checked as a generated record; list its types. */
    const ZirType *results = fn->return_type[0] ?
        FindType(module, fn->return_type, NULL) : NULL;
    if(results != NULL && results->is_results) {
        size_t offset = 0, used = 0;
        ZirTypeField field;
        result[used++] = '(';
        while(TypeNextField(results, &offset, &field) == 1 && used + 4 < result_size) {
            source_spelling(module, field.type, text->spelled, sizeof(text->spelled), 0);
            int written = snprintf(result + used, result_size - used, "%s%s",
                                   used > 1 ? ", " : "", text->spelled);
            if(written < 0 || (size_t)written >= result_size - used) break;
            used += (size_t)written;
        }
        snprintf(result + used, result_size - used, ")");
    }
    if(json) {
        putchar('{');
        json_member("name", name);
        putchar(',');
        json_member("parameters", args);
        putchar(',');
        json_member("return_type", result);
        putchar(',');
        json_member("defaults", FunctionDefaultArgs(fn));
        putchar(',');
        json_member("effect", fn->effect_class);
        printf(",\"must_use\":%s,\"template\":%s,\"uses_host\":%s",
               fn->must_use ? "true" : "false",
               fn->is_template ? "true" : "false",
               fn->uses_host ? "true" : "false");
        symbol_metadata(module, "function", name, FunctionArgs(fn), fn->return_type, fn->span);
        fputs(",\"ownership\":{\"result\":", api_output); ownership_mode(module, fn->return_type);
        fputs(",\"parameters\":[", api_output);
        const ZirParameters *parameters = ParametersOf(FunctionArgs(fn));
        for(int i = 0; i < parameters->count; i++) {
            if(i) putchar(',');
            putchar('{'); json_member("name", parameters->items[i].binding_name);
            fputs(",\"mode\":", api_output); ownership_mode(module, parameters->items[i].value_type);
            putchar('}');
        }
        fputs("]},\"examples\":null,", api_output);
        json_member("path", SpanPath(fn->span));
        printf(",\"line\":%d}", fn->span.line);
    } else {
        printf("  %s :: (%s) -> %s", name, args, result);
        if(fn->is_template) fputs(" [template]", api_output);
        if(fn->must_use) fputs(" [must use]", api_output);
        if(fn->effect_class[0]) printf(" [effect: %s]", fn->effect_class);
        if(fn->uses_host) fputs(" [host]", api_output);
        printf(" @ %s:%d\n", SpanPath(fn->span), fn->span.line);
    }
    free(text);
}

static int
visible_function(const ZirFunction *fn)
{
    if(strncmp(fn->name, "__zi_dependency_", 16) == 0)
        return 0;
    /* Default-expression helpers inherit their declaration's visibility.
     * Their reserved generated names are never user-facing APIs, including
     * when reading saved IR that no longer marks their source role. */
    if(strncmp(fn->name, "zi_default_", 11) == 0 &&
       strlen(fn->name) == 27) {
        int generated = 1;
        for(const unsigned char *p = (const unsigned char *)fn->name + 11;
            *p; p++)
            if(!isxdigit(*p)) generated = 0;
        if(generated) return 0;
    }
    return fn->is_public && !fn->is_file_private &&
           !fn->is_specialization && !fn->is_global_initializer;
}

static int
visible_import(const ZirImport *item)
{
    return item->is_public && !item->is_file_private;
}

static const char *
import_kind(const ZirImport *item)
{
    return item->kind == ZIR_IMPORT_EXTERN ? "foreign" :
           item->kind == ZIR_IMPORT_MODULE ? "module" : "open";
}

static const char *
extern_kind(const ZirImport *item)
{
    return item->extern_kind == ZIR_EXTERN_C ? "c" :
           item->extern_kind == ZIR_EXTERN_GO ? "go" :
           item->extern_kind == ZIR_EXTERN_HOST ? "host" : "";
}

static void
show_module(const ZirModule *module, int json)
{
    int first = 1;

    if(json) {
        putchar('{');
        json_member("name", module->name);
        putchar(',');
        json_member("source", module->source_path);
        fputs(",\"document\":", api_output); document_json(module->document);
        fputs(",\"imports\":[", api_output);
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *item = &module->imports[i];
            if(!visible_import(item) || !take_symbol(module, "import", item->name, item->signature, item->target)) continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("kind", import_kind(item));
            putchar(',');
            json_member("name", item->name);
            putchar(',');
            json_member("target", item->target);
            fputs(",\"resolved_module\":", api_output);
            if(item->resolved_module) json_string(item->resolved_module->name); else fputs("null", api_output);
            putchar(',');
            json_member("foreign_target", extern_kind(item));
            putchar(',');
            json_member("foreign_symbol", item->extern_symbol);
            putchar(',');
            json_member("signature", item->signature);
            putchar(',');
            json_member("parameters", item->args);
            putchar(',');
            json_member("return_type", item->return_type);
            printf(",\"must_use\":%s,\"required\":%s,\"using\":%s",
                   item->must_use ? "true" : "false",
                   item->required ? "true" : "false",
                   item->is_using ? "true" : "false");
            symbol_metadata(module, "import", item->name, item->signature, item->target, item->span);
            putchar(',');
            json_member("path", (SpanPath(item->span)));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"constants\":[", api_output);
        first = 1;
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *item = &module->defines[i];
            if(!item->is_public || item->is_file_private || !take_symbol(module, "constant", item->name, "", "")) continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("name", item->name);
            putchar(',');
            json_member("value", item->value);
            symbol_metadata(module, "constant", item->name, "", "", item->span);
            putchar(',');
            json_member("path", (SpanPath(item->span)));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"globals\":[", api_output);
        first = 1;
        for(int i = 0; i < module->global_count; i++) {
            const ZirGlobal *item = &module->globals[i];
            if(item->is_static || item->is_file_private || !take_symbol(module, "global", item->name, item->type, "")) continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("name", item->name);
            putchar(',');
            char spelled[ZIR_TEXT_MAX];
            source_spelling(module, item->type, spelled, sizeof(spelled), 0);
            json_member("type", spelled);
            putchar(',');
            json_member("initializer", item->init);
            symbol_metadata(module, "global", item->name, item->type, "", item->span);
            putchar(',');
            json_member("path", (SpanPath(item->span)));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"types\":[", api_output);
        first = 1;
        for(int i = 0; i < module->type_count; i++) {
            const ZirType *type = &module->types[i];
            if(!type->is_public || type->is_file_private || type->is_results ||
               type->is_synthetic_application || type->is_type_instance ||
               !take_symbol(module, "type", type->name, type->template_params, ""))
                continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("name", type->name);
            putchar(',');
            char body[sizeof(type->body)];
            source_spelling(module, type->body, body, sizeof(body), 0);
            json_member("body", body);
            putchar(',');
            json_member("type_parameters", type->template_params);
            printf(",\"record_template\":%s,\"enum\":%s,\"procedure\":%s,",
                   type->is_record_template ? "true" : "false",
                   type->is_enum ? "true" : "false",
                   type->is_procedure_type ? "true" : "false");
            json_member("return_type", type->procedure_return_type);
            symbol_metadata(module, "type", type->name, type->template_params, "", type->span);
            fputs(",\"ownership\":", api_output); ownership_mode(module, type->name); putchar(',');
            json_member("path", SpanPath(type->span));
            printf(",\"line\":%d}", type->span.line);
        }
        fputs("],\"functions\":[", api_output);
        first = 1;
        for(int i = 0; i < module->function_count; i++) {
            const ZirFunction *fn = &module->functions[i];
            if(!visible_function(fn) || !take_symbol(module, "function", fn->overload_name[0] ? fn->overload_name : fn->name, FunctionArgs(fn), fn->return_type)) continue;
            if(!first) putchar(',');
            first = 0;
            show_function(module, fn, 1);
        }
        fputs("],\"laws\":[", api_output);
        first = 1;
        for(int i = 0; i < module->law_count; i++) {
            const ZirLaw *law = &module->laws[i];
            if(!take_symbol(module, "law", law->name, law->kind, "")) continue;
            if(!first) putchar(','); first = 0;
            fputs("{\"visibility\":\"module_contract\",", api_output); json_member("name", law->name); putchar(','); json_member("kind", law->kind);
            putchar(','); json_member("claim", law->payload);
            fputs(",\"status\":", api_output);
            if(law->evaluated_in) json_string(LawStatusName(law->evaluated_status)); else fputs("null", api_output);
            symbol_metadata(module, "law", law->name, law->kind, "", law->span);
            putchar('}');
        }
        fputs("]}", api_output);
    } else {
        printf("module %s (%s)\n", module->name, module->source_path);
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *item = &module->imports[i];
            if(!visible_import(item)) continue;
            printf("  import %s %s", import_kind(item), item->name);
            if(item->target[0]) printf(" -> %s", item->target);
            if(item->signature[0]) printf(" : %s", item->signature);
            printf(" @ %s:%d\n", SpanPath(item->span), item->span.line);
        }
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *item = &module->defines[i];
            if(!item->is_public || item->is_file_private) continue;
            printf("  constant %s = %s @ %s:%d\n", item->name,
                   item->value, SpanPath(item->span), item->span.line);
        }
        for(int i = 0; i < module->global_count; i++) {
            const ZirGlobal *item = &module->globals[i];
            if(item->is_static || item->is_file_private) continue;
            char spelled[ZIR_TEXT_MAX];
            source_spelling(module, item->type, spelled, sizeof(spelled), 0);
            printf("  global %s: %s @ %s:%d\n", item->name,
                   spelled, SpanPath(item->span), item->span.line);
        }
        for(int i = 0; i < module->type_count; i++) {
            const ZirType *type = &module->types[i];
            if(!type->is_public || type->is_file_private || type->is_results ||
               type->is_synthetic_application || type->is_type_instance)
                continue;
            printf("  type %s", type->name);
            if(type->template_params[0])
                printf("(%s)", type->template_params);
            printf(" @ %s:%d\n", SpanPath(type->span), type->span.line);
        }
        for(int i = 0; i < module->function_count; i++)
            if(visible_function(&module->functions[i]))
                show_function(module, &module->functions[i], 0);
    }
}

static int module_order(const void *left, const void *right)
{
    const ZirModule *a = *(const ZirModule *const *)left, *b = *(const ZirModule *const *)right;
    int name = strcmp(a->name, b->name);
    return name ? name : strcmp(a->source_path, b->source_path);
}
static char *render_api(const ZirModule *const *modules, int count, int json,
                        int requested_symbols, size_t *bytes)
{
    TextStream stream; char *text = NULL;
    api_output = TextStreamOpen(&stream, &text, bytes);
    if(api_output == NULL) return NULL;
    emitted = matched = 0;
    if(json) {
        fputs("{\"schema_version\":1,\"query\":{", api_output);
        json_member("module", filter_module ? filter_module : ""); putchar(',');
        json_member("symbol", filter_symbol ? filter_symbol : "");
        printf(",\"imports_only\":%s,\"max_symbols\":%d,\"max_bytes\":%zu},\"modules\":[",
               imports_only ? "true" : "false", requested_symbols, byte_limit);
    }
    int first = 1;
    for(int i = 0; i < count; i++) {
        if(filter_module && strcmp(filter_module, modules[i]->name)) continue;
        if(json && !first) putchar(','); first = 0;
        show_module(modules[i], json);
    }
    if(json) {
        printf("],\"truncation\":{\"matched_symbols\":%d,\"emitted_symbols\":%d,\"complete\":%s,\"symbol_limit\":%s,\"byte_limit\":%s}}\n",
               matched, emitted, matched == emitted ? "true" : "false",
               matched > requested_symbols ? "true" : "false",
               emitted < matched && symbol_limit < requested_symbols ? "true" : "false");
    }
    int failed = ferror(api_output);
    if(TextStreamClose(&stream) != 0) failed = 1;
    api_output = stdout;
    if(failed) { free(text); return NULL; }
    return text;
}
static int positive_bound(const char *text, size_t minimum, size_t maximum, size_t *value)
{
    if(text == NULL || !*text) return 0;
    size_t result = 0;
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if(*p < '0' || *p > '9' || result > (maximum - (*p - '0')) / 10) return 0;
        result = result * 10 + *p - '0';
    }
    if(result < minimum || result > maximum) return 0;
    *value = result; return 1;
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char **module_paths = CommandModulePaths(argc);
    if(module_paths == NULL) return 1;
    const char *defines[64];
    int module_path_count = 0, first_file = 0, json = 0, result = 1;
    int define_count = 0;
    ProgramSet set = {0};

    api_output = stdout;
    const char *max_symbols = NULL, *max_bytes = NULL;
    SetDiagnosticFormatFromArguments(argc, argv);
    const CommandOption options[] = {
        COMMAND_FLAG("--json", json),
        COMMAND_TEXT("--module", filter_module),
        COMMAND_TEXT("--symbol", filter_symbol),
        COMMAND_FLAG("--imports-only", imports_only),
        COMMAND_TEXT("--max-symbols", max_symbols),
        COMMAND_TEXT("--max-bytes", max_bytes),
        COMMAND_TEXT("--root", root),
        COMMAND_LIST("--module-path", module_paths, module_path_count, argc, NULL),
        COMMAND_LIST("--define", defines, define_count, 64, valid_define),
    };
    first_file = ReadCommandOptions(argc, argv, options,
                                    sizeof(options) / sizeof(*options));
    if(first_file < 0) {
        usage();
        return 2;
    }
    if(root == NULL || first_file == 0) {
        usage();
        return 2;
    }
    size_t bound;
    if((max_symbols && !positive_bound(max_symbols, 1, 4096, &bound)) ||
       (!json && (filter_symbol || imports_only || max_symbols))) {
        usage(); return 2;
    }
    if(max_symbols) symbol_limit = (int)bound;
    if(max_bytes && !positive_bound(max_bytes, 1024, 1048576, &byte_limit)) {
        usage(); return 2;
    }
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        goto done;
    if(!CheckCanonicalPrograms(set.programs, set.count,
                               (const char *const *)set.paths))
        goto done;
    int module_count = 0;
    for(int i = 0; i < set.count; i++) module_count += set.programs[i]->module_count;
    const ZirModule **modules = calloc((size_t)module_count, sizeof(*modules));
    if(modules == NULL) { DiagnosticOutOfMemory(); goto done; }
    int index = 0;
    for(int i = 0; i < set.count; i++)
        for(int m = 0; m < set.programs[i]->module_count; m++) modules[index++] = &set.programs[i]->modules[m];
    qsort(modules, (size_t)module_count, sizeof(*modules), module_order);
    int requested = symbol_limit;
    size_t bytes = 0;
    char *text = render_api(modules, module_count, json, requested, &bytes);
    if(text && bytes > byte_limit && json) {
        free(text); text = NULL;
        int low = 0, high = requested;
        while(low < high) {
            symbol_limit = low + (high - low + 1) / 2;
            char *trial = render_api(modules, module_count, json, requested, &bytes);
            if(trial == NULL) break;
            free(trial);
            if(bytes <= byte_limit) low = symbol_limit; else high = symbol_limit - 1;
        }
        symbol_limit = low;
        text = render_api(modules, module_count, json, requested, &bytes);
    }
    free(modules);
    if(text == NULL) DiagnosticOutOfMemory();
    else if(bytes > byte_limit)
        Diagnostic((ZirSourceSpan){0}, "command.arguments", "API framing exceeds --max-bytes; select --module or increase the budget");
    else {
        if(fwrite(text, 1, bytes, stdout) == bytes && !ferror(stdout)) result = 0;
    }
    free(text);
done:
    ProgramsFree(&set);
    return result;
}
