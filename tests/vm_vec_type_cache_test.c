#include "zir_vm_internal.h"
#include <assert.h>

static void
add_type(ZirModule *module, const char *name, const char *body, int owned)
{
    ZirType *type = ModuleAddType(module, name, Span("vec-cache.zi", 1, 1));
    assert(type != NULL);
    strcpy(type->body, body);
    type->is_owned_vec = owned;
    type->is_public = 1;
    TypeLookupsChanged();
}

static void
check(Vm *vm, const ZirModule *module, const char *name, const char *expected)
{
    char output[ZIR_NAME_MAX];
    for(int repeat = 0; repeat < 8; repeat++) {
        memset(output, 'x', sizeof(output));
        /* Buffer refusal must preserve both output and cached validity. */
        assert(!vm_vec_element_type(vm, module, name, output, 1));
        assert(output[0] == 'x');
        assert(vm_vec_element_type(vm, module, name, NULL, 0) == (expected != NULL));
        int valid = vm_vec_element_type(vm, module, name, output, sizeof(output));
        assert(valid == (expected != NULL));
        if(expected != NULL)
            assert(strcmp(output, expected) == 0);
        else
            assert(output[0] == 'x');
    }
}

int
main(void)
{
    ZirProgram *program = ProgramNew();
    assert(program != NULL);
    ProgramAddModule(program, "first", "first.zi", Span("first.zi", 1, 1));
    ProgramAddModule(program, "second", "second.zi", Span("second.zi", 1, 1));
    ZirModule *first = &program->modules[0];
    ZirModule *second = &program->modules[1];
    add_type(first, "Numbers", "data: *s64; count: s64; capacity: s64;", 1);
    add_type(second, "Numbers", "data: *u8; count: s64; capacity: s64;", 1);
    add_type(first, "Ordinary", "data: *s64; count: s64; capacity: s64;", 0);
    add_type(first, "Malformed", "data: *u8; count: s32; capacity: s64;", 1);
    add_type(first, "Extra", "data: *u8; count: s64; capacity: s64; extra: s64;", 1);
    add_type(first, "Missing", "data: *u8; count: s64;", 1);
    ZirImport *import = ModuleAddImport(first, ZIR_IMPORT_MODULE, "Other", "second.zi",
                                       "", 0, Span("first.zi", 1, 1));
    assert(import != NULL);
    import->resolved_module = second;
    TypeLookupsChanged();

    /* Direct cache collisions must replace metadata without mixing types. */
    char collision[ZIR_NAME_MAX];
    unsigned suffix = 0;
    do {
        snprintf(collision, sizeof(collision), "Collision%u", suffix++);
    } while((vm_field_hash(collision) & 2047) != (vm_field_hash("Numbers") & 2047));
    add_type(first, collision, "data: *string; count: s64; capacity: s64;", 1);

    Vm *instances[2] = {calloc(1, sizeof(Vm)), calloc(1, sizeof(Vm))};
    assert(instances[0] != NULL && instances[1] != NULL);
    for(int instance = 0; instance < 2; instance++) {
        check(instances[instance], first, "Numbers", "s64");
        check(instances[instance], second, "Numbers", "u8");
        check(instances[instance], first, "Other.Numbers", "u8");
        check(instances[instance], first, "Ordinary", NULL);
        check(instances[instance], first, "Malformed", NULL);
        check(instances[instance], first, "Extra", NULL);
        check(instances[instance], first, "Missing", NULL);
        check(instances[instance], first, "Absent", NULL);
        check(instances[instance], first, collision, "string");
        check(instances[instance], first, "Numbers", "s64");
    }
    free(instances[0]->type_sites);
    free(instances[0]);
    check(instances[1], second, "Numbers", "u8");
    free(instances[1]->type_sites);
    free(instances[1]);
    ProgramFree(program);

    /* Aggregate verification borrows a temporary VM. Its type metadata must
     * be reclaimed after each validation, including repeated bundle checks. */
    program = ProgramNew();
    assert(program != NULL);
    first = ProgramAddModule(program, "validation", "validation.zi",
                             Span("validation.zi", 1, 1));
    assert(first != NULL);
    add_type(first, "Payload", "item: s64;", 0);
    ModuleAddGlobal(first, "payload", "Payload", "Payload.{item = 7}",
                    Span("validation.zi", 2, 1));
    ZirFunction *entry = ModuleAddFunction(first, "Check", "", "void", 1,
                                          Span("validation.zi", 3, 1));
    assert(entry != NULL);
    entry->checked = 1;
    for(int repeat = 0; repeat < 128; repeat++)
        assert(VmVerify(program, "validation", "Check"));
    ProgramFree(program);
    return 0;
}
