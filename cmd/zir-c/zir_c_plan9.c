/*
 * zir_c_plan9.c - 8c-safe post-pass for generated C.
 *
 * The native Plan 9 compiler accepts C89 plus a few extensions, but not
 * __auto_type, compound literals with designated initializers, partial
 * positional aggregate initializers, or declarations inside for(). zir_c
 * lowers Kry statements textually and has no type checker, so the
 * default output leans on those GNU forms. Under --plan9 this pass
 * rewrites the generated C before it is written:
 *
 *   __auto_type x = (Type){...};   ->  Type x = ...;   (type from cast)
 *   __auto_type x = fn(...);       ->  Ret x = fn(...); (header map)
 *   (Type){.a = 1, .b = 2}         ->  zeroed temp + field assignments
 *   (Type){0}                      ->  zeroed temp
 *   for(int i = 0; ...; ...) {...} ->  { int i; for(i = 0; ...) {...} }
 *
 * Each generated statement sits on one line, so the pass works
 * line-by-line exactly like the per-app scripts it replaces.
 */
#include "zir_c_plan9.h"
#include "zir.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAN9_MAX_INCLUDE_DIRS 16
#define PLAN9_MAX_PROTOS 8192
#define PLAN9_NAME_MAX 128
#define PLAN9_TYPE_MAX 128
#define PLAN9_LINE_MAX 65536
#define PLAN9_TEMP_PREFIX "__zir_c_p9"

typedef struct {
    char name[PLAN9_NAME_MAX];
    char type[PLAN9_TYPE_MAX];
} Plan9Proto;

static char include_dirs[PLAN9_MAX_INCLUDE_DIRS][1024];
static int include_dir_count;
static Plan9Proto protos[PLAN9_MAX_PROTOS];
static int proto_count;
static int protos_loaded;
static int unresolved_count;
static int plan9_enabled;

void
c_plan9_set_enabled(int enabled)
{
    plan9_enabled = enabled;
    unresolved_count = 0;
}

int
c_plan9_enabled(void)
{
    return plan9_enabled;
}

/* ------------------------------------------------------------------ */
/* Growable output buffer                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static int
buf_init(Buf *b)
{
    b->cap = 8192;
    b->len = 0;
    b->data = malloc(b->cap);
    if(b->data != NULL)
        b->data[0] = '\0';
    return b->data != NULL;
}

static int
buf_reserve(Buf *b, size_t n)
{
    if(b->data == NULL)
        return -1;
    if(b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        char *grown;
        while(b->len + n + 1 > cap)
            cap *= 2;
        grown = realloc(b->data, cap);
        if(grown == NULL)
            return -1;
        b->data = grown;
        b->cap = cap;
    }
    return 0;
}

static int
buf_append(Buf *b, const char *text, size_t n)
{
    if(buf_reserve(b, n) != 0)
        return -1;
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int
buf_puts(Buf *b, const char *text)
{
    return buf_append(b, text, strlen(text));
}

static int
buf_printf(Buf *b, const char *fmt, ...)
{
    va_list ap, measure;
    int n;

    /* Measure first, then format straight into the buffer: no stack copy
     * and no length limit. */
    va_start(ap, fmt);
    va_copy(measure, ap);
    n = vsnprintf(NULL, 0, fmt, measure);
    va_end(measure);
    if(n < 0 || buf_reserve(b, (size_t)n) != 0) {
        va_end(ap);
        return -1;
    }
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Prototype map: name -> return type, from project headers          */
/* ------------------------------------------------------------------ */

static int
skip_head_word(const char *p, const char *word)
{
    size_t n = strlen(word);

    if(strncmp(p, word, n) != 0)
        return 0;
    if(p[n] == ' ' || p[n] == '\t')
        return (int)n + 1;
    return 0;
}

static int
proto_line_parse(const char *line, char *name, size_t name_size,
                 char *type, size_t type_size)
{
    const char *p = line;
    const char *args;
    const char *semi;
    const char *name_start;
    const char *name_end;
    const char *type_end;
    const char *brace;
    int is_extern_var = 0;
    size_t n;
    int skipped;

    while(*p == ' ' || *p == '\t')
        p++;
    if(*p == '\0' || *p == '#' || *p == '/' || *p == '*')
        return 0;

    skipped = skip_head_word(p, "RLAPI");
    if(skipped == 0)
        skipped = skip_head_word(p, "KRYAPI");
    if(skipped == 0)
        skipped = skip_head_word(p, "extern");
    if(skipped == 0)
        skipped = skip_head_word(p, "static");
    if(skipped != 0)
        p += skipped;
    skipped = skip_head_word(p, "inline");
    if(skipped != 0)
        p += skipped;
    if(skipped > 7) {
        /* RLAPI/KRYAPI carry no meaning for variables */
    }

    /* cut at a body opener: covers "RET name(args) {" headers and
     * one-line inline definitions alike */
    semi = strchr(p, ';');
    brace = strchr(p, '{');
    if(brace != NULL && (semi == NULL || brace < semi))
        semi = brace;
    if(semi == NULL)
        return 0;
    args = strchr(p, '(');
    if((args == NULL || args > semi) && (semi - line) >= 1) {
        /* extern variable declaration: extern Type name; */
        const char *q = line;

        while(*q == ' ' || *q == '\t')
            q++;
        if(strncmp(q, "extern ", 7) != 0)
            return 0;
        is_extern_var = 1;
        args = NULL;
        name_end = semi;
        while(name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\t'))
            name_end--;
        name_start = name_end;
        while(name_start > p && (isalnum((unsigned char)name_start[-1])
                                 || name_start[-1] == '_'))
            name_start--;
        if(name_start == name_end)
            return 0;
        n = (size_t)(name_end - name_start);
        if(n >= name_size)
            return 0;
        memcpy(name, name_start, n);
        name[n] = '\0';
        type_end = name_start;
        while(type_end > p && (type_end[-1] == ' ' || type_end[-1] == '\t'))
            type_end--;
        n = (size_t)(type_end - p);
        if(n == 0 || n >= type_size)
            return 0;
        memcpy(type, p, n);
        type[n] = '\0';
        if(strpbrk(type, "=().,+/%<>!&|[]") != NULL)
            return 0;
        if(!(isalpha((unsigned char)type[0]) || type[0] == '_'))
            return 0;
        return 1;
    }
    (void)is_extern_var;
    if(args == NULL || args > semi || args == p)
        return 0;

    name_end = args;
    while(name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\t'
                           || name_end[-1] == '*'))
        name_end--;
    name_start = name_end;
    while(name_start > p && (isalnum((unsigned char)name_start[-1])
                             || name_start[-1] == '_'))
        name_start--;
    if(name_start == name_end)
        return 0;
    n = (size_t)(name_end - name_start);
    if(n >= name_size)
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';

    type_end = name_start;
    while(type_end > p && (type_end[-1] == ' ' || type_end[-1] == '\t'))
        type_end--;
    n = (size_t)(type_end - p);
    if(n == 0 || n >= type_size)
        return 0;
    memcpy(type, p, n);
    type[n] = '\0';
    if(strcmp(type, "void") == 0 || strcmp(type, "static") == 0)
        return 0;
    /* statement heads from inline bodies ("return fn(...)", "if (...)",
     * ...) parse like prototypes with a keyword where the type sits */
    {
        static const char * const keywords[] = {
            "return", "if", "for", "while", "switch", "case", "default",
            "goto", "do", "else", "sizeof", "break", "continue", "typedef",
        };
        size_t k;

        for(k = 0; k < sizeof(keywords) / sizeof(keywords[0]); k++) {
            if(strcmp(type, keywords[k]) == 0)
                return 0;
        }
    }
    /* the return type must read like one: identifiers, qualifiers,
     * struct/enum tags, stars. Anything else means the line was
     * expression text from an inline body (raymath-style headers). */
    if(strpbrk(type, "=().,+/%<>!&|[]") != NULL)
        return 0;
    if(!(isalpha((unsigned char)type[0]) || type[0] == '_'))
        return 0;
    return 1;
}

/* Literal classification for an initializer expression. */
static int
init_is_int_literal(const char *s)
{
    int digits = 0;

    if(*s == '+' || *s == '-')
        s++;
    while(isdigit((unsigned char)*s)) {
        s++;
        digits++;
    }
    if(digits == 0)
        return 0;
    if(*s == 'u' || *s == 'U')
        s++;
    if(*s == 'l' || *s == 'L')
        s++;
    return *s == '\0' || *s == ';' || *s == ' ' || *s == '\t';
}

static int
expr_looks_float(const char *s)
{
    const char *p;

    for(p = s; *p != '\0' && *p != ';'; p++) {
        if(isdigit((unsigned char)p[0]) && p[1] == '.')
            return 1;
        if(isdigit((unsigned char)p[0]) && (p[1] == 'f' || p[1] == 'F')
           && !isalnum((unsigned char)p[2]) && p[2] != '_')
            return 1;
    }
    return 0;
}

