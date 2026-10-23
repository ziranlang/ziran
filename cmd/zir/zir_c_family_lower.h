#include "zir_scratch.h"
/* Shared C/C++ lowering implementation; select the native target in its wrapper. */
#if !ZIR_C_FAMILY_CPP
#define C_FAMILY_MODULE_SYMS ZirCModuleSyms
#define C_FAMILY_FUNCTION_SYM ZirCFunctionSym
#define C_FAMILY_FUNCTION_NAME c_function_name
#define C_FAMILY_BUILD_SYMS c_build_syms
#define C_FAMILY_FREE_SYMS c_free_syms
#define C_FAMILY_LOWER_IN_CONTEXT c_lower_in_context
#define C_FAMILY_LOWER c_lower
#define C_FAMILY_TARGET ZIR_C
#endif
#if ZIR_C_FAMILY_CPP
#define C_FAMILY_MODULE_SYMS ZirCppModuleSyms
#define C_FAMILY_FUNCTION_SYM ZirCppFunctionSym
#define C_FAMILY_FUNCTION_NAME cpp_function_name
#define C_FAMILY_BUILD_SYMS cpp_build_syms
#define C_FAMILY_FREE_SYMS cpp_free_syms
#define C_FAMILY_LOWER_IN_CONTEXT cpp_lower_in_context
#define C_FAMILY_LOWER cpp_lower
#define C_FAMILY_TARGET ZIR_CPP
#endif
#if ZIR_C_FAMILY_CPP
/*
 * zir_cpp_lower.c - ZIR to C++ backend. Lowers a ZirProgram to .cpp/.hpp
 * source files that can call declared C host functions. Emitted declarations
 * keep C linkage (extern "C") so generated C and C++ modules can interoperate.
 */
#else
/*
 * zir_c_lower.c - ZIR to C backend. Lowers a ZirProgram to .c/.h source files.
 * Source input is checked .zi or saved .zir.
 */
#endif
#if ZIR_C_FAMILY_CPP
#include "../zir-cpp/zir_cpp_lower.h"
#else
#include "../zir-c/zir_c_lower.h"
#include "../zir-c/zir_c_plan9.h"
#endif
#include "zir.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_runtime.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#if !ZIR_C_FAMILY_CPP
#include "zir_stream.h"
#endif

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
#if ZIR_C_FAMILY_CPP

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
#endif
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

#if !ZIR_C_FAMILY_CPP
static void
rewrite_self_pointer_type(const char *owner, char *base, size_t base_size)
{
    char work[LOWER_TEXT_MAX];
    char *p;
    char *star;
    size_t n;

    snprintf(work, sizeof(work), "%s", base);
    p = work;
    while(*p == ' ' || *p == '\t')
        p++;
    star = strrchr(p, '*');
    if(star == NULL)
        return;
    *star = '\0';
    n = strlen(p);
    while(n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
        p[--n] = '\0';
    if(strcmp(p, owner) != 0)
        return;
    snprintf(base, base_size, "struct %s*", owner);
}

#endif
static int is_module_alias(const ZirModule *m, const char *alias,
                           size_t alias_len);
static void function_c_name(const ZirModule *m, const ZirFunction *fn,
                            char *dst, size_t dst_size);
#if !ZIR_C_FAMILY_CPP
static void strip_alias_type(const ZirModule *m, const char *type,
                             char *dst, size_t dst_size);
#endif

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
    for(int i = 0; i < m->import_count; i++) {
        const ZirImport *foreign = &m->imports[i];
        if(foreign->kind == ZIR_IMPORT_EXTERN &&
           !strcmp(foreign->name, name) && !strncmp(foreign->target, "c.", 2)) {
            NativeCForeignName(m, foreign, dst, dst_size);
            return strlen(dst);
        }
        /* A host capability bound to a program export calls that export. */
        if(foreign->kind == ZIR_IMPORT_EXTERN &&
           foreign->extern_kind == ZIR_EXTERN_HOST &&
           !strncmp(foreign->target, "ziran:", 6) &&
           foreign->extern_symbol[0] != '\0' && !strcmp(foreign->name, name)) {
            snprintf(dst, dst_size, "%s", foreign->extern_symbol);
            return strlen(dst);
        }
    }
    if(ResolveFunction(m, name, &owner, &fn) == 1) {
        function_c_name(owner, fn, dst, dst_size);
        return strlen(dst);
    }
    return 0;
}

/* Resolve alias.fn( via the cross-module symbol table: find the import
 * named alias, get its target module, look up fn there. */
