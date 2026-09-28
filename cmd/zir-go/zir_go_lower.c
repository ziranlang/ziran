/*
 * zir_go_lower.c - ZIR to Go backend. See zir_go_lower.h for scope.
 */
#include "zir_go_lower.h"
#include "zir_text.h"
#include "zir_emit.h"
#include "zir_check.h"
#include "zir_expr.h"
#include "zir_diagnostic.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ZIR_GO_TEXT_MAX 8192
#define ZIR_GO_NAME_MAX 256
#define ZIR_GO_EXTERN_PARAM_MAX 16

static const char *
enum_storage_type(const char *backing)
{
    static const struct { const char *name, *go_type; } types[] = {
        {"s8", "int8"}, {"u8", "uint8"},
        {"s16", "int16"}, {"u16", "uint16"},
        {"s32", "int32"}, {"u32", "uint32"},
        {"s64", "int64"}, {"u64", "uint64"}
    };
    for(size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if(strcmp(backing, types[i].name) == 0)
            return types[i].go_type;
    return NULL;
}
static const ZirModule *type_scope;

typedef struct {
    char source[ZIR_GO_NAME_MAX];
    char go[ZIR_GO_NAME_MAX];
} ZirGoLocalName;

static ZirGoLocalName zir_go_locals[256];
static int zir_go_local_count;

/* ---------------------------------------------------------------- helpers */

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

static const char *
go_local_name_for(const char *name)
{
    for(int i = zir_go_local_count - 1; i >= 0; i--)
        if(strcmp(zir_go_locals[i].source, name) == 0)
            return zir_go_locals[i].go;
    return NULL;
}

static void
go_register_local_name(const ZirFunction *fn, const char *name)
{
    char mapped[ZIR_GO_NAME_MAX];

    if(name == NULL || name[0] == '\0')
        return;
    for(int i = 0; i < zir_go_local_count; i++)
        if(strcmp(zir_go_locals[i].source, name) == 0)
            return;
    TargetBindingName(fn, ZIR_GO, name, mapped, sizeof(mapped));
    if(strcmp(mapped, name) == 0 || zir_go_local_count >= 256)
        return;
    snprintf(zir_go_locals[zir_go_local_count].source,
             sizeof(zir_go_locals[zir_go_local_count].source), "%s", name);
    snprintf(zir_go_locals[zir_go_local_count].go,
             sizeof(zir_go_locals[zir_go_local_count].go), "%s", mapped);
    zir_go_local_count++;
}

static int g_union_unsafe;

/* Byte width of a scalar field type; zero when the type cannot live in a
 * portable union. */
static size_t
scalar_byte_width(const char *type)
{
    if(!strcmp(type, "s8") || !strcmp(type, "u8") ||
       !strcmp(type, "bool"))
        return 1;
    if(!strcmp(type, "s16") || !strcmp(type, "u16"))
        return 2;
    if(!strcmp(type, "s32") || !strcmp(type, "u32") ||
       !strcmp(type, "float32"))
        return 4;
    if(!strcmp(type, "s64") || !strcmp(type, "u64") ||
       !strcmp(type, "float64") ||
       !strcmp(type, "integer"))
        return 8;
    return 0;
}

static const ZirModule *module_constant_owner(const ZirModule *module,
                                               const char *name);

/* Checked Ziran and generated Go scalar type -> Go type. */
static int
go_type(const char *type, char *dst, size_t dst_size)
{
    char t[ZIR_GO_NAME_MAX];
    size_t n;

    snprintf(t, sizeof(t), "%s", type);
    n = strlen(t);
    while(n > 0 && isspace((unsigned char)t[n - 1]))
        t[--n] = '\0';
    {
        const char *p = t;

        while(*p != '\0' && isspace((unsigned char)*p))
            p++;
        if(p != t)
            memmove(t, p, strlen(p) + 1);
        n = strlen(t);
    }
    const char *scalar = ScalarType(t);
    if(*scalar && TargetType(t,ZIR_GO)) {
        snprintf(dst,dst_size,"%s",TargetType(t,ZIR_GO)); return 1;
    }
    if(t[0] == '[') {
        char *close = strchr(t, ']');
        const char *base;

        if(close != NULL) {
            char bound[ZIR_GO_NAME_MAX];
            snprintf(bound, sizeof(bound), "%.*s", (int)(close - t - 1), t + 1);
            if(type_scope != NULL &&
               module_constant_owner(type_scope, bound) != NULL) {
                char mapped[ZIR_GO_NAME_MAX];
                TargetDefineName(module_constant_owner(type_scope, bound),
                                 ZIR_GO, bound, mapped, sizeof(mapped));
                snprintf(bound, sizeof(bound), "%s", mapped);
            }
            base = close + 1;
            while(*base == ' ' || *base == '\t')
                base++;
            {
                char gt[ZIR_GO_NAME_MAX];

                if(go_type(base, gt, sizeof(gt))) {
                    *close = '\0';
                    snprintf(dst, dst_size, "[%s]%s", bound, gt);
                    return 1;
                }
            }
        }
        return 0;
    }
    if(t[0] == '*' && t[1] != '\0') {
        char gt[ZIR_GO_NAME_MAX];

        if(go_type(t + 1, gt, sizeof(gt)) && strcmp(gt, "string") != 0) {
            snprintf(dst, dst_size, "*%s", gt);
            return 1;
        }
        return 0;
    }
    /* A name alone is not evidence that a type exists. */
    {
        int identish = t[0] != '\0';

        for(char *c = t; *c != '\0'; c++)
            if(!is_ident_char((unsigned char)*c) && *c != '.')
                identish = 0;
        const ZirModule *owner = NULL;
        const ZirType *declared = identish && type_scope != NULL ?
            FindType(type_scope, t, &owner) : NULL;
        if(declared != NULL) {
            if(owner != NULL)
                NativeTypeName(owner, declared, dst, dst_size);
            else
                snprintf(dst, dst_size, "%s", declared->name);
            return 1;
        }
    }
    return 0;
}

static void
require_go_type(const char *type, char *dst, size_t size, ZirSourceSpan span)
{
    if(go_type(type, dst, size))
        return;
    Diagnostic(span, "zir_go.type", "unsupported or unresolved Go type: %s", type);
    exit(1);
}

static int
module_fn_index(const ZirModule *m, const char *name, size_t len)
{
    for(int i = 0; i < m->function_count; i++) {
        if(strlen(m->functions[i].name) == len &&
           strncmp(m->functions[i].name, name, len) == 0)
            return i;
    }
    return -1;
}

typedef struct {
    char source[ZIR_GO_NAME_MAX];
    char go[ZIR_GO_NAME_MAX * 2];
    char guard[ZIR_GO_NAME_MAX];
    const ZirModule *module;
} ZirGoGlobalFunction;

enum { ZIR_GO_GLOBAL_FUNCTION_MAX = 4096 };

static ZirGoGlobalFunction g_functions[ZIR_GO_GLOBAL_FUNCTION_MAX];
static int g_function_count;

static int
go_global_function_index(const ZirModule *module, const char *name, size_t len)
{
    char ident[ZIR_NAME_MAX];
    const ZirModule *owner = NULL;
    const ZirFunction *function = NULL;
    if(len >= sizeof(ident))
        return -1;
    memcpy(ident, name, len);
    ident[len] = '\0';
    if(ResolveFunction(module, ident, &owner, &function) != 1)
        return -1;
    for(int i = 0; i < g_function_count; i++) {
        if(g_functions[i].module == owner &&
           strcmp(g_functions[i].source, function->name) == 0)
            return i;
    }
    return -1;
}

static const char *
go_bound_provider(const char *module, const char *function)
{
    for(int i = 0; i < g_function_count; i++)
        if(strcmp(g_functions[i].module->name, module) == 0 &&
           strcmp(g_functions[i].source, function) == 0)
            return g_functions[i].go;
    return NULL;
}

static void
go_build_global_functions(const ZirProgram *const *progs, int prog_count)
{
    g_function_count = 0;
    for(int pi = 0; pi < prog_count; pi++) {
        const ZirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const ZirModule *m = &prog->modules[mi];
            char stem[ZIR_PATH_MAX];
            char guard[ZIR_GO_NAME_MAX];

            NativeGoModuleIdentity(progs, prog_count, m, stem, sizeof(stem),
                                   guard, sizeof(guard));
            for(int fi = 0; fi < m->function_count; fi++) {
                const ZirFunction *fn = &m->functions[fi];
                char fname[ZIR_GO_NAME_MAX];

                if(fn->is_extern || fn->is_template ||
                   g_function_count >= ZIR_GO_GLOBAL_FUNCTION_MAX)
                    continue;
                camel_ident(fn->name, fname, sizeof(fname));
                snprintf(g_functions[g_function_count].source,
                         sizeof(g_functions[0].source), "%s", fn->name);
                snprintf(g_functions[g_function_count].guard,
                         sizeof(g_functions[0].guard), "%s", guard);
                snprintf(g_functions[g_function_count].go,
                         sizeof(g_functions[0].go), "%s_%s", guard, fname);
                g_functions[g_function_count].module = m;
                g_function_count++;
            }
        }
    }
}

