#include "zir_memory.h"
#include "zir_native.h"
#include "zir_emit.h"
#include "zir_check.h"
#include "zir_diagnostic.h"
#include <string.h>

/* Native symbol planning is independent of language validity. It only changes
 * in-memory output metadata, which never participates in saved IR identity. */
int
PrepareNativeNames(ZirProgram **programs, int count)
{
    NativeNamesReset();
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int g = 0; g < module->global_count; g++)
                module->globals[g].native_name_collision = 0;
            for(int d = 0; d < module->define_count; d++)
                module->defines[d].native_name_collision = 0;
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                type->native_name_mangled = type->is_file_private &&
                    !strncmp(type->name, "zir_file_type_", 14);
                if(type->is_record_template ||
                   (type->is_extern && !type->foreign_target[0])) continue;
                char mapped[ZIR_NAME_MAX * 2];
                TargetBindingName(NULL, ZIR_GO, type->name, mapped, sizeof(mapped));
                if(NativeHeaderKeyword(type->name) || strcmp(mapped, type->name))
                    type->native_name_mangled = 1;
            }
        }
    if(!MarkNativeNameCollisions(programs, count)) {
        DiagnosticOutOfMemory();
        return 0;
    }
    for(int p = 0; p < count; p++)
        for(int m = 0; m < programs[p]->module_count; m++) {
            ZirModule *module = &programs[p]->modules[m];
            for(int t = 0; t < module->type_count; t++) {
                ZirType *type = &module->types[t];
                if(type->is_record_template ||
                   (type->is_extern && !type->foreign_target[0])) continue;
                for(int q = p; q < count; q++)
                    for(int n = q == p ? m : 0;
                        n < programs[q]->module_count; n++) {
                        ZirModule *other_module = &programs[q]->modules[n];
                        for(int u = q == p && n == m ? t + 1 : 0;
                            u < other_module->type_count; u++) {
                            ZirType *other = &other_module->types[u];
                            if(other->is_record_template ||
                               (other->is_extern && !other->foreign_target[0]) ||
                               strcmp(type->name, other->name)) continue;
                            if(same_type_application(module, type, other_module, other) ||
                               SameMapType(module, type, other_module, other))
                                continue;
                            type->native_name_mangled = 1;
                            other->native_name_mangled = 1;
                        }
                    }
            }
        }
    return 1;
}

int zir_native_blocks;

const char *
VectorTypeHere(const char *type, ZirTarget target)
{
    ZirVector vector;
    if(!VectorType(type, &vector) || !vector.is_native)
        return type;
    if(target != ZIR_C || zir_native_blocks == 1)
        return vector.block;
    if(zir_native_blocks == 2)
        return VectorTypeName(vector.lane, vector.lanes * 2);
    return vector.name;
}

const char *
TargetType(const char *type, ZirTarget target)
{
    static const struct { const char *type, *c, *go; } map[] = {
        {"s8", "int8_t", "int8"}, {"s16", "int16_t", "int16"},
        {"s32", "int32_t", "int32"}, {"s64", "int64_t", "int64"},
        {"isize", "ptrdiff_t", "int"}, {"usize", "size_t", "uint"},
        {"u8", "uint8_t", "uint8"}, {"u16", "uint16_t", "uint16"},
        {"u32", "uint32_t", "uint32"}, {"u64", "uint64_t", "uint64"},
        {"float32", "float", "float32"}, {"float64", "double", "float64"},
        {"bool", "bool", "bool"}, {"void", "void", ""},
        {"char", "char", "byte"},
        {"string", "String", "string"}, {NULL, NULL, NULL}
    };
    /* Vectors are value types: a compiler vector in C (see ziran_simd.h) and
     * a lane array in Go. A native vector, such as s16xN, is the type
     * VectorTypeHere gives, which in Go is always a fixed one. */
    static const char *const c_vectors[] = {
        "zi_u8x16", "zi_s8x16", "zi_u16x8", "zi_s16x8", "zi_u32x4", "zi_s32x4",
        "zi_u64x2", "zi_s64x2", "zi_f32x4", "zi_f64x2", "zi_u8x32", "zi_s8x32",
        "zi_u16x16", "zi_s16x16", "zi_u32x8", "zi_s32x8", "zi_u64x4", "zi_s64x4",
        "zi_f32x8", "zi_f64x4", "zi_u8xN", "zi_s8xN", "zi_u16xN", "zi_s16xN", "zi_u32xN",
        "zi_s32xN", "zi_u64xN", "zi_s64xN", "zi_f32xN", "zi_f64xN"
    };
    static const char *const go_vectors[] = {
        "[16]uint8", "[16]int8", "[8]uint16", "[8]int16", "[4]uint32", "[4]int32",
        "[2]uint64", "[2]int64", "[4]float32", "[2]float64", "[32]uint8", "[32]int8",
        "[16]uint16", "[16]int16", "[8]uint32", "[8]int32", "[4]uint64", "[4]int64",
        "[8]float32", "[4]float64"
    };
    if(VectorType(type, NULL)) {
        type = VectorTypeHere(type, target);
        for(size_t i = 0; i < sizeof(c_vectors) / sizeof(*c_vectors); i++)
            if(!strcmp(type, c_vectors[i] + 3))
                return target == ZIR_C ? c_vectors[i] :
                    i < sizeof(go_vectors) / sizeof(*go_vectors) ? go_vectors[i] : NULL;
    }
    const char *scalar = ScalarType(type);
    if(!strcmp(type, "integer")) type = "s64";
    else if(!strcmp(type, "real")) type = "float64";
    else if(*scalar) type = scalar;
    if(SliceElementType(type, NULL, 0) && target == ZIR_C)
        return "Slice";
    for(int i = 0; map[i].type; i++)
        if(!strcmp(type, map[i].type)) return target == ZIR_GO ? map[i].go : map[i].c;
    return NULL;
}
