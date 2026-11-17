/*
 * std/simd helpers for Go. Generated code calls the same helper names as C
 * (zi_add_s16x8, zi_narrow_s16x8_u8x16, zi_load_ptr_s32x4); this file writes
 * the ones a package calls into ziran_simd.go. Value helpers take their typed
 * bodies from zir_simd_go_bodies.h, which the build generates with scripts/simd;
 * GoSimdWriteCheck writes each beside a wrapper around zi_simd_ref, which
 * tests/simd_go.sh binds to zi_ref in include/ziran_simd.h through cgo, so the
 * check holds every typed body to the C reference.
 */
#include "zir_emit_internal.h"
#include "zir_simd.h"
#define ZIRAN_SIMD_REF_ONLY 1
#include "ziran_simd.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GoSimdText {
    const char *name, *text;
} GoSimdText;
#include "zir_simd_go_bodies.h"

#define COUNT(array) (sizeof(array) / sizeof(*(array)))

static const char go_checks[] =
    "\nfunc zi_simd_check(length, index int64, count int) {\n"
    "	if index < 0 || index > length-int64(count) {\n"
    "		panic(\"ziran: SIMD access of \" + strconv.Itoa(count) + \" lanes at \" +\n"
    "			strconv.FormatInt(index, 10) + \" out of bounds (length \" +\n"
    "			strconv.FormatInt(length, 10) + \")\")\n"
    "	}\n"
    "}\n"
    "\n"
    "func zi_simd_gather_check(index, length int64) {\n"
    "	if index < 0 || index >= length {\n"
    "		panic(\"ziran: gather index \" + strconv.FormatInt(index, 10) +\n"
    "			\" out of bounds (length \" + strconv.FormatInt(length, 10) + \")\")\n"
    "	}\n"
    "}\n";

static char **go_helpers;
static int go_helper_count, go_helper_capacity;

void
GoSimdBegin(void)
{
    for(int i = 0; i < go_helper_count; i++) free(go_helpers[i]);
    free(go_helpers);
    go_helpers = NULL;
    go_helper_count = go_helper_capacity = 0;
}

static void
remember(const char *start, size_t length)
{
    for(int i = 0; i < go_helper_count; i++)
        if(strlen(go_helpers[i]) == length && !memcmp(go_helpers[i], start, length)) return;
    if(go_helper_count == go_helper_capacity) {
        go_helper_capacity = go_helper_capacity ? go_helper_capacity * 2 : 16;
        char **grown = realloc(go_helpers, (size_t)go_helper_capacity * sizeof(*grown));
        if(grown == NULL) { fputs("zi2go: out of memory\n", stderr); exit(1); }
        go_helpers = grown;
    }
    char *name = AllocateOrExit(length + 1);
    memcpy(name, start, length);
    name[length] = '\0';
    go_helpers[go_helper_count++] = name;
}

/* A helper name: the operation, then one or two vector types, with "_ptr"
 * after the operation for unchecked pointer access. */
typedef struct GoHelper {
    const SimdOp *op;
    ZirVector first, second;
    int types, pointer, string, checked;
} GoHelper;

static int
vector_component(const char *start, size_t length, ZirVector *vector)
{
    char name[8];
    if(length == 0 || length >= sizeof(name)) return 0;
    memcpy(name, start, length);
    name[length] = '\0';
    return VectorType(name, vector);
}

