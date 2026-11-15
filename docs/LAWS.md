# Laws and scalar proofs

`ziran check` checks every law reachable through the input modules. A failed
law blocks native output and portable bundles. Pick the kind that states the
guarantee you need:

| Kind | Evidence | Scope |
| --- | --- | --- |
| `type`, `bounds`, `size`, `effect`, `abi` | `structural` | A compiler-derived property or supported ABI shape |
| `custom` | `evaluation` | One closed expression |
| `forall` | `exhaustive` | Every member of a declared finite domain |
| `theorem` | `kernel` | Every value of the declared scalar parameter types |

`proved` means that the selected checker established its obligation. The
method and domain tell you how far that result extends. A finite test of
`n: 0..4` says nothing about other values. An `abi` result checks a signature;
it does not prove what a foreign procedure does.

## Write an obligation and certificate

Long quantified laws can continue after each domain's comma and after `=>`.
Put each predicate on its own line, continuing after `&&` or `||`. The
formatter keeps these continuations indented until the terminating `;`:

```zi
#law AddCommutes forall left: 0..4,
    right: 0..4 =>
    Add(left, right) == Add(right, left) &&
    Add(left, 0) == left;
```

In `scalar.zi`:

```zi
Add :: (a: s32, b: s32) -> s32 { return a + b }
Larger :: (a: s32, b: s32) -> s32 {
    if a > b { return a }
    return b
}
```

In `LAWS.zi`:

```zi
Code :: #import "scalar";
#law AddZero theorem x: s32 => Code.Add(x, 0) == x;
#law MaximumCommutes theorem a: s32, b: s32 => Code.Larger(a, b) == Code.Larger(b, a);
```

In `PROOF.zi`:

```zi
Laws :: #import "LAWS";
#proof Laws.AddZero { unfold Code.Add; ring; }
#proof Laws.MaximumCommutes { unfold Code.Larger; cases; order; }
```

Run `ziran check --root src src/PROOF.zi`. These filenames are a convention;
the compiler follows imports. A certificate can also follow its theorem in
the same file. A law's identity belongs to its declaring module; use import
aliases to refer to another module's law. The same short name in unrelated
modules is allowed. Each theorem can have one certificate.

## Certificate steps

Every step ends with `;`. Put `unfold` steps first and end with one closing
step. Comments follow ordinary source comment rules.

| Step | Meaning |
| --- | --- |
| `unfold Procedure;` | Project this checked pure procedure and its acyclic callees into scalar expressions |
| `unfold;` | Permit all supported pure calls in the proposition |
| `cases;` | Permit the closing checker to split undecided boolean conditions and check both branches |
| `rewrite Equality(arguments);` | Check the named theorem's certificate and replace its left side with its right side, including inside larger expressions |
| `use Theorem(arguments);` | Close the goal with the exact proposition of a checked theorem instance |
| `refl;` | Close by reflexivity, boolean evaluation, and enabled case splits |
| `ring;` | Also normalize `+`, `-`, and `*` as modular polynomials |
| `order;` | Also use exact scalar order, transitivity, antisymmetry, and contradictory branches |

Theorem application uses explicit positional arguments. Lemmas are checked
with an empty hypothesis context. A waiver cannot supply a certificate, and
circular dependencies are invalid.

The first fragment includes `bool`, `s8/s16/s32/s64`, `u8/u16/u32/u64`, and
enum equality. Enum variables cover the entire backing integer domain,
including unnamed values obtainable through casts. Integer addition,
subtraction, and multiplication wrap modulo `2^width`. Signed order interprets
the same bits as a signed value. `ring` can establish associativity and
distribution through overflow; `order` cannot assume that addition increases
a value. For example, `x + 1 > x` is false at unsigned maximum.

Procedure projection supports scalar parameters and returns, local scalar
declarations and assignments (`=`, `+=`, `-=`, `*=`), lexical blocks,
conditionals, and acyclic calls with derived `pure` effects. Symbolic casts
between different types, loops, recursion, pointers, arrays, floats, runtime
globals, and foreign calls are outside this proof fragment. Constant casts
and casts to the identical type are supported. Arrays and bounded loops can
still be checked with existing finite `forall` laws. Integer evaluation uses
exact bits rather than floating-point comparison. Float laws assume unfused
IEEE arithmetic at the declared width: `+`, `-`, `*`, and `/` on `float32`
values round once to `float32`, and on `float64` values to `float64`; a result
that is infinite or NaN is `unknown`. An integer converts to `float64` within
2^53 (`float32` within 2^24); a wider conversion would round and is `unknown`.
A float literal is decided only when its text is the exact `%.17g` spelling of
its double, such as `2.5`; text like `0.1`, which rounds, is `unknown`.

## Results, budgets, and waivers

Each JSON result retains `law`, `kind`, `span`, `status`, and `detail`, adds the
declaring `module` for qualified identity, and includes:

```json
"evidence": {
  "method": "kernel",
  "domain": "x: s32",
  "cases_checked": 0,
  "counterexample": null,
  "waived": false
}
```

`cases_checked` counts concrete evaluations for `custom` and `forall`. It is
zero for symbolic theorems. A failed `forall` includes a JSON object mapping
bindings to their concrete numbers or sequence value text. `unknown` reports unsupported semantics,
a missing/incomplete certificate, or an exhausted budget. A supplied closing
step that fails to establish its goal is `invalid`; it is not a concrete
counterexample. `disproved` and `invalid` always fail the gate.

