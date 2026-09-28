/*
 * zir_cpp_lower.c - ZIR to C++ backend. Lowers a ZirProgram to .cpp/.hpp
 * source files that can call declared C host functions. Emitted declarations
 * keep C linkage (extern "C") so generated C and C++ modules can interoperate.
 */
#include "zir_cpp_lower.h"
#include "zir.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_runtime.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Whether the header keeps declarations behind its _PRIVATE guard. */
static int
header_has_private(const ZirModule *m)
{
    for(int i = 0; i < m->define_count; i++)
        if(!m->defines[i].is_public)
            return 1;
    for(int i = 0; i < m->type_count; i++)
        if(!m->types[i].is_public)
            return 1;
    return 0;
}

/* An extern "C" block with nothing in it, left around main, is dropped. */
static int
drop_empty_extern_blocks(const char *path)
{
    static const char open_block[] = "extern \"C\" {\n";
    FILE *file = fopen(path, "rb");
    char *text = NULL;
    long size;
    if(file == NULL || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
       fseek(file, 0, SEEK_SET) != 0 || (text = malloc((size_t)size + 1)) == NULL ||
       fread(text, 1, (size_t)size, file) != (size_t)size) {
        if(file != NULL) fclose(file);
        free(text);
        return 0;
    }
    fclose(file);
    text[size] = '\0';
    size_t used = 0;
    for(const char *p = text; *p;) {
        if(!strncmp(p, open_block, sizeof(open_block) - 1)) {
            const char *q = p + sizeof(open_block) - 1;
            while(*q == '\n') q++;
            if(!strncmp(q, "}\n", 2)) {
                p = q + 2;
                while(used > 0 && text[used - 1] == '\n' && *p == '\n') p++;
                continue;
            }
        }
        text[used++] = *p++;
    }
    file = fopen(path, "wb");
    if(file == NULL) {
        free(text);
        return 0;
    }
    int ok = fwrite(text, 1, used, file) == used;
    ok &= fclose(file) == 0;
    free(text);
    return ok;
}
#include <sys/stat.h>

#define LOWER_NAME_MAX 128
#define LOWER_TEXT_MAX 4096

