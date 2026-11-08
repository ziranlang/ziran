#include "zir.h"
#include "zir_bundle.h"
#include "zir_check.h"
#include "zir_parse.h"
#include "zir_serial.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static ZirModule *
module_named(ZirProgram *program, const char *name)
{
    for(int i = 0; i < program->module_count; i++) {
        if(strcmp(program->modules[i].name, name) == 0)
            return &program->modules[i];
    }
    return NULL;
}

static int
has_type(const ZirModule *module, const char *name)
{
    for(int i = 0; i < module->type_count; i++) {
        if(strcmp(module->types[i].name, name) == 0)
            return 1;
    }
    return 0;
}

/* Linkers copy checked functions before later lowering can append nodes.
 * Exercise both nonempty and empty arrays, including growth beyond the
 * source's spare capacity, without changing the original checked graph. */
static void
linked_function_growth(void)
{
    static const char source[] =
        "Answer :: () -> s32 { if false { print(\"dead\") }; return 42 }\n"
        "Empty :: () { if false { print(\"dead\") } }\n";
    ZirProgram *program = parse_source_text("growth.zi", source);
    assert(program != NULL);
    ZirProgram *set[] = {program};
    assert(CheckPrograms(set, 1));
    for(int native = 0; native < 2; native++) {
        for(int f = 0; f < 2; f++) {
            ZirFunction *original = &program->modules[0].functions[f];
            int exprs = original->expr_count, stmts = original->stmt_count;
            ZirProgram *linked = native ? NativeLink(program, "growth", original->name) :
                                         BundleLink(program, "growth", original->name);
            assert(linked != NULL);
            ZirFunction *copy = &module_named(linked, "growth")->functions[0];
            int copied_exprs = copy->expr_count, copied_stmts = copy->stmt_count;
            assert(copied_exprs < exprs && copied_stmts < stmts);
            assert(!copied_exprs || copy->exprs != original->exprs);
            assert(!copied_stmts || copy->stmts != original->stmts);
            const char *first_text = copied_exprs ? copy->exprs[0].text : "";
            ZirStmtKind first_kind = copied_stmts ? copy->stmts[0].kind : ZIR_STMT_UNKNOWN;
            int additions = original->expr_cap + original->stmt_cap + 1;
            for(int i = 0; i < additions; i++) {
                assert(FunctionAddExpr(copy, ZIR_EXPR_INT, "7", original->span));
                assert(FunctionAddStmt(copy, ZIR_STMT_RETURN, "return 7", original->span));
            }
            assert(copy->expr_count == copied_exprs + additions);
            assert(copy->stmt_count == copied_stmts + additions);
            assert(original->expr_count == exprs && original->stmt_count == stmts);
            if(copied_exprs) assert(!strcmp(copy->exprs[0].text, first_text));
            if(copied_stmts) assert(copy->stmts[0].kind == first_kind);
            ProgramFree(linked);
        }
    }
    ProgramFree(program);
}

