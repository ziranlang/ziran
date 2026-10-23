#include "ziran_parallel.h"
#include <assert.h>

static pthread_t identities[4];
static int visits[4];

static void visit(int64_t low, int64_t high, void *unused)
{
    (void)unused;
    for(int64_t i = low; i <= high; ++i) {
        identities[i] = pthread_self();
        ++visits[i];
    }
}

int main(void)
{
    ziran_parallel_run(visit, 0, 3, NULL);
    for(int i = 0; i < 4; ++i)
        assert(visits[i] == 1);
    assert((!pthread_equal(identities[0], identities[3])) ==
           (ziran_parallel_threads() > 1));
    ziran_parallel_run(visit, 0, -1, NULL);
    ziran_parallel_run(visit, 0, 0, NULL);
    assert(visits[0] == 2 && visits[1] == 1);
    return 0;
}
