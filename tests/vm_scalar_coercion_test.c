#include "zir_vm_internal.h"
#include <assert.h>

static ZirModule module;

static Value
convert(Vm *vm, Value input, const char *type)
{
    vm->failed = 0;
    Value result = coerce(vm, &module, input, type);
    assert(!vm->failed);
    return result;
}

static void
reject(Vm *vm, Value input, const char *type)
{
    vm->failed = 0;
    (void)coerce(vm, &module, input, type);
    assert(vm->failed);
}

int
main(void)
{
    Vm vm = {0};
    const char *types[] = {"s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64"};
    const unsigned widths[] = {8, 8, 16, 16, 32, 32, 64, 64};
    const uint64_t inputs[] = {0, 1, 127, 128, 255, 256, 32767, 32768, 65535,
        UINT32_MAX, UINT64_C(0x80000000), UINT64_C(0x8000000000000000),
        INT64_MAX, UINT64_MAX, UINT64_MAX - 128, UINT64_MAX - 32768};
    for(size_t t = 0; t < sizeof types / sizeof *types; t++) {
        unsigned width = widths[t];
        int is_unsigned = (int)(t % 2);
        uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
        for(size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
            uint64_t expected = inputs[i] & mask;
            if(!is_unsigned && width < 64 &&
               (expected & (UINT64_C(1) << (width - 1))))
                expected |= ~mask;
            for(int representation = 0; representation < 2; representation++) {
                int64_t signed_input;
                memcpy(&signed_input, &inputs[i], sizeof signed_input);
                Value input = representation ? uint_value(inputs[i]) : int_value(signed_input);
                Value result = convert(&vm, input, types[t]);
                assert(result.kind == VALUE_INT && integer_bits(result) == expected);
                assert(result.unsigned64 == (is_unsigned && width == 64));
            }
        }

        /* Independent mathematical bounds, including adjacent representable
         * values and nonfinite input, guard casts before C integer conversion. */
        double upper = ldexp(1.0, width - !is_unsigned);
        double lower = is_unsigned ? 0.0 : -upper;
        double values[] = {0.0, -0.0, 1.75, -1.75, lower,
            nextafter(lower, INFINITY), nextafter(lower, -INFINITY),
            nextafter(upper, -INFINITY), upper, nextafter(upper, INFINITY),
            -0.5, INFINITY, -INFINITY, NAN};
        for(size_t i = 0; i < sizeof values / sizeof *values; i++) {
            double number = values[i];
            if(!isfinite(number) || number < lower || number >= upper) {
                reject(&vm, real_value(number), types[t]);
                continue;
            }
            Value result = convert(&vm, real_value(number), types[t]);
            uint64_t expected = is_unsigned ? (uint64_t)trunc(number) :
                                             (uint64_t)(int64_t)trunc(number);
            assert(result.kind == VALUE_INT && integer_bits(result) == expected);
            assert(result.unsigned64 == (is_unsigned && width == 64));
        }
    }
    assert(convert(&vm, real_value(0.0), "bool").integer == 0);
    assert(convert(&vm, real_value(NAN), "bool").integer == 1);
    assert(convert(&vm, int_value(-1), "bool").integer == 1);
    assert(convert(&vm, int_value(-1), "*u8").bits == UINT64_MAX);
    Value pointee = int_value(9);
    Value pointer = {.kind = VALUE_POINTER, .pointee = &pointee};
    assert(convert(&vm, pointer, "*s64").pointee == &pointee);
    assert(convert(&vm, real_value(1.75), "integer").real == 1.75);
    assert(convert(&vm, uint_value(UINT64_MAX), "integer").bits == UINT64_MAX);
    assert(convert(&vm, uint_value(UINT64_MAX), "null").integer == -1);
    assert(convert(&vm, real_value(-1.75), "null").integer == -1);
    reject(&vm, real_value(2147483648.0), "null");
    double precise = 1.0 + ldexp(1.0, -30);
    assert(convert(&vm, real_value(precise), "float32").real == (float)precise);
    assert(convert(&vm, real_value(precise), "float64").real == precise);
    assert(convert(&vm, real_value(precise), "real").real == precise);
    Value text = string_value((const unsigned char *)"abc", 3);
    assert(convert(&vm, text, "string").data == text.data);
    assert(convert(&vm, text, "void").kind == VALUE_VOID);
    reject(&vm, text, "s32");
    reject(&vm, int_value(3), "string");
    const char *invalid[] = {"", "s", "u", "s8x", "u64x", "boolean", "integerx",
        "float3", "float32x", "float64x", "realx", "nullx", "voidx", "stringx"};
    for(size_t i = 0; i < sizeof invalid / sizeof *invalid; i++)
        reject(&vm, int_value(1), invalid[i]);
    free(vm.type_sites);
    free(vm.vec_types);
    return 0;
}
