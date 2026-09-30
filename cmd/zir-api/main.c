#include "zir.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_serial.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fputs("usage: ziran api [--json] [--diagnostics=text|json] "
          "[--module-path DIR] [--define NAME] --root DIR file.zi|file.zir ...\n", stderr);
}

static void
json_string(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

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
    /* Overloads share the name callers write; fn->name is unique. */
    const char *name = fn->overload_name[0] ? fn->overload_name : fn->name;
    struct { char args[ZIR_TEXT_MAX * 2], result[ZIR_TEXT_MAX], spelled[ZIR_TEXT_MAX]; }
        *text = calloc(1, sizeof(*text));
    if(text == NULL) {
        fputs("out of memory\n", stderr);
        exit(1);
    }
    char *args = text->args, *result = text->result;
    const size_t result_size = sizeof(text->result);
    source_spelling(module, fn->default_args[0] && !json ? fn->default_args : fn->args,
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
        json_member("defaults", fn->default_args);
        putchar(',');
        json_member("effect", fn->effect_class);
        printf(",\"must_use\":%s,\"template\":%s,\"uses_host\":%s,",
               fn->must_use ? "true" : "false",
               fn->is_template ? "true" : "false",
               fn->uses_host ? "true" : "false");
        json_member("path", SpanPath(fn->span));
        printf(",\"line\":%d}", fn->span.line);
    } else {
        printf("  %s :: (%s) -> %s", name, args, result);
        if(fn->is_template) fputs(" [template]", stdout);
        if(fn->must_use) fputs(" [must use]", stdout);
        if(fn->effect_class[0]) printf(" [effect: %s]", fn->effect_class);
        if(fn->uses_host) fputs(" [host]", stdout);
        printf(" @ %s:%d\n", SpanPath(fn->span), fn->span.line);
    }
    free(text);
}

static int
visible_function(const ZirFunction *fn)
{
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
valid_define(const char *name)
{
    if(name == NULL || (!isalpha((unsigned char)*name) && *name != '_') ||
       strlen(name) >= ZIR_NAME_MAX)
        return 0;
    for(const unsigned char *p = (const unsigned char *)name + 1; *p; p++)
        if(!isalnum(*p) && *p != '_')
            return 0;
    return 1;
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
        fputs(",\"imports\":[", stdout);
        for(int i = 0; i < module->import_count; i++) {
            const ZirImport *item = &module->imports[i];
            if(!visible_import(item)) continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("kind", import_kind(item));
            putchar(',');
            json_member("name", item->name);
            putchar(',');
            json_member("target", item->target);
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
            printf(",\"must_use\":%s,\"required\":%s,\"using\":%s,",
                   item->must_use ? "true" : "false",
                   item->required ? "true" : "false",
                   item->is_using ? "true" : "false");
            json_member("path", SpanPath(item->span));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"constants\":[", stdout);
        first = 1;
        for(int i = 0; i < module->define_count; i++) {
            const ZirDefine *item = &module->defines[i];
            if(!item->is_public || item->is_file_private) continue;
            if(!first) putchar(',');
            first = 0;
            putchar('{');
            json_member("name", item->name);
            putchar(',');
            json_member("value", item->value);
            putchar(',');
            json_member("path", SpanPath(item->span));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"globals\":[", stdout);
        first = 1;
        for(int i = 0; i < module->global_count; i++) {
            const ZirGlobal *item = &module->globals[i];
            if(item->is_static || item->is_file_private) continue;
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
            putchar(',');
            json_member("path", SpanPath(item->span));
            printf(",\"line\":%d}", item->span.line);
        }
        fputs("],\"types\":[", stdout);
        first = 1;
        for(int i = 0; i < module->type_count; i++) {
            const ZirType *type = &module->types[i];
            if(!type->is_public || type->is_file_private || type->is_results ||
               type->is_synthetic_application || type->is_type_instance)
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
            putchar(',');
            json_member("path", SpanPath(type->span));
            printf(",\"line\":%d}", type->span.line);
        }
        fputs("],\"functions\":[", stdout);
        first = 1;
        for(int i = 0; i < module->function_count; i++) {
            const ZirFunction *fn = &module->functions[i];
            if(!visible_function(fn)) continue;
            if(!first) putchar(',');
            first = 0;
            show_function(module, fn, 1);
        }
        fputs("]}", stdout);
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

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *module_paths[64];
    const char *defines[64];
    int module_path_count = 0, first_file = 0, json = 0, result = 1;
    int define_count = 0;
    int first_module = 1;
    ProgramSet set = {0};

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--json") == 0) {
            json = 1;
        } else if(strncmp(argv[i], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[i] + 14)) {
                usage();
                return 2;
            }
        } else if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc &&
                  module_path_count < 64) {
            module_paths[module_path_count++] = argv[++i];
        } else if(strcmp(argv[i], "--define") == 0 && i + 1 < argc &&
                  define_count < 64 && valid_define(argv[i + 1])) {
            defines[define_count++] = argv[++i];
        } else if(argv[i][0] == '-') {
            usage();
            return 2;
        } else {
            first_file = i;
            break;
        }
    }
    if(root == NULL || first_file == 0) {
        usage();
        return 2;
    }
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        goto done;
    if(!CheckCanonicalPrograms(set.programs, set.count,
                               (const char *const *)set.paths))
        goto done;
    if(json) fputs("{\"schema_version\":1,\"modules\":[", stdout);
    for(int i = 0; i < set.count; i++) {
        for(int m = 0; m < set.programs[i]->module_count; m++) {
            if(json && !first_module) putchar(',');
            first_module = 0;
            show_module(&set.programs[i]->modules[m], json);
        }
    }
    if(json) fputs("]}\n", stdout);
    result = ferror(stdout) ? 1 : 0;
done:
    ProgramsFree(&set);
    return result;
}