/* ------------------------------------------------ module lowering context */

static int split_top(const char *s, char parts[][ZIR_GO_TEXT_MAX], int max);

/* One '#foreign host_api' declaration bridged to a Go host method. */
typedef struct {
    char source[ZIR_GO_NAME_MAX];        /* source call name */
    char go[ZIR_GO_NAME_MAX];         /* Host interface method */
    char go_import_path[ZIR_PATH_MAX];  /* direct Go package import path */
    char go_import_alias[ZIR_GO_NAME_MAX]; /* import alias for direct calls */
    char pnames[ZIR_GO_EXTERN_PARAM_MAX][ZIR_GO_NAME_MAX];  /* parameter names */
    char ptypes[ZIR_GO_EXTERN_PARAM_MAX][ZIR_GO_NAME_MAX];  /* parameter source types */
    int pcount;
    char ret[ZIR_GO_NAME_MAX];
    char host_var[ZIR_GO_NAME_MAX + 8];   /* guard-prefixed host var */
    int direct_go;
    int direct_ziran;
    char provider_go[ZIR_GO_NAME_MAX * 2];
} ZirGoExtern;

typedef struct {
    char go[ZIR_GO_NAME_MAX * 2];
    char val[ZIR_GO_TEXT_MAX];        /* explicit value text, or "" */
} ZirGoEnumMember;

typedef struct {
    char go_type[ZIR_GO_NAME_MAX];
    char prefix[ZIR_GO_NAME_MAX];     /* enum-name prefix for Go const names */
    ZirGoEnumMember members[64];
    int count;
} ZirGoEnum;

/* Lowering is single-threaded and per-module sequential: one cached context. */
static char g_guard[ZIR_GO_NAME_MAX];
static ZirGoExtern g_externs[64];
static int g_extern_count;
static ZirGoEnum g_enums[32];
static int g_enum_count;

static int
go_extern_index(const char *name, size_t len)
{
    for(int i = 0; i < g_extern_count; i++) {
        if(strlen(g_externs[i].source) == len &&
           strncmp(g_externs[i].source, name, len) == 0)
            return i;
    }
    return -1;
}

/* "a: int, b: char*" -> parameter names/types on ex. */
static void
split_params(const char *args, ZirGoExtern *ex)
{
    char parts[ZIR_GO_EXTERN_PARAM_MAX][ZIR_GO_TEXT_MAX];
    int n = split_top(args, parts, ZIR_GO_EXTERN_PARAM_MAX);

    ex->pcount = 0;
    for(int i = 0; i < n; i++) {
        char *colon = strchr(parts[i], ':');
        size_t nl;

        if(colon == NULL)
            continue;
        nl = (size_t)(colon - parts[i]);
        while(nl > 0 && parts[i][nl - 1] == ' ')
            nl--;
        if(nl == 0 || ex->pcount >= ZIR_GO_EXTERN_PARAM_MAX)
            continue;
        snprintf(ex->pnames[ex->pcount], ZIR_GO_NAME_MAX, "%.*s", (int)nl,
                 parts[i]);
        {
            const char *pt = colon + 1;

            while(*pt == ' ' || *pt == '\t')
                pt++;
            snprintf(ex->ptypes[ex->pcount], ZIR_GO_NAME_MAX, "%s", pt);
        }
        ex->pcount++;
    }
}

/* Extract the Go method name from a resolved #foreign package target: the segment
 * after the last dot. */
static void
extern_go_name(const char *target, const char *source, char *dst, size_t dst_size)
{
    const char *dot = strrchr(target, '.');
    const char *base = dot != NULL ? dot + 1 : target;

    if(base[0] != '\0') {
        snprintf(dst, dst_size, "%s", base);
        for(char *c = dst; *c != '\0'; c++)
            if(!is_ident_char((unsigned char)*c))
                *c = '_';
    } else {
        camel_ident(source, dst, dst_size);
    }
}

/* A fully-qualified Go extern target uses an import path plus function name,
 * e.g. 'github.com/waozixyz/pass.Generate'. Short targets use the declared
 * host interface. */
static int
extern_direct_go_target(const char *target, char *import_path,
                        size_t import_path_size, char *alias,
                        size_t alias_size)
{
    const char *dot;
    const char *slash;
    const char *base;
    size_t n = 0;

    if(target == NULL)
        return 0;
    dot = strrchr(target, '.');
    slash = strrchr(target, '/');
    if(dot == NULL || slash == NULL || slash > dot)
        return 0;
    snprintf(import_path, import_path_size, "%.*s", (int)(dot - target),
             target);
    base = slash + 1;
    while(base < dot && n + 1 < alias_size) {
        char c = *base++;

        if(isalnum((unsigned char)c) || c == '_')
            alias[n++] = c;
        else
            alias[n++] = '_';
    }
    if(n == 0 && alias_size > 1)
        alias[n++] = 'x';
    if(isdigit((unsigned char)alias[0]) && n + 1 < alias_size) {
        memmove(alias + 1, alias, n + 1);
        alias[0] = 'x';
        n++;
    }
    alias[n] = '\0';
    return import_path[0] != '\0';
}

/* Register one extern (idempotent: first declaration wins). */
static void
add_extern(const char *source, const char *args, const char *ret,
           const char *target, const char *provider_symbol,
           ZirSourceSpan span)
{
    ZirGoExtern *ex;
    if(target && !strncmp(target, "c.", 2)) {
        Diagnostic(span, "zir_go.import",
                   "native Go cannot import a C ABI symbol: %s; use a Go package or host interface",
                   target);
        exit(1);
    }

    if(go_extern_index(source, strlen(source)) >= 0 || g_extern_count >= 64)
        return;
    ex = &g_externs[g_extern_count++];
    memset(ex, 0, sizeof(*ex));
    snprintf(ex->source, sizeof(ex->source), "%s", source);
    snprintf(ex->ret, sizeof(ex->ret), "%s", ret);
    split_params(args, ex);
    if(target != NULL && strncmp(target, "ziran:", 6) == 0) {
        const char *bound = go_bound_provider(target + 6, provider_symbol);
        if(bound == NULL) {
            Diagnostic(span, "zir_go.bind",
                       "bound Ziran host provider is missing: %s:%s",
                       target + 6, provider_symbol);
            exit(1);
        }
        ex->direct_ziran = 1;
        snprintf(ex->provider_go, sizeof(ex->provider_go), "%s", bound);
    } else {
        extern_go_name(target, source, ex->go, sizeof(ex->go));
        ex->direct_go = extern_direct_go_target(target, ex->go_import_path,
                                                sizeof(ex->go_import_path),
                                                ex->go_import_alias,
                                                sizeof(ex->go_import_alias));
    }
    /* Derive the host variable name from the module guard. */
    snprintf(ex->host_var, sizeof(ex->host_var), "%c%sHost",
             (char)tolower((unsigned char)g_guard[0]), g_guard + 1);
}

