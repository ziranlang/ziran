#include "zir_files.h"
#include "zir_bundle.h"
#include "zir_diagnostic.h"
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int
asset_name(const char *name)
{
    if(name == NULL || *name == '\0') return 0;
    const char *part = name;
    for(const char *p = name;; p++) {
        if(*p == '\\' || *p == ':') return 0;
        if(*p == '/' || *p == '\0') {
            size_t n = (size_t)(p - part);
            if(n == 0 || (n == 1 && part[0] == '.') ||
               (n == 2 && part[0] == '.' && part[1] == '.')) return 0;
            if(*p == '\0') return 1;
            part = p + 1;
        }
    }
}

void
ZibAssetsFree(ZibAssets *assets)
{
    if(assets == NULL) return;
    for(size_t i = 0; i < assets->count; i++) {
        free(assets->items[i].name);
        free(assets->items[i].data);
    }
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
collect_path(ZibAssets *assets, const char *name, const char *path)
{
    struct stat stat;
    if(!asset_name(name) || FileStatusNoLinks(path, &stat) != 0) return 0;
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
    ZibAsset item = {0};
    item.name = strdup(name);
    item.size = (uint32_t)stat.st_size;
    item.data = malloc(item.size ? item.size : 1);
    FILE *input = fopen(path, "rb");
    int ok = item.name != NULL && item.data != NULL && input != NULL &&
        fread(item.data, 1, item.size, input) == item.size &&
        fgetc(input) == EOF && !ferror(input);
    if(input != NULL && fclose(input) != 0) ok = 0;
    ZibAsset *items = ok ? realloc(assets->items,
        (assets->count + 1) * sizeof(*items)) : NULL;
    if(items == NULL) { free(item.name); free(item.data); return 0; }
    assets->items = items;
    assets->items[assets->count++] = item;
    return 1;
}

static int
compare_asset(const void *left, const void *right)
{
    return strcmp(((const ZibAsset *)left)->name, ((const ZibAsset *)right)->name);
}

int
ZibAssetsCollect(ZibAssets *assets, const char *spec)
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

static int
write_size(FILE *out, uint32_t size)
{
    unsigned char bytes[4] = {(unsigned char)size, (unsigned char)(size >> 8),
        (unsigned char)(size >> 16), (unsigned char)(size >> 24)};
    return fwrite(bytes, 1, 4, out) == 4;
}

static int
read_size(FILE *in, uint32_t *size)
{
    unsigned char bytes[4];
    if(fread(bytes, 1, 4, in) != 4) return 0;
    *size = (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
        (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    return 1;
}

int
ZibAssetsWrite(FILE *out, const ZibAssets *assets)
{
    size_t count = assets != NULL ? assets->count : 0;
    if(count > UINT32_MAX || !write_size(out, (uint32_t)count)) return 0;
    for(size_t i = 0; i < count; i++) {
        const ZibAsset *item = &assets->items[i];
        size_t n = strlen(item->name);
        if(n > UINT32_MAX || !asset_name(item->name) ||
           (i > 0 && strcmp(assets->items[i - 1].name, item->name) >= 0) ||
           !write_size(out, (uint32_t)n) || fwrite(item->name, 1, n, out) != n ||
           !write_size(out, item->size) ||
           fwrite(item->data, 1, item->size, out) != item->size) return 0;
    }
    return 1;
}

/* Bound every allocation by the actual remaining bytes before allocating.
 * Section sizes use u32; no additional arbitrary file-size limit is imposed. */
static int
remaining(FILE *in, uint64_t wanted)
{
    long start = ftell(in);
    if(start < 0 || fseek(in, 0, SEEK_END) != 0) return 0;
    long end = ftell(in);
    if(fseek(in, start, SEEK_SET) != 0) return 0;
    return end >= start && (uint64_t)(end - start) >= wanted;
}

int
ZibAssetsRead(FILE *in, ZibAssets *assets)
{
    uint32_t count;
    if(!read_size(in, &count) || !remaining(in, (uint64_t)count * 9) ||
       (uint64_t)count > SIZE_MAX / sizeof(*assets->items)) return 0;
    if(count == 0) return 1;
    assets->items = calloc(count, sizeof(*assets->items));
    if(assets->items == NULL) return 0;
    for(uint32_t i = 0; i < count; i++) {
        ZibAsset *item = &assets->items[i];
        assets->count++;
        uint32_t n;
        if(!read_size(in, &n) || n == 0 || (uint64_t)n + 1 > SIZE_MAX ||
           !remaining(in, (uint64_t)n + 4)) return 0;
        item->name = malloc((size_t)n + 1);
        if(item->name == NULL || fread(item->name, 1, n, in) != n) return 0;
        item->name[n] = '\0';
        if(memchr(item->name, 0, n) != NULL || !asset_name(item->name) ||
           (i > 0 && strcmp(assets->items[i - 1].name, item->name) >= 0) ||
           !read_size(in, &item->size) || !remaining(in, item->size)) return 0;
        item->data = malloc(item->size ? item->size : 1);
        if(item->data == NULL || fread(item->data, 1, item->size, in) != item->size)
            return 0;
    }
    return 1;
}