static void
proto_add_macro(const char *line)
{
    /* #define NAME 123  (object-like, numeric) */
    const char *p = line;
    const char *name_start;
    const char *name_end;
    char name[PLAN9_NAME_MAX];
    char value[64];
    size_t n;
    int i;

    if(strncmp(p, "#define ", 8) != 0)
        return;
    p += 8;
    while(*p == ' ' || *p == '\t')
        p++;
    name_start = p;
    while(*p != '\0' && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    if(*p != '(') {
        name_end = p;
        n = (size_t)(name_end - name_start);
        if(n == 0 || n >= sizeof(name))
            return;
        memcpy(name, name_start, n);
        name[n] = '\0';
        while(*p == ' ' || *p == '\t')
            p++;
        for(i = 0; p[i] != '\0' && p[i] != '\n' && i < (int)sizeof(value) - 1;
            i++)
            value[i] = p[i];
        value[i] = '\0';
        if(init_is_int_literal(value) || expr_looks_float(value)) {
            int j;
            int taken = 0;

            for(j = 0; j < proto_count; j++) {
                if(strcmp(protos[j].name, name) == 0) {
                    taken = 1;
                    break;
                }
            }
            if(!taken && proto_count < PLAN9_MAX_PROTOS) {
                snprintf(protos[proto_count].name,
                         sizeof(protos[0].name), "%s", name);
                snprintf(protos[proto_count].type,
                         sizeof(protos[0].type), "%s",
                         expr_looks_float(value) ? "float" : "int");
                proto_count++;
            }
        }
    }
}
/* Buffers proto_add_file keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ProtoAddFileBuffers {
    char pending[16384];
    char pending_text[16384];
    char line[8192];
    char joined[16384];
} ProtoAddFileBuffers;

static void proto_add_file(const char *path, int allow_c);

static void
proto_add_file_with_buffers(const char *path, int allow_c, ProtoAddFileBuffers *buffers)
{
    FILE *f;
    buffers->pending[0] = '\0';
    f = fopen(path, "r");
    if(f == NULL)
        return;
    while(fgets(buffers->line, sizeof(buffers->line), f) != NULL) {
        char name[PLAN9_NAME_MAX];
        char type[PLAN9_TYPE_MAX];
        int i;
        if(!allow_c) {
            proto_add_macro(buffers->line);
        } else {
            /* in .c files only static definition headers are recorded,
             * before any continuation joining so body lines never
             * accumulate into a candidate */
            const char *nospace = buffers->line + strspn(buffers->line, " \t");
            if(strncmp(nospace, "static ", 7) != 0)
                continue;
        }
        if(buffers->pending[0] != '\0') {
            snprintf(buffers->pending_text, sizeof(buffers->pending_text), "%s", buffers->pending);
            buffers->pending[0] = '\0';
            if(snprintf(buffers->joined, sizeof(buffers->joined), "%s%s", buffers->pending_text, buffers->line)
               >= (int)sizeof(buffers->joined))
                continue;
            /* a '{' after the arguments is a definition header: the
             * parser cuts at it */
            if(strchr(buffers->joined, ';') == NULL && strchr(buffers->joined, '{') == NULL) {
                if(strlen(buffers->joined) + 1 < sizeof(buffers->pending))
                    snprintf(buffers->pending, sizeof(buffers->pending), "%s", buffers->joined);
                continue;
            }
        } else {
            snprintf(buffers->joined, sizeof(buffers->joined), "%s", buffers->line);
            if(strchr(buffers->joined, ';') == NULL && strchr(buffers->joined, '{') == NULL
               && strlen(buffers->joined) + 1 < sizeof(buffers->pending)
               && (strchr(buffers->joined, '(') != NULL
                   || strncmp(buffers->joined + strspn(buffers->joined, " \t"),
                              "static ", 7) == 0)) {
                snprintf(buffers->pending, sizeof(buffers->pending), "%s", buffers->joined);
                continue;
            }
        }
        if(!proto_line_parse(buffers->joined, name, sizeof(name), type, sizeof(type)))
            continue;
        for(i = 0; i < proto_count; i++) {
            if(strcmp(protos[i].name, name) == 0)
                break;
        }
        if(i < proto_count)
            continue;
        if(proto_count >= PLAN9_MAX_PROTOS)
            break;
        snprintf(protos[proto_count].name, sizeof(protos[0].name), "%s", name);
        snprintf(protos[proto_count].type, sizeof(protos[0].type), "%s", type);
        proto_count++;
    }
    fclose(f);
}

static void
proto_add_file(const char *path, int allow_c)
{
    static _Thread_local ProtoAddFileBuffers *spares[16];
    static _Thread_local int spare_count;
    ProtoAddFileBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    proto_add_file_with_buffers(path, allow_c, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}

static void
proto_scan_dir(const char *path, int depth)
{
    DIR *dir;
    struct dirent *ent;

    if(depth > 4)
        return;
    dir = opendir(path);
    if(dir == NULL)
        return;
    while((ent = readdir(dir)) != NULL) {
        char child[2048];
        const char *dot;

        if(ent->d_name[0] == '.')
            continue;
        if(snprintf(child, sizeof(child), "%s/%s", path, ent->d_name)
           >= (int)sizeof(child))
            continue;
        dot = strrchr(ent->d_name, '.');
        if(dot != NULL && strcmp(dot, ".h") == 0)
            proto_add_file(child, 0);
        else if(dot != NULL && strcmp(dot, ".c") == 0)
            proto_add_file(child, 1);
        else if(dot == NULL)
            proto_scan_dir(child, depth + 1);
    }
    closedir(dir);
}

static const char *
proto_return_type(const char *name)
{
    static const struct {
        const char *name;
        const char *type;
    } builtins[] = {
        {"localtime", "struct tm *"},
        {"gmtime", "struct tm *"},
        {"clock", "long"},
        {"time", "long"},
        {"malloc", "void *"},
        {"calloc", "void *"},
        {"realloc", "void *"},
        {"getenv", "char *"},
        {"getcwd", "char *"},
        {"strerror", "char *"},
    };
    int i;

    for(i = 0; (size_t)i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if(strcmp(builtins[i].name, name) == 0)
            return builtins[i].type;
    }
    if(!protos_loaded) {
        int d;
        for(d = 0; d < include_dir_count; d++)
            proto_scan_dir(include_dirs[d], 0);
        protos_loaded = 1;
    }
    for(i = 0; i < proto_count; i++) {
        if(strcmp(protos[i].name, name) == 0)
            return protos[i].type;
    }
    return NULL;
}

void
c_plan9_add_include_dir(const char *dir)
{
    if(include_dir_count >= PLAN9_MAX_INCLUDE_DIRS)
        return;
    snprintf(include_dirs[include_dir_count],
             sizeof(include_dirs[0]), "%s", dir);
    include_dir_count++;
}

int
c_plan9_unresolved(void)
{
    return unresolved_count;
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                      */
/* ------------------------------------------------------------------ */

static int
rewrite_ziran_abi_marker(const char *line, Buf *out)
{
    static const char marker[] = "#pragma ziran abi_incomplete ";
    char name[PLAN9_NAME_MAX];
    const char *cursor;
    const char *end;
    size_t length;

    if(strncmp(line, marker, sizeof(marker) - 1) != 0)
        return 0;
    cursor = line + sizeof(marker) - 1;
    end = strchr(cursor, '\n');
    end = end != NULL ? end : cursor + strlen(cursor);
    while(end > cursor && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r'))
        end--;
    length = (size_t)(end - cursor);
    if(length == 0 || length >= sizeof(name))
        return -1;
    memcpy(name, cursor, length);
    name[length] = '\0';
    for(cursor = name; *cursor != '\0'; cursor++)
        if(!isalnum((unsigned char)*cursor) && *cursor != '_')
            return -1;
    return buf_printf(out, "#pragma incomplete %s\n", name) < 0 ? -1 : 1;
}

static int
line_passthrough(const char *line)
{
    const char *p = line;

    while(*p == ' ' || *p == '\t')
        p++;
    return *p == '\0' || *p == '\n' || *p == '#';
}

static int
brace_delta(const char *line)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    const char *p;

    for(p = line; *p != '\0'; p++) {
        char ch = *p;
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '{')
            depth++;
        else if(ch == '}')
            depth--;
    }
    return depth;
}

static int
find_matching_brace(const char *text, int open_index)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    int i;

    for(i = open_index; text[i] != '\0'; i++) {
        char ch = text[i];
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '{')
            depth++;
        else if(ch == '}') {
            depth--;
            if(depth == 0)
                return i;
        }
    }
    return -1;
}

static int
find_top_level_comma(const char *text, int start, int end)
{
    int paren = 0, brace = 0, bracket = 0;
    int in_string = 0;
    int escape = 0;
    int i;

    for(i = start; i < end; i++) {
        char ch = text[i];
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '(')
            paren++;
        else if(ch == ')')
            paren--;
        else if(ch == '{')
            brace++;
        else if(ch == '}')
            brace--;
        else if(ch == '[')
            bracket++;
        else if(ch == ']')
            bracket--;
        else if(ch == ',' && paren == 0 && brace == 0 && bracket == 0)
            return i;
    }
    return -1;
}

static int
copy_trimmed(const char *text, int start, int end, char *out, size_t out_size)
{
    size_t n;

    while(start < end && isspace((unsigned char)text[start]))
        start++;
    while(end > start && isspace((unsigned char)text[end - 1]))
        end--;
    n = (size_t)(end - start);
    if(n >= out_size)
        return -1;
    memcpy(out, text + start, n);
    out[n] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/* Compound literal flattening                                       */
/* ------------------------------------------------------------------ */

static int
body_all_designated(const char *body, int len)
{
    int pos = 0;

    while(pos < len) {
        int comma = find_top_level_comma(body, pos, len);
        int stop = comma < 0 ? len : comma;
        int i = pos;

        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '.')
            return 0;
        if(comma < 0)
            break;
        pos = comma + 1;
    }
    return 1;
}

