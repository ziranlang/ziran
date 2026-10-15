#ifndef ZIR_C_CACHE_H
#define ZIR_C_CACHE_H
typedef struct CBuildCache CBuildCache;
/* NULL means ordinary uncached compilation, including unsupported compiler
 * wrappers/options. A cache failure must never make a valid build fail. */
CBuildCache *c_cache_open(char *const *arguments, int count, const char *output,
                        const char *executable);
int c_cache_hit(CBuildCache *cache, const char *object);
void c_cache_record(CBuildCache *cache, const char *object, const char *source);
void c_cache_close(CBuildCache *cache);
#endif
