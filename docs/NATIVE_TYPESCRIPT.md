# Native TypeScript and TSX

`.ts` and `.tsx` are source frontends of the Ziran toolchain. They construct
structured expressions and declarations in the common IR. The common checker,
borrow checker, saved `.zir` format and C, Go, portable ZX, native ZX and Wasm
paths apply to them. There is no generated Ziran screen source, JavaScript
engine, npm module resolver or React runtime. The experimental `tsx2zi`
command has been removed.

The Kryon migration is still in progress. Owned and scoped capturing callables
use the common checked IR. The complete component authoring contract, keyed
mounting integration and UI package migration remain open work.

## Commands and imports

Use the ordinary commands, for example:

```sh
ziran check --root src src/main.tsx
ziran ir --root src -o build/ir src/main.tsx
ziran build --target=c --exe --entry main:main --root src -o build/c src/main.tsx
ziran bundle --entry main:main --root src -o build/main.zx src/main.tsx
```

Project commands discover `.zi`, `.ts` and `.tsx` together. Module names must be
unique across source extensions. Package exports and lockfiles resolve imports
in the same way as Ziran imports; an import of a Ziran module reads its actual
checked declarations. Do not write a duplicate TypeScript interface for a
Ziran record or action.

Exported native globals retain their declaration's record and callable types,
including pointers, arrays and generic storage, across different import aliases.
Reading or updating that storage uses the same native ownership checks.

Exported native foreign procedures use the same checked calls as Ziran actions.
Namespace imports, named aliases and native re-exports preserve their signatures,
ownership annotations and external ABI. Host capabilities retain the declaration
module's identity and normal `--bind`/`--bind-host` behavior through saved IR.
For a C build with a native host, an unbound capability calls the original
declaration's linker symbol; importing it does not rename the native ABI.

```tsx
import * as Model from "model";
import { Half } from "arithmetic";

interface LabelProps { value: number; }
function Label(props: LabelProps): number { return Half(props.value); }

export function main(): s32 {
    const snapshot: Model.Snapshot = Model.Read();
    return s32(Label({value: snapshot.value}));
}
```

Imports have module scope regardless of declaration order. Namespace imports
and named imports with `as` aliases are supported. Default imports,
re-exports and JavaScript packages are unsupported.

Named functions support typed parameter defaults, evaluated only when omitted.
Initializers use declaration-module scope and can call checked native procedures;
they cannot read the function's parameters. This follows native Ziran defaults.
The native default `origin: Source_Code_Location = nativeCallerLocation()` receives
the authored caller's filename, line and column, including ordinary JSX calls.
Two components on the same source line have distinct columns. Defaults on
arrow functions and procedure type signatures are currently rejected.

## Types and values

- `number` is native IEEE 754 binary64 (`float64`). Integer-spelled ordinary
  numeric literals are also binary64; `5 / 2` is `2.5`. No integer type is
  silently substituted for TypeScript `number`.
- `boolean` is native `bool`. `string` is an immutable borrowed UTF-8 byte view,
  with native `.count`; it is not a JavaScript UTF-16 string object.
- `NativeRef<Native.Record>` names an explicit checked native reference type.
  `nativeRef(place)` borrows an address, `nativeDeref(reference)` reads its
  pointee and `nativeSet(reference, value)` writes it through the common checks.
  References follow native
  lifetime rules and cannot be captured by an owned callback. These are native
  intrinsics with TypeScript syntax; no runtime wrapper or second representation
  is allocated. Ziran modules retain their ordinary pointer syntax.
- `NativeSlice<T>` names a checked native borrowed slice, including slices
  returned by Ziran actions and `nativeSlice(array)`. It supports native
  indexing with explicit native integers and `.count`, with the same borrowing
  rules as Ziran `[]T`; for example, `view[s64(0)]` reads its first element.
  It is distinct from a JavaScript array object and exposes no array methods.
- `s8` through `s64`, `u8` through `u64`, `isize`, `usize`, `float32` and
  `float64` name explicit native scalar types. Conversions use `s32(value)`,
  `u64(value)` and the other native type names. These follow the existing
  Ziran explicit-cast semantics; they are not JavaScript coercions.
- An integer literal ending in `n` carries exact native integer bits. Use it
  at an explicit native boundary, such as `u64(18446744073709551615n)`, to
  preserve a wide identifier without going through binary64. This suffix does
  not provide a dynamically sized JavaScript BigInt runtime.
- Interfaces declare native records and require every declared field in a
  record literal or JSX props. Imported Ziran records, procedure types and enums retain
  their checked identity. Records use value semantics and native ownership.
  Keep an owned native result in an explicitly typed binding, for example
  `const text: Text = TextCopy("label")` with `Text` and `TextCopy` imported
  from `std/vec`. The existing native convention detaches a `Text` call result
  to a program-lifetime string when the binding has no owned `Text` context.
  Use `TextView(text)` to borrow an explicitly owned binding during its lifetime;
  returning that view from the owning scope is rejected at the authored source.
