#include "zir.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_command.h"
#include "zir_load.h"
#include "zir_serial.h"
#include "zir_text.h"
#include "zir_document.h"
#include "zir_files.h"
#include "zir_overlay.h"
#include "zir_semantic.h"
#include "zir_law.h"
#include "zir_stream.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* One invocation renders to a private stream, then publishes only a complete
 * JSON value that fits the requested byte budget (including its metadata). */
static FILE *api_output;
static TextStream *api_stream;
#define printf(...) fprintf(api_output, __VA_ARGS__)
#define putchar(c) fputc((c), api_output)
static const char *filter_module, *filter_symbol;
static int imports_only, symbol_limit = 256, emitted, matched;
static size_t byte_limit = 65536;

enum { EXAMPLE_LIMIT = 64, EXAMPLE_BYTES = 16384 };

typedef struct ExampleSource {
    const ZirModule *module;
    char *file;
    char *text;
    char entry[ZIR_NAME_MAX + sizeof(":main")];
} ExampleSource;

typedef struct ExampleAttachment {
    ExampleSource *source;
    char *kind;
    char *selector;
    int line;
    int seen;
} ExampleAttachment;

static struct {
    ExampleSource sources[EXAMPLE_LIMIT];
    ExampleAttachment attachments[EXAMPLE_LIMIT];
    int source_count, attachment_count, failed;
    const char *root, *target;
    const char *const *module_paths, *const *defines;
    int module_path_count, define_count;
} examples;

static const char *document_path(const ZirDocument *document)
{
    return SpanPath((ZirSourceSpan){.context = document->context, .file = document->file});
}

static int example_error(const char *path, int line, const char *message)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments", "%s:%d: API example: %s", path, line, message);
    return 0;
}

/* Return 0 only for a missing optional sidecar. Both inputs are bounded
 * regular files, so a directory, link or pipe cannot act as example text. */
static int read_example_file(const char *path, char *text, size_t *bytes, int optional)
{
    int flags = O_RDONLY;
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
#ifdef O_BINARY
    flags |= O_BINARY;
#endif
    int fd = open(path, flags);
    if(fd < 0 && optional && errno == ENOENT) return 0;
    struct stat info;
    FILE *file = fd >= 0 && FileStatusNoLinks(path, &info) == 0 &&
                 fstat(fd, &info) == 0 && S_ISREG(info.st_mode) ? fdopen(fd, "rb") : NULL;
    if(file == NULL) {
        if(fd >= 0) close(fd);
        example_error(path, 1, "expected a readable regular file");
        return -1;
    }
    *bytes = fread(text, 1, EXAMPLE_BYTES + 1, file);
    int failed = ferror(file);
    fclose(file);
    if(failed || *bytes > EXAMPLE_BYTES || memchr(text, '\0', *bytes)) {
        example_error(path, 1, "expected text without NUL bytes, at most 16384 bytes");
        return -1;
    }
    text[*bytes] = '\0';
    return 1;
}

static int example_utf8(const char *text, size_t bytes)
{
    const unsigned char *s = (const unsigned char *)text;
    for(size_t i = 0; i < bytes;) {
        unsigned char lead = s[i++];
        if(lead < 0x80) continue;
        size_t tail = lead >= 0xc2 && lead <= 0xdf ? 1 :
                      lead >= 0xe0 && lead <= 0xef ? 2 :
                      lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
        if(!tail || tail > bytes - i) return 0;
        if((lead == 0xe0 && s[i] < 0xa0) || (lead == 0xed && s[i] >= 0xa0) ||
           (lead == 0xf0 && s[i] < 0x90) || (lead == 0xf4 && s[i] >= 0x90)) return 0;
        for(size_t j = 0; j < tail; j++, i++)
            if(s[i] < 0x80 || s[i] > 0xbf) return 0;
    }
    return 1;
}

static char *example_line(char **cursor)
{
    char *line = *cursor;
    char *end = strchr(line, '\n');
    if(end) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        end = line + strlen(line);
        *cursor = end;
    }
    if(end > line && end[-1] == '\r') end[-1] = '\0';
    return line;
}

