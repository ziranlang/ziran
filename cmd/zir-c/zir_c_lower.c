/*
 * zir_c_lower.c - ZIR to C backend. Lowers a ZirProgram to .c/.h source files.
 * Source input is checked .zi or saved .zir.
 */
#include "zir_c_lower.h"
#include "zir_simd.h"
#include "zir.h"
#include "zir_scratch.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_runtime.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include "zir_stream.h"

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
#include <sys/stat.h>

#define LOWER_NAME_MAX 128
#define LOWER_TEXT_MAX 4096
/* A global's whole initializer, such as a table of translated strings. */
#define LOWER_GLOBAL_MAX (256 * 1024)

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

static int is_module_alias(const ZirModule *m, const char *alias,
                           size_t alias_len);
static void function_c_name(const ZirModule *m, const ZirFunction *fn,
                            char *dst, size_t dst_size);
static void strip_alias_type(const ZirModule *m, const char *type,
                             char *dst, size_t dst_size);

/* The program being lowered, for host capabilities bound to Ziran
 * providers in other modules. */
static const ZirProgram *lowering_program;

/* A host capability bound to an exported Ziran provider calls the
 * provider's C function, under whatever name collisions gave it. */
static int
bound_provider_c_name(const ZirImport *imp, char *dst, size_t dst_size)
{
    if(imp->extern_kind != ZIR_EXTERN_HOST || strncmp(imp->target, "ziran:", 6) != 0 ||
       imp->extern_symbol[0] == '\0')
        return 0;
    for(int m = 0; lowering_program != NULL && m < lowering_program->module_count; m++) {
        const ZirModule *module = &lowering_program->modules[m];
        if(strcmp(module->name, imp->target + 6) != 0)
            continue;
        for(int f = 0; f < module->function_count; f++)
            if(!module->functions[f].is_extern &&
               strcmp(module->functions[f].name, imp->extern_symbol) == 0) {
                function_c_name(module, &module->functions[f], dst, dst_size);
                return 1;
            }
    }
    snprintf(dst, dst_size, "%s", imp->extern_symbol);
    return 1;
}

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
    }
    if(ResolveFunction(m, name, &owner, &fn) == 1) {
        function_c_name(owner, fn, dst, dst_size);
        return strlen(dst);
    }
    /* A host capability bound to a program export calls that export. An
     * imported module's function of the same name still wins, as in a
     * bundle. */
    for(int i = 0; i < m->import_count; i++) {
        const ZirImport *foreign = &m->imports[i];
        if(foreign->kind == ZIR_IMPORT_EXTERN && !strcmp(foreign->name, name) &&
           bound_provider_c_name(foreign, dst, dst_size))
            return strlen(dst);
    }
    return 0;
}

/* Resolve alias.fn( via the cross-module symbol table: find the import
 * named alias, get its target module, look up fn there. */
static size_t
resolve_aliased_fn(const ZirModule *m, const ZirCModuleSyms *restab,
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

/* Rewrite the remaining textual constant, global initializer, or type bound.
 * Function bodies are emitted from checked expression graphs. */
/* Spells each integer literal in TEXT the way C reads it: _ separators and
 * 0b binary become decimal, with ULL past the s64 maximum. Strings,
 * characters and identifiers stay as written. Returns 0 when the result
 * does not fit SIZE bytes. */
static int
c_integer_spellings(char *text, size_t size)
{
    char *out = malloc(size);
    size_t used = 0;
    const char *read = text;
    if(out == NULL) return 0;
#define C_SPELLING_PUT(s, n) do { size_t put_n = (n); \
        if(used + put_n >= size) { free(out); return 0; } \
        memcpy(out + used, (s), put_n); used += put_n; } while(0)
    while(*read) {
        const char *start = read;
        if(*read == '"' || *read == '\'') {
            char quote = *read++;
            while(*read && *read != quote) read += read[0] == '\\' && read[1] ? 2 : 1;
            if(*read) read++;
            C_SPELLING_PUT(start, (size_t)(read - start));
            continue;
        }
        if(!isalnum((unsigned char)*read) && *read != '_') {
            C_SPELLING_PUT(read, 1);
            read++;
            continue;
        }
        while(isalnum((unsigned char)*read) || *read == '_') read++;
        size_t length = (size_t)(read - start), digits = length;
        while(digits > 1 && strchr("uUlL", start[digits - 1]) != NULL) digits--;
        char literal[96], spelling[32];
        unsigned long long value;
        int rewrite = isdigit((unsigned char)start[0]) && *read != '.' &&
            digits < sizeof(literal) &&
            (memchr(start, '_', length) != NULL ||
             (start[0] == '0' && (start[1] == 'b' || start[1] == 'B')));
        if(rewrite) {
            memcpy(literal, start, digits);
            literal[digits] = 0;
            rewrite = ZirIntegerLiteral(literal, &value);
        }
        if(!rewrite) {
            C_SPELLING_PUT(start, length);
            continue;
        }
        int n = snprintf(spelling, sizeof(spelling), "%llu%s", value,
                         digits == length && value > INT64_MAX ? "ULL" : "");
        C_SPELLING_PUT(spelling, (size_t)n);
        C_SPELLING_PUT(start + digits, length - digits);
    }
#undef C_SPELLING_PUT
    out[used] = 0;
    memcpy(text, out, used + 1);
    free(out);
    return 1;
}

