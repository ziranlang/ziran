#include "zir_memory.h"
#include "zir_files.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"
#include "compiler_bundle.h"
#include <dirent.h>
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
        if(assets->storage == NULL) free(assets->items[i].data);
    }
    free(assets->storage);
    free(assets->items);
    memset(assets, 0, sizeof(*assets));
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
    if(!S_ISREG(stat.st_mode) || stat.st_size < 0 ||
       (uint64_t)stat.st_size > UINT32_MAX ||
       assets->count >= SIZE_MAX / sizeof(*assets->items)) return 0;
    ZxAsset item = {0};
    item.name = strdup(name);
    item.size = (uint32_t)stat.st_size;
    item.data = malloc(item.size ? item.size : 1);
    FILE *input = fopen(path, "rb");
    int ok = item.name != NULL && item.data != NULL && input != NULL &&
        fread(item.data, 1, item.size, input) == item.size &&
        fgetc(input) == EOF && !ferror(input);
    if(input != NULL && fclose(input) != 0) ok = 0;
    ZxAsset *items = ok ? realloc(assets->items,
        (assets->count + 1) * sizeof(*items)) : NULL;
    if(items == NULL) { free(item.name); free(item.data); return 0; }
    assets->items = items;
    assets->items[assets->count++] = item;
    return 1;
}

static int
compare_asset(const void *left, const void *right)
{
    return strcmp(((const ZxAsset *)left)->name, ((const ZxAsset *)right)->name);
}

int
ZxAssetsCollect(ZxAssets *assets, const char *spec)
{
    const char *separator = spec ? strchr(spec, '=') : NULL;
    if(separator == NULL || separator == spec || separator[1] == '\0') return 0;
    size_t n = (size_t)(separator - spec);
    char *name = malloc(n + 1);
    if(name == NULL) return 0;
    memcpy(name, spec, n);
    name[n] = '\0';
    int ok = collect_path(assets, name, separator + 1);
    free(name);
    if(ok) {
        if(assets->count > 1)
            qsort(assets->items, assets->count, sizeof(*assets->items), compare_asset);
        for(size_t i = 1; i < assets->count; i++)
            if(!strcmp(assets->items[i - 1].name, assets->items[i].name)) return 0;
    }
    return ok;
}