static int
parse_helper(const char *name, GoHelper *helper)
{
    memset(helper, 0, sizeof(*helper));
    if(strncmp(name, "zi_", 3)) return 0;
    const char *end = name + strlen(name);
    for(int types = 0; types < 2; types++) {
        const char *underscore = end;
        while(underscore > name + 3 && underscore[-1] != '_') underscore--;
        if(underscore <= name + 3) break;
        ZirVector vector;
        if(!vector_component(underscore, (size_t)(end - underscore), &vector)) break;
        /* The last component is the second type, the one before it the first. */
        if(types == 0) helper->second = vector;
        else helper->first = vector;
        helper->types = types + 1;
        end = underscore - 1;
    }
    if(helper->types == 0) return 0;
    if(helper->types == 1) helper->first = helper->second;
    char snake[64];
    size_t length = (size_t)(end - (name + 3));
    if(length == 0 || length >= sizeof(snake) - 8) return 0;
    memcpy(snake, name + 3, length);
    snake[length] = '\0';
    if(length > 8 && !strcmp(snake + length - 8, "_checked")) {
        helper->checked = 1;
        length -= 8;
        snake[length] = '\0';
    }
    if(length > 4 && !strcmp(snake + length - 4, "_ptr")) {
        helper->pointer = 1;
        snake[length - 4] = '\0';
    }
    if(length > 7 && !strcmp(snake + length - 7, "_string")) {
        helper->string = 1;
        snake[length - 7] = '\0';
    }
    char checked[80];
    format(checked, sizeof(checked), "zi_simd_%s", snake);
    helper->op = SimdOpChecked(checked);
    if(helper->op == NULL) return 0;
    if(helper->string && (helper->op->shape != SIMD_LOAD || strcmp(helper->first.lane, "u8"))) return 0;
    int two = helper->op->shape == SIMD_NARROW || helper->op->shape == SIMD_REINTERPRET ||
              helper->op->shape == SIMD_CONVERT || helper->op->shape == SIMD_GATHER;
    int memory = helper->op->shape == SIMD_LOAD || helper->op->shape == SIMD_STORE ||
                 helper->op->shape == SIMD_GATHER;
    if(two != (helper->types == 2) || (helper->pointer && !memory) ||
       (helper->checked && (!memory || helper->pointer))) return 0;
    if(!(helper->op->lanes & (1u << SimdLaneKind(&helper->first))) &&
       helper->op->shape != SIMD_REINTERPRET && helper->op->shape != SIMD_CONVERT &&
       helper->op->shape != SIMD_NARROW && helper->op->shape != SIMD_GATHER)
        return 0;
    return 1;
}

void
GoSimdCollect(const char *text)
{
    for(const char *p = text; p != NULL && *p;) {
        if(!(p[0] == 'z' && p[1] == 'i' && p[2] == '_') ||
           (p > text && (isalnum((unsigned char)p[-1]) || p[-1] == '_'))) {
            p++;
            continue;
        }
        const char *start = p;
        while(isalnum((unsigned char)*p) || *p == '_') p++;
        size_t length = (size_t)(p - start);
        char name[96];
        GoHelper helper;
        if(*p != '(' || length >= sizeof(name)) continue;
        memcpy(name, start, length);
        name[length] = '\0';
        if(parse_helper(name, &helper)) remember(start, length);
    }
}

/* Short lane names in helper names: f32 for float32. */
static const char *
lane_name(const ZirVector *vector)
{
    static const char *const names[] = {
        "u8", "s8", "u16", "s16", "u32", "s32", "u64", "s64", "f32", "f64"
    };
    return names[SimdLaneKind(vector)];
}

static const char *
go_type(const char *type)
{
    return TargetType(type, ZIR_GO);
}

static const char *const go_vectors[] = {
    "u8x16", "s8x16", "u16x8", "s16x8", "u32x4", "s32x4", "u64x2", "s64x2",
    "f32x4", "f64x2", "u8x32", "s8x32", "u16x16", "s16x16", "u32x8", "s32x8",
    "u64x4", "s64x4", "f32x8", "f64x4"
};

/* Lane conversions to and from bit patterns, and vectors to and from the
 * reference's lane arrays, for every type. */
static void
write_conversions(FILE *out)
{
    static const char *const lanes[][4] = {
        {"u8", "uint8", "uint64(v)", "uint8(b)"},
        {"s8", "int8", "uint64(uint8(v))", "int8(b)"},
        {"u16", "uint16", "uint64(v)", "uint16(b)"},
        {"s16", "int16", "uint64(uint16(v))", "int16(b)"},
        {"u32", "uint32", "uint64(v)", "uint32(b)"},
        {"s32", "int32", "uint64(uint32(v))", "int32(b)"},
        {"u64", "uint64", "v", "b"},
        {"s64", "int64", "uint64(v)", "int64(b)"},
        {"f32", "float32", "uint64(math.Float32bits(v))", "math.Float32frombits(uint32(b))"},
        {"f64", "float64", "math.Float64bits(v)", "math.Float64frombits(b)"},
    };
    for(size_t i = 0; i < COUNT(lanes); i++)
        fprintf(out, "\nfunc zi_simd_bits_%s(v %s) uint64 {\n\treturn %s\n}\n"
                     "\nfunc zi_simd_lane_%s(b uint64) %s {\n\treturn %s\n}\n",
                lanes[i][0], lanes[i][1], lanes[i][2], lanes[i][0], lanes[i][1], lanes[i][3]);
    for(size_t i = 0; i < COUNT(go_vectors); i++) {
        const char *name = go_vectors[i];
        ZirVector vector;
        VectorType(name, &vector);
        const char *lane = lane_name(&vector);
        fprintf(out, "\nfunc zi_simd_in_%s(x *zi_lanes, v %s) {\n"
                     "\tfor i := range v {\n\t\tx[i] = zi_simd_bits_%s(v[i])\n\t}\n}\n"
                     "\nfunc zi_simd_out_%s(x *zi_lanes) (v %s) {\n"
                     "\tfor i := range v {\n\t\tv[i] = zi_simd_lane_%s(x[i])\n\t}\n\treturn v\n}\n",
                name, go_type(name), lane, name, go_type(name), lane);
    }
}

