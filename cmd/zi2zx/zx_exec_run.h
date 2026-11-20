/*
 * zx_exec_run.h - run verified executable ZX on the portable instruction
 * engine for zi2zx run, with the platform console services bound, and write
 * it for zi2zx. zi2zx run opens it through the
 * embedding API (cmd/zir/zir_host_exec.c).
 */
#ifndef ZX_EXEC_RUN_H
#define ZX_EXEC_RUN_H

#include "compiler_zir_lower.h"
#include "compiler_exec.h"
#include "compiler_lowered_ir.h"
#include "compiler_opt.h"
#include "zir_exec_console.h"
#include "ziran_host.h"

#include <stddef.h>

/* Runs the program's entry. Returns the exit status: the low byte of an
 * integer result, 0 for void, 125 for a failed run. */
int ZxExecRunProgram(const LirProgram *program, LirExecLimits limits, int heap_report);

typedef struct ZxRunModuleImage {
    const char *name;
    size_t name_length;
    const char *path;
    unsigned char sha256[32];
} ZxRunModuleImage;

int ZxRunModuleImageParse(const char *spec, ZxRunModuleImage *image);

/* Resolve dependencies from the exact image map, or from the ordinary
 * roots when no map was supplied. A null PATH denotes a source run. */
int ZxExecRunBundle(const Bundle *bundle, const char *path, const char *const *roots,
                    size_t root_count, const ZxRunModuleImage *images, size_t image_count,
                    const BundleInstanceOptions *options,
                    long long *result, int *has_result);

struct ZirProgram;

typedef struct ZxImportRoot {
    const char *module;
    const char *function;
} ZxImportRoot;

typedef struct ZxModuleBuild {
    AssemblyExportRoot *exports;
    int export_count;
    ZxImportRoot *imports;
    int import_count;
    String linkage;
} ZxModuleBuild;

/* Source lowering shared by zi2zx's ZX, LIR and optimization-report views.
 * The owner retains the source-side interface spelling and, when requested,
 * the optimized program and counters. */
typedef struct ZxLowerOwner {
    AssemblyProgram *lowered;
    LirOptOwner optimized;
    LirOptResult optimization;
    LirProgram program;
    int has_optimization;
} ZxLowerOwner;

int ZxLowerBuild(struct ZirProgram *program, const char *module,
                 const char *function, const ZxModuleBuild *link,
                 int optimize, ZxLowerOwner *owner);
const LirProgram *ZxLowerView(const ZxLowerOwner *owner);
const LirOptResult *ZxLowerOptimization(const ZxLowerOwner *owner);
void ZxLowerFree(ZxLowerOwner *owner);

/* Lowers the linked program's entry, optimizes it and writes revision-28
 * executable ZX with its host interface section: what zi2zx
 * writes. `assets` is a slice of compiler_bundle Asset values in name order.
 * The entry must take no arguments and return an integer, bool or nothing.
 * A library supplies sorted export roots as its generated entry instead.
 * With `debug`, source texts and origins go to the optional debug
 * attachment. Returns malloc'd bytes, or NULL after a diagnostic. */
unsigned char *ZxExecBuild(struct ZirProgram *program, const char *module, const char *function,
                           const ZxModuleBuild *link, Slice assets, int debug, size_t *size);

#endif