static int example_basename(const char *file)
{
    size_t length = strcspn(file, ".");
    for(size_t i = 0; i < length; i++) {
        unsigned char c = file[i];
        if(c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
           !(i > 0 && c >= '0' && c <= '9')) return 0;
    }
    return length > 0 && !strcmp(file + length, ".zi");
}

static int read_attachments(const ZirModule *module, const char *path, char *text)
{
    char *cursor = text;
    if(strcmp(example_line(&cursor), "ziran-api-examples-v1"))
        return example_error(path, 1, "expected ziran-api-examples-v1 header");
    for(int line = 2; *cursor; line++) {
        char *kind = example_line(&cursor);
        if(!*kind || *kind == '#') continue;
        char *selector = strchr(kind, '\t');
        if(selector) *selector++ = '\0';
        char *file = selector ? strchr(selector, '\t') : NULL;
        if(file) *file++ = '\0';
        if(!*kind || !selector || !*selector || !file || !example_basename(file))
            return example_error(path, line, "expected KIND<TAB>NAME_OR_ID<TAB>FILE.zi");
        if(examples.attachment_count == EXAMPLE_LIMIT)
            return example_error(path, line, "too many attachments (maximum 64 per query)");
        ExampleSource *source = NULL;
        for(int i = 0; i < examples.source_count; i++)
            if(examples.sources[i].module == module && !strcmp(examples.sources[i].file, file))
                source = &examples.sources[i];
        if(source == NULL) {
            source = &examples.sources[examples.source_count++];
            source->module = module;
            source->file = strdup(file);
            if(source->file == NULL) { DiagnosticOutOfMemory(); return 0; }
        }
        for(int i = 0; i < examples.attachment_count; i++) {
            const ExampleAttachment *entry = &examples.attachments[i];
            if(entry->source == source && !strcmp(entry->kind, kind) && !strcmp(entry->selector, selector))
                return example_error(path, line, "duplicate attachment");
        }
        ExampleAttachment *entry = &examples.attachments[examples.attachment_count++];
        *entry = (ExampleAttachment){.source = source, .kind = strdup(kind),
                                    .selector = strdup(selector), .line = line};
        if(entry->kind == NULL || entry->selector == NULL) { DiagnosticOutOfMemory(); return 0; }
    }
    return 1;
}

static int load_attachments(const ZirModule *const *modules, int count)
{
    char *text = malloc(EXAMPLE_BYTES + 2);
    if(text == NULL) { DiagnosticOutOfMemory(); return 0; }
    int ok = 1;
    for(int i = 0; ok && i < count; i++) {
        const ZirModule *module = modules[i];
        if(!module->document || (filter_module && strcmp(filter_module, module->name))) continue;
        char path[ZIR_PATH_MAX + sizeof(".examples")];
        int length = snprintf(path, sizeof(path), "%s.examples", document_path(module->document));
        if(length < 0 || (size_t)length >= sizeof(path)) {
            ok = example_error(document_path(module->document), 1, "sidecar path is too long");
            break;
        }
        size_t bytes;
        int found = read_example_file(path, text, &bytes, 1);
        ok = found == 0 || (found > 0 && read_attachments(module, path, text));
    }
    free(text);
    return ok;
}

static int example_matches(const ExampleAttachment *entry, const ZirModule *module,
                           const char *kind, const char *name, const char *id)
{
    return entry->source->module == module && !strcmp(entry->kind, kind) &&
           (!strcmp(entry->selector, name) || !strcmp(entry->selector, id));
}

static int example_has_entry(const ZirModule *entry)
{
    for(int i = 0; i < entry->function_count; i++) {
        const ZirFunction *fn = &entry->functions[i];
        if(fn->exported && !fn->is_extern && !fn->is_template && !strcmp(fn->name, "main") &&
           (!fn->export_symbol[0] || !strcmp(fn->export_symbol, "main")) &&
           !FunctionArgs(fn)[0] && !strcmp(fn->return_type, "s32")) return 1;
    }
    return 0;
}

