/* Atomic operations on the scalar storage of the Ziran compiler. */
#include <stddef.h>

int S15FlagTake(void *flag) { return __atomic_test_and_set(flag, __ATOMIC_ACQUIRE); }
void S15FlagClear(void *flag) { __atomic_clear(flag, __ATOMIC_RELEASE); }
int S15IntExchange(void *slot, int value) { return __atomic_exchange_n((int *)slot, value, __ATOMIC_ACQUIRE); }
void S15IntStore(void *slot, int value) { __atomic_store_n((int *)slot, value, __ATOMIC_RELEASE); }
int S15IntLoad(void *slot) { return __atomic_load_n((int *)slot, __ATOMIC_SEQ_CST); }
int S15IntAdd(void *slot, int value) { return __atomic_fetch_add((int *)slot, value, __ATOMIC_SEQ_CST); }
int S15IntCompare(void *slot, int *expected, int value)
{
    return __atomic_compare_exchange_n((int *)slot, expected, value, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
size_t S15SizeAdd(void *slot, size_t value) { return __atomic_fetch_add((size_t *)slot, value, __ATOMIC_SEQ_CST); }
size_t S15SizeLoad(void *slot) { return __atomic_load_n((size_t *)slot, __ATOMIC_SEQ_CST); }