static const char *
enum_storage_type(const char *backing)
{
    static const struct { const char *name, *c_type; } types[] = {
        {"s8", "int8_t"}, {"u8", "uint8_t"},
        {"s16", "int16_t"}, {"u16", "uint16_t"},
        {"s32", "int32_t"}, {"u32", "uint32_t"},
        {"s64", "int64_t"}, {"u64", "uint64_t"}
    };
    for(size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if(strcmp(backing, types[i].name) == 0)
            return types[i].c_type;
    return NULL;
}

static void
mkdir_parent(const char *path)
{
    char tmp[1024];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(i = 1; i < strlen(tmp); i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
}

static void
stem_from_source(const char *src, char *dst, size_t dst_size)
{
    size_t n = strlen(src);

    if(n > 3 && strcmp(src + n - 3, ".zi") == 0)
        n -= 3;
    if(n >= dst_size)
        n = dst_size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Convert a .zi type like "[64] char" or "[2][3] int" to C declarator
 * pieces: base "char" + array suffix "[64]" (placed after the name). */
static void
split_array_type(const char *type, char *base, size_t base_size,
                 char *suffix, size_t suffix_size)
{
    const char *p = type;
    size_t sn = 0;

    suffix[0] = '\0';
    if(p[0] == '[' && p[1] == ']') {
        /* A borrowed slice is a descriptor value, not a flexible array. */
        snprintf(base, base_size, "Slice");
        snprintf(suffix, suffix_size, "");
        return;
    }
    while(*p == '[') {
        const char *close = strchr(p, ']');

        if(close == NULL)
            break;
        {
            size_t len = (size_t)(close - p + 1);

            if(sn + len + 1 < suffix_size) {
                memcpy(suffix + sn, p, len);
                sn += len;
                suffix[sn] = '\0';
            }
        }
        p = close + 1;
        while(*p == ' ' || *p == '\t')
            p++;
    }
    snprintf(base, base_size, "%s", p);
}

static int is_module_alias(const ZirModule *m, const char *alias,
                           size_t alias_len);
static void function_c_name(const ZirModule *m, const ZirFunction *fn,
                            char *dst, size_t dst_size);

/* If ident (len chars, followed by '(') names a function in this module,
 * write its full C name into dst and return its length; else return 0. */
static size_t
resolve_module_fn(const ZirModule *m, const char *ident, size_t len,
                  char *dst, size_t dst_size)
{
    char name[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirFunction *fn = NULL;
    if(len >= sizeof(name))
        return 0;
    memcpy(name, ident, len);
    name[len] = '\0';
    if(ResolveFunction(m, name, &owner, &fn) == 1) {
        function_c_name(owner, fn, dst, dst_size);
        return strlen(dst);
    }
    return 0;
}

/* Resolve alias.fn( via the cross-module symbol table: find the import
 * named alias, get its target module, look up fn there. */
static size_t
resolve_aliased_fn(const ZirModule *m, const ZirCppModuleSyms *restab,
                   int restab_count, const char *alias, size_t alen,
                   const char *fn, size_t flen, char *dst, size_t dst_size)
{
    int i, j;

    if(restab == NULL)
        return 0;
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];

        if(imp->kind != ZIR_IMPORT_MODULE)
            continue;
        if(strlen(imp->name) != alen || strncmp(imp->name, alias, alen) != 0)
            continue;
        for(j = 0; j < restab_count; j++) {
            if(strcmp(restab[j].module_slash, imp->target) != 0 &&
               strcmp(restab[j].module_stem, imp->target) != 0)
                continue;
            for(int k = 0; k < restab[j].fn_count; k++) {
                if(strlen(restab[j].fns[k].source) == flen &&
                   strncmp(restab[j].fns[k].source, fn, flen) == 0) {
                    snprintf(dst, dst_size, "%s", restab[j].fns[k].c);
                    return strlen(dst);
                }
            }
        }
    }
    return 0;
}

/* Rewrite textual constants, global initializers, and type bounds. Function
 * bodies are emitted from checked expression graphs. */

static void
rewrite_body2(const ZirModule *m, const ZirCppModuleSyms *restab,
              int restab_count, const char *src, char *dst, size_t dst_size)
{
    size_t n = 0;

    for(const char *p = src; *p != '\0' && n + 6 < dst_size; p++) {
        if(*p == '"' || *p == '\'') {
            char quote = *p;
            dst[n++] = *p++;
            while(*p && n + 2 < dst_size) {
                char ch = *p++;
                dst[n++] = ch;
                if(ch == '\\' && *p)
                    dst[n++] = *p++;
                else if(ch == quote)
                    break;
            }
            p--;
        } else if(strncmp(p, "null", 4) == 0 &&
                   (p == src || !isalnum((unsigned char)p[-1])) &&
                   !isalnum((unsigned char)p[4]) && p[4] != '_') {
            dst[n++] = 'N';
            dst[n++] = 'U';
            dst[n++] = 'L';
            dst[n++] = 'L';
            p += 3;
        } else if(isalpha((unsigned char)*p) || *p == '_') {
            const char *e = p;

            while(isalnum((unsigned char)*e) || *e == '_')
                e++;
            if(*e == '.' &&
               (isalpha((unsigned char)e[1]) || e[1] == '_')) {
                const char *member = e + 1;
                const char *end = member;
                while(isalnum((unsigned char)*end) || *end == '_') end++;
                for(int t = 0; t < m->type_count; t++) {
                    const ZirType *enumeration = &m->types[t];
                    char member_name[ZIR_NAME_MAX];
                    int64_t value;
                    size_t length = (size_t)(end - member);
                    if(!enumeration->is_enum ||
                       strlen(enumeration->name) != (size_t)(e - p) ||
                       strncmp(enumeration->name, p, (size_t)(e - p)) ||
                       length >= sizeof(member_name)) continue;
                    memcpy(member_name, member, length);
                    member_name[length] = '\0';
                    if(!EnumMemberValue(enumeration, member_name, &value))
                        continue;
                    char mapped[LOWER_TEXT_MAX];
                    int written;
                    if(NativeEnumMemberName(m, enumeration, member_name,
                                            mapped, sizeof(mapped)))
                        written = (int)strlen(mapped);
                    else {
                        int prefixed = strncmp(member_name, enumeration->name,
                                               strlen(enumeration->name)) == 0;
                        written = prefixed ?
                            snprintf(mapped, sizeof(mapped), "%s", member_name) :
                            snprintf(mapped, sizeof(mapped), "%s_%s",
                                     enumeration->name, member_name);
                    }
                    if(written < 0 || (size_t)written >= sizeof(mapped) ||
                       n + (size_t)written >= dst_size) {
                        Diagnostic(enumeration->span, "zir_cpp.enum",
                                   "enum member target name exceeds output limit");
                        exit(1);
                    }
                    memcpy(dst + n, mapped, (size_t)written);
                    n += (size_t)written;
                    p = end - 1;
                    goto next_character;
                }
            }
            if(*e == '.' && e[1] != '\0' &&
               is_module_alias(m, p, (size_t)(e - p))) {
                /* alias.member — cross-module call or enum/type member */
                const char *m0 = e + 1;
                const char *me = m0;

                while(isalnum((unsigned char)*me) || *me == '_')
                    me++;
                char qualified[ZIR_NAME_MAX], native[LOWER_NAME_MAX * 2];
                int type_length = snprintf(qualified, sizeof(qualified),
                                           "%.*s", (int)(me - p), p);
                if(type_length >= 0 &&
                   (size_t)type_length < sizeof(qualified) &&
                   NativeTypeAtUse(m, qualified, native, sizeof(native))) {
                    size_t length = strlen(native);
                    if(n + length >= dst_size) {
                        dst[n] = '\0';
                        return;
                    }
                    memcpy(dst + n, native, length);
                    n += length;
                    p = me - 1;
                    continue;
                }
                if(*me == '(' && restab != NULL) {
                    char cname[LOWER_NAME_MAX * 3];
                    size_t clen = resolve_aliased_fn(m, restab, restab_count,
                                                     p, (size_t)(e - p),
                                                     m0, (size_t)(me - m0),
                                                     cname, sizeof(cname));

                    if(clen > 0) {
                        if(n + clen < dst_size) {
                            memcpy(dst + n, cname, clen);
                            n += clen;
                        }
                        p = me - 1;   /* loop's p++ lands on '(' */
                        continue;
                    }
                }
                p = e;   /* strip the alias; loop's p++ skips the '.' */
                continue;
            }
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e == '(' && e[-1] != ' ') {
                /* a call (not a member access 'x.fn' or generated 'p->fn'): resolve
                 * module-local functions to C names */
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;   /* loop's p++ lands past the ident */
                    continue;
                }
            }
            /* Bare function reference in an initializer. Generated C pointer
             * members and source members are handled as values. */
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e != '.' && !(*e == '-' && e[1] == '>') &&
               *e != '(' && n >= 2 && dst[n - 1] == ' ' && dst[n - 2] == '=' &&
               (n < 3 || (dst[n - 3] != '=' && dst[n - 3] != '!'))) {
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;
                    continue;
                }
            }
            /* standalone call argument ('set_cb(name)' / 'f(a, name)'):
             * a bare identifier passing a function by reference. */
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-') &&
               *e != '(' && (*e == ')' || *e == ',') &&
               n >= 1 && (dst[n - 1] == '(' ||
                          (n >= 2 && dst[n - 1] == ' ' &&
                           dst[n - 2] == ','))) {
                char cname[LOWER_NAME_MAX * 2];
                size_t clen = resolve_module_fn(m, p, (size_t)(e - p),
                                                cname, sizeof(cname));

                if(clen > 0) {
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = e - 1;
                    continue;
                }
            }
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-')) {
                int resolved_top = 0;
                for(int i = 0; i < m->global_count; i++) {
                    if(strlen(m->globals[i].name) != (size_t)(e - p) ||
                       strncmp(m->globals[i].name, p, (size_t)(e - p)))
                        continue;
                    char mapped[LOWER_NAME_MAX];
                    TargetGlobalName(m, ZIR_CPP, m->globals[i].name,
                                     mapped, sizeof(mapped));
                    size_t length = strlen(mapped);
                    if(n + length < dst_size) {
                        memcpy(dst + n, mapped, length);
                        n += length;
                        p = e - 1;
                        resolved_top = 1;
                        break;
                    }
                }
                if(resolved_top) continue;
                for(int i = 0; i < m->define_count; i++) {
                    if(strlen(m->defines[i].name) != (size_t)(e - p) ||
                       strncmp(m->defines[i].name, p, (size_t)(e - p)))
                        continue;
                    char mapped[LOWER_NAME_MAX];
                    TargetDefineName(m, ZIR_CPP, m->defines[i].name,
                                     mapped, sizeof(mapped));
                    size_t length = strlen(mapped);
                    if(n + length < dst_size) {
                        memcpy(dst + n, mapped, length);
                        n += length;
                        p = e - 1;
                        resolved_top = 1;
                        break;
                    }
                }
                if(resolved_top) continue;
            }
            if(!(p > src && p[-1] == '.') &&
               !(p > src + 1 && p[-1] == '>' && p[-2] == '-')) {
                char source_type[ZIR_NAME_MAX], native[LOWER_NAME_MAX * 2];
                size_t length = (size_t)(e - p);
                if(length < sizeof(source_type)) {
                    memcpy(source_type, p, length);
                    source_type[length] = '\0';
                    if(NativeTypeAtUse(m, source_type, native,
                                       sizeof(native))) {
                        length = strlen(native);
                        if(n + length >= dst_size) {
                            dst[n] = '\0';
                            return;
                        }
                        memcpy(dst + n, native, length);
                        n += length;
                        p = e - 1;
                        continue;
                    }
                }
            }
            while(p < e && n + 1 < dst_size)
                dst[n++] = *p++;
            p--;   /* compensate for the loop's p++ */
        } else {
            dst[n++] = *p;
        }
