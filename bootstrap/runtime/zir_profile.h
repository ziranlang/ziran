#ifndef ZIR_PROFILE_H
#define ZIR_PROFILE_H
#include <stddef.h>
#include <stdint.h>
/* Opt-in process-local profiling. Phase times are inclusive, not additive. */
uint64_t ProfileStart(void);
void ProfileEnd(const char *phase, uint64_t started);
void ProfileAllocation(size_t bytes);
void ProfileCount(const char *counter);
#endif