static int example_imports(const ZirModule *entry, const ZirModule *owner)
{
    for(int i = 0; i < entry->import_count; i++) {
        const ZirModule *target = entry->imports[i].resolved_module;
        if(target && target->document && !strcmp(target->name, owner->name) &&
           !strcmp(document_path(target->document), document_path(owner->document)) &&
           !strcmp(target->document->sha256, owner->document->sha256)) return 1;
    }
    return 0;
}

static int check_api_programs(const ProgramSet *set, const char *target)
{
    if(!CheckCanonicalPrograms(set->programs, set->count, (const char *const *)set->paths))
        return 0;
    if(target)
        for(int i = 0; i < set->count; i++)
            if(!CheckTargetCapabilities(set->programs[i], target))
                return 0;
    return 1;
}

static int check_example(ExampleSource *source)
{
    const char *owner_path = document_path(source->module->document);
    const char *slash = strrchr(owner_path, '/');
    char path[ZIR_PATH_MAX];
    int length = snprintf(path, sizeof(path), "%.*s%s",
                          slash ? (int)(slash - owner_path + 1) : 0, owner_path, source->file);
    if(length < 0 || (size_t)length >= sizeof(path))
        return example_error(owner_path, 1, "example source path is too long");
    if(!strcmp(path, owner_path))
        return example_error(path, 1, "example source must be a separate file");
    char *text = malloc(EXAMPLE_BYTES + 2);
    if(text == NULL) { DiagnosticOutOfMemory(); return 0; }
    size_t bytes;
    int ok = read_example_file(path, text, &bytes, 0) > 0;
    if(ok && !example_utf8(text, bytes)) ok = example_error(path, 1, "source must be UTF-8");
    if(ok && !SourceOverlayBegin(path, text, bytes)) ok = example_error(path, 1, "cannot snapshot source");
    if(ok) {
        const char *inputs[] = {path};
        ProgramSet set = {0};
        ok = ProgramsLoadWithDefines(&set, examples.root, examples.module_paths, examples.module_path_count,
                                     examples.defines, examples.define_count, inputs, 1) &&
             check_api_programs(&set, examples.target);
        SourceOverlayEnd();
        if(ok) {
            const ZirModule *entry = &set.programs[0]->modules[0];
            char hash[65];
            DocumentDigest(text, bytes, hash);
            if(!entry->document || strcmp(entry->document->sha256, hash))
                ok = example_error(path, 1, "checked source did not match the example snapshot");
            else if(!example_has_entry(entry))
                ok = example_error(path, 1, "expected #program_export main :: () -> s32");
            else if(!example_imports(entry, source->module))
                ok = example_error(path, 1, "example must directly import the documented module");
            else
                snprintf(source->entry, sizeof(source->entry), "%s:main", entry->name);
        }
        ProgramsFree(&set);
    }
    if(ok) source->text = text;
    else free(text);
    return ok;
}

static int attachments_resolved(void)
{
    for(int i = 0; i < examples.attachment_count; i++) {
        const ExampleAttachment *entry = &examples.attachments[i];
        if(entry->seen) continue;
        char path[ZIR_PATH_MAX + sizeof(".examples")];
        snprintf(path, sizeof(path), "%s.examples", document_path(entry->source->module->document));
        return example_error(path, entry->line, "selector does not name a visible checked symbol of that kind");
    }
    return 1;
}

static void free_examples(void)
{
    for(int i = 0; i < examples.attachment_count; i++) {
        free(examples.attachments[i].kind);
        free(examples.attachments[i].selector);
    }
    for(int i = 0; i < examples.source_count; i++) {
        free(examples.sources[i].file);
        free(examples.sources[i].text);
    }
}

