# Owned values and recoverable failures

This is the target contract for owned `Vec[T]` and a string builder. A scoped
`Vec(T)` subset now supports default initialization (including locals in the
portable engine), checked indexing, fallible `VecPush`, `VecClear`, `VecFree`,
`VecSwap`, `VecPop` and `VecGet` results carried as `Option(T)` records, a
`Vec(u8)` string builder through `BuilderAppend` and `BuilderFinish`, an
explicit `VecClone(dest, src)` with a recoverable failure result, and
`VecSlice(values, low, high)` borrowed views, in C99,
Go, and the portable engine. A vector can live in a local, global, record
field, or fixed-array element reached from a record. Direct vector bindings,
whole aggregates containing vectors, vector-bearing record-literal fields, and
plain record-member paths rooted at a local aggregate have move checks on
declaration, assignment, argument passing, and return. Go can report capacity overflow but
its runtime may terminate on physical allocation failure; this is not yet the
full recoverable failure contract below. `VecPop` and `VecGet` require the
`Option` record template from `std/option.zi` to be visible in the using module.

**Current safety limits:** C99, Go, and the portable engine drop direct owned
`Vec` locals and parameters at block close, return, break, and continue. They
also recursively drop vectors reachable through record fields and fixed arrays.
A direct move clears its source before the next drop; an aggregate move clears
every reachable vector field in native targets, while the portable engine clones at
the destination boundary and relies on the checker to make the source
unreachable. Moving a plain record-member path clears only that field in native
targets and invalidates only the corresponding VM storage slot. An aggregate
move source must be a whole local binding or fresh call result; a member move
must be rooted at a local aggregate. A record literal can initialize a
vector-bearing field from a local vector or fresh call result; using that
source again is rejected. Indexed and pointer-backed member paths, and paths
rooted at a global, remain whole-aggregate/global move errors. A direct or
record-held `TextView` rejects mutation of its local backing place until the
view leaves scope, including views returned by checked calls and views nested
in their result records. The check is field-sensitive: sibling record fields
remain writable, while the borrowed field, its array storage, and whole
aggregate replacement are rejected. A view-bearing global conservatively keeps
its global backing aliases for the rest of the program: assignment from local
or parameter-backed storage is rejected, and mutation of a tracked backing
global is rejected in any function. A view reached through a local pointer created
with Jai `*value` protects its pointee binding and pointer aliases while live.
Local copies of pointer parameters keep the parameter's backing identity.
`TextView` through an opaque local pointer is rejected when its backing lifetime
cannot be proved. Separate pointer or slice parameters with the same type are
treated as possible aliases, with sibling record fields still disjoint.
Checked procedure bodies summarize parameter and global writes, including
transitive calls; a live view rejects a call that writes its backing storage.
String argument temporaries stay live through later argument evaluation and
the call, and unknown callbacks conservatively write reference arguments.
The checks use lexical scopes; close a view's scope before reusing its buffer.
Differently typed pointer aliases and raw native FFI still need explicit
memory contracts. A raw foreign declaration has no checked read/write summary;
the caller remains responsible for its pointer arguments. The current origin
summary tracks every global in the program. The
rest of this page describes the intended contract, not a guarantee that every
case is enforced today.

A fresh `Vec` call result used as a whole expression statement is dropped
immediately. A result used for member or index access must first be bound to a
local so its lifetime is explicit; an ordinary call can consume the fresh
result directly.

`BuilderFinish` consumes the builder: the returned `string` keeps the builder's
bytes without copying them on native C when nonempty. That target
still retains nonempty finished bytes for process lifetime; it now releases
reserved storage when the result is empty. Go copies into its immutable string
representation. The VM copies into an owned snapshot, preserves its owner
through aliases and ranges, and collects unused snapshots at safe statement
and call boundaries. Appending an empty string always succeeds
without allocating.

`Text` in `std/vec` is owned text: a record whose one field is
`bytes: Vec(u8)`, so it moves and drops like any record that owns a `Vec`,
in a local, a record field or a `Vec(Text)` element. `TextCopy(value)` copies
a `string` into one. `TextView(text)` borrows a Text place in place, and a
Text place passed, returned, assigned or declared where a `string` is
expected lends the same view. The view follows the lifetime rules above: it
cannot outlive its Text, and the Text cannot be mutated or moved while the
view is live. `TextView` of a fresh call result is rejected; bind the Text
first. `tests/owned_text.sh` checks C with an allocation counter and under
AddressSanitizer, Go and portable bundles from source and saved IR.

