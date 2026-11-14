# Compiler and standard library guide

[Back to the README](../README.md) · [Implementation status](IMPLEMENTATION_STATUS.md)

## Assembly with a portable body

An ordinary procedure can supply architecture-specific assembly and a required
Ziran implementation. C output selects assembly when built by GCC or Clang
for that architecture. Go can opt into native assembly through cgo as described
below. Other compilers and `.zx` use the checked Ziran body. Define
`ZIRAN_NO_ASM` when compiling generated C to force the portable body.

```zi
Increment :: (value: $T) -> T #asm("x86_64", "add $1, %[out]", "=r", "0", "cc", "") #asm("aarch64", "add %[out], %[value], #1", "=r", "r", "", "") {
    return value + 1
}
```

Each `#asm` takes four to six arguments: architecture, assembly template,
output constraints, input constraints, and optionally clobbers and options
(both default to `""`). The architecture and options are literal strings.
Templates, constraints and clobbers can also name compile-time string
constants; options are `""` or `"volatile"`. Use semicolons to separate constraints and clobbers; commas
remain available for the assembler's constraint alternatives. Templates use GNU
extended assembly syntax, including named operands, register modifiers,
multiple instructions, escaped newlines, and assembler directives. Templates
are quoted as data in generated source. The native compiler and assembler
validate instruction syntax and machine-specific constraints.

A template constant can be generated in the same file with
`TEMPLATE :: #run MakeTemplate();` and used as `#asm("x86_64", TEMPLATE, ...)`.
The generator can use builders, loops, pointers and slices. Put its helpers
under `#scope_file` and restore `#scope_export` before the public kernel;
`tests/spec/asm_dct8.zi` generates three architectures from one transform
description this way.

Generated C uses positional operands internally, so source operand aliases
may also be C keywords. Named matching inputs retain the same binding.

Input operands follow parameter order and use the parameter names. Parameters
must be scalars, pointers or slices of scalars; a slice operand is its data
pointer, so pass the count separately. Pointer and slice parameters imply a
`"memory"` clobber. A scalar or pointer result has operand name
`out`; a record result has one operand named `out_FIELD` per declared field,
in field order. Result fields must be named scalars or pointers. Void results
have no output constraints. Outputs require `=` constraints, including
early-clobber `=&r`; use an input constraint such as `"0"` to initialize a tied
read/write output. Output and input operand names must be distinct. The limit
is 30 combined input/output operands.

Generic procedures retain their variants through specialization, including
from separately saved libraries. Declarations check syntax, input counts and
known operand types; every concrete instance checks all scalar/pointer inputs,
result fields, local bindings and its portable body. A type-dependent result
must match the declared output constraints at each call. Aggregate inputs are
rejected even when the current target would execute the portable body.
Generic local blocks follow the same rules. `tests/generic_assembly.sh` checks
integer widths, floating-point copies, tied outputs, memory aliases, caller-owned
record types and multiple results across native and portable targets.

