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
exact bits rather than floating-point comparison. Floating-point values are
decided only where the answer is exact on every target: an integer cast to
`float64` within 2^53 (`float32` within 2^24), and comparisons of such values,
including with integers in that range. Float arithmetic, float literals, and
wider casts round, so a law that uses them returns `unknown`.

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
their typed expression graphs, and evidence. Version 25 `.zib` carries the
linked verification dependencies and an evidence table. Loaders retype and
recheck certificates; saved status, type annotations, and evidence cannot
authorize a proof. Canonical checking rejects divergent saved fields. The
bundle loader also compares its outer evidence and waiver tables against
the checked embedded IR. Earlier artifact versions are rejected.

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
