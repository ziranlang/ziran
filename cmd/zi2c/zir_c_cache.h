#ifndef ZIR_C_CACHE_H
#define ZIR_C_CACHE_H
typedef struct CBuildCache CBuildCache;
/* NULL means ordinary uncached compilation, including unsupported compiler
 * wrappers/options. A cache failure must never make a valid build fail. */
CBuildCache *c_cache_open(char *const *arguments, int count, const char *output,
                        const char *executable);
/* The successful identity probe also verifies the driver's piped compile.
 * Unsupported/failed probes keep ordinary object compilation. */
int c_cache_piped(const CBuildCache *cache);
/* Recognize an actual option, excluding values such as -include -pipe. */
int c_cache_requested_pipe(char *const *arguments, int count);
int c_cache_hit(CBuildCache *cache, const char *object);
void c_cache_record(CBuildCache *cache, const char *object, const char *source);
void c_cache_close(CBuildCache *cache);
#endif
