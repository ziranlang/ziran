#include "zir_vm_internal.h"
#include <assert.h>

static Value
calculate(Vm *vm, const char *operation, Value left, Value right,
          const char *left_type, const char *right_type)
{
    vm->failed = 0;
    Value result = binary_value(vm, operation, left, right, left_type, right_type);
    assert(!vm->failed);
    return result;
}

int
main(void)
{
    Vm vm = {0};
    Value a = int_value(-17), b = int_value(5);
    const char *arithmetic[] = {"+", "-", "*", "/", "%"};
    const int64_t expected[] = {-12, -22, -85, -3, -2};
    for(size_t i = 0; i < sizeof expected / sizeof *expected; i++)
        assert(calculate(&vm, arithmetic[i], a, b, "s64", "s64").integer == expected[i]);
    assert(calculate(&vm, "/", int_value(INT64_MIN), int_value(-1), "s64", "s64").integer == INT64_MIN);
    assert(calculate(&vm, "%", int_value(INT64_MIN), int_value(-1), "s64", "s64").integer == 0);
    assert(calculate(&vm, "+", uint_value(UINT64_MAX), uint_value(1), "u64", "u64").bits == 0);
    assert(calculate(&vm, ">", uint_value(UINT64_MAX), int_value(5), "u64", "s64").integer == 1);
    assert(calculate(&vm, "/", uint_value(UINT64_MAX), uint_value(3), "u64", "u64").bits == UINT64_MAX / 3);
    assert(calculate(&vm, "&", int_value(13), int_value(6), "s32", "s32").integer == 4);
    assert(calculate(&vm, "|", int_value(13), int_value(6), "s32", "s32").integer == 15);
    assert(calculate(&vm, "^", int_value(13), int_value(6), "s32", "s32").integer == 11);
    assert(calculate(&vm, "<<", int_value(129), int_value(1), "u8", "s32").integer == 2);
    assert(calculate(&vm, ">>", int_value(-128), int_value(1), "s8", "s32").integer == 192);
    assert(calculate(&vm, ">>", int_value(-8), int_value(2), "s64", "s32").integer == -2);
    assert(calculate(&vm, "<<", uint_value(1), int_value(63), "u64", "s32").bits == UINT64_C(0x8000000000000000));

    Value real = real_value(1.5);
    assert(calculate(&vm, "*", real, int_value(4), "float64", "s32").real == 6.0);
    assert(calculate(&vm, "<=", real, real_value(1.5), "float64", "float64").integer == 1);
    assert(calculate(&vm, ">=", real, real_value(2.0), "float64", "float64").integer == 0);
    static const unsigned char text[] = {'a', 0, 'b'};
    static const unsigned char other[] = {'a', 0, 'c'};
    Value string = string_value(text, sizeof text);
    assert(calculate(&vm, "==", string, string, "string", "string").integer == 1);
    assert(calculate(&vm, "!=", string, string_value(other, sizeof other), "string", "string").integer == 1);

    Value pointer = {.kind = VALUE_POINTER, .pointee = &a};
    Value second_pointer = {.kind = VALUE_POINTER, .pointee = &b};
    assert(calculate(&vm, "==", pointer, pointer, "*s64", "*s64").integer == 1);
    assert(calculate(&vm, "!=", pointer, second_pointer, "*s64", "*s64").integer == 1);
    ZirType tone = {0}, flags = {0};
    strcpy(tone.name, "Tone");
    strcpy(flags.name, "Flags");
    strcpy(flags.enum_backing, "u32");
    flags.is_enum_flags = 1;
    Value enumeration = enum_value(&tone, 3);
    assert(calculate(&vm, "==", enumeration, enumeration, "Tone", "Tone").integer == 1);
    assert(calculate(&vm, "|", enum_value(&flags, 1), enum_value(&flags, 4), "Flags", "Flags").integer == 5);
    Value slot = {.kind = VALUE_SLOT};
    assert(calculate(&vm, "==", slot, int_value(0), "Handler", "null").integer == 1);

    /* A checked operation is exact: reject malformed saved spellings even
     * for operands whose special equality path would otherwise accept them. */
    const char *invalid[] = {"", "=", "!", "++", "+=", "==x", "<<<", "&&", "||", "**", "<=>"};
    Value values[] = {a, real, string, pointer, enumeration, enum_value(&flags, 1), slot};
    const char *types[] = {"s64", "float64", "string", "*s64", "Tone", "Flags", "Handler"};
    for(size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        for(size_t j = 0; j < sizeof values / sizeof *values; j++) {
            vm.failed = 0;
            (void)binary_value(&vm, invalid[i], values[j], values[j], types[j], types[j]);
            assert(vm.failed);
        }
    }
    const char *divide[] = {"/", "%"};
    for(size_t i = 0; i < sizeof divide / sizeof *divide; i++) {
        vm.failed = 0;
        (void)binary_value(&vm, divide[i], a, int_value(0), "s64", "s64");
        assert(vm.failed);
    }
    vm.failed = 0;
    (void)binary_value(&vm, "<<", uint_value(1), int_value(64), "u64", "s32");
    assert(vm.failed);
    vm.failed = 0;
    (void)binary_value(&vm, ">>", a, int_value(-1), "s64", "s32");
    assert(vm.failed);
    return 0;
}
