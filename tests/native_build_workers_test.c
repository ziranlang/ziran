/* Exercise the production worker policy against this process's real CPU
 * mask. Changing this test's mask must never change its parent's mask. */
#include "../cmd/zir-c/zir_c_build.c"
#include <assert.h>

int main(void)
{
    int limit = c_worker_limit();
    assert(limit >= 1 && limit <= MAX_JOBS);
#ifdef __linux__
    cpu_set_t original, one;
    CPU_ZERO(&original);
    assert(sched_getaffinity(0, sizeof(original), &original) == 0);
    int available = CPU_COUNT(&original);
    assert(available > 0 && limit <= available);
    CPU_ZERO(&one);
    for(int cpu = 0; cpu < CPU_SETSIZE; cpu++)
        if(CPU_ISSET(cpu, &original)) { CPU_SET(cpu, &one); break; }
    assert(sched_setaffinity(0, sizeof(one), &one) == 0);
    int restricted = c_worker_limit();
    assert(sched_setaffinity(0, sizeof(original), &original) == 0);
    assert(restricted == 1);
    cpu_set_t restored;
    CPU_ZERO(&restored);
    assert(sched_getaffinity(0, sizeof(restored), &restored) == 0);
    assert(CPU_EQUAL(&original, &restored));
    printf("{\"available_cpus\":%d,\"worker_limit\":%d,\"restricted_workers\":%d,\"restored\":true}\n",
           available, limit, restricted);
#else
    printf("{\"worker_limit\":%d}\n", limit);
#endif
    return 0;
}
