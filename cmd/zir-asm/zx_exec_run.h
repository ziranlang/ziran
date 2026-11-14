/*
 * zx_exec_run.h - run verified executable ZX on the portable instruction
 * engine for zi2asm --run, with the platform console services bound, and
 * write it for zi2asm and zi2zx bundle. zi2zx run opens it through the
 * embedding API (cmd/zir/zir_host_exec.c).
 */
#ifndef ZX_EXEC_RUN_H
#define ZX_EXEC_RUN_H

#include "compiler_exec.h"
#include "compiler_lowered_ir.h"
#include "zir_exec_console.h"

#include <stddef.h>

/* Runs the program's entry. Returns the exit status: the low byte of an
 * integer result, 0 for void, 125 for a failed run. */
int ZxExecRunProgram(const LirProgram *program, LirExecLimits limits, int heap_report);

/* Lowers the linked program's entry, optimizes it and writes revision-28
 * executable ZX with its host interface section: what zi2asm --emit zx
 * writes. `assets` is a slice of compiler_bundle Asset values in name order.
 * The entry must take no arguments and return an integer, bool or nothing.
 * Returns malloc'd bytes, or NULL after a diagnostic. */
struct ZirProgram;
unsigned char *ZxExecBuild(struct ZirProgram *program, const char *module, const char *function, Slice assets,
                           size_t *size);

#endif
