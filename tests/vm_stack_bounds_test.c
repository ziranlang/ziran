#include "ziran_host.h"
#include <assert.h>
#include <pthread.h>
#include <sys/resource.h>

static _Thread_local unsigned queries;
int __real_pthread_getattr_np(pthread_t thread, pthread_attr_t *attributes);
int __wrap_pthread_getattr_np(pthread_t thread, pthread_attr_t *attributes)
{
    queries++;
    return __real_pthread_getattr_np(thread, attributes);
}

typedef struct Run {
    BundleInstance *instance;
    long long first;
} Run;

static void *run_worker(void *context)
{
    Run *run = context;
    for(int i = 0; i < 20; i++) {
        long long value = 0;
        int present = 0;
        assert(BundleInstanceRun(run->instance, &value, &present));
        assert(present && value == run->first + i);
    }
    assert(queries == 1);
    return NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    BundleInstance *instance = BundleInstantiatePortable(bundle, NULL, 0);
    assert(instance != NULL);
    Run run = {instance, 1};
    run_worker(&run);
    for(int pass = 0; pass < 2; pass++) {
        pthread_t worker;
        run.first += 20;
        assert(pthread_create(&worker, NULL, run_worker, &run) == 0);
        assert(pthread_join(worker, NULL) == 0);
    }
    run.first += 20;
    run_worker(&run);
    /* A changed process stack limit invalidates this thread's old bounds.
     * Other workers have already joined, so this change stays in the test. */
    struct rlimit original;
    assert(getrlimit(RLIMIT_STACK, &original) == 0);
    if(original.rlim_cur != RLIM_INFINITY && original.rlim_cur > 4 * 1024 * 1024) {
        struct rlimit smaller = original;
        smaller.rlim_cur -= 4096;
        assert(setrlimit(RLIMIT_STACK, &smaller) == 0);
        long long value = 0;
        int present = 0;
        assert(BundleInstanceRun(instance, &value, &present) && present && value == 81);
        assert(queries == 2);
        assert(setrlimit(RLIMIT_STACK, &original) == 0);
        assert(BundleInstanceRun(instance, &value, &present) && present && value == 82);
        assert(queries == 3);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
