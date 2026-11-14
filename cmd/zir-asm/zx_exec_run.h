/*
 * zx_exec_run.h - run verified executable ZX on the portable instruction
 * engine, with the platform console services bound. zi2asm --run and
 * zi2zx run both use it.
 */
#ifndef ZX_EXEC_RUN_H
#define ZX_EXEC_RUN_H

#include "compiler_exec.h"
#include "compiler_lowered_ir.h"

#include <stddef.h>

enum {
    ZX_SERVICE_NONE,
    ZX_SERVICE_PRINT_BYTES,
    ZX_SERVICE_PRINT_SIGNED,
    ZX_SERVICE_PRINT_UNSIGNED,
    ZX_SERVICE_PRINT_BOOL,
    ZX_SERVICE_PRINT_FLOAT32,
    ZX_SERVICE_PRINT_FLOAT64
};

/* The platform console service a host function of the program stands for:
 * one of the ZX_SERVICE_ names in the ziran.platform module, else none. */
int ZxPlatformService(const LirProgram *program, int64_t function);

/* Limits for a command-line run, which like a native build continues until
 * the entry returns: no work or fuel budget, deep recursion. */
LirExecLimits ZxExecCommandLimits(void);

/* Runs the program's entry. Returns the exit status: the low byte of an
 * integer result, 0 for void, 125 for a failed run. With print_result a
 * non-void result is printed on its own line instead, the status is 0, and
 * a failed run gives 1, as zi2zx run treats a bundle. */
int ZxExecRunProgram(const LirProgram *program, LirExecLimits limits, int heap_report, int print_result);

/* Runs the bytes of a file whose header names revision 28 and returns 1
 * with *status set, or -1 with a diagnostic when its content is invalid.
 * Returns 0 for any other file, which the caller gives to the bundle reader. */
int ZxExecRunBytes(const unsigned char *bytes, size_t size, LirExecLimits limits,
                   int print_result, int *status);

#endif
