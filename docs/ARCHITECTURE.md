# Ziran architecture

This is the agreed target architecture for the Ziran and Kryon repositories.
It is not a claim that every part is implemented. Current gaps are tracked in
[Implementation status](IMPLEMENTATION_STATUS.md).

## Repository ownership

| Repository | Owns | Must not own |
| --- | --- | --- |
| Ziran | Language semantics, parser, checker, laws, standard library, `.zir`, native backends, `.zib` linker, verifier, loader, and generic execution runtime | Widgets, UI trees, styling, DOM policy, renderers, Kryon-specific names or implicit UI startup |
| Kryon | Importable `.zi` modules for widgets, layout, themes, interaction, accessibility, rendering, and platform adapters | Ziran syntax, compiler branches for widgets, or an application's terminal or product UI |

The dependency direction is **Kryon → Ziran**. A CLI, service, or library can
use Ziran without Kryon. Importing Kryon is an explicit source dependency.
Kryon's maintained implementation, including its widget and runtime logic, is
100% Ziran source. A platform adapter may call operating-system or third-party
APIs through declared FFI or host capabilities; those external libraries are
dependencies, not a second copy of Kryon's implementation. Generated C, C++,
or Go is compiler output, not Kryon source.

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
.zir modules ──native backend──> C, C++, native Go, Rust, or Python output
.zir modules ──link/verify──> .zib ──portable runtime + host capabilities──> process
```

Inside the compiler, names in that model are interned: an expression's or
statement's name and type, and a function's parameter list, are shared
immutable strings (`KeepName`, `KeepParameters`), so equal spellings are one
pointer. Parameter lists are split once into names and types
(`ParametersOf`), and `FindType` keeps its answers until something it reads
changes (`TypeLookupsChanged`; `make sanitize` checks each kept answer
against a fresh lookup).

Every backend consumes the same checked `.zir` model. Native output may bind
declared platform libraries. `.zib` contains portable executable content and
cannot depend on arbitrary target-specific C, C++, or Go source fragments.
Native and portable builds of the same supported program must agree on
observable language behavior.

The linker resolves explicit imports, includes the reachable library code,
and reports unresolved symbols or unavailable capabilities. A program with no
Kryon import has no Kryon code or graphics requirement. A program importing
Kryon links the Kryon modules it uses; graphics, input, audio, filesystem, and
other host access must be declared as capabilities when needed. The host
supplies those capabilities. A missing required capability is a reported
error, never silently ignored.

The portable runtime launches a non-graphical `.zib` without initializing a
window or display. Kryon decides how to use the host's graphical capabilities
through its imported `.zi` implementation. The compiler and loader do not
inspect widget names to make that decision.

## Product boundary

Kryon is a reusable library. Applications keep their own product logic and
UI in their repositories and import Kryon. Kapsule's terminal emulator, for
example, belongs to Kapsule; Kryon only gains small reusable primitives when
multiple applications need them. See [Migration](MIGRATION.md) for the single
breaking cutover and verification gates.