/* Whether any module prints a float, which needs formatFloat. */
static int
go_prints_floats(const ZirProgram *const *programs, int count)
{
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *fn = &module->functions[f];
                for(int e = 0; e < fn->expr_count; e++) {
                    const ZirExpr *call = &fn->exprs[e];
                    if(call->kind != ZIR_EXPR_CALL || strcmp(call->name, "print") ||
                       call->first_child < 0)
                        continue;
                    for(int arg = fn->exprs[call->first_child].next_sibling; arg >= 0;
                        arg = fn->exprs[arg].next_sibling)
                        if(!strcmp(fn->exprs[arg].type, "float32") ||
                           !strcmp(fn->exprs[arg].type, "float64"))
                            return 1;
                }
            }
        }
    return 0;
}

/* Whether any module names Source_Code_Location, the #caller_location type. */
static int
go_uses_caller_location(const ZirProgram *const *programs, int count)
{
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            const ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; t < module->type_count; t++)
                if(strstr(module->types[t].body, "Source_Code_Location"))
                    return 1;
            for(int g = 0; g < module->global_count; g++)
                if(strstr(module->globals[g].type, "Source_Code_Location"))
                    return 1;
            for(int f = 0; f < module->function_count; f++) {
                const ZirFunction *fn = &module->functions[f];
                if(strstr(fn->args, "Source_Code_Location") ||
                   strstr(fn->return_type, "Source_Code_Location"))
                    return 1;
                for(int e = 0; e < fn->expr_count; e++)
                    if(strstr(fn->exprs[e].type, "Source_Code_Location"))
                        return 1;
                for(int s = 0; s < fn->stmt_count; s++)
                    if(strstr(fn->stmts[s].type, "Source_Code_Location"))
                        return 1;
            }
        }
    return 0;
}

/* Parse "name :: (args) -> ret #foreign library;" from a raw foreign import
 * line (the ZirImport.signature keeps the whole declaration). */
static void
parse_extern_import(const ZirImport *imp)
{
    char args[ZIR_GO_TEXT_MAX];
    char ret[ZIR_GO_NAME_MAX];
    char target[ZIR_PATH_MAX];
    const char *lp = strchr(imp->signature, '(');
    const char *rp = lp != NULL ? strchr(lp, ')') : NULL;
    const char *dir = strstr(imp->signature, "#foreign");

    args[0] = '\0';
    snprintf(ret, sizeof(ret), "void");
    target[0] = '\0';
    if(lp != NULL && rp != NULL && rp > lp)
        snprintf(args, sizeof(args), "%.*s", (int)(rp - lp - 1), lp + 1);
    if(rp != NULL) {
        const char *arrow = strstr(rp, "->");

        if(arrow != NULL && (dir == NULL || arrow < dir)) {
            const char *r = arrow + 2;
            size_t n = 0;

            while(*r == ' ' || *r == '\t')
                r++;
            while(*r != '\0' && *r != '#' && n + 1 < sizeof(ret))
                ret[n++] = *r++;
            while(n > 0 && (ret[n - 1] == ' ' || ret[n - 1] == '\t'))
                n--;
            ret[n] = '\0';
        }
    }
    if(imp->target[0] != '\0' && strcmp(imp->target, imp->name) != 0) {
        snprintf(target, sizeof(target), "%s", imp->target);
    } else if(dir != NULL) {
        const char *q = strchr(dir + 7, '"');

        if(q != NULL) {
            size_t n = 0;
            const char *r = q + 1;

            while(*r != '\0' && *r != '"' && n + 1 < sizeof(target))
                target[n++] = *r++;
            target[n] = '\0';
        }
    }
    add_extern(imp->name, args, ret, target, imp->extern_symbol, imp->span);
}

/* Parse enum body members. The parser may deliver them newline-separated or
 * comma-joined on one line ('A = 0, B, C'), so split on both. */
static void
parse_enum(const ZirModule *owner, const ZirType *t)
{
    ZirGoEnum *e;
    const char *p = t->body;

    if(g_enum_count >= 32)
        return;
    e = &g_enums[g_enum_count++];
    memset(e, 0, sizeof(*e));
    if(t->native_name_mangled)
        NativeTypeName(owner, t, e->go_type, sizeof(e->go_type));
    else
        camel_ident(t->name, e->go_type, sizeof(e->go_type));
    camel_ident(t->name, e->prefix, sizeof(e->prefix));
    while(*p != '\0') {
        char line[ZIR_GO_TEXT_MAX];
        char *name, *val;
        size_t n = 0;

        /* collect one member: up to ',' or newline */
        while(*p != '\0' && *p != ',' && *p != '\n' &&
              n + 1 < sizeof(line))
            line[n++] = *p++;
        if(*p == ',' || *p == '\n')
            p++;
        line[n] = '\0';
        /* trim */
        {
            char *s = line;

            while(*s == ' ' || *s == '\t' || *s == '\r')
                s++;
            memmove(line, s, strlen(s) + 1);
        }
        {
            size_t ln = strlen(line);

            while(ln > 0 && (line[ln - 1] == ' ' || line[ln - 1] == '\t' ||
                             line[ln - 1] == '\r'))
                line[--ln] = '\0';
        }
        if(line[0] == '\0')
            continue;
        name = line;
        val = strchr(line, '=');
        if(val != NULL) {
            *val = '\0';
            val++;
            while(*val == ' ' || *val == '\t')
                val++;
        }
        {
            size_t nn = strlen(name);

            while(nn > 0 && (name[nn - 1] == ' ' || name[nn - 1] == '\t'))
                name[--nn] = '\0';
        }
        if(name[0] == '\0' || e->count >= 64)
            continue;
        ZirGoEnumMember *m = &e->members[e->count++];
        memset(m, 0, sizeof(*m));
        if(!NativeEnumMemberName(owner, t, name, m->go, sizeof(m->go))) {
            if(e->prefix[0] != '\0' &&
               strncmp(name, e->prefix, strlen(e->prefix)) == 0)
                camel_ident(name, m->go, sizeof(m->go));
            else if(e->prefix[0] != '\0')
                snprintf(m->go, sizeof(m->go), "%s%s", e->prefix, name);
            else
                camel_ident(name, m->go, sizeof(m->go));
        }
        if(val != NULL)
            snprintf(m->val, sizeof(m->val), "%s", val);
    }
}

/* (Re)build the per-module context: extern bridge + enum constants. Must run
 * before any expression or statement is emitted for the module. */
static void
go_set_module(const ZirModule *m, const char *guard)
{
    snprintf(g_guard, sizeof(g_guard), "%s", guard);
    g_extern_count = 0;
    g_enum_count = 0;
    for(int i = 0; i < m->import_count; i++) {
        if(m->imports[i].kind == ZIR_IMPORT_EXTERN)
            parse_extern_import(&m->imports[i]);
    }
    for(int i = 0; i < m->function_count; i++) {
        const ZirFunction *fn = &m->functions[i];

        if(!fn->is_extern)
            continue;
        if(fn->extern_target[0] != '\0')
            add_extern(fn->name, fn->args, fn->return_type,
                       fn->extern_target, "", fn->span);
        else
            add_extern(fn->name, fn->args, fn->return_type, "", "", fn->span);
    }
    for(int i = 0; i < m->type_count; i++) {
        if(m->types[i].is_enum)
            parse_enum(m, &m->types[i]);
    }
}


