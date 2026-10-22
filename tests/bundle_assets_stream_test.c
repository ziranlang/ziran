#include "zir_bundle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t seeks;
int __real_fseek(FILE *, long, int);
int __wrap_fseek(FILE *file, long offset, int origin)
{
    seeks++;
    return __real_fseek(file, offset, origin);
}

static FILE *input_bytes(const unsigned char *bytes, size_t size)
{
    FILE *file = tmpfile();
    assert(file && fwrite(bytes, 1, size, file) == size);
    rewind(file);
    return file;
}

int main(void)
{
    enum { count = 4096 };
    ZibAssets original = {0};
    original.items = calloc(count, sizeof(*original.items));
    assert(original.items);
    original.count = count;
    for(size_t i = 0; i < count; i++) {
        ZibAsset *item = &original.items[i];
        item->name = malloc(32);
        item->size = (uint32_t)(i % 257);
        item->data = malloc(item->size ? item->size : 1);
        assert(item->name && item->data);
        snprintf(item->name, 32, "files/%05zu.bin", i);
        for(size_t j = 0; j < item->size; j++)
            item->data[j] = (unsigned char)(i + j);
    }
    FILE *file = tmpfile();
    assert(file && ZibAssetsWrite(file, &original));
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    seeks = 0;
    ZibAssets loaded = {0};
    assert(ZibAssetsRead(file, &loaded));
    assert(seeks <= 2); /* Independent of the number of resources. */
    assert(loaded.count == original.count && fgetc(file) == EOF);
    for(size_t i = 0; i < count; i++) {
        assert(strcmp(loaded.items[i].name, original.items[i].name) == 0);
        assert(loaded.items[i].size == original.items[i].size);
        assert(memcmp(loaded.items[i].data, original.items[i].data,
                      original.items[i].size) == 0);
    }
    ZibAssetsFree(&loaded);
    ZibAssetsFree(&original);
    fclose(file);

    /* Every truncation of a section containing empty and binary resources
     * must reject cleanly, including partial sizes and partial names. */
    const unsigned char section[] = {
        2,0,0,0, 1,0,0,0, 'a', 0,0,0,0,
        1,0,0,0, 'b', 3,0,0,0, 0,255,1
    };
    for(size_t n = 0; n < sizeof(section); n++) {
        file = input_bytes(section, n);
        assert(!ZibAssetsRead(file, &loaded));
        ZibAssetsFree(&loaded);
        fclose(file);
    }
    file = input_bytes(section, sizeof(section));
    assert(ZibAssetsRead(file, &loaded) && loaded.count == 2);
    assert(loaded.items[0].size == 0 && loaded.items[0].data);
    assert(loaded.items[1].size == 3 && loaded.items[1].data[1] == 255);
    ZibAssetsFree(&loaded);
    fclose(file);

    unsigned char malformed[sizeof(section)];
    const size_t offsets[] = {0, 4, 9, 13, 18};
    for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        size_t offset = offsets[i];
        memcpy(malformed, section, sizeof(section));
        memset(malformed + offset, 255, 4);
        file = input_bytes(malformed, sizeof(malformed));
        assert(!ZibAssetsRead(file, &loaded));
        ZibAssetsFree(&loaded);
        fclose(file);
    }
    puts("Resource reads use constant seeks; empty/binary bytes, bounds and truncations passed");
    return 0;
}