/* The result type of a value operation, from the same signature the checker
 * applies; constant operands are given as zero. */
static const char *
helper_result(const GoHelper *helper, const char *const *types, int count)
{
    SimdOperand operands[40];
    const char *message = NULL;
    for(int i = 0; i < count; i++)
        operands[i] = (SimdOperand){.type = types[i], .is_constant = 1, .value = 0};
    return SimdSignature(helper->op, helper->second.name, operands, count, &message);
}

static void
write_memory(FILE *out, const char *name, const GoHelper *helper)
{
    const ZirVector *v = &helper->first;
    const char *vt = go_type(v->name), *lt = go_type(v->lane);
    int lanes = helper->op->id == ZI_OP_LOAD_LOW || helper->op->id == ZI_OP_STORE_LOW ?
        64 / v->lane_bits : v->lanes;
    char view[160];
    if(helper->pointer)
        format(view, sizeof(view), "unsafe.Slice((*%s)(unsafe.Add(unsafe.Pointer(p), i*%d)), %d)",
               lt, v->lane_bits / 8, lanes);
    else
        format(view, sizeof(view), "m[i:i+%d]", lanes);
    const char *memory = helper->pointer ? "p *" : "m []";
    if(helper->op->shape == SIMD_STORE) {
        fprintf(out, "\nfunc %s(%s%s, i int64, v %s) {\n", name, memory, lt, vt);
        if(helper->checked) fprintf(out, "\tzi_simd_check(int64(len(m)), i, %d)\n", lanes);
        fprintf(out, "\tcopy(%s, v[:%d])\n}\n", view, lanes);
        return;
    }
    fprintf(out, "\nfunc %s(%s%s, i int64) (v %s) {\n", name,
            helper->string ? "m " : memory, helper->string ? "string" : lt, vt);
    if(helper->checked) fprintf(out, "\tzi_simd_check(int64(len(m)), i, %d)\n", lanes);
    fprintf(out, "\tcopy(v[:%d], %s)\n\treturn v\n}\n", lanes, view);
}

static void
write_gather(FILE *out, const char *name, const GoHelper *helper)
{
    const ZirVector *index = &helper->first, *result = &helper->second;
    const char *lt = go_type(result->lane);
    const char *read = index->is_signed ? "int64(idx[l])" : "int64(uint32(idx[l]))";
    if(helper->pointer) {
        fprintf(out, "\nfunc %s(p *%s, idx %s) (v %s) {\n"
                     "\tfor l := range v {\n"
                     "\t\tv[l] = *(*%s)(unsafe.Add(unsafe.Pointer(p), %s*%d))\n"
                     "\t}\n\treturn v\n}\n",
                name, lt, go_type(index->name), go_type(result->name), lt, read,
                result->lane_bits / 8);
        return;
    }
    /* A checked gather checks every index before any lane is read, as in C. */
    fprintf(out, "\nfunc %s(t []%s, idx %s) (v %s) {\n", name, lt, go_type(index->name), go_type(result->name));
    if(helper->checked)
        fprintf(out, "\tfor l := range idx {\n\t\tzi_simd_gather_check(%s, int64(len(t)))\n\t}\n", read);
    fprintf(out, "\tfor l := range v {\n\t\tv[l] = t[%s]\n\t}\n\treturn v\n}\n", read);
}