static int
rewrite_body2(const ZirModule *m, const ZirCModuleSyms *restab,
              int restab_count, const char *src, char *dst, size_t dst_size)
{
    size_t n = 0;
    const char *p;

    for(p = src; *p != '\0' && n + 6 < dst_size; p++) {
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
                       n + (size_t)written >= dst_size) return 0;
                    memcpy(dst + n, mapped, (size_t)written);
                    n += (size_t)written;
                    p = end - 1;
                    goto next_character;
                }
            }
            if(*e == '.' && e[1] != '\0' &&
               !is_module_alias(m, p, (size_t)(e - p))) {
                /* module_name.Procedure( names a public procedure of a
                 * module reached only through other imports, as the
                 * checker's calls to std/vec's Text helpers do. */
                const char *m0 = e + 1, *me = m0;
                while(isalnum((unsigned char)*me) || *me == '_')
                    me++;
                char dotted[ZIR_NAME_MAX];
                const ZirModule *owner = NULL;
                const ZirFunction *callee = NULL;
                int length = snprintf(dotted, sizeof(dotted), "%.*s", (int)(me - p), p);
                if(*me == '(' && length > 0 && (size_t)length < sizeof(dotted) &&
                   ResolveFunction(m, dotted, &owner, &callee) == 1) {
                    char cname[LOWER_NAME_MAX * 3];
                    function_c_name(owner, callee, cname, sizeof(cname));
                    size_t clen = strlen(cname);
                    if(n + clen < dst_size) {
                        memcpy(dst + n, cname, clen);
                        n += clen;
                    }
                    p = me - 1;
                    continue;
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
                    if(n + length >= dst_size) return 0;
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
                    TargetGlobalName(m, ZIR_C, m->globals[i].name,
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
                    TargetDefineName(m, ZIR_C, m->defines[i].name,
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
                        if(n + length >= dst_size) return 0;
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
    return *p == '\0';
}

/* C function name: <module>_<name> (module dots -> underscores). */
static void
function_c_name(const ZirModule *m, const ZirFunction *fn,
                char *dst, size_t dst_size)
{
    NativeCFunctionName(m, fn, dst, dst_size);
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
    if(VectorType(type, NULL)) {
        copy_text(dst, dst_size, TargetType(type, ZIR_C));
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

/* Convert Jai parameters to C and strip imported type qualifiers. */
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
                        TargetBindingName(fn, ZIR_C, name, binding,
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
static void
c_rewrite_overflow(const char *path, int line)
{
    Diagnostic(Span(path != NULL ? path : "", line, 1), "zir_c.body",
               "statement is too long to lower (buffer limit)");
    exit(1);
}

typedef struct BodySymbols {
    const ZirModule *module;
    const ZirCModuleSyms *symbols;
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
        TargetGlobalName(owner, ZIR_C, global->name, out, size);
        return;
    }
    for(int i = 0; i < symbols->module->define_count; i++)
        if(strcmp(symbols->module->defines[i].name, text) == 0) {
            TargetDefineName(symbols->module, ZIR_C, text, out, size);
            return;
        }
    rewrite_body2(symbols->module, symbols->symbols, symbols->count, text, out, size);
}

static void
lower_body(FILE *out, const ZirModule *module, const ZirCModuleSyms *symbols,
           int symbol_count, const ZirFunction *function)
{
    BodySymbols context = {module, symbols, symbol_count,
                           SpanPath(function->span)};
    if(!EmitBody(out, module, function, ZIR_C,
                 resolve_body_symbol, &context, NULL, NULL)) {
        Diagnostic(function->span, "zir_c.body",
                   "function has no checked typed body: %s", function->name);
        exit(1);
    }
}

/* The x86-64 tiers a #target procedure gets a clone for, best first, with
 * the ziran_simd.h tier each needs above the build's own. */
static const struct {
    const char *name, *attribute;
    int tier;
} simd_tiers[] = {
    {"avx2", "ZI_TARGET_AVX2", 4},
    {"sse41", "ZI_TARGET_SSE41", 3},
    {"ssse3", "ZI_TARGET_SSSE3", 2},
};

/* The expressions a C compiler builds for one copy of fn: its own and,
 * at every call, those of each #inline procedure of the module it calls,
 * as the compiler copies those into it. */
static long
expanded_expressions(const ZirModule *m, const ZirFunction *fn, int depth)
{
    long total = fn->expr_count;
    if(depth >= 8)
        return total;
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *x = &fn->exprs[i];
        if(x->kind != ZIR_EXPR_CALL || x->name[0] == '\0' || strchr(x->name, '.'))
            continue;
        for(int f = 0; f < m->function_count; f++)
            if(&m->functions[f] != fn && m->functions[f].inlining == ZIR_INLINE_ALWAYS &&
               !strcmp(m->functions[f].name, x->name)) {
                total += expanded_expressions(m, &m->functions[f], depth + 1);
                break;
            }
    }
    return total;
}

enum {
    /* Clones this large take a C compiler longer than parsing the SIMD
     * header again in a unit of their own. */
    TIER_UNIT_EXPRESSIONS = 4000
};

/* Whether a module's #target clones build in translation units of their
 * own, one per tier, which a parallel C build compiles beside the module.
 * Each unit includes the module source with ZIRAN_TIER_UNIT set and keeps
 * its tier's clones and the module's file-scope procedures, so a module
 * with state of its own, which a unit would copy, stays one unit. */
static int
module_splits_tiers(const ZirModule *m)
{
    long targets = 0;
    if(EmitRunGuardsEnabled())
        return 0;
    for(int i = 0; i < m->global_count; i++)
        if(m->globals[i].is_static)
            return 0;
    for(int i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        if(fn->is_template || fn->is_extern)
            continue;
        if(fn->assembly_text && *fn->assembly_text)
            return 0;
        for(int s = 0; s < fn->stmt_count; s++)
            if(fn->stmts[s].is_parallel || fn->stmts[s].is_gpu ||
               StatementHasAssembly(&fn->stmts[s]))
                return 0;
        if(fn->simd_target)
            targets += expanded_expressions(m, fn, 0);
    }
    return targets >= TIER_UNIT_EXPRESSIONS;
}

/* A #target procedure's clones, emitted before it when ziran_simd.h
 * dispatches: the same body with each tier's std/simd helpers. A module
 * that splits its tiers declares every clone and defines each only in
 * its tier's unit. */
static void
emit_target_clones(FILE *c, const ZirModule *m, const ZirCModuleSyms *restab,
                   int restab_count, const ZirFunction *fn, const char *cret,
                   const char *cname, const char *cargs, int split)
{
    fputs("#if defined(ZIRAN_SIMD_DISPATCH)\n", c);
    for(size_t t = 0; t < sizeof(simd_tiers) / sizeof(simd_tiers[0]); t++) {
        if(simd_tiers[t].tier < 4) fprintf(c, "#if ZI_BASE_TIER < %d\n", simd_tiers[t].tier);
        if(split)
            fprintf(c, "ZIRAN_TIER_CLONE %s %s %s_ziran_%s(%s);\n"
                       "#if defined(ZIRAN_TIER_UNIT) && ZIRAN_TIER_UNIT == %d\n"
                       "ZIRAN_TIER_CLONE %s %s\n%s_ziran_%s(%s)\n{\n",
                    simd_tiers[t].attribute, cret[0] ? cret : "void", cname,
                    simd_tiers[t].name, cargs, simd_tiers[t].tier,
                    simd_tiers[t].attribute, cret[0] ? cret : "void", cname,
                    simd_tiers[t].name, cargs);
        else
            fprintf(c, "static %s %s\n%s_ziran_%s(%s)\n{\n", simd_tiers[t].attribute,
                    cret[0] ? cret : "void", cname, simd_tiers[t].name, cargs);
        if(EmitRunGuardsEnabled())
            fputs("    ZIR_BUDGET();\n", c);
        EmitUseSimdTier(simd_tiers[t].name);
        lower_body(c, m, restab, restab_count, fn);
        EmitUseSimdTier(NULL);
        fputs("}\n", c);
        if(split) fputs("#endif\n", c);
        if(simd_tiers[t].tier < 4) fputs("#endif\n", c);
    }
    fputs("#endif\n", c);
}

/* The unit that builds one tier's clones of a split module. */
static int
write_tier_unit(const ZirModule *m, const char *cpath, const char *name, int tier)
{
    char path[ZIR_PATH_MAX], temp[ZIR_PATH_MAX];
    size_t length = strlen(cpath);
    if(length < 2 || snprintf(path, sizeof(path), "%.*s.%s.c", (int)(length - 2),
                              cpath, name) >= (int)sizeof(path))
        return 0;
    FILE *unit = GeneratedOutputOpen(path, temp, sizeof(temp));
    if(unit == NULL)
        return 0;
    const char *base = strrchr(cpath, '/');
    base = base != NULL ? base + 1 : cpath;
    fprintf(unit, "/* Generated by zi2c from %s. */\n"
                  "/* The %s clones of %s, which builds the rest of the module. */\n"
                  "#define ZIRAN_TIER_UNIT %d\n"
                  "#if defined(__GNUC__) || defined(__clang__)\n"
                  "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
                  "#pragma GCC diagnostic ignored \"-Wunused-variable\"\n"
                  "#pragma GCC diagnostic ignored \"-Wunused-const-variable\"\n"
                  "#endif\n"
                  "#include \"%s\"\n",
            m->source_path, name, base, tier, base);
    int failed = ferror(unit) != 0;
    if(fclose(unit) != 0 || failed) {
        remove(temp);
        return 0;
    }
    return GeneratedOutputReplace(temp, path) == 0;
}

/* The start of a #target procedure's body: the best clone this CPU runs, or
 * the body that follows when the build's own tier is as good. */
static void
emit_target_dispatch(FILE *c, const ZirFunction *fn, const char *cret,
                     const char *cname, const char *abi_args)
{
    const ZirParameters *parameters = ParametersOf(abi_args);
    int returns = cret[0] && strcmp(cret, "void");
    fputs("#if defined(ZIRAN_SIMD_DISPATCH)\n    switch(zi_simd_tier()) {\n", c);
    for(size_t t = 0; t < sizeof(simd_tiers) / sizeof(simd_tiers[0]); t++) {
        if(simd_tiers[t].tier < 4) fprintf(c, "#if ZI_BASE_TIER < %d\n", simd_tiers[t].tier);
        fprintf(c, "    case %d: %s%s_ziran_%s(", simd_tiers[t].tier, returns ? "return " : "",
                cname, simd_tiers[t].name);
        for(int i = 0; i < parameters->count; i++) {
            char name[LOWER_NAME_MAX];
            TargetBindingName(fn, ZIR_C, parameters->items[i].name, name, sizeof(name));
            fprintf(c, "%s%s", i ? ", " : "", name);
        }
        fprintf(c, ");%s\n", returns ? "" : " return;");
        if(simd_tiers[t].tier < 4) fputs("#endif\n", c);
    }
    fputs("    default: break;\n    }\n#endif\n", c);
}

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
                TargetBindingName(fn, ZIR_C, name, binding, sizeof(binding));
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
    char provider[LOWER_NAME_MAX];
    const char *cname = bound_provider_c_name(imp, provider, sizeof(provider)) ?
        provider : imp->name;
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
        /* Use a local C name for the declaration. Standard headers may
         * already declare the ABI symbol with a different pointer spelling
         * (for example, renameat takes const char* rather than Ziran *u8).
         * The assembler label preserves the requested symbol at link time. */
        char foreign_name[LOWER_NAME_MAX * 2];
        char local_name[LOWER_NAME_MAX];
        NativeCForeignName(m, imp, local_name, sizeof(local_name));
        convert_args(m, NULL, buffers->abi_args, buffers->binding_args, sizeof(buffers->binding_args), 1);
        uint32_t binding = foreign_binding_hash(symbol, ret[0] ? ret : "void", buffers->binding_args);
        snprintf(foreign_name, sizeof(foreign_name), "zir_foreign_%s", local_name);
        fprintf(c, "#ifndef ZIR_FOREIGN_%s_%08x\n#define ZIR_FOREIGN_%s_%08x\n",
                local_name, (unsigned)binding, local_name, (unsigned)binding);
        EmitSymbolDeclaration(c, ret[0] ? ret : "void", foreign_name,
                              buffers->conv, symbol);
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
    return rewrite_body2(module, NULL, 0, source, out, size);
}
/* Buffers lower_module keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
/* Small public procedures get a static inline copy in their module's
 * header, which a function-like macro routes other modules' calls to, so a
 * plain -O3 build can inline them across modules. The module's own source
 * defines MODULE_SOURCE and keeps the single external definition; taking the
 * procedure's address names that definition. A copy may only reference what
 * the header declares, which inline_candidate checks on the checked body and
 * inline_text_ok rechecks on the emitted C. */
enum { INLINE_STATEMENTS = 48, INLINE_EXPRESSIONS = 192, INLINE_TEXT = 8192 };

static int
has_word(const char *text, const char *word)
{
    size_t length = strlen(word);
    if(length == 0) return 0;
    for(const char *p = text; (p = strstr(p, word)) != NULL; p += length) {
        int before = p > text && (isalnum((unsigned char)p[-1]) || p[-1] == '_');
        int after = isalnum((unsigned char)p[length]) || p[length] == '_';
        if(!before && !after) return 1;
    }
    return 0;
}

static const ZirImport *
module_import(const ZirModule *m, const char *alias, size_t length)
{
    for(int i = 0; i < m->import_count; i++)
        if(m->imports[i].kind == ZIR_IMPORT_MODULE &&
           strlen(m->imports[i].name) == length &&
           !strncmp(m->imports[i].name, alias, length))
            return &m->imports[i];
    return NULL;
}

/* An imported module whose header this header includes unconditionally. */
static const ZirModule *
header_module(const ZirModule *m, const char *alias, size_t length)
{
    const ZirImport *imp = module_import(m, alias, length);
    if(imp == NULL || imp->resolved_module == NULL ||
       NativeHeaderImportUse(m, imp) != 1)
        return NULL;
    return imp->resolved_module;
}

static int
inline_type_ok(const ZirModule *m, const char *type)
{
    if(type == NULL || *type == '\0') return 1;
    for(int i = 0; i < m->type_count; i++)
        if(!m->types[i].is_public && has_word(type, m->types[i].name))
            return 0;
    for(const char *dot = strchr(type, '.'); dot != NULL; dot = strchr(dot + 1, '.')) {
        const char *start = dot;
        while(start > type && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
            start--;
        if(module_import(m, start, (size_t)(dot - start)) != NULL &&
           header_module(m, start, (size_t)(dot - start)) == NULL)
            return 0;
    }
    return 1;
}

static const ZirFunction *
public_function(const ZirModule *m, const char *name)
{
    for(int i = 0; m != NULL && i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        if(!strcmp(fn->name, name))
            return fn->is_public && !fn->is_template && !fn->is_extern ? fn : NULL;
    }
    return NULL;
}

/* A procedure called by its bare name: this module's own, or one that open
 * or `using` imports supply through headers this header includes, directly
 * or not. */
static const ZirFunction *
open_function(const ZirModule *m, const char *name, int depth)
{
    if(depth > 8) return NULL;
    for(int i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        const ZirFunction *fn;
        if((imp->kind != ZIR_IMPORT_OPEN && !(imp->kind == ZIR_IMPORT_MODULE && imp->is_using)) ||
           imp->resolved_module == NULL ||
           NativeHeaderImportUse(m, imp) != 1)
            continue;
        if((fn = public_function(imp->resolved_module, name)) != NULL ||
           (fn = open_function(imp->resolved_module, name, depth + 1)) != NULL)
            return fn;
    }
    return NULL;
}

static const ZirFunction *
header_function(const ZirModule *m, const char *name)
{
    for(int i = 0; i < m->function_count; i++)
        if(!strcmp(m->functions[i].name, name)) return public_function(m, name);
    return open_function(m, name, 0);
}

/* A name the header declares: a public procedure, define or global of M. */
static int
public_member(const ZirModule *m, const char *name)
{
    if(public_function(m, name) != NULL) return 1;
    for(int i = 0; i < m->define_count; i++)
        if(!strcmp(m->defines[i].name, name)) return m->defines[i].is_public;
    for(int i = 0; i < m->global_count; i++)
        if(!strcmp(m->globals[i].name, name)) return !m->globals[i].is_static;
    return 0;
}

/* A module header named like a standard header can be included by the C
 * library's own headers, before the declarations its inline copies need. */
static int
system_header_stem(const char *stem)
{
    static const char *const names[] = {
        "alloca", "assert", "complex", "ctype", "dirent", "endian", "errno", "fcntl",
        "features", "fenv", "float", "inttypes", "iso646", "limits", "locale", "math",
        "poll", "pthread", "sched", "setjmp", "signal", "stdalign", "stdarg",
        "stdatomic", "stdbool", "stddef", "stdint", "stdio", "stdlib", "stdnoreturn",
        "string", "strings", "tgmath", "threads", "time", "uchar", "unistd", "wchar",
        "wctype"
    };
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if(!strcmp(stem, names[i])) return 1;
    return 0;
}

static int
inline_candidate(const ZirModule *m, const ZirFunction *fn)
{
    if(m->name[0] == '\0' || !strcmp(m->name, "main") || EmitRunGuardsEnabled())
        return 0;
    if(fn->is_template || !fn->is_public || fn->is_extern || fn->exported ||
       fn->simd_target || fn->is_global_initializer || fn->uses_host || fn->go_method[0] ||
       (fn->assembly_text && *fn->assembly_text) || NativeProgramMain(m, fn) ||
       ArrayElementType(fn->return_type, NULL, 0, NULL) || fn->inlining == ZIR_INLINE_COLD ||
       (fn->inlining != ZIR_INLINE_ALWAYS &&
        (fn->stmt_count > INLINE_STATEMENTS || fn->expr_count > INLINE_EXPRESSIONS)))
        return 0;
    for(int i = 0; i < fn->stmt_count; i++) {
        const ZirStmt *st = &fn->stmts[i];
        if(st->is_parallel || st->is_gpu || st->is_using || StatementHasAssembly(st))
            return 0;
        switch(st->kind) {
        case ZIR_STMT_BLOCK_OPEN: case ZIR_STMT_BLOCK_CLOSE: case ZIR_STMT_DECL:
        case ZIR_STMT_ASSIGN: case ZIR_STMT_EXPR: case ZIR_STMT_IF:
        case ZIR_STMT_WHILE: case ZIR_STMT_FOR: case ZIR_STMT_RETURN:
        case ZIR_STMT_BREAK: case ZIR_STMT_CONTINUE:
            break;
        default:
            return 0;
        }
        if(st->kind == ZIR_STMT_DECL && !inline_type_ok(m, st->type))
            return 0;
    }
    for(int i = 0; i < fn->expr_count; i++) {
        const ZirExpr *e = &fn->exprs[i];
        const char *dot;
        if(e->is_function_value || (e->slot_type && *e->slot_type) ||
           !inline_type_ok(m, e->type))
            return 0;
        switch(e->kind) {
        case ZIR_EXPR_INT: case ZIR_EXPR_FLOAT: case ZIR_EXPR_BINARY:
        case ZIR_EXPR_UNARY: case ZIR_EXPR_POINTER_MEMBER: case ZIR_EXPR_INDEX:
        case ZIR_EXPR_CONDITIONAL:
            break;
        case ZIR_EXPR_CAST: case ZIR_EXPR_SIZE_OF:
            if(!inline_type_ok(m, e->name)) return 0;
            break;
        case ZIR_EXPR_IDENT:
            if(module_import(m, e->name, strlen(e->name)) != NULL) {
                if(header_module(m, e->name, strlen(e->name)) == NULL) return 0;
                break;
            }
            for(int j = 0; j < m->global_count; j++)
                if(!strcmp(m->globals[j].name, e->name) && m->globals[j].is_static)
                    return 0;
            for(int j = 0; j < m->define_count; j++)
                if(!strcmp(m->defines[j].name, e->name) && !m->defines[j].is_public)
                    return 0;
            break;
        case ZIR_EXPR_MEMBER:
            if(e->left >= 0 && fn->exprs[e->left].kind == ZIR_EXPR_IDENT) {
                const char *alias = fn->exprs[e->left].name;
                if(module_import(m, alias, strlen(alias)) != NULL &&
                   !public_member(header_module(m, alias, strlen(alias)), e->name))
                    return 0;
            }
            break;
        case ZIR_EXPR_CALL:
            if(BitBuiltin(e->name) != ZIR_BIT_NONE || SimdOpChecked(e->name) != NULL) break;
            dot = strchr(e->name, '.');
            if(dot == NULL ? header_function(m, e->name) == NULL :
               public_function(header_module(m, e->name, (size_t)(dot - e->name)), dot + 1) == NULL)
                return 0;
            break;
        default:
            return 0;
        }
    }
    return 1;
}

/* Names only this module's source or an unincluded header declares. */
typedef struct InlineNames {
    char **names;
    int count;
    int cap;
} InlineNames;

static void
inline_name(InlineNames *names, const char *name)
{
    if(name == NULL || *name == '\0') return;
    if(names->count == names->cap) {
        int cap = names->cap ? names->cap * 2 : 64;
        char **grown = realloc(names->names, (size_t)cap * sizeof(*grown));
        if(grown == NULL) { perror("realloc"); exit(1); }
        names->names = grown;
        names->cap = cap;
    }
    names->names[names->count] = strdup(name);
    if(names->names[names->count] == NULL) { perror("strdup"); exit(1); }
    names->count++;
}

static void
inline_module_names(InlineNames *names, const ZirModule *m, int private_only)
{
    char name[LOWER_NAME_MAX * 2];
    for(int i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        if(fn->is_template || (private_only && fn->is_public)) continue;
        function_c_name(m, fn, name, sizeof(name));
        inline_name(names, name);
    }
    for(int i = 0; i < m->type_count; i++) {
        if(private_only && m->types[i].is_public) continue;
        NativeTypeName(m, &m->types[i], name, sizeof(name));
        inline_name(names, name);
    }
    for(int i = 0; i < m->define_count; i++) {
        if(private_only && m->defines[i].is_public) continue;
        TargetDefineName(m, ZIR_C, m->defines[i].name, name, sizeof(name));
        inline_name(names, name);
    }
    for(int i = 0; i < m->global_count; i++) {
        if(private_only && !m->globals[i].is_static) continue;
        TargetGlobalName(m, ZIR_C, m->globals[i].name, name, sizeof(name));
        inline_name(names, name);
    }
    for(int i = 0; private_only && i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->kind == ZIR_IMPORT_EXTERN && !imp->is_public &&
           c_extern_symbol(imp, name, sizeof(name)))
            inline_name(names, name);
    }
}

/* Modules whose headers this header includes unconditionally, directly or
 * not; each is listed once. */
typedef struct HeaderModules {
    const ZirModule **items;
    int count;
    int cap;
} HeaderModules;

static int
header_modules_add(HeaderModules *set, const ZirModule *m)
{
    for(int i = 0; i < set->count; i++)
        if(set->items[i] == m) return 0;
    if(set->count == set->cap) {
        int cap = set->cap ? set->cap * 2 : 32;
        const ZirModule **grown = realloc(set->items, (size_t)cap * sizeof(*grown));
        if(grown == NULL) { perror("realloc"); exit(1); }
        set->items = grown;
        set->cap = cap;
    }
    set->items[set->count++] = m;
    return 1;
}

static void
header_modules_reached(HeaderModules *set, const ZirModule *m)
{
    for(int i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->resolved_module != NULL && NativeHeaderImportUse(m, imp) == 1 &&
           header_modules_add(set, imp->resolved_module))
            header_modules_reached(set, imp->resolved_module);
    }
}

static int
header_reaches(const ZirModule *from, const ZirModule *target)
{
    HeaderModules set = {0};
    header_modules_reached(&set, from);
    int found = 0;
    for(int i = 0; i < set.count && !found; i++)
        found = set.items[i] == target;
    free(set.items);
    return found;
}

/* When modules include each other's headers, whichever header the C source
 * reaches first is read before the others' declarations, so an inline copy
 * must not name anything from a module in its own include cycle. */
static void
inline_cycle_names(InlineNames *names, const ZirModule *m)
{
    HeaderModules set = {0};
    header_modules_reached(&set, m);
    for(int i = 0; i < set.count; i++)
        if(set.items[i] != m && header_reaches(set.items[i], m))
            inline_module_names(names, set.items[i], 0);
    free(set.items);
}

static int
inline_text_ok(const char *text, const InlineNames *names, int forced)
{
    if((!forced && strlen(text) > INLINE_TEXT) || has_word(text, "static"))
        return 0;
    for(const char *p = text; *p;) {
        if(!(isalpha((unsigned char)*p) || *p == '_')) { p++; continue; }
        const char *start = p;
        while(isalnum((unsigned char)*p) || *p == '_') p++;
        size_t length = (size_t)(p - start);
        for(int i = 0; i < names->count; i++)
            if(strlen(names->names[i]) == length && !memcmp(names->names[i], start, length))
                return 0;
    }
    return 1;
}

typedef struct LowerModuleBuffers LowerModuleBuffers;
static int emit_header_inlines(FILE *h, const ZirModule *m, const ZirCModuleSyms *restab,
                               int restab_count, LowerModuleBuffers *buffers);

typedef struct LowerModuleBuffers {
    char stem[512];
    char guard[600];
    char hpath[1024];
    char cpath[1024];
    char htemp[1100];
    char ctemp[1100];
    char value[LOWER_GLOBAL_MAX];
    char raw[LOWER_TEXT_MAX];
    char member[LOWER_TEXT_MAX];
    char mapped[LOWER_TEXT_MAX];
    char type[LOWER_TEXT_MAX];
    char base[LOWER_TEXT_MAX];
    char tmpb[LOWER_TEXT_MAX];
    char cargs[LOWER_TEXT_MAX];
    char abi_args[ZIR_TEXT_MAX];
    char initw[LOWER_GLOBAL_MAX];
} LowerModuleBuffers;

static int lower_module(const ZirModule *m, const ZirCModuleSyms *restab,
             int restab_count, const char *out_dir, int linked);

/* #inline and #cold attributes; other compilers see plain declarations. */
static void
emit_inlining_attributes(FILE *out, const ZirModule *m)
{
    int i;
    for(i = 0; i < m->function_count; i++)
        if(m->functions[i].inlining != ZIR_INLINE_NONE && !m->functions[i].is_template)
            break;
    if(i == m->function_count) return;
    fputs("#ifndef ZIRAN_ALWAYS_INLINE\n"
          "#if defined(__GNUC__) || defined(__clang__)\n"
          "#define ZIRAN_ALWAYS_INLINE __attribute__((always_inline))\n"
          "#define ZIRAN_COLD __attribute__((noinline, cold))\n"
          "#else\n"
          "#define ZIRAN_ALWAYS_INLINE\n"
          "#define ZIRAN_COLD\n"
          "#endif\n"
          "#endif\n", out);
}

/* What precedes a definition's result type after any static. A public
 * #inline definition stays external: the header declares it without
 * inline. */
static const char *
inlining_prefix(const ZirFunction *fn)
{
    if(fn->inlining == ZIR_INLINE_COLD) return "ZIRAN_COLD ";
    if(fn->inlining != ZIR_INLINE_ALWAYS) return "";
    return fn->is_public ? "ZIRAN_ALWAYS_INLINE inline " : "inline ZIRAN_ALWAYS_INLINE ";
}

static FILE *
inline_stream(TextStream *stream, char **text, size_t *size)
{
    FILE *file = TextStreamOpen(stream, text, size);
    if(file == NULL) { perror("inline header text"); exit(1); }
    return file;
}

static char *
inline_stream_close(TextStream *stream, char **text)
{
    if(TextStreamClose(stream) != 0) { perror("inline header text"); exit(1); }
    return *text;
}

/* Write the header's inline section; returns how many procedures it holds. */
static int
emit_header_inlines(FILE *h, const ZirModule *m, const ZirCModuleSyms *restab,
                    int restab_count, LowerModuleBuffers *buffers)
{
    InlineNames names = {0};
    TextStream declarations, bodies;
    char *declaration_text, *body_text;
    size_t declaration_size, body_size;
    FILE *declaration_file = NULL, *body_file = NULL;
    int count = 0;
    if(system_header_stem(buffers->stem)) return 0;
    for(int i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];
        char cname[LOWER_NAME_MAX], cret[LOWER_NAME_MAX];
        TextStream scratch, body;
        char *scratch_text, *text;
        size_t scratch_size, text_size;
        if(!inline_candidate(m, fn)) continue;
        if(declaration_file == NULL) {
            inline_module_names(&names, m, 1);
            for(int j = 0; j < m->import_count; j++) {
                const ZirImport *imp = &m->imports[j];
                if((imp->kind == ZIR_IMPORT_MODULE || imp->kind == ZIR_IMPORT_OPEN) &&
                   imp->resolved_module != NULL && NativeHeaderImportUse(m, imp) != 1)
                    inline_module_names(&names, imp->resolved_module, 0);
            }
            inline_cycle_names(&names, m);
            declaration_file = inline_stream(&declarations, &declaration_text, &declaration_size);
            body_file = inline_stream(&bodies, &body_text, &body_size);
        }
        function_c_name(m, fn, cname, sizeof(cname));
        ArrayAbiArgs(fn, buffers->abi_args, sizeof(buffers->abi_args));
        convert_args(m, fn, buffers->abi_args, buffers->cargs, sizeof(buffers->cargs), 0);
        strip_alias_type(m, fn->return_type, cret, sizeof(cret));
        /* A procedure that needs a helper beside its body stays out of line. */
        FILE *out = inline_stream(&scratch, &scratch_text, &scratch_size);
        BodySymbols symbols = {m, restab, restab_count};
        EmitAssemblyBlockHelpers(out, m, fn, ZIR_C);
        EmitParallelWorkers(out, m, fn, ZIR_C, resolve_body_symbol, &symbols);
        EmitSlotWrappers(out, m, fn, ZIR_C, resolve_body_symbol, &symbols);
        EmitAssemblyHelpers(out, m, fn, ZIR_C, cret, buffers->cargs);
        int alone = inline_stream_close(&scratch, &scratch_text)[0] == '\0';
        free(scratch_text);
        if(!alone) continue;
        out = inline_stream(&body, &text, &text_size);
        lower_body(out, m, restab, restab_count, fn);
        inline_stream_close(&body, &text);
        if(inline_text_ok(text, &names, fn->inlining == ZIR_INLINE_ALWAYS)) {
            const char *result = cret[0] ? cret : "void";
            const char *force = fn->inlining == ZIR_INLINE_ALWAYS ? "ZIRAN_ALWAYS_INLINE " : "";
            fprintf(declaration_file, "static inline %s%s %s_ziran_inline(%s);\n"
                    "#define %s(...) %s_ziran_inline(__VA_ARGS__)\n",
                    force, result, cname, buffers->cargs, cname, cname);
            fprintf(body_file, "\nstatic inline %s%s\n%s_ziran_inline(%s)\n{\n%s}\n",
                    force, result, cname, buffers->cargs, text);
            count++;
        }
        free(text);
    }
    for(int i = 0; i < names.count; i++) free(names.names[i]);
    free(names.names);
    if(declaration_file == NULL) return 0;
    inline_stream_close(&declarations, &declaration_text);
    inline_stream_close(&bodies, &body_text);
    if(count) {
        size_t size = strlen(RUNTIME_INCLUDES_MARK) + strlen(NUMBER_HELPERS_MARK) +
                      strlen(declaration_text) + strlen(body_text) + 1;
        char *section = malloc(size);
        if(section == NULL) { perror("malloc"); exit(1); }
        snprintf(section, size, "%s%s%s%s", RUNTIME_INCLUDES_MARK, NUMBER_HELPERS_MARK,
                 declaration_text, body_text);
        fprintf(h, "\n/* Small procedures other modules inline; %s.c defines them. */\n"
                   "#if !defined(__cplusplus) && !defined(%s_SOURCE)\n",
                buffers->stem, buffers->guard);
        if(ModuleUsesVecOperations(m))
            fputs("#include \"zir_vec.h\"\n", h);
        if(EmitNativeHelpers(h, section) != 0) { perror("inline header"); exit(1); }
        fputs("#endif\n", h);
        free(section);
    }
    free(declaration_text);
    free(body_text);
    return count;
}

static int
lower_module_with_buffers(const ZirModule *m, const ZirCModuleSyms *restab,
             int restab_count, const char *out_dir, int linked, LowerModuleBuffers *buffers)
{
    FILE *h;
    FILE *c;
    int i;
    int needs_startup = ModuleNeedsStartup(m);
    int split = module_splits_tiers(m);
    char init_name[LOWER_NAME_MAX];
    NativeCModuleInitName(m, init_name, sizeof(init_name));
    stem_from_source(m->source_path, buffers->stem, sizeof(buffers->stem));
    NativeHeaderGuard(buffers->stem, buffers->guard, sizeof(buffers->guard));
    snprintf(buffers->hpath, sizeof(buffers->hpath), "%s/%s.h", out_dir, buffers->stem);
    snprintf(buffers->cpath, sizeof(buffers->cpath), "%s/%s.c", out_dir, buffers->stem);
    mkdir_parent(buffers->hpath);
    /* --- header --- */
    h = GeneratedOutputOpen(buffers->hpath, buffers->htemp, sizeof(buffers->htemp));
    if(h == NULL) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot create C header output: %s", buffers->hpath);
        return 0;
    }
    fprintf(h, "/* Generated by zi2c from %s. */\n", m->source_path);
    fprintf(h, "#ifndef %s\n#define %s\n\n#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n", buffers->guard, buffers->guard);
    emit_inlining_attributes(h, m);
    if(ModuleUsesSlices(m))
        fputs("#include \"zir_slice.h\"\n", h);
    if(ModuleHeaderUsesVectors(m))
        fputs("#include \"ziran_simd.h\"\n", h);
    EmitStringType(h);
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        int use = NativeHeaderImportUse(m, imp);
        if(!use) continue;
        if(use == 2) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        if(imp->kind == ZIR_IMPORT_OPEN) {
            fprintf(h, "#include \"%s.h\"\n", imp->target);
        } else if(imp->kind == ZIR_IMPORT_MODULE)
            fprintf(h, "#include \"%s.h\"\n", imp->target);
        if(use == 2) fputs("#endif\n", h);
    }
    /* C modules can be embedded by C++ hosts; their exported symbols must
     * retain C linkage when the generated header is included by C++. */
    fputs("\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n", h);
    /* Compile-time constants first: headers reference them in
     * array bounds and extern declarations, and other modules use them
     * through the generated header -- a .c-only emission starves those. */
    for(i = 0; i < m->define_count; i++) {
        const ZirDefine *d = &m->defines[i];
        char name[LOWER_NAME_MAX];
        TargetDefineName(m, ZIR_C, d->name, name, sizeof(name));
        if(!d->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        if(!rewrite_body2(m, NULL, 0, d->value, buffers->value, sizeof(buffers->value)))
            c_rewrite_overflow(m->source_path, d->span.line);
        /* A table constant written over several lines stays one macro. */
        for(char *p = buffers->value; *p; p++)
            if(*p == '\n' || *p == '\r') *p = ' ';
        if(!c_integer_spellings(buffers->value, sizeof(buffers->value)))
            c_rewrite_overflow(m->source_path, d->span.line);
        fprintf(h, "#define %s %s\n", name, buffers->value);
        if(!d->is_public) fputs("#endif\n", h);
    }
    /* Stored procedure values may appear in record fields and in #c_call
     * descriptors. Forward record names before defining any procedure
     * typedefs. */
    for(i = 0; i < m->type_count; i++) {
        const ZirType *ty = &m->types[i];
        char native[LOWER_NAME_MAX * 2];
        NativeTypeName(m, ty, native, sizeof(native));
        if(!ty->is_extern && !ty->is_record_template &&
           !ty->is_procedure_type && !ty->is_enum) {
            if(!ty->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
            /* One checked type application can occur in several module
             * headers. C99 requires its typedef to be declared only once. */
            if(ty->is_synthetic_application)
                fprintf(h, "#ifndef ZIRAN_CONCRETE_FORWARD_%s\n#define ZIRAN_CONCRETE_FORWARD_%s\n",
                        native, native);
            fprintf(h, "typedef %s %s %s;\n",
                    ty->is_union ? "union" : "struct", native, native);
            if(ty->is_abi_incomplete)
                fprintf(h, "#pragma ziran abi_incomplete %s\n", native);
            if(ty->is_synthetic_application) fputs("#endif\n", h);
            if(!ty->is_public) fputs("#endif\n", h);
        }
        if(ty->is_enum) {
            const char *backing = enum_storage_type(ty->enum_backing);
            if(backing == NULL) {
                Diagnostic(ty->span, "zir_c.enum", "invalid enum backing type");
                exit(1);
            }
            fprintf(h, "typedef %s %s;\n", backing, native);
        }
    }
    for(i = 0; i < m->type_count; i++) {
        const ZirType *slot = &m->types[i];
        if(!slot->is_procedure_type)
            continue;
        if(!slot->is_public) fprintf(h, "#ifdef %s_PRIVATE\n", buffers->guard);
        EmitSlotType(h, slot, ZIR_C, resolve_slot_type, (void *)m);
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
                Diagnostic(ty->span, "zir_c.enum", "invalid enum backing type");
                exit(1);
            }
            fprintf(h, "\nenum {\n");
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
                                Diagnostic(ty->span, "zir_c.enum",
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
                TargetFieldName(ty, ZIR_C, field.name, mapped, sizeof(mapped));
                split_array_type(field.type, buffers->base, sizeof(buffers->base),
                                 suffix, sizeof(suffix));
                strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
                snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
                rewrite_self_pointer_type(ty->name, buffers->base, sizeof(buffers->base));
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
        TargetGlobalName(m, ZIR_C, g->name, name, sizeof(name));
        split_array_type(g->type, buffers->base, sizeof(buffers->base), suffix, sizeof(suffix));
        {
            strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
            snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];
                if(!rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps)))
                    c_rewrite_overflow(m->source_path, g->span.line);
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        fprintf(h, "%sextern %s %s%s;\n",
                TypeHasZeroArray(m, g->type) ? "__extension__ " : "",
                buffers->base, name, suffix);
    }
    /* A public extern declaration is part of the generated module interface. */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->kind != ZIR_IMPORT_EXTERN || !imp->is_public ||
           imp->signature[0] == '\0')
            continue;
        emit_extern_prototype(h, m, imp);
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
        if(symbol != NULL)
            EmitSymbolDeclaration(h, cret[0] ? cret : "void", cname,
                                  buffers->cargs, symbol);
        else
            fprintf(h, "%s%s %s(%s);\n", fn->inlining == ZIR_INLINE_COLD ? "ZIRAN_COLD " : "",
                    cret[0] ? cret : "void", cname, buffers->cargs);
        if(NativeProgramMain(m, fn))
            EmitNativeMainDeclaration(h);
    }
    int inlines = emit_header_inlines(h, m, restab, restab_count, buffers);
    fprintf(h, "\n#ifdef __cplusplus\n}\n#endif\n\n#endif /* %s */\n", buffers->guard);
    if(ferror(h) != 0 || fclose(h) != 0) {
        remove(buffers->htemp);
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C header output: %s", buffers->hpath);
        return 0;
    }
    if(GeneratedOutputReplace(buffers->htemp, buffers->hpath) != 0) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot replace C header output: %s", buffers->hpath);
        return 0;
    }
    if(!EmitRuntimeHeaders(out_dir, buffers->hpath)) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot write runtime headers beside %s", buffers->hpath);
        return 0;
    }
    /* Type-only modules have a header but no translation unit to compile. */
    if(linked && m->function_count == 0 && m->global_count == 0 &&
       m->assert_count == 0 && !ModuleNeedsStartup(m))
        return 1;
    /* --- source --- */
    TextStream stream;
    char *source_text = NULL;
    size_t source_size = 0;
    c = TextStreamOpen(&stream, &source_text, &source_size);
    if(c == NULL) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot create C source output: %s", buffers->cpath);
        return 0;
    }
    fprintf(c, "/* Generated by zi2c from %s. */\n", m->source_path);
    /* The module's own source keeps the one definition of each inline copy,
     * even when a system header includes this module's header first. */
    if(inlines)
        fprintf(c, "#define %s_SOURCE 1\n", buffers->guard);
    /* Runtime declarations precede public macros such as FILE. */
    fputs(RUNTIME_INCLUDES_MARK, c);
    if(ModuleUsesVecOperations(m))
        fputs("#include \"zir_vec.h\"\n", c);
    /* The module's own source sees its header's private declarations. */
    if(header_has_private(m))
        fprintf(c, "#define %s_PRIVATE 1\n#include \"%s.h\"\n#undef %s_PRIVATE\n",
                buffers->guard, buffers->stem, buffers->guard);
    else
        fprintf(c, "#include \"%s.h\"\n", buffers->stem);
    EmitNumbers(c, m, ZIR_C);
    fputs(CONSTANT_TABLES_MARK, c);
    EmitConstantTablesBegin(buffers->guard, ZIR_C);
    /* Private-scope imports include here (implementation-only). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->required || (imp->kind != ZIR_IMPORT_OPEN &&
                             imp->kind != ZIR_IMPORT_MODULE))
            continue;
        fprintf(c, "#include \"%s.h\"\n", imp->target);
    }
    /* The module header already supplies its public and private constants. */
    /* #foreign imports: emit C prototypes parsed from the raw signature
     * ('name :: (args) -> Ret #foreign library;'). */
    for(i = 0; i < m->import_count; i++) {
        const ZirImport *imp = &m->imports[i];
        if(imp->kind != ZIR_IMPORT_EXTERN || imp->is_public ||
           imp->signature[0] == '\0')
            continue;
        emit_extern_prototype(c, m, imp);
    }
    emit_inlining_attributes(c, m);
    for(i = 0; i < m->function_count; i++)
        if(m->functions[i].simd_target && !m->functions[i].is_template &&
           !m->functions[i].is_extern) {
            fputs("#define ZIRAN_SIMD_TIERS 1\n#include \"ziran_simd.h\"\n", c);
            break;
        }
    /* A tier unit's clones are hidden symbols the module source calls. */
    if(split)
        fputs("#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32) && !defined(__CYGWIN__)\n"
              "#define ZIRAN_TIER_CLONE __attribute__((visibility(\"hidden\")))\n"
              "#else\n"
              "#define ZIRAN_TIER_CLONE\n"
              "#endif\n", c);
    /* Forward prototypes for private functions: initializers and earlier
     * definitions may reference them before their definition. A private
     * procedure every caller inlined has no call left, which is no fault. */
    for(i = 0; i < m->function_count; i++)
        if(!m->functions[i].is_template && !m->functions[i].is_public && !m->functions[i].is_extern)
            break;
    if(i < m->function_count)
        fputs("#ifndef ZIRAN_MAYBE_UNUSED\n"
              "#if defined(__GNUC__) || defined(__clang__)\n"
              "#define ZIRAN_MAYBE_UNUSED __attribute__((unused))\n"
              "#else\n"
              "#define ZIRAN_MAYBE_UNUSED\n"
              "#endif\n"
              "#endif\n", c);
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
        fprintf(c, "static %s%s %s(%s) ZIRAN_MAYBE_UNUSED;\n", inlining_prefix(fn),
                cret[0] ? cret : "void", cname, buffers->cargs);
    }
    /* A tier unit leaves the module's variables and exported procedures to
     * the module source, which defines them once. */
    if(split)
        fputs("#ifndef ZIRAN_TIER_UNIT\n", c);
    for(i = 0; i < m->global_count; i++) {
        BodySymbols symbols = {m, restab, restab_count, SpanPath(m->globals[i].span)};
        EmitGlobalSlotWrappers(c, m, &m->globals[i], ZIR_C,
                               resolve_body_symbol, &symbols);
    }
    for(i = 0; i < m->global_count; i++) {
        const ZirGlobal *g = &m->globals[i];
        char suffix[LOWER_NAME_MAX];
        char name[LOWER_NAME_MAX];
        TargetGlobalName(m, ZIR_C, g->name, name, sizeof(name));
        split_array_type(g->type, buffers->base, sizeof(buffers->base), suffix, sizeof(suffix));
        {
            strip_alias_type(m, buffers->base, buffers->tmpb, sizeof(buffers->tmpb));
            snprintf(buffers->base, sizeof(buffers->base), "%s", buffers->tmpb);
            if(suffix[0] != '\0') {
                char tmps[LOWER_NAME_MAX];
                /* the alias sits inside brackets ('[state.MAX]'), so use the
                 * body rewriter (strips alias.member anywhere), not the
                 * leading-alias-only type strip */
                if(!rewrite_body2(m, NULL, 0, suffix, tmps, sizeof(tmps)))
                    c_rewrite_overflow(m->source_path, g->span.line);
                snprintf(suffix, sizeof(suffix), "%s", tmps);
            }
        }
        {
            buffers->initw[0] = '\0';
            char *compound_text = NULL;
            /* initializers carry 'null' and module-local function refs */
            if(!ScalarLiteral(g->type, g->init, ZIR_C, g->span, buffers->initw, sizeof(buffers->initw))) {
                int compound = 0;
                compound_text = EmitGlobalInitializerText(m, g, ZIR_C,
                    rewrite_global_scalar, NULL, NULL, NULL, &compound);
                if(compound < 0) {
                    Diagnostic(g->span, "zir_c.global",
                               "cannot lower compound global initializer: %s",
                               g->name);
                    exit(1);
                }
                if(compound == 0 &&
                   !rewrite_body2(m, NULL, 0, g->init, buffers->initw,
                                  sizeof(buffers->initw)))
                    c_rewrite_overflow(m->source_path, g->span.line);
            }
            int zero = TypeHasZeroArray(m, g->type);
            /* The native engine places module globals itself (ZIR_GLOBAL). */
            fprintf(c, "%s%s%s%s %s%s = %s;\n",
                    zero ? "__extension__ " : "",
                    g->is_static ? "static " : "",
                    EmitRunGuardsEnabled() ? "ZIR_GLOBAL " : "", buffers->base, name, suffix,
                    compound_text != NULL ? compound_text :
                    buffers->initw[0] ? buffers->initw : zero ? "{}" : "{0}");
            free(compound_text);
        }
    }
    if(split)
        fputs("#endif\n", c);
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
        EmitAssemblyBlockHelpers(c, m, fn, ZIR_C);
        EmitParallelWorkers(c, m, fn, ZIR_C,
                           resolve_body_symbol, &(BodySymbols){
                               m, restab, restab_count});
        BodySymbols symbols = {m, restab, restab_count};
        EmitSlotWrappers(c, m, fn, ZIR_C, resolve_body_symbol, &symbols);
        EmitAssemblyHelpers(c, m, fn, ZIR_C, cret, buffers->cargs);
        if(fn->simd_target)
            emit_target_clones(c, m, restab, restab_count, fn, cret, cname, buffers->cargs, split);
        if(split && fn->is_public)
            fputs("#ifndef ZIRAN_TIER_UNIT\n", c);
        if(fn->is_public)
            fprintf(c, "%s%s\n%s(%s)\n{\n", inlining_prefix(fn), cret[0] ? cret : "void",
                    cname, buffers->cargs);
        else
            fprintf(c, "static %s%s\n%s(%s)\n{\n", inlining_prefix(fn),
                    cret[0] ? cret : "void", cname, buffers->cargs);
        if(EmitRunGuardsEnabled())
            fputs("    ZIR_ENTER();\n", c);
        if(fn->simd_target)
            emit_target_dispatch(c, fn, cret, cname, buffers->abi_args);
        EmitAssembly(c, m, fn, ZIR_C, cret);
        lower_body(c, m, restab, restab_count, fn);
        fprintf(c, "}\n");
        if(NativeProgramMain(m, fn))
            EmitNativeMain(c, m, fn, ZIR_C);
        if(split && fn->is_public)
            fputs("#endif\n", c);
    }
    if(split)
        fputs("\n#ifndef ZIRAN_TIER_UNIT", c);
    int startup_count = 0;
    for(i = 0; i < m->function_count; i++)
        startup_count += m->functions[i].is_global_initializer;
    /* A module with no globals to set up, directly or through imports,
     * needs no init function; callers skip it by the same test. */
    if(needs_startup) {
        fprintf(c, "\n#include <stdlib.h>\n");
        for(i = 0; i < m->import_count; i++) {
            const ZirModule *dependency = m->imports[i].resolved_module;
            if(ModuleNeedsStartup(dependency)) {
                char name[LOWER_NAME_MAX];
                NativeCModuleInitName(dependency, name, sizeof(name));
                fprintf(c, "void %s(void);\n", name);
            }
        }
        fprintf(c, "\nstatic %sint %s_state;", EmitRunGuardsEnabled() ? "ZIR_GLOBAL " : "", init_name);
        fprintf(c, "\nvoid\n%s(void)\n{\n"
                   "    if(%s_state == 2) return;\n"
                   "    if(%s_state == 1) abort();\n"
                   "    %s_state = 1;\n",
                init_name, init_name, init_name, init_name);
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
    /* The native engine runs module initializers when an instance starts. */
    if(startup_count && !EmitRunGuardsEnabled()) {
        fprintf(c, "\n__attribute__((constructor)) static void\n"
                   "%s_constructor(void)\n{\n"
                   "    %s();\n}\n", init_name, init_name);
    }
    if(split)
        fputs("\n#endif\n", c);
    char *tables = EmitConstantTablesEnd();
    if(TextStreamClose(&stream) != 0) {
        free(tables);
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C source output: %s", buffers->cpath);
        return 0;
    }
    {
        /* Shared constant tables go before the first function. */
        char *mark = strstr(source_text, CONSTANT_TABLES_MARK);
        size_t before = mark ? (size_t)(mark - source_text) : 0;
        size_t added = tables ? strlen(tables) : 0;
        size_t after = mark ? strlen(mark + strlen(CONSTANT_TABLES_MARK)) : 0;
        char *joined = mark ? malloc(before + added + after + 1) : NULL;
        if(mark == NULL || joined == NULL) {
            free(tables);
            free(source_text);
            Diagnostic(m->span, "zir_c.global",
                       "cannot finish C source output: %s", buffers->cpath);
            return 0;
        }
        memcpy(joined, source_text, before);
        if(added) memcpy(joined + before, tables, added);
        memcpy(joined + before + added, mark + strlen(CONSTANT_TABLES_MARK), after + 1);
        free(tables);
        free(source_text);
        source_text = joined;
    }
    c = GeneratedOutputOpen(buffers->cpath, buffers->ctemp, sizeof(buffers->ctemp));
    int finished = c ? EmitNativeHelpers(c, source_text) : -1;
    free(source_text);
    if(c && fclose(c) != 0) finished = -1;
    if(finished != 0) {
        remove(buffers->ctemp);
        Diagnostic(m->span, "zir_c.global",
                   "cannot finish C source output: %s", buffers->cpath);
        return 0;
    }
    if(GeneratedOutputReplace(buffers->ctemp, buffers->cpath) != 0) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot replace C source output: %s", buffers->cpath);
        return 0;
    }
    if(!EmitRuntimeHeaders(out_dir, buffers->cpath)) {
        Diagnostic(m->span, "zir_c.global",
                   "cannot write runtime headers beside %s", buffers->cpath);
        return 0;
    }
    for(size_t t = 0; split && t < sizeof(simd_tiers) / sizeof(simd_tiers[0]); t++)
        if(!write_tier_unit(m, buffers->cpath, simd_tiers[t].name, simd_tiers[t].tier)) {
            Diagnostic(m->span, "zir_c.global",
                       "cannot write the %s unit beside %s", simd_tiers[t].name,
                       buffers->cpath);
            return 0;
        }
    return 1;
}

