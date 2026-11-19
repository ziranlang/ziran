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
#include "ziran_host.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

/* Standard output a program writes, gathered so that a run hands it to
 * stdio in one write rather than one per print. It goes out when the
 * console is flushed (at the end of a run, before any other host call and
 * when an instance closes), before anything is written to standard error,
 * and when it fills. A terminal gets every write at once, as before. */
typedef struct ZxConsole {
    size_t used;
    int direct;
    BundleConsoleWrite write;
    void *context;
    unsigned char data[4096];
} ZxConsole;

/* Binds every declared platform console service of the instance's program.
 * Other host capabilities stay unbound. `console` is zeroed here and must
 * outlive the instance; NULL writes every print straight to stdio. */
LirExecProblem ZxBindConsole(LirExecInstance *instance, const LirProgram *program, ZxConsole *console);

/* Hands what the console holds to standard output: 0 when that failed. */
int ZxConsoleFlush(ZxConsole *console);
/* An empty console; a terminal gets every write at once. */
void ZxConsoleOpen(ZxConsole *console);
/* Standard output through CONSOLE: 0 when that failed. */
int ZxConsoleWrite(ZxConsole *console, const void *bytes, size_t size);

/* What a failed run's problem means to the program's reader, or NULL. */
const char *ZxExecProblemText(LirExecProblem problem);

#endif
