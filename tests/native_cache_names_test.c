#include "../cmd/zir-c/zir_c_cache.c"
#include <assert.h>

static void path_name(char *text, size_t size, int value)
{
    assert(snprintf(text, size, "/shared/include/long/common/prefix/header-%08d.h", value) < (int)size);
}

static void ordered_names(int count, int limit)
{
    Names names = {0};
    char text[256];
    for(int i = 0; i < count; i++) {
        int value = (i * 8191) % count;
        path_name(text, sizeof(text), value);
        assert(add_name(&names, text));
    }
    assert(names.count == count);
    for(int i = 0; i < count; i++) {
        path_name(text, sizeof(text), (i * 8191) % count);
        assert(!strcmp(names.items[i], text));
        assert(add_name(&names, text));
    }
    assert(names.count == count);
    sort_names(&names);
    for(int i = 0; i < count; i++) {
        path_name(text, sizeof(text), i);
        assert(!strcmp(names.items[i], text));
        assert(add_name(&names, text));
    }
    assert(names.count == count);
    path_name(text, sizeof(text), count);
    assert(add_name(&names, text) == !limit);
    assert(names.count == count + !limit);
    free_names(&names);
}

int main(void)
{
    ordered_names(257, 0);
    ordered_names(MAX_ITEMS, 1);
    Names collisions = {0};
    char text[256];
    for(int value = 0; value < 1000000 && collisions.count < 96; value++) {
        path_name(text, sizeof(text), value);
        if((memo_slot(text, 0) & 255) == 0) assert(add_name(&collisions, text));
    }
    assert(collisions.count == 96);
    for(int i = 95; i >= 0; i--) assert(add_name(&collisions, collisions.items[i]));
    assert(collisions.count == 96);
    free_names(&collisions);
    Names exact = {0};
    for(int i = 0; i < 40; i++) {
        path_name(text, sizeof(text), i);
        assert(add_name(&exact, text));
    }
    assert(add_name(&exact, "Header.h") && add_name(&exact, "header.h"));
    assert(add_name(&exact, "caf\xc3\xa9.h") && add_name(&exact, "caf\xc3\xa9.h"));
    assert(exact.count == 43);
    free_names(&exact);
    for(int value = 0; value < 1024; value++) {
        path_name(text, sizeof(text), value);
        for(int kind = 0; kind < 3; kind++) {
            uint64_t reference = UINT64_C(14695981039346656037) ^ (unsigned)kind;
            for(const unsigned char *p = (const unsigned char *)text; *p; p++)
                reference = (reference ^ *p) * UINT64_C(1099511628211);
            assert(memo_slot(text, kind) == (reference & (MEMO_SLOTS - 1)));
        }
    }
    puts("Cache names preserve insertion/sorted order, duplicates, collisions, limits and exact memo buckets");
    return 0;
}