- A homogeneous tuple such as `[string, string]` is a fixed native array.
  `T[]` names a borrowed native slice. Slice literals need backing storage;
  `nativeSlice(values)` borrows the whole backing array, vector, string or
  slice. `nativeSlice(values, s32(start), s32(end))` borrows a checked subrange
  with explicit native integer bounds. Omitting the end borrows through the
  end of the backing storage. These views do not copy their
  elements and cannot outlive their backing storage. General owned-array and
  JSX child-range construction is still open work.
- Native generic records use `Vec<number>` or `Domain.Box<number>` at type
  positions and retain the actual checked Ziran declaration and specialization.
  An `interface Holder<T> { value: T; }` declares a native generic record.
  Nested type arguments are supported. Generic functions, constraints and
  defaults are not implemented in the TSX frontend yet.
- `any`, `unknown`, `never`, `undefined` and `null` are rejected. Optional values,
  errors and resources must have explicit native declarations. The frontend
  does not invent a nullable or serialized representation for them.

Use `===` and `!==`. Equality is checked for compatible native operands, with
native value comparison for supported operands and IEEE behavior for floats.
Coercive `==` and `!=` are rejected. Arithmetic, comparisons, boolean operators,
conditional expressions, record/array literals, calls and indexing use the
common typed expression graph. Unsupported syntax produces a source diagnostic.
Bitwise `&`, `|`, `^`, `~`, `<<` and `>>` require native integer operands and
preserve their checked width and signedness; `number` operands are rejected.
They use TypeScript operator precedence, without JavaScript's implicit 32-bit
number coercion. Native `u64` flags and identifiers retain all 64 bits.

`nativeAt(collection, index)` reads an element with a native integer index,
including a wide index. On a native UTF-8 string it returns a `u8` byte.
`nativeSet(collection, index, value)` writes an array, vector or slice element;
borrowed string bytes remain read-only. `nativeSet` is a statement, so a callback
which writes uses a block body. The same common assignment and borrow checks
apply as for Ziran storage. These operations need no wrappers or extra copies.

Imported module bindings are read-only, as in TypeScript modules. Prefer exported
native actions for domain updates. A mutable native global can also be updated
through `nativeSet(nativeRef(Model.scene), value)`; the common checker verifies
that the referenced storage is mutable and that the write respects its lifetime.

`nativeInit<T>({...})` constructs a checked native record. Fields omitted from
a native zero-initialized record receive their native defaults; the result
has the full record type, so reading an omitted numeric field returns zero.
For example, `nativeInit<Rectangle>({width: 100, height: 40})` initializes `x`
and `y` to zero. Declared required fields remain required, including fields
of TS interfaces. Unknown fields, wrong field types and unsafe ownership
transfers are rejected by the common record checker. This operation uses
ordinary native record storage and introduces no allocation or wrapper.

`BuilderPrint(nativeRef(builder), "Count: %", count)` appends formatted values
to a native `Vec<u8>` through the same formatting operations as Ziran. Each `%`
takes one argument and `%%` writes a percent sign. The format must be a literal;
C/Go conversion suffixes and mismatched argument counts are rejected at the
authored call. Integer formatting writes directly into the builder without an
intermediate owned string. Keep the finished result as a native `Text` for a
bounded owned lifetime, and use `TextView` to borrow its UTF-8 bytes. Finishing
into `string` instead detaches the bytes for the rest of the program, following
the native builder contract.

`NativeRef`, `NativeSlice`, `nativeInit`, `nativeRef`, `nativeDeref`, `nativeSlice`, `nativeAt`, `nativeSet`
and `nativeCallerLocation` are reserved intrinsic names. `nativeCallerLocation()` is supported only as a
named function parameter default. The old `*Type`, `&place`, `*pointer`, colon
slice and `#caller_location` source extensions are not TypeScript syntax and
are rejected in `.ts` and `.tsx` modules.

## Functions, statements and JSX

Functions have typed parameters and result types. `export` controls public
visibility. Local `const` and `let` bindings require an initializer. `const`
prevents rebinding; it does not freeze the fields of a record. Blocks, `if` /
`else`, `while`, C-style `for` with explicit assignment steps, `break`,
`continue`, assignment and local compound assignment are supported. Loop steps
execute before a matching `continue` and do not execute after `break`.
Loop identities and continue steps belong to their function; nested callbacks
start a separate loop scope and cannot target an enclosing function's loop.
`for (const item of values)` and `for (let item of values)` iterate native
arrays, slices, strings and vectors. The collection expression runs once.
Existing storage is borrowed for the loop; a fresh owned result is kept in
the loop's scope and released on exit. Mutating or moving a borrowed vector
is rejected. The iteration binding holds an element value; changing a `let`
binding does not replace the element in its collection. Array literals
currently require a typed backing binding before iteration.