static int
body_is_zero(const char *body, int len)
{
    int i;
    int seen = 0;

    for(i = 0; i < len; i++) {
        if(isspace((unsigned char)body[i]))
            continue;
        if(body[i] != '0')
            return 0;
        if(seen)
            return 0;
        seen = 1;
    }
    return seen;
}
/* Buffers emit_compound_temp keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct EmitCompoundTempBuffers {
    char value[PLAN9_LINE_MAX];
    char ibody[PLAN9_LINE_MAX];
    char inner[PLAN9_LINE_MAX];
} EmitCompoundTempBuffers;

static int emit_compound_temp(Buf *out, const char *indent, const char *type,
                   const char *body, int body_len, const char *temp);

static int
emit_compound_temp_with_buffers(Buf *out, const char *indent, const char *type,
                   const char *body, int body_len, const char *temp, EmitCompoundTempBuffers *buffers)
{
    char field[PLAN9_NAME_MAX];
    int pos = 0;
    const char *bracket = type != NULL ? strchr(type, '[') : NULL;
    if(bracket != NULL) {
        /* array literal: declare the temporary with its size */
        char base[PLAN9_TYPE_MAX];
        size_t n = (size_t)(bracket - type);
        if(n == 0 || n >= sizeof(base))
            return -1;
        memcpy(base, type, n);
        base[n] = '\0';
        if(buf_printf(out, "%s%s %s%s;\n", indent, base, temp, bracket) < 0)
            return -1;
        return buf_printf(out, "%smemset(%s, 0, sizeof(%s));\n",
                          indent, temp, temp);
    }
    if(body_is_zero(body, body_len)) {
        if(type == NULL)
            return 0; /* The enclosing aggregate has already been zeroed. */
        if(buf_printf(out, "%s%s %s;\n", indent, type, temp) < 0)
            return -1;
        return buf_printf(out, "%smemset(&%s, 0, sizeof(%s));\n",
                          indent, temp, temp);
    }
    if(!body_all_designated(body, body_len)) {
        if(type == NULL)
            return -1;
        /* full positional initializer in a declaration is 8c-safe */
        if(copy_trimmed(body, 0, body_len, buffers->value, sizeof(buffers->value)) < 0)
            return -1;
        return buf_printf(out, "%s%s %s = {%s};\n", indent, type, temp, buffers->value);
    }
    if(type != NULL) {
        if(buf_printf(out, "%s%s %s;\n", indent, type, temp) < 0)
            return -1;
        if(buf_printf(out, "%smemset(&%s, 0, sizeof(%s));\n", indent, temp, temp) < 0)
            return -1;
    }
    while(pos < body_len) {
        int comma = find_top_level_comma(body, pos, body_len);
        int stop = comma < 0 ? body_len : comma;
        int i = pos;
        int fstart;
        int fend;
        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '.')
            return -1;
        i++;
        fstart = i;
        while(i < stop && (isalnum((unsigned char)body[i]) || body[i] == '_'))
            i++;
        fend = i;
        if(fend == fstart || (size_t)(fend - fstart) >= sizeof(field))
            return -1;
        memcpy(field, body + fstart, (size_t)(fend - fstart));
        field[fend - fstart] = '\0';
        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '=')
            return -1;
        i++;
        if(copy_trimmed(body, i, stop, buffers->value, sizeof(buffers->value)) < 0)
            return -1;
        /* a field value that is itself a cast literal needs its own
         * temporary before the assignment */
        if(buffers->value[0] == '(' ) {
            char itype[PLAN9_TYPE_MAX];
            size_t it = 1;
            size_t vn = strlen(buffers->value);
            size_t ob;
            int cb;
            int valid = 0;
            while(it < vn && (isalnum((unsigned char)buffers->value[it])
                              || buffers->value[it] == '_' || buffers->value[it] == ' '
                              || buffers->value[it] == '*'))
                it++;
            if(it > 1 && it < sizeof(itype) && buffers->value[it] == ')'
               && buffers->value[it + 1] == '{'
               && (isalpha((unsigned char)buffers->value[1]) || buffers->value[1] == '_')) {
                memcpy(itype, buffers->value + 1, it - 1);
                itype[it - 1] = '\0';
                ob = it + 2;
                cb = find_matching_brace(buffers->value, (int)ob);
                if(cb > 0 && (buffers->value[cb + 1] == '\0'))
                    valid = 1;
            }
            if(valid) {
                char itemp[PLAN9_NAME_MAX];
                static int inner_seq = 0;
                size_t ibn = (size_t)cb - ob;
                snprintf(itemp, sizeof(itemp), "__zir_c_p9i%d", inner_seq++);
                if(ibn >= sizeof(buffers->ibody))
                    return -1;
                memcpy(buffers->ibody, buffers->value + ob, ibn);
                buffers->ibody[ibn] = '\0';
                if(emit_compound_temp(out, indent, itype, buffers->ibody,
                                      (int)ibn, itemp) < 0)
                    return -1;
                if(buf_printf(out, "%s%s.%s = %s;\n",
                              indent, temp, field, itemp) < 0)
                    return -1;
                if(comma < 0)
                    break;
                pos = comma + 1;
                continue;
            }
        }
        if(buffers->value[0] == '{' && buffers->value[strlen(buffers->value) - 1] == '}') {
            size_t n = strlen(buffers->value) - 2;
            char path[PLAN9_NAME_MAX];
            if(n >= sizeof(buffers->inner))
                return -1;
            memcpy(buffers->inner, buffers->value + 1, n);
            buffers->inner[n] = '\0';
            if(snprintf(path, sizeof(path), "%s.%s", temp, field) >= (int)sizeof(path))
                return -1;
            if(emit_compound_temp(out, indent, NULL, buffers->inner, (int)n, path) < 0)
                return -1;
        } else {
            if(buf_printf(out, "%s%s.%s = %s;\n",
                          indent, temp, field, buffers->value) < 0)
                return -1;
        }
        if(comma < 0)
            break;
        pos = comma + 1;
    }
    return 0;
}

/* Emit statements that build `Type temp` from the literal body and
 * append them to out. Returns 0 on success. */
static int
emit_compound_temp(Buf *out, const char *indent, const char *type,
                   const char *body, int body_len, const char *temp)
{
    static _Thread_local EmitCompoundTempBuffers *spares[16];
    static _Thread_local int spare_count;
    EmitCompoundTempBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = emit_compound_temp_with_buffers(out, indent, type, body, body_len, temp, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* Find the next cast-shaped compound literal "(Ident]){{" on the line,
 * starting at *cursor. Sets the cast span and brace span. */
static int
find_cast_literal(const char *line, int cursor, int *cast_start, int *cast_end,
                  int *brace_open, char *type, size_t type_size)
{
    int i = cursor;
    int tstart;
    int tend;
    size_t n;
    int k;

    while(line[i] != '\0') {
        if(line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            i++;
            while(line[i] != '\0' && line[i] != quote) {
                if(line[i] == '\\')
                    i++;
                i++;
            }
            if(line[i] != '\0')
                i++;
            continue;
        }
        if(line[i] == '(') {
            tstart = i + 1;
            tend = tstart;
            while(line[tend] != '\0') {
                if(line[tend] == '[') {
                    while(line[tend] != '\0' && line[tend] != ']')
                        tend++;
                    if(line[tend] == ']')
                        tend++;
                    continue;
                }
                if(!(isalnum((unsigned char)line[tend]) || line[tend] == '_'
                     || line[tend] == ' ' || line[tend] == '\t'
                     || line[tend] == '*'))
                    break;
                tend++;
            }
            n = (size_t)(tend - tstart);
            if(n > 0 && n < type_size && line[tend] == ')'
               && line[tend + 1] == '{'
               && (isalpha((unsigned char)line[tstart])
                   || line[tstart] == '_')) {
                int ok = 1;
                for(k = 0; k < (int)n; k++) {
                    char ch = line[tstart + k];
                    if(ch == '[') {
                        while(k < (int)n && line[tstart + k] != ']')
                            k++;
                        continue;
                    }
                    if(!(isalnum((unsigned char)ch) || ch == '_' || ch == ' '
                         || ch == '\t' || ch == '*' || ch == '+')) {
                        ok = 0;
                        break;
                    }
                }
                if(ok) {
                    memcpy(type, line + tstart, n);
                    type[n] = '\0';
                    *cast_start = i;
                    *cast_end = tend;
                    *brace_open = tend + 1;
                    return 1;
                }
            }
        }
        i++;
    }
    return 0;
}
/* Buffers rewrite_line_compound keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RewriteLineCompoundBuffers {
    char body[PLAN9_LINE_MAX];
} RewriteLineCompoundBuffers;

static int rewrite_line_compound(Buf *out, const char *line, int lineno, int *rewrote);

static int
rewrite_line_compound_with_buffers(Buf *out, const char *line, int lineno, int *rewrote, RewriteLineCompoundBuffers *buffers)
{
    char indent[256];
    size_t ilen = 0;
    int cursor = 0;
    int temp_index = 0;
    Buf temps;
    Buf rebuilt;
    char type[PLAN9_TYPE_MAX];
    int cast_start;
    int cast_end;
    int brace_open;
    int brace_close;
    char temp[PLAN9_NAME_MAX];
    int n;
    while(ilen + 1 < sizeof(indent) &&
          (line[ilen] == ' ' || line[ilen] == '\t')) {
        indent[ilen] = line[ilen];
        ilen++;
    }
    indent[ilen] = '\0';
    if(!buf_init(&temps) || !buf_init(&rebuilt)) {
        free(temps.data);
        free(rebuilt.data);
        return -1;
    }
    while(find_cast_literal(line, cursor, &cast_start, &cast_end,
                            &brace_open, type, sizeof(type))) {
        brace_close = find_matching_brace(line, brace_open);
        if(brace_close < 0)
            break; /* multi-line literal: leave the rest untouched */
        n = brace_close - brace_open - 1;
        if((size_t)n >= sizeof(buffers->body))
            break;
        memcpy(buffers->body, line + brace_open + 1, (size_t)n);
        buffers->body[n] = '\0';
        snprintf(temp, sizeof(temp), "%s%d_%d", PLAN9_TEMP_PREFIX,
                 lineno, temp_index);
        temp_index++;
        if(emit_compound_temp(&temps, indent, type, buffers->body, n, temp) < 0)
            break;
        if(buf_append(&rebuilt, line + cursor,
                      (size_t)(cast_start - cursor)) < 0)
            break;
        if(buf_puts(&rebuilt, temp) < 0)
            break;
        cursor = brace_close + 1;
        (*rewrote)++;
    }
    if(*rewrote > 0) {
        int failed = buf_append(&rebuilt, line + cursor,
                                strlen(line + cursor)) < 0
                     || buf_puts(out, temps.data) < 0
                     || buf_puts(out, rebuilt.data) < 0;
        free(temps.data);
        free(rebuilt.data);
        return failed ? -1 : 0;
    }
    free(temps.data);
    free(rebuilt.data);
    return 0;
}

