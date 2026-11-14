#include "zir_memory.h"
#include "zir_files.h"
#include "zir_link.h"
#include "zir_diagnostic.h"
#include "compiler_bundle.h"
#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void
ZxAssetsFree(ZxAssets *assets)
{
    if(assets == NULL) return;
    for(size_t i = 0; i < assets->count; i++) {
        free(assets->items[i].name);
        free(assets->items[i].data);
    }
    free(assets->items);
    memset(assets, 0, sizeof(*assets));
}

/* The rest of IN in one block; NULL when it cannot be read. */
static unsigned char *
read_rest(FILE *in, size_t *size)
{
    long start = ftell(in), end;
    if(start < 0 || fseek(in, 0, SEEK_END) != 0 || (end = ftell(in)) < start ||
       fseek(in, start, SEEK_SET) != 0 || (uint64_t)(end - start) >= SIZE_MAX)
        return NULL;
    *size = (size_t)(end - start);
    unsigned char *bytes = malloc(*size ? *size : 1);
    if(bytes != NULL &&
       (fread(bytes, 1, *size, in) != *size || fgetc(in) != EOF || ferror(in))) {
        free(bytes);
        return NULL;
    }
    return bytes;
}

static char *
join_path(const char *left, const char *right)
{
    size_t a = strlen(left), b = strlen(right);
    if(a > SIZE_MAX - b - 2) return NULL;
    char *result = malloc(a + b + 2);
    if(result != NULL) {
        memcpy(result, left, a);
        result[a] = '/';
        memcpy(result + a + 1, right, b + 1);
    }
    return result;
}

int
ZxAssetsAdd(ZxAssets *assets, const char *name, const void *data, size_t size)
{
    size_t low = 0, high = assets->count;
    while(low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(assets->items[middle].name, name);
        if(order == 0) return 0;
        if(order < 0) low = middle + 1; else high = middle;
    }
    if(size > UINT32_MAX || assets->count >= SIZE_MAX / sizeof(*assets->items) - 1) return 0;
    ZxAsset item = {strdup(name), malloc(size ? size : 1), (uint32_t)size};
    ZxAsset *items = item.name != NULL && item.data != NULL ?
        realloc(assets->items, (assets->count + 1) * sizeof(*items)) : NULL;
    if(items == NULL) { free(item.name); free(item.data); return 0; }
    if(size) memcpy(item.data, data, size);
    memmove(items + low + 1, items + low, (assets->count - low) * sizeof(*items));
    items[low] = item;
    assets->items = items;
    assets->count++;
    return 1;
}

static int
collect_path(ZxAssets *assets, const char *name, const char *path)
{
    struct stat stat;
    if(!compiler_bundle_AssetName(StringView(name, strlen(name))) ||
       FileStatusNoLinks(path, &stat) != 0) return 0;
    if(S_ISDIR(stat.st_mode)) {
        DIR *dir = opendir(path);
        if(dir == NULL) return 0;
        int ok = 1;
        struct dirent *entry;
        while(ok && (entry = readdir(dir)) != NULL) {
            if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char *child_name = join_path(name, entry->d_name);
            char *child_path = join_path(path, entry->d_name);
            ok = child_name != NULL && child_path != NULL &&
                collect_path(assets, child_name, child_path);
            free(child_name);
            free(child_path);
        }
        if(closedir(dir) != 0) ok = 0;
        return ok;
    }
    size_t size = 0;
    FILE *input = S_ISREG(stat.st_mode) ? fopen(path, "rb") : NULL;
    unsigned char *data = input != NULL ? read_rest(input, &size) : NULL;
    int ok = data != NULL && ZxAssetsAdd(assets, name, data, size);
    if(input != NULL && fclose(input) != 0) ok = 0;
    free(data);
    return ok;
}

int
ZxAssetsCollect(ZxAssets *assets, const char *spec)
{
    const char *separator = spec ? strchr(spec, '=') : NULL;
    if(separator == NULL || separator == spec || separator[1] == '\0') return 0;
    char *name = DuplicatePrefix(spec, (size_t)(separator - spec));
    int ok = name != NULL && collect_path(assets, name, separator + 1);
    free(name);
    return ok;
}