/* Convert scalar arguments to the checked parameter type at the Go boundary. */
static const char *
conv_arg(const char *source_type, const char *expr)
{
    static char buf[ZIR_GO_TEXT_MAX];
    const char *go_type_name = TargetType(source_type, ZIR_GO);

    if(go_type_name != NULL && go_type_name[0] != '\0' &&
       strcmp(go_type_name, "string") != 0 &&
       strcmp(go_type_name, "bool") != 0)
        snprintf(buf, sizeof(buf), "%s(%s)", go_type_name, expr);
    else
        snprintf(buf, sizeof(buf), "%s", expr);
    return buf;
}

/* ------------------------------------------------------- expression pass */

/* Forward */
static void tx_expr(const ZirModule *m, const char *src, char *dst,
                    size_t dst_size);

/* Translate the inside of a braced/paren group starting after the opener;
 * returns the position after the matching closer. */
static const char *
tx_group(const ZirModule *m, const char *src, char *dst, size_t *dn,
         char open, char close)
{
    int depth = 1;
    char mid[ZIR_GO_TEXT_MAX];
    size_t mn = 0;
    const char *p = src;

    while(*p != '\0' && depth > 0 && mn + 1 < sizeof(mid)) {
        if(*p == open)
            depth++;
        else if(*p == close) {
            depth--;
            if(depth == 0)
                break;
        } else if(*p == '"' || *p == '\'') {
            char q = *p;
            mid[mn++] = *p++;
            while(*p != '\0' && *p != q && mn + 1 < sizeof(mid))
                mid[mn++] = *p++;
            if(*p == q)
                mid[mn++] = *p++;
            continue;
        }
        mid[mn++] = *p++;
    }
    mid[mn] = '\0';
    {
        char out[ZIR_GO_TEXT_MAX];

        tx_expr(m, mid, out, sizeof(out));
        if(*dn + strlen(out) + 1 < ZIR_GO_TEXT_MAX) {
            memcpy(dst + *dn, out, strlen(out));
            *dn += strlen(out);
        }
    }
    return *p == close && depth == 0 ? p + 1 : p;
}

/* Split top-level (depth-0) comma parts. */
static int
split_top(const char *s, char parts[][ZIR_GO_TEXT_MAX], int max)
{
    int depth = 0, n = 0;
    const char *start = s;

    for(const char *p = s; ; p++) {
        if(*p == '\0' || (*p == ',' && depth == 0)) {
            size_t len = (size_t)(p - start);

            if(n < max && len < ZIR_GO_TEXT_MAX) {
                memcpy(parts[n], start, len);
                parts[n][len] = '\0';
                n++;
            }
            if(*p == '\0')
                break;
            start = p + 1;
        } else if(*p == '(' || *p == '[' || *p == '{')
            depth++;
        else if(*p == ')' || *p == ']' || *p == '}')
            depth--;
    }
    return n;
}

static void
resolve_slot_type(void *context, const char *source, char *out, size_t size)
{
    (void)context;
    if(!go_type(source, out, size)) {
        Diagnostic(type_scope != NULL ? type_scope->span : (ZirSourceSpan){0},
                   "zir_go.parameter", "unsupported slot parameter type: %s",
                   source);
        exit(1);
    }
}

static const ZirModule *
module_constant_owner(const ZirModule *module, const char *name)
{
    for(int pass = 0; pass < 2; pass++) {
        int count = pass == 0 ? 1 : module->import_count;
        for(int i = 0; i < count; i++) {
            const ZirModule *scope = pass == 0 ? module : module->imports[i].resolved_module;
            if(scope == NULL)
                continue;
            for(int j = 0; j < scope->define_count; j++) {
                if((pass == 0 || scope->defines[j].is_public) &&
                   !strcmp(scope->defines[j].name, name))
                    return scope;
            }
        }
    }
    return NULL;
}

static const ZirModule *
module_global_owner(const ZirModule *module, const char *name)
{
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    return ResolveGlobal(module, name, &owner, &global) == 1 ? owner : NULL;
}

