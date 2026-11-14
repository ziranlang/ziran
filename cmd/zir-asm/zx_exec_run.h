/*
 * zx_exec_run.h - run verified executable ZX on the portable instruction
 * engine, with the platform console services bound. zi2asm --run and
 * zi2zx run both use it.
 */
#ifndef ZX_EXEC_RUN_H
#define ZX_EXEC_RUN_H

#include "compiler_exec.h"
#include "compiler_lowered_ir.h"
#include "zir_exec_console.h"

#include <stddef.h>

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

/* Lowers the linked program's entry, optimizes it and writes revision-28
 * executable ZX with its host interface section: what zi2asm --emit zx
 * writes. Returns malloc'd bytes, or NULL after a diagnostic. */
struct ZirProgram;
unsigned char *ZxExecBuild(struct ZirProgram *program, const char *module, const char *function, size_t *size);

#endif