/* Rewrite every cast literal on one line into temps + rebuilt line. */
static int
rewrite_line_compound(Buf *out, const char *line, int lineno, int *rewrote)
{
    static _Thread_local RewriteLineCompoundBuffers *spares[16];
    static _Thread_local int spare_count;
    RewriteLineCompoundBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = rewrite_line_compound_with_buffers(out, line, lineno, rewrote, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

/* ------------------------------------------------------------------ */
/* __auto_type resolution                                            */
/* ------------------------------------------------------------------ */


/* Classify an initializer expression by resolving every identifier and
 * call head in it: uniform int stays int, any float makes float, a
 * single non-int resolved type wins, mixed types fail. */
static int
expr_type(const char *expr, size_t len, char *type, size_t type_size);

static int
head_type(const char *head, size_t len, char *type, size_t type_size)
{
    char name[PLAN9_NAME_MAX];
    const char *ret;

    if(len == 0 || len >= sizeof(name))
        return 0;
    memcpy(name, head, len);
    name[len] = '\0';
    if(init_is_int_literal(name)) {
        snprintf(type, type_size, "int");
        return 1;
    }
    if(expr_looks_float(name)) {
        snprintf(type, type_size, "float");
        return 1;
    }
    ret = proto_return_type(name);
    if(ret == NULL || strlen(ret) >= type_size)
        return 0;
    snprintf(type, type_size, "%s", ret);
    return 1;
}

static int
merge_type(char *acc, int *have, const char *next)
{
    if(!*have) {
        snprintf(acc, PLAN9_TYPE_MAX, "%s", next);
        *have = 1;
        return 1;
    }
    if(strcmp(acc, next) == 0)
        return 1;
    if((strcmp(acc, "int") == 0 && strcmp(next, "float") == 0)
       || (strcmp(acc, "float") == 0 && strcmp(next, "int") == 0)) {
        snprintf(acc, PLAN9_TYPE_MAX, "float");
        return 1;
    }
    if(strcmp(acc, "void *") == 0 && strchr(next, '*') != NULL) {
        snprintf(acc, PLAN9_TYPE_MAX, "%s", next);
        return 1;
    }
    if(strcmp(next, "void *") == 0 && strchr(acc, '*') != NULL)
        return 1;
    return 0;
}
/* Buffers expr_type keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct ExprTypeBuffers {
    char inner[PLAN9_LINE_MAX];
} ExprTypeBuffers;

static int expr_type(const char *expr, size_t len, char *type, size_t type_size);

static int
expr_type_with_buffers(const char *expr, size_t len, char *type, size_t type_size, ExprTypeBuffers *buffers)
{
    char acc[PLAN9_TYPE_MAX];
    char one[PLAN9_TYPE_MAX];
    int have = 0;
    size_t i = 0;
    while(i < len && (expr[i] == ' ' || expr[i] == '\t'))
        i++;
    if(i >= len)
        return 0;
    if(expr[i] == '&' || expr[i] == '*')
        return 0; /* pointers handled by the caller */
    while(i < len) {
        if(expr[i] == '(') {
            size_t depth = 0;
            size_t k = i;
            char inner_type[PLAN9_TYPE_MAX];
            while(k < len) {
                if(expr[k] == '(')
                    depth++;
                else if(expr[k] == ')') {
                    depth--;
                    if(depth == 0)
                        break;
                }
                k++;
            }
            if(k >= len || k - i - 1 >= sizeof(buffers->inner))
                return 0;
            memcpy(buffers->inner, expr + i + 1, k - i - 1);
            buffers->inner[k - i - 1] = '\0';
            if(!expr_type(buffers->inner, strlen(buffers->inner), inner_type,
                          sizeof(inner_type)))
                return 0;
            if(!merge_type(acc, &have, inner_type))
                return 0;
            i = k + 1;
            continue;
        }
        if(isdigit((unsigned char)expr[i])) {
            size_t j = i;
            while(j < len && (isdigit((unsigned char)expr[j])
                              || expr[j] == '.' || expr[j] == 'u'
                              || expr[j] == 'U' || expr[j] == 'l'
                              || expr[j] == 'L' || expr[j] == 'f'
                              || expr[j] == 'F'))
                j++;
            if(!head_type(expr + i, j - i, one, sizeof(one)))
                return 0;
            if(!merge_type(acc, &have, one))
                return 0;
            i = j;
            continue;
        }
        if(isalpha((unsigned char)expr[i]) || expr[i] == '_') {
            size_t j = i;
            while(j < len && (isalnum((unsigned char)expr[j])
                              || expr[j] == '_'))
                j++;
            if(j < len && expr[j] == '(') {
                size_t depth = 0;
                size_t k = j;
                while(k < len) {
                    if(expr[k] == '(')
                        depth++;
                    else if(expr[k] == ')') {
                        depth--;
                        if(depth == 0)
                            break;
                    }
                    k++;
                }
                if(k >= len)
                    return 0;
                if(!head_type(expr + i, j - i, one, sizeof(one)))
                    return 0;
                if(!merge_type(acc, &have, one))
                    return 0;
                i = k + 1;
                continue;
            }
            /* the base identifier: a macro constant or an int local */
            if(!head_type(expr + i, j - i, one, sizeof(one)))
                snprintf(one, sizeof(one), "int");
            if(!merge_type(acc, &have, one))
                return 0;
            i = j;
            continue;
        }
        if(expr[i] == '?') {
            /* ternary: the condition's contribution does not type the
             * result; the branches do */
            have = 0;
        }
        i++;
    }
    if(!have)
        return 0;
    if(strlen(acc) >= type_size)
        return 0;
    snprintf(type, type_size, "%s", acc);
    return 1;
}

static int
expr_type(const char *expr, size_t len, char *type, size_t type_size)
{
    static _Thread_local ExprTypeBuffers *spares[16];
    static _Thread_local int spare_count;
    ExprTypeBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    int returned = expr_type_with_buffers(expr, len, type, type_size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}

static int
autotype_type_for_init(const char *init, char *type, size_t type_size)
{
    size_t i = 0;
    size_t tstart;
    size_t tend;
    size_t n;
    const char *call;
    const char *call_end;
    char name[PLAN9_NAME_MAX];
    const char *ret;

    while(init[i] == ' ' || init[i] == '\t')
        i++;

    /* single dereference: layout pointers are int in the lowered UI
     * code; anything more complex stays unresolved */
    if(init[i] == '*') {
        const char *s = init + i + 1;
        size_t k = 0;

        while(s[k] == ' ')
            k++;
        while(s[k] != '\0' && (isalnum((unsigned char)s[k]) || s[k] == '_'))
            k++;
        while(s[k] == ' ')
            k++;
        if(s[k] == ';' || s[k] == '+' || s[k] == '-' || s[k] == ')') {
            if(type_size < 4)
                return 0;
            snprintf(type, type_size, "int");
            return 1;
        }
        return 0;
    }

    /* cast expression, compound literal or not: the type is spelled */
    if(init[i] == '(') {
        tstart = i + 1;
        tend = tstart;
        while(init[tend] != '\0') {
            if(init[tend] == '[') {
                while(init[tend] != '\0' && init[tend] != ']')
                    tend++;
                if(init[tend] == ']')
                    tend++;
                continue;
            }
            if(!(isalnum((unsigned char)init[tend]) || init[tend] == '_'
                 || init[tend] == ' ' || init[tend] == '*'))
                break;
            tend++;
        }
        n = tend - tstart;
        if(n > 0 && init[tend] == ')' && n < type_size
           && (isalpha((unsigned char)init[tstart])
               || init[tstart] == '_')) {
            size_t k;
            int ok = 1;

            for(k = 0; k < n; k++) {
                char ch = init[tstart + k];
                if(!(isalnum((unsigned char)ch) || ch == '_' || ch == ' '
                     || ch == '\t' || ch == '*' || ch == '['
                     || ch == ']' || ch == '+')) {
                    ok = 0;
                    break;
                }
            }
            if(ok) {
                memcpy(type, init + tstart, n);
                type[n] = '\0';
                return 1;
            }
        }
    }

    /* string literal */
    if(init[i] == '"') {
        if(type_size < 12)
            return 0;
        snprintf(type, type_size, "const char *");
        return 1;
    }

    /* numeric literal or constant expression over numbers and macros */
    {
        char head[256];
        size_t len = strlen(init + i);
        const char *semi = strchr(init + i, ';');

        if(semi != NULL)
            len = (size_t)(semi - (init + i));
        if(len == 0 || len >= sizeof(head))
            len = sizeof(head) - 1;
        memcpy(head, init + i, len);
        head[len] = '\0';
        if(init_is_int_literal(head)) {
            if(type_size < 4)
                return 0;
            snprintf(type, type_size, "int");
            return 1;
        }
        if(head[0] != '*') {
            /* expression over literals, macros, locals, and calls.
             * Member selections need the field's type, which is not
             * tracked here: refuse instead of guessing. */
            if(strstr(head, "->") != NULL || strchr(head, '.') != NULL)
                return 0;
            if(expr_type(head, strlen(head), type, type_size))
                return 1;
        }
        (void)name;
        (void)ret;
    }

    /* function call: resolve the return type from the prototype map */
    call = init + i;
    call_end = call;
    while(*call_end != '\0' && (isalnum((unsigned char)*call_end)
                                || *call_end == '_'))
        call_end++;
    n = (size_t)(call_end - call);
    if(n == 0 || *call_end != '(' || n >= sizeof(name))
        return 0;
    memcpy(name, call, n);
    name[n] = '\0';
    ret = proto_return_type(name);
    if(ret == NULL || strlen(ret) >= type_size)
        return 0;
    /* a member selection on the result needs the field's type, which
     * is not tracked: refuse rather than type it as the call's return */
    {
        const char *close = strchr(call_end, ')');

        if(close != NULL) {
            const char *after = close + 1;

            while(*after == ' ')
                after++;
            if(*after == '.' || (*after == '-' && after[1] == '>'))
                return 0;
        }
    }
    snprintf(type, type_size, "%s", ret);
    return 1;
}

static int
rewrite_autotype(const char *line, char *out, size_t out_size)
{
    const char *p = line;
    const char *name_start;
    const char *name_end;
    const char *init;
    char name[PLAN9_NAME_MAX];
    char type[PLAN9_TYPE_MAX];
    size_t n;

    while(*p == ' ' || *p == '\t')
        p++;
    if(strncmp(p, "__auto_type ", 12) != 0)
        return 0;
    p += 12;
    name_start = p;
    while(*p != '\0' && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    name_end = p;
    n = (size_t)(name_end - name_start);
    if(n == 0 || n >= sizeof(name))
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';
    while(*p == ' ')
        p++;
    if(*p != '=')
        return 0;
    init = p + 1;
    if(!autotype_type_for_init(init, type, sizeof(type)))
        return 0;
    {
        const char *bracket = strchr(type, '[');

        if(bracket != NULL) {
            char base[PLAN9_TYPE_MAX];
            size_t n = (size_t)(bracket - type);

            if(n == 0 || n >= sizeof(base))
                return 0;
            memcpy(base, type, n);
            base[n] = '\0';
            if(snprintf(out, out_size, "%.*s%s %s%s = %s",
                        (int)(name_start - line - 12), line, base, name,
                        bracket, init)
               >= (int)out_size)
                return 0;
            return 1;
        }
    }
    if(snprintf(out, out_size, "%.*s%s %s = %s",
                (int)(name_start - line - 12), line, type, name, init)
       >= (int)out_size)
        return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* for-declaration hoisting                                          */
/* ------------------------------------------------------------------ */

static int
for_decl_split(const char *line, char *type, size_t type_size,
               char *name, size_t name_size, const char **rest)
{
    const char *p = line;
    const char *q;
    const char *type_start;
    const char *type_end;
    const char *name_start;
    const char *name_end;
    size_t n;

    while(*p == ' ' || *p == '\t')
        p++;
    if(strncmp(p, "for(", 4) != 0)
        return 0;
    q = p + 4;
    type_start = q;
    if(strncmp(q, "unsigned ", 9) == 0)
        q += 9;
    if(strncmp(q, "int ", 4) != 0 && strncmp(q, "long ", 5) != 0
       && strncmp(q, "size_t ", 7) != 0)
        return 0;
    while(*q != '\0' && *q != ' ' && *q != '\t')
        q++;
    type_end = q;
    n = (size_t)(type_end - type_start);
    if(n == 0 || n >= type_size)
        return 0;
    memcpy(type, type_start, n);
    type[n] = '\0';
    while(*q == ' ' || *q == '\t')
        q++;
    name_start = q;
    while(*q != '\0' && (isalnum((unsigned char)*q) || *q == '_'))
        q++;
    name_end = q;
    n = (size_t)(name_end - name_start);
    if(n == 0 || n >= name_size)
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';
    while(*q == ' ' || *q == '\t')
        q++;
    if(*q != '=')
        return 0;
    /* rest keeps " = init; cond; step) {" */
    *rest = name_end;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Native Plan 9 runtime                                             */
/* ------------------------------------------------------------------ */

int
c_plan9_write_runtime(const char *out_dir)
{
    char path[1024];
    FILE *file;

    snprintf(path, sizeof(path), "%s/zir_plan9_runtime.h", out_dir);
    file = fopen(path, "wb");
    if(file == NULL)
        return -1;
    fputs(
"#ifndef ZIR_PLAN9_RUNTIME_H\n"
"#define ZIR_PLAN9_RUNTIME_H\n\n"
"/* Native C callers which already included u.h/libc.h set this guard. */\n"
"#ifndef ZIR_PLAN9_NATIVE_HEADERS_INCLUDED\n"
"#include <u.h>\n"
"#include <libc.h>\n"
"#define ZIR_PLAN9_NATIVE_HEADERS_INCLUDED 1\n"
"#endif\n\n"
"/* plan9port's u.h (_U_H_) already brings the POSIX integer types. */\n"
"#ifndef _U_H_\n"
"typedef char int8_t;\n"
"typedef uchar uint8_t;\n"
"typedef short int16_t;\n"
"typedef ushort uint16_t;\n"
"typedef int int32_t;\n"
"typedef uint uint32_t;\n"
"typedef vlong int64_t;\n"
"typedef uvlong uint64_t;\n"
"typedef long ptrdiff_t;\n"
"typedef usize size_t;\n"
"#endif\n"
"typedef int bool;\n\n"
"#ifndef NULL\n"
"#define NULL ((void*)0)\n"
"#endif\n"
"typedef struct Slice {\n"
"    void *data;\n"
"    int64_t length;\n"
"} Slice;\n\n"
"static int64_t\n"
"SliceIndex(Slice source, int64_t index)\n"
"{\n"
"    if(index < 0 || index >= source.length) {\n"
"        fprint(2, \"ziran: slice index out of bounds\\n\");\n"
"        abort();\n"
"    }\n"
"    return index;\n"
"}\n\n"
"static Slice\n"
"SliceRange(Slice source, int64_t low, int64_t high,\n"
"           size_t element_size)\n"
"{\n"
"    Slice result;\n"
"    if(low < 0 || high < low || high > source.length) {\n"
"        fprint(2, \"ziran: slice range out of bounds\\n\");\n"
"        abort();\n"
"    }\n"
"    result.data = source.data != NULL ?\n"
"        (char *)source.data + (size_t)low * element_size : NULL;\n"
"    result.length = high - low;\n"
"    return result;\n"
"}\n\n"
"#define false 0\n"
"#define true 1\n"
"#ifndef _U_H_\n"
"#define UINT64_C(value) ((uint64_t)(value))\n"
"#define INT64_C(value) ((int64_t)(value))\n"
"#define UINT64_MAX ((uint64_t)-1)\n"
"#define INT64_MAX ((int64_t)((((uint64_t)1 << 63) - 1)))\n"
"#define INT64_MIN (-INT64_MAX - 1)\n"
"#define SIZE_MAX ((size_t)-1)\n"
"#endif\n\n"
"typedef struct String {\n"
"    const char *data;\n"
"    size_t length;\n"
"} String;\n"
"typedef struct Source_Code_Location {\n"
"    String fully_pathed_filename;\n"
"    int64_t line_number;\n"
"} Source_Code_Location;\n\n"
"static String\nStringView(const char *data, size_t length)\n"
"{\n"
"    String value;\n"
"    value.data = data;\n"
"    value.length = length;\n"
"    return value;\n"
"}\n\n"
"#define StringLiteral(text) StringView((text), sizeof(text) - 1)\n\n"
"static String\nStringRange(String source, int64_t low, int64_t high)\n"
"{\n"
"    if(low < 0 || high < low || (uint64_t)high > source.length ||\n"
"       (source.data == NULL && source.length != 0)) {\n"
"        fprint(2, \"ziran: string range out of bounds\\n\");\n"
"        abort();\n"
"    }\n"
"    return StringView(source.data == NULL ? \"\" : source.data + low,\n"
"                      (size_t)(high - low));\n"
"}\n\n"
"static bool\nStringEqual(String left, String right)\n"
"{\n"
"    return left.length == right.length &&\n"
"        (left.length == 0 ||\n"
"         memcmp(left.data, right.data, left.length) == 0);\n"
"}\n\n"
"/* print writes floats as the shortest decimal that reads back the same. */\n"
"#ifdef ZIR_PLAN9_PRINT\n"
"/* Plan 9's %e is not correctly rounded, so the exact decimal expansion of\n"
" * the double is computed here, then rounded at each precision until the\n"
" * digits read back: the same digits C's printf and strtod agree on. */\n"
"enum { PLAN9_LIMBS = 40, PLAN9_DIGITS = 40 };\n"
"\n"
"/* Exact significant digits of a positive double, rounded to PLAN9_DIGITS\n"
" * with a sticky flag for anything beyond; returns the decimal exponent. */\n"
"static int\n"
"plan9_exact_digits(double value, char *digits, int *count, int *sticky)\n"
"{\n"
"    uint32_t big[PLAN9_LIMBS];\n"
"    int binary, i, used = 0, exponent = 0, started = 0;\n"
"    double fraction = frexp(value, &binary);\n"
"    uint64_t mantissa = (uint64_t)ldexp(fraction, 53);\n"
"    binary -= 53;\n"
"    *sticky = 0;\n"
"    memset(big, 0, sizeof(big));\n"
"    big[0] = (uint32_t)mantissa;\n"
"    big[1] = (uint32_t)(mantissa >> 32);\n"
"    if(binary >= 0) {\n"
"        /* An integer: shift left, then peel decimal digits from the end. */\n"
"        char reversed[400];\n"
"        int length = 0, nonzero;\n"
"        for(i = 0; i < binary; i++) {\n"
"            uint32_t carry = 0;\n"
"            int limb;\n"
"            for(limb = 0; limb < PLAN9_LIMBS; limb++) {\n"
"                uint32_t next = big[limb] >> 31;\n"
"                big[limb] = (big[limb] << 1) | carry;\n"
"                carry = next;\n"
"            }\n"
"        }\n"
"        do {\n"
"            uint64_t remainder = 0;\n"
"            int limb;\n"
"            nonzero = 0;\n"
"            for(limb = PLAN9_LIMBS - 1; limb >= 0; limb--) {\n"
"                uint64_t current = (remainder << 32) | big[limb];\n"
"                big[limb] = (uint32_t)(current / 10);\n"
"                remainder = current % 10;\n"
"                nonzero |= big[limb] != 0;\n"
"            }\n"
"            reversed[length++] = (char)('0' + remainder);\n"
"        } while(nonzero && length < (int)sizeof(reversed));\n"
"        exponent = length - 1;\n"
"        for(i = length - 1; i >= 0; i--) {\n"
"            if(used < PLAN9_DIGITS) digits[used++] = reversed[i];\n"
"            else if(reversed[i] != '0') *sticky = 1;\n"
"        }\n"
"    } else {\n"
"        /* value = mantissa / 2^shift: take the integer part, then multiply\n"
"         * the fraction by ten, one digit at a time. */\n"
"        int shift = -binary, limb, position = -1;\n"
"        uint64_t whole = shift >= 64 ? 0 : mantissa >> shift;\n"
"        char integer[24];\n"
"        int length = 0;\n"
"        if(shift < 64) {\n"
"            uint64_t mask = (((uint64_t)1) << shift) - 1;\n"
"            uint64_t low = mantissa & mask;\n"
"            memset(big, 0, sizeof(big));\n"
"            big[0] = (uint32_t)low;\n"
"            big[1] = (uint32_t)(low >> 32);\n"
"        }\n"
"        while(whole > 0) { integer[length++] = (char)('0' + whole % 10); whole /= 10; }\n"
"        for(i = length - 1; i >= 0; i--) digits[used++] = integer[i];\n"
"        started = length > 0;\n"
"        exponent = length - 1;\n"
"        for(;;) {\n"
"            uint64_t carry = 0;\n"
"            int digit, zero = 1;\n"
"            for(limb = 0; limb < PLAN9_LIMBS; limb++) {\n"
"                uint64_t current = (uint64_t)big[limb] * 10 + carry;\n"
"                big[limb] = (uint32_t)current;\n"
"                carry = current >> 32;\n"
"            }\n"
"            digit = (int)((big[shift / 32] >> (shift % 32)) |\n"
"                          (shift % 32 ? (uint64_t)big[shift / 32 + 1] << (32 - shift % 32) : 0)) & 15;\n"
"            /* Clear the bits at and above the binary point. */\n"
"            big[shift / 32] &= (uint32_t)((((uint64_t)1) << (shift % 32)) - 1);\n"
"            for(limb = shift / 32 + 1; limb < PLAN9_LIMBS; limb++) big[limb] = 0;\n"
"            if(!started) {\n"
"                if(digit == 0) { position--; continue; }\n"
"                started = 1;\n"
"                exponent = position;\n"
"            }\n"
"            if(used < PLAN9_DIGITS) digits[used++] = (char)('0' + digit);\n"
"            else if(digit != 0) *sticky = 1;\n"
"            for(limb = 0; limb < PLAN9_LIMBS; limb++) if(big[limb]) { zero = 0; break; }\n"
"            if(zero) break;\n"
"            if(used >= PLAN9_DIGITS) { *sticky = 1; break; }\n"
"        }\n"
"    }\n"
"    *count = used;\n"
"    return exponent;\n"
"}\n"
"\n"
"static double\n"
"plan9_digits_value(char *digits, int count, int exponent)\n"
"{\n"
"    char text[64];\n"
"    int used = 0, i;\n"
"    text[used++] = digits[0];\n"
"    text[used++] = '.';\n"
"    for(i = 1; i < count; i++) text[used++] = digits[i];\n"
"    if(count == 1) text[used++] = '0';\n"
"    snprint(text + used, sizeof(text) - used, \"e%d\", exponent);\n"
"    return strtod(text, 0);\n"
"}\n"
"\n"
"static void\n"
"print_float(double value, int single)\n"
"{\n"
"    char exact[PLAN9_DIGITS], digits[24];\n"
"    int exact_count, sticky, exponent, precision, count, i;\n"
"    if(isNaN(value)) { write(1, \"nan\", 3); return; }\n"
"    if(isInf(value, 0)) {\n"
"        if(value < 0) write(1, \"-inf\", 4); else write(1, \"inf\", 3);\n"
"        return;\n"
"    }\n"
"    /* Plan 9 formatting drops the sign of -0.0, so write it here. */\n"
"    if(value < 0 || (value == 0 && 1 / value < 0)) {\n"
"        write(1, \"-\", 1);\n"
"        value = -value;\n"
"    }\n"
"    if(value == 0) { write(1, \"0\", 1); return; }\n"
"    exponent = plan9_exact_digits(value, exact, &exact_count, &sticky);\n"
"    for(precision = 1; precision <= 17; precision++) {\n"
"        int round_exponent = exponent, up;\n"
"        count = precision;\n"
"        for(i = 0; i < count; i++) digits[i] = i < exact_count ? exact[i] : '0';\n"
"        /* Round half to even on the exact expansion. */\n"
"        if(precision < exact_count) {\n"
"            int rest_nonzero = sticky;\n"
"            for(i = precision + 1; i < exact_count; i++) rest_nonzero |= exact[i] != '0';\n"
"            up = exact[precision] > '5' || (exact[precision] == '5' &&\n"
"                 (rest_nonzero || ((digits[count - 1] - '0') & 1)));\n"
"        } else\n"
"            up = 0;\n"
"        if(up) {\n"
"            for(i = count - 1; i >= 0 && digits[i] == '9'; i--) digits[i] = '0';\n"
"            if(i >= 0) digits[i]++;\n"
"            else { memmove(digits + 1, digits, count - 1); digits[0] = '1'; round_exponent++; }\n"
"        }\n"
"        double parsed = plan9_digits_value(digits, count, round_exponent);\n"
"        if(single ? (float)parsed == (float)value : parsed == value) {\n"
"            exponent = round_exponent;\n"
"            break;\n"
"        }\n"
"    }\n"
"    while(count > 1 && digits[count - 1] == '0') count--;\n"
"    if(exponent < 0) {\n"
"        write(1, \"0.\", 2);\n"
"        for(i = 1; i < -exponent; i++) write(1, \"0\", 1);\n"
"        write(1, digits, count);\n"
"    } else {\n"
"        for(i = 0; i <= exponent; i++)\n"
"            write(1, i < count ? digits + i : \"0\", 1);\n"
"        if(count > exponent + 1) {\n"
"            write(1, \".\", 1);\n"
"            write(1, digits + exponent + 1, count - exponent - 1);\n"
"        }\n"
"    }\n"
"}\n"
"\n"
"#endif\n\n"
"static size_t\n"
"ZirVecIndex(int64_t count, int64_t index)\n"
"{\n"
"    if(count < 0 || index < 0 || index >= count) {\n"
"        fprint(2, \"ziran: Vec index %lld out of bounds\\n\",\n"
"                (vlong)index);\n"
"        abort();\n"
"    }\n"
"    return (size_t)index;\n"
"}\n\n"
"#define ZIRAN_VEC_INDEX(data, count, index) \\\n"
"    ((data)[ZirVecIndex((int64_t)(count), (int64_t)(index))])\n\n"
"static void *\n"
"ZirVecReserve(void *data, int64_t *capacity, int64_t count,\n"
"              int64_t additional, size_t element_size)\n"
"{\n"
"    if(capacity == NULL || element_size == 0 || count < 0 ||\n"
"       *capacity < count || additional <= 0)\n"
"        return NULL;\n"
"    if(count > INT64_MAX - additional)\n"
"        return NULL;\n"
"    uint64_t needed = (uint64_t)count + (uint64_t)additional;\n"
"    uint64_t next = *capacity > 0 ? (uint64_t)*capacity : 8u;\n"
"    while(next < needed) {\n"
"        if(next > (uint64_t)INT64_MAX / 2u) {\n"
"            next = needed;\n"
"            break;\n"
"        }\n"
"        next *= 2u;\n"
"    }\n"
"    if(next > (uint64_t)INT64_MAX || next > SIZE_MAX / element_size)\n"
"        return NULL;\n"
"    if((uint64_t)*capacity >= needed)\n"
"        return data;\n"
"    void *grown = realloc(data, (size_t)next * element_size);\n"
"    if(grown == NULL)\n"
"        return NULL;\n"
"    *capacity = (int64_t)next;\n"
"    return grown;\n"
"}\n\n"
"static void *\n"
"ZirVecGrow(void *data, int64_t *capacity, int64_t count,\n"
"           size_t element_size)\n"
"{\n"
"    return ZirVecReserve(data, capacity, count, 1, element_size);\n"
"}\n\n"
"static size_t\nPlan9EmptyArrayIndex(size_t index)\n"
"{\n"
"    (void)index;\n"
"    fprint(2, \"ziran: index out of bounds for empty array\\n\");\n"
"    abort();\n"
"    return 0;\n"
"}\n"
"#define ZIRAN_INDEX(base, length, index) ((base)[(index)])\n"
"#define ZIRAN_EMPTY_INDEX(base, index) \\\n"
"    ((base)[Plan9EmptyArrayIndex((size_t)(index))])\n\n"
"#endif\n",
        file);
    if(fclose(file) != 0)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public entry points                                               */
/* ------------------------------------------------------------------ */

int
c_plan9_rewrite_file(const char *path)
{
    FILE *f;
    char *text;
    long size;
    char *rewritten;
    const char *write_text;

    if(!plan9_enabled)
        return 0;
    f = fopen(path, "rb");
    if(f == NULL)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size < 0) {
        fclose(f);
        return -1;
    }
    text = malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(f);
        return -1;
    }
    if(fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return -1;
    }
    text[size] = '\0';
    fclose(f);

    write_text = text;
    rewritten = NULL;
    rewritten = c_plan9_rewrite(text);
    if(rewritten == NULL) {
        free(text);
        return -1;
    }
    write_text = rewritten;

    if(write_text != text) {
        f = fopen(path, "wb");
        if(f == NULL) {
            free(rewritten);
            free(text);
            return -1;
        }
        fwrite(write_text, 1, strlen(write_text), f);
        fclose(f);
    }
    free(rewritten);
    free(text);
    return 0;
}

static int
plan9_main_arguments(const char *line, char *parameters, size_t capacity,
                     int *prototype)
{
    const char *name = line;
    const char *end;
    char count[128], arguments[128];
    int consumed = 0;
    size_t length;

    *prototype = strncmp(name, "int32_t ", 8) == 0;
    if(*prototype) name += 8;
    if(strncmp(name, "main(", 5) != 0) return 0;
    end = strchr(name + 5, ')');
    if(end == NULL || strcmp(end + 1, *prototype ? ";\n" : "\n")) return 0;
    length = (size_t)(end - name - 5);
    if(length >= capacity) return 0;
    memcpy(parameters, name + 5, length);
    parameters[length] = '\0';
    return sscanf(parameters, "int32_t %127[a-zA-Z0-9_], int8_t** %127[a-zA-Z0-9_]%n",
                  count, arguments, &consumed) == 2 &&
           parameters[consumed] == '\0';
}

static char *
append_plan9_main_wrapper(const char *text, int arguments)
{
    const char *header = arguments ?
        "\nvoid\nmain(int argc, char** argv)\n{\n" :
        "\nvoid\nmain(void)\n{\n";
    const char *call = arguments ?
        "    status = ziran_plan9_main((int32_t)argc, (int8_t**)argv);\n" :
        "    status = ziran_plan9_main();\n";
    static const char locals[] =
"    int32_t status;\n"
"    char status_text[32];\n";
    static const char finish[] =
"    if(status == 0)\n"
"        exits(0);\n"
"    if(snprint(status_text, sizeof(status_text), \"%d\",\n"
"               (int)status) <= 0)\n"
"        exits(\"ziran: invalid exit status\");\n"
"    exits(status_text);\n"
"}\n";
    size_t length = strlen(text);
    char *result;

    if(length > 0 && text[length - 1] != '\n')
        length++;
    result = malloc(length + strlen(header) + strlen(call) + sizeof(locals) + sizeof(finish));
    if(result == NULL)
        return NULL;
    memcpy(result, text, strlen(text));
    result[strlen(text)] = '\0';
    if(strlen(text) == 0 || text[strlen(text) - 1] != '\n')
        strcat(result, "\n");
    strcat(result, header);
    strcat(result, locals);
    strcat(result, call);
    strcat(result, finish);
    return result;
}

char *c_plan9_rewrite_once(const char *text);

char *
c_plan9_rewrite(const char *text)
{
    char *current;
    char *next;
    int pass;

    /* the result is always freshly allocated: callers free it */
    current = c_plan9_rewrite_once(text);
    if(current == NULL)
        return NULL;

    /* literals can nest inside call arguments inside positional bodies;
     * iterate until the output stabilizes */
    for(pass = 1; pass < 4; pass++) {
        next = c_plan9_rewrite_once(current);
        if(next == NULL)
            return current;
        if(strcmp(next, current) == 0) {
            char *with_main;

            free(next);
            int arguments = strstr(current, "\nziran_plan9_main(int32_t ") != NULL;
            if(strstr(current, "ziran_plan9_main(void)\n{") != NULL || arguments) {
                with_main = append_plan9_main_wrapper(current, arguments);
                free(current);
                return with_main;
            }
            return current;
        }
        free(current);
        current = next;
    }
    return current;
}

static void
rewrite_integer_suffixes(char *line)
{
    enum { CODE, STRING, CHARACTER, LINE_COMMENT, BLOCK_COMMENT } state = CODE;
    char *write = line;
    const char *read;
    char *source = strdup(line);
    if(source == NULL) {
        fprintf(stderr, "zi2c: no memory for Plan 9 integer lowering\n");
        exit(1);
    }

    for(read = source; *read != '\0'; read++) {
        if(state == LINE_COMMENT) {
            *write++ = *read;
            if(*read == '\n')
                state = CODE;
            continue;
        }
        if(state == BLOCK_COMMENT) {
            *write++ = *read;
            if(read[0] == '*' && read[1] == '/') {
                *write++ = *++read;
                state = CODE;
            }
            continue;
        }
        if(state == STRING || state == CHARACTER) {
            *write++ = *read;
            if(*read == '\\')
                *write++ = *++read;
            else if((*read == '"' && state == STRING) ||
                    (*read == '\'' && state == CHARACTER))
                state = CODE;
            continue;
        }
        if(read[0] == '/' && read[1] == '/') {
            *write++ = *read++;
            *write++ = *read;
            state = LINE_COMMENT;
            continue;
        }
        if(read[0] == '/' && read[1] == '*') {
            *write++ = *read++;
            *write++ = *read;
            state = BLOCK_COMMENT;
            continue;
        }
        if(*read == '"') {
            *write++ = *read;
            state = STRING;
            continue;
        }
        if(*read == '\'') {
            *write++ = *read;
            state = CHARACTER;
            continue;
        }
        /* Empty arrays are native 8c declarations, without GNU's marker. */
        if(strncmp(read, "__extension__ ", 14) == 0 &&
           (read == source || (!isalnum((unsigned char)read[-1]) && read[-1] != '_'))) {
            read += 13;
            continue;
        }
        /* Native 8c accepts LL/ULL, but an unsuffixed wide integer is
         * truncated to a 32-bit int before a cast or UINT64_C sees it.
         * Preserve typed suffixes and widen large generated literals,
         * including macro arguments and comparison sentinels. */
        if(isdigit((unsigned char)*read) &&
           (read == source || (!isalnum((unsigned char)read[-1]) &&
                               read[-1] != '_' && read[-1] != '.'))) {
            char *digits_end;
            const char *token_end;
            unsigned long long value;
            int unsigned_suffix = 0;
            errno = 0;
            value = strtoull(read, &digits_end, 0);
            token_end = digits_end;
            while(*token_end == 'u' || *token_end == 'U' ||
                  *token_end == 'l' || *token_end == 'L') {
                if(*token_end == 'u' || *token_end == 'U')
                    unsigned_suffix = 1;
                token_end++;
            }
            if(errno == 0 && digits_end > read && value > 0xffffffffULL &&
               *token_end != '.' && !isalnum((unsigned char)*token_end) &&
               *token_end != '_') {
                size_t digits = (size_t)(digits_end - read);
                size_t suffix = unsigned_suffix ? 3 : 2;
                if((size_t)(write - line) + digits + suffix +
                   strlen(token_end) >= PLAN9_LINE_MAX) {
                    fprintf(stderr, "zi2c: Plan 9 integer line too long\n");
                    free(source);
                    exit(1);
                }
                memcpy(write, read, digits);
                write += digits;
                if(unsigned_suffix) *write++ = 'U';
                *write++ = 'L';
                *write++ = 'L';
                read = token_end - 1;
                continue;
            }
        }
        *write++ = *read;
    }
    *write = '\0';
    free(source);
}

typedef struct {
    char alias[PLAN9_NAME_MAX];
    char symbol[PLAN9_NAME_MAX];
} Plan9ForeignAlias;

static int
collect_plan9_foreign_aliases(const char *text,
                              Plan9ForeignAlias *aliases,
                              int alias_count)
{
    const char *cursor = text;
    int count = 0;

    while(*cursor != '\0') {
        const char *nl = strchr(cursor, '\n');
        const char *asm_at = strstr(cursor, "__asm__(\"");
        const char *quote;
        const char *close;
        const char *alias_at;
        const char *alias_end;
        const char *scan;
        size_t length;

        if(asm_at == NULL)
            break;
        if(nl != NULL && asm_at > nl) {
            cursor = nl + 1;
            continue;
        }
        quote = asm_at + strlen("__asm__(\"");
        close = strchr(quote, '"');
        if(close == NULL || close == quote ||
           (size_t)(close - quote) >= sizeof(aliases[0].symbol))
            return -1;
        alias_at = strstr(cursor, "zir_foreign_");
        if(alias_at == NULL || alias_at > asm_at)
            return -1;
        alias_end = alias_at + strlen("zir_foreign_");
        while(isalnum((unsigned char)*alias_end) || *alias_end == '_')
            alias_end++;
        length = (size_t)(alias_end - alias_at);
        if(length == 0 || length >= sizeof(aliases[0].alias))
            return -1;
        if(count < alias_count) {
            memcpy(aliases[count].alias, alias_at, length);
            aliases[count].alias[length] = '\0';
            length = (size_t)(close - quote);
            memcpy(aliases[count].symbol, quote, length);
            aliases[count].symbol[length] = '\0';
            count++;
        }
        cursor = close + 1;
    }
    return count;
}

static int
rewrite_plan9_foreign_aliases(const char *line,
                              const Plan9ForeignAlias *aliases,
                              int alias_count, char *output, size_t size)
{
    const char *read;
    size_t written = 0;
    char *asm_at;
    const char *quote;
    const char *close;
    const char *tail;
    size_t length;

    for(read = line; *read != '\0';) {
        int matched = -1;
        int index;

        for(index = 0; index < alias_count; index++) {
            length = strlen(aliases[index].alias);
            if(strncmp(read, aliases[index].alias, length) == 0 &&
               !isalnum((unsigned char)read[length]) &&
               read[length] != '_') {
                matched = index;
                break;
            }
        }
        if(matched >= 0) {
            const char *replacement = aliases[matched].symbol;

            length = strlen(replacement);
            if(written + length + 1 >= size)
                return -1;
            memcpy(output + written, replacement, length);
            written += length;
            read += strlen(aliases[matched].alias);
            continue;
        }
        if(written + 2 > size)
            return -1;
        output[written++] = *read++;
    }
    output[written] = '\0';

    asm_at = strstr(output, "__asm__(\"");
    if(asm_at == NULL)
        return 0;
    quote = asm_at + strlen("__asm__(\"");
    close = strchr(quote, '"');
    if(close == NULL)
        return -1;
    tail = close + 1;
    while(*tail == ' ' || *tail == '\t')
        tail++;
    if(*tail != ')' || tail[1] != ';')
        return -1;
    while(asm_at > output &&
          (asm_at[-1] == ' ' || asm_at[-1] == '\t'))
        asm_at--;
    if((size_t)(asm_at - output) + 3 >= size)
        return -1;
    {
        size_t symbol_length = (size_t)(close - quote);
        char symbol[PLAN9_NAME_MAX];
        if(symbol_length >= sizeof(symbol)) return -1;
        memcpy(symbol, quote, symbol_length);
        symbol[symbol_length] = '\0';
        if(c_plan9_runtime_symbol(symbol)) return 1;
    }
    asm_at[0] = ';';
    asm_at[1] = '\n';
    asm_at[2] = '\0';
    return 0;
}

int
c_plan9_runtime_symbol(const char *name)
{
    static const char * const symbols[] = {
        "snprint", "getenv", "create", "write", "close",
        "malloc", "calloc", "realloc", "free", "open", "pread", "pwrite",
        "seek", "remove", "dirstat", "nulldir", "dirwstat", "dirreadall",
        "read", "pipe", "rfork", "exec", "exits", "getpid", "putenv", "sleep",
        "nsec", "localtime", "gmtime", "getwd", "chdir", NULL
    };
    int index;
    for(index = 0; symbols[index] != NULL; index++)
        if(strcmp(symbols[index], name) == 0) return 1;
    return 0;
}
/* Buffers rewrite_plan9_print keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RewritePlan9PrintBuffers {
    char out[PLAN9_LINE_MAX];
} RewritePlan9PrintBuffers;

static void rewrite_plan9_print(char *line, size_t size);

static void
rewrite_plan9_print_with_buffers(char *line, size_t size, RewritePlan9PrintBuffers *buffers)
{
    size_t used = 0;
    const char *p = line;
    if(strstr(line, "printf(") == NULL && strstr(line, "fwrite(") == NULL)
        return;
    while(*p && used + 8 < sizeof(buffers->out)) {
        if(!strncmp(p, "printf(", 7)) {
            memcpy(buffers->out + used, "print(", 6);
            used += 6;
            p += 7;
        } else if(!strncmp(p, "%llu", 4)) {
            memcpy(buffers->out + used, "%llud", 5);
            used += 5;
            p += 4;
        } else if(!strncmp(p, "fwrite(", 7)) {
            /* fwrite(text, 1, n, stdout) -> write(1, text, n) */
            const char *text = p + 7, *one = strstr(text, ", 1, ");
            const char *count = one ? one + 5 : NULL;
            const char *stream = count ? strstr(count, ", stdout)") : NULL;
            if(stream == NULL) {
                buffers->out[used++] = *p++;
                continue;
            }
            used += (size_t)snprintf(buffers->out + used, sizeof(buffers->out) - used, "write(1, %.*s, %.*s)",
                                     (int)(one - text), text, (int)(stream - count), count);
            p = stream + 9;
        } else
            buffers->out[used++] = *p++;
    }
    buffers->out[used] = '\0';
    snprintf(line, size, "%s", buffers->out);
}

/* Plan 9's libc prints with print, spells unsigned %llud, and writes raw
 * bytes with write. */
static void
rewrite_plan9_print(char *line, size_t size)
{
    static _Thread_local RewritePlan9PrintBuffers *spares[16];
    static _Thread_local int spare_count;
    RewritePlan9PrintBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    rewrite_plan9_print_with_buffers(line, size, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
}
/* Buffers c_plan9_rewrite_once keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct CPlan9RewriteOnceBuffers {
    int pending_depths[256];
    Plan9ForeignAlias aliases[128];
    char current[PLAN9_LINE_MAX];
    char rewritten[PLAN9_LINE_MAX];
} CPlan9RewriteOnceBuffers;

char *c_plan9_rewrite_once(const char *text);

static char *
c_plan9_rewrite_once_with_buffers(const char *text, CPlan9RewriteOnceBuffers *buffers)
{
    Buf out;
    char *line;
    const char *cursor = text;
    int lineno = 0;
    int pending_count = 0;
    int depth = 0;
    int runtime_include = 0;
    int alias_count;
    alias_count = collect_plan9_foreign_aliases(text, buffers->aliases, 128);
    if(alias_count < 0)
        return NULL;
    line = malloc(PLAN9_LINE_MAX);
    if(line == NULL)
        return NULL;
    if(!buf_init(&out)) {
        free(line);
        return NULL;
    }
    /* print helpers need write and strtod; only printing files ask for them,
     * before any header can include the runtime. */
    int print_guard = strstr(text, "print_float(") != NULL &&
                      strstr(text, "#define ZIR_PLAN9_PRINT 1\n") == NULL;
    /* The guard goes after the generated-file comment, else first. */
    if(print_guard && strncmp(text, "/* Generated", 12) != 0) {
        if(buf_puts(&out, "#define ZIR_PLAN9_PRINT 1\n") < 0)
            goto fail;
        print_guard = 0;
    }
    while(*cursor != '\0') {
        const char *nl = strchr(cursor, '\n');
        size_t n = nl != NULL ? (size_t)(nl - cursor) + 1 : strlen(cursor);
        char indent[256];
        size_t ilen = 0;
        char ftype[64];
        char fname[PLAN9_NAME_MAX];
        const char *rest;
        int emitted = 0;
        if(n >= PLAN9_LINE_MAX)
            n = PLAN9_LINE_MAX - 1;
        memcpy(buffers->current, cursor, n);
        buffers->current[n] = '\0';
        cursor += n;
        lineno++;
        if(lineno == 1 && print_guard) {
            if(buf_puts(&out, buffers->current) < 0 ||
               buf_puts(&out, "#define ZIR_PLAN9_PRINT 1\n") < 0)
                goto fail;
            continue;
        }
        {
            int foreign_result =
                rewrite_plan9_foreign_aliases(buffers->current, buffers->aliases, alias_count,
                                              buffers->rewritten, sizeof(buffers->rewritten));
            if(foreign_result < 0)
                goto fail;
            if(foreign_result > 0)
                continue;
        }
        if(snprintf(buffers->current, sizeof(buffers->current), "%s", buffers->rewritten) >=
            (int)sizeof(buffers->current))
            goto fail;
        rewrite_integer_suffixes(buffers->current);
        {
            char *empty = strstr(buffers->current, " = {};");
            if(empty != NULL && empty[6] == '\n' && empty[7] == '\0') {
                /* A zero-sized array needs no initializer. Records which
                 * contain an empty array still need their other fields zero. */
                if(memchr(buffers->current, '[', (size_t)(empty - buffers->current)) != NULL) {
                    char *bound = buffers->current;
                    /* 8c requires positive storage, even when the language
                     * array's logical count and size are zero. This byte is
                     * never exposed by its slice or checked indexing. */
                    while((bound = strstr(bound, "[0]")) != NULL && bound < empty) {
                        bound[1] = '1';
                        bound += 3;
                    }
                    strcpy(empty, ";\n");
                } else {
                    size_t used = strlen(buffers->current);
                    if(used + 1 >= sizeof(buffers->current))
                        goto fail;
                    memmove(empty + 5, empty + 4, strlen(empty + 4) + 1);
                    empty[4] = '0';
                }
            }
        }
        rewrite_plan9_print(buffers->current, sizeof(buffers->current));
        while(ilen + 1 < sizeof(indent) &&
              (buffers->current[ilen] == ' ' || buffers->current[ilen] == '\t')) {
            indent[ilen] = buffers->current[ilen];
            ilen++;
        }
        indent[ilen] = '\0';
        /* close for-decl blocks that ended at this depth */
        while(pending_count > 0 && depth == buffers->pending_depths[pending_count - 1]) {
            pending_count--;
            if(buf_printf(&out, "%s}\n", indent) < 0)
                goto fail;
        }
        if(strcmp(buffers->current, "int32_t main(void);\n") == 0) {
            if(buf_puts(&out,
                        "int32_t ziran_plan9_main(void);\n"
                        "void main(void);\n") < 0)
                goto fail;
            continue;
        }
        if(strcmp(buffers->current, "main(void)\n") == 0) {
            if(buf_puts(&out, "ziran_plan9_main(void)\n") < 0)
                goto fail;
            continue;
        }
        {
            char parameters[512];
            int prototype;

            if(plan9_main_arguments(buffers->current, parameters, sizeof(parameters), &prototype)) {
                if(buf_printf(&out, prototype ?
                              "int32_t ziran_plan9_main(%s);\nvoid main(int argc, char** argv);\n" :
                              "ziran_plan9_main(%s)\n", parameters) < 0)
                    goto fail;
                continue;
            }
        }
        if(strcmp(buffers->current, "#include \"zir_plan9_runtime.h\"\n") == 0) {
            if(runtime_include == 0) {
                if(buf_puts(&out, buffers->current) < 0)
                    goto fail;
                runtime_include = 1;
            }
            continue;
        }

        if(strstr(buffers->current, "#include <stdint.h>") != NULL ||
           strstr(buffers->current, "#include <stddef.h>") != NULL ||
           strstr(buffers->current, "#include <stdbool.h>") != NULL ||
           strstr(buffers->current, "#include <stdlib.h>") != NULL ||
           strstr(buffers->current, "#include <string.h>") != NULL ||
           strstr(buffers->current, "#include \"zir_bounds.h\"") != NULL ||
           strstr(buffers->current, "#include \"zir_string.h\"") != NULL ||
           strstr(buffers->current, "#include \"zir_slice.h\"") != NULL ||
           strstr(buffers->current, "#include \"zir_vec.h\"") != NULL) {
            if(runtime_include == 0) {
                if(buf_puts(&out, "#include \"zir_plan9_runtime.h\"\n") < 0)
                    goto fail;
                runtime_include = 1;
            }
            continue;
        }
        if(strstr(buffers->current, "static inline ") != NULL) {
            const char *inline_at = strstr(buffers->current, "static inline ");
            size_t prefix = (size_t)(inline_at - buffers->current);
            if(snprintf(buffers->rewritten, sizeof(buffers->rewritten), "%.*sstatic %s",
                        (int)prefix, buffers->current,
                        inline_at + strlen("static inline ")) >=
                (int)sizeof(buffers->rewritten))
                goto fail;
            if(buf_puts(&out, buffers->rewritten) < 0)
                goto fail;
            continue;
        }
        {
            int abi_result = rewrite_ziran_abi_marker(buffers->current, &out);
            if(abi_result < 0)
                goto fail;
            if(abi_result > 0)
                continue;
        }
        if(line_passthrough(buffers->current)) {
            if(buf_puts(&out, buffers->current) < 0)
                goto fail;
            continue;
        }
        /* 1. __auto_type: resolve to a concrete type where possible */
        if(strstr(buffers->current, "__auto_type") != NULL) {
            if(rewrite_autotype(buffers->current, buffers->rewritten, sizeof(buffers->rewritten))) {
                snprintf(buffers->current, sizeof(buffers->current), "%s", buffers->rewritten);
            } else {
                unresolved_count++;
            }
        }
        /* 2. for-declaration hoisting */
        if(for_decl_split(buffers->current, ftype, sizeof(ftype), fname,
                          sizeof(fname), &rest)) {
            if(buf_printf(&out, "%s{\n", indent) < 0)
                goto fail;
            if(buf_printf(&out, "%s    %s %s;\n", indent, ftype, fname) < 0)
                goto fail;
            if(buf_printf(&out, "%s    for(%s%s", indent, fname, rest) < 0)
                goto fail;
            if(pending_count < 256) {
                buffers->pending_depths[pending_count] = depth;
                pending_count++;
            }
            depth += brace_delta(buffers->current);
            emitted = 1; /* statement fully emitted above */
        }
        /* 3. compound literals in the statement */
        if(!emitted) {
            int rewrote = 0;
            /* 8c does not zero omitted local aggregate fields. Build direct
             * designated initializers through the same zeroed temporary path
             * as cast-shaped literals, including reordered fields. */
            if(depth > 0) {
                const char *initializer = strstr(buffers->current, " = {");
                if(initializer != NULL) {
                    const char *name_end = initializer;
                    const char *name_start = name_end;
                    char type[PLAN9_TYPE_MAX], name[PLAN9_NAME_MAX];
                    int open = (int)(initializer - buffers->current) + 3;
                    int close = find_matching_brace(buffers->current, open);
                    while(name_start > buffers->current + ilen &&
                          (isalnum((unsigned char)name_start[-1]) || name_start[-1] == '_'))
                        name_start--;
                    if(name_start > buffers->current + ilen && close > open &&
                       buffers->current[close + 1] == ';' &&
                       body_all_designated(buffers->current + open + 1, close - open - 1) &&
                       copy_trimmed(buffers->current, (int)ilen,
                                    (int)(name_start - buffers->current), type, sizeof(type)) == 0 &&
                       copy_trimmed(buffers->current, (int)(name_start - buffers->current),
                                    (int)(name_end - buffers->current), name, sizeof(name)) == 0 &&
                       strchr(type, '[') == NULL && strchr(type, '(') == NULL &&
                       strchr(type, '.') == NULL && strchr(type, '=') == NULL) {
                        if(emit_compound_temp(&out, indent, type,
                                              buffers->current + open + 1,
                                              close - open - 1, name) < 0)
                            goto fail;
                        emitted = 2;
                    }
                }
            }
            /* "Type name[N] = (Type[N]){0};" is an array declaration:
             * zero the named array instead of assigning through a temp */
            {
                const char *eq = strchr(buffers->current, '=');
                const char *assign = NULL;
                const char *scan_eq;
                const char *cut = NULL;
                for(scan_eq = eq; scan_eq != NULL;
                    scan_eq = strchr(scan_eq + 1, '=')) {
                    const char *after = scan_eq + 1;
                    while(*after == ' ' || *after == '\t')
                        after++;
                    if(*after == '(' && after[-1] != '=' && after[-1] != '!'
                       && after[-1] != '<' && after[-1] != '>') {
                        assign = after;
                        cut = scan_eq;
                        break;
                    }
                }
                if(assign != NULL && strchr(buffers->current, '[') != NULL) {
                    const char *close = strchr(assign + 1, ')');
                    const char *brace_p = close != NULL ? strchr(close, '{')
                                                        : NULL;
                    const char *close_b = brace_p != NULL
                        ? strchr(brace_p + 1, '}') : NULL;
                    const char *tail = close_b != NULL ? close_b + 1 : NULL;
                    if(tail != NULL && (*tail == ';' || *tail == '\n'
                                        || *tail == '\0')
                       && assign > buffers->current
                       && memchr(buffers->current, ']', (size_t)(assign - buffers->current))
                          != NULL
                       && memchr(buffers->current, '(', (size_t)(assign - buffers->current))
                          == NULL) {
                        char name[PLAN9_NAME_MAX];
                        const char *bracket = memchr(buffers->current, '[',
                            (size_t)(assign - buffers->current));
                        const char *name_end = bracket;
                        const char *name_start;
                        size_t nlen;
                        while(name_end > buffers->current
                              && (name_end[-1] == ' ' || name_end[-1] == '\t'))
                            name_end--;
                        name_start = name_end;
                        while(name_start > buffers->current
                              && (isalnum((unsigned char)name_start[-1])
                                  || name_start[-1] == '_'))
                            name_start--;
                        nlen = (size_t)(name_end - name_start);
                        if(nlen > 0 && nlen < sizeof(name)
                           && name_start > buffers->current + ilen
                           && memchr(buffers->current, '.', (size_t)(name_start - buffers->current)) == NULL
                           && (name_start - buffers->current < 2
                               || memmem(buffers->current, (size_t)(name_start - buffers->current), "->", 2) == NULL)) {
                            memcpy(name, name_start, nlen);
                            name[nlen] = '\0';
                            if(buf_append(&out, buffers->current,
                                          (size_t)(cut - buffers->current)) < 0
                               || buf_puts(&out, ";\n") < 0
                               || buf_printf(&out,
                                             "%smemset(%s, 0, sizeof(%s));\n",
                                             indent, name, name) < 0)
                                goto fail;
                            emitted = 2;
                        }
                    }
                }
            }
            if(!emitted && strstr(buffers->current, "){") != NULL
               && strchr(buffers->current, '(') != NULL) {
                if(rewrite_line_compound(&out, buffers->current, lineno, &rewrote) < 0)
                    goto fail;
            }
            if(rewrote == 0 && emitted == 0) {
                if(buf_puts(&out, buffers->current) < 0)
                    goto fail;
            }
            depth += brace_delta(buffers->current);
        }
        while(pending_count > 0 && depth == buffers->pending_depths[pending_count - 1]) {
            pending_count--;
            if(buf_printf(&out, "%s}\n", indent) < 0)
                goto fail;
        }
    }
    free(line);
    return out.data;
fail:
    free(line);
    free(out.data);
    return NULL;
}

char *
c_plan9_rewrite_once(const char *text)
{
    static _Thread_local CPlan9RewriteOnceBuffers *spares[16];
    static _Thread_local int spare_count;
    CPlan9RewriteOnceBuffers *buffers = spare_count > 0 ? spares[--spare_count] :
        AllocateOrExit(sizeof(*buffers));
    char *returned = c_plan9_rewrite_once_with_buffers(text, buffers);
    if(spare_count < 16)
        spares[spare_count++] = buffers;
    else
        free(buffers);
    return returned;
}