static void
tx_expr(const ZirModule *m, const char *src, char *dst, size_t dst_size)
{
    size_t dn = 0;
    const char *p = src;
    char out[ZIR_GO_TEXT_MAX];

    if(dst_size > ZIR_GO_TEXT_MAX)
        dst_size = ZIR_GO_TEXT_MAX;
    while(*p != '\0' && dn + 8 < dst_size) {
        if(*p == ';' || *p == '\n' || *p == '\r') {
            p++;
            continue;
        }
        if(*p == '"') { /* string literal, verbatim */
            dst[dn++] = *p++;
            while(*p != '\0' && *p != '"' && dn + 2 < dst_size)
                dst[dn++] = *p++;
            if(*p == '"')
                dst[dn++] = *p++;
            continue;
        }
        if(*p == '(') {
            dst[dn++] = *p++;
            p = tx_group(m, p, dst, &dn, '(', ')');
            if(dn + 1 < dst_size)
                dst[dn++] = ')';
            continue;
        }
        if(*p == '{') {
            dst[dn++] = *p++;
            p = tx_group(m, p, dst, &dn, '{', '}');
            if(dn + 1 < dst_size)
                dst[dn++] = '}';
            continue;
        }
        /* Keep numeric literals intact while translating identifiers. */
        if(isdigit((unsigned char)*p)) {
            const char *q = p;
            const char *num_end;

            if(*q == '0' && (q[1] == 'x' || q[1] == 'X')) {
                q += 2;
                while(isxdigit((unsigned char)*q))
                    q++;
            } else {
                while(isdigit((unsigned char)*q) || *q == '.')
                    q++;
                if(*q == 'e' || *q == 'E') {  /* exponent */
                    q++;
                    if(*q == '+' || *q == '-')
                        q++;
                    while(isdigit((unsigned char)*q))
                        q++;
                }
            }
            num_end = q;
            while(p < num_end && dn + 1 < dst_size)
                dst[dn++] = *p++;
            p = q;
            continue;
        }
        if(is_ident_char((unsigned char)*p)) {
            const char *q = p;
            char ident[ZIR_GO_NAME_MAX];
            size_t il = 0;
            int fni;

            while(is_ident_char((unsigned char)*q) && il + 1 < sizeof(ident))
                ident[il++] = *q++;
            ident[il] = '\0';
            if(il == 0) {
                dst[dn++] = *p++;
                continue;
            }
            if(*q == '.' &&
               (isalpha((unsigned char)q[1]) || q[1] == '_') &&
               go_local_name_for(ident) == NULL) {
                const ZirModule *enum_owner = NULL;
                const ZirType *enumeration = FindType(m, ident, &enum_owner);
                const char *member = q + 1;
                const char *end = member;
                while(is_ident_char((unsigned char)*end)) end++;
                size_t length = (size_t)(end - member);
                if(enumeration != NULL && enumeration->is_enum &&
                   length < ZIR_GO_NAME_MAX) {
                    char source_name[ZIR_GO_NAME_MAX];
                    char prefix[ZIR_GO_NAME_MAX];
                    char mapped[ZIR_GO_NAME_MAX * 2];
                    int64_t value;
                    memcpy(source_name, member, length);
                    source_name[length] = '\0';
                    if(EnumMemberValue(enumeration, source_name, &value)) {
                        if(enum_owner == NULL ||
                           !NativeEnumMemberName(enum_owner, enumeration,
                                                 source_name, mapped,
                                                 sizeof(mapped))) {
                            camel_ident(enumeration->name, prefix,
                                        sizeof(prefix));
                            if(prefix[0] &&
                               strncmp(source_name, prefix,
                                       strlen(prefix)) == 0)
                                camel_ident(source_name, mapped, sizeof(mapped));
                            else
                                snprintf(mapped, sizeof(mapped), "%s%s",
                                         prefix, source_name);
                        }
                        length = strlen(mapped);
                        if(dn + length >= dst_size) {
                            Diagnostic(enumeration->span, "zir_go.enum",
                                       "enum member target name exceeds output limit");
                            exit(1);
                        }
                        memcpy(dst + dn, mapped, length);
                        dn += length;
                        p = end;
                        continue;
                    }
                }
            }
            /* '#foreign' bridge: direct Go import for fully-qualified targets,
             * otherwise the declared host-interface method. The check
             * precedes the module-function path because extern prototypes
             * also sit in the function table (bodyless). */
            {
                int xi = go_extern_index(ident, il);

                if(xi >= 0 && *skip_ws(q) == '(') {
                    char raw[ZIR_GO_TEXT_MAX];
                    const char *ap = skip_ws(q) + 1;
                    const char *ae = ap;
                    int depth = 1;
                    size_t rn = 0;
                    int all_ws = 1;

                    while(*ae != '\0' && depth > 0 && rn + 1 < sizeof(raw)) {
                        if(*ae == '"') {
                            all_ws = 0;
                            raw[rn++] = *ae++;
                            while(*ae != '\0' && *ae != '"' &&
                                  rn + 1 < sizeof(raw))
                                raw[rn++] = *ae++;
                            if(*ae == '"')
                                raw[rn++] = *ae++;
                            continue;
                        }
                        if(*ae == '(' || *ae == '[' || *ae == '{')
                            depth++;
                        else if(*ae == ')' || *ae == ']' || *ae == '}') {
                            depth--;
                            if(depth == 0)
                                break;
                        } else if(*ae != ' ' && *ae != '\t')
                            all_ws = 0;
                        raw[rn++] = *ae++;
                    }
                    raw[rn] = '\0';
                    if(*ae == ')')
                        ae++;
                    p = ae;
                    if(g_externs[xi].direct_ziran)
                        dn += (size_t)snprintf(dst + dn, ZIR_GO_TEXT_MAX - dn,
                                               "%s(",
                                               g_externs[xi].provider_go);
                    else if(g_externs[xi].direct_go)
                        dn += (size_t)snprintf(dst + dn, ZIR_GO_TEXT_MAX - dn,
                                               "%s.%s(",
                                               g_externs[xi].go_import_alias,
                                               g_externs[xi].go);
                    else {
                        dn += (size_t)snprintf(dst + dn, ZIR_GO_TEXT_MAX - dn,
                                               "%s.%s(",
                                               g_externs[xi].host_var,
                                               g_externs[xi].go);
                    }
                    if(!all_ws) {
                        char parts[ZIR_GO_EXTERN_PARAM_MAX][ZIR_GO_TEXT_MAX];
                        int n = split_top(raw, parts, ZIR_GO_EXTERN_PARAM_MAX);

                        for(int i = 0; i < n; i++) {
                            char arg[ZIR_GO_TEXT_MAX];

                            tx_expr(m, skip_ws(parts[i]), arg, sizeof(arg));
                            if(i > 0)
                                dn += (size_t)snprintf(dst + dn,
                                                       ZIR_GO_TEXT_MAX - dn, ", ");
                            if(i < g_externs[xi].pcount) {
                                dn += (size_t)snprintf(
                                    dst + dn, ZIR_GO_TEXT_MAX - dn, "%s",
                                    conv_arg(g_externs[xi].ptypes[i], arg));
                            } else {
                                dn += (size_t)snprintf(dst + dn,
                                                       ZIR_GO_TEXT_MAX - dn, "%s",
                                                       arg);
                            }
                        }
                    }
                    if(dn + 1 < dst_size)
                        dst[dn++] = ')';
                    continue;
                }
            }
            if(*q == '.') {
                const char *member = q + 1;
                const char *end = member;
                while(is_ident_char((unsigned char)*end))
                    end++;
                if(end > member && (size_t)(end - p) < ZIR_NAME_MAX) {
                    char qualified[ZIR_NAME_MAX], native[ZIR_GO_NAME_MAX];
                    memcpy(qualified, p, (size_t)(end - p));
                    qualified[end - p] = '\0';
                    if(NativeTypeAtUse(m, qualified, native,
                                       sizeof(native))) {
                        size_t length = strlen(native);
                        if(dn + length >= dst_size) break;
                        memcpy(dst + dn, native, length);
                        dn += length;
                        p = end;
                        continue;
                    }
                }
                if(end > member && *skip_ws(end) == '(' &&
                   (size_t)(end - p) < ZIR_NAME_MAX) {
                    int gfi = go_global_function_index(m, p,
                                                       (size_t)(end - p));
                    if(gfi >= 0) {
                        size_t length = strlen(g_functions[gfi].go);
                        if(dn + length + 2 < dst_size) {
                            memcpy(dst + dn, g_functions[gfi].go, length);
                            dn += length;
                            dst[dn++] = '(';
                        }
                        p = skip_ws(end) + 1;
                        continue;
                    }
                }
            }
            fni = module_fn_index(m, ident, il);
            if(fni >= 0 && *skip_ws(q) == '(') {
                char fname[ZIR_GO_NAME_MAX * 2];
                size_t fl;

                {
                    char local[ZIR_GO_NAME_MAX];

                    camel_ident(m->functions[fni].name, local, sizeof(local));
                    snprintf(fname, sizeof(fname), "%s_%s", g_guard, local);
                }
                fl = strlen(fname);
                if(dn + fl + 16 < dst_size) {
                    memcpy(dst + dn, fname, fl);
                    dn += fl;
                    dst[dn++] = '(';
                }
                p = skip_ws(q) + 1;
                continue;
            }
            if(*skip_ws(q) == '(') {
                int gfi = go_global_function_index(m, ident, il);

                if(gfi >= 0 && strcmp(g_functions[gfi].guard, g_guard) != 0) {
                    size_t fl = strlen(g_functions[gfi].go);

                    if(dn + fl + 16 < dst_size) {
                        memcpy(dst + dn, g_functions[gfi].go, fl);
                        dn += fl;
                        dst[dn++] = '(';
                    }
                    p = skip_ws(q) + 1;
                    continue;
                }
            }
            /* plain identifier: verbatim */
            if(dn + il + 1 < dst_size) {
                const char *local = go_local_name_for(ident);

                if(local != NULL) {
                    size_t ll = strlen(local);
                    if(dn + ll + 1 < dst_size) {
                        memcpy(dst + dn, local, ll);
                        dn += ll;
                    }
                } else if(module_global_owner(m, ident) != NULL) {
                    char mapped[ZIR_GO_NAME_MAX];
                    TargetGlobalName(module_global_owner(m, ident), ZIR_GO,
                                     ident, mapped, sizeof(mapped));
                    size_t length = strlen(mapped);
                    if(dn + length + 1 < dst_size) {
                        memcpy(dst + dn, mapped, length);
                        dn += length;
                    }
                } else if(module_constant_owner(m, ident) != NULL) {
                    char mapped[ZIR_GO_NAME_MAX];
                    TargetDefineName(module_constant_owner(m, ident), ZIR_GO,
                                     ident, mapped, sizeof(mapped));
                    size_t length = strlen(mapped);
                    if(dn + length + 1 < dst_size) {
                        memcpy(dst + dn, mapped, length);
                        dn += length;
                    }
                } else {
                    char native[ZIR_GO_NAME_MAX];
                    if(NativeTypeAtUse(m, ident, native,
                                       sizeof(native))) {
                        size_t length = strlen(native);
                        if(dn + length + 1 < dst_size) {
                            memcpy(dst + dn, native, length);
                            dn += length;
                        }
                    } else {
                        memcpy(dst + dn, ident, il);
                        dn += il;
                    }
                }
            }
            p = q;
            continue;
        }
        if(*p == '.' && (isalpha((unsigned char)p[1]) || p[1] == '_')) {
            const char *q = p + 1;
            char field[ZIR_GO_NAME_MAX], mapped[ZIR_GO_NAME_MAX];
            size_t fl = 0;

            while(is_ident_char((unsigned char)*q) && fl + 1 < sizeof(field))
                field[fl++] = *q++;
            field[fl] = '\0';
            go_field_ident(field, mapped, sizeof(mapped));
            if(dn + strlen(mapped) + 2 < dst_size) {
                dst[dn++] = '.';
                memcpy(dst + dn, mapped, strlen(mapped));
                dn += strlen(mapped);
            }
            p = q;
            continue;
        }
        dst[dn++] = *p == '~' ? '^' : *p;
        p++;
    }
    dst[dn] = '\0';
    (void)out;
}

