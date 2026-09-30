#ifndef ZIRAN_ZIR_LOAD_H
#define ZIRAN_ZIR_LOAD_H

#include "zir.h"

typedef struct ProgramSet {
    ZirProgram **programs;
    char **paths;
    char **roots;
    int count;
} ProgramSet;

/* Load explicit inputs and their extensionless module imports. Module paths
 * are ordinary search roots, independent of any UI library. */
int ProgramsLoad(ProgramSet *set, const char *root,
                 const char *const *module_paths, int module_path_count,
                 const char *const *inputs, int input_count);
/* The standard modules and C headers that came with this compiler, or
 * NULL. ZIRAN_STD and ZIRAN_INCLUDE override them. */
const char *ToolchainStandardDirectory(void);
const char *ToolchainIncludeDirectory(void);
int ProgramsLoadWithDefines(ProgramSet *set, const char *root,
                            const char *const *module_paths,
                            int module_path_count,
                            const char *const *defines, int define_count,
                            const char *const *inputs, int input_count);
void ProgramsFree(ProgramSet *set);

#endif
