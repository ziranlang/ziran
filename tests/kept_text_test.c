#include "zir.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *shared;

static void check_limits(size_t length)
{
    char *input = malloc(length + 1);
    assert(input);
    for(size_t i = 0; i < length; i++) input[i] = 'a' + i % 23;
    input[length] = '\0';
    const char *whole = KeepText(input);
    const char *name = KeepName(whole);
    const char *parameters = KeepParameters(whole);
    size_t name_length = length < ZIR_NAME_MAX ? length : ZIR_NAME_MAX - 1;
    size_t parameters_length = length < ZIR_TEXT_MAX ? length : ZIR_TEXT_MAX - 1;
    assert(strlen(name) == name_length && !memcmp(name, input, name_length));
    assert(strlen(parameters) == parameters_length && !memcmp(parameters, input, parameters_length));
    assert(KeepName(input) == name && KeepParameters(input) == parameters);
    for(int i = 0; i < 10; i++) {
        assert(KeepText(whole) == whole && strlen(whole) == length);
        assert(KeepName(whole) == name && KeepParameters(whole) == parameters);
        assert(KeepName(parameters) == name);
        assert(KeepText(name) == name && KeepText(parameters) == parameters);
    }
    memset(input, 'X', length);
    assert(strlen(whole) == length);
    if(length) assert(whole[0] == 'a');
    free(input);
}

static void *grow_texts(void *argument)
{
    intptr_t lane = (intptr_t)argument;
    char buffer[128];
    const char *retained[32];
    for(int i = 0; i < 4096; i++) {
        snprintf(buffer, sizeof(buffer), "kept worker %ld value %d", (long)lane, i);
        const char *kept = KeepText(buffer);
        if(i < 32) retained[i] = kept;
        assert(!strcmp(kept, buffer));
        assert(KeepName(kept) == kept && KeepParameters(kept) == kept);
        assert(KeepText(kept) == kept && KeepText(buffer) == kept);
        /* Reuse the same caller address for different bytes. */
        strcpy(buffer, "shared kept identity");
        assert(KeepText(buffer) == shared && KeepText(shared) == shared);
        assert(!strncmp(kept, "kept worker ", 12));
    }
    for(int i = 0; i < 32; i++) {
        snprintf(buffer, sizeof(buffer), "kept worker %ld value %d", (long)lane, i);
        assert(KeepText(buffer) == retained[i] && !strcmp(retained[i], buffer));
    }
    return NULL;
}

int main(void)
{
    assert(!strcmp(KeepText(NULL), "") && !strcmp(KeepName(NULL), ""));
    assert(!strcmp(KeepParameters(NULL), "") && !strcmp(KeepText(""), ""));
    char mutable[] = "owned bytes";
    const char *owned = KeepText(mutable);
    assert(owned != mutable && KeepText("owned bytes") == owned);
    strcpy(mutable, "other bytes");
    assert(!strcmp(owned, "owned bytes") && !strcmp(KeepText(mutable), "other bytes"));
    strcpy(mutable, "other");
    assert(KeepText(mutable) == KeepText("other"));
    strcpy(mutable, "other byteZ");
    assert(!strcmp(KeepText(mutable), "other byteZ"));
    strcpy(mutable, "other bytes");
    char *bounded = malloc(ZIR_TEXT_MAX);
    assert(bounded);
    strcpy(bounded, "short");
    assert(KeepName(bounded) == KeepText("short"));
    memset(bounded, 'X', ZIR_TEXT_MAX);
    assert(strlen(KeepName(bounded)) == ZIR_NAME_MAX - 1);
    assert(strlen(KeepParameters(bounded)) == ZIR_TEXT_MAX - 1);
    bounded[ZIR_TEXT_MAX - 1] = '\0';
    const char *long_kept = KeepText(bounded);
    bounded[ZIR_TEXT_MAX - 2] = 'Y';
    const char *changed = KeepText(bounded);
    assert(changed != long_kept && changed[ZIR_TEXT_MAX - 2] == 'Y');
    assert(long_kept[ZIR_TEXT_MAX - 2] == 'X');
    bounded[ZIR_TEXT_MAX - 1] = 'Z';
    assert(KeepParameters(bounded) == changed);
    free(bounded);
    const char *whole = KeepText("prefix suffix");
    const char *suffix = KeepText(whole + 7);
    assert(suffix != whole + 7 && suffix == KeepText("suffix"));
    assert(!strcmp(suffix, "suffix"));
    const char *bytes = "\303\261\n\"\\\t";
    assert(!strcmp(KeepText(bytes), bytes));
    assert(KeepNameFormat("name_%d", 42) == KeepText("name_42"));
    size_t lengths[] = {0, 1, ZIR_NAME_MAX - 2, ZIR_NAME_MAX - 1,
                        ZIR_NAME_MAX, ZIR_NAME_MAX + 1, ZIR_TEXT_MAX - 1,
                        ZIR_TEXT_MAX, ZIR_TEXT_MAX + 128};
    for(size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
        check_limits(lengths[i]);
    shared = KeepText("shared kept identity");
    pthread_t workers[8];
    for(intptr_t i = 0; i < 8; i++) assert(!pthread_create(&workers[i], NULL, grow_texts, (void *)i));
    for(int i = 0; i < 8; i++) assert(!pthread_join(workers[i], NULL));
    assert(KeepText(mutable) == KeepText("other bytes"));
    assert(!strcmp(owned, "owned bytes") && KeepText(owned) == owned);
    puts("Kept text identity, truncation, mutable inputs and concurrent growth passed");
}