/* -------------------------------------------------------- module lowering */

typedef struct GoBodySymbols {
    const ZirModule *module;
    const char *source_path;
} GoBodySymbols;

static void
resolve_body_symbol(void *context, const char *text, char *out, size_t size)
{
    const GoBodySymbols *symbols = context;
    const ZirModule *module = symbols->module;
    const ZirModule *owner = NULL;
    const ZirGlobal *global = NULL;
    if((SliceElementType(text, NULL, 0) || strchr(text, '*') != NULL ||
        FindType(module, text, NULL) != NULL) &&
       go_type(text, out, size))
        return;
    if(ResolveGlobalAt(module, text, symbols->source_path,
                       &owner, &global) == 1) {
        TargetGlobalName(owner, ZIR_GO, global->name, out, size);
        return;
    }
    tx_expr(module, text, out, size);
}

static void
lower_function(FILE *f, const ZirModule *m, const ZirFunction *fn,
               const char *guard)
{
    char fname[ZIR_GO_NAME_MAX * 2];
    char ret[ZIR_GO_NAME_MAX];
    int saved_local_count = zir_go_local_count;

    if(fn->export_symbol[0]) {
        Diagnostic(fn->span, "zir_go.export",
                   "quoted #program_export symbol requires the C or C++ target");
        exit(1);
    }

    zir_go_local_count = 0;
    camel_ident(fn->name, fname, sizeof(fname));
    /* signature: converted arguments */
    {
        char parts[32][ZIR_GO_TEXT_MAX];
        int n, i;
        int emitted = 0;

        fputs("func ", f);
        fprintf(f, "%s_%s(", guard, fname);
        if(fn->args[0] != '\0') {
            n = split_top(fn->args, parts, 32);
            for(i = 0; i < n; i++) {
                char *colon = strchr(parts[i], ':');
                char aname[ZIR_GO_NAME_MAX], atype[ZIR_GO_NAME_MAX];
                char gt[ZIR_GO_NAME_MAX];
                const char *name_start;
                size_t al;

                if(colon == NULL) {
                    Diagnostic(fn->span, "zir_go.parameter", "parameters require name: type: %s", parts[i]);
                    exit(1);
                }
                name_start = skip_ws(parts[i]);
                al = (size_t)(colon - name_start);
                while(al > 0 && isspace((unsigned char)name_start[al - 1]))
                    al--;
                memcpy(aname, name_start, al);
                aname[al] = '\0';
                snprintf(atype, sizeof(atype), "%s", colon + 1);
                go_register_local_name(fn, aname);
                {
                    char mapped[ZIR_GO_NAME_MAX];
                    TargetBindingName(fn, ZIR_GO, aname, mapped,
                                      sizeof(mapped));
                    snprintf(aname, sizeof(aname), "%s", mapped);
                }
                require_go_type(atype, gt, sizeof(gt), fn->span);
                fprintf(f, "%s%s %s", emitted ? ", " : "", aname, gt);
                emitted = 1;
            }
        }
        fprintf(f, ")");
        require_go_type(fn->return_type, ret, sizeof(ret), fn->span);
        if(ret[0] != '\0')
            fprintf(f, " %s", ret);
        fprintf(f, " {\n");
    }
    GoBodySymbols symbols = {m, fn->span.path};
    if(!EmitBody(f, m, fn, ZIR_GO, resolve_body_symbol, &symbols)) {
        Diagnostic(fn->span, "zir_go.body",
                   "function has no checked typed body: %s", fn->name);
        exit(1);
    }
    fprintf(f, "}\n\n");
    zir_go_local_count = saved_local_count;
}

static int
rewrite_global_scalar(const ZirModule *module, const char *source,
                      char *out, size_t size, void *context)
{
    (void)context;
    tx_expr(module, source, out, size);
    return out[0] != '\0';
}

static int
rewrite_global_type(const ZirModule *module, const char *source,
                    char *out, size_t size, void *context)
{
    (void)module;
    const ZirGlobal *global = context;
    require_go_type(source, out, size, global->span);
    return out[0] != '\0';
}

static void
rewrite_global_field(const ZirType *record, const char *source,
                     char *out, size_t size, void *context)
{
    (void)record;
    (void)context;
    go_field_ident(source, out, size);
}

