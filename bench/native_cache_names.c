/* Build the same insertion workload with the current or a preserved cache
 * implementation. This is a container benchmark, not total compiler time. */
#include "../cmd/zir-c/zir_c_cache.c"
#include <assert.h>
#include <inttypes.h>

static uint64_t nanoseconds(void)
{
    struct timespec value;
    assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    int count = atoi(argv[1]), rounds = atoi(argv[2]);
    assert(count > 0 && count <= MAX_ITEMS && !(count & (count - 1)) && rounds > 0);
    char **input = calloc((size_t)count, sizeof(*input));
    assert(input);
    for(int i = 0; i < count; i++)
        assert(asprintf(&input[i], "/shared/project/include/long/common/path/component/header-%08d.h", (i * 1297) % count) >= 0);
    uint64_t total = 0, checksum = 0;
    for(int round = 0; round < rounds; round++) {
        Names names = {0};
        uint64_t started = nanoseconds();
        for(int i = 0; i < count; i++) assert(add_name(&names, input[i]));
        for(int i = count - 1; i >= 0; i--) assert(add_name(&names, input[i]));
        assert(names.count == count);
        checksum = UINT64_C(14695981039346656037);
        for(int i = 0; i < count; i++) {
            assert(!strcmp(names.items[i], input[i]));
            for(const unsigned char *p = (const unsigned char *)names.items[i]; *p; p++)
                checksum = (checksum ^ *p) * UINT64_C(1099511628211);
            checksum *= UINT64_C(1099511628211);
        }
        free_names(&names);
        total += nanoseconds() - started;
    }
    for(int i = 0; i < count; i++) free(input[i]);
    free(input);
    printf("{\"count\":%d,\"rounds\":%d,\"total_ns\":%" PRIu64 ",\"checksum\":%" PRIu64 "}\n",
           count, rounds, total, checksum);
    return 0;
}