/* A value helper's typed body, or NULL. */
static const GoSimdText *
typed_body(const char *name)
{
    size_t low = 0, high = COUNT(go_simd_bodies);
    while(low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(name, go_simd_bodies[middle].name);
        if(order == 0) return &go_simd_bodies[middle];
        if(order < 0) high = middle;
        else low = middle + 1;
    }
    return NULL;
}

/* A value helper's Go parameters, result type and the statements that pass
 * its operands to the reference: vector operands go to the x, y and z lanes
 * in order, constants to k, a scalar lane operand to y[0] and Make's lanes
 * to x, as zi_ref reads them. */
typedef struct GoValue {
    char params[2048], body[4096];
    const char *result;
} GoValue;

/* Fills value; 0 where the checker would not accept the helper. */
static int
value_signature(const GoHelper *helper, GoValue *value)
{
    const SimdOp *op = helper->op;
    const ZirVector *v = &helper->first;
    const char *types[40];
    char *params = value->params, *body = value->body;
    int count = 0, vectors = 0;
    size_t used;
    params[0] = body[0] = '\0';
#define PARAM(...) (used = strlen(params), format(params + used, sizeof(value->params) - used, __VA_ARGS__))
#define BODY(...) (used = strlen(body), format(body + used, sizeof(value->body) - used, __VA_ARGS__))
#define VECTOR(param, type) do { \
        PARAM("%s%s %s", count ? ", " : "", param, go_type(type)); \
        BODY("\tzi_simd_in_%s(&%c, %s)\n", type, "xyz"[vectors++], param); \
        types[count++] = type; \
    } while(0)
    char lane[8];
    switch(op->shape) {
    case SIMD_UNARY: case SIMD_WIDEN_HALF: case SIMD_WIDEN: case SIMD_ADD_PAIRS:
    case SIMD_REDUCE: case SIMD_ALL_ZERO: case SIMD_HALF:
    case SIMD_REINTERPRET: case SIMD_CONVERT:
        VECTOR("a", v->name);
        break;
    case SIMD_BINARY: case SIMD_COMPARE: case SIMD_SHIFT_LANES: case SIMD_MUL_WIDE:
    case SIMD_MADD: case SIMD_SAD: case SIMD_COMBINE: case SIMD_NARROW:
        VECTOR("a", v->name);
        VECTOR("b", v->name);
        break;
    case SIMD_MADD_SAT:
        VECTOR("a", v->name);
        VECTOR("b", VectorTypeName("s8", v->lanes));
        break;
    case SIMD_BYTE_SHUFFLE:
        VECTOR("a", v->name);
        VECTOR("b", VectorTypeName("u8", v->lanes));
        break;
    case SIMD_SELECT:
        format(lane, sizeof(lane), "u%d", v->lane_bits);
        VECTOR("m", VectorTypeName(lane, v->lanes));
        VECTOR("a", v->name);
        VECTOR("b", v->name);
        break;
    case SIMD_SHIFT:
        VECTOR("a", v->name);
        PARAM(", n uint64");
        BODY("\tk[0] = int64(n)\n");
        types[count++] = "u64";
        break;
    case SIMD_BYTE_SHIFT:
        VECTOR("a", v->name);
        PARAM(", n int");
        BODY("\tk[0] = int64(n)\n");
        types[count++] = "integer";
        break;
    case SIMD_ALIGN:
        VECTOR("a", v->name);
        VECTOR("b", v->name);
        PARAM(", n int");
        BODY("\tk[0] = int64(n)\n");
        types[count++] = "integer";
        break;
    case SIMD_LANE:
        VECTOR("a", v->name);
        PARAM(", i int");
        BODY("\tk[0] = int64(i)\n");
        types[count++] = "integer";
        break;
    case SIMD_WITH_LANE:
        VECTOR("a", v->name);
        PARAM(", i int, l %s", go_type(v->lane));
        BODY("\tk[0] = int64(i)\n\ty[0] = zi_simd_bits_%s(l)\n", lane_name(v));
        types[count++] = "integer";
        types[count++] = v->lane;
        break;
    case SIMD_SHUFFLE: {
        VECTOR("a", v->name);
        for(int i = 0; i < SimdBlockLanes(v); i++) {
            PARAM(", k%d int", i);
            BODY("\tk[%d] = int64(k%d)\n", i, i);
            types[count++] = "integer";
        }
        break;
    }
    case SIMD_MAKE:
        for(int i = 0; i < v->lanes; i++) {
            PARAM("%sl%d %s", i ? ", " : "", i, go_type(v->lane));
            BODY("\tx[%d] = zi_simd_bits_%s(l%d)\n", i, lane_name(v), i);
            types[count++] = v->lane;
        }
        break;
    case SIMD_SPLAT:
        PARAM("l %s", go_type(v->lane));
        BODY("\ty[0] = zi_simd_bits_%s(l)\n", lane_name(v));
        types[count++] = v->lane;
        break;
    case SIMD_ZERO:
        break;
    default:
        return 0;
    }
