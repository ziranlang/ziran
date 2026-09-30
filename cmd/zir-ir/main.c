#include "zir.h"
#include "zir_law.h"
#include "zir_parse.h"
#include "zir_check.h"
#include "zir_serial.h"
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "zir_bundle.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void
usage(void)
{
    fprintf(stderr, "usage: zi2zir [--diagnostics=text|json] [--lint] [--entry module:function] [--module-path DIR] [--define NAME] --root DIR -o DIR file.zi|file.zir ...\n");
}

static int
valid_define(const char *name)
{
    if(name == NULL || (!isalpha((unsigned char)*name) && *name != '_') ||
       strlen(name) >= ZIR_NAME_MAX)
        return 0;
    for(const unsigned char *p = (const unsigned char *)name + 1; *p; p++)
        if(!isalnum(*p) && *p != '_')
            return 0;
    return 1;
}

static int
split_entry(const char *text, char *module, char *function)
{
    const char *separator = text ? strrchr(text, ':') : NULL;
    size_t length;
    if(separator == NULL || separator == text || separator[1] == 0)
        return 0;
    length = (size_t)(separator - text);
    if(length >= ZIR_NAME_MAX || strlen(separator + 1) >= ZIR_NAME_MAX)
        return 0;
    memcpy(module, text, length);
    module[length] = 0;
    strcpy(function, separator + 1);
    return 1;
}

static int
write_program(const ZirProgram *program, const char *out_dir)
{
    const char *source = program->modules[0].source_path;
    size_t length = strlen(source);
    char output[ZIR_PATH_MAX * 2];
    FILE *file;

    if(length > 3 && strcmp(source + length - 3, ".zi") == 0)
        length -= 3;
    if(snprintf(output, sizeof(output), "%s/%.*s.zir", out_dir,
                (int)length, source) >= (int)sizeof(output)) {
        Diagnostic(program->modules[0].span, "zir.output",
                      "IR output path is too long");
        return 0;
    }
    for(char *cursor = output + 1; *cursor; cursor++) {
        if(*cursor != '/')
            continue;
        *cursor = '\0';
        mkdir(output, 0755);
        *cursor = '/';
    }
    file = fopen(output, "wb");
    if(file == NULL) {
        Diagnostic(program->modules[0].span, "zir.output",
                      "cannot open IR output: %s", output);
        return 0;
    }
    if(!ProgramWrite(program, file)) {
        Diagnostic(program->modules[0].span, "zir.output",
                      "cannot serialize checked IR: %s", output);
        fclose(file);
        remove(output);
        return 0;
    }
    if(fclose(file) != 0) {
        Diagnostic(program->modules[0].span, "zir.output",
                      "cannot finish IR output: %s", output);
        remove(output);
        return 0;
    }
    return 1;
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    const char *entry = NULL;
    char entry_module[ZIR_NAME_MAX], entry_function[ZIR_NAME_MAX];
    int check_only = 0;
    int lint = 0;
    int first_file = 0;
    int result = 1;
    ProgramSet set = {0};
    ZirProgram merged = {0};
    ZirProgram *linked = NULL;
    const char *module_paths[64];
    int module_path_count = 0;
    const char *defines[64];
    int define_count = 0;
    int count;

    for(int i = 1; i < argc; i++) {
        if(strncmp(argv[i], "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(argv[i] + 14)) {
                usage();
                return 1;
            }
        } else if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "--module-path") == 0 && i + 1 < argc && module_path_count < 64) {
            module_paths[module_path_count++] = argv[++i];
        } else if(strcmp(argv[i], "--define") == 0 && i + 1 < argc &&
                  define_count < 64 && valid_define(argv[i + 1])) {
            defines[define_count++] = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            entry = argv[++i];
        } else if(strcmp(argv[i], "--check-only") == 0) {
            check_only = 1;
        } else if(strcmp(argv[i], "--lint") == 0) {
            lint = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(root == NULL || (!check_only && out_dir == NULL) || first_file == 0 ||
       (entry != NULL && (check_only ||
                          !split_entry(entry, entry_module, entry_function)))) {
        usage();
        return 1;
    }
    if(!ProgramsLoadWithDefines(&set, root, module_paths, module_path_count,
                                defines, define_count,
                                (const char *const *)(argv + first_file),
                                argc - first_file))
        goto done;
    count = set.count;
    if(!CheckCanonicalPrograms(set.programs, count,
                               (const char *const *)set.paths)) {
        if(check_only)
            PrintLawResults(set.programs, count, stdout);
        goto done;
    }
    if(lint)
        LintPrograms(set.programs, count);
    if(check_only)
        PrintLawResults(set.programs, count, stdout);
    if(!check_only) {
        if(entry != NULL) {
            for(int i = 0; i < count; i++)
                merged.module_count += set.programs[i]->module_count;
            merged.modules = calloc((size_t)merged.module_count,
                                    sizeof(*merged.modules));
            if(merged.modules == NULL)
                goto done;
            int position = 0;
            for(int i = 0; i < count; i++)
                for(int m = 0; m < set.programs[i]->module_count; m++)
                    merged.modules[position++] = set.programs[i]->modules[m];
            ZirProgram *merged_ptr = &merged;
            if(!LinkImports(&merged_ptr, 1))
                goto done;
            linked = NativeLink(&merged, entry_module, entry_function);
            if(linked == NULL)
                goto done;
            for(int i = 0; i < linked->module_count; i++) {
                ZirProgram view = {0};
                view.modules = &linked->modules[i];
                view.module_count = 1;
                if(!write_program(&view, out_dir))
                    goto done;
            }
        } else {
            for(int i = 0; i < count; i++) {
                if(set.programs[i]->module_count < 1 ||
                   !write_program(set.programs[i], out_dir))
                    goto done;
            }
        }
    }
    result = 0;
done:
    ProgramFree(linked);
    free(merged.modules);
    ProgramsFree(&set);
    return result;
}
