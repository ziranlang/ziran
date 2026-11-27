# Native TypeScript and TSX

`.ts` and `.tsx` are source frontends of the Ziran toolchain. They construct
structured expressions and declarations in the common IR. The common checker,
borrow checker, saved `.zir` format and C, Go, portable ZX, native ZX and Wasm
paths apply to them. There is no generated Ziran screen source, JavaScript
engine, npm module resolver or React runtime. The experimental `tsx2zi`
command has been removed.

The Kryon migration is still in progress. Owned capturing callables are
supported. Scoped borrowed callbacks, component state, keyed mounting and the
complete UI package integration remain open work.

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

```tsx
import * as Model from "model";
import { Half } from "arithmetic";

interface LabelProps { value: number; }
function Label(props: LabelProps): number { return Half(props.value); }

export function main(): s32 {
    const snapshot: Model.Snapshot = Model.Read();
    return s32(<Label value={snapshot.value}/>);
}
```

Imports have module scope regardless of declaration order. Namespace imports
and named imports with `as` aliases are supported. Default imports,
re-exports and JavaScript packages are unsupported.

## Types and values

- `number` is native IEEE 754 binary64 (`float64`). Integer-spelled ordinary
  numeric literals are also binary64; `5 / 2` is `2.5`. No integer type is
  silently substituted for TypeScript `number`.
- `boolean` is native `bool`. `string` is an immutable borrowed UTF-8 byte view,
  with native `.count`; it is not a JavaScript UTF-16 string object.
- `*Native.Record` names an explicit checked native pointer type. It follows
  the native borrow rules and cannot be captured by an owned callback.
- `s8` through `s64`, `u8` through `u64`, `isize`, `usize`, `float32` and
  `float64` name explicit native scalar types. Conversions use `s32(value)`,
  `u64(value)` and the other native type names. These follow the existing
  Ziran explicit-cast semantics; they are not JavaScript coercions.
- An integer literal ending in `n` carries exact native integer bits. Use it
  at an explicit native boundary, such as `u64(18446744073709551615n)`, to
  preserve a wide identifier without going through binary64. This suffix does
  not provide a dynamically sized JavaScript BigInt runtime.
- Interfaces declare native records. Imported Ziran records, procedure types and enums retain
  their checked identity. Records use value semantics and native ownership.
  Keep an owned native result in an explicitly typed binding, for example
  `const text: Text = TextCopy("label")` with `Text` and `TextCopy` imported
  from `std/vec`. The existing native convention detaches a `Text` call result
  to a program-lifetime string when the binding has no owned `Text` context.
  Use `TextView(text)` to borrow an explicitly owned binding during its lifetime;
  returning that view from the owning scope is rejected at the authored source.
- A homogeneous tuple such as `[string, string]` is a fixed native array.
  `T[]` names a borrowed native slice. Slice literals need backing storage;
  general owned-array and JSX child-range construction is still open work.
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

## Functions, statements and JSX

Functions have typed parameters and result types. `export` controls public
visibility. Local `const` and `let` bindings require an initializer. `const`
prevents rebinding; it does not freeze the fields of a record. Blocks, `if` /
`else`, `while`, C-style `for` with explicit assignment steps, `break`,
`continue`, assignment and local compound assignment are supported. Loop steps
execute before a matching `continue` and do not execute after `break`.
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

Scoped borrowed callbacks, classes, promises, async functions and type unions are not
implemented yet. Generic record applications retain their declaring module
and file scope: a local `Box<T>` and an imported `Domain.Box<T>` remain
distinct, including inside nested applications. Import aliases for the same
declaration share its identity.

JSX calls an ordinary typed component value with one props record. The compiler
knows no widget names and does not choose a UI library. Qualified component
names, nested components, expression props and children use ordinary checked
calls. The library defines the props and result types. Unknown and incorrectly typed fields are checked against those declarations.
Props currently use native record initialization, including native defaults for
omitted fields; TypeScript required-field enforcement remains open work. Duplicate props
and mismatched closing tags are errors.
Empty paired tags and indentation-only content do not add a `children` property.

Inline JSX text preserves spaces around expressions. Multiline text removes
line indentation and joins nonempty lines with a space. Children are positional
values in the props' `children` aggregate; the current finite tuple support does
not establish general UI child arrays or component lifecycle behavior.

Diagnostics retain the authored `.ts` or `.tsx` source span. Frontend caches
preserve the structured expression graph and rerun common checking, including
when an imported Ziran declaration changes. Generated declarations and target
code belong under the normal build output.