next_character:;
    }
    dst[n] = '\0';
}

/* C function name: <module>_<name> (module dots -> underscores). */
static void
function_c_name(const ZirModule *m, const ZirFunction *fn,
                char *dst, size_t dst_size)
{
    NativeCFunctionName(m, fn, dst, dst_size);
}

void
cpp_function_name(const ZirModule *m, const ZirFunction *fn,
                    char *dst, size_t dst_size)
{
    function_c_name(m, fn, dst, dst_size);
}

/* Is `alias` a module-import alias in this module (alias :: #import "path")?
 * If so, `alias.Type` qualifiers strip to the bare type. */
static int
is_module_alias(const ZirModule *m, const char *alias, size_t alias_len)
{
    int i;

    for(i = 0; i < m->import_count; i++) {
        if(m->imports[i].kind == ZIR_IMPORT_MODULE &&
           strlen(m->imports[i].name) == alias_len &&
           strncmp(m->imports[i].name, alias, alias_len) == 0)
            return 1;
    }
    return 0;
}

/* Strip leading `alias.` module qualifiers from types imported by this module. */
static void
strip_alias_type(const ZirModule *m, const char *type,
                 char *dst, size_t dst_size)
{
    if(SliceElementType(type, NULL, 0)) {
        copy_text(dst, dst_size, "Slice");
        return;
    }
    if(NativeTypeAtUse(m, type, dst, dst_size)) return;
    const char *dot = strchr(type, '.');
    const char *scalar = ScalarType(type);
    if(*scalar && !strcmp(scalar,type) && TargetType(type,ZIR_C)) {
        snprintf(dst,dst_size,"%s",TargetType(type,ZIR_C)); return;
    }
    if(type[0] == '*' && type[1] != '\0') {
        char base[LOWER_NAME_MAX * 2];
        strip_alias_type(m, type + 1, base, sizeof(base));
        snprintf(dst, dst_size, "%s*", base);
        return;
    }
    if(dot != NULL) {
        size_t alen = (size_t)(dot - type);

        if(is_module_alias(m, type, alen)) {
            snprintf(dst, dst_size, "%s", dot + 1);
            return;
        }
    }
    snprintf(dst, dst_size, "%s", type);
}

static void
resolve_slot_type(void *context, const char *source,
                  char *out, size_t size)
{
    strip_alias_type((const ZirModule *)context, source, out, size);
}

/* Convert Jai parameters to C++ and strip imported type qualifiers. */
static void
convert_args(const ZirModule *m, const ZirFunction *fn,
             const char *args, char *dst, size_t dst_size)
{
    size_t n = 0;

    dst[0] = '\0';
    if(args == NULL || args[0] == '\0') {
        snprintf(dst, dst_size, "void");
        return;
    }
    /* Split on top-level commas; each part is "name: Type". */
    {
        const char *p = args;
        int depth = 0;
        const char *start = p;
        int first = 1;

        while(1) {
            if(*p == '(' || *p == '[' || *p == '{')
                depth++;
            else if(*p == ')' || *p == ']' || *p == '}')
                depth--;
            if((*p == ',' && depth == 0) || *p == '\0') {
                char part[LOWER_TEXT_MAX];
                size_t len = (size_t)(p - start);
                const char *colon;

                if(len >= sizeof(part))
                    len = sizeof(part) - 1;
                memcpy(part, start, len);
                part[len] = '\0';
                /* trim */
                {
                    char *e = part + strlen(part);
                    while(e > part && (e[-1] == ' ' || e[-1] == '\t'))
                        *--e = '\0';
                }
                colon = strchr(part, ':');
                if(colon != NULL) {
                    char name[LOWER_NAME_MAX];
                    char type[LOWER_NAME_MAX];
                    const char *name_start = skip_ws(part);
                    size_t nl = (size_t)(colon - name_start);
                    const char *ty = colon + 1;

                    while(*ty == ' ' || *ty == '\t')
                        ty++;
                    if(strcmp(ty, "..any") == 0) {
                        if(!first && n + 2 < dst_size)
                            dst[n++] = ',';
                        if(!first && n + 1 < dst_size)
                            dst[n++] = ' ';
                        n += (size_t)snprintf(dst + n, dst_size - n, "...");
                        first = 0;
                        goto next_arg;
                    }
                    while(nl > 0 && isspace((unsigned char)name_start[nl - 1]))
                        nl--;
                    if(nl >= sizeof(name))
                        nl = sizeof(name) - 1;
                    memcpy(name, name_start, nl);
                    name[nl] = '\0';
                    {
                        char binding[LOWER_NAME_MAX];
                        TargetBindingName(fn, ZIR_CPP, name, binding,
                                          sizeof(binding));
                        copy_text(name, sizeof(name), binding);
                    }
                    strip_alias_type(m, ty, type, sizeof(type));
                    {
                        /* 'name: [N] Type' parameters must emit C array
                         * syntax 'Type name[N]', not '[N] Type name'. */
                        char pbase[LOWER_NAME_MAX];
                        char psuffix[LOWER_NAME_MAX];

                        split_array_type(type, pbase, sizeof(pbase),
                                         psuffix, sizeof(psuffix));
                        strip_alias_type(m, pbase, type, sizeof(type));
                        copy_text(pbase, sizeof(pbase), type);
                        if(!first && n + 2 < dst_size)
                            dst[n++] = ',';
                        if(!first && n + 1 < dst_size)
                            dst[n++] = ' ';
                        if(!strcmp(psuffix, "[0]"))
                            n += (size_t)snprintf(dst + n, dst_size - n,
                                                  "%s *%s", pbase, name);
                        else
                            n += (size_t)snprintf(dst + n, dst_size - n,
                                                  "%s %s%s", pbase, name,
                                                  psuffix);
                    }
                    first = 0;
                }
next_arg:
                if(*p == '\0')
                    break;
                start = p + 1;
            }
            p++;
        }
    }
    if(n == 0)
        snprintf(dst, dst_size, "void");
}

