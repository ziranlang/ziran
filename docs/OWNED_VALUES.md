# Owned values and recoverable failures

This is the target contract for owned `Vec[T]` and a string builder. A scoped
`Vec(T)` subset now supports default initialization (including locals in the
portable VM), checked indexing, fallible `VecPush`, `VecClear`, `VecFree`,
`VecSwap`, `VecPop` and `VecGet` results carried as `Option(T)` records, a
`Vec(u8)` string builder through `BuilderAppend` and `BuilderFinish`, an
explicit `VecClone(dest, src)` with a recoverable failure result, and
`VecSlice(values, low, high)` borrowed views, in C99,
C++, Go, and the portable VM. A vector can live in a local, global, record
field, or fixed-array element reached from a record. Direct vector bindings and
whole aggregates containing vectors have move checks on declaration,
assignment, argument passing, and return. Go can report capacity overflow but
its runtime may terminate on physical allocation failure; this is not yet the
full recoverable failure contract below. `VecPop` and `VecGet` require the
`Option` record template from `std/option.zi` to be visible in the using module.

**Current safety limits:** C99, C++, Go, and the portable VM drop direct owned
`Vec` locals and parameters at block close, return, break, and continue. They
also recursively drop vectors reachable through record fields and fixed arrays.
A direct move clears its source before the next drop; an aggregate move clears
every reachable vector field in native targets, while the portable VM clones at
the destination boundary and relies on the checker to make the source
unreachable. An aggregate move source must be a whole local binding or fresh
call result, and its destination must be a simple local. Record literals cannot
yet initialize vector-bearing fields by moving a vector. A direct or
record-held `TextView` rejects mutation of its local backing binding until the
view leaves scope, including views returned by checked calls and views nested
in their result records. A view-bearing global conservatively keeps its global
backing aliases for the rest of the program: assignment from local or
parameter-backed storage is rejected, and mutation of a tracked backing global
is rejected in any function. Pointer-derived views and other unmodeled aliases
are not covered. The current origin summary tracks at most 64 globals. The rest
of this page describes the intended contract, not a guarantee that every case
is enforced today.

A fresh `Vec` call result used as a whole expression statement is dropped
immediately. A result used for member or index access must first be bound to a
local so its lifetime is explicit; an ordinary call can consume the fresh
result directly.

`BuilderFinish` consumes the builder: the returned `string` keeps the builder's
bytes without copying them. C99 and C++ detach the buffer instead of freeing
it, so a finished string stays valid for the remaining process lifetime; Go
must copy into its immutable string representation, and the portable VM copies
into its interned string storage. Appending an empty string always succeeds
without allocating.

Generic
`Option` and `Result` are currently ordinary records with explicit status
fields, applied as `Name :: Option(T)` or `Name :: Result(T, E)`. They copy
their fields. Fixed arrays and borrowed slices remain value and view types,
respectively.

## Values and ownership

Scalars, immutable strings, and aggregates whose members are copyable retain
value-copy semantics. An owned value, including `Vec[T]`, should move on assignment,
argument passing, and return. A move source must be a local binding or a fresh
call result: the checker rejects moving nested record storage and moving a
global, because both keep shared storage that other code can reach. The
checker rejects use after move and assignment over a direct or aggregate-held
owned vector; moves inside an `if` whose every arm returns do not reach the
join. The target rule is that every path drops each owned local exactly once,
with a moved source zeroed so a later drop is harmless. All current backends
implement this for direct `Vec` bindings and for recursively owned vectors in
records and fixed arrays.
A view declared as `[]T` from
`VecSlice(values, low, high)` borrows its source binding until the view's
scope closes: moving or mutating the source, including `VecPush`, `VecPop`,
`VecClear`, `VecFree`, `VecSwap`, and the string builder operations, is
rejected while the view is live; `VecGet` and `VecClone` sources stay
readable. `VecClone(dest, src)` requires a fresh or moved-from destination
and returns `false` on allocation failure without changing the source.
Native output and the portable VM agree on the observable value of
every moved binding: the source is unreachable after the move.

## Recoverable errors

`Result(T, E)` stores `is_ok`, `value`, and `error`; a caller checks `is_ok`
and handles the error explicitly. `Option(T)` stores `has_value` and `value`.
Neither record has a hidden active case, and the language does not provide
postfix error propagation. An owned error type will need defined move and drop
behavior across all backends before it can be used in a portable `Result`.

## Growable storage

`Vec[T]` owns its allocation and reports length and capacity separately.
Growth checks size overflow and returns an allocation error without changing
the vector. `VecPush` consumes its input on either outcome; `VecPop` removes
the last element and returns `Option(T)`, leaving the vector unchanged when it
is empty. Indexing has the same checked bounds behavior as fixed arrays, and
`VecGet` returns `Option(T)` instead of trapping on an out-of-range index. A
string builder owns UTF-8 bytes in a `Vec[u8]`; `BuilderAppend` can fail, and
`BuilderFinish` consumes the builder without copying its bytes. Immutable
`string` values remain unchanged.

The parser, checker, checked `.zir`, C/C++/Go emitters, portable verifier, VM,
and host boundary implement the direct-vector operations above, with the
ownership and automatic-drop limitations called out at the start of this page.
