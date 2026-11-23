# Bootstrap runtime boundary

The generated compiler modules under `bootstrap/compiler_*` contain every
parser, checker, lowering, emission, cache, package and diagnostic rule. The
C in this directory is only what generated code cannot express: the process
boundary that hosts it.

## Permitted functions

`ziran_bootstrap_runtime.h` declares the whole boundary surface. A C function
defined here must be declared there; `scripts/bootstrap_fixed_point.py
validate_runtime_boundary` rejects any other non-static definition. The
permitted categories are:

- process and argument startup, and the call into the generated `zi2c` root;
- checked allocation, reallocation and release;
- panic and recovery transfer (`setjmp`/`longjmp` bridges);
- file, environment, process, time, thread and dynamic-library callbacks.

## Forbidden

No parser, type, checker, lowering, emission, cache policy, package policy or
diagnostic formatting may live here. When a callback starts making a compiler
decision, the decision moves into a generated Ziran module and the callback
keeps only the OS call.

## Current state

The host files still live under `cmd/zir` (`zir_memory.c`, `zir_services.c`,
`zir_atomics.c`, `zir_diagnostic_host.c`, `zir_command_host.c`,
`zir_edit_host.c`, `zir_files_host.c`, `zir_sha256_host.c`,
`zir_native_memory.c`, `zir_persistence_host.c`, `zir_host_native*.c`,
`zir_exec_i386.c`, `zir_zx_wasm_install.c`, `zir_zx_wasm_web.c`) and the
parser recovery bridge is `cmd/zir/zir_parse.c`. They move here, and the
Makefile's compiler source lists shrink to generated modules plus this
directory, when the parser recovery port lands. Until then the fixed-point
proof covers the generated roots and this boundary check stays opt-in
(`--runtime bootstrap/runtime`).
