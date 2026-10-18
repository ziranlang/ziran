# Compiler embedding

`parse_file`, `parse_file_with_imports_defined`, and `parse_source_text` return
a program on success and `NULL` on a source-read or syntax error. Diagnostics
use the existing text or JSON format. A bad source file does not exit the host
process. Parser temporaries, open input streams, and partial trees are released
before the call returns; a subsequent parse can use the same thread normally.

Free successful results with `ProgramFree`. Source parsing and `ProgramRead`
automatically create a compilation context when no context is selected.
The result owns that context. Loading a module graph with `ProgramsLoad`
instead gives the graph one context, released by `ProgramsFree`.

To construct raw IR or keep multiple programs in one compilation region,
select an explicit context:

```c
CompilerContext *context = CompilerContextOpen();
if(context == NULL) return 0;
CompilerContext *previous = CompilerContextSelect(context);
ZirProgram *program = parse_source_text("example.zi",
    "Answer :: () -> s32 { return 42 }\n");
CompilerContextSelect(previous);
int valid = program != NULL && CheckPrograms(&program, 1);
ProgramFree(program);
if(!CompilerContextClose(context)) return 0;
return valid;
```

`CompilerContextClose` refuses a selected context or one retained by live
programs. Restore the previous selection and free the programs first. Strings,
parameter views, field cursors, and source spans borrowed from a context must
not be used after that context is destroyed. Copies created by the linker
retain their dependencies automatically. Internal code that copies an IR
graph must use `ProgramInheritContexts` and assign copied modules the new
graph's lookup context.

Public checking, lowering, and VM entry points select their program's context
for the operation and restore the caller's selection afterward. Raw IR
construction and internal compiler helpers use the explicitly selected
context. Unscoped low-level construction uses the shared process context;
long-running hosts should select an explicit region instead.

Each mutable compilation graph has one compiling owner. Separate contexts can
parse and check on separate threads. Native backend configuration still has
process state, so embedding hosts must serialize native backend operations.
Independent VM instances may execute an immutable program concurrently while
that program and its contexts remain alive.
Closing a context or mutating or freeing its graph during those operations is
unsupported. Shared immutable snapshots and scratch-pool publication are
synchronized; scratch buffers have one borrower at a time.

`ZirFunction.checked` records semantic validity. Native symbol planning and
body eligibility are separate operations. Native-only functions can be valid
even when the portable verifier rejects them. The source/saved-IR checks and
the portable verifier must still run before executable use.