#undef VECTOR
#undef PARAM
#undef BODY
    value->result = helper_result(helper, types, count);
    return value->result != NULL;
}

/* One value operation as name followed by suffix: the typed body, or with
 * typed NULL a wrapper around the reference. */
static int
write_value(FILE *out, const char *name, const char *suffix, const GoHelper *helper,
            const GoSimdText *typed)
{
    const SimdOp *op = helper->op;
    const ZirVector *v = &helper->first;
    GoValue value;
    if(!value_signature(helper, &value)) return 0;
    const char *result = value.result, *params = value.params, *body = value.body;
    ZirVector r;
    int vector_result = VectorType(result, &r);
    int r_kind = vector_result ? SimdLaneKind(&r) :
        !strcmp(result, v->lane) ? SimdLaneKind(v) : ZI_K_S64;
    int r_lanes = vector_result ? r.lanes : 1;
    const char *a_type = op->shape == SIMD_MAKE || op->shape == SIMD_SPLAT ||
                         op->shape == SIMD_ZERO ? result : v->name;
    ZirVector a;
    VectorType(a_type, &a);
    if(typed != NULL) {
        fprintf(out, "\nfunc %s%s(%s) %s {\n%s}\n", name, suffix, params, go_type(result), typed->text);
        return 1;
    }
    fprintf(out, "\nfunc %s%s(%s) %s {\n\tvar x, y, z, o zi_lanes\n\tvar k [32]int64\n%s",
            name, suffix, params, go_type(result), body);
    fprintf(out, "\tzi_simd_ref(%d, %d, %d, %d, %d, &x, &y, &z, &k, &o)\n",
            op->id, r_kind, r_lanes, SimdLaneKind(&a), a.lanes);
    if(vector_result) fprintf(out, "\treturn zi_simd_out_%s(&o)\n}\n", result);
    else if(!strcmp(result, "bool")) fputs("\treturn o[0] != 0\n}\n", out);
    else if(!strcmp(result, v->lane)) fprintf(out, "\treturn zi_simd_lane_%s(o[0])\n}\n", lane_name(v));
    else fputs("\treturn int64(o[0])\n}\n", out);
    return 1;
}

