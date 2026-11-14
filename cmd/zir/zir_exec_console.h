/*
 * zir_exec_console.h - the platform console services of revision-28
 * executable ZX: print and write_all in the ziran.platform module, bound to
 * standard output and standard error. Uses the generated engine headers
 * (cmd/compiler_runtime.zi), found with -iquote.
 */
#ifndef ZIR_EXEC_CONSOLE_H
#define ZIR_EXEC_CONSOLE_H

#include "compiler_exec.h"
#include "compiler_lowered_ir.h"

#include <stdint.h>

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

/* Binds every declared platform console service of the instance's program.
 * Other host capabilities stay unbound. */
LirExecProblem ZxBindConsole(LirExecInstance *instance, const LirProgram *program);

#endif
