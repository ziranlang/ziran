#ifndef ZX_PTHREAD_H
#define ZX_PTHREAD_H
/* Bundles run `#parallel for` regions on the calling thread, as the portable
 * VM does: thread creation declines, and the region runs each chunk itself. */
typedef unsigned long pthread_t;
typedef struct pthread_attr_t pthread_attr_t;
int pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
                   void *(*start)(void *), void *argument);
int pthread_join(pthread_t thread, void **result);
#endif