static int
lower_module(const ZirModule *m, const ZirCModuleSyms *restab,
             int restab_count, const char *out_dir, int linked)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    LowerModuleBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    uint64_t profile_started = ProfileStart();
    int returned = lower_module_with_buffers(m, restab, restab_count, out_dir, linked, buffers);
    ProfileEnd("emit.c", profile_started);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
    return returned;
}

int
c_lower_in_context(const ZirProgram *program, const char *root, const char *out_dir,
        const ZirCModuleSyms *restab, int restab_count, int linked)
{
    int i;

    (void)root;
    if(program == NULL)
        return 0;
    if(!CheckTargetCapabilities(program, "c"))
        return 0;
    for(i = 0; i < program->module_count; i++)
        for(int f = 0; f < program->modules[i].function_count; f++) {
            const ZirFunction *fn = &program->modules[i].functions[f];
            if(!fn->is_template && NativeProgramMain(&program->modules[i], fn) &&
               !CheckNativeEntry(fn, "c")) return 0;
        }
    lowering_program = program;
    for(i = 0; i < program->module_count; i++)
        if(!lower_module(&program->modules[i], restab, restab_count,
                         out_dir, linked)) {
            lowering_program = NULL;
            return 0;
        }
    lowering_program = NULL;
    return 1;
}

int
c_build_syms(const ZirProgram *program, ZirCModuleSyms *out)
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
c_free_syms(ZirCModuleSyms *syms, int count)
{
    for(int i = 0; i < count; i++)
        free(syms[i].fns);
}

int c_lower(const ZirProgram *program, const char *root, const char *out_dir, const ZirCModuleSyms *restab, int restab_count, int linked)
{
    CompilerContext *previous = CompilerContextSelect(program ? program->context : NULL);
    int result = c_lower_in_context(program, root, out_dir, restab, restab_count, linked);
    CompilerContextSelect(previous);
    return result;
}
