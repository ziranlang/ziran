# Ziran architecture

This is the agreed target architecture for the Ziran and Kryon repositories.
It is not a claim that every part is implemented. Current gaps are tracked in
[Implementation status](IMPLEMENTATION_STATUS.md).

## Repository ownership

| Repository | Owns | Must not own |
| --- | --- | --- |
| Ziran | Language semantics, parser, checker, laws, standard library, `.zir`, native backends, `.zx` linker, verifier, loader, and generic execution runtime | Widgets, UI trees, styling, DOM policy, renderers, Kryon-specific names or implicit UI startup |
| Kryon | Importable `.zi` modules for widgets, layout, themes, interaction, accessibility, rendering, and platform adapters | Ziran syntax, compiler branches for widgets, or an application's terminal or product UI |

The dependency direction is **Kryon → Ziran**. A CLI, service, or library can
use Ziran without Kryon. Importing Kryon is an explicit source dependency.
Kryon's maintained implementation, including its widget and runtime logic, is
100% Ziran source. A platform adapter may call operating-system or third-party
APIs through declared FFI or host capabilities; those external libraries are
dependencies, not a second copy of Kryon's implementation. Generated C or Go
is compiler output, not Kryon source.

## Language boundary

Modules, records, functions, imports, and procedure values are general
language features. Calls resolve against visible function signatures and
record types. Their semantics do not depend on the callee's spelling.
`Button`, `Text`, and `Image` mean nothing until imported or declared.
Kryon's intended `Image(ImageProps)` surface is an ordinary library API,
including when used for semantic images; its full rendering implementation
is still being migrated. A record argument uses `Function(Props.{value = 42})`;
`result: Result = Function(Props.{value = 42})` binds a non-void return value.

Ziran has no `#ui` function modifier, app/route syntax, widget fallback,
`#style` directive, DOM fields in its IR, or renderer-specific type checks.
Kryon defines its own composition, styling, navigation, and lifecycle APIs in
`.zi`. If a general language feature is needed for those APIs, it must work
for unrelated libraries too and pass the same checker and backend tests.

Language laws constrain semantics, types, evaluation order, IR validity, and
backend equivalence. Kryon owns its UI behavior and rendering laws.
[Language direction](LANGUAGE_DIRECTION.md) sets the additional requirements
for law-driven development, LLM tooling, parallel execution, and evolution.

## Compilation and execution

```text
.zi modules ──parse/check──> .zir modules
.zir modules ──link/lower/verify/optimize──> portable .zx instructions
.zx ──verify/read──> portable interpreter ──> process
.zx ──verify/read/direct target lowering──> platform binary
.zir modules ──C/Go generation──> C or Go ──downstream toolchain──> platform binary
```

The direct-target branch is the native pipeline: Ziran lowers the verified ZX
instructions directly to machine code, applies the target ABI and platform
veneers, and writes the platform binary. It is an experimental, incomplete
path, not a description of support on every target. Generated C and generated
Go can also become binaries, but those are generated-code routes through their
own compilers and linkers; they are not native-backend fallback paths.

This is implemented in the experimental `zi2asm` route: source-native output
also serializes and reads ZX first. Default semantic `ziran`/`zi2zx` bundles
remain separate until cutover. `zir_zx_wasm` still lowers semantic `ZirProgram`
through `zir_wasm_lower`, not revision-28 ZX; the shared instruction-to-Wasm
route remains a hard gap. Native source maps currently lose source origins at
the stripped ZX boundary, while source LIR inspection retains them.

The first implemented direct console veneer is the opt-in Linux x86-64 static
ELF path. It owns `_start`, calls Linux `write` syscall 1 through a retrying
`write_all`, and terminates with `exit_group` syscall 231. It uses no libc,
CRT, or dynamic linker. Other OS/CPU service rows and floating-point print
formatting remain named gaps rather than alternate-engine requests.

Inside the compiler, names in that model are interned: an expression's or
statement's name and type, and a function's parameter list, are shared
immutable strings (`KeepName`, `KeepParameters`), so equal spellings within a
compilation context are one pointer. Parameter lists are split once into names and types
(`ParametersOf`). Type and procedure resolution keep entry-scope answers,
including misses and ambiguity, until declarations or imports change
(`SymbolLookupsChanged`). Lookup file spellings remain part of each key;
recursive import walks retain separate cycle state. `make sanitize` checks
each kept answer against a fresh lookup.

Record bodies share immutable parsed field snapshots. Field lookup uses an
index over that snapshot, and ownership checking can reuse the fact that every
field is a built-in value or pointer. This shortcut never assumes facts about
named types, arrays, or generic applications: those still resolve in their
owning scope. Rewriting a body selects a new snapshot, so copied or relocated
record declarations cannot retain stale field names or ownership facts.

Every backend consumes the same `.zir` model. The ZX linker produces
one verified portable instruction program for the interpreter and direct
target lowering. `.zx` cannot depend on arbitrary target-specific C or Go
source fragments. C and Go exporters remain separate consumers of ZIR.
Every supported route for the same program must agree on observable language
behavior.

Host capabilities are portable typed requirements in the linked program.
Platform bindings satisfy those requirements for one host. Direct target
lowering separately owns CPU instructions, calling conventions, object and
executable formats, and small ABI veneers for services such as process entry,
standard-stream writes, and process exit. A platform binding never makes a
missing instruction lowering or ABI veneer implemented, and a CPU backend does
not silently turn an OS service into a portable capability.

Semantic checking sets `ZirFunction.checked` only from language validity.
Native symbol planning runs afterward, and body eligibility is a separate
emitter query. An emitter's inability to represent a valid function does not
turn that function into unchecked source. The portable verifier remains the
authority for executable `.zx` support.

Parameter declarations have one immutable parsed view, including binding
names, types, namespace modifiers, and defaults. Record fields likewise use
the shared field snapshots. Backends consume these views; source spellings
remain available for diagnostics, template rewriting, and serialization.

Compilation contexts own interned text, source paths, symbol answers,
declaration snapshots, expression caches, and reusable scratch buffers.
Source and artifact readers create a context when the caller has not selected
one. Programs and linked copies retain the contexts whose bytes they borrow;
closing the original program therefore cannot invalidate a linked copy.
Source spans keep their original context independently of the module's lookup
context. Context pointers and cache identities are never serialized.

Public source parsing is a recoverable operation. Syntax and source-read
errors return `NULL` after reporting the ordinary structured diagnostic.
An operation-local allocation transaction releases partial trees, temporary
buffers, and open streams after an error, and restores the enclosing parser
state. Transactions nest across import callbacks and are local to each thread.
Published immutable caches remain owned by their context. See
[Compiler embedding](COMPILER_API.md) for lifetime and concurrency rules.

The linker resolves explicit imports, includes the reachable library code,
and reports unresolved symbols or unavailable capabilities. A program with no
Kryon import has no Kryon code or graphics requirement. A program importing
Kryon links the Kryon modules it uses; graphics, input, audio, filesystem, and
other host access must be declared as capabilities when needed. The host
supplies those capabilities. A missing required capability is a reported
error, never silently ignored.

The portable runtime launches a non-graphical `.zx` without initializing a
window or display. Kryon decides how to use the host's graphical capabilities
through its imported `.zi` implementation. The compiler and loader do not
inspect widget names to make that decision.

## Product boundary

Kryon is a reusable library. Applications keep their own product logic and
UI in their repositories and import Kryon. Kapsule's terminal emulator, for
example, belongs to Kapsule; Kryon only gains small reusable primitives when
multiple applications need them. See [Migration](MIGRATION.md) for the single
breaking cutover and verification gates.