static size_t
resolve_aliased_fn(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab,
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

#if ZIR_C_FAMILY_CPP
/* Rewrite textual constants, global initializers, and type bounds. Function
 * bodies are emitted from checked expression graphs. */

static void
#else
/* Rewrite the remaining textual constant, global initializer, or type bound.
 * Function bodies are emitted from checked expression graphs. */
static int
#endif
rewrite_body2(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab,
              int restab_count, const char *src, char *dst, size_t dst_size)
{
    size_t n = 0;
#if !ZIR_C_FAMILY_CPP
    const char *p;
#endif

#if ZIR_C_FAMILY_CPP
    for(const char *p = src; *p != '\0' && n + 6 < dst_size; p++) {
#else
    for(p = src; *p != '\0' && n + 6 < dst_size; p++) {
#endif
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
#if ZIR_C_FAMILY_CPP
                   (p == src || !isalnum((unsigned char)p[-1])) &&
                   !isalnum((unsigned char)p[4]) && p[4] != '_') {
#else
           (p == src || !isalnum((unsigned char)p[-1])) &&
           !isalnum((unsigned char)p[4]) && p[4] != '_') {
#endif
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
#if ZIR_C_FAMILY_CPP
                       n + (size_t)written >= dst_size) {
                        Diagnostic(enumeration->span, "zir_cpp.enum",
                                   "enum member target name exceeds output limit");
                        exit(1);
                    }
#else
                       n + (size_t)written >= dst_size) return 0;
#endif
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
                /* A call names the module's own procedure, even when a type of
                 * the same name is visible through that module's using imports. */
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
                char qualified[ZIR_NAME_MAX], native[LOWER_NAME_MAX * 2];
                int type_length = snprintf(qualified, sizeof(qualified),
                                           "%.*s", (int)(me - p), p);
                if(type_length >= 0 &&
                   (size_t)type_length < sizeof(qualified) &&
                   NativeTypeAtUse(m, qualified, native, sizeof(native))) {
                    size_t length = strlen(native);
#if ZIR_C_FAMILY_CPP
                    if(n + length >= dst_size) {
                        dst[n] = '\0';
                        return;
                    }
#else
                    if(n + length >= dst_size) return 0;
#endif
                    memcpy(dst + n, native, length);
                    n += length;
                    p = me - 1;
                    continue;
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
                    TargetGlobalName(m, C_FAMILY_TARGET, m->globals[i].name,
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
                    TargetDefineName(m, C_FAMILY_TARGET, m->defines[i].name,
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
#if ZIR_C_FAMILY_CPP
                        if(n + length >= dst_size) {
                            dst[n] = '\0';
                            return;
                        }
#else
                        if(n + length >= dst_size) return 0;
#endif
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
#if !ZIR_C_FAMILY_CPP
    return *p == '\0';
#endif
}

/* C function name: <module>_<name> (module dots -> underscores). */
static void
function_c_name(const ZirModule *m, const ZirFunction *fn,
                char *dst, size_t dst_size)
{
    NativeCFunctionName(m, fn, dst, dst_size);
}

void
C_FAMILY_FUNCTION_NAME(const ZirModule *m, const ZirFunction *fn,
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
#if ZIR_C_FAMILY_CPP
    if(*scalar && !strcmp(scalar,type) && TargetType(type,ZIR_C)) {
        snprintf(dst,dst_size,"%s",TargetType(type,ZIR_C)); return;
#else
    if(*scalar && !strcmp(scalar,type) && TargetType(type,C_FAMILY_TARGET)) {
        snprintf(dst,dst_size,"%s",TargetType(type,C_FAMILY_TARGET)); return;
#endif
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

#if ZIR_C_FAMILY_CPP
/* Convert Jai parameters to C++ and strip imported type qualifiers. */
#else
/* Convert Jai parameters to C and strip imported type qualifiers. */
#endif
static void
convert_args(const ZirModule *m, const ZirFunction *fn,
             const char *args, char *dst, size_t dst_size, int canonical_names)
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
                        TargetBindingName(fn, C_FAMILY_TARGET, name, binding,
                                          sizeof(binding));
                        copy_text(name, sizeof(name), binding);
                    }
                    strip_alias_type(m, ty, type, sizeof(type));
                    /* Parameter labels do not change the foreign ABI. */
                    if(canonical_names)
                        copy_text(name, sizeof(name), "_");
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
#if !ZIR_C_FAMILY_CPP
static void
c_rewrite_overflow(const char *path, int line)
{
    Diagnostic(Span(path != NULL ? path : "", line, 1), "zir_c.body",
               "statement is too long to lower (buffer limit)");
    exit(1);
}

#endif
typedef struct BodySymbols {
    const ZirModule *module;
    const C_FAMILY_MODULE_SYMS *symbols;
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
        TargetGlobalName(owner, C_FAMILY_TARGET, global->name, out, size);
        return;
    }
    for(int i = 0; i < symbols->module->define_count; i++)
        if(strcmp(symbols->module->defines[i].name, text) == 0) {
            TargetDefineName(symbols->module, C_FAMILY_TARGET, text, out, size);
            return;
        }
    rewrite_body2(symbols->module, symbols->symbols, symbols->count, text, out, size);
}

static void
lower_body(FILE *out, const ZirModule *module, const C_FAMILY_MODULE_SYMS *symbols,
           int symbol_count, const ZirFunction *function)
{
    BodySymbols context = {module, symbols, symbol_count,
                           SpanPath(function->span)};
    if(!EmitBody(out, module, function, C_FAMILY_TARGET,
                 resolve_body_symbol, &context, NULL, NULL)) {
#if ZIR_C_FAMILY_CPP
        Diagnostic(function->span, "zir_cpp.body",
#else
        Diagnostic(function->span, "zir_c.body",
#endif
                   "function has no checked typed body: %s", function->name);
        exit(1);
    }
}

#if ZIR_C_FAMILY_CPP
/* '#if' regions stamp their captures with the expanded C preprocessor
 * condition; emit each guarded item wrapped in '#if cond / #endif'. */
#endif
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
extern_call_args(const ZirFunction *fn, const char *args, char *dst, size_t dst_size)
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
                char name[LOWER_NAME_MAX], binding[LOWER_NAME_MAX];
                size_t length = (size_t)(name_end - name_start);
                if(length >= sizeof(name)) length = sizeof(name) - 1;
                memcpy(name, name_start, length); name[length] = '\0';
                /* Use the same binding as the wrapper parameter declaration. */
                TargetBindingName(fn, C_FAMILY_TARGET, name, binding, sizeof(binding));
                n += (size_t)snprintf(dst + n, dst_size > n ? dst_size - n : 0,
                                      "%s%s", first ? "" : ", ", binding);
                first = 0;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        }
        p++;
    }
}
/* Buffers emit_extern_prototype keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitExternPrototypeBuffers {
    char cargs[LOWER_TEXT_MAX];
    char abi_args[LOWER_TEXT_MAX];
    char conv[LOWER_TEXT_MAX];
    char binding_args[LOWER_TEXT_MAX];
    ZirFunction abi;
    char call[LOWER_TEXT_MAX];
} EmitExternPrototypeBuffers;

static void emit_extern_prototype(FILE *c, const ZirModule *m, const ZirImport *imp);

/* A module owns each foreign alias and wrapper. The signature guard avoids
 * re-emitting that module's identical binding through multiple headers. */
static uint32_t
foreign_binding_hash(const char *symbol, const char *ret, const char *args)
{
    const char *parts[3];
    uint32_t hash = 2166136261u;
    int part;
    parts[0] = symbol; parts[1] = ret; parts[2] = args;
    for(part = 0; part < 3; part++) {
        const unsigned char *at;
        for(at = (const unsigned char *)parts[part]; *at != '\0'; at++)
            hash = (hash ^ *at) * 16777619u;
        hash = (hash ^ '|') * 16777619u;
    }
    return hash;
}

static void
emit_extern_prototype_with_buffers(FILE *c, const ZirModule *m, const ZirImport *imp, EmitExternPrototypeBuffers *buffers)
{
    char ret[LOWER_NAME_MAX];
    char return_type[LOWER_NAME_MAX];
    char symbol[LOWER_NAME_MAX];
    const char *cname = imp->extern_kind == ZIR_EXTERN_HOST && !strncmp(imp->target, "ziran:", 6) &&
        imp->extern_symbol[0] != '\0' ? imp->extern_symbol : imp->name;
    extract_extern_signature(imp, ret, sizeof(ret), buffers->cargs, sizeof(buffers->cargs));
    strip_alias_type(m, ret, return_type, sizeof(return_type));
    copy_text(ret, sizeof(ret), return_type);
    memset(&buffers->abi, 0, sizeof(buffers->abi));
    buffers->abi.args_text = KeepParameters(buffers->cargs);
    copy_text(buffers->abi.return_type, sizeof(buffers->abi.return_type), ret);
    ArrayAbiArgs(&buffers->abi, buffers->abi_args, sizeof(buffers->abi_args));
    convert_args(m, &buffers->abi, buffers->abi_args, buffers->conv, sizeof(buffers->conv), 0);
    if(ArrayElementType(ret, NULL, 0, NULL))
        copy_text(ret, sizeof(ret), "void");
    if(c_extern_symbol(imp, symbol, sizeof(symbol))) {
        cname = symbol;
#if ZIR_C_FAMILY_CPP
        /* Use a local C++ name for the declaration. Standard headers may
         * already declare the ABI symbol with a different pointer spelling
         * (for example, renameat takes const char* rather than Ziran *u8).
         * The assembler label preserves the requested symbol at link time. */
#else
        /* Use a local C name for the declaration. Standard headers may
         * already declare the ABI symbol with a different pointer spelling
         * (for example, renameat takes const char* rather than Ziran *u8).
         * The assembler label preserves the requested symbol at link time. */
#endif
        char foreign_name[LOWER_NAME_MAX * 2];
        char local_name[LOWER_NAME_MAX];
        NativeCForeignName(m, imp, local_name, sizeof(local_name));
        convert_args(m, NULL, buffers->abi_args, buffers->binding_args, sizeof(buffers->binding_args), 1);
        uint32_t binding = foreign_binding_hash(symbol, ret[0] ? ret : "void", buffers->binding_args);
        snprintf(foreign_name, sizeof(foreign_name), "zir_foreign_%s", local_name);
        fprintf(c, "#ifndef ZIR_FOREIGN_%s_%08x\n#define ZIR_FOREIGN_%s_%08x\n",
                local_name, (unsigned)binding, local_name, (unsigned)binding);
#if ZIR_C_FAMILY_CPP
        EmitSymbolDeclaration(c, ret[0] ? ret : "void", foreign_name,
                              buffers->conv, symbol);
#else
        if(c_plan9_enabled())
            fprintf(c, "%s %s(%s) __asm__(\"%s\");\n",
                    ret[0] ? ret : "void", foreign_name, buffers->conv, symbol);
        else
            EmitSymbolDeclaration(c, ret[0] ? ret : "void", foreign_name,
                                  buffers->conv, symbol);
#endif
        if(imp->is_varargs) {
            /* A C variadic argument list cannot be forwarded by a
             * regular wrapper. Let the call target the ABI symbol. */
            fprintf(c, "#define %s %s\n#endif\n", local_name, foreign_name);
            return;
        }
        extern_call_args(&buffers->abi, buffers->abi_args, buffers->call, sizeof(buffers->call));
        fprintf(c, "static %s\n%s(%s)\n{\n",
                ret[0] ? ret : "void", local_name, buffers->conv);
        if(ret[0] != '\0' && strcmp(ret, "void") != 0)
            fprintf(c, "    return %s(%s);\n", foreign_name, buffers->call);
        else
            fprintf(c, "    %s(%s);\n", foreign_name, buffers->call);
        fprintf(c, "}\n#endif\n");
    } else {
        fprintf(c, "%s %s(%s);\n", ret[0] ? ret : "void", cname, buffers->conv);
    }
}

COMPILER_SCRATCH_VOID(static, emit_extern_prototype,
    (FILE *c, const ZirModule *m, const ZirImport *imp),
    EmitExternPrototypeBuffers, POOLED, free,
    emit_extern_prototype_with_buffers(c, m, imp, buffers))


static int
rewrite_global_scalar(const ZirModule *module, const char *source,
                      char *out, size_t size, void *context)
{
    (void)context;
#if ZIR_C_FAMILY_CPP
    rewrite_body2(module, NULL, 0, source, out, size);
    return out[0] != '\0';
#else
    return rewrite_body2(module, NULL, 0, source, out, size);
#endif
}
/* Buffers lower_module keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct LowerModuleBuffers {
#if !ZIR_C_FAMILY_CPP
    ZirExpr literal;
#endif
    char stem[512];
    char guard[600];
    char hpath[1024];
    char cpath[1024];
    char htemp[1100];
    char ctemp[1100];
    char value[LOWER_TEXT_MAX];
    char raw[LOWER_TEXT_MAX];
    char member[LOWER_TEXT_MAX];
    char mapped[LOWER_TEXT_MAX];
    char type[LOWER_TEXT_MAX];
    char base[LOWER_TEXT_MAX];
    char tmpb[LOWER_TEXT_MAX];
    char cargs[LOWER_TEXT_MAX];
    char abi_args[ZIR_TEXT_MAX];
    char initw[LOWER_TEXT_MAX * 2 + 64];
    char recordw[LOWER_TEXT_MAX * 2 + 64];
} LowerModuleBuffers;

#if ZIR_C_FAMILY_CPP
static int lower_module(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab, int restab_count,
             const char *out_dir);
#else
static int lower_module(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab,
             int restab_count, const char *out_dir, int linked);
#endif

static int
#if ZIR_C_FAMILY_CPP
lower_module_with_buffers(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab, int restab_count,
             const char *out_dir, LowerModuleBuffers *buffers)
#else
lower_module_with_buffers(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab,
             int restab_count, const char *out_dir, int linked, LowerModuleBuffers *buffers)
#endif
{
    FILE *h;
    FILE *c;
    int i;
#if !ZIR_C_FAMILY_CPP
    int needs_startup = ModuleNeedsStartup(m);
    char init_name[LOWER_NAME_MAX];
    NativeCModuleInitName(m, init_name, sizeof(init_name));
#endif
    stem_from_source(m->source_path, buffers->stem, sizeof(buffers->stem));
    NativeHeaderGuard(buffers->stem, buffers->guard, sizeof(buffers->guard));
#if ZIR_C_FAMILY_CPP
    snprintf(buffers->hpath, sizeof(buffers->hpath), "%s/%s.hpp", out_dir, buffers->stem);
    snprintf(buffers->cpath, sizeof(buffers->cpath), "%s/%s.cpp", out_dir, buffers->stem);
#else
    snprintf(buffers->hpath, sizeof(buffers->hpath), "%s/%s.h", out_dir, buffers->stem);
    snprintf(buffers->cpath, sizeof(buffers->cpath), "%s/%s.c", out_dir, buffers->stem);
#endif
    mkdir_parent(buffers->hpath);
#if !ZIR_C_FAMILY_CPP
    if(c_plan9_enabled() && c_plan9_write_runtime(out_dir) != 0) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot write Plan 9 runtime to output directory");
        return 0;
    }
#endif
    /* --- header --- */
    h = GeneratedOutputOpen(buffers->hpath, buffers->htemp, sizeof(buffers->htemp));
    if(h == NULL) {
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot create C++ header output: %s", buffers->hpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot create C header output: %s", buffers->hpath);
#endif
        return 0;
    }
#if ZIR_C_FAMILY_CPP
    fprintf(h, "/* Generated by zi2cpp from %s. */\n", m->source_path);
#else
    fprintf(h, "/* Generated by zi2c from %s. */\n", m->source_path);
#endif
    fprintf(h, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n", buffers->guard, buffers->guard);
    if(ModuleUsesSlices(m))
        fputs("#include \"zir_slice.h\"\n", h);
    EmitStringType(h);
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        int use = NativeHeaderImportUse(m, imp);
        if(!use) continue;
        if(use == 2) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        if(imp->kind == ZIR_IMPORT_OPEN) {
#if ZIR_C_FAMILY_CPP
            fprintf(h, "#include \"%s.hpp\"\n", imp->target);
#else
            fprintf(h, "#include \"%s.h\"\n", imp->target);
#endif
        } else if(imp->kind == ZIR_IMPORT_MODULE)
#if ZIR_C_FAMILY_CPP
            fprintf(h, "#include \"%s.hpp\"\n", imp->target);
#else
            fprintf(h, "#include \"%s.h\"\n", imp->target);
#endif
        if(use == 2) fputs("#endif\n", h);
    }
#if ZIR_C_FAMILY_CPP
    /* Generated decls keep C linkage; includes above guard themselves. */
    fprintf(h, "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n");
#else
    /* C modules can be embedded by C++ hosts; their exported symbols must
     * retain C linkage when the generated header is included by C++. */
    fputs("\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n", h);
#endif
    /* Compile-time constants first: headers reference them in
     * array bounds and extern declarations, and other modules use them
     * through the generated header -- a .c-only emission starves those. */
    for(i = 0; i < m->define_count; i++) {
        const ZirDefine *d = &m->defines[i];
        char name[LOWER_NAME_MAX];
        TargetDefineName(m, C_FAMILY_TARGET, d->name, name, sizeof(name));
#if ZIR_C_FAMILY_CPP
        rewrite_body2(m, NULL, 0, d->value, buffers->value, sizeof(buffers->value));
#endif
        if(!d->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
#if ZIR_C_FAMILY_CPP
        fprintf(h, "#define %s %s\n", name, buffers->value);
#else
        if(c_plan9_enabled() && d->value[0] == '"') {
            size_t length;
            if(!DecodeStringLiteral(d->value, (unsigned char *)buffers->raw,
                                    sizeof(buffers->raw), &length)) {
                Diagnostic(d->span, "zir_c.global", "cannot decode string constant");
                fclose(h);
                return 0;
            }
            /* 8c's macro-definition scanner also has a text limit. Keep one
             * named byte array, with its exact bound available to C hosts. */
            fprintf(h, "extern const char %s[%zu];\n", name, length + 1);
        } else {
            if(!rewrite_body2(m, NULL, 0, d->value, buffers->value, sizeof(buffers->value)))
                c_rewrite_overflow(m->source_path, d->span.line);
            fprintf(h, "#define %s %s\n", name, buffers->value);
        }
#endif
        if(!d->is_public) fputs("#endif\n", h);
    }
#if ZIR_C_FAMILY_CPP
    /* Callback signatures can reference records and enums owned by this
     * module. Declare those names before either kind of procedure value. */
#else
    /* Stored procedure values may appear in record fields and in #c_call
     * descriptors. Forward record names before defining any procedure
     * typedefs. */
#endif
    for(i = 0; i < m->type_count; i++) {
        const ZirType *ty = &m->types[i];
        char native[LOWER_NAME_MAX * 2];
        NativeTypeName(m, ty, native, sizeof(native));
#if ZIR_C_FAMILY_CPP
        if(ty->is_extern || ty->is_record_template ||
           (ty->is_procedure_type && !ty->is_enum))
            continue;
        if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
#endif
        if(!ty->is_extern && !ty->is_record_template &&
#if ZIR_C_FAMILY_CPP
           !ty->is_procedure_type && !ty->is_enum)
#else
           !ty->is_procedure_type && !ty->is_enum) {
            if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
            /* One checked type application can occur in several module
             * headers. C99 requires its typedef to be declared only once. */
            if(ty->is_synthetic_application)
                fprintf(h, "#ifndef ZIRAN_CONCRETE_FORWARD_%s\n#define ZIRAN_CONCRETE_FORWARD_%s\n",
                        native, native);
#endif
            fprintf(h, "typedef %s %s %s;\n",
                    ty->is_union ? "union" : "struct", native, native);
#if !ZIR_C_FAMILY_CPP
            if(ty->is_abi_incomplete)
                fprintf(h, "#pragma ziran abi_incomplete %s\n", native);
            if(ty->is_synthetic_application) fputs("#endif\n", h);
            if(!ty->is_public) fputs("#endif\n", h);
        }
#endif
        if(ty->is_enum) {
            const char *backing = enum_storage_type(ty->enum_backing);
            if(backing == NULL) {
#if ZIR_C_FAMILY_CPP
                Diagnostic(ty->span, "zir_cpp.enum", "invalid enum backing type");
#else
                Diagnostic(ty->span, "zir_c.enum", "invalid enum backing type");
#endif
                exit(1);
            }
#if ZIR_C_FAMILY_CPP
            if(ty->is_enum_flags)
                fprintf(h, "using %s = %s;\n", native, backing);
            else
                fprintf(h, "enum %s : %s;\n", native, backing);
#else
            fprintf(h, "typedef %s %s;\n", backing, native);
#endif
        }
#if ZIR_C_FAMILY_CPP
        if(!ty->is_public) fputs("#endif\n", h);
#endif
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *slot = &m->types[i];
        if(!slot->is_procedure_type)
            continue;
        if(!slot->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        EmitSlotType(h, slot, C_FAMILY_TARGET, resolve_slot_type, (void *)m);
        if(!slot->is_public) fputs("#endif\n", h);
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *ty = &m->types[i];
        char native[LOWER_NAME_MAX * 2];
        NativeTypeName(m, ty, native, sizeof(native));
        if(ty->is_extern || ty->is_record_template)
            continue;
        if(ty->is_procedure_type)
            continue;
        if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        if(ty->is_enum) {
            const char *backing = enum_storage_type(ty->enum_backing);
            if(backing == NULL) {
#if ZIR_C_FAMILY_CPP
                Diagnostic(ty->span, "zir_cpp.enum", "invalid enum backing type");
#else
                Diagnostic(ty->span, "zir_c.enum", "invalid enum backing type");
#endif
                exit(1);
            }
#if ZIR_C_FAMILY_CPP
            if(ty->is_enum_flags)
                fprintf(h, "\nenum : %s {\n", backing);
            else
                fprintf(h, "\nenum %s : %s {\n", native, backing);
#else
            fprintf(h, "\nenum {\n");
#endif
            {
                const char *line = ty->body;
                while(line != NULL && *line != '\0') {
                    const char *nl = strchr(line, '\n');
                    size_t len = nl ? (size_t)(nl - line) : strlen(line);
                    if(len > 0) {
                        if(len >= sizeof(buffers->raw))
                            len = sizeof(buffers->raw) - 1;
                        memcpy(buffers->raw, line, len);
                        buffers->raw[len] = '\0';
                        while(len > 0 && (buffers->raw[len - 1] == ' ' ||
                                          buffers->raw[len - 1] == ','))
                            buffers->raw[--len] = '\0';
                        if(buffers->raw[0] != '\0') {
                            size_t member_length = 0;
                            while(isalnum((unsigned char)buffers->raw[member_length]) ||
                                  buffers->raw[member_length] == '_')
                                member_length++;
                            if(member_length == 0) {
#if ZIR_C_FAMILY_CPP
                                Diagnostic(ty->span, "zir_cpp.enum",
#else
                                Diagnostic(ty->span, "zir_c.enum",
#endif
                                           "invalid enum member declaration");
                                exit(1);
                            }
                            memcpy(buffers->member, buffers->raw, member_length);
                            buffers->member[member_length] = '\0';
                            if(NativeEnumMemberName(m, ty, buffers->member, buffers->mapped,
                                                    sizeof(buffers->mapped)))
                                fprintf(h, "    %s%s,\n", buffers->mapped,
                                        buffers->raw + member_length);
                            else {
                                int prefixed = member_length >= strlen(ty->name) &&
                                    strncmp(buffers->raw, ty->name, strlen(ty->name)) == 0;
                                fprintf(h, "    %s%s%.*s%s,\n",
                                        prefixed ? "" : ty->name,
                                        prefixed ? "" : "_",
                                        (int)member_length, buffers->raw,
                                        buffers->raw + member_length);
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
        if(ty->is_synthetic_application)
            fprintf(h, "#ifndef ZIRAN_CONCRETE_%s\n#define ZIRAN_CONCRETE_%s\n",
                    native, native);
        fprintf(h, "\n%s%s %s {\n",
                TypeHasZeroArray(m, ty->name) ? "__extension__ " : "",
                ty->is_union ? "union" : "struct", native);
        /* Shared checked fields omit target-specific reflection metadata. */
        {
            size_t offset = 0;
            ZirTypeField field;
            int status;
            while((status = TypeNextField(ty, &offset, &field)) == 1) {
                char mapped[LOWER_NAME_MAX], suffix[LOWER_NAME_MAX];
                TargetFieldName(ty, C_FAMILY_TARGET, field.name, mapped, sizeof(mapped));
                split_array_type(field.type, buffers->base, sizeof(buffers->base),
                                 suffix, sizeof(suffix));
                strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
                snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
#if !ZIR_C_FAMILY_CPP
                rewrite_self_pointer_type(ty->name, buffers->base, sizeof(buffers->base));
#endif
                fprintf(h, "    %s %s%s;\n", buffers->base, mapped, suffix);
            }
            if(status < 0) {
                Diagnostic(ty->span, "check.record", "malformed field in %s", ty->name);
                exit(1);
            }
        }
        fprintf(h, "};\n");
        if(ty->is_synthetic_application) fputs("#endif\n", h);
        if(!ty->is_public) fputs("#endif\n", h);
    }
    /* Public file-scope variables have external linkage: declare extern in the header,
     * after every named type they reference. Private-scope globals stay
     * in the .c. */
    for(i = 0; i < m->global_count; i++) {
        const ZirGlobal *g = &m->globals[i];
        char suffix[LOWER_NAME_MAX];
        char name[LOWER_NAME_MAX];
        if(g->is_static)
            continue;
        TargetGlobalName(m, C_FAMILY_TARGET, g->name, name, sizeof(name));
        split_array_type(g->type, buffers->base, sizeof(buffers->base), suffix, sizeof(suffix));
        {
            strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
            snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];
#if ZIR_C_FAMILY_CPP
                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps));
#else
                if(!rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps)))
                    c_rewrite_overflow(m->source_path, g->span.line);
#endif
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        fprintf(h, "%sextern %s %s%s;\n",
                TypeHasZeroArray(m, g->type) ? "__extension__ " : "",
                buffers->base, name, suffix);
#if !ZIR_C_FAMILY_CPP
    }
    /* A public extern declaration is part of the generated module interface. */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->kind != ZIR_IMPORT_EXTERN || !imp->is_public ||
           imp->signature[0] == '\0')
            continue;
        emit_extern_prototype(h, m, imp);
#endif
    }
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cret[LOWER_NAME_MAX];
        if(fn->is_template || !fn->is_public)
            continue;   /* private functions are file-static */
        function_c_name(m, fn, cname, sizeof(cname));
        ArrayAbiArgs(fn, buffers->abi_args, sizeof(buffers->abi_args));
        convert_args(m, fn, buffers->abi_args, buffers->cargs, sizeof(buffers->cargs), 0);
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
        const char *symbol = NULL;
        if(fn->exported && !NativeProgramMain(m, fn)) {
            symbol = fn->export_symbol[0] ? fn->export_symbol : fn->name;
            if(!strcmp(cname, symbol)) symbol = NULL;
        }
#if ZIR_C_FAMILY_CPP
        if(symbol != NULL)
#else
        if(symbol != NULL && !c_plan9_enabled())
#endif
            EmitSymbolDeclaration(h, cret[0] ? cret : "void", cname,
                                  buffers->cargs, symbol);
        else {
            fprintf(h, "%s %s(%s)", cret[0] ? cret : "void", cname, buffers->cargs);
            if(symbol != NULL) fprintf(h, " __asm__(\"%s\")", symbol);
            fputs(";\n", h);
        }
#if ZIR_C_FAMILY_CPP
        if(NativeProgramMain(m, fn)) EmitNativeMainDeclaration(h, C_FAMILY_TARGET);
#else
        if(NativeProgramMain(m, fn)) {
            if(c_plan9_enabled()) c_plan9_main(h, m, fn, 1);
            else EmitNativeMainDeclaration(h, C_FAMILY_TARGET);
        }
#endif
    }
#if ZIR_C_FAMILY_CPP
    fprintf(h, "\n#ifdef __cplusplus\n}\n#endif\n");
    fprintf(h, "\n#endif /* %s */\n", buffers->guard);
#else
    fprintf(h, "\n#ifdef __cplusplus\n}\n#endif\n\n#endif /* %s */\n", buffers->guard);
#endif
    if(ferror(h) != 0 || fclose(h) != 0) {
        remove(buffers->htemp);
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ header output: %s", buffers->hpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C header output: %s", buffers->hpath);
#endif
        return 0;
    }
#if !ZIR_C_FAMILY_CPP
    c_plan9_rewrite_file(buffers->htemp);
#endif
    if(GeneratedOutputReplace(buffers->htemp, buffers->hpath) != 0) {
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot replace C++ header output: %s", buffers->hpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot replace C header output: %s", buffers->hpath);
#endif
        return 0;
    }
#if ZIR_C_FAMILY_CPP
    if(!EmitRuntimeHeaders(out_dir, buffers->hpath)) {
        Diagnostic(m->span, "zir_cpp.global",
#else
    if(!c_plan9_enabled() && !EmitRuntimeHeaders(out_dir, buffers->hpath)) {
        Diagnostic(m->span, "zir_c.global",
#endif
                   "cannot write runtime headers beside %s", buffers->hpath);
        return 0;
    }
#if !ZIR_C_FAMILY_CPP
    /* Type-only modules have a header but no translation unit to compile. */
    if(linked && m->function_count == 0 && m->global_count == 0 &&
       m->assert_count == 0 && !ModuleNeedsStartup(m))
        return 1;
#endif
    /* --- source --- */
#if ZIR_C_FAMILY_CPP
    c = GeneratedOutputOpen(buffers->cpath, buffers->ctemp, sizeof(buffers->ctemp));
#else
    TextStream stream;
    char *source_text = NULL;
    size_t source_size = 0;
    c = TextStreamOpen(&stream, &source_text, &source_size);
#endif
    if(c == NULL) {
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot create C++ source output: %s", buffers->cpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot create C source output: %s", buffers->cpath);
#endif
        return 0;
    }
#if ZIR_C_FAMILY_CPP
    fprintf(c, "/* Generated by zi2cpp from %s. */\n", m->source_path);
#else
    fprintf(c, "/* Generated by zi2c from %s. */\n", m->source_path);
#endif
    /* Runtime declarations precede public macros such as FILE. */
    fputs(RUNTIME_INCLUDES_MARK, c);
    if(ModuleUsesVecOperations(m))
        fputs("#include \"zir_vec.h\"\n", c);
    /* The module's own source sees its header's private declarations. */
    if(header_has_private(m))
#if ZIR_C_FAMILY_CPP
        fprintf(c, "#define %s_PRIVATE 1\n#include \"%s.hpp\"\n#undef %s_PRIVATE\n",
#else
        fprintf(c, "#define %s_PRIVATE 1\n#include \"%s.h\"\n#undef %s_PRIVATE\n",
#endif
                buffers->guard, buffers->stem, buffers->guard);
    else
#if ZIR_C_FAMILY_CPP
        fprintf(c, "#include \"%s.hpp\"\n", buffers->stem);
#else
        fprintf(c, "#include \"%s.h\"\n", buffers->stem);
#endif
    EmitNumbers(c, m, C_FAMILY_TARGET);
#if !ZIR_C_FAMILY_CPP
    if(c_plan9_enabled()) {
        for(i = 0; i < m->define_count; i++) {
            const ZirDefine *d = &m->defines[i];
            char name[LOWER_NAME_MAX];
            if(d->value[0] != '"') continue;
            TargetDefineName(m, C_FAMILY_TARGET, d->name, name, sizeof(name));
            memset(&buffers->literal, 0, sizeof(buffers->literal));
            buffers->literal.kind = ZIR_EXPR_STRING;
            buffers->literal.span = d->span;
            buffers->literal.text = d->value;
            EmitStringLiteral(&buffers->literal, C_FAMILY_TARGET, buffers->value, sizeof(buffers->value));
            fprintf(c, "const char %s[] = %s;\n", name, buffers->value);
        }
    }
#endif
    /* Private-scope imports include here (implementation-only). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->required || (imp->kind != ZIR_IMPORT_OPEN &&
                             imp->kind != ZIR_IMPORT_MODULE))
            continue;
#if ZIR_C_FAMILY_CPP
        fprintf(c, "#include \"%s.hpp\"\n", imp->target);
#else
        fprintf(c, "#include \"%s.h\"\n", imp->target);
#endif
    }
#if ZIR_C_FAMILY_CPP
    fprintf(c, "\nextern \"C\" {\n");
    /* Module constants lowered to C++ preprocessor constants. */
    for(i = 0; i < m->define_count; i++) {
        const ZirDefine *d = &m->defines[i];
        char name[LOWER_NAME_MAX];
        TargetDefineName(m, C_FAMILY_TARGET, d->name, name, sizeof(name));
        rewrite_body2(m, NULL, 0, d->value, buffers->value, sizeof(buffers->value));
        fprintf(c, "#define %s %s\n", name, buffers->value);
    }
#else
    /* The module header already supplies its public and private constants.
     * Repeating them here is rejected by native Plan 9's preprocessor. */
#endif
    /* #foreign imports: emit C prototypes parsed from the raw signature
     * ('name :: (args) -> Ret #foreign library;'). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
#if ZIR_C_FAMILY_CPP
        if(imp->kind != ZIR_IMPORT_EXTERN || imp->signature[0] == '\0')
#else
        if(imp->kind != ZIR_IMPORT_EXTERN || imp->is_public ||
           imp->signature[0] == '\0')
#endif
            continue;
        emit_extern_prototype(c, m, imp);
    }
    /* Forward prototypes for private functions: initializers and earlier
     * definitions may reference them before their definition. */
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cret[LOWER_NAME_MAX];
        if(fn->is_template || fn->is_public || fn->is_extern)
            continue;
        function_c_name(m, fn, cname, sizeof(cname));
        ArrayAbiArgs(fn, buffers->abi_args, sizeof(buffers->abi_args));
        convert_args(m, fn, buffers->abi_args, buffers->cargs, sizeof(buffers->cargs), 0);
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
#if ZIR_C_FAMILY_CPP
        fprintf(c, "[[maybe_unused]] static %s %s(%s);\n",
#else
        fprintf(c, "static %s %s(%s);\n",
#endif
                cret[0] ? cret : "void", cname, buffers->cargs);
    }
    for(i = 0; i < m->global_count; i++) {
        BodySymbols symbols = {m, restab, restab_count, SpanPath(m->globals[i].span)};
        EmitGlobalSlotWrappers(c, m, &m->globals[i], C_FAMILY_TARGET,
                               resolve_body_symbol, &symbols);
    }
    for(i = 0; i < m->global_count; i++) {
        const ZirGlobal *g = &m->globals[i];
        char suffix[LOWER_NAME_MAX];
        char name[LOWER_NAME_MAX];
        TargetGlobalName(m, C_FAMILY_TARGET, g->name, name, sizeof(name));
        split_array_type(g->type, buffers->base, sizeof(buffers->base), suffix, sizeof(suffix));
        {
            strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
            snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];
                /* the alias sits inside brackets ('[state.MAX]'), so use the
                 * body rewriter (strips alias.member anywhere), not the
                 * leading-alias-only type strip */
#if ZIR_C_FAMILY_CPP
                rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps));
#else
                if(!rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps)))
                    c_rewrite_overflow(m->source_path, g->span.line);