/* Native bodies are emitted from checked expressions and statements. */
typedef struct BodySymbols {
    const ZirModule *module;
    const ZirCppModuleSyms *symbols;
    int count;
    const char *source_path;
} BodySymbols;

static void
resolve_body_symbol(void *context, const char *text, char *out, size_t size)
{
    BodySymbols *symbols = context;
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    /* A checked type qualified by an alias or by the module that declares
     * it (a type reached only through another module) has one native name. */
    if(strchr(text, '.') != NULL &&
       NativeTypeAtUse(symbols->module, text, out, size))
        return;
    if(ResolveGlobalAt(symbols->module, text, symbols->source_path,
                       &owner, &global) == 1) {
        TargetGlobalName(owner, ZIR_CPP, global->name, out, size);
        return;
    }
    for(int i = 0; i < symbols->module->define_count; i++)
        if(strcmp(symbols->module->defines[i].name, text) == 0) {
            TargetDefineName(symbols->module, ZIR_CPP, text, out, size);
            return;
        }
    rewrite_body2(symbols->module, symbols->symbols, symbols->count, text, out, size);
}

static void
lower_body(FILE *out, const ZirModule *module, const ZirCppModuleSyms *symbols,
           int symbol_count, const ZirFunction *function)
{
    BodySymbols context = {module, symbols, symbol_count,
                           function->span.path};
    if(!EmitBody(out, module, function, ZIR_CPP,
                 resolve_body_symbol, &context, NULL)) {
        Diagnostic(function->span, "zir_cpp.body",
                   "function has no checked typed body: %s", function->name);
        exit(1);
    }
}

/* '#if' regions stamp their captures with the expanded C preprocessor
 * condition; emit each guarded item wrapped in '#if cond / #endif'. */
static int
c_extern_symbol(const ZirImport *imp, char *dst, size_t dst_size)
{
    if(imp == NULL || strncmp(imp->target, "c.", 2) != 0 ||
       imp->target[2] == '\0')
        return 0;
    snprintf(dst, dst_size, "%s", imp->target + 2);
    return 1;
}

static void
extract_extern_signature(const ZirImport *imp, char *ret, size_t ret_size,
                         char *args, size_t args_size)
{
    const char *sig = imp->signature;
    const char *op = strchr(sig, '(');
    const char *cl = op != NULL ? strchr(op, ')') : NULL;
    const char *arrow = cl != NULL ? strstr(cl, "->") : NULL;

    snprintf(ret, ret_size, "void");
    if(arrow != NULL) {
        const char *r = arrow + 2;
        size_t rn = 0;

        while(*r == ' ' || *r == '\t')
            r++;
        while(*r != '\0' && *r != '#' && rn + 1 < ret_size)
            ret[rn++] = *r++;
        while(rn > 0 && (ret[rn - 1] == ' ' || ret[rn - 1] == '\t'))
            rn--;
        ret[rn] = '\0';
    }
    if(op != NULL && cl != NULL && cl > op)
        snprintf(args, args_size, "%.*s", (int)(cl - op - 1), op + 1);
    else
        snprintf(args, args_size, "void");
}

