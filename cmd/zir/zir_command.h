#ifndef ZIR_COMMAND_H
#define ZIR_COMMAND_H

#include "zir_diagnostic.h"
#include "zir_load.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Options belong to each command; only their bounded argv mechanics are
 * shared. A value consumes the next word even when it starts with '-'. */
typedef struct CommandOption {
    const char *name;
    const char **value;
    int *flag;
    int *count;
    int capacity;
    int (*valid)(const char *);
    void (*consume)(const char *);
} CommandOption;

#define COMMAND_TEXT(name_, value_) {name_, .value = &(value_)}
#define COMMAND_FLAG(name_, flag_) {name_, .flag = &(flag_)}
#define COMMAND_LIST(name_, values_, count_, capacity_, valid_) \
    {name_, .value = (values_), .count = &(count_), \
     .capacity = (capacity_), .valid = (valid_)}
#define COMMAND_CALLBACK(name_, consume_) {name_, .consume = (consume_)}

/* Return the first input index, zero if there is none, or -1 for a usage
 * error. Stop at the first input: subsequent words are filenames. */
static inline int
ReadCommandOptions(int argc, char *const *argv, const CommandOption *options,
                   size_t option_count)
{
    for(int index = 1; index < argc; index++) {
        const char *word = argv[index];
        if(strncmp(word, "--diagnostics=", 14) == 0) {
            if(!SetDiagnosticFormat(word + 14)) return -1;
            continue;
        }
        if(word[0] != '-') return index;
        size_t option = 0;
        while(option < option_count && strcmp(word, options[option].name))
            option++;
        if(option == option_count) return -1;
        const CommandOption *item = &options[option];
        if(item->flag != NULL) {
            *item->flag = 1;
            continue;
        }
        if(index + 1 == argc ||
           (item->count != NULL && *item->count >= item->capacity) ||
           (item->valid != NULL && !item->valid(argv[index + 1])))
            return -1;
        const char *value = argv[++index];
        if(item->consume != NULL) item->consume(value);
        else if(item->count != NULL) item->value[(*item->count)++] = value;
        else *item->value = value;
    }
    return 0;
}

/* A command's entry linker borrows every loaded module. The caller frees
 * only merged->modules; ProgramsFree retains ownership of their contents. */
static inline int
CommandMergePrograms(const ProgramSet *set, ZirProgram *merged)
{
    merged->module_count = 0;
    for(int index = 0; index < set->count; index++)
        merged->module_count += set->programs[index]->module_count;
    merged->context = set->context;
    merged->modules = calloc((size_t)merged->module_count, sizeof(*merged->modules));
    if(merged->modules == NULL) return 0;
    int position = 0;
    for(int index = 0; index < set->count; index++)
        for(int module = 0; module < set->programs[index]->module_count; module++)
            merged->modules[position++] = set->programs[index]->modules[module];
    return 1;
}

static inline int
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

static inline int
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

#endif