#endif
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        {
            buffers->initw[0] = '\0';
            /* initializers carry 'null' and module-local function refs */
            if(!ScalarLiteral(g->type, g->init, C_FAMILY_TARGET, g->span, buffers->initw, sizeof(buffers->initw))) {
                buffers->recordw[0] = '\0';
                int compound = EmitGlobalInitializer(m, g, C_FAMILY_TARGET,
                    rewrite_global_scalar, NULL, NULL, NULL, buffers->recordw,
                    sizeof(buffers->recordw));
                if(compound < 0) {
#if ZIR_C_FAMILY_CPP
                    Diagnostic(g->span, "zir_cpp.global",
#else
                    Diagnostic(g->span, "zir_c.global",
#endif
                               "cannot lower compound global initializer: %s",
                               g->name);
                    exit(1);
                }
                if(compound > 0)
                    snprintf(buffers->initw, sizeof(buffers->initw), "%s", buffers->recordw);
#if ZIR_C_FAMILY_CPP
                else
                    rewrite_body2(m, NULL, 0, g->init, buffers->initw, sizeof(buffers->initw));
#else
                else if(!rewrite_body2(m, NULL, 0, g->init, buffers->initw,
                                       sizeof(buffers->initw)))
                    c_rewrite_overflow(m->source_path, g->span.line);
#endif
            }
            int zero = TypeHasZeroArray(m, g->type);
            fprintf(c, "%s%s%s %s%s = %s;\n",
                    zero ? "__extension__ " : "",
                    g->is_static ? "static " : "", buffers->base, name, suffix,
#if ZIR_C_FAMILY_CPP
                    buffers->initw[0] ? buffers->initw : "{}");
#else
                    buffers->initw[0] ? buffers->initw : zero ? "{}" : "{0}");
#endif
        }
    }