Module `let` bindings require an explicit native type and initializer. They
use native global storage and run their initializers once, in source order,
after imported modules initialize. `export` makes the slot visible to other
modules; other slots are private. Module `const` currently supports only
explicit `string` bindings with literal initializers; other types are rejected
until typed readonly storage is implemented in the common IR.

An import cycle through modules without runtime initializers does not create a
startup dependency on itself. A native model can import a TSX view which imports
the model's types and actions, while the model still initializes once. Cycles
between distinct modules with runtime initializers remain startup errors.

Native procedure signatures can appear as field/parameter types or named aliases:

```tsx
type Transform = (value: number) => number;
function Calculate(): number {
    const half: Transform = (value: number): number => value / 2;
    return half(10);
}
```

Noncapturing arrows require typed parameters
and an explicit result type, and accept either an expression or a block body.
They use ordinary native procedure values, including calls through imported
Ziran procedure parameters. Bind to the imported procedure type at that
boundary instead of duplicating it in a local alias; named native procedure
types retain their declared identity. Signatures are checked by the common checker.
Capturing arrows use `Owned<Signature>` from `std/callable`:

```tsx
import { Owned } from "std/callable";
type Action = (value: number) => number;
function Offset(delta: number): Owned<Action> {
    return (value: number): number => value + delta;
}
```

The callable owns an immutable snapshot of its captures. Scalars, enums and
records containing only ownership-safe fields are accepted; owned Text, Vec
and other owned callables move into that snapshot. Strings, slices, pointers
and records containing borrowed fields are rejected. Reading a view of owned
capture storage during invocation is valid; returning that view is rejected.
Move and scope cleanup rules apply to the callable, including record fields,
arguments, returns and replacement. One signature accepts different generated
capture record types. The `__tsx_` identifier prefix is reserved for generated
IR declarations.
Invoking an empty callable traps; dropping an empty or moved callable is harmless.

`Scoped<Signature>` borrows captured lexical places for the calling scope. It
uses typed stack storage and a checked invoke trampoline, with no heap-owned
capture or release slot. The callback reads the captured places when invoked;
it does not move their owned values. A callee may invoke the callback or pass
it to another checked Scoped parameter. Transient props records and local
bindings preserve its lexical lifetime. Returning it, retaining it in global,
reference or owned collection storage, taking its address, or moving an owned
capture is rejected. Owned callbacks cannot capture Scoped values. Foreign
procedures cannot take Scoped parameters because their bodies have no checked
no-escape guarantee.

These restrictions follow nested records, arrays and collection element types
without a nesting cutoff. Recursive declarations are checked by their identity;
adding another record layer cannot make a borrowed callback safe to retain.

Classes, promises, async functions and type unions are not implemented yet. Generic record applications retain their declaring module
and file scope: a local `Box<T>` and an imported `Domain.Box<T>` remain
distinct, including inside nested applications. Import aliases for the same
declaration share its identity.

JSX calls an ordinary typed component value with one props record. The compiler
knows no widget names and does not choose a UI library. Qualified component
names, nested components, expression props and children use ordinary checked
calls. The library defines the props and result types. Unknown and incorrectly typed fields are checked against those declarations.
TS interfaces require every declared field. Native Ziran records retain their
declared initialization rules for omitted fields. Duplicate props and mismatched
closing tags are errors.
Empty paired tags and indentation-only content do not add a `children` property.

Use JSX statements for composition. When application logic consumes a component's
immediate result, call that same imported component with its typed props record,
for example `if (Action({model: model})) { ... }`. This preserves its declared
native result without assigning that result to the stock TypeScript editor's
`JSX.Element` type. Both forms invoke the same ordinary typed component value.

Inline JSX text preserves spaces around expressions. Multiline text removes
line indentation and joins nonempty lines with a space. The declared `children`
prop controls evaluation: an aggregate receives positional child values; a
`Scoped<() => void>` prop receives a borrowed child body that the component
invokes synchronously. Nested child calls therefore run inside the container's
begin/end lifetime. The compiler does not insert text widgets or a UI runtime.
Finite aggregate children do not establish general child arrays or component
lifecycle behavior.

Diagnostics retain the authored `.ts` or `.tsx` source span. Frontend caches
preserve the structured expression graph and rerun common checking, including
when an imported Ziran declaration changes. Generated declarations and target
code belong under the normal build output.