static void
extern_call_args(const char *args, char *dst, size_t dst_size)
{
    const char *p = args;
    const char *start = args;
    int depth = 0;
    int first = 1;
    size_t n = 0;

    dst[0] = '\0';
    if(args == NULL || args[0] == '\0' || strcmp(args, "void") == 0)
        return;
    while(1) {
        if(*p == '(' || *p == '[' || *p == '{')
            depth++;
        else if(*p == ')' || *p == ']' || *p == '}')
            depth--;
        if((*p == ',' && depth == 0) || *p == '\0') {
            const char *colon = start;
            const char *name_start = start;
            const char *name_end;

            while(*name_start == ' ' || *name_start == '\t')
                name_start++;
            while(colon < p && *colon != ':')
                colon++;
            name_end = colon;
            while(name_end > name_start &&
                  (name_end[-1] == ' ' || name_end[-1] == '\t'))
                name_end--;
            if(colon < p && name_end > name_start) {
                n += (size_t)snprintf(dst + n, dst_size > n ? dst_size - n : 0,
                                      "%s%.*s", first ? "" : ", ",
                                      (int)(name_end - name_start), name_start);
                first = 0;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        }
        p++;
    }
}

static void
emit_extern_prototype(FILE *c, const ZirModule *m, const ZirImport *imp)
{
    char ret[LOWER_NAME_MAX];
    char return_type[LOWER_NAME_MAX];
    char cargs[LOWER_TEXT_MAX];
    char abi_args[LOWER_TEXT_MAX];
    char conv[LOWER_TEXT_MAX];
    char symbol[LOWER_NAME_MAX];
    const char *cname = imp->name;

    extract_extern_signature(imp, ret, sizeof(ret), cargs, sizeof(cargs));
    strip_alias_type(m, ret, return_type, sizeof(return_type));
    copy_text(ret, sizeof(ret), return_type);
    ZirFunction abi = {0};
    copy_text(abi.args, sizeof(abi.args), cargs);
    copy_text(abi.return_type, sizeof(abi.return_type), ret);
    ArrayAbiArgs(&abi, abi_args, sizeof(abi_args));
    convert_args(m, NULL, abi_args, conv, sizeof(conv));
    if(ArrayElementType(ret, NULL, 0, NULL))
        copy_text(ret, sizeof(ret), "void");
    if(c_extern_symbol(imp, symbol, sizeof(symbol))) {
        cname = symbol;
        if(strcmp(symbol, imp->name) == 0) {
            fprintf(c, "%s %s(%s);\n", ret[0] ? ret : "void", cname, conv);
            return;
        }
        /* Use a local C++ name for the declaration. Standard headers may
         * already declare the ABI symbol with a different pointer spelling
         * (for example, renameat takes const char* rather than Ziran *u8).
         * The assembler label preserves the requested symbol at link time. */
        char foreign_name[LOWER_NAME_MAX * 2];

        snprintf(foreign_name, sizeof(foreign_name), "zir_foreign_%s", imp->name);
        fprintf(c, "%s %s(%s) __asm__(\"%s\");\n",
                ret[0] ? ret : "void", foreign_name, conv, symbol);
        if(imp->is_varargs) {
            /* A C variadic argument list cannot be forwarded by a
             * regular wrapper. Let the call target the ABI symbol. */
            fprintf(c, "#define %s %s\n", imp->name, foreign_name);
            return;
        }
        char call[LOWER_TEXT_MAX];

        extern_call_args(abi_args, call, sizeof(call));
        fprintf(c, "static %s\n%s(%s)\n{\n",
                ret[0] ? ret : "void", imp->name, conv);
        if(ret[0] != '\0' && strcmp(ret, "void") != 0)
            fprintf(c, "    return %s(%s);\n", foreign_name, call);
        else
            fprintf(c, "    %s(%s);\n", foreign_name, call);
        fprintf(c, "}\n");
    } else {
        fprintf(c, "%s %s(%s);\n", ret[0] ? ret : "void", cname, conv);
    }
}


static int
rewrite_global_scalar(const ZirModule *module, const char *source,
                      char *out, size_t size, void *context)
{
    (void)context;
    rewrite_body2(module, NULL, 0, source, out, size);
    return out[0] != '\0';
}

static int
lower_module(const ZirModule *m, const ZirCppModuleSyms *restab, int restab_count,
             const char *out_dir)
{
    char stem[512];
    char guard[600];
    char hpath[1024];
    char cpath[1024];
    char htemp[1100];
    char ctemp[1100];
    FILE *h;
    FILE *c;
    int i;

    stem_from_source(m->source_path, stem, sizeof(stem));
    NativeHeaderGuard(stem, guard, sizeof(guard));
    snprintf(hpath, sizeof(hpath), "%s/%s.hpp", out_dir, stem);
    snprintf(cpath, sizeof(cpath), "%s/%s.cpp", out_dir, stem);
    mkdir_parent(hpath);

    /* --- header --- */
    h = GeneratedOutputOpen(hpath, htemp, sizeof(htemp));
    if(h == NULL) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot create C++ header output: %s", hpath);
        return 0;
    }
    fprintf(h, "/* Generated by zi2cpp from %s. */\n", m->source_path);
    fprintf(h, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n", guard, guard);
    fputs("#include \"zir_bounds.h\"\n", h);
    if(ModuleUsesSlices(m))
        fputs("#include \"zir_slice.h\"\n", h);
    EmitStringType(h);
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];

        if(!imp->required)
            continue;   /* private-scope imports go to the .c only */
        if(imp->kind == ZIR_IMPORT_OPEN) {
            fprintf(h, "#include \"%s.hpp\"\n", imp->target);
        } else if(imp->kind == ZIR_IMPORT_MODULE)
            fprintf(h, "#include \"%s.hpp\"\n", imp->target);
    }
    /* Generated decls keep C linkage; includes above guard themselves. */
    fprintf(h, "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n");
    /* Compile-time constants first: headers reference them in
     * array bounds and extern declarations, and other modules use them
     * through the generated header -- a .c-only emission starves those. */
    for(i = 0; i < m->define_count; i++) {
        const ZirDefine *d = &m->defines[i];
        char name[LOWER_NAME_MAX], value[LOWER_TEXT_MAX];

        TargetDefineName(m, ZIR_CPP, d->name, name, sizeof(name));
        rewrite_body2(m, NULL, 0, d->value, value, sizeof(value));
        if(!d->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", guard);
        fprintf(h, "#define %s %s\n", name, value);
        if(!d->is_public) fputs("#endif\n", h);
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *slot = &m->types[i];
        if(!slot->is_procedure_type || !slot->is_c_call)
            continue;
        EmitSlotType(h, slot, ZIR_CPP, resolve_slot_type, (void *)m);
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *ty = &m->types[i];
        char native[LOWER_NAME_MAX * 2];
        NativeTypeName(m, ty, native, sizeof(native));
        if(ty->is_extern || ty->is_record_template ||
           (ty->is_procedure_type && !ty->is_enum))
            continue;
        if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", guard);
        if(!ty->is_extern && !ty->is_record_template &&
           !ty->is_procedure_type && !ty->is_enum)
            fprintf(h, "typedef %s %s %s;\n",
                    ty->is_union ? "union" : "struct", native, native);
        if(ty->is_enum) {
            const char *backing = enum_storage_type(ty->enum_backing);
            if(backing == NULL) {
                Diagnostic(ty->span, "zir_cpp.enum", "invalid enum backing type");
                exit(1);
            }
            if(ty->is_enum_flags)
                fprintf(h, "using %s = %s;\n", native, backing);
            else
                fprintf(h, "enum %s : %s;\n", native, backing);
        }
        if(!ty->is_public) fputs("#endif\n", h);
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *slot = &m->types[i];
        if(!slot->is_procedure_type || slot->is_c_call)
            continue;
        EmitSlotType(h, slot, ZIR_CPP, resolve_slot_type, (void *)m);
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *ty = &m->types[i];
        char native[LOWER_NAME_MAX * 2];
        NativeTypeName(m, ty, native, sizeof(native));

        if(ty->is_extern || ty->is_record_template)
            continue;
        if(ty->is_procedure_type)
            continue;
        if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", guard);
        if(ty->is_enum) {
            const char *backing = enum_storage_type(ty->enum_backing);
            if(backing == NULL) {
                Diagnostic(ty->span, "zir_cpp.enum", "invalid enum backing type");
                exit(1);
            }
            if(ty->is_enum_flags)
                fprintf(h, "\nenum : %s {\n", backing);
            else
                fprintf(h, "\nenum %s : %s {\n", native, backing);
            {
                const char *line = ty->body;

                while(line != NULL && *line != '\0') {
                    const char *nl = strchr(line, '\n');
                    size_t len = nl ? (size_t)(nl - line) : strlen(line);

                    if(len > 0) {
                        char raw[LOWER_TEXT_MAX];

                        if(len >= sizeof(raw))
                            len = sizeof(raw) - 1;
                        memcpy(raw, line, len);
                        raw[len] = '\0';
                        while(len > 0 && (raw[len - 1] == ' ' ||
                                          raw[len - 1] == ','))
                            raw[--len] = '\0';
                        if(raw[0] != '\0') {
                            size_t member_length = 0;
                            while(isalnum((unsigned char)raw[member_length]) ||
                                  raw[member_length] == '_')
                                member_length++;
                            if(member_length == 0) {
                                Diagnostic(ty->span, "zir_cpp.enum",
                                           "invalid enum member declaration");
                                exit(1);
                            }
                            char member[LOWER_TEXT_MAX];
                            memcpy(member, raw, member_length);
                            member[member_length] = '\0';
                            char mapped[LOWER_TEXT_MAX];
                            if(NativeEnumMemberName(m, ty, member, mapped,
                                                    sizeof(mapped)))
                                fprintf(h, "    %s%s,\n", mapped,
                                        raw + member_length);
                            else {
                                int prefixed = member_length >= strlen(ty->name) &&
                                    strncmp(raw, ty->name, strlen(ty->name)) == 0;
                                fprintf(h, "    %s%s%.*s%s,\n",
                                        prefixed ? "" : ty->name,
                                        prefixed ? "" : "_",
                                        (int)member_length, raw,
                                        raw + member_length);
                            }
                        }
                    }
                    line = nl ? nl + 1 : NULL;
                }
            }
            fprintf(h, "};\n");
            if(!ty->is_public) fputs("#endif\n", h);
            continue;
        }
        fprintf(h, "\n%s%s %s {\n",
                TypeHasZeroArray(m, ty->name) ? "__extension__ " : "",
                ty->is_union ? "union" : "struct", native);
        /* Each body line is a field decl: 'name: [N] Type' / 'name: Type'. */
        {
            const char *line = ty->body;

            while(line != NULL && *line != '\0') {
                const char *nl = strchr(line, '\n');
                size_t len = nl ? (size_t)(nl - line) : strlen(line);
                char raw[LOWER_TEXT_MAX];
                char name[LOWER_NAME_MAX];
                char mapped[LOWER_NAME_MAX];
                char type[LOWER_TEXT_MAX];
                char base[LOWER_TEXT_MAX];
                char suffix[LOWER_NAME_MAX];
                const char *colon;

                if(len >= sizeof(raw))
                    len = sizeof(raw) - 1;
                memcpy(raw, line, len);
                raw[len] = '\0';
                colon = strchr(raw, ':');
                if(colon != NULL) {
                    const char *ty2 = colon + 1;
                    size_t nl2 = (size_t)(colon - raw);

                    while(*ty2 == ' ' || *ty2 == '\t')
                        ty2++;
                    if(nl2 >= sizeof(name))
                        nl2 = sizeof(name) - 1;
                    memcpy(name, raw, nl2);
                    name[nl2] = '\0';
                    trim_in_place(name);
                    if(strncmp(name, "using", 5) == 0 &&
                       isspace((unsigned char)name[5])) {
                        const char *field_name = name + 5;
                        while(isspace((unsigned char)*field_name))
                            field_name++;
                        memmove(name, field_name, strlen(field_name) + 1);
                    }
                    TargetFieldName(ty, ZIR_CPP, name, mapped, sizeof(mapped));
                    snprintf(type, sizeof(type), "%s", ty2);
                    trim_in_place(type);
                    split_array_type(type, base, sizeof(base),
                                     suffix, sizeof(suffix));
                    {
                        char tmpb[LOWER_TEXT_MAX];

                        strip_alias_type(m, base, tmpb, sizeof(tmpb));
                        snprintf(base, sizeof(base), "%s", tmpb);
                    }
                    fprintf(h, "    %s %s%s;\n", base, mapped, suffix);
                }
                line = nl ? nl + 1 : NULL;
            }
        }
        fprintf(h, "};\n");
        if(!ty->is_public) fputs("#endif\n", h);
    }
    /* Public file-scope variables have external linkage: declare extern in the header,
     * after every named type they reference. Private-scope globals stay
     * in the .c. */
    for(i = 0; i < m->global_count; i++) {
        const ZirGlobal *g = &m->globals[i];
        char base[LOWER_TEXT_MAX];
        char suffix[LOWER_NAME_MAX];
        char name[LOWER_NAME_MAX];

        if(g->is_static)
            continue;
        TargetGlobalName(m, ZIR_CPP, g->name, name, sizeof(name));
        split_array_type(g->type, base, sizeof(base), suffix, sizeof(suffix));
        {
            char tmpb[LOWER_TEXT_MAX];

            strip_alias_type(m, base, tmpb, sizeof(tmpb));
            snprintf(base, sizeof(base), "%s", tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];

                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps));
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        fprintf(h, "%sextern %s %s%s;\n",
                TypeHasZeroArray(m, g->type) ? "__extension__ " : "",
                base, name, suffix);
    }
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        if(fn->is_template || !fn->is_public)
            continue;   /* private functions are file-static */
        function_c_name(m, fn, cname, sizeof(cname));
        char abi_args[ZIR_TEXT_MAX];
        ArrayAbiArgs(fn, abi_args, sizeof(abi_args));
        convert_args(m, fn, abi_args, cargs, sizeof(cargs));
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
        if(NativeMainReturnsStatus(fn))
            copy_text(cret, sizeof(cret), "int32_t");
        if(strcmp(cname, "main") == 0) {
            /* The C++ runtime expects main with C++ language linkage. */
            fprintf(h, "}\nextern \"C++\" {\n%s %s(%s);\n"
                      "}\nextern \"C\" {\n",
                    cret[0] ? cret : "void", cname, cargs);
            continue;
        }
        fprintf(h, "%s %s(%s)", cret[0] ? cret : "void", cname, cargs);
        if(fn->exported) {
            const char *symbol = fn->export_symbol[0] ?
                fn->export_symbol : fn->name;
            if(strcmp(cname, symbol))
                fprintf(h, " __asm__(\"%s\")", symbol);
        }
        fputs(";\n", h);
    }
    fprintf(h, "\n#ifdef __cplusplus\n}\n#endif\n");
    fprintf(h, "\n#endif /* %s */\n", guard);
    if(ferror(h) != 0 || fclose(h) != 0) {
        remove(htemp);
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ header output: %s", hpath);
        return 0;
    }
    if(GeneratedOutputReplace(htemp, hpath) != 0) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot replace C++ header output: %s", hpath);
        return 0;
    }
    if(!EmitRuntimeHeaders(out_dir, hpath)) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot write runtime headers beside %s", hpath);
        return 0;
    }

    /* --- source --- */
    c = GeneratedOutputOpen(cpath, ctemp, sizeof(ctemp));
    if(c == NULL) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot create C++ source output: %s", cpath);
        return 0;
    }
    fprintf(c, "/* Generated by zi2cpp from %s. */\n", m->source_path);
    /* The module's own source sees its header's private declarations. */
    if(header_has_private(m))
        fprintf(c, "#define %s_PRIVATE 1\n#include \"%s.hpp\"\n#undef %s_PRIVATE\n",
                guard, stem, guard);
    else
        fprintf(c, "#include \"%s.hpp\"\n", stem);
    if(ModuleUsesVecOperations(m))
        fputs("#include \"zir_vec.h\"\n", c);
    EmitNumbers(c, m, ZIR_CPP);
    /* Private-scope imports include here (implementation-only). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->required || imp->kind != ZIR_IMPORT_OPEN)
            continue;
        fprintf(c, "#include \"%s.hpp\"\n", imp->target);
    }
    fprintf(c, "\nextern \"C\" {\n");
    /* Module constants lowered to C++ preprocessor constants. */
    for(i = 0; i < m->define_count; i++) {
        const ZirDefine *d = &m->defines[i];
        char name[LOWER_NAME_MAX], value[LOWER_TEXT_MAX];

        TargetDefineName(m, ZIR_CPP, d->name, name, sizeof(name));
        rewrite_body2(m, NULL, 0, d->value, value, sizeof(value));
        fprintf(c, "#define %s %s\n", name, value);
    }
    /* #foreign imports: emit C prototypes parsed from the raw signature
     * ('name :: (args) -> Ret #foreign library;'). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];

        if(imp->kind != ZIR_IMPORT_EXTERN || imp->signature[0] == '\0')
            continue;
        emit_extern_prototype(c, m, imp);
    }
    /* Forward prototypes for private functions: initializers and earlier
     * definitions may reference them before their definition. */
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        if(fn->is_template || fn->is_public || fn->is_extern)
            continue;
        function_c_name(m, fn, cname, sizeof(cname));
        char abi_args[ZIR_TEXT_MAX];
        ArrayAbiArgs(fn, abi_args, sizeof(abi_args));
        convert_args(m, fn, abi_args, cargs, sizeof(cargs));
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
        if(NativeMainReturnsStatus(fn))
            copy_text(cret, sizeof(cret), "int32_t");
        fprintf(c, "[[maybe_unused]] static %s %s(%s);\n",
                cret[0] ? cret : "void", cname, cargs);
    }
    for(i = 0; i < m->global_count; i++) {
        BodySymbols symbols = {m, restab, restab_count, m->globals[i].span.path};
        EmitGlobalSlotWrappers(c, m, &m->globals[i], ZIR_CPP,
                               resolve_body_symbol, &symbols);
    }
    for(i = 0; i < m->global_count; i++) {
        const ZirGlobal *g = &m->globals[i];
        char base[LOWER_TEXT_MAX];
        char suffix[LOWER_NAME_MAX];
        char name[LOWER_NAME_MAX];

        TargetGlobalName(m, ZIR_CPP, g->name, name, sizeof(name));
        split_array_type(g->type, base, sizeof(base), suffix, sizeof(suffix));
        {
            char tmpb[LOWER_TEXT_MAX];

            strip_alias_type(m, base, tmpb, sizeof(tmpb));
            snprintf(base, sizeof(base), "%s", tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];

                /* the alias sits inside brackets ('[state.MAX]'), so use the
                 * body rewriter (strips alias.member anywhere), not the
                 * leading-alias-only type strip */
                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps));
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        {
            char initw[LOWER_TEXT_MAX * 2 + 64] = "";

            /* initializers carry 'null' and module-local function refs */
            if(!ScalarLiteral(g->type, g->init, ZIR_CPP, g->span, initw, sizeof(initw))) {
                char recordw[LOWER_TEXT_MAX * 2 + 64] = "";
                int compound = EmitGlobalInitializer(m, g, ZIR_CPP,
                    rewrite_global_scalar, NULL, NULL, NULL, recordw,
                    sizeof(recordw));
                if(compound < 0) {
                    Diagnostic(g->span, "zir_cpp.global",
                               "cannot lower compound global initializer: %s",
                               g->name);
                    exit(1);
                }
                if(compound > 0)
                    snprintf(initw, sizeof(initw), "%s", recordw);
                else
                    rewrite_body2(m, NULL, 0, g->init, initw, sizeof(initw));
            }
            int zero = TypeHasZeroArray(m, g->type);
            fprintf(c, "%s%s%s %s%s = %s;\n",
                    zero ? "__extension__ " : "",
                    g->is_static ? "static " : "", base, name, suffix,
                    initw[0] ? initw : zero ? "{}" : "{0}");
        }
    }
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cargs[LOWER_TEXT_MAX];
        char cret[LOWER_NAME_MAX];

        if(fn->is_template) continue;
        function_c_name(m, fn, cname, sizeof(cname));
        char abi_args[ZIR_TEXT_MAX];
        ArrayAbiArgs(fn, abi_args, sizeof(abi_args));
        convert_args(m, fn, abi_args, cargs, sizeof(cargs));
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
        if(NativeMainReturnsStatus(fn))
            copy_text(cret, sizeof(cret), "int32_t");
        if(fn->is_extern) {
            /* extern: prototype only, no body */
            fprintf(c, "\n");
            fprintf(c, "%s %s(%s);\n",
                    cret[0] ? cret : "void", cname, cargs);
            continue;
        }
        fprintf(c, "\n");
        EmitParallelWorkers(c, m, fn, ZIR_CPP,
                           resolve_body_symbol, &(BodySymbols){
                               m, restab, restab_count});
        BodySymbols symbols = {m, restab, restab_count};
        EmitSlotWrappers(c, m, fn, ZIR_CPP, resolve_body_symbol, &symbols);
        int cpp_main = strcmp(cname, "main") == 0;
        if(cpp_main)
            fputs("}\n", c);
        if(fn->is_public)
            fprintf(c, "%s\n%s(%s)\n{\n", cret[0] ? cret : "void",
                    cname, cargs);
        else
            fprintf(c, "[[maybe_unused]] static %s\n%s(%s)\n{\n",
                    cret[0] ? cret : "void", cname, cargs);
        lower_body(c, m, restab, restab_count, fn);
        if(NativeMainReturnsStatus(fn))
            fputs("    return 0;\n", c);
        fprintf(c, "}\n");
        if(cpp_main)
            fputs("extern \"C\" {\n", c);
    }
    int startup_count = 0;
    for(i = 0; i < m->function_count; i++)
        startup_count += m->functions[i].is_global_initializer;
    char init_name[LOWER_NAME_MAX];
    NativeCModuleInitName(m, init_name, sizeof(init_name));
    /* A module with no globals to set up, directly or through imports,
     * needs no init function; callers skip it by the same test. */
    if(ModuleNeedsStartup(m)) {
        for(i = 0; i < m->import_count; i++) {
            const ZirModule *dependency = m->imports[i].resolved_module;
            if(ModuleNeedsStartup(dependency)) {
                char name[LOWER_NAME_MAX];
                NativeCModuleInitName(dependency, name, sizeof(name));
                fprintf(c, "void %s(void);\n", name);
            }
        }
        fprintf(c, "\nstatic int %s_state;\nvoid\n%s(void)\n{\n"
                   "    if(%s_state == 2) return;\n"
                   "    if(%s_state == 1) abort();\n"
                   "    %s_state = 1;\n",
                init_name, init_name, init_name, init_name, init_name);
        for(i = 0; i < m->import_count; i++) {
            const ZirModule *dependency = m->imports[i].resolved_module;
            if(ModuleNeedsStartup(dependency)) {
                char name[LOWER_NAME_MAX];
                NativeCModuleInitName(dependency, name, sizeof(name));
                fprintf(c, "    %s();\n", name);
            }
        }
        for(i = 0; i < m->function_count; i++)
            if(m->functions[i].is_global_initializer) {
                char name[LOWER_NAME_MAX];
                function_c_name(m, &m->functions[i], name, sizeof(name));
                fprintf(c, "    %s();\n", name);
            }
        fprintf(c, "    %s_state = 2;\n}\n", init_name);
    }
    if(startup_count) {
        fprintf(c, "\n__attribute__((constructor)) static void\n"
                   "%s_constructor(void)\n{\n"
                   "    %s();\n}\n", init_name, init_name);
    }
    fprintf(c, "\n}\n");
    if(ferror(c) != 0 || fclose(c) != 0) {
        remove(ctemp);
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ source output: %s", cpath);
        return 0;
    }
    if(!drop_empty_extern_blocks(ctemp)) {
        remove(ctemp);
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ source output: %s", cpath);
        return 0;
    }
    if(GeneratedOutputReplace(ctemp, cpath) != 0) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot replace C++ source output: %s", cpath);
        return 0;
    }
    if(!EmitRuntimeHeaders(out_dir, cpath)) {
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot write runtime headers beside %s", cpath);
        return 0;
    }
    return 1;
}