#if !ZIR_C_FAMILY_CPP
    if(c_plan9_enabled() && needs_startup)
        fprintf(c, "\nstatic int %s_state;\nvoid %s(void);\n", init_name, init_name);
#endif
    for(i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX];
        char cret[LOWER_NAME_MAX];
        if(fn->is_template) continue;
        function_c_name(m, fn, cname, sizeof(cname));
        ArrayAbiArgs(fn, buffers->abi_args, sizeof(buffers->abi_args));
        convert_args(m, fn, buffers->abi_args, buffers->cargs, sizeof(buffers->cargs), 0);
        strip_alias_type(m, ArrayElementType(fn->return_type, NULL, 0, NULL) ? "void" : fn->return_type,
                         cret, sizeof(cret));
        if(fn->is_extern) {
            /* extern: prototype only, no body */
            fprintf(c, "\n");
            fprintf(c, "%s %s(%s);\n",
                    cret[0] ? cret : "void", cname, buffers->cargs);
            continue;
        }
        fprintf(c, "\n");
#if ZIR_C_FAMILY_CPP
        EmitAssemblyBlockHelpers(c, m, fn, C_FAMILY_TARGET);
#else
        if(!c_plan9_enabled()) EmitAssemblyBlockHelpers(c, m, fn, C_FAMILY_TARGET);