A `Text` call result, and `BuilderFinish` where a `Text` is expected, is
kept by an owner that takes it: a declared, assigned or returned `Text`, a
`Text` parameter, element or field, or a discarded statement. The checker
rewrites a `Text` call result anywhere else into `vec.TextDetach(result)`,
which keeps the string semantics described above: the bytes stay allocated
for the rest of the program. A detached result that is only read within its
statement, by a comparison, `.count` or a `string` parameter whose callee's
checked body only reads it, instead moves into a scoped `Text` local declared
before the statement, which lends its view and frees it when the block
closes; `BuilderFinish` in such a position becomes `TextFromBuilder` the same
way, and so does a `string` local such as `line := Render(...)` initialized
from one when every use after its declaration inside its block only reads
it. A read may pass through an `ifx` arm, a local, a procedure that returns
its argument where that result is only read, or a record literal field
handed to a callee whose record parameter only reads that field, followed
transitively. Results computed after another call in the same expression,
after a global the producing call may write, in a `while` condition or
`for` step, or in an `ifx` arm giving a string stay detached so nothing runs
earlier than written; literals, `-1` and globals it leaves alone may come
first. An `else if` becomes an `else` block holding a plain `if`, and the
second operand of `&&` or `||` or a scalar `ifx` arm runs as statements
that leave its value in a local, so results read there are scoped to their
own blocks (`tests/text_condition_arms.sh`). `tests/text_scoped_arguments.sh` shows no allocations left in 1000 rounds of such reads and keeps stored, pushed,
returned, aliased, deferred and global arguments and locals alive.
`tests/owned_text_results.sh` counts 1000 allocations kept in 1000 rounds of
three `Text` results used as strings, from the one read after a call, and
none kept as `Text`. The standard producers, such as `TextReplace`,
`Float64Repr`, `JsonWrite` and `JoinPath`, return `Text` the same way;
`tests/std_text_results.sh` measures 1000 allocations kept in 1000 rounds of
four producers used as strings, and none kept as `Text`.

Generic
`Option` and `Result` are currently ordinary records with explicit status
fields, applied as `Name :: Option(T)` or `Name :: Result(T, E)`. They copy
their fields. Fixed arrays and borrowed slices remain value and view types,
respectively.

## Values and ownership

Scalars, immutable strings, and aggregates whose members are copyable retain
value-copy semantics. An owned value, including `Vec[T]`, should move on assignment,
argument passing, and return. A move source must be a local binding, a plain
record-member path rooted at a local aggregate, or a fresh call result. The
member can be a Vec or a record holding Vecs, as in `last := popped.value`. The
checker rejects moving indexed or pointer-backed member storage and moving a
global, because their aliases cannot yet be represented precisely enough. The
checker rejects use after move and assignment over a direct or aggregate-held
owned vector; moves inside an `if` whose every arm returns do not reach the
join. The target rule is that every path drops each owned local exactly once,
with a moved source zeroed so a later drop is harmless. All current backends
implement this for direct `Vec` bindings, plain record-member paths, and
recursively owned vectors in records and fixed arrays.
A view declared as `[]T` from
`VecSlice(values, low, high)` borrows its source binding until the view's
scope closes: moving or mutating the source, including `VecPush`, `VecPop`,
`VecClear`, `VecFree`, `VecSwap`, and the string builder operations, is
rejected while the view is live; `VecGet` and `VecClone` sources stay
readable. The source can also be a Vec field reached from a local record or
pointer, as in `VecSlice(h.items, 0, n)`, `h.items[1:]`, or the view a
`for h.items` or `for it.parts` loop takes. That borrow is field-sensitive:
elements stay writable, and so do sibling fields and their Vecs, while
resizing or freeing the viewed Vec, moving or assigning it or a record that
holds it, or calling a procedure whose summary does either, is rejected.
The source can be a global Vec, or a Vec field of a global record, under the
same rules: while the view is live, growing, freeing or replacing that Vec is
rejected here and in any procedure whose summary does so. Because a later
push anywhere may move a global's elements, such a view also lives no longer
than its block: returning it or storing it in a global or an outer binding
is rejected. `VecClone(dest, src)` requires a fresh or moved-from destination
and returns `false` on allocation failure without changing the source.
Native output and the portable engine agree on the observable value of
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

The parser, checker, `.zir`, C/Go emitters, portable verifier, VM,
and host boundary implement the direct-vector operations above, with the
ownership and automatic-drop limitations called out at the start of this page.