For example, a two-field record can use output constraints `"=a;=d"` to capture
two registers. A pointer parameter can give assembly access to memory; declare
`"memory"` in the clobbers when accessing memory beyond named operands, and
use `"volatile"` when the operation must survive dead-result elimination.
Follow the [GNU extended assembly contract](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
for register lifetimes, early clobbers, matching constraints, flags and memory.

Architecture names are `x86_64`, `x86`, `aarch64`, `arm`, `wasm32`, `riscv64`,
`riscv32`, `powerpc64`, `powerpc`, `s390x`, and `loongarch64`. Selection uses
the native compiler's target macros, so cross-compilation selects the
destination architecture. Append required CPU features with `+`, for example
`"x86_64+popcnt"`, `"x86_64+avx2+bmi2"`, `"aarch64+crc32"`, `"arm+neon"`, or
`"wasm32+simd128"`.

```zi
Count :: (value: u64) -> s64 #asm("x86_64+popcnt", "popcnt %[value], %[out]", "=r", "r", "cc", "") {
    bits := value
    total: s64
    while bits != 0 { total += cast(s64)(bits & 1); bits >>= 1 }
    return total
}
```

A kernel can list one variant per line between its header and its portable body,
long templates as `#string` text. The body is required: it runs on every other
target (Go, WebAssembly, `.zx`) and is the reference the variants are tested
against.

```zi
// dst[i] = saturate(dst[i] + src[i]), eight lanes per block.
AddSaturate :: (dst: []s16, src: []s16, blocks: s64)
#asm("x86_64", #string END
    xorl %%eax, %%eax
1:  cmpq %[blocks], %%rax
    jge 2f
    movq %%rax, %%rcx
    shlq $4, %%rcx
    movdqu (%[dst],%%rcx), %%xmm0
    movdqu (%[src],%%rcx), %%xmm1
    paddsw %%xmm1, %%xmm0
    movdqu %%xmm0, (%[dst],%%rcx)
    incq %%rax
    jmp 1b
2:
END, "", "r;r;r", "rax;rcx;xmm0..xmm1;cc")
#asm("aarch64", #string END
    cbz %[blocks], 2f
    mov x9, %[dst]
    mov x10, %[src]
    mov x11, %[blocks]
1:  ld1 {v0.8h}, [x9]
    ld1 {v1.8h}, [x10], #16
    sqadd v0.8h, v0.8h, v1.8h
    st1 {v0.8h}, [x9], #16
    subs x11, x11, #1
    b.ne 1b
2:
END, "", "r;r;r", "x9..x11;v0..v1;cc") {
    for i: 0..blocks * 8 - 1 {
        sum := cast(s32)dst[i] + src[i]
        if sum > 32767 { sum = 32767 }
        if sum < -32768 { sum = -32768 }
        dst[i] = cast(s16)sum
    }
}
```

Clobbers name registers or numbered ranges: `xmm0..xmm7`, `x9..x11`,
`v0..v3`, and on ARMv7 `q0..q1`, which stands for `d0..d3`. The checker
requires every register a template names to be a clobber or pinned by a
constraint (`"a"` holds `rax`), every `%[name]` and `%N` to be an operand, and
no clobber to overlap a pinned register. Templates may read, but never
clobber, the stack, frame and platform registers: `rbp`/`rsp` on x86-64,
`x18`/`x29`/`sp` on AArch64, `r7`/`r11`/`sp`/`pc` on ARMv7.

Build generated C with `-DZIRAN_ASM_CHECK` to test the variants: each call of
such a procedure runs its portable body, then every variant the CPU supports
on copies of the same inputs, compares results and the memory its slice and
pointer parameters reach, and aborts with `#asm check failed: AddSaturate
variant "x86_64" differs from its portable body in dst` on a difference.
Callers receive the portable results. Run it over random and edge-case
inputs. Local `#asm` blocks are not
checked this way. `tests/spec/asm_dct8.zi` is a complete kernel: AV1's 8x8
inverse DCT with SSSE3, AArch64 and ARMv7 NEON variants, about ten times
faster than its portable body.

A `.zx` bundle is lowered from each procedure's portable body, so a bundle
holds no machine code and a downloaded bundle cannot run instructions of its
own; `ziran run FILE.zi` runs the portable bodies too. Native builds of the
source keep every variant. Run `sh tests/zx_native_asm.sh build/bin/ziran`.

Feature variants use isolated functions compiled for the requested ISA. The
ordinary caller keeps the build's baseline ISA, checks CPU availability once,
and caches its selection with atomic accesses. x86 GCC/Clang GNU-ABI drivers
use the compiler's CPU detection; Linux/Android AArch64 and ARMv7 C use the
kernel's hardware-capability information. Other platforms or unavailable
capabilities use the next eligible variant or the checked Ziran body. `ZIRAN_NO_ASM` forces
the portable body. Build the caller for the oldest CPU you intend to support;
globally enabling `-march=native` or extra ISA options can also affect its
ordinary generated code.

x86 and x86_64 features are `popcnt`, `sse2`, `ssse3`, `sse4.1`, `sse4.2`,
`avx`, `avx2`, `avx512f`, `bmi`, `bmi2`, `lzcnt`, `fma`, `aes`, and `pclmul`.
AArch64 features are `aes`, `crc32`, `pmull`, `sha2`, `atomics`, and `simd`.
ARMv7 (`arm`) has `neon`, and `wasm32` has `simd128`.
Other architecture/feature combinations are rejected. Feature combinations
require every listed feature. Variants are considered in source order; put
more demanding variants first and an architecture-only variant last. Duplicate
or shadowed variants are rejected. Baseline variants must use instructions
guaranteed by the selected build target. See
[GCC CPU detection](https://gcc.gnu.org/onlinedocs/gcc/x86-Built-in-Functions.html),
[function target attributes](https://clang.llvm.org/docs/AttributeReference.html#target),
and [ARM64 hardware capabilities](https://www.kernel.org/doc/html/latest/arch/arm64/elf_hwcaps.html).

`arm+neon` variants need ARMv7-A or later with a hard-float ABI (Linux
`armhf`, Android `armeabi-v7a` from API 18). Their helpers run in ARM state, so
Thumb-2 callers work, and the caller keeps the build's FPU option. Name the
NEON registers you use in the clobbers (`q0` or `d0;d1`). Write `vld1`/`vst1`
without `:64`/`:128` alignment qualifiers unless the address is known to be
aligned: a qualified access to a misaligned address faults.

A WebAssembly engine rejects a whole module that holds SIMD instructions it
does not support, so `wasm32+simd128` is a build option rather than a run-time
check. Its variants are compiled in, inline, only when the generated C is
built with `-msimd128`; otherwise the next variant or the Ziran body runs.
Build the module twice, with and without `-msimd128`, and load the SIMD module
only where `WebAssembly.validate` accepts a SIMD module. Operands are locals
(`local.get %[value]`, `local.set %[out]`), and inputs must not be written.
`v128` values cannot be operands: keep them on the operand stack or in memory
passed by pointer. From Clang 19 (current Emscripten qualifies) a template can
carry them through `block (v128) -> (v128)` and `loop (v128) -> (v128)`; Clang 18
crashes on any typed block in inline assembly. Use `v128.load 0:p2align=0` for
unaligned data.

Assembly and the fallback must implement the same behavior; checking the
fallback does not prove their equivalence, so test with `ZIRAN_ASM_CHECK`. These variants do not yet provide
`asm goto`, naked functions, standalone assembler files, or direct Go ABI
assembler output. Saved IR preserves the variants; `.zx`
executes only portable code. Rebuild older IR/bundles with the current compiler.

Generated Go packages use their checked Go implementation by default. Enable
the native bridge with `CGO_ENABLED=1 go build -tags=ziran_asm .` in the
generated package. Build packages rather than explicit filename lists so Go
applies companion-file build constraints. A working C cross-compiler is
required when cross-compiling with cgo. The bridge reuses the C assembly
emitter, including GNU constraints, labels, architecture selection and CPU
feature guards; it does not translate templates into Go assembler syntax.
`CGO_ENABLED=0` always uses the checked body. To force that body while testing
the native build configuration, set
`CGO_CFLAGS="-O2 -g -DZIRAN_NO_ASM"` together with the build tag.

Go bridge signatures carry scalars and pointers to storage containing no Go
pointers. Flat record results are reconstructed from primitive result fields.
Pointer-bearing objects, pointer-valued local-block outputs or memory inputs,
and local-block pointers to arrays retain the checked body. Standard cgo
pointer checks remain enabled. Assembly must not retain Go pointers after the
call or create pointers that violate Go's pointer rules. Unavailable variants
leave their Go dispatch unset, including before Ziran startup code executes.

Each bridge invocation pays a cgo transition. A whole-loop assembly kernel can
amortize that cost; calling the bridge for every small operation can be much
slower than the Go body. Measure the intended call granularity before enabling
the tag. Default builds retain a compile-time-disabled native branch.

Run `tests/assembly.sh build/bin/ziran` for source/saved IR execution and
rejection checks, and `tests/assembly_features.sh` for CPU selection,
concurrent calls, unavailable features and isolated helpers. It runs ARMv7
NEON and its masked fallback under `qemu-arm` with GCC and Clang
(`ZIRAN_ARM_CC`, `ZIRAN_ARM_CLANG`, `ZIRAN_QEMU_ARM`; `ARM_REQUIRED=1` fails
instead of skipping).
`tests/zx_wasm.sh` builds SIMD128 and baseline modules
with `ZIRAN_WASM_CC` (default `clang`, `WASM_REQUIRED=1` to require it).
`tests/go_assembly.sh` checks native and default Go builds, forced fallback,
strict cgo pointer checking, startup dispatch, memory aliases, concurrency and
source/saved IR. `bench/go_assembly.py` compares whole-loop and per-operation
bridges with checked Go bodies, a preserved compiler and native C controls.
`bench/assembly.py`
measures baseline assembly against its forced fallback and handwritten C/Go;
`bench/cpu_features.py` measures CPU-selected loop kernels against an optimized
Ziran body and handwritten C, with independent result validation. Use assembly
where measurements or access to a specific instruction justify it; compilers
already recognize many byte swaps and arithmetic patterns.

### Local assembly blocks

A block can read and write scalar or pointer locals and parameters inside an
ordinary procedure. Its six strings use the same architecture, template,
clobber and option rules. Each output/input item is `constraint:binding`, or
`constraint:alias=binding` when the assembly operand needs a different name.
The template refers to the alias, while the binding names existing lexical
storage. Every variant must bind the same operands in the same order.

```zi
Answer :: () -> s64 {
    value: s64 = 41
    result: s64
    #asm("x86_64", "leaq 1(%[value]), %[result]", "=r:result", "r:value", "", "") {
        result = value + 1
    }
    return result
}
```

For a read/write value, use output `"=r:total"` with an input such as
`"0:old=total;r:value"`; the first input initializes the output register and
`value` supplies another operand. Aliases must be distinct across all inputs
and outputs. Multiple outputs and zero-operand blocks are supported. Bindings
cannot be global names, constants, members, indexed expressions or aggregates;
pass a pointer when assembly needs to access other storage, and declare the
memory and register clobbers required by the GNU contract.

Architecture-only blocks emit inline assembly. CPU-feature blocks use isolated
helpers. C helpers receive input values and return a single `=r`/`=&r` output directly;
other or multiple outputs use each binding's actual address. Selection is cached
atomically per block. Their portable body is always checked, runs on unsupported
targets or with `ZIRAN_NO_ASM`, and must fall through to the following statement.
It may contain local loops and their loop control, but cannot return or break
out of the assembly block. Native instructions must preserve that fallthrough,
stack discipline and the declared side effects as well as result values;
checking cannot prove this equivalence. Run
`tests/assembly_blocks.sh` for source/saved IR, native/portable, concurrent
selection, typed operands and malformed-metadata checks. Inside a native
`#parallel` loop, operand bindings from the enclosing scope are captured even
when only the assembly template uses them; the portable body retains the same
loop bindings. Assembly outputs must remain iteration-local, and GPU regions
reject pointer operands. Native parallel regions currently support up to 16
captured bindings.

## Build and test

Run `make` to build the current toolchain and `make check` for the complete
lean language, backend, and portable bundle suite. Pruning removed 263
redundant check scripts and their unused helpers. Local and CI runs
execute every retained check. Explicit `CHECK_FILTER` runs include every
matching check. Workers follow the build queue's allocation; set `CHECK_JOBS=1`
to run serially or choose another positive worker count. `make sanitize` runs
the retained suite under AddressSanitizer and UBSan.

A project's GitHub Actions job sets up the Ziran its `ziran.lock` pins with
`uses: ziranlang/ziran/.github/actions/setup@COMMIT`. It checks out that
commit with its compiler built and puts `build/bin` on `PATH`; `fetch: true`
also fetches the locked packages. On Linux the build tree is the one the
Toolchains workflow published for that commit, so the job builds nothing; on
other systems, or for a commit without one, the job builds Ziran once and
caches it. Run the Toolchains workflow for an older commit a lock still pins.

Dependency-owned native recipes accept per-build options without editing project
settings: `ziran pkg artifact provider:native --platform linux-x86_64-gnu
--option provider:mode=release`. Repeat `--option Alias:key=value` for declared
string or boolean keys, including `group.name.key` for named tables. Values are
literal argv data; quote paths containing spaces. Overrides reach prerequisite
recipes and contribute to cache identity. See [package artifacts](PACKAGES.md#dependency-owned-native-artifacts)
for validation, alias mapping and the programmatic entry points.

## Compiler commands

The compiler commands are `zi2zir` (including
`--check-only`), `zi2c`, `zi2go`, and `zi2zx`.
Run a program with `ziran run hello.zi` or `zi2zx hello.zi`; Ziran checks
the file and executes `main` in one step. `make install-user` installs both
commands in `~/.local/bin`.
`zi-fmt` formats source. `ziran guide` prints a short reference from the
installed compiler, including current safety and target limits.
`ziran capabilities --target=go --json` reports a versioned machine-readable
target summary; see the [capability schema](CAPABILITIES.md).
`ziran api --json --root src src/main.zi` checks an entry and its imports,
then reports their public types and procedure signatures; see the
[API query schema](API_DISCOVERY.md).
`ziran check --lint` also warns about casts a program no longer needs: a cast
to the value's own type, and one that only widens a whole typed initializer,
assignment, argument, or result, which the compiler now widens itself.
It also warns when a byte is compared with a printable character's code
written as a number, as C ports do: `text[i] == 65` reads better as
`text[i] == #char "A"`. And it warns about an integer local that only ever
holds 0 or 1 and is only compared with them, which reads better as a `bool`.
Warnings do not fail the check (`"severity":"warning"` in JSON).
`ziran check|ir|api|inspect|build|bundle|run|fmt` remains a convenience dispatcher
for the same tools. [Cross-target benchmarks](../bench/README.md) measure the
compiler, downstream toolchains, and validated runtime cases separately.

Portable bundles run with `ziran run app.zx`. Graphical
bundles use installed package-provided platform hosts through the same command;
see [runtime providers](ZX.md#running-bundles).

### Output routes

The experimental `zi2asm` pipeline checks source into `.zir`, links and lowers
LIR, verifies it, runs LIR optimizations, and writes portable ZX instructions.
The portable interpreter executes those instructions directly.
The experimental direct-native route lowers the same verified instructions to
machine code, applies a target ABI/platform veneer, and writes a platform
binary; only rows with explicit implementation and execution evidence count as
supported. It never changes to another engine after a gap.

Native output from source first encodes and reads verified ZX bytes, exactly
as `zi2asm --input zx --emit elf -o app app.zx` does. `--emit zx` is a first-class
portable output; `--emit lir` remains a text inspection view. The standalone
`.lirb`, `--emit exec` and `--input exec` routes are removed. Optimization is
mandatory before ZX; `--opt` only selects the optimized source LIR view or its
source-only `--emit opt-report`. ZX input is not reoptimized. Native map source
origins remain a gap because the required ZX payload is stripped.
`ziran bundle` writes the same ZX; see [ZX](ZX.md).

The C and Go targets are separate generated-code exporters. With `--exe`, their
downstream C or Go toolchains also produce platform binaries. These outputs are
not called native by the direct-backend documentation and do not satisfy or
rescue a direct-native request.

Portable host capabilities and platform bindings are also separate layers. A
capability gives ZX code a typed service contract; a host binding implements
it. A direct target's ABI veneer instead implements target-specific entry,
calling-convention, syscall, and exit details used by direct machine code.

The opt-in `zi2asm --target x86_64-linux` ELF path is the first such veneer. It
produces a static no-libc executable with target-owned `write_all` and process
exit. Its literal formatter supports strings/text views, bools, every fixed
signed and unsigned integer width, and `%%`; floats and unavailable platform
services fail with stable named diagnostics.

## Generate C and Go binaries

Use `ziran build --target=c` for C99 output. Add `--exe --entry
module:function` to also compile it into `DIR/module`, a program that runs
that procedure and exits with its integer result; the entry takes nothing or
`(argc: s32, argv: **u8)` (signed byte pointers are also accepted). `CC`,
`CFLAGS` (default `-O3`), `LDFLAGS`, and `LDLIBS` apply as in make. Add
`--release` for final builds: the default flags become `-O3 -flto=auto`, which
optimizes across modules. Each link then generates code for the whole program
again, so one-file edits rebuild many times slower. `CFLAGS` still wins.

Small public procedures are inlinable across modules in every C build: the
module's header carries a `static inline` copy that other modules' calls use,
while the module's own `.c` keeps the one external definition, so a
procedure's address is the same everywhere. Each module still compiles and
caches on its own; editing such a procedure's body rebuilds the modules that
call it. Procedures that call file-scope code, or that use string
literals, constant tables or function values, stay out of line. A C file that
defines a module's procedures itself, such as a test mock, puts
`#define ZIRAN_<MODULE>_H_SOURCE 1` (the header guard plus `_SOURCE`) before
including that module's header, so the header leaves out its copies.
`-DZIRAN_NO_HEADER_INLINE` leaves out every module's copies, so all calls
reach the external definitions; a test that interposes procedures at link
time, as with `ld --wrap`, builds with it.

`#inline` after a procedure's result asks generated C to always inline it:
`Mix :: (value: s64) -> s64 #inline { ... }`. GCC and Clang get
`always_inline`, a public procedure's header copy is kept whatever its
size, and a procedure that calls itself is rejected. `#cold` does the
opposite for slow paths such as a refill: the procedure is never inlined,
is laid out as rarely run, and other modules call its one definition
rather than a header copy. Neither changes what a call does, and Go marks
a `#cold` procedure `//go:noinline`.

A call that returns an array writes its result straight into the array a
declaration creates or a plain assignment names, such as `grid := Make(4)`,
`row = Next(row)` or `holder.values = Make(10)`, with no temporary and copy in
C. The callee stores its result only when it returns, so arguments and
globals that name the destination still read the old value during the call.
Returning an array works the same way: `return Make(n)` hands the caller's
destination on, and `return out`, `return holder.values` or a parameter is
copied out once. A local array passed by value to a Ziran procedure, as in
`Sum(values)`, is copied once, by the procedure as it starts; the caller
copies it first only when a later argument could change it.

Indexing stays checked in every build, but generated C leaves out a check
it can prove always passes: a loop counter its condition bounds, such as `i` in
`for i: 0..7` over a `[8]T` array or in `for i: 0..view.count-1` over a
slice or string nothing reassigns, and values bounded by their type or by
`&`, `%`, `>>` or `<<` by a constant, such as `table[x & 255]`,
`bytes[(row << 4) + t]` for `row` in `0..15` and `t` in `0..3`, or `bytes[b]`
for a `u8` `b` and 256-entry arrays.
Locals built from such values keep their ranges: after `line := row * 16`
and `at := line + (offset & 15)` for `row` in `0..15`, `bytes[at]` over a
256-entry array needs no check, and a counter declared without a value
starts at zero. An
index whose variable has its address taken or is changed before the read keeps
its check. Run `sh tests/proven_index_ranges.sh build/bin/ziran`.

Guards count too. After
`if stride > 64 || input.count < cast(s64)(height + 3) * stride + width + 4 { return }`,
with `width` and `height` bounded the same way, `input[row * stride + column]`
in `for row: 0..height + 3` and `for column: 0..width + 3` reads without a
check. An enclosing `if` or `while` condition, earlier `&&` and `||`
operands, and an earlier `if` that ends in `return`, `break` or `continue`
all act as guards until a value they compare changes. A local declared
before the guard counts too: after `at := row * stride + column` and a
guard bounding `row`, `column` and `4 <= stride <= 36` that returns when
`cast(s64)at + 4 * cast(s64)stride >= levels.count`, `levels[at + 1]`,
`levels[at + stride]` and `levels[at + 4 * stride]` need no check unless
something writes `row`, `column` or `stride` in between. Every step of the
guard and of the index must provably not wrap, so
`if first + 1 + second > view.count { return }` does not prove `view[first]`. Run
`sh tests/guarded_index_ranges.sh build/bin/ziran`.

An `if` inside a loop only adds to what the loop proves. After
`if flag > 3 { continue }`, `if i >= 32 { continue }` or
`if code != 0 { return code }`, and inside `if i < 32 { ... }`, the read
`view[i * width + j]` in `for i: 0..height - 1` still uses `i <= height - 1`.
Facts chain, too: inside `for j: 0..width - 1` the width is at least 1, so
after `if stride < width || samples.count < (cast(s64)height - 1) * stride + width { return }`
the read `samples[i * stride + j]` needs no check. A local chosen by `ifx`,
such as `yy := ifx flip_rows then cast(s64)height - 1 - i else i`, is proven
for both choices, so `samples[yy * stride + xx]` needs none either. The
guard may also write `cast(s64)(height - 1) * stride`: inside the loop
`height` is at least 1, so `height - 1` did not wrap in `s32`. A long
`||` guard works as well as a short one: comparisons about values no index
reads, such as `gain > 255`, do not crowd out the ones it needs. A window
offset such as `(iy + 2 - radius) * stride + 2 - radius + k * stride + j`
is proven too after `if radius < 1 || radius > 2 { return }`. An `else`
runs where each branch before it was false: after `if i == 0 { ... }` and
`else if j == 0 { ... }`, the `else` reads `view[(i - 1) * width + j - 1]`
without a check, and `if i != 8` inside `for i: 0..8` leaves `i <= 7`.

Fields count as well. For `frame: *Frame`, after
`if source.count != frame.count || destination.count != frame.count { return }`
the copy `for i: 0..frame.count - 1 { destination[i] = source[i] }` needs no
checks: the loop reads `frame.count` once, before its stores. A field read
through a pointer keeps what guards showed until a call or a store through
any pointer, slice or global could change it, so a read after `Grow(frame)`
or `other.count = 0`, or in a loop body that stores through a slice, keeps
its check. A record parameter's fields, such as `shape.width` for
`shape: Shape`, change only with the parameter. Run
`sh tests/field_guard_ranges.sh build/bin/ziran`.

A local declared without a value starts at zero, and generated C leaves
the fill out when every element is stored before anything could read it.
For `block: Block` with `samples: [64]u16`, the loop
`for i: 0..7 { Simd.Store(block.samples[:], i * 8, row) }` writes every
lane, so C declares `Block block;`. Stores count at constant positions, in
loops with constant bounds, one field at a time or whole; a branch or loop
that names nothing of the local is passed over. The fill stays when a store
may be missing or late: a loop that stops or skips early, a store or break
under a condition not known there, any earlier use of the local such as a
read, a view or a `using` field, an index not known or one that wraps, and
records whose fields differ in size. Run
`sh tests/definite_initialization.sh build/bin/ziran`.

Generated C keeps Ziran's integer rules: a zero divisor or a shift count
outside 0 to width-1 aborts, and the minimum signed value divided by -1 wraps
to itself. Arithmetic compiles to C operators, a constant divisor or shift
count needs no check, and other divisions and shifts call a small checked
helper of their width. Generated Go shifts by a variable count in the
operand's own type after checking the count.

`#parallel_gpu for` runs a loop on the fastest GPU in native C builds on Linux
and Android, and on CPU threads otherwise. Programs need no graphics libraries
to build or start. A region qualifies when it uses only `s32`, `u32`, `float32`,
`u8` and `bool` locals, parameters and slices, assigns slices only at
`values[it]`, and calls plain functions of those types. Use `cast(s32)it` for
arithmetic on the index. The generated C names why any other region stays on
CPU threads. Integers, bytes and booleans match the CPU exactly; `float32`
results may round differently. A region that fails a bounds check or runs a
loop past about a million rounds per iteration reruns on the CPU, so failures
are reported as usual. `ZIRAN_GPU=off` keeps regions on the CPU, `ZIRAN_GPU=N`
chooses a device, `ZIRAN_GPU_MIN` sets the fewest iterations worth sending
(default 4096) and `ZIRAN_GPU_LOG=1` reports what ran where.

C program entries use a standard `int main(int, char **)` adapter.
The Ziran procedure keeps its declared parameter and result types for ordinary
calls and function values. The adapter converts its integer result to a process
status; a procedure returning nothing exits successfully. This also applies to
a procedure exported under the symbol `main` through `#program_export "main"`.

C-generated executable builds compile source files concurrently on at most half
of the available CPUs, with no more workers than free memory holds at 512 MiB
each; `ZIRAN_JOBS=N` lowers the count. Compiler commands run at niceness 10
with low disk priority, which their C compilers inherit; `ZIRAN_NICE=N`
chooses another niceness from 0 to 19. The build links and replaces the
executable only after success. The output directory has
a writer lock and keeps intermediate objects under `.objects`; unchanged
objects can be reused with supported GCC/Clang invocations. Reuse checks source
and header contents (including system headers), compiler executables and their
reported subtools, command flags, the environment and header search locations.
New headers that shadow an earlier dependency invalidate reuse too. The driver
always links again, so changed libraries reach the executable.

Compiler wrappers, unsupported flags, time macros, nonliteral header probes and
assembly includes use ordinary compilation. Missing or damaged cache files
are rebuilt. Nested generated modules and manually supplied root-level C hosts
participate in the link.

Use
`ziran build --target=go --pkg main --exe --entry module:function -o DIR`
for Go output. C foreign procedures with scalar or pointer parameters and
results route through generated cgo adapters; native Go package imports
continue to call those packages directly. C ABI calls need `CGO_ENABLED=1`
and a C compiler. `LDFLAGS` and `LDLIBS` are preserved in the cgo preamble.
Slice, record and variadic C ABI parameters are rejected on Go; modules
can wrap those APIs with scalar and pointer declarations.

Go can also bind a C header directly with `go:C/<relative-header>`. The header
supplies the function declarations and native type layout; generated adapters
do not redeclare its ABI. For example:

```ziran
c :: #system_library "go:C/example/signature.h";
builtin :: #system_library "go:builtin";
Signature :: #type #foreign c "signature";
New :: () -> *Signature #foreign c "signature_new";
Length :: (value: *Signature) -> usize #go_field #foreign c "(*signature).length";
FreeAtReturn :: (value: *Signature) #go_defer #foreign c "signature_free";
NativeAvailable :: () -> bool #foreign builtin "cgo_enabled";
```

Header calls accept scalar values and pointers to scalars or declared C types.
`#go_field` supports native fields and header constants; `#go_defer` preserves
cleanup during returns and panic unwinding. Pass include and library paths to
Go through `CGO_CFLAGS` and `CGO_LDFLAGS`, or preserve linker flags at generation
through `LDFLAGS` and `LDLIBS`. Calls still obey cgo's pointer ownership rules.
Header paths cannot be absolute or contain empty, `.` or `..` segments.

Header bindings generate `.cgo.go` and `.nocgo.go` companions. Ordinary Ziran
procedures remain available in either build. `cgo_enabled` reports the native
build setting so application code can choose its unavailable path before a C
call. With `CGO_ENABLED=0`, C pointer types are opaque and C calls panic instead
of fabricating results. Build the generated Go package with `go build .` or
`go run .`, which honor the build tags. Reachable header bindings and native
availability checks require Go; pruning a portable entry can remove them.

Pass `--entry module:function` to a C99 build to retain functions, types,
globals, and constants reachable from that entry. A native host implementation
of a called foreign function is retained when it is among the input modules.
Type-only linked modules emit headers without empty C files. Without `--entry`,
all checked declarations and module C files are emitted as before. Runtime
branches in reachable functions remain; this pass does not specialize them.

`ziran compile-commands DIR` writes `DIR/compile_commands.json` for the C
files in `DIR`, so clangd and other editors can read generated code with the
flags it builds with. `--compiler "cc -O2"` sets the compiler and its extra
flags (default `$CC`, then `cc`), `--include DIR` names Ziran's headers
(default: the pinned toolchain's with `--project`, otherwise the checkout
beside the launcher), and `-o FILE` writes elsewhere.

## Save and inspect ZIR modules

`ziran ir --entry module:function` saves that reachable graph as
per-module `.zir` files before code generation.

### Compile-time target facts

The typed compile-time values `TARGET`, `OS`, and `CPU` describe the selected
output, never the machine running the compiler. `TARGET` is an enum with `.C`,
`.GO`, `.WASM`, and `.ZX`; `OS` has `.LINUX`, `.WINDOWS`, `.MACOS`, `.ANDROID`,
`.WEB`, and `.NONE`; and `CPU` has `.X64`, `.ARM64`, `.WASM32`, and `.NONE`.
They work in `#if`, `#ifx`, `#assert`, and `#run`, including compound tests:

```ziran
#if TARGET == .C && OS == .LINUX {
    File :: #import "std/file_linux"
}
```

Imports in an inactive `#if` arm are not loaded. Native C and Go compilers use
the host OS and CPU by default and accept `--target-os` and `--target-cpu` for
cross-target output. WebAssembly selects `.WASM`/`.WEB`/`.WASM32`; portable ZX
selects `.ZX`/`.NONE`/`.NONE`.

Metaprograms that actually need compiler-machine information use
`Compiler.HostOS` and `Compiler.HostCPU`. `Compiler.Target`,
`Compiler.TargetOS`, `Compiler.TargetCPU`, and the string `Compiler.Options`
report the selected compilation. Host facts must not select target imports.

An untargeted `ziran ir`/`zi2zir` invocation is deliberately portable: it
checks with `TARGET == .ZX`, `OS == .NONE`, and `CPU == .NONE`, folds all
compile-time directives, and saves those results in `.zir`. Loading that file
later does not reevaluate compile-time branches for the consuming backend.
Select an explicit target while creating IR when a native-platform result is
intended.

`ziran bundle --entry module:function --retain other:entry` also keeps the
named function and its dependencies. Repeat `--retain` for further entry
points; unrelated functions remain outside the payload. Every retained entry
must exist in the checked input graph.
Saved `.zir` files are binary. `ziran inspect path/to/module.zir` shows their
declarations, statements, and checked expression links as readable text;
`ziran inspect --hex path/to/module.zir` shows bytes and offsets.
`ziran inspect program.zx` shows a portable bundle’s entry, linked modules and
assets without executing it; `--hex` shows its binary bytes.
`ziran build --target=wasm --entry module:main --root DIR -o program.wasm FILE.zi`
produces WebAssembly. Its `zx.host_i64` dispatch reserves index `UINT32_MAX`
for builtin string output: the i64 argument packs byte length in its upper
32 bits and memory offset in its lower 32 bits; the callback writes those bytes
and returns zero. Other reserved indices format signed integers (`UINT32_MAX - 1`),
unsigned integers (`- 2`), float64 bits (`- 3`), float32 bits (`- 4`), and booleans
(`- 5`). Scalar formatted print arguments use these callbacks. Host capabilities
cross through the typed `zx.host` import that [ZX](ZX.md) describes. Slices support
omitted bounds (`values[:]`, `values[:end]`, `values[start:]`) and empty views
at the end of their storage. Both inspection views read the saved file and
leave it unchanged.

## Imports

For ordinary imports, pass the entry file and a library directory with
`--module-path DIR` (repeat for multiple directories). There is no fixed
module-root count limit; storage grows with the inputs, subject to available
memory and operating-system argument limits. Search order remains the order
of the supplied paths. `check`, `ir`,
`api`, `build`, and `bundle` load extensionless `#import "module"` dependencies
transitively from `.zi` or saved `.zir`. For example:

```sh
build/bin/zi2zir --check-only --root app --module-path ../kryon/src/ui app/main.zi
```

A package-qualified import such as `#import "kryon/text"` resolves
through the package's dependencies in a `--project` build. Quoted export keys
may contain identifier-only namespaces: `[exports] "host/desktop" =
"src/host/desktop.zi"` is imported as `#import "kryon/host/desktop"`.
Outside a project, name the package's module directory instead:
`--module-path kryon=../kryon/src/ui` serves `kryon/NAME` imports from that
directory (and only those; repeat it for more directories). A package
builds the same way in both modes when each export has its module's name.
With `--project`, `--module-path DIR` adds a folder of generated modules to
the project's own package only; see
[generated modules](PACKAGES.md#generated-modules).

`#import, file "../lib/helper.zi";` loads an explicit source file relative to
the importing file. Saved `.zir` builds resolve its module by name from the
saved IR directory or a module path.
`Helper :: #import "helper";` and
`Helper :: #import, file "../lib/helper.zi";` expose public declarations as
`Helper.Name` without adding them to the unqualified scope.
`Helper :: #import, dir "../lib/helper";` loads that directory's `module.zi`.
An ordinary `#import "helper"` also discovers `helper/module.zi` on a module
search path.
`#load "relative/file.zi";` adds another file to the current module. Loads
can nest, and imports inside loaded files resolve relative to those files.
`#import, string "Value :: () -> s32 { return 42 }";` compiles source text
as a module. It also accepts a raw `#string` body, and
`Generated :: #import, string "...";` exposes its declarations as
`Generated.Name`.

### Module initializers

A module-level variable whose initializer calls a procedure is the module's
initializer. It runs once, before the entry, after the initializers of the
modules it imports, on the C, Go and portable targets alike:

```zi
// feature.zi
Host :: #import "host";

Double :: (value: s32) -> s32 { return value * 2 }

registered: bool = Host.Register(Double);
```

Importing a module is what includes it. `#import "feature"` runs the
initializer even when the importer names nothing from the module, so a module
can register a procedure with another module that never imports it. A program
that does not import `feature` carries neither that module nor anything only
it reaches, and `Host` finds its procedure variable still null.

This is how an optional part of a library stays out of programs that do not
use it. The library calls the part through a procedure variable instead of
importing it, and the module a caller imports to use the part registers it.
An exported procedure that nothing calls is left out in the same way.
A startup cycle between initializers aborts the program at startup; no
initializer is skipped or run twice.

## Foreign functions

Ordinary `#import` accepts a module identifier. C headers are no longer
imported as source modules; foreign procedures use `#system_library` and
`#foreign` declarations.
For a native symbol with an unmangled name, put `#program_export` on its own
line immediately before the function declaration.
Foreign procedures use a Jai-style library declaration and `#foreign`:

```jai
libc :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign libc "abs";
```

### Go packages

For a Go package, use an explicit `go:` library path. This also supports
standard packages whose import paths contain no slash:

```jai
strings :: #system_library "go:strings";
TrimSpace :: (value: string) -> string #foreign strings "TrimSpace";
```

The Go target emits a direct package call. These declarations remain Go
imports in saved `.zir`; they are not portable host capabilities.

An explicit `#program_export "NativeName"` emits a typed Go function with that
exact name. Checked Ziran calls retain their module-qualified implementation
name. Reserved names and collisions with functions, types, imports and module
startup are rejected; colliding globals and constants keep their generated
private names.

Native Go tests use `_test.zi` files and named exports such as
`#program_export "TestSigning"`. Modules using native `testing` bindings or
named test, fuzz and benchmark exports generate `_test.go` files, including
when their module names need disambiguation. Their native adapters expose
`*testing.T`, `*testing.F`, `*testing.B` or `*testing.M` directly for Go's test
discovery. Test helper modules may import the testing bindings from another
Ziran module. Ordinary Ziran checks without this native boundary retain the
`_test_ziran.go` filename and remain ordinary package code.

To require a package for its native initialization, bind its `init` symbol as a
nullary void procedure:

```ziran
driver :: #system_library "go:example.org/database/driver";
Initialize :: () #foreign driver "init";
```

The generated Go import runs the package's initialization once, before the
generated package's globals. The binding's call then has no work to repeat.
If no types or functions need a named import, the compiler emits a blank import.
Initialization bindings accept no receiver, parameters, results or modifiers.
Keep a call on the entry's dependency path when generating a pruned executable.

Generated Go executable entries carry the same provenance header and output
ownership as other generated files. `--prune-stale` removes an old entry when
the output directory is later generated with `--no-main`.

### Foreign Go types

Declare an opaque Go type alongside the package's foreign procedures:

```jai
json :: #system_library "go:encoding/json";
RawMessage :: #type #foreign json "RawMessage";
Envelope :: struct {
    payload: RawMessage #go_tag "json:\"payload\""
}
```

The Go target emits a type alias, preserving the imported type's identity,
methods, interface behavior and zero value. Ziran can store, pass, return and
assign these values, including across imports. Fields and layout remain opaque:
record literals and `size_of` are unavailable. These declarations require an
explicit `go:` package and are preserved in ZIR. Other targets reject
foreign Go types; an entry build can discard them when they are unused.

A slice can be explicitly converted to or from a foreign Go type whose
underlying storage supports the conversion. The Go compiler checks that
underlying type. Conversions preserve native length, capacity, nil values
and shared backing storage; returned native slices keep that storage alive:

```jai
Wrap :: (value: []u8) -> RawMessage { return cast(RawMessage)value }
Unwrap :: (value: RawMessage) -> []u8 { return cast([]u8)value }
```

Fixed-array casts, slice-to-slice casts and owned vector elements are rejected.

### Go receivers, fields and multiple results

A quoted method expression supplies its receiver as the first parameter.
The receiver must match an opaque type declared from the same Go package:

```jai
sync :: #system_library "go:sync";
Mutex :: #type #foreign sync "Mutex";
Lock :: (value: *Mutex) #foreign sync "(*Mutex).Lock";
Unlock :: (value: *Mutex) #foreign sync "(*Mutex).Unlock";
```

Use `#go_field` before `#foreign` to read a native field through a typed
getter. It takes exactly one receiver and returns the field value. The Go
compiler verifies that the field exists and has the declared storage type:

```jai
http :: #system_library "go:net/http";
Request :: #type #foreign http "Request";
Remote :: (request: *Request) -> string #go_field #foreign http "(*Request).RemoteAddr";
```

A getter without a receiver or parameters reads a native package variable or
constant each time it is called. It preserves native interface identity:

```jai
#import "go_types"
sql :: #system_library "go:database/sql";
NoRows :: () -> Error #go_field #foreign sql "ErrNoRows";
```

Getters cannot return owned vectors or records/slices containing them.
A field accessor with a void return is a setter. It takes a pointer receiver
and one value, or just one value when assigning a package variable:

```jai
time :: #system_library "go:time";
Duration :: #type #foreign time "Duration";
Client :: #type #foreign http "Client";
SetTimeout :: (client: *Client, value: Duration) #go_field #foreign http "(*Client).Timeout";
SetDefault :: (client: *Client) #go_field #foreign http "DefaultClient";
```

Setters cannot take owned storage or a value receiver. The Go compiler checks
the field's type and whether the package value can be assigned. Accessors
cannot use variadic parameters, `#go_results`, or `#go_defer`.
Methods of predeclared Go interfaces use their native receiver spelling,
for example `#foreign builtin "error.Error"` with an `Error` parameter.

Ordinary procedures can implement native Go interfaces with
`#go_method "MethodName"`. The first parameter is a local named record or its
pointer; remaining parameters and the result form the native method signature.
The generated method calls the ordinary Ziran procedure, preserving pointer,
error and panic identity. Method names must be Go identifiers, unique for the
receiver record, and different from its generated field names. Foreign types,
anonymous record aliases, generic procedures and owned vector signatures are
rejected. Go checks interface satisfaction when compiling the generated code.

```jai
Message :: struct { text: string }
Describe :: (message: Message) -> string #go_method "Error" {
    return message.text
}
```

On an ordinary method procedure, `#go_results` flattens a nonempty concrete
result record into native Go multiple results in field order. Its fields must
be named without `using` or owned vectors. Ordinary Ziran calls still receive
that record. C and the portable engine keep the same ordinary procedure body;
method adapters are Go ABI metadata. Both attributes survive saved IR. Go
executable entry selection retains methods of reachable receiver records for
native interface dispatch.

Use `#go_results` on foreign declarations to pack a Go function's multiple
results into a concrete record. Record fields correspond to Go results in
declaration order, including native error interfaces. It also works with method expressions:

```jai
#import "go_types"
net :: #system_library "go:net";
HostPort :: struct { host: string; port: string; error: Error }
Split :: (address: string) -> HostPort #go_results #foreign net "SplitHostPort";
```

The result record must be nonempty, without owned vectors, `using` fields or
discard fields. Variadic declarations and combinations with `#go_field` are
rejected. Both attributes require explicit Go package targets. ZIR
stores their typed signatures and attributes; generation from saved IR does
not infer them from diagnostic source text.

The standard modules `sync_go`, `time_go`, `random_go`, `text_go`, `net_go`
and `http_go` expose mutexes, native monotonic timestamps, cryptographic random
bytes, copied byte strings, IP primitives, HTTP request fields and headers.
`atomic_go` provides native `Uint64` counters with atomic `Add` and `Load`.
`http_go` also preserves the native `ResponseWriter` interface and exposes
response headers and `SetHeader`.
`time_go` provides parsing, UTC conversion and formatting while preserving
native `time.Time` values and parse errors. It also provides native ticker
creation, stopping and boxed channel access. `context_go` preserves native
contexts and provides `Background` and the boxed cancellation channel `Done`.
`select_go` builds receive, send and default cases over boxed native channels.
`Select` blocks until a case is ready and returns its index, native reflected
value and receive-success flag. It uses Go's `reflect.Select`; channel direction
and send-value compatibility are checked at runtime. Nil channels disable their
case, closed channels retain their zero-value/false receive result, and multiple
ready cases use Go's selection behavior. These operations require the Go target.
`sql_go` exposes native database,
transaction, row and result handles, nullable strings, row iteration and
transaction cleanup. Declare query, execution and scan bindings with the
argument types required by the application; Go checks those native signatures.
`errors_go` provides native error creation, `Is`, `Unwrap` and exact messages.
SQL and context modules expose their native error sentinels through getters.

### Go deferred foreign calls

Declare a void foreign procedure with `#go_defer` to schedule its native Go
call at the end of the calling function, including panic unwinding:

```jai
#import "sync_go"
sync :: #system_library "go:sync";
UnlockAtReturn :: (value: *Mutex) #go_defer #foreign sync "(*Mutex).Unlock";
Work :: (mutex: *Mutex) {
    Lock(mutex)
    UnlockAtReturn(mutex)
    // Work while the mutex is locked.
}
```

Arguments are evaluated and captured at the call statement. Scheduled calls
run in reverse order when that function returns or panics; a nested block
does not end the schedule. Declare and call the foreign binding in the
function that needs cleanup. A Ziran wrapper would schedule cleanup at the
wrapper's own return.

The binding requires an explicit Go package target, a void result and no
owned vector arguments or variadic parameters. `#go_results`, `#go_field`
and Go predeclared builtins other than typed `call` cannot be combined with this
attribute. Calls must be standalone statements. ZIR preserves the attribute, and
other targets reject reachable Go bindings. Ordinary Ziran `defer` keeps
its lexical scope cleanup semantics across targets.

Typed `go:builtin` `call` declarations invoke their first procedure argument
with the remaining arguments. The declaration must match that procedure's
parameter and result types and cannot carry owned vector values. A void call
can use `#go_defer` to run application cleanup during native Go panic unwinding:

```jai
builtin :: #system_library "go:builtin";
Cleanup :: #type (completed: *bool) -> void;
CleanupAtReturn :: (callback: Cleanup, completed: *bool) #go_defer #foreign builtin "call";
```

The callback and its arguments are captured when scheduled. Pass a pointer
when cleanup needs to observe a value changed later in the calling function.
ZIR retains the typed callback and the schedule independently of the
declaration's diagnostic source text.

Typed `go:builtin` `bind` declarations capture one or more leading procedure
arguments and return a procedure for the remaining arguments. The first
parameter is the full procedure type; subsequent parameters match its leading
parameters. The returned procedure matches its remaining parameters and result.
Captured values are copied when binding, so pointers retain shared storage while
later replacement of a value does not alter the capture. Binding never invokes
the procedure; native errors, slices and panic values retain their identity.
Captured parameters, remaining parameters and results cannot contain owned
vectors. C callback types are rejected. These signatures are checked in both
source and saved IR.

Ordinary procedure values also resolve each parameter and result in the
declaring procedure type's module. Imported records, pointers, slices and native
Go interfaces keep their type identity across different import aliases.

```jai
builtin :: #system_library "go:builtin";
Full :: #type (count: *s64, amount: s64) -> s64;
Bound :: #type (amount: s64) -> s64;
Bind :: (callback: Full, count: *s64) -> Bound #foreign builtin "bind";
```

Typed `go:builtin` `retain` declarations return their one argument unchanged
under Go's managed storage lifetime. The parameter and result types must match
and cannot contain owned vectors. This explicit native boundary allows strings,
slices and records read through pointers to escape a procedure while preserving
their original storage and nil values. It does not copy backing data or keep
foreign C allocations alive; such allocations retain their foreign lifetime.

Typed `go:builtin` `spawn` declarations start the first procedure argument in
a native Go goroutine with the remaining arguments. The callback and binding
must return `void`; their parameter types must match and cannot contain owned
vectors. Arguments are captured before starting the goroutine. Native Go
pointer, slice, map and channel storage stays alive under Go's collector;
shared mutation still requires synchronization. `spawn` cannot use `#go_defer`.

Typed `go:builtin` `assert` declarations use `#go_results` and one `Any`
argument. Their result record has exactly two fields: the target value type
and a `bool` indicating whether the native Go type assertion succeeded. A
failed assertion returns the target type's zero value and `false`; a typed
nil can succeed and stays distinct from a nil interface. Asserted slices,
pointers and interfaces retain native storage and identity.

```jai
#import "std/go_types"
builtin :: #system_library "go:builtin";
Worker :: #type (value: s64) -> void;
Start :: (callback: Worker, value: s64) #foreign builtin "spawn";
IntegerAssertion :: struct { value: s64; present: bool }
AsInteger :: (value: Any) -> IntegerAssertion #go_results #foreign builtin "assert";
```

These operations and the `std/channel_go` native channel handles are specific
to Go. `std/context_go.WithCancel` and `WithTimeout` return a native context
and its cancellation function; call `Cancel` to release that context. Schedule
cancellation in the owning function with a typed deferred callback when it
must also run during native panic unwinding. Other targets reject reachable
Go bindings; unused Go-only functions can still be excluded from a portable
bundle.

`io_go` exposes native readers, closable readers, writers and `ReadAll`.
`http_go` supplies request bodies and contexts, bounded body readers and
response status writes. `json_go` supplies JSON validation and streaming
encoders and decoders; `url_go` supplies decoded query values. Native interface
conversions keep the original stream and context objects, and these operations preserve
Go error identity. Schedule `ReadCloser.Close` with `#go_defer` in the function
that owns the stream when cleanup must also run after a panic.

### Go predeclared primitives

Typed foreign declarations in `go:builtin` can allocate Go heap objects and
slices/maps, copy bytes into a string, or read a native length:

```jai
builtin :: #system_library "go:builtin";
Allocate :: () -> *HostPort #foreign builtin "new";
Bytes :: (count: isize, capacity: isize) -> []u8 #foreign builtin "make";
Text :: (value: []u8) -> string #foreign builtin "string";
Length :: (value: []u8) -> isize #foreign builtin "len";
Append :: (values: []u8, value: u8) -> []u8 #foreign builtin "append";
Panic :: (value: Error) #foreign builtin "panic";
```

`new` takes no arguments and derives the allocated type from its pointer
result. `make` derives its slice or map type from the result; slices take a
length and optional capacity, and maps take an optional size hint. `len`
returns `isize` and accepts strings, arrays, slices, maps and declared foreign
Go types whose underlying type supports native `len`. Go compilation checks
the underlying operation for opaque types. These calls need no package import
and retain Go's allocation, zero-value and byte-copy behavior.
`append` takes a slice and one element, returns the same slice type, and keeps
Go's length, capacity and shared backing-storage behavior. Owned vector
elements are rejected; variadic slice expansion is not supported by this binding.
`panic` takes one non-owned value and returns void. It preserves the supplied
native Go value, including error identity, during panic unwinding.

### Go record metadata

Go record fields can carry reflection tags. The checked string is preserved
in saved IR and emitted as a Go struct tag; other targets keep the same fields
and ignore this Go metadata:

```jai
Response :: struct {
    userID: string #go_tag "json:\"user_id\""
    expiresAt: s64 #go_tag "json:\"expires_at,omitempty\""
}
```

Go can preserve an anonymous native struct's identity with `#go_anonymous`
alone in its record body:

```jai
Reply :: struct {
    #go_anonymous
    code: isize #go_tag "json:\"code\""
    message: string #go_tag "json:\"message\""
}
```

The Go declaration is `type Reply = struct { ... }`. Native reflection has
an empty type name, and JSON decoding retains anonymous struct error details.
The annotation preserves ordinary checked Ziran fields and is retained in
saved IR. Other targets keep their ordinary record representation.

### C header spellings

`#c "SPELLING"` lets a generated C header match C code that shares the
records. On a field it replaces the C type, or an array's element type. Written
last on an enum, it prefixes each C constant. Saved IR keeps it; Go and the
`.zx` VM ignore it:

```jai
ExprKind :: enum s32 #c "ZIR_EXPR_" { Unknown; Ident }  // ZIR_EXPR_Ident
Expr :: struct {
    name: *u8 #c "const char *"
    op: [8]u8 #c "char"                                    // char op[8]
}
```

A `using` field spelled `#c "union"` (or `"struct"`) also lays its record's
fields out as an anonymous C member over the same storage, so C reads
`stmt->name` where Ziran reads `stmt.name`.

### C-generated scripts

`ziran tool.zi ARGS` runs ordinary Ziran source as a C-generated script. Use `.zi`
for scripts and compiled programs alike. On Unix, start executable scripts
with `#!/usr/bin/env ziran`, give them an ordinary `main` procedure, and run
`chmod +x tool.zi` once. Then `./tool.zi ARGS` works with `ziran` on `PATH`.
Every argument after the filename belongs to the script, including `--help`.
The caller's working directory and standard streams are preserved.

The explicit form, `ziran run --target=c [--no-cache] tool.zi -- ARGS`,
also accepts compiler options before the filename. The C-generated runner
builds the program once into `$XDG_CACHE_HOME/ziran/native`, rebuilds it when
its sources change, and runs it with the arguments, returning its status. The
entry takes nothing or `(argc: s32, argv: **u8)`. In a project, a tool beside
the module roots, such as `tools/check.zi`, still imports the project's modules.
The tool reads its source path from `ZIRAN_SCRIPT_FILE`; see
`scripts/build_playground.zi` for a maintained build tool.

### Host capabilities and visibility

Ziran also resolves `host_api :: #system_library "host_api";` to its host
capability bridge. `ziran bundle --bind caller:capability=provider:function`
can satisfy a portable host capability with an exported Ziran function in the
bundle; the provider must have the same signature. `--bind-host MODULE`
binds every remaining capability to MODULE's exported function of the same
name, as a C program links a host's symbols by name, so one Ziran host module
serves `ziran bundle` and the Go target alike.
The portable host ABI copies returned string bytes into VM-managed storage,
including strings inside records, arrays, slices, and mutable slice updates.
A host can reuse its result buffers on its next call; saved strings retain
their bytes for as long as the program uses them, including across instance
runs. Returned fields and elements must remain valid until that next call.
Opaque pointer results still refer to host-owned storage.
Use `#scope_file`, `#scope_module`, and `#scope_export` to change the
visibility of following declarations.

## Multiline strings

Raw multiline text uses Jai's `#string` delimiter form. The body keeps its
whitespace and the newline before the closing delimiter:

```jai
Greeting :: #string END
Hello, "world"!
END;
```

## Returned views

A returned slice, text view or record of views must borrow storage the caller
still owns after the call: a slice or string parameter, a view already stored
in a pointed-to record, or static text. A pointer parameter lends its own
array storage only for the call, so return a view of it by taking the storage
as a slice instead:

```jai
Owner :: struct { values: [4]u16; label: string; }
Rest :: (values: []u16) -> []u16 { return values[1:] }    // view of caller storage
Label :: (owner: *Owner) -> string { return owner.label }  // view stored in the record
// Rejected: (owner: *Owner) -> []u16 { return owner.values[:] }
```

Call it as `Rest(owner.values[:])`. Inside the call, views of
`owner.values` may be read and written freely.

## Shadowing

A declaration in an inner block or a `for` body may reuse an outer name. Its
initializer still reads the outer binding, so `x := x + 5` adds to the outer
`x`, and `for i: 0..7 { i := i * 10 }` keeps counting with the loop's own
`i`. Two bindings of one name in the same block are rejected.

## Integer literals

Integer literals are decimal, `0x` hex or `0b` binary, with `_` between
digits: `1_000_000`, `0xDEAD_BEEF`, `0b1010_0101`. A `u64` literal may use the
whole unsigned range, as in `0xFFFF_FFFF_FFFF_FFFF`. Locals, globals, `::`
constants, constant tables, array sizes and range bounds accept the same
spellings on every target, and generated C headers spell them in decimal.

## Standard library

Import a standard module as `#import "std/text"`. In a project the pinned
toolchain supplies it. A standalone command takes it from
`--module-path std=DIR`, a `--module-path` directory named `std`, or the
standard modules that came with the compiler: `std` beside an installed
toolchain (`make install-user` copies it), otherwise the checkout the
compiler was built from. `ZIRAN_STD=DIR` names another standard directory.
`std/text` always means the standard module, even beside a library module
named `text`. The short spelling `#import "text"` also works when no module
of your own has that name.
Dependencies from other Git repositories import the same way, as
`#import "NAME/Module"`; see [Packages](PACKAGES.md).

### Vector capacity and initialized length

Import `vec` to use `Vec(T)`. `VecReserve(values, capacity: s64) -> bool`
reserves at least that **total** capacity without changing `values.count`.
`VecResize(values, count: s64) -> bool` changes the initialized length: new
entries receive the same default values as uninitialized local declarations
(zero scalars, empty strings, nil views/maps, recursively empty owned vectors).
Shrinking drops removed owned entries and retains capacity. Reused entries
are initialized again on growth; capacity beyond count is never indexable.
Neither operation consumes the vector. Pass mutable storage directly; a live
borrowed view prevents either operation, just as it prevents `VecPush`.

```jai
#import "vec"
pixels: Vec(u8)
if !VecResize(pixels, 4096) { return false }
pixels[0] = 255
```

Sizes are `s64`; negative sizes, size overflow, and failed allocation return
`false`, preserving the old elements, count and capacity. Zero is valid and
reserving zero allocates nothing. Native C uses one fallible growth allocation;
byte resize initializes its new range in bulk. Other element types use typed
default initialization rather than raw byte clearing of managed values. C,
Go, and portable `.zx` support both operations from source and saved `.zir`.
The VM also returns `false` when its array-storage budget prevents growth.
Go catches recoverable allocation range panics, but physical out-of-memory
failure can terminate the Go runtime. Capacities and allocation limits depend
on the target; a successful capacity request never guarantees future growth.
`VecGet` and `VecClone` cannot copy owned elements; mutate them in place or
move an element with `VecPop`.

A Vec can hold records that own Vecs. `VecPush` moves such an element in from
a local binding, a call result or a `.{...}` literal, so the source cannot be
used afterwards. Read and change elements in place: `groups[i].parts[j]`,
`VecPush(groups[i].parts, x)`, or `for * groups { ... }`. `for groups` and
`x := groups[i]` would copy owned storage and are rejected. Clearing, popping,
freeing or leaving scope frees the inner Vecs too. A Vec field iterates and
slices in place like a local Vec: `for h.items`, `for * h.parts`, `for
it.tags` inside `for * h.parts`, and `h.items[1:]`. While the loop or slice is
live, its elements and the record's other fields stay writable, but pushing
to, freeing or moving that Vec, or a record holding it, is rejected. A record
field that holds Vecs moves out like a Vec field, as in
`last := popped.value`. `Vec(Vec(T))` works the
same way. C, Go and portable `.zx` support this; wasm has no Vec storage.
`tests/nested_vec_elements.sh` checks it, including a leak check.

```zi
Group :: struct { name: string; parts: Vec(string); }
groups: Vec(Group)
group: Group
VecPush(group.parts, "a")
VecPush(groups, group)
groups[0].parts[0] = "b"
```

A global can hold a Vec or `Text`, alone or in a record. Assigning the
global, or a field of it, frees the old value and takes the new one, as in
`collection = CollectionParse(text)` or `shelf.items = TextCopy(name)`. A
declaration, assignment or return that moves a global, as in
`taken := collection`, takes its storage and leaves the global empty. A call
argument or `VecPush` element cannot move a global, because that would empty
it as a side effect: pass its address or move it into a local first. A
global cannot be replaced or moved while a view of it is live.
`tests/global_owned_storage.sh` checks C, saved IR and native and VM bundles.

A field path of a local record, or of a by-value parameter, takes owned
values the same way: `shelf.label = TextCopy(name)` frees the old label, and
a field moved out, as in `taken := shelf.bag.items`, can be assigned again.
A whole local still has to be moved before it is replaced. A field cannot be
assigned while a view of it is live, after its binding moved, below a moved
field or through a pointer. `tests/local_owned_fields.sh` checks these.


`VecResizeBounded(values, count: s64, maximum_capacity: s64) -> bool` uses
`VecResize`'s initialized-length and owned-element semantics while bounding the
resulting backing capacity. It rejects negative sizes, a count above the
ceiling, or an existing capacity above the ceiling, including on a shrink/no-op.
Rejection or allocation failure leaves storage, length and elements unchanged.
Growth clamps the selected capacity to the ceiling; reuse retains capacity.
A ceiling is not an allocation request, so a large positive ceiling is allowed
when the actual selected allocation fits the target's element/byte limits.
An empty Vec accepts count/ceiling zero without allocation. Future operations
must supply their own ceilings; the bound is not a stored property of the Vec.

```zi
scratch: Vec(u32)
maximum_elements: s64 = 1024
if !VecResizeBounded(scratch, 700, maximum_elements) { return false; }
// scratch.count == 700; 700 <= scratch.capacity <= maximum_elements.
```

This bounds logical backing element capacity, not allocator size classes,
runtime objects, managed-element allocations, VM validity maps or physical RSS.
During growth old and new backing allocations may be live together. A caller
with a concurrent-live scratch budget must reserve the full possible new
backing allocation in addition to already-live old capacity, then reconcile
against actual capacity after success. Default staging for managed elements
and runtime object overhead require separate accounting. Ordinary Go fatal
physical OOM remains outside recoverable allocation failures.

### Building text

`std/format.zi` builds text on an owned `Vec(u8)` builder. `Append(*builder,
value)` adds a string, integer, bool, or float, and
`BuilderPrint(*builder, "% of %\n", done, total)` appends a formatted piece:
each `%` takes the next argument and `%%` writes one percent. A file that uses
`BuilderPrint` imports `std/format` automatically, and `std/format` makes
`Vec` visible. `BuilderFinish(builder)` returns the text. Floats keep up to six
digits after the point, with trailing zeros dropped.

### Owned text

`std/vec` defines `Text :: struct { bytes: Vec(u8); }`, UTF-8 text that owns
its bytes. A file that names `Text` or `Vec(T)` without declaring its own
imports `std/vec` automatically. A `Text` moves and drops like the `Vec` it holds: it is freed when
its local, record field or `Vec` element goes away. `TextCopy(value)` copies a
string into a new `Text`, and `BuilderAppend(text.bytes, more)` grows it.
`TextView(text)` borrows the `Text` in place as a `string`; `TextView(it)`
also accepts the pointer `it` of a `for *` loop. A `Text` place passed or
stored where a `string` is expected lends the same view. A view cannot
outlive its `Text`, so returning a view of a local `Text` or storing it in a
global is rejected. The `Text` cannot move or change while a view of it is
live. `TextView` of a fresh call result is rejected; bind the `Text` first.

A procedure can return `Text`, and `BuilderFinish(builder)` gives a `Text`
where one is expected, as in `return BuilderFinish(out)` from a `-> Text`
procedure. A `Text` result is kept where its owner takes it: a declared,
assigned or returned `Text`, a `Text` parameter or `Vec(Text)` element, or a
discarded statement, which frees it. Anywhere else, as in
`line := Render(...)` or an argument for a `string` parameter, the result
detaches into a `string` that keeps its bytes for the rest of the program,
as `BuilderFinish` strings always have, so callers written for a `string`
result keep working. Write `line: Text = Render(...)` to free it with `line`.
A result that is only read is freed when its block closes instead: an
argument to a procedure whose body only reads that `string` parameter, a
comparison, `.count`, or `BuilderFinish` passed the same way, as in
`Count(Render(a))` or `Render(a) == Render(b)`, and a local such as
`line := Render(...)` whose uses only do the same. Reads follow `ifx` arms,
locals, a procedure that returns its argument when that result is only
read, and a record literal field, as in `Draw(Props.{.text = Render(a)})`,
whose callee only reads that field of its record parameter. Inside a
callee, a parameter may also be copied into a local `[N]string` whose
elements are only read, or into a `string` local that `=` later points at
other views of parameters or at literals, as in `v = s[1:3]`, and still
only be read. A callee or local that stores, returns, aliases or defers the
string, or a foreign callee not marked `#reads`, keeps it. The result is
never computed earlier than written, so one after another call in its
expression, after a global its procedure may change, in a `while` condition
or `for` step, or in an `ifx` arm that gives a string keeps its bytes.
Literals such as `-1`, other globals and a local's address, as in
`BuilderPrint(*out, "%", Render(a))`, may come first. A procedure that only
reads through a pointer argument, as in `AwardsText(*awards)`, is freed
too; one that writes through it stays in place. Each local is its own
binding, so a same-named local in another block does not keep it. An `else if`
condition, the second operand of `&&` or `||`, and an `ifx` arm that gives
a number or `bool` run on their own just before the statement, still only
when reached, so a result read there is freed too.

A foreign procedure that only reads its string and byte arguments during
the call says so with `#reads` before `#foreign`:

```
MeasureWidth :: (value: string, font: s32) -> s32 #reads #foreign host_api;
```

Text passed to it is then freed like Text passed to a Ziran procedure that
only reads it. Leave the marker off when the host may keep the pointer,
such as a binding that stores it; `#reads` on a procedure with a Ziran body
is an error, because that body already shows what it reads.
`TextDetach(text)` detaches explicitly, and `TextFromBuilder(bytes)` takes a
builder's bytes as a `Text`.

A procedure type makes the same promise for each procedure stored in it
with `#reads` after its result:

```
SelectionPainter :: #type (x: s32, props: TextProps, label: string) -> s32 #reads;
```

Each procedure stored in the type, as a slot's value, a record field, an
argument or a global's initializer, must only read its string parameters
and the string fields of its record parameters, including fields of records
they hold by value; one that keeps one is an error naming that parameter or
field path, such as `wrapped.data.value`. A record copied into a field of a
local record, as in `kept.command = command`, is followed to that local's
later uses, and `=` into the field path, or a record on it, later in the
same block replaces the string, so storing the record afterwards keeps
nothing. A pointer to the record, or a store before the replacement or
past it in a loop, keeps the string. Text passed through a value of the
type is then freed like Text passed straight to such a procedure.

The standard text producers return `Text`: `TextReplace`, `TextJoin`, `TextLower`, `TextHex`, `Float64Repr`, `JsonWrite`, `JsonString`, `JsonKey`, `JsonQuote`, `JsonNumberText`, `JoinPath`, `ResolvePath`, `WorkingDirectory`, `CreateTemporaryDirectory`, `CopyText`, `ReadStandardInput`, `DecodeReplace`, `Lower`, `CaseFold`, `Capitalize` and `Transform`. Existing callers keep
getting a `string`; `json: Text = JsonWrite(...)` frees the text with `json`.

`std/float_text.zi` supplies portable binary64 decimal conversion:
`ParseFloat64(text: string) -> (value: float64, ok: bool)` accepts a complete
ASCII decimal with optional sign, point and exponent, or `inf`/`Infinity` and
`nan`/`NaN`. It rejects whitespace and trailing bytes. Rounding is nearest,
ties to even; overflow succeeds as infinity and underflow as signed zero.
`Float64Repr(value: float64) -> string` returns owned shortest roundtrip text
with Python's `repr` layout: fixed notation for decimal orders -4 through 15,
otherwise lowercase `e`, a signed exponent with at least two digits, and `.0`
on fixed integral values. It preserves negative zero and writes `inf`, `-inf`
and `nan`. `Float64ReprInto(value, output: *Vec(u8)) -> bool` appends the
same text to caller-owned bytes, reserving before writing and reporting
allocation rejection. Both conversions use bounded exact integer arithmetic in Ziran,
without foreign calls, locale state or a Python runtime. Parsing scans all
input bytes, retaining at most 1100 significant digits plus a sticky flag;
this exceeds the length of every binary64 rounding boundary. Run
`build-queue -- sh tests/std_float_text.sh build/bin/ziran` for independent
edge/random C/Go/VM tests from source and saved IR.

### Hash maps

`std/hash_map.zi` maps string or integer keys to values on every target and
the portable runner:

```jai
#import "std/hash_map"

ages: HashMap(string, s32)
HashMapSet(*ages, "ada", 36)       // insert or replace
age: s32
if HashMapGet(*ages, "ada", *age) { print("%\n", age) }
HashMapRemove(*ages, "ada")        // returns whether the key was present
HashMapFree(*ages)
```

`HashMapHas` tests a key and `ages.count` is the number of entries. The table
doubles when half its slots are in use. Values may be records; keys are
`string`, `s64`, `u64`, or `u32` (and narrower unsigned integers). The Go
target's native `Map(K, V)` from `std/map_go` is separate and keeps its
`MapSet`/`MapGet` operations.

### Text and UTF-8

`std/text.zi` supplies ASCII case folding, prefix matching, and substring
matching over immutable UTF-8 strings. Non-ASCII bytes compare unchanged. These
functions use only portable Ziran operations, so the same source builds for C,
Go, and `.zx`. Add `--module-path std` when importing it from an app.

For full Unicode behavior, import `std/unicode`. `CaseFold(text: string)`,
`Lower(text: string)` and `Capitalize(text: string)` return strings using
Unicode 17.0 full default mappings, including expansions and contextual Greek
final sigma. Capitalization titlecases the first scalar and lowercases the
rest, as Python's `str.capitalize` does. `IsWord(codepoint: u32) -> bool`
matches Python Unicode regex `\w`; `IsDecimal(codepoint: u32) -> bool`
matches `\d` using only General_Category Nd. `IsSpace(codepoint: u32) -> bool` and
`IsWhitespace(text: string) -> bool` use Python's Unicode whitespace rules.
`IsPrintable(codepoint: u32) -> bool` accepts Unicode L/M/N/P/S categories
and ASCII space, excluding other separators, controls, format characters,
private-use and unassigned values under Unicode 17.0.
`SplitWhitespace(text: string) -> Vec(string)` collapses those whitespace runs
and omits empty fields, like `str.split()` without a separator. Its pieces
borrow the input. All operations run unchanged on C, Go and the portable engine,
without a host Unicode library. See [Unicode](UNICODE.md) for exact semantics,
the native table generator and conformance tests.

`NFCInto(text: string, output: *Vec(u8)) -> bool` and `LowerInto` use reusable
caller-owned buffers: they replace previous bytes, reject malformed UTF-8 and
empty the output on failure. NFC performs canonical Unicode 17 decomposition,
ordering and composition, including Hangul; compatibility normalization is
excluded. `IsLetter` and `IsNumber` match exact General_Category L and N;
`IsWhiteSpace` matches Unicode White_Space, excluding `IsSpace`'s extra
U+001C..U+001F controls. See [Unicode](UNICODE.md) for ownership and conformance.

`std/string_range.zi` provides `Substring(source, start, length)` over borrowed
bytes, using Ziran's checked `source[low:high]` string range syntax. Choose
UTF-8 codepoint boundaries when the result must remain valid text.

`std/utf8.zi` advances through UTF-8 scalar boundaries and counts codepoints.
Invalid bytes advance one byte, which keeps a scanner moving while preserving
valid multibyte sequences.
`Copy(source, destination)` writes valid UTF-8 unchanged and replaces each
malformed lead byte (following `Decode`'s consumption rule) with U+FFFD. It is
bounded and allocation-free, leaves separate source storage unchanged,
requires destination bytes not to overlap the source, and stops without
splitting a source sequence or replacement when capacity runs out. Its result
reports the written byte count and whether all input was consumed; embedded
NUL remains data.

`DecodeReplace(source: []u8) -> string` returns independently stored valid
UTF-8 with Python `errors='replace'` consumption: one replacement for each
maximal valid prefix of a malformed sequence. Thus `E2 82` becomes one U+FFFD
and `E1 80 41` becomes U+FFFD + `A`; forbidden prefixes such as `ED A0 80`
produce three replacements. It preserves valid UTF-8 and embedded NUL.
`Decode` and `Copy` keep their documented consume-one-lead-byte behavior.

### Generic values

`std/option.zi` and `std/result.zi` define generic records. Import a
template and create a concrete type with `Number :: Option(s32)` or
`Outcome :: Result(s32, string)`. An `Option` has `has_value` and `value`
fields; a `Result` has `is_ok`, `value`, and `error` fields. Check the status
field explicitly before reading the associated value.

`std/pair.zi` defines a generic record. Import it and write
`PairNumberText :: Pair(s32, string)` to create a concrete record
with `first: s32` and `second: string` fields.

A polymorphic procedure binds its type parameter through a direct `$T`, a
slice `[]$T`, a fixed array `[N]$T`, or a pointer `*$T` parameter: `Sum :: (values: []$T) -> T`
specializes for whatever element type the caller passes. A generic record
parameter binds its arguments too: `Add :: (bag: *Bag($K), item: K)` called
with a `*Bag(string)` binds `K` to `string`.

`Copy :: (values: [3]$T) -> [3]T { return values; }` infers `T` while
requiring exactly three elements. The capacity may be a compile-time constant
or expression from the procedure's module; it is not inferred from the caller.
Zero-capacity arrays retain their declared element type. Array parameters keep
ordinary value semantics, so changing a parameter does not change the caller's
array. Nested-array element types work natively; the portable linker rejects
nested-array returns that its runtime cannot represent yet.

### Sorting and queues

`std/sort.zi` sorts any slice whose elements support `<` in place with
`Sort(values[:])`, and searches a sorted slice with `LowerBound` and
`BinarySearch`. Sorting allocates nothing and is not stable.
`SortText(texts[:])` orders strings by unsigned UTF-8 bytes, including embedded
NUL and prefixes, independently of the system locale. It also allocates nothing.

`std/queue.zi` is an allocation-free bounded byte FIFO. Queue state is passed
and returned by value, and every operation receives the caller-owned backing
storage explicitly. Full pushes and empty pops are rejected without unwinding
the queue; partial byte pops report the number removed.

### TOML manifests

`std/toml_document.zi` parses TOML 1.0 directly in Ziran on C, Go and the
portable engine. It shares its parser with the package manifest reader. Quoted
keys and strings (including escapes, literal and multiline strings), dotted
keys, tables, inline tables, arrays and arrays of tables retain their meaning.
Duplicate definitions, malformed values, invalid UTF-8 and trailing input
reject the whole document, including errors in fields a caller does not query.

```zi
#import "std/toml_document"

document: TomlDocument
if TomlRead(*document, text) {
    package := TomlMember(*document, document.root, "package")
    name := TomlString(*document, TomlMember(*document, package, "name"))
    dependencies := TomlMember(*document, document.root, "dependencies")
    dependency := TomlFirst(*document, dependencies)
    while dependency >= 0 {
        alias := TomlKey(*document, dependency)
        dependency = TomlNext(*document, dependency)
    }
}
```

`TomlRead` returns `bool`; failure clears the tree, sets `root = -1`, and
reports `error` and `error_at` (a byte offset). Its default byte limit is
262144; pass a third `max_bytes` argument to change it. Nesting is limited
to 128. Keep the input alive while querying the document. Node queries accept
`-1` for missing values; use `TomlKindOf` to distinguish an empty string from
another type. `TomlCount` counts members/elements, `TomlBool` returns a value
and a success flag, and `TomlScalarText` returns the validated spelling of a
number, boolean or date/time without converting or rounding it. Run
`sh tests/toml_document.sh build/bin/ziran` for source, saved IR, C, Go and
portable execution tests.

### JSON and ZIP

`std/json_scan.zi` supplies allocation-free JSON value skipping, object member
and array element lookup, string spans, and decimal number reading. It validates
the value shape and escapes while scanning; member names match unescaped ASCII
bytes. Callers keep the original input string and use byte offsets returned by
the scanner. Its tests compare source and saved-IR portable bundles, and the
same module is exercised through native C and Go by a downstream client.

`ArrayRangesReuse` accepts value ranges from a successful scan and their
unchanged backing document. It reuses validation only for complete values
whose bytes match at the same array index; other values and the array's
separators are checked normally. Keep the preceding document and ranges alive
until the call returns, use a separate output slice, and consume offsets only
after a nonnegative return. Short output buffers still validate every element.
Run `sh tests/json_array_reuse.sh build/bin/ziran` for source, saved-IR and
cross-target transition checks, including malformed replies and numeric prefixes.

`ReadStringValue(text, at, max_bytes)` decodes a quoted value into owned bytes
and reports `valid`, including for empty strings. Its limit counts decoded
bytes, so a long escaped value can fit. Consume the returned `value` vector
with `BuilderFinish`, or let normal vector cleanup release it. Ordinary UTF-8
runs append directly, and escaped code points use a five-byte temporary buffer
instead of a maximum-size string buffer. Invalid input or an exceeded limit
returns no decoded bytes.

`std/json_document.zi` reads a whole document into a tree of numbered nodes
that a program queries (`JsonMember`, `JsonElement`, `JsonFirst`/`JsonNext`,
`JsonString`, `JsonInteger`), changes (`JsonSet` keeps a replaced member in
place, `JsonAppend`, `JsonRemove`, `JsonCopy`) and writes back. `JsonWrite`
lays the text out as Python's `json.dumps` does for the same style: indent,
separators, ensure_ascii escapes and sort_keys, so tools can rewrite files that
Python wrote without changing their bytes. Numbers keep the text they were read
with by default. `JsonPythonStyle(indent, ascii, sort_keys, true)` opts into
binary64 float canonicalization (`JsonStyle.python_floats` for a custom style):
`1e3` becomes `1000.0`, `-0e0` becomes `-0.0`, and `1e-7` becomes `1e-07`.
Only tokens containing a point or exponent are converted. Integer tokens,
including arbitrary-precision integers and `-0`, keep their original spelling;
the stored node text is never changed. Float overflow writes `Infinity` or
`-Infinity`, as Python's default JSON encoder does. Run
`sh tests/std_json_document.sh build/bin/ziran` for tree operations and default
layout, and `sh tests/std_float_text.sh build/bin/ziran` for canonical floats.

Documents own decoded escape buffers and generated integer text; normal
document cleanup releases them. `JsonReset` releases those buffers and clears
the tree, retaining vector capacity for reuse. Unescaped input, `JsonNewString`
and `JsonNewNumber` remain borrowed. Consume their views before changing that
storage. `JsonSetCopy` acts like `JsonSet` but owns a copy of its key; ordinary
`JsonSet` borrows its key. `JsonNewStringCopy` owns a copy of its value;
`JsonNewString` and `JsonNewNumber` continue to borrow. `JsonCopy` deep-copies
text and keys into the target, so the source may
be reset or dropped first, including when copying within one document.
Read member names with `JsonKey` or `JsonKeyInto`: a node's raw `key` field is
internal, and `JsonCopy` and `JsonSetCopy` leave it empty.
`JsonString`, `JsonKey` and `JsonNumberText` still return independent owned
strings. `JsonStringInto(document, node, output)`, `JsonKeyInto` and
`JsonNumberTextInto` instead append to caller-owned `Vec(u8)` storage and return
false on allocation failure or a missing/wrong-kind node. `JsonWriteInto` and
`JsonQuoteInto` also return false on allocation failure; discard partial output
on failure. Their output must not alias the document's input or owned buffers.
`JsonWrite` and `JsonQuote` detach owned strings.

For private data, pass `JsonRead(document, text, erase, context)` or register
`JsonSetEraser(document, erase, context)` before constructing a document. The
crypto-independent callback `(context: *void, bytes: []u8) -> void` must overwrite
the supplied mutable bytes. It runs before owned decoded/copied/generated text,
discarded keys and internal text temporaries are freed, including failed reads.
Explicitly `defer { JsonReset(*document) }` before a private document leaves
scope; normal implicit vector destruction cannot call the registered eraser.
The caller owns erasure of borrowed input and Into output, and starts output
empty/wiped or reserves its full lifetime maximum before storing private bytes.
Use Into accessors rather than detached string returns for private data.
The parser and writer reserve before copying private bytes, so their own growth
does not relocate plaintext into allocations the eraser cannot reach.

`std/zip.zi` reads classic ZIP directories from caller-owned bytes, verifies
stored entries with CRC-32, and writes stored archives into caller-owned output.
It rejects split, encrypted, and malformed archives, and duplicate requested
entries. ZIP64 archives are unsupported.
It runs in portable bundles. `std/zip_linux.zi` adds raw DEFLATE extraction
through the system zlib library for 64-bit Linux native C builds; link those
builds with `-lz`. The caller supplies file I/O, memory limits, and which entry names
are meaningful to the application.
`std/gzip_linux.zi` compresses and decompresses a single gzip stream into
caller-owned, disjoint byte buffers. `GzipDecompress` requires the expected
uncompressed length and rejects expansion beyond that length, bad checksums,
truncation and trailing streams. Native C builds link zlib (`-lz`); Emscripten
builds use `-sUSE_ZLIB=1`. Run `make gzip-test` for independent GNU gzip
interoperability and malformed-input tests.
`std/image.zi` holds RGBA pictures (`Image`: width, height and straight-alpha
pixels, rows top to bottom) and does what build tools did with Pillow:
`ResizeNearest`, `ResizeBox`, `ResizeBicubic` and `ResizeLanczos` give Pillow's exact pixels
(filtered resizing premultiplies alpha as Pillow does), `AlphaComposite` is
`Image.alpha_composite`, and `PasteImage`, `RotateLeft`, `DropAlpha` and
`QuantizeImage` (median cut with Floyd–Steinberg dithering) complete it.
`Thumbnail(image: *Image, max_width: s32, max_height: s32,
reducing_gap: float64 = 2.0) -> Image` returns an owned bicubic thumbnail,
preserving aspect ratio with Pillow's floor/ceil choice and never enlarging.
`ResizeBicubic(image: *Image, width: s32, height: s32,
reducing_gap: float64 = 0.0) -> Image` also permits enlargement. Both keep the
source unchanged and return an empty image for invalid images or nonpositive
sizes. Gap `0` means Pillow's `None`; other gaps must be finite and at least
`1` when resizing (an unchanged thumbnail returns before checking the gap).
Like Pillow's RGBA path, they resample premultiplied bytes without an
integer reduction step, regardless of gap. Unchanged sizes preserve all bytes,
including RGB under zero alpha; neither operation applies EXIF orientation.
`ResizeBicubicInto(image: *Image, width: s32, height: s32, output: *Image) -> bool`
provides the same default bicubic pixels with checked allocation and dimension
arithmetic. Output owns its pixels and can be reused; it must not alias the
source or borrow its pixel storage. Failure empties output and releases partial
buffers; null output or identical source/output records fail unchanged. The
source remains unchanged. Run `sh tests/image_resize_fallible.sh build/bin/ziran`
for portable parity, native allocation faults and sanitizer lifetime tests.
Run `make -f Makefile -f tests/native_image_thumbnail.mk image-thumbnail-test`
for 40 frozen Pillow 12.3.0 bitmap oracles through native C source/saved IR,
plus Go and portable source/saved-IR assertions (no Python dependency).
`std/resample.zi` makes the same box, bicubic and Lanczos bytes for native
programs that resize repeatedly, such as a UI drawing a picture at its
on-screen size: `ResampleAxisInto` computes one axis's weights once, and
`ResampleRowHorizontal`/`ResampleRowVertical` then write rows without
allocating. `ResamplePlanInto` plans a whole premultiplied picture or a
fractional source box as Pillow's `Image.resize(box=...)` does, and its two
passes split into row ranges for worker threads. SSE2 and AVX2 on x86-64 and
NEON on AArch64 and ARMv7 run the rows with the scalar loops' exact bytes,
chosen once per CPU. A WebAssembly module built with `-msimd128` runs SIMD128
ones, so a web host loads it where `WebAssembly.validate` accepts SIMD and a
build without it elsewhere. Other targets, `ZIRAN_NO_ASM`
and `.zx` use the scalar loops. Filtering is on the stored bytes, as in
Pillow, not in linear light. Run `sh tests/lanczos_isa_bitexact.sh
build/bin/ziran` (with `ANDROID_NDK_HOME` and qemu-user for AArch64 and
ARMv7, or `ARMV7_ADB_SERIAL` for an ARMv7 device; `WASM_RUN`, a command that
runs an Emscripten `.js` such as `node`, or `WASI_RUN`, one that runs a
standalone `.wasm` such as `wasmtime run`, with `EMCC` for WebAssembly) to
compare every kernel with the scalar loops and those with std/image.
`std/png_linux.zi` reads every standard PNG into an `Image` and writes RGBA,
RGB and palette files; `std/jpeg.zi` writes baseline JPEGs at an IJG quality;
`std/zlib_linux.zi` compresses and decompresses whole zlib, gzip or raw
deflate streams into growing vectors. The PNG and zlib modules link `-lz`.
Run `sh tests/std_image_linux.sh build/bin/ziran`, which also compares with
Pillow when it is installed.
`std/vorbis.zi` reads what an Ogg Vorbis stream in memory declares without
decoding it: `VorbisProbe(bytes, *audio)` checks that the bytes are whole
pages of one stream with a Vorbis identification header and gives its
channels, rate and length, so a program can bound a stream before any
decoder allocates for it. It needs no library.
`std/vorbis_linux.zi` decodes one whole Ogg Vorbis stream held in memory
into interleaved signed 16-bit little-endian PCM over the platform
libvorbisfile: `VorbisDecode(bytes, *pcm, max_frames, *audio)` probes first,
reports the channels, rate and frame count, and refuses truncated or chained
streams, trailing bytes, damaged pages and anything longer than `max_frames`. Its
records mirror the Xiph headers, and a guard after the decoder state is
checked on every call. Only programs that import it link
`-l:libvorbisfile.so.3` (or `-lvorbisfile` with the development package).
`tests/vorbis_linux.sh` needs ffmpeg with libvorbis; with the Vorbis
development headers (or `VORBIS_INCLUDE`) it also compares the record sizes
with the C compiler's.

The reusable `audio` package, maintained in `ziranlang/packages/audio`, adds
Opus and quality sinc conversion without a GUI or audio-device dependency.
`audio/opus` exposes bounded Ogg decoding and single-owner encoder/decoder
states with caller-owned scratch. `audio/pipeline` adds whole-buffer encoding
with automatic sinc resampling. The Ogg decoder applies
pre-skip, header gain and final trim; `DecoderRewind` resets a validated
immutable input without rescanning it. `audio/resample` wraps libsamplerate's
best-quality sinc, including 44.1–48 kHz conversion and incremental processing.
`audio/ogg` inspection and `audio/wav` PCM16/float32 input with PCM16 output
also run in portable bundles. Native codec/sinc calls require the C target;
portable bundles reject those FFI calls explicitly. Native consumers declare
`links = ["opus", "samplerate", "m"]` in their root package manifest.
The package's `audio` CLI provides `encode`, `decode`, `inspect`, `transcode`
and `resample`, with bitrate, VBR, constrained VBR and named bandwidth caps.
Its README documents ownership, frame/byte limits, supported formats and
tested targets. Existing Vorbis modules remain independent.

`std/zip_file_linux.zi` writes stored ZIP entries directly to a caller-owned
file from borrowed byte slices, including mapped files. The caller controls
file creation and publication. Its Ziran test checks the output with Info-ZIP's
reader as well as Ziran's reader.

`std/zstd_go.zi` provides single-owner reusable Zstandard encoders and bounded
stream decoding for the Go target. The consuming Go module supplies
`github.com/klauspost/compress` (tested with v1.20.3, requiring Go 1.25 or newer).
`NewEncoder(writer, level, window_bytes, literal_entropy)` uses one worker;
numeric levels choose the provider's nearest preset. Literal entropy defaults
to true so base64-like data can compress even without repeated strings.
Close an encoder before `Reset` attaches its next writer. The module does not
pool contexts or retain response buffers itself. `DecodeLimited` caps returned
bytes and the frame's window, rejects malformed/trailing data, and returns no
partial bytes on failure. It accepts valid concatenated frames within the same
total limit. Native C targets continue to use their transport's negotiated
decoder; this Go module does not provide a portable bundle capability. Run
`sh tests/zstd_go.sh build/bin/ziran` for source/saved-IR, race, independent
libzstd interoperability and malformed/bounded-input verification.

### Binary fields

`std/binary.zi` reads little-endian numbers from byte text such as file or
asset contents: `ReadU16LE`, `ReadU32LE`, `ReadS32LE` and `ReadF32LE` take
the text and a byte offset, and a field that does not fit reads as zero.
`Float32FromBits` and `Float64FromBits` turn IEEE 754 bit patterns into
values with exact arithmetic, including subnormals, signed zeros, infinities
and NaN, so native targets and portable bundles agree without reinterpreting
memory. Run `sh tests/std_binary.sh build/bin/ziran`, which also compares the
decoding with the C compiler's over a million random patterns.
`std/binary_linux.zi` keeps 64-bit fields for native Linux programs.

### Constant tables

A constant array of numbers or booleans is a read-only table:
`COS :: s32.[4096, 4095, 4091]` or `#run s32.[...]`, of any length and over
as many lines as it needs. Reading `COS[i]` reads one table the module shares
(a file-scope `static const` in C, a package `var` in Go, built once in the
portable engine) instead of copying the array; copying it into a variable, as in
`local: [3]s32 = COS`, still makes an independent copy, which Go takes
straight from the shared `var`, as it does when passing the table by
value. Indexes are
bounds-checked as for any array. A function may read a table, and its
`.count`, in as many loops as it needs.

A table of rows is a constant table too. Each row names its type, as in
`GRID :: [3]s32.[s32.[1, 2, 3], s32.[4, 5, 6]]`, and `#run` can build one
in a procedure that sets `g[a][b][c]` and returns it. `GRID[i][j]` reads the
shared table in place without copying the row, and `grid: [2][3]s32 = GRID`
starts a variable from it.

A file-scope variable may start from a table of any length, named as in
`table: [4096]u8 = Lib.BIG` or written out. Each target defines the variable
with its values, so nothing fills it when the program starts, and writing it
leaves the table unchanged.

### Bit counts and byte swaps

`clz(x)`, `ctz(x)` and `popcount(x)` count leading zero bits, trailing zero
bits and set bits; `bswap(x)` reverses the bytes. Each takes one `u8`, `u16`,
`u32` or `u64` and returns that type, and the counts give the width for zero
(`clz(cast(u32) 0) == 32`). Native C uses the compiler's `__builtin_*` with a
portable fallback, Go uses `math/bits`, and the portable engine computes the same
results. A procedure of your own with one of these names is called instead.
Run `sh tests/bit_builtins.sh build/bin/ziran`.

### SIMD vectors

`Simd :: #import "simd"` gives fixed-width vectors (`u8x16`, `s8x16`,
`u16x8`, `s16x8`, `u32x4`, `s32x4`, `u64x2`, `s64x2`, `f32x4` and `f64x2`,
and 256-bit forms such as `s16x16`) with operations such as `Simd.Add`,
`Simd.Load(s16x8, row, at)` and `Simd.ByteShuffle`; `std/simd.zi` lists
them all. `#import "std/simd"` works the same, under any alias. Vectors are
values, used through these operations rather than `+` or `==`. Every
operation gives the same bits on every target. Native C uses SSE2 on x86-64,
or SSSE3, SSE4.1 and AVX2 when the build enables them, NEON on AArch64 and
ARMv7, and WebAssembly SIMD128 when a wasm32 build passes `-msimd128`. Go,
and bundles compute lane by lane.

`Simd.Load(u8x16, text, at)` and `Simd.LoadLow(u8x16, text, at)` also
read immutable strings directly, including embedded NUL bytes. They check the
entire access against the string length without allocating a byte slice. Other
lane types and stores require their existing slice, array or pointer inputs.

Mark a procedure `#target` to let native x86-64 C pick its SIMD instructions
when the program runs:

```jai
Blend :: (row: []u8, out: []u8) #target {
    ...
}
```

Generated C holds SSSE3, SSE4.1 and AVX2 copies of `Blend` and calls the
best one the CPU runs, chosen once, so one SSE2 build uses AVX2 where it can.
`ZIRAN_SIMD_TIER=1` (SSE2) to `4` (AVX2) in the environment lowers the
choice for testing, and `#asm` variants needing a vector feature beyond that
tier, such as `x86_64+avx2` under `ZIRAN_SIMD_TIER=3`, are passed over. Builds that already enable AVX2, and other targets,
compile the procedure once. Each copy calls its own tier's copy of every
`#inline` procedure of the module, so helpers run with the caller's
instructions. A `#target` procedure cannot take or return a
256-bit vector by value, or pass one to another procedure, because AVX2 code
passes those in different registers; use an array or a pointer. It cannot be
`#inline` or hold `#asm` variants. Run `sh tests/simd.sh build/bin/ziran`
and `sh tests/simd_target.sh build/bin/ziran`. With `WASMTIME` set to a
wasmtime command, `tests/simd.sh` also runs its wasm32 modules; with
`ANDROID_NDK_HOME` it builds AArch64 and ARMv7, and runs them under
`QEMU_AARCH64` and `QEMU_ARM` or on the ARMv7 device `ARMV7_ADB_SERIAL`
names.

Native vectors (`u8xN`, `s8xN`, `u16xN`, `s16xN`, `u32xN`, `s32xN`,
`u64xN`, `s64xN`, `f32xN` and `f64xN`) are as wide as the code running
them handles well: two 128-bit blocks in a `#target` procedure's AVX2 copy
and in builds that enable AVX2, one elsewhere, Go and bundles included.
`Simd.Lanes(T)` and `Simd.Blocks(T)` give the count, which is a constant
for fixed vectors, so a loop steps by it:

```jai
Brighten :: (row: []u8) #target {
    i := 0
    while i + Simd.Lanes(u8xN) <= row.count {
        Simd.Store(row, i, Simd.AddSat(Simd.Load(u8xN, row, i), Simd.Splat(u8xN, 8)))
        i += Simd.Lanes(u8xN)
    }
}
```

They take the operations fixed vectors take. `Make`, `Splat` and `Shuffle`
name one block's lanes, which repeat; `Shuffle`, `ByteShuffle`,
`AlignBytes`, `ShiftBytesLeft`, `ShiftBytesRight`, `InterleaveLow` and
`InterleaveHigh` work on each block; `WidenLow`, `WidenHigh`, `MulWideLow`,
`MulWideHigh` and `Narrow` count lanes across the whole vector; `LoadLow`
and `StoreLow` move half of it. `Lane`, `WithLane`, `Widen`, `Low`, `High`
and `Combine` take no native vectors, and native and fixed vectors do not
mix. A native vector is a plain local, parameter or result: no global,
record field, array, pointer, slice, `size_of` or `#foreign` signature holds
one, and a `#target` procedure does not take, return or pass one. Run
`sh tests/simd_native.sh build/bin/ziran`.

### HTTP and process capabilities

`std/net_http.zi` defines an explicit request/response host capability. A
platform adapter supplies transport and TLS; checked Ziran code owns request
construction and response interpretation. A missing host binding is reported
before a portable bundle runs.

`std/constant_time.zi` compares equal-length byte spans without branching on
byte values. Use fixed-size buffers for secret tags so unequal lengths remain
public shape rather than a secret-dependent oracle.

`std/process.zi` defines a line-oriented child-process capability with explicit
arguments, stdin, an optional credential binding, a timeout, and a desktop
isolation request. The host starts the child, yields output lines, and returns
its exit result. Its contract runs from source and saved `.zir` as a portable
bundle and through native C and Go mocks.

### Files

`#import "std/file"` gives one set of file operations on every native C
target: it uses `std/file_linux.zi` on Linux, Android, the web, and Windows.
The shared operations are `OpenRead`, `OpenWrite`,
`OpenUpdate`, `OpenReplace`, `OpenNew`, `Read`, `Write`, `ReadAt`, `WriteAt`,
`FileSize`, `CloseFile`, `RemoveFile`, `RenameNoReplace`, `PathExists`,
`DirectoryExists`, `CreateDirectory`, `CreatePrivateDirectory`, and the
directory cursor. `OpenNew(path)` exclusively creates a writable file
(0644 restricted by umask); an occupied file or symbolic link fails unchanged.
Check the returned handle's `valid` field and close successful handles with
`CloseFile`. Two helpers read and replace whole files:

```jai
#import "std/file"

if WriteEntireFile("notes.txt", "hello\n") {
    text, ok := ReadEntireFile("notes.txt")
}
```

The Go target and portable bundles do not have these operations; Go
programs use `std/file_go`.

### Browser byte snapshots

`std/web` provides `WebBytes(kind, data, count)` for a copied JavaScript typed
array and `WebBlobBytes(data, count)` for a browser `Blob` snapshot. The latter
passes a Wasm memory view directly to the Blob constructor within one host
call, avoiding a separate copy of encoded file, media or request-body bytes.
The source may be changed or freed, and Wasm memory may grow, after it returns.
An invalid byte range returns handle `0`; an empty valid range returns an empty
Blob. Shared heaps require an explicit unshared snapshot because Blob excludes
shared BufferSources. Handles follow the usual `WebScope`/`WebKeep` lifetime.
`WebBytes` continues to return an independent typed-array copy.

### Native Linux adapters

Native Linux C builds can import `std/file_linux.zi` for positional byte I/O,
`std/binary_linux.zi` for little-endian numbers, `std/date_time_linux.zi` for
Unix time, `std/random_linux.zi` for operating-system secure randomness, and
`std/byte_text_linux.zi` for borrowed byte-to-text views, and
`std/environment_linux.zi` for environment values (`EnvironmentText`,
`EnvironmentNumber`). `std/socket_linux.zi`
opens literal-IPv4 TCP sockets with bounded polling, cancellation, and I/O.
`std/timer_linux.zi` supplies monotonic millisecond timestamps and bounded,
interruptible sleeps; it does not expose wall-clock time.
`std/thread_linux.zi` runs a procedure on a background thread that the
starting thread can poll for completion without waiting, which suits a frame
loop with a request in flight; it builds on pthreads, including Android's
and Windows winpthreads.
`ThreadStart` also accepts an optional `ThreadNotification` containing a
`procedure` and its `argument`.
The worker calls that notification once after publishing completion, allowing
an event loop or pipe waiter to wake immediately. The notification must not
join or restart its own thread. Keep the thread and argument storage alive
until `ThreadFinished` or `ThreadWait` joins it before consuming results.
macOS has no `pthread_tryjoin_np`, so there `ThreadFinished` reports the end
once the notification has returned and then joins, which can wait for the
thread's thread-local destructors.
`std/thread_windows.zi` provides the same worker interface for native Win64
without winpthreads. It starts through the C runtime, polls the native thread
handle with a zero timeout, and closes that handle only after the thread and
its cleanup finish. Keep its record and arguments at stable addresses until
completion, including while a canceled request is being collected.
`tests/thread_windows.sh` compiles and links C source and saved IR with
MinGW against the thread completion host.
`std/parallel_linux.zi` supplies a reusable `ParallelPool` for independent
half-open ranges. `ParallelRun` divides a range among persistent workers and
the submitting thread, then waits for every lane before returning. Short ranges
use a subset of the pool instead of becoming serial when more lanes are
configured. Keep the pool at one address, submit from one owner, and call `ParallelClose` before
releasing it. Callbacks must write only their own range and keep shared inputs
immutable. Four lanes run by default; `ZIRAN_PAR_THREADS` selects 1–64 lanes.
Native C source builds define the genuine `POSIX_THREADS` capability
automatically when pthread-compatible threads are available, including
Windows. On `_WIN32`, the pool uses `std/thread_windows.zi`, native SRW locks
and condition variables from kernel32; it needs no winpthreads or `-pthread`.
`tests/native_workers_windows.sh` compiles and links source and saved IR
with MinGW against the native worker pool. MinGW tests accept `MCFGTHREADS` as the installation
prefix of an mcfgthread runtime when its `lib` directory is outside the compiler's
search path. Web selection uses `OS == .WEB`; `PLATFORM_WEB` is not a target
selector. Untargeted IR stays portable. Explicit C IR freezes the same
capability selection as source builds. Libraries
can use that capability to parallelize their native adapters by default while
keeping portable output independent of pthreads. This does not parallelize
arbitrary program state or make shared mutable data safe.
The file module can create a private file exclusively, sync its contents, and
publish it through a hard link that fails if the destination already exists.
`std/byte_text_linux.zi` also borrows caller-owned C strings and byte buffers;
the caller must keep their memory alive. `std/mapped_file_linux.zi` maps a file
privately for bounded byte parsing without copying it. Its borrowed slice is
valid only until `UnmapFile`, and writes to that slice do not change the file.
`std/process_capture_linux.zi` captures a child process without a shell;
`std/net_http_linux.zi` uses it to send JSON over HTTPS through `curl`.
`std/process_channel_linux.zi` keeps an owned child open for bidirectional byte
I/O. `StartChannel` takes 1 through 32 explicit arguments (each at most 4095
non-NUL bytes) and optionally merges stderr into the channel. Invalid argument
counts fail before creating a socket or child. `ChannelSend` and `ChannelReceive` use caller-owned
buffers and bounded waits. A third `discard_stderr` argument defaults to false;
when true the child sends stderr to `/dev/null` without mixing it into stdout.
Merging and discarding together are rejected before launch. `WaitChannel` joins
the child and records its exit
code. `CloseChannel` closes the stream, waits for the requested grace period,
then terminates only the child's own process group if it is still running.
`CancelChannel` instead signals the still-owned unreaped child group before
joining, so cancellation also stops descendants retaining stdout after their
parent exits. It closes the endpoint and reaps the direct child; repeating it
after joining is harmless. Do not reap the parent separately while awaiting
stdout, because its unreaped PID proves this process-group ownership.
Keep one owner for each channel and close it before releasing its storage;
failed sends may have delivered a prefix and must not be blindly replayed.

Build and release programs use three more modules. `std/process_run_linux.zi`
runs a child with any number of arguments, a working directory, environment
changes, and each stream inherited, read from or written to a file, or
captured (`RunProcess`); `StartProcess`, `PollProcess`, `WaitProcess` and
`SignalProcess` follow a child left running, and a program that cannot start
reports `started = false`. `std/file_tree_linux.zi` creates, lists, copies and
removes folders, reads a path's kind, owner UID (`PathInfo.owner_uid`), permissions,
size and modification time,
replaces a file atomically, takes `flock` locks and matches shell wildcards.
`PathInfo.modified` gives signed seconds since the Unix epoch;
`modified_nanoseconds` gives its fractional nanoseconds from 0 through 999999999,
not total nanoseconds. Missing paths return zero for both fields. Programs that
combine them into a signed 64-bit timestamp must check multiplication/addition
overflow for their supported date range.
`PathStatus(path, false)` reports a symbolic link's own metadata. `PathStatus`
returns zero `PathInfo` on any failure. Use
`PathQuery(path: string, follow_links: bool = true) -> PathQueryResult` when
absence must be distinguished from inaccessible paths. The result contains
`info: PathInfo`, `valid: bool`, `missing: bool` and `error: s32`.
Success sets `valid = true`, `missing = false` and `error = 0`. Failure leaves
zero `info` and sets `valid = false`; `error` is the positive Linux errno and
`missing` is true only for ENOENT (2) or ENOTDIR (20). EACCES (13), ELOOP (40)
and other errors are not absence. Embedded NUL or paths exceeding 4095 bytes
return EINVAL (22), with `missing = false`. Empty paths reach the OS and return
ENOENT. Following a dangling symlink reports absence; `follow_links = false`
queries the link itself. These queries are snapshots; later operations can fail.
`ListDirectory(path, names)` appends independently owned names to the caller's
`Vec(string)`. Call `FreeDirectoryNames(names)` to free every name and the vector
storage and reset it to empty. Empty, repeated and null releases are harmless;
borrowed views expire on release. Do not mix borrowed strings or duplicate
ownership into that vector.
`CopySymbolicLink(source, target)` copies the link itself, including dangling
and directory links. `CopyFileNew(source, target)` and `CopyTreeNew(source, target)`
copy a file or tree exclusively, preserving links and refusing occupied
destinations. Tree destinations require an existing parent; self and descendant
copies are rejected, including through parent aliases. Keep ancestors and source
entries stable during copying. On failure these functions return false and may
leave a partial new destination; they do not delete it. Copy files start with
0600 permissions and directories with 0700 (restricted by umask); source
permission bits are applied after each successful copy. `CopyFile` still replaces
files and `CopyTree` still merges into existing directories.
`ResolvePath(path)` returns the canonical absolute path with symbolic links and
dot components resolved, or empty for a missing or invalid path.
`CreateTemporaryDirectory(parent, prefix = "ziran-")` atomically creates a unique
child under an existing directory owned by the effective user. It returns the
absolute path or empty on failure. The prefix must be one name component;
six random characters follow it. Permissions are 0700 restricted by umask.
Keep the parent and its ancestors stable during creation, and call `RemoveTree`
when finished. Run `sh tests/std_file_tree_linux.sh build/bin/ziran` for C source
and saved-IR query success/errno (including inaccessible parents), ownership,
canonicalization and private-directory tests. The EACCES assertion requires a
non-root user; root runs explicitly report that assertion as skipped.
`std/text_tools.zi` finds, splits, trims, replaces and joins text, and decodes
bounded hexadecimal into a caller-owned byte vector on any target.
`std/stdio_linux.zi` writes to standard output or standard error and reads all
of standard input.
Run `sh tests/std_tools_linux.sh build/bin/ziran` for their checks.

These native adapters keep libc calls out of applications and require glibc
Linux and a `curl` executable. Portable bundles should use the host capabilities
above instead.

Windows C builds (`--define _WIN32`) keep the same file and mapping API over
Win32. Paths pass as UTF-8 and are converted to UTF-16. Positional reads and
writes leave the file position unchanged, as on POSIX, and a private mapping
is a copy-on-write view. An open file can itself be renamed or removed, but
unlike POSIX, a rename cannot replace a destination that is still open. The
libcurl HTTP and WebSocket adapters also build there; `std/zip_linux.zi` needs
a MinGW zlib. `tests/std_file_windows.sh` compiles and links the file modules with MinGW
when it is installed.

### Native Go maps

Import `map_go` to use `Map(K, V)` with the Go target. Keys must be comparable;
values cannot own `Vec` storage. A map's zero value is nil. Copies share entries,
while replacing a map binding changes only that binding. Maps can be compared
with `null`, and their contents are accessed through these operations:

| Operation | Behavior |
|---|---|
| `MapInit(values)` | Allocate an empty map if the binding is nil |
| `MapSet(values, key, value)` | Insert or replace; allocate nil storage first |
| `MapGet(values, key)` | Read an entry, or return the value type's zero value |
| `MapLookup(values, key)` | Return `Option(V)`; import `option` to use this operation |
| `MapContains(values, key)` | Test key presence, including entries with zero values |
| `MapDelete(values, key)` | Remove an entry; missing keys are harmless |
| `MapClear(values)` | Remove every entry while preserving allocated storage |
| `MapCount(values)` | Return the entry count as `s64` |
| `MapKeys(values)` | Return an unordered `[]K` snapshot |

`MapInit` and `MapSet` require a mutable binding or field. Nil maps support
reads, membership checks, deletion and clearing. Map mutation in `#parallel`
regions is rejected because copies share storage. Other targets and portable
ABIs reject maps; `.zir` preserves their Go behavior.

Import `go_types` for the predeclared Go `Any` and `Error` interfaces. `Any`
accepts values that do not own `Vec` storage, including nested maps for JSON.

### Native civil clocks and calendars

Import `date_time_linux` for `UnixNow()`, `LocalNow()`,
`LocalAt(seconds)`, and `UtcAt(seconds)`. `LocalAt` uses the operating system's
timezone and daylight-saving rules. The shared `LocalDateTime` record lives
in `date_time_types`; the hosted `date_time` module re-exports that same type.
Months and days are one-based, `day_of_year` is zero-based, and `valid` reports
whether the provider could obtain a reading. Dates after 2038 retain their full value.

With `--define _WIN32`, `date_time_linux` and `timer_linux` use native Windows
APIs instead of POSIX calls. Windows civil clocks use
FILETIME and the system's timezone rules, with valid dates from 1601 through
9999. Its named-zone query recognizes UTC/GMT; other IANA names return an
invalid offset because these Windows APIs do not expose an IANA database.
Monotonic timestamps use QueryPerformanceCounter.

Import `calendar` for Gregorian `LeapYear`, `DaysInMonth`, `DayOfWeek`,
`DayOfYear`, `DateTimeValid`, and `FormatDateTime`. `DayOfWeek` returns zero for
Sunday and minus one for an invalid date. Calendar dates range from year 1 to
9999. `FormatDateTime(reading, pattern, output)` writes into a caller-owned
byte slice, leaves a NUL terminator, and returns a view of the complete result.
Invalid readings, unsupported tokens, and insufficient space return an empty
view and clear the first output byte. Names are English and independent of
locale. Supported civil tokens are `%a`, `%A`, `%b`, `%B`, `%h`, `%C`, `%d`,
`%e`, `%F`, `%D`, `%H`, `%I`, `%j`, `%k`, `%m`, `%M`, `%p`, `%P`, `%r`, `%R`,
`%S`, `%T`, `%u`, `%w`, `%X`, `%y`, `%Y`, `%n`, `%t`, and `%%`. The calendar
calculations and formatting also run from portable bundles without a clock
host.

Local conditional construction can share an explicitly named ordinary record
type between its arms:

```zi
Pair :: struct { value: s32; }
Choose :: (yes: bool) -> Pair {
    result := ifx yes then Pair.{value=41} else .{value=42};
    return result;
}
```

The reverse arm order works too, including nested fields with their checked
record context. At least one arm must explicitly name that record. This does
not search types by their fields, infer public signatures or imports, or infer
owned, foreign, union, enum or procedure types from an unnamed sibling.
Ambiguous constructors and mismatched fields remain errors. The checked source
and saved IR retain the resolved record type; both arms keep their ordinary
lazy runtime evaluation.

For checked, hash-bound source changes see [source edits](SEMANTIC_EDIT.md).
For the opt-in scalar direct-native compiler, lowered-IR dumps and final-byte
source maps, see [the ASSEMBLY prototype](../ASSEMBLY/README.md). It is not yet a
replacement for the portable and generated-C runtime paths.
An optional byte-exact transport with source maps is described in
[compact transport](TRANSPORT_VIEW.md); `.zi` remains canonical source.