#endif
        EmitParallelWorkers(c, m, fn, C_FAMILY_TARGET,
                           resolve_body_symbol, &(BodySymbols){
                               m, restab, restab_count});
        BodySymbols symbols = {m, restab, restab_count};
        EmitSlotWrappers(c, m, fn, C_FAMILY_TARGET, resolve_body_symbol, &symbols);
#if ZIR_C_FAMILY_CPP
        EmitAssemblyHelpers(c, m, fn, C_FAMILY_TARGET, cret, buffers->cargs);
#else
        if(!c_plan9_enabled()) EmitAssemblyHelpers(c, m, fn, C_FAMILY_TARGET, cret, buffers->cargs);
#endif
        if(fn->is_public)
            fprintf(c, "%s\n%s(%s)\n{\n", cret[0] ? cret : "void",
                    cname, buffers->cargs);
        else
#if ZIR_C_FAMILY_CPP
            fprintf(c, "[[maybe_unused]] static %s\n%s(%s)\n{\n",
#else
            fprintf(c, "static %s\n%s(%s)\n{\n",
#endif
                    cret[0] ? cret : "void", cname, buffers->cargs);
#if ZIR_C_FAMILY_CPP
        EmitAssembly(c, m, fn, C_FAMILY_TARGET, cret);
#else
        /* Native Plan 9 has no ELF constructor attributes. Native and exported entry
         * points initialize their dependency graph once. Calls made by an
         * initializer itself may use already initialized earlier globals. */
        if(c_plan9_enabled() && needs_startup && (fn->exported || NativeProgramMain(m, fn)))
            fprintf(c, "    if(%s_state == 0) %s();\n", init_name, init_name);
        if(!c_plan9_enabled()) EmitAssembly(c, m, fn, C_FAMILY_TARGET, cret);
#endif
        lower_body(c, m, restab, restab_count, fn);
        fprintf(c, "}\n");
#if ZIR_C_FAMILY_CPP
        if(NativeProgramMain(m, fn)) EmitNativeMain(c, m, fn, C_FAMILY_TARGET);
#else
        if(NativeProgramMain(m, fn)) {
            if(c_plan9_enabled()) c_plan9_main(c, m, fn, 0);
            else EmitNativeMain(c, m, fn, C_FAMILY_TARGET);
        }
#endif
    }
    int startup_count = 0;
    for(i = 0; i < m->function_count; i++)
        startup_count += m->functions[i].is_global_initializer;