int
main(void)
{
    linked_function_growth();
    static const char shapes[] =
        "Point :: struct {\n"
        "    x: s32\n"
        "}\n"
        "Rectangle :: struct {\n"
        "    origin: Point\n"
        "    width: s32\n"
        "}\n"
        "Unused :: struct {\n"
        "    value: s32\n"
        "}\n";
    static const char operations[] =
        "#import \"shapes\"\n"
        "#program_export\n"
        "Measure :: (rect: Rectangle) -> s32 {\n"
        "    return rect.origin.x + rect.width\n"
        "}\n";
    static const char application[] =
        "#import \"operations\"\n"
        "#import \"shapes\"\n"
        "#program_export\n"
        "Answer :: () -> s32 {\n"
        "    rect: Rectangle\n"
        "    rect.origin.x = 2\n"
        "    rect.width = 40\n"
        "    return Measure(rect)\n"
        "}\n";
    ZirProgram *programs[] = {
        parse_source_text("shapes.zi", shapes),
        parse_source_text("operations.zi", operations),
        parse_source_text("application.zi", application)
    };
    ZirProgram merged = {0};
    ZirProgram *linked;
    const ZirModule *shape_module;
    const ZirModule *operation_module;
    ZirModule *application_module;

    for(int i = 0; i < 3; i++)
        assert(programs[i] != NULL);
    assert(CheckPrograms(programs, 3));
    for(int i = 0; i < 3; i++)
        merged.module_count += programs[i]->module_count;
    merged.modules = calloc((size_t)merged.module_count, sizeof(*merged.modules));
    assert(merged.modules != NULL);
    int next = 0;
    for(int i = 0; i < 3; i++) {
        for(int m = 0; m < programs[i]->module_count; m++)
            merged.modules[next++] = programs[i]->modules[m];
    }
    ZirProgram *merged_ptr = &merged;
    assert(LinkImports(&merged_ptr, 1));
    linked = BundleLink(&merged, "application", "Answer");
    assert(linked != NULL);
    shape_module = module_named(linked, "shapes");
    operation_module = module_named(linked, "operations");
    application_module = module_named(linked, "application");
    assert(shape_module != NULL);
    assert(operation_module != NULL);
    assert(application_module != NULL);
    assert(has_type(shape_module, "Point"));
    assert(has_type(shape_module, "Rectangle"));
    assert(!has_type(shape_module, "Unused"));
    assert(shape_module->function_count == 0);
    assert(operation_module->function_count == 1);
    assert(application_module->function_count == 1);
    assert(FindType(application_module, "Rectangle", NULL) != NULL);
    assert(FindType(operation_module, "Point", NULL) != NULL);

    /* A checked IR graph cannot contain a cycle that would recurse forever
     * in a backend or the portable verifier. */
    ZirExpr *first = &application_module->functions[0].exprs[0];
    int original_left = first->left;
    first->left = 0;
    FILE *cycle_output = tmpfile();
    assert(cycle_output != NULL);
    assert(!ProgramWrite(linked, cycle_output));
    fclose(cycle_output);
    first->left = original_left;
    ZirFunction *entry = &application_module->functions[0];
    int parent = -1;
    for(int i = 0; i < entry->expr_count; i++)
        if(entry->exprs[i].first_child >= 0) {
            parent = i;
            break;
        }
    assert(parent >= 0);
    ZirExpr *child = &entry->exprs[entry->exprs[parent].first_child];
    int original_sibling = child->next_sibling;
    child->next_sibling = parent;
    cycle_output = tmpfile();
    assert(cycle_output != NULL);
    assert(!ProgramWrite(linked, cycle_output));
    fclose(cycle_output);
    child->next_sibling = original_sibling;

    ProgramFree(linked);
    free(merged.modules);
    for(int i = 0; i < 3; i++)
        ProgramFree(programs[i]);

    static const char ordinary_source[] =
        "#program_export\n"
        "Answer :: () -> s32 { return 42 }\n";
    ZirProgram *ordinary = parse_source_text("ordinary.zi", ordinary_source);
    assert(ordinary != NULL);
    ZirProgram *ordinary_set[] = {ordinary};
    assert(CheckPrograms(ordinary_set, 1));
    FILE *saved;
    ZirStmt *statement = &ordinary->modules[0].functions[0].stmts[0];
    ZirStmtKind original_kind = statement->kind;
    const ZirStmtKind retired_kinds[] = {
        ZIR_STMT_UNKNOWN, (ZirStmtKind)(ZIR_STMT_IF_CASE + 1), ZIR_STMT_IF_CASE
    };
    for(size_t i = 0; i < sizeof(retired_kinds) / sizeof(retired_kinds[0]); i++) {
        statement->kind = retired_kinds[i];
        saved = tmpfile();
        assert(saved != NULL && !ProgramWrite(ordinary, saved));
        fclose(saved);
    }
    statement->kind = original_kind;
    ProgramFree(ordinary);

    static const char named_source[] =
        "#program_export\n"
        "Answer :: () -> s32 {\n"
        "    for outer: 0..1 { break outer }\n"
        "    return 42\n"
        "}\n";
    ZirProgram *named = parse_source_text("named.zi", named_source);
    assert(named != NULL);
    ZirProgram *named_set[] = {named};
    assert(CheckPrograms(named_set, 1));
    ZirFunction *named_function = &named->modules[0].functions[0];
    ZirStmt *named_break = NULL;
    for(int i = 0; i < named_function->stmt_count; i++)
        if(named_function->stmts[i].target_id != 0) {
            named_break = &named_function->stmts[i];
            break;
        }
    assert(named_break != NULL);
    named_break->target_id = named_function->stmt_count + 1;
    saved = tmpfile();
    assert(saved != NULL && ProgramWrite(named, saved));
    rewind(saved);
    ZirProgram *stale_target = ProgramRead(saved, "stale-target.zir");
    assert(stale_target != NULL);
    fclose(saved);
    ZirProgram *stale_set[] = {stale_target};
    assert(!CheckCanonicalPrograms(stale_set, 1, NULL));
    ProgramFree(stale_target);
    ProgramFree(named);

    static const char private_source[] =
        "#scope_file\n"
        "Hidden :: () -> s32 { return 42 }\n"
        "#scope_module\n"
        "Answer :: () -> s32 { return Hidden() }\n";
    ZirProgram *private_program =
        parse_source_text("private.zi", private_source);
    assert(private_program != NULL);
    ZirProgram *private_set[] = {private_program};
    assert(CheckPrograms(private_set, 1));
    assert(private_program->modules[0].functions[0].is_file_private);
    ZirSourceSpan *private_span = &private_program->modules[0].functions[1].span;
    *private_span = SpanEnd("other.zi", private_span->line, private_span->column,
                            private_span->end_line, private_span->end_column);
    saved = tmpfile();
    assert(saved != NULL && ProgramWrite(private_program, saved));
    rewind(saved);
    ZirProgram *cross_file = ProgramRead(saved, "cross-file.zir");
    assert(cross_file != NULL);
    fclose(saved);
    ZirProgram *cross_file_set[] = {cross_file};
    assert(!CheckCanonicalPrograms(cross_file_set, 1, NULL));
    ProgramFree(cross_file);
    ProgramFree(private_program);
    return 0;
}