int
go_lower(const ZirProgram *const *progs, int prog_count,
          const char *root, const char *out_dir, const char *pkg,
          int no_main)
{
    char path[1024];
    unsigned number_helpers = 0;
    go_build_global_functions(progs, prog_count);
    for(int pi = 0; pi < prog_count; pi++) {
        const ZirProgram *prog = progs[pi];

        for(int mi = 0; mi < prog->module_count; mi++) {
            const ZirModule *m = &prog->modules[mi];
            char stem[512], guard[ZIR_GO_NAME_MAX];
            FILE *f;

            NativeGoModuleIdentity(progs, prog_count, m, stem, sizeof(stem),
                                   guard, sizeof(guard));
            for(int fi = 0; fi < m->function_count; fi++)
                if(m->functions[fi].is_global_initializer &&
                   m->functions[fi].uses_host) {
                    Diagnostic(m->functions[fi].span, "zir_go.global",
                               "Go global startup cannot call a host before binding");
                    return 1;
                }
            snprintf(path, sizeof(path), "%s/%s.go", out_dir, stem);
            mkdir_parent(path);
            f = tmpfile();
            if(f == NULL) {
                Diagnostic(m->span, "zir_go.global",
                           "cannot create Go output: %s", path);
                return 1;
            }
            type_scope = m;
            go_set_module(m, guard);
            g_union_unsafe = 0;
            for(int ti = 0; ti < m->type_count; ti++)
                if(m->types[ti].is_union)
                    g_union_unsafe = 1;
            fprintf(f, "// Code generated by zi2go from %s. DO NOT EDIT.\n",
                    m->source_path);
            fprintf(f, "package %s\n\n", pkg);
            int pointer_index = 0;
            for(int fi = 0; fi < m->function_count && !pointer_index; fi++) {
                const ZirFunction *fn = &m->functions[fi];
                for(int ei = 0; ei < fn->expr_count; ei++) {
                    const ZirExpr *expr = &fn->exprs[ei];
                    if(expr->kind == ZIR_EXPR_SIZE_OF ||
                       (expr->kind == ZIR_EXPR_INDEX && expr->left >= 0 &&
                        fn->exprs[expr->left].type[0] == '*')) {
                        pointer_index = 1;
                        break;
                    }
                }
            }
            /* print uses fmt; the package's first file holds formatFloat
             * when any file prints a float. */
            int prints = 0;
            for(int fi = 0; fi < m->function_count && !prints; fi++)
                for(int ei = 0; ei < m->functions[fi].expr_count; ei++)
                    if(m->functions[fi].exprs[ei].kind == ZIR_EXPR_CALL &&
                       !strcmp(m->functions[fi].exprs[ei].name, "print")) {
                        prints = 1;
                        break;
                    }
            int float_helper = pi == 0 && mi == 0 && go_prints_floats(progs, prog_count);
            /* Imports are written as one gofmt block, standard packages first. */
            char imports[64][ZIR_PATH_MAX + ZIR_GO_NAME_MAX + 8];
            int import_count = 0;
            if(prints)
                snprintf(imports[import_count++], sizeof(imports[0]), "\"fmt\"");
            if(float_helper) {
                snprintf(imports[import_count++], sizeof(imports[0]), "\"math\"");
                snprintf(imports[import_count++], sizeof(imports[0]), "\"strconv\"");
            }
            if(pointer_index || g_union_unsafe)
                snprintf(imports[import_count++], sizeof(imports[0]), "\"unsafe\"");
            for(int i = 0; i < g_extern_count && import_count < 64; i++) {
                int duplicate = 0;

                if(!g_externs[i].direct_go)
                    continue;
                for(int j = 0; j < i; j++) {
                    if(g_externs[j].direct_go &&
                       strcmp(g_externs[j].go_import_path,
                              g_externs[i].go_import_path) == 0) {
                        duplicate = 1;
                        break;
                    }
                }
                if(!duplicate)
                    snprintf(imports[import_count++], sizeof(imports[0]), "%s \"%s\"",
                             g_externs[i].go_import_alias, g_externs[i].go_import_path);
            }
            if(import_count == 1)
                fprintf(f, "import %s\n\n", imports[0]);
            else if(import_count > 1) {
                fputs("import (\n", f);
                for(int i = 0; i < import_count; i++)
                    fprintf(f, "\t%s\n", imports[i]);
                fputs(")\n\n", f);
            }
            if(pi == 0 && mi == 0 && go_uses_caller_location(progs, prog_count))
                fputs("type Source_Code_Location struct {\n"
                      "\tFullyPathedFilename string\n"
                      "\tLineNumber int64\n}\n\n", f);

            for(int i = 0; i < m->import_count; i++) {
                const ZirImport *imp = &m->imports[i];

                if(imp->kind == ZIR_IMPORT_OPEN)
                    fprintf(f, "// #import %s\n", imp->target);
                /* ZIR_IMPORT_EXTERN lowers either to direct Go imports above
                 * or to the Host interface below. */
            }
            if(float_helper)
                EmitGoPrintSupport(f);
            /* '#foreign host_api' bridge: one interface, one package var, one
             * setter. Generated frames call hostVar.Method(...) directly. */
            {
                int first_host = -1;
                int host_count = 0;

                for(int i = 0; i < g_extern_count; i++) {
                    if(g_externs[i].direct_go || g_externs[i].direct_ziran)
                        continue;
                    if(first_host < 0)
                        first_host = i;
                    host_count++;
                }
            if(host_count > 0) {
                fprintf(f, "// %sHost bridges '#foreign host_api' declarations to the",
                        guard);
                fprintf(f, " embedding Go program.\ntype %sHost interface {\n",
                        guard);
                for(int i = 0; i < g_extern_count; i++) {
                    const ZirGoExtern *ex = &g_externs[i];
                    char gt[ZIR_GO_NAME_MAX];

                    if(ex->direct_go || ex->direct_ziran)
                        continue;
                    fprintf(f, "\t%s(", ex->go);
                    for(int a = 0; a < ex->pcount; a++) {
                        char aname[ZIR_GO_NAME_MAX];

                        camel_ident(ex->pnames[a], aname, sizeof(aname));
                        require_go_type(ex->ptypes[a], gt, sizeof(gt), m->span);
                        fprintf(f, "%s%s %s", a > 0 ? ", " : "", aname, gt);
                    }
                    fprintf(f, ")");
                    require_go_type(ex->ret, gt, sizeof(gt), m->span);
                    if(gt[0] != '\0')
                        fprintf(f, " %s", gt);
                    fprintf(f, "\n");
                }
                fprintf(f, "}\n\n");
                fprintf(f, "var %s %sHost\n\n",
                        g_externs[first_host].host_var, guard);
                fprintf(f, "// Set%sHost wires the '#foreign host_api' bridge before",
                        guard);
                fprintf(f, " the first frame runs.\nfunc Set%sHost(host %sHost)",
                        guard, guard);
                fprintf(f, " {\n\t%s = host\n}\n\n",
                        g_externs[first_host].host_var);
            }
            }
            /* types */
            int enum_idx = 0;

            for(int i = 0; i < m->type_count; i++) {
                const ZirType *t = &m->types[i];
                char native[ZIR_GO_NAME_MAX];
                NativeTypeName(m, t, native, sizeof(native));
                if(t->is_union) {
                    size_t offset = 0;
                    ZirTypeField field;
                    size_t size = 0, alignment = 1;
                    while(TypeNextField(t, &offset, &field) == 1) {
                        const char *backing = field.type;
                        const ZirType *enumeration = FindType(m, field.type,
                                                              NULL);
                        if(enumeration != NULL && enumeration->is_enum)
                            backing = enumeration->enum_backing;
                        size_t width = scalar_byte_width(backing);
                        if(width == 0) {
                            Diagnostic(t->span, "zir_go.union",
                                       "Go unions support scalar fields only: %s",
                                       field.name);
                            exit(1);
                        }
                        if(width > size)
                            size = width;
                        if(width > alignment)
                            alignment = width;
                    }
                    if(size == 0)
                        size = 1;
                    fprintf(f, "type %s struct {\n"
                           "\t_ [0]uint%zu\n"
                           "\tdata [%zu]byte\n"
                           "}\n\n", native, alignment * 8, size);
                    g_union_unsafe = 1;
                    continue;
                }
                if(t->is_extern || t->is_record_template)
                    continue;

                if(t->is_procedure_type) {
                    if(t->is_c_call) {
                        Diagnostic(t->span, "zir_go.type",
                                   "#c_call procedure types require the native C or C++ target");
                        exit(1);
                    }
                    EmitSlotType(f, t, ZIR_GO, resolve_slot_type, NULL);
                    continue;
                }
                if(t->is_enum) {
                    /* enums: typed constants with C counter semantics
                         * (g_enums was built in the same order as m->types) */
                        ZirGoEnum *e = enum_idx < g_enum_count
                                         ? &g_enums[enum_idx++] : NULL;

                        if(e == NULL)
                            continue;
                        const char *backing = enum_storage_type(t->enum_backing);
                        if(backing == NULL) {
                            Diagnostic(t->span, "zir_go.enum",
                                       "invalid enum backing type");
                            exit(1);
                        }
                        fprintf(f, "type %s %s\n\n", e->go_type, backing);
                        fprintf(f, "const (\n");
                        for(int mI = 0; mI < e->count; mI++) {
                            ZirGoEnumMember *mem = &e->members[mI];
                            char val[ZIR_GO_TEXT_MAX];

                            if(mem->val[0] != '\0') {
                                tx_expr(m, mem->val, val, sizeof(val));
                            } else if(mI == 0) {
                                snprintf(val, sizeof(val), "0");
                            } else {
                                /* Refer to the previous constant: negative
                                 * values and expressions need no host-side
                                 * evaluator or sentinel counter. */
                                snprintf(val, sizeof(val), "%s + 1",
                                         e->members[mI - 1].go);
                            }
                            fprintf(f, "\t%s = %s\n", mem->go, val);
                        }
                        fprintf(f, ")\n\n");
                    } else {
                        fprintf(f, "type %s struct {\n", native);
                    {
                        size_t offset = 0;
                        ZirTypeField field;
                        int status;

                        while((status = TypeNextField(t, &offset, &field)) == 1) {
                            char fname[ZIR_GO_NAME_MAX], gt[ZIR_GO_NAME_MAX];
                            go_field_ident(field.name, fname, sizeof(fname));
                            if(!strcmp(field.name, "data") &&
                               VecElementType(m, t->name, NULL, 0)) {
                                char item[ZIR_NAME_MAX];
                                VecElementType(m, t->name, item, sizeof(item));
                                char mapped[ZIR_GO_NAME_MAX];
                                require_go_type(item, mapped, sizeof(mapped),
                                                t->span);
                                snprintf(gt, sizeof(gt), "[]%s", mapped);
                            } else
                                require_go_type(field.type, gt, sizeof(gt),
                                                t->span);
                            fprintf(f, "\t%s %s\n", fname, gt);
                        }
                        if(status < 0) {
                            Diagnostic(t->span, "zir_go.type",
                                       "malformed field in %s", t->name);
                            exit(1);
                        }
                    }
                    fprintf(f, "}\n\n");
                }
            }
            /* defines -> consts; array type aliases become Go types */
            for(int i = 0; i < m->define_count; i++) {
                char cname[ZIR_GO_NAME_MAX];
                char cval[ZIR_GO_TEXT_MAX];
                const char *value = skip_ws(m->defines[i].value);

                TargetDefineName(m, ZIR_GO, m->defines[i].name,
                                 cname, sizeof(cname));
                if(value[0] == '[') {
                    char aliased[ZIR_GO_NAME_MAX];
                    require_go_type(value, aliased, sizeof(aliased),
                                    m->defines[i].span);
                    fprintf(f, "type %s %s\n", cname, aliased);
                    continue;
                }
                ZirFunction probe = {0};
                int root = ParseExpr(&probe, m, value,
                                     m->defines[i].span);
                int aggregate = root >= 0 &&
                    probe.exprs[root].kind == ZIR_EXPR_COMPOUND;
                free(probe.exprs);
                if(aggregate) {
                    /* The checker embeds aggregate values in checked uses
                     * and lowers direct global initializers to literals. */
                    continue;
                }
                tx_expr(m, m->defines[i].value, cval, sizeof(cval));
                fprintf(f, "const %s = %s\n", cname, cval);
            }
            /* globals */
            for(int i = 0; i < m->global_count; i++) {
                const ZirGlobal *g = &m->globals[i];
                char gname[ZIR_GO_NAME_MAX], gt[ZIR_GO_NAME_MAX], ginit[ZIR_GO_TEXT_MAX];

                TargetGlobalName(m, ZIR_GO, g->name, gname,
                                 sizeof(gname));
                require_go_type(g->type, gt, sizeof(gt), g->span);
                if(!ScalarLiteral(g->type, g->init, ZIR_GO, g->span,
                                  ginit, sizeof(ginit))) {
                    int compound = EmitGlobalInitializer(m, g, ZIR_GO,
                        rewrite_global_scalar, rewrite_global_type,
                        rewrite_global_field, (void *)g, ginit,
                        sizeof(ginit));
                    if(compound < 0) {
                        Diagnostic(g->span, "zir_go.global",
                                   "cannot lower compound global initializer: %s",
                                   g->name);
                        exit(1);
                    }
                    if(compound == 0)
                        tx_expr(m, g->init, ginit, sizeof(ginit));
                }
                if(ginit[0] != '\0') {
                    if(ginit[0] == '{')
                        fprintf(f, "var %s = %s%s\n", gname, gt, ginit);
                    else
                        fprintf(f, "var %s %s = %s\n", gname, gt, ginit);
                } else
                    fprintf(f, "var %s %s\n", gname, gt);
            }
            /* functions ('#foreign' prototypes have no body: they lower to
             * Host interface methods, not Go functions) */
            for(int i = 0; i < m->function_count; i++) {
                if(m->functions[i].is_extern || m->functions[i].is_template)
                    continue;
                lower_function(f, m, &m->functions[i], guard);
            }
            int startup_count = 0;
            for(int i = 0; i < m->function_count; i++)
                startup_count += m->functions[i].is_global_initializer;
            /* A module with nothing to set up needs no init; importers call
             * a dependency's init only when it needs startup. */
            if(ModuleNeedsStartup(m)) {
            fprintf(f, "var %s_ziranInitState uint8\n"
                       "func %s_ziranInit() {\n"
                       "\tif %s_ziranInitState == 2 { return }\n"
                       "\tif %s_ziranInitState == 1 { panic(\"cyclic module startup\") }\n"
                       "\t%s_ziranInitState = 1\n",
                    guard, guard, guard, guard, guard);
            for(int i = 0; i < m->import_count; i++) {
                const ZirModule *dependency = m->imports[i].resolved_module;
                if(ModuleNeedsStartup(dependency)) {
                    char stem[ZIR_PATH_MAX], dep_guard[ZIR_GO_NAME_MAX];
                    NativeGoModuleIdentity(progs, prog_count,
                        dependency, stem, sizeof(stem), dep_guard,
                        sizeof(dep_guard));
                    fprintf(f, "\t%s_ziranInit()\n", dep_guard);
                }
            }
            for(int i = 0; i < m->function_count; i++)
                if(m->functions[i].is_global_initializer) {
                    char name[ZIR_GO_NAME_MAX];
                    camel_ident(m->functions[i].name, name, sizeof(name));
                    fprintf(f, "\t%s_%s()\n", guard, name);
                }
            fprintf(f, "\t%s_ziranInitState = 2\n}\n\n", guard);
            if(startup_count) {
                fprintf(f, "func init() { %s_ziranInit() }\n\n", guard);
            }
            }

            /* Numeric helpers follow the code, once per package. */
            {
                long length = ftell(f);
                char *text = malloc((size_t)length + 1);
                if(text == NULL) {
                    Diagnostic(m->span, "zir_go.global", "out of memory writing %s", path);
                    fclose(f);
                    return 1;
                }
                rewind(f);
                text[fread(text, 1, (size_t)length, f)] = '\0';
                fseek(f, 0, SEEK_END);
                EmitGoNumberHelpers(f, text, &number_helpers);
                free(text);
            }
            /* End with one newline, as gofmt does, not a blank line. */
            long remaining = ftell(f);
            while(remaining > 1) {
                char tail[2];
                if(fseek(f, remaining - 2, SEEK_SET) != 0 ||
                   fread(tail, 1, 2, f) != 2 || tail[0] != '\n' || tail[1] != '\n')
                    break;
                remaining--;
            }
            rewind(f);
            GeneratedOutputRecord(path);
            FILE *output = fopen(path, "wb");
            if(output == NULL) {
                Diagnostic(m->span, "zir_go.global",
                           "cannot create Go output: %s", path);
                fclose(f);
                return 1;
            }
            char buffer[8192];
            size_t bytes;
            int failed = 0;
            while(remaining > 0 &&
                  (bytes = fread(buffer, 1, remaining < (long)sizeof(buffer) ?
                                 (size_t)remaining : sizeof(buffer), f)) != 0) {
                remaining -= (long)bytes;
                if(fwrite(buffer, 1, bytes, output) != bytes) {
                    failed = 1;
                    break;
                }
            }
            failed |= ferror(f) != 0;
            failed |= fclose(output) != 0;
            fclose(f);
            if(failed) {
                Diagnostic(m->span, "zir_go.global",
                           "cannot finish Go output: %s", path);
                return 1;
            }
        }
    }
    return 0;
}