#if ZIR_C_FAMILY_CPP
    char init_name[LOWER_NAME_MAX];
    NativeCModuleInitName(m, init_name, sizeof(init_name));
#endif
    /* A module with no globals to set up, directly or through imports,
     * needs no init function; callers skip it by the same test. */
#if ZIR_C_FAMILY_CPP
    if(ModuleNeedsStartup(m)) {
#else
    if(needs_startup) {
        fprintf(c, "\n#include <stdlib.h>\n");
#endif
        for(i = 0; i < m->import_count; i++) {
            const ZirModule *dependency = m->imports[i].resolved_module;
            if(ModuleNeedsStartup(dependency)) {
                char name[LOWER_NAME_MAX];
                NativeCModuleInitName(dependency, name, sizeof(name));
                fprintf(c, "void %s(void);\n", name);
            }
        }
#if ZIR_C_FAMILY_CPP
        fprintf(c, "\nstatic int %s_state;\nvoid\n%s(void)\n{\n"
#else
        if(!c_plan9_enabled()) fprintf(c, "\nstatic int %s_state;", init_name);
        fprintf(c, "\nvoid\n%s(void)\n{\n"
#endif
                   "    if(%s_state == 2) return;\n"
                   "    if(%s_state == 1) abort();\n"
                   "    %s_state = 1;\n",
#if ZIR_C_FAMILY_CPP
                init_name, init_name, init_name, init_name, init_name);
#else
                init_name, init_name, init_name, init_name);
#endif
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
#if ZIR_C_FAMILY_CPP
    if(startup_count) {
#else
    if(startup_count && !c_plan9_enabled()) {
#endif
        fprintf(c, "\n__attribute__((constructor)) static void\n"
                   "%s_constructor(void)\n{\n"
                   "    %s();\n}\n", init_name, init_name);
    }
#if ZIR_C_FAMILY_CPP
    fprintf(c, "\n}\n");
    if(ferror(c) != 0 || fclose(c) != 0) {
        remove(buffers->ctemp);
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ source output: %s", buffers->cpath);
#else
    if(TextStreamClose(&stream) != 0) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C source output: %s", buffers->cpath);
#endif
        return 0;
    }
#if ZIR_C_FAMILY_CPP
    if(!drop_empty_extern_blocks(buffers->ctemp) || EmitResolveNativeHelpers(buffers->ctemp) != 0) {
#else
    c = GeneratedOutputOpen(buffers->cpath, buffers->ctemp, sizeof(buffers->ctemp));
    int finished = c ? EmitNativeHelpers(c, source_text) : -1;
    free(source_text);
    if(c && fclose(c) != 0) finished = -1;
    if(finished != 0) {
#endif
        remove(buffers->ctemp);
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot finish C++ source output: %s", buffers->cpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C source output: %s", buffers->cpath);
#endif
        return 0;
    }
#if !ZIR_C_FAMILY_CPP
    c_plan9_rewrite_file(buffers->ctemp);
#endif
    if(GeneratedOutputReplace(buffers->ctemp, buffers->cpath) != 0) {
#if ZIR_C_FAMILY_CPP
        Diagnostic(m->span, "zir_cpp.global",
                   "cannot replace C++ source output: %s", buffers->cpath);
#else
        Diagnostic(m->span, "zir_c.global",
                   "cannot replace C source output: %s", buffers->cpath);
#endif
        return 0;
    }
#if ZIR_C_FAMILY_CPP
    if(!EmitRuntimeHeaders(out_dir, buffers->cpath)) {
        Diagnostic(m->span, "zir_cpp.global",
#else
    if(!c_plan9_enabled() && !EmitRuntimeHeaders(out_dir, buffers->cpath)) {
        Diagnostic(m->span, "zir_c.global",
#endif
                   "cannot write runtime headers beside %s", buffers->cpath);
        return 0;
    }
    return 1;
}

static int
#if ZIR_C_FAMILY_CPP
lower_module(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab, int restab_count,
             const char *out_dir)
#else
lower_module(const ZirModule *m, const C_FAMILY_MODULE_SYMS *restab,
             int restab_count, const char *out_dir, int linked)
#endif
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    LowerModuleBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    uint64_t profile_started = ProfileStart();
#if ZIR_C_FAMILY_CPP
    int returned = lower_module_with_buffers(m, restab, restab_count, out_dir, buffers);
    ProfileEnd("emit.cpp", profile_started);
#else
    int returned = lower_module_with_buffers(m, restab, restab_count, out_dir, linked, buffers);
    ProfileEnd("emit.c", profile_started);
#endif
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
    return returned;
}

int
#if ZIR_C_FAMILY_CPP
C_FAMILY_LOWER_IN_CONTEXT(const ZirProgram *program, const char *root, const char *out_dir, const C_FAMILY_MODULE_SYMS *restab, int restab_count)
#else
C_FAMILY_LOWER_IN_CONTEXT(const ZirProgram *program, const char *root, const char *out_dir,
        const C_FAMILY_MODULE_SYMS *restab, int restab_count, int linked)
#endif
{
    int i;

    (void)root;
    if(program == NULL)
        return 0;
#if ZIR_C_FAMILY_CPP
    if(!CheckTargetCapabilities(program, "cpp"))
#else
    if(!CheckTargetCapabilities(program, c_plan9_enabled() ? "plan9-c" : "c"))
#endif
        return 0;
    for(i = 0; i < program->module_count; i++)
        for(int f = 0; f < program->modules[i].function_count; f++) {
            const ZirFunction *fn = &program->modules[i].functions[f];
            if(!fn->is_template && NativeProgramMain(&program->modules[i], fn) &&
#if ZIR_C_FAMILY_CPP
               !CheckNativeEntry(fn, "cpp")) return 0;
#else
               !CheckNativeEntry(fn, c_plan9_enabled() ? "plan9-c" : "c")) return 0;
#endif
        }
    for(i = 0; i < program->module_count; i++)
        if(!lower_module(&program->modules[i], restab, restab_count,
#if ZIR_C_FAMILY_CPP
                         out_dir))
#else
                         out_dir, linked))
#endif
            return 0;
    return 1;
}