`#law_waive Name "reason";` allows only `unknown`. An imported law can be
waived by qualified name. Its result stays unknown, with `waived: true`;
the waiver also has its own result and is saved with the artifact.

The scalar kernel uses deterministic limits: 100,000 operations, 4,096 terms
and polynomial monomials, depth 64, 128 branch facts, 64 order terms, 16
parameters, and 64 locals. Exceeding a checking budget cannot establish a
proof. Finite `forall` has its separate 1,000,000-case budget.

## Saved artifacts and trust

Version 48 `.zir` stores theorem proposition graphs, certificate steps and
their typed expression graphs, and evidence. Version 25 `.zx` carries the
linked verification dependencies and an evidence table. Loaders retype and
recheck certificates; saved status, type annotations, and evidence cannot
authorize a proof. Canonical checking rejects divergent saved fields. The
bundle loader also compares its outer evidence and waiver tables against
the checked embedded IR. Earlier artifact versions are rejected.

Trust changes how much input validation a boundary needs; it never changes
memory safety. Checked-pointer slot, version, lifetime, extent, and
writability rules apply at every tier. A compiler may omit a runtime check
only when a verifier-visible fact proves every one of those obligations.
Anything unknown keeps the check.

| Tier | Boundary rule |
| --- | --- |
| 0, compiler-proved | Internal IR may consume a proved fact only after its verifier accepts the complete obligation. A backend optimization is not evidence by itself. |
| 1, process-controlled | Environment and argv values are copied into bounded caller storage and parsed through `EnvironmentFailure` or `ArgumentFailure`. Missing, empty, oversized, malformed, and out-of-range input are distinct results; compatibility readers do not authorize compiler decisions. |
| 2, semi-trusted local | Self-built `.zx`, local files, and caches are still editable. Loaders check framing, structure, identity, size, and digest before reuse. A cache hit may skip only hash-proven redundant compilation, never verification or a memory-safety check. Any cache refusal is named, removed, and recompiled. |
| 3, untrusted | Downloaded or host-supplied bytes are capped before allocation, checked for valid framing and identity, decoded, and reverified. No short read, malformed field, invalid URL, host error, or oversized body becomes zero, empty text, truncated success, or another fallback value. |

## Boundary diagnostic inventory

Compiler diagnostic codes are stable identities and are listed by
`ziran explain`. Typed library results are stable program-visible names. The
message text may improve without changing either identity.

| Boundary | Named outcome | Refusal contract |
| --- | --- | --- |
| Bundle path/open | `zx.input` | A missing or unreadable executable bundle never reaches decoding. |
| ZX framing, revision, digest, and canonical sections | `zx.container`; `ZxExecProblem` and its nested container/codec result | Truncated, wrong-revision, edited, or noncanonical bytes are refused before execution. |
| Decoded LIR | `ZxExecProblem.Codec`, `.Verify`, `.Identity`, `.Entry`, `.Capability`, `.Limit` | Decoding is bounded, content identities are recomputed, and `LirVerify` must succeed. |
| Native disk cache | `NativeCacheFailure.TooLarge`, `.Truncated`, `.WrongFormat`, `.WrongKey`, `.WrongLength`, `.WrongDigest`, `.WrongStructure` | The entry is removed and compilation runs; profile counters use the matching `native_cache_disk_reject_*` name. |
| Host capability/state/runtime | `zx.capability`, `zx.state`, `zx.runtime` | Missing bindings, incompatible state, and runtime traps are explicit terminal outcomes; there is no backend fallback. |
| Environment and argv | `EnvironmentFailure`; `ArgumentFailure` | Values are bounded and copied before use; validation failures stay distinct. |
| Binary fields | `BinaryReadFailure.NegativeOffset`, `.Misaligned`, `.ShortField` | A field is returned only when its complete aligned bytes exist. |
| Files | `FileReadFailure.InvalidLimit`, `.InvalidPath`, `.NotFound`, `.TooLarge`, `.ReadError` | A capped read either returns the whole file or a named failure, never a prefix. |
| HTTP and host bytes | `HttpInputFailure`, `CaptureFailure`, `CurlFailure` | URLs, limits, transport/host errors, and oversized bodies are validated and named before bytes are accepted. |

`tests/trust_boundary_cases.zi` pins the explainable compiler codes and typed
std names. `tests/native_disk_cache_cases.zi` corrupts every cache framing
layer and proves the named refusal is followed by recompilation and a reusable
replacement. `tests/std_environment_cases.zi` and `tests/std_input_cases.zi`
cover every new Tier-1 and Tier-3 failure result with negative cases.

The built-in C checker has no runtime Lean or solver dependency.
[`proofs/Scalar.lean`](../proofs/Scalar.lean) models fixed-width semantics and
proves the algebra, order, case, lemma-use, and rewrite rules sound. Its
certificates are mathematical derivations, not the serialized Ziran format.
The C polynomial normalizer, rule implementation, graph projection, parser,
serialization, and executable backends remain trusted code. This is not a
machine-checked proof of the whole compiler. Independent C and Lean
interpreters exhaustively check generated accepted claims at a small width;
integration tests cover runtime boundaries and tampering.

With the toolchain pinned in `proofs/lean-toolchain`, run:

```sh
make proof-model LEAN=/path/to/lean-4.34.1/bin/lean
make check
```

The model imports Lean's bundled standard libraries only. There are no
admitted obligations, custom axioms, or external proof packages. Induction
over arbitrary sequences and sorting remains future work.
