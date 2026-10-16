#include "zir_c_cache.h"
#include <assert.h>

int main(void)
{
    char *values[] = {"cc", "-Xlinker", "-pipe", "-B", "-pipe"};
    assert(!c_cache_requested_pipe(values, 5));
    char *actual[] = {"cc", "-Xlinker", "-pipe", "-B", "tools", "-pipe"};
    assert(c_cache_requested_pipe(actual, 6));
    char *joined[] = {"cc", "-Wl,-pipe", "-B-pipe", "-fuse-ld=lld"};
    assert(!c_cache_requested_pipe(joined, 4));
    return 0;
}
