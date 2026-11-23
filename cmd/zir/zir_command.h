#ifndef ZIR_COMMAND_H
#define ZIR_COMMAND_H

/* Command-line mechanics: option parsing, target selection, program merging
 * and bounded reads. The implementation is cmd/compiler_cli.zi; these
 * wrappers keep the C drivers' call syntax until they become Ziran. */
#include "zir_diagnostic.h"
#include "zir_load.h"
#include "compiler_cli.h"
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Options belong to each command; only their bounded argv mechanics are
 * shared. A value consumes the next word even when it starts with '-'. */
#define COMMAND_TEXT(name_, value_) \
    {(uint8_t *)(name_), .value = (uint8_t **)(void *)&(value_)}
#define COMMAND_FLAG(name_, flag_) \
    {(uint8_t *)(name_), .flag = (int32_t *)(void *)&(flag_)}
#define COMMAND_LIST(name_, values_, count_, capacity_, valid_) \
    {(uint8_t *)(name_), .value = (uint8_t **)(void *)(values_), \
     .count = (int32_t *)(void *)&(count_), .capacity = (int32_t)(capacity_), \
     .valid = (OptionValid)(void *)(valid_)}

/* Compiling is background work. */
static inline void
CommandLowerPriority(void)
{
    compiler_cli_CommandLowerPriority();
}

static inline const char **
CommandModulePaths(int argc)
{
    return (const char **)compiler_cli_CommandModulePaths(argc);
}

static inline int
ReadCommandOptions(int argc, char *const *argv, const CommandOption *options,
                   size_t option_count)
{
    return compiler_cli_ReadCommandOptions(argc, (uint8_t **)(void *)argv,
            (CommandOption *)(void *)options, option_count);
}

static inline int
CommandMergePrograms(const ProgramSet *set, ZirProgram *merged)
{
    return compiler_cli_CommandMergePrograms((ProgramSet *)(void *)set, merged);
}

static inline int
CommandConfigureTarget(ZirTargetBackend backend, const char *os_name,
                       const char *cpu_name, ZirTargetOS *os)
{
    return compiler_cli_CommandConfigureTarget((int32_t)backend,
            (uint8_t *)(void *)os_name, (uint8_t *)(void *)cpu_name,
            (int32_t *)(void *)os);
}

static inline unsigned char *
CommandReadBounded(const char *path, long limit, size_t *size)
{
    return (unsigned char *)compiler_cli_CommandReadBounded(
            (uint8_t *)(void *)path, (int64_t)limit, size);
}

static inline void
CommandEnableChecks(const ProgramSet *set)
{
    compiler_cli_CommandEnableChecks((ProgramSet *)(void *)set);
}

static inline int
valid_define(const char *name)
{
    return compiler_cli_ValidDefine((uint8_t *)(void *)name);
}

static inline int
split_entry(const char *text, char *module, char *function)
{
    return compiler_cli_SplitEntry((uint8_t *)(void *)text, (uint8_t *)module,
            (uint8_t *)function);
}

#endif