static void
usage(void)
{
    Diagnostic((ZirSourceSpan){0}, "command.arguments",
          "usage: ziran api [--json] [--diagnostics=text|json] "
          "[--target c|go|zx] [--target-os OS] [--target-cpu CPU] "
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

static void example_json(const ZirModule *module, const char *kind, const char *name, const char *id)
{
    int included[EXAMPLE_LIMIT] = {0};
    int count = 0;
    for(int i = 0; !examples.failed && i < examples.attachment_count; i++) {
        const ExampleAttachment *entry = &examples.attachments[i];
        if(!example_matches(entry, module, kind, name, id)) continue;
        ExampleSource *source = entry->source;
        int index = (int)(source - examples.sources);
        if(included[index]) continue;
        /* The caller discards over-budget trials. Stop writing example text
         * once a trial is already too large, even for an overloaded name. */
        size_t position;
        if(TextStreamPosition(api_stream, &position) != 0 || position > byte_limit) break;
        if(!source->text && !check_example(source)) {
            examples.failed = 1;
            break;
        }
        included[index] = 1;
        fputs(count++ ? "," : ",\"examples\":[", api_output);
        fputs("{\"schema_version\":1,", api_output);
        json_member("path", source->file); putchar(',');
        json_member("entry", source->entry); putchar(',');
        json_member("source", source->text); putchar('}');
    }
    if(count) putchar(']');
    else if(!strcmp(kind, "function")) fputs(",\"examples\":null", api_output);
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
    for(int i = 0; i < examples.attachment_count; i++)
        if(example_matches(&examples.attachments[i], module, kind, name, id))
            examples.attachments[i].seen = 1;
    if(filter_symbol && strcmp(filter_symbol, name) && strcmp(filter_symbol, id)) return 0;
    matched++;
    if(emitted >= symbol_limit) return 0;
    size_t position;
    if(TextStreamPosition(api_stream, &position) != 0 || position > byte_limit) return 0;
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
    example_json(module, kind, name, id);
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
        fputs("]},", api_output);
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
    api_stream = &stream;
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
    api_stream = NULL;
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
    const char *target = NULL, *target_os_name = NULL, *target_cpu_name = NULL;
    SetDiagnosticFormatFromArguments(argc, argv);
    const CommandOption options[] = {
        COMMAND_FLAG("--json", json),
        COMMAND_TEXT("--target", target),
        COMMAND_TEXT("--target-os", target_os_name),
        COMMAND_TEXT("--target-cpu", target_cpu_name),
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
    if(target && !TargetNameKnown(target)) {
        Diagnostic(Span("<command>", 1, 1), "command.arguments", "unknown API target: %s", target);
        return 2;
    }
    ZirTargetBackend backend = target == NULL || !strcmp(target, "zx") ?
        ZIR_TARGET_ZX : !strcmp(target, "go") ? ZIR_TARGET_GO : ZIR_TARGET_C;
    ZirTargetOS target_os;
    if(!CommandConfigureTarget(backend, target_os_name, target_cpu_name, &target_os)) {
        usage();
        return 2;
    }
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        goto done;
    if(!check_api_programs(&set, target))
        goto done;
    int module_count = 0;
    for(int i = 0; i < set.count; i++) module_count += set.programs[i]->module_count;
    const ZirModule **modules = calloc((size_t)module_count, sizeof(*modules));
    if(modules == NULL) { DiagnosticOutOfMemory(); goto done; }
    int index = 0;
    for(int i = 0; i < set.count; i++)
        for(int m = 0; m < set.programs[i]->module_count; m++) modules[index++] = &set.programs[i]->modules[m];
    qsort(modules, (size_t)module_count, sizeof(*modules), module_order);
    examples.root = root;
    examples.target = target;
    examples.module_paths = module_paths;
    examples.module_path_count = module_path_count;
    examples.defines = defines;
    examples.define_count = define_count;
    if(json && !imports_only && !load_attachments(modules, module_count)) {
        free(modules);
        goto done;
    }
    int requested = symbol_limit;
    size_t bytes = 0;
    char *text = render_api(modules, module_count, json, requested, &bytes);
    if(text && (examples.failed || !attachments_resolved())) {
        free(text);
        free(modules);
        goto done;
    }
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
    free_examples();
    ProgramsFree(&set);
    return result;
}