static int
compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Whether text calls the function name. */
static int
calls(const char *text, const char *name)
{
    size_t length = strlen(name);
    for(const char *p = strstr(text, name); p != NULL; p = strstr(p + 1, name))
        if(p[length] == '(' && (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')))
            return 1;
    return 0;
}

/* Writes the remembered helpers into out_dir/ziran_simd.go. With check set,
 * each value helper also comes as <name>_ref around zi_simd_ref, which the
 * check supplies, and zi_simd_helpers lists the pairs. */
static int
write_helpers(const char *out_dir, const char *pkg, int check)
{
    qsort(go_helpers, (size_t)go_helper_count, sizeof(*go_helpers), compare_names);
    int pointer = 0, checks = 0, math = check;
    unsigned char lane_used[COUNT(go_simd_lane_functions)] = {0};
    for(int i = 0; i < go_helper_count; i++) {
        GoHelper helper;
        if(!parse_helper(go_helpers[i], &helper)) continue;
        SimdShape shape = helper.op->shape;
        if(shape == SIMD_LOAD || shape == SIMD_STORE || shape == SIMD_GATHER) {
            pointer |= helper.pointer;
            checks |= helper.checked;
            continue;
        }
        const GoSimdText *typed = typed_body(go_helpers[i]);
        if(typed == NULL) {
            if(check) continue;
            Diagnostic((ZirSourceSpan){0}, "compiler.internal", "no typed Go body for %s", go_helpers[i]);
            return 1;
        }
        math |= strstr(typed->text, "math.") != NULL;
        for(size_t f = 0; f < COUNT(go_simd_lane_functions); f++)
            if(!lane_used[f] && calls(typed->text, go_simd_lane_functions[f].name)) {
                lane_used[f] = 1;
                math |= strstr(go_simd_lane_functions[f].text, "math.") != NULL;
            }
    }
    char path[ZIR_PATH_MAX], temp[ZIR_PATH_MAX];
    format(path, sizeof(path), "%s/ziran_simd.go", out_dir);
    FILE *out = GeneratedOutputOpen(path, temp, sizeof(temp));
    if(out == NULL) {
        Diagnostic((ZirSourceSpan){0}, "zir_go.global", "cannot create Go output: %s", path);
        return 1;
    }
    fprintf(out, "// Code generated by zi2go from std/simd. DO NOT EDIT.\npackage %s\n", pkg);
    if(math || checks || pointer)
        fprintf(out, "\nimport (\n%s%s%s)\n", math ? "\t\"math\"\n" : "",
                checks ? "\t\"strconv\"\n" : "", pointer ? "\t\"unsafe\"\n" : "");
    int failed = 0;
    for(int i = 0; i < go_helper_count && !failed; i++) {
        GoHelper helper;
        parse_helper(go_helpers[i], &helper);
        const char *name = go_helpers[i];
        if(helper.op->shape == SIMD_LOAD || helper.op->shape == SIMD_STORE)
            write_memory(out, name, &helper);
        else if(helper.op->shape == SIMD_GATHER)
            write_gather(out, name, &helper);
        else if(!write_value(out, name, "", &helper, typed_body(name)) ||
                (check && !write_value(out, name, "_ref", &helper, NULL))) {
            Diagnostic((ZirSourceSpan){0}, "compiler.internal", "no Go helper for %s", name);
            failed = 1;
        }
    }
    for(size_t f = 0; f < COUNT(go_simd_lane_functions); f++)
        if(lane_used[f]) fprintf(out, "\n%s", go_simd_lane_functions[f].text);
    if(checks) fputs(go_checks, out);
    if(check) write_conversions(out);
    if(check) {
        fputs("\nvar zi_simd_helpers = [][3]any{\n", out);
        for(int i = 0; i < go_helper_count; i++)
            fprintf(out, "\t{\"%s\", %s, %s_ref},\n", go_helpers[i], go_helpers[i], go_helpers[i]);
        fputs("}\n", out);
    }
    failed |= ferror(out) != 0;
    failed |= fclose(out) != 0;
    if(failed || GeneratedOutputReplace(temp, path) != 0) {
        if(!failed) Diagnostic((ZirSourceSpan){0}, "zir_go.global", "cannot finish Go output: %s", path);
        remove(temp);
        return 1;
    }
    return 0;
}

int
GoSimdWrite(const char *out_dir, const char *pkg)
{
    if(go_helper_count == 0) return 0;
    return write_helpers(out_dir, pkg, 0);
}

/* Every value helper the checker accepts, found from each operation's name
 * and one or two vector types, written beside its reference form. */
int
GoSimdWriteCheck(const char *out_dir, const char *pkg)
{
    GoSimdBegin();
    int missing = 0;
    for(int o = 0; SimdOpAt(o) != NULL; o++) {
        const SimdOp *op = SimdOpAt(o);
        if(op->shape == SIMD_LOAD || op->shape == SIMD_STORE || op->shape == SIMD_GATHER) continue;
        int two = op->shape == SIMD_NARROW || op->shape == SIMD_REINTERPRET || op->shape == SIMD_CONVERT;
        for(size_t i = 0; i < COUNT(go_vectors); i++)
            for(size_t j = 0; j < (two ? COUNT(go_vectors) : 1); j++) {
                char name[96];
                if(two) format(name, sizeof(name), "zi_%s_%s_%s", op->snake, go_vectors[i], go_vectors[j]);
                else format(name, sizeof(name), "zi_%s_%s", op->snake, go_vectors[i]);
                GoHelper helper;
                GoValue value;
                if(!parse_helper(name, &helper) || !value_signature(&helper, &value)) continue;
                remember(name, strlen(name));
                if(typed_body(name) == NULL) {
                    fprintf(stderr, "no typed Go body for %s\n", name);
                    missing = 1;
                }
            }
    }
    int failed = write_helpers(out_dir, pkg, 1);
    GoSimdBegin();
    return failed || missing;
}