int
cpp_lower(const ZirProgram *program, const char *root, const char *out_dir, const ZirCppModuleSyms *restab, int restab_count)
{
    int i;

    (void)root;
    if(program == NULL)
        return 0;
    for(i = 0; i < program->module_count; i++)
        if(!lower_module(&program->modules[i], restab, restab_count,
                         out_dir))
            return 0;
    return 1;
}

void
cpp_build_syms(const ZirProgram *program, ZirCppModuleSyms *out)
{
    int i, j;

    memset(out, 0, sizeof(*out));
    if(program == NULL || program->module_count == 0)
        return;
    /* module slash path: the module name with dots -> slashes ("ide.state"
     * -> "ide/state"), matching how #import targets name modules. */
    {
        size_t n = 0;
        const char *p = program->modules[0].name;

        for(; *p != '\0' && n + 1 < sizeof(out->module_slash); p++)
            out->module_slash[n++] = (*p == '.') ? '/' : *p;
        out->module_slash[n] = '\0';
    }
    /* module stem: the source path minus '.zi' — imports may name the
     * file path ('src/screens/settings/settings_theme') instead of the
     * dotted module name. */
    {
        const char *sp = program->modules[0].source_path;
        size_t n = strlen(sp);

        if(n > 3 && strcmp(sp + n - 3, ".zi") == 0)
            n -= 3;
        if(n >= sizeof(out->module_stem))
            n = sizeof(out->module_stem) - 1;
        memcpy(out->module_stem, sp, n);
        out->module_stem[n] = '\0';
    }
    for(i = 0; i < program->module_count && out->fn_count < 256; i++) {
        const ZirModule *m = &program->modules[i];

        for(j = 0; j < m->function_count && out->fn_count < 256; j++) {
            const ZirFunction *fn = &m->functions[j];
            if(fn->is_template) continue;

            snprintf(out->fns[out->fn_count].source,
                     sizeof(out->fns[0].source), "%s", fn->name);
            function_c_name(m, fn, out->fns[out->fn_count].c,
                            sizeof(out->fns[0].c));
            out->fn_count++;
        }
    }
}