int
C_FAMILY_BUILD_SYMS(const ZirProgram *program, C_FAMILY_MODULE_SYMS *out)
{
    int i, j;
    int total = 0;

    memset(out, 0, sizeof(*out));
    if(program == NULL || program->module_count == 0)
        return 1;
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
    for(i = 0; i < program->module_count; i++)
        for(j = 0; j < program->modules[i].function_count; j++)
            if(!program->modules[i].functions[j].is_template)
                total++;
    out->fns = calloc(total > 0 ? (size_t)total : 1, sizeof(*out->fns));
    if(out->fns == NULL)
        return 0;
    for(i = 0; i < program->module_count; i++) {
        const ZirModule *m = &program->modules[i];

        for(j = 0; j < m->function_count; j++) {
            const ZirFunction *fn = &m->functions[j];
            if(fn->is_template) continue;

            snprintf(out->fns[out->fn_count].source,
                     sizeof(out->fns[0].source), "%s", fn->name);
            function_c_name(m, fn, out->fns[out->fn_count].c,
                            sizeof(out->fns[0].c));
            out->fn_count++;
        }
    }
    return 1;
}

void
C_FAMILY_FREE_SYMS(C_FAMILY_MODULE_SYMS *syms, int count)
{
    for(int i = 0; i < count; i++)
        free(syms[i].fns);
}

#if ZIR_C_FAMILY_CPP
int C_FAMILY_LOWER(const ZirProgram *program, const char *root, const char *out_dir, const C_FAMILY_MODULE_SYMS *restab, int restab_count)
#else
int C_FAMILY_LOWER(const ZirProgram *program, const char *root, const char *out_dir, const C_FAMILY_MODULE_SYMS *restab, int restab_count, int linked)
#endif
{
    CompilerContext *previous = CompilerContextSelect(program ? program->context : NULL);
#if ZIR_C_FAMILY_CPP
    int result = C_FAMILY_LOWER_IN_CONTEXT(program, root, out_dir, restab, restab_count);
#else
    int result = C_FAMILY_LOWER_IN_CONTEXT(program, root, out_dir, restab, restab_count, linked);
#endif
    CompilerContextSelect(previous);
    return result;
}
