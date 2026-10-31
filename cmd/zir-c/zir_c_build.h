#ifndef ZIR_C_BUILD_H
#define ZIR_C_BUILD_H
/* Serialize writers of one generated output directory. Close the returned
 * descriptor after emission and native linking. */
int c_build_lock(const char *directory);
int c_compile_executable(const char *directory, const char *name, int threads);
/* Workers a build may run at once: at most half of the available CPUs,
 * bounded by free memory and ZIRAN_JOBS. */
int c_worker_limit(void);
#endif
